
/*
 *                     RCS common definitions and data structures
 */
#define RCSBASE "$Id: rcsbase.h,v 5.11 1991/10/07 17:32:46 eggert Exp $"

/* Copyright (C) 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991 by Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/



/*****************************************************************************
 * INSTRUCTIONS:
 * =============
 * See the Makefile for how to define C preprocessor symbols.
 * If you need to change the comment leaders, update the table comtable[]
 * in rcsfnms.c. (This can wait until you know what a comment leader is.)
 *****************************************************************************
 */


/* $Log: rcsbase.h,v $
 * Revision 5.11  1991/10/07  17:32:46  eggert
 * Support piece tables even if !has_mmap.
 *
 * Revision 5.10  1991/09/24  00:28:39  eggert
 * Remove unexported functions.
 *
 * Revision 5.9  1991/08/19  03:13:55  eggert
 * Add piece tables and other tuneups, and NFS workarounds.
 *
 * Revision 5.8  1991/04/21  11:58:20  eggert
 * Add -x, RCSINIT, MS-DOS support.
 *
 * Revision 5.7  1991/02/28  19:18:50  eggert
 * Try setuid() if seteuid() doesn't work.
 *
 * Revision 5.6  1991/02/26  17:48:37  eggert
 * Support new link behavior.  Move ANSI C / Posix declarations into conf.sh.
 *
 * Revision 5.5  1990/12/04  05:18:43  eggert
 * Use -I for prompts and -q for diagnostics.
 *
 * Revision 5.4  1990/11/01  05:03:35  eggert
 * Don't assume that builtins are functions; they may be macros.
 * Permit arbitrary data in logs.
 *
 * Revision 5.3  1990/09/26  23:36:58  eggert
 * Port wait() to non-Posix ANSI C hosts.
 *
 * Revision 5.2  1990/09/04  08:02:20  eggert
 * Don't redefine NAME_MAX, PATH_MAX.
 * Improve incomplete line handling.  Standardize yes-or-no procedure.
 *
 * Revision 5.1  1990/08/29  07:13:53  eggert
 * Add -kkvl.  Fix type typos exposed by porting.  Clean old log messages too.
 *
 * Revision 5.0  1990/08/22  08:12:44  eggert
 * Adjust ANSI C / Posix support.  Add -k, -V, setuid.  Don't call access().
 * Remove compile-time limits; use malloc instead.
 * Ansify and Posixate.  Add support for ISO 8859.
 * Remove snoop and v2 support.
 *
 * Revision 4.9  89/05/01  15:17:14  narten
 * botched previous USG fix 
 * 
 * Revision 4.8  89/05/01  14:53:05  narten
 * changed #include <strings.h> -> string.h for USG systems.
 * 
 * Revision 4.7  88/11/08  15:58:45  narten
 * removed defs for functions loaded from libraries
 * 
 * Revision 4.6  88/08/09  19:12:36  eggert
 * Shrink stdio code size; remove lint; permit -Dhshsize=nn.
 * 
 * Revision 4.5  87/12/18  17:06:41  narten
 * made removed BSD ifdef, now uses V4_2BSD
 * 
 * Revision 4.4  87/10/18  10:29:49  narten
 * Updating version numbers
 * Changes relative to 1.1 are actually relative to 4.2
 * 
 * Revision 1.3  87/09/24  14:02:25  narten
 * changes for lint
 * 
 * Revision 1.2  87/03/27  14:22:02  jenkins
 * Port to suns
 * 
 * Revision 4.2  83/12/20  16:04:20  wft
 * merged 3.6.1.1 and 4.1 (SMALLOG, logsize).
 * moved setting of STRICT_LOCKING to Makefile.
 * changed DOLLAR to UNKN (conflict with KDELIM).
 * 
 * Revision 4.1  83/05/04  09:12:41  wft
 * Added markers Id and RCSfile.
 * Added Dbranch for default branches.
 * 
 * Revision 3.6.1.1  83/12/02  21:56:22  wft
 * Increased logsize, added macro SMALLOG.
 * 
 * Revision 3.6  83/01/15  16:43:28  wft
 * 4.2 prerelease
 * 
 * Revision 3.6  83/01/15  16:43:28  wft
 * Replaced dbm.h with BYTESIZ, fixed definition of rindex().
 * Added variants of NCPFN and NCPPN for bsd 4.2, selected by defining V4_2BSD.
 * Added macro DELNUMFORM to have uniform format for printing delta text nodes.
 * Added macro DELETE to mark deleted deltas.
 *
 * Revision 3.5  82/12/10  12:16:56  wft
 * Added two forms of DATEFORM, one using %02d, the other %.2d.
 *
 * Revision 3.4  82/12/04  20:01:25  wft
 * added LOCKER, Locker, and USG (redefinition of rindex).
 *
 * Revision 3.3  82/12/03  12:22:04  wft
 * Added dbm.h, stdio.h, RCSBASE, RCSSEP, RCSSUF, WORKMODE, TMPFILE3,
 * PRINTDATE, PRINTTIME, map, and ctab; removed Suffix. Redefined keyvallength
 * using NCPPN. Changed putc() to abort on write error.
 *
 * Revision 3.2  82/10/18  15:03:52  wft
 * added macro STRICT_LOCKING, removed RCSUMASK.
 * renamed JOINFILE[1,2] to JOINFIL[1,2].
 *
 * Revision 3.1  82/10/11  19:41:17  wft
 * removed NBPW, NBPC, NCPW.
 * added typdef int void to aid compiling
 */


#include "conf.h"


#define EXIT_TROUBLE DIFF_TROUBLE

#ifdef PATH_MAX
#	define SIZEABLE_PATH PATH_MAX /* size of a large path; not a hard limit */
#else
#	define SIZEABLE_PATH _POSIX_PATH_MAX
#endif

/* for traditional C hosts with unusual size arguments */
#define Fread(p,s,n,f)  fread(p, (freadarg_type)(s), (freadarg_type)(n), f)
#define Fwrite(p,s,n,f)  fwrite(p, (freadarg_type)(s), (freadarg_type)(n), f)


/*
 * Parameters
 */

/* backwards compatibility with old versions of RCS */
#define VERSION_min 3		/* old output RCS format supported */
#define VERSION_max 5		/* newest output RCS format supported */
#ifndef VERSION_DEFAULT		/* default RCS output format */
#	define VERSION_DEFAULT VERSION_max
#endif
#define VERSION(n) ((n) - VERSION_DEFAULT) /* internally, 0 is the default */

#ifndef STRICT_LOCKING
#define STRICT_LOCKING 1
#endif
			      /* 0 sets the default locking to non-strict;  */
                              /* used in experimental environments.         */
                              /* 1 sets the default locking to strict;      */
                              /* used in production environments.           */

#define yearlength	   16 /* (good through AD 9,999,999,999,999,999)    */
#define datesize (yearlength+16) /* size of output of DATEFORM		    */
#define joinlength         20 /* number of joined revisions permitted       */
#define RCSTMPPREFIX '_' /* prefix for temp files in working dir  */
#define KDELIM            '$' /* delimiter for keywords                     */
#define VDELIM            ':' /* separates keywords from values             */
#define DEFAULTSTATE    "Exp" /* default state of revisions                 */



#define true     1
#define false    0
#define nil      0


/*
 * RILE - readonly file
 * declarecache; - declares local cache for RILE variable(s)
 * setupcache - sets up the local RILE cache, but does not initialize it
 * cache, uncache - caches and uncaches the local RILE;
 *	(uncache,cache) is needed around functions that advance the RILE pointer
 * Igeteof(f,c,s) - get a char c from f, executing statement s at EOF
 * cachegeteof(c,s) - Igeteof applied to the local RILE
 * Iget(f,c) - like Igeteof, except EOF is an error
 * cacheget(c) - Iget applied to the local RILE
 * Ifileno, Irewind, Iseek, Itell - analogs to stdio routines
 */

#if large_memory
	typedef unsigned char const *Iptr_type;
	typedef struct {
		Iptr_type ptr, lim;
		unsigned char *base; /* for lint, not Iptr_type even if has_mmap */
#		if has_mmap
#			define Ifileno(f) ((f)->fd)
			int fd;
#		else
#			define Ifileno(f) fileno((f)->stream)
			FILE *stream;
			unsigned char *readlim;
#		endif
	} RILE;
#	if has_mmap
#		define declarecache register Iptr_type ptr, lim
#		define setupcache(f) (lim = (f)->lim)
#		define Igeteof(f,c,s) if ((f)->ptr==(f)->lim) s else (c)= *(f)->ptr++
#		define cachegeteof(c,s) if (ptr==lim) s else (c)= *ptr++
#	else
#		define declarecache register Iptr_type ptr; register RILE *rRILE
#		define setupcache(f) (rRILE = (f))
#		define Igeteof(f,c,s) if ((f)->ptr==(f)->readlim && !Igetmore(f)) s else (c)= *(f)->ptr++
#		define cachegeteof(c,s) if (ptr==rRILE->readlim && !Igetmore(rRILE)) s else (c)= *ptr++
#	endif
#	define uncache(f) ((f)->ptr = ptr)
#	define cache(f) (ptr = (f)->ptr)
#	define Iget(f,c) Igeteof(f,c,Ieof();)
#	define cacheget(c) cachegeteof(c,Ieof();)
#	define Itell(f) ((f)->ptr)
#	define Iseek(f,p) ((f)->ptr = (p))
#	define Irewind(f) Iseek(f, (f)->base)
#	define cachetell() ptr
#else
#	define RILE FILE
#	define declarecache register FILE *ptr
#	define setupcache(f) (ptr = (f))
#	define uncache(f)
#	define cache(f)
#	define Igeteof(f,c,s) if(((c)=getc(f))<0){testIerror(f);if(feof(f))s}else
#	define cachegeteof(c,s) Igeteof(ptr,c,s)
#	define Iget(f,c) if (((c)=getc(f))<0) testIeof(f); else
#	define cacheget(c) Iget(ptr,c)
#	define Ifileno(f) fileno(f)
#endif

/* Print a char, but abort on write error.  */
#define aputc(c,o) if (putc(c,o)<0) testOerror(o); else

/* Get a character from an RCS file, perhaps copying to a new RCS file.  */
#define GETCeof(o,c,s) { cachegeteof(c,s); if (o) aputc(c,o); }
#define GETC(o,c) { cacheget(c); if (o) aputc(c,o); }


#define WORKMODE(RCSmode, writable) ((RCSmode)&~(S_IWUSR|S_IWGRP|S_IWOTH) | ((writable)?S_IWUSR:0))
/* computes mode of working file: same as RCSmode, but write permission     */
/* determined by writable */


/* character classes and token codes */
enum tokens {
/* classes */	DELIM,	DIGIT,	IDCHAR,	NEWLN,	LETTER,	Letter,
		PERIOD,	SBEGIN,	SPACE,	UNKN,
/* tokens */	COLON,	ID,	NUM,	SEMI,	STRING
};

#define SDELIM  '@'     /* the actual character is needed for string handling*/
/* SDELIM must be consistent with ctab[], so that ctab[SDELIM]==SBEGIN.
 * there should be no overlap among SDELIM, KDELIM, and VDELIM
 */

#define isdigit(c) ((unsigned)((c)-'0') <= 9) /* faster than ctab[c]==DIGIT */





/***************************************
 * Data structures for the symbol table
 ***************************************/

/* Buffer of arbitrary data */
struct buf {
	char *string;
	size_t size;
};
struct cbuf {
	char const *string;
	size_t size;
};

/* Hash table entry */
struct hshentry {
	char const	  * num;      /* pointer to revision number (ASCIZ) */
	char const	  * date;     /* pointer to date of checkin	    */
	char const	  * author;   /* login of person checking in	    */
	char const	  * lockedby; /* who locks the revision		    */
	char const	  * state;    /* state of revision (Exp by default) */
	struct cbuf	    log;      /* log message requested at checkin   */
        struct branchhead * branches; /* list of first revisions on branches*/
	struct cbuf	    ig;	      /* ignored phrases of revision	    */
        struct hshentry   * next;     /* next revision on same branch       */
	struct hshentry   * nexthsh;  /* next revision with same hash value */
	unsigned long	    insertlns;/* lines inserted (computed by rlog)  */
	unsigned long	    deletelns;/* lines deleted  (computed by rlog)  */
	char		    selector; /* true if selected, false if deleted */
};

/* list of hash entries */
struct hshentries {
	struct hshentries *rest;
	struct hshentry *first;
};

/* list element for branch lists */
struct branchhead {
        struct hshentry   * hsh;
        struct branchhead * nextbranch;
};

/* accesslist element */
struct access {
	char const	  * login;
        struct access     * nextaccess;
};

/* list element for locks  */
struct lock {
	char const	  * login;
        struct hshentry   * delta;
        struct lock       * nextlock;
};

/* list element for symbolic names */
struct assoc {
	char const	  * symbol;
	char const	  * num;
        struct assoc      * nextassoc;
};


#define mainArgs (argc,argv) int argc; char **argv;

#if lint
#	define libId(name,rcsid)
#	define mainProg(name,cmd,rcsid) int name mainArgs
#else
#	define libId(name,rcsid) char const name[] = rcsid;
#	define mainProg(name,cmd,rcsid) char const copyright[] = "Copyright 1982,1988,1989 by Walter F. Tichy\nPurdue CS\nCopyright 1990,1991 by Paul Eggert", rcsbaseId[] = RCSBASE, cmdid[] = cmd; libId(name,rcsid) int main mainArgs
#endif

/*
 * Markers for keyword expansion (used in co and ident)
 *	Every byte must have class LETTER or Letter.
 */
#define AUTHOR          "Author"
#define DATE            "Date"
#define HEADER          "Header"
#define IDH             "Id"
#define LOCKER          "Locker"
#define LOG             "Log"
#define RCSFILE         "RCSfile"
#define REVISION        "Revision"
#define SOURCE          "Source"
#define STATE           "State"
#define keylength 8 /* max length of any of the above keywords */

enum markers { Nomatch, Author, Date, Header, Id,
	       Locker, Log, RCSfile, Revision, Source, State };
	/* This must be in the same order as rcskeys.c's Keyword[] array. */

#define DELNUMFORM      "\n\n%s\n%s\n"
/* used by putdtext and scanlogtext */

#define EMPTYLOG "*** empty log message ***" /* used by ci and rlog */

/* main program */
extern char const cmdid[];
exiting void exiterr P((void));

/* maketime */
int setfiledate P((char const*,char const[datesize]));
void str2date P((char const*,char[datesize]));
void time2date P((time_t,char[datesize]));

/* merge */
int merge P((int,char const*const[2],char const*const[3]));

/* partime */
int partime P((char const*,struct tm*,int*));

/* rcsedit */
#define ciklogsize 23 /* sizeof("checked in with -k by ") */
extern FILE *fcopy;
extern char const *resultfile;
extern char const ciklog[ciklogsize];
extern int locker_expansion;
extern struct buf dirtfname[];
#define newRCSfilename (dirtfname[0].string)
RILE *rcswriteopen P((struct buf*,struct stat*,int));
char const *makedirtemp P((char const*,int));
char const *getcaller P((void));
int addlock P((struct hshentry*));
int addsymbol P((char const*,char const*,int));
int checkaccesslist P((void));
int chnamemod P((FILE**,char const*,char const*,mode_t));
int donerewrite P((int));
int dorewrite P((int,int));
int expandline P((RILE*,FILE*,struct hshentry const*,int,FILE*));
int findlock P((int,struct hshentry**));
void aflush P((FILE*));
void copystring P((void));
void dirtempunlink P((void));
void enterstring P((void));
void finishedit P((struct hshentry const*,FILE*,int));
void keepdirtemp P((char const*));
void openfcopy P((FILE*));
void snapshotedit P((FILE*));
void xpandstring P((struct hshentry const*));
#if has_NFS || bad_unlink
	int un_link P((char const*));
#else
#	define un_link(s) unlink(s)
#endif
#if large_memory
	void edit_string P((void));
#	define editstring(delta) edit_string()
#else
	void editstring P((struct hshentry const*));
#endif

/* rcsfcmp */
int rcsfcmp P((RILE*,struct stat const*,char const*,struct hshentry const*));

/* rcsfnms */
#define bufautobegin(b) ((void) ((b)->string = 0, (b)->size = 0))
extern FILE *workstdout;
extern char *workfilename;
extern char const *RCSfilename;
extern char const *suffixes;
extern struct stat RCSstat;
RILE *rcsreadopen P((struct buf*,struct stat*,int));
char *bufenlarge P((struct buf*,char const**));
char const *basename P((char const*));
char const *getfullRCSname P((void));
char const *maketemp P((int));
char const *rcssuffix P((char const*));
int pairfilenames P((int,char**,RILE*(*)P((struct buf*,struct stat*,int)),int,int));
size_t dirlen P((char const*));
struct cbuf bufremember P((struct buf*,size_t));
void bufalloc P((struct buf*,size_t));
void bufautoend P((struct buf*));
void bufrealloc P((struct buf*,size_t));
void bufscat P((struct buf*,char const*));
void bufscpy P((struct buf*,char const*));
void tempunlink P((void));

/* rcsgen */
extern int interactiveflag;
extern struct buf curlogbuf;
char const *buildrevision P((struct hshentries const*,struct hshentry*,FILE*,int));
int getcstdin P((void));
int ttystdin P((void));
int yesorno P((int,char const*,...));
struct cbuf cleanlogmsg P((char*,size_t));
struct cbuf getsstdin P((char const*,char const*,char const*,struct buf*));
void putdesc P((int,char*));

/* rcskeep */
extern int prevkeys;
extern struct buf prevauthor, prevdate, prevrev, prevstate;
int getoldkeys P((RILE*));

/* rcskeys */
extern char const *const Keyword[];
enum markers trymatch P((char const*));

/* rcslex */
extern FILE *foutptr;
extern FILE *frewrite;
extern RILE *finptr;
extern char const *NextString;
extern enum tokens nexttok;
extern int hshenter;
extern int nerror;
extern int nextc;
extern int quietflag;
extern unsigned long rcsline;
char const *getid P((void));
exiting void efaterror P((char const*));
exiting void enfaterror P((int,char const*));
exiting void faterror P((char const*,...));
exiting void fatserror P((char const*,...));
exiting void Ieof P((void));
exiting void Ierror P((void));
exiting void Oerror P((void));
char *checkid P((char*,int));
int eoflex P((void));
int getkeyopt P((char const*));
int getlex P((enum tokens));
struct cbuf getphrases P((char const*));
struct cbuf savestring P((struct buf*));
struct hshentry *getnum P((void));
void Ifclose P((RILE*));
void Izclose P((RILE**));
void Lexinit P((void));
void Ofclose P((FILE*));
void Ozclose P((FILE**));
void afputc P((int,FILE*));
void aprintf P((FILE*,char const*,...));
void aputs P((char const*,FILE*));
void checksid P((char*));
void diagnose P((char const*,...));
void eerror P((char const*));
void eflush P((void));
void enerror P((int,char const*));
void error P((char const*,...));
void fvfprintf P((FILE*,char const*,va_list));
void getkey P((char const*));
void getkeystring P((char const*));
void nextlex P((void));
void oflush P((void));
void printstring P((void));
void readstring P((void));
void redefined P((int));
void testIerror P((FILE*));
void testOerror P((FILE*));
void warn P((char const*,...));
void warnignore P((void));
#if has_madvise && has_mmap && large_memory
	void advise_access P((RILE*,int));
#	define if_advise_access(p,f,advice) if (p) advise_access(f,advice)
#else
#	define advise_access(f,advice)
#	define if_advise_access(p,f,advice)
#endif
#if has_mmap && large_memory
	RILE *I_open P((char const*,struct stat*));
#	define Iopen(f,m,s) I_open(f,s)
#else
	RILE *Iopen P((char const*,char const*,struct stat*));
#endif
#if !large_memory
	void testIeof P((FILE*));
	void Irewind P((RILE*));
#endif

/* rcsmap */
extern const enum tokens ctab[];

/* rcsrev */
char *partialno P((struct buf*,char const*,unsigned));
char const *tiprev P((void));
int cmpnum P((char const*,char const*));
int cmpnumfld P((char const*,char const*,unsigned));
int compartial P((char const*,char const*,unsigned));
int expandsym P((char const*,struct buf*));
int fexpandsym P((char const*,struct buf*,RILE*));
struct hshentry *genrevs P((char const*,char const*,char const*,char const*,struct hshentries**));
unsigned countnumflds P((char const*));
void getbranchno P((char const*,struct buf*));

/* rcssyn */
/* These expand modes must agree with Expand_names[] in rcssyn.c.  */
#define KEYVAL_EXPAND 0 /* -kkv `$Keyword: value $' */
#define KEYVALLOCK_EXPAND 1 /* -kkvl `$Keyword: value locker $' */
#define KEY_EXPAND 2 /* -kk `$Keyword$' */
#define VAL_EXPAND 3 /* -kv `value' */
#define OLD_EXPAND 4 /* -ko use old string, omitting expansion */
struct diffcmd {
	unsigned long
		line1, /* number of first line */
		nlines, /* number of lines affected */
		adprev, /* previous 'a' line1+1 or 'd' line1 */
		dafter; /* sum of previous 'd' line1 and previous 'd' nlines */
};
extern char const      * Dbranch;
extern struct access   * AccessList;
extern struct assoc    * Symbols;
extern struct cbuf Comment;
extern struct lock     * Locks;
extern struct hshentry * Head;
extern int		 Expand;
extern int               StrictLocks;
extern unsigned TotalDeltas;
extern char const *const expand_names[];
extern char const Kdesc[];
extern char const Klog[];
extern char const Ktext[];
int getdiffcmd P((RILE*,int,FILE*,struct diffcmd*));
int putdftext P((char const*,struct cbuf,RILE*,FILE*,int));
int putdtext P((char const*,struct cbuf,char const*,FILE*,int));
int str2expmode P((char const*));
void getadmin P((void));
void getdesc P((int));
void gettree P((void));
void ignorephrase P((void));
void initdiffcmd P((struct diffcmd*));
void putadmin P((FILE*));
void putstring P((FILE*,int,struct cbuf,int));
void puttree P((struct hshentry const*,FILE*));

/* rcsutil */
extern int RCSversion;
char *cgetenv P((char const*));
char *fstr_save P((char const*));
char *str_save P((char const*));
char const *date2str P((char const[datesize],char[datesize]));
char const *getusername P((int));
int getRCSINIT P((int,char**,char***));
int run P((char const*,char const*,...));
int runv P((char const**));
malloc_type fremember P((malloc_type));
malloc_type ftestalloc P((size_t));
malloc_type testalloc P((size_t));
malloc_type testrealloc P((malloc_type,size_t));
#define ftalloc(T) ftnalloc(T,1)
#define talloc(T) tnalloc(T,1)
#if lint
	extern malloc_type lintalloc;
#	define ftnalloc(T,n) (lintalloc = ftestalloc(sizeof(T)*(n)), (T*)0)
#	define tnalloc(T,n) (lintalloc = testalloc(sizeof(T)*(n)), (T*)0)
#	define trealloc(T,p,n) (lintalloc = testrealloc((malloc_type)0, sizeof(T)*(n)), p)
#	define tfree(p)
#else
#	define ftnalloc(T,n) ((T*) ftestalloc(sizeof(T)*(n)))
#	define tnalloc(T,n) ((T*) testalloc(sizeof(T)*(n)))
#	define trealloc(T,p,n) ((T*) testrealloc((malloc_type)(p), sizeof(T)*(n)))
#	define tfree(p) free((malloc_type)(p))
#endif
void awrite P((char const*,size_t,FILE*));
void fastcopy P((RILE*,FILE*));
void ffree P((void));
void ffree1 P((char const*));
void setRCSversion P((char const*));
#if has_signal
	void catchints P((void));
	void ignoreints P((void));
	void restoreints P((void));
#else
#	define catchints()
#	define ignoreints()
#	define restoreints()
#endif
#if has_getuid
	uid_t ruid P((void));
#	define myself(u) ((u) == ruid())
#else
#	define myself(u) true
#endif
#if has_setuid
	uid_t euid P((void));
	void nosetid P((void));
	void seteid P((void));
	void setrid P((void));
#else
#	define nosetid()
#	define seteid()
#	define setrid()
#endif
