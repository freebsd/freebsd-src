/* $FreeBSD$ */

/* config.h.  Generated automatically by configure.  */
/* config.in.  Generated automatically from configure.in by autoheader.  */

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

/* Define if you have a working `mmap' system call.  */
#define HAVE_MMAP 1

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

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

/* Define if lex declares yytext as a char * by default, not a char[].  */
#define YYTEXT_POINTER 1

/* Define if you have the __argz_count function.  */
/* #undef HAVE___ARGZ_COUNT */

/* Define if you have the __argz_next function.  */
/* #undef HAVE___ARGZ_NEXT */

/* Define if you have the __argz_stringify function.  */
/* #undef HAVE___ARGZ_STRINGIFY */

/* Define if you have the dcgettext function.  */
/* #undef HAVE_DCGETTEXT */

/* Define if you have the getcwd function.  */
#define HAVE_GETCWD 1

/* Define if you have the getpagesize function.  */
#define HAVE_GETPAGESIZE 1

/* Define if you have the munmap function.  */
#define HAVE_MUNMAP 1

/* Define if you have the putenv function.  */
#define HAVE_PUTENV 1

/* Define if you have the remove function.  */
/* #undef HAVE_REMOVE */

/* Define if you have the sbrk function.  */
#define HAVE_SBRK 1

/* Define if you have the setenv function.  */
#define HAVE_SETENV 1

/* Define if you have the setlocale function.  */
#define HAVE_SETLOCALE 1

/* Define if you have the stpcpy function.  */
/* #undef HAVE_STPCPY */

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the unlink function.  */
#define HAVE_UNLINK 1

/* Define if you have the <argz.h> header file.  */
/* #undef HAVE_ARGZ_H */

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <locale.h> header file.  */
#define HAVE_LOCALE_H 1

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <nl_types.h> header file.  */
#define HAVE_NL_TYPES_H 1

/* Define if you have the <stdarg.h> header file.  */
#define HAVE_STDARG_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <values.h> header file.  */
/* #undef HAVE_VALUES_H */

/* Define if you have the <varargs.h> header file.  */
#define HAVE_VARARGS_H 1

/* Name of package */
#define PACKAGE "gas"

/* Version number of package */
/* #define VERSION "2.11" */

/* Define if defaulting to ELF on SCO 5. */
/* #undef SCO_ELF */

/* Using strict COFF? */
/* #undef STRICTCOFF */

/* Use ELF stabs for MIPS, not ECOFF stabs */
/* #undef MIPS_STABS_ELF */

/* Define if default target is PowerPC Solaris. */
/* #undef TARGET_SOLARIS_COMMENT */

/* Define as 1 if big endian. */
/* #undef TARGET_BYTES_BIG_ENDIAN */

/* Default architecture. */
/* #undef DEFAULT_ARCH */

/* Using cgen code? */
/* #undef USING_CGEN */

/* Using i386 COFF? */
/* #undef I386COFF */

/* Using m68k COFF? */
/* #undef M68KCOFF */

/* Using m88k COFF? */
/* #undef M88KCOFF */

/* a.out support? */
/* #undef OBJ_MAYBE_AOUT */

/* b.out support? */
/* #undef OBJ_MAYBE_BOUT */

/* COFF support? */
/* #undef OBJ_MAYBE_COFF */

/* ECOFF support? */
/* #undef OBJ_MAYBE_ECOFF */

/* ELF support? */
/* #undef OBJ_MAYBE_ELF */

/* generic support? */
/* #undef OBJ_MAYBE_GENERIC */

/* HP300 support? */
/* #undef OBJ_MAYBE_HP300 */

/* IEEE support? */
/* #undef OBJ_MAYBE_IEEE */

/* SOM support? */
/* #undef OBJ_MAYBE_SOM */

/* VMS support? */
/* #undef OBJ_MAYBE_VMS */

/* Use emulation support? */
/* #undef USE_EMULATIONS */

/* Supported emulations. */
#define EMULATIONS 

/* Default emulation. */
#define DEFAULT_EMULATION ""

/* old COFF support? */
/* #undef MANY_SEGMENTS */

/* Use BFD interface? */
#define BFD_ASSEMBLER 1

/* Target alias. */
#define TARGET_ALIAS "alpha-unknown-freebsd5.0"

/* Canonical target. */
#define TARGET_CANONICAL "alpha-unknown-freebsd5.0"

/* Target CPU. */
#define TARGET_CPU "alpha"

/* Target vendor. */
#define TARGET_VENDOR "unknown"

/* Target OS. */
#define TARGET_OS "freebsd5.0"

/* Define if you have the stpcpy function */
/* #undef HAVE_STPCPY */

/* Define if your locale.h file contains LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1

/* Define to 1 if NLS is requested */
/* #define ENABLE_NLS 1 */

/* Define as 1 if you have gettext and don't want to use GNU gettext. */
/* #undef HAVE_GETTEXT */

/* Compiling cross-assembler? */
/* #undef CROSS_COMPILE */

/* assert broken? */
/* #undef BROKEN_ASSERT */

/* Define if strstr is not declared in system header files. */
/* #undef NEED_DECLARATION_STRSTR */

/* Define if malloc is not declared in system header files. */
/* #undef NEED_DECLARATION_MALLOC */

/* Define if free is not declared in system header files. */
/* #undef NEED_DECLARATION_FREE */

/* Define if sbrk is not declared in system header files. */
/* #undef NEED_DECLARATION_SBRK */

/* Define if environ is not declared in system header files. */
#define NEED_DECLARATION_ENVIRON 1

/* Define if errno is not declared in system header files. */
/* #undef NEED_DECLARATION_ERRNO */

