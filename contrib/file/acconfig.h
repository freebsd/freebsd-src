/* Autoheader needs me */
#define PACKAGE "file"

/* Autoheader needs me */
#define VERSION "3.33"

/* Define if builtin ELF support is enabled.  */
#undef BUILTIN_ELF

/* Define if ELF core file support is enabled.  */
#undef ELFCORE

/* Define if the `long long' type works.  */
#undef HAVE_LONG_LONG

/* Define to `unsigned char' if standard headers don't define.  */
#undef uint8_t

/* Define to `unsigned short' if standard headers don't define.  */
#undef uint16_t

/* Define to `unsigned int' if standard headers don't define.  */
#undef uint32_t

/* Define to `unsigned long long', if available, or `unsigned long', if
   standard headers don't define.  */
#undef uint64_t

/* FIXME: These have to be added manually because autoheader doesn't know
   about AC_CHECK_SIZEOF_INCLUDES.  */

/* The number of bytes in a uint8_t.  */
#define SIZEOF_UINT8_T 0

/* The number of bytes in a uint16_t.  */
#define SIZEOF_UINT16_T 0

/* The number of bytes in a uint32_t.  */
#define SIZEOF_UINT32_T 0

/* The number of bytes in a uint64_t.  */
#define SIZEOF_UINT64_T 0
