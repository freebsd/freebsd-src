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
 * sup collection parsing routines
 **********************************************************************
 * HISTORY
 *
 * 7-July-93  Nate Williams at Montana State University
 *	Modified SUP to use gzip based compression when sending files
 *	across the network to save BandWidth
 *
 * $Log: supcparse.c,v $
 * Revision 1.1.1.1  1995/12/26 04:54:46  peter
 * Import the unmodified version of the sup that we are using.
 * The heritage of this version is not clear.  It appears to be NetBSD
 * derived from some time ago.
 *
 * Revision 1.2  1994/08/11  02:46:25  rich
 * Added extensions written by David Dawes.  From the man page:
 *
 * The -u flag, or the noupdate supfile option prevent updates from
 * occurring for regular files where the modification time and mode
 * hasn't changed.
 *
 * Now, how do we feed these patches back to CMU for consideration?
 *
 * Revision 1.1.1.1  1993/08/21  00:46:34  jkh
 * Current sup with compression support.
 *
 * Revision 1.1.1.1  1993/05/21  14:52:18  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 1.6  92/08/11  12:07:38  mrt
 * 	Added use-rel-suffix option corresponding to -u switch.
 * 	[92/07/26            mrt]
 * 
 * Revision 1.5  92/02/08  18:24:19  mja
 * 	Added "keep" supfile option, corresponding to -k switch.
 * 	[92/01/17            vdelvecc]
 * 
 * Revision 1.4  91/05/16  14:49:50  ern
 * 	Change default timeout from none to 3 hours so we don't accumalute 
 * 	processes running sups to dead hosts especially for users.
 * 	[91/05/16  14:49:21  ern]
 * 
 *
 * 10-Feb-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added timeout to backoff.
 *
 * 28-Jun-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added code for "release" support.  Removed obsolete options.
 *
 * 25-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Split off from sup.c
 *
 **********************************************************************
 */

#include "supcdefs.h"


#ifdef	lint
static char _argbreak;
#else
extern char _argbreak;			/* break character from nxtarg */
#endif

char default_renamelog[] = RENAMELOG;

typedef enum {				/* supfile options */
	OHOST, OBASE, OHOSTBASE, OPREFIX, ORELEASE,
	ONOTIFY, OLOGIN, OPASSWORD, OCRYPT, ORENAMELOG,
	OBACKUP, ODELETE, OEXECUTE, OOLD, OTIMEOUT, OKEEP, OURELSUF,
	OCOMPRESS, ONOUPDATE, OUNLINKBUSY
} OPTION;

struct option {
	char *op_name;
	OPTION op_enum;
} options[] = {
	"host",		OHOST,
	"base",		OBASE,
	"hostbase",	OHOSTBASE,
	"prefix",	OPREFIX,
	"release",	ORELEASE,
	"notify",	ONOTIFY,
	"login",	OLOGIN,
	"password",	OPASSWORD,
	"crypt",	OCRYPT,
	"renamelog",	ORENAMELOG,
	"backup",	OBACKUP,
	"delete",	ODELETE,
	"execute",	OEXECUTE,
	"old",		OOLD,
	"timeout",	OTIMEOUT,
	"keep",		OKEEP,
	"use-rel-suffix", OURELSUF,
 	"compress", 	OCOMPRESS,
	"noupdate",	ONOUPDATE,
	"unlinkbusy",	OUNLINKBUSY,
};

passdelim (ptr,delim)		/* skip over delimiter */
char **ptr,delim;
{
	*ptr = skipover (*ptr, " \t");
	if (_argbreak != delim && **ptr == delim) {
		(*ptr)++;
		*ptr = skipover (*ptr, " \t");
	}
}

parsecoll(c,collname,args)
COLLECTION *c;
char *collname,*args;
{
	register char *arg,*p;
	register OPTION option;
	int opno;

	c->Cnext = NULL;
	c->Cname = salloc (collname);
	c->Chost = NULL;
	c->Chtree = NULL;
	c->Cbase = NULL;
	c->Chbase = NULL;
	c->Cprefix = NULL;
	c->Crelease = NULL;
	c->Cnotify = NULL;
	c->Clogin = NULL;
	c->Cpswd = NULL;
	c->Ccrypt = NULL;
	c->Crenamelog = default_renamelog;
	c->Ctimeout = 3*60*60;	/* default to 3 hours instead of no timeout */
	c->Cflags = 0;
	c->Cnogood = FALSE;
	c->Clockfd = -1;
	args = skipover (args," \t");
	while (*(arg=nxtarg(&args," \t="))) {
		for (opno = 0; opno < sizeofA(options); opno++)
			if (strcmp (arg,options[opno].op_name) == 0)
				break;
		if (opno == sizeofA(options)) {
			logerr ("Invalid supfile option %s for collection %s",
				arg,c->Cname);
			return(-1);
		}
		option = options[opno].op_enum;
		switch (option) {
		case OHOST:
			passdelim (&args,'=');
			do {
				arg = nxtarg (&args,", \t");
				(void) Tinsert (&c->Chtree,arg,FALSE);
				arg = args;
				p = skipover (args," \t");
				if (*p++ == ',')  args = p;
			} while (arg != args);
			break;
		case OBASE:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Cbase = salloc (arg);
			break;
		case OHOSTBASE:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Chbase = salloc (arg);
			break;
		case OPREFIX:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Cprefix = salloc (arg);
			break;
		case ORELEASE:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Crelease = salloc (arg);
			break;
		case ONOTIFY:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Cnotify = salloc (arg);
			break;
		case OLOGIN:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Clogin = salloc (arg);
			break;
		case OPASSWORD:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Cpswd = salloc (arg);
			break;
		case OCRYPT:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Ccrypt = salloc (arg);
			break;
		case ORENAMELOG:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Crenamelog= salloc (arg);
			break;
		case OBACKUP:
			c->Cflags |= CFBACKUP;
			break;
		case ODELETE:
			c->Cflags |= CFDELETE;
			break;
		case OEXECUTE:
			c->Cflags |= CFEXECUTE;
			break;
		case OOLD:
			c->Cflags |= CFOLD;
			break;
		case OKEEP:
			c->Cflags |= CFKEEP;
			break;
		case OURELSUF:
			c->Cflags |= CFURELSUF;
			break;
		case OCOMPRESS:
			c->Cflags |= CFCOMPRESS;
			break;
		case ONOUPDATE:
			c->Cflags |= CFNOUPDATE;
			break;
		case OUNLINKBUSY:
			c->Cflags |= CFUNLINKBUSY;
			break;
		case OTIMEOUT:
			passdelim (&args,'=');
			arg = nxtarg (&args," \t");
			c->Ctimeout = atoi (arg);
			break;
		}
	}
	return(0);
}
