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
 * Portions Copyright (c) 2008-2009 Stacey Son <sson@FreeBSD.org> 
 *
 * $FreeBSD$
 *
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include "opt_kdtrace.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <sys/dtrace.h>
#include <sys/lockstat.h>

#if defined(__i386__) || defined(__amd64__)
#define LOCKSTAT_AFRAMES 1
#else
#error "architecture not supported"
#endif

static d_open_t lockstat_open;
static void     lockstat_provide(void *, dtrace_probedesc_t *);
static void     lockstat_destroy(void *, dtrace_id_t, void *);
static void     lockstat_enable(void *, dtrace_id_t, void *);
static void     lockstat_disable(void *, dtrace_id_t, void *);
static void     lockstat_load(void *);
static int     	lockstat_unload(void);


typedef struct lockstat_probe {
	char		*lsp_func;
	char		*lsp_name;
	int		lsp_probe;
	dtrace_id_t	lsp_id;
#ifdef __FreeBSD__
	int		lsp_frame;
#endif
} lockstat_probe_t;

#ifdef __FreeBSD__
lockstat_probe_t lockstat_probes[] =
{
  /* Spin Locks */
  { LS_MTX_SPIN_LOCK,	LSS_ACQUIRE,	LS_MTX_SPIN_LOCK_ACQUIRE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_MTX_SPIN_LOCK, 	LSS_SPIN,	LS_MTX_SPIN_LOCK_SPIN,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_MTX_SPIN_UNLOCK,	LSS_RELEASE,	LS_MTX_SPIN_UNLOCK_RELEASE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  /* Adaptive Locks */
  { LS_MTX_LOCK,	LSA_ACQUIRE,	LS_MTX_LOCK_ACQUIRE,
	  DTRACE_IDNONE, (LOCKSTAT_AFRAMES + 1) },
  { LS_MTX_LOCK,	LSA_BLOCK,	LS_MTX_LOCK_BLOCK,
	  DTRACE_IDNONE, (LOCKSTAT_AFRAMES + 1) },
  { LS_MTX_LOCK,	LSA_SPIN,	LS_MTX_LOCK_SPIN,
	  DTRACE_IDNONE, (LOCKSTAT_AFRAMES + 1) },
  { LS_MTX_UNLOCK,	LSA_RELEASE,	LS_MTX_UNLOCK_RELEASE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_MTX_TRYLOCK,	LSA_ACQUIRE,	LS_MTX_TRYLOCK_ACQUIRE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  /* Reader/Writer Locks */
  { LS_RW_RLOCK,	LSR_ACQUIRE,	LS_RW_RLOCK_ACQUIRE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_RW_RLOCK,	LSR_BLOCK,	LS_RW_RLOCK_BLOCK,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_RW_RLOCK,	LSR_SPIN,	LS_RW_RLOCK_SPIN,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_RW_RUNLOCK,	LSR_RELEASE,	LS_RW_RUNLOCK_RELEASE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_RW_WLOCK,	LSR_ACQUIRE,	LS_RW_WLOCK_ACQUIRE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_RW_WLOCK,	LSR_BLOCK,	LS_RW_WLOCK_BLOCK,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_RW_WLOCK,	LSR_SPIN,	LS_RW_WLOCK_SPIN,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_RW_WUNLOCK,	LSR_RELEASE,	LS_RW_WUNLOCK_RELEASE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_RW_TRYUPGRADE,	LSR_UPGRADE,   	LS_RW_TRYUPGRADE_UPGRADE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_RW_DOWNGRADE,	LSR_DOWNGRADE, 	LS_RW_DOWNGRADE_DOWNGRADE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  /* Shared/Exclusive Locks */
  { LS_SX_SLOCK,	LSX_ACQUIRE,	LS_SX_SLOCK_ACQUIRE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_SX_SLOCK,	LSX_BLOCK,	LS_SX_SLOCK_BLOCK,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_SX_SLOCK,	LSX_SPIN,	LS_SX_SLOCK_SPIN,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_SX_SUNLOCK,	LSX_RELEASE,	LS_SX_SUNLOCK_RELEASE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_SX_XLOCK,	LSX_ACQUIRE,	LS_SX_XLOCK_ACQUIRE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_SX_XLOCK,	LSX_BLOCK,	LS_SX_XLOCK_BLOCK,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_SX_XLOCK,	LSX_SPIN,	LS_SX_XLOCK_SPIN,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_SX_XUNLOCK,	LSX_RELEASE,	LS_SX_XUNLOCK_RELEASE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_SX_TRYUPGRADE,	LSX_UPGRADE,	LS_SX_TRYUPGRADE_UPGRADE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { LS_SX_DOWNGRADE,	LSX_DOWNGRADE,	LS_SX_DOWNGRADE_DOWNGRADE,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  /* Thread Locks */
  { LS_THREAD_LOCK,	LST_SPIN,	LS_THREAD_LOCK_SPIN,
	  DTRACE_IDNONE, LOCKSTAT_AFRAMES },
  { NULL }
};
#else
#error "OS not supported"
#endif


static struct cdevsw lockstat_cdevsw = {
	.d_version	= D_VERSION,
	.d_open		= lockstat_open,
	.d_name		= "lockstat",
};

static struct cdev		*lockstat_cdev; 
static dtrace_provider_id_t 	lockstat_id;

/*ARGSUSED*/
static void
lockstat_enable(void *arg, dtrace_id_t id, void *parg)
{
	lockstat_probe_t *probe = parg;

	ASSERT(!lockstat_probemap[probe->lsp_probe]);

	lockstat_enabled++;

	lockstat_probemap[probe->lsp_probe] = id;
#ifdef DOODAD
	membar_producer();
#endif

	lockstat_probe_func = dtrace_probe;
#ifdef DOODAD
	membar_producer();

	lockstat_hot_patch();
	membar_producer();
#endif
}

/*ARGSUSED*/
static void
lockstat_disable(void *arg, dtrace_id_t id, void *parg)
{
	lockstat_probe_t *probe = parg;
	int i;

	ASSERT(lockstat_probemap[probe->lsp_probe]);

	lockstat_enabled--;

	lockstat_probemap[probe->lsp_probe] = 0;
#ifdef DOODAD
	lockstat_hot_patch();
	membar_producer();
#endif

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

}

/*ARGSUSED*/
static int
lockstat_open(struct cdev *dev __unused, int oflags __unused, 
	      int devtype __unused, struct thread *td __unused)
{
	return (0);
}

/*ARGSUSED*/
static void
lockstat_provide(void *arg, dtrace_probedesc_t *desc)
{
	int i = 0;

	for (i = 0; lockstat_probes[i].lsp_func != NULL; i++) {
		lockstat_probe_t *probe = &lockstat_probes[i];

		if (dtrace_probe_lookup(lockstat_id, "kernel",
		    probe->lsp_func, probe->lsp_name) != 0)
			continue;

		ASSERT(!probe->lsp_id);
#ifdef __FreeBSD__
		probe->lsp_id = dtrace_probe_create(lockstat_id,
		    "kernel", probe->lsp_func, probe->lsp_name,
		    probe->lsp_frame, probe);
#else
		probe->lsp_id = dtrace_probe_create(lockstat_id,
		    "kernel", probe->lsp_func, probe->lsp_name,
		    LOCKSTAT_AFRAMES, probe);
#endif
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

static void
lockstat_load(void *dummy)
{
	/* Create the /dev/dtrace/lockstat entry. */
	lockstat_cdev = make_dev(&lockstat_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "dtrace/lockstat");

	if (dtrace_register("lockstat", &lockstat_attr, DTRACE_PRIV_USER,
	    NULL, &lockstat_pops, NULL, &lockstat_id) != 0)
	        return;
}

static int
lockstat_unload()
{
	int error = 0;

	if ((error = dtrace_unregister(lockstat_id)) != 0)
	    return (error);

	destroy_dev(lockstat_cdev);

	return (error);
}

/* ARGSUSED */
static int
lockstat_modevent(module_t mod __unused, int type, void *data __unused)
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

SYSINIT(lockstat_load, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, lockstat_load, NULL);
SYSUNINIT(lockstat_unload, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, lockstat_unload, NULL);

DEV_MODULE(lockstat, lockstat_modevent, NULL);
MODULE_VERSION(lockstat, 1);
MODULE_DEPEND(lockstat, dtrace, 1, 1, 1);
MODULE_DEPEND(lockstat, opensolaris, 1, 1, 1);
