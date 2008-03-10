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
 *  	@(#) src/sys/cfs/coda_vfsops.c,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>

#include <fs/coda/coda.h>
#include <fs/coda/cnode.h>
#include <fs/coda/coda_vfsops.h>
#include <fs/coda/coda_venus.h>
#include <fs/coda/coda_subr.h>
#include <fs/coda/coda_opstats.h>

MALLOC_DEFINE(M_CODA, "coda", "Various Coda Structures");

int codadebug = 0;
int coda_vfsop_print_entry = 0;
#define ENTRY    if(coda_vfsop_print_entry) myprintf(("Entered %s\n",__func__))

struct vnode *coda_ctlvp;

/* structure to keep statistics of internally generated/satisfied calls */

struct coda_op_stats coda_vfsopstats[CODA_VFSOPS_SIZE];

#define MARK_ENTRY(op) (coda_vfsopstats[op].entries++)
#define MARK_INT_SAT(op) (coda_vfsopstats[op].sat_intrn++)
#define MARK_INT_FAIL(op) (coda_vfsopstats[op].unsat_intrn++)
#define MARK_INT_GEN(op) (coda_vfsopstats[op].gen_intrn++)

int
coda_vfsopstats_init(void)
{
	register int i;
	
	for (i=0;i<CODA_VFSOPS_SIZE;i++) {
		coda_vfsopstats[i].opcode = i;
		coda_vfsopstats[i].entries = 0;
		coda_vfsopstats[i].sat_intrn = 0;
		coda_vfsopstats[i].unsat_intrn = 0;
		coda_vfsopstats[i].gen_intrn = 0;
	}
	
	return 0;
}

static const char *coda_opts[] = { "from", NULL };
/*
 * cfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
/*ARGSUSED*/
int
coda_mount(struct mount *vfsp, struct thread *td)
{
    struct vnode *dvp;
    struct cnode *cp;
    struct cdev *dev;
    struct coda_mntinfo *mi;
    struct vnode *rootvp;
    CodaFid rootfid = INVAL_FID;
    CodaFid ctlfid = CTL_FID;
    int error;
    struct nameidata ndp;
    ENTRY;
    char *from;

    if (vfs_filteropt(vfsp->mnt_optnew, coda_opts))
	return (EINVAL);

    from = vfs_getopts(vfsp->mnt_optnew, "from", &error);
    if (error)
	return (error);

    coda_vfsopstats_init();
    coda_vnodeopstats_init();
    
    MARK_ENTRY(CODA_MOUNT_STATS);
    if (CODA_MOUNTED(vfsp)) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	return(EBUSY);
    }
    
    /* Validate mount device.  Similar to getmdev(). */
    NDINIT(&ndp, LOOKUP, FOLLOW, UIO_SYSSPACE, from, td);
    error = namei(&ndp);
    dvp = ndp.ni_vp;

    if (error) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	return (error);
    }
    if (dvp->v_type != VCHR) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	vrele(dvp);
	NDFREE(&ndp, NDF_ONLY_PNBUF);
	return(ENXIO);
    }
    dev = dvp->v_rdev;
    vrele(dvp);
    NDFREE(&ndp, NDF_ONLY_PNBUF);

    /*
     * Initialize the mount record and link it to the vfs struct
     */
    mi = dev2coda_mntinfo(dev);
    if (!mi) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	printf("Coda mount: %s is not a cfs device\n", from);
	return(ENXIO);
    }
    
    if (!VC_OPEN(&mi->mi_vcomm)) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	return(ENODEV);
    }
    
    /* No initialization (here) of mi_vcomm! */
    vfsp->mnt_data = mi;
    vfs_getnewfsid (vfsp);

    mi->mi_vfsp = vfsp;
    mi->mi_started = 0;			/* XXX See coda_root() */
    
    /*
     * Make a root vnode to placate the Vnode interface, but don't
     * actually make the CODA_ROOT call to venus until the first call
     * to coda_root in case a server is down while venus is starting.
     */
    cp = make_coda_node(&rootfid, vfsp, VDIR);
    rootvp = CTOV(cp);
    rootvp->v_vflag |= VV_ROOT;
	
    cp = make_coda_node(&ctlfid, vfsp, VREG);
    coda_ctlvp = CTOV(cp);

    /* Add vfs and rootvp to chain of vfs hanging off mntinfo */
    mi->mi_vfsp = vfsp;
    mi->mi_rootvp = rootvp;
    
    vfs_mountedfrom(vfsp, from);
    /* error is currently guaranteed to be zero, but in case some
       code changes... */
    CODADEBUG(1,
	     myprintf(("coda_omount returned %d\n",error)););
    if (error)
	MARK_INT_FAIL(CODA_MOUNT_STATS);
    else
	MARK_INT_SAT(CODA_MOUNT_STATS);
    
    return(error);
}

int
coda_unmount(vfsp, mntflags, td)
    struct mount *vfsp;
    int mntflags;
    struct thread *td;
{
    struct coda_mntinfo *mi = vftomi(vfsp);
    int active, error = 0;
    
    ENTRY;
    MARK_ENTRY(CODA_UMOUNT_STATS);
    if (!CODA_MOUNTED(vfsp)) {
	MARK_INT_FAIL(CODA_UMOUNT_STATS);
	return(EINVAL);
    }
    
    if (mi->mi_vfsp == vfsp) {	/* We found the victim */
	if (!IS_UNMOUNTING(VTOC(mi->mi_rootvp)))
	    return (EBUSY); 	/* Venus is still running */

#ifdef	DEBUG
	printf("coda_unmount: ROOT: vp %p, cp %p\n", mi->mi_rootvp, VTOC(mi->mi_rootvp));
#endif
	vrele(mi->mi_rootvp);
	mi->mi_rootvp = NULL;
	vrele(coda_ctlvp);
	coda_ctlvp = NULL;
	active = coda_kill(vfsp, NOT_DOWNCALL);
	error = vflush(mi->mi_vfsp, 0, FORCECLOSE, td);
#ifdef CODA_VERBOSE
	printf("coda_unmount: active = %d, vflush active %d\n", active, error);
#endif
	error = 0;
	/* I'm going to take this out to allow lookups to go through. I'm
	 * not sure it's important anyway. -- DCS 2/2/94
	 */
	/* vfsp->VFS_DATA = NULL; */

	/* No more vfsp's to hold onto */
	mi->mi_vfsp = NULL;

	if (error)
	    MARK_INT_FAIL(CODA_UMOUNT_STATS);
	else
	    MARK_INT_SAT(CODA_UMOUNT_STATS);

	return(error);
    }
    return (EINVAL);
}

/*
 * find root of cfs
 */
int
coda_root(vfsp, flags, vpp, td)
	struct mount *vfsp;
	int flags;
	struct vnode **vpp;
	struct thread *td;
{
    struct coda_mntinfo *mi = vftomi(vfsp);
    struct vnode **result;
    int error;
    struct proc *p = td->td_proc;
    CodaFid VFid;
    static const CodaFid invalfid = INVAL_FID;
 
    ENTRY;
    MARK_ENTRY(CODA_ROOT_STATS);
    result = NULL;
    
    if (vfsp == mi->mi_vfsp) {
	/*
	 * Cache the root across calls. We only need to pass the request
	 * on to Venus if the root vnode is the dummy we installed in
	 * coda_omount() with all c_fid members zeroed.
	 *
	 * XXX In addition, we assume that the first call to coda_root()
	 * is from vfs_omount()
	 * (before the call to checkdirs()) and return the dummy root
	 * node to avoid a deadlock. This bug is fixed in the Coda CVS
	 * repository but not in any released versions as of 6 Mar 2003.
	 */
	if (memcmp(&VTOC(mi->mi_rootvp)->c_fid, &invalfid,
	    sizeof(CodaFid)) != 0 || mi->mi_started == 0)
	    { /* Found valid root. */
		*vpp = mi->mi_rootvp;
		mi->mi_started = 1;

		/* On Mach, this is vref.  On FreeBSD, vref + vn_lock. */
		vref(*vpp);
		vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY, td);
		MARK_INT_SAT(CODA_ROOT_STATS);
		return(0);
	    }
    }

    error = venus_root(vftomi(vfsp), td->td_ucred, p, &VFid);

    if (!error) {
	/*
	 * Save the new rootfid in the cnode, and rehash the cnode into the
	 * cnode hash with the new fid key.
	 */
	coda_unsave(VTOC(mi->mi_rootvp));
	VTOC(mi->mi_rootvp)->c_fid = VFid;
	coda_save(VTOC(mi->mi_rootvp));

	*vpp = mi->mi_rootvp;
	vref(*vpp);
	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY, td);

	MARK_INT_SAT(CODA_ROOT_STATS);
	goto exit;
    } else if (error == ENODEV || error == EINTR) {
	/* Gross hack here! */
	/*
	 * If Venus fails to respond to the CODA_ROOT call, coda_call returns
	 * ENODEV. Return the uninitialized root vnode to allow vfs
	 * operations such as unmount to continue. Without this hack,
	 * there is no way to do an unmount if Venus dies before a 
	 * successful CODA_ROOT call is done. All vnode operations 
	 * will fail.
	 */
	*vpp = mi->mi_rootvp;
	vref(*vpp);
	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY, td);

	MARK_INT_FAIL(CODA_ROOT_STATS);
	error = 0;
	goto exit;
    } else {
	CODADEBUG( CODA_ROOT, myprintf(("error %d in CODA_ROOT\n", error)); );
	MARK_INT_FAIL(CODA_ROOT_STATS);
		
	goto exit;
    }

 exit:
    return(error);
}

/*
 * Get filesystem statistics.
 */
int
coda_statfs(vfsp, sbp, td)
    register struct mount *vfsp;
    struct statfs *sbp;
    struct thread *td;
{
    ENTRY;
    MARK_ENTRY(CODA_STATFS_STATS);
    if (!CODA_MOUNTED(vfsp)) {
	MARK_INT_FAIL(CODA_STATFS_STATS);
	return(EINVAL);
    }
    
    /* XXX - what to do about f_flags, others? --bnoble */
    /* Below This is what AFS does
    	#define CODA_SFS_SIZ 0x895440
     */
    sbp->f_flags = 0;
    sbp->f_bsize = 8192; /* XXX */
    sbp->f_iosize = 8192; /* XXX */
#define CODA_SFS_SIZ 0x8AB75D
    sbp->f_blocks = CODA_SFS_SIZ;
    sbp->f_bfree = CODA_SFS_SIZ;
    sbp->f_bavail = CODA_SFS_SIZ;
    sbp->f_files = CODA_SFS_SIZ;
    sbp->f_ffree = CODA_SFS_SIZ;
    MARK_INT_SAT(CODA_STATFS_STATS);
    return(0);
}

/*
 * Flush any pending I/O.
 */
int
coda_sync(vfsp, waitfor, td)
    struct mount *vfsp;
    int    waitfor;
    struct thread *td;
{
    ENTRY;
    MARK_ENTRY(CODA_SYNC_STATS);
    MARK_INT_SAT(CODA_SYNC_STATS);
    return(0);
}

/* 
 * fhtovp is now what vget used to be in 4.3-derived systems.  For
 * some silly reason, vget is now keyed by a 32 bit ino_t, rather than
 * a type-specific fid.  
 *
 * XXX: coda_fhtovp is currently not hooked up, so no NFS export for Coda.
 * We leave it here in the hopes that someone will find it someday and hook
 * it up.  Among other things, it will need some reworking to match the
 * vfs_fhtovp_t prototype.
 */
int
coda_fhtovp(vfsp, fhp, nam, vpp, exflagsp, creadanonp)
    register struct mount *vfsp;    
    struct fid *fhp;
    struct mbuf *nam;
    struct vnode **vpp;
    int *exflagsp;
    struct ucred **creadanonp;
{
    struct cfid *cfid = (struct cfid *)fhp;
    struct cnode *cp = 0;
    int error;
    struct thread *td = curthread; /* XXX -mach */
    struct proc *p = td->td_proc;
    CodaFid VFid;
    int vtype;

    ENTRY;
    
    MARK_ENTRY(CODA_VGET_STATS);
    /* Check for vget of control object. */
    if (IS_CTL_FID(&cfid->cfid_fid)) {
	*vpp = coda_ctlvp;
	vref(coda_ctlvp);
	MARK_INT_SAT(CODA_VGET_STATS);
	return(0);
    }
    
    error = venus_fhtovp(vftomi(vfsp), &cfid->cfid_fid, td->td_ucred, p, &VFid, &vtype);
    
    if (error) {
	CODADEBUG(CODA_VGET, myprintf(("vget error %d\n",error));)
	    *vpp = (struct vnode *)0;
    } else {
	CODADEBUG(CODA_VGET, 
		 myprintf(("vget: %s type %d result %d\n",
			coda_f2s(&VFid), vtype, error)); )	    
	cp = make_coda_node(&VFid, vfsp, vtype);
	*vpp = CTOV(cp);
    }
    return(error);
}

struct vfsops coda_vfsops = {
    .vfs_mount =		coda_mount,
    .vfs_root = 		coda_root,
    .vfs_statfs =		coda_statfs,
    .vfs_sync = 		coda_sync,
    .vfs_unmount =		coda_unmount,
};

VFS_SET(coda_vfsops, coda, VFCF_NETWORK);
