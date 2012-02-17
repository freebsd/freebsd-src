/*-
 * Copyright (c) 1999-2002, 2006, 2009 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001-2005 Networks Associates Technology, Inc.
 * Copyright (c) 2005-2006 SPARTA, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson and Ilmar Habibulin for the
 * TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
 *
 * This software was enhanced by SPARTA ISSO under SPAWAR contract 
 * N66001-04-C-6019 ("SEFOS").
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
 */

/*-
 * Framework for extensible kernel access control.  This file contains core
 * kernel infrastructure for the TrustedBSD MAC Framework, including policy
 * registration, versioning, locking, error composition operator, and system
 * calls.
 *
 * The MAC Framework implements three programming interfaces:
 *
 * - The kernel MAC interface, defined in mac_framework.h, and invoked
 *   throughout the kernel to request security decisions, notify of security
 *   related events, etc.
 *
 * - The MAC policy module interface, defined in mac_policy.h, which is
 *   implemented by MAC policy modules and invoked by the MAC Framework to
 *   forward kernel security requests and notifications to policy modules.
 *
 * - The user MAC API, defined in mac.h, which allows user programs to query
 *   and set label state on objects.
 *
 * The majority of the MAC Framework implementation may be found in
 * src/sys/security/mac.  Sample policy modules may be found in
 * src/sys/security/mac_*.
 */

#include "opt_kdtrace.h"
#include "opt_mac.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mac.h>
#include <sys/module.h>
#include <sys/sdt.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <security/mac/mac_framework.h>
#include <security/mac/mac_internal.h>
#include <security/mac/mac_policy.h>

/*
 * DTrace SDT provider for MAC.
 */
SDT_PROVIDER_DEFINE(mac);
SDT_PROBE_DEFINE2(mac, kernel, policy, modevent, "int",
    "struct mac_policy_conf *mpc");
SDT_PROBE_DEFINE1(mac, kernel, policy, register, "struct mac_policy_conf *");
SDT_PROBE_DEFINE1(mac, kernel, policy, unregister, "struct mac_policy_conf *");

/*
 * Root sysctl node for all MAC and MAC policy controls.
 */
SYSCTL_NODE(_security, OID_AUTO, mac, CTLFLAG_RW, 0,
    "TrustedBSD MAC policy controls");

/*
 * Declare that the kernel provides MAC support, version 3 (FreeBSD 7.x).
 * This permits modules to refuse to be loaded if the necessary support isn't
 * present, even if it's pre-boot.
 */
MODULE_VERSION(kernel_mac_support, MAC_VERSION);

static unsigned int	mac_version = MAC_VERSION;
SYSCTL_UINT(_security_mac, OID_AUTO, version, CTLFLAG_RD, &mac_version, 0,
    "");

/*
 * Labels consist of a indexed set of "slots", which are allocated policies
 * as required.  The MAC Framework maintains a bitmask of slots allocated so
 * far to prevent reuse.  Slots cannot be reused, as the MAC Framework
 * guarantees that newly allocated slots in labels will be NULL unless
 * otherwise initialized, and because we do not have a mechanism to garbage
 * collect slots on policy unload.  As labeled policies tend to be statically
 * loaded during boot, and not frequently unloaded and reloaded, this is not
 * generally an issue.
 */
#if MAC_MAX_SLOTS > 32
#error "MAC_MAX_SLOTS too large"
#endif

static unsigned int mac_max_slots = MAC_MAX_SLOTS;
static unsigned int mac_slot_offsets_free = (1 << MAC_MAX_SLOTS) - 1;
SYSCTL_UINT(_security_mac, OID_AUTO, max_slots, CTLFLAG_RD, &mac_max_slots,
    0, "");

/*
 * Has the kernel started generating labeled objects yet?  All read/write
 * access to this variable is serialized during the boot process.  Following
 * the end of serialization, we don't update this flag; no locking.
 */
static int	mac_late = 0;

/*
 * Flag to indicate whether or not we should allocate label storage for new
 * mbufs.  Since most dynamic policies we currently work with don't rely on
 * mbuf labeling, try to avoid paying the cost of mtag allocation unless
 * specifically notified of interest.  One result of this is that if a
 * dynamically loaded policy requests mbuf labels, it must be able to deal
 * with a NULL label being returned on any mbufs that were already in flight
 * when the policy was loaded.  Since the policy already has to deal with
 * uninitialized labels, this probably won't be a problem.  Note: currently
 * no locking.  Will this be a problem?
 *
 * In the future, we may want to allow objects to request labeling on a per-
 * object type basis, rather than globally for all objects.
 */
#ifndef MAC_ALWAYS_LABEL_MBUF
int	mac_labelmbufs = 0;
#endif

MALLOC_DEFINE(M_MACTEMP, "mactemp", "MAC temporary label storage");

/*
 * mac_static_policy_list holds a list of policy modules that are not loaded
 * while the system is "live", and cannot be unloaded.  These policies can be
 * invoked without holding the busy count.
 *
 * mac_policy_list stores the list of dynamic policies.  A busy count is
 * maintained for the list, stored in mac_policy_busy.  The busy count is
 * protected by mac_policy_mtx; the list may be modified only while the busy
 * count is 0, requiring that the lock be held to prevent new references to
 * the list from being acquired.  For almost all operations, incrementing the
 * busy count is sufficient to guarantee consistency, as the list cannot be
 * modified while the busy count is elevated.  For a few special operations
 * involving a change to the list of active policies, the mtx itself must be
 * held.  A condition variable, mac_policy_cv, is used to signal potential
 * exclusive consumers that they should try to acquire the lock if a first
 * attempt at exclusive access fails.
 *
 * This design intentionally avoids fairness, and may starve attempts to
 * acquire an exclusive lock on a busy system.  This is required because we
 * do not ever want acquiring a read reference to perform an unbounded length
 * sleep.  Read references are acquired in ithreads, network isrs, etc, and
 * any unbounded blocking could lead quickly to deadlock.
 *
 * Another reason for never blocking on read references is that the MAC
 * Framework may recurse: if a policy calls a VOP, for example, this might
 * lead to vnode life cycle operations (such as init/destroy).
 *
 * If the kernel option MAC_STATIC has been compiled in, all locking becomes
 * a no-op, and the global list of policies is not allowed to change after
 * early boot.
 *
 * XXXRW: Currently, we signal mac_policy_cv every time the framework becomes
 * unbusy and there is a thread waiting to enter it exclusively.  Since it 
 * may take some time before the thread runs, we may issue a lot of signals.
 * We should instead keep track of the fact that we've signalled, taking into 
 * account that the framework may be busy again by the time the thread runs, 
 * requiring us to re-signal. 
 */
#ifndef MAC_STATIC
static struct mtx mac_policy_mtx;
static struct cv mac_policy_cv;
static int mac_policy_count;
static int mac_policy_wait;
#endif
struct mac_policy_list_head mac_policy_list;
struct mac_policy_list_head mac_static_policy_list;

/*
 * We manually invoke WITNESS_WARN() to allow Witness to generate warnings
 * even if we don't end up ever triggering the wait at run-time.  The
 * consumer of the exclusive interface must not hold any locks (other than
 * potentially Giant) since we may sleep for long (potentially indefinite)
 * periods of time waiting for the framework to become quiescent so that a
 * policy list change may be made.
 */
void
mac_policy_grab_exclusive(void)
{

#ifndef MAC_STATIC
	if (!mac_late)
		return;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
 	    "mac_policy_grab_exclusive() at %s:%d", __FILE__, __LINE__);
	mtx_lock(&mac_policy_mtx);
	while (mac_policy_count != 0) {
		mac_policy_wait++;
		cv_wait(&mac_policy_cv, &mac_policy_mtx);
		mac_policy_wait--;
	}
#endif
}

void
mac_policy_assert_exclusive(void)
{

#ifndef MAC_STATIC
	if (!mac_late)
		return;

	mtx_assert(&mac_policy_mtx, MA_OWNED);
	KASSERT(mac_policy_count == 0,
	    ("mac_policy_assert_exclusive(): not exclusive"));
#endif
}

void
mac_policy_release_exclusive(void)
{
#ifndef MAC_STATIC
	int dowakeup;

	if (!mac_late)
		return;

	KASSERT(mac_policy_count == 0,
	    ("mac_policy_release_exclusive(): not exclusive"));
	dowakeup = (mac_policy_wait != 0);
	mtx_unlock(&mac_policy_mtx);
	if (dowakeup)
		cv_signal(&mac_policy_cv);
#endif
}

void
mac_policy_list_busy(void)
{

#ifndef MAC_STATIC
	if (!mac_late)
		return;

	mtx_lock(&mac_policy_mtx);
	mac_policy_count++;
	mtx_unlock(&mac_policy_mtx);
#endif
}

int
mac_policy_list_conditional_busy(void)
{
#ifndef MAC_STATIC
	int ret;

	if (!mac_late)
		return (1);

	mtx_lock(&mac_policy_mtx);
	if (!LIST_EMPTY(&mac_policy_list)) {
		mac_policy_count++;
		ret = 1;
	} else
		ret = 0;
	mtx_unlock(&mac_policy_mtx);
	return (ret);
#else
	return (1);
#endif
}

void
mac_policy_list_unbusy(void)
{
#ifndef MAC_STATIC
	int dowakeup;

	if (!mac_late)
		return;

	mtx_lock(&mac_policy_mtx);
	mac_policy_count--;
	KASSERT(mac_policy_count >= 0, ("MAC_POLICY_LIST_LOCK"));
	dowakeup = (mac_policy_count == 0 && mac_policy_wait != 0);
	mtx_unlock(&mac_policy_mtx);

	if (dowakeup)
		cv_signal(&mac_policy_cv);
#endif
}

/*
 * Initialize the MAC subsystem, including appropriate SMP locks.
 */
static void
mac_init(void)
{

	LIST_INIT(&mac_static_policy_list);
	LIST_INIT(&mac_policy_list);
	mac_labelzone_init();

#ifndef MAC_STATIC
	mtx_init(&mac_policy_mtx, "mac_policy_mtx", NULL, MTX_DEF);
	cv_init(&mac_policy_cv, "mac_policy_cv");
#endif
}

/*
 * For the purposes of modules that want to know if they were loaded "early",
 * set the mac_late flag once we've processed modules either linked into the
 * kernel, or loaded before the kernel startup.
 */
static void
mac_late_init(void)
{

	mac_late = 1;
}

/*
 * After the policy list has changed, walk the list to update any global
 * flags.  Currently, we support only one flag, and it's conditionally
 * defined; as a result, the entire function is conditional.  Eventually, the
 * #else case might also iterate across the policies.
 */
static void
mac_policy_updateflags(void)
{
#ifndef MAC_ALWAYS_LABEL_MBUF
	struct mac_policy_conf *tmpc;
	int labelmbufs;

	mac_policy_assert_exclusive();

	labelmbufs = 0;
	LIST_FOREACH(tmpc, &mac_static_policy_list, mpc_list) {
		if (tmpc->mpc_loadtime_flags & MPC_LOADTIME_FLAG_LABELMBUFS)
			labelmbufs++;
	}
	LIST_FOREACH(tmpc, &mac_policy_list, mpc_list) {
		if (tmpc->mpc_loadtime_flags & MPC_LOADTIME_FLAG_LABELMBUFS)
			labelmbufs++;
	}
	mac_labelmbufs = (labelmbufs != 0);
#endif
}

static int
mac_policy_register(struct mac_policy_conf *mpc)
{
	struct mac_policy_conf *tmpc;
	int error, slot, static_entry;

	error = 0;

	/*
	 * We don't technically need exclusive access while !mac_late, but
	 * hold it for assertion consistency.
	 */
	mac_policy_grab_exclusive();

	/*
	 * If the module can potentially be unloaded, or we're loading late,
	 * we have to stick it in the non-static list and pay an extra
	 * performance overhead.  Otherwise, we can pay a light locking cost
	 * and stick it in the static list.
	 */
	static_entry = (!mac_late &&
	    !(mpc->mpc_loadtime_flags & MPC_LOADTIME_FLAG_UNLOADOK));

	if (static_entry) {
		LIST_FOREACH(tmpc, &mac_static_policy_list, mpc_list) {
			if (strcmp(tmpc->mpc_name, mpc->mpc_name) == 0) {
				error = EEXIST;
				goto out;
			}
		}
	} else {
		LIST_FOREACH(tmpc, &mac_policy_list, mpc_list) {
			if (strcmp(tmpc->mpc_name, mpc->mpc_name) == 0) {
				error = EEXIST;
				goto out;
			}
		}
	}
	if (mpc->mpc_field_off != NULL) {
		slot = ffs(mac_slot_offsets_free);
		if (slot == 0) {
			error = ENOMEM;
			goto out;
		}
		slot--;
		mac_slot_offsets_free &= ~(1 << slot);
		*mpc->mpc_field_off = slot;
	}
	mpc->mpc_runtime_flags |= MPC_RUNTIME_FLAG_REGISTERED;

	/*
	 * If we're loading a MAC module after the framework has initialized,
	 * it has to go into the dynamic list.  If we're loading it before
	 * we've finished initializing, it can go into the static list with
	 * weaker locker requirements.
	 */
	if (static_entry)
		LIST_INSERT_HEAD(&mac_static_policy_list, mpc, mpc_list);
	else
		LIST_INSERT_HEAD(&mac_policy_list, mpc, mpc_list);

	/*
	 * Per-policy initialization.  Currently, this takes place under the
	 * exclusive lock, so policies must not sleep in their init method.
	 * In the future, we may want to separate "init" from "start", with
	 * "init" occuring without the lock held.  Likewise, on tear-down,
	 * breaking out "stop" from "destroy".
	 */
	if (mpc->mpc_ops->mpo_init != NULL)
		(*(mpc->mpc_ops->mpo_init))(mpc);
	mac_policy_updateflags();

	SDT_PROBE(mac, kernel, policy, register, mpc, 0, 0, 0, 0);
	printf("Security policy loaded: %s (%s)\n", mpc->mpc_fullname,
	    mpc->mpc_name);

out:
	mac_policy_release_exclusive();
	return (error);
}

static int
mac_policy_unregister(struct mac_policy_conf *mpc)
{

	/*
	 * If we fail the load, we may get a request to unload.  Check to see
	 * if we did the run-time registration, and if not, silently succeed.
	 */
	mac_policy_grab_exclusive();
	if ((mpc->mpc_runtime_flags & MPC_RUNTIME_FLAG_REGISTERED) == 0) {
		mac_policy_release_exclusive();
		return (0);
	}
#if 0
	/*
	 * Don't allow unloading modules with private data.
	 */
	if (mpc->mpc_field_off != NULL) {
		MAC_POLICY_LIST_UNLOCK();
		return (EBUSY);
	}
#endif
	/*
	 * Only allow the unload to proceed if the module is unloadable by
	 * its own definition.
	 */
	if ((mpc->mpc_loadtime_flags & MPC_LOADTIME_FLAG_UNLOADOK) == 0) {
		mac_policy_release_exclusive();
		return (EBUSY);
	}
	if (mpc->mpc_ops->mpo_destroy != NULL)
		(*(mpc->mpc_ops->mpo_destroy))(mpc);

	LIST_REMOVE(mpc, mpc_list);
	mpc->mpc_runtime_flags &= ~MPC_RUNTIME_FLAG_REGISTERED;
	mac_policy_updateflags();

	mac_policy_release_exclusive();

	SDT_PROBE(mac, kernel, policy, unregister, mpc, 0, 0, 0, 0);
	printf("Security policy unload: %s (%s)\n", mpc->mpc_fullname,
	    mpc->mpc_name);

	return (0);
}

/*
 * Allow MAC policy modules to register during boot, etc.
 */
int
mac_policy_modevent(module_t mod, int type, void *data)
{
	struct mac_policy_conf *mpc;
	int error;

	error = 0;
	mpc = (struct mac_policy_conf *) data;

#ifdef MAC_STATIC
	if (mac_late) {
		printf("mac_policy_modevent: MAC_STATIC and late\n");
		return (EBUSY);
	}
#endif

	SDT_PROBE(mac, kernel, policy, modevent, type, mpc, 0, 0, 0);
	switch (type) {
	case MOD_LOAD:
		if (mpc->mpc_loadtime_flags & MPC_LOADTIME_FLAG_NOTLATE &&
		    mac_late) {
			printf("mac_policy_modevent: can't load %s policy "
			    "after booting\n", mpc->mpc_name);
			error = EBUSY;
			break;
		}
		error = mac_policy_register(mpc);
		break;
	case MOD_UNLOAD:
		/* Don't unregister the module if it was never registered. */
		if ((mpc->mpc_runtime_flags & MPC_RUNTIME_FLAG_REGISTERED)
		    != 0)
			error = mac_policy_unregister(mpc);
		else
			error = 0;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

/*
 * Define an error value precedence, and given two arguments, selects the
 * value with the higher precedence.
 */
int
mac_error_select(int error1, int error2)
{

	/* Certain decision-making errors take top priority. */
	if (error1 == EDEADLK || error2 == EDEADLK)
		return (EDEADLK);

	/* Invalid arguments should be reported where possible. */
	if (error1 == EINVAL || error2 == EINVAL)
		return (EINVAL);

	/* Precedence goes to "visibility", with both process and file. */
	if (error1 == ESRCH || error2 == ESRCH)
		return (ESRCH);

	if (error1 == ENOENT || error2 == ENOENT)
		return (ENOENT);

	/* Precedence goes to DAC/MAC protections. */
	if (error1 == EACCES || error2 == EACCES)
		return (EACCES);

	/* Precedence goes to privilege. */
	if (error1 == EPERM || error2 == EPERM)
		return (EPERM);

	/* Precedence goes to error over success; otherwise, arbitrary. */
	if (error1 != 0)
		return (error1);
	return (error2);
}

int
mac_check_structmac_consistent(struct mac *mac)
{

	if (mac->m_buflen < 0 ||
	    mac->m_buflen > MAC_MAX_LABEL_BUF_LEN)
		return (EINVAL);

	return (0);
}

SYSINIT(mac, SI_SUB_MAC, SI_ORDER_FIRST, mac_init, NULL);
SYSINIT(mac_late, SI_SUB_MAC_LATE, SI_ORDER_FIRST, mac_late_init, NULL);
