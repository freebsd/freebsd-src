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
 *	supcdefs.h -- Declarations shared by the collection of files
 *			that build the sup client.
 *
 **********************************************************************
 * HISTORY
 * 7-July-93  Nate Williams at Montana State University
 *	Modified SUP to use gzip based compression when sending files
 *	across the network to save BandWidth
 *
 * $Log: supcdefs.h,v $
 * Revision 1.3  1994/08/11  02:46:21  rich
 * Added extensions written by David Dawes.  From the man page:
 *
 * The -u flag, or the noupdate supfile option prevent updates from
 * occurring for regular files where the modification time and mode
 * hasn't changed.
 *
 * Now, how do we feed these patches back to CMU for consideration?
 *
 * Revision 1.2  1994/06/20  06:04:06  rgrimes
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
 * Revision 1.6  92/08/11  12:06:52  mrt
 * 	Added CFURELSUF  - use-release-suffix flag
 * 	Made rpause code conditional on MACH rather than CMUCS
 * 	[92/07/26            mrt]
 * 
 * Revision 1.5  92/02/08  18:23:57  mja
 * 	Added CFKEEP flag.
 * 	[92/01/17            vdelvecc]
 * 
 * 10-Feb-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added timeout for backoff.
 *
 * 28-Jun-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added Crelease for "release" support.
 *
 * 25-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Created.
 *
 **********************************************************************
 */

#include <libc.h>
#include <netdb.h>
#include <signal.h>
#include <setjmp.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/errno.h>
#if	MACH			/* used by resource pausing code only */
#include <sys/ioctl.h>
#include <sys/resource.h>
#endif	/* MACH */
#include <c.h>
#include "sup.h"
#include "supmsg.h"

extern int errno;
extern uid_t getuid();
extern gid_t getgid();
extern long time();

extern int PGMVERSION;

/*******************************************
 ***    D A T A   S T R U C T U R E S    ***
 *******************************************/

struct collstruct {			/* one per collection to be upgraded */
	char *Cname;			/* collection name */
	TREE *Chost;			/* attempted host for collection */
	TREE *Chtree;			/* possible hosts for collection */
	char *Cbase;			/* local base directory */
	char *Chbase;			/* remote base directory */
	char *Cprefix;			/* local collection pathname prefix */
	char *Crelease;			/* release name */
	char *Cnotify;			/* user to notify of status */
	char *Clogin;			/* remote login name */
	char *Cpswd;			/* remote password */
	char *Ccrypt;			/* data encryption key */
	int Ctimeout;			/* timeout for backoff */
	int Cflags;			/* collection flags */
	int Cnogood;			/* upgrade no good, "when" unchanged */
	int Clockfd;			/* >= 0 if collection is locked */
	struct collstruct *Cnext;	/* next collection */
};
typedef struct collstruct COLLECTION;

#define CFALL		00001
#define CFBACKUP	00002
#define CFDELETE	00004
#define CFEXECUTE	00010
#define CFLIST		00020
#define CFLOCAL		00040
#define CFMAIL		00100
#define CFOLD		00200
#define CFVERBOSE	00400
#define CFKEEP		01000
#define CFURELSUF	02000
#define CFCOMPRESS	04000
#define CFNOUPDATE     010000

/*************************
 ***	M A C R O S    ***
 *************************/

#define vnotify	if (thisC->Cflags&CFVERBOSE)  notify
/*
 * C prototypes
 */
#if __STDC__
void	done	__P((int value,char *fmt,...));
void	goaway	__P((char *fmt,...));
void	notify	__P((char *fmt,...));
#endif
