/*
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * sup misc. routines, include list processing.
 **********************************************************************
 * HISTORY
 * $Log: supcmisc.c,v $
 * Revision 1.2  1994/06/20  06:04:11  rgrimes
 * Humm.. they did a lot of #if __STDC__ but failed to properly prototype
 * the code.  Also fixed one bad argument to a wait3 call.
 *
 * It won't compile -Wall, but atleast it compiles standard without warnings
 * now.
 *
 * Revision 1.1.1.1  1993/08/21  00:46:34  jkh
 * Current sup with compression support.
 *
 * Revision 1.1.1.1  1993/05/21  14:52:18  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 1.5  92/08/11  12:07:22  mrt
 * 	Added release to FILEWHEN name.
 * 	Brad's changes: delinted and updated variable arguement usage.
 * 	[92/07/26            mrt]
 * 
 * Revision 1.3  89/08/15  15:31:28  bww
 * 	Updated to use v*printf() in place of _doprnt().
 * 	From "[89/04/19            mja]" at CMU.
 * 	[89/08/15            bww]
 * 
 * 27-Dec-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Fixed bug in ugconvert() which left pw uninitialized.
 *
 * 25-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Split off from sup.c and changed goaway to use printf
 *	instead of notify if thisC is NULL.
 *
 **********************************************************************
 */

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "supcdefs.h"

struct liststruct {		/* uid and gid lists */
	char *Lname;		/* name */
	int Lnumber;		/* uid or gid */
	struct liststruct *Lnext;
};
typedef struct liststruct LIST;

#define HASHBITS	4
#define HASHSIZE	(1<<HASHBITS)
#define HASHMASK	(HASHSIZE-1)
#define LISTSIZE	(HASHSIZE*HASHSIZE)

static LIST *uidL[LISTSIZE];		/* uid and gid lists */
static LIST *gidL[LISTSIZE];

extern COLLECTION *thisC;		/* collection list pointer */

/*************************************************
 ***    P R I N T   U P D A T E   T I M E S    ***
 *************************************************/

prtime ()
{
	char buf[STRINGLENGTH];
	char relsufix[STRINGLENGTH];
	long twhen;
	int f;

	if ((thisC->Cflags&CFURELSUF) && thisC->Crelease)
		(void) sprintf (relsufix,".%s",thisC->Crelease);
	else
		relsufix[0] = '\0';
	if (chdir (thisC->Cbase) < 0)
		logerr ("Can't change to base directory %s for collection %s",
			thisC->Cbase,thisC->Cname);
	(void) sprintf (buf,FILEWHEN,thisC->Cname,relsufix);
	f = open (buf,O_RDONLY,0);
	if (f >= 0) {
		if (read(f,(char *)&twhen,sizeof(long)) != sizeof(long))
			twhen = 0;
		(void) close (f);
	} else
		twhen = 0;
	(void) strcpy (buf,ctime (&twhen));
	buf[strlen(buf)-1] = '\0';
	loginfo ("Last update occurred at %s for collection %s",
		buf,thisC->Cname);
}

int establishdir (fname)
char *fname;
{
	char dpart[STRINGLENGTH],fpart[STRINGLENGTH];
	path (fname,dpart,fpart);
	return (estabd (fname,dpart));
}

int estabd (fname,dname)
char *fname,*dname;
{
	char dpart[STRINGLENGTH],fpart[STRINGLENGTH];
	struct stat sbuf;
	register int x;

	if (stat (dname,&sbuf) >= 0)  return (FALSE); /* exists */
	path (dname,dpart,fpart);
	if (strcmp (fpart,".") == 0) {		/* dname is / or . */
		notify ("SUP: Can't create directory %s for %s\n",dname,fname);
		return (TRUE);
	}
	x = estabd (fname,dpart);
	if (x)  return (TRUE);
	(void) mkdir (dname,0755);
	if (stat (dname,&sbuf) < 0) {		/* didn't work */
		notify ("SUP: Can't create directory %s for %s\n",dname,fname);
		return (TRUE);
	}
	vnotify ("SUP Created directory %s for %s\n",dname,fname);
	return (FALSE);
}

/***************************************
 ***    L I S T   R O U T I N E S    ***
 ***************************************/

static
int Lhash (name)
char *name;
{
	/* Hash function is:  HASHSIZE * (strlen mod HASHSIZE)
	 *		      +          (char   mod HASHSIZE)
	 * where "char" is last character of name (if name is non-null).
	 */

	register int len;
	register char c;

	len = strlen (name);
	if (len > 0)	c = name[len-1];
	else		c = 0;
	return (((len&HASHMASK)<<HASHBITS)|(((int)c)&HASHMASK));
}

static
Linsert (table,name,number)
LIST **table;
char *name;
int number;
{
	register LIST *l;
	register int lno;
	lno = Lhash (name);
	l = (LIST *) malloc (sizeof(LIST));
	l->Lname = name;
	l->Lnumber = number;
	l->Lnext = table[lno];
	table[lno] = l;
}

static
LIST *Llookup (table,name)
LIST **table;
char *name;
{
	register int lno;
	register LIST *l;
	lno = Lhash (name);
	for (l = table[lno]; l && strcmp(l->Lname,name) != 0; l = l->Lnext);
	return (l);
}

ugconvert (uname,gname,uid,gid,mode)
char *uname,*gname;
int *uid,*gid,*mode;
{
	register LIST *u,*g;
	register struct passwd *pw;
	register struct group *gr;
	struct stat sbuf;
	static int defuid = -1;
	static int defgid;
	static int first = TRUE;

	if (first) {
		bzero ((char *)uidL, sizeof (uidL));
		bzero ((char *)gidL, sizeof (gidL));
		first = FALSE;
	}
	pw = NULL;
	if (u = Llookup (uidL,uname))
		*uid = u->Lnumber;
	else if (pw = getpwnam (uname)) {
		Linsert (uidL,salloc(uname),pw->pw_uid);
		*uid = pw->pw_uid;
	}
	if (u || pw) {
		if (g = Llookup (gidL,gname)) {
			*gid = g->Lnumber;
			return;
		}
		if (gr = getgrnam (gname)) {
			Linsert (gidL,salloc(gname),gr->gr_gid);
			*gid = gr->gr_gid;
			return;
		}
		if (pw == NULL)
			pw = getpwnam (uname);
		*mode &= ~S_ISGID;
		*gid = pw->pw_gid;
		return;
	}
	*mode &= ~(S_ISUID|S_ISGID);
	if (defuid >= 0) {
		*uid = defuid;
		*gid = defgid;
		return;
	}
	if (stat (".",&sbuf) < 0) {
		*uid = defuid = getuid ();
		*gid = defgid = getgid ();
		return;
	}
	*uid = defuid = sbuf.st_uid;
	*gid = defgid = sbuf.st_gid;
}


/*********************************************
 ***    U T I L I T Y   R O U T I N E S    ***
 *********************************************/

#if __STDC__
void
notify (char *fmt,...)		/* record error message */
#else
/*VARARGS*//*ARGSUSED*/
notify (va_alist)		/* record error message */
va_dcl
#endif
{
#if !__STDC__
	char *fmt;
#endif
	char buf[STRINGLENGTH];
	char collrelname[STRINGLENGTH];
	long tloc;
	static FILE *noteF = NULL;	/* mail program on pipe */
	va_list ap;

#if __STDC__
	va_start(ap,fmt);
#else
	va_start(ap);
	fmt = va_arg(ap,char *);
#endif
	if (fmt == NULL) {
		if (noteF && noteF != stdout)
			(void) pclose (noteF);
		noteF = NULL;
		return;
	}
	if ((thisC->Cflags&CFURELSUF) && thisC->Crelease) 
		(void) sprintf (collrelname,"%s-%s",collname,thisC->Crelease);
	else
		(void) strcpy (collrelname,collname);
	
	if (noteF == NULL) {
		if ((thisC->Cflags&CFMAIL) && thisC->Cnotify) {
			(void) sprintf (buf,"mail -s \"SUP Upgrade of %s\" %s >/dev/null",
				collrelname,thisC->Cnotify);
			noteF = popen (buf,"w");
			if (noteF == NULL) {
				logerr ("Can't send mail to %s for %s",
					thisC->Cnotify,collrelname);
				noteF = stdout;
			}
		} else
			noteF = stdout;
		tloc = time ((long *)NULL);
		fprintf (noteF,"SUP Upgrade of %s at %s",
			collrelname,ctime (&tloc));
		(void) fflush (noteF);
	}
	vfprintf(noteF,fmt,ap);
	va_end(ap);
	(void) fflush (noteF);
}

lockout (on)		/* lock out interrupts */
int on;
{
	register int x;
	static int lockmask;

	if (on) {
		x = sigmask (SIGHUP) | sigmask (SIGINT) |
		    sigmask (SIGQUIT) | sigmask (SIGTERM);
		lockmask = sigblock (x);
	}
	else {
		(void) sigsetmask (lockmask);
	}
}

char *fmttime (time)
long time;
{
	static char buf[STRINGLENGTH];
	int len;

	(void) strcpy (buf,ctime (&time));
	len = strlen(buf+4)-6;
	(void) strncpy (buf,buf+4,len);
	buf[len] = '\0';
	return (buf);
}
