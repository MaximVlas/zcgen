typedef struct
{
  int __count;
  union
  {
    unsigned int __wch;
    char __wchb[4];
  } __value;
} __mbstate_t;

typedef struct _G_fpos64_t
{
  long __pos;
  __mbstate_t __state;
} __fpos64_t;

int main(void) {
    return 0;
}