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
 * 	@(#) src/sys/coda/coda_fbsd.cr,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $
 *  $Id: coda_fbsd.c,v 1.4 1998/09/13 13:57:59 rvb Exp $
 * 
 */

#ifdef	ACTUALLY_LKM_NOT_KERNEL
#define NVCODA 4
#else
#include "vcoda.h"
#include "opt_devfs.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ucred.h>
#include <sys/vnode.h>
#include <sys/conf.h>

#include <vm/vm.h>
#include <vm/vnode_pager.h>

#include <coda/coda.h>
#include <coda/cnode.h>
#include <coda/coda_vnops.h>
#include <coda/coda_psdev.h>

#ifdef DEVFS
#include <sys/devfsext.h>

static	void	*devfs_token[NVCODA];
#endif

/* 
   From: "Jordan K. Hubbard" <jkh@time.cdrom.com>
   Subject: Re: New 3.0 SNAPshot CDROM about ready for production.. 
   To: "Robert.V.Baron" <rvb@GLUCK.CODA.CS.CMU.EDU>
   Date: Fri, 20 Feb 1998 15:57:01 -0800

   > Also I need a character device major number. (and might want to reserve
   > a block of 10 syscalls.)

   Just one char device number?  No block devices?  Very well, cdev 93 is yours!
*/

#define VC_DEV_NO      93

static struct cdevsw codadevsw =
{ 
  vc_nb_open,      vc_nb_close,    vc_nb_read,        vc_nb_write,	/*93*/
  vc_nb_ioctl,     nostop,         nullreset,         nodevtotty,
  vc_nb_poll,      nommap,         NULL,              "Coda", NULL, -1 
};

void vcattach __P((void));
static dev_t codadev;

int     vcdebug = 1;
#define VCDEBUG if (vcdebug) printf

void
vcattach(void)
{
  /*
   * In case we are an LKM, set up device switch.
   */
  if (0 == (codadev = makedev(VC_DEV_NO, 0)))
    VCDEBUG("makedev returned null\n");
  else 
    VCDEBUG("makedev OK.\n");
    
  cdevsw_add(&codadev, &codadevsw, NULL);
  VCDEBUG("coda: codadevsw entry installed at %d.\n", major(codadev));
}

static vc_devsw_installed = 0;
static void 	vc_drvinit __P((void *unused));

static void
vc_drvinit(void *unused)
{
	dev_t dev;
#ifdef DEVFS
	int i;
#endif

	if( ! vc_devsw_installed ) {
		dev = makedev(VC_DEV_NO, 0);
		cdevsw_add(&dev,&codadevsw, NULL);
		vc_devsw_installed = 1;
    	}
#ifdef DEVFS
	for (i = 0; i < NVCODA; i++) {
		devfs_token[i] =
			devfs_add_devswf(&codadevsw, i
				DV_CHR, 0, 0, 0666,
				"cfs%d", i);
		devfs_token[i] =
			devfs_add_devswf(&codadevsw, i
				DV_CHR, 0, 0, 0666,
				"coda%d", i);
	}
#endif

}

int
coda_fbsd_getpages(v)
	void *v;
{
    struct vop_getpages_args *ap = v;
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    int ret = 0;

#if	1
	/*??? a_offset */
	ret = vnode_pager_generic_getpages(ap->a_vp, ap->a_m, ap->a_count,
		ap->a_reqpage);
	return ret;
#else
  {
    struct vnode *vp = ap->a_vp;
    struct cnode *cp = VTOC(vp);
    struct vnode *cfvp = cp->c_ovp;
    int opened_internally = 0;
    struct ucred *cred = (struct ucred *) 0;
    struct proc *p = curproc;
    int error = 0;
	
    if (IS_CTL_VP(vp)) {
	return(EINVAL);
    }

    /* Redirect the request to UFS. */

    if (cfvp == NULL) {
	opened_internally = 1;

	error = VOP_OPEN(vp, FREAD,  cred, p);
printf("coda_getp: Internally Opening %p\n", vp);

	if (error) {
	    printf("coda_getpage: VOP_OPEN on container failed %d\n", error);
		return (error);
	}
	if (vp->v_type == VREG) {
	    error = vfs_object_create(vp, p, cred, 1);
	    if (error != 0) {
		printf("coda_getpage: vfs_object_create() returns %d\n", error);
		vput(vp);
		return(error);
	    }
	}

	cfvp = cp->c_ovp;
    } else {
printf("coda_getp: has container %p\n", cfvp);
    }

printf("coda_fbsd_getpages: using container ");
/*
    error = vnode_pager_generic_getpages(cfvp, ap->a_m, ap->a_count,
	ap->a_reqpage);
*/
    error = VOP_GETPAGES(cfvp, ap->a_m, ap->a_count,
	ap->a_reqpage, ap->a_offset);
printf("error = %d\n", error);

    /* Do an internal close if necessary. */
    if (opened_internally) {
	(void)VOP_CLOSE(vp, FREAD, cred, p);
    }

    return(error);
  }
#endif
}

int
coda_fbsd_putpages(v)
	void *v;
{
	struct vop_putpages_args *ap = v;

	/*??? a_offset */
	return vnode_pager_generic_putpages(ap->a_vp, ap->a_m, ap->a_count,
		ap->a_sync, ap->a_rtvals);
}


SYSINIT(codadev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+VC_DEV_NO,vc_drvinit,NULL)

#ifdef	ACTUALLY_LKM_NOT_KERNEL

#include <sys/mount.h>
#include <sys/lkm.h>

extern struct vfsops coda_vfsops;

static struct vfsconf _fs_vfsconf = { &coda_vfsops, "coda", -1, 0, 0 };

extern struct linker_set coda_modvnops ;

static struct lkm_vfs coda_mod_vfs  = {
	LM_VFS,	LKM_VERSION, "coda", 0, &coda_modvnops, &_fs_vfsconf };

static struct lkm_dev coda_mod_dev = {
	LM_DEV, LKM_VERSION, "codadev", VC_DEV_NO, LM_DT_CHAR, (void *) &codadevsw};

int coda_mod(struct lkm_table *, int, int);
int
coda_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	int error = 0;

	if (ver != LKM_VERSION)
		return EINVAL;

	switch (cmd) {
	case LKM_E_LOAD:
		lkmtp->private.lkm_any = (struct lkm_any *) &coda_mod_dev;
		error = lkmdispatch(lkmtp, cmd);
		if (error)
			break;
		lkmtp->private.lkm_any = (struct lkm_any *) &coda_mod_vfs ;
		error = lkmdispatch(lkmtp, cmd);
		break;
	case LKM_E_UNLOAD:
		lkmtp->private.lkm_any = (struct lkm_any *) &coda_mod_vfs ;
		error = lkmdispatch(lkmtp, cmd);
		if (error)
			break;
		lkmtp->private.lkm_any = (struct lkm_any *) &coda_mod_dev;
		error = lkmdispatch(lkmtp, cmd);
		break;
	case LKM_E_STAT:
		error = lkmdispatch(lkmtp, cmd);
		break;
	}
	return error;
}
#endif
