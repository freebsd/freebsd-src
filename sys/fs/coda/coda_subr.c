/*-
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
 * 	@(#) src/sys/coda/coda_subr.c,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 */
/*-
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda filesystem at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.
 */

/* NOTES: rvb
 * 1.	Added coda_unmounting to mark all cnodes as being UNMOUNTING.  This has to
 *	 be done before dounmount is called.  Because some of the routines that
 *	 dounmount calls before coda_unmounted might try to force flushes to venus.
 *	 The vnode pager does this.
 * 2.	coda_unmounting marks all cnodes scanning coda_cache.
 * 3.	cfs_checkunmounting (under DEBUG) checks all cnodes by chasing the vnodes
 *	 under the /coda mount point.
 * 4.	coda_cacheprint (under DEBUG) prints names with vnode/cnode address
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mount.h>

#include <fs/coda/coda.h>
#include <fs/coda/cnode.h>
#include <fs/coda/coda_subr.h>
#include <fs/coda/coda_namecache.h>

int coda_active = 0;
int coda_reuse = 0;
int coda_new = 0;

struct cnode *coda_freelist = NULL;
struct cnode *coda_cache[CODA_CACHESIZE];

#define	CNODE_NEXT(cp)	((cp)->c_next)

#ifdef CODA_COMPAT_5
#define coda_hash(fid) \
    (((fid)->Volume + (fid)->Vnode) & (CODA_CACHESIZE-1))
#define IS_DIR(cnode)        (cnode.Vnode & 0x1)
#else
#define coda_hash(fid) (coda_f2i(fid) & (CODA_CACHESIZE-1))
#define IS_DIR(cnode)        (cnode.opaque[2] & 0x1)
#endif

/*
 * Allocate a cnode.
 */
struct cnode *
coda_alloc(void)
{
    struct cnode *cp;

    if (coda_freelist) {
	cp = coda_freelist;
	coda_freelist = CNODE_NEXT(cp);
	coda_reuse++;
    }
    else {
	CODA_ALLOC(cp, struct cnode *, sizeof(struct cnode));
	/* NetBSD vnodes don't have any Pager info in them ('cause there are
	   no external pagers, duh!) */
#define VNODE_VM_INFO_INIT(vp)         /* MT */
	VNODE_VM_INFO_INIT(CTOV(cp));
	coda_new++;
    }
    bzero(cp, sizeof (struct cnode));

    return(cp);
}

/*
 * Deallocate a cnode.
 */
void
coda_free(cp)
     register struct cnode *cp;
{

    CNODE_NEXT(cp) = coda_freelist;
    coda_freelist = cp;
}

/*
 * Put a cnode in the hash table
 */
void
coda_save(cp)
     struct cnode *cp;
{
	CNODE_NEXT(cp) = coda_cache[coda_hash(&cp->c_fid)];
	coda_cache[coda_hash(&cp->c_fid)] = cp;
}

/*
 * Remove a cnode from the hash table
 */
void
coda_unsave(cp)
     struct cnode *cp;
{
    struct cnode *ptr;
    struct cnode *ptrprev = NULL;
    
    ptr = coda_cache[coda_hash(&cp->c_fid)]; 
    while (ptr != NULL) { 
	if (ptr == cp) { 
	    if (ptrprev == NULL) {
		coda_cache[coda_hash(&cp->c_fid)] 
		    = CNODE_NEXT(ptr);
	    } else {
		CNODE_NEXT(ptrprev) = CNODE_NEXT(ptr);
	    }
	    CNODE_NEXT(cp) = (struct cnode *)NULL;
	    
	    return; 
	}	
	ptrprev = ptr;
	ptr = CNODE_NEXT(ptr);
    }	
}

/*
 * Lookup a cnode by fid. If the cnode is dying, it is bogus so skip it.
 * NOTE: this allows multiple cnodes with same fid -- dcs 1/25/95
 */
struct cnode *
coda_find(fid) 
     CodaFid *fid;
{
    struct cnode *cp;

    cp = coda_cache[coda_hash(fid)];
    while (cp) {
	    if (coda_fid_eq(&(cp->c_fid), fid) &&
	    (!IS_UNMOUNTING(cp)))
	    {
		coda_active++;
		return(cp); 
	    }		    
	cp = CNODE_NEXT(cp);
    }
    return(NULL);
}

/*
 * coda_kill is called as a side effect to vcopen. To prevent any
 * cnodes left around from an earlier run of a venus or warden from
 * causing problems with the new instance, mark any outstanding cnodes
 * as dying. Future operations on these cnodes should fail (excepting
 * coda_inactive of course!). Since multiple venii/wardens can be
 * running, only kill the cnodes for a particular entry in the
 * coda_mnttbl. -- DCS 12/1/94 */

int
coda_kill(whoIam, dcstat)
	struct mount *whoIam;
	enum dc_status dcstat;
{
	int hash, count = 0;
	struct cnode *cp;
	
	/* 
	 * Algorithm is as follows: 
	 *     Second, flush whatever vnodes we can from the name cache.
	 * 
	 *     Finally, step through whatever is left and mark them dying.
	 *        This prevents any operation at all.
	 */
	
	/* This is slightly overkill, but should work. Eventually it'd be
	 * nice to only flush those entries from the namecache that
	 * reference a vnode in this vfs.  */
	coda_nc_flush(dcstat);
	
	for (hash = 0; hash < CODA_CACHESIZE; hash++) {
		for (cp = coda_cache[hash]; cp != NULL; cp = CNODE_NEXT(cp)) {
			if (CTOV(cp)->v_mount == whoIam) {
#ifdef	DEBUG
				printf("coda_kill: vp %p, cp %p\n", CTOV(cp), cp);
#endif
				count++;
				CODADEBUG(CODA_FLUSH, 
					  myprintf(("Live cnode fid %s flags %d count %d\n",
						    coda_f2s(&cp->c_fid),
						    cp->c_flags,
						    vrefcnt(CTOV(cp)))); );
			}
		}
	}
	return count;
}

/*
 * There are two reasons why a cnode may be in use, it may be in the
 * name cache or it may be executing.  
 */
void
coda_flush(dcstat)
	enum dc_status dcstat;
{
    int hash;
    struct cnode *cp;
    
    coda_clstat.ncalls++;
    coda_clstat.reqs[CODA_FLUSH]++;
    
    coda_nc_flush(dcstat);	    /* flush files from the name cache */

    for (hash = 0; hash < CODA_CACHESIZE; hash++) {
	for (cp = coda_cache[hash]; cp != NULL; cp = CNODE_NEXT(cp)) {  
	    if (!IS_DIR(cp->c_fid)) /* only files can be executed */
		coda_vmflush(cp);
	}
    }
}

/*
 * As a debugging measure, print out any cnodes that lived through a
 * name cache flush.  
 */
void
coda_testflush(void)
{
    int hash;
    struct cnode *cp;
    
    for (hash = 0; hash < CODA_CACHESIZE; hash++) {
	for (cp = coda_cache[hash];
	     cp != NULL;
	     cp = CNODE_NEXT(cp)) {  
	    myprintf(("Live cnode fid %s count %d\n",
		      coda_f2s(&cp->c_fid), CTOV(cp)->v_usecount));
	}
    }
}

/*
 *     First, step through all cnodes and mark them unmounting.
 *         NetBSD kernels may try to fsync them now that venus
 *         is dead, which would be a bad thing.
 *
 */
void
coda_unmounting(whoIam)
	struct mount *whoIam;
{	
	int hash;
	struct cnode *cp;

	for (hash = 0; hash < CODA_CACHESIZE; hash++) {
		for (cp = coda_cache[hash]; cp != NULL; cp = CNODE_NEXT(cp)) {
			if (CTOV(cp)->v_mount == whoIam) {
				if (cp->c_flags & (C_LOCKED|C_WANTED)) {
					printf("coda_unmounting: Unlocking %p\n", cp);
					cp->c_flags &= ~(C_LOCKED|C_WANTED);
					wakeup((caddr_t) cp);
				}
				cp->c_flags |= C_UNMOUNTING;
			}
		}
	}
}

#ifdef	DEBUG
void
coda_checkunmounting(mp)
	struct mount *mp;
{
	struct vnode *vp, *nvp;
	struct cnode *cp;
	int count = 0, bad = 0;

	MNT_ILOCK(mp);
	MNT_VNODE_FOREACH(vp, mp, nvp) {
		VI_LOCK(vp);
		if (vp->v_iflag & VI_DOOMED) {
			VI_UNLOCK(vp);
			continue;
		}
		cp = VTOC(vp);
		count++;
		if (!(cp->c_flags & C_UNMOUNTING)) {
			bad++;
			printf("vp %p, cp %p missed\n", vp, cp);
			cp->c_flags |= C_UNMOUNTING;
		}
		VI_UNLOCK(vp);
	}
	MNT_IUNLOCK(mp);
}

void
coda_cacheprint(whoIam)
	struct mount *whoIam;
{	
	int hash;
	struct cnode *cp;
	int count = 0;

	printf("coda_cacheprint: coda_ctlvp %p, cp %p", coda_ctlvp, VTOC(coda_ctlvp));
	coda_nc_name(VTOC(coda_ctlvp));
	printf("\n");

	for (hash = 0; hash < CODA_CACHESIZE; hash++) {
		for (cp = coda_cache[hash]; cp != NULL; cp = CNODE_NEXT(cp)) {
			if (CTOV(cp)->v_mount == whoIam) {
				printf("coda_cacheprint: vp %p, cp %p", CTOV(cp), cp);
				coda_nc_name(cp);
				printf("\n");
				count++;
			}
		}
	}
	printf("coda_cacheprint: count %d\n", count);
}
#endif

/*
 * There are 6 cases where invalidations occur. The semantics of each
 * is listed here.
 *
 * CODA_FLUSH     -- flush all entries from the name cache and the cnode cache.
 * CODA_PURGEUSER -- flush all entries from the name cache for a specific user
 *                  This call is a result of token expiration.
 *
 * The next two are the result of callbacks on a file or directory.
 * CODA_ZAPDIR    -- flush the attributes for the dir from its cnode.
 *                  Zap all children of this directory from the namecache.
 * CODA_ZAPFILE   -- flush the attributes for a file.
 *
 * The fifth is a result of Venus detecting an inconsistent file.
 * CODA_PURGEFID  -- flush the attribute for the file
 *                  If it is a dir (odd vnode), purge its 
 *                  children from the namecache
 *                  remove the file from the namecache.
 *
 * The sixth allows Venus to replace local fids with global ones
 * during reintegration.
 *
 * CODA_REPLACE -- replace one CodaFid with another throughout the name cache 
 */

int handleDownCall(opcode, out)
     int opcode; union outputArgs *out;
{
    int error;

    /* Handle invalidate requests. */
    switch (opcode) {
      case CODA_FLUSH : {

	  coda_flush(IS_DOWNCALL);
	  
	  CODADEBUG(CODA_FLUSH,coda_testflush();)    /* print remaining cnodes */
	      return(0);
      }
	
      case CODA_PURGEUSER : {
	  coda_clstat.ncalls++;
	  coda_clstat.reqs[CODA_PURGEUSER]++;
	  
	  /* XXX - need to prevent fsync's */
#ifdef CODA_COMPAT_5
	  coda_nc_purge_user(out->coda_purgeuser.cred.cr_uid, IS_DOWNCALL);
#else
	  coda_nc_purge_user(out->coda_purgeuser.uid, IS_DOWNCALL);
#endif
	  return(0);
      }
	
      case CODA_ZAPFILE : {
	  struct cnode *cp;

	  error = 0;
	  coda_clstat.ncalls++;
	  coda_clstat.reqs[CODA_ZAPFILE]++;
	  
	  cp = coda_find(&out->coda_zapfile.Fid);
	  if (cp != NULL) {
	      vref(CTOV(cp));
	      
	      cp->c_flags &= ~C_VATTR;
	      ASSERT_VOP_LOCKED(CTOV(cp), "coda HandleDownCall");
	      if (CTOV(cp)->v_vflag & VV_TEXT)
		  error = coda_vmflush(cp);
	      CODADEBUG(CODA_ZAPFILE, 
			myprintf(("zapfile: fid = %s, refcnt = %d, error = %d\n",
				  coda_f2s(&cp->c_fid), CTOV(cp)->v_usecount - 1, error)););
    if (vrefcnt(CTOV(cp)) == 1) {
		  cp->c_flags |= C_PURGING;
	      }
	      vrele(CTOV(cp));
	  }
	  
	  return(error);
      }
	
      case CODA_ZAPDIR : {
	  struct cnode *cp;

	  coda_clstat.ncalls++;
	  coda_clstat.reqs[CODA_ZAPDIR]++;
	  
	  cp = coda_find(&out->coda_zapdir.Fid);
	  if (cp != NULL) {
	      vref(CTOV(cp));
	      
	      cp->c_flags &= ~C_VATTR;
	      coda_nc_zapParentfid(&out->coda_zapdir.Fid, IS_DOWNCALL);

	      CODADEBUG(CODA_ZAPDIR, myprintf((
		  "zapdir: fid = %s, refcnt = %d\n",
		  coda_f2s(&cp->c_fid), CTOV(cp)->v_usecount - 1)););
	      if (vrefcnt(CTOV(cp)) == 1) {
		  cp->c_flags |= C_PURGING;
	      }
	      vrele(CTOV(cp));
	  }
	  
	  return(0);
      }
	
      case CODA_PURGEFID : {
	  struct cnode *cp;

	  error = 0;
	  coda_clstat.ncalls++;
	  coda_clstat.reqs[CODA_PURGEFID]++;

	  cp = coda_find(&out->coda_purgefid.Fid);
	  if (cp != NULL) {
	      vref(CTOV(cp));
	      if (IS_DIR(out->coda_purgefid.Fid)) { /* Vnode is a directory */
		      coda_nc_zapParentfid(&out->coda_purgefid.Fid,IS_DOWNCALL);     
	      }
	      cp->c_flags &= ~C_VATTR;
	      coda_nc_zapfid(&out->coda_purgefid.Fid, IS_DOWNCALL);
	      ASSERT_VOP_LOCKED(CTOV(cp), "coda HandleDownCall");
	      if (!(IS_DIR(out->coda_purgefid.Fid)) 
		  && (CTOV(cp)->v_vflag & VV_TEXT)) {
		  
		  error = coda_vmflush(cp);
	      }
	      CODADEBUG(CODA_PURGEFID, myprintf((
			 "purgefid: fid = %s, refcnt = %d, error = %d\n",
			 coda_f2s(&cp->c_fid), CTOV(cp)->v_usecount - 1, error)););
	      if (vrefcnt(CTOV(cp)) == 1) {
		  cp->c_flags |= C_PURGING;
	      }
	      vrele(CTOV(cp));
	  }
	  return(error);
      }

      case CODA_REPLACE : {
	  struct cnode *cp = NULL;

	  coda_clstat.ncalls++;
	  coda_clstat.reqs[CODA_REPLACE]++;
	  
	  cp = coda_find(&out->coda_replace.OldFid);
	  if (cp != NULL) { 
	      /* remove the cnode from the hash table, replace the fid, and reinsert */
	      vref(CTOV(cp));
	      coda_unsave(cp);
	      cp->c_fid = out->coda_replace.NewFid;
	      coda_save(cp);

	      CODADEBUG(CODA_REPLACE, myprintf((
			"replace: oldfid = %s, newfid = %s, cp = %p\n",
			coda_f2s(&out->coda_replace.OldFid),
			coda_f2s(&cp->c_fid), cp));)	      vrele(CTOV(cp));
	  }
	  return (0);
      }
      default:
      	myprintf(("handleDownCall: unknown opcode %d\n", opcode));
	return (EINVAL);
    }
}

/* coda_grab_vnode: lives in either cfs_mach.c or cfs_nbsd.c */

int
coda_vmflush(cp)
     struct cnode *cp;
{
    return 0;
}


/* 
 * kernel-internal debugging switches
 */
void coda_debugon(void)
{
    codadebug = -1;
    coda_nc_debug = -1;
    coda_vnop_print_entry = 1;
    coda_psdev_print_entry = 1;
    coda_vfsop_print_entry = 1;
}

void coda_debugoff(void)
{
    codadebug = 0;
    coda_nc_debug = 0;
    coda_vnop_print_entry = 0;
    coda_psdev_print_entry = 0;
    coda_vfsop_print_entry = 0;
}

/*
 * Utilities used by both client and server
 * Standard levels:
 * 0) no debugging
 * 1) hard failures
 * 2) soft failures
 * 3) current test software
 * 4) main procedure entry points
 * 5) main procedure exit points
 * 6) utility procedure entry points
 * 7) utility procedure exit points
 * 8) obscure procedure entry points
 * 9) obscure procedure exit points
 * 10) random stuff
 * 11) all <= 1
 * 12) all <= 2
 * 13) all <= 3
 * ...
 */
