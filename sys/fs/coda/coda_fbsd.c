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

/* $Header: /afs/cs/project/coda-src/cvs/coda/kernel-src/vfs/freebsd/cfs/cfs_fbsd.c,v 1.6 1998/08/28 18:12:11 rvb Exp $ */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/ucred.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/vnode_pager.h>

#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <sys/conf.h>

#include <sys/vnode.h>
#include <cfs/coda.h>
#include <cfs/cnode.h>
#include <cfs/cfs_vnodeops.h>

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

#ifdef	__FreeBSD_version
/* Type of device methods. */
extern d_open_t  vc_nb_open;
extern d_close_t vc_nb_close;
extern d_read_t  vc_nb_read;
extern d_write_t vc_nb_write;
extern d_ioctl_t vc_nb_ioctl;
extern d_poll_t	 vc_nb_poll;

static struct cdevsw vccdevsw =
{ 
  vc_nb_open,      vc_nb_close,    vc_nb_read,        vc_nb_write,	/*93*/
  vc_nb_ioctl,     nostop,         nullreset,         nodevtotty,
  vc_nb_poll,      nommap,         NULL,              "Coda", NULL, -1 };
#else
/* Type of device methods. */
#define D_OPEN_T    d_open_t
#define D_CLOSE_T   d_close_t
#define D_RDWR_T    d_rdwr_t
#define D_READ_T    d_read_t
#define D_WRITE_T   d_write_t
#define D_IOCTL_T   d_ioctl_t
#define D_SELECT_T  d_select_t

/* rvb why */
D_OPEN_T    vc_nb_open;		/* was is defined in cfs_FreeBSD.h */
D_CLOSE_T   vc_nb_close;
D_READ_T    vc_nb_read;
D_WRITE_T   vc_nb_write;
D_IOCTL_T   vc_nb_ioctl;
D_SELECT_T  vc_nb_select;

static struct cdevsw vccdevsw =
{ 
  vc_nb_open,      vc_nb_close,    vc_nb_read,        vc_nb_write,
  vc_nb_ioctl,     nostop,         nullreset,         nodevtotty,
  vc_nb_select,    nommap,         NULL,              "Coda", NULL, -1 };

PSEUDO_SET(vcattach, vc);
#endif

void vcattach __P((void));
static dev_t vccdev;

int     vcdebug = 1;
#define VCDEBUG if (vcdebug) printf

void
vcattach(void)
{
  /*
   * In case we are an LKM, set up device switch.
   */
  if (0 == (vccdev = makedev(VC_DEV_NO, 0)))
    VCDEBUG("makedev returned null\n");
  else 
    VCDEBUG("makedev OK.\n");
    
  cdevsw_add(&vccdev, &vccdevsw, NULL);
  VCDEBUG("cfs: vccdevsw entry installed at %d.\n", major(vccdev));
}

void
cvref(vp)
	struct vnode *vp;
{
	if (vp->v_usecount <= 0)
		panic("vref used where vget required");

	vp->v_usecount++;
}


#ifdef	__FreeBSD_version
static vc_devsw_installed = 0;

static void 	vc_drvinit __P((void *unused));
static void
vc_drvinit(void *unused)
{
	dev_t dev;

	if( ! vc_devsw_installed ) {
		dev = makedev(VC_DEV_NO, 0);
		cdevsw_add(&dev,&vccdevsw, NULL);
		vc_devsw_installed = 1;
    	}
}

int
cfs_fbsd_getpages(v)
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
printf("cfs_getp: Internally Opening %p\n", vp);

	if (error) {
	    printf("cfs_getpage: VOP_OPEN on container failed %d\n", error);
		return (error);
	}
	if (vp->v_type == VREG) {
	    error = vfs_object_create(vp, p, cred, 1);
	    if (error != 0) {
		printf("cfs_getpage: vfs_object_create() returns %d\n", error);
		vput(vp);
		return(error);
	    }
	}

	cfvp = cp->c_ovp;
    } else {
printf("cfs_getp: has container %p\n", cfvp);
    }

printf("cfs_fbsd_getpages: using container ");
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
cfs_fbsd_putpages(v)
	void *v;
{
	struct vop_putpages_args *ap = v;

	/*??? a_offset */
	return vnode_pager_generic_putpages(ap->a_vp, ap->a_m, ap->a_count,
		ap->a_sync, ap->a_rtvals);
}


SYSINIT(vccdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+VC_DEV_NO,vc_drvinit,NULL)
#endif
