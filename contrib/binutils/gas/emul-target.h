#ifndef emul_init
#define emul_init			common_emul_init
#endif

#ifndef emul_bfd_name
#define emul_bfd_name			default_emul_bfd_name
#endif

#ifndef emul_local_labels_fb
#define emul_local_labels_fb		0
#endif

#ifndef emul_local_labels_dollar
#define emul_local_labels_dollar	0
#endif

#ifndef emul_leading_underscore
#define emul_leading_underscore		2
#endif

#ifndef emul_strip_underscore
#define emul_strip_underscore		0
#endif

#ifndef emul_default_endian
#define emul_default_endian		2
#endif

#ifndef emul_fake_label_name
#define emul_fake_label_name		0
#endif

struct emulation emul_struct_name = {
  0,
  emul_name,
  emul_init,
  emul_bfd_name,
  emul_local_labels_fb, emul_local_labels_dollar,
  emul_leading_underscore, emul_strip_underscore,
  emul_default_endian,
  emul_fake_label_name,
  emul_format,
};
