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
 * 	@(#) src/sys/coda/coda_namecache.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $ 
 * $FreeBSD$
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
 * This code was written for the Coda filesystem at Carnegie Mellon University.
 * Contributers include David Steere, James Kistler, and M. Satyanarayanan.
 */

#ifndef _CODA_NC_HEADER_
#define _CODA_NC_HEADER_

/*
 * Coda constants
 */
#define CODA_NC_NAMELEN	15		/* longest name stored in cache */
#define CODA_NC_CACHESIZE 256		/* Default cache size */
#define CODA_NC_HASHSIZE	64		/* Must be multiple of 2 */

/*
 * Hash function for the primary hash.
 */

/* 
 * First try -- (first + last letters + length + (int)cp) mod size
 * 2nd try -- same, except dir fid.vnode instead of cp
 */

#ifdef	oldhash
#define CODA_NC_HASH(name, namelen, cp) \
	((name[0] + name[namelen-1] + namelen + (int)(intptr_t)(cp)) & (coda_nc_hashsize-1))
#else
#define CODA_NC_HASH(name, namelen, cp) \
	((name[0] + (name[namelen-1]<<4) + namelen + (((int)(intptr_t)cp)>>8)) & (coda_nc_hashsize-1))
#endif

#define CODA_NAMEMATCH(cp, name, namelen, dcp) \
	((namelen == cp->namelen) && (dcp == cp->dcp) && \
		 (bcmp(cp->name,name,namelen) == 0))

/*
 * Functions to modify the hash and lru chains.
 * insque and remque assume that the pointers are the first thing
 * in the list node, thus the trickery for lru.
 */

#define CODA_NC_HSHINS(elem, pred)	insque(elem,pred)
#define CODA_NC_HSHREM(elem)		remque(elem)
#define CODA_NC_HSHNUL(elem)		(elem)->hash_next = \
					(elem)->hash_prev = (elem)

#define CODA_NC_LRUINS(elem, pred)	insque(LRU_PART(elem), LRU_PART(pred))
#define CODA_NC_LRUREM(elem)		remque(LRU_PART(elem));
#define CODA_NC_LRUGET(lruhead)		LRU_TOP((lruhead).lru_prev)

#define CODA_NC_VALID(cncp)	(cncp->dcp != (struct cnode *)0)
 
#define LRU_PART(cncp)			(struct coda_cache *) \
				((char *)cncp + (2*sizeof(struct coda_cache *)))
#define LRU_TOP(cncp)				(struct coda_cache *) \
			((char *)cncp - (2*sizeof(struct coda_cache *)))
#define DATA_PART(cncp)				(struct coda_cache *) \
			((char *)cncp + (4*sizeof(struct coda_cache *)))
#define DATA_SIZE	(sizeof(struct coda_cache)-(4*sizeof(struct coda_cache *)))

/*
 * Structure for an element in the CODA Name Cache.
 * NOTE: I use the position of arguments and their size in the
 * implementation of the functions CODA_NC_LRUINS, CODA_NC_LRUREM, and
 * DATA_PART.
 */

struct coda_cache {	
	struct coda_cache	*hash_next,*hash_prev;	/* Hash list */
	struct coda_cache	*lru_next, *lru_prev;	/* LRU list */
	struct cnode	*cp;			/* vnode of the file */
	struct cnode	*dcp;			/* parent's cnode */
	struct ucred	*cred;			/* user credentials */
	char		name[CODA_NC_NAMELEN];	/* segment name */
	int		namelen;		/* length of name */
};

struct	coda_lru {		/* Start of LRU chain */
	char *dummy1, *dummy2;			/* place holders */
	struct coda_cache *lru_next, *lru_prev;   /* position of pointers is important */
};


struct coda_hash {		/* Start of Hash chain */
	struct coda_cache *hash_next, *hash_prev; /* NOTE: chain pointers must be first */
        int length;                             /* used for tuning purposes */
};


/* 
 * Symbols to aid in debugging the namecache code. Assumes the existence
 * of the variable coda_nc_debug, which is defined in cfs_namecache.c
 */
#define CODA_NC_DEBUG(N, STMT)     { if (coda_nc_debug & (1 <<N)) { STMT } }

/* Prototypes of functions exported within cfs */
extern void coda_nc_init(void);
extern void coda_nc_enter(struct cnode *, const char *, int, struct ucred *, struct cnode *);
extern struct cnode *coda_nc_lookup(struct cnode *, const char *, int, struct ucred *);

extern void coda_nc_zapParentfid(CodaFid *, enum dc_status);
extern void coda_nc_zapfid(CodaFid *, enum dc_status);
extern void coda_nc_zapvnode(CodaFid *, struct ucred *, enum dc_status);
extern void coda_nc_zapfile(struct cnode *, const char *, int);
extern void coda_nc_purge_user(uid_t, enum dc_status);
extern void coda_nc_flush(enum dc_status);

extern void print_coda_nc(void);
extern void coda_nc_gather_stats(void);
extern int  coda_nc_resize(int, int, enum dc_status);
extern void coda_nc_name(struct cnode *cp);

/*
 * Structure to contain statistics on the cache usage
 */

struct coda_nc_statistics {
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

#define CODA_NC_FIND		((u_long) 1)
#define CODA_NC_REMOVE		((u_long) 2)
#define CODA_NC_INIT		((u_long) 3)
#define CODA_NC_ENTER		((u_long) 4)
#define CODA_NC_LOOKUP		((u_long) 5)
#define CODA_NC_ZAPPFID		((u_long) 6)
#define CODA_NC_ZAPFID		((u_long) 7)
#define CODA_NC_ZAPVNODE		((u_long) 8)
#define CODA_NC_ZAPFILE		((u_long) 9)
#define CODA_NC_PURGEUSER		((u_long) 10)
#define CODA_NC_FLUSH		((u_long) 11)
#define CODA_NC_PRINTCODA_NC	((u_long) 12)
#define CODA_NC_PRINTSTATS	((u_long) 13)

#endif
