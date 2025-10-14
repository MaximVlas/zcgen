bool c_is_type_name(CParser *parser, const char *name) {
  /* Check if identifier is a typedef name */
  if (symbol_table_contains(parser->typedef_names, name)) {
    return true;
  }

  /* Check for compiler builtins */
  if (strncmp(name, "__builtin_", 10) == 0) {
    return true;
  }

  /* Common GCC/Clang builtins and system types */
  if (strcmp(name, "__gnuc_va_list") == 0 ||
      strcmp(name, "__builtin_va_list") == 0 ||
      strcmp(name, "__va_list_tag") == 0 ||

      /* Integer types */
      strcmp(name, "__int8_t") == 0 || strcmp(name, "__int16_t") == 0 ||
      strcmp(name, "__int32_t") == 0 || strcmp(name, "__int64_t") == 0 ||
      strcmp(name, "__uint8_t") == 0 || strcmp(name, "__uint16_t") == 0 ||
      strcmp(name, "__uint32_t") == 0 || strcmp(name, "__uint64_t") == 0 ||
      strcmp(name, "__intptr_t") == 0 || strcmp(name, "__uintptr_t") == 0 ||
      strcmp(name, "__size_t") == 0 || strcmp(name, "__ptrdiff_t") == 0 ||
      strcmp(name, "__wchar_t") == 0 || strcmp(name, "__int128_t") == 0 ||
      strcmp(name, "__uint128_t") == 0 || strcmp(name, "__int128") == 0 ||
      strcmp(name, "__uint128") == 0 || strcmp(name, "__intmax_t") == 0 ||
      strcmp(name, "__uintmax_t") == 0 ||

      /* GCC/Clang specific types */
      strcmp(name, "__int8") == 0 || strcmp(name, "__int16") == 0 ||
      strcmp(name, "__int32") == 0 || strcmp(name, "__int64") == 0 ||
      strcmp(name, "__float128") == 0 || strcmp(name, "__float80") == 0 ||
      strcmp(name, "__fp16") == 0 || strcmp(name, "__bf16") == 0 ||
      strcmp(name, "__m64") == 0 || strcmp(name, "__m128") == 0 ||
      strcmp(name, "__m128i") == 0 || strcmp(name, "__m128d") == 0 ||
      strcmp(name, "__m256") == 0 || strcmp(name, "__m256i") == 0 ||
      strcmp(name, "__m256d") == 0 || strcmp(name, "__m512") == 0 ||
      strcmp(name, "__m512i") == 0 || strcmp(name, "__m512d") == 0 ||
      strcmp(name, "__v2df") == 0 || strcmp(name, "__v2di") == 0 ||
      strcmp(name, "__v4df") == 0 || strcmp(name, "__v4di") == 0 ||
      strcmp(name, "__v4sf") == 0 || strcmp(name, "__v4si") == 0 ||
      strcmp(name, "__v8sf") == 0 || strcmp(name, "__v8si") == 0 ||

      /* Atomic types */
      strcmp(name, "__atomic_int") == 0 || strcmp(name, "__atomic_uint") == 0 ||
      strcmp(name, "__atomic_long") == 0 ||
      strcmp(name, "__atomic_ulong") == 0 ||
      strcmp(name, "__atomic_llong") == 0 ||
      strcmp(name, "__atomic_ullong") == 0 ||

      /* System types from headers */
      strcmp(name, "__off_t") == 0 || strcmp(name, "__off64_t") == 0 ||
      strcmp(name, "__mbstate_t") == 0 || strcmp(name, "__fpos_t") == 0 ||
      strcmp(name, "__fpos64_t") == 0 || strcmp(name, "__u_char") == 0 ||
      strcmp(name, "__u_short") == 0 || strcmp(name, "__u_int") == 0 ||
      strcmp(name, "__u_long") == 0 || strcmp(name, "__quad_t") == 0 ||
      strcmp(name, "__u_quad_t") == 0 || strcmp(name, "__dev_t") == 0 ||
      strcmp(name, "__uid_t") == 0 || strcmp(name, "__gid_t") == 0 ||
      strcmp(name, "__ino_t") == 0 || strcmp(name, "__ino64_t") == 0 ||
      strcmp(name, "__mode_t") == 0 || strcmp(name, "__nlink_t") == 0 ||
      strcmp(name, "__pid_t") == 0 || strcmp(name, "__fsid_t") == 0 ||
      strcmp(name, "__clock_t") == 0 || strcmp(name, "__rlim_t") == 0 ||
      strcmp(name, "__rlim64_t") == 0 || strcmp(name, "__id_t") == 0 ||
      strcmp(name, "__time_t") == 0 || strcmp(name, "__useconds_t") == 0 ||
      strcmp(name, "__suseconds_t") == 0 ||
      strcmp(name, "__suseconds64_t") == 0 || strcmp(name, "__daddr_t") == 0 ||
      strcmp(name, "__key_t") == 0 || strcmp(name, "__clockid_t") == 0 ||
      strcmp(name, "__timer_t") == 0 || strcmp(name, "__blksize_t") == 0 ||
      strcmp(name, "__blkcnt_t") == 0 || strcmp(name, "__blkcnt64_t") == 0 ||
      strcmp(name, "__fsblkcnt_t") == 0 ||
      strcmp(name, "__fsblkcnt64_t") == 0 ||
      strcmp(name, "__fsfilcnt_t") == 0 ||
      strcmp(name, "__fsfilcnt64_t") == 0 || strcmp(name, "__fsword_t") == 0 ||
      strcmp(name, "__ssize_t") == 0 ||
      strcmp(name, "__syscall_slong_t") == 0 ||
      strcmp(name, "__syscall_ulong_t") == 0 || strcmp(name, "__loff_t") == 0 ||
      strcmp(name, "__caddr_t") == 0 || strcmp(name, "__socklen_t") == 0 ||
      strcmp(name, "__sig_atomic_t") == 0 || strcmp(name, "__sigset_t") == 0 ||
      strcmp(name, "__fd_mask") == 0 || strcmp(name, "__fd_set") == 0 ||

      /* Thread types */
      strcmp(name, "__pthread_t") == 0 ||
      strcmp(name, "__pthread_attr_t") == 0 ||
      strcmp(name, "__pthread_mutex_t") == 0 ||
      strcmp(name, "__pthread_mutexattr_t") == 0 ||
      strcmp(name, "__pthread_cond_t") == 0 ||
      strcmp(name, "__pthread_condattr_t") == 0 ||
      strcmp(name, "__pthread_key_t") == 0 ||
      strcmp(name, "__pthread_once_t") == 0 ||
      strcmp(name, "__pthread_rwlock_t") == 0 ||
      strcmp(name, "__pthread_rwlockattr_t") == 0 ||
      strcmp(name, "__pthread_spinlock_t") == 0 ||
      strcmp(name, "__pthread_barrier_t") == 0 ||
      strcmp(name, "__pthread_barrierattr_t") == 0 ||

      /* Signal types */
      strcmp(name, "__sigval_t") == 0 || strcmp(name, "__siginfo_t") == 0 ||
      strcmp(name, "__sigevent_t") == 0 ||

      /* Locale types */
      strcmp(name, "__locale_t") == 0 || strcmp(name, "__locale_data") == 0 ||

      /* Regex types */
      strcmp(name, "__regex_t") == 0 || strcmp(name, "__regmatch_t") == 0 ||

      /* Directory types */
      strcmp(name, "__DIR") == 0 || strcmp(name, "__dirstream") == 0 ||

      /* Time types */
      strcmp(name, "__time64_t") == 0 || strcmp(name, "__timespec") == 0 ||
      strcmp(name, "__timeval") == 0 || strcmp(name, "__itimerspec") == 0 ||
      strcmp(name, "__timezone") == 0 ||

      /* Standard I/O types */
      strcmp(name, "__FILE") == 0 ||
      strcmp(name, "__cookie_io_functions_t") == 0 ||

      /* Other common system types */
      strcmp(name, "__jmp_buf") == 0 || strcmp(name, "__sigjmp_buf") == 0 ||
      strcmp(name, "__rlimit") == 0 || strcmp(name, "__rlimit64") == 0 ||
      strcmp(name, "__rusage") == 0 || strcmp(name, "__timex") == 0 ||
      strcmp(name, "__iovec") == 0 || strcmp(name, "__sockaddr") == 0 ||
      strcmp(name, "__msghdr") == 0 || strcmp(name, "__cmsghdr") == 0 ||
      strcmp(name, "__stat") == 0 || strcmp(name, "__stat64") == 0 ||
      strcmp(name, "__statfs") == 0 || strcmp(name, "__statfs64") == 0 ||
      strcmp(name, "__statvfs") == 0 || strcmp(name, "__statvfs64") == 0 ||
      strcmp(name, "__dirent") == 0 || strcmp(name, "__dirent64") == 0 ||
      strcmp(name, "__ucontext") == 0 || strcmp(name, "__mcontext_t") == 0 ||
      strcmp(name, "__sigcontext") == 0 || strcmp(name, "__stack_t") == 0 ||
      strcmp(name, "__sigaction") == 0 ||

      /* Standard C library types (non-underscore versions) */
      strcmp(name, "FILE") == 0 || strcmp(name, "va_list") == 0 ||
      strcmp(name, "off_t") == 0 || strcmp(name, "ssize_t") == 0 ||
      strcmp(name, "size_t") == 0 || strcmp(name, "fpos_t") == 0 ||
      strcmp(name, "ptrdiff_t") == 0 || strcmp(name, "wchar_t") == 0 ||
      strcmp(name, "wint_t") == 0 || strcmp(name, "wctype_t") == 0 ||
      strcmp(name, "mbstate_t") == 0 || strcmp(name, "int8_t") == 0 ||
      strcmp(name, "int16_t") == 0 || strcmp(name, "int32_t") == 0 ||
      strcmp(name, "int64_t") == 0 || strcmp(name, "uint8_t") == 0 ||
      strcmp(name, "uint16_t") == 0 || strcmp(name, "uint32_t") == 0 ||
      strcmp(name, "uint64_t") == 0 || strcmp(name, "intptr_t") == 0 ||
      strcmp(name, "uintptr_t") == 0 || strcmp(name, "intmax_t") == 0 ||
      strcmp(name, "uintmax_t") == 0 || strcmp(name, "pid_t") == 0 ||
      strcmp(name, "uid_t") == 0 || strcmp(name, "gid_t") == 0 ||
      strcmp(name, "dev_t") == 0 || strcmp(name, "ino_t") == 0 ||
      strcmp(name, "mode_t") == 0 || strcmp(name, "nlink_t") == 0 ||
      strcmp(name, "time_t") == 0 || strcmp(name, "clock_t") == 0 ||
      strcmp(name, "clockid_t") == 0 || strcmp(name, "timer_t") == 0 ||
      strcmp(name, "suseconds_t") == 0 || strcmp(name, "useconds_t") == 0 ||
      strcmp(name, "blksize_t") == 0 || strcmp(name, "blkcnt_t") == 0 ||
      strcmp(name, "fsblkcnt_t") == 0 || strcmp(name, "fsfilcnt_t") == 0 ||
      strcmp(name, "id_t") == 0 || strcmp(name, "key_t") == 0 ||
      strcmp(name, "pthread_t") == 0 || strcmp(name, "pthread_attr_t") == 0 ||
      strcmp(name, "pthread_mutex_t") == 0 ||
      strcmp(name, "pthread_mutexattr_t") == 0 ||
      strcmp(name, "pthread_cond_t") == 0 ||
      strcmp(name, "pthread_condattr_t") == 0 ||
      strcmp(name, "pthread_key_t") == 0 ||
      strcmp(name, "pthread_once_t") == 0 ||
      strcmp(name, "pthread_rwlock_t") == 0 ||
      strcmp(name, "pthread_rwlockattr_t") == 0 ||
      strcmp(name, "pthread_spinlock_t") == 0 ||
      strcmp(name, "pthread_barrier_t") == 0 ||
      strcmp(name, "pthread_barrierattr_t") == 0 ||
      strcmp(name, "sigset_t") == 0 || strcmp(name, "sig_atomic_t") == 0 ||
      strcmp(name, "socklen_t") == 0 || strcmp(name, "sa_family_t") == 0 ||
      strcmp(name, "in_addr_t") == 0 || strcmp(name, "in_port_t") == 0 ||
      strcmp(name, "locale_t") == 0 || strcmp(name, "DIR") == 0 ||
      strcmp(name, "regex_t") == 0 || strcmp(name, "regmatch_t") == 0 ||
      strcmp(name, "regoff_t") == 0 || strcmp(name, "div_t") == 0 ||
      strcmp(name, "ldiv_t") == 0 || strcmp(name, "lldiv_t") == 0 ||
      strcmp(name, "imaxdiv_t") == 0 || strcmp(name, "jmp_buf") == 0 ||
      strcmp(name, "sigjmp_buf") == 0 || strcmp(name, "fenv_t") == 0 ||
      strcmp(name, "fexcept_t") == 0) {