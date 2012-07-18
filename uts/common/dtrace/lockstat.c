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
 */
/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/open.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/bitmap.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/lockstat.h>
#include <sys/atomic.h>
#include <sys/dtrace.h>

#include <sys/ddi.h>
#include <sys/sunddi.h>

typedef struct lockstat_probe {
	const char	*lsp_func;
	const char	*lsp_name;
	int		lsp_probe;
	dtrace_id_t	lsp_id;
} lockstat_probe_t;

lockstat_probe_t lockstat_probes[] =
{
	{ LS_MUTEX_ENTER,	LSA_ACQUIRE,	LS_MUTEX_ENTER_ACQUIRE },
	{ LS_MUTEX_ENTER,	LSA_BLOCK,	LS_MUTEX_ENTER_BLOCK },
	{ LS_MUTEX_ENTER,	LSA_SPIN,	LS_MUTEX_ENTER_SPIN },
	{ LS_MUTEX_EXIT,	LSA_RELEASE,	LS_MUTEX_EXIT_RELEASE },
	{ LS_MUTEX_DESTROY,	LSA_RELEASE,	LS_MUTEX_DESTROY_RELEASE },
	{ LS_MUTEX_TRYENTER,	LSA_ACQUIRE,	LS_MUTEX_TRYENTER_ACQUIRE },
	{ LS_LOCK_SET,		LSS_ACQUIRE,	LS_LOCK_SET_ACQUIRE },
	{ LS_LOCK_SET,		LSS_SPIN,	LS_LOCK_SET_SPIN },
	{ LS_LOCK_SET_SPL,	LSS_ACQUIRE,	LS_LOCK_SET_SPL_ACQUIRE },
	{ LS_LOCK_SET_SPL,	LSS_SPIN,	LS_LOCK_SET_SPL_SPIN },
	{ LS_LOCK_TRY,		LSS_ACQUIRE,	LS_LOCK_TRY_ACQUIRE },
	{ LS_LOCK_CLEAR,	LSS_RELEASE,	LS_LOCK_CLEAR_RELEASE },
	{ LS_LOCK_CLEAR_SPLX,	LSS_RELEASE,	LS_LOCK_CLEAR_SPLX_RELEASE },
	{ LS_CLOCK_UNLOCK,	LSS_RELEASE,	LS_CLOCK_UNLOCK_RELEASE },
	{ LS_RW_ENTER,		LSR_ACQUIRE,	LS_RW_ENTER_ACQUIRE },
	{ LS_RW_ENTER,		LSR_BLOCK,	LS_RW_ENTER_BLOCK },
	{ LS_RW_EXIT,		LSR_RELEASE,	LS_RW_EXIT_RELEASE },
	{ LS_RW_TRYENTER,	LSR_ACQUIRE,	LS_RW_TRYENTER_ACQUIRE },
	{ LS_RW_TRYUPGRADE,	LSR_UPGRADE,	LS_RW_TRYUPGRADE_UPGRADE },
	{ LS_RW_DOWNGRADE,	LSR_DOWNGRADE,	LS_RW_DOWNGRADE_DOWNGRADE },
	{ LS_THREAD_LOCK,	LST_SPIN,	LS_THREAD_LOCK_SPIN },
	{ LS_THREAD_LOCK_HIGH,	LST_SPIN,	LS_THREAD_LOCK_HIGH_SPIN },
	{ NULL }
};

static dev_info_t	*lockstat_devi;	/* saved in xxattach() for xxinfo() */
static kmutex_t		lockstat_test;	/* for testing purposes only */
static dtrace_provider_id_t lockstat_id;

/*ARGSUSED*/
static int
lockstat_enable(void *arg, dtrace_id_t id, void *parg)
{
	lockstat_probe_t *probe = parg;

	ASSERT(!lockstat_probemap[probe->lsp_probe]);

	lockstat_probemap[probe->lsp_probe] = id;
	membar_producer();

	lockstat_hot_patch();
	membar_producer();

	/*
	 * Immediately generate a record for the lockstat_test mutex
	 * to verify that the mutex hot-patch code worked as expected.
	 */
	mutex_enter(&lockstat_test);
	mutex_exit(&lockstat_test);
	return (0);
}

/*ARGSUSED*/
static void
lockstat_disable(void *arg, dtrace_id_t id, void *parg)
{
	lockstat_probe_t *probe = parg;
	int i;

	ASSERT(lockstat_probemap[probe->lsp_probe]);

	lockstat_probemap[probe->lsp_probe] = 0;
	lockstat_hot_patch();
	membar_producer();

	/*
	 * See if we have any probes left enabled.
	 */
	for (i = 0; i < LS_NPROBES; i++) {
		if (lockstat_probemap[i]) {
			/*
			 * This probe is still enabled.  We don't need to deal
			 * with waiting for all threads to be out of the
			 * lockstat critical sections; just return.
			 */
			return;
		}
	}

	/*
	 * The delay() here isn't as cheesy as you might think.  We don't
	 * want to busy-loop in the kernel, so we have to give up the
	 * CPU between calls to lockstat_active_threads(); that much is
	 * obvious.  But the reason it's a do..while loop rather than a
	 * while loop is subtle.  The memory barrier above guarantees that
	 * no threads will enter the lockstat code from this point forward.
	 * However, another thread could already be executing lockstat code
	 * without our knowledge if the update to its t_lockstat field hasn't
	 * cleared its CPU's store buffer.  Delaying for one clock tick
	 * guarantees that either (1) the thread will have *ample* time to
	 * complete its work, or (2) the thread will be preempted, in which
	 * case it will have to grab and release a dispatcher lock, which
	 * will flush that CPU's store buffer.  Either way we're covered.
	 */
	do {
		delay(1);
	} while (lockstat_active_threads());
}

/*ARGSUSED*/
static int
lockstat_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	return (0);
}

/* ARGSUSED */
static int
lockstat_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *) lockstat_devi;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*ARGSUSED*/
static void
lockstat_provide(void *arg, const dtrace_probedesc_t *desc)
{
	int i = 0;

	for (i = 0; lockstat_probes[i].lsp_func != NULL; i++) {
		lockstat_probe_t *probe = &lockstat_probes[i];

		if (dtrace_probe_lookup(lockstat_id, "genunix",
		    probe->lsp_func, probe->lsp_name) != 0)
			continue;

		ASSERT(!probe->lsp_id);
		probe->lsp_id = dtrace_probe_create(lockstat_id,
		    "genunix", probe->lsp_func, probe->lsp_name,
		    1, probe);
	}
}

/*ARGSUSED*/
static void
lockstat_destroy(void *arg, dtrace_id_t id, void *parg)
{
	lockstat_probe_t *probe = parg;

	ASSERT(!lockstat_probemap[probe->lsp_probe]);
	probe->lsp_id = 0;
}

static dtrace_pattr_t lockstat_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
};

static dtrace_pops_t lockstat_pops = {
	lockstat_provide,
	NULL,
	lockstat_enable,
	lockstat_disable,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	lockstat_destroy
};

static int
lockstat_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (ddi_create_minor_node(devi, "lockstat", S_IFCHR, 0,
	    DDI_PSEUDO, 0) == DDI_FAILURE ||
	    dtrace_register("lockstat", &lockstat_attr, DTRACE_PRIV_KERNEL,
	    NULL, &lockstat_pops, NULL, &lockstat_id) != 0) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}

	lockstat_probe = dtrace_probe;
	membar_producer();

	ddi_report_dev(devi);
	lockstat_devi = devi;
	return (DDI_SUCCESS);
}

static int
lockstat_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (dtrace_unregister(lockstat_id) != 0)
		return (DDI_FAILURE);

	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/*
 * Configuration data structures
 */
static struct cb_ops lockstat_cb_ops = {
	lockstat_open,		/* open */
	nodev,			/* close */
	nulldev,		/* strategy */
	nulldev,		/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab */
	D_MP | D_NEW		/* Driver compatibility flag */
};

static struct dev_ops lockstat_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt */
	lockstat_info,		/* getinfo */
	nulldev,		/* identify */
	nulldev,		/* probe */
	lockstat_attach,	/* attach */
	lockstat_detach,	/* detach */
	nulldev,		/* reset */
	&lockstat_cb_ops,	/* cb_ops */
	NULL,			/* bus_ops */
	NULL,			/* power */
	ddi_quiesce_not_needed,		/* quiesce */
};

static struct modldrv modldrv = {
	&mod_driverops,		/* Type of module.  This one is a driver */
	"Lock Statistics",	/* name of module */
	&lockstat_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modldrv, NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
