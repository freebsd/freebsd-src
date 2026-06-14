/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <dev/ntsync/ntsyncvar.h>

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <compat/linux/linux_common.h>
#include <compat/linux/linux_ioctl.h>
#include <dev/ntsync/linux_ntsync.h>

MODULE_DEPEND(linux_ntsync, linux, 1, 1, 1);
MODULE_DEPEND(linux_ntsync, ntsync, 1, 1, 1);

static linux_ioctl_function_t linux_ntsync_ioctl;
static struct linux_ioctl_handler linux_ntsync_handler = {linux_ntsync_ioctl,
    LNTSYNC_IOCTL_MIN, LNTSYNC_IOCTL_MAX};

static int
linux_ntsync_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error;

	error = 0;
	switch (type) {
	case MOD_LOAD:
		error = linux_ioctl_register_handler(&linux_ntsync_handler);
		if (error != 0) {
			printf("linux_ntsync: cannot register ioctl handler, "
			    "error %d\n", error);
		} else if (bootverbose)
			printf("linux_ntsync\n");
		break;

	case MOD_UNLOAD:
		linux_ioctl_unregister_handler(&linux_ntsync_handler);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
	}

	return (error);
}

DEV_MODULE(linux_ntsync, linux_ntsync_modevent, NULL);
MODULE_VERSION(linux_ntsync, 1);

/* XXXKIB no translation of structs */
static void
ntsync_lsa_to_sa(struct ntsync_sem_args *sa,
    const struct linux_ntsync_sem_args *lsa)
{
	memcpy(sa, lsa, sizeof(*sa));
}

static void
ntsync_sa_to_lsa(struct linux_ntsync_sem_args *lsa,
    const struct ntsync_sem_args *sa)
{
	memcpy(lsa, sa, sizeof(*lsa));
}

static void
ntsync_lma_to_ma(struct ntsync_mutex_args *ma,
    const struct linux_ntsync_mutex_args *lma)
{
	memcpy(ma, lma, sizeof(*ma));
}

static void
ntsync_ma_to_lma(struct linux_ntsync_mutex_args *ma,
    const struct ntsync_mutex_args *lma)
{
	memcpy(ma, lma, sizeof(*ma));
}

static void
ntsync_lea_to_ea(struct ntsync_event_args *ea,
    const struct linux_ntsync_event_args *lea)
{
	memcpy(ea, lea, sizeof(*ea));
}

static void
ntsync_ea_to_lea(struct linux_ntsync_event_args *lea,
    const struct ntsync_event_args *ea)
{
	memcpy(lea, ea, sizeof(*lea));
}

static void
ntsync_lwa_to_wa(struct ntsync_wait_args *wa,
    const struct linux_ntsync_wait_args *lwa)
{
	memcpy(wa, lwa, sizeof(*wa));
}

static void
ntsync_wa_to_lwa(struct linux_ntsync_wait_args *lwa,
    const struct ntsync_wait_args *wa)
{
	memcpy(lwa, wa, sizeof(*lwa));
}

static int
linux_ntsync_cdev_ioctl(struct thread *td, u_long cmd, void *data,
    struct file *fp)
{
	struct cdev *dev;
	struct cdevsw *dsw;
	struct vnode *vp;
	struct file *fpop;
	int error, ref;

	if (fp->f_type != DTYPE_VNODE)
		return (error = ENOIOCTL);

	vp = fp->f_vnode;
	if (vp->v_type != VCHR)
		return (ENOIOCTL);
	dev = vp->v_rdev;
	dsw = dev_refthread(dev, &ref);
	if (dsw == NULL)
		return (ENXIO);
	if (dsw != &ntsync_cdevsw) {
		error = ENOIOCTL;
	} else {
		fpop = td->td_fpop;
		td->td_fpop = fp;
		error = dsw->d_ioctl(dev, cmd, data, 0, td);
		td->td_fpop = fpop;
	}
	dev_relthread(dev, ref);
	return (error);
}

static int
linux_ntsync_ioctl(struct thread *td, struct linux_ioctl_args *args)
{
	struct file *fp;
	void *data;
	struct linux_ntsync_sem_args lsa;
	struct linux_ntsync_mutex_args lma;
	struct linux_ntsync_event_args lea;
	struct linux_ntsync_wait_args lwa;
	struct ntsync_sem_args sa;
	struct ntsync_mutex_args ma;
	struct ntsync_event_args ea;
	struct ntsync_wait_args wa;
	uint32_t val;
	int error, error1, lcmd;
	bool doco;

	lcmd = args->cmd;
	data = (void *)args->arg;

	error = fget_cap(td, args->fd, &cap_no_rights, NULL, &fp, NULL);
	if (error != 0)
		goto out;

	doco = false;
	switch (lcmd) {
	case LNTSYNC_IOC_CREATE_SEM:
		error = copyin(data, &lsa, sizeof(lsa));
		ntsync_lsa_to_sa(&sa, &lsa);
		if (error == 0) {
			error = linux_ntsync_cdev_ioctl(td,
			    NTSYNC_IOC_CREATE_SEM, &sa, fp);
		}
		break;
	case LNTSYNC_IOC_CREATE_MUTEX:
		error = copyin(data, &lma, sizeof(lma));
		ntsync_lma_to_ma(&ma, &lma);
		if (error == 0) {
			error = linux_ntsync_cdev_ioctl(td,
			    NTSYNC_IOC_CREATE_MUTEX, &ma, fp);
		}
		break;
	case LNTSYNC_IOC_CREATE_EVENT:
		error = copyin(data, &lea, sizeof(lea));
		ntsync_lea_to_ea(&ea, &lea);
		if (error == 0) {
			error = linux_ntsync_cdev_ioctl(td,
			    NTSYNC_IOC_CREATE_EVENT, &ea, fp);
		}
		break;
	case LNTSYNC_IOC_WAIT_ANY:
		error = copyin(data, &lwa, sizeof(lwa));
		ntsync_lwa_to_wa(&wa, &lwa);
		if (error == 0) {
			error = linux_ntsync_cdev_ioctl(td,
			    NTSYNC_IOC_WAIT_ANY, &wa, fp);
			if (error == 0 || error == EOWNERDEAD) {
				ntsync_wa_to_lwa(&lwa, &wa);
				error1 = copyout(&lwa, data, sizeof(lwa));
				if (error == 0)
					error = error1;
			}
		}
		break;
	case LNTSYNC_IOC_WAIT_ALL:
		error = copyin(data, &lwa, sizeof(lwa));
		ntsync_lwa_to_wa(&wa, &lwa);
		if (error == 0) {
			error = linux_ntsync_cdev_ioctl(td,
			    NTSYNC_IOC_WAIT_ALL, &wa, fp);
			if (error == 0 || error == EOWNERDEAD) {
				ntsync_wa_to_lwa(&lwa, &wa);
				error1 = copyout(&lwa, data, sizeof(lwa));
				if (error == 0)
					error = error1;
			}
		}
		break;
	case LNTSYNC_IOC_SEM_RELEASE:
		if (fp->f_type != DTYPE_NTSYNC) {
			error = ENOTTY;
			break;
		}
		error = copyin(data, &val, sizeof(val));
		if (error == 0) {
			error = ntsync_sem_release(td, fp, &val);
			if (error == 0)
				error = copyout(&val, data, sizeof(val));
		}
		break;
	case LNTSYNC_IOC_SEM_READ:
		if (fp->f_type != DTYPE_NTSYNC) {
			error = ENOTTY;
			break;
		}
		error = ntsync_sem_read(td, fp, &sa);
		if (error == 0) {
			ntsync_sa_to_lsa(&lsa, &sa);
			error = copyout(&lsa, data, sizeof(lsa));
		}
		break;
	case LNTSYNC_IOC_MUTEX_UNLOCK:
		if (fp->f_type != DTYPE_NTSYNC) {
			error = ENOTTY;
			break;
		}
		error = copyin(data, &lma, sizeof(lma));
		ntsync_lma_to_ma(&ma, &lma);
		if (error == 0) {
			error = ntsync_mutex_unlock(td, fp, &ma);
			if (error == 0) {
				ntsync_ma_to_lma(&lma, &ma);
				error = copyout(&lma, data, sizeof(lma));
			}
		}
		break;
	case LNTSYNC_IOC_MUTEX_KILL:
		if (fp->f_type != DTYPE_NTSYNC) {
			error = ENOTTY;
			break;
		}
		error = copyin(data, &val, sizeof(val));
		if (error == 0)
			error = ntsync_mutex_kill(td, fp, val);
		break;
	case LNTSYNC_IOC_MUTEX_READ:
		if (fp->f_type != DTYPE_NTSYNC) {
			error = ENOTTY;
			break;
		}
		error = ntsync_mutex_read(td, fp, &ma, &doco);
		if (doco) {
			ntsync_ma_to_lma(&lma, &ma);
			error1 = copyout(&lma, data, sizeof(lma));
			if (error == 0)
				error = error1;
		}
		break;
	case LNTSYNC_IOC_EVENT_SET:
		if (fp->f_type != DTYPE_NTSYNC) {
			error = ENOTTY;
			break;
		}
		error = ntsync_event_set(td, fp, &val);
		if (error == 0)
			error = copyout(&val, data, sizeof(val));
		break;
	case LNTSYNC_IOC_EVENT_RESET:
		if (fp->f_type != DTYPE_NTSYNC) {
			error = ENOTTY;
			break;
		}
		error = ntsync_event_reset(td, fp, &val);
		if (error == 0)
			error = copyout(&val, data, sizeof(val));
		break;
	case LNTSYNC_IOC_EVENT_PULSE:
		if (fp->f_type != DTYPE_NTSYNC) {
			error = ENOTTY;
			break;
		}
		error = ntsync_event_pulse(td, fp, &val);
		if (error == 0)
			error = copyout(&val, data, sizeof(val));
		break;
	case LNTSYNC_IOC_EVENT_READ:
		if (fp->f_type != DTYPE_NTSYNC) {
			error = ENOTTY;
			break;
		}
		error = ntsync_event_read(td, fp, &ea);
		if (error == 0) {
			ntsync_ea_to_lea(&lea, &ea);
			error = copyout(&lea, data, sizeof(lea));
		}
		break;
	default:
		error = ENOTTY;
		break;
	}
	fdrop(fp, td);
out:
	return (error);
}
