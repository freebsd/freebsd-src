/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include "kernel_interface.h"
#include "kernel_mediate.h"
#include "kernel_plm.h"
#include "kernel_util.h"
#include "kernel_pipe.h"
#include "kernel_socket.h"
#include "lomacfs.h"
#include "lomacio.h"

static d_ioctl_t lomac_ioctl;

#define	CDEV_MAJOR	207
#define	LOMAC_MINOR	0

static struct cdevsw lomac_cdevsw = {
	/* open */	(d_open_t *)nullop,
	/* close */	(d_open_t *)nullop,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	lomac_ioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"lomac",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static dev_t lomac_dev = NULL;

int
lomac_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct thread *td) {
	struct nameidata nd;
	struct proc *p;
	struct proc *targp;
	struct lomac_fioctl *fio;
	lomac_object_t lobj;
	lattr_t lattr;
	int error;

	p = td->td_proc;
	switch (cmd) {
	case LIOGETPLEVEL:
		targp = pfind(*(int *)data);
		if (targp == NULL)
			return (ESRCH);
		if (p_cansee(p, targp) != 0) {
			PROC_UNLOCK(targp);
			return (ESRCH);
		}
		get_subject_lattr(targp, &lattr);
		*(level_t *)data = lattr.level;
		PROC_UNLOCK(targp);
		return (0);
	case LIOGETFLEVEL:
		fio = (struct lomac_fioctl *)data;
		NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | NOOBJ, UIO_SYSSPACE,
		    fio->path, td);
		if ((error = namei(&nd)) != 0)
			return (error);
		if (VISLOMAC(nd.ni_vp))
			lobj.lo_type = LO_TYPE_LVNODE;
		else
			lobj.lo_type = LO_TYPE_UVNODE;
		lobj.lo_object.vnode = nd.ni_vp;
		get_object_lattr(&lobj, &lattr);
		*(level_t *)&fio->level = lattr.level;
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_vp);
		return (error);
	case LIOGETFLATTR:
		fio = (struct lomac_fioctl *)data;
		NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | NOOBJ, UIO_SYSSPACE,
		    fio->path, td);
		if ((error = namei(&nd)) != 0)
			return (error);
		if (VISLOMAC(nd.ni_vp))
			lobj.lo_type = LO_TYPE_LVNODE;
		else
			lobj.lo_type = LO_TYPE_UVNODE;
		lobj.lo_object.vnode = nd.ni_vp;
		get_object_lattr(&lobj, (lattr_t *)&fio->level);
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_vp);
		return (error);
	case LIOPMAKELOWLEVEL:
		lattr.level = LOMAC_LOWEST_LEVEL;
		lattr.flags = 0;
		set_subject_lattr(p, lattr);
		return (0);
	default:
		return (ENOTTY);
	}
}

int (*old_execve)(struct proc *, void *);
#ifdef __i386__
int (*old_sysarch)(struct proc *, void *);
#endif

/*
 * This is "borrowed" from kern_module.c and MUST be kept in synch!
 */
struct module {
    TAILQ_ENTRY(module)	link;		/* chain together all modules */
    TAILQ_ENTRY(module)	flink;		/* all modules in a file */
    struct linker_file*	file;		/* file which contains this module */
    int			refs;		/* reference count */
    int			id;		/* unique id number */
    char		*name;		/* module name */
    modeventhand_t	handler;	/* event handler */
    void		*arg;		/* argument for handler */
    modspecific_t	data;		/* module specific data */
};

static int
lomac_modevent(module_t module, int event, void *unused) {
	static int initialized_procs = 0;
	static int initialized_syscalls = 0;
	static int initialized_pipes = 0;
	static int initialized_sockets = 0;
	static int initialized_vm = 0;
	static linker_file_t kernlf;
	int error;

	switch ((enum modeventtype)event) {
	case MOD_LOAD:
		if (!lomac_plm_initialized)
			return (EINVAL);
		kernlf = linker_kernel_file;
		old_execve =
		    (int (*)(struct proc *, void *))linker_file_lookup_symbol(
			kernlf, "execve", 1);
		if (old_execve == NULL)
			return (ENOENT);
#ifdef __i386__
		old_sysarch =
		    (int (*)(struct proc *, void *))linker_file_lookup_symbol(
			kernlf, "sysarch", 1);
		if (old_sysarch == NULL)
			return (ENOENT);
#endif
		error = lomac_initialize_procs();
		if (error)
			break;
		initialized_procs = 1;
		error = lomac_initialize_syscalls();
		if (error)
			break;
		initialized_syscalls = 1;
		error = lomac_initialize_pipes();
		if (error)
			break;
		initialized_pipes = 1;
		error = lomac_initialize_sockets();
		if (error)
			break;
		initialized_sockets = 1;
		lomac_dev = make_dev(&lomac_cdevsw, LOMAC_MINOR, UID_ROOT,
		    GID_WHEEL, 0666, "lomac");
		linker_kernel_file = module->file;
		error = vfs_mount(curthread, "lomacfs", "/", 0, NULL);
		if (error)
			return (error);
		error = lomac_initialize_cwds();
		if (error)
			return (error);
		printf("LOMAC: Low-Watermark Mandatory Access Control v2.0.0\n");
		return (error);
	case MOD_UNLOAD:
		/*
		 * It's always a bad idea to let a low-security process
		 * unload the module providing security.
		 */
		if (initialized_procs &&
		    !mediate_subject_at_level("kldunload", curthread->td_proc,
		        LOMAC_HIGHEST_LEVEL))
			return (EPERM);
		/*
		 * Unloading doesn't work well at the moment...
		 */
		return (EPERM);
		if (initialized_sockets) {
			error = lomac_uninitialize_sockets();
			if (error)
				break;
			initialized_sockets = 0;
		}
		if (initialized_pipes) {
			error = lomac_uninitialize_pipes();
			if (error)
				break;
			initialized_pipes = 0;
		}
		if (initialized_syscalls) {
			error = lomac_uninitialize_syscalls();
			if (error)
				break;
			initialized_syscalls = 0;
		}
		if (initialized_procs) {
			error = lomac_uninitialize_procs();
			if (error)
				break;
			initialized_procs = 0;
		}
		if (initialized_vm) {
			error = lomac_uninitialize_vm();
			if (error)
				break;
			initialized_vm = 0;
		}
		if (lomac_dev) {
			if (count_dev(lomac_dev) != 0)
				return (EBUSY);
			destroy_dev(lomac_dev);
		}
		printf("LOMAC: unloading\n");
		linker_kernel_file = kernlf;
		break;
	case MOD_SHUTDOWN:
		break;
	}
	return (0);
}

static moduledata_t lomac_moduledata = {
	"lomac",
	&lomac_modevent,
	NULL
};
DECLARE_MODULE(lomac, lomac_moduledata, SI_SUB_VFS, SI_ORDER_ANY);
MODULE_VERSION(lomac, 1);
MODULE_DEPEND(lomac, syscall_gate, 1, 1, 1);
MODULE_DEPEND(lomac, lomacfs, 1, 1, 1);
MODULE_DEPEND(lomac, lomac_plm, 1, 1, 1);
