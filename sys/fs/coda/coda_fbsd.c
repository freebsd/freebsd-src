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
 * $FreeBSD$
 * 
 */

#include "vcoda.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vnode_pager.h>

#include <coda/coda.h>
#include <coda/cnode.h>
#include <coda/coda_vnops.h>
#include <coda/coda_psdev.h>

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

static struct cdevsw codadevsw = {
	/* open */	vc_nb_open,
	/* close */	vc_nb_close,
	/* read */	vc_nb_read,
	/* write */	vc_nb_write,
	/* ioctl */	vc_nb_ioctl,
	/* poll */	vc_nb_poll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"Coda",
	/* maj */	VC_DEV_NO,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

int     vcdebug = 1;
#define VCDEBUG if (vcdebug) printf

static int
codadev_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		cdevsw_add(&codadevsw);
		break;
	case MOD_UNLOAD:
		break;
	default:
		break;
	}
	return 0;
}
static moduledata_t codadev_mod = {
	"codadev",
	codadev_modevent,
	NULL
};
DECLARE_MODULE(codadev, codadev_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE+VC_DEV_NO);

int
coda_fbsd_getpages(v)
	void *v;
{
    struct vop_getpages_args *ap = v;
    int ret = 0;

#if	1
	return vop_stdgetpages(ap);
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
	    error = vfs_object_create(vp, p, cred);
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
