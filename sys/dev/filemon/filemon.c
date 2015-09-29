/*-
 * Copyright (c) 2011, David E. O'Brien.
 * Copyright (c) 2009-2011, Juniper Networks, Inc.
 * Copyright (c) 2015, EMC Corp.
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
 * THIS SOFTWARE IS PROVIDED BY JUNIPER NETWORKS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL JUNIPER NETWORKS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/file.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sx.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/uio.h>

#if __FreeBSD_version >= 900041
#include <sys/capability.h>
#endif

#include "filemon.h"

#if defined(COMPAT_IA32) || defined(COMPAT_FREEBSD32) || defined(COMPAT_ARCH32)
#include <compat/freebsd32/freebsd32_syscall.h>
#include <compat/freebsd32/freebsd32_proto.h>

extern struct sysentvec ia32_freebsd_sysvec;
#endif

extern struct sysentvec elf32_freebsd_sysvec;
extern struct sysentvec elf64_freebsd_sysvec;

static d_close_t	filemon_close;
static d_ioctl_t	filemon_ioctl;
static d_open_t		filemon_open;
static int		filemon_unload(void);
static void		filemon_load(void *);

static struct cdevsw filemon_cdevsw = {
	.d_version	= D_VERSION,
	.d_close	= filemon_close,
	.d_ioctl	= filemon_ioctl,
	.d_open		= filemon_open,
	.d_name		= "filemon",
};

MALLOC_DECLARE(M_FILEMON);
MALLOC_DEFINE(M_FILEMON, "filemon", "File access monitor");

struct filemon {
	TAILQ_ENTRY(filemon) link;	/* Link into the in-use list. */
	struct sx	lock;		/* Lock mutex for this filemon. */
	struct file	*fp;		/* Output file pointer. */
	pid_t		pid;		/* The process ID being monitored. */
	char		fname1[MAXPATHLEN]; /* Temporary filename buffer. */
	char		fname2[MAXPATHLEN]; /* Temporary filename buffer. */
	char		msgbufr[1024];	/* Output message buffer. */
};

static TAILQ_HEAD(, filemon) filemons_inuse = TAILQ_HEAD_INITIALIZER(filemons_inuse);
static TAILQ_HEAD(, filemon) filemons_free = TAILQ_HEAD_INITIALIZER(filemons_free);
static struct sx access_lock;

#if __FreeBSD_version < 701000
static struct clonedevs *filemon_clones;
static eventhandler_tag	eh_tag;
#else
static struct cdev *filemon_dev;
#endif

#include "filemon_lock.c"
#include "filemon_wrapper.c"

#if __FreeBSD_version < 701000
static void
filemon_clone(void *arg, struct ucred *cred, char *name, int namelen,
    struct cdev **dev)
{
	int u = -1;
	size_t len;

	if (*dev != NULL)
		return;

	len = strlen(name);

	if (len != 7)
		return;

	if (bcmp(name,"filemon", 7) != 0)
		return;

	/* Clone the device to the new minor number. */
	if (clone_create(&filemon_clones, &filemon_cdevsw, &u, dev, 0) != 0)
		/* Create the /dev/filemonNN entry. */
		*dev = make_dev_cred(&filemon_cdevsw, u, cred, UID_ROOT,
		    GID_WHEEL, 0666, "filemon%d", u);
	if (*dev != NULL) {
		dev_ref(*dev);
		(*dev)->si_flags |= SI_CHEAPCLONE;
	}
}
#endif

static void
filemon_dtr(void *data)
{
	struct filemon *filemon = data;

	if (filemon != NULL) {
		struct file *fp = filemon->fp;

		/* Get exclusive write access. */
		filemon_lock_write();

		/* Remove from the in-use list. */
		TAILQ_REMOVE(&filemons_inuse, filemon, link);

		filemon->fp = NULL;
		filemon->pid = -1;

		/* Add to the free list. */
		TAILQ_INSERT_TAIL(&filemons_free, filemon, link);

		/* Give up write access. */
		filemon_unlock_write();

		if (fp != NULL)
			fdrop(fp, curthread);
	}
}

static int
filemon_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag __unused,
    struct thread *td)
{
	int error = 0;
	struct filemon *filemon;
	struct proc *p;

#if __FreeBSD_version < 701000
	filemon = dev->si_drv1;
#else
	devfs_get_cdevpriv((void **) &filemon);
#endif

	switch (cmd) {
	/* Set the output file descriptor. */
	case FILEMON_SET_FD:
#if __FreeBSD_version < 900041
#define FGET_WRITE(a1, a2, a3) fget_write((a1), (a2), (a3))
#else
#define FGET_WRITE(a1, a2, a3) fget_write((a1), (a2), CAP_WRITE | CAP_SEEK, (a3))
#endif
		if ((error = FGET_WRITE(td, *(int *)data, &filemon->fp)) == 0)
			/* Write the file header. */
			filemon_comment(filemon);
		break;

	/* Set the monitored process ID. */
	case FILEMON_SET_PID:
		error = pget(*((pid_t *)data), PGET_CANDEBUG | PGET_NOTWEXIT,
		    &p);
		if (error == 0) {
			filemon->pid = p->p_pid;
			PROC_UNLOCK(p);
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static int
filemon_open(struct cdev *dev, int oflags __unused, int devtype __unused,
    struct thread *td __unused)
{
	struct filemon *filemon;

	/* Get exclusive write access. */
	filemon_lock_write();

	if ((filemon = TAILQ_FIRST(&filemons_free)) != NULL)
		TAILQ_REMOVE(&filemons_free, filemon, link);

	/* Give up write access. */
	filemon_unlock_write();

	if (filemon == NULL) {
		filemon = malloc(sizeof(struct filemon), M_FILEMON,
		    M_WAITOK | M_ZERO);

		filemon->fp = NULL;

		sx_init(&filemon->lock, "filemon");
	}

	filemon->pid = curproc->p_pid;

#if __FreeBSD_version < 701000
	dev->si_drv1 = filemon;
#else
	devfs_set_cdevpriv(filemon, filemon_dtr);
#endif

	/* Get exclusive write access. */
	filemon_lock_write();

	/* Add to the in-use list. */
	TAILQ_INSERT_TAIL(&filemons_inuse, filemon, link);

	/* Give up write access. */
	filemon_unlock_write();

	return (0);
}

static int
filemon_close(struct cdev *dev __unused, int flag __unused, int fmt __unused,
    struct thread *td __unused)
{
#if __FreeBSD_version < 701000
	filemon_dtr(dev->si_drv1);

	dev->si_drv1 = NULL;

	/* Schedule this cloned device to be destroyed. */
	destroy_dev_sched(dev);
#endif

	return (0);
}

static void
filemon_load(void *dummy __unused)
{
	sx_init(&access_lock, "filemons_inuse");

	/* Install the syscall wrappers. */
	filemon_wrapper_install();

#if __FreeBSD_version < 701000
	/* Enable device cloning. */
	clone_setup(&filemon_clones);

	/* Setup device cloning events. */
	eh_tag = EVENTHANDLER_REGISTER(dev_clone, filemon_clone, 0, 1000);
#else
	filemon_dev = make_dev(&filemon_cdevsw, 0, UID_ROOT, GID_WHEEL, 0666,
	    "filemon");
#endif
}

static int
filemon_unload(void)
{
 	struct filemon *filemon;
	int error = 0;

	/* Get exclusive write access. */
	filemon_lock_write();

	if (TAILQ_FIRST(&filemons_inuse) != NULL)
		error = EBUSY;
	else {
#if __FreeBSD_version >= 701000
		destroy_dev(filemon_dev);
#endif

		/* Deinstall the syscall wrappers. */
		filemon_wrapper_deinstall();
	}

	/* Give up write access. */
	filemon_unlock_write();

	if (error == 0) {
#if __FreeBSD_version < 701000
		/*
		 * Check if there is still an event handler callback registered.
		*/
		if (eh_tag != 0) {
			/* De-register the device cloning event handler. */
			EVENTHANDLER_DEREGISTER(dev_clone, eh_tag);
			eh_tag = 0;

			/* Stop device cloning. */
			clone_cleanup(&filemon_clones);
		}
#endif
		/* free() filemon structs free list. */
		filemon_lock_write();
		while ((filemon = TAILQ_FIRST(&filemons_free)) != NULL) {
			TAILQ_REMOVE(&filemons_free, filemon, link);
			sx_destroy(&filemon->lock);
			free(filemon, M_FILEMON);
		}
		filemon_unlock_write();

		sx_destroy(&access_lock);
	}

	return (error);
}

static int
filemon_modevent(module_t mod __unused, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		filemon_load(data);
		break;

	case MOD_UNLOAD:
		error = filemon_unload();
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}

	return (error);
}

DEV_MODULE(filemon, filemon_modevent, NULL);
MODULE_VERSION(filemon, 1);
