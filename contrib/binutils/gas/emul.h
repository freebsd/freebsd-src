#ifndef EMUL_DEFS
#define EMUL_DEFS

struct emulation {
  void (*match) PARAMS ((const char *));
  const char *name;
  void (*init) PARAMS ((void));
  const char *(*bfd_name) PARAMS ((void));
  unsigned local_labels_fb : 1;
  unsigned local_labels_dollar : 1;
  unsigned leading_underscore : 2;
  unsigned strip_underscore : 1;
  unsigned default_endian : 2;
  const char *fake_label_name;
  const struct format_ops *format;
};

COMMON struct emulation *this_emulation;

extern const char *default_emul_bfd_name PARAMS ((void));
extern void common_emul_init PARAMS ((void));

#endif
