/* $RCSfile: perl.h,v $$Revision: 1.2 $$Date: 1995/05/30 05:03:11 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: perl.h,v $
 * Revision 1.2  1995/05/30 05:03:11  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1994/09/10  06:27:35  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:35  nate
 * PERL!
 *
 * Revision 4.0.1.7  1993/02/05  19:40:30  lwall
 * patch36: worked around certain busted compilers that don't init statics right
 *
 * Revision 4.0.1.6  92/06/08  14:55:10  lwall
 * patch20: added Atari ST portability
 * patch20: bcopy() and memcpy() now tested for overlap safety
 * patch20: Perl now distinguishes overlapped copies from non-overlapped
 * patch20: removed implicit int declarations on functions
 *
 * Revision 4.0.1.5  91/11/11  16:41:07  lwall
 * patch19: uts wrongly defines S_ISDIR() et al
 * patch19: too many preprocessors can't expand a macro right in #if
 * patch19: added little-endian pack/unpack options
 *
 * Revision 4.0.1.4  91/11/05  18:06:10  lwall
 * patch11: various portability fixes
 * patch11: added support for dbz
 * patch11: added some support for 64-bit integers
 * patch11: hex() didn't understand leading 0x
 *
 * Revision 4.0.1.3  91/06/10  01:25:10  lwall
 * patch10: certain pattern optimizations were botched
 *
 * Revision 4.0.1.2  91/06/07  11:28:33  lwall
 * patch4: new copyright notice
 * patch4: made some allowances for "semi-standard" C
 * patch4: many, many itty-bitty portability fixes
 *
 * Revision 4.0.1.1  91/04/11  17:49:51  lwall
 * patch1: hopefully straightened out some of the Xenix mess
 *
 * Revision 4.0  91/03/20  01:37:56  lwall
 * 4.0 baseline.
 *
 */

#define VOIDWANT 1
#include "config.h"

#ifdef MYMALLOC
#   ifdef HIDEMYMALLOC
#	define malloc Mymalloc
#	define realloc Myremalloc
#	define free Myfree
#   endif
#   define safemalloc malloc
#   define saferealloc realloc
#   define safefree free
#endif

/* work around some libPW problems */
#define fatal Myfatal
#ifdef DOINIT
char Error[1];
#endif

/* define this once if either system, instead of cluttering up the src */
#if defined(MSDOS) || defined(atarist)
#define DOSISH 1
#endif

#ifdef DOSISH
/* This stuff now in the MS-DOS config.h file. */
#else /* !MSDOS */

/*
 * The following symbols are defined if your operating system supports
 * functions by that name.  All Unixes I know of support them, thus they
 * are not checked by the configuration script, but are directly defined
 * here.
 */
#define HAS_ALARM
#define HAS_CHOWN
#define HAS_CHROOT
#define HAS_FORK
#define HAS_GETLOGIN
#define HAS_GETPPID
#define HAS_KILL
#define HAS_LINK
#define HAS_PIPE
#define HAS_WAIT
#define HAS_UMASK
/*
 * The following symbols are defined if your operating system supports
 * password and group functions in general.  All Unix systems do.
 */
#define HAS_GROUP
#define HAS_PASSWD

#endif /* !MSDOS */

#if defined(__STDC__) || defined(_AIX) || defined(__stdc__)
# define STANDARD_C 1
#endif

#if defined(HASVOLATILE) || defined(STANDARD_C)
#define VOLATILE volatile
#else
#define VOLATILE
#endif

#ifdef IAMSUID
#   ifndef TAINT
#	define TAINT
#   endif
#endif

#ifndef HAS_VFORK
#   define vfork fork
#endif

#ifdef HAS_GETPGRP2
#   ifndef HAS_GETPGRP
#	define HAS_GETPGRP
#   endif
#   define getpgrp getpgrp2
#endif

#ifdef HAS_SETPGRP2
#   ifndef HAS_SETPGRP
#	define HAS_SETPGRP
#   endif
#   define setpgrp setpgrp2
#endif

#include <stdio.h>
#include <ctype.h>
#include <setjmp.h>
#ifndef MSDOS
#ifdef PARAM_NEEDS_TYPES
#include <sys/types.h>
#endif
#include <sys/param.h>
#endif
#ifdef STANDARD_C
/* Use all the "standard" definitions */
#include <stdlib.h>
#include <string.h>
#define MEM_SIZE size_t
#else
typedef unsigned int MEM_SIZE;
#endif /* STANDARD_C */

#if defined(HAS_MEMCMP) && defined(mips) && defined(ultrix)
#undef HAS_MEMCMP
#endif

#ifdef HAS_MEMCPY
#  ifndef STANDARD_C
#    ifndef memcpy
	extern char * memcpy();
#    endif
#  endif
#else
#   ifndef memcpy
#	ifdef HAS_BCOPY
#	    define memcpy(d,s,l) bcopy(s,d,l)
#	else
#	    define memcpy(d,s,l) my_bcopy(s,d,l)
#	endif
#   endif
#endif /* HAS_MEMCPY */

#ifdef HAS_MEMSET
#  ifndef STANDARD_C
#    ifndef memset
	extern char *memset();
#    endif
#  endif
#  define memzero(d,l) memset(d,0,l)
#else
#   ifndef memzero
#	ifdef HAS_BZERO
#	    define memzero(d,l) bzero(d,l)
#	else
#	    define memzero(d,l) my_bzero(d,l)
#	endif
#   endif
#endif /* HAS_MEMSET */

#ifdef HAS_MEMCMP
#  ifndef STANDARD_C
#    ifndef memcmp
	extern int memcmp();
#    endif
#  endif
#else
#   ifndef memcmp
#	define memcmp(s1,s2,l) my_memcmp(s1,s2,l)
#   endif
#endif /* HAS_MEMCMP */

/* we prefer bcmp slightly for comparisons that don't care about ordering */
#ifndef HAS_BCMP
#   ifndef bcmp
#	define bcmp(s1,s2,l) memcmp(s1,s2,l)
#   endif
#endif /* HAS_BCMP */

#ifndef HAS_MEMMOVE
#if defined(HAS_BCOPY) && defined(SAFE_BCOPY)
#define memmove(d,s,l) bcopy(s,d,l)
#else
#if defined(HAS_MEMCPY) && defined(SAFE_MEMCPY)
#define memmove(d,s,l) memcpy(d,s,l)
#else
#define memmove(d,s,l) my_bcopy(s,d,l)
#endif
#endif
#endif

#ifndef _TYPES_		/* If types.h defines this it's easy. */
#ifndef major		/* Does everyone's types.h define this? */
#include <sys/types.h>
#endif
#endif

#ifdef I_NETINET_IN
#include <netinet/in.h>
#endif

#include <sys/stat.h>
#if defined(uts) || defined(UTekV)
#undef S_ISDIR
#undef S_ISCHR
#undef S_ISBLK
#undef S_ISREG
#undef S_ISFIFO
#undef S_ISLNK
#define S_ISDIR(P) (((P)&S_IFMT)==S_IFDIR)
#define S_ISCHR(P) (((P)&S_IFMT)==S_IFCHR)
#define S_ISBLK(P) (((P)&S_IFMT)==S_IFBLK)
#define S_ISREG(P) (((P)&S_IFMT)==S_IFREG)
#define S_ISFIFO(P) (((P)&S_IFMT)==S_IFIFO)
#ifdef S_IFLNK
#define S_ISLNK(P) (((P)&S_IFMT)==S_IFLNK)
#endif
#endif

#include <sys/mount.h>

#ifdef I_TIME
#   include <time.h>
#endif

#ifdef I_SYS_TIME
#   ifdef SYSTIMEKERNEL
#	define KERNEL
#   endif
#   include <sys/time.h>
#   ifdef SYSTIMEKERNEL
#	undef KERNEL
#   endif
#endif

#ifndef MSDOS
#include <sys/times.h>
#endif

#if defined(HAS_STRERROR) && (!defined(HAS_MKDIR) || !defined(HAS_RMDIR))
#undef HAS_STRERROR
#endif

#include <errno.h>
#ifndef MSDOS
#ifndef errno
extern int errno;     /* ANSI allows errno to be an lvalue expr */
#endif
#endif

#ifndef strerror
#ifdef HAS_STRERROR
char *strerror();
#else
extern int sys_nerr;
extern char *sys_errlist[];
#define strerror(e) ((e) < 0 || (e) >= sys_nerr ? "(unknown)" : sys_errlist[e])
#endif
#endif

#ifdef I_SYSIOCTL
#ifndef _IOCTL_
#include <sys/ioctl.h>
#endif
#endif

#if defined(mc300) || defined(mc500) || defined(mc700) || defined(mc6000)
#ifdef HAS_SOCKETPAIR
#undef HAS_SOCKETPAIR
#endif
#ifdef HAS_NDBM
#undef HAS_NDBM
#endif
#endif

#ifdef WANT_DBZ
#include <dbz.h>
#define SOME_DBM
#define dbm_fetch(db,dkey) fetch(dkey)
#define dbm_delete(db,dkey) fatal("dbz doesn't implement delete")
#define dbm_store(db,dkey,dcontent,flags) store(dkey,dcontent)
#define dbm_close(db) dbmclose()
#define dbm_firstkey(db) (fatal("dbz doesn't implement traversal"),fetch())
#define nextkey() (fatal("dbz doesn't implement traversal"),fetch())
#define dbm_nextkey(db) (fatal("dbz doesn't implement traversal"),fetch())
#ifdef HAS_NDBM
#undef HAS_NDBM
#endif
#ifndef HAS_ODBM
#define HAS_ODBM
#endif
#else
#ifdef HAS_GDBM
#ifdef I_GDBM
#include <gdbm.h>
#endif
#define SOME_DBM
#ifdef HAS_NDBM
#undef HAS_NDBM
#endif
#ifdef HAS_ODBM
#undef HAS_ODBM
#endif
#else
#ifdef HAS_NDBM
#include <ndbm.h>
#define SOME_DBM
#ifdef HAS_ODBM
#undef HAS_ODBM
#endif
#else
#ifdef HAS_ODBM
#ifdef NULL
#undef NULL		/* suppress redefinition message */
#endif
#include <dbm.h>
#ifdef NULL
#undef NULL
#endif
#define NULL 0		/* silly thing is, we don't even use this */
#define SOME_DBM
#define dbm_fetch(db,dkey) fetch(dkey)
#define dbm_delete(db,dkey) delete(dkey)
#define dbm_store(db,dkey,dcontent,flags) store(dkey,dcontent)
#define dbm_close(db) dbmclose()
#define dbm_firstkey(db) firstkey()
#endif /* HAS_ODBM */
#endif /* HAS_NDBM */
#endif /* HAS_GDBM */
#endif /* WANT_DBZ */
#ifdef SOME_DBM
EXT char *dbmkey;
EXT int dbmlen;
#endif

#if INTSIZE == 2
#define htoni htons
#define ntohi ntohs
#else
#define htoni htonl
#define ntohi ntohl
#endif

#if defined(I_DIRENT)
#   include <dirent.h>
#   define DIRENT dirent
#else
#   ifdef I_SYS_NDIR
#	include <sys/ndir.h>
#	define DIRENT direct
#   else
#	ifdef I_SYS_DIR
#	    ifdef hp9000s500
#		include <ndir.h>	/* may be wrong in the future */
#	    else
#		include <sys/dir.h>
#	    endif
#	    define DIRENT direct
#	endif
#   endif
#endif

#ifdef FPUTS_BOTCH
/* work around botch in SunOS 4.0.1 and 4.0.2 */
#   ifndef fputs
#	define fputs(str,fp) fprintf(fp,"%s",str)
#   endif
#endif

/*
 * The following gobbledygook brought to you on behalf of __STDC__.
 * (I could just use #ifndef __STDC__, but this is more bulletproof
 * in the face of half-implementations.)
 */

#ifndef S_IFMT
#   ifdef _S_IFMT
#	define S_IFMT _S_IFMT
#   else
#	define S_IFMT 0170000
#   endif
#endif

#ifndef S_ISDIR
#   define S_ISDIR(m) ((m & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISCHR
#   define S_ISCHR(m) ((m & S_IFMT) == S_IFCHR)
#endif

#ifndef S_ISBLK
#   ifdef S_IFBLK
#	define S_ISBLK(m) ((m & S_IFMT) == S_IFBLK)
#   else
#	define S_ISBLK(m) (0)
#   endif
#endif

#ifndef S_ISREG
#   define S_ISREG(m) ((m & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISFIFO
#   ifdef S_IFIFO
#	define S_ISFIFO(m) ((m & S_IFMT) == S_IFIFO)
#   else
#	define S_ISFIFO(m) (0)
#   endif
#endif

#ifndef S_ISLNK
#   ifdef _S_ISLNK
#	define S_ISLNK(m) _S_ISLNK(m)
#   else
#	ifdef _S_IFLNK
#	    define S_ISLNK(m) ((m & S_IFMT) == _S_IFLNK)
#	else
#	    ifdef S_IFLNK
#		define S_ISLNK(m) ((m & S_IFMT) == S_IFLNK)
#	    else
#		define S_ISLNK(m) (0)
#	    endif
#	endif
#   endif
#endif

#ifndef S_ISSOCK
#   ifdef _S_ISSOCK
#	define S_ISSOCK(m) _S_ISSOCK(m)
#   else
#	ifdef _S_IFSOCK
#	    define S_ISSOCK(m) ((m & S_IFMT) == _S_IFSOCK)
#	else
#	    ifdef S_IFSOCK
#		define S_ISSOCK(m) ((m & S_IFMT) == S_IFSOCK)
#	    else
#		define S_ISSOCK(m) (0)
#	    endif
#	endif
#   endif
#endif

#ifndef S_IRUSR
#   ifdef S_IREAD
#	define S_IRUSR S_IREAD
#	define S_IWUSR S_IWRITE
#	define S_IXUSR S_IEXEC
#   else
#	define S_IRUSR 0400
#	define S_IWUSR 0200
#	define S_IXUSR 0100
#   endif
#   define S_IRGRP (S_IRUSR>>3)
#   define S_IWGRP (S_IWUSR>>3)
#   define S_IXGRP (S_IXUSR>>3)
#   define S_IROTH (S_IRUSR>>6)
#   define S_IWOTH (S_IWUSR>>6)
#   define S_IXOTH (S_IXUSR>>6)
#endif

#ifndef S_ISUID
#   define S_ISUID 04000
#endif

#ifndef S_ISGID
#   define S_ISGID 02000
#endif

#ifdef f_next
#undef f_next
#endif

#if defined(cray) || defined(gould) || defined(i860)
#   define SLOPPYDIVIDE
#endif

#if defined(cray) || defined(convex) || defined (uts) || BYTEORDER > 0xffff
#   define QUAD
#endif

#ifdef QUAD
#   ifdef cray
#	define quad int
#   else
#	if defined(convex) || defined (uts)
#	    define quad long long
#	else
#	    define quad long
#	endif
#   endif
#endif

typedef MEM_SIZE STRLEN;

typedef struct arg ARG;
typedef struct cmd CMD;
typedef struct formcmd FCMD;
typedef struct scanpat SPAT;
typedef struct stio STIO;
typedef struct sub SUBR;
typedef struct string STR;
typedef struct atbl ARRAY;
typedef struct htbl HASH;
typedef struct regexp REGEXP;
typedef struct stabptrs STBP;
typedef struct stab STAB;
typedef struct callsave CSV;

#include "handy.h"
#include "regexp.h"
#include "str.h"
#include "util.h"
#include "form.h"
#include "stab.h"
#include "spat.h"
#include "arg.h"
#include "cmd.h"
#include "array.h"
#include "hash.h"

#if defined(iAPX286) || defined(M_I286) || defined(I80286)
#   define I286
#endif

#ifndef	STANDARD_C
#ifdef CHARSPRINTF
    char *sprintf();
#else
    int sprintf();
#endif
#endif

EXT char *Yes INIT("1");
EXT char *No INIT("");

/* "gimme" values */

/* Note: cmd.c assumes that it can use && to produce one of these values! */
#define G_SCALAR 0
#define G_ARRAY 1

#ifdef CRIPPLED_CC
int str_true();
#else /* !CRIPPLED_CC */
#define str_true(str) (Str = (str), \
	(Str->str_pok ? \
	    ((*Str->str_ptr > '0' || \
	      Str->str_cur > 1 || \
	      (Str->str_cur && *Str->str_ptr != '0')) ? 1 : 0) \
	: \
	    (Str->str_nok ? (Str->str_u.str_nval != 0.0) : 0 ) ))
#endif /* CRIPPLED_CC */

#ifdef DEBUGGING
#define str_peek(str) (Str = (str), \
	(Str->str_pok ? \
	    Str->str_ptr : \
	    (Str->str_nok ? \
		(sprintf(tokenbuf,"num(%g)",Str->str_u.str_nval), \
		    (char*)tokenbuf) : \
		"" )))
#endif

#ifdef CRIPPLED_CC
char *str_get();
#else
#ifdef TAINT
#define str_get(str) (Str = (str), tainted |= Str->str_tainted, \
	(Str->str_pok ? Str->str_ptr : str_2ptr(Str)))
#else
#define str_get(str) (Str = (str), (Str->str_pok ? Str->str_ptr : str_2ptr(Str)))
#endif /* TAINT */
#endif /* CRIPPLED_CC */

#ifdef CRIPPLED_CC
double str_gnum();
#else /* !CRIPPLED_CC */
#ifdef TAINT
#define str_gnum(str) (Str = (str), tainted |= Str->str_tainted, \
	(Str->str_nok ? Str->str_u.str_nval : str_2num(Str)))
#else /* !TAINT */
#define str_gnum(str) (Str = (str), (Str->str_nok ? Str->str_u.str_nval : str_2num(Str)))
#endif /* TAINT*/
#endif /* CRIPPLED_CC */
EXT STR *Str;

#define GROWSTR(pp,lp,len) if (*(lp) < (len)) growstr(pp,lp,len)

#ifndef DOSISH
#define STR_GROW(str,len) if ((str)->str_len < (len)) str_grow(str,len)
#define Str_Grow str_grow
#else
/* extra parentheses intentionally NOT placed around "len"! */
#define STR_GROW(str,len) if ((str)->str_len < (unsigned long)len) \
		str_grow(str,(unsigned long)len)
#define Str_Grow(str,len) str_grow(str,(unsigned long)(len))
#endif /* DOSISH */

#ifndef BYTEORDER
#define BYTEORDER 0x1234
#endif

#if defined(htonl) && !defined(HAS_HTONL)
#define HAS_HTONL
#endif
#if defined(htons) && !defined(HAS_HTONS)
#define HAS_HTONS
#endif
#if defined(ntohl) && !defined(HAS_NTOHL)
#define HAS_NTOHL
#endif
#if defined(ntohs) && !defined(HAS_NTOHS)
#define HAS_NTOHS
#endif
#ifndef HAS_HTONL
#if (BYTEORDER & 0xffff) != 0x4321
#define HAS_HTONS
#define HAS_HTONL
#define HAS_NTOHS
#define HAS_NTOHL
#define MYSWAP
#define htons my_swap
#define htonl my_htonl
#define ntohs my_swap
#define ntohl my_ntohl
#endif
#else
#if (BYTEORDER & 0xffff) == 0x4321
#undef HAS_HTONS
#undef HAS_HTONL
#undef HAS_NTOHS
#undef HAS_NTOHL
#endif
#endif

/*
 * Little-endian byte order functions - 'v' for 'VAX', or 'reVerse'.
 * -DWS
 */
#if BYTEORDER != 0x1234
# define HAS_VTOHL
# define HAS_VTOHS
# define HAS_HTOVL
# define HAS_HTOVS
# if BYTEORDER == 0x4321
#  define vtohl(x)	((((x)&0xFF)<<24)	\
			+(((x)>>24)&0xFF)	\
			+(((x)&0x0000FF00)<<8)	\
			+(((x)&0x00FF0000)>>8)	)
#  define vtohs(x)	((((x)&0xFF)<<8) + (((x)>>8)&0xFF))
#  define htovl(x)	vtohl(x)
#  define htovs(x)	vtohs(x)
# endif
	/* otherwise default to functions in util.c */
#endif

#ifdef CASTNEGFLOAT
#define U_S(what) ((unsigned short)(what))
#define U_I(what) ((unsigned int)(what))
#define U_L(what) ((unsigned long)(what))
#else
unsigned long castulong();
#define U_S(what) ((unsigned int)castulong(what))
#define U_I(what) ((unsigned int)castulong(what))
#define U_L(what) (castulong(what))
#endif

CMD *add_label();
CMD *block_head();
CMD *append_line();
CMD *make_acmd();
CMD *make_ccmd();
CMD *make_icmd();
CMD *invert();
CMD *addcond();
CMD *addloop();
CMD *wopt();
CMD *over();

STAB *stabent();
STAB *genstab();

ARG *stab2arg();
ARG *op_new();
ARG *make_op();
ARG *make_match();
ARG *make_split();
ARG *rcatmaybe();
ARG *listish();
ARG *maybelistish();
ARG *localize();
ARG *fixeval();
ARG *jmaybe();
ARG *l();
ARG *fixl();
ARG *mod_match();
ARG *make_list();
ARG *cmd_to_arg();
ARG *addflags();
ARG *hide_ary();
ARG *cval_to_arg();

STR *str_new();
STR *stab_str();

int apply();
int do_each();
int do_subr();
int do_match();
int do_unpack();
int eval();		/* this evaluates expressions */
int do_eval();		/* this evaluates eval operator */
int do_assign();

SUBR *make_sub();

FCMD *load_format();

char *scanpat();
char *scansubst();
char *scantrans();
char *scanstr();
char *scanident();
char *str_append_till();
char *str_gets();
char *str_grow();

bool do_open();
bool do_close();
bool do_print();
bool do_aprint();
bool do_exec();
bool do_aexec();

int do_subst();
int cando();
int ingroup();
int whichsig();
int userinit();
#ifdef CRYPTSCRIPT
void cryptswitch();
#endif

void str_replace();
void str_inc();
void str_dec();
void str_free();
void cmd_free();
void arg_free();
void spat_free();
void regfree();
void stab_clear();
void do_chop();
void do_vop();
void do_write();
void do_join();
void do_sprintf();
void do_accept();
void do_pipe();
void do_vecset();
void do_unshift();
void do_execfree();
void magicalize();
void magicname();
void savelist();
void saveitem();
void saveint();
void savelong();
void savesptr();
void savehptr();
void restorelist();
void repeatcpy();
void make_form();
void dehoist();
void format();
void my_unexec();
void fatal();
void warn();
#ifdef DEBUGGING
void dump_all();
void dump_cmd();
void dump_arg();
void dump_flags();
void dump_stab();
void dump_spat();
#endif
#ifdef MSTATS
void mstats();
#endif

HASH *savehash();
ARRAY *saveary();

EXT char **origargv;
EXT int origargc;
EXT char **origenviron;
extern char **environ;

EXT long subline INIT(0);
EXT STR *subname INIT(Nullstr);
EXT int arybase INIT(0);

struct outrec {
    long	o_lines;
    char	*o_str;
    int		o_len;
};

EXT struct outrec outrec;
EXT struct outrec toprec;

EXT STAB *stdinstab INIT(Nullstab);
EXT STAB *last_in_stab INIT(Nullstab);
EXT STAB *defstab INIT(Nullstab);
EXT STAB *argvstab INIT(Nullstab);
EXT STAB *envstab INIT(Nullstab);
EXT STAB *sigstab INIT(Nullstab);
EXT STAB *defoutstab INIT(Nullstab);
EXT STAB *curoutstab INIT(Nullstab);
EXT STAB *argvoutstab INIT(Nullstab);
EXT STAB *incstab INIT(Nullstab);
EXT STAB *leftstab INIT(Nullstab);
EXT STAB *amperstab INIT(Nullstab);
EXT STAB *rightstab INIT(Nullstab);
EXT STAB *DBstab INIT(Nullstab);
EXT STAB *DBline INIT(Nullstab);
EXT STAB *DBsub INIT(Nullstab);

EXT HASH *defstash;		/* main symbol table */
EXT HASH *curstash;		/* symbol table for current package */
EXT HASH *debstash;		/* symbol table for perldb package */

EXT STR *curstname;		/* name of current package */

EXT STR *freestrroot INIT(Nullstr);
EXT STR *lastretstr INIT(Nullstr);
EXT STR *DBsingle INIT(Nullstr);
EXT STR *DBtrace INIT(Nullstr);
EXT STR *DBsignal INIT(Nullstr);
EXT STR *formfeed INIT(Nullstr);

EXT int lastspbase;
EXT int lastsize;

EXT char *hexdigit INIT("0123456789abcdef0123456789ABCDEFx");
EXT char *origfilename;
EXT FILE * VOLATILE rsfp INIT(Nullfp);
EXT char buf[1024];
EXT char *bufptr;
EXT char *oldbufptr;
EXT char *oldoldbufptr;
EXT char *bufend;

EXT STR *linestr INIT(Nullstr);

EXT char *rs INIT("\n");
EXT int rschar INIT('\n');	/* final char of rs, or 0777 if none */
EXT int rslen INIT(1);
EXT bool rspara INIT(FALSE);
EXT char *ofs INIT(Nullch);
EXT int ofslen INIT(0);
EXT char *ors INIT(Nullch);
EXT int orslen INIT(0);
EXT char *ofmt INIT(Nullch);
EXT char *inplace INIT(Nullch);
EXT char *nointrp INIT("");

EXT bool preprocess INIT(FALSE);
EXT bool minus_n INIT(FALSE);
EXT bool minus_p INIT(FALSE);
EXT bool minus_l INIT(FALSE);
EXT bool minus_a INIT(FALSE);
EXT bool doswitches INIT(FALSE);
EXT bool dowarn INIT(FALSE);
EXT bool doextract INIT(FALSE);
EXT bool allstabs INIT(FALSE);	/* init all customary symbols in symbol table?*/
EXT bool sawampersand INIT(FALSE);	/* must save all match strings */
EXT bool sawstudy INIT(FALSE);		/* do fbminstr on all strings */
EXT bool sawi INIT(FALSE);		/* study must assume case insensitive */
EXT bool sawvec INIT(FALSE);
EXT bool localizing INIT(FALSE);	/* are we processing a local() list? */

#ifndef MAXSYSFD
#   define MAXSYSFD 2
#endif
EXT int maxsysfd INIT(MAXSYSFD);	/* top fd to pass to subprocesses */

#ifdef CSH
EXT char *cshname INIT(CSH);
EXT int cshlen INIT(0);
#endif /* CSH */

#ifdef TAINT
EXT bool tainted INIT(FALSE);		/* using variables controlled by $< */
EXT bool taintanyway INIT(FALSE);	/* force taint checks when !set?id */
#endif

EXT bool nomemok INIT(FALSE);		/* let malloc context handle nomem */

#ifndef DOSISH
#define TMPPATH "/tmp/perl-eXXXXXX"
#else
#define TMPPATH "plXXXXXX"
#endif /* MSDOS */
EXT char *e_tmpname;
EXT FILE *e_fp INIT(Nullfp);

EXT char tokenbuf[256];
EXT int expectterm INIT(TRUE);		/* how to interpret ambiguous tokens */
EXT VOLATILE int in_eval INIT(FALSE);	/* trap fatal errors? */
EXT int multiline INIT(0);		/* $*--do strings hold >1 line? */
EXT int forkprocess;			/* so do_open |- can return proc# */
EXT int do_undump INIT(0);		/* -u or dump seen? */
EXT int error_count INIT(0);		/* how many errors so far, max 10 */
EXT int multi_start INIT(0);		/* 1st line of multi-line string */
EXT int multi_end INIT(0);		/* last line of multi-line string */
EXT int multi_open INIT(0);		/* delimiter of said string */
EXT int multi_close INIT(0);		/* delimiter of said string */

FILE *popen();
/* char *str_get(); */
STR *interp();
void free_arg();
STIO *stio_new();
void hoistmust();
void scanconst();

EXT struct stat statbuf;
EXT struct stat statcache;
EXT STAB *statstab INIT(Nullstab);
EXT STR *statname INIT(Nullstr);
#ifndef MSDOS
EXT struct tms timesbuf;
#endif
EXT int uid;
EXT int euid;
EXT int gid;
EXT int egid;
UIDTYPE getuid();
UIDTYPE geteuid();
GIDTYPE getgid();
GIDTYPE getegid();
EXT int unsafe;

#ifdef DEBUGGING
EXT VOLATILE int debug INIT(0);
EXT int dlevel INIT(0);
EXT int dlmax INIT(128);
EXT char *debname;
EXT char *debdelim;
#define YYDEBUG 1
#endif
EXT int perldb INIT(0);
#define YYMAXDEPTH 300

EXT line_t cmdline INIT(NOLINE);

EXT STR str_undef;
EXT STR str_no;
EXT STR str_yes;

/* runtime control stuff */

EXT struct loop {
    char *loop_label;		/* what the loop was called, if anything */
    int loop_sp;		/* stack pointer to copy stuff down to */
    jmp_buf loop_env;
} *loop_stack;

EXT int loop_ptr INIT(-1);
EXT int loop_max INIT(128);

EXT jmp_buf top_env;

EXT char * VOLATILE goto_targ INIT(Nullch); /* cmd_exec gets strange when set */

struct ufuncs {
    int (*uf_val)();
    int (*uf_set)();
    int uf_index;
};

EXT ARRAY *stack;		/* THE STACK */

EXT ARRAY * VOLATILE savestack;		/* to save non-local values on */

EXT ARRAY *tosave;		/* strings to save on recursive subroutine */

EXT ARRAY *lineary;		/* lines of script for debugger */
EXT ARRAY *dbargs;		/* args to call listed by caller function */

EXT ARRAY *fdpid;		/* keep fd-to-pid mappings for mypopen */
EXT HASH *pidstatus;		/* keep pid-to-status mappings for waitpid */

EXT int *di;			/* for tmp use in debuggers */
EXT char *dc;
EXT short *ds;

/* Fix these up for __STDC__ */
EXT time_t basetime INIT(0);
char *mktemp();
#ifndef STANDARD_C
/* All of these are in stdlib.h or time.h for ANSI C */
double atof();
long time();
struct tm *gmtime(), *localtime();
char *index(), *rindex();
char *strcpy(), *strcat();
#endif /* ! STANDARD_C */

#ifdef EUNICE
#define UNLINK unlnk
int unlnk();
#else
#define UNLINK unlink
#endif

#ifndef HAS_SETREUID
#ifdef HAS_SETRESUID
#define setreuid(r,e) setresuid(r,e,-1)
#define HAS_SETREUID
#endif
#endif
#ifndef HAS_SETREGID
#ifdef HAS_SETRESGID
#define setregid(r,e) setresgid(r,e,-1)
#define HAS_SETREGID
#endif
#endif

#define SCAN_DEF 0
#define SCAN_TR 1
#define SCAN_REPL 2
