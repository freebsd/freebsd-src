/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * Portions Copyright 2006-2008 John Birrell jb@freebsd.org
 *
 * $FreeBSD: src/sys/cddl/dev/systrace/systrace.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cpuvar.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/selinfo.h>
#include <sys/smp.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <machine/stdarg.h>

#include <sys/dtrace.h>

#ifdef LINUX_SYSTRACE
#include <linux.h>
#include <linux_syscall.h>
#include <linux_proto.h>
#include <linux_syscallnames.c>
#include <linux_systrace.c>
extern struct sysent linux_sysent[];
#define	DEVNAME		"dtrace/linsystrace"
#define	PROVNAME	"linsyscall"
#define	MAXSYSCALL	LINUX_SYS_MAXSYSCALL
#define	SYSCALLNAMES	linux_syscallnames
#define	SYSENT		linux_sysent
#else
/*
 * The syscall arguments are processed into a DTrace argument array
 * using a generated function. See sys/kern/makesyscalls.sh.
 */
#include <sys/syscall.h>
#include <kern/systrace_args.c>
extern const char	*syscallnames[];
#define	DEVNAME		"dtrace/systrace"
#define	PROVNAME	"syscall"
#define	MAXSYSCALL	SYS_MAXSYSCALL
#define	SYSCALLNAMES	syscallnames
#define	SYSENT		sysent
#endif

#define	SYSTRACE_ARTIFICIAL_FRAMES	1

#define	SYSTRACE_SHIFT			16
#define	SYSTRACE_ISENTRY(x)		((int)(x) >> SYSTRACE_SHIFT)
#define	SYSTRACE_SYSNUM(x)		((int)(x) & ((1 << SYSTRACE_SHIFT) - 1))
#define	SYSTRACE_ENTRY(id)		((1 << SYSTRACE_SHIFT) | (id))
#define	SYSTRACE_RETURN(id)		(id)

#if ((1 << SYSTRACE_SHIFT) <= MAXSYSCALL)
#error 1 << SYSTRACE_SHIFT must exceed number of system calls
#endif

static d_open_t	systrace_open;
static int	systrace_unload(void);
static void	systrace_getargdesc(void *, dtrace_id_t, void *, dtrace_argdesc_t *);
static void	systrace_provide(void *, dtrace_probedesc_t *);
static void	systrace_destroy(void *, dtrace_id_t, void *);
static void	systrace_enable(void *, dtrace_id_t, void *);
static void	systrace_disable(void *, dtrace_id_t, void *);
static void	systrace_load(void *);

static struct cdevsw systrace_cdevsw = {
	.d_version	= D_VERSION,
	.d_open		= systrace_open,
#ifdef LINUX_SYSTRACE
	.d_name		= "linsystrace",
#else
	.d_name		= "systrace",
#endif
};

static union	{
	const char	**p_constnames;
	char		**pp_syscallnames;
} uglyhack = { SYSCALLNAMES };

static dtrace_pattr_t systrace_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

static dtrace_pops_t systrace_pops = {
	systrace_provide,
	NULL,
	systrace_enable,
	systrace_disable,
	NULL,
	NULL,
	systrace_getargdesc,
	NULL,
	NULL,
	systrace_destroy
};

static struct cdev		*systrace_cdev;
static dtrace_provider_id_t	systrace_id;

#if !defined(LINUX_SYSTRACE)
/*
 * Probe callback function.
 *
 * Note: This function is called for _all_ syscalls, regardless of which sysent
 *       array the syscall comes from. It could be a standard syscall or a
 *       compat syscall from something like Linux.
 */
static void
systrace_probe(u_int32_t id, int sysnum, struct sysent *sysent, void *params)
{
	int		n_args	= 0;
	u_int64_t	uargs[8];

	/*
	 * Check if this syscall has an argument conversion function
	 * registered.
	 */
	if (sysent->sy_systrace_args_func != NULL)
		/*
		 * Convert the syscall parameters using the registered
		 * function.
		 */
		(*sysent->sy_systrace_args_func)(sysnum, params, uargs, &n_args);
	else
		/*
		 * Use the built-in system call argument conversion
		 * function to translate the syscall structure fields
		 * into the array of 64-bit values that DTrace 
		 * expects.
		 */
		systrace_args(sysnum, params, uargs, &n_args);

	/* Process the probe using the converted argments. */
	dtrace_probe(id, uargs[0], uargs[1], uargs[2], uargs[3], uargs[4]);
}
#endif

static void
systrace_getargdesc(void *arg, dtrace_id_t id, void *parg, dtrace_argdesc_t *desc)
{
	int sysnum = SYSTRACE_SYSNUM((uintptr_t)parg);

	systrace_setargdesc(sysnum, desc->dtargd_ndx, desc->dtargd_native,
	    sizeof(desc->dtargd_native));

	if (desc->dtargd_native[0] == '\0')
		desc->dtargd_ndx = DTRACE_ARGNONE;

	return;
}

static void
systrace_provide(void *arg, dtrace_probedesc_t *desc)
{
	int i;

	if (desc != NULL)
		return;

	for (i = 0; i < MAXSYSCALL; i++) {
		if (dtrace_probe_lookup(systrace_id, NULL,
		    uglyhack.pp_syscallnames[i], "entry") != 0)
			continue;

		(void) dtrace_probe_create(systrace_id, NULL, uglyhack.pp_syscallnames[i],
		    "entry", SYSTRACE_ARTIFICIAL_FRAMES,
		    (void *)((uintptr_t)SYSTRACE_ENTRY(i)));
		(void) dtrace_probe_create(systrace_id, NULL, uglyhack.pp_syscallnames[i],
		    "return", SYSTRACE_ARTIFICIAL_FRAMES,
		    (void *)((uintptr_t)SYSTRACE_RETURN(i)));
	}
}

static void
systrace_destroy(void *arg, dtrace_id_t id, void *parg)
{
#ifdef DEBUG
	int sysnum = SYSTRACE_SYSNUM((uintptr_t)parg);

	/*
	 * There's nothing to do here but assert that we have actually been
	 * disabled.
	 */
	if (SYSTRACE_ISENTRY((uintptr_t)parg)) {
		ASSERT(sysent[sysnum].sy_entry == 0);
	} else {
		ASSERT(sysent[sysnum].sy_return == 0);
	}
#endif
}

static void
systrace_enable(void *arg, dtrace_id_t id, void *parg)
{
	int sysnum = SYSTRACE_SYSNUM((uintptr_t)parg);

	if (SYSENT[sysnum].sy_systrace_args_func == NULL)
		SYSENT[sysnum].sy_systrace_args_func = systrace_args;

	if (SYSTRACE_ISENTRY((uintptr_t)parg))
		SYSENT[sysnum].sy_entry = id;
	else
		SYSENT[sysnum].sy_return = id;
}

static void
systrace_disable(void *arg, dtrace_id_t id, void *parg)
{
	int sysnum = SYSTRACE_SYSNUM((uintptr_t)parg);

	SYSENT[sysnum].sy_entry = 0;
	SYSENT[sysnum].sy_return = 0;
}

static void
systrace_load(void *dummy)
{
	/* Create the /dev/dtrace/systrace entry. */
	systrace_cdev = make_dev(&systrace_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	   DEVNAME);

	if (dtrace_register(PROVNAME, &systrace_attr, DTRACE_PRIV_USER,
	    NULL, &systrace_pops, NULL, &systrace_id) != 0)
		return;

#if !defined(LINUX_SYSTRACE)
	systrace_probe_func = systrace_probe;
#endif
}


static int
systrace_unload()
{
	int error = 0;

	if ((error = dtrace_unregister(systrace_id)) != 0)
		return (error);

#if !defined(LINUX_SYSTRACE)
	systrace_probe_func = NULL;
#endif

	destroy_dev(systrace_cdev);

	return (error);
}

static int
systrace_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		break;

	case MOD_UNLOAD:
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}
	return (error);
}

static int
systrace_open(struct cdev *dev __unused, int oflags __unused, int devtype __unused, struct thread *td __unused)
{
	return (0);
}

SYSINIT(systrace_load, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, systrace_load, NULL);
SYSUNINIT(systrace_unload, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, systrace_unload, NULL);

#ifdef LINUX_SYSTRACE
DEV_MODULE(linsystrace, systrace_modevent, NULL);
MODULE_VERSION(linsystrace, 1);
MODULE_DEPEND(linsystrace, linux, 1, 1, 1);
MODULE_DEPEND(linsystrace, systrace, 1, 1, 1);
MODULE_DEPEND(linsystrace, dtrace, 1, 1, 1);
MODULE_DEPEND(linsystrace, opensolaris, 1, 1, 1);
#else
DEV_MODULE(systrace, systrace_modevent, NULL);
MODULE_VERSION(systrace, 1);
MODULE_DEPEND(systrace, dtrace, 1, 1, 1);
MODULE_DEPEND(systrace, opensolaris, 1, 1, 1);
#endif
