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
 * stree.c -- SUP Tree Routines
 *
 **********************************************************************
 * HISTORY
 * $Log: stree.c,v $
 * Revision 1.1.1.1  1995/12/26 04:54:47  peter
 * Import the unmodified version of the sup that we are using.
 * The heritage of this version is not clear.  It appears to be NetBSD
 * derived from some time ago.
 *
 * Revision 1.1.1.1  1993/08/21  00:46:34  jkh
 * Current sup with compression support.
 *
 * Revision 1.1.1.1  1993/05/21  14:52:17  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 1.4  92/08/11  12:06:32  mrt
 * 	Added copyright. Delinted
 * 	[92/08/10            mrt]
 * 
 * 
 * Revision 1.3  89/08/15  15:30:57  bww
 * 	Changed code in Tlookup to Tsearch for each subpart of path.
 * 	Added indent formatting code to Tprint.
 * 	From "[89/06/24            gm0w]" at CMU.
 * 	[89/08/15            bww]
 * 
 * 20-May-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code to please lint.
 *
 * 29-Dec-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code to initialize new fields.  Added Tfree routine.
 *
 * 27-Sep-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Created.
 *
 **********************************************************************
 */

#include <libc.h>
#include <c.h>
#include <sys/param.h>
#include "sup.h"

#define Static	/* static		/* comment for debugging */

/*************************************************************
 ***    T R E E   P R O C E S S I N G   R O U T I N E S    ***
 *************************************************************/

Tfree (t)
register TREE **t;
{
	if (*t == NULL)  return;
	Tfree (&((*t)->Tlink));
	Tfree (&((*t)->Texec));
	Tfree (&((*t)->Tlo));
	Tfree (&((*t)->Thi));
	if ((*t)->Tname)  free ((*t)->Tname);
	if ((*t)->Tnewname)  free ((*t)->Tnewname);
	if ((*t)->Tuser)  free ((*t)->Tuser);
	if ((*t)->Tgroup)  free ((*t)->Tgroup);
	free (*(char **)t);
	*t = NULL;
}

Static
TREE *Tmake (p)
char *p;
{
	register TREE *t;
	t = (TREE *) malloc (sizeof (TREE));
	t->Tname = (p == NULL) ? NULL : salloc (p);
	t->Tnewname = NULL;
	t->Tflags = 0;
	t->Tuid = 0;
	t->Tgid = 0;
	t->Tuser = NULL;
	t->Tgroup = NULL;
	t->Tmode = 0;
	t->Tctime = 0;
	t->Tmtime = 0;
	t->Tlink = NULL;
	t->Texec = NULL;
	t->Tbf = 0;
	t->Tlo = NULL;
	t->Thi = NULL;
	return (t);
}

Static
TREE *Trotll (tp,tl)
register TREE *tp,*tl;
{
    tp->Tlo = tl->Thi;
    tl->Thi = tp;
    tp->Tbf = tl->Tbf = 0;
    return(tl);
}

Static
TREE *Trotlh (tp,tl)
register TREE *tp,*tl;
{
    register TREE *th;

    th = tl->Thi;
    tp->Tlo = th->Thi;
    tl->Thi = th->Tlo;
    th->Thi = tp;
    th->Tlo = tl;
    tp->Tbf = tl->Tbf = 0;
    if (th->Tbf == 1)
	tp->Tbf = -1;
    else if (th->Tbf == -1)
	tl->Tbf = 1;
    th->Tbf = 0;
    return(th);
}

Static
TREE *Trothl (tp,th)
register TREE *tp,*th;
{
    register TREE *tl;

    tl = th->Tlo;
    tp->Thi = tl->Tlo;
    th->Tlo = tl->Thi;
    tl->Tlo = tp;
    tl->Thi = th;
    tp->Tbf = th->Tbf = 0;
    if (tl->Tbf == -1)
	tp->Tbf = 1;
    else if (tl->Tbf == 1)
	th->Tbf = -1;
    tl->Tbf = 0;
    return(tl);
}

Static
TREE *Trothh (tp,th)
register TREE *tp,*th;
{
    tp->Thi = th->Tlo;
    th->Tlo = tp;
    tp->Tbf = th->Tbf = 0;
    return(th);
}

Static
Tbalance (t)
TREE **t;
{
    if ((*t)->Tbf < 2 && (*t)->Tbf > -2)
	return;
    if ((*t)->Tbf > 0) {
	if ((*t)->Tlo->Tbf > 0)
	    *t = Trotll(*t, (*t)->Tlo);
	else
	    *t = Trotlh(*t, (*t)->Tlo);
    } else {
	if ((*t)->Thi->Tbf > 0)
	    *t = Trothl(*t, (*t)->Thi);
	else
	    *t = Trothh(*t, (*t)->Thi);
    }
}

Static
TREE *Tinsertavl (t,p,find,dh)
TREE **t;
char *p;
int find;
int *dh;
{
	register TREE *newt;
	register int cmp;
	int deltah;

	if (*t == NULL) {
	    *t = Tmake (p);
	    *dh = 1;
	    return (*t);
	}
	if ((cmp = strcmp(p, (*t)->Tname)) == 0) {
	    if (!find)  return (NULL);	/* node already exists */
	    *dh = 0;
	    return (*t);
	} else if (cmp < 0) {
	    if ((newt = Tinsertavl (&((*t)->Tlo),p,find,&deltah)) == NULL)
		return (NULL);
	    (*t)->Tbf += deltah;
	} else {
	    if ((newt = Tinsertavl (&((*t)->Thi),p,find,&deltah)) == NULL)
		return (NULL);
	    (*t)->Tbf -= deltah;
	}
	Tbalance(t);
	if ((*t)->Tbf == 0) deltah = 0;
	*dh = deltah;
	return (newt);
}

TREE *Tinsert (t,p,find)
TREE **t;
register char *p;
int find;
{
	int deltah;

	if (p != NULL && p[0] == '.' && p[1] == '/') {
		p += 2;
		while (*p == '/') p++;
		if (*p == 0) p = ".";
	}
	return (Tinsertavl (t,p,find,&deltah));
}

TREE *Tsearch (t,p)
TREE *t;
char *p;
{
	register TREE *x;
	register int cmp;

	x = t;
	while (x) {
		cmp = strcmp (p,x->Tname);
		if (cmp == 0)  return (x);
		if (cmp < 0)	x = x->Tlo;
		else		x = x->Thi;
	}
	return (NULL);
}

TREE *Tlookup (t,p)
TREE *t;
char *p;
{
	register TREE *x;
	char buf[MAXPATHLEN+1];

	if (p == NULL)
		return (NULL);
	if (p[0] == '.' && p[1] == '/') {
		p += 2;
		while (*p == '/') p++;
		if (*p == 0) p = ".";
	}
	if ((x = Tsearch (t,p)) != NULL)
		return (x);
	if (*p != '/' && (x = Tsearch (t,".")) != NULL)
		return (x);
	(void) strncpy(buf, p, sizeof(buf)-1);
	buf[MAXPATHLEN] = '\0';
	while ((p = rindex(buf, '/')) != NULL) {
		while (p >= buf && *(p-1) == '/')
			p--;
		if (p == buf)
			*(p+1) = '\0';
		else
			*p = '\0';
		if ((x = Tsearch (t,buf)) != NULL)
			return (x);
		if (p == buf)
			break;
	}
	return (NULL);
}

Static int process_level;

Static
int Tsubprocess (t,reverse,f,argp)
TREE *t;
int reverse;
int (*f)();
int *argp;
{
	register int x = SCMOK;
	process_level++;
	if (reverse?t->Thi:t->Tlo)
		x = Tsubprocess (reverse?t->Thi:t->Tlo,
				 reverse,f,argp);
	if (x == SCMOK) {
		x = (*f) (t,argp);
		if (x == SCMOK) {
			if (reverse?t->Tlo:t->Thi)
				x = Tsubprocess (reverse?t->Tlo:t->Thi,
						 reverse,f,argp);
		}
	}
	process_level--;
	return (x);
}

/* VARARGS2 */
int Trprocess (t,f,args)
TREE *t;
int (*f)();
int args;
{
	if (t == NULL)  return (SCMOK);
	process_level = 0;
	return (Tsubprocess (t,TRUE,f,&args));
}

/* VARARGS2 */
int Tprocess (t,f,args)
TREE *t;
int (*f)();
int args;
{
	if (t == NULL)  return (SCMOK);
	process_level = 0;
	return (Tsubprocess (t,FALSE,f,&args));
}

Static
int Tprintone (t)
TREE *t;
{
	int i;
	for (i = 0; i < (process_level*2); i++)
		(void) putchar(' ');
	printf ("Node at %X name '%s' flags %o hi %X lo %X\n",t,t->Tname,t->Tflags,t->Thi,t->Tlo);
	return (SCMOK);
}

Tprint (t,p)		/* print tree -- for debugging */
TREE *t;
char *p;
{
	printf ("%s\n",p);
	(void) Tprocess (t,Tprintone);
	printf ("End of tree\n");
	(void) fflush (stdout);
}
