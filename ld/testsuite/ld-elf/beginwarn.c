static const char _evoke_link_warning_foo []
  __attribute__ ((used, section (".gnu.warning.foo")))
    = "function foo is deprecated";

extern void foo (void);

static void (*const init_array []) (void)
  __attribute__ ((used, section (".init_array"), aligned (sizeof (void *))))
  = { foo };
