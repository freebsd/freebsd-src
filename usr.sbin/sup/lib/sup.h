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
/* sup.h -- declarations for sup, supnamesrv, supfilesrv
 *
 * VERSION NUMBER for any program is given by:  a.b (c)
 * where	a = PROTOVERSION	is the protocol version #
 *		b = PGMVERSION		is program # within protocol
 *		c = scmversion		is communication module version
 *			(i.e. operating system for which scm is configured)
 **********************************************************************
 * HISTORY
 * 13-Sep-92  Mary Thompson (mrt) at Carnegie-Mellon University
 *	Changed name of DEFDIR from /usr/cs to /usr.
 *
 * 7-July-93  Nate Williams at Montana State University
 *	Modified SUP to use gzip based compression when sending files
 *	across the network to save BandWidth
 *
 * $Log: sup.h,v $
 * Revision 1.1.1.1  1993/08/21  00:46:34  jkh
 * Current sup with compression support.
 *
 * Revision 1.1.1.1  1993/05/21  14:52:18  cgd
 * initial import of CMU's SUP to NetBSD
 *
 * Revision 1.10  92/08/11  12:06:42  mrt
 * 	Added definition for DEBUGFPORTNUM, the debugging port number.
 * 	Changed so that last and when file names could include
 * 	the relase name if any.
 * 	[92/07/23            mrt]
 * 
 * Revision 1.9  91/04/29  14:39:03  mja
 * 	Reduce MAXCHILDREN from 8 to 3.
 * 
 * Revision 1.8  89/08/23  14:55:30  gm0w
 * 	Moved coll.dir from supservers to supfiles.
 * 	[89/08/23            gm0w]
 * 
 * 18-Mar-88  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added host=<hostfile> support to releases file.
 *
 * 27-Dec-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added crosspatch support.  Removed nameserver support.
 *
 * 27-Jun-87  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added TREELIST and other changes for "release" support.
 *
 * 25-May-87  Doug Philips (dwp) at Carnegie-Mellon University
 *	Version 6 of the network protocol, better support to reflect errors
 *	back to server logfile.
 *
 * 21-May-87  Chriss Stephens (chriss) at Carnegie Mellon University
 *	Merged divergent CS and EE versions.
 *
 * 19-Sep-86  Mike Accetta (mja) at Carnegie-Mellon University
 *	Added FILESUPTDEFAULT definition.
 *
 * 07-Jun-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Removed FILESRVBUSYWAIT.  Now uses exponential backoff.
 *
 * 30-May-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Added numeric port numbers to use when port names are not in the
 *	host table.
 *
 * 04-Jan-86  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Update protocol version to 5 for name server protocol change to
 *	allow multiple repositories per collection.  Added FILESRVBUSYWAIT
 *	of 5 minutes.  Added FILELOCK file to indicate collections that
 *	should be exclusively locked when upgraded.
 *
 * 22-Sep-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Merged 4.1 and 4.2 versions together.
 *
 * 04-Jun-85  Steven Shafer (sas) at Carnegie-Mellon University
 *	Created for 4.2 BSD.
 *
 **********************************************************************
 */

/* PGMVERSION is defined separately in each program */
extern char scmversion[];		/* string version of scm */
#define PROTOVERSION 8			/* version of network protocol */
#define SCANVERSION  2			/* version of scan file format */

/* TCP servers for name server and file server */
#define FILEPORT	"supfilesrv"
#define FILEPORTNUM	871
#define DEBUGFPORT	"supfiledbg"
#define DEBUGFPORTNUM	1127

/* Data files used in scan.c */
#define FILELIST	"sup/%s/%s"
#define FILESCAN	"sup/%s/%s"
#define FILEHOST	"sup/%s/%s"
#define FILELISTDEF	"list"
#define FILESCANDEF	"scan"
#define FILEHOSTDEF	"host"
#define DEFRELEASE	"default"

/* Data files used in sup.c */
#define FILEBASEDEFAULT	"/usr/%s" /* also supfilesrv and supscan */
#ifdef EE_XXX
#define FILESUPDEFAULT	"%s/supfiles/coll.list"
#define FILESUPTDEFAULT	"%s/supfiles/coll.what"
#define FILEHOSTS	"%s/supfiles/coll.host"
#else  EE_XXX
#define FILESUPDEFAULT	"%s/lib/supfiles/coll.list"
#define FILESUPTDEFAULT	"%s/lib/supfiles/coll.what"
#define FILEHOSTS	"%s/lib/supfiles/coll.host"
#endif EE_XXX
#define FILEBKDIR	"%s/BACKUP"
#define FILEBACKUP	"%s/BACKUP/%s"
#define FILELAST	"sup/%s/last%s"
#define FILELASTTEMP	"sup/%s/last.temp"
#define FILELOCK	"sup/%s/lock"	/* also supfilesrv */
#define FILEREFUSE	"sup/%s/refuse"
#define FILEWHEN	"sup/%s/when%s"

/* Data files used in supfilesrv.c */
#define FILEXPATCH	"%s/sup/xpatch.host"
#ifdef EE_XXX
#define FILEDIRS	"%s/supfiles/coll.dir" /* also supscan */
#else  EE_XXX
#define FILEDIRS	"%s/lib/supfiles/coll.dir" /* also supscan */
#endif EE_XXX
#define FILECRYPT	"sup/%s/crypt"
#define FILELOGFILE	"sup/%s/logfile"
#define FILEPREFIX	"sup/%s/prefix"	/* also supscan */
#define FILERELEASES	"sup/%s/releases" /* also supscan */

/* String length */
#define STRINGLENGTH	2000

/* Password transmission encryption key */
#define PSWDCRYPT	"SuperMan"
/* Test string for encryption */
#define CRYPTTEST	"Hello there, Sailor Boy!"

/* Default directory for system sup information */
#ifndef	DEFDIR
#ifdef EE_XXX
#define DEFDIR		"/etc"
#else  EE_XXX
#define DEFDIR		"/usr"
#endif EE_XXX
#endif	DEFDIR

/* Default login account for file server */
#ifndef	DEFUSER
#define DEFUSER		"anon"
#endif	DEFUSER

/* subroutine return codes */
#define SCMOK		(1)		/* routine performed correctly */
#define SCMEOF		(0)		/* read EOF on network connection */
#define SCMERR		(-1)		/* error occurred during routine */

/* data structure for describing a file being upgraded */

struct treestruct {
/* fields for file information */
	char *Tname;			/* path component name */
	int Tflags;			/* flags of file */
	int Tmode;			/* st_mode of file */
	char *Tuser;			/* owner of file */
	int Tuid;			/* owner id of file */
	char *Tgroup;			/* group of file */
	int Tgid;			/* group id of file */
	int Tctime;			/* inode modification time */
	int Tmtime;			/* data modification time */
	struct treestruct *Tlink;	/* tree of link names */
	struct treestruct *Texec;	/* tree of execute commands */
/* fields for sibling AVL tree */
	int Tbf;			/* balance factor */
	struct treestruct *Tlo,*Thi;	/* ordered sibling tree */
};
typedef struct treestruct TREE;

/* data structure to represent a list of trees to upgrade */

struct tliststruct {
	struct tliststruct *TLnext;	/* next entry in tree list */
/* fields for tree information */
	char *TLname;			/* release name for tree */
	char *TLprefix;			/* prefix of tree */
	char *TLlist;			/* name of list file */
	char *TLscan;			/* name of scan file */
	char *TLhost;			/* name of host file */
	TREE *TLtree;			/* tree of files to upgrade */
};
typedef struct tliststruct TREELIST;

/* bitfield not defined in stat.h */
#define S_IMODE		  07777		/* part of st_mode that chmod sets */

/* flag bits for files */
#define FNEW		     01		/* ctime of file has changed */
#define FBACKUP		     02		/* backup of file is allowed */
#define FNOACCT		     04		/* don't set file information */
#define FUPDATE		    010		/* only set file information */
#define FNEEDED		0100000		/* file needed for upgrade */

/* version 3 compatability */
#define	FCOMPAT		0010000		/* Added to detect execute commands to send */

/* message types now obsolete */
#define MSGFEXECQ	(115)
#define MSGFEXECNAMES	(116)

/* flag bits for files in list of all files */
#define ALLNEW		      01
#define ALLBACKUP	      02
#define ALLEND		      04
#define ALLDIR		     010
#define ALLNOACCT	     020
#define ALLSLINK	    0100

/* flag bits for file mode word */
#define MODELINK	  010000
#define MODEDIR		  040000
#define MODESYM		 0100000
#define MODENOACCT	 0200000
#define MODEUPDATE	01000000

/* blocking factor for filenames in list of all file names */
#define BLOCKALL	32

/* end version 3 compatability */

#define MAXCHILDREN 3			/* maximum number of children allowed
					   to sup at the same time */

/* scm and stree external declarations */
char *remotehost();
TREE *Tinsert(),*Tsearch(),*Tlookup();
