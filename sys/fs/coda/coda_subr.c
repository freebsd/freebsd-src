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
 * 	@(#) src/sys/coda/coda_subr.c,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 *  $Id: coda_subr.c,v 1.4 1998/09/11 18:50:17 rvb Exp $
 * 
  */

/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */

/*
 * This code was written for the Coda file system at Carnegie Mellon
 * University.  Contributers include David Steere, James Kistler, and
 * M. Satyanarayanan.  */

/*
 * HISTORY
 * $Log: coda_subr.c,v $
 * Revision 1.4  1998/09/11 18:50:17  rvb
 * All the references to cfs, in symbols, structs, and strings
 * have been changed to coda.  (Same for CFS.)
 *
 * Revision 1.2  1998/09/02 19:09:53  rvb
 * Pass2 complete
 *
 * Revision 1.1.1.1  1998/08/29 21:14:52  rvb
 * Very Preliminary Coda
 *
 * Revision 1.11  1998/08/28 18:12:18  rvb
 * Now it also works on FreeBSD -current.  This code will be
 * committed to the FreeBSD -current and NetBSD -current
 * trees.  It will then be tailored to the particular platform
 * by flushing conditional code.
 *
 * Revision 1.10  1998/08/18 17:05:16  rvb
 * Don't use __RCSID now
 *
 * Revision 1.9  1998/08/18 16:31:41  rvb
 * Sync the code for NetBSD -current; test on 1.3 later
 *
 * Revision 1.8  98/01/31  20:53:12  rvb
 * First version that works on FreeBSD 2.2.5
 * 
 * Revision 1.7  98/01/23  11:53:42  rvb
 * Bring RVB_CODA1_1 to HEAD
 * 
 * Revision 1.6.2.3  98/01/23  11:21:05  rvb
 * Sync with 2.2.5
 * 
 * Revision 1.6.2.2  97/12/16  12:40:06  rvb
 * Sync with 1.3
 * 
 * Revision 1.6.2.1  97/12/06  17:41:21  rvb
 * Sync with peters coda.h
 * 
 * Revision 1.6  97/12/05  10:39:17  rvb
 * Read CHANGES
 * 
 * Revision 1.5.4.8  97/11/26  15:28:58  rvb
 * Cant make downcall pbuf == union cfs_downcalls yet
 * 
 * Revision 1.5.4.7  97/11/20  11:46:42  rvb
 * Capture current cfs_venus
 * 
 * Revision 1.5.4.6  97/11/18  10:27:16  rvb
 * cfs_nbsd.c is DEAD!!!; integrated into cfs_vf/vnops.c
 * cfs_nb_foo and cfs_foo are joined
 * 
 * Revision 1.5.4.5  97/11/13  22:03:00  rvb
 * pass2 cfs_NetBSD.h mt
 * 
 * Revision 1.5.4.4  97/11/12  12:09:39  rvb
 * reorg pass1
 * 
 * Revision 1.5.4.3  97/11/06  21:02:38  rvb
 * first pass at ^c ^z
 * 
 * Revision 1.5.4.2  97/10/29  16:06:27  rvb
 * Kill DYING
 * 
 * Revision 1.5.4.1  97/10/28 23:10:16  rvb
 * >64Meg; venus can be killed!
 *
 * Revision 1.5  97/08/05  11:08:17  lily
 * Removed cfsnc_replace, replaced it with a coda_find, unhash, and
 * rehash.  This fixes a cnode leak and a bug in which the fid is
 * not actually replaced.  (cfs_namecache.c, cfsnc.h, cfs_subr.c)
 * 
 * Revision 1.4  96/12/12  22:10:59  bnoble
 * Fixed the "downcall invokes venus operation" deadlock in all known cases. 
 * There may be more
 * 
 * Revision 1.3  1996/12/05 16:20:15  bnoble
 * Minor debugging aids
 *
 * Revision 1.2  1996/01/02 16:57:01  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:27  bnoble
 * Added CODA-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:07:59  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:07:58  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.8  1995/03/03  17:00:04  dcs
 * Fixed kernel bug involving sleep and upcalls. Basically if you killed
 * a job waiting on venus, the venus upcall queues got trashed. Depending
 * on luck, you could kill the kernel or not.
 * (mods to cfs_subr.c and cfs_mach.d)
 *
 * Revision 2.7  95/03/02  22:45:21  dcs
 * Sun4 compatibility
 * 
 * Revision 2.6  95/02/17  16:25:17  dcs
 * These versions represent several changes:
 * 1. Allow venus to restart even if outstanding references exist.
 * 2. Have only one ctlvp per client, as opposed to one per mounted cfs device.d
 * 3. Allow ody_expand to return many members, not just one.
 * 
 * Revision 2.5  94/11/09  15:56:26  dcs
 * Had the thread sleeping on the wrong thing!
 * 
 * Revision 2.4  94/10/14  09:57:57  dcs
 * Made changes 'cause sun4s have braindead compilers
 * 
 * Revision 2.3  94/10/12  16:46:26  dcs
 * Cleaned kernel/venus interface by removing XDR junk, plus
 * so cleanup to allow this code to be more easily ported.
 * 
 * Revision 1.2  92/10/27  17:58:22  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.4  92/09/30  14:16:26  mja
 * 	Incorporated Dave Steere's fix for the GNU-Emacs bug.
 * 	Also, included his coda_flush routine in place of the former coda_nc_flush.
 * 	[91/02/07            jjk]
 * 
 * 	Added contributors blurb.
 * 	[90/12/13            jjk]
 * 
 * 	Hack to allow users to keep coda venus calls uninterruptible. THis
 * 	basically prevents the Gnu-emacs bug from appearing, in which a call
 * 	was being interrupted, and return EINTR, but gnu didn't check for the
 * 	error and figured the file was buggered.
 * 	[90/12/09            dcs]
 * 
 * Revision 2.3  90/08/10  10:23:20  mrt
 * 	Removed include of vm/vm_page.h as it no longer exists.
 * 	[90/08/10            mrt]
 * 
 * Revision 2.2  90/07/05  11:26:35  mrt
 * 	Initialize name cache on first call to vcopen.
 * 	[90/05/23            dcs]
 * 
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.5  90/05/31  17:01:35  dcs
 * Prepare for merge with facilities kernel.
 * 
 * Revision 1.2  90/03/19  15:56:25  dcs
 * Initialize name cache on first call to vcopen.
 * 
 * Revision 1.1  90/03/15  10:43:26  jjk
 * Initial revision
 * 
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

#include <vcoda.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/select.h>
#include <sys/mount.h>

#include <coda/coda.h>
#include <coda/cnode.h>
#include <coda/coda_subr.h>
#include <coda/coda_namecache.h>

int coda_active = 0;
int coda_reuse = 0;
int coda_new = 0;

struct cnode *coda_freelist = NULL;
struct cnode *coda_cache[CODA_CACHESIZE];

#define coda_hash(fid) (((fid)->Volume + (fid)->Vnode) & (CODA_CACHESIZE-1))
#define	CNODE_NEXT(cp)	((cp)->c_next)
#define ODD(vnode)        ((vnode) & 0x1)

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
     ViceFid *fid;
{
    struct cnode *cp;

    cp = coda_cache[coda_hash(fid)];
    while (cp) {
	if ((cp->c_fid.Vnode == fid->Vnode) &&
	    (cp->c_fid.Volume == fid->Volume) &&
	    (cp->c_fid.Unique == fid->Unique) &&
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
					 myprintf(("Live cnode fid %lx.%lx.%lx flags %d count %d\n",
						   (cp->c_fid).Volume,
						   (cp->c_fid).Vnode,
						   (cp->c_fid).Unique, 
						   cp->c_flags,
						   CTOV(cp)->v_usecount)); );
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
	    if (!ODD(cp->c_fid.Vnode)) /* only files can be executed */
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
	    myprintf(("Live cnode fid %lx.%lx.%lx count %d\n",
		      (cp->c_fid).Volume,(cp->c_fid).Vnode,
		      (cp->c_fid).Unique, CTOV(cp)->v_usecount));
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
coda_checkunmounting(mp)
	struct mount *mp;
{	
	register struct vnode *vp, *nvp;
	struct cnode *cp;
	int count = 0, bad = 0;
loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp; vp = nvp) {
		if (vp->v_mount != mp)
			goto loop;
		nvp = vp->v_mntvnodes.le_next;
		cp = VTOC(vp);
		count++;
		if (!(cp->c_flags & C_UNMOUNTING)) {
			bad++;
			printf("vp %p, cp %p missed\n", vp, cp);
			cp->c_flags |= C_UNMOUNTING;
		}
	}
}

int
coda_cacheprint(whoIam)
	struct mount *whoIam;
{	
	int hash;
	struct cnode *cp;
	int count = 0;

	printf("coda_cacheprint: coda_ctlvp %p, cp %p", coda_ctlvp, VTOC(coda_ctlvp));
	coda_nc_name(coda_ctlvp);
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
 * CODA_REPLACE -- replace one ViceFid with another throughout the name cache 
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
	  coda_nc_purge_user(out->coda_purgeuser.cred.cr_uid, IS_DOWNCALL);
	  return(0);
      }
	
      case CODA_ZAPFILE : {
	  struct cnode *cp;

	  error = 0;
	  coda_clstat.ncalls++;
	  coda_clstat.reqs[CODA_ZAPFILE]++;
	  
	  cp = coda_find(&out->coda_zapfile.CodaFid);
	  if (cp != NULL) {
	      vref(CTOV(cp));
	      
	      cp->c_flags &= ~C_VATTR;
	      if (CTOV(cp)->v_flag & VTEXT)
		  error = coda_vmflush(cp);
	      CODADEBUG(CODA_ZAPFILE, myprintf(("zapfile: fid = (%lx.%lx.%lx), 
                                              refcnt = %d, error = %d\n",
					      cp->c_fid.Volume, 
					      cp->c_fid.Vnode, 
					      cp->c_fid.Unique, 
					      CTOV(cp)->v_usecount - 1, error)););
	      if (CTOV(cp)->v_usecount == 1) {
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
	  
	  cp = coda_find(&out->coda_zapdir.CodaFid);
	  if (cp != NULL) {
	      vref(CTOV(cp));
	      
	      cp->c_flags &= ~C_VATTR;
	      coda_nc_zapParentfid(&out->coda_zapdir.CodaFid, IS_DOWNCALL);     
	      
	      CODADEBUG(CODA_ZAPDIR, myprintf(("zapdir: fid = (%lx.%lx.%lx), 
                                          refcnt = %d\n",cp->c_fid.Volume, 
					     cp->c_fid.Vnode, 
					     cp->c_fid.Unique, 
					     CTOV(cp)->v_usecount - 1)););
	      if (CTOV(cp)->v_usecount == 1) {
		  cp->c_flags |= C_PURGING;
	      }
	      vrele(CTOV(cp));
	  }
	  
	  return(0);
      }
	
      case CODA_ZAPVNODE : {
	  coda_clstat.ncalls++;
	  coda_clstat.reqs[CODA_ZAPVNODE]++;
	  
	  myprintf(("CODA_ZAPVNODE: Called, but uniplemented\n"));
	  /*
	   * Not that below we must really translate the returned coda_cred to
	   * a netbsd cred.  This is a bit muddled at present and the cfsnc_zapnode
	   * is further unimplemented, so punt!
	   * I suppose we could use just the uid.
	   */
	  /* coda_nc_zapvnode(&out->coda_zapvnode.VFid, &out->coda_zapvnode.cred,
			 IS_DOWNCALL); */
	  return(0);
      }	
	
      case CODA_PURGEFID : {
	  struct cnode *cp;

	  error = 0;
	  coda_clstat.ncalls++;
	  coda_clstat.reqs[CODA_PURGEFID]++;

	  cp = coda_find(&out->coda_purgefid.CodaFid);
	  if (cp != NULL) {
	      vref(CTOV(cp));
	      if (ODD(out->coda_purgefid.CodaFid.Vnode)) { /* Vnode is a directory */
		  coda_nc_zapParentfid(&out->coda_purgefid.CodaFid,
				     IS_DOWNCALL);     
	      }
	      cp->c_flags &= ~C_VATTR;
	      coda_nc_zapfid(&out->coda_purgefid.CodaFid, IS_DOWNCALL);
	      if (!(ODD(out->coda_purgefid.CodaFid.Vnode)) 
		  && (CTOV(cp)->v_flag & VTEXT)) {
		  
		  error = coda_vmflush(cp);
	      }
	      CODADEBUG(CODA_PURGEFID, myprintf(("purgefid: fid = (%lx.%lx.%lx), refcnt = %d, error = %d\n",
                                            cp->c_fid.Volume, cp->c_fid.Vnode,
                                            cp->c_fid.Unique, 
					    CTOV(cp)->v_usecount - 1, error)););
	      if (CTOV(cp)->v_usecount == 1) {
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

	      CODADEBUG(CODA_REPLACE, myprintf(("replace: oldfid = (%lx.%lx.%lx), newfid = (%lx.%lx.%lx), cp = %p\n",
					   out->coda_replace.OldFid.Volume,
					   out->coda_replace.OldFid.Vnode,
					   out->coda_replace.OldFid.Unique,
					   cp->c_fid.Volume, cp->c_fid.Vnode, 
					   cp->c_fid.Unique, cp));)
	      vrele(CTOV(cp));
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
