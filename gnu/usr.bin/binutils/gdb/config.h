/* config.h.  Generated automatically by configure.  */
/* config.in.  Generated automatically from configure.in by autoheader.  */

/* Whether malloc must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_MALLOC */

/* Whether realloc must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_REALLOC */

/* Whether free must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_FREE */

/* Whether strerror must be declared even if <string.h> is included.  */
/* #undef NEED_DECLARATION_STRERROR */

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

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

/* Define if the `long double' type works.  */
#define HAVE_LONG_DOUBLE 1

/* Define if you have a working `mmap' system call.  */
#define HAVE_MMAP 1

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

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

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* #undef STAT_MACROS_BROKEN */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if ioctl argument PIOCSET is available. */
/* #undef HAVE_PROCFS_PIOCSET */

/* /proc PID entries are directories containing the files
   ctl as map status */
/* #undef HAVE_MULTIPLE_PROC_FDS */

/* Define if the `long long' type works.  */
#define CC_HAS_LONG_LONG 1

/* Define if the "ll" format works to print long long ints. */
#define PRINTF_HAS_LONG_LONG 1

/* Define if the "%Lg" format works to print long doubles. */
#define PRINTF_HAS_LONG_DOUBLE 1

/* Define if the "%Lg" format works to scan long doubles. */
#define SCANF_HAS_LONG_DOUBLE 1

/* Define if using Solaris thread debugging.  */
/* #undef HAVE_THREAD_DB_LIB */

/* Define on a GNU/Linux system to work around problems in sys/procfs.h.  */
/* #undef START_INFERIOR_TRAPS_EXPECTED */
/* #undef sys_quotactl */

/* Define if you have HPUX threads */
/* #undef HAVE_HPUX_THREAD_SUPPORT */

/* Define if you want to use the memory mapped malloc package (mmalloc). */
/* #undef USE_MMALLOC */

/* Define if the runtime uses a routine from mmalloc before gdb has a chance
   to initialize mmalloc, and we want to force checking to be used anyway.
   This may cause spurious memory corruption messages if the runtime tries
   to explicitly deallocate that memory when gdb calls exit. */
/* #undef MMCHECK_FORCE */

/* Define if you want to use the full-screen terminal user interface.  */
/* #undef TUI */

/* Define if <proc_service.h> on solaris uses int instead of
   size_t, and assorted other type changes. */
/* #undef PROC_SERVICE_IS_OLD */

/* Set to true if the save_state_t structure is present */
#define HAVE_STRUCT_SAVE_STATE_T 0

/* Set to true if the save_state_t structure has the ss_wide member */
#define HAVE_STRUCT_MEMBER_SS_WIDE 0

/* Define if you have the __argz_count function.  */
/* #undef HAVE___ARGZ_COUNT */

/* Define if you have the __argz_next function.  */
/* #undef HAVE___ARGZ_NEXT */

/* Define if you have the __argz_stringify function.  */
/* #undef HAVE___ARGZ_STRINGIFY */

/* Define if you have the bcopy function.  */
#define HAVE_BCOPY 1

/* Define if you have the btowc function.  */
/* #undef HAVE_BTOWC */

/* Define if you have the bzero function.  */
#define HAVE_BZERO 1

/* Define if you have the dcgettext function.  */
/* #undef HAVE_DCGETTEXT */

/* Define if you have the getcwd function.  */
#define HAVE_GETCWD 1

/* Define if you have the getpagesize function.  */
#define HAVE_GETPAGESIZE 1

/* Define if you have the isascii function.  */
#define HAVE_ISASCII 1

/* Define if you have the munmap function.  */
#define HAVE_MUNMAP 1

/* Define if you have the putenv function.  */
#define HAVE_PUTENV 1

/* Define if you have the sbrk function.  */
#define HAVE_SBRK 1

/* Define if you have the setenv function.  */
#define HAVE_SETENV 1

/* Define if you have the setlocale function.  */
#define HAVE_SETLOCALE 1

/* Define if you have the setpgid function.  */
#define HAVE_SETPGID 1

/* Define if you have the sigaction function.  */
#define HAVE_SIGACTION 1

/* Define if you have the stpcpy function.  */
/* #undef HAVE_STPCPY */

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the <argz.h> header file.  */
/* #undef HAVE_ARGZ_H */

/* Define if you have the <asm/debugreg.h> header file.  */
/* #undef HAVE_ASM_DEBUGREG_H */

/* Define if you have the <ctype.h> header file.  */
#define HAVE_CTYPE_H 1

/* Define if you have the <curses.h> header file.  */
#define HAVE_CURSES_H 1

/* Define if you have the <endian.h> header file.  */
/* #undef HAVE_ENDIAN_H */

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <link.h> header file.  */
#define HAVE_LINK_H 1

/* Define if you have the <locale.h> header file.  */
#define HAVE_LOCALE_H 1

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <nl_types.h> header file.  */
#define HAVE_NL_TYPES_H 1

/* Define if you have the <objlist.h> header file.  */
/* #undef HAVE_OBJLIST_H */

/* Define if you have the <ptrace.h> header file.  */
/* #undef HAVE_PTRACE_H */

/* Define if you have the <sgtty.h> header file.  */
#define HAVE_SGTTY_H 1

/* Define if you have the <stddef.h> header file.  */
#define HAVE_STDDEF_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/debugreg.h> header file.  */
/* #undef HAVE_SYS_DEBUGREG_H */

/* Define if you have the <sys/param.h> header file.  */
#define HAVE_SYS_PARAM_H 1

/* Define if you have the <sys/procfs.h> header file.  */
#define HAVE_SYS_PROCFS_H 1

/* Define if you have the <sys/ptrace.h> header file.  */
#define HAVE_SYS_PTRACE_H 1

/* Define if you have the <sys/reg.h> header file.  */
/* #undef HAVE_SYS_REG_H */

/* Define if you have the <sys/wait.h> header file.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have the <term.h> header file.  */
#define HAVE_TERM_H 1

/* Define if you have the <termio.h> header file.  */
/* #undef HAVE_TERMIO_H */

/* Define if you have the <termios.h> header file.  */
#define HAVE_TERMIOS_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <values.h> header file.  */
/* #undef HAVE_VALUES_H */

/* Define if you have the <wait.h> header file.  */
/* #undef HAVE_WAIT_H */

/* Define if you have the <wchar.h> header file.  */
/* #undef HAVE_WCHAR_H */

/* Define if you have the <wctype.h> header file.  */
/* #undef HAVE_WCTYPE_H */

/* Define if you have the dl library (-ldl).  */
/* #undef HAVE_LIBDL */

/* Define if you have the m library (-lm).  */
#define HAVE_LIBM 1

/* Define if you have the w library (-lw).  */
/* #undef HAVE_LIBW */

/* Define if you have the stpcpy function */
/* #undef HAVE_STPCPY */

/* Define if your locale.h file contains LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1

/* Define to 1 if NLS is requested */
#define ENABLE_NLS 1

/* Define as 1 if you have gettext and don't want to use GNU gettext. */
/* #undef HAVE_GETTEXT */

/* Define if malloc is not declared in system header files. */
/* #undef NEED_DECLARATION_MALLOC */

/* Define if realloc is not declared in system header files. */
/* #undef NEED_DECLARATION_REALLOC */

/* Define if free is not declared in system header files. */
/* #undef NEED_DECLARATION_FREE */

/* Define if strerror is not declared in system header files. */
/* #undef NEED_DECLARATION_STRERROR */

/* Define if strdup is not declared in system header files. */
/* #undef NEED_DECLARATION_STRDUP */

/* Define if <sys/procfs.h> has pstatus_t. */
/* #undef HAVE_PSTATUS_T */

/* Define if <sys/procfs.h> has prrun_t. */
/* #undef HAVE_PRRUN_T */

/* Define if <sys/procfs.h> has gregset_t. */
#define HAVE_GREGSET_T 1

/* Define if <sys/procfs.h> has fpregset_t. */
#define HAVE_FPREGSET_T 1

