/*-
 * Copyright (c) 2002-2006 Sam Leffler.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Cryptographic Subsystem.
 *
 * This code is derived from the Openbsd Cryptographic Framework (OCF)
 * that has the copyright shown below.  Very little of the original
 * code remains.
 */

/*-
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000, 2001 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#define	CRYPTO_TIMING				/* enable timing support */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>

#include <ddb/ddb.h>

#include <vm/uma.h>
#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>			/* XXX for M_XDATA */

#include <sys/kobj.h>
#include <sys/bus.h>
#include "cryptodev_if.h"

#if defined(__i386__) || defined(__amd64__)
#include <machine/pcb.h>
#endif

SDT_PROVIDER_DEFINE(opencrypto);

/*
 * Crypto drivers register themselves by allocating a slot in the
 * crypto_drivers table with crypto_get_driverid() and then registering
 * each algorithm they support with crypto_register() and crypto_kregister().
 */
static	struct mtx crypto_drivers_mtx;		/* lock on driver table */
#define	CRYPTO_DRIVER_LOCK()	mtx_lock(&crypto_drivers_mtx)
#define	CRYPTO_DRIVER_UNLOCK()	mtx_unlock(&crypto_drivers_mtx)
#define	CRYPTO_DRIVER_ASSERT()	mtx_assert(&crypto_drivers_mtx, MA_OWNED)

/*
 * Crypto device/driver capabilities structure.
 *
 * Synchronization:
 * (d) - protected by CRYPTO_DRIVER_LOCK()
 * (q) - protected by CRYPTO_Q_LOCK()
 * Not tagged fields are read-only.
 */
struct cryptocap {
	device_t	cc_dev;			/* (d) device/driver */
	u_int32_t	cc_sessions;		/* (d) # of sessions */
	u_int32_t	cc_koperations;		/* (d) # os asym operations */
	/*
	 * Largest possible operator length (in bits) for each type of
	 * encryption algorithm. XXX not used
	 */
	u_int16_t	cc_max_op_len[CRYPTO_ALGORITHM_MAX + 1];
	u_int8_t	cc_alg[CRYPTO_ALGORITHM_MAX + 1];
	u_int8_t	cc_kalg[CRK_ALGORITHM_MAX + 1];

	int		cc_flags;		/* (d) flags */
#define CRYPTOCAP_F_CLEANUP	0x80000000	/* needs resource cleanup */
	int		cc_qblocked;		/* (q) symmetric q blocked */
	int		cc_kqblocked;		/* (q) asymmetric q blocked */
};
static	struct cryptocap *crypto_drivers = NULL;
static	int crypto_drivers_num = 0;

/*
 * There are two queues for crypto requests; one for symmetric (e.g.
 * cipher) operations and one for asymmetric (e.g. MOD)operations.
 * A single mutex is used to lock access to both queues.  We could
 * have one per-queue but having one simplifies handling of block/unblock
 * operations.
 */
static	int crp_sleep = 0;
static	TAILQ_HEAD(,cryptop) crp_q;		/* request queues */
static	TAILQ_HEAD(,cryptkop) crp_kq;
static	struct mtx crypto_q_mtx;
#define	CRYPTO_Q_LOCK()		mtx_lock(&crypto_q_mtx)
#define	CRYPTO_Q_UNLOCK()	mtx_unlock(&crypto_q_mtx)

/*
 * There are two queues for processing completed crypto requests; one
 * for the symmetric and one for the asymmetric ops.  We only need one
 * but have two to avoid type futzing (cryptop vs. cryptkop).  A single
 * mutex is used to lock access to both queues.  Note that this lock
 * must be separate from the lock on request queues to insure driver
 * callbacks don't generate lock order reversals.
 */
static	TAILQ_HEAD(,cryptop) crp_ret_q;		/* callback queues */
static	TAILQ_HEAD(,cryptkop) crp_ret_kq;
static	struct mtx crypto_ret_q_mtx;
#define	CRYPTO_RETQ_LOCK()	mtx_lock(&crypto_ret_q_mtx)
#define	CRYPTO_RETQ_UNLOCK()	mtx_unlock(&crypto_ret_q_mtx)
#define	CRYPTO_RETQ_EMPTY()	(TAILQ_EMPTY(&crp_ret_q) && TAILQ_EMPTY(&crp_ret_kq))

static	uma_zone_t cryptop_zone;
static	uma_zone_t cryptodesc_zone;

int	crypto_userasymcrypto = 1;	/* userland may do asym crypto reqs */
SYSCTL_INT(_kern, OID_AUTO, userasymcrypto, CTLFLAG_RW,
	   &crypto_userasymcrypto, 0,
	   "Enable/disable user-mode access to asymmetric crypto support");
int	crypto_devallowsoft = 0;	/* only use hardware crypto */
SYSCTL_INT(_kern, OID_AUTO, cryptodevallowsoft, CTLFLAG_RW,
	   &crypto_devallowsoft, 0,
	   "Enable/disable use of software crypto by /dev/crypto");

MALLOC_DEFINE(M_CRYPTO_DATA, "crypto", "crypto session records");

static	void crypto_proc(void);
static	struct proc *cryptoproc;
static	void crypto_ret_proc(void);
static	struct proc *cryptoretproc;
static	void crypto_destroy(void);
static	int crypto_invoke(struct cryptocap *cap, struct cryptop *crp, int hint);
static	int crypto_kinvoke(struct cryptkop *krp, int flags);

static	struct cryptostats cryptostats;
SYSCTL_STRUCT(_kern, OID_AUTO, crypto_stats, CTLFLAG_RW, &cryptostats,
	    cryptostats, "Crypto system statistics");

#ifdef CRYPTO_TIMING
static	int crypto_timing = 0;
SYSCTL_INT(_debug, OID_AUTO, crypto_timing, CTLFLAG_RW,
	   &crypto_timing, 0, "Enable/disable crypto timing support");
#endif

static int
crypto_init(void)
{
	int error;

	mtx_init(&crypto_drivers_mtx, "crypto", "crypto driver table",
		MTX_DEF|MTX_QUIET);

	TAILQ_INIT(&crp_q);
	TAILQ_INIT(&crp_kq);
	mtx_init(&crypto_q_mtx, "crypto", "crypto op queues", MTX_DEF);

	TAILQ_INIT(&crp_ret_q);
	TAILQ_INIT(&crp_ret_kq);
	mtx_init(&crypto_ret_q_mtx, "crypto", "crypto return queues", MTX_DEF);

	cryptop_zone = uma_zcreate("cryptop", sizeof (struct cryptop),
				    0, 0, 0, 0,
				    UMA_ALIGN_PTR, UMA_ZONE_ZINIT);
	cryptodesc_zone = uma_zcreate("cryptodesc", sizeof (struct cryptodesc),
				    0, 0, 0, 0,
				    UMA_ALIGN_PTR, UMA_ZONE_ZINIT);
	if (cryptodesc_zone == NULL || cryptop_zone == NULL) {
		printf("crypto_init: cannot setup crypto zones\n");
		error = ENOMEM;
		goto bad;
	}

	crypto_drivers_num = CRYPTO_DRIVERS_INITIAL;
	crypto_drivers = malloc(crypto_drivers_num *
	    sizeof(struct cryptocap), M_CRYPTO_DATA, M_NOWAIT | M_ZERO);
	if (crypto_drivers == NULL) {
		printf("crypto_init: cannot setup crypto drivers\n");
		error = ENOMEM;
		goto bad;
	}

	error = kproc_create((void (*)(void *)) crypto_proc, NULL,
		    &cryptoproc, 0, 0, "crypto");
	if (error) {
		printf("crypto_init: cannot start crypto thread; error %d",
			error);
		goto bad;
	}

	error = kproc_create((void (*)(void *)) crypto_ret_proc, NULL,
		    &cryptoretproc, 0, 0, "crypto returns");
	if (error) {
		printf("crypto_init: cannot start cryptoret thread; error %d",
			error);
		goto bad;
	}
	return 0;
bad:
	crypto_destroy();
	return error;
}

/*
 * Signal a crypto thread to terminate.  We use the driver
 * table lock to synchronize the sleep/wakeups so that we
 * are sure the threads have terminated before we release
 * the data structures they use.  See crypto_finis below
 * for the other half of this song-and-dance.
 */
static void
crypto_terminate(struct proc **pp, void *q)
{
	struct proc *p;

	mtx_assert(&crypto_drivers_mtx, MA_OWNED);
	p = *pp;
	*pp = NULL;
	if (p) {
		wakeup_one(q);
		PROC_LOCK(p);		/* NB: insure we don't miss wakeup */
		CRYPTO_DRIVER_UNLOCK();	/* let crypto_finis progress */
		msleep(p, &p->p_mtx, PWAIT, "crypto_destroy", 0);
		PROC_UNLOCK(p);
		CRYPTO_DRIVER_LOCK();
	}
}

static void
crypto_destroy(void)
{
	/*
	 * Terminate any crypto threads.
	 */
	CRYPTO_DRIVER_LOCK();
	crypto_terminate(&cryptoproc, &crp_q);
	crypto_terminate(&cryptoretproc, &crp_ret_q);
	CRYPTO_DRIVER_UNLOCK();

	/* XXX flush queues??? */

	/* 
	 * Reclaim dynamically allocated resources.
	 */
	if (crypto_drivers != NULL)
		free(crypto_drivers, M_CRYPTO_DATA);

	if (cryptodesc_zone != NULL)
		uma_zdestroy(cryptodesc_zone);
	if (cryptop_zone != NULL)
		uma_zdestroy(cryptop_zone);
	mtx_destroy(&crypto_q_mtx);
	mtx_destroy(&crypto_ret_q_mtx);
	mtx_destroy(&crypto_drivers_mtx);
}

static struct cryptocap *
crypto_checkdriver(u_int32_t hid)
{
	if (crypto_drivers == NULL)
		return NULL;
	return (hid >= crypto_drivers_num ? NULL : &crypto_drivers[hid]);
}

/*
 * Compare a driver's list of supported algorithms against another
 * list; return non-zero if all algorithms are supported.
 */
static int
driver_suitable(const struct cryptocap *cap, const struct cryptoini *cri)
{
	const struct cryptoini *cr;

	/* See if all the algorithms are supported. */
	for (cr = cri; cr; cr = cr->cri_next)
		if (cap->cc_alg[cr->cri_alg] == 0)
			return 0;
	return 1;
}

/*
 * Select a driver for a new session that supports the specified
 * algorithms and, optionally, is constrained according to the flags.
 * The algorithm we use here is pretty stupid; just use the
 * first driver that supports all the algorithms we need. If there
 * are multiple drivers we choose the driver with the fewest active
 * sessions.  We prefer hardware-backed drivers to software ones.
 *
 * XXX We need more smarts here (in real life too, but that's
 * XXX another story altogether).
 */
static struct cryptocap *
crypto_select_driver(const struct cryptoini *cri, int flags)
{
	struct cryptocap *cap, *best;
	int match, hid;

	CRYPTO_DRIVER_ASSERT();

	/*
	 * Look first for hardware crypto devices if permitted.
	 */
	if (flags & CRYPTOCAP_F_HARDWARE)
		match = CRYPTOCAP_F_HARDWARE;
	else
		match = CRYPTOCAP_F_SOFTWARE;
	best = NULL;
again:
	for (hid = 0; hid < crypto_drivers_num; hid++) {
		cap = &crypto_drivers[hid];
		/*
		 * If it's not initialized, is in the process of
		 * going away, or is not appropriate (hardware
		 * or software based on match), then skip.
		 */
		if (cap->cc_dev == NULL ||
		    (cap->cc_flags & CRYPTOCAP_F_CLEANUP) ||
		    (cap->cc_flags & match) == 0)
			continue;

		/* verify all the algorithms are supported. */
		if (driver_suitable(cap, cri)) {
			if (best == NULL ||
			    cap->cc_sessions < best->cc_sessions)
				best = cap;
		}
	}
	if (best == NULL && match == CRYPTOCAP_F_HARDWARE &&
	    (flags & CRYPTOCAP_F_SOFTWARE)) {
		/* sort of an Algol 68-style for loop */
		match = CRYPTOCAP_F_SOFTWARE;
		goto again;
	}
	return best;
}

/*
 * Create a new session.  The crid argument specifies a crypto
 * driver to use or constraints on a driver to select (hardware
 * only, software only, either).  Whatever driver is selected
 * must be capable of the requested crypto algorithms.
 */
int
crypto_newsession(u_int64_t *sid, struct cryptoini *cri, int crid)
{
	struct cryptocap *cap;
	u_int32_t hid, lid;
	int err;

	CRYPTO_DRIVER_LOCK();
	if ((crid & (CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE)) == 0) {
		/*
		 * Use specified driver; verify it is capable.
		 */
		cap = crypto_checkdriver(crid);
		if (cap != NULL && !driver_suitable(cap, cri))
			cap = NULL;
	} else {
		/*
		 * No requested driver; select based on crid flags.
		 */
		cap = crypto_select_driver(cri, crid);
		/*
		 * if NULL then can't do everything in one session.
		 * XXX Fix this. We need to inject a "virtual" session
		 * XXX layer right about here.
		 */
	}
	if (cap != NULL) {
		/* Call the driver initialization routine. */
		hid = cap - crypto_drivers;
		lid = hid;		/* Pass the driver ID. */
		err = CRYPTODEV_NEWSESSION(cap->cc_dev, &lid, cri);
		if (err == 0) {
			(*sid) = (cap->cc_flags & 0xff000000)
			       | (hid & 0x00ffffff);
			(*sid) <<= 32;
			(*sid) |= (lid & 0xffffffff);
			cap->cc_sessions++;
		} else
			CRYPTDEB("dev newsession failed");
	} else {
		CRYPTDEB("no driver");
		err = EINVAL;
	}
	CRYPTO_DRIVER_UNLOCK();
	return err;
}

static void
crypto_remove(struct cryptocap *cap)
{

	mtx_assert(&crypto_drivers_mtx, MA_OWNED);
	if (cap->cc_sessions == 0 && cap->cc_koperations == 0)
		bzero(cap, sizeof(*cap));
}

/*
 * Delete an existing session (or a reserved session on an unregistered
 * driver).
 */
int
crypto_freesession(u_int64_t sid)
{
	struct cryptocap *cap;
	u_int32_t hid;
	int err;

	CRYPTO_DRIVER_LOCK();

	if (crypto_drivers == NULL) {
		err = EINVAL;
		goto done;
	}

	/* Determine two IDs. */
	hid = CRYPTO_SESID2HID(sid);

	if (hid >= crypto_drivers_num) {
		err = ENOENT;
		goto done;
	}
	cap = &crypto_drivers[hid];

	if (cap->cc_sessions)
		cap->cc_sessions--;

	/* Call the driver cleanup routine, if available. */
	err = CRYPTODEV_FREESESSION(cap->cc_dev, sid);

	if (cap->cc_flags & CRYPTOCAP_F_CLEANUP)
		crypto_remove(cap);

done:
	CRYPTO_DRIVER_UNLOCK();
	return err;
}

/*
 * Return an unused driver id.  Used by drivers prior to registering
 * support for the algorithms they handle.
 */
int32_t
crypto_get_driverid(device_t dev, int flags)
{
	struct cryptocap *newdrv;
	int i;

	if ((flags & (CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE)) == 0) {
		printf("%s: no flags specified when registering driver\n",
		    device_get_nameunit(dev));
		return -1;
	}

	CRYPTO_DRIVER_LOCK();

	for (i = 0; i < crypto_drivers_num; i++) {
		if (crypto_drivers[i].cc_dev == NULL &&
		    (crypto_drivers[i].cc_flags & CRYPTOCAP_F_CLEANUP) == 0) {
			break;
		}
	}

	/* Out of entries, allocate some more. */
	if (i == crypto_drivers_num) {
		/* Be careful about wrap-around. */
		if (2 * crypto_drivers_num <= crypto_drivers_num) {
			CRYPTO_DRIVER_UNLOCK();
			printf("crypto: driver count wraparound!\n");
			return -1;
		}

		newdrv = malloc(2 * crypto_drivers_num *
		    sizeof(struct cryptocap), M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
		if (newdrv == NULL) {
			CRYPTO_DRIVER_UNLOCK();
			printf("crypto: no space to expand driver table!\n");
			return -1;
		}

		bcopy(crypto_drivers, newdrv,
		    crypto_drivers_num * sizeof(struct cryptocap));

		crypto_drivers_num *= 2;

		free(crypto_drivers, M_CRYPTO_DATA);
		crypto_drivers = newdrv;
	}

	/* NB: state is zero'd on free */
	crypto_drivers[i].cc_sessions = 1;	/* Mark */
	crypto_drivers[i].cc_dev = dev;
	crypto_drivers[i].cc_flags = flags;
	if (bootverbose)
		printf("crypto: assign %s driver id %u, flags %u\n",
		    device_get_nameunit(dev), i, flags);

	CRYPTO_DRIVER_UNLOCK();

	return i;
}

/*
 * Lookup a driver by name.  We match against the full device
 * name and unit, and against just the name.  The latter gives
 * us a simple widlcarding by device name.  On success return the
 * driver/hardware identifier; otherwise return -1.
 */
int
crypto_find_driver(const char *match)
{
	int i, len = strlen(match);

	CRYPTO_DRIVER_LOCK();
	for (i = 0; i < crypto_drivers_num; i++) {
		device_t dev = crypto_drivers[i].cc_dev;
		if (dev == NULL ||
		    (crypto_drivers[i].cc_flags & CRYPTOCAP_F_CLEANUP))
			continue;
		if (strncmp(match, device_get_nameunit(dev), len) == 0 ||
		    strncmp(match, device_get_name(dev), len) == 0)
			break;
	}
	CRYPTO_DRIVER_UNLOCK();
	return i < crypto_drivers_num ? i : -1;
}

/*
 * Return the device_t for the specified driver or NULL
 * if the driver identifier is invalid.
 */
device_t
crypto_find_device_byhid(int hid)
{
	struct cryptocap *cap = crypto_checkdriver(hid);
	return cap != NULL ? cap->cc_dev : NULL;
}

/*
 * Return the device/driver capabilities.
 */
int
crypto_getcaps(int hid)
{
	struct cryptocap *cap = crypto_checkdriver(hid);
	return cap != NULL ? cap->cc_flags : 0;
}

/*
 * Register support for a key-related algorithm.  This routine
 * is called once for each algorithm supported a driver.
 */
int
crypto_kregister(u_int32_t driverid, int kalg, u_int32_t flags)
{
	struct cryptocap *cap;
	int err;

	CRYPTO_DRIVER_LOCK();

	cap = crypto_checkdriver(driverid);
	if (cap != NULL &&
	    (CRK_ALGORITM_MIN <= kalg && kalg <= CRK_ALGORITHM_MAX)) {
		/*
		 * XXX Do some performance testing to determine placing.
		 * XXX We probably need an auxiliary data structure that
		 * XXX describes relative performances.
		 */

		cap->cc_kalg[kalg] = flags | CRYPTO_ALG_FLAG_SUPPORTED;
		if (bootverbose)
			printf("crypto: %s registers key alg %u flags %u\n"
				, device_get_nameunit(cap->cc_dev)
				, kalg
				, flags
			);
		err = 0;
	} else
		err = EINVAL;

	CRYPTO_DRIVER_UNLOCK();
	return err;
}

/*
 * Register support for a non-key-related algorithm.  This routine
 * is called once for each such algorithm supported by a driver.
 */
int
crypto_register(u_int32_t driverid, int alg, u_int16_t maxoplen,
    u_int32_t flags)
{
	struct cryptocap *cap;
	int err;

	CRYPTO_DRIVER_LOCK();

	cap = crypto_checkdriver(driverid);
	/* NB: algorithms are in the range [1..max] */
	if (cap != NULL &&
	    (CRYPTO_ALGORITHM_MIN <= alg && alg <= CRYPTO_ALGORITHM_MAX)) {
		/*
		 * XXX Do some performance testing to determine placing.
		 * XXX We probably need an auxiliary data structure that
		 * XXX describes relative performances.
		 */

		cap->cc_alg[alg] = flags | CRYPTO_ALG_FLAG_SUPPORTED;
		cap->cc_max_op_len[alg] = maxoplen;
		if (bootverbose)
			printf("crypto: %s registers alg %u flags %u maxoplen %u\n"
				, device_get_nameunit(cap->cc_dev)
				, alg
				, flags
				, maxoplen
			);
		cap->cc_sessions = 0;		/* Unmark */
		err = 0;
	} else
		err = EINVAL;

	CRYPTO_DRIVER_UNLOCK();
	return err;
}

static void
driver_finis(struct cryptocap *cap)
{
	u_int32_t ses, kops;

	CRYPTO_DRIVER_ASSERT();

	ses = cap->cc_sessions;
	kops = cap->cc_koperations;
	bzero(cap, sizeof(*cap));
	if (ses != 0 || kops != 0) {
		/*
		 * If there are pending sessions,
		 * just mark as invalid.
		 */
		cap->cc_flags |= CRYPTOCAP_F_CLEANUP;
		cap->cc_sessions = ses;
		cap->cc_koperations = kops;
	}
}

/*
 * Unregister a crypto driver. If there are pending sessions using it,
 * leave enough information around so that subsequent calls using those
 * sessions will correctly detect the driver has been unregistered and
 * reroute requests.
 */
int
crypto_unregister(u_int32_t driverid, int alg)
{
	struct cryptocap *cap;
	int i, err;

	CRYPTO_DRIVER_LOCK();
	cap = crypto_checkdriver(driverid);
	if (cap != NULL &&
	    (CRYPTO_ALGORITHM_MIN <= alg && alg <= CRYPTO_ALGORITHM_MAX) &&
	    cap->cc_alg[alg] != 0) {
		cap->cc_alg[alg] = 0;
		cap->cc_max_op_len[alg] = 0;

		/* Was this the last algorithm ? */
		for (i = 1; i <= CRYPTO_ALGORITHM_MAX; i++)
			if (cap->cc_alg[i] != 0)
				break;

		if (i == CRYPTO_ALGORITHM_MAX + 1)
			driver_finis(cap);
		err = 0;
	} else
		err = EINVAL;
	CRYPTO_DRIVER_UNLOCK();

	return err;
}

/*
 * Unregister all algorithms associated with a crypto driver.
 * If there are pending sessions using it, leave enough information
 * around so that subsequent calls using those sessions will
 * correctly detect the driver has been unregistered and reroute
 * requests.
 */
int
crypto_unregister_all(u_int32_t driverid)
{
	struct cryptocap *cap;
	int err;

	CRYPTO_DRIVER_LOCK();
	cap = crypto_checkdriver(driverid);
	if (cap != NULL) {
		driver_finis(cap);
		err = 0;
	} else
		err = EINVAL;
	CRYPTO_DRIVER_UNLOCK();

	return err;
}

/*
 * Clear blockage on a driver.  The what parameter indicates whether
 * the driver is now ready for cryptop's and/or cryptokop's.
 */
int
crypto_unblock(u_int32_t driverid, int what)
{
	struct cryptocap *cap;
	int err;

	CRYPTO_Q_LOCK();
	cap = crypto_checkdriver(driverid);
	if (cap != NULL) {
		if (what & CRYPTO_SYMQ)
			cap->cc_qblocked = 0;
		if (what & CRYPTO_ASYMQ)
			cap->cc_kqblocked = 0;
		if (crp_sleep)
			wakeup_one(&crp_q);
		err = 0;
	} else
		err = EINVAL;
	CRYPTO_Q_UNLOCK();

	return err;
}

/*
 * Add a crypto request to a queue, to be processed by the kernel thread.
 */
int
crypto_dispatch(struct cryptop *crp)
{
	struct cryptocap *cap;
	u_int32_t hid;
	int result;

	cryptostats.cs_ops++;

#ifdef CRYPTO_TIMING
	if (crypto_timing)
		binuptime(&crp->crp_tstamp);
#endif

	hid = CRYPTO_SESID2HID(crp->crp_sid);

	if ((crp->crp_flags & CRYPTO_F_BATCH) == 0) {
		/*
		 * Caller marked the request to be processed
		 * immediately; dispatch it directly to the
		 * driver unless the driver is currently blocked.
		 */
		cap = crypto_checkdriver(hid);
		/* Driver cannot disappeared when there is an active session. */
		KASSERT(cap != NULL, ("%s: Driver disappeared.", __func__));
		if (!cap->cc_qblocked) {
			result = crypto_invoke(cap, crp, 0);
			if (result != ERESTART)
				return (result);
			/*
			 * The driver ran out of resources, put the request on
			 * the queue.
			 */
		}
	}
	CRYPTO_Q_LOCK();
	TAILQ_INSERT_TAIL(&crp_q, crp, crp_next);
	if (crp_sleep)
		wakeup_one(&crp_q);
	CRYPTO_Q_UNLOCK();
	return 0;
}

/*
 * Add an asymetric crypto request to a queue,
 * to be processed by the kernel thread.
 */
int
crypto_kdispatch(struct cryptkop *krp)
{
	int error;

	cryptostats.cs_kops++;

	error = crypto_kinvoke(krp, krp->krp_crid);
	if (error == ERESTART) {
		CRYPTO_Q_LOCK();
		TAILQ_INSERT_TAIL(&crp_kq, krp, krp_next);
		if (crp_sleep)
			wakeup_one(&crp_q);
		CRYPTO_Q_UNLOCK();
		error = 0;
	}
	return error;
}

/*
 * Verify a driver is suitable for the specified operation.
 */
static __inline int
kdriver_suitable(const struct cryptocap *cap, const struct cryptkop *krp)
{
	return (cap->cc_kalg[krp->krp_op] & CRYPTO_ALG_FLAG_SUPPORTED) != 0;
}

/*
 * Select a driver for an asym operation.  The driver must
 * support the necessary algorithm.  The caller can constrain
 * which device is selected with the flags parameter.  The
 * algorithm we use here is pretty stupid; just use the first
 * driver that supports the algorithms we need. If there are
 * multiple suitable drivers we choose the driver with the
 * fewest active operations.  We prefer hardware-backed
 * drivers to software ones when either may be used.
 */
static struct cryptocap *
crypto_select_kdriver(const struct cryptkop *krp, int flags)
{
	struct cryptocap *cap, *best, *blocked;
	int match, hid;

	CRYPTO_DRIVER_ASSERT();

	/*
	 * Look first for hardware crypto devices if permitted.
	 */
	if (flags & CRYPTOCAP_F_HARDWARE)
		match = CRYPTOCAP_F_HARDWARE;
	else
		match = CRYPTOCAP_F_SOFTWARE;
	best = NULL;
	blocked = NULL;
again:
	for (hid = 0; hid < crypto_drivers_num; hid++) {
		cap = &crypto_drivers[hid];
		/*
		 * If it's not initialized, is in the process of
		 * going away, or is not appropriate (hardware
		 * or software based on match), then skip.
		 */
		if (cap->cc_dev == NULL ||
		    (cap->cc_flags & CRYPTOCAP_F_CLEANUP) ||
		    (cap->cc_flags & match) == 0)
			continue;

		/* verify all the algorithms are supported. */
		if (kdriver_suitable(cap, krp)) {
			if (best == NULL ||
			    cap->cc_koperations < best->cc_koperations)
				best = cap;
		}
	}
	if (best != NULL)
		return best;
	if (match == CRYPTOCAP_F_HARDWARE && (flags & CRYPTOCAP_F_SOFTWARE)) {
		/* sort of an Algol 68-style for loop */
		match = CRYPTOCAP_F_SOFTWARE;
		goto again;
	}
	return best;
}

/*
 * Dispatch an asymmetric crypto request.
 */
static int
crypto_kinvoke(struct cryptkop *krp, int crid)
{
	struct cryptocap *cap = NULL;
	int error;

	KASSERT(krp != NULL, ("%s: krp == NULL", __func__));
	KASSERT(krp->krp_callback != NULL,
	    ("%s: krp->crp_callback == NULL", __func__));

	CRYPTO_DRIVER_LOCK();
	if ((crid & (CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE)) == 0) {
		cap = crypto_checkdriver(crid);
		if (cap != NULL) {
			/*
			 * Driver present, it must support the necessary
			 * algorithm and, if s/w drivers are excluded,
			 * it must be registered as hardware-backed.
			 */
			if (!kdriver_suitable(cap, krp) ||
			    (!crypto_devallowsoft &&
			     (cap->cc_flags & CRYPTOCAP_F_HARDWARE) == 0))
				cap = NULL;
		}
	} else {
		/*
		 * No requested driver; select based on crid flags.
		 */
		if (!crypto_devallowsoft)	/* NB: disallow s/w drivers */
			crid &= ~CRYPTOCAP_F_SOFTWARE;
		cap = crypto_select_kdriver(krp, crid);
	}
	if (cap != NULL && !cap->cc_kqblocked) {
		krp->krp_hid = cap - crypto_drivers;
		cap->cc_koperations++;
		CRYPTO_DRIVER_UNLOCK();
		error = CRYPTODEV_KPROCESS(cap->cc_dev, krp, 0);
		CRYPTO_DRIVER_LOCK();
		if (error == ERESTART) {
			cap->cc_koperations--;
			CRYPTO_DRIVER_UNLOCK();
			return (error);
		}
	} else {
		/*
		 * NB: cap is !NULL if device is blocked; in
		 *     that case return ERESTART so the operation
		 *     is resubmitted if possible.
		 */
		error = (cap == NULL) ? ENODEV : ERESTART;
	}
	CRYPTO_DRIVER_UNLOCK();

	if (error) {
		krp->krp_status = error;
		crypto_kdone(krp);
	}
	return 0;
}

#ifdef CRYPTO_TIMING
static void
crypto_tstat(struct cryptotstat *ts, struct bintime *bt)
{
	struct bintime now, delta;
	struct timespec t;
	uint64_t u;

	binuptime(&now);
	u = now.frac;
	delta.frac = now.frac - bt->frac;
	delta.sec = now.sec - bt->sec;
	if (u < delta.frac)
		delta.sec--;
	bintime2timespec(&delta, &t);
	timespecadd(&ts->acc, &t);
	if (timespeccmp(&t, &ts->min, <))
		ts->min = t;
	if (timespeccmp(&t, &ts->max, >))
		ts->max = t;
	ts->count++;

	*bt = now;
}
#endif

/*
 * Dispatch a crypto request to the appropriate crypto devices.
 */
static int
crypto_invoke(struct cryptocap *cap, struct cryptop *crp, int hint)
{

	KASSERT(crp != NULL, ("%s: crp == NULL", __func__));
	KASSERT(crp->crp_callback != NULL,
	    ("%s: crp->crp_callback == NULL", __func__));
	KASSERT(crp->crp_desc != NULL, ("%s: crp->crp_desc == NULL", __func__));

#ifdef CRYPTO_TIMING
	if (crypto_timing)
		crypto_tstat(&cryptostats.cs_invoke, &crp->crp_tstamp);
#endif
	if (cap->cc_flags & CRYPTOCAP_F_CLEANUP) {
		struct cryptodesc *crd;
		u_int64_t nid;

		/*
		 * Driver has unregistered; migrate the session and return
		 * an error to the caller so they'll resubmit the op.
		 *
		 * XXX: What if there are more already queued requests for this
		 *      session?
		 */
		crypto_freesession(crp->crp_sid);

		for (crd = crp->crp_desc; crd->crd_next; crd = crd->crd_next)
			crd->CRD_INI.cri_next = &(crd->crd_next->CRD_INI);

		/* XXX propagate flags from initial session? */
		if (crypto_newsession(&nid, &(crp->crp_desc->CRD_INI),
		    CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE) == 0)
			crp->crp_sid = nid;

		crp->crp_etype = EAGAIN;
		crypto_done(crp);
		return 0;
	} else {
		/*
		 * Invoke the driver to process the request.
		 */
		return CRYPTODEV_PROCESS(cap->cc_dev, crp, hint);
	}
}

/*
 * Release a set of crypto descriptors.
 */
void
crypto_freereq(struct cryptop *crp)
{
	struct cryptodesc *crd;

	if (crp == NULL)
		return;

#ifdef DIAGNOSTIC
	{
		struct cryptop *crp2;

		CRYPTO_Q_LOCK();
		TAILQ_FOREACH(crp2, &crp_q, crp_next) {
			KASSERT(crp2 != crp,
			    ("Freeing cryptop from the crypto queue (%p).",
			    crp));
		}
		CRYPTO_Q_UNLOCK();
		CRYPTO_RETQ_LOCK();
		TAILQ_FOREACH(crp2, &crp_ret_q, crp_next) {
			KASSERT(crp2 != crp,
			    ("Freeing cryptop from the return queue (%p).",
			    crp));
		}
		CRYPTO_RETQ_UNLOCK();
	}
#endif

	while ((crd = crp->crp_desc) != NULL) {
		crp->crp_desc = crd->crd_next;
		uma_zfree(cryptodesc_zone, crd);
	}
	uma_zfree(cryptop_zone, crp);
}

/*
 * Acquire a set of crypto descriptors.
 */
struct cryptop *
crypto_getreq(int num)
{
	struct cryptodesc *crd;
	struct cryptop *crp;

	crp = uma_zalloc(cryptop_zone, M_NOWAIT|M_ZERO);
	if (crp != NULL) {
		while (num--) {
			crd = uma_zalloc(cryptodesc_zone, M_NOWAIT|M_ZERO);
			if (crd == NULL) {
				crypto_freereq(crp);
				return NULL;
			}

			crd->crd_next = crp->crp_desc;
			crp->crp_desc = crd;
		}
	}
	return crp;
}

/*
 * Invoke the callback on behalf of the driver.
 */
void
crypto_done(struct cryptop *crp)
{
	KASSERT((crp->crp_flags & CRYPTO_F_DONE) == 0,
		("crypto_done: op already done, flags 0x%x", crp->crp_flags));
	crp->crp_flags |= CRYPTO_F_DONE;
	if (crp->crp_etype != 0)
		cryptostats.cs_errs++;
#ifdef CRYPTO_TIMING
	if (crypto_timing)
		crypto_tstat(&cryptostats.cs_done, &crp->crp_tstamp);
#endif
	/*
	 * CBIMM means unconditionally do the callback immediately;
	 * CBIFSYNC means do the callback immediately only if the
	 * operation was done synchronously.  Both are used to avoid
	 * doing extraneous context switches; the latter is mostly
	 * used with the software crypto driver.
	 */
	if ((crp->crp_flags & CRYPTO_F_CBIMM) ||
	    ((crp->crp_flags & CRYPTO_F_CBIFSYNC) &&
	     (CRYPTO_SESID2CAPS(crp->crp_sid) & CRYPTOCAP_F_SYNC))) {
		/*
		 * Do the callback directly.  This is ok when the
		 * callback routine does very little (e.g. the
		 * /dev/crypto callback method just does a wakeup).
		 */
#ifdef CRYPTO_TIMING
		if (crypto_timing) {
			/*
			 * NB: We must copy the timestamp before
			 * doing the callback as the cryptop is
			 * likely to be reclaimed.
			 */
			struct bintime t = crp->crp_tstamp;
			crypto_tstat(&cryptostats.cs_cb, &t);
			crp->crp_callback(crp);
			crypto_tstat(&cryptostats.cs_finis, &t);
		} else
#endif
			crp->crp_callback(crp);
	} else {
		/*
		 * Normal case; queue the callback for the thread.
		 */
		CRYPTO_RETQ_LOCK();
		if (CRYPTO_RETQ_EMPTY())
			wakeup_one(&crp_ret_q);	/* shared wait channel */
		TAILQ_INSERT_TAIL(&crp_ret_q, crp, crp_next);
		CRYPTO_RETQ_UNLOCK();
	}
}

/*
 * Invoke the callback on behalf of the driver.
 */
void
crypto_kdone(struct cryptkop *krp)
{
	struct cryptocap *cap;

	if (krp->krp_status != 0)
		cryptostats.cs_kerrs++;
	CRYPTO_DRIVER_LOCK();
	/* XXX: What if driver is loaded in the meantime? */
	if (krp->krp_hid < crypto_drivers_num) {
		cap = &crypto_drivers[krp->krp_hid];
		KASSERT(cap->cc_koperations > 0, ("cc_koperations == 0"));
		cap->cc_koperations--;
		if (cap->cc_flags & CRYPTOCAP_F_CLEANUP)
			crypto_remove(cap);
	}
	CRYPTO_DRIVER_UNLOCK();
	CRYPTO_RETQ_LOCK();
	if (CRYPTO_RETQ_EMPTY())
		wakeup_one(&crp_ret_q);		/* shared wait channel */
	TAILQ_INSERT_TAIL(&crp_ret_kq, krp, krp_next);
	CRYPTO_RETQ_UNLOCK();
}

int
crypto_getfeat(int *featp)
{
	int hid, kalg, feat = 0;

	CRYPTO_DRIVER_LOCK();
	for (hid = 0; hid < crypto_drivers_num; hid++) {
		const struct cryptocap *cap = &crypto_drivers[hid];

		if ((cap->cc_flags & CRYPTOCAP_F_SOFTWARE) &&
		    !crypto_devallowsoft) {
			continue;
		}
		for (kalg = 0; kalg < CRK_ALGORITHM_MAX; kalg++)
			if (cap->cc_kalg[kalg] & CRYPTO_ALG_FLAG_SUPPORTED)
				feat |=  1 << kalg;
	}
	CRYPTO_DRIVER_UNLOCK();
	*featp = feat;
	return (0);
}

/*
 * Terminate a thread at module unload.  The process that
 * initiated this is waiting for us to signal that we're gone;
 * wake it up and exit.  We use the driver table lock to insure
 * we don't do the wakeup before they're waiting.  There is no
 * race here because the waiter sleeps on the proc lock for the
 * thread so it gets notified at the right time because of an
 * extra wakeup that's done in exit1().
 */
static void
crypto_finis(void *chan)
{
	CRYPTO_DRIVER_LOCK();
	wakeup_one(chan);
	CRYPTO_DRIVER_UNLOCK();
	kproc_exit(0);
}

/*
 * Crypto thread, dispatches crypto requests.
 */
static void
crypto_proc(void)
{
	struct cryptop *crp, *submit;
	struct cryptkop *krp;
	struct cryptocap *cap;
	u_int32_t hid;
	int result, hint;

#if defined(__i386__) || defined(__amd64__)
	fpu_kern_thread(FPU_KERN_NORMAL);
#endif

	CRYPTO_Q_LOCK();
	for (;;) {
		/*
		 * Find the first element in the queue that can be
		 * processed and look-ahead to see if multiple ops
		 * are ready for the same driver.
		 */
		submit = NULL;
		hint = 0;
		TAILQ_FOREACH(crp, &crp_q, crp_next) {
			hid = CRYPTO_SESID2HID(crp->crp_sid);
			cap = crypto_checkdriver(hid);
			/*
			 * Driver cannot disappeared when there is an active
			 * session.
			 */
			KASSERT(cap != NULL, ("%s:%u Driver disappeared.",
			    __func__, __LINE__));
			if (cap == NULL || cap->cc_dev == NULL) {
				/* Op needs to be migrated, process it. */
				if (submit == NULL)
					submit = crp;
				break;
			}
			if (!cap->cc_qblocked) {
				if (submit != NULL) {
					/*
					 * We stop on finding another op,
					 * regardless whether its for the same
					 * driver or not.  We could keep
					 * searching the queue but it might be
					 * better to just use a per-driver
					 * queue instead.
					 */
					if (CRYPTO_SESID2HID(submit->crp_sid) == hid)
						hint = CRYPTO_HINT_MORE;
					break;
				} else {
					submit = crp;
					if ((submit->crp_flags & CRYPTO_F_BATCH) == 0)
						break;
					/* keep scanning for more are q'd */
				}
			}
		}
		if (submit != NULL) {
			TAILQ_REMOVE(&crp_q, submit, crp_next);
			hid = CRYPTO_SESID2HID(submit->crp_sid);
			cap = crypto_checkdriver(hid);
			KASSERT(cap != NULL, ("%s:%u Driver disappeared.",
			    __func__, __LINE__));
			result = crypto_invoke(cap, submit, hint);
			if (result == ERESTART) {
				/*
				 * The driver ran out of resources, mark the
				 * driver ``blocked'' for cryptop's and put
				 * the request back in the queue.  It would
				 * best to put the request back where we got
				 * it but that's hard so for now we put it
				 * at the front.  This should be ok; putting
				 * it at the end does not work.
				 */
				/* XXX validate sid again? */
				crypto_drivers[CRYPTO_SESID2HID(submit->crp_sid)].cc_qblocked = 1;
				TAILQ_INSERT_HEAD(&crp_q, submit, crp_next);
				cryptostats.cs_blocks++;
			}
		}

		/* As above, but for key ops */
		TAILQ_FOREACH(krp, &crp_kq, krp_next) {
			cap = crypto_checkdriver(krp->krp_hid);
			if (cap == NULL || cap->cc_dev == NULL) {
				/*
				 * Operation needs to be migrated, invalidate
				 * the assigned device so it will reselect a
				 * new one below.  Propagate the original
				 * crid selection flags if supplied.
				 */
				krp->krp_hid = krp->krp_crid &
				    (CRYPTOCAP_F_SOFTWARE|CRYPTOCAP_F_HARDWARE);
				if (krp->krp_hid == 0)
					krp->krp_hid =
				    CRYPTOCAP_F_SOFTWARE|CRYPTOCAP_F_HARDWARE;
				break;
			}
			if (!cap->cc_kqblocked)
				break;
		}
		if (krp != NULL) {
			TAILQ_REMOVE(&crp_kq, krp, krp_next);
			result = crypto_kinvoke(krp, krp->krp_hid);
			if (result == ERESTART) {
				/*
				 * The driver ran out of resources, mark the
				 * driver ``blocked'' for cryptkop's and put
				 * the request back in the queue.  It would
				 * best to put the request back where we got
				 * it but that's hard so for now we put it
				 * at the front.  This should be ok; putting
				 * it at the end does not work.
				 */
				/* XXX validate sid again? */
				crypto_drivers[krp->krp_hid].cc_kqblocked = 1;
				TAILQ_INSERT_HEAD(&crp_kq, krp, krp_next);
				cryptostats.cs_kblocks++;
			}
		}

		if (submit == NULL && krp == NULL) {
			/*
			 * Nothing more to be processed.  Sleep until we're
			 * woken because there are more ops to process.
			 * This happens either by submission or by a driver
			 * becoming unblocked and notifying us through
			 * crypto_unblock.  Note that when we wakeup we
			 * start processing each queue again from the
			 * front. It's not clear that it's important to
			 * preserve this ordering since ops may finish
			 * out of order if dispatched to different devices
			 * and some become blocked while others do not.
			 */
			crp_sleep = 1;
			msleep(&crp_q, &crypto_q_mtx, PWAIT, "crypto_wait", 0);
			crp_sleep = 0;
			if (cryptoproc == NULL)
				break;
			cryptostats.cs_intrs++;
		}
	}
	CRYPTO_Q_UNLOCK();

	crypto_finis(&crp_q);
}

/*
 * Crypto returns thread, does callbacks for processed crypto requests.
 * Callbacks are done here, rather than in the crypto drivers, because
 * callbacks typically are expensive and would slow interrupt handling.
 */
static void
crypto_ret_proc(void)
{
	struct cryptop *crpt;
	struct cryptkop *krpt;

	CRYPTO_RETQ_LOCK();
	for (;;) {
		/* Harvest return q's for completed ops */
		crpt = TAILQ_FIRST(&crp_ret_q);
		if (crpt != NULL)
			TAILQ_REMOVE(&crp_ret_q, crpt, crp_next);

		krpt = TAILQ_FIRST(&crp_ret_kq);
		if (krpt != NULL)
			TAILQ_REMOVE(&crp_ret_kq, krpt, krp_next);

		if (crpt != NULL || krpt != NULL) {
			CRYPTO_RETQ_UNLOCK();
			/*
			 * Run callbacks unlocked.
			 */
			if (crpt != NULL) {
#ifdef CRYPTO_TIMING
				if (crypto_timing) {
					/*
					 * NB: We must copy the timestamp before
					 * doing the callback as the cryptop is
					 * likely to be reclaimed.
					 */
					struct bintime t = crpt->crp_tstamp;
					crypto_tstat(&cryptostats.cs_cb, &t);
					crpt->crp_callback(crpt);
					crypto_tstat(&cryptostats.cs_finis, &t);
				} else
#endif
					crpt->crp_callback(crpt);
			}
			if (krpt != NULL)
				krpt->krp_callback(krpt);
			CRYPTO_RETQ_LOCK();
		} else {
			/*
			 * Nothing more to be processed.  Sleep until we're
			 * woken because there are more returns to process.
			 */
			msleep(&crp_ret_q, &crypto_ret_q_mtx, PWAIT,
				"crypto_ret_wait", 0);
			if (cryptoretproc == NULL)
				break;
			cryptostats.cs_rets++;
		}
	}
	CRYPTO_RETQ_UNLOCK();

	crypto_finis(&crp_ret_q);
}

#ifdef DDB
static void
db_show_drivers(void)
{
	int hid;

	db_printf("%12s %4s %4s %8s %2s %2s\n"
		, "Device"
		, "Ses"
		, "Kops"
		, "Flags"
		, "QB"
		, "KB"
	);
	for (hid = 0; hid < crypto_drivers_num; hid++) {
		const struct cryptocap *cap = &crypto_drivers[hid];
		if (cap->cc_dev == NULL)
			continue;
		db_printf("%-12s %4u %4u %08x %2u %2u\n"
		    , device_get_nameunit(cap->cc_dev)
		    , cap->cc_sessions
		    , cap->cc_koperations
		    , cap->cc_flags
		    , cap->cc_qblocked
		    , cap->cc_kqblocked
		);
	}
}

DB_SHOW_COMMAND(crypto, db_show_crypto)
{
	struct cryptop *crp;

	db_show_drivers();
	db_printf("\n");

	db_printf("%4s %8s %4s %4s %4s %4s %8s %8s\n",
	    "HID", "Caps", "Ilen", "Olen", "Etype", "Flags",
	    "Desc", "Callback");
	TAILQ_FOREACH(crp, &crp_q, crp_next) {
		db_printf("%4u %08x %4u %4u %4u %04x %8p %8p\n"
		    , (int) CRYPTO_SESID2HID(crp->crp_sid)
		    , (int) CRYPTO_SESID2CAPS(crp->crp_sid)
		    , crp->crp_ilen, crp->crp_olen
		    , crp->crp_etype
		    , crp->crp_flags
		    , crp->crp_desc
		    , crp->crp_callback
		);
	}
	if (!TAILQ_EMPTY(&crp_ret_q)) {
		db_printf("\n%4s %4s %4s %8s\n",
		    "HID", "Etype", "Flags", "Callback");
		TAILQ_FOREACH(crp, &crp_ret_q, crp_next) {
			db_printf("%4u %4u %04x %8p\n"
			    , (int) CRYPTO_SESID2HID(crp->crp_sid)
			    , crp->crp_etype
			    , crp->crp_flags
			    , crp->crp_callback
			);
		}
	}
}

DB_SHOW_COMMAND(kcrypto, db_show_kcrypto)
{
	struct cryptkop *krp;

	db_show_drivers();
	db_printf("\n");

	db_printf("%4s %5s %4s %4s %8s %4s %8s\n",
	    "Op", "Status", "#IP", "#OP", "CRID", "HID", "Callback");
	TAILQ_FOREACH(krp, &crp_kq, krp_next) {
		db_printf("%4u %5u %4u %4u %08x %4u %8p\n"
		    , krp->krp_op
		    , krp->krp_status
		    , krp->krp_iparams, krp->krp_oparams
		    , krp->krp_crid, krp->krp_hid
		    , krp->krp_callback
		);
	}
	if (!TAILQ_EMPTY(&crp_ret_q)) {
		db_printf("%4s %5s %8s %4s %8s\n",
		    "Op", "Status", "CRID", "HID", "Callback");
		TAILQ_FOREACH(krp, &crp_ret_kq, krp_next) {
			db_printf("%4u %5u %08x %4u %8p\n"
			    , krp->krp_op
			    , krp->krp_status
			    , krp->krp_crid, krp->krp_hid
			    , krp->krp_callback
			);
		}
	}
}
#endif

int crypto_modevent(module_t mod, int type, void *unused);

/*
 * Initialization code, both for static and dynamic loading.
 * Note this is not invoked with the usual MODULE_DECLARE
 * mechanism but instead is listed as a dependency by the
 * cryptosoft driver.  This guarantees proper ordering of
 * calls on module load/unload.
 */
int
crypto_modevent(module_t mod, int type, void *unused)
{
	int error = EINVAL;

	switch (type) {
	case MOD_LOAD:
		error = crypto_init();
		if (error == 0 && bootverbose)
			printf("crypto: <crypto core>\n");
		break;
	case MOD_UNLOAD:
		/*XXX disallow if active sessions */
		error = 0;
		crypto_destroy();
		return 0;
	}
	return error;
}
MODULE_VERSION(crypto, 1);
MODULE_DEPEND(crypto, zlib, 1, 1, 1);
