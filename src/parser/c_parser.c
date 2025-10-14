#include "c_parser.h"
#include "../ast/ast.h"
#include "../common/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== PERFORMANCE OPTIMIZATIONS =====
 * 
 * This parser includes several performance optimizations:
 * 
 * 1. Hash Function Bitmasking: Uses bitwise AND instead of modulo for hash table
 *    indexing. Table sizes are powers of 2 for fast masking.
 * 
 * 2. Token Caching: Caches CURRENT(parser) calls in hot loops to avoid repeated
 *    function calls (e.g., struct_declaration_list).
 * 
 * 3. Declaration Specifier Caching: Caches c_is_declaration_specifier() results
 *    to avoid repeated checks in compound statements and for-loops.
 * 
 * 4. Optimized String Prefix Check: Uses character-by-character comparison
 *    instead of strncmp for __builtin_ prefix.
 * 
 * 5. Reduced String Duplication: Uses direct lexeme references for member access
 *    instead of xstrdup/xfree.
 * 
 * 6. Pre-allocated Argument Arrays: Function call parsing pre-allocates 16
 *    argument slots to avoid reallocation in typical cases.
 */

/* Simple symbol table entry */
typedef struct SymbolEntry {
  char *name;
  struct SymbolEntry *next;
} SymbolEntry;

/* Simple hash table for symbol tracking */
#define SYMBOL_TABLE_SIZE 1024  /* Power of 2 for bitmasking */
#define SYMBOL_TABLE_MASK (SYMBOL_TABLE_SIZE - 1)
#define BUILTIN_TYPES_TABLE_SIZE 512  /* Power of 2 for bitmasking */
#define BUILTIN_TYPES_TABLE_MASK (BUILTIN_TYPES_TABLE_SIZE - 1)

/* Improved hash function with bitmasking instead of modulo */
static unsigned int hash_string(const char *str) {
  uint64_t hash = 0xcbf29ce484222325ULL;  /* FNV-1a hash */
  for (const unsigned char *p = (const unsigned char *)str; *p; p++) {
    hash ^= *p;
    hash *= 0x100000001b3ULL;
  }
  return (unsigned int)(hash & SYMBOL_TABLE_MASK);  /* Bitmasking is much faster than % */
}

static unsigned int hash_builtin_type(const char *str) {
  uint64_t hash = 0xcbf29ce484222325ULL;
  for (const unsigned char *p = (const unsigned char *)str; *p; p++) {
    hash ^= *p;
    hash *= 0x100000001b3ULL;
  }
  return (unsigned int)(hash & BUILTIN_TYPES_TABLE_MASK);  /* Bitmasking is much faster than % */
}

/* Global builtin types hash table */
static SymbolEntry *builtin_types_table[BUILTIN_TYPES_TABLE_SIZE] = {NULL};
static bool builtin_types_initialized = false;

/* Add a builtin type to the global hash table */
static void add_builtin_type(const char *name) {
  unsigned int index = hash_builtin_type(name);
  SymbolEntry *entry = xmalloc(sizeof(SymbolEntry));
  entry->name = (char *)name;  /* Static string, no need to dup */
  entry->next = builtin_types_table[index];
  builtin_types_table[index] = entry;
}

/* Check if a name is a builtin type (O(1) average case) */
static bool is_builtin_type(const char *name) {
  unsigned int index = hash_builtin_type(name);
  for (SymbolEntry *entry = builtin_types_table[index]; entry; entry = entry->next) {
    if (strcmp(entry->name, name) == 0) {
      return true;
    }
  }
  return false;
}

/* Initialize builtin types hash table (called once) */
static void init_builtin_types(void) {
  if (builtin_types_initialized) return;
  builtin_types_initialized = true;
  
  /* Add ALL builtin types from the original list - runs once at startup */
  /* va_list types */
  add_builtin_type("__gnuc_va_list"); add_builtin_type("__builtin_va_list"); add_builtin_type("__va_list_tag");
  
  /* Integer types */
  add_builtin_type("__int8_t"); add_builtin_type("__int16_t"); add_builtin_type("__int32_t"); add_builtin_type("__int64_t");
  add_builtin_type("__uint8_t"); add_builtin_type("__uint16_t"); add_builtin_type("__uint32_t"); add_builtin_type("__uint64_t");
  add_builtin_type("__intptr_t"); add_builtin_type("__uintptr_t"); add_builtin_type("__size_t"); add_builtin_type("__ptrdiff_t");
  add_builtin_type("__wchar_t"); add_builtin_type("__int128_t"); add_builtin_type("__uint128_t"); add_builtin_type("__int128");
  add_builtin_type("__uint128"); add_builtin_type("__intmax_t"); add_builtin_type("__uintmax_t");
  
  /* GCC/Clang specific */
  add_builtin_type("__int8"); add_builtin_type("__int16"); add_builtin_type("__int32"); add_builtin_type("__int64");
  add_builtin_type("__float128"); add_builtin_type("__float80"); add_builtin_type("__fp16"); add_builtin_type("__bf16");
  
  /* SIMD types */
  add_builtin_type("__m64"); add_builtin_type("__m128"); add_builtin_type("__m128i"); add_builtin_type("__m128d");
  add_builtin_type("__m256"); add_builtin_type("__m256i"); add_builtin_type("__m256d");
  add_builtin_type("__m512"); add_builtin_type("__m512i"); add_builtin_type("__m512d");
  add_builtin_type("__v2df"); add_builtin_type("__v2di"); add_builtin_type("__v4df"); add_builtin_type("__v4di");
  add_builtin_type("__v4sf"); add_builtin_type("__v4si"); add_builtin_type("__v8sf"); add_builtin_type("__v8si");
  
  /* Atomic types */
  add_builtin_type("__atomic_int"); add_builtin_type("__atomic_uint"); add_builtin_type("__atomic_long");
  add_builtin_type("__atomic_ulong"); add_builtin_type("__atomic_llong"); add_builtin_type("__atomic_ullong");
  
  /* System types from headers */
  add_builtin_type("__off_t"); add_builtin_type("__off64_t"); add_builtin_type("__mbstate_t"); add_builtin_type("__fpos_t");
  add_builtin_type("__fpos64_t"); add_builtin_type("__u_char"); add_builtin_type("__u_short"); add_builtin_type("__u_int");
  add_builtin_type("__u_long"); add_builtin_type("__quad_t"); add_builtin_type("__u_quad_t"); add_builtin_type("__dev_t");
  add_builtin_type("__uid_t"); add_builtin_type("__gid_t"); add_builtin_type("__ino_t"); add_builtin_type("__ino64_t");
  add_builtin_type("__mode_t"); add_builtin_type("__nlink_t"); add_builtin_type("__pid_t"); add_builtin_type("__fsid_t");
  add_builtin_type("__clock_t"); add_builtin_type("__rlim_t"); add_builtin_type("__rlim64_t"); add_builtin_type("__id_t");
  add_builtin_type("__time_t"); add_builtin_type("__useconds_t"); add_builtin_type("__suseconds_t"); add_builtin_type("__suseconds64_t");
  add_builtin_type("__daddr_t"); add_builtin_type("__key_t"); add_builtin_type("__clockid_t"); add_builtin_type("__timer_t");
  add_builtin_type("__blksize_t"); add_builtin_type("__blkcnt_t"); add_builtin_type("__blkcnt64_t");
  add_builtin_type("__fsblkcnt_t"); add_builtin_type("__fsblkcnt64_t"); add_builtin_type("__fsfilcnt_t"); add_builtin_type("__fsfilcnt64_t");
  add_builtin_type("__fsword_t"); add_builtin_type("__ssize_t"); add_builtin_type("__syscall_slong_t"); add_builtin_type("__syscall_ulong_t");
  add_builtin_type("__loff_t"); add_builtin_type("__caddr_t"); add_builtin_type("__socklen_t"); add_builtin_type("__sig_atomic_t");
  add_builtin_type("__sigset_t"); add_builtin_type("__fd_mask"); add_builtin_type("__fd_set");
  
  /* Thread types */
  add_builtin_type("__pthread_t"); add_builtin_type("__pthread_attr_t"); add_builtin_type("__pthread_mutex_t");
  add_builtin_type("__pthread_mutexattr_t"); add_builtin_type("__pthread_cond_t"); add_builtin_type("__pthread_condattr_t");
  add_builtin_type("__pthread_key_t"); add_builtin_type("__pthread_once_t"); add_builtin_type("__pthread_rwlock_t");
  add_builtin_type("__pthread_rwlockattr_t"); add_builtin_type("__pthread_spinlock_t"); add_builtin_type("__pthread_barrier_t");
  add_builtin_type("__pthread_barrierattr_t");
  
  /* Signal types */
  add_builtin_type("__sigval_t"); add_builtin_type("__siginfo_t"); add_builtin_type("__sigevent_t");
  
  /* Locale types */
  add_builtin_type("__locale_t"); add_builtin_type("__locale_data");
  
  /* Regex types */
  add_builtin_type("__regex_t"); add_builtin_type("__regmatch_t");
  
  /* Directory types */
  add_builtin_type("__DIR"); add_builtin_type("__dirstream");
  
  /* Time types */
  add_builtin_type("__time64_t"); add_builtin_type("__timespec"); add_builtin_type("__timeval"); add_builtin_type("__itimerspec");
  add_builtin_type("__timezone");
  
  /* Standard I/O types */
  add_builtin_type("__FILE"); add_builtin_type("__cookie_io_functions_t");
  
  /* Other common system types */
  add_builtin_type("__jmp_buf"); add_builtin_type("__sigjmp_buf"); add_builtin_type("__rlimit"); add_builtin_type("__rlimit64");
  add_builtin_type("__rusage"); add_builtin_type("__timex"); add_builtin_type("__iovec"); add_builtin_type("__sockaddr");
  add_builtin_type("__msghdr"); add_builtin_type("__cmsghdr"); add_builtin_type("__stat"); add_builtin_type("__stat64");
  add_builtin_type("__statfs"); add_builtin_type("__statfs64"); add_builtin_type("__statvfs"); add_builtin_type("__statvfs64");
  add_builtin_type("__dirent"); add_builtin_type("__dirent64"); add_builtin_type("__ucontext"); add_builtin_type("__mcontext_t");
  add_builtin_type("__sigcontext"); add_builtin_type("__stack_t"); add_builtin_type("__sigaction");
  
  /* Standard C library types (non-underscore versions) */
  add_builtin_type("FILE"); add_builtin_type("va_list"); add_builtin_type("off_t"); add_builtin_type("ssize_t");
  add_builtin_type("size_t"); add_builtin_type("fpos_t"); add_builtin_type("ptrdiff_t"); add_builtin_type("wchar_t");
  add_builtin_type("wint_t"); add_builtin_type("wctype_t"); add_builtin_type("mbstate_t");
  add_builtin_type("int8_t"); add_builtin_type("int16_t"); add_builtin_type("int32_t"); add_builtin_type("int64_t");
  add_builtin_type("uint8_t"); add_builtin_type("uint16_t"); add_builtin_type("uint32_t"); add_builtin_type("uint64_t");
  add_builtin_type("intptr_t"); add_builtin_type("uintptr_t"); add_builtin_type("intmax_t"); add_builtin_type("uintmax_t");
  add_builtin_type("pid_t"); add_builtin_type("uid_t"); add_builtin_type("gid_t"); add_builtin_type("dev_t");
  add_builtin_type("ino_t"); add_builtin_type("mode_t"); add_builtin_type("nlink_t"); add_builtin_type("time_t");
  add_builtin_type("clock_t"); add_builtin_type("clockid_t"); add_builtin_type("timer_t");
  add_builtin_type("suseconds_t"); add_builtin_type("useconds_t"); add_builtin_type("blksize_t"); add_builtin_type("blkcnt_t");
  add_builtin_type("fsblkcnt_t"); add_builtin_type("fsfilcnt_t"); add_builtin_type("id_t"); add_builtin_type("key_t");
  add_builtin_type("pthread_t"); add_builtin_type("pthread_attr_t"); add_builtin_type("pthread_mutex_t");
  add_builtin_type("pthread_mutexattr_t"); add_builtin_type("pthread_cond_t"); add_builtin_type("pthread_condattr_t");
  add_builtin_type("pthread_key_t"); add_builtin_type("pthread_once_t"); add_builtin_type("pthread_rwlock_t");
  add_builtin_type("pthread_rwlockattr_t"); add_builtin_type("pthread_spinlock_t"); add_builtin_type("pthread_barrier_t");
  add_builtin_type("pthread_barrierattr_t"); add_builtin_type("sigset_t"); add_builtin_type("sig_atomic_t");
  add_builtin_type("socklen_t"); add_builtin_type("sa_family_t"); add_builtin_type("in_addr_t"); add_builtin_type("in_port_t");
  add_builtin_type("locale_t"); add_builtin_type("DIR"); add_builtin_type("regex_t"); add_builtin_type("regmatch_t");
  add_builtin_type("regoff_t"); add_builtin_type("div_t"); add_builtin_type("ldiv_t"); add_builtin_type("lldiv_t");
  add_builtin_type("imaxdiv_t"); add_builtin_type("jmp_buf"); add_builtin_type("sigjmp_buf"); add_builtin_type("fenv_t");
  add_builtin_type("fexcept_t");
}

static void symbol_table_init(void **table) {
  *table = xcalloc(SYMBOL_TABLE_SIZE, sizeof(SymbolEntry *));
}

static void symbol_table_destroy(void *table) {
  if (!table)
    return;

  SymbolEntry **entries = (SymbolEntry **)table;
  for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
    SymbolEntry *entry = entries[i];
    while (entry) {
      SymbolEntry *next = entry->next;
      xfree(entry->name);
      xfree(entry);
      entry = next;
    }
  }
  xfree(table);
}

static void symbol_table_add(void *table, const char *name) {
  if (!table || !name)
    return;

  SymbolEntry **entries = (SymbolEntry **)table;
  unsigned int index = hash_string(name);

  /* Check if already exists */
  SymbolEntry *entry = entries[index];
  while (entry) {
    if (strcmp(entry->name, name) == 0) {
      return; /* Already exists */
    }
    entry = entry->next;
  }

  /* Add new entry */
  SymbolEntry *new_entry = xcalloc(1, sizeof(SymbolEntry));
  new_entry->name = xstrdup(name);
  new_entry->next = entries[index];
  entries[index] = new_entry;
}

static bool symbol_table_contains(void *table, const char *name) {
  if (!table || !name)
    return false;

  SymbolEntry **entries = (SymbolEntry **)table;
  unsigned int index = hash_string(name);

  SymbolEntry *entry = entries[index];
  while (entry) {
    if (strcmp(entry->name, name) == 0) {
      return true;
    }
    entry = entry->next;
  }
  return false;
}

/* Helper macros for cleaner code - cast CTokenType to TokenType */
#define CURRENT(p) parser_current(&(p)->base)
#define PEEK(p, n) parser_peek(&(p)->base, n)
#define MATCH(p, t) parser_match(&(p)->base, (TokenType)(t))
#define CHECK(p, t) parser_check(&(p)->base, (TokenType)(t))
#define ADVANCE(p) parser_advance(&(p)->base)
#define EXPECT(p, t, msg) parser_expect(&(p)->base, (TokenType)(t), msg)
#define AT_END(p) parser_at_end(&(p)->base)
#define ERROR(p, msg) parser_error(&(p)->base, msg)

/* Forward declarations */
static const char *c_extract_declarator_name(ASTNode *declarator);

/* ===== GCC EXTENSIONS ===== */

/* Parse GCC __attribute__ */
static void c_parse_gcc_attribute(CParser *parser) {
  if (!CHECK(parser, TOKEN___ATTRIBUTE__)) {
    return;
  }

  ADVANCE(parser); /* consume __attribute__ */

  if (!MATCH(parser, TOKEN_LPAREN)) {
    return;
  }

  if (!MATCH(parser, TOKEN_LPAREN)) {
    return;
  }

  /* Parse attribute list - skip everything until matching )) */
  int paren_depth = 2;
  while (!AT_END(parser) && paren_depth > 0) {
    if (CHECK(parser, TOKEN_LPAREN)) {
      paren_depth++;
    } else if (CHECK(parser, TOKEN_RPAREN)) {
      paren_depth--;
    }
    ADVANCE(parser);
  }
}

/* Parse GCC __asm__ */
static void c_parse_gcc_asm(CParser *parser) {
  if (!CHECK(parser, TOKEN___ASM__)) {
    return;
  }

  ADVANCE(parser); /* consume __asm__ */

  if (!MATCH(parser, TOKEN_LPAREN)) {
    return;
  }

  /* Skip asm content until matching ) */
  int paren_depth = 1;
  while (!AT_END(parser) && paren_depth > 0) {
    if (CHECK(parser, TOKEN_LPAREN)) {
      paren_depth++;
    } else if (CHECK(parser, TOKEN_RPAREN)) {
      paren_depth--;
    }
    ADVANCE(parser);
  }
}

/* Parse all GCC attributes/extensions that might appear */
static void c_parse_gcc_extensions(CParser *parser) {
  while (true) {
    if (CHECK(parser, TOKEN___ATTRIBUTE__)) {
      c_parse_gcc_attribute(parser);
    } else if (CHECK(parser, TOKEN___ASM__)) {
      c_parse_gcc_asm(parser);
    } else if (MATCH(parser, TOKEN___EXTENSION__)) {
      /* __extension__ just suppresses warnings, skip it */
      continue;
    } else {
      break;
    }
  }
}

/* ===== LIFECYCLE ===== */

CParser *c_parser_create(TokenList *tokens, CStandard standard) {
  CParser *parser = xcalloc(1, sizeof(CParser));
  parser->base.tokens = tokens;
  parser->base.position = 0;
  parser->base.current = tokens->head;
  /* Select syntax based on C standard */
  switch (standard) {
  case C_STD_C89:
  case C_STD_C90:
  case C_STD_GNU89:
    parser->base.syntax = syntax_c99_create(); /* Use C99 for now */
    break;
  default:
    parser->base.syntax = syntax_c99_create();
    break;
  }
  parser->base.panic_mode = false;
  parser->base.error_count = 0;
  parser->standard = standard;
  parser->scope_depth = 0;
  /* Initialize symbol tables */
  symbol_table_init(&parser->typedef_names);
  symbol_table_init(&parser->struct_tags);
  symbol_table_init(&parser->union_tags);
  symbol_table_init(&parser->enum_tags);
  return parser;
}

void c_parser_destroy(CParser *parser) {
  /* Free symbol tables */
  symbol_table_destroy(parser->typedef_names);
  symbol_table_destroy(parser->struct_tags);
  symbol_table_destroy(parser->union_tags);
  symbol_table_destroy(parser->enum_tags);
  /* Free syntax definition */
  if (parser->base.syntax) {
    syntax_c99_destroy(parser->base.syntax);
  }
  xfree(parser);
}

ASTNode *c_parser_parse(CParser *parser) {
  return c_parse_translation_unit(parser);
}

/* ===== DECLARATIONS ===== */

ASTNode *c_parse_translation_unit(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;
  ASTNode *unit = ast_create_translation_unit(loc);

  while (!AT_END(parser)) {
    Token *before = CURRENT(parser);
    ASTNode *decl = c_parse_external_declaration(parser);
    Token *after = CURRENT(parser);

    if (decl) {
      ast_add_child(unit, decl);
    }

    if (parser->base.panic_mode) {
      parser_synchronize(&parser->base);
    }

    /* Safety check: ensure we're making progress */
    if (before == after && !AT_END(parser)) {
      /* No progress made - force advance to avoid infinite loop */
      ERROR(parser, "parser stuck - forcing advance");
      ADVANCE(parser);
    }

    /* Additional safety: if we've had too many consecutive errors, skip more
     * aggressively */
    static int consecutive_errors = 0;
    if (!decl) {
      consecutive_errors++;
      if (consecutive_errors > 10) {
        /* Skip to next likely declaration start */
        while (!AT_END(parser) && !CHECK(parser, TOKEN_TYPEDEF) &&
               !CHECK(parser, TOKEN_STRUCT) && !CHECK(parser, TOKEN_UNION) &&
               !CHECK(parser, TOKEN_ENUM) && !CHECK(parser, TOKEN_STATIC) &&
               !CHECK(parser, TOKEN_EXTERN) && !CHECK(parser, TOKEN_INLINE) &&
               !CHECK(parser, TOKEN___UINT16_T) &&
               !CHECK(parser, TOKEN___UINT32_T) &&
               !CHECK(parser, TOKEN___UINT64_T) && !CHECK(parser, TOKEN_INT) &&
               !CHECK(parser, TOKEN_CHAR) && !CHECK(parser, TOKEN_VOID) &&
               !CHECK(parser, TOKEN_BOOL)) {
          ADVANCE(parser);
        }
        consecutive_errors = 0;
      }
    } else {
      consecutive_errors = 0;
    }
  }

  return unit;
}

ASTNode *c_parse_external_declaration(CParser *parser) {
  /* External declaration is either a function definition or declaration */

  /* Handle GCC extensions at global scope */
  if (CHECK(parser, TOKEN___EXTENSION__)) {
    ADVANCE(parser); /* consume __extension__ */
                     /* Continue parsing the actual declaration */
  }

  /* Allow empty declarations (standalone semicolons) */
  if (MATCH(parser, TOKEN_SEMICOLON)) {
    SourceLocation loc = CURRENT(parser)->location;
    return ast_create_node(AST_NULL_STMT, loc);
  }

  if (c_is_declaration_specifier(parser)) {
    return c_parse_declaration(parser);
  }

  /* Check if this looks like a function definition that we can try to parse */
  if (CHECK(parser, TOKEN___UINT16_T) || CHECK(parser, TOKEN___UINT32_T) ||
      CHECK(parser, TOKEN___UINT64_T) || CHECK(parser, TOKEN___INT8_T) ||
      CHECK(parser, TOKEN___INT16_T) || CHECK(parser, TOKEN___INT32_T) ||
      CHECK(parser, TOKEN___INT64_T)) {
    /* Try to parse as a declaration - this might be a function definition */
    return c_parse_declaration(parser);
  }

  /* Invalid construct at global scope - skip to next valid declaration */
  ERROR(parser, "expected declaration at global scope");

  /* Skip tokens until we find a semicolon, opening brace, or known declaration
   * starter */
  while (!AT_END(parser) && !CHECK(parser, TOKEN_SEMICOLON) &&
         !CHECK(parser, TOKEN_LBRACE) && !CHECK(parser, TOKEN_TYPEDEF) &&
         !CHECK(parser, TOKEN_STRUCT) && !CHECK(parser, TOKEN_UNION) &&
         !CHECK(parser, TOKEN_ENUM) && !CHECK(parser, TOKEN_STATIC) &&
         !CHECK(parser, TOKEN_EXTERN) && !CHECK(parser, TOKEN_INLINE) &&
         !CHECK(parser, TOKEN_INT) && !CHECK(parser, TOKEN_CHAR) &&
         !CHECK(parser, TOKEN_VOID) && !CHECK(parser, TOKEN_FLOAT) &&
         !CHECK(parser, TOKEN_DOUBLE) && !CHECK(parser, TOKEN_LONG) &&
         !CHECK(parser, TOKEN_SHORT) && !CHECK(parser, TOKEN_SIGNED) &&
         !CHECK(parser, TOKEN_UNSIGNED) && !CHECK(parser, TOKEN_CONST) &&
         !CHECK(parser, TOKEN_VOLATILE) && !CHECK(parser, TOKEN___UINT8_T) &&
         !CHECK(parser, TOKEN___UINT16_T) && !CHECK(parser, TOKEN___UINT32_T) &&
         !CHECK(parser, TOKEN___UINT64_T) && !CHECK(parser, TOKEN___INT8_T) &&
         !CHECK(parser, TOKEN___INT16_T) && !CHECK(parser, TOKEN___INT32_T) &&
         !CHECK(parser, TOKEN___INT64_T) && !CHECK(parser, TOKEN___INT128) &&
         !CHECK(parser, TOKEN___UINT128_T)) {
    ADVANCE(parser);
  }

  /* If we found a semicolon, consume it */
  if (CHECK(parser, TOKEN_SEMICOLON)) {
    ADVANCE(parser);
  }

  return NULL;
}

ASTNode *c_parse_function_definition(CParser *parser) {
  /* Function definition is handled in c_parse_declaration */
  return c_parse_declaration(parser);
}

ASTNode *c_parse_declaration(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;

  /* Parse any leading GCC extensions */
  c_parse_gcc_extensions(parser);

  /* Check if this is a typedef */
  bool is_typedef = CHECK(parser, TOKEN_TYPEDEF);

  /* Parse declaration specifiers (storage class, type, qualifiers) */
  ASTNode *decl_specs = c_parse_declaration_specifiers(parser);
  if (!decl_specs) {
    return NULL;
  }

  /* Parse GCC attributes after declaration specifiers */
  c_parse_gcc_extensions(parser);

  /* Check for just type declaration (struct/union/enum without declarator) */
  if (MATCH(parser, TOKEN_SEMICOLON)) {
    return decl_specs;
  }

  /* Parse declarator */
  ASTNode *declarator = c_parse_declarator(parser);


  /* Parse GCC attributes after declarator */
  c_parse_gcc_extensions(parser);

  /* If this is a typedef, register the typedef name */
  if (is_typedef && declarator) {
    /* Extract the identifier from the declarator (could be nested in function
     * pointers) */
    const char *typedef_name = c_extract_declarator_name(declarator);
    if (typedef_name) {
      c_parser_add_typedef(parser, typedef_name);
    }
  }

  /* Check for function definition vs declaration */
  if (declarator && CHECK(parser, TOKEN_LBRACE)) {
    /* Function definition */
    ASTNode *body = c_parse_compound_statement(parser);
    /* Extract function name from declarator */
    const char *func_name = c_extract_declarator_name(declarator);
    if (!func_name) {
      func_name = "function"; /* Fallback if extraction fails */
    }
    ASTNode *func =
        ast_create_function_decl(func_name, decl_specs, NULL, 0, body, loc);
    /* Attach declarator as child to preserve it */
    if (declarator) {
      ast_add_child(func, declarator);
    }
    return func;
  }
  
  /* Check if this is a function declaration (prototype) */
  if (declarator && declarator->type == AST_FUNCTION_TYPE) {
    /* Function prototype without body */
    const char *func_name = c_extract_declarator_name(declarator);
    if (!func_name) {
      func_name = "function"; /* Fallback */
    }
    ASTNode *func =
        ast_create_function_decl(func_name, decl_specs, NULL, 0, NULL, loc);
    /* Attach declarator as child to preserve it */
    if (declarator) {
      ast_add_child(func, declarator);
    }
    
    /* Expect semicolon */
    if (!MATCH(parser, TOKEN_SEMICOLON)) {
      ERROR(parser, "expected ';' after function declaration");
    }
    
    return func;
  }

  /* Variable declaration - handle multiple declarators */
  ASTNode *var_list = ast_create_node(AST_DECL_STMT, loc);

  /* Process first declarator */
  if (declarator) {
    ASTNode *init = NULL;
    if (MATCH(parser, TOKEN_EQUAL)) {
      init = c_parse_initializer(parser);
    } else if (CHECK(parser, TOKEN_LBRACE)) {
      /* Compound literal without explicit = */
      init = c_parse_initializer(parser);
    }

    const char *var_name = c_extract_declarator_name(declarator);
    if (!var_name) {
      var_name = "variable"; /* Fallback */
    }
    ASTNode *var = ast_create_var_decl(var_name, decl_specs, init, loc);
    ast_add_child(var, declarator);
    ast_add_child(var_list, var);
  }

  /* Handle additional declarators separated by commas */
  while (MATCH(parser, TOKEN_COMMA)) {
    ASTNode *additional_declarator = c_parse_declarator(parser);
    c_parse_gcc_extensions(parser);

    if (additional_declarator) {
      /* If this is a typedef, register additional names */
      if (is_typedef) {
        const char *typedef_name =
            c_extract_declarator_name(additional_declarator);
        if (typedef_name) {
          c_parser_add_typedef(parser, typedef_name);
        }
      }

      ASTNode *init = NULL;
      if (MATCH(parser, TOKEN_EQUAL)) {
        init = c_parse_initializer(parser);
      } else if (CHECK(parser, TOKEN_LBRACE)) {
        /* Compound literal without explicit = */
        init = c_parse_initializer(parser);
      }

      const char *var_name = c_extract_declarator_name(additional_declarator);
      if (!var_name) {
        var_name = "variable"; /* Fallback */
      }
      ASTNode *var = ast_create_var_decl(var_name, decl_specs, init, loc);
      ast_add_child(var, additional_declarator);
      ast_add_child(var_list, var);
    }
  }

  /* Expect semicolon, but handle common error cases gracefully */

  if (!MATCH(parser, TOKEN_SEMICOLON)) {
    if (CHECK(parser, TOKEN_LBRACE)) {
      /* Found { instead of ; - might be a compound statement or initializer */
      /* This could be a parsing context error, try to recover */
      ERROR(parser, "expected ';' after declaration, found '{'");
      /* Skip the compound statement to recover */
      int brace_count = 1;
      ADVANCE(parser); /* consume { */
      while (!AT_END(parser) && brace_count > 0) {
        if (CHECK(parser, TOKEN_LBRACE)) {
          brace_count++;
        } else if (CHECK(parser, TOKEN_RBRACE)) {
          brace_count--;
        }
        ADVANCE(parser);
      }
    } else {
      /* Other unexpected token */
      ERROR(parser, "expected ';' after declaration");
      /* Try to find next semicolon or declaration starter */
      while (!AT_END(parser) && !CHECK(parser, TOKEN_SEMICOLON) &&
             !c_is_declaration_specifier(parser)) {
        ADVANCE(parser);
      }
      if (CHECK(parser, TOKEN_SEMICOLON)) {
        ADVANCE(parser);
      }
    }
  }

  return var_list;
}

ASTNode *c_parse_declaration_specifiers(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;
  ASTNode *specs = ast_create_node(AST_TYPE, loc);



  /* Parse all declaration specifiers */
  int spec_count = 0;
  while (c_is_declaration_specifier(parser)) {
    spec_count++;

    if (c_is_storage_class_specifier(parser)) {
      /* auto, register, static, extern, typedef */
      ADVANCE(parser);
    } else if (c_is_type_specifier(parser)) {
      ASTNode *type = c_parse_type_specifier(parser);
      if (type) {
        ast_add_child(specs, type);
      }
    } else if (c_is_type_qualifier(parser)) {
      /* const, volatile, restrict, _Atomic */
      ADVANCE(parser);
    } else if (c_is_function_specifier(parser)) {
      /* inline, _Noreturn */
      ADVANCE(parser);
    } else {
      break;
    }
  }


  return specs;
}

ASTNode *c_parse_declarator(CParser *parser) {
  /* pointer* direct-declarator */
  ASTNode *pointer = c_parse_pointer(parser);
  ASTNode *direct = c_parse_direct_declarator(parser);

  if (pointer && direct) {
    ast_add_child(pointer, direct);
    return pointer;
  }

  return direct ? direct : pointer;
}

ASTNode *c_parse_direct_declarator(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;
  ASTNode *declarator = NULL;

  if (CHECK(parser, TOKEN_IDENTIFIER)) {
    declarator = ast_create_identifier(CURRENT(parser)->lexeme, loc);
    ADVANCE(parser);
  } else if (CHECK(parser, TOKEN_LPAREN)) {
    /* Could be:
     * 1. Parenthesized declarator: (declarator)
     * 2. Function parameters: (params) - but this is handled in postfix below
     *
     * Disambiguate: Look ahead to see if this is a parenthesized declarator
     * vs function parameters. Parenthesized declarators typically have:
     * - * (pointer)
     * - another ( (nested)
     * BUT NOT a type specifier (which would indicate parameters)
     */
    Token *next = PEEK(parser, 1);
    if (next && (next->type == TOKEN_STAR || next->type == TOKEN_LPAREN)) {
      /* Likely parenthesized declarator */
      ADVANCE(parser); /* consume ( */
      declarator = c_parse_declarator(parser);
      EXPECT(parser, TOKEN_RPAREN, "expected ')' after declarator");
    }
    /* Otherwise leave the ( for the function declarator parsing below */
  }
  /* Note: declarator can be NULL for abstract declarators (e.g., in function
   * parameters) */

  /* Postfix: arrays and functions */
  while (true) {
    if (MATCH(parser, TOKEN_LBRACKET)) {
      /* Array declarator - can exist without identifier (abstract declarator)
       */
      ASTNode *size = NULL;
      if (!CHECK(parser, TOKEN_RBRACKET)) {
        size = c_parse_assignment_expression(parser);
      }
      EXPECT(parser, TOKEN_RBRACKET, "expected ']' after array size");

      ASTNode *array = ast_create_array_type(declarator, size, loc);
      declarator = array;

    } else if (CHECK(parser, TOKEN_LPAREN)) {
      /* Function declarator - but only if we haven't already parsed params
       * above */
      /* This handles cases like: int (*func)(int, int) */
      ADVANCE(parser); /* consume ( */
      ASTNode *params = NULL;
      if (!CHECK(parser, TOKEN_RPAREN)) {
        params = c_parse_parameter_list(parser);
      }
      EXPECT(parser, TOKEN_RPAREN, "expected ')' after parameters");

      /* === CORRECTED LOGIC START === */
      /* Create a new function type node, wrapping the existing declarator
       * (which might be NULL) */
      ASTNode *func_type = ast_create_node(AST_FUNCTION_TYPE, loc);

      /* The declarator (e.g., the function name) becomes a child of the
       * function type */
      if (declarator) {
        ast_add_child(func_type, declarator);
      }

      /* The parameters also become a child of the function type */
      if (params) {
        ast_add_child(func_type, params);
      }

      /* CRITICAL: Assign the new, complete node back to the declarator */
      declarator = func_type;
      /* === CORRECTED LOGIC END === */
    } else {
      break;
    }
  }

  return declarator;
}

ASTNode *c_parse_pointer(CParser *parser) {
  if (!MATCH(parser, TOKEN_STAR)) {
    return NULL;
  }

  SourceLocation loc = CURRENT(parser)->location;
  ASTNode *pointer = ast_create_node(AST_POINTER_TYPE, loc);

  /* Type qualifiers (const, volatile, restrict) */
  while (c_is_type_qualifier(parser)) {
    ADVANCE(parser);
  }

  /* Nested pointer */
  ASTNode *nested = c_parse_pointer(parser);
  if (nested) {
    ast_add_child(pointer, nested);
  }

  return pointer;
}

ASTNode *c_parse_parameter_list(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;
  ASTNode *list = ast_create_node(AST_PARAM_LIST, loc);
  
  /* Mark as non-variadic by default */
  list->data.int_literal.value = 0;

  do {
    /* Check for ... (variadic) */
    if (MATCH(parser, TOKEN_ELLIPSIS)) {
      /* Variadic function - mark the param list */
      list->data.int_literal.value = 1;  /* Use int_literal as a flag */
      break;
    }

    ASTNode *param = c_parse_parameter_declaration(parser);
    if (param) {
      ast_add_child(list, param);
    }

    if (!MATCH(parser, TOKEN_COMMA)) {
      break;
    }
  } while (true);

  return list;
}

ASTNode *c_parse_parameter_declaration(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;

  ASTNode *specs = c_parse_declaration_specifiers(parser);
  ASTNode *declarator = c_parse_declarator(parser);

  /* Extract parameter name from declarator */
  const char *param_name =
      "param"; /* Simplified - would extract from declarator */
  ASTNode *param = ast_create_param_decl(param_name, specs, loc);

  /* Attach declarator as child to preserve it */
  if (declarator) {
    ast_add_child(param, declarator);
  }

  return param;
}

ASTNode *c_parse_initializer(CParser *parser) {
  if (MATCH(parser, TOKEN_LBRACE)) {
    /* Aggregate initializer: { ... } */
    ASTNode *list = c_parse_initializer_list(parser);
    MATCH(parser, TOKEN_COMMA); /* Optional trailing comma */
    EXPECT(parser, TOKEN_RBRACE, "expected '}' after initializer list");
    return list;
  }

  /* Single expression initializer */
  return c_parse_assignment_expression(parser);
}

ASTNode *c_parse_initializer_list(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;
  ASTNode *list = ast_create_node(AST_INIT_LIST_EXPR, loc);

  do {
    /* C99 designated initializers: .member = or [index] = */
    if (MATCH(parser, TOKEN_DOT)) {
      EXPECT(parser, TOKEN_IDENTIFIER, "expected member name");
      EXPECT(parser, TOKEN_EQUAL, "expected '=' after designator");
    } else if (MATCH(parser, TOKEN_LBRACKET)) {
      c_parse_constant_expression(parser);
      EXPECT(parser, TOKEN_RBRACKET, "expected ']' after array index");
      EXPECT(parser, TOKEN_EQUAL, "expected '=' after designator");
    }

    ASTNode *init = c_parse_initializer(parser);
    if (init) {
      ast_add_child(list, init);
    }

    if (!MATCH(parser, TOKEN_COMMA)) {
      break;
    }

    /* Check for trailing comma before } */
    if (CHECK(parser, TOKEN_RBRACE)) {
      break;
    }
  } while (true);

  return list;
}

/* ===== TYPE SPECIFIERS ===== */

ASTNode *c_parse_type_specifier(CParser *parser) {
  Token *token = CURRENT(parser);
  SourceLocation loc = token->location;

  switch (token->type) {
  /* Basic types → LLVM: i8, i16, i32, i64, float, double */
  case TOKEN_VOID:
    ADVANCE(parser);
    return ast_create_type("void", loc);

  case TOKEN_CHAR:
    ADVANCE(parser);
    return ast_create_type("char", loc);

  case TOKEN_SHORT:
    ADVANCE(parser);
    return ast_create_type("short", loc);

  case TOKEN_INT:
    ADVANCE(parser);
    return ast_create_type("int", loc);

  case TOKEN_LONG:
    ADVANCE(parser);
    return ast_create_type("long", loc);

  case TOKEN_FLOAT:
    ADVANCE(parser);
    return ast_create_type("float", loc);

  case TOKEN_DOUBLE:
    ADVANCE(parser);
    return ast_create_type("double", loc);

  case TOKEN__FLOAT32:
    ADVANCE(parser);
    return ast_create_type("_Float32", loc);

  case TOKEN__FLOAT64:
    ADVANCE(parser);
    return ast_create_type("_Float64", loc);

  case TOKEN__FLOAT128:
    ADVANCE(parser);
    return ast_create_type("_Float128", loc);

  /* GCC built-in types */
  case TOKEN___UINT8_T:
    ADVANCE(parser);
    return ast_create_type("__uint8_t", loc);

  case TOKEN___UINT16_T:
    ADVANCE(parser);
    return ast_create_type("__uint16_t", loc);

  case TOKEN___UINT32_T:
    ADVANCE(parser);
    return ast_create_type("__uint32_t", loc);

  case TOKEN___UINT64_T:
    ADVANCE(parser);
    return ast_create_type("__uint64_t", loc);

  case TOKEN___INT8_T:
    ADVANCE(parser);
    return ast_create_type("__int8_t", loc);

  case TOKEN___INT16_T:
    ADVANCE(parser);
    return ast_create_type("__int16_t", loc);

  case TOKEN___INT32_T:
    ADVANCE(parser);
    return ast_create_type("__int32_t", loc);

  case TOKEN___INT64_T:
    ADVANCE(parser);
    return ast_create_type("__int64_t", loc);

  case TOKEN___INT128:
    ADVANCE(parser);
    return ast_create_type("__int128", loc);

  case TOKEN___UINT128_T:
    ADVANCE(parser);
    return ast_create_type("__uint128_t", loc);

  case TOKEN___SIZE_T:
    ADVANCE(parser);
    return ast_create_type("__size_t", loc);

  case TOKEN___SSIZE_T:
    ADVANCE(parser);
    return ast_create_type("__ssize_t", loc);

  case TOKEN___PTRDIFF_T:
    ADVANCE(parser);
    return ast_create_type("__ptrdiff_t", loc);

  case TOKEN___INTPTR_T:
    ADVANCE(parser);
    return ast_create_type("__intptr_t", loc);

  case TOKEN___UINTPTR_T:
    ADVANCE(parser);
    return ast_create_type("__uintptr_t", loc);

  case TOKEN___WCHAR_T:
    ADVANCE(parser);
    return ast_create_type("__wchar_t", loc);

  case TOKEN___WINT_T:
    ADVANCE(parser);
    return ast_create_type("__wint_t", loc);

  case TOKEN___INTMAX_T:
    ADVANCE(parser);
    return ast_create_type("__intmax_t", loc);

  case TOKEN___UINTMAX_T:
    ADVANCE(parser);
    return ast_create_type("__uintmax_t", loc);

  case TOKEN_SIGNED:
    ADVANCE(parser);
    return ast_create_type("signed", loc);

  case TOKEN_UNSIGNED:
    ADVANCE(parser);
    return ast_create_type("unsigned", loc);

  case TOKEN__BOOL:
    ADVANCE(parser);
    return ast_create_type("_Bool", loc);

  case TOKEN_BOOL:
    ADVANCE(parser);
    return ast_create_type("bool", loc);

  case TOKEN_SIZE_T:
    ADVANCE(parser);
    return ast_create_type("size_t", loc);

  case TOKEN_SSIZE_T:
    ADVANCE(parser);
    return ast_create_type("ssize_t", loc);

  case TOKEN_PTRDIFF_T:
    ADVANCE(parser);
    return ast_create_type("ptrdiff_t", loc);

  case TOKEN_TVALUE:
    ADVANCE(parser);
    return ast_create_type("TValue", loc);

  case TOKEN__COMPLEX:
    ADVANCE(parser);
    return ast_create_type("_Complex", loc);

  case TOKEN__IMAGINARY:
    ADVANCE(parser);
    return ast_create_type("_Imaginary", loc);

  /* Aggregate types → LLVM: struct type */
  case TOKEN_STRUCT:
  case TOKEN_UNION:
    return c_parse_struct_or_union_specifier(parser);

  /* Enum → LLVM: i32 */
  case TOKEN_ENUM:
    return c_parse_enum_specifier(parser);

  /* Typedef name */
  case TOKEN_IDENTIFIER:
    if (c_is_type_name(parser, token->lexeme)) {
      ADVANCE(parser);
      return ast_create_type(token->lexeme, loc);
    }
    break;

  /* GNU/C23 extensions */
  case TOKEN___TYPEOF__:
  case TOKEN_TYPEOF:
    return c_parse_typeof(parser);

  case TOKEN__ATOMIC:
    if (parser->standard >= C_STD_C11) {
      return c_parse_atomic_type_specifier(parser);
    }
    break;

  default:
    break;
  }

  return NULL;
}

ASTNode *c_parse_struct_or_union_specifier(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;
  bool is_union = CHECK(parser, TOKEN_UNION);
  ADVANCE(parser); /* struct or union */

  /* Optional tag */
  char *tag = NULL;
  if (CHECK(parser, TOKEN_IDENTIFIER) || CHECK(parser, TOKEN_TVALUE)) {
    tag = xstrdup(CURRENT(parser)->lexeme);
    ADVANCE(parser);
  }

  /* Optional body */
  if (MATCH(parser, TOKEN_LBRACE)) {
    ASTNode *body = c_parse_struct_declaration_list(parser);
    EXPECT(parser, TOKEN_RBRACE, "expected '}' after struct body");

    ASTNode *node =
        ast_create_node(is_union ? AST_UNION_DECL : AST_STRUCT_DECL, loc);
    if (tag) {
      node->data.identifier.name = tag;
    }
    if (body) {
      ast_add_child(node, body);
    }
    return node;
  }

  if (!tag) {
    ERROR(parser, "expected struct tag or body");
    return NULL;
  }

  ASTNode *node =
      ast_create_node(is_union ? AST_UNION_TYPE : AST_STRUCT_TYPE, loc);
  node->data.identifier.name = tag;
  return node;
}

ASTNode *c_parse_struct_declaration_list(CParser *parser) {
  Token *current = CURRENT(parser);  /* Cache token lookup */
  SourceLocation loc = current->location;
  ASTNode *list = ast_create_node(AST_COMPOUND_STMT, loc);

  while (true) {
    current = CURRENT(parser);  /* Cache once per iteration */
    if (current->type == TOKEN_RBRACE || current->type == TOKEN_EOF) break;
    
    Token *before = current;
    ASTNode *decl = c_parse_struct_declaration(parser);
    Token *after = CURRENT(parser);

    if (decl) {
      ast_add_child(list, decl);
    } else if (before == after) {
      /* No progress made - avoid infinite loop */
      ERROR(parser, "failed to parse struct member");
      /* Skip to next semicolon or closing brace */
      while (!CHECK(parser, TOKEN_SEMICOLON) && !CHECK(parser, TOKEN_RBRACE) &&
             !CHECK(parser, TOKEN_EOF)) {
        ADVANCE(parser);
      }
      if (CHECK(parser, TOKEN_SEMICOLON)) {
        ADVANCE(parser);
      }
    }
  }

  return list;
}

ASTNode *c_parse_struct_declaration(CParser *parser) {
  /* Similar to regular declaration but for struct members */

  /* Handle GCC extensions at the start of struct members */
  if (CHECK(parser, TOKEN___EXTENSION__)) {
    ADVANCE(parser); /* consume __extension__ */
  }

  ASTNode *specs = c_parse_declaration_specifiers(parser);

  if (!specs) {
    /* Failed to parse declaration specifiers */
    return NULL;
  }

  /* Check for anonymous struct/union (no declarator) */
  if (CHECK(parser, TOKEN_SEMICOLON)) {
    ADVANCE(parser);
    return specs;
  }

  /* Parse declarators (can be multiple) */
  int declarator_count = 0;
  do {
    Token *before_declarator = CURRENT(parser);
    ASTNode *declarator = c_parse_declarator(parser);
    Token *after_declarator = CURRENT(parser);

    if (declarator) {
      declarator_count++;
      /* Attach declarator to specs */
      if (specs) {
        ast_add_child(specs, declarator);
      } else {
        ast_destroy_node(declarator);
      }
    } else {
      /* No declarator parsed */
      if (before_declarator == after_declarator &&
          !CHECK(parser, TOKEN_SEMICOLON)) {
        /* No tokens consumed and not at semicolon - infinite loop risk! */
        ERROR(parser, "expected declarator in struct member");
        /* Try to recover by skipping to semicolon or closing brace */
        while (!CHECK(parser, TOKEN_SEMICOLON) &&
               !CHECK(parser, TOKEN_RBRACE) && !CHECK(parser, TOKEN_EOF)) {
          ADVANCE(parser);
        }
        break;
      } else if (before_declarator != after_declarator) {
        /* Tokens were consumed but no declarator returned - parsing failed */
        /* This can happen with complex syntax errors */
        ERROR(parser, "failed to parse declarator in struct member");
        /* Try to recover by skipping to semicolon or closing brace */
        while (!CHECK(parser, TOKEN_SEMICOLON) &&
               !CHECK(parser, TOKEN_RBRACE) && !CHECK(parser, TOKEN_EOF)) {
          ADVANCE(parser);
        }
        break;
      }
      /* If we're at a semicolon, that's fine - no declarator needed for
       * anonymous types */
    }

    /* Parse any GCC attributes after declarator */
    c_parse_gcc_extensions(parser);

    /* Optional bitfield */
    if (MATCH(parser, TOKEN_COLON)) {
      c_parse_constant_expression(parser);
    }

    /* Parse any GCC attributes after bitfield */
    c_parse_gcc_extensions(parser);

    if (!MATCH(parser, TOKEN_COMMA)) {
      break;
    }

    /* Safety check: if we've looped many times without progress, break */
    if (declarator_count > 100) {
      ERROR(parser, "too many declarators in struct member");
      break;
    }
  } while (true);

  EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after struct member");

  return specs;
}

ASTNode *c_parse_enum_specifier(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;
  ADVANCE(parser); /* enum */

  /* Optional tag */
  char *tag = NULL;
  if (CHECK(parser, TOKEN_IDENTIFIER)) {
    tag = xstrdup(CURRENT(parser)->lexeme);
    ADVANCE(parser);
  }

  /* Optional body */
  if (MATCH(parser, TOKEN_LBRACE)) {
    ASTNode *body = c_parse_enumerator_list(parser);
    MATCH(parser, TOKEN_COMMA); /* Optional trailing comma */
    EXPECT(parser, TOKEN_RBRACE, "expected '}' after enum body");

    ASTNode *node = ast_create_node(AST_ENUM_DECL, loc);
    if (tag) {
      node->data.identifier.name = tag;
    }
    if (body) {
      ast_add_child(node, body);
    }
    return node;
  }

  if (!tag) {
    ERROR(parser, "expected enum tag or body");
    return NULL;
  }

  ASTNode *node = ast_create_node(AST_ENUM_TYPE, loc);
  node->data.identifier.name = tag;
  return node;
}

ASTNode *c_parse_enumerator_list(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;
  ASTNode *list = ast_create_node(AST_COMPOUND_STMT, loc);

  do {
    ASTNode *enumerator = c_parse_enumerator(parser);
    if (enumerator) {
      ast_add_child(list, enumerator);
    }

    if (!MATCH(parser, TOKEN_COMMA)) {
      break;
    }

    /* Check for trailing comma before } */
    if (CHECK(parser, TOKEN_RBRACE)) {
      break;
    }
  } while (true);

  return list;
}

ASTNode *c_parse_enumerator(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;

  if (!CHECK(parser, TOKEN_IDENTIFIER)) {
    ERROR(parser, "expected enumerator name");
    return NULL;
  }

  char *name = xstrdup(CURRENT(parser)->lexeme);
  ADVANCE(parser);

  ASTNode *value = NULL;
  if (MATCH(parser, TOKEN_EQUAL)) {
    value = c_parse_constant_expression(parser);
  }

  ASTNode *node = ast_create_node(AST_ENUM_CONSTANT, loc);
  node->data.identifier.name = name;
  if (value) {
    ast_add_child(node, value);
  }

  return node;
}

ASTNode *c_parse_type_qualifier(CParser *parser) {
  /* Type qualifiers don't create nodes, just consumed */
  if (c_is_type_qualifier(parser)) {
    ADVANCE(parser);
  }
  return NULL;
}

ASTNode *c_parse_type_qualifier_list(CParser *parser) {
  while (c_is_type_qualifier(parser)) {
    ADVANCE(parser);
  }
  return NULL;
}

ASTNode *c_parse_storage_class_specifier(CParser *parser) {
  /* Storage class specifiers don't create nodes, just consumed */
  if (c_is_storage_class_specifier(parser)) {
    ADVANCE(parser);
  }
  return NULL;
}

ASTNode *c_parse_function_specifier(CParser *parser) {
  /* Function specifiers don't create nodes, just consumed */
  if (c_is_function_specifier(parser)) {
    ADVANCE(parser);
  }
  return NULL;
}

/* ===== STATEMENTS ===== */

ASTNode *c_parse_statement(CParser *parser) {
  Token *token = CURRENT(parser);

  switch (token->type) {
  case TOKEN_IF:
  case TOKEN_SWITCH:
    return c_parse_selection_statement(parser);

  case TOKEN_WHILE:
  case TOKEN_DO:
  case TOKEN_FOR:
    return c_parse_iteration_statement(parser);

  case TOKEN_GOTO:
  case TOKEN_CONTINUE:
  case TOKEN_BREAK:
  case TOKEN_RETURN:
    return c_parse_jump_statement(parser);

  case TOKEN_LBRACE:
    return c_parse_compound_statement(parser);

  case TOKEN_CASE:
  case TOKEN_DEFAULT:
    return c_parse_labeled_statement(parser);

  case TOKEN___ASM__:
  case TOKEN_ASM:
    return c_parse_asm_statement(parser);

  case TOKEN___ATTRIBUTE__:
    /* Handle standalone attributes like __attribute__((fallthrough)); */
    c_parse_gcc_extensions(parser);
    EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after attribute");
    return ast_create_node(AST_NULL_STMT, token->location);

  case TOKEN_IDENTIFIER:
    /* Could be label or expression */
    if (PEEK(parser, 1) && PEEK(parser, 1)->type == (TokenType)TOKEN_COLON) {
      return c_parse_labeled_statement(parser);
    }
    /* Fall through to expression statement */
    __attribute__((fallthrough));

  default:
    return c_parse_expression_statement(parser);
  }
}

ASTNode *c_parse_labeled_statement(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;

  if (MATCH(parser, TOKEN_CASE)) {
    /* case → LLVM: switch case */
    ASTNode *expr = c_parse_constant_expression(parser);
    EXPECT(parser, TOKEN_COLON, "expected ':' after case value");
    ASTNode *stmt = c_parse_statement(parser);

    ASTNode *case_stmt = ast_create_node(AST_CASE_STMT, loc);
    if (expr)
      ast_add_child(case_stmt, expr);
    if (stmt)
      ast_add_child(case_stmt, stmt);
    return case_stmt;

  } else if (MATCH(parser, TOKEN_DEFAULT)) {
    /* default → LLVM: switch default */
    EXPECT(parser, TOKEN_COLON, "expected ':' after 'default'");
    ASTNode *stmt = c_parse_statement(parser);

    ASTNode *default_stmt = ast_create_node(AST_DEFAULT_STMT, loc);
    if (stmt)
      ast_add_child(default_stmt, stmt);
    return default_stmt;

  } else if (CHECK(parser, TOKEN_IDENTIFIER)) {
    /* label: → LLVM: basic block label */
    char *label = xstrdup(CURRENT(parser)->lexeme);
    ADVANCE(parser);
    EXPECT(parser, TOKEN_COLON, "expected ':' after label");
    ASTNode *stmt = c_parse_statement(parser);

    ASTNode *label_stmt = ast_create_node(AST_LABEL_STMT, loc);
    label_stmt->data.identifier.name = label;
    if (stmt)
      ast_add_child(label_stmt, stmt);
    return label_stmt;
  }

  return NULL;
}

ASTNode *c_parse_compound_statement(CParser *parser) {
  /* { ... } → LLVM: basic block */
  SourceLocation loc = CURRENT(parser)->location;
  EXPECT(parser, TOKEN_LBRACE, "expected '{'");

  c_parser_enter_scope(parser);

  ASTNode *compound = ast_create_compound_stmt(loc);

  while (!CHECK(parser, TOKEN_RBRACE) && !AT_END(parser)) {
    ASTNode *stmt = NULL;

    /* Cache declaration specifier check to avoid repeated function calls */
    bool is_decl = c_is_declaration_specifier(parser);
    
    /* Could be declaration or statement */
    if (is_decl) {
      stmt = c_parse_declaration(parser);
    } else {
      stmt = c_parse_statement(parser);
    }

    if (stmt) {
      ast_add_child(compound, stmt);
    }

    if (parser->base.panic_mode) {
      parser_synchronize(&parser->base);
    }
  }

  EXPECT(parser, TOKEN_RBRACE, "expected '}'");

  c_parser_exit_scope(parser);

  return compound;
}

ASTNode *c_parse_expression_statement(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;

  /* Empty statement: ; */
  if (MATCH(parser, TOKEN_SEMICOLON)) {
    return ast_create_node(AST_NULL_STMT, loc);
  }

  /* Expression followed by semicolon */
  ASTNode *expr = c_parse_expression(parser);
  EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after expression");

  return ast_create_expr_stmt(expr, loc);
}

ASTNode *c_parse_selection_statement(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;

  if (MATCH(parser, TOKEN_IF)) {
    /* if → LLVM: br instruction with basic blocks */
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after 'if'");
    ASTNode *condition = c_parse_expression(parser);
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after condition");

    ASTNode *then_branch = c_parse_statement(parser);
    ASTNode *else_branch = NULL;

    if (MATCH(parser, TOKEN_ELSE)) {
      else_branch = c_parse_statement(parser);
    }

    return ast_create_if_stmt(condition, then_branch, else_branch, loc);

  } else if (MATCH(parser, TOKEN_SWITCH)) {
    /* switch → LLVM: switch instruction */
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after 'switch'");
    ASTNode *expr = c_parse_expression(parser);
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after expression");

    ASTNode *body = c_parse_statement(parser);

    ASTNode *switch_stmt = ast_create_node(AST_SWITCH_STMT, loc);
    if (expr)
      ast_add_child(switch_stmt, expr);
    if (body)
      ast_add_child(switch_stmt, body);

    return switch_stmt;
  }

  return NULL;
}

ASTNode *c_parse_iteration_statement(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;

  if (MATCH(parser, TOKEN_WHILE)) {
    /* while → LLVM: loop with basic blocks */
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after 'while'");
    ASTNode *condition = c_parse_expression(parser);
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after condition");

    ASTNode *body = c_parse_statement(parser);

    return ast_create_while_stmt(condition, body, loc);

  } else if (MATCH(parser, TOKEN_DO)) {
    /* do-while → LLVM: loop with basic blocks */
    ASTNode *body = c_parse_statement(parser);
    EXPECT(parser, TOKEN_WHILE, "expected 'while' after do body");
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after 'while'");
    ASTNode *condition = c_parse_expression(parser);
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after condition");
    EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after do-while");

    ASTNode *do_while = ast_create_node(AST_DO_WHILE_STMT, loc);
    if (condition)
      ast_add_child(do_while, condition);
    if (body)
      ast_add_child(do_while, body);

    return do_while;

  } else if (MATCH(parser, TOKEN_FOR)) {
    /* for → LLVM: loop with basic blocks */
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after 'for'");

    ASTNode *init = NULL;
    if (!CHECK(parser, TOKEN_SEMICOLON)) {
      /* Cache declaration specifier check */
      bool is_decl = c_is_declaration_specifier(parser);
      if (is_decl) {
        init = c_parse_declaration(parser);
      } else {
        init = c_parse_expression(parser);
        EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after for init");
      }
    } else {
      ADVANCE(parser);
    }

    ASTNode *condition = NULL;
    if (!CHECK(parser, TOKEN_SEMICOLON)) {
      condition = c_parse_expression(parser);
    }
    EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after for condition");

    ASTNode *increment = NULL;
    if (!CHECK(parser, TOKEN_RPAREN)) {
      increment = c_parse_expression(parser);
    }
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after for clauses");

    ASTNode *body = c_parse_statement(parser);

    return ast_create_for_stmt(init, condition, increment, body, loc);
  }

  return NULL;
}

ASTNode *c_parse_jump_statement(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;

  if (MATCH(parser, TOKEN_GOTO)) {
    /* goto → LLVM: br to labeled block */
    if (CHECK(parser, TOKEN_STAR)) {
      /* Computed goto: goto *expression; */
      ADVANCE(parser); /* consume * */
      ASTNode *expr = c_parse_expression(parser);
      EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after computed goto");
      if (expr)
        ast_destroy_node(expr); /* Clean up for now */
      return ast_create_node(AST_GOTO_STMT, loc);
    } else {
      /* Regular goto: goto label; */
      EXPECT(parser, TOKEN_IDENTIFIER, "expected label name after 'goto'");
      EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after goto");
      return ast_create_node(AST_GOTO_STMT, loc);
    }

  } else if (MATCH(parser, TOKEN_CONTINUE)) {
    /* continue → LLVM: br to loop header */
    EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after 'continue'");
    return ast_create_node(AST_CONTINUE_STMT, loc);

  } else if (MATCH(parser, TOKEN_BREAK)) {
    /* break → LLVM: br to exit block */
    EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after 'break'");
    return ast_create_node(AST_BREAK_STMT, loc);

  } else if (MATCH(parser, TOKEN_RETURN)) {
    /* return → LLVM: ret instruction */
    ASTNode *expr = NULL;
    if (!CHECK(parser, TOKEN_SEMICOLON)) {
      expr = c_parse_expression(parser);
    }
    EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after return");
    return ast_create_return_stmt(expr, loc);
  }

  return NULL;
}

ASTNode *c_parse_asm_statement(CParser *parser) {
  SourceLocation loc = CURRENT(parser)->location;
  ADVANCE(parser); /* __asm__ or asm */

  bool is_volatile = false;
  if (MATCH(parser, TOKEN_VOLATILE) || MATCH(parser, TOKEN___VOLATILE__)) {
    is_volatile = true;
  }

  EXPECT(parser, TOKEN_LPAREN, "expected '(' after asm");

  /* Assembly string */
  if (!CHECK(parser, TOKEN_STRING_LITERAL)) {
    ERROR(parser, "expected assembly string");
    return NULL;
  }

  char *asm_string = xstrdup(CURRENT(parser)->lexeme);
  ADVANCE(parser);

  /* Optional output/input operands and clobbers */
  if (MATCH(parser, TOKEN_COLON)) {
    /* Output operands */
    if (!CHECK(parser, TOKEN_COLON) && !CHECK(parser, TOKEN_RPAREN)) {
      /* Parse asm output operands */
      c_parse_asm_operands(parser);
    }

    if (MATCH(parser, TOKEN_COLON)) {
      /* Input operands */
      if (!CHECK(parser, TOKEN_COLON) && !CHECK(parser, TOKEN_RPAREN)) {
        /* Parse asm input operands */
        c_parse_asm_operands(parser);
      }

      if (MATCH(parser, TOKEN_COLON)) {
        /* Clobbers - string literals */
        do {
          if (CHECK(parser, TOKEN_STRING_LITERAL)) {
            ADVANCE(parser);
          }
          if (!MATCH(parser, TOKEN_COMMA))
            break;
        } while (true);
      }
    }
  }

  EXPECT(parser, TOKEN_RPAREN, "expected ')' after asm");
  EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after asm statement");

  ASTNode *asm_stmt = ast_create_node(AST_ASM_STMT, loc);
  asm_stmt->data.asm_stmt.asm_string = asm_string;
  asm_stmt->data.asm_stmt.is_volatile = is_volatile;

  return asm_stmt;
}

/* ===== EXPRESSIONS (Precedence Climbing) ===== */

ASTNode *c_parse_expression(CParser *parser) {
  /* Comma operator: evaluate left, discard, evaluate right, return right */
  ASTNode *left = c_parse_assignment_expression(parser);
  if (!left)
    return NULL;

  while (MATCH(parser, TOKEN_COMMA)) {
    /* , → LLVM: evaluate both, return second */
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *right = c_parse_assignment_expression(parser);
    if (!right)
      return left;

    ASTNode *node = ast_create_node(AST_COMMA_EXPR, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    left = node;
  }

  return left;
}

ASTNode *c_parse_assignment_expression(CParser *parser) {
  /* Precedence 1: =, +=, -=, etc. (right-associative) */
  ASTNode *left = c_parse_conditional_expression(parser);
  if (!left)
    return NULL;

  SourceLocation loc = CURRENT(parser)->location;
  ASTNodeType node_type;

  if (MATCH(parser, TOKEN_EQUAL)) {
    /* = → LLVM: store */
    node_type = AST_ASSIGN_EXPR;
  } else if (MATCH(parser, TOKEN_PLUS_EQUAL)) {
    /* += → LLVM: load, add, store */
    node_type = AST_ADD_ASSIGN_EXPR;
  } else if (MATCH(parser, TOKEN_MINUS_EQUAL)) {
    /* -= → LLVM: load, sub, store */
    node_type = AST_SUB_ASSIGN_EXPR;
  } else if (MATCH(parser, TOKEN_STAR_EQUAL)) {
    /* *= → LLVM: load, mul, store */
    node_type = AST_MUL_ASSIGN_EXPR;
  } else if (MATCH(parser, TOKEN_SLASH_EQUAL)) {
    /* /= → LLVM: load, div, store */
    node_type = AST_DIV_ASSIGN_EXPR;
  } else if (MATCH(parser, TOKEN_PERCENT_EQUAL)) {
    /* %= → LLVM: load, rem, store */
    node_type = AST_MOD_ASSIGN_EXPR;
  } else if (MATCH(parser, TOKEN_AMPERSAND_EQUAL)) {
    /* &= → LLVM: load, and, store */
    node_type = AST_AND_ASSIGN_EXPR;
  } else if (MATCH(parser, TOKEN_PIPE_EQUAL)) {
    /* |= → LLVM: load, or, store */
    node_type = AST_OR_ASSIGN_EXPR;
  } else if (MATCH(parser, TOKEN_CARET_EQUAL)) {
    /* ^= → LLVM: load, xor, store */
    node_type = AST_XOR_ASSIGN_EXPR;
  } else if (MATCH(parser, TOKEN_LESS_LESS_EQUAL)) {
    /* <<= → LLVM: load, shl, store */
    node_type = AST_SHL_ASSIGN_EXPR;
  } else if (MATCH(parser, TOKEN_GREATER_GREATER_EQUAL)) {
    /* >>= → LLVM: load, shr, store */
    node_type = AST_SHR_ASSIGN_EXPR;
  } else {
    /* Not an assignment */
    return left;
  }

  /* Right-associative: parse another assignment expression */
  ASTNode *right = c_parse_assignment_expression(parser);
  if (!right)
    return left;

  ASTNode *node = ast_create_node(node_type, loc);
  ast_add_child(node, left);
  ast_add_child(node, right);
  return node;
}

ASTNode *c_parse_conditional_expression(CParser *parser) {
  /* Precedence 2: ?: (right-associative) */
  ASTNode *condition = c_parse_logical_or_expression(parser);
  if (!condition)
    return NULL;

  if (MATCH(parser, TOKEN_QUESTION)) {
    /* ?: → LLVM: select or phi nodes */
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *then_expr = c_parse_expression(parser);
    EXPECT(parser, TOKEN_COLON, "expected ':' in conditional expression");
    ASTNode *else_expr = c_parse_conditional_expression(parser);

    ASTNode *node = ast_create_node(AST_CONDITIONAL_EXPR, loc);
    ast_add_child(node, condition);
    ast_add_child(node, then_expr);
    ast_add_child(node, else_expr);
    return node;
  }

  return condition;
}

ASTNode *c_parse_logical_or_expression(CParser *parser) {
  /* Precedence 3: || (left-associative, short-circuit) */
  ASTNode *left = c_parse_logical_and_expression(parser);
  if (!left)
    return NULL;

  while (MATCH(parser, TOKEN_PIPE_PIPE)) {
    /* || → LLVM: short-circuit with basic blocks */
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *right = c_parse_logical_and_expression(parser);
    if (!right)
      return left;

    ASTNode *node = ast_create_node(AST_LOGICAL_OR_EXPR, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    left = node;
  }

  return left;
}

ASTNode *c_parse_logical_and_expression(CParser *parser) {
  /* Precedence 4: && (left-associative, short-circuit) */
  ASTNode *left = c_parse_inclusive_or_expression(parser);
  if (!left)
    return NULL;

  while (MATCH(parser, TOKEN_AMPERSAND_AMPERSAND)) {
    /* && → LLVM: short-circuit with basic blocks */
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *right = c_parse_inclusive_or_expression(parser);
    if (!right)
      return left;

    ASTNode *node = ast_create_node(AST_LOGICAL_AND_EXPR, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    left = node;
  }

  return left;
}

ASTNode *c_parse_inclusive_or_expression(CParser *parser) {
  /* Precedence 5: | (left-associative) */
  ASTNode *left = c_parse_exclusive_or_expression(parser);
  if (!left)
    return NULL;

  while (MATCH(parser, TOKEN_PIPE)) {
    /* | → LLVM: or */
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *right = c_parse_exclusive_or_expression(parser);
    if (!right)
      return left;

    ASTNode *node = ast_create_node(AST_OR_EXPR, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    left = node;
  }

  return left;
}

ASTNode *c_parse_exclusive_or_expression(CParser *parser) {
  /* Precedence 6: ^ (left-associative) */
  ASTNode *left = c_parse_and_expression(parser);
  if (!left)
    return NULL;

  while (MATCH(parser, TOKEN_CARET)) {
    /* ^ → LLVM: xor */
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *right = c_parse_and_expression(parser);
    if (!right)
      return left;

    ASTNode *node = ast_create_node(AST_XOR_EXPR, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    left = node;
  }

  return left;
}

ASTNode *c_parse_and_expression(CParser *parser) {
  /* Precedence 7: & (left-associative) */
  ASTNode *left = c_parse_equality_expression(parser);
  if (!left)
    return NULL;

  while (MATCH(parser, TOKEN_AMPERSAND)) {
    /* & → LLVM: and */
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *right = c_parse_equality_expression(parser);
    if (!right)
      return left;

    ASTNode *node = ast_create_node(AST_AND_EXPR, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    left = node;
  }

  return left;
}

ASTNode *c_parse_equality_expression(CParser *parser) {
  /* Precedence 8: ==, != (left-associative) */
  ASTNode *left = c_parse_relational_expression(parser);
  if (!left)
    return NULL;

  while (true) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNodeType node_type;

    if (MATCH(parser, TOKEN_EQUAL_EQUAL)) {
      /* == → LLVM: icmp eq or fcmp oeq */
      node_type = AST_EQ_EXPR;
    } else if (MATCH(parser, TOKEN_EXCLAIM_EQUAL)) {
      /* != → LLVM: icmp ne or fcmp one */
      node_type = AST_NE_EXPR;
    } else {
      break;
    }

    ASTNode *right = c_parse_relational_expression(parser);
    if (!right)
      return left;

    ASTNode *node = ast_create_node(node_type, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    left = node;
  }

  return left;
}

ASTNode *c_parse_relational_expression(CParser *parser) {
  /* Precedence 9: <, >, <=, >= (left-associative) */
  ASTNode *left = c_parse_shift_expression(parser);
  if (!left)
    return NULL;

  while (true) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNodeType node_type;

    if (MATCH(parser, TOKEN_LESS)) {
      /* < → LLVM: icmp slt/ult or fcmp olt */
      node_type = AST_LT_EXPR;
    } else if (MATCH(parser, TOKEN_GREATER)) {
      /* > → LLVM: icmp sgt/ugt or fcmp ogt */
      node_type = AST_GT_EXPR;
    } else if (MATCH(parser, TOKEN_LESS_EQUAL)) {
      /* <= → LLVM: icmp sle/ule or fcmp ole */
      node_type = AST_LE_EXPR;
    } else if (MATCH(parser, TOKEN_GREATER_EQUAL)) {
      /* >= → LLVM: icmp sge/uge or fcmp oge */
      node_type = AST_GE_EXPR;
    } else {
      break;
    }

    ASTNode *right = c_parse_shift_expression(parser);
    if (!right)
      return left;

    ASTNode *node = ast_create_node(node_type, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    left = node;
  }

  return left;
}

ASTNode *c_parse_shift_expression(CParser *parser) {
  /* Precedence 10: <<, >> (left-associative) */
  ASTNode *left = c_parse_additive_expression(parser);
  if (!left)
    return NULL;

  while (true) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNodeType node_type;

    if (MATCH(parser, TOKEN_LESS_LESS)) {
      /* << → LLVM: shl */
      node_type = AST_SHL_EXPR;
    } else if (MATCH(parser, TOKEN_GREATER_GREATER)) {
      /* >> → LLVM: lshr (unsigned) or ashr (signed) */
      node_type = AST_SHR_EXPR;
    } else {
      break;
    }

    ASTNode *right = c_parse_additive_expression(parser);
    if (!right)
      return left;

    ASTNode *node = ast_create_node(node_type, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    left = node;
  }

  return left;
}

ASTNode *c_parse_additive_expression(CParser *parser) {
  /* Precedence 11: +, - (left-associative) */
  ASTNode *left = c_parse_multiplicative_expression(parser);
  if (!left)
    return NULL;

  while (true) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNodeType node_type;

    if (MATCH(parser, TOKEN_PLUS)) {
      /* + → LLVM: add (int) or fadd (float) or GEP (pointer) */
      node_type = AST_ADD_EXPR;
    } else if (MATCH(parser, TOKEN_MINUS)) {
      /* - → LLVM: sub (int) or fsub (float) or GEP (pointer) */
      node_type = AST_SUB_EXPR;
    } else {
      break;
    }

    ASTNode *right = c_parse_multiplicative_expression(parser);
    if (!right)
      return left;

    ASTNode *node = ast_create_node(node_type, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    left = node;
  }

  return left;
}

ASTNode *c_parse_multiplicative_expression(CParser *parser) {
  /* Precedence 12: *, /, % (left-associative) */
  ASTNode *left = c_parse_cast_expression(parser);
  if (!left)
    return NULL;

  while (true) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNodeType node_type;

    if (MATCH(parser, TOKEN_STAR)) {
      /* * → LLVM: mul (int) or fmul (float) */
      node_type = AST_MUL_EXPR;
    } else if (MATCH(parser, TOKEN_SLASH)) {
      /* / → LLVM: sdiv/udiv (int) or fdiv (float) */
      node_type = AST_DIV_EXPR;
    } else if (MATCH(parser, TOKEN_PERCENT)) {
      /* % → LLVM: srem/urem (int only) */
      node_type = AST_MOD_EXPR;
    } else {
      break;
    }

    ASTNode *right = c_parse_cast_expression(parser);
    if (!right)
      return left;

    ASTNode *node = ast_create_node(node_type, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    left = node;
  }

  return left;
}

ASTNode *c_parse_cast_expression(CParser *parser) {
  /* Check for cast: (type)expr → LLVM: bitcast/trunc/zext/sext/fptrunc/fpext */
  Token *tok = CURRENT(parser);

  if (CHECK(parser, TOKEN_LPAREN)) {
    /* Could be cast or parenthesized expression - need lookahead */
    /* Save position to restore if not a cast */
    size_t saved_pos = parser->base.position;

    ADVANCE(parser); /* consume ( */

    /* Check if next token is a type specifier */
    if (c_is_type_specifier(parser) || c_is_type_qualifier(parser)) {
      /* Likely a cast - try to parse it */
      SourceLocation loc = CURRENT(parser)->location;



      /* Parse type name (declaration specifiers + optional declarator) */
      ASTNode *type_specs = c_parse_declaration_specifiers(parser);
      ASTNode *declarator = NULL;

      /* Parse optional abstract declarator (for pointer types like void*) */
      if (CHECK(parser, TOKEN_STAR) || CHECK(parser, TOKEN_LBRACKET)) {
        declarator = c_parse_declarator(parser);
      }

      /* Check for closing paren */
      if (CHECK(parser, TOKEN_RPAREN)) {
        ADVANCE(parser); /* consume ) */

        /* Parse the expression being cast */
        ASTNode *expr = c_parse_cast_expression(parser);
        if (expr) {
          /* Combine type specs and declarator */
          ASTNode *complete_type = type_specs;
          if (declarator && complete_type) {
            ast_add_child(complete_type, declarator);
          }
          return ast_create_cast_expr(complete_type, expr, loc);
        }

        /* If expression parsing failed, clean up */
        if (type_specs)
          ast_destroy_node(type_specs);
        if (declarator)
          ast_destroy_node(declarator);
      } else {

        /* Not a valid cast - restore position */
        if (type_specs)
          ast_destroy_node(type_specs);
        if (declarator)
          ast_destroy_node(declarator);
        parser->base.position = saved_pos;
        parser->base.current = token_list_get(parser->base.tokens, saved_pos);
      }
    } else {
      /* Not a type - restore position and parse as parenthesized expr */
      parser->base.position = saved_pos;
      parser->base.current = token_list_get(parser->base.tokens, saved_pos);
    }
  }

  /* Not a cast, parse unary */
  return c_parse_unary_expression(parser);
}

ASTNode *c_parse_unary_expression(CParser *parser) {
  Token *token = CURRENT(parser);
  SourceLocation loc = token->location;

  /* Pre-increment: ++x → LLVM: load, add 1, store, return new value */
  if (MATCH(parser, TOKEN_PLUS_PLUS)) {
    ASTNode *operand = c_parse_unary_expression(parser);
    ASTNode *node = ast_create_node(AST_PRE_INC_EXPR, loc);
    ast_add_child(node, operand);
    return node;
  }

  /* Pre-decrement: --x → LLVM: load, sub 1, store, return new value */
  if (MATCH(parser, TOKEN_MINUS_MINUS)) {
    ASTNode *operand = c_parse_unary_expression(parser);
    ASTNode *node = ast_create_node(AST_PRE_DEC_EXPR, loc);
    ast_add_child(node, operand);
    return node;
  }

  /* Address-of: &x → LLVM: alloca address (no load) */
  if (MATCH(parser, TOKEN_AMPERSAND)) {
    ASTNode *operand = c_parse_cast_expression(parser);
    ASTNode *node = ast_create_node(AST_ADDR_OF_EXPR, loc);
    ast_add_child(node, operand);
    return node;
  }

  /* Dereference: *p → LLVM: load */
  if (MATCH(parser, TOKEN_STAR)) {
    ASTNode *operand = c_parse_cast_expression(parser);
    ASTNode *node = ast_create_node(AST_DEREF_EXPR, loc);
    ast_add_child(node, operand);
    return node;
  }

  /* Unary plus: +x → LLVM: nop (identity) */
  if (MATCH(parser, TOKEN_PLUS)) {
    ASTNode *operand = c_parse_cast_expression(parser);
    ASTNode *node = ast_create_node(AST_UNARY_PLUS_EXPR, loc);
    ast_add_child(node, operand);
    return node;
  }

  /* Unary minus: -x → LLVM: sub 0, x */
  if (MATCH(parser, TOKEN_MINUS)) {
    ASTNode *operand = c_parse_cast_expression(parser);
    ASTNode *node = ast_create_node(AST_UNARY_MINUS_EXPR, loc);
    ast_add_child(node, operand);
    return node;
  }

  /* Bitwise NOT: ~x → LLVM: xor -1, x */
  if (MATCH(parser, TOKEN_TILDE)) {
    ASTNode *operand = c_parse_cast_expression(parser);
    ASTNode *node = ast_create_node(AST_BIT_NOT_EXPR, loc);
    ast_add_child(node, operand);
    return node;
  }

  /* Logical NOT: !x → LLVM: icmp eq 0, x */
  if (MATCH(parser, TOKEN_EXCLAIM)) {
    ASTNode *operand = c_parse_cast_expression(parser);
    ASTNode *node = ast_create_node(AST_NOT_EXPR, loc);
    ast_add_child(node, operand);
    return node;
  }

  /* sizeof: sizeof(type) or sizeof expr → LLVM: constant (compile-time) */
  if (MATCH(parser, TOKEN_SIZEOF)) {
    ASTNode *operand;
    if (CHECK(parser, TOKEN_LPAREN)) {
      /* Could be sizeof(type-name) or sizeof(expression) */
      /* Need to disambiguate by looking ahead */
      size_t saved_pos = parser->base.position;
      ADVANCE(parser); /* consume ( */

      /* Check if next token is a type specifier or qualifier */
      if (c_is_type_specifier(parser) || c_is_type_qualifier(parser)) {
        /* sizeof(type-name) - parse full type including pointers */
        ASTNode *type_specs = c_parse_declaration_specifiers(parser);
        /* Optional abstract declarator for pointers, arrays, etc. */
        if (CHECK(parser, TOKEN_STAR)) {
          ASTNode *pointer = c_parse_pointer(parser);
          if (pointer && type_specs) {
            ast_add_child(type_specs, pointer);
          } else if (pointer) {
            ast_destroy_node(pointer);
          }
        }
        operand = type_specs;
        EXPECT(parser, TOKEN_RPAREN, "expected ')' after sizeof type");
      } else {
        /* sizeof(expression) - restore and parse as expression */
        parser->base.position = saved_pos;
        parser->base.current = token_list_get(parser->base.tokens, saved_pos);
        ADVANCE(parser); /* consume ( again */
        operand = c_parse_unary_expression(parser);
        EXPECT(parser, TOKEN_RPAREN, "expected ')' after sizeof expression");
      }
    } else {
      /* sizeof unary-expression (no parentheses) */
      operand = c_parse_unary_expression(parser);
    }
    return ast_create_sizeof_expr(operand, loc);
  }

  /* _Alignof: _Alignof(type) → LLVM: constant (compile-time) */
  if (MATCH(parser, TOKEN__ALIGNOF) || MATCH(parser, TOKEN___ALIGNOF__)) {
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after _Alignof");
    ASTNode *operand = c_parse_unary_expression(parser);
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after _Alignof");
    ASTNode *node = ast_create_node(AST_ALIGNOF_EXPR, loc);
    ast_add_child(node, operand);
    return node;
  }

  /* Not a unary operator, parse postfix */
  return c_parse_postfix_expression(parser);
}

ASTNode *c_parse_postfix_expression(CParser *parser) {
  ASTNode *expr = c_parse_primary_expression(parser);
  if (!expr)
    return NULL;

  while (true) {
    Token *token = CURRENT(parser);
    SourceLocation loc = token->location;

    if (MATCH(parser, TOKEN_LBRACKET)) {
      /* Array subscript: a[i] → LLVM: GEP + load */
      ASTNode *index = c_parse_expression(parser);
      EXPECT(parser, TOKEN_RBRACKET, "expected ']' after array index");
      expr = ast_create_array_subscript(expr, index, loc);

    } else if (MATCH(parser, TOKEN_LPAREN)) {
      /* Function call: f(args) → LLVM: call instruction */
      size_t arg_count = 0;
      size_t capacity = 16;  /* Pre-allocate for typical case (most functions have <16 args) */
      ASTNode **args = xcalloc(capacity, sizeof(ASTNode *));

      if (!CHECK(parser, TOKEN_RPAREN)) {
        do {
          if (arg_count >= capacity) {
            capacity *= 2;
            args = xrealloc(args, capacity * sizeof(ASTNode *));
          }
          args[arg_count++] = c_parse_assignment_expression(parser);
        } while (MATCH(parser, TOKEN_COMMA));
      }

      EXPECT(parser, TOKEN_RPAREN, "expected ')' after arguments");
      expr = ast_create_call_expr(expr, args, arg_count, loc);
      xfree(args);

    } else if (MATCH(parser, TOKEN_DOT)) {
      /* Member access: s.member → LLVM: GEP */
      if (!CHECK(parser, TOKEN_IDENTIFIER)) {
        ERROR(parser, "expected member name after '.'");
        return expr;
      }
      const char *member = CURRENT(parser)->lexeme;  /* Direct reference - no copy needed */
      ADVANCE(parser);
      expr = ast_create_member_expr(expr, member, false, loc);

    } else if (MATCH(parser, TOKEN_ARROW)) {
      /* Arrow: p->member → LLVM: load + GEP */
      if (!CHECK(parser, TOKEN_IDENTIFIER)) {
        ERROR(parser, "expected member name after '->'");
        return expr;
      }
      const char *member = CURRENT(parser)->lexeme;  /* Direct reference - no copy needed */
      ADVANCE(parser);
      expr = ast_create_member_expr(expr, member, true, loc);

    } else if (MATCH(parser, TOKEN_PLUS_PLUS)) {
      /* Post-increment: x++ → LLVM: load, add 1, store, return old value */
      ASTNode *node = ast_create_node(AST_POST_INC_EXPR, loc);
      ast_add_child(node, expr);
      expr = node;

    } else if (MATCH(parser, TOKEN_MINUS_MINUS)) {
      /* Post-decrement: x-- → LLVM: load, sub 1, store, return old value */
      ASTNode *node = ast_create_node(AST_POST_DEC_EXPR, loc);
      ast_add_child(node, expr);
      expr = node;

    } else {
      break;
    }
  }

  return expr;
}

ASTNode *c_parse_primary_expression(CParser *parser) {
  Token *token = CURRENT(parser);
  SourceLocation loc = token->location;



  switch (token->type) {
  /* Identifier - maps to LLVM load instruction */
  case TOKEN_IDENTIFIER: {
    const char *name = token->lexeme;
    ADVANCE(parser);
    return ast_create_identifier(name, loc);
  }

  /* Integer literal - maps to LLVM constant int */
  case TOKEN_INTEGER_LITERAL: {
    int64_t value = token->value.int_value;
    ADVANCE(parser);
    return ast_create_integer_literal(value, loc);
  }

  /* Float literal - maps to LLVM constant float/double */
  case TOKEN_FLOAT_LITERAL: {
    double value = token->value.float_value;
    ADVANCE(parser);
    return ast_create_float_literal(value, loc);
  }

  /* String literal - maps to LLVM global constant */
  case TOKEN_STRING_LITERAL: {
    /* Use string_value which has quotes removed */
    const char *value = token->value.string_value ? token->value.string_value : token->lexeme;
    ADVANCE(parser);

    /* Handle string literal concatenation (adjacent strings) */
    while (CHECK(parser, TOKEN_STRING_LITERAL)) {
      /* In a real implementation, we'd concatenate the strings */
      /* For now, just consume additional string literals */
      ADVANCE(parser);
    }

    return ast_create_string_literal(value, loc);
  }

  /* Char literal - maps to LLVM constant i8 */
  case TOKEN_CHAR_LITERAL: {
    char value = token->value.char_value;
    ADVANCE(parser);
    return ast_create_char_literal(value, loc);
  }

  /* GCC built-in functions */
  case TOKEN___BUILTIN_OFFSETOF: {
    ADVANCE(parser); /* consume __builtin_offsetof */
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after __builtin_offsetof");

    /* Parse type name */
    ASTNode *type = c_parse_type_specifier(parser);
    EXPECT(parser, TOKEN_COMMA,
           "expected ',' after type in __builtin_offsetof");

    /* Parse member name */
    if (!CHECK(parser, TOKEN_IDENTIFIER)) {
      ERROR(parser, "expected member name in __builtin_offsetof");
      return NULL;
    }
    const char *member = CURRENT(parser)->lexeme;
    ADVANCE(parser);

    EXPECT(parser, TOKEN_RPAREN, "expected ')' after __builtin_offsetof");

    /* Create a simple identifier node for now - could be enhanced later */
    return ast_create_identifier("__builtin_offsetof_result", loc);
  }

  case TOKEN___BUILTIN_VA_ARG: {
    ADVANCE(parser); /* consume __builtin_va_arg */
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after __builtin_va_arg");

    /* Parse va_list argument */
    ASTNode *va_list_arg = c_parse_assignment_expression(parser);
    EXPECT(parser, TOKEN_COMMA,
           "expected ',' after va_list in __builtin_va_arg");

    /* Parse type name */
    ASTNode *type_specs = c_parse_declaration_specifiers(parser);
    ASTNode *declarator = NULL;
    if (CHECK(parser, TOKEN_STAR) || CHECK(parser, TOKEN_LBRACKET)) {
      declarator = c_parse_declarator(parser);
    }

    EXPECT(parser, TOKEN_RPAREN, "expected ')' after __builtin_va_arg");

    /* Clean up parsed nodes */
    if (va_list_arg)
      ast_destroy_node(va_list_arg);
    if (type_specs)
      ast_destroy_node(type_specs);
    if (declarator)
      ast_destroy_node(declarator);

    /* Create a simple identifier node for now - could be enhanced later */
    return ast_create_identifier("__builtin_va_arg_result", loc);
  }

  /* GCC label address (&&label) */
  case TOKEN_AMPERSAND_AMPERSAND: {
    ADVANCE(parser); /* consume && */
    if (!CHECK(parser, TOKEN_IDENTIFIER)) {
      ERROR(parser, "expected label name after &&");
      return NULL;
    }
    const char *label = CURRENT(parser)->lexeme;
    ADVANCE(parser);

    /* Create a simple identifier node for the label address */
    return ast_create_identifier(label, loc);
  }

  /* Parenthesized expression */
  case TOKEN_LPAREN: {

    ADVANCE(parser); /* consume '(' */



    /* Check for GCC statement expression: ({ ... }) */
    if (CHECK(parser, TOKEN_LBRACE)) {
      ADVANCE(parser); /* consume { */

      /* Parse statements until } */
      while (!CHECK(parser, TOKEN_RBRACE) && !AT_END(parser)) {
        ASTNode *stmt = c_parse_statement(parser);
        if (stmt)
          ast_destroy_node(stmt); /* Clean up for now */
      }

      EXPECT(parser, TOKEN_RBRACE, "expected '}' in statement expression");
      EXPECT(parser, TOKEN_RPAREN, "expected ')' after statement expression");

      /* Return a placeholder node */
      return ast_create_identifier("statement_expr_result", loc);
    }

    ASTNode *expr = c_parse_expression(parser);
    if (!expr) {
      /* If expression parsing failed, might be at end or error */

      if (CHECK(parser, TOKEN_RPAREN)) {
        ADVANCE(parser);
      }
      return NULL;
    }
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after expression");
    return expr;
  }

  /* C11 _Generic selection */
  case TOKEN__GENERIC:
    if (parser->standard >= C_STD_C11) {
      return c_parse_generic_selection(parser);
    }
    break;

  default:
    break;
  }

  ERROR(parser, "expected primary expression");
  return NULL;
}

ASTNode *c_parse_argument_expression_list(CParser *parser) {
  /* Parse comma-separated argument list */
  SourceLocation loc = CURRENT(parser)->location;
  ASTNode *list = ast_create_node(AST_COMPOUND_STMT, loc);

  do {
    ASTNode *arg = c_parse_assignment_expression(parser);
    if (arg) {
      ast_add_child(list, arg);
    }

    if (!MATCH(parser, TOKEN_COMMA)) {
      break;
    }
  } while (true);

  return list;
}

ASTNode *c_parse_constant_expression(CParser *parser) {
  /* Constant expressions are just conditional expressions that must be
   * compile-time evaluable */
  return c_parse_conditional_expression(parser);
}

/* ===== C99/C11/C23 SPECIFIC ===== */

ASTNode *c_parse_generic_selection(CParser *parser) {
  /* C11 _Generic: compile-time type selection */
  SourceLocation loc = CURRENT(parser)->location;
  ADVANCE(parser); /* _Generic */

  EXPECT(parser, TOKEN_LPAREN, "expected '(' after _Generic");

  /* Controlling expression */
  ASTNode *expr = c_parse_assignment_expression(parser);
  EXPECT(parser, TOKEN_COMMA, "expected ',' after expression");

  ASTNode *node = ast_create_node(AST_GENERIC_EXPR, loc);
  if (expr)
    ast_add_child(node, expr);

  /* Generic associations */
  do {
    if (MATCH(parser, TOKEN_DEFAULT)) {
      EXPECT(parser, TOKEN_COLON, "expected ':' after default");
      ASTNode *value = c_parse_assignment_expression(parser);
      if (value)
        ast_add_child(node, value);
    } else {
      /* Type name */
      c_parse_type_specifier(parser);
      EXPECT(parser, TOKEN_COLON, "expected ':' after type");
      ASTNode *value = c_parse_assignment_expression(parser);
      if (value)
        ast_add_child(node, value);
    }

    if (!MATCH(parser, TOKEN_COMMA)) {
      break;
    }
  } while (true);

  EXPECT(parser, TOKEN_RPAREN, "expected ')' after _Generic");

  return node;
}

ASTNode *c_parse_static_assert(CParser *parser) {
  /* C11 _Static_assert: compile-time assertion */
  SourceLocation loc = CURRENT(parser)->location;
  ADVANCE(parser); /* _Static_assert */

  EXPECT(parser, TOKEN_LPAREN, "expected '(' after _Static_assert");
  ASTNode *expr = c_parse_constant_expression(parser);
  EXPECT(parser, TOKEN_COMMA, "expected ',' after expression");

  /* Message string */
  if (!CHECK(parser, TOKEN_STRING_LITERAL)) {
    ERROR(parser, "expected string literal");
  } else {
    ADVANCE(parser);
  }

  EXPECT(parser, TOKEN_RPAREN, "expected ')' after _Static_assert");
  EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after _Static_assert");

  ASTNode *node = ast_create_node(AST_STATIC_ASSERT, loc);
  if (expr)
    ast_add_child(node, expr);

  return node;
}

ASTNode *c_parse_alignas_specifier(CParser *parser) {
  /* C11 _Alignas */
  ADVANCE(parser); /* _Alignas */
  EXPECT(parser, TOKEN_LPAREN, "expected '(' after _Alignas");

  /* Can be type or constant expression */
  c_parse_conditional_expression(parser);

  EXPECT(parser, TOKEN_RPAREN, "expected ')' after _Alignas");

  return NULL; /* Consumed, no node created */
}

ASTNode *c_parse_atomic_type_specifier(CParser *parser) {
  /* C11 _Atomic(type) */
  SourceLocation loc = CURRENT(parser)->location;
  ADVANCE(parser); /* _Atomic */

  EXPECT(parser, TOKEN_LPAREN, "expected '(' after _Atomic");
  ASTNode *type = c_parse_type_specifier(parser);
  EXPECT(parser, TOKEN_RPAREN, "expected ')' after type");

  return type;
}

/* ===== GNU EXTENSIONS ===== */

ASTNode *c_parse_attribute(CParser *parser) {
  /* GNU __attribute__ */
  ADVANCE(parser); /* __attribute__ */

  EXPECT(parser, TOKEN_LPAREN, "expected '(' after __attribute__");
  EXPECT(parser, TOKEN_LPAREN, "expected '(' after __attribute__(");

  /* Parse attribute list */
  do {
    if (CHECK(parser, TOKEN_IDENTIFIER)) {
      ADVANCE(parser);

      /* Optional parameters */
      if (MATCH(parser, TOKEN_LPAREN)) {
        /* Skip parameters for now */
        int depth = 1;
        while (depth > 0 && !AT_END(parser)) {
          if (CHECK(parser, TOKEN_LPAREN))
            depth++;
          if (CHECK(parser, TOKEN_RPAREN))
            depth--;
          ADVANCE(parser);
        }
      }
    }

    if (!MATCH(parser, TOKEN_COMMA)) {
      break;
    }
  } while (true);

  EXPECT(parser, TOKEN_RPAREN, "expected ')' after attributes");
  EXPECT(parser, TOKEN_RPAREN, "expected ')' after __attribute__((...)))");

  return NULL; /* Consumed, no node created */
}

ASTNode *c_parse_asm_operands(CParser *parser) {
  /* Parse asm operand list */
  /* Format: "constraint" (expression) */
  do {
    if (CHECK(parser, TOKEN_STRING_LITERAL)) {
      ADVANCE(parser); /* constraint */
      EXPECT(parser, TOKEN_LPAREN, "expected '(' after constraint");
      c_parse_assignment_expression(parser);
      EXPECT(parser, TOKEN_RPAREN, "expected ')' after operand");
    }

    if (!MATCH(parser, TOKEN_COMMA)) {
      break;
    }
  } while (true);

  return NULL;
}

ASTNode *c_parse_typeof(CParser *parser) {
  /* GNU/C23 typeof */
  SourceLocation loc = CURRENT(parser)->location;
  ADVANCE(parser); /* typeof or __typeof__ */

  EXPECT(parser, TOKEN_LPAREN, "expected '(' after typeof");
  ASTNode *expr = c_parse_conditional_expression(parser);
  EXPECT(parser, TOKEN_RPAREN, "expected ')' after typeof");

  /* Return the type of the expression */
  return expr;
}

/* ===== UTILITIES ===== */

bool c_is_type_specifier(CParser *parser) {
  Token *token = CURRENT(parser);
  switch (token->type) {
  case TOKEN_VOID:
  case TOKEN_CHAR:
  case TOKEN_SHORT:
  case TOKEN_INT:
  case TOKEN_LONG:
  case TOKEN_FLOAT:
  case TOKEN_DOUBLE:
  case TOKEN__FLOAT32:
  case TOKEN__FLOAT64:
  case TOKEN__FLOAT128:
  case TOKEN_SIGNED:
  case TOKEN_UNSIGNED:
  case TOKEN__BOOL:
  case TOKEN_BOOL:
  case TOKEN_SIZE_T:
  case TOKEN_SSIZE_T:
  case TOKEN_PTRDIFF_T:
  case TOKEN_TVALUE:
  case TOKEN__COMPLEX:
  case TOKEN__IMAGINARY:
  case TOKEN_STRUCT:
  case TOKEN_UNION:
  case TOKEN_ENUM:
  case TOKEN___TYPEOF__:
  case TOKEN_TYPEOF:
  case TOKEN__ATOMIC:
  case TOKEN___UINT8_T:
  case TOKEN___UINT16_T:
  case TOKEN___UINT32_T:
  case TOKEN___UINT64_T:
  case TOKEN___INT8_T:
  case TOKEN___INT16_T:
  case TOKEN___INT32_T:
  case TOKEN___INT64_T:
  case TOKEN___INT128:
  case TOKEN___UINT128_T:
  case TOKEN___SIZE_T:
  case TOKEN___SSIZE_T:
  case TOKEN___PTRDIFF_T:
  case TOKEN___INTPTR_T:
  case TOKEN___UINTPTR_T:
  case TOKEN___WCHAR_T:
  case TOKEN___WINT_T:
  case TOKEN___INTMAX_T:
  case TOKEN___UINTMAX_T:
    return true;
  case TOKEN_IDENTIFIER:
    /* Check if it's a typedef name */
    return c_is_type_name(parser, token->lexeme);
  default:
    return false;
  }
}

bool c_is_type_qualifier(CParser *parser) {
  Token *token = CURRENT(parser);
  switch (token->type) {
  case TOKEN_CONST:
  case TOKEN_VOLATILE:
  case TOKEN_RESTRICT:
  case TOKEN__ATOMIC:
  case TOKEN___CONST__:
  case TOKEN___VOLATILE__:
  case TOKEN___RESTRICT__:
    return true;
  case TOKEN_IDENTIFIER:
    /* GCC extensions that might be lexed as identifiers */
    if (token->lexeme) {
      if (strcmp(token->lexeme, "__restrict") == 0 ||
          strcmp(token->lexeme, "__const") == 0 ||
          strcmp(token->lexeme, "__volatile") == 0) {
        return true;
      }
    }
    return false;
  default:
    return false;
  }
}

bool c_is_storage_class_specifier(CParser *parser) {
  Token *token = CURRENT(parser);
  switch (token->type) {
  case TOKEN_AUTO:
  case TOKEN_REGISTER:
  case TOKEN_STATIC:
  case TOKEN_EXTERN:
  case TOKEN_TYPEDEF:
  case TOKEN__THREAD_LOCAL:
    return true;
  default:
    return false;
  }
}

bool c_is_function_specifier(CParser *parser) {
  Token *token = CURRENT(parser);
  switch (token->type) {
  case TOKEN_INLINE:
  case TOKEN__NORETURN:
  case TOKEN___INLINE__:
    return true;
  default:
    return false;
  }
}

bool c_is_declaration_specifier(CParser *parser) {
  return c_is_storage_class_specifier(parser) || c_is_type_specifier(parser) ||
         c_is_type_qualifier(parser) || c_is_function_specifier(parser);
}

bool c_is_type_name(CParser *parser, const char *name) {
  /* Initialize builtin types hash table on first call */
  if (!builtin_types_initialized) {
    init_builtin_types();
  }
  
  /* Check if identifier is a typedef name */
  if (symbol_table_contains(parser->typedef_names, name)) {
    return true;
  }

  /* Check for compiler builtins with prefix (optimized prefix check) */
  if (name[0] == '_' && name[1] == '_' && 
      name[2] == 'b' && name[3] == 'u' && name[4] == 'i' &&
      name[5] == 'l' && name[6] == 't' && name[7] == 'i' &&
      name[8] == 'n' && name[9] == '_') {
    return true;
  }

  /* Check hash table for known builtin types (O(1) average case) */
  return is_builtin_type(name);
}

/* ===== SCOPE MANAGEMENT ===== */

void c_parser_enter_scope(CParser *parser) {
  parser->scope_depth++;
  /* Push new scope - symbol table implementation deferred */
  /* Scope tracking works, symbol resolution needs full symbol table */
}

void c_parser_exit_scope(CParser *parser) {
  parser->scope_depth--;
  /* Pop scope - symbol table implementation deferred */
}

void c_parser_add_typedef(CParser *parser, const char *name) {
  /* Add typedef name to symbol table */
  symbol_table_add(parser->typedef_names, name);
}

/* Extract the identifier name from a declarator (handles complex declarators)
 */
static const char *c_extract_declarator_name(ASTNode *declarator) {
  if (!declarator)
    return NULL;

  switch (declarator->type) {
  case AST_IDENTIFIER:
    return declarator->data.identifier.name;

  case AST_POINTER_TYPE:
  case AST_ARRAY_TYPE:
  case AST_FUNCTION_TYPE:
    /* For complex declarators, the identifier is usually the first child */
    if (declarator->children && declarator->children[0]) {
      return c_extract_declarator_name(declarator->children[0]);
    }
    break;

  default:
    /* Try to find an identifier in the children */
    for (size_t i = 0; i < declarator->child_count; i++) {
      const char *name = c_extract_declarator_name(declarator->children[i]);
      if (name)
        return name;
    }
    break;
  }

  return NULL;
}
