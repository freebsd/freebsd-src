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
 * 	@(#) src/sys/cfs/cfs_namecache.c,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
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
 * $Log: cfs_namecache.c,v $
 * Revision 1.1.1.1  1998/08/29 21:14:52  rvb
 * Very Preliminary Coda
 *
 * Revision 1.11  1998/08/28 18:12:16  rvb
 * Now it also works on FreeBSD -current.  This code will be
 * committed to the FreeBSD -current and NetBSD -current
 * trees.  It will then be tailored to the particular platform
 * by flushing conditional code.
 *
 * Revision 1.10  1998/08/18 17:05:14  rvb
 * Don't use __RCSID now
 *
 * Revision 1.9  1998/08/18 16:31:39  rvb
 * Sync the code for NetBSD -current; test on 1.3 later
 *
 * Revision 1.8  98/01/31  20:53:10  rvb
 * First version that works on FreeBSD 2.2.5
 * 
 * Revision 1.7  98/01/23  11:53:39  rvb
 * Bring RVB_CFS1_1 to HEAD
 * 
 * Revision 1.6.2.4  98/01/23  11:21:02  rvb
 * Sync with 2.2.5
 * 
 * Revision 1.6.2.3  97/12/16  12:40:03  rvb
 * Sync with 1.3
 * 
 * Revision 1.6.2.2  97/12/09  16:07:10  rvb
 * Sync with vfs/include/coda.h
 * 
 * Revision 1.6.2.1  97/12/06  17:41:18  rvb
 * Sync with peters coda.h
 * 
 * Revision 1.6  97/12/05  10:39:13  rvb
 * Read CHANGES
 * 
 * Revision 1.5.4.7  97/11/25  08:08:43  rvb
 * cfs_venus ... done; until cred/vattr change
 * 
 * Revision 1.5.4.6  97/11/24  15:44:43  rvb
 * Final cfs_venus.c w/o macros, but one locking bug
 * 
 * Revision 1.5.4.5  97/11/20  11:46:38  rvb
 * Capture current cfs_venus
 * 
 * Revision 1.5.4.4  97/11/18  10:27:13  rvb
 * cfs_nbsd.c is DEAD!!!; integrated into cfs_vf/vnops.c
 * cfs_nb_foo and cfs_foo are joined
 * 
 * Revision 1.5.4.3  97/11/13  22:02:57  rvb
 * pass2 cfs_NetBSD.h mt
 * 
 * Revision 1.5.4.2  97/11/12  12:09:35  rvb
 * reorg pass1
 * 
 * Revision 1.5.4.1  97/10/28  23:10:12  rvb
 * >64Meg; venus can be killed!
 * 
 * Revision 1.5  97/08/05  11:08:01  lily
 * Removed cfsnc_replace, replaced it with a cfs_find, unhash, and
 * rehash.  This fixes a cnode leak and a bug in which the fid is
 * not actually replaced.  (cfs_namecache.c, cfsnc.h, cfs_subr.c)
 * 
 * Revision 1.4  96/12/12  22:10:57  bnoble
 * Fixed the "downcall invokes venus operation" deadlock in all known cases.
 * There may be more
 * 
 * Revision 1.3  1996/11/08 18:06:09  bnoble
 * Minor changes in vnode operation signature, VOP_UPDATE signature, and
 * some newly defined bits in the include files.
 *
 * Revision 1.2  1996/01/02 16:56:50  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:15  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:07:57  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:07:56  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.3  1994/10/14  09:57:54  dcs
 * Made changes 'cause sun4s have braindead compilers
 *
 * Revision 2.2  94/08/28  19:37:35  luqi
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
 * 
 * Revision 2.1  94/07/21  16:25:15  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 *
 * Revision 1.2  92/10/27  17:58:21  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.3  92/09/30  14:16:20  mja
 * 	call cfs_flush instead of calling inode_uncache_try directly 
 * 	(from dcs). Also...
 * 
 * 	Substituted rvb's history blurb so that we agree with Mach 2.5 sources.
 * 	[91/02/09            jjk]
 * 
 * 	Added contributors blurb.
 * 	[90/12/13            jjk]
 * 
 * Revision 2.2  90/07/05  11:26:30  mrt
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.3  90/05/31  17:01:24  dcs
 * Prepare for merge with facilities kernel.
 * 
 * 
 */

/*
 * This module contains the routines to implement the CFS name cache. The
 * purpose of this cache is to reduce the cost of translating pathnames 
 * into Vice FIDs. Each entry in the cache contains the name of the file,
 * the vnode (FID) of the parent directory, and the cred structure of the
 * user accessing the file.
 *
 * The first time a file is accessed, it is looked up by the local Venus
 * which first insures that the user has access to the file. In addition
 * we are guaranteed that Venus will invalidate any name cache entries in
 * case the user no longer should be able to access the file. For these
 * reasons we do not need to keep access list information as well as a
 * cred structure for each entry.
 *
 * The table can be accessed through the routines cnc_init(), cnc_enter(),
 * cnc_lookup(), cnc_rmfidcred(), cnc_rmfid(), cnc_rmcred(), and cnc_purge().
 * There are several other routines which aid in the implementation of the
 * hash table.
 */

/*
 * NOTES: rvb@cs
 * 1.	The name cache holds a reference to every vnode in it.  Hence files can not be
 *	 closed or made inactive until they are released.
 * 2.	cfsnc_name(cp) was added to get a name for a cnode pointer for debugging.
 * 3.	cfsnc_find() has debug code to detect when entries are stored with different
 *	 credentials.  We don't understand yet, if/how entries are NOT EQ but still
 *	 EQUAL
 * 4.	I wonder if this name cache could be replace by the vnode name cache.
 *	The latter has no zapping functions, so probably not.
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/ucred.h>
#include <sys/select.h>

#ifndef insque
#include <sys/systm.h>
#endif /* insque */

#include <vm/vm.h>
#include <vm/vm_object.h>

#include <cfs/coda.h>
#include <cfs/cnode.h>
#include <cfs/cfsnc.h>

/* 
 * Declaration of the name cache data structure.
 */

int 	cfsnc_use = 1;			 /* Indicate use of CFS Name Cache */
int	cfsnc_size = CFSNC_CACHESIZE;	 /* size of the cache */
int	cfsnc_hashsize = CFSNC_HASHSIZE; /* size of the primary hash */

struct 	cfscache *cfsncheap;	/* pointer to the cache entries */
struct	cfshash  *cfsnchash;	/* hash table of cfscache pointers */
struct	cfslru   cfsnc_lru;	/* head of lru chain */

struct cfsnc_statistics cfsnc_stat;	/* Keep various stats */

/* 
 * for testing purposes
 */
int cfsnc_debug = 0;

/*
 * Entry points for the CFS Name Cache
 */
static struct cfscache *cfsnc_find(struct cnode *dcp, const char *name, int namelen,
	struct ucred *cred, int hash);
static void cfsnc_remove(struct cfscache *cncp, enum dc_status dcstat);

/*  
 * Initialize the cache, the LRU structure and the Hash structure(s)
 */

#define TOTAL_CACHE_SIZE 	(sizeof(struct cfscache) * cfsnc_size)
#define TOTAL_HASH_SIZE 	(sizeof(struct cfshash)  * cfsnc_hashsize)

int cfsnc_initialized = 0;      /* Initially the cache has not been initialized */

void
cfsnc_init(void)
{
    int i;

    /* zero the statistics structure */
    
    bzero(&cfsnc_stat, (sizeof(struct cfsnc_statistics)));

    printf("CFS NAME CACHE: CACHE %d, HASH TBL %d\n", CFSNC_CACHESIZE, CFSNC_HASHSIZE);
    CFS_ALLOC(cfsncheap, struct cfscache *, TOTAL_CACHE_SIZE);
    CFS_ALLOC(cfsnchash, struct cfshash *, TOTAL_HASH_SIZE);
    
    cfsnc_lru.lru_next = 
	cfsnc_lru.lru_prev = (struct cfscache *)LRU_PART(&cfsnc_lru);
    
    
    for (i=0; i < cfsnc_size; i++) {	/* initialize the heap */
	CFSNC_LRUINS(&cfsncheap[i], &cfsnc_lru);
	CFSNC_HSHNUL(&cfsncheap[i]);
	cfsncheap[i].cp = cfsncheap[i].dcp = (struct cnode *)0;
    }
    
    for (i=0; i < cfsnc_hashsize; i++) {	/* initialize the hashtable */
	CFSNC_HSHNUL((struct cfscache *)&cfsnchash[i]);
    }
    
    cfsnc_initialized++;
}

/*
 * Auxillary routines -- shouldn't be entry points
 */

static struct cfscache *
cfsnc_find(dcp, name, namelen, cred, hash)
	struct cnode *dcp;
	const char *name;
	int namelen;
	struct ucred *cred;
	int hash;
{
	/* 
	 * hash to find the appropriate bucket, look through the chain
	 * for the right entry (especially right cred, unless cred == 0) 
	 */
	struct cfscache *cncp;
	int count = 1;

	CFSNC_DEBUG(CFSNC_FIND, 
		    myprintf(("cfsnc_find(dcp %p, name %s, len %d, cred %p, hash %d\n",
			   dcp, name, namelen, cred, hash));)

	for (cncp = cfsnchash[hash].hash_next; 
	     cncp != (struct cfscache *)&cfsnchash[hash];
	     cncp = cncp->hash_next, count++) 
	{

	    if ((CFS_NAMEMATCH(cncp, name, namelen, dcp)) &&
		((cred == 0) || (cncp->cred == cred))) 
	    { 
		/* compare cr_uid instead */
		cfsnc_stat.Search_len += count;
		return(cncp);
	    }
#ifdef	DEBUG
	    else if (CFS_NAMEMATCH(cncp, name, namelen, dcp)) {
	    	printf("cfsnc_find: name %s, new cred = %p, cred = %p\n",
			name, cred, cncp->cred);
		printf("nref %d, nuid %d, ngid %d // oref %d, ocred %d, ogid %d\n",
			cred->cr_ref, cred->cr_uid, cred->cr_gid,
			cncp->cred->cr_ref, cncp->cred->cr_uid, cncp->cred->cr_gid);
		print_cred(cred);
		print_cred(cncp->cred);
	    }
#endif
	}

	return((struct cfscache *)0);
}

/*
 * Enter a new (dir cnode, name) pair into the cache, updating the
 * LRU and Hash as needed.
 */
void
cfsnc_enter(dcp, name, namelen, cred, cp)
    struct cnode *dcp;
    const char *name;
    int namelen;
    struct ucred *cred;
    struct cnode *cp;
{
    struct cfscache *cncp;
    int hash;
    
    if (cfsnc_use == 0)			/* Cache is off */
	return;
    
    CFSNC_DEBUG(CFSNC_ENTER, 
		myprintf(("Enter: dcp %p cp %p name %s cred %p \n",
		       dcp, cp, name, cred)); )
	
    if (namelen > CFSNC_NAMELEN) {
	CFSNC_DEBUG(CFSNC_ENTER, 
		    myprintf(("long name enter %s\n",name));)
	    cfsnc_stat.long_name_enters++;	/* record stats */
	return;
    }
    
    hash = CFSNC_HASH(name, namelen, dcp);
    cncp = cfsnc_find(dcp, name, namelen, cred, hash);
    if (cncp != (struct cfscache *) 0) {	
	cfsnc_stat.dbl_enters++;		/* duplicate entry */
	return;
    }
    
    cfsnc_stat.enters++;		/* record the enters statistic */
    
    /* Grab the next element in the lru chain */
    cncp = CFSNC_LRUGET(cfsnc_lru);
    
    CFSNC_LRUREM(cncp);	/* remove it from the lists */
    
    if (CFSNC_VALID(cncp)) {
	/* Seems really ugly, but we have to decrement the appropriate
	   hash bucket length here, so we have to find the hash bucket
	   */
	cfsnchash[CFSNC_HASH(cncp->name, cncp->namelen, cncp->dcp)].length--;
	
	cfsnc_stat.lru_rm++;	/* zapped a valid entry */
	CFSNC_HSHREM(cncp);
	vrele(CTOV(cncp->dcp)); 
	vrele(CTOV(cncp->cp));
	crfree(cncp->cred);
    }
    
    /*
     * Put a hold on the current vnodes and fill in the cache entry.
     */
    vref(CTOV(cp));
    vref(CTOV(dcp));
    crhold(cred); 
    cncp->dcp = dcp;
    cncp->cp = cp;
    cncp->namelen = namelen;
    cncp->cred = cred;
    
    bcopy(name, cncp->name, (unsigned)namelen);
    
    /* Insert into the lru and hash chains. */
    
    CFSNC_LRUINS(cncp, &cfsnc_lru);
    CFSNC_HSHINS(cncp, &cfsnchash[hash]);
    cfsnchash[hash].length++;                      /* Used for tuning */
    
    CFSNC_DEBUG(CFSNC_PRINTCFSNC, print_cfsnc(); )
}

/*
 * Find the (dir cnode, name) pair in the cache, if it's cred
 * matches the input, return it, otherwise return 0
 */
struct cnode *
cfsnc_lookup(dcp, name, namelen, cred)
	struct cnode *dcp;
	const char *name;
	int namelen;
	struct ucred *cred;
{
	int hash;
	struct cfscache *cncp;

	if (cfsnc_use == 0)			/* Cache is off */
		return((struct cnode *) 0);

	if (namelen > CFSNC_NAMELEN) {
	        CFSNC_DEBUG(CFSNC_LOOKUP, 
			    myprintf(("long name lookup %s\n",name));)
		cfsnc_stat.long_name_lookups++;		/* record stats */
		return((struct cnode *) 0);
	}

	/* Use the hash function to locate the starting point,
	   then the search routine to go down the list looking for
	   the correct cred.
 	 */

	hash = CFSNC_HASH(name, namelen, dcp);
	cncp = cfsnc_find(dcp, name, namelen, cred, hash);
	if (cncp == (struct cfscache *) 0) {
		cfsnc_stat.misses++;			/* record miss */
		return((struct cnode *) 0);
	}

	cfsnc_stat.hits++;

	/* put this entry at the end of the LRU */
	CFSNC_LRUREM(cncp);
	CFSNC_LRUINS(cncp, &cfsnc_lru);

	/* move it to the front of the hash chain */
	/* don't need to change the hash bucket length */
	CFSNC_HSHREM(cncp);
	CFSNC_HSHINS(cncp, &cfsnchash[hash]);

	CFSNC_DEBUG(CFSNC_LOOKUP, 
		printf("lookup: dcp %p, name %s, cred %p = cp %p\n",
			dcp, name, cred, cncp->cp); )

	return(cncp->cp);
}

static void
cfsnc_remove(cncp, dcstat)
	struct cfscache *cncp;
	enum dc_status dcstat;
{
	/* 
	 * remove an entry -- vrele(cncp->dcp, cp), crfree(cred),
	 * remove it from it's hash chain, and
	 * place it at the head of the lru list.
	 */
        CFSNC_DEBUG(CFSNC_REMOVE,
		    myprintf(("cfsnc_remove %s from parent %lx.%lx.%lx\n",
			   cncp->name, (cncp->dcp)->c_fid.Volume,
			   (cncp->dcp)->c_fid.Vnode, (cncp->dcp)->c_fid.Unique));)

  	CFSNC_HSHREM(cncp);

	CFSNC_HSHNUL(cncp);		/* have it be a null chain */
	if ((dcstat == IS_DOWNCALL) && (CTOV(cncp->dcp)->v_usecount == 1)) {
		cncp->dcp->c_flags |= C_PURGING;
	}
	vrele(CTOV(cncp->dcp)); 

	if ((dcstat == IS_DOWNCALL) && (CTOV(cncp->cp)->v_usecount == 1)) {
		cncp->cp->c_flags |= C_PURGING;
	}
	vrele(CTOV(cncp->cp)); 

	crfree(cncp->cred); 
	bzero(DATA_PART(cncp),DATA_SIZE);

	/* Put the null entry just after the least-recently-used entry */
	/* LRU_TOP adjusts the pointer to point to the top of the structure. */
	CFSNC_LRUREM(cncp);
	CFSNC_LRUINS(cncp, LRU_TOP(cfsnc_lru.lru_prev));
}

/*
 * Remove all entries with a parent which has the input fid.
 */
void
cfsnc_zapParentfid(fid, dcstat)
	ViceFid *fid;
	enum dc_status dcstat;
{
	/* To get to a specific fid, we might either have another hashing
	   function or do a sequential search through the cache for the
	   appropriate entries. The later may be acceptable since I don't
	   think callbacks or whatever Case 1 covers are frequent occurences.
	 */
	struct cfscache *cncp, *ncncp;
	int i;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CFSNC_DEBUG(CFSNC_ZAPPFID, 
		myprintf(("ZapParent: fid 0x%lx, 0x%lx, 0x%lx \n",
			fid->Volume, fid->Vnode, fid->Unique)); )

	cfsnc_stat.zapPfids++;

	for (i = 0; i < cfsnc_hashsize; i++) {

		/*
		 * Need to save the hash_next pointer in case we remove the
		 * entry. remove causes hash_next to point to itself.
		 */

		for (cncp = cfsnchash[i].hash_next; 
		     cncp != (struct cfscache *)&cfsnchash[i];
		     cncp = ncncp) {
			ncncp = cncp->hash_next;
			if ((cncp->dcp->c_fid.Volume == fid->Volume) &&
			    (cncp->dcp->c_fid.Vnode == fid->Vnode)   &&
			    (cncp->dcp->c_fid.Unique == fid->Unique)) {
			        cfsnchash[i].length--;      /* Used for tuning */
				cfsnc_remove(cncp, dcstat); 
			}
		}
	}
}


/*
 * Remove all entries which have the same fid as the input
 */
void
cfsnc_zapfid(fid, dcstat)
	ViceFid *fid;
	enum dc_status dcstat;
{
	/* See comment for zapParentfid. This routine will be used
	   if attributes are being cached. 
	 */
	struct cfscache *cncp, *ncncp;
	int i;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CFSNC_DEBUG(CFSNC_ZAPFID, 
		myprintf(("Zapfid: fid 0x%lx, 0x%lx, 0x%lx \n",
			fid->Volume, fid->Vnode, fid->Unique)); )

	cfsnc_stat.zapFids++;

	for (i = 0; i < cfsnc_hashsize; i++) {
		for (cncp = cfsnchash[i].hash_next; 
		     cncp != (struct cfscache *)&cfsnchash[i];
		     cncp = ncncp) {
			ncncp = cncp->hash_next;
			if ((cncp->cp->c_fid.Volume == fid->Volume) &&
			    (cncp->cp->c_fid.Vnode == fid->Vnode)   &&
			    (cncp->cp->c_fid.Unique == fid->Unique)) {
			        cfsnchash[i].length--;     /* Used for tuning */
				cfsnc_remove(cncp, dcstat); 
			}
		}
	}
}

/* 
 * Remove all entries which match the fid and the cred
 */
void
cfsnc_zapvnode(fid, cred, dcstat)	
	ViceFid *fid;
	struct ucred *cred;
	enum dc_status dcstat;
{
	/* See comment for zapfid. I don't think that one would ever
	   want to zap a file with a specific cred from the kernel.
	   We'll leave this one unimplemented.
	 */
	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CFSNC_DEBUG(CFSNC_ZAPVNODE, 
		myprintf(("Zapvnode: fid 0x%lx, 0x%lx, 0x%lx cred %p\n",
			  fid->Volume, fid->Vnode, fid->Unique, cred)); )

}

/*
 * Remove all entries which have the (dir vnode, name) pair
 */
void
cfsnc_zapfile(dcp, name, namelen)
	struct cnode *dcp;
	const char *name;
	int namelen;
{
	/* use the hash function to locate the file, then zap all
 	   entries of it regardless of the cred.
	 */
	struct cfscache *cncp;
	int hash;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CFSNC_DEBUG(CFSNC_ZAPFILE, 
		myprintf(("Zapfile: dcp %p name %s \n",
			  dcp, name)); )

	if (namelen > CFSNC_NAMELEN) {
		cfsnc_stat.long_remove++;		/* record stats */
		return;
	}

	cfsnc_stat.zapFile++;

	hash = CFSNC_HASH(name, namelen, dcp);
	cncp = cfsnc_find(dcp, name, namelen, 0, hash);

	while (cncp) {
	  cfsnchash[hash].length--;                 /* Used for tuning */

	  cfsnc_remove(cncp, NOT_DOWNCALL);
	  cncp = cfsnc_find(dcp, name, namelen, 0, hash);
	}
}

/* 
 * Remove all the entries for a particular user. Used when tokens expire.
 * A user is determined by his/her effective user id (id_uid).
 */
void
cfsnc_purge_user(uid, dcstat)
	vuid_t	uid;
	enum dc_status  dcstat;
{
	/* 
	 * I think the best approach is to go through the entire cache
	 * via HASH or whatever and zap all entries which match the
	 * input cred. Or just flush the whole cache.  It might be
	 * best to go through on basis of LRU since cache will almost
	 * always be full and LRU is more straightforward.  
	 */

	struct cfscache *cncp, *ncncp;
	int hash;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	CFSNC_DEBUG(CFSNC_PURGEUSER, 
		myprintf(("ZapDude: uid %lx\n", uid)); )
	cfsnc_stat.zapUsers++;

	for (cncp = CFSNC_LRUGET(cfsnc_lru);
	     cncp != (struct cfscache *)(&cfsnc_lru);
	     cncp = ncncp) {
		ncncp = CFSNC_LRUGET(*cncp);

		if ((CFSNC_VALID(cncp)) &&
		   ((cncp->cred)->cr_uid == uid)) {
		        /* Seems really ugly, but we have to decrement the appropriate
			   hash bucket length here, so we have to find the hash bucket
			   */
		        hash = CFSNC_HASH(cncp->name, cncp->namelen, cncp->dcp);
			cfsnchash[hash].length--;     /* For performance tuning */

			cfsnc_remove(cncp, dcstat); 
		}
	}
}

/*
 * Flush the entire name cache. In response to a flush of the Venus cache.
 */
void
cfsnc_flush(dcstat)
	enum dc_status dcstat;
{
	/* One option is to deallocate the current name cache and
	   call init to start again. Or just deallocate, then rebuild.
	   Or again, we could just go through the array and zero the 
	   appropriate fields. 
	 */
	
	/* 
	 * Go through the whole lru chain and kill everything as we go.
	 * I don't use remove since that would rebuild the lru chain
	 * as it went and that seemed unneccesary.
	 */
	struct cfscache *cncp;
	int i;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	cfsnc_stat.Flushes++;

	for (cncp = CFSNC_LRUGET(cfsnc_lru);
	     cncp != (struct cfscache *)&cfsnc_lru;
	     cncp = CFSNC_LRUGET(*cncp)) {
		if (CFSNC_VALID(cncp)) {

			CFSNC_HSHREM(cncp);	/* only zero valid nodes */
			CFSNC_HSHNUL(cncp);
			if ((dcstat == IS_DOWNCALL) 
			    && (CTOV(cncp->dcp)->v_usecount == 1))
			{
				cncp->dcp->c_flags |= C_PURGING;
			}
			vrele(CTOV(cncp->dcp)); 

			if (CTOV(cncp->cp)->v_flag & VTEXT) {
			    if (cfs_vmflush(cncp->cp))
				CFSDEBUG(CFS_FLUSH, 
					 myprintf(("cfsnc_flush: (%lx.%lx.%lx) busy\n", cncp->cp->c_fid.Volume, cncp->cp->c_fid.Vnode, cncp->cp->c_fid.Unique)); )
			}

			if ((dcstat == IS_DOWNCALL) 
			    && (CTOV(cncp->cp)->v_usecount == 1))
			{
				cncp->cp->c_flags |= C_PURGING;
			}
			vrele(CTOV(cncp->cp));  

			crfree(cncp->cred); 
			bzero(DATA_PART(cncp),DATA_SIZE);
		}
	}

	for (i = 0; i < cfsnc_hashsize; i++)
	  cfsnchash[i].length = 0;
}

/*
 * Debugging routines
 */

/* 
 * This routine should print out all the hash chains to the console.
 */
void
print_cfsnc(void)
{
	int hash;
	struct cfscache *cncp;

	for (hash = 0; hash < cfsnc_hashsize; hash++) {
		myprintf(("\nhash %d\n",hash));

		for (cncp = cfsnchash[hash].hash_next; 
		     cncp != (struct cfscache *)&cfsnchash[hash];
		     cncp = cncp->hash_next) {
			myprintf(("cp %p dcp %p cred %p name %s\n",
				  cncp->cp, cncp->dcp,
				  cncp->cred, cncp->name));
		     }
	}
}

void
cfsnc_gather_stats(void)
{
    int i, max = 0, sum = 0, temp, zeros = 0, ave, n;

	for (i = 0; i < cfsnc_hashsize; i++) {
	  if (cfsnchash[i].length) {
	    sum += cfsnchash[i].length;
	  } else {
	    zeros++;
	  }

	  if (cfsnchash[i].length > max)
	    max = cfsnchash[i].length;
	}

	/*
	 * When computing the Arithmetic mean, only count slots which 
	 * are not empty in the distribution.
	 */
        cfsnc_stat.Sum_bucket_len = sum;
        cfsnc_stat.Num_zero_len = zeros;
        cfsnc_stat.Max_bucket_len = max;

	if ((n = cfsnc_hashsize - zeros) > 0) 
	  ave = sum / n;
	else
	  ave = 0;

	sum = 0;
	for (i = 0; i < cfsnc_hashsize; i++) {
	  if (cfsnchash[i].length) {
	    temp = cfsnchash[i].length - ave;
	    sum += temp * temp;
	  }
	}
        cfsnc_stat.Sum2_bucket_len = sum;
}

/*
 * The purpose of this routine is to allow the hash and cache sizes to be
 * changed dynamically. This should only be used in controlled environments,
 * it makes no effort to lock other users from accessing the cache while it
 * is in an improper state (except by turning the cache off).
 */
int
cfsnc_resize(hashsize, heapsize, dcstat)
     int hashsize, heapsize;
     enum dc_status dcstat;
{
    if ((hashsize % 2) || (heapsize % 2)) { /* Illegal hash or cache sizes */
	return(EINVAL);
    }                 
    
    cfsnc_use = 0;                       /* Turn the cache off */
    
    cfsnc_flush(dcstat);                 /* free any cnodes in the cache */
    
    /* WARNING: free must happen *before* size is reset */
    CFS_FREE(cfsncheap,TOTAL_CACHE_SIZE);
    CFS_FREE(cfsnchash,TOTAL_HASH_SIZE);
    
    cfsnc_hashsize = hashsize;
    cfsnc_size = heapsize;
    
    cfsnc_init();                        /* Set up a cache with the new size */
    
    cfsnc_use = 1;                       /* Turn the cache back on */
    return(0);
}

#define DEBUG
#ifdef	DEBUG
char cfsnc_name_buf[CFS_MAXNAMLEN+1];

void
cfsnc_name(struct cnode *cp)
{
	struct cfscache *cncp, *ncncp;
	int i;

	if (cfsnc_use == 0)			/* Cache is off */
		return;

	for (i = 0; i < cfsnc_hashsize; i++) {
		for (cncp = cfsnchash[i].hash_next; 
		     cncp != (struct cfscache *)&cfsnchash[i];
		     cncp = ncncp) {
			ncncp = cncp->hash_next;
			if (cncp->cp == cp) {
				bcopy(cncp->name, cfsnc_name_buf, cncp->namelen);
				cfsnc_name_buf[cncp->namelen] = 0;
				printf(" is %s (%p,%p)@%p",
					cfsnc_name_buf, cncp->cp, cncp->dcp, cncp);
			}

		}
	}
}
#endif
