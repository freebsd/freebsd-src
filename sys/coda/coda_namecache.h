/*
 * 
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 * 
 *           Copyright (c) 1987-1998 Carnegie Mellon University
 *                          All Rights Reserved
 * 
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 * 
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 * 
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 * 
 * 	@(#) src/sys/cfs/cfsnc.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $ 
 *  $Id: $
 * 
 */

/* 
 * Mach Operating System
 * Copyright (c) 1990 Carnegie-Mellon University
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon University.
 * Contributers include David Steere, James Kistler, and M. Satyanarayanan.
 */

/* 
 * HISTORY
 * $Log: cfsnc.h,v $
 * Revision 1.1.1.1  1998/08/29 21:14:52  rvb
 * Very Preliminary Coda
 *
 * Revision 1.8  1998/08/28 18:12:25  rvb
 * Now it also works on FreeBSD -current.  This code will be
 * committed to the FreeBSD -current and NetBSD -current
 * trees.  It will then be tailored to the particular platform
 * by flushing conditional code.
 *
 * Revision 1.7  1998/08/18 17:05:24  rvb
 * Don't use __RCSID now
 *
 * Revision 1.6  1998/08/18 16:31:49  rvb
 * Sync the code for NetBSD -current; test on 1.3 later
 *
 * Revision 1.5  98/01/23  11:53:51  rvb
 * Bring RVB_CFS1_1 to HEAD
 * 
 * Revision 1.4.2.1  97/12/16  12:40:23  rvb
 * Sync with 1.3
 * 
 * Revision 1.4  97/12/05  10:39:29  rvb
 * Read CHANGES
 * 
 * Revision 1.3.4.3  97/11/24  15:44:51  rvb
 * Final cfs_venus.c w/o macros, but one locking bug
 * 
 * Revision 1.3.4.2  97/11/12  12:09:44  rvb
 * reorg pass1
 * 
 * Revision 1.3.4.1  97/11/06  21:06:05  rvb
 * don't include headers in headers
 * 
 * Revision 1.3  97/08/05  11:08:19  lily
 * Removed cfsnc_replace, replaced it with a cfs_find, unhash, and
 * rehash.  This fixes a cnode leak and a bug in which the fid is
 * not actually replaced.  (cfs_namecache.c, cfsnc.h, cfs_subr.c)
 * 
 * Revision 1.2  96/01/02  16:57:19  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 * 
 * Revision 1.1.2.1  1995/12/20 01:57:45  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:22  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:21  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.2  1994/08/28  19:37:39  luqi
 * Add a new CFS_REPLACE call to allow venus to replace a ViceFid in the
 * mini-cache.
 *
 * In "cfs.h":
 * Add CFS_REPLACE decl.
 *
 * In "cfs_namecache.c":
 * Add routine cfsnc_replace.
 *
 * In "cfs_subr.c":
 * Add case-statement to process CFS_REPLACE.
 *
 * In "cfsnc.h":
 * Add decl for CFSNC_REPLACE.
 *
 * Revision 2.1  94/07/21  16:25:27  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 *
 * Revision 1.2  92/10/27  17:58:34  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.2  90/07/05  11:27:04  mrt
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.4  90/05/31  17:02:12  dcs
 * Prepare for merge with facilities kernel.
 * 
 * 
 */
#ifndef _CFSNC_HEADER_
#define _CFSNC_HEADER_

/*
 * Cfs constants
 */
#define CFSNC_NAMELEN	15		/* longest name stored in cache */
#define CFSNC_CACHESIZE 256		/* Default cache size */
#define CFSNC_HASHSIZE	64		/* Must be multiple of 2 */

/*
 * Hash function for the primary hash.
 */

/* 
 * First try -- (first + last letters + length + (int)cp) mod size
 * 2nd try -- same, except dir fid.vnode instead of cp
 */

#ifdef	oldhash
#define CFSNC_HASH(name, namelen, cp) \
	((name[0] + name[namelen-1] + namelen + (int)(cp)) & (cfsnc_hashsize-1))
#else
#define CFSNC_HASH(name, namelen, cp) \
	((name[0] + (name[namelen-1]<<4) + namelen + (((int)cp)>>8)) & (cfsnc_hashsize-1))
#endif

#define CFS_NAMEMATCH(cp, name, namelen, dcp) \
	((namelen == cp->namelen) && (dcp == cp->dcp) && \
		 (bcmp(cp->name,name,namelen) == 0))

/*
 * Functions to modify the hash and lru chains.
 * insque and remque assume that the pointers are the first thing
 * in the list node, thus the trickery for lru.
 */

#define CFSNC_HSHINS(elem, pred)	insque(elem,pred)
#define CFSNC_HSHREM(elem)		remque(elem)
#define CFSNC_HSHNUL(elem)		(elem)->hash_next = \
					(elem)->hash_prev = (elem)

#define CFSNC_LRUINS(elem, pred)	insque(LRU_PART(elem), LRU_PART(pred))
#define CFSNC_LRUREM(elem)		remque(LRU_PART(elem));
#define CFSNC_LRUGET(lruhead)		LRU_TOP((lruhead).lru_prev)

#define CFSNC_VALID(cncp)	(cncp->dcp != (struct cnode *)0)
 
#define LRU_PART(cncp)			(struct cfscache *) \
				((char *)cncp + (2*sizeof(struct cfscache *)))
#define LRU_TOP(cncp)				(struct cfscache *) \
			((char *)cncp - (2*sizeof(struct cfscache *)))
#define DATA_PART(cncp)				(struct cfscache *) \
			((char *)cncp + (4*sizeof(struct cfscache *)))
#define DATA_SIZE	(sizeof(struct cfscache)-(4*sizeof(struct cfscache *)))

/*
 * Structure for an element in the CFS Name Cache.
 * NOTE: I use the position of arguments and their size in the
 * implementation of the functions CFSNC_LRUINS, CFSNC_LRUREM, and
 * DATA_PART.
 */

struct cfscache {	
	struct cfscache	*hash_next,*hash_prev;	/* Hash list */
	struct cfscache	*lru_next, *lru_prev;	/* LRU list */
	struct cnode	*cp;			/* vnode of the file */
	struct cnode	*dcp;			/* parent's cnode */
	struct ucred	*cred;			/* user credentials */
	char		name[CFSNC_NAMELEN];	/* segment name */
	int		namelen;		/* length of name */
};

struct	cfslru {		/* Start of LRU chain */
	char *dummy1, *dummy2;			/* place holders */
	struct cfscache *lru_next, *lru_prev;   /* position of pointers is important */
};


struct cfshash {		/* Start of Hash chain */
	struct cfscache *hash_next, *hash_prev; /* NOTE: chain pointers must be first */
        int length;                             /* used for tuning purposes */
};


/* 
 * Symbols to aid in debugging the namecache code. Assumes the existence
 * of the variable cfsnc_debug, which is defined in cfs_namecache.c
 */
#define CFSNC_DEBUG(N, STMT)     { if (cfsnc_debug & (1 <<N)) { STMT } }

/* Prototypes of functions exported within cfs */
extern void cfsnc_init(void);
extern void cfsnc_enter(struct cnode *, const char *, int, struct ucred *, struct cnode *);
extern struct cnode *cfsnc_lookup(struct cnode *, const char *, int, struct ucred *);

extern void cfsnc_zapParentfid(ViceFid *, enum dc_status);
extern void cfsnc_zapfid(ViceFid *, enum dc_status);
extern void cfsnc_zapvnode(ViceFid *, struct ucred *, enum dc_status);
extern void cfsnc_zapfile(struct cnode *, const char *, int);
extern void cfsnc_purge_user(vuid_t, enum dc_status);
extern void cfsnc_flush(enum dc_status);

extern void print_cfsnc(void);
extern void cfsnc_gather_stats(void);
extern int  cfsnc_resize(int, int, enum dc_status);
extern void cfsnc_name(struct cnode *cp);

/*
 * Structure to contain statistics on the cache usage
 */

struct cfsnc_statistics {
	unsigned	hits;
	unsigned	misses;
	unsigned	enters;
	unsigned	dbl_enters;
	unsigned	long_name_enters;
	unsigned	long_name_lookups;
	unsigned	long_remove;
	unsigned	lru_rm;
	unsigned	zapPfids;
	unsigned	zapFids;
	unsigned	zapFile;
	unsigned	zapUsers;
	unsigned	Flushes;
	unsigned        Sum_bucket_len;
	unsigned        Sum2_bucket_len;
	unsigned        Max_bucket_len;
	unsigned        Num_zero_len;
	unsigned        Search_len;
};

#define CFSNC_FIND		((u_long) 1)
#define CFSNC_REMOVE		((u_long) 2)
#define CFSNC_INIT		((u_long) 3)
#define CFSNC_ENTER		((u_long) 4)
#define CFSNC_LOOKUP		((u_long) 5)
#define CFSNC_ZAPPFID		((u_long) 6)
#define CFSNC_ZAPFID		((u_long) 7)
#define CFSNC_ZAPVNODE		((u_long) 8)
#define CFSNC_ZAPFILE		((u_long) 9)
#define CFSNC_PURGEUSER		((u_long) 10)
#define CFSNC_FLUSH		((u_long) 11)
#define CFSNC_PRINTCFSNC	((u_long) 12)
#define CFSNC_PRINTSTATS	((u_long) 13)

#endif
