/* $FreeBSD: src/gnu/usr.bin/patch/common.h,v 1.7 1999/09/05 17:31:55 peter Exp $
 *
 * $Log: common.h,v $
 * Revision 2.0.1.2  88/06/22  20:44:53  lwall
 * patch12: sprintf was declared wrong
 *
 * Revision 2.0.1.1  88/06/03  15:01:56  lwall
 * patch10: support for shorter extensions.
 *
 * Revision 2.0  86/09/17  15:36:39  lwall
 * Baseline for netwide release.
 *
 */

#define DEBUGGING

#define VOIDUSED 7
#include "config.h"

/* shut lint up about the following when return value ignored */

#define Signal (void)signal
#define Unlink (void)unlink
#define Lseek (void)lseek
#define Fseek (void)fseek
#define Fstat (void)fstat
#define Pclose (void)pclose
#define Close (void)close
#define Fclose (void)fclose
#define Fflush (void)fflush
#define Sprintf (void)sprintf
#define Mktemp (void)mktemp
#define Strcpy (void)strcpy
#define Strcat (void)strcat

/* NeXT declares malloc and realloc incompatibly from us in some of
   these files.  Temporarily redefine them to prevent errors.  */
#define malloc system_malloc
#define realloc system_realloc
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <signal.h>
#undef malloc
#undef realloc

/* constants */

/* AIX predefines these.  */
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#define TRUE (1)
#define FALSE (0)

#define MAXHUNKSIZE 200000		/* is this enough lines? */
#define INITHUNKMAX 125			/* initial dynamic allocation size */
#define MAXLINELEN 4096
#define BUFFERSIZE 4096

#define SCCSPREFIX "s."
#define GET "get %s"
#define GET_LOCKED "get -e %s"
#define SCCSDIFF "get -p %s | diff - %s >/dev/null"

#define RCSSUFFIX ",v"
#define CHECKOUT "co %s"
#define CHECKOUT_LOCKED "co -l %s"
#define RCSDIFF "rcsdiff %s > /dev/null"

/* handy definitions */

#define Null(t) ((t)0)
#define Nullch Null(char *)
#define Nullfp Null(FILE *)
#define Nulline Null(LINENUM)

#define Ctl(ch) ((ch) & 037)

#define strNE(s1,s2) (strcmp(s1, s2))
#define strEQ(s1,s2) (!strcmp(s1, s2))
#define strnNE(s1,s2,l) (strncmp(s1, s2, l))
#define strnEQ(s1,s2,l) (!strncmp(s1, s2, l))

/* typedefs */

typedef char bool;
typedef long LINENUM;			/* must be signed */
typedef unsigned MEM;			/* what to feed malloc */

/* globals */

EXT int Argc;				/* guess */
EXT char **Argv;
EXT int optind_last;			/* for restarting plan_b */

EXT struct stat filestat;		/* file statistics area */
EXT int filemode INIT(0644);

EXT char buf[MAXLINELEN];		/* general purpose buffer */
EXT FILE *ofp INIT(Nullfp);		/* output file pointer */
EXT FILE *rejfp INIT(Nullfp);		/* reject file pointer */

EXT int myuid;				/* cache getuid return value */

EXT bool using_plan_a INIT(TRUE);	/* try to keep everything in memory */
EXT bool out_of_mem INIT(FALSE);	/* ran out of memory in plan a */

#define MAXFILEC 2
EXT int filec INIT(0);			/* how many file arguments? */
EXT char *filearg[MAXFILEC];
EXT bool ok_to_create_file INIT(FALSE);
EXT char *bestguess INIT(Nullch);	/* guess at correct filename */

EXT char *outname INIT(Nullch);
EXT char rejname[128];

EXT char *origprae INIT(Nullch);

EXT char *TMPOUTNAME;
EXT char *TMPINNAME;
EXT char *TMPREJNAME;
EXT char *TMPPATNAME;
EXT bool toutkeep INIT(FALSE);
EXT bool trejkeep INIT(FALSE);

EXT LINENUM last_offset INIT(0);
#ifdef DEBUGGING
EXT int debug INIT(0);
#endif
EXT LINENUM maxfuzz INIT(2);
EXT bool force INIT(FALSE);
EXT bool batch INIT(FALSE);
EXT bool verbose INIT(TRUE);
EXT bool reverse INIT(FALSE);
EXT bool noreverse INIT(FALSE);
EXT bool skip_rest_of_patch INIT(FALSE);
EXT int strippath INIT(957);
EXT bool canonicalize INIT(FALSE);

#define CONTEXT_DIFF 1
#define NORMAL_DIFF 2
#define ED_DIFF 3
#define NEW_CONTEXT_DIFF 4
#define UNI_DIFF 5
EXT int diff_type INIT(0);

EXT bool do_defines INIT(FALSE);	/* patch using ifdef, ifndef, etc. */
EXT char if_defined[128];		/* #ifdef xyzzy */
EXT char not_defined[128];		/* #ifndef xyzzy */
EXT char else_defined[] INIT("#else\n");/* #else */
EXT char end_defined[128];		/* #endif xyzzy */

EXT char *revision INIT(Nullch);	/* prerequisite revision, if any */

#include <errno.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#else
extern int errno;
FILE *popen();
char *malloc();
char *realloc();
long atol();
char *getenv();
char *strcpy();
char *strcat();
#endif
char *mktemp();
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#else
long lseek();
#endif
#if defined(_POSIX_VERSION) || defined(HAVE_FCNTL_H)
#include <fcntl.h>
#endif

#if !defined(S_ISDIR) && defined(S_IFDIR)
#define	S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG) && defined(S_IFREG)
#define	S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
