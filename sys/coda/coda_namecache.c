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
 *  $Id: cfs_namecache.c,v 1.2 1998/09/02 19:09:53 rvb Exp $
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
 * Revision 1.2  1998/09/02 19:09:53  rvb
 * Pass2 complete
 *
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
 * Bring RVB_CODA1_1 to HEAD
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
 * Removed cfsnc_replace, replaced it with a coda_find, unhash, and
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
 * Added CODA-specific files
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
 * Add a new CODA_REPLACE call to allow venus to replace a ViceFid in the
 * mini-cache. 
 * 
 * In "cfs.h":
 * Add CODA_REPLACE decl.
 * 
 * In "cfs_namecache.c":
 * Add routine cfsnc_replace.
 * 
 * In "cfs_subr.c":
 * Add case-statement to process CODA_REPLACE.
 * 
 * In "cfsnc.h":
 * Add decl for CODA_NC_REPLACE.
 * 
 * 
 * Revision 2.1  94/07/21  16:25:15  satya
 * Conversion to C++ 3.0; start of Coda Release 2.0
 *
 * Revision 1.2  92/10/27  17:58:21  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.3  92/09/30  14:16:20  mja
 * 	call coda_flush instead of calling inode_uncache_try directly 
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
 * This module contains the routines to implement the CODA name cache. The
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
 * 2.	coda_nc_name(cp) was added to get a name for a cnode pointer for debugging.
 * 3.	coda_nc_find() has debug code to detect when entries are stored with different
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

int 	coda_nc_use = 1;			 /* Indicate use of CODA Name Cache */
int	coda_nc_size = CODA_NC_CACHESIZE;	 /* size of the cache */
int	coda_nc_hashsize = CODA_NC_HASHSIZE; /* size of the primary hash */

struct 	coda_cache *coda_nc_heap;	/* pointer to the cache entries */
struct	coda_hash  *coda_nc_hash;	/* hash table of coda_cache pointers */
struct	coda_lru   coda_nc_lru;		/* head of lru chain */

struct coda_nc_statistics coda_nc_stat;	/* Keep various stats */

/* 
 * for testing purposes
 */
int coda_nc_debug = 0;

/*
 * Entry points for the CODA Name Cache
 */
static struct coda_cache *coda_nc_find(struct cnode *dcp, const char *name, int namelen,
	struct ucred *cred, int hash);
static void coda_nc_remove(struct coda_cache *cncp, enum dc_status dcstat);

/*  
 * Initialize the cache, the LRU structure and the Hash structure(s)
 */

#define TOTAL_CACHE_SIZE 	(sizeof(struct coda_cache) * coda_nc_size)
#define TOTAL_HASH_SIZE 	(sizeof(struct coda_hash)  * coda_nc_hashsize)

int coda_nc_initialized = 0;      /* Initially the cache has not been initialized */

void
coda_nc_init(void)
{
    int i;

    /* zero the statistics structure */
    
    bzero(&coda_nc_stat, (sizeof(struct coda_nc_statistics)));

    printf("CODA NAME CACHE: CACHE %d, HASH TBL %d\n", CODA_NC_CACHESIZE, CODA_NC_HASHSIZE);
    CODA_ALLOC(coda_nc_heap, struct coda_cache *, TOTAL_CACHE_SIZE);
    CODA_ALLOC(coda_nc_hash, struct coda_hash *, TOTAL_HASH_SIZE);
    
    coda_nc_lru.lru_next = 
	coda_nc_lru.lru_prev = (struct coda_cache *)LRU_PART(&coda_nc_lru);
    
    
    for (i=0; i < coda_nc_size; i++) {	/* initialize the heap */
	CODA_NC_LRUINS(&coda_nc_heap[i], &coda_nc_lru);
	CODA_NC_HSHNUL(&coda_nc_heap[i]);
	coda_nc_heap[i].cp = coda_nc_heap[i].dcp = (struct cnode *)0;
    }
    
    for (i=0; i < coda_nc_hashsize; i++) {	/* initialize the hashtable */
	CODA_NC_HSHNUL((struct coda_cache *)&coda_nc_hash[i]);
    }
    
    coda_nc_initialized++;
}

/*
 * Auxillary routines -- shouldn't be entry points
 */

static struct coda_cache *
coda_nc_find(dcp, name, namelen, cred, hash)
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
	struct coda_cache *cncp;
	int count = 1;

	CODA_NC_DEBUG(CODA_NC_FIND, 
		    myprintf(("coda_nc_find(dcp %p, name %s, len %d, cred %p, hash %d\n",
			   dcp, name, namelen, cred, hash));)

	for (cncp = coda_nc_hash[hash].hash_next; 
	     cncp != (struct coda_cache *)&coda_nc_hash[hash];
	     cncp = cncp->hash_next, count++) 
	{

	    if ((CODA_NAMEMATCH(cncp, name, namelen, dcp)) &&
		((cred == 0) || (cncp->cred == cred))) 
	    { 
		/* compare cr_uid instead */
		coda_nc_stat.Search_len += count;
		return(cncp);
	    }
#ifdef	DEBUG
	    else if (CODA_NAMEMATCH(cncp, name, namelen, dcp)) {
	    	printf("coda_nc_find: name %s, new cred = %p, cred = %p\n",
			name, cred, cncp->cred);
		printf("nref %d, nuid %d, ngid %d // oref %d, ocred %d, ogid %d\n",
			cred->cr_ref, cred->cr_uid, cred->cr_gid,
			cncp->cred->cr_ref, cncp->cred->cr_uid, cncp->cred->cr_gid);
		print_cred(cred);
		print_cred(cncp->cred);
	    }
#endif
	}

	return((struct coda_cache *)0);
}

/*
 * Enter a new (dir cnode, name) pair into the cache, updating the
 * LRU and Hash as needed.
 */
void
coda_nc_enter(dcp, name, namelen, cred, cp)
    struct cnode *dcp;
    const char *name;
    int namelen;
    struct ucred *cred;
    struct cnode *cp;
{
    struct coda_cache *cncp;
    int hash;
    
    if (coda_nc_use == 0)			/* Cache is off */
	return;
    
    CODA_NC_DEBUG(CODA_NC_ENTER, 
		myprintf(("Enter: dcp %p cp %p name %s cred %p \n",
		       dcp, cp, name, cred)); )
	
    if (namelen > CODA_NC_NAMELEN) {
	CODA_NC_DEBUG(CODA_NC_ENTER, 
		    myprintf(("long name enter %s\n",name));)
	    coda_nc_stat.long_name_enters++;	/* record stats */
	return;
    }
    
    hash = CODA_NC_HASH(name, namelen, dcp);
    cncp = coda_nc_find(dcp, name, namelen, cred, hash);
    if (cncp != (struct coda_cache *) 0) {	
	coda_nc_stat.dbl_enters++;		/* duplicate entry */
	return;
    }
    
    coda_nc_stat.enters++;		/* record the enters statistic */
    
    /* Grab the next element in the lru chain */
    cncp = CODA_NC_LRUGET(coda_nc_lru);
    
    CODA_NC_LRUREM(cncp);	/* remove it from the lists */
    
    if (CODA_NC_VALID(cncp)) {
	/* Seems really ugly, but we have to decrement the appropriate
	   hash bucket length here, so we have to find the hash bucket
	   */
	coda_nc_hash[CODA_NC_HASH(cncp->name, cncp->namelen, cncp->dcp)].length--;
	
	coda_nc_stat.lru_rm++;	/* zapped a valid entry */
	CODA_NC_HSHREM(cncp);
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
    
    CODA_NC_LRUINS(cncp, &coda_nc_lru);
    CODA_NC_HSHINS(cncp, &coda_nc_hash[hash]);
    coda_nc_hash[hash].length++;                      /* Used for tuning */
    
    CODA_NC_DEBUG(CODA_NC_PRINTCODA_NC, print_coda_nc(); )
}

/*
 * Find the (dir cnode, name) pair in the cache, if it's cred
 * matches the input, return it, otherwise return 0
 */
struct cnode *
coda_nc_lookup(dcp, name, namelen, cred)
	struct cnode *dcp;
	const char *name;
	int namelen;
	struct ucred *cred;
{
	int hash;
	struct coda_cache *cncp;

	if (coda_nc_use == 0)			/* Cache is off */
		return((struct cnode *) 0);

	if (namelen > CODA_NC_NAMELEN) {
	        CODA_NC_DEBUG(CODA_NC_LOOKUP, 
			    myprintf(("long name lookup %s\n",name));)
		coda_nc_stat.long_name_lookups++;		/* record stats */
		return((struct cnode *) 0);
	}

	/* Use the hash function to locate the starting point,
	   then the search routine to go down the list looking for
	   the correct cred.
 	 */

	hash = CODA_NC_HASH(name, namelen, dcp);
	cncp = coda_nc_find(dcp, name, namelen, cred, hash);
	if (cncp == (struct coda_cache *) 0) {
		coda_nc_stat.misses++;			/* record miss */
		return((struct cnode *) 0);
	}

	coda_nc_stat.hits++;

	/* put this entry at the end of the LRU */
	CODA_NC_LRUREM(cncp);
	CODA_NC_LRUINS(cncp, &coda_nc_lru);

	/* move it to the front of the hash chain */
	/* don't need to change the hash bucket length */
	CODA_NC_HSHREM(cncp);
	CODA_NC_HSHINS(cncp, &coda_nc_hash[hash]);

	CODA_NC_DEBUG(CODA_NC_LOOKUP, 
		printf("lookup: dcp %p, name %s, cred %p = cp %p\n",
			dcp, name, cred, cncp->cp); )

	return(cncp->cp);
}

static void
coda_nc_remove(cncp, dcstat)
	struct coda_cache *cncp;
	enum dc_status dcstat;
{
	/* 
	 * remove an entry -- vrele(cncp->dcp, cp), crfree(cred),
	 * remove it from it's hash chain, and
	 * place it at the head of the lru list.
	 */
        CODA_NC_DEBUG(CODA_NC_REMOVE,
		    myprintf(("coda_nc_remove %s from parent %lx.%lx.%lx\n",
			   cncp->name, (cncp->dcp)->c_fid.Volume,
			   (cncp->dcp)->c_fid.Vnode, (cncp->dcp)->c_fid.Unique));)

  	CODA_NC_HSHREM(cncp);

	CODA_NC_HSHNUL(cncp);		/* have it be a null chain */
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
	CODA_NC_LRUREM(cncp);
	CODA_NC_LRUINS(cncp, LRU_TOP(coda_nc_lru.lru_prev));
}

/*
 * Remove all entries with a parent which has the input fid.
 */
void
coda_nc_zapParentfid(fid, dcstat)
	ViceFid *fid;
	enum dc_status dcstat;
{
	/* To get to a specific fid, we might either have another hashing
	   function or do a sequential search through the cache for the
	   appropriate entries. The later may be acceptable since I don't
	   think callbacks or whatever Case 1 covers are frequent occurences.
	 */
	struct coda_cache *cncp, *ncncp;
	int i;

	if (coda_nc_use == 0)			/* Cache is off */
		return;

	CODA_NC_DEBUG(CODA_NC_ZAPPFID, 
		myprintf(("ZapParent: fid 0x%lx, 0x%lx, 0x%lx \n",
			fid->Volume, fid->Vnode, fid->Unique)); )

	coda_nc_stat.zapPfids++;

	for (i = 0; i < coda_nc_hashsize; i++) {

		/*
		 * Need to save the hash_next pointer in case we remove the
		 * entry. remove causes hash_next to point to itself.
		 */

		for (cncp = coda_nc_hash[i].hash_next; 
		     cncp != (struct coda_cache *)&coda_nc_hash[i];
		     cncp = ncncp) {
			ncncp = cncp->hash_next;
			if ((cncp->dcp->c_fid.Volume == fid->Volume) &&
			    (cncp->dcp->c_fid.Vnode == fid->Vnode)   &&
			    (cncp->dcp->c_fid.Unique == fid->Unique)) {
			        coda_nc_hash[i].length--;      /* Used for tuning */
				coda_nc_remove(cncp, dcstat); 
			}
		}
	}
}


/*
 * Remove all entries which have the same fid as the input
 */
void
coda_nc_zapfid(fid, dcstat)
	ViceFid *fid;
	enum dc_status dcstat;
{
	/* See comment for zapParentfid. This routine will be used
	   if attributes are being cached. 
	 */
	struct coda_cache *cncp, *ncncp;
	int i;

	if (coda_nc_use == 0)			/* Cache is off */
		return;

	CODA_NC_DEBUG(CODA_NC_ZAPFID, 
		myprintf(("Zapfid: fid 0x%lx, 0x%lx, 0x%lx \n",
			fid->Volume, fid->Vnode, fid->Unique)); )

	coda_nc_stat.zapFids++;

	for (i = 0; i < coda_nc_hashsize; i++) {
		for (cncp = coda_nc_hash[i].hash_next; 
		     cncp != (struct coda_cache *)&coda_nc_hash[i];
		     cncp = ncncp) {
			ncncp = cncp->hash_next;
			if ((cncp->cp->c_fid.Volume == fid->Volume) &&
			    (cncp->cp->c_fid.Vnode == fid->Vnode)   &&
			    (cncp->cp->c_fid.Unique == fid->Unique)) {
			        coda_nc_hash[i].length--;     /* Used for tuning */
				coda_nc_remove(cncp, dcstat); 
			}
		}
	}
}

/* 
 * Remove all entries which match the fid and the cred
 */
void
coda_nc_zapvnode(fid, cred, dcstat)	
	ViceFid *fid;
	struct ucred *cred;
	enum dc_status dcstat;
{
	/* See comment for zapfid. I don't think that one would ever
	   want to zap a file with a specific cred from the kernel.
	   We'll leave this one unimplemented.
	 */
	if (coda_nc_use == 0)			/* Cache is off */
		return;

	CODA_NC_DEBUG(CODA_NC_ZAPVNODE, 
		myprintf(("Zapvnode: fid 0x%lx, 0x%lx, 0x%lx cred %p\n",
			  fid->Volume, fid->Vnode, fid->Unique, cred)); )

}

/*
 * Remove all entries which have the (dir vnode, name) pair
 */
void
coda_nc_zapfile(dcp, name, namelen)
	struct cnode *dcp;
	const char *name;
	int namelen;
{
	/* use the hash function to locate the file, then zap all
 	   entries of it regardless of the cred.
	 */
	struct coda_cache *cncp;
	int hash;

	if (coda_nc_use == 0)			/* Cache is off */
		return;

	CODA_NC_DEBUG(CODA_NC_ZAPFILE, 
		myprintf(("Zapfile: dcp %p name %s \n",
			  dcp, name)); )

	if (namelen > CODA_NC_NAMELEN) {
		coda_nc_stat.long_remove++;		/* record stats */
		return;
	}

	coda_nc_stat.zapFile++;

	hash = CODA_NC_HASH(name, namelen, dcp);
	cncp = coda_nc_find(dcp, name, namelen, 0, hash);

	while (cncp) {
	  coda_nc_hash[hash].length--;                 /* Used for tuning */

	  coda_nc_remove(cncp, NOT_DOWNCALL);
	  cncp = coda_nc_find(dcp, name, namelen, 0, hash);
	}
}

/* 
 * Remove all the entries for a particular user. Used when tokens expire.
 * A user is determined by his/her effective user id (id_uid).
 */
void
coda_nc_purge_user(uid, dcstat)
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

	struct coda_cache *cncp, *ncncp;
	int hash;

	if (coda_nc_use == 0)			/* Cache is off */
		return;

	CODA_NC_DEBUG(CODA_NC_PURGEUSER, 
		myprintf(("ZapDude: uid %lx\n", uid)); )
	coda_nc_stat.zapUsers++;

	for (cncp = CODA_NC_LRUGET(coda_nc_lru);
	     cncp != (struct coda_cache *)(&coda_nc_lru);
	     cncp = ncncp) {
		ncncp = CODA_NC_LRUGET(*cncp);

		if ((CODA_NC_VALID(cncp)) &&
		   ((cncp->cred)->cr_uid == uid)) {
		        /* Seems really ugly, but we have to decrement the appropriate
			   hash bucket length here, so we have to find the hash bucket
			   */
		        hash = CODA_NC_HASH(cncp->name, cncp->namelen, cncp->dcp);
			coda_nc_hash[hash].length--;     /* For performance tuning */

			coda_nc_remove(cncp, dcstat); 
		}
	}
}

/*
 * Flush the entire name cache. In response to a flush of the Venus cache.
 */
void
coda_nc_flush(dcstat)
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
	struct coda_cache *cncp;
	int i;

	if (coda_nc_use == 0)			/* Cache is off */
		return;

	coda_nc_stat.Flushes++;

	for (cncp = CODA_NC_LRUGET(coda_nc_lru);
	     cncp != (struct coda_cache *)&coda_nc_lru;
	     cncp = CODA_NC_LRUGET(*cncp)) {
		if (CODA_NC_VALID(cncp)) {

			CODA_NC_HSHREM(cncp);	/* only zero valid nodes */
			CODA_NC_HSHNUL(cncp);
			if ((dcstat == IS_DOWNCALL) 
			    && (CTOV(cncp->dcp)->v_usecount == 1))
			{
				cncp->dcp->c_flags |= C_PURGING;
			}
			vrele(CTOV(cncp->dcp)); 

			if (CTOV(cncp->cp)->v_flag & VTEXT) {
			    if (coda_vmflush(cncp->cp))
				CODADEBUG(CODA_FLUSH, 
					 myprintf(("coda_nc_flush: (%lx.%lx.%lx) busy\n", cncp->cp->c_fid.Volume, cncp->cp->c_fid.Vnode, cncp->cp->c_fid.Unique)); )
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

	for (i = 0; i < coda_nc_hashsize; i++)
	  coda_nc_hash[i].length = 0;
}

/*
 * Debugging routines
 */

/* 
 * This routine should print out all the hash chains to the console.
 */
void
print_coda_nc(void)
{
	int hash;
	struct coda_cache *cncp;

	for (hash = 0; hash < coda_nc_hashsize; hash++) {
		myprintf(("\nhash %d\n",hash));

		for (cncp = coda_nc_hash[hash].hash_next; 
		     cncp != (struct coda_cache *)&coda_nc_hash[hash];
		     cncp = cncp->hash_next) {
			myprintf(("cp %p dcp %p cred %p name %s\n",
				  cncp->cp, cncp->dcp,
				  cncp->cred, cncp->name));
		     }
	}
}

void
coda_nc_gather_stats(void)
{
    int i, max = 0, sum = 0, temp, zeros = 0, ave, n;

	for (i = 0; i < coda_nc_hashsize; i++) {
	  if (coda_nc_hash[i].length) {
	    sum += coda_nc_hash[i].length;
	  } else {
	    zeros++;
	  }

	  if (coda_nc_hash[i].length > max)
	    max = coda_nc_hash[i].length;
	}

	/*
	 * When computing the Arithmetic mean, only count slots which 
	 * are not empty in the distribution.
	 */
        coda_nc_stat.Sum_bucket_len = sum;
        coda_nc_stat.Num_zero_len = zeros;
        coda_nc_stat.Max_bucket_len = max;

	if ((n = coda_nc_hashsize - zeros) > 0) 
	  ave = sum / n;
	else
	  ave = 0;

	sum = 0;
	for (i = 0; i < coda_nc_hashsize; i++) {
	  if (coda_nc_hash[i].length) {
	    temp = coda_nc_hash[i].length - ave;
	    sum += temp * temp;
	  }
	}
        coda_nc_stat.Sum2_bucket_len = sum;
}

/*
 * The purpose of this routine is to allow the hash and cache sizes to be
 * changed dynamically. This should only be used in controlled environments,
 * it makes no effort to lock other users from accessing the cache while it
 * is in an improper state (except by turning the cache off).
 */
int
coda_nc_resize(hashsize, heapsize, dcstat)
     int hashsize, heapsize;
     enum dc_status dcstat;
{
    if ((hashsize % 2) || (heapsize % 2)) { /* Illegal hash or cache sizes */
	return(EINVAL);
    }                 
    
    coda_nc_use = 0;                       /* Turn the cache off */
    
    coda_nc_flush(dcstat);                 /* free any cnodes in the cache */
    
    /* WARNING: free must happen *before* size is reset */
    CODA_FREE(coda_nc_heap,TOTAL_CACHE_SIZE);
    CODA_FREE(coda_nc_hash,TOTAL_HASH_SIZE);
    
    coda_nc_hashsize = hashsize;
    coda_nc_size = heapsize;
    
    coda_nc_init();                        /* Set up a cache with the new size */
    
    coda_nc_use = 1;                       /* Turn the cache back on */
    return(0);
}

#define DEBUG
#ifdef	DEBUG
char coda_nc_name_buf[CODA_MAXNAMLEN+1];

void
coda_nc_name(struct cnode *cp)
{
	struct coda_cache *cncp, *ncncp;
	int i;

	if (coda_nc_use == 0)			/* Cache is off */
		return;

	for (i = 0; i < coda_nc_hashsize; i++) {
		for (cncp = coda_nc_hash[i].hash_next; 
		     cncp != (struct coda_cache *)&coda_nc_hash[i];
		     cncp = ncncp) {
			ncncp = cncp->hash_next;
			if (cncp->cp == cp) {
				bcopy(cncp->name, coda_nc_name_buf, cncp->namelen);
				coda_nc_name_buf[cncp->namelen] = 0;
				printf(" is %s (%p,%p)@%p",
					coda_nc_name_buf, cncp->cp, cncp->dcp, cncp);
			}

		}
	}
}
#endif
