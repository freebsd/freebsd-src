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
 *  $Id: coda_vfsops.c,v 1.13 1999/05/09 13:11:37 phk Exp $
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

/*
 * HISTORY
 * $Log: coda_vfsops.c,v $
 * Revision 1.13  1999/05/09 13:11:37  phk
 * remove cast from dev_t to dev_t.
 *
 * Revision 1.12  1999/05/08 06:39:04  phk
 * I got tired of seeing all the cdevsw[major(foo)] all over the place.
 *
 * Made a new (inline) function devsw(dev_t dev) and substituted it.
 *
 * Changed to the BDEV variant to this format as well: bdevsw(dev_t dev)
 *
 * DEVFS will eventually benefit from this change too.
 *
 * Revision 1.11  1999/01/17 20:25:17  peter
 * Clean up the KLD/LKM goop a bit.
 *
 * Revision 1.10  1998/12/04 22:54:43  archie
 * Examine all occurrences of sprintf(), strcat(), and str[n]cpy()
 * for possible buffer overflow problems. Replaced most sprintf()'s
 * with snprintf(); for others cases, added terminating NUL bytes where
 * appropriate, replaced constants like "16" with sizeof(), etc.
 *
 * These changes include several bug fixes, but most changes are for
 * maintainability's sake. Any instance where it wasn't "immediately
 * obvious" that a buffer overflow could not occur was made safer.
 *
 * Reviewed by:	Bruce Evans <bde@zeta.org.au>
 * Reviewed by:	Matthew Dillon <dillon@apollo.backplane.com>
 * Reviewed by:	Mike Spengler <mks@networkcs.com>
 *
 * Revision 1.9  1998/11/16 19:48:26  rvb
 * A few bug fixes for Robert Watson
 *
 * Revision 1.8  1998/11/03 08:55:06  peter
 * Support KLD.  We register and unregister two modules. "coda" (the vfs)
 * via VFS_SET(), and "codadev" for the cdevsw entry.  From kldstat -v:
 *  3    1 0xf02c5000 115d8    coda.ko
 *         Contains modules:
 *                 Id Name
 *                  2 codadev
 *                  3 coda
 *
 * Revision 1.7  1998/09/29 20:19:45  rvb
 * Fixes for lkm:
 * 1. use VFS_LKM vs ACTUALLY_LKM_NOT_KERNEL
 * 2. don't pass -DCODA to lkm build
 *
 * Revision 1.6  1998/09/25 17:38:32  rvb
 * Put "stray" printouts under DIAGNOSTIC.  Make everything build
 * with DEBUG on.  Add support for lkm.  (The macro's don't work
 * for me; for a good chuckle look at the end of coda_fbsd.c.)
 *
 * Revision 1.5  1998/09/13 13:57:59  rvb
 * Finish conversion of cfs -> coda
 *
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
 * Bring RVB_CODA1_1 to HEAD
 * 
 * Revision 1.6.2.6  98/01/23  11:21:07  rvb
 * Sync with 2.2.5
 * 
 * Revision 1.6.2.5  98/01/22  13:05:33  rvb
 * Move make_coda_node ctlfid later so vfsp is known
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
 * Catch a few coda_calls in coda_vfsops.c
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
 * Added CODA-specific files
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
 * 	Added call to coda_flush to coda_unmount.
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

#include <vcoda.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/select.h>

#include <coda/coda.h>
#include <coda/cnode.h>
#include <coda/coda_vfsops.h>
#include <coda/coda_venus.h>
#include <coda/coda_subr.h>
#include <coda/coda_opstats.h>

#include <miscfs/specfs/specdev.h>

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
	return(ENXIO);
    }
    dev = dvp->v_specinfo->si_rdev;
    vrele(dvp);

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
coda_start(vfsp, flags, p)
    struct mount *vfsp;
    int flags;
    struct proc *p;
{
    ENTRY;
    return (0);
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

    error = venus_root(vftomi(vfsp), p->p_cred->pc_ucred, p, &VFid);

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

int
coda_quotactl(vfsp, cmd, uid, arg, p)
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

int
coda_vget(vfsp, ino, vpp)
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
    
    error = venus_fhtovp(vftomi(vfsp), &cfid->cfid_fid, p->p_cred->pc_ucred, p, &VFid, &vtype);
    
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

int
coda_vptofh(vnp, fidp)
    struct vnode *vnp;
    struct fid   *fidp;
{
    ENTRY;
    return (EOPNOTSUPP);
}

int
coda_init(struct vfsconf *vfsp)
{
    ENTRY;
    return 0;
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
    coda_start,
    coda_unmount,
    coda_root,
    coda_quotactl,
    coda_nb_statfs,
    coda_sync,
    coda_vget,
    (int (*) (struct mount *, struct fid *, struct sockaddr *, struct vnode **,
	      int *, struct ucred **))
	eopnotsupp,
    (int (*) (struct vnode *, struct fid *)) eopnotsupp,
    coda_init,
};

VFS_SET(coda_vfsops, coda, VFCF_NETWORK);
