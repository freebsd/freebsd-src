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
 * sup client name server interface
 **********************************************************************
 * HISTORY
 * $Log: supcname.c,v $
 * Revision 1.1.1.1  1993/08/21  00:46:34  jkh
 * Current sup with compression support.
 *
 * Revision 1.1.1.1  1993/05/21  14:52:18  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 1.4  92/08/11  12:07:32  mrt
 * 	Added copyright.
 * 	[92/08/10            mrt]
 * 
 * 21-Dec-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Changed to no longer use a name server.
 *
 * 26-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Changed getnams and added several new routines to change the
 *	way that sup finds nameservers.  It now builds a tree of
 *	servers to check.  It walks over the tree.  At each node, it
 *	tries to contact the name server and get as many names
 *	resolved as it can.  It stops after either all collections
 *	have host names, or if some collections don't have host names
 *	but either everyone doesn't know what they are, or after too
 *	many tries, none could be reached.
 *
 * 25-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Split off from sup.c
 *
 **********************************************************************
 */

#include "supcdefs.h"

extern COLLECTION *firstC;		/* collection list pointer */

/*****************************************
 ***    G E T   H O S T   N A M E S    ***
 *****************************************/

/*
 * For each collection that doesn't have a host name specified, read
 * the file server list for the name of the host for that collection.
 * It's a fatal error if a collection has no file server.
 */

getnams ()
{
	register COLLECTION *c;
	char buf[STRINGLENGTH];
	register FILE *f;
	char *p,*q;

	for (c = firstC; c && c->Chtree != NULL; c = c->Cnext);
	if (c == NULL) return;
	(void) sprintf (buf,FILEHOSTS,DEFDIR);
	f = fopen (buf,"r");
	if (f == NULL)  logquit (1,"Can't open %s",buf);
	while ((p = fgets (buf,STRINGLENGTH,f)) != NULL) {
		if (q = index (p,'\n'))  *q = '\0';
		if (index ("#;:",*p))  continue;
		q = nxtarg (&p,"= \t");
		p = skipover (p," \t");
		if (*p == '=')  p++;
		p = skipover (p," \t");
		if (*p == '\0')  goaway ("error in collection/host file");
		do {
			if (strcmp (c->Cname, q) == 0) {
				do {
					q = nxtarg (&p,", \t");
					p = skipover (p," \t");
					if (*p == ',')  p++;
					p = skipover (p," \t");
					(void) Tinsert (&c->Chtree,q,FALSE);
				} while (*p != '\0');
			}
			while ((c = c->Cnext) != NULL && c->Chtree != NULL);
		} while (c != NULL);
		for (c = firstC; c && c->Chtree != NULL; c = c->Cnext);
		if (c == NULL) break;
	}
	(void) fclose (f);
	if (c == NULL)  return;
	do {
		logerr ("Host for collection %s not found",c->Cname);
		while ((c = c->Cnext) != NULL && c->Chtree != NULL);
	} while (c);
	logquit (1,"Hosts not found for all collections");
}
