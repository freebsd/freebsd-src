/* $FreeBSD$ */

#include <sys/param.h>

/* auto-host.h.  Generated automatically by configure.  */
/* config.in.  Generated automatically from configure.in by autoheader.  */

/* Define if using alloca.c.  */
/* #undef C_ALLOCA */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems.  */
/* #undef CRAY_STACKSEG_END */

/* Define to the type of elements in the array set by `getgroups'.
   Usually this is either `int' or `gid_t'.  */
#define GETGROUPS_T gid_t

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define if you have alloca, as a function or macro.  */
#define HAVE_ALLOCA 1

/* Define if you have <alloca.h> and it should be used (not on Ultrix).  */
/* #undef HAVE_ALLOCA_H */

/* Define if you have the ANSI # stringizing operator in cpp. */
#define HAVE_STRINGIZE 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have <vfork.h>.  */
/* #undef HAVE_VFORK_H */

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

/* Define if your C compiler doesn't accept -c and -o together.  */
/* #undef NO_MINUS_C_MINUS_O */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
 STACK_DIRECTION > 0 => grows toward higher addresses
 STACK_DIRECTION < 0 => grows toward lower addresses
 STACK_DIRECTION = 0 => direction of growth unknown
 */
/* #undef STACK_DIRECTION */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define vfork as fork if vfork does not work.  */
/* #undef vfork */

/* Define if your assembler supports specifying the maximum number
   of bytes to skip when using the GAS .p2align command.  */
#define HAVE_GAS_MAX_SKIP_P2ALIGN 1

/* Define if your assembler supports .balign and .p2align.  */
#define HAVE_GAS_BALIGN_AND_P2ALIGN 1

/* Define if your assembler uses the old HImode fild and fist notation.  */
#define HAVE_GAS_FILDS_FISTS 1

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef ssize_t */

/* Define if cpp should also search $prefix/include.  */
/* #undef PREFIX_INCLUDE_DIR */

/* Define if you have the __argz_count function.  */
/* #undef HAVE___ARGZ_COUNT */

/* Define if you have the __argz_next function.  */
/* #undef HAVE___ARGZ_NEXT */

/* Define if you have the __argz_stringify function.  */
/* #undef HAVE___ARGZ_STRINGIFY */

/* Define if you have the alphasort function.  */
#define HAVE_ALPHASORT 1

/* Define if you have the atoll function.  */
#if __FreeBSD_version >= 500027
/* FreeBSD didn't always have atoll(3). */
#define HAVE_ATOLL 1
#endif

/* Define if you have the atoq function.  */
/* #undef HAVE_ATOQ */

/* Define if you have the clock function.  */
#define HAVE_CLOCK 1

/* Define if you have the dcgettext function.  */
/* #undef HAVE_DCGETTEXT */

/* Define if you have the dup2 function.  */
#define HAVE_DUP2 1

/* Define if you have the feof_unlocked function.  */
#define HAVE_FEOF_UNLOCKED 1

/* Define if you have the fgets_unlocked function.  */
/* #undef HAVE_FGETS_UNLOCKED */

/* Define if you have the fprintf_unlocked function.  */
/* #undef HAVE_FPRINTF_UNLOCKED */

/* Define if you have the fputc_unlocked function.  */
/* #undef HAVE_FPUTC_UNLOCKED */

/* Define if you have the fputs_unlocked function.  */
/* #undef HAVE_FPUTS_UNLOCKED */

/* Define if you have the fwrite_unlocked function.  */
/* #undef HAVE_FWRITE_UNLOCKED */

/* Define if you have the getcwd function.  */
#define HAVE_GETCWD 1

/* Define if you have the getegid function.  */
#define HAVE_GETEGID 1

/* Define if you have the geteuid function.  */
#define HAVE_GETEUID 1

/* Define if you have the getgid function.  */
#define HAVE_GETGID 1

/* Define if you have the getpagesize function.  */
#define HAVE_GETPAGESIZE 1

/* Define if you have the getrlimit function.  */
#define HAVE_GETRLIMIT 1

/* Define if you have the getrusage function.  */
#define HAVE_GETRUSAGE 1

/* Define if you have the getuid function.  */
#define HAVE_GETUID 1

/* Define if you have the kill function.  */
#define HAVE_KILL 1

/* Define if you have the lstat function.  */
#define HAVE_LSTAT 1

/* Define if you have the mempcpy function.  */
/* #undef HAVE_MEMPCPY */

/* Define if you have the munmap function.  */
#define HAVE_MUNMAP 1

/* Define if you have the nl_langinfo function.  */
#define HAVE_NL_LANGINFO 1

/* Define if you have the putc_unlocked function.  */
#define HAVE_PUTC_UNLOCKED 1

/* Define if you have the putenv function.  */
#define HAVE_PUTENV 1

/* Define if you have the scandir function.  */
#define HAVE_SCANDIR 1

/* Define if you have the setenv function.  */
#define HAVE_SETENV 1

/* Define if you have the setlocale function.  */
#define HAVE_SETLOCALE 1

/* Define if you have the setrlimit function.  */
#define HAVE_SETRLIMIT 1

/* Define if you have the stpcpy function.  */
#define HAVE_STPCPY 1

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strsignal function.  */
#define HAVE_STRSIGNAL 1

/* Define if you have the strtoul function.  */
#define HAVE_STRTOUL 1

/* Define if you have the sysconf function.  */
#define HAVE_SYSCONF 1

/* Define if you have the times function.  */
#define HAVE_TIMES 1

/* Define if you have the tsearch function.  */
#define HAVE_TSEARCH 1

/* Define if you have the <argz.h> header file.  */
/* #undef HAVE_ARGZ_H */

/* Define if you have the <direct.h> header file.  */
/* #undef HAVE_DIRECT_H */

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <langinfo.h> header file.  */
#define HAVE_LANGINFO_H 1

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <locale.h> header file.  */
#define HAVE_LOCALE_H 1

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <nl_types.h> header file.  */
#define HAVE_NL_TYPES_H 1

/* Define if you have the <stddef.h> header file.  */
#define HAVE_STDDEF_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/stat.h> header file.  */
#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/times.h> header file.  */
#define HAVE_SYS_TIMES_H 1

/* Define if you have the <time.h> header file.  */
#define HAVE_TIME_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define to enable the use of a default linker. */
/* #undef DEFAULT_LINKER */

/* Define to enable the use of a default assembler. */
/* #undef DEFAULT_ASSEMBLER */

/* Define if your compiler understands volatile. */
#define HAVE_VOLATILE 1

/* Define if your compiler supports the `long double' type. */
#define HAVE_LONG_DOUBLE 1

/* Define if your compiler supports the `long long' type. */
#define HAVE_LONG_LONG 1

/* Define if your compiler supports the `__int64' type. */
/* #undef HAVE___INT64 */

/* Define if the `_Bool' type is built-in. */
#define HAVE__BOOL 1

/* The number of bytes in type short */
#define SIZEOF_SHORT 2

/* The number of bytes in type int */
#define SIZEOF_INT 4

/* The number of bytes in type long */
/* #define SIZEOF_LONG 4 */
#if defined(__i386__) || defined(__powerpc__) || defined(__strongarm__)
#define SIZEOF_LONG SIZEOF_INT
#elif defined(__alpha__) || defined(__sparc64__) || defined(__ia64__) || defined(__amd64__)
#define SIZEOF_LONG SIZEOF_LONG_LONG
#else
#error "I don't know what arch this is."
#endif

/* The number of bytes in type long long */
#define SIZEOF_LONG_LONG 8

/* The number of bytes in type __int64 */
/* #undef SIZEOF___INT64 */

/* Define if the host execution character set is EBCDIC. */
/* #undef HOST_EBCDIC */

#ifdef WANT_COMPILER_INVARIANTS
//#warning WANT_COMPILER_INVARIANTS turned on

/* Define if you want more run-time sanity checks.  This one gets a grab
   bag of miscellaneous but relatively cheap checks. */
#define ENABLE_CHECKING 1

/* Define if you want all operations on trees (the basic data
   structure of the front ends) to be checked for dynamic type safety
   at runtime.  This is moderately expensive. */
#define ENABLE_TREE_CHECKING 1

/* Define if you want all operations on RTL (the basic data structure
   of the optimizer and back end) to be checked for dynamic type safety
   at runtime.  This is quite expensive. */
#define ENABLE_RTL_CHECKING 1

/* Define if you want RTL flag accesses to be checked against the RTL
   codes that are supported for each access macro.  This is relatively
   cheap. */
#define ENABLE_RTL_FLAG_CHECKING 1

/* Define if you want the garbage collector to do object poisoning and
   other memory allocation checks.  This is quite expensive. */
#define ENABLE_GC_CHECKING 1

/* Define if you want the garbage collector to operate in maximally
   paranoid mode, validating the entire heap and collecting garbage at
   every opportunity.  This is extremely expensive. */
#define ENABLE_GC_ALWAYS_COLLECT 1

/* Define if you want to run subprograms and generated programs
   through valgrind (a memory checker).  This is extremely expensive. */
/* #undef ENABLE_VALGRIND_CHECKING */

#endif	/* WANT_COMPILER_INVARIANTS */

/* Define if you want to use __cxa_atexit, rather than atexit, to
   register C++ destructors for local statics and global objects.
   This is essential for fully standards-compliant handling of
   destructors, but requires __cxa_atexit in libc. */
/* #undef DEFAULT_USE_CXA_ATEXIT */

/* Define if you want the C and C++ compilers to support multibyte
   character sets for source code. */
/* #undef MULTIBYTE_CHARS */

/* Always define this when using the GNU C Library */
/* #undef _GNU_SOURCE */

/* Define if you have a working <stdbool.h> header file. */
#if (__FreeBSD_version >= 440003 && __FreeBSD_version < 500000) || \
    __FreeBSD_version >= 500014
#define HAVE_STDBOOL_H 1
#endif

/* Define if you can safely include both <string.h> and <strings.h>. */
#define STRING_WITH_STRINGS 1

/* Define as the number of bits in a byte, if `limits.h' doesn't. */
/* #undef CHAR_BIT */

/* Define if the host machine stores words of multi-word integers in
   big-endian order. */
/* #undef HOST_WORDS_BIG_ENDIAN */

/* Define to the floating point format of the host machine, if not IEEE. */
/* #undef HOST_FLOAT_FORMAT */

/* Define to 1 if the host machine stores floating point numbers in
   memory with the word containing the sign bit at the lowest address,
   or to 0 if it does it the other way around.

   This macro should not be defined if the ordering is the same as for
   multi-word integers. */
/* #undef HOST_FLOAT_WORDS_BIG_ENDIAN */

/* Define if you have a working <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if printf supports %p. */
#define HAVE_PRINTF_PTR 1

/* Define if mmap can get us zeroed pages from /dev/zero. */
#define HAVE_MMAP_DEV_ZERO 1

/* Define if mmap can get us zeroed pages using MAP_ANON(YMOUS). */
#define HAVE_MMAP_ANON 1

/* Define if read-only mmap of a plain file works. */
#define HAVE_MMAP_FILE 1

/* Define if you have the iconv() function. */
/* #undef HAVE_ICONV */

/* Define as const if the declaration of iconv() needs const. */
/* #undef ICONV_CONST */

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETENV 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_ATOL 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_SBRK 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_ABORT 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_ATOF 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETCWD 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETWD 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_STRSIGNAL 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_PUTC_UNLOCKED 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_FPUTS_UNLOCKED 0

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_FWRITE_UNLOCKED 0

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_FPRINTF_UNLOCKED 0

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_STRSTR 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_ERRNO 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_VASPRINTF 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_MALLOC 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_REALLOC 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_CALLOC 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_FREE 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_BASENAME 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETOPT 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_CLOCK 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETRLIMIT 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_SETRLIMIT 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETRUSAGE 1

/* Define to `long' if <sys/resource.h> doesn't define. */
/* #undef rlim_t */

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_TIMES 1

/* Define if <sys/times.h> defines struct tms. */
#define HAVE_STRUCT_TMS 1

/* Define if <time.h> defines clock_t. */
#define HAVE_CLOCK_T 1

/* Define .init_array/.fini_array sections are available and working. */
/* #undef HAVE_INITFINI_ARRAY */

/* Define if host mkdir takes a single argument. */
/* #undef MKDIR_TAKES_ONE_ARG */

/* Define if you have the iconv() function. */
/* #undef HAVE_ICONV */

/* Define as const if the declaration of iconv() needs const. */
/* #undef ICONV_CONST */

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
#define HAVE_LANGINFO_CODESET 1

/* Define if your <locale.h> file defines LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1

/* Define to 1 if translation of program messages to the user's native language
   is requested. */
/* #undef ENABLE_NLS */

/* Define if you have the <libintl.h> header file. */
/* #undef HAVE_LIBINTL_H */

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

/* Define to use the libintl included with this package instead of any
   version in the system libraries. */
/* #undef USE_INCLUDED_LIBINTL */

/* Define to 1 if installation paths should be looked up in Windows32
   Registry. Ignored on non windows32 hosts. */
/* #undef ENABLE_WIN32_REGISTRY */

/* Define to be the last portion of registry key on windows hosts. */
/* #undef WIN32_REGISTRY_KEY */

/* Define if your assembler supports .subsection and .subsection -1 starts
   emitting at the beginning of your section. */
#define HAVE_GAS_SUBSECTION_ORDERING 1

/* Define if your assembler supports .weak. */
#define HAVE_GAS_WEAK 1

/* Define if your assembler supports .hidden. */
#define HAVE_GAS_HIDDEN 1

/* Define if your assembler supports .uleb128. */
#define HAVE_AS_LEB128 1

/* Define if your assembler mis-optimizes .eh_frame data. */
/* #undef USE_AS_TRADITIONAL_FORMAT */

/* Define if your assembler supports marking sections with SHF_MERGE flag. */
#define HAVE_GAS_SHF_MERGE 1

/* Define if your assembler supports thread-local storage. */
#define HAVE_AS_TLS 1

/* Define if your assembler supports explicit relocations. */
/* #undef HAVE_AS_EXPLICIT_RELOCS */

/* Define if your assembler supports .register. */
/* #undef HAVE_AS_REGISTER_PSEUDO_OP */

/* Define if your assembler supports -relax option. */
/* #undef HAVE_AS_RELAX_OPTION */

/* Define if your assembler and linker support unaligned PC relative relocs. */
/* #undef HAVE_AS_SPARC_UA_PCREL */

/* Define if your assembler and linker support unaligned PC relative relocs against hidden symbols. */
/* #undef HAVE_AS_SPARC_UA_PCREL_HIDDEN */

/* Define if your assembler supports offsetable %lo(). */
/* #undef HAVE_AS_OFFSETABLE_LO10 */

/* Define true if the assembler supports '.long foo@GOTOFF'. */
#define HAVE_AS_GOTOFF_IN_DATA 1

/* Define if your assembler supports ltoffx and ldxmov relocations. */
/* #undef HAVE_AS_LTOFFX_LDXMOV_RELOCS */

/* Define if your assembler supports dwarf2 .file/.loc directives,
   and preserves file table indices exactly as given. */
#define HAVE_AS_DWARF2_DEBUG_LINE 1

/* Define if your assembler supports the --gdwarf2 option. */
#define HAVE_AS_GDWARF2_DEBUG_FLAG 1

/* Define if your assembler supports the --gstabs option. */
#define HAVE_AS_GSTABS_DEBUG_FLAG 1

/* Define if your linker links a mix of read-only
   and read-write sections into a read-write section. */
#define HAVE_LD_RO_RW_SECTION_MIXING 1

/* Define if your linker supports --eh-frame-hdr option. */
#define HAVE_LD_EH_FRAME_HDR 1

/* Define if your MIPS libgloss linker scripts consistently include STARTUP directives. */
/* #undef HAVE_MIPS_LIBGLOSS_STARTUP_DIRECTIVES */

/* Define 0/1 to force the choice for exception handling model. */
/* #undef CONFIG_SJLJ_EXCEPTIONS */

/* Define if gcc should use -lunwind. */
/* #undef USE_LIBUNWIND_EXCEPTIONS */


/* Bison unconditionally undefines `const' if neither `__STDC__' nor
   __cplusplus are defined.  That's a problem since we use `const' in
   the GCC headers, and the resulting bison code is therefore type
   unsafe.  Thus, we must match the bison behavior here.  */

#ifndef __STDC__
#ifndef __cplusplus
/* #undef const */
#define const
#endif
#endif
