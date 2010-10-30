extern void foo (void);

static void (*const init_array []) (void)
  __attribute__ ((used, section (".init_array"), aligned (sizeof (void *))))
  = { foo };
