// $FreeBSD$
#define ASSUME_RAM 128
#define HAVE_CHECK_CRC32 1
#define HAVE_CHECK_CRC64 1
#define HAVE_CHECK_SHA256 1
#define HAVE_DECL_PROGRAM_INVOCATION_NAME 0
#define HAVE_DECODER 1
#define HAVE_DECODER_ARM 1
#define HAVE_DECODER_ARMTHUMB 1
#define HAVE_DECODER_DELTA 1
#define HAVE_DECODER_IA64 1
#define HAVE_DECODER_LZMA1 1
#define HAVE_DECODER_LZMA2 1
#define HAVE_DECODER_POWERPC 1
#define HAVE_DECODER_SPARC 1
#define HAVE_DECODER_X86 1
#define HAVE_DLFCN_H 1
#define HAVE_ENCODER 1
#define HAVE_ENCODER_ARM 1
#define HAVE_ENCODER_ARMTHUMB 1
#define HAVE_ENCODER_DELTA 1
#define HAVE_ENCODER_IA64 1
#define HAVE_ENCODER_LZMA1 1
#define HAVE_ENCODER_LZMA2 1
#define HAVE_ENCODER_POWERPC 1
#define HAVE_ENCODER_SPARC 1
#define HAVE_ENCODER_X86 1
#define HAVE_FCNTL_H 1
#define HAVE_FUTIMES 1
#define HAVE_GETOPT_H 1
#define HAVE_GETOPT_LONG 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MEMORY_H 1
#define HAVE_MF_BT2 1
#define HAVE_MF_BT3 1
#define HAVE_MF_BT4 1
#define HAVE_MF_HC3 1
#define HAVE_MF_HC4 1
#define HAVE_OPTRESET 1
#define HAVE_PTHREAD 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC 1
#define HAVE_SYS_ENDIAN_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UINTPTR_T 1
#define HAVE_UNISTD_H 1
#define HAVE_VISIBILITY 1
#define HAVE__BOOL 1
#define LT_OBJDIR ".libs/"
#define NDEBUG 1
#define PACKAGE "xz"
#define PACKAGE_BUGREPORT "lasse.collin@tukaani.org"
#define PACKAGE_NAME "XZ Utils"
#define PACKAGE_STRING "XZ Utils 4.999.9beta"
#define PACKAGE_TARNAME "xz"
#define PACKAGE_URL "http://tukaani.org/xz/"
#define PACKAGE_VERSION "4.999.9beta"
#define SIZEOF_SIZE_T 8
#define STDC_HEADERS 1
#define TUKLIB_CPUCORES_SYSCONF 1
#define TUKLIB_FAST_UNALIGNED_ACCESS 1
#define TUKLIB_PHYSMEM_SYSCONF 1
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif
#define VERSION "4.999.9beta"
#if defined(__FreeBSD__)
#include <machine/endian.h>
#if _BYTE_ORDER == _BIG_ENDIAN
# define WORDS_BIGENDIAN 1
#endif
#else
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif
#endif
