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
 *  	@(#) src/sys/cfs/coda_vfsops.c,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 * $FreeBSD$
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
 * M. Satyanarayanan.  
 */

#include <vcoda.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/namei.h>
#include <net/radix.h>
#include <sys/socket.h>
#include <sys/mount.h>

#include <coda/coda.h>
#include <coda/cnode.h>
#include <coda/coda_vfsops.h>
#include <coda/coda_venus.h>
#include <coda/coda_subr.h>
#include <coda/coda_opstats.h>

MALLOC_DEFINE(M_CODA, "CODA storage", "Various Coda Structures");

int codadebug = 0;
int coda_vfsop_print_entry = 0;
#define ENTRY    if(coda_vfsop_print_entry) myprintf(("Entered %s\n",__FUNCTION__))

struct vnode *coda_ctlvp;
struct coda_mntinfo coda_mnttbl[NVCODA]; /* indexed by minor device number */

/* structure to keep statistics of internally generated/satisfied calls */

struct coda_op_stats coda_vfsopstats[CODA_VFSOPS_SIZE];

#define MARK_ENTRY(op) (coda_vfsopstats[op].entries++)
#define MARK_INT_SAT(op) (coda_vfsopstats[op].sat_intrn++)
#define MARK_INT_FAIL(op) (coda_vfsopstats[op].unsat_intrn++)
#define MRAK_INT_GEN(op) (coda_vfsopstats[op].gen_intrn++)

extern int coda_nc_initialized;     /* Set if cache has been initialized */
extern int vc_nb_open __P((dev_t, int, int, struct proc *));

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

/*
 * cfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
/*ARGSUSED*/
int
coda_mount(vfsp, path, data, ndp, p)
    struct mount *vfsp;		/* Allocated and initialized by mount(2) */
    char *path;			/* path covered: ignored by the fs-layer */
    caddr_t data;		/* Need to define a data type for this in netbsd? */
    struct nameidata *ndp;	/* Clobber this to lookup the device name */
    struct proc *p;		/* The ever-famous proc pointer */
{
    struct vnode *dvp;
    struct cnode *cp;
    dev_t dev;
    struct coda_mntinfo *mi;
    struct vnode *rootvp;
    ViceFid rootfid;
    ViceFid ctlfid;
    int error;

    ENTRY;

    coda_vfsopstats_init();
    coda_vnodeopstats_init();
    
    MARK_ENTRY(CODA_MOUNT_STATS);
    if (CODA_MOUNTED(vfsp)) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	return(EBUSY);
    }
    
    /* Validate mount device.  Similar to getmdev(). */
    NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, data, p);
    error = namei(ndp);
    dvp = ndp->ni_vp;

    if (error) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	return (error);
    }
    if (dvp->v_type != VCHR) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	vrele(dvp);
	NDFREE(ndp, NDF_ONLY_PNBUF);
	return(ENXIO);
    }
    dev = dvp->v_rdev;
    vrele(dvp);
    NDFREE(ndp, NDF_ONLY_PNBUF);

    /*
     * See if the device table matches our expectations.
     */
    if (devsw(dev)->d_open != vc_nb_open)
    {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	return(ENXIO);
    }
    
    if (minor(dev) >= NVCODA || minor(dev) < 0) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	return(ENXIO);
    }
    
    /*
     * Initialize the mount record and link it to the vfs struct
     */
    mi = &coda_mnttbl[minor(dev)];
    
    if (!VC_OPEN(&mi->mi_vcomm)) {
	MARK_INT_FAIL(CODA_MOUNT_STATS);
	return(ENODEV);
    }
    
    /* No initialization (here) of mi_vcomm! */
    vfsp->mnt_data = (qaddr_t)mi;
    vfs_getnewfsid (vfsp);

    mi->mi_vfsp = vfsp;
    
    /*
     * Make a root vnode to placate the Vnode interface, but don't
     * actually make the CODA_ROOT call to venus until the first call
     * to coda_root in case a server is down while venus is starting.
     */
    rootfid.Volume = 0;
    rootfid.Vnode = 0;
    rootfid.Unique = 0;
    cp = make_coda_node(&rootfid, vfsp, VDIR);
    rootvp = CTOV(cp);
    rootvp->v_flag |= VROOT;
	
    ctlfid.Volume = CTL_VOL;
    ctlfid.Vnode = CTL_VNO;
    ctlfid.Unique = CTL_UNI;
/*  cp = make_coda_node(&ctlfid, vfsp, VCHR);
    The above code seems to cause a loop in the cnode links.
    I don't totally understand when it happens, it is caught
    when closing down the system.
 */
    cp = make_coda_node(&ctlfid, 0, VCHR);

    coda_ctlvp = CTOV(cp);

    /* Add vfs and rootvp to chain of vfs hanging off mntinfo */
    mi->mi_vfsp = vfsp;
    mi->mi_rootvp = rootvp;
    
    /* set filesystem block size */
    vfsp->mnt_stat.f_bsize = 8192;	    /* XXX -JJK */

    /* Set f_iosize.  XXX -- inamura@isl.ntt.co.jp. 
       For vnode_pager_haspage() references. The value should be obtained 
       from underlying UFS. */
    /* Checked UFS. iosize is set as 8192 */
    vfsp->mnt_stat.f_iosize = 8192;

    /* error is currently guaranteed to be zero, but in case some
       code changes... */
    CODADEBUG(1,
	     myprintf(("coda_mount returned %d\n",error)););
    if (error)
	MARK_INT_FAIL(CODA_MOUNT_STATS);
    else
	MARK_INT_SAT(CODA_MOUNT_STATS);
    
    return(error);
}

int
coda_unmount(vfsp, mntflags, p)
    struct mount *vfsp;
    int mntflags;
    struct proc *p;
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

	active = coda_kill(vfsp, NOT_DOWNCALL);
	mi->mi_rootvp->v_flag &= ~VROOT;
	error = vflush(mi->mi_vfsp, NULLVP, FORCECLOSE);
	printf("coda_unmount: active = %d, vflush active %d\n", active, error);
	error = 0;
	/* I'm going to take this out to allow lookups to go through. I'm
	 * not sure it's important anyway. -- DCS 2/2/94
	 */
	/* vfsp->VFS_DATA = NULL; */

	/* No more vfsp's to hold onto */
	mi->mi_vfsp = NULL;
	mi->mi_rootvp = NULL;

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
coda_root(vfsp, vpp)
	struct mount *vfsp;
	struct vnode **vpp;
{
    struct coda_mntinfo *mi = vftomi(vfsp);
    struct vnode **result;
    int error;
    struct proc *p = curproc;    /* XXX - bnoble */
    ViceFid VFid;

    ENTRY;
    MARK_ENTRY(CODA_ROOT_STATS);
    result = NULL;
    
    if (vfsp == mi->mi_vfsp) {
	if ((VTOC(mi->mi_rootvp)->c_fid.Volume != 0) ||
	    (VTOC(mi->mi_rootvp)->c_fid.Vnode != 0) ||
	    (VTOC(mi->mi_rootvp)->c_fid.Unique != 0))
	    { /* Found valid root. */
		*vpp = mi->mi_rootvp;
		/* On Mach, this is vref.  On NetBSD, VOP_LOCK */
#if	1
		vref(*vpp);
		vn_lock(*vpp, LK_EXCLUSIVE, p);
#else
		vget(*vpp, LK_EXCLUSIVE, p);
#endif
		MARK_INT_SAT(CODA_ROOT_STATS);
		return(0);
	    }
    }

    error = venus_root(vftomi(vfsp), p->p_ucred, p, &VFid);

    if (!error) {
	/*
	 * Save the new rootfid in the cnode, and rehash the cnode into the
	 * cnode hash with the new fid key.
	 */
	coda_unsave(VTOC(mi->mi_rootvp));
	VTOC(mi->mi_rootvp)->c_fid = VFid;
	coda_save(VTOC(mi->mi_rootvp));

	*vpp = mi->mi_rootvp;
#if	1
	vref(*vpp);
	vn_lock(*vpp, LK_EXCLUSIVE, p);
#else
	vget(*vpp, LK_EXCLUSIVE, p);
#endif

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
#if	1
	vref(*vpp);
	vn_lock(*vpp, LK_EXCLUSIVE, p);
#else
	vget(*vpp, LK_EXCLUSIVE, p);
#endif

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
 * Get file system statistics.
 */
int
coda_nb_statfs(vfsp, sbp, p)
    register struct mount *vfsp;
    struct statfs *sbp;
    struct proc *p;
{
    ENTRY;
/*  MARK_ENTRY(CODA_STATFS_STATS); */
    if (!CODA_MOUNTED(vfsp)) {
/*	MARK_INT_FAIL(CODA_STATFS_STATS);*/
	return(EINVAL);
    }
    
    bzero(sbp, sizeof(struct statfs));
    /* XXX - what to do about f_flags, others? --bnoble */
    /* Below This is what AFS does
    	#define NB_SFS_SIZ 0x895440
     */
    /* Note: Normal fs's have a bsize of 0x400 == 1024 */
    sbp->f_type = vfsp->mnt_vfc->vfc_typenum;
    sbp->f_bsize = 8192; /* XXX */
    sbp->f_iosize = 8192; /* XXX */
#define NB_SFS_SIZ 0x8AB75D
    sbp->f_blocks = NB_SFS_SIZ;
    sbp->f_bfree = NB_SFS_SIZ;
    sbp->f_bavail = NB_SFS_SIZ;
    sbp->f_files = NB_SFS_SIZ;
    sbp->f_ffree = NB_SFS_SIZ;
    bcopy((caddr_t)&(vfsp->mnt_stat.f_fsid), (caddr_t)&(sbp->f_fsid), sizeof (fsid_t));
    snprintf(sbp->f_mntonname, sizeof(sbp->f_mntonname), "/coda");
    snprintf(sbp->f_mntfromname, sizeof(sbp->f_mntfromname), "CODA");
/*  MARK_INT_SAT(CODA_STATFS_STATS); */
    return(0);
}

/*
 * Flush any pending I/O.
 */
int
coda_sync(vfsp, waitfor, cred, p)
    struct mount *vfsp;
    int    waitfor;
    struct ucred *cred;
    struct proc *p;
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
    struct proc *p = curproc; /* XXX -mach */
    ViceFid VFid;
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
    
    error = venus_fhtovp(vftomi(vfsp), &cfid->cfid_fid, p->p_ucred, p, &VFid, &vtype);
    
    if (error) {
	CODADEBUG(CODA_VGET, myprintf(("vget error %d\n",error));)
	    *vpp = (struct vnode *)0;
    } else {
	CODADEBUG(CODA_VGET, 
		 myprintf(("vget: vol %lx vno %lx uni %lx type %d result %d\n",
			VFid.Volume, VFid.Vnode, VFid.Unique, vtype, error)); )
	    
	cp = make_coda_node(&VFid, vfsp, vtype);
	*vpp = CTOV(cp);
    }
    return(error);
}

/*
 * To allow for greater ease of use, some vnodes may be orphaned when
 * Venus dies.  Certain operations should still be allowed to go
 * through, but without propagating ophan-ness.  So this function will
 * get a new vnode for the file from the current run of Venus.  */
 
int
getNewVnode(vpp)
     struct vnode **vpp;
{
    struct cfid cfid;
    struct coda_mntinfo *mi = vftomi((*vpp)->v_mount);
    
    ENTRY;

    cfid.cfid_len = (short)sizeof(ViceFid);
    cfid.cfid_fid = VTOC(*vpp)->c_fid;	/* Structure assignment. */
    /* XXX ? */

    /* We're guessing that if set, the 1st element on the list is a
     * valid vnode to use. If not, return ENODEV as venus is dead.
     */
    if (mi->mi_vfsp == NULL)
	return ENODEV;
    
    return coda_fhtovp(mi->mi_vfsp, (struct fid*)&cfid, NULL, vpp,
		      NULL, NULL);
}

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
/* get the mount structure corresponding to a given device.  Assume 
 * device corresponds to a UFS. Return NULL if no device is found.
 */ 
struct mount *devtomp(dev)
    dev_t dev;
{
    struct mount *mp;
   
    TAILQ_FOREACH(mp, &mountlist, mnt_list) {
	if (((VFSTOUFS(mp))->um_dev == dev)) {
	    /* mount corresponds to UFS and the device matches one we want */
	    return(mp); 
	}
    }
    /* mount structure wasn't found */ 
    return(NULL); 
}

struct vfsops coda_vfsops = {
    coda_mount,
    vfs_stdstart,
    coda_unmount,
    coda_root,
    vfs_stdquotactl,
    coda_nb_statfs,
    coda_sync,
    vfs_stdvget,
    vfs_stdfhtovp,
    vfs_stdcheckexp,
    vfs_stdvptofh,
    vfs_stdinit,
    vfs_stduninit,
    vfs_stdextattrctl,
};

VFS_SET(coda_vfsops, coda, VFCF_NETWORK);
