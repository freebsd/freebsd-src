#define ABORT() abort();

#ifndef SH_PATH
#define SH_PATH "/bin/sh"
#endif

#ifdef DJGPP
#  define BIT_BUCKET "nul"
#  define OP_BINARY O_BINARY
#  define PERL_SYS_INIT(c,v) Perl_DJGPP_init(c,v)
#  define init_os_extras Perl_init_os_extras
#  include <signal.h>
#  define HAS_UTIME
#  define HAS_KILL
   char *djgpp_pathexp (const char*);
#  if (DJGPP==2 && DJGPP_MINOR < 2)
#    define NO_LOCALECONV_MON_THOUSANDS_SEP
#  endif
#  ifdef USE_THREADS
#    define OLD_PTHREADS_API
#  endif
#  define PERL_FS_VER_FMT	"%d_%d_%d"
#else	/* DJGPP */
#  ifdef WIN32
#    define PERL_SYS_INIT(c,v)	Perl_win32_init(c,v)
#    define BIT_BUCKET "nul"
#  else
#    define PERL_SYS_INIT(c,v)
#    define BIT_BUCKET "\\dev\\nul" /* "wanna be like, umm, Newlined, or somethin?" */
#  endif
#endif	/* DJGPP */

#define PERL_SYS_TERM() OP_REFCNT_TERM; MALLOC_TERM
#define dXSUB_SYS

/*
 * 5.003_07 and earlier keyed on #ifdef MSDOS for determining if we were 
 * running on DOS, *and* if we had to cope with 16 bit memory addressing 
 * constraints, *and* we need to have memory allocated as unsigned long.
 *
 * with the advent of *real* compilers for DOS, they are not locked together.
 * MSDOS means "I am running on MSDOS". HAS_64K_LIMIT means "I have 
 * 16 bit memory addressing constraints".
 *
 * if you need the last, try #DEFINE MEM_SIZE unsigned long.
 */
#ifdef MSDOS
 #ifndef DJGPP
  #define HAS_64K_LIMIT
 #endif
#endif

/* USEMYBINMODE
 *	This symbol, if defined, indicates that the program should
 *	use the routine my_binmode(FILE *fp, char iotype, int mode) to insure
 *	that a file is in "binary" mode -- that is, that no translation
 *	of bytes occurs on read or write operations.
 */
#undef USEMYBINMODE

/* Stat_t:
 *	This symbol holds the type used to declare buffers for information
 *	returned by stat().  It's usually just struct stat.  It may be necessary
 *	to include <sys/stat.h> and <sys/types.h> to get any typedef'ed
 *	information.
 */
#define Stat_t struct stat

/* USE_STAT_RDEV:
 *	This symbol is defined if this system has a stat structure declaring
 *	st_rdev
 */
#define USE_STAT_RDEV 	/**/

/* ACME_MESS:
 *	This symbol, if defined, indicates that error messages should be 
 *	should be generated in a format that allows the use of the Acme
 *	GUI/editor's autofind feature.
 */
#undef ACME_MESS	/**/

/* ALTERNATE_SHEBANG:
 *	This symbol, if defined, contains a "magic" string which may be used
 *	as the first line of a Perl program designed to be executed directly
 *	by name, instead of the standard Unix #!.  If ALTERNATE_SHEBANG
 *	begins with a character other then #, then Perl will only treat
 *	it as a command line if if finds the string "perl" in the first
 *	word; otherwise it's treated as the first line of code in the script.
 *	(IOW, Perl won't hand off to another interpreter via an alternate
 *	shebang sequence that might be legal Perl code.)
 */
/* #define ALTERNATE_SHEBANG "#!" / **/

/*
 * fwrite1() should be a routine with the same calling sequence as fwrite(),
 * but which outputs all of the bytes requested as a single stream (unlike
 * fwrite() itself, which on some systems outputs several distinct records
 * if the number_of_items parameter is >1).
 */
#define fwrite1 fwrite

#define Fstat(fd,bufptr)   fstat((fd),(bufptr))
#ifdef DJGPP
#   define Fflush(fp)      djgpp_fflush(fp)
#else
#   define Fflush(fp)      fflush(fp)
#endif
#define Mkdir(path,mode)   mkdir((path),(mode))

#ifndef WIN32
#  define Stat(fname,bufptr) stat((fname),(bufptr))
#else
#  define HAS_IOCTL
#  define HAS_UTIME
#  define HAS_KILL
#  define HAS_WAIT
#  define HAS_CHOWN
#endif	/* WIN32 */
