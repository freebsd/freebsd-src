/* $FreeBSD: src/gnu/usr.bin/cc/cc_tools/auto-host.h,v 1.2 1999/11/06 05:57:53 obrien Exp $ */

/* auto-host.h.  Generated automatically by configure.  */
/* config.in.  Generated automatically from configure.in by autoheader.  */

/* Define if you can safely include both <string.h> and <strings.h>.  */
#define STRING_WITH_STRINGS 1

/* Define if printf supports "%p".  */
#define HAVE_PRINTF_PTR 1

/* Define if you want expensive run-time checks. */
/* #undef ENABLE_CHECKING */

/* Define to 1 if NLS is requested.  */
/* #undef ENABLE_NLS */

/* Define as 1 if you have catgets and don't want to use GNU gettext.  */
/* #undef HAVE_CATGETS */

/* Define as 1 if you have gettext and don't want to use GNU gettext.  */
/* #undef HAVE_GETTEXT */

/* Define if your cpp understands the stringify operator.  */
#define HAVE_CPP_STRINGIFY 1

/* Define if your compiler understands volatile.  */
#define HAVE_VOLATILE 1

/* Define if your assembler supports specifying the maximum number
   of bytes to skip when using the GAS .p2align command. */
/* #undef HAVE_GAS_MAX_SKIP_P2ALIGN */

/* Define if your assembler supports .balign and .p2align.  */
/* #undef HAVE_GAS_BALIGN_AND_P2ALIGN */

/* Define if your assembler supports .subsection and .subsection -1 starts
   emitting at the beginning of your section */
/* #undef HAVE_GAS_SUBSECTION_ORDERING */

/* Define if your assembler uses the old HImode fild and fist notation.  */
/* #undef HAVE_GAS_FILDS_FISTS */

/* Define if you have a working <inttypes.h> header file.  */
/* #undef HAVE_INTTYPES_H */

/* Define if your locale.h file contains LC_MESSAGES.  */
#define HAVE_LC_MESSAGES 1

/* Whether malloc must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_MALLOC */

/* Whether realloc must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_REALLOC */

/* Whether calloc must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_CALLOC */

/* Whether free must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_FREE */

/* Whether bcopy must be declared even if <string.h> is included.  */
/* #undef NEED_DECLARATION_BCOPY */

/* Whether bcmp must be declared even if <string.h> is included.  */
/* #undef NEED_DECLARATION_BCMP */

/* Whether bzero must be declared even if <string.h> is included.  */
/* #undef NEED_DECLARATION_BZERO */

/* Whether index must be declared even if <string.h> is included.  */
/* #undef NEED_DECLARATION_INDEX */

/* Whether rindex must be declared even if <string.h> is included.  */
/* #undef NEED_DECLARATION_RINDEX */

/* Whether getenv must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_GETENV */

/* Whether atol must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_ATOL */

/* Whether atof must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_ATOF */

/* Whether sbrk must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_SBRK */

/* Whether abort must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_ABORT */

/* Whether strerror must be declared even if <string.h> is included.  */
/* #undef NEED_DECLARATION_STRERROR */

/* Whether strsignal must be declared even if <string.h> is included.  */
/* #undef NEED_DECLARATION_STRSIGNAL */

/* Whether strstr must be declared even if <string.h> is included.  */
/* #undef NEED_DECLARATION_STRSTR */

/* Whether getcwd must be declared even if <unistd.h> is included.  */
/* #undef NEED_DECLARATION_GETCWD */

/* Whether getwd must be declared even if <unistd.h> is included.  */
/* #undef NEED_DECLARATION_GETWD */

/* Whether getrlimit must be declared even if <sys/resource.h> is included.  */
#define NEED_DECLARATION_GETRLIMIT 1

/* Whether setrlimit must be declared even if <sys/resource.h> is included.  */
#define NEED_DECLARATION_SETRLIMIT 1

/* Whether putc_unlocked must be declared even if <stdio.h> is included.  */
#define NEED_DECLARATION_PUTC_UNLOCKED 1

/* Whether fputs_unlocked must be declared even if <stdio.h> is included.  */
#define NEED_DECLARATION_FPUTS_UNLOCKED 1

/* Define to enable the use of a default assembler. */
/* #undef DEFAULT_ASSEMBLER */

/* Define to enable the use of a default linker. */
/* #undef DEFAULT_LINKER */

/* Define if host mkdir takes a single argument. */
/* #undef MKDIR_TAKES_ONE_ARG */

/* Define to the name of the distribution.  */
#define PACKAGE "gcc"

/* Define to the version of the distribution.  */
#define VERSION "2.95.2"

/* Define if using alloca.c.  */
/* #undef C_ALLOCA */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems.  */
/* #undef CRAY_STACKSEG_END */

/* Define if you have alloca, as a function or macro.  */
#define HAVE_ALLOCA 1

/* Define if you have <alloca.h> and it should be used (not on Ultrix).  */
/* #undef HAVE_ALLOCA_H */

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if you have a working `mmap' system call.  */
#define HAVE_MMAP 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have <vfork.h>.  */
/* #undef HAVE_VFORK_H */

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

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

/* Define if `sys_siglist' is declared by <signal.h>.  */
#define SYS_SIGLIST_DECLARED 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define vfork as fork if vfork does not work.  */
/* #undef vfork */

/* Define if you have the __argz_count function.  */
/* #undef HAVE___ARGZ_COUNT */

/* Define if you have the __argz_next function.  */
/* #undef HAVE___ARGZ_NEXT */

/* Define if you have the __argz_stringify function.  */
/* #undef HAVE___ARGZ_STRINGIFY */

/* Define if you have the atoll function.  */
/* #undef HAVE_ATOLL */

/* Define if you have the atoq function.  */
/* #undef HAVE_ATOQ */

/* Define if you have the bcmp function.  */
#define HAVE_BCMP 1

/* Define if you have the bcopy function.  */
#define HAVE_BCOPY 1

/* Define if you have the bsearch function.  */
#define HAVE_BSEARCH 1

/* Define if you have the bzero function.  */
#define HAVE_BZERO 1

/* Define if you have the dcgettext function.  */
/* #undef HAVE_DCGETTEXT */

/* Define if you have the fputc_unlocked function.  */
/* #undef HAVE_FPUTC_UNLOCKED */

/* Define if you have the fputs_unlocked function.  */
/* #undef HAVE_FPUTS_UNLOCKED */

/* Define if you have the getcwd function.  */
#define HAVE_GETCWD 1

/* Define if you have the getpagesize function.  */
#define HAVE_GETPAGESIZE 1

/* Define if you have the getrlimit function.  */
#define HAVE_GETRLIMIT 1

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the index function.  */
#define HAVE_INDEX 1

/* Define if you have the isascii function.  */
#define HAVE_ISASCII 1

/* Define if you have the kill function.  */
#define HAVE_KILL 1

/* Define if you have the munmap function.  */
#define HAVE_MUNMAP 1

/* Define if you have the popen function.  */
#define HAVE_POPEN 1

/* Define if you have the putc_unlocked function.  */
/* #undef HAVE_PUTC_UNLOCKED */

/* Define if you have the putenv function.  */
#define HAVE_PUTENV 1

/* Define if you have the rindex function.  */
#define HAVE_RINDEX 1

/* Define if you have the setenv function.  */
#define HAVE_SETENV 1

/* Define if you have the setlocale function.  */
#define HAVE_SETLOCALE 1

/* Define if you have the setrlimit function.  */
#define HAVE_SETRLIMIT 1

/* Define if you have the stpcpy function.  */
/* #undef HAVE_STPCPY */

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strrchr function.  */
#define HAVE_STRRCHR 1

/* Define if you have the strsignal function.  */
#define HAVE_STRSIGNAL 1

/* Define if you have the strtoul function.  */
#define HAVE_STRTOUL 1

/* Define if you have the sysconf function.  */
#define HAVE_SYSCONF 1

/* Define if you have the <argz.h> header file.  */
/* #undef HAVE_ARGZ_H */

/* Define if you have the <direct.h> header file.  */
/* #undef HAVE_DIRECT_H */

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <locale.h> header file.  */
#define HAVE_LOCALE_H 1

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <nl_types.h> header file.  */
#define HAVE_NL_TYPES_H 1

/* Define if you have the <stab.h> header file.  */
#define HAVE_STAB_H 1

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

/* Define if you have the i library (-li).  */
/* #undef HAVE_LIBI */
