/*

            Coda: an Experimental Distributed File System
                             Release 3.1

          Copyright (c) 1987-1998 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

/* $Header: /afs/cs/project/coda-src/cvs/coda/kernel-src/vfs/freebsd/cfs/cfs_vfsops.c,v 1.11 1998/08/28 18:12:22 rvb Exp $ */

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

/*
 * HISTORY
 * $Log: cfs_vfsops.c,v $
 * Revision 1.11  1998/08/28 18:12:22  rvb
 * Now it also works on FreeBSD -current.  This code will be
 * committed to the FreeBSD -current and NetBSD -current
 * trees.  It will then be tailored to the particular platform
 * by flushing conditional code.
 *
 * Revision 1.10  1998/08/18 17:05:19  rvb
 * Don't use __RCSID now
 *
 * Revision 1.9  1998/08/18 16:31:44  rvb
 * Sync the code for NetBSD -current; test on 1.3 later
 *
 * Revision 1.8  98/02/24  22:22:48  rvb
 * Fixes up mainly to flush iopen and friends
 * 
 * Revision 1.7  98/01/23  11:53:45  rvb
 * Bring RVB_CFS1_1 to HEAD
 * 
 * Revision 1.6.2.6  98/01/23  11:21:07  rvb
 * Sync with 2.2.5
 * 
 * Revision 1.6.2.5  98/01/22  13:05:33  rvb
 * Move makecfsnode ctlfid later so vfsp is known
 * 
 * Revision 1.6.2.4  97/12/19  14:26:05  rvb
 * session id
 * 
 * Revision 1.6.2.3  97/12/16  12:40:11  rvb
 * Sync with 1.3
 * 
 * Revision 1.6.2.2  97/12/10  11:40:25  rvb
 * No more ody
 * 
 * Revision 1.6.2.1  97/12/06  17:41:24  rvb
 * Sync with peters coda.h
 * 
 * Revision 1.6  97/12/05  10:39:21  rvb
 * Read CHANGES
 * 
 * Revision 1.5.14.8  97/11/24  15:44:46  rvb
 * Final cfs_venus.c w/o macros, but one locking bug
 * 
 * Revision 1.5.14.7  97/11/21  13:22:03  rvb
 * Catch a few cfscalls in cfs_vfsops.c
 * 
 * Revision 1.5.14.6  97/11/20  11:46:48  rvb
 * Capture current cfs_venus
 * 
 * Revision 1.5.14.5  97/11/18  10:27:17  rvb
 * cfs_nbsd.c is DEAD!!!; integrated into cfs_vf/vnops.c
 * cfs_nb_foo and cfs_foo are joined
 * 
 * Revision 1.5.14.4  97/11/13  22:03:01  rvb
 * pass2 cfs_NetBSD.h mt
 * 
 * Revision 1.5.14.3  97/11/12  12:09:40  rvb
 * reorg pass1
 * 
 * Revision 1.5.14.2  97/10/29  16:06:28  rvb
 * Kill DYING
 * 
 * Revision 1.5.14.1  1997/10/28 23:10:17  rvb
 * >64Meg; venus can be killed!
 *
 * Revision 1.5  1997/01/13 17:11:07  bnoble
 * Coda statfs needs to return something other than -1 for blocks avail. and
 * files available for wabi (and other windowsish) programs to install
 * there correctly.
 *
 * Revision 1.4  1996/12/12 22:11:00  bnoble
 * Fixed the "downcall invokes venus operation" deadlock in all known cases.
 * There may be more
 *
 * Revision 1.3  1996/11/08 18:06:12  bnoble
 * Minor changes in vnode operation signature, VOP_UPDATE signature, and
 * some newly defined bits in the include files.
 *
 * Revision 1.2  1996/01/02 16:57:04  bnoble
 * Added support for Coda MiniCache and raw inode calls (final commit)
 *
 * Revision 1.1.2.1  1995/12/20 01:57:32  bnoble
 * Added CFS-specific files
 *
 * Revision 3.1.1.1  1995/03/04  19:08:02  bnoble
 * Branch for NetBSD port revisions
 *
 * Revision 3.1  1995/03/04  19:08:01  bnoble
 * Bump to major revision 3 to prepare for NetBSD port
 *
 * Revision 2.4  1995/02/17  16:25:22  dcs
 * These versions represent several changes:
 * 1. Allow venus to restart even if outstanding references exist.
 * 2. Have only one ctlvp per client, as opposed to one per mounted cfs device.d
 * 3. Allow ody_expand to return many members, not just one.
 *
 * Revision 2.3  94/10/14  09:58:21  dcs
 * Made changes 'cause sun4s have braindead compilers
 * 
 * Revision 2.2  94/10/12  16:46:33  dcs
 * Cleaned kernel/venus interface by removing XDR junk, plus
 * so cleanup to allow this code to be more easily ported.
 * 
 * Revision 1.3  93/05/28  16:24:29  bnoble
 * *** empty log message ***
 * 
 * Revision 1.2  92/10/27  17:58:24  lily
 * merge kernel/latest and alpha/src/cfs
 * 
 * Revision 2.3  92/09/30  14:16:32  mja
 * 	Added call to cfs_flush to cfs_unmount.
 * 	[90/12/15            dcs]
 * 
 * 	Added contributors blurb.
 * 	[90/12/13            jjk]
 * 
 * Revision 2.2  90/07/05  11:26:40  mrt
 * 	Created for the Coda File System.
 * 	[90/05/23            dcs]
 * 
 * Revision 1.3  90/05/31  17:01:42  dcs
 * Prepare for merge with facilities kernel.
 * 
 * 
 */ 
#include <vcfs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/select.h>

#include <cfs/coda.h>
#include <cfs/cnode.h>
#include <cfs/cfs_vfsops.h>
#include <cfs/cfs_venus.h>
#include <cfs/cfs_subr.h>
#include <cfs/coda_opstats.h>
/* for VN_RDEV */
#include <miscfs/specfs/specdev.h>

#ifdef	__FreeBSD__
#ifdef	__FreeBSD_version
MALLOC_DEFINE(M_CFS, "CFS storage", "Various Coda Structures");
#endif
#endif

int cfsdebug = 0;

int cfs_vfsop_print_entry = 0;
#ifdef __GNUC__
#define ENTRY    \
    if(cfs_vfsop_print_entry) myprintf(("Entered %s\n",__FUNCTION__))
#else
#define ENTRY
#endif 


struct vnode *cfs_ctlvp;
struct cfs_mntinfo cfs_mnttbl[NVCFS]; /* indexed by minor device number */

/* structure to keep statistics of internally generated/satisfied calls */

struct cfs_op_stats cfs_vfsopstats[CFS_VFSOPS_SIZE];

#define MARK_ENTRY(op) (cfs_vfsopstats[op].entries++)
#define MARK_INT_SAT(op) (cfs_vfsopstats[op].sat_intrn++)
#define MARK_INT_FAIL(op) (cfs_vfsopstats[op].unsat_intrn++)
#define MRAK_INT_GEN(op) (cfs_vfsopstats[op].gen_intrn++)

extern int cfsnc_initialized;     /* Set if cache has been initialized */
extern int vc_nb_open __P((dev_t, int, int, struct proc *));
#ifdef	__NetBSD__
extern struct cdevsw cdevsw[];    /* For sanity check in cfs_mount */
#endif
/* NetBSD interface to statfs */

#if	defined(__NetBSD__) && defined(NetBSD1_3) && (NetBSD1_3 >= 5)
extern struct vnodeopv_desc cfs_vnodeop_opv_desc;

struct vnodeopv_desc *cfs_vnodeopv_descs[] = {
	&cfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops cfs_vfsops = {
    MOUNT_CFS,
    cfs_mount,
    cfs_start,
    cfs_unmount,
    cfs_root,
    cfs_quotactl,
    cfs_nb_statfs,
    cfs_sync,
    cfs_vget,
    (int (*) (struct mount *, struct fid *, struct mbuf *, struct vnode **,
	      int *, struct ucred **))
	eopnotsupp,
    (int (*) (struct vnode *, struct fid *)) eopnotsupp,
    cfs_init,
#if (NetBSD1_3 >= 7)
    cfs_sysctl,
#endif
    (int (*)(void)) eopnotsupp,
    cfs_vnodeopv_descs,
    0
};
#elif	defined(__NetBSD__)
struct vfsops cfs_vfsops = {
    MOUNT_CFS,
    cfs_mount,
    cfs_start,
    cfs_unmount,
    cfs_root,
    cfs_quotactl,
    cfs_nb_statfs,
    cfs_sync,
    cfs_vget,
    (int (*) (struct mount *, struct fid *, struct mbuf *, struct vnode **,
	      int *, struct ucred **))
	eopnotsupp,
    (int (*) (struct vnode *, struct fid *)) eopnotsupp,
    cfs_init,
#ifdef	NetBSD1_3
    (int (*)(void)) eopnotsupp,
#endif
    0
};

#elif	defined(__FreeBSD__)
#ifdef	__FreeBSD_version
struct vfsops cfs_vfsops = {
    cfs_mount,
    cfs_start,
    cfs_unmount,
    cfs_root,
    cfs_quotactl,
    cfs_nb_statfs,
    cfs_sync,
    cfs_vget,
    (int (*) (struct mount *, struct fid *, struct sockaddr *, struct vnode **,
	      int *, struct ucred **))
	eopnotsupp,
    (int (*) (struct vnode *, struct fid *)) eopnotsupp,
    cfs_init,
};

#else
struct vfsops cfs_vfsops = {
    cfs_mount,
    cfs_start,
    cfs_unmount,
    cfs_root,
    cfs_quotactl,
    cfs_nb_statfs,
    cfs_sync,
    cfs_vget,
    (int (*) (struct mount *, struct fid *, struct mbuf *, struct vnode **,
	      int *, struct ucred **))
	eopnotsupp,
    (int (*) (struct vnode *, struct fid *)) eopnotsupp,
    cfs_init,
};

#endif


#include <sys/kernel.h>
VFS_SET(cfs_vfsops, cfs, MOUNT_CFS, VFCF_NETWORK);
#endif

int
cfs_vfsopstats_init(void)
{
	register int i;
	
	for (i=0;i<CFS_VFSOPS_SIZE;i++) {
		cfs_vfsopstats[i].opcode = i;
		cfs_vfsopstats[i].entries = 0;
		cfs_vfsopstats[i].sat_intrn = 0;
		cfs_vfsopstats[i].unsat_intrn = 0;
		cfs_vfsopstats[i].gen_intrn = 0;
	}
	
	return 0;
}


/*
 * cfs mount vfsop
 * Set up mount info record and attach it to vfs struct.
 */
/*ARGSUSED*/
int
cfs_mount(vfsp, path, data, ndp, p)
    struct mount *vfsp;		/* Allocated and initialized by mount(2) */
#ifdef	NetBSD1_3
    const char *path;		/* path covered: ignored by the fs-layer */
    void *data;			/* Need to define a data type for this in netbsd? */
#else
    char *path;			/* path covered: ignored by the fs-layer */
    caddr_t data;		/* Need to define a data type for this in netbsd? */
#endif
    struct nameidata *ndp;	/* Clobber this to lookup the device name */
    struct proc *p;		/* The ever-famous proc pointer */
{
    struct vnode *dvp;
    struct cnode *cp;
    dev_t dev;
    struct cfs_mntinfo *mi;
    struct vnode *rootvp;
    ViceFid rootfid;
    ViceFid ctlfid;
    int error;

    ENTRY;

    cfs_vfsopstats_init();
    cfs_vnodeopstats_init();
    
    MARK_ENTRY(CFS_MOUNT_STATS);
    if (CFS_MOUNTED(vfsp)) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return(EBUSY);
    }
    
    /* Validate mount device.  Similar to getmdev(). */

    NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, data, p);
    error = namei(ndp);
    dvp = ndp->ni_vp;

    if (error) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return (error);
    }
    if (dvp->v_type != VCHR) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	vrele(dvp);
	return(ENXIO);
    }
    dev = dvp->v_specinfo->si_rdev;
    vrele(dvp);
    if (major(dev) >= nchrdev || major(dev) < 0) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return(ENXIO);
    }

    /*
     * See if the device table matches our expectations.
     */
#ifdef	__NetBSD__
    if (cdevsw[major(dev)].d_open != vc_nb_open)
#elif	defined(__FreeBSD__)
    if (cdevsw[major(dev)]->d_open != vc_nb_open)
#endif
    {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return(ENXIO);
    }
    
    if (minor(dev) >= NVCFS || minor(dev) < 0) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return(ENXIO);
    }
    
    /*
     * Initialize the mount record and link it to the vfs struct
     */
    mi = &cfs_mnttbl[minor(dev)];
    
    if (!VC_OPEN(&mi->mi_vcomm)) {
	MARK_INT_FAIL(CFS_MOUNT_STATS);
	return(ENODEV);
    }
    
    /* No initialization (here) of mi_vcomm! */
    vfsp->mnt_data = (qaddr_t)mi;
#ifdef	__NetBSD__
    vfsp->mnt_stat.f_fsid.val[0] = 0;
    vfsp->mnt_stat.f_fsid.val[1] = makefstype(MOUNT_CFS);
#elif	defined(__FreeBSD__) && defined(__FreeBSD_version)

    vfs_getnewfsid (vfsp);

#elif	defined(__FreeBSD__)
    /* Seems a bit overkill, since usualy /coda is the only mount point
     * for cfs.
     */
    getnewfsid (vfsp, MOUNT_CFS);
#endif
    mi->mi_vfsp = vfsp;
    
    /*
     * Make a root vnode to placate the Vnode interface, but don't
     * actually make the CFS_ROOT call to venus until the first call
     * to cfs_root in case a server is down while venus is starting.
     */
    rootfid.Volume = 0;
    rootfid.Vnode = 0;
    rootfid.Unique = 0;
    cp = makecfsnode(&rootfid, vfsp, VDIR);
    rootvp = CTOV(cp);
    rootvp->v_flag |= VROOT;
	
    ctlfid.Volume = CTL_VOL;
    ctlfid.Vnode = CTL_VNO;
    ctlfid.Unique = CTL_UNI;
/*  cp = makecfsnode(&ctlfid, vfsp, VCHR);
    The above code seems to cause a loop in the cnode links.
    I don't totally understand when it happens, it is caught
    when closing down the system.
 */
    cp = makecfsnode(&ctlfid, 0, VCHR);

    cfs_ctlvp = CTOV(cp);

    /* Add vfs and rootvp to chain of vfs hanging off mntinfo */
    mi->mi_vfsp = vfsp;
    mi->mi_rootvp = rootvp;
    
    /* set filesystem block size */
    vfsp->mnt_stat.f_bsize = 8192;	    /* XXX -JJK */
#ifdef	 __FreeBSD__
    /* Set f_iosize.  XXX -- inamura@isl.ntt.co.jp. 
       For vnode_pager_haspage() references. The value should be obtained 
       from underlying UFS. */
    /* Checked UFS. iosize is set as 8192 */
    vfsp->mnt_stat.f_iosize = 8192;
#endif

    /* error is currently guaranteed to be zero, but in case some
       code changes... */
    CFSDEBUG(1,
	     myprintf(("cfs_mount returned %d\n",error)););
    if (error)
	MARK_INT_FAIL(CFS_MOUNT_STATS);
    else
	MARK_INT_SAT(CFS_MOUNT_STATS);
    
    return(error);
}

int
cfs_start(vfsp, flags, p)
    struct mount *vfsp;
    int flags;
    struct proc *p;
{
    ENTRY;
    return (0);
}

int
cfs_unmount(vfsp, mntflags, p)
    struct mount *vfsp;
    int mntflags;
    struct proc *p;
{
    struct cfs_mntinfo *mi = vftomi(vfsp);
    int active, error = 0;
    
    ENTRY;
    MARK_ENTRY(CFS_UMOUNT_STATS);
    if (!CFS_MOUNTED(vfsp)) {
	MARK_INT_FAIL(CFS_UMOUNT_STATS);
	return(EINVAL);
    }
    
    if (mi->mi_vfsp == vfsp) {	/* We found the victim */
	if (!IS_UNMOUNTING(VTOC(mi->mi_rootvp)))
	    return (EBUSY); 	/* Venus is still running */

#ifdef	DEBUG
	printf("cfs_unmount: ROOT: vp %p, cp %p\n", mi->mi_rootvp, VTOC(mi->mi_rootvp));
#endif
	vrele(mi->mi_rootvp);

#ifdef	NetBSD1_3
	active = cfs_kill(vfsp, NOT_DOWNCALL);

#if	defined(__NetBSD__) && defined(NetBSD1_3) && (NetBSD1_3 >= 7)
	if (1)
#else
	if ((error = vfs_busy(mi->mi_vfsp)) == 0)
#endif
	{
		error = vflush(mi->mi_vfsp, NULLVP, FORCECLOSE);
		printf("cfs_unmount: active = %d, vflush active %d\n", active, error);
		error = 0;
	} else {
		printf("cfs_unmount: busy\n");
	} 
#else	/* FreeBSD I guess */
	active = cfs_kill(vfsp, NOT_DOWNCALL);
	error = vflush(mi->mi_vfsp, NULLVP, FORCECLOSE);
	printf("cfs_unmount: active = %d, vflush active %d\n", active, error);
	error = 0;
#endif
	/* I'm going to take this out to allow lookups to go through. I'm
	 * not sure it's important anyway. -- DCS 2/2/94
	 */
	/* vfsp->VFS_DATA = NULL; */

	/* No more vfsp's to hold onto */
	mi->mi_vfsp = NULL;
	mi->mi_rootvp = NULL;

	if (error)
	    MARK_INT_FAIL(CFS_UMOUNT_STATS);
	else
	    MARK_INT_SAT(CFS_UMOUNT_STATS);

	return(error);
    }
    return (EINVAL);
}

/*
 * find root of cfs
 */
int
cfs_root(vfsp, vpp)
	struct mount *vfsp;
	struct vnode **vpp;
{
    struct cfs_mntinfo *mi = vftomi(vfsp);
    struct vnode **result;
    int error;
    struct proc *p = curproc;    /* XXX - bnoble */
    ViceFid VFid;

    ENTRY;
    MARK_ENTRY(CFS_ROOT_STATS);
    result = NULL;
    
    if (vfsp == mi->mi_vfsp) {
	if ((VTOC(mi->mi_rootvp)->c_fid.Volume != 0) ||
	    (VTOC(mi->mi_rootvp)->c_fid.Vnode != 0) ||
	    (VTOC(mi->mi_rootvp)->c_fid.Unique != 0))
	    { /* Found valid root. */
		*vpp = mi->mi_rootvp;
		/* On Mach, this is vref.  On NetBSD, VOP_LOCK */
		vref(*vpp);
		VOP_X_LOCK(*vpp, LK_EXCLUSIVE);
		MARK_INT_SAT(CFS_ROOT_STATS);
		return(0);
	    }
    }

    error = venus_root(vftomi(vfsp), p->p_cred->pc_ucred, p, &VFid);

    if (!error) {
	/*
	 * Save the new rootfid in the cnode, and rehash the cnode into the
	 * cnode hash with the new fid key.
	 */
	cfs_unsave(VTOC(mi->mi_rootvp));
	VTOC(mi->mi_rootvp)->c_fid = VFid;
	cfs_save(VTOC(mi->mi_rootvp));

	*vpp = mi->mi_rootvp;
	vref(*vpp);
	VOP_X_LOCK(*vpp, LK_EXCLUSIVE);
	MARK_INT_SAT(CFS_ROOT_STATS);
	goto exit;
    } else if (error == ENODEV) {
	/* Gross hack here! */
	/*
	 * If Venus fails to respond to the CFS_ROOT call, cfscall returns
	 * ENODEV. Return the uninitialized root vnode to allow vfs
	 * operations such as unmount to continue. Without this hack,
	 * there is no way to do an unmount if Venus dies before a 
	 * successful CFS_ROOT call is done. All vnode operations 
	 * will fail.
	 */
	*vpp = mi->mi_rootvp;
	vref(*vpp);
	VOP_X_LOCK(*vpp, LK_EXCLUSIVE);
	MARK_INT_FAIL(CFS_ROOT_STATS);
	error = 0;
	goto exit;
    } else {
	CFSDEBUG( CFS_ROOT, myprintf(("error %d in CFS_ROOT\n", error)); );
	MARK_INT_FAIL(CFS_ROOT_STATS);
		
	goto exit;
    }
 exit:
    return(error);
}

int
cfs_quotactl(vfsp, cmd, uid, arg, p)
    struct mount *vfsp;
    int cmd;
    uid_t uid;
    caddr_t arg;
    struct proc *p;
{
    ENTRY;
    return (EOPNOTSUPP);
}
     
/*
 * Get file system statistics.
 */
int
cfs_nb_statfs(vfsp, sbp, p)
    register struct mount *vfsp;
    struct statfs *sbp;
    struct proc *p;
{
    ENTRY;
/*  MARK_ENTRY(CFS_STATFS_STATS); */
    if (!CFS_MOUNTED(vfsp)) {
/*	MARK_INT_FAIL(CFS_STATFS_STATS);*/
	return(EINVAL);
    }
    
    bzero(sbp, sizeof(struct statfs));
    /* XXX - what to do about f_flags, others? --bnoble */
    /* Below This is what AFS does
    	#define NB_SFS_SIZ 0x895440
     */
    /* Note: Normal fs's have a bsize of 0x400 == 1024 */
#ifdef	__NetBSD__
    sbp->f_type = 0;
#elif	defined(__FreeBSD__)
    sbp->f_type = MOUNT_CFS;
#endif
    sbp->f_bsize = 8192; /* XXX */
    sbp->f_iosize = 8192; /* XXX */
#define NB_SFS_SIZ 0x8AB75D
    sbp->f_blocks = NB_SFS_SIZ;
    sbp->f_bfree = NB_SFS_SIZ;
    sbp->f_bavail = NB_SFS_SIZ;
    sbp->f_files = NB_SFS_SIZ;
    sbp->f_ffree = NB_SFS_SIZ;
    bcopy((caddr_t)&(vfsp->mnt_stat.f_fsid), (caddr_t)&(sbp->f_fsid), sizeof (fsid_t));
#ifdef	__NetBSD__
    strncpy(sbp->f_fstypename, MOUNT_CFS, MFSNAMELEN-1);
#endif
    strcpy(sbp->f_mntonname, "/coda");
    strcpy(sbp->f_mntfromname, "CFS");
/*  MARK_INT_SAT(CFS_STATFS_STATS); */
    return(0);
}

/*
 * Flush any pending I/O.
 */
int
cfs_sync(vfsp, waitfor, cred, p)
    struct mount *vfsp;
    int    waitfor;
    struct ucred *cred;
    struct proc *p;
{
    ENTRY;
    MARK_ENTRY(CFS_SYNC_STATS);
    MARK_INT_SAT(CFS_SYNC_STATS);
    return(0);
}

int
cfs_vget(vfsp, ino, vpp)
    struct mount *vfsp;
    ino_t ino;
    struct vnode **vpp;
{
    ENTRY;
    return (EOPNOTSUPP);
}

/* 
 * fhtovp is now what vget used to be in 4.3-derived systems.  For
 * some silly reason, vget is now keyed by a 32 bit ino_t, rather than
 * a type-specific fid.  
 */
int
cfs_fhtovp(vfsp, fhp, nam, vpp, exflagsp, creadanonp)
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
    
    MARK_ENTRY(CFS_VGET_STATS);
    /* Check for vget of control object. */
    if (IS_CTL_FID(&cfid->cfid_fid)) {
	*vpp = cfs_ctlvp;
	vref(cfs_ctlvp);
	MARK_INT_SAT(CFS_VGET_STATS);
	return(0);
    }
    
    error = venus_fhtovp(vftomi(vfsp), &cfid->cfid_fid, p->p_cred->pc_ucred, p, &VFid, &vtype);
    
    if (error) {
	CFSDEBUG(CFS_VGET, myprintf(("vget error %d\n",error));)
	    *vpp = (struct vnode *)0;
    } else {
	CFSDEBUG(CFS_VGET, 
		 myprintf(("vget: vol %lx vno %lx uni %lx type %d result %d\n",
			VFid.Volume, VFid.Vnode, VFid.Unique, vtype, error)); )
	    
	cp = makecfsnode(&VFid, vfsp, vtype);
	*vpp = CTOV(cp);
    }
    return(error);
}

int
cfs_vptofh(vnp, fidp)
    struct vnode *vnp;
    struct fid   *fidp;
{
    ENTRY;
    return (EOPNOTSUPP);
}

#ifdef	__NetBSD__ 
void
cfs_init(void)
{
    ENTRY;
}
#elif	defined(__FreeBSD__)
#ifdef	__FreeBSD_version
int
cfs_init(struct vfsconf *vfsp)
{
    ENTRY;
    return 0;
}
#else
int
cfs_init(void)
{
    ENTRY;
    return 0;
}
#endif
#endif

#if	defined(__NetBSD__) && defined(NetBSD1_3) && (NetBSD1_3 >= 7)
int
cfs_sysctl(name, namelen, oldp, oldlp, newp, newl, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlp;
	void *newp;
	size_t newl;
	struct proc *p;
{

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
/*
	case FFS_CLUSTERREAD:
		return (sysctl_int(oldp, oldlp, newp, newl, &doclusterread));
 */
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
#endif

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
    struct cfs_mntinfo *mi = vftomi((*vpp)->v_mount);
    
    ENTRY;

    cfid.cfid_len = (short)sizeof(ViceFid);
    cfid.cfid_fid = VTOC(*vpp)->c_fid;	/* Structure assignment. */
    /* XXX ? */

    /* We're guessing that if set, the 1st element on the list is a
     * valid vnode to use. If not, return ENODEV as venus is dead.
     */
    if (mi->mi_vfsp == NULL)
	return ENODEV;
    
    return cfs_fhtovp(mi->mi_vfsp, (struct fid*)&cfid, NULL, vpp,
		      NULL, NULL);
}

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
/* get the mount structure corresponding to a given device.  Assume 
 * device corresponds to a UFS. Return NULL if no device is found.
 */ 
struct mount *devtomp(dev)
    dev_t dev;
{
    struct mount *mp, *nmp;
    
    for (mp = mountlist.cqh_first; mp != (void*)&mountlist; mp = nmp) {
	nmp = mp->mnt_list.cqe_next;
	if (
#ifdef	__NetBSD__
	    (!strcmp(mp->mnt_op->vfs_name, MOUNT_UFS)) &&
#endif
	    ((VFSTOUFS(mp))->um_dev == (dev_t) dev)) {
	    /* mount corresponds to UFS and the device matches one we want */
	    return(mp); 
	}
    }
    /* mount structure wasn't found */ 
    return(NULL); 
}
