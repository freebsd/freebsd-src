/* RCS compile-time configuration */

	/* $Id: conf.sh,v 5.14 1991/11/20 18:21:10 eggert Exp $ */

/*
 * This file is generated automatically.
 * If you edit it by hand your changes may be lost.
 * Instead, please try to fix conf.sh,
 * and send your fixes to rcs-bugs@cs.purdue.edu.
 */

#define exitmain(n) return n /* how to exit from main() */
/* #define _POSIX_SOURCE */ /* Define this if Posix + strict Standard C.  */

#include <errno.h>
#include <stdio.h>
#include <time.h>

/* Comment out #include lines below that do not work.  */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>
/* #include <vfork.h> */

/* Define the following symbols to be 1 or 0.  */
#define has_sys_dir_h 1 /* Does #include <sys/dir.h> work?  */
#define has_sys_param_h 1 /* Does #include <sys/param.h> work?  */
#define has_readlink 1 /* Does readlink() work?  */

/* #undef NAME_MAX */ /* Uncomment this if NAME_MAX is broken.  */

#if !defined(NAME_MAX) && !defined(_POSIX_NAME_MAX)
#	if has_sys_dir_h
#		include <sys/dir.h>
#	endif
#	ifndef NAME_MAX
#		ifndef MAXNAMLEN
#			define MAXNAMLEN 14
#		endif
#		define NAME_MAX MAXNAMLEN
#	endif
#endif
#if !defined(PATH_MAX) && !defined(_POSIX_PATH_MAX)
#	if has_sys_param_h
#		include <sys/param.h>
#		define included_sys_param_h 1
#	endif
#	ifndef PATH_MAX
#		ifndef MAXPATHLEN
#			define MAXPATHLEN 1024
#		endif
#		define PATH_MAX (MAXPATHLEN-1)
#	endif
#endif
#if has_readlink && !defined(MAXSYMLINKS)
#	if has_sys_param_h && !included_sys_param_h
#		include <sys/param.h>
#	endif
#	ifndef MAXSYMLINKS
#		define MAXSYMLINKS 20 /* BSD; not standard yet */
#	endif
#endif

/* Comment out the keyword definitions below if the keywords work.  */
/* #define const */
/* #define volatile */

/* Comment out the typedefs below if the types are already declared.  */
/* Fix any uncommented typedefs that are wrong.  */
/* typedef int mode_t; */
/* typedef int pid_t; */
typedef int sig_atomic_t;
/* typedef unsigned size_t; */
/* typedef int ssize_t; */
/* typedef long time_t; */
/* typedef int uid_t; */

/* Define the following symbols to be 1 or 0.  */
#define has_prototypes 1 /* Do function prototypes work?  */
#define has_stdarg 1 /* Does <stdarg.h> work?  */
#define has_varargs 0 /* Does <varargs.h> work?  */
#define va_start_args 2 /* How many args does va_start() take?  */
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

#define text_equals_binary_stdio 1 /* Does stdio treat text like binary?  */
#define text_work_stdio 0 /* Text i/o for working file, binary for RCS file?  */
#if text_equals_binary_stdio
	/* Text and binary i/o behave the same, or binary i/o does not work.  */
#	define FOPEN_R "r"
#	define FOPEN_W "w"
#	define FOPEN_WPLUS "w+"
#else
	/* Text and binary i/o behave differently.  */
	/* This is incompatible with Posix and Unix.  */
#	define FOPEN_R "rb"
#	define FOPEN_W "wb"
#	define FOPEN_WPLUS "w+b"
#endif
#if text_work_stdio
#	define FOPEN_R_WORK "r"
#	define FOPEN_W_WORK "w"
#	define FOPEN_WPLUS_WORK "w+"
#else
#	define FOPEN_R_WORK FOPEN_R
#	define FOPEN_W_WORK FOPEN_W
#	define FOPEN_WPLUS_WORK FOPEN_WPLUS
#endif

/* Define or comment out the following symbols as needed.  */
#define bad_fopen_wplus 0 /* Does fopen(f,FOPEN_WPLUS) fail to truncate f?  */
#define getlogin_is_secure 0 /* Is getlogin() secure?  Usually it's not.  */
#define has_dirent 1 /* Do opendir(), readdir(), closedir() work?  */
#define has_fchmod 0 /* Does fchmod() work?  */
#define has_fputs 0 /* Does fputs() work?  */
#define has_ftruncate 1 /* Does ftruncate() work?  */
#define has_getuid 1 /* Does getuid() work?  */
#define has_getpwuid 1 /* Does getpwuid() work?  */
#define has_link 1 /* Does link() work?  */
#define has_memcmp 1 /* Does memcmp() work?  */
#define has_memcpy 1 /* Does memcpy() work?  */
#define has_memmove 1 /* Does memmove() work?  */
#define has_madvise 0 /* Does madvise() work?  */
#define has_mmap 0 /* Does mmap() work on regular files?  */
#define has_rename 1 /* Does rename() work?  */
#define bad_a_rename 0 /* Does rename(A,B) fail if A is unwritable?  */
#define bad_b_rename 0 /* Does rename(A,B) fail if B is unwritable?  */
#define VOID (void) /* 'VOID e;' discards the value of an expression 'e'.  */
#define has_seteuid 0 /* Does seteuid() work?  See README.  */
#define has_setuid 1 /* Does setuid() exist?  */
#define has_signal 1 /* Does signal() work?  */
#define signal_args P((int)) /* arguments of signal handlers */
#define signal_type void /* type returned by signal handlers */
#define sig_zaps_handler 0 /* Must a signal handler reinvoke signal()?  */
#define has_sigaction 1 /* Does struct sigaction work?  */
/* #define has_sigblock ? */ /* Does sigblock() work?  */
/* #define sigmask(s) (1 << ((s)-1)) */ /* Yield mask for signal number.  */
#define has_sys_siglist 0 /* Does sys_siglist[] work?  */
typedef ssize_t fread_type; /* type returned by fread() and fwrite() */
typedef size_t freadarg_type; /* type of their size arguments */
typedef void *malloc_type; /* type returned by malloc() */
#define has_getcwd 1 /* Does getcwd() work?  */
/* #define has_getwd ? */ /* Does getwd() work?  */
#define has_mktemp 1 /* Does mktemp() work?  */
#define has_NFS 1 /* Might NFS be used?  */
/* #define strchr index */ /* Use old-fashioned name for strchr()?  */
/* #define strrchr rindex */ /* Use old-fashioned name for strrchr()?  */
#define bad_unlink 0 /* Does unlink() fail on unwritable files?  */
#define has_vfork 0 /* Does vfork() work?  */
#define has_fork 1 /* Does fork() work?  */
#define has_spawn 0 /* Does spawn*() work?  */
#define has_wait 1 /* Does wait() work?  */
#define has_waitpid 0 /* Does waitpid() work?  */
#define RCS_SHELL "/bin/sh" /* shell to run RCS subprograms */
#define has_vfprintf 1 /* Does vfprintf() work?  */
/* #define has__doprintf ? */ /* Does _doprintf() work?  */
/* #define has__doprnt ? */ /* Does _doprnt() work?  */
/* #undef EXIT_FAILURE */ /* Uncomment this if EXIT_FAILURE is broken.  */
#define large_memory 0 /* Can main memory hold entire RCS files?  */
/* #undef ULONG_MAX */ /* Uncomment this if ULONG_MAX is broken (e.g. < 0).  */
/* struct utimbuf { time_t actime, modtime; }; */ /* Uncomment this if needed.  */
#define CO "/usr/bin/co" /* name of 'co' program */
#define COMPAT2 0 /* Are version 2 files supported?  */
#define DATEFORM "%.2d.%.2d.%.2d.%.2d.%.2d.%.2d" /* e.g. 01.01.01.01.01.01 */
#define DIFF "/usr/bin/diff" /* name of 'diff' program */
#define DIFF3 "/usr/bin/diff3" /* name of 'diff3' program */
#define DIFF3_BIN 1 /* Is diff3 user-visible (not the /usr/lib auxiliary)?  */
#define DIFF_FLAGS , "-an" /* Make diff output suitable for RCS.  */
#define DIFF_L 1 /* Does diff -L work? */
#define DIFF_SUCCESS 0 /* DIFF status if no differences are found */
#define DIFF_FAILURE 1 /* DIFF status if differences are found */
#define DIFF_TROUBLE 2 /* DIFF status if trouble */
#define ED "/bin/ed" /* name of 'ed' program (used only if !DIFF3_BIN) */
#define MERGE "/usr/bin/merge" /* name of 'merge' program */
#define TMPDIR "/tmp" /* default directory for temporary files */
#define SLASH '/' /* principal pathname separator */
#define SLASHes '/' /* `case SLASHes:' labels all pathname separators */
#define isSLASH(c) ((c) == SLASH) /* Is arg a pathname separator?  */
#define ROOTPATH(p) isSLASH((p)[0]) /* Is p an absolute pathname?  */
#define X_DEFAULT ",v/" /* default value for -x option */
#define DIFF_ABSOLUTE 1 /* Is ROOTPATH(DIFF) true?  */
#define ALL_ABSOLUTE 1 /* Are all subprograms absolute pathnames?  */
#define SENDMAIL "/usr/bin/mail" /* how to send mail */
#define TZ_must_be_set 0 /* Must TZ be set for gmtime() to work?  */



/* Adjust the following declarations as needed.  */


#if __GNUC__ && !__STRICT_ANSI__
#	define exiting volatile /* GCC extension: function cannot return */
#else
#	define exiting
#endif

#if has_ftruncate
	int ftruncate P((int,off_t));
#endif

/* <sys/mman.h> */
#if has_madvise
	int madvise P((caddr_t,size_t,int));
#endif
#if has_mmap
	caddr_t mmap P((caddr_t,size_t,int,int,int,off_t));
	int munmap P((caddr_t,size_t));
#endif


/* Posix (ISO/IEC 9945-1: 1990 / IEEE Std 1003.1-1990) */
/* These definitions are for the benefit of non-Posix hosts, and */
/* Posix hosts that have Standard C compilers but traditional include files.  */
/* Unfortunately, mixed-up hosts are all too common.  */

/* <fcntl.h> */
#ifdef F_DUPFD
	int fcntl P((int,int,...));
#else
	int dup2 P((int,int));
#endif
#ifndef O_BINARY /* some non-Posix hosts need O_BINARY */
#	define O_BINARY 0 /* no effect on Posix */
#endif
#ifdef O_CREAT
#	define open_can_creat 1
#else
#	define open_can_creat 0
#	define O_RDONLY 0
#	define O_WRONLY 1
#	define O_RDWR 2
#	define O_CREAT 01000
#	define O_TRUNC 02000
	int creat P((char const*,mode_t));
#endif
#ifndef O_EXCL
#	define O_EXCL 0
#endif

/* <pwd.h> */
#if has_getpwuid
	struct passwd *getpwuid P((uid_t));
#endif

/* <signal.h> */
#if has_sigaction
	int sigaction P((int,struct sigaction const*,struct sigaction*));
	int sigaddset P((sigset_t*,int));
	int sigemptyset P((sigset_t*));
#else
#if has_sigblock
	/* BSD */
	int sigblock P((int));
	int sigmask P((int));
	int sigsetmask P((int));
#endif
#endif

/* <stdio.h> */
FILE *fdopen P((int,char const*));
int fileno P((FILE*));

/* <sys/stat.h> */
int chmod P((char const*,mode_t));
int fstat P((int,struct stat*));
int stat P((char const*,struct stat*));
mode_t umask P((mode_t));
#if has_fchmod
	int fchmod P((int,mode_t));
#endif
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
#	define S_ISREG(n) (((n) & S_IFMT) == S_IFREG)
#endif

/* <sys/wait.h> */
#if has_wait
	pid_t wait P((int*));
#endif
#ifndef WEXITSTATUS
#	define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#	undef WIFEXITED /* Avoid 4.3BSD incompatibility with Posix.  */
#endif
#ifndef WIFEXITED
#	define WIFEXITED(stat_val) (!((stat_val) & 255))
#endif

/* <unistd.h> */
char *getlogin P((void));
int close P((int));
int isatty P((int));
int link P((char const*,char const*));
int open P((char const*,int,...));
int unlink P((char const*));
int _filbuf P((FILE*)); /* keeps lint quiet in traditional C */
int _flsbuf P((int,FILE*)); /* keeps lint quiet in traditional C */
long pathconf P((char const*,int));
ssize_t write P((int,void const*,size_t));
#ifndef STDIN_FILENO
#	define STDIN_FILENO 0
#	define STDOUT_FILENO 1
#	define STDERR_FILENO 2
#endif
#if has_fork
#	if !has_vfork
#		undef vfork
#		define vfork fork
#	endif
	pid_t vfork P((void)); /* vfork is nonstandard but faster */
#endif
#if has_getcwd || !has_getwd
	char *getcwd P((char*,size_t));
#else
	char *getwd P((char*));
#endif
#if has_getuid
	uid_t getuid P((void));
#endif
#if has_readlink
/* 	ssize_t readlink P((char const*,char*,size_t));  *//* BSD; not standard yet */
#endif
#if has_setuid
#	if !has_seteuid
#		undef seteuid
#		define seteuid setuid
#	endif
	int seteuid P((uid_t));
	uid_t geteuid P((void));
#endif
#if has_spawn
	int spawnv P((int,char const*,char*const*));
#	if ALL_ABSOLUTE
#		define spawn_RCS spawnv
#	else
#		define spawn_RCS spawnvp
		int spawnvp P((int,char const*,char*const*));
#	endif
#else
	int execv P((char const*,char*const*));
#	if ALL_ABSOLUTE
#		define exec_RCS execv
#	else
#		define exec_RCS execvp
		int execvp P((char const*,char*const*));
#	endif
#endif

/* utime.h */
int utime P((char const*,struct utimbuf const*));


/* Standard C library */
/* These definitions are for the benefit of hosts that have */
/* traditional C include files, possibly with Standard C compilers.  */
/* Unfortunately, mixed-up hosts are all too common.  */

/* <errno.h> */
extern int errno;

/* <limits.h> */
#ifndef ULONG_MAX
	/* This does not work in #ifs, but it's good enough for us.  */
#	define ULONG_MAX ((unsigned long)-1)
#endif

/* <signal.h> */
#if has_signal
	signal_type (*signal P((int,signal_type(*)signal_args)))signal_args;
#endif

/* <stdio.h> */
FILE *fopen P((char const*,char const*));
fread_type fread P((void*,freadarg_type,freadarg_type,FILE*));
fread_type fwrite P((void const*,freadarg_type,freadarg_type,FILE*));
int fclose P((FILE*));
int feof P((FILE*));
int ferror P((FILE*));
int fflush P((FILE*));
int fprintf P((FILE*,char const*,...));
int fputs P((char const*,FILE*));
int fseek P((FILE*,long,int));
int printf P((char const*,...));
int rename P((char const*,char const*));
int sprintf P((char*,char const*,...));
/* long ftell P((FILE*)); */
void clearerr P((FILE*));
void perror P((char const*));
#ifndef L_tmpnam
#	define L_tmpnam 32 /* power of 2 > sizeof("/usr/tmp/xxxxxxxxxxxxxxx") */
#endif
#ifndef SEEK_SET
#	define SEEK_SET 0
#endif
#if has_mktemp
	char *mktemp P((char*)); /* traditional */
#else
	char *tmpnam P((char*));
#endif
#if has_vfprintf
	int vfprintf P((FILE*,char const*,va_list));
#else
#if has__doprintf
	void _doprintf P((FILE*,char const*,va_list)); /* Minix */
#else
	void _doprnt P((char const*,va_list,FILE*)); /* BSD */
#endif
#endif

/* <stdlib.h> */
char *getenv P((char const*));
exiting void _exit P((int));
exiting void exit P((int));
malloc_type malloc P((size_t));
malloc_type realloc P((malloc_type,size_t));
void free P((malloc_type));
#ifndef EXIT_FAILURE
#	define EXIT_FAILURE 1
#endif
#ifndef EXIT_SUCCESS
#	define EXIT_SUCCESS 0
#endif
#if !has_fork && !has_spawn
	int system P((char const*));
#endif

/* <string.h> */
char *strcpy P((char*,char const*));
char *strchr P((char const*,int));
char *strrchr P((char const*,int));
int memcmp P((void const*,void const*,size_t));
int strcmp P((char const*,char const*));
size_t strlen P((char const*));
void *memcpy P((void*,void const*,size_t));
#if has_memmove
	void *memmove P((void*,void const*,size_t));
#endif

/* <time.h> */
time_t time P((time_t*));
