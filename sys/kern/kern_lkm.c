/*-
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1994 Christopher G. Demetriou
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/sysent.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/lkm.h>
#include <sys/vnode.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>


#define PAGESIZE 1024		/* kmem_alloc() allocation quantum */

#define	LKM_ALLOC	0x01
#define	LKM_WANT	0x02

#define	LKMS_IDLE	0x00
#define	LKMS_RESERVED	0x01
#define	LKMS_LOADING	0x02
#define	LKMS_LOADED	0x04
#define	LKMS_UNLOADING	0x08

static int	lkm_v = 0;
static int	lkm_state = LKMS_IDLE;

#ifndef MAXLKMS
#define	MAXLKMS		20
#endif

static struct lkm_table	lkmods[MAXLKMS];	/* table of loaded modules */
static struct lkm_table	*curp;			/* global for in-progress ops */

static int	_lkm_dev __P((struct lkm_table *lkmtp, int cmd));
static int	_lkm_exec __P((struct lkm_table *lkmtp, int cmd));
static int	_lkm_vfs __P((struct lkm_table *lkmtp, int cmd));
static int	_lkm_syscall __P((struct lkm_table *lkmtp, int cmd));
static void	lkmunreserve __P((void));

static	d_open_t	lkmcopen;
static	d_close_t	lkmcclose;
static	d_ioctl_t	lkmcioctl;

#define CDEV_MAJOR 32
static struct cdevsw lkmc_cdevsw = 
	{ lkmcopen,	lkmcclose,	noread,		nowrite,	/*32*/
	  lkmcioctl,	nostop,		nullreset,	nodevtotty,
	  noselect,	nommap,		NULL,	"lkm",	NULL,	-1 };


/*ARGSUSED*/
static	int
lkmcopen(dev, flag, devtype, p)
	dev_t dev;
	int flag;
	int devtype;
	struct proc *p;
{
	int error;

	if (minor(dev) != 0)
		return(ENXIO);		/* bad minor # */

	/*
	 * Use of the loadable kernel module device must be exclusive; we
	 * may try to remove this restriction later, but it's really no
	 * hardship.
	 */
	while (lkm_v & LKM_ALLOC) {
		if (flag & FNONBLOCK)		/* don't hang */
			return(EBUSY);
		lkm_v |= LKM_WANT;
		/*
		 * Sleep pending unlock; we use tsleep() to allow
		 * an alarm out of the open.
		 */
		error = tsleep((caddr_t)&lkm_v, TTIPRI|PCATCH, "lkmopn", 0);
		if (error)
			return(error);	/* leave LKM_WANT set -- no problem */
	}
	lkm_v |= LKM_ALLOC;

	return(0);		/* pseudo-device open */
}

/*
 * Unreserve the memory associated with the current loaded module; done on
 * a coerced close of the lkm device (close on premature exit of modload)
 * or explicitly by modload as a result of a link failure.
 */
static void
lkmunreserve()
{

	if (lkm_state == LKMS_IDLE)
		return;

	/*
	 * Actually unreserve the memory
	 */
	if (curp && curp->area) {
		kmem_free(kernel_map, curp->area, curp->size);/**/
		curp->area = 0;
		if (curp->private.lkm_any != NULL)
			curp->private.lkm_any = NULL;
	}

	lkm_state = LKMS_IDLE;
}

static	int
lkmcclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{

	if (!(lkm_v & LKM_ALLOC)) {
#ifdef DEBUG
		printf("LKM: close before open!\n");
#endif	/* DEBUG */
		return(EBADF);
	}

	/* do this before waking the herd... */
	if (curp && !curp->used) {
		/*
		 * If we close before setting used, we have aborted
		 * by way of error or by way of close-on-exit from
		 * a premature exit of "modload".
		 */
		lkmunreserve();	/* coerce state to LKM_IDLE */
	}

	lkm_v &= ~LKM_ALLOC;
	wakeup((caddr_t)&lkm_v);	/* thundering herd "problem" here */

	return(0);		/* pseudo-device closed */
}

/*ARGSUSED*/
static	int
lkmcioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int err = 0;
	int i;
	struct lmc_resrv *resrvp;
	struct lmc_loadbuf *loadbufp;
	struct lmc_unload *unloadp;
	struct lmc_stat	 *statp;
	char istr[MAXLKMNAME];

	switch(cmd) {
	case LMRESERV:		/* reserve pages for a module */
		if ((flag & FWRITE) == 0 || securelevel > 0) 
			/* only allow this if writing and insecure */
			return EPERM;

		resrvp = (struct lmc_resrv *)data;

		/*
		 * Find a free slot.
		 */
		for (i = 0; i < MAXLKMS; i++)
			if (!lkmods[i].used)
				break;
		if (i == MAXLKMS) {
			err = ENOMEM;		/* no slots available */
			break;
		}
		curp = &lkmods[i];
		curp->id = i;		/* self reference slot offset */

		resrvp->slot = i;		/* return slot */

		/*
		 * Get memory for module
		 */
		curp->size = resrvp->size;

		curp->area = kmem_alloc(kernel_map, curp->size);/**/

		curp->offset = 0;		/* load offset */

		resrvp->addr = curp->area; /* ret kernel addr */

#ifdef DEBUG
		printf("LKM: LMRESERV (actual   = 0x%08x)\n", curp->area);
		printf("LKM: LMRESERV (adjusted = 0x%08x)\n",
			trunc_page(curp->area));
#endif	/* DEBUG */
		lkm_state = LKMS_RESERVED;
		break;

	case LMLOADBUF:		/* Copy in; stateful, follows LMRESERV */
		if ((flag & FWRITE) == 0 || securelevel > 0)
			/* only allow this if writing and insecure */
			return EPERM;

		loadbufp = (struct lmc_loadbuf *)data;
		i = loadbufp->cnt;
		if ((lkm_state != LKMS_RESERVED && lkm_state != LKMS_LOADING)
		    || i < 0
		    || i > MODIOBUF
		    || i > curp->size - curp->offset) {
			err = ENOMEM;
			break;
		}

		/* copy in buffer full of data */
		err = copyin((caddr_t)loadbufp->data,
		    (caddr_t)curp->area + curp->offset, i);
		if (err)
			break;

		if ((curp->offset + i) < curp->size) {
			lkm_state = LKMS_LOADING;
#ifdef DEBUG
			printf("LKM: LMLOADBUF (loading @ %d of %d, i = %d)\n",
			curp->offset, curp->size, i);
#endif	/* DEBUG */
		} else {
			lkm_state = LKMS_LOADED;
#ifdef DEBUG
			printf("LKM: LMLOADBUF (loaded)\n");
#endif	/* DEBUG */
		}
		curp->offset += i;
		break;

	case LMUNRESRV:		/* discard reserved pages for a module */
		if ((flag & FWRITE) == 0 || securelevel > 0)
			/* only allow this if writing and insecure */
			return EPERM;

		lkmunreserve();	/* coerce state to LKM_IDLE */
#ifdef DEBUG
		printf("LKM: LMUNRESERV\n");
#endif	/* DEBUG */
		break;

	case LMREADY:		/* module loaded: call entry */
		if ((flag & FWRITE) == 0 || securelevel > 0)
			/* only allow this if writing or insecure */
			return EPERM;

		switch (lkm_state) {
		case LKMS_LOADED:
			break;
		case LKMS_LOADING:
			/* The remainder must be bss, so we clear it */
			bzero((caddr_t)curp->area + curp->offset,
			      curp->size - curp->offset);
			break;
		default:

#ifdef DEBUG
			printf("lkm_state is %02x\n", lkm_state);
#endif	/* DEBUG */
			return ENXIO;
		}

		/* XXX gack */
		curp->entry = (int (*) __P((struct lkm_table *, int, int)))
			      (*((int *)data));

		/* call entry(load)... (assigns "private" portion) */
		err = (*(curp->entry))(curp, LKM_E_LOAD, LKM_VERSION);
		if (err) {
			/*
			 * Module may refuse loading or may have a
			 * version mismatch...
			 */
			lkm_state = LKMS_UNLOADING;	/* for lkmunreserve */
			lkmunreserve();			/* free memory */
			curp->used = 0;			/* free slot */
			break;
		}
		/*
		 * It's possible for a user to load a module that doesn't
		 * initialize itself correctly. (You can even get away with
		 * using it for a while.) Unfortunately, we are faced with
		 * the following problems:
		 * - we can't tell a good module from a bad one until
		 *   after we've run its entry function (if the private
		 *   section is uninitalized after we return from the
		 *   entry, then something's fishy)
		 * - now that we've called the entry function, we can't
		 *   forcibly unload the module without risking a crash
		 * - since we don't know what the module's entry function
		 *   did, we can't easily clean up the mess it may have
		 *   made, so we can't know just how unstable the system
		 *   may be
		 * So, being stuck between a rock and a hard place, we
		 * have no choice but to do this...
		 */
		if (curp->private.lkm_any == NULL)
			panic("loadable module initialization failed");

		curp->used = 1;
#ifdef DEBUG
		printf("LKM: LMREADY\n");
#endif	/* DEBUG */
		lkm_state = LKMS_IDLE;
		break;

	case LMUNLOAD:		/* unload a module */
		if ((flag & FWRITE) == 0 || securelevel > 0)
			/* only allow this if writing and insecure */
			return EPERM;

		unloadp = (struct lmc_unload *)data;

		if ((i = unloadp->id) == -1) {		/* unload by name */
			/*
			 * Copy name and lookup id from all loaded
			 * modules.  May fail.
			 */
		 	err =copyinstr(unloadp->name, istr, MAXLKMNAME-1, NULL);
		 	if (err)
				break;

			/*
			 * look up id...
			 */
			for (i = 0; i < MAXLKMS; i++) {
				if (!lkmods[i].used)
					continue;
				if (!strcmp(istr,
				        lkmods[i].private.lkm_any->lkm_name))
					break;
			}
		}

		/*
		 * Range check the value; on failure, return EINVAL
		 */
		if (i < 0 || i >= MAXLKMS) {
			err = EINVAL;
			break;
		}

		curp = &lkmods[i];

		if (!curp->used) {
			err = ENOENT;
			break;
		}

		/* call entry(unload) */
		if ((*(curp->entry))(curp, LKM_E_UNLOAD, LKM_VERSION)) {
			err = EBUSY;
			break;
		}

		lkm_state = LKMS_UNLOADING;	/* non-idle for lkmunreserve */
		lkmunreserve();			/* free memory */
		curp->used = 0;			/* free slot */
		break;

	case LMSTAT:		/* stat a module by id/name */
		/* allow readers and writers to stat */

		statp = (struct lmc_stat *)data;

		if ((i = statp->id) == -1) {		/* stat by name */
			/*
			 * Copy name and lookup id from all loaded
			 * modules.
			 */
		 	copystr(statp->name, istr, MAXLKMNAME-1, NULL);
			/*
			 * look up id...
			 */
			for (i = 0; i < MAXLKMS; i++) {
				if (!lkmods[i].used)
					continue;
				if (!strcmp(istr,
				        lkmods[i].private.lkm_any->lkm_name))
					break;
			}

			if (i == MAXLKMS) {		/* Not found */
				err = ENOENT;
				break;
			}
		}

		/*
		 * Range check the value; on failure, return EINVAL
		 */
		if (i < 0 || i >= MAXLKMS) {
			err = EINVAL;
			break;
		}

		curp = &lkmods[i];

		if (!curp->used) {			/* Not found */
			err = ENOENT;
			break;
		}

		/*
		 * Copy out stat information for this module...
		 */
		statp->id	= curp->id;
		statp->offset	= curp->private.lkm_any->lkm_offset;
		statp->type	= curp->private.lkm_any->lkm_type;
		statp->area	= curp->area;
		statp->size	= curp->size / PAGESIZE;
		statp->private	= (unsigned long)curp->private.lkm_any;
		statp->ver	= curp->private.lkm_any->lkm_ver;
		copystr(curp->private.lkm_any->lkm_name,
			  statp->name,
			  MAXLKMNAME - 2,
			  NULL);

		break;

	default:		/* bad ioctl()... */
		err = ENOTTY;
		break;
	}

	return (err);
}

/*
 * Acts like "nosys" but can be identified in sysent for dynamic call
 * number assignment for a limited number of calls.
 *
 * Place holder for system call slots reserved for loadable modules.
 */
int
lkmnosys(p, args, retval)
	struct proc *p;
	struct nosys_args *args;
	int *retval;
{

	return(nosys(p, args, retval));
}

int
lkmexists(lkmtp)
	struct lkm_table *lkmtp;
{
	int i;

	/*
	 * see if name exists...
	 */
	for (i = 0; i < MAXLKMS; i++) {
		/*
		 * An unused module and the one we are testing are not
		 * considered.
		 */
		if (!lkmods[i].used || &lkmods[i] == lkmtp)
			continue;
		if (!strcmp(lkmtp->private.lkm_any->lkm_name,
			lkmods[i].private.lkm_any->lkm_name))
			return(1);		/* already loaded... */
	}

	return(0);		/* module not loaded... */
}

/*
 * For the loadable system call described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_syscall(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	struct lkm_syscall *args = lkmtp->private.lkm_syscall;
	int i;
	int err = 0;

	switch(cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return(EEXIST);
		if ((i = args->lkm_offset) == -1) {	/* auto */
			/*
			 * Search the table looking for a slot...
			 */
			for (i = 0; i < aout_sysvec.sv_size; i++)
				if (aout_sysvec.sv_table[i].sy_call ==
				    (sy_call_t *)lkmnosys)
					break;		/* found it! */
			/* out of allocable slots? */
			if (i == aout_sysvec.sv_size) {
				err = ENFILE;
				break;
			}
		} else {				/* assign */
			if (i < 0 || i >= aout_sysvec.sv_size) {
				err = EINVAL;
				break;
			}
		}

		/* save old */
		bcopy(&aout_sysvec.sv_table[i],
		      &(args->lkm_oldent),
		      sizeof(struct sysent));

		/* replace with new */
		bcopy(args->lkm_sysent,
		      &aout_sysvec.sv_table[i],
		      sizeof(struct sysent));

		/* done! */
		args->lkm_offset = i;	/* slot in sysent[] */

		break;

	case LKM_E_UNLOAD:
		/* current slot... */
		i = args->lkm_offset;

		/* replace current slot contents with old contents */
		bcopy(&(args->lkm_oldent),
		      &aout_sysvec.sv_table[i],
		      sizeof(struct sysent));

		break;

	case LKM_E_STAT:	/* no special handling... */
		break;
	}

	return(err);
}

/*
 * For the loadable virtual file system described by the structure pointed
 * to by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_vfs(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	struct lkm_vfs *args = lkmtp->private.lkm_vfs;
	struct vfsconf *vfc = args->lkm_vfsconf;
	int i;
	int err = 0;

	switch(cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return(EEXIST);

		for(i = 0; i < MOUNT_MAXTYPE; i++) {
			if(!strcmp(vfc->vfc_name, vfsconf[i]->vfc_name)) {
				return EEXIST;
			}
		}

		i = args->lkm_offset = vfc->vfc_index;
		if (i < 0) {
			for (i = MOUNT_MAXTYPE - 1; i >= 0; i--) {
				if(vfsconf[i] == &void_vfsconf)
					break;
			}
		}
		if (i < 0) {
			return EINVAL;
		}
		args->lkm_offset = vfc->vfc_index = i;

		vfsconf[i] = vfc;
		vfssw[i] = vfc->vfc_vfsops;

		/* like in vfs_op_init */
		for(i = 0; args->lkm_vnodeops->ls_items[i]; i++) {
			const struct vnodeopv_desc *opv =
				args->lkm_vnodeops->ls_items[i];
			*(opv->opv_desc_vector_p) = NULL;
		}
		vfs_opv_init((struct vnodeopv_desc **)args->lkm_vnodeops->ls_items);

		/*
		 * Call init function for this VFS...
		 */
	 	(*(vfssw[vfc->vfc_index]->vfs_init))();

		/* done! */
		break;

	case LKM_E_UNLOAD:
		/* current slot... */
		i = args->lkm_offset;

		if (vfsconf[i]->vfc_refcount) {
			return EBUSY;
		}

		/* replace current slot contents with old contents */
		vfssw[i] = (struct vfsops *)0;
		vfsconf[i] = &void_vfsconf;

		break;

	case LKM_E_STAT:	/* no special handling... */
		break;
	}
	return(err);
}

/*
 * For the loadable device driver described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_dev(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	struct lkm_dev *args = lkmtp->private.lkm_dev;
	int i;
	dev_t descrip;
	int err = 0;

	switch(cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return(EEXIST);
		switch(args->lkm_devtype) {
		case LM_DT_BLOCK:
			if ((i = args->lkm_offset) == -1)
				descrip = (dev_t) -1;
			else
				descrip = makedev(args->lkm_offset,0);
			if ( err = bdevsw_add(&descrip, args->lkm_dev.bdev,
					&(args->lkm_olddev.bdev))) {
				break;
			}
			args->lkm_offset = major(descrip) ;
			break;

		case LM_DT_CHAR:
			break;

		default:
			err = ENODEV;
			break;
		}
		break;

	case LKM_E_UNLOAD:
		/* current slot... */
		i = args->lkm_offset;

		switch(args->lkm_devtype) {
		case LM_DT_BLOCK:
			/* replace current slot contents with old contents */
			descrip = makedev(i,0);
			bdevsw_add(&descrip, args->lkm_olddev.bdev,NULL);
			break;

		case LM_DT_CHAR:
			/* replace current slot contents with old contents */
			cdevsw_add(&descrip, args->lkm_olddev.cdev,NULL);
			break;

		default:
			err = ENODEV;
			break;
		}
		break;

	case LKM_E_STAT:	/* no special handling... */
		break;
	}

	return(err);
}

#ifdef STREAMS
/*
 * For the loadable streams module described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_strmod(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	struct lkm_strmod *args = lkmtp->private.lkm_strmod;
	int i;
	int err = 0;

	switch(cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return(EEXIST);
		break;

	case LKM_E_UNLOAD:
		break;

	case LKM_E_STAT:	/* no special handling... */
		break;
	}

	return(err);
}
#endif	/* STREAMS */

/*
 * For the loadable execution class described by the structure pointed to
 * by lkmtp, load/unload/stat it depending on the cmd requested.
 */
static int
_lkm_exec(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	struct lkm_exec *args = lkmtp->private.lkm_exec;
	int i;
	int err = 0;
	const struct execsw **execsw =
		(const struct execsw **)&execsw_set.ls_items[0];

	switch(cmd) {
	case LKM_E_LOAD:
		/* don't load twice! */
		if (lkmexists(lkmtp))
			return(EEXIST);
		if ((i = args->lkm_offset) == -1) {	/* auto */
			/*
			 * Search the table looking for a slot...
			 */
			for (i = 0; execsw[i] != NULL; i++)
				if (execsw[i]->ex_imgact == NULL)
					break;		/* found it! */
			/* out of allocable slots? */
			if (execsw[i] == NULL) {
				err = ENFILE;
				break;
			}
		} else {				/* assign */
			err = EINVAL;
			break;
		}

		/* save old */
		bcopy(&execsw[i], &(args->lkm_oldexec), sizeof(struct execsw*));

		/* replace with new */
		bcopy(&(args->lkm_exec), &execsw[i], sizeof(struct execsw*));

		/* done! */
		args->lkm_offset = i;	/* slot in execsw[] */

		break;

	case LKM_E_UNLOAD:
		/* current slot... */
		i = args->lkm_offset;

		/* replace current slot contents with old contents */
		bcopy(&(args->lkm_oldexec), &execsw[i], sizeof(struct execsw*));

		break;

	case LKM_E_STAT:	/* no special handling... */
		break;
	}
	return(err);
}

/* XXX: This is bogus.  we should find a better method RSN! */
static const struct execsw lkm_exec_dummy1 = { NULL, "lkm" };
static const struct execsw lkm_exec_dummy2 = { NULL, "lkm" };
static const struct execsw lkm_exec_dummy3 = { NULL, "lkm" };
static const struct execsw lkm_exec_dummy4 = { NULL, "lkm" };
TEXT_SET(execsw_set, lkm_exec_dummy1);
TEXT_SET(execsw_set, lkm_exec_dummy2);
TEXT_SET(execsw_set, lkm_exec_dummy3);
TEXT_SET(execsw_set, lkm_exec_dummy4);

/*
 * This code handles the per-module type "wiring-in" of loadable modules
 * into existing kernel tables.  For "LM_MISC" modules, wiring and unwiring
 * is assumed to be done in their entry routines internal to the module
 * itself.
 */
int
lkmdispatch(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	int err = 0;		/* default = success */

	switch(lkmtp->private.lkm_any->lkm_type) {
	case LM_SYSCALL:
		err = _lkm_syscall(lkmtp, cmd);
		break;

	case LM_VFS:
		err = _lkm_vfs(lkmtp, cmd);
		break;

	case LM_DEV:
		err = _lkm_dev(lkmtp, cmd);
		break;

#ifdef STREAMS
	case LM_STRMOD:
	    {
		struct lkm_strmod *args = lkmtp->private.lkm_strmod;
	    }
		break;

#endif	/* STREAMS */

	case LM_EXEC:
		err = _lkm_exec(lkmtp, cmd);
		break;

	case LM_MISC:	/* ignore content -- no "misc-specific" procedure */
		if (lkmexists(lkmtp))
			err = EEXIST;
		break;

	default:
		err = ENXIO;	/* unknown type */
		break;
	}

	return(err);
}

int
lkm_nullcmd(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{

	return (0);
}

static lkm_devsw_installed = 0;
#ifdef DEVFS
static void	*lkmc_devfs_token;
#endif

static void 	lkm_drvinit(void *unused)
{
	dev_t dev;

	if( ! lkm_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&lkmc_cdevsw, NULL);
		lkm_devsw_installed = 1;
#ifdef DEVFS
		lkmc_devfs_token = devfs_add_devswf(&lkmc_cdevsw, 0, DV_CHR,
						    UID_ROOT, GID_WHEEL, 0644,
						    "lkm");
#endif
    	}
}

SYSINIT(lkmdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,lkm_drvinit,NULL)


