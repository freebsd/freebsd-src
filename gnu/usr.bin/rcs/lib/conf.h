/* RCS compile-time configuration */

	/* $FreeBSD$ */

/*
 * This file is generated automatically.
 * If you edit it by hand your changes may be lost.
 * Instead, please try to fix conf.sh,
 * and send your fixes to rcs-bugs@cs.purdue.edu.
 */

#define exitmain(n) return n /* how to exit from main() */
/* #define _POSIX_C_SOURCE 2147483647L */ /* if strict C + Posix 1003.1b-1993 or later */
/* #define _POSIX_SOURCE */ /* if strict C + Posix 1003.1-1990 */

#include <errno.h>
#include <stdio.h>
#include <time.h>

/* Comment out #include lines below that do not work.  */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
/* #include <mach/mach.h> */
/* #include <net/errno.h> */
#include <pwd.h>
/* #include <siginfo.h> */
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
/* #include <ucontext.h> */
#include <unistd.h>
#include <utime.h>
/* #include <vfork.h> */

/* Define boolean symbols to be 0 (false, the default), or 1 (true).  */
#define has_sys_param_h 1 /* Does #include <sys/param.h> work?  */
/* extern int errno; */ /* Uncomment if <errno.h> doesn't declare errno.  */
#define has_readlink 1 /* Does readlink() work?  */
#define readlink_isreg_errno EINVAL /* errno after readlink on regular file */

#if has_readlink && !defined(MAXSYMLINKS)
#	if has_sys_param_h
#		include <sys/param.h>
#	endif
#	ifndef MAXSYMLINKS
#		define MAXSYMLINKS 20 /* BSD; not standard yet */
#	endif
#endif

/* Comment out the typedefs below if the types are already declared.  */
/* Fix any uncommented typedefs that are wrong.  */
/* typedef int mode_t; */
/* typedef long off_t; */
/* typedef int pid_t; */
/* typedef int sig_atomic_t; */
/* typedef unsigned size_t; */
/* typedef int ssize_t; */
/* typedef long time_t; */
/* typedef int uid_t; */

/* Comment out the keyword definitions below if the keywords work.  */
/* #define const */
/* #define volatile */

/* Define boolean symbols to be 0 (false, the default), or 1 (true).  */
#define has_prototypes 1 /* Do function prototypes work?  */
#define has_stdarg 1 /* Does <stdarg.h> work?  */
/* #define has_varargs ? */ /* Does <varargs.h> work?  */
#define va_start_args 2 /* How many args does va_start() take?  */

#if O_BINARY
	/* Text and binary i/o behave differently.  */
	/* This is incompatible with Posix and Unix.  */
#	define FOPEN_RB "rb"
#	define FOPEN_R_WORK (Expand==BINARY_EXPAND ? "r" : "rb")
#	define FOPEN_WB "wb"
#	define FOPEN_W_WORK (Expand==BINARY_EXPAND ? "w" : "wb")
#	define FOPEN_WPLUS_WORK (Expand==BINARY_EXPAND ? "w+" : "w+b")
#	define OPEN_O_BINARY O_BINARY
#else
	/*
	* Text and binary i/o behave the same.
	* Omit "b", since some nonstandard hosts reject it.
	*/
#	define FOPEN_RB "r"
#	define FOPEN_R_WORK "r"
#	define FOPEN_WB "w"
#	define FOPEN_W_WORK "w"
#	define FOPEN_WPLUS_WORK "w+"
#	define OPEN_O_BINARY 0
#endif

/* This may need changing on non-Unix systems (notably DOS).  */
#define OPEN_CREAT_READONLY (S_IRUSR|S_IRGRP|S_IROTH) /* lock file mode */
#define OPEN_O_LOCK 0 /* extra open flags for creating lock file */
#define OPEN_O_WRONLY O_WRONLY /* main open flag for creating a lock file */

/* Define or comment out the following symbols as needed.  */
#if has_prototypes
#	define P(params) params
#else
#	define P(params) ()
#endif
#if has_stdarg
#	include <stdarg.h>
#else
#	if has_varargs
#		include <varargs.h>
#	else
		typedef char *va_list;
#		define va_dcl int va_alist;
#		define va_start(ap) ((ap) = (va_list)&va_alist)
#		define va_arg(ap,t) (((t*) ((ap)+=sizeof(t)))  [-1])
#		define va_end(ap)
#	endif
#endif
#if va_start_args == 2
#	define vararg_start va_start
#else
#	define vararg_start(ap,p) va_start(ap)
#endif
#define bad_chmod_close 0 /* Can chmod() close file descriptors?  */
#define bad_creat0 0 /* Do writes fail after creat(f,0)?  */
#define bad_fopen_wplus 0 /* Does fopen(f,"w+") fail to truncate f?  */
#define getlogin_is_secure 0 /* Is getlogin() secure?  Usually it's not.  */
#define has_attribute_noreturn 1 /* Does __attribute__((noreturn)) work?  */
#if has_attribute_noreturn
#	define exiting __attribute__((noreturn))
#else
#	define exiting
#endif
#define has_dirent 1 /* Do opendir(), readdir(), closedir() work?  */
#define void_closedir 0 /* Does closedir() yield void?  */
#define has_fchmod 1 /* Does fchmod() work?  */
#define has_fflush_input 0 /* Does fflush() work on input files?  */
#define has_fputs 1 /* Does fputs() work?  */
#define has_ftruncate 1 /* Does ftruncate() work?  */
#define has_getuid 1 /* Does getuid() work?  */
#define has_getpwuid 1 /* Does getpwuid() work?  */
#define has_memcmp 1 /* Does memcmp() work?  */
#define has_memcpy 1 /* Does memcpy() work?  */
#define has_memmove 1 /* Does memmove() work?  */
#define has_map_fd 0 /* Does map_fd() work?  */
#define has_mmap 1 /* Does mmap() work on regular files?  */
#define has_madvise 0 /* Does madvise() work?  */
#define mmap_signal SIGBUS /* signal received if you reference nonexistent part of mmapped file */
#define has_rename 1 /* Does rename() work?  */
#define bad_a_rename 0 /* Does rename(A,B) fail if A is unwritable?  */
#define bad_b_rename 0 /* Does rename(A,B) fail if B is unwritable?  */
#define bad_NFS_rename 0 /* Can rename(A,B) falsely report success?  */
/* typedef int void; */ /* Some ancient compilers need this.  */
#define VOID (void) /* 'VOID e;' discards the value of an expression 'e'.  */
#define has_seteuid 1 /* Does seteuid() work?  See ../INSTALL.RCS.  */
#define has_setreuid 0 /* Does setreuid() work?  See ../INSTALL.RCS.  */
#define has_setuid 1 /* Does setuid() exist?  */
#define has_sigaction 1 /* Does struct sigaction work?  */
#define has_sa_sigaction 0 /* Does struct sigaction have sa_sigaction?  */
#define has_signal 1 /* Does signal() work?  */
#define signal_type void /* type returned by signal handlers */
#define sig_zaps_handler 0 /* Must a signal handler reinvoke signal()?  */
/* #define has_sigblock ? */ /* Does sigblock() work?  */
/* #define sigmask(s) (1 << ((s)-1)) */ /* Yield mask for signal number.  */
typedef size_t fread_type; /* type returned by fread() and fwrite() */
typedef size_t freadarg_type; /* type of their size arguments */
typedef void *malloc_type; /* type returned by malloc() */
#define has_getcwd 1 /* Does getcwd() work?  */
/* #define has_getwd ? */ /* Does getwd() work?  */
#define needs_getabsname 0 /* Must we define getabsname?  */
#define has_mktemp 1 /* Does mktemp() work?  */
#define has_mkstemp 1 /* DOes mkstemp() work?  */
#define has_NFS 1 /* Might NFS be used?  */
#define has_psiginfo 0 /* Does psiginfo() work?  */
#define has_psignal 1 /* Does psignal() work?  */
/* #define has_si_errno ? */ /* Does siginfo_t have si_errno?  */
/* #define has_sys_siglist ? */ /* Does sys_siglist[] work?  */
/* #define strchr index */ /* Use old-fashioned name for strchr()?  */
/* #define strrchr rindex */ /* Use old-fashioned name for strrchr()?  */
#define bad_unlink 0 /* Does unlink() fail on unwritable files?  */
#define has_vfork 1 /* Does vfork() work?  */
#define has_fork 1 /* Does fork() work?  */
#define has_spawn 0 /* Does spawn*() work?  */
#define has_waitpid 1 /* Does waitpid() work?  */
#define bad_wait_if_SIGCHLD_ignored 0 /* Does ignoring SIGCHLD break wait()?  */
#define RCS_SHELL "/bin/sh" /* shell to run RCS subprograms */
#define has_printf_dot 1 /* Does "%.2d" print leading 0?  */
#define has_vfprintf 1 /* Does vfprintf() work?  */
#define has_attribute_format_printf 1 /* Does __attribute__((format(printf,N,N+1))) work?  */
#if has_attribute_format_printf
#	define printf_string(m, n) __attribute__((format(printf, m, n)))
#else
#	define printf_string(m, n)
#endif
#if has_attribute_format_printf && has_attribute_noreturn
	/* Work around a bug in GCC 2.5.x.  */
#	define printf_string_exiting(m, n) __attribute__((format(printf, m, n), noreturn))
#else
#	define printf_string_exiting(m, n) printf_string(m, n) exiting
#endif
/* #define has__doprintf ? */ /* Does _doprintf() work?  */
/* #define has__doprnt ? */ /* Does _doprnt() work?  */
/* #undef EXIT_FAILURE */ /* Uncomment this if EXIT_FAILURE is broken.  */
#define large_memory 1 /* Can main memory hold entire RCS files?  */
#ifndef LONG_MAX
#define LONG_MAX 2147483647L /* long maximum */
#endif
/* Do struct stat s and t describe the same file?  Answer d if unknown.  */
#define same_file(s,t,d) ((s).st_ino==(t).st_ino && (s).st_dev==(t).st_dev)
#define has_utimbuf 1 /* Does struct utimbuf work?  */
#define CO "/usr/bin/co" /* name of 'co' program */
#define COMPAT2 0 /* Are version 2 files supported?  */
#define DIFF "/usr/bin/diff" /* name of 'diff' program */
#define DIFF3 "/usr/bin/diff3" /* name of 'diff3' program */
#define DIFF3_BIN 1 /* Is diff3 user-visible (not the /usr/lib auxiliary)?  */
#define DIFFFLAGS "-an" /* Make diff output suitable for RCS.  */
#define DIFF_L 1 /* Does diff -L work?  */
#define DIFF_SUCCESS 0 /* DIFF status if no differences are found */
#define DIFF_FAILURE 1 /* DIFF status if differences are found */
#define DIFF_TROUBLE 2 /* DIFF status if trouble */
#define ED "/bin/ed" /* name of 'ed' program (used only if !DIFF3_BIN) */
#define MERGE "/usr/bin/merge" /* name of 'merge' program */
#define TMPDIR "/tmp" /* default directory for temporary files */
#define SLASH '/' /* principal filename separator */
#define SLASHes '/' /* `case SLASHes:' labels all filename separators */
#define isSLASH(c) ((c) == SLASH) /* Is arg a filename separator?  */
#define ROOTPATH(p) isSLASH((p)[0]) /* Is p an absolute pathname?  */
#define X_DEFAULT ",v/" /* default value for -x option */
#define SLASHSLASH_is_SLASH 1 /* Are // and / the same directory?  */
#define ALL_ABSOLUTE 1 /* Do all subprograms satisfy ROOTPATH?  */
#define DIFF_ABSOLUTE 1 /* Is ROOTPATH(DIFF) true?  */
#define SENDMAIL "/usr/sbin/sendmail" /* how to send mail */
#define TZ_must_be_set 0 /* Must TZ be set for gmtime() to work?  */



/* Adjust the following declarations as needed.  */


/* The rest is for the benefit of non-standard, traditional hosts.  */
/* Don't bother to declare functions that in traditional hosts do not appear, */
/* or are declared in .h files, or return int or void.  */


/* traditional BSD */

#if has_sys_siglist && !defined(sys_siglist)
	extern char const * const sys_siglist[];
#endif


/* Posix (ISO/IEC 9945-1: 1990 / IEEE Std 1003.1-1990) */

/* <fcntl.h> */
#ifdef O_CREAT
#	define open_can_creat 1
#else
#	define open_can_creat 0
#	define O_RDONLY 0
#	define O_WRONLY 1
#	define O_RDWR 2
#	define O_CREAT 01000
#	define O_TRUNC 02000
#endif
#ifndef O_EXCL
#define O_EXCL 0
#endif

/* <sys/stat.h> */
#ifndef S_IRUSR
#	ifdef S_IREAD
#		define S_IRUSR S_IREAD
#	else
#		define S_IRUSR 0400
#	endif
#	ifdef S_IWRITE
#		define S_IWUSR S_IWRITE
#	else
#		define S_IWUSR (S_IRUSR/2)
#	endif
#endif
#ifndef S_IRGRP
#	if has_getuid
#		define S_IRGRP (S_IRUSR / 0010)
#		define S_IWGRP (S_IWUSR / 0010)
#		define S_IROTH (S_IRUSR / 0100)
#		define S_IWOTH (S_IWUSR / 0100)
#	else
		/* single user OS -- not Posix or Unix */
#		define S_IRGRP 0
#		define S_IWGRP 0
#		define S_IROTH 0
#		define S_IWOTH 0
#	endif
#endif
#ifndef S_ISREG
#define S_ISREG(n) (((n) & S_IFMT) == S_IFREG)
#endif

/* <sys/wait.h> */
#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#undef WIFEXITED /* Avoid 4.3BSD incompatibility with Posix.  */
#endif
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val)  &  0377) == 0)
#endif
#ifndef WTERMSIG
#define WTERMSIG(stat_val) ((stat_val) & 0177)
#undef WIFSIGNALED /* Avoid 4.3BSD incompatibility with Posix.  */
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(stat_val) ((unsigned)(stat_val) - 1  <  0377)
#endif

/* <unistd.h> */
char *getlogin P((void));
#ifndef STDIN_FILENO
#	define STDIN_FILENO 0
#	define STDOUT_FILENO 1
#	define STDERR_FILENO 2
#endif
#if has_fork && !has_vfork
#	undef vfork
#	define vfork fork
#endif
#if has_getcwd || !has_getwd
	char *getcwd P((char*,size_t));
#else
	char *getwd P((char*));
#endif
#if has_setuid && !has_seteuid
#	undef seteuid
#	define seteuid setuid
#endif
#if has_spawn
#	if ALL_ABSOLUTE
#		define spawn_RCS spawnv
#	else
#		define spawn_RCS spawnvp
#	endif
#else
#	if ALL_ABSOLUTE
#		define exec_RCS execv
#	else
#		define exec_RCS execvp
#	endif
#endif

/* utime.h */
#if !has_utimbuf
	struct utimbuf { time_t actime, modtime; };
#endif


/* Standard C library */

/* <stdio.h> */
#ifndef L_tmpnam
#define L_tmpnam 32 /* power of 2 > sizeof("/usr/tmp/xxxxxxxxxxxxxxx") */
#endif
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#if has_mktemp
	char *mktemp P((char*)); /* traditional */
#else
	char *tmpnam P((char*));
#endif

/* <stdlib.h> */
char *getenv P((char const*));
void _exit P((int)) exiting;
void exit P((int)) exiting;
malloc_type malloc P((size_t));
malloc_type realloc P((malloc_type,size_t));
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

/* <string.h> */
char *strcpy P((char*,char const*));
char *strchr P((char const*,int));
char *strrchr P((char const*,int));
void *memcpy P((void*,void const*,size_t));
#if has_memmove
	void *memmove P((void*,void const*,size_t));
#endif

/* <time.h> */
time_t time P((time_t*));
