/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001, 2002 Networks Associates Technology, Inc.
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
/*
 * Developed by the TrustedBSD Project.
 *
 * Framework for extensible kernel access control.  Kernel and userland
 * interface to the framework, policy registration and composition.
 */

#include "opt_mac.h"
#include "opt_devfs.h"

#include <sys/param.h>
#include <sys/extattr.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mac.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/socket.h>
#include <sys/pipe.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>

#include <sys/mac_policy.h>

#include <fs/devfs/devfs.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>

#ifdef MAC

/*
 * Declare that the kernel provides MAC support, version 1.  This permits
 * modules to refuse to be loaded if the necessary support isn't present,
 * even if it's pre-boot.
 */
MODULE_VERSION(kernel_mac_support, 1);

SYSCTL_DECL(_security);

SYSCTL_NODE(_security, OID_AUTO, mac, CTLFLAG_RW, 0,
    "TrustedBSD MAC policy controls");

#if MAC_MAX_POLICIES > 32
#error "MAC_MAX_POLICIES too large"
#endif

static unsigned int mac_max_policies = MAC_MAX_POLICIES;
static unsigned int mac_policy_offsets_free = (1 << MAC_MAX_POLICIES) - 1;
SYSCTL_UINT(_security_mac, OID_AUTO, max_policies, CTLFLAG_RD,
    &mac_max_policies, 0, "");

/*
 * Has the kernel started generating labeled objects yet?  All read/write
 * access to this variable is serialized during the boot process.  Following
 * the end of serialization, we don't update this flag; no locking.
 */
static int	mac_late = 0;

/*
 * Warn about EA transactions only the first time they happen.
 * Weak coherency, no locking.
 */
static int	ea_warn_once = 0;

static int	mac_enforce_fs = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_fs, CTLFLAG_RW,
    &mac_enforce_fs, 0, "Enforce MAC policy on file system objects");
TUNABLE_INT("security.mac.enforce_fs", &mac_enforce_fs);

static int	mac_enforce_network = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_network, CTLFLAG_RW,
    &mac_enforce_network, 0, "Enforce MAC policy on network packets");
TUNABLE_INT("security.mac.enforce_network", &mac_enforce_network);

static int	mac_enforce_pipe = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_pipe, CTLFLAG_RW,
    &mac_enforce_pipe, 0, "Enforce MAC policy on pipe operations");
TUNABLE_INT("security.mac.enforce_pipe", &mac_enforce_pipe);

static int	mac_enforce_process = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_process, CTLFLAG_RW,
    &mac_enforce_process, 0, "Enforce MAC policy on inter-process operations");
TUNABLE_INT("security.mac.enforce_process", &mac_enforce_process);

static int	mac_enforce_socket = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_socket, CTLFLAG_RW,
    &mac_enforce_socket, 0, "Enforce MAC policy on socket operations");
TUNABLE_INT("security.mac.enforce_socket", &mac_enforce_socket);

static int	mac_enforce_system = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_system, CTLFLAG_RW,
    &mac_enforce_system, 0, "Enforce MAC policy on system operations");
TUNABLE_INT("security.mac.enforce_system", &mac_enforce_system);

static int	mac_enforce_vm = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_vm, CTLFLAG_RW,
    &mac_enforce_vm, 0, "Enforce MAC policy on vm operations");
TUNABLE_INT("security.mac.enforce_vm", &mac_enforce_vm);

static int	mac_mmap_revocation = 1;
SYSCTL_INT(_security_mac, OID_AUTO, mmap_revocation, CTLFLAG_RW,
    &mac_mmap_revocation, 0, "Revoke mmap access to files on subject "
    "relabel");
static int	mac_mmap_revocation_via_cow = 0;
SYSCTL_INT(_security_mac, OID_AUTO, mmap_revocation_via_cow, CTLFLAG_RW,
    &mac_mmap_revocation_via_cow, 0, "Revoke mmap access to files via "
    "copy-on-write semantics, or by removing all write access");

#ifdef MAC_DEBUG
SYSCTL_NODE(_security_mac, OID_AUTO, debug, CTLFLAG_RW, 0,
    "TrustedBSD MAC debug info");

static int	mac_debug_label_fallback = 0;
SYSCTL_INT(_security_mac_debug, OID_AUTO, label_fallback, CTLFLAG_RW,
    &mac_debug_label_fallback, 0, "Filesystems should fall back to fs label"
    "when label is corrupted.");
TUNABLE_INT("security.mac.debug_label_fallback",
    &mac_debug_label_fallback);

SYSCTL_NODE(_security_mac_debug, OID_AUTO, counters, CTLFLAG_RW, 0,
    "TrustedBSD MAC object counters");

static unsigned int nmacmbufs, nmaccreds, nmacifnets, nmacbpfdescs,
    nmacsockets, nmacmounts, nmactemp, nmacvnodes, nmacdevfsdirents,
    nmacipqs, nmacpipes;

SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, mbufs, CTLFLAG_RD,
    &nmacmbufs, 0, "number of mbufs in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, creds, CTLFLAG_RD,
    &nmaccreds, 0, "number of ucreds in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, ifnets, CTLFLAG_RD,
    &nmacifnets, 0, "number of ifnets in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, ipqs, CTLFLAG_RD,
    &nmacipqs, 0, "number of ipqs in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, bpfdescs, CTLFLAG_RD,
    &nmacbpfdescs, 0, "number of bpfdescs in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, sockets, CTLFLAG_RD,
    &nmacsockets, 0, "number of sockets in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, pipes, CTLFLAG_RD,
    &nmacpipes, 0, "number of pipes in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, mounts, CTLFLAG_RD,
    &nmacmounts, 0, "number of mounts in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, temp, CTLFLAG_RD,
    &nmactemp, 0, "number of temporary labels in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, vnodes, CTLFLAG_RD,
    &nmacvnodes, 0, "number of vnodes in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, devfsdirents, CTLFLAG_RD,
    &nmacdevfsdirents, 0, "number of devfs dirents inuse");
#endif

static int	error_select(int error1, int error2);
static int	mac_policy_register(struct mac_policy_conf *mpc);
static int	mac_policy_unregister(struct mac_policy_conf *mpc);

static void	mac_check_vnode_mmap_downgrade(struct ucred *cred,
		    struct vnode *vp, int *prot);
static void	mac_cred_mmapped_drop_perms_recurse(struct thread *td,
		    struct ucred *cred, struct vm_map *map);

static void	mac_destroy_socket_label(struct label *label);

static int	mac_setlabel_vnode_extattr(struct ucred *cred,
		    struct vnode *vp, struct label *intlabel);


MALLOC_DEFINE(M_MACOPVEC, "macopvec", "MAC policy operation vector");
MALLOC_DEFINE(M_MACPIPELABEL, "macpipelabel", "MAC labels for pipes");
MALLOC_DEFINE(M_MACTEMP, "mactemp", "MAC temporary label storage");

/*
 * mac_policy_list_lock protects the consistency of 'mac_policy_list',
 * the linked list of attached policy modules.  Read-only consumers of
 * the list must acquire a shared lock for the duration of their use;
 * writers must acquire an exclusive lock.  Note that for compound
 * operations, locks should be held for the entire compound operation,
 * and that this is not yet done for relabel requests.
 */
static struct mtx mac_policy_list_lock;
static LIST_HEAD(, mac_policy_conf) mac_policy_list;
static int mac_policy_list_busy;
#define	MAC_POLICY_LIST_LOCKINIT()	mtx_init(&mac_policy_list_lock,	\
	"mac_policy_list_lock", NULL, MTX_DEF);
#define	MAC_POLICY_LIST_LOCK()	mtx_lock(&mac_policy_list_lock);
#define	MAC_POLICY_LIST_UNLOCK()	mtx_unlock(&mac_policy_list_lock);

#define	MAC_POLICY_LIST_BUSY() do {					\
	MAC_POLICY_LIST_LOCK();						\
	mac_policy_list_busy++;						\
	MAC_POLICY_LIST_UNLOCK();					\
} while (0)

#define	MAC_POLICY_LIST_UNBUSY() do {					\
	MAC_POLICY_LIST_LOCK();						\
	mac_policy_list_busy--;						\
	if (mac_policy_list_busy < 0)					\
		panic("Extra mac_policy_list_busy--");			\
	MAC_POLICY_LIST_UNLOCK();					\
} while (0)

/*
 * MAC_CHECK performs the designated check by walking the policy
 * module list and checking with each as to how it feels about the
 * request.  Note that it returns its value via 'error' in the scope
 * of the caller.
 */
#define	MAC_CHECK(check, args...) do {					\
	struct mac_policy_conf *mpc;					\
									\
	error = 0;							\
	MAC_POLICY_LIST_BUSY();						\
	LIST_FOREACH(mpc, &mac_policy_list, mpc_list) {			\
		if (mpc->mpc_ops->mpo_ ## check != NULL)		\
			error = error_select(				\
			    mpc->mpc_ops->mpo_ ## check (args),		\
			    error);					\
	}								\
	MAC_POLICY_LIST_UNBUSY();					\
} while (0)

/*
 * MAC_BOOLEAN performs the designated boolean composition by walking
 * the module list, invoking each instance of the operation, and
 * combining the results using the passed C operator.  Note that it
 * returns its value via 'result' in the scope of the caller, which
 * should be initialized by the caller in a meaningful way to get
 * a meaningful result.
 */
#define	MAC_BOOLEAN(operation, composition, args...) do {		\
	struct mac_policy_conf *mpc;					\
									\
	MAC_POLICY_LIST_BUSY();						\
	LIST_FOREACH(mpc, &mac_policy_list, mpc_list) {			\
		if (mpc->mpc_ops->mpo_ ## operation != NULL)		\
			result = result composition			\
			    mpc->mpc_ops->mpo_ ## operation (args);	\
	}								\
	MAC_POLICY_LIST_UNBUSY();					\
} while (0)

#define	MAC_EXTERNALIZE(type, label, elementlist, outbuf, 		\
    outbuflen) do {							\
	char *curptr, *curptr_start, *element_name, *element_temp;	\
	size_t left, left_start, len;					\
	int claimed, first, first_start, ignorenotfound;		\
									\
	error = 0;							\
	element_temp = elementlist;					\
	curptr = outbuf;						\
	curptr[0] = '\0';						\
	left = outbuflen;						\
	first = 1;							\
	while ((element_name = strsep(&element_temp, ",")) != NULL) {	\
		curptr_start = curptr;					\
		left_start = left;					\
		first_start = first;					\
		if (element_name[0] == '?') {				\
			element_name++;					\
			ignorenotfound = 1;				\
		} else							\
			ignorenotfound = 0;				\
		claimed = 0;						\
		if (first) {						\
			len = snprintf(curptr, left, "%s/",		\
			    element_name);				\
			first = 0;					\
		} else							\
			len = snprintf(curptr, left, ",%s/",		\
			    element_name);				\
		if (len >= left) {					\
			error = EINVAL;		/* XXXMAC: E2BIG */	\
			break;						\
		}							\
		curptr += len;						\
		left -= len;						\
									\
		MAC_CHECK(externalize_ ## type, label, element_name,	\
		    curptr, left, &len, &claimed);			\
		if (error)						\
			break;						\
		if (claimed == 1) {					\
			if (len >= outbuflen) {				\
				error = EINVAL;	/* XXXMAC: E2BIG */	\
				break;					\
			}						\
			curptr += len;					\
			left -= len;					\
		} else if (claimed == 0 && ignorenotfound) {		\
			/*						\
			 * Revert addition of the label element		\
			 * name.					\
			 */						\
			curptr = curptr_start;				\
			*curptr = '\0';					\
			left = left_start;				\
			first = first_start;				\
		} else {						\
			error = EINVAL;		/* XXXMAC: ENOLABEL */	\
			break;						\
		}							\
	}								\
} while (0)

#define	MAC_INTERNALIZE(type, label, instring) do {			\
	char *element, *element_name, *element_data;			\
	int claimed;							\
									\
	error = 0;							\
	element = instring;						\
	while ((element_name = strsep(&element, ",")) != NULL) {	\
		element_data = element_name;				\
		element_name = strsep(&element_data, "/");		\
		if (element_data == NULL) {				\
			error = EINVAL;					\
			break;						\
		}							\
		claimed = 0;						\
		MAC_CHECK(internalize_ ## type, label, element_name,	\
		    element_data, &claimed);				\
		if (error)						\
			break;						\
		if (claimed != 1) {					\
			/* XXXMAC: Another error here? */		\
			error = EINVAL;					\
			break;						\
		}							\
	}								\
} while (0)

/*
 * MAC_PERFORM performs the designated operation by walking the policy
 * module list and invoking that operation for each policy.
 */
#define	MAC_PERFORM(operation, args...) do {				\
	struct mac_policy_conf *mpc;					\
									\
	MAC_POLICY_LIST_BUSY();						\
	LIST_FOREACH(mpc, &mac_policy_list, mpc_list) {			\
		if (mpc->mpc_ops->mpo_ ## operation != NULL)		\
			mpc->mpc_ops->mpo_ ## operation (args);		\
	}								\
	MAC_POLICY_LIST_UNBUSY();					\
} while (0)

/*
 * Initialize the MAC subsystem, including appropriate SMP locks.
 */
static void
mac_init(void)
{

	LIST_INIT(&mac_policy_list);
	MAC_POLICY_LIST_LOCKINIT();
}

/*
 * For the purposes of modules that want to know if they were loaded
 * "early", set the mac_late flag once we've processed modules either
 * linked into the kernel, or loaded before the kernel startup.
 */
static void
mac_late_init(void)
{

	mac_late = 1;
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
		break;
	}

	return (error);
}

static int
mac_policy_register(struct mac_policy_conf *mpc)
{
	struct mac_policy_conf *tmpc;
	int slot;

	MAC_POLICY_LIST_LOCK();
	if (mac_policy_list_busy > 0) {
		MAC_POLICY_LIST_UNLOCK();
		return (EBUSY);
	}
	LIST_FOREACH(tmpc, &mac_policy_list, mpc_list) {
		if (strcmp(tmpc->mpc_name, mpc->mpc_name) == 0) {
			MAC_POLICY_LIST_UNLOCK();
			return (EEXIST);
		}
	}
	if (mpc->mpc_field_off != NULL) {
		slot = ffs(mac_policy_offsets_free);
		if (slot == 0) {
			MAC_POLICY_LIST_UNLOCK();
			return (ENOMEM);
		}
		slot--;
		mac_policy_offsets_free &= ~(1 << slot);
		*mpc->mpc_field_off = slot;
	}
	mpc->mpc_runtime_flags |= MPC_RUNTIME_FLAG_REGISTERED;
	LIST_INSERT_HEAD(&mac_policy_list, mpc, mpc_list);

	/* Per-policy initialization. */
	if (mpc->mpc_ops->mpo_init != NULL)
		(*(mpc->mpc_ops->mpo_init))(mpc);
	MAC_POLICY_LIST_UNLOCK();

	printf("Security policy loaded: %s (%s)\n", mpc->mpc_fullname,
	    mpc->mpc_name);

	return (0);
}

static int
mac_policy_unregister(struct mac_policy_conf *mpc)
{

	/*
	 * If we fail the load, we may get a request to unload.  Check
	 * to see if we did the run-time registration, and if not,
	 * silently succeed.
	 */
	MAC_POLICY_LIST_LOCK();
	if ((mpc->mpc_runtime_flags & MPC_RUNTIME_FLAG_REGISTERED) == 0) {
		MAC_POLICY_LIST_UNLOCK();
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
	 * Only allow the unload to proceed if the module is unloadable
	 * by its own definition.
	 */
	if ((mpc->mpc_loadtime_flags & MPC_LOADTIME_FLAG_UNLOADOK) == 0) {
		MAC_POLICY_LIST_UNLOCK();
		return (EBUSY);
	}
	/*
	 * Right now, we EBUSY if the list is in use.  In the future,
	 * for reliability reasons, we might want to sleep and wakeup
	 * later to try again.
	 */
	if (mac_policy_list_busy > 0) {
		MAC_POLICY_LIST_UNLOCK();
		return (EBUSY);
	}
	if (mpc->mpc_ops->mpo_destroy != NULL)
		(*(mpc->mpc_ops->mpo_destroy))(mpc);

	LIST_REMOVE(mpc, mpc_list);
	MAC_POLICY_LIST_UNLOCK();

	mpc->mpc_runtime_flags &= ~MPC_RUNTIME_FLAG_REGISTERED;

	printf("Security policy unload: %s (%s)\n", mpc->mpc_fullname,
	    mpc->mpc_name);

	return (0);
}

/*
 * Define an error value precedence, and given two arguments, selects the
 * value with the higher precedence.
 */
static int
error_select(int error1, int error2)
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

static void
mac_init_label(struct label *label)
{

	bzero(label, sizeof(*label));
	label->l_flags = MAC_FLAG_INITIALIZED;
}

static void
mac_destroy_label(struct label *label)
{

	KASSERT(label->l_flags & MAC_FLAG_INITIALIZED,
	    ("destroying uninitialized label"));

	bzero(label, sizeof(*label));
	/* implicit: label->l_flags &= ~MAC_FLAG_INITIALIZED; */
}

void
mac_init_bpfdesc(struct bpf_d *bpf_d)
{

	mac_init_label(&bpf_d->bd_label);
	MAC_PERFORM(init_bpfdesc_label, &bpf_d->bd_label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmacbpfdescs, 1);
#endif
}

static void
mac_init_cred_label(struct label *label)
{

	mac_init_label(label);
	MAC_PERFORM(init_cred_label, label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmaccreds, 1);
#endif
}

void
mac_init_cred(struct ucred *cred)
{

	mac_init_cred_label(&cred->cr_label);
}

void
mac_init_devfsdirent(struct devfs_dirent *de)
{

	mac_init_label(&de->de_label);
	MAC_PERFORM(init_devfsdirent_label, &de->de_label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmacdevfsdirents, 1);
#endif
}

static void
mac_init_ifnet_label(struct label *label)
{

	mac_init_label(label);
	MAC_PERFORM(init_ifnet_label, label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmacifnets, 1);
#endif
}

void
mac_init_ifnet(struct ifnet *ifp)
{

	mac_init_ifnet_label(&ifp->if_label);
}

void
mac_init_ipq(struct ipq *ipq)
{

	mac_init_label(&ipq->ipq_label);
	MAC_PERFORM(init_ipq_label, &ipq->ipq_label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmacipqs, 1);
#endif
}

int
mac_init_mbuf(struct mbuf *m, int flag)
{
	int error;

	KASSERT(m->m_flags & M_PKTHDR, ("mac_init_mbuf on non-header mbuf"));

	mac_init_label(&m->m_pkthdr.label);

	MAC_CHECK(init_mbuf_label, &m->m_pkthdr.label, flag);
	if (error) {
		MAC_PERFORM(destroy_mbuf_label, &m->m_pkthdr.label);
		mac_destroy_label(&m->m_pkthdr.label);
	}

#ifdef MAC_DEBUG
	if (error == 0)
		atomic_add_int(&nmacmbufs, 1);
#endif
	return (error);
}

void
mac_init_mount(struct mount *mp)
{

	mac_init_label(&mp->mnt_mntlabel);
	mac_init_label(&mp->mnt_fslabel);
	MAC_PERFORM(init_mount_label, &mp->mnt_mntlabel);
	MAC_PERFORM(init_mount_fs_label, &mp->mnt_fslabel);
#ifdef MAC_DEBUG
	atomic_add_int(&nmacmounts, 1);
#endif
}

static void
mac_init_pipe_label(struct label *label)
{

	mac_init_label(label);
	MAC_PERFORM(init_pipe_label, label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmacpipes, 1);
#endif
}

void
mac_init_pipe(struct pipe *pipe)
{
	struct label *label;

	label = malloc(sizeof(struct label), M_MACPIPELABEL, M_ZERO|M_WAITOK);
	pipe->pipe_label = label;
	pipe->pipe_peer->pipe_label = label;
	mac_init_pipe_label(label);
}

static int
mac_init_socket_label(struct label *label, int flag)
{
	int error;

	mac_init_label(label);

	MAC_CHECK(init_socket_label, label, flag);
	if (error) {
		MAC_PERFORM(destroy_socket_label, label);
		mac_destroy_label(label);
	}

#ifdef MAC_DEBUG
	if (error == 0)
		atomic_add_int(&nmacsockets, 1);
#endif

	return (error);
}

static int
mac_init_socket_peer_label(struct label *label, int flag)
{
	int error;

	mac_init_label(label);

	MAC_CHECK(init_socket_peer_label, label, flag);
	if (error) {
		MAC_PERFORM(destroy_socket_label, label);
		mac_destroy_label(label);
	}

	return (error);
}

int
mac_init_socket(struct socket *socket, int flag)
{
	int error;

	error = mac_init_socket_label(&socket->so_label, flag);
	if (error)
		return (error);

	error = mac_init_socket_peer_label(&socket->so_peerlabel, flag);
	if (error)
		mac_destroy_socket_label(&socket->so_label);

	return (error);
}

void
mac_init_vnode_label(struct label *label)
{

	mac_init_label(label);
	MAC_PERFORM(init_vnode_label, label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmacvnodes, 1);
#endif
}

void
mac_init_vnode(struct vnode *vp)
{

	mac_init_vnode_label(&vp->v_label);
}

void
mac_destroy_bpfdesc(struct bpf_d *bpf_d)
{

	MAC_PERFORM(destroy_bpfdesc_label, &bpf_d->bd_label);
	mac_destroy_label(&bpf_d->bd_label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacbpfdescs, 1);
#endif
}

static void
mac_destroy_cred_label(struct label *label)
{

	MAC_PERFORM(destroy_cred_label, label);
	mac_destroy_label(label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmaccreds, 1);
#endif
}

void
mac_destroy_cred(struct ucred *cred)
{

	mac_destroy_cred_label(&cred->cr_label);
}

void
mac_destroy_devfsdirent(struct devfs_dirent *de)
{

	MAC_PERFORM(destroy_devfsdirent_label, &de->de_label);
	mac_destroy_label(&de->de_label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacdevfsdirents, 1);
#endif
}

static void
mac_destroy_ifnet_label(struct label *label)
{

	MAC_PERFORM(destroy_ifnet_label, label);
	mac_destroy_label(label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacifnets, 1);
#endif
}

void
mac_destroy_ifnet(struct ifnet *ifp)
{

	mac_destroy_ifnet_label(&ifp->if_label);
}

void
mac_destroy_ipq(struct ipq *ipq)
{

	MAC_PERFORM(destroy_ipq_label, &ipq->ipq_label);
	mac_destroy_label(&ipq->ipq_label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacipqs, 1);
#endif
}

void
mac_destroy_mbuf(struct mbuf *m)
{

	MAC_PERFORM(destroy_mbuf_label, &m->m_pkthdr.label);
	mac_destroy_label(&m->m_pkthdr.label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacmbufs, 1);
#endif
}

void
mac_destroy_mount(struct mount *mp)
{

	MAC_PERFORM(destroy_mount_label, &mp->mnt_mntlabel);
	MAC_PERFORM(destroy_mount_fs_label, &mp->mnt_fslabel);
	mac_destroy_label(&mp->mnt_fslabel);
	mac_destroy_label(&mp->mnt_mntlabel);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacmounts, 1);
#endif
}

static void
mac_destroy_pipe_label(struct label *label)
{

	MAC_PERFORM(destroy_pipe_label, label);
	mac_destroy_label(label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacpipes, 1);
#endif
}

void
mac_destroy_pipe(struct pipe *pipe)
{

	mac_destroy_pipe_label(pipe->pipe_label);
	free(pipe->pipe_label, M_MACPIPELABEL);
}

static void
mac_destroy_socket_label(struct label *label)
{

	MAC_PERFORM(destroy_socket_label, label);
	mac_destroy_label(label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacsockets, 1);
#endif
}

static void
mac_destroy_socket_peer_label(struct label *label)
{

	MAC_PERFORM(destroy_socket_peer_label, label);
	mac_destroy_label(label);
}

void
mac_destroy_socket(struct socket *socket)
{

	mac_destroy_socket_label(&socket->so_label);
	mac_destroy_socket_peer_label(&socket->so_peerlabel);
}

void
mac_destroy_vnode_label(struct label *label)
{

	MAC_PERFORM(destroy_vnode_label, label);
	mac_destroy_label(label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacvnodes, 1);
#endif
}

void
mac_destroy_vnode(struct vnode *vp)
{

	mac_destroy_vnode_label(&vp->v_label);
}

static void
mac_copy_pipe_label(struct label *src, struct label *dest)
{

	MAC_PERFORM(copy_pipe_label, src, dest);
}

void
mac_copy_vnode_label(struct label *src, struct label *dest)
{

	MAC_PERFORM(copy_vnode_label, src, dest);
}

static int
mac_check_structmac_consistent(struct mac *mac)
{

	if (mac->m_buflen > MAC_MAX_LABEL_BUF_LEN)
		return (EINVAL);

	return (0);
}

static int
mac_externalize_cred_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen, int flags)
{
	int error;

	MAC_EXTERNALIZE(cred_label, label, elements, outbuf, outbuflen);

	return (error);
}

static int
mac_externalize_ifnet_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen, int flags)
{
	int error;

	MAC_EXTERNALIZE(ifnet_label, label, elements, outbuf, outbuflen);

	return (error);
}

static int
mac_externalize_pipe_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen, int flags)
{
	int error;

	MAC_EXTERNALIZE(pipe_label, label, elements, outbuf, outbuflen);

	return (error);
}

static int
mac_externalize_socket_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen, int flags)
{
	int error;

	MAC_EXTERNALIZE(socket_label, label, elements, outbuf, outbuflen);

	return (error);
}

static int
mac_externalize_socket_peer_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen, int flags)
{
	int error;

	MAC_EXTERNALIZE(socket_peer_label, label, elements, outbuf, outbuflen);

	return (error);
}

static int
mac_externalize_vnode_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen, int flags)
{
	int error;

	MAC_EXTERNALIZE(vnode_label, label, elements, outbuf, outbuflen);

	return (error);
}

static int
mac_internalize_cred_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(cred_label, label, string);

	return (error);
}

static int
mac_internalize_ifnet_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(ifnet_label, label, string);

	return (error);
}

static int
mac_internalize_pipe_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(pipe_label, label, string);

	return (error);
}

static int
mac_internalize_socket_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(socket_label, label, string);

	return (error);
}

static int
mac_internalize_vnode_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(vnode_label, label, string);

	return (error);
}

/*
 * Initialize MAC label for the first kernel process, from which other
 * kernel processes and threads are spawned.
 */
void
mac_create_proc0(struct ucred *cred)
{

	MAC_PERFORM(create_proc0, cred);
}

/*
 * Initialize MAC label for the first userland process, from which other
 * userland processes and threads are spawned.
 */
void
mac_create_proc1(struct ucred *cred)
{

	MAC_PERFORM(create_proc1, cred);
}

void
mac_thread_userret(struct thread *td)
{

	MAC_PERFORM(thread_userret, td);
}

/*
 * When a new process is created, its label must be initialized.  Generally,
 * this involves inheritence from the parent process, modulo possible
 * deltas.  This function allows that processing to take place.
 */
void
mac_create_cred(struct ucred *parent_cred, struct ucred *child_cred)
{

	MAC_PERFORM(create_cred, parent_cred, child_cred);
}

void
mac_update_devfsdirent(struct devfs_dirent *de, struct vnode *vp)
{

	MAC_PERFORM(update_devfsdirent, de, &de->de_label, vp, &vp->v_label);
}

void
mac_associate_vnode_devfs(struct mount *mp, struct devfs_dirent *de,
    struct vnode *vp)
{

	MAC_PERFORM(associate_vnode_devfs, mp, &mp->mnt_fslabel, de,
	    &de->de_label, vp, &vp->v_label);
}

int
mac_associate_vnode_extattr(struct mount *mp, struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_associate_vnode_extattr");

	MAC_CHECK(associate_vnode_extattr, mp, &mp->mnt_fslabel, vp,
	    &vp->v_label);

	return (error);
}

void
mac_associate_vnode_singlelabel(struct mount *mp, struct vnode *vp)
{

	MAC_PERFORM(associate_vnode_singlelabel, mp, &mp->mnt_fslabel, vp,
	    &vp->v_label);
}

int
mac_create_vnode_extattr(struct ucred *cred, struct mount *mp,
    struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
	int error;

	ASSERT_VOP_LOCKED(dvp, "mac_create_vnode_extattr");
	ASSERT_VOP_LOCKED(vp, "mac_create_vnode_extattr");

	error = VOP_OPENEXTATTR(vp, cred, curthread);
	if (error == EOPNOTSUPP) {
		/* XXX: Optionally abort if transactions not supported. */
		if (ea_warn_once == 0) {
			printf("Warning: transactions not supported "
			    "in EA write.\n");
			ea_warn_once = 1;
		}
	} else if (error)
		return (error);

	MAC_CHECK(create_vnode_extattr, cred, mp, &mp->mnt_fslabel,
	    dvp, &dvp->v_label, vp, &vp->v_label, cnp);

	if (error) {
		VOP_CLOSEEXTATTR(vp, 0, NOCRED, curthread);
		return (error);
	}

	error = VOP_CLOSEEXTATTR(vp, 1, NOCRED, curthread);

	if (error == EOPNOTSUPP)
		error = 0;				/* XXX */

	return (error);
}

static int
mac_setlabel_vnode_extattr(struct ucred *cred, struct vnode *vp,
    struct label *intlabel)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_setlabel_vnode_extattr");

	error = VOP_OPENEXTATTR(vp, cred, curthread);
	if (error == EOPNOTSUPP) {
		/* XXX: Optionally abort if transactions not supported. */
		if (ea_warn_once == 0) {
			printf("Warning: transactions not supported "
			    "in EA write.\n");
			ea_warn_once = 1;
		}
	} else if (error)
		return (error);

	MAC_CHECK(setlabel_vnode_extattr, cred, vp, &vp->v_label, intlabel);

	if (error) {
		VOP_CLOSEEXTATTR(vp, 0, NOCRED, curthread);
		return (error);
	}

	error = VOP_CLOSEEXTATTR(vp, 1, NOCRED, curthread);

	if (error == EOPNOTSUPP)
		error = 0;				/* XXX */

	return (error);
}

void
mac_execve_transition(struct ucred *old, struct ucred *new, struct vnode *vp)
{

	ASSERT_VOP_LOCKED(vp, "mac_execve_transition");

	MAC_PERFORM(execve_transition, old, new, vp, &vp->v_label);
}

int
mac_execve_will_transition(struct ucred *old, struct vnode *vp)
{
	int result;

	result = 0;
	MAC_BOOLEAN(execve_will_transition, ||, old, vp, &vp->v_label);

	return (result);
}

int
mac_check_vnode_access(struct ucred *cred, struct vnode *vp, int acc_mode)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_access");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_access, cred, vp, &vp->v_label, acc_mode);
	return (error);
}

int
mac_check_vnode_chdir(struct ucred *cred, struct vnode *dvp)
{
	int error;

	ASSERT_VOP_LOCKED(dvp, "mac_check_vnode_chdir");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_chdir, cred, dvp, &dvp->v_label);
	return (error);
}

int
mac_check_vnode_chroot(struct ucred *cred, struct vnode *dvp)
{
	int error;

	ASSERT_VOP_LOCKED(dvp, "mac_check_vnode_chroot");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_chroot, cred, dvp, &dvp->v_label);
	return (error);
}

int
mac_check_vnode_create(struct ucred *cred, struct vnode *dvp,
    struct componentname *cnp, struct vattr *vap)
{
	int error;

	ASSERT_VOP_LOCKED(dvp, "mac_check_vnode_create");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_create, cred, dvp, &dvp->v_label, cnp, vap);
	return (error);
}

int
mac_check_vnode_delete(struct ucred *cred, struct vnode *dvp, struct vnode *vp,
    struct componentname *cnp)
{
	int error;

	ASSERT_VOP_LOCKED(dvp, "mac_check_vnode_delete");
	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_delete");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_delete, cred, dvp, &dvp->v_label, vp,
	    &vp->v_label, cnp);
	return (error);
}

int
mac_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
    acl_type_t type)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_deleteacl");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_deleteacl, cred, vp, &vp->v_label, type);
	return (error);
}

int
mac_check_vnode_exec(struct ucred *cred, struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_exec");

	if (!mac_enforce_process && !mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_exec, cred, vp, &vp->v_label);

	return (error);
}

int
mac_check_vnode_getacl(struct ucred *cred, struct vnode *vp, acl_type_t type)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_getacl");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_getacl, cred, vp, &vp->v_label, type);
	return (error);
}

int
mac_check_vnode_getextattr(struct ucred *cred, struct vnode *vp,
    int attrnamespace, const char *name, struct uio *uio)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_getextattr");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_getextattr, cred, vp, &vp->v_label,
	    attrnamespace, name, uio);
	return (error);
}

int
mac_check_vnode_link(struct ucred *cred, struct vnode *dvp,
    struct vnode *vp, struct componentname *cnp)
{
	int error;

	ASSERT_VOP_LOCKED(dvp, "mac_check_vnode_link");
	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_link");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_link, cred, dvp, &dvp->v_label, vp,
	    &vp->v_label, cnp);
	return (error);
}

int
mac_check_vnode_lookup(struct ucred *cred, struct vnode *dvp,
    struct componentname *cnp)
{
	int error;

	ASSERT_VOP_LOCKED(dvp, "mac_check_vnode_lookup");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_lookup, cred, dvp, &dvp->v_label, cnp);
	return (error);
}

int
mac_check_vnode_mmap(struct ucred *cred, struct vnode *vp, int prot)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_mmap");

	if (!mac_enforce_fs || !mac_enforce_vm)
		return (0);

	MAC_CHECK(check_vnode_mmap, cred, vp, &vp->v_label, prot);
	return (error);
}

void
mac_check_vnode_mmap_downgrade(struct ucred *cred, struct vnode *vp, int *prot)
{
	int result = *prot;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_mmap_downgrade");

	if (!mac_enforce_fs || !mac_enforce_vm)
		return;

	MAC_PERFORM(check_vnode_mmap_downgrade, cred, vp, &vp->v_label,
	    &result);

	*prot = result;
}

int
mac_check_vnode_mprotect(struct ucred *cred, struct vnode *vp, int prot)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_mprotect");

	if (!mac_enforce_fs || !mac_enforce_vm)
		return (0);

	MAC_CHECK(check_vnode_mprotect, cred, vp, &vp->v_label, prot);
	return (error);
}

int
mac_check_vnode_open(struct ucred *cred, struct vnode *vp, int acc_mode)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_open");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_open, cred, vp, &vp->v_label, acc_mode);
	return (error);
}

int
mac_check_vnode_poll(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_poll");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_poll, active_cred, file_cred, vp,
	    &vp->v_label);

	return (error);
}

int
mac_check_vnode_read(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_read");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_read, active_cred, file_cred, vp,
	    &vp->v_label);

	return (error);
}

int
mac_check_vnode_readdir(struct ucred *cred, struct vnode *dvp)
{
	int error;

	ASSERT_VOP_LOCKED(dvp, "mac_check_vnode_readdir");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_readdir, cred, dvp, &dvp->v_label);
	return (error);
}

int
mac_check_vnode_readlink(struct ucred *cred, struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_readlink");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_readlink, cred, vp, &vp->v_label);
	return (error);
}

static int
mac_check_vnode_relabel(struct ucred *cred, struct vnode *vp,
    struct label *newlabel)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_relabel");

	MAC_CHECK(check_vnode_relabel, cred, vp, &vp->v_label, newlabel);

	return (error);
}

int
mac_check_vnode_rename_from(struct ucred *cred, struct vnode *dvp,
    struct vnode *vp, struct componentname *cnp)
{
	int error;

	ASSERT_VOP_LOCKED(dvp, "mac_check_vnode_rename_from");
	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_rename_from");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_rename_from, cred, dvp, &dvp->v_label, vp,
	    &vp->v_label, cnp);
	return (error);
}

int
mac_check_vnode_rename_to(struct ucred *cred, struct vnode *dvp,
    struct vnode *vp, int samedir, struct componentname *cnp)
{
	int error;

	ASSERT_VOP_LOCKED(dvp, "mac_check_vnode_rename_to");
	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_rename_to");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_rename_to, cred, dvp, &dvp->v_label, vp,
	    vp != NULL ? &vp->v_label : NULL, samedir, cnp);
	return (error);
}

int
mac_check_vnode_revoke(struct ucred *cred, struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_revoke");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_revoke, cred, vp, &vp->v_label);
	return (error);
}

int
mac_check_vnode_setacl(struct ucred *cred, struct vnode *vp, acl_type_t type,
    struct acl *acl)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_setacl");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_setacl, cred, vp, &vp->v_label, type, acl);
	return (error);
}

int
mac_check_vnode_setextattr(struct ucred *cred, struct vnode *vp,
    int attrnamespace, const char *name, struct uio *uio)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_setextattr");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_setextattr, cred, vp, &vp->v_label,
	    attrnamespace, name, uio);
	return (error);
}

int
mac_check_vnode_setflags(struct ucred *cred, struct vnode *vp, u_long flags)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_setflags");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_setflags, cred, vp, &vp->v_label, flags);
	return (error);
}

int
mac_check_vnode_setmode(struct ucred *cred, struct vnode *vp, mode_t mode)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_setmode");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_setmode, cred, vp, &vp->v_label, mode);
	return (error);
}

int
mac_check_vnode_setowner(struct ucred *cred, struct vnode *vp, uid_t uid,
    gid_t gid)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_setowner");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_setowner, cred, vp, &vp->v_label, uid, gid);
	return (error);
}

int
mac_check_vnode_setutimes(struct ucred *cred, struct vnode *vp,
    struct timespec atime, struct timespec mtime)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_setutimes");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_setutimes, cred, vp, &vp->v_label, atime,
	    mtime);
	return (error);
}

int
mac_check_vnode_stat(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_stat");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_stat, active_cred, file_cred, vp,
	    &vp->v_label);
	return (error);
}

int
mac_check_vnode_write(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_write");

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_vnode_write, active_cred, file_cred, vp,
	    &vp->v_label);

	return (error);
}

/*
 * When relabeling a process, call out to the policies for the maximum
 * permission allowed for each object type we know about in its
 * memory space, and revoke access (in the least surprising ways we
 * know) when necessary.  The process lock is not held here.
 */
static void
mac_cred_mmapped_drop_perms(struct thread *td, struct ucred *cred)
{

	/* XXX freeze all other threads */
	mac_cred_mmapped_drop_perms_recurse(td, cred,
	    &td->td_proc->p_vmspace->vm_map);
	/* XXX allow other threads to continue */
}

static __inline const char *
prot2str(vm_prot_t prot)
{

	switch (prot & VM_PROT_ALL) {
	case VM_PROT_READ:
		return ("r--");
	case VM_PROT_READ | VM_PROT_WRITE:
		return ("rw-");
	case VM_PROT_READ | VM_PROT_EXECUTE:
		return ("r-x");
	case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
		return ("rwx");
	case VM_PROT_WRITE:
		return ("-w-");
	case VM_PROT_EXECUTE:
		return ("--x");
	case VM_PROT_WRITE | VM_PROT_EXECUTE:
		return ("-wx");
	default:
		return ("---");
	}
}

static void
mac_cred_mmapped_drop_perms_recurse(struct thread *td, struct ucred *cred,
    struct vm_map *map)
{
	struct vm_map_entry *vme;
	int result;
	vm_prot_t revokeperms;
	vm_object_t object;
	vm_ooffset_t offset;
	struct vnode *vp;

	if (!mac_mmap_revocation)
		return;

	vm_map_lock_read(map);
	for (vme = map->header.next; vme != &map->header; vme = vme->next) {
		if (vme->eflags & MAP_ENTRY_IS_SUB_MAP) {
			mac_cred_mmapped_drop_perms_recurse(td, cred,
			    vme->object.sub_map);
			continue;
		}
		/*
		 * Skip over entries that obviously are not shared.
		 */
		if (vme->eflags & (MAP_ENTRY_COW | MAP_ENTRY_NOSYNC) ||
		    !vme->max_protection)
			continue;
		/*
		 * Drill down to the deepest backing object.
		 */
		offset = vme->offset;
		object = vme->object.vm_object;
		if (object == NULL)
			continue;
		while (object->backing_object != NULL) {
			object = object->backing_object;
			offset += object->backing_object_offset;
		}
		/*
		 * At the moment, vm_maps and objects aren't considered
		 * by the MAC system, so only things with backing by a
		 * normal object (read: vnodes) are checked.
		 */
		if (object->type != OBJT_VNODE)
			continue;
		vp = (struct vnode *)object->handle;
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		result = vme->max_protection;
		mac_check_vnode_mmap_downgrade(cred, vp, &result);
		VOP_UNLOCK(vp, 0, td);
		/*
		 * Find out what maximum protection we may be allowing
		 * now but a policy needs to get removed.
		 */
		revokeperms = vme->max_protection & ~result;
		if (!revokeperms)
			continue;
		printf("pid %ld: revoking %s perms from %#lx:%ld "
		    "(max %s/cur %s)\n", (long)td->td_proc->p_pid,
		    prot2str(revokeperms), (u_long)vme->start,
		    (long)(vme->end - vme->start),
		    prot2str(vme->max_protection), prot2str(vme->protection));
		vm_map_lock_upgrade(map);
		/*
		 * This is the really simple case: if a map has more
		 * max_protection than is allowed, but it's not being
		 * actually used (that is, the current protection is
		 * still allowed), we can just wipe it out and do
		 * nothing more.
		 */
		if ((vme->protection & revokeperms) == 0) {
			vme->max_protection -= revokeperms;
		} else {
			if (revokeperms & VM_PROT_WRITE) {
				/*
				 * In the more complicated case, flush out all
				 * pending changes to the object then turn it
				 * copy-on-write.
				 */
				vm_object_reference(object);
				vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
				vm_object_page_clean(object,
				    OFF_TO_IDX(offset),
				    OFF_TO_IDX(offset + vme->end - vme->start +
					PAGE_MASK),
				    OBJPC_SYNC);
				VOP_UNLOCK(vp, 0, td);
				vm_object_deallocate(object);
				/*
				 * Why bother if there's no read permissions
				 * anymore?  For the rest, we need to leave
				 * the write permissions on for COW, or
				 * remove them entirely if configured to.
				 */
				if (!mac_mmap_revocation_via_cow) {
					vme->max_protection &= ~VM_PROT_WRITE;
					vme->protection &= ~VM_PROT_WRITE;
				} if ((revokeperms & VM_PROT_READ) == 0)
					vme->eflags |= MAP_ENTRY_COW |
					    MAP_ENTRY_NEEDS_COPY;
			}
			if (revokeperms & VM_PROT_EXECUTE) {
				vme->max_protection &= ~VM_PROT_EXECUTE;
				vme->protection &= ~VM_PROT_EXECUTE;
			}
			if (revokeperms & VM_PROT_READ) {
				vme->max_protection = 0;
				vme->protection = 0;
			}
			pmap_protect(map->pmap, vme->start, vme->end,
			    vme->protection & ~revokeperms);
			vm_map_simplify_entry(map, vme);
		}
		vm_map_lock_downgrade(map);
	}
	vm_map_unlock_read(map);
}

/*
 * When the subject's label changes, it may require revocation of privilege
 * to mapped objects.  This can't be done on-the-fly later with a unified
 * buffer cache.
 */
static void
mac_relabel_cred(struct ucred *cred, struct label *newlabel)
{

	MAC_PERFORM(relabel_cred, cred, newlabel);
}

void
mac_relabel_vnode(struct ucred *cred, struct vnode *vp, struct label *newlabel)
{

	MAC_PERFORM(relabel_vnode, cred, vp, &vp->v_label, newlabel);
}

void
mac_create_ifnet(struct ifnet *ifnet)
{

	MAC_PERFORM(create_ifnet, ifnet, &ifnet->if_label);
}

void
mac_create_bpfdesc(struct ucred *cred, struct bpf_d *bpf_d)
{

	MAC_PERFORM(create_bpfdesc, cred, bpf_d, &bpf_d->bd_label);
}

void
mac_create_socket(struct ucred *cred, struct socket *socket)
{

	MAC_PERFORM(create_socket, cred, socket, &socket->so_label);
}

void
mac_create_pipe(struct ucred *cred, struct pipe *pipe)
{

	MAC_PERFORM(create_pipe, cred, pipe, pipe->pipe_label);
}

void
mac_create_socket_from_socket(struct socket *oldsocket,
    struct socket *newsocket)
{

	MAC_PERFORM(create_socket_from_socket, oldsocket, &oldsocket->so_label,
	    newsocket, &newsocket->so_label);
}

static void
mac_relabel_socket(struct ucred *cred, struct socket *socket,
    struct label *newlabel)
{

	MAC_PERFORM(relabel_socket, cred, socket, &socket->so_label, newlabel);
}

static void
mac_relabel_pipe(struct ucred *cred, struct pipe *pipe, struct label *newlabel)
{

	MAC_PERFORM(relabel_pipe, cred, pipe, pipe->pipe_label, newlabel);
}

void
mac_set_socket_peer_from_mbuf(struct mbuf *mbuf, struct socket *socket)
{

	MAC_PERFORM(set_socket_peer_from_mbuf, mbuf, &mbuf->m_pkthdr.label,
	    socket, &socket->so_peerlabel);
}

void
mac_set_socket_peer_from_socket(struct socket *oldsocket,
    struct socket *newsocket)
{

	MAC_PERFORM(set_socket_peer_from_socket, oldsocket,
	    &oldsocket->so_label, newsocket, &newsocket->so_peerlabel);
}

void
mac_create_datagram_from_ipq(struct ipq *ipq, struct mbuf *datagram)
{

	MAC_PERFORM(create_datagram_from_ipq, ipq, &ipq->ipq_label,
	    datagram, &datagram->m_pkthdr.label);
}

void
mac_create_fragment(struct mbuf *datagram, struct mbuf *fragment)
{

	MAC_PERFORM(create_fragment, datagram, &datagram->m_pkthdr.label,
	    fragment, &fragment->m_pkthdr.label);
}

void
mac_create_ipq(struct mbuf *fragment, struct ipq *ipq)
{

	MAC_PERFORM(create_ipq, fragment, &fragment->m_pkthdr.label, ipq,
	    &ipq->ipq_label);
}

void
mac_create_mbuf_from_mbuf(struct mbuf *oldmbuf, struct mbuf *newmbuf)
{

	MAC_PERFORM(create_mbuf_from_mbuf, oldmbuf, &oldmbuf->m_pkthdr.label,
	    newmbuf, &newmbuf->m_pkthdr.label);
}

void
mac_create_mbuf_from_bpfdesc(struct bpf_d *bpf_d, struct mbuf *mbuf)
{

	MAC_PERFORM(create_mbuf_from_bpfdesc, bpf_d, &bpf_d->bd_label, mbuf,
	    &mbuf->m_pkthdr.label);
}

void
mac_create_mbuf_linklayer(struct ifnet *ifnet, struct mbuf *mbuf)
{

	MAC_PERFORM(create_mbuf_linklayer, ifnet, &ifnet->if_label, mbuf,
	    &mbuf->m_pkthdr.label);
}

void
mac_create_mbuf_from_ifnet(struct ifnet *ifnet, struct mbuf *mbuf)
{

	MAC_PERFORM(create_mbuf_from_ifnet, ifnet, &ifnet->if_label, mbuf,
	    &mbuf->m_pkthdr.label);
}

void
mac_create_mbuf_multicast_encap(struct mbuf *oldmbuf, struct ifnet *ifnet,
    struct mbuf *newmbuf)
{

	MAC_PERFORM(create_mbuf_multicast_encap, oldmbuf,
	    &oldmbuf->m_pkthdr.label, ifnet, &ifnet->if_label, newmbuf,
	    &newmbuf->m_pkthdr.label);
}

void
mac_create_mbuf_netlayer(struct mbuf *oldmbuf, struct mbuf *newmbuf)
{

	MAC_PERFORM(create_mbuf_netlayer, oldmbuf, &oldmbuf->m_pkthdr.label,
	    newmbuf, &newmbuf->m_pkthdr.label);
}

int
mac_fragment_match(struct mbuf *fragment, struct ipq *ipq)
{
	int result;

	result = 1;
	MAC_BOOLEAN(fragment_match, &&, fragment, &fragment->m_pkthdr.label,
	    ipq, &ipq->ipq_label);

	return (result);
}

void
mac_update_ipq(struct mbuf *fragment, struct ipq *ipq)
{

	MAC_PERFORM(update_ipq, fragment, &fragment->m_pkthdr.label, ipq,
	    &ipq->ipq_label);
}

void
mac_create_mbuf_from_socket(struct socket *socket, struct mbuf *mbuf)
{

	MAC_PERFORM(create_mbuf_from_socket, socket, &socket->so_label, mbuf,
	    &mbuf->m_pkthdr.label);
}

void
mac_create_mount(struct ucred *cred, struct mount *mp)
{

	MAC_PERFORM(create_mount, cred, mp, &mp->mnt_mntlabel,
	    &mp->mnt_fslabel);
}

void
mac_create_root_mount(struct ucred *cred, struct mount *mp)
{

	MAC_PERFORM(create_root_mount, cred, mp, &mp->mnt_mntlabel,
	    &mp->mnt_fslabel);
}

int
mac_check_bpfdesc_receive(struct bpf_d *bpf_d, struct ifnet *ifnet)
{
	int error;

	if (!mac_enforce_network)
		return (0);

	MAC_CHECK(check_bpfdesc_receive, bpf_d, &bpf_d->bd_label, ifnet,
	    &ifnet->if_label);

	return (error);
}

static int
mac_check_cred_relabel(struct ucred *cred, struct label *newlabel)
{
	int error;

	MAC_CHECK(check_cred_relabel, cred, newlabel);

	return (error);
}

int
mac_check_cred_visible(struct ucred *u1, struct ucred *u2)
{
	int error;

	if (!mac_enforce_process)
		return (0);

	MAC_CHECK(check_cred_visible, u1, u2);

	return (error);
}

int
mac_check_ifnet_transmit(struct ifnet *ifnet, struct mbuf *mbuf)
{
	int error;

	if (!mac_enforce_network)
		return (0);

	KASSERT(mbuf->m_flags & M_PKTHDR, ("packet has no pkthdr"));
	if (!(mbuf->m_pkthdr.label.l_flags & MAC_FLAG_INITIALIZED))
		if_printf(ifnet, "not initialized\n");

	MAC_CHECK(check_ifnet_transmit, ifnet, &ifnet->if_label, mbuf,
	    &mbuf->m_pkthdr.label);

	return (error);
}

int
mac_check_kenv_dump(struct ucred *cred)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_kenv_dump, cred);

	return (error);
}

int
mac_check_kenv_get(struct ucred *cred, char *name)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_kenv_get, cred, name);

	return (error);
}

int
mac_check_kenv_set(struct ucred *cred, char *name, char *value)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_kenv_set, cred, name, value);

	return (error);
}

int
mac_check_kenv_unset(struct ucred *cred, char *name)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_kenv_unset, cred, name);

	return (error);
}

int
mac_check_mount_stat(struct ucred *cred, struct mount *mount)
{
	int error;

	if (!mac_enforce_fs)
		return (0);

	MAC_CHECK(check_mount_stat, cred, mount, &mount->mnt_mntlabel);

	return (error);
}

int
mac_check_pipe_ioctl(struct ucred *cred, struct pipe *pipe, unsigned long cmd,
    void *data)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_ioctl, cred, pipe, pipe->pipe_label, cmd, data);

	return (error);
}

int
mac_check_pipe_poll(struct ucred *cred, struct pipe *pipe)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_poll, cred, pipe, pipe->pipe_label);

	return (error);
}

int
mac_check_pipe_read(struct ucred *cred, struct pipe *pipe)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_read, cred, pipe, pipe->pipe_label);

	return (error);
}

static int
mac_check_pipe_relabel(struct ucred *cred, struct pipe *pipe,
    struct label *newlabel)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_relabel, cred, pipe, pipe->pipe_label, newlabel);

	return (error);
}

int
mac_check_pipe_stat(struct ucred *cred, struct pipe *pipe)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_stat, cred, pipe, pipe->pipe_label);

	return (error);
}

int
mac_check_pipe_write(struct ucred *cred, struct pipe *pipe)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	if (!mac_enforce_pipe)
		return (0);

	MAC_CHECK(check_pipe_write, cred, pipe, pipe->pipe_label);

	return (error);
}

int
mac_check_proc_debug(struct ucred *cred, struct proc *proc)
{
	int error;

	PROC_LOCK_ASSERT(proc, MA_OWNED);

	if (!mac_enforce_process)
		return (0);

	MAC_CHECK(check_proc_debug, cred, proc);

	return (error);
}

int
mac_check_proc_sched(struct ucred *cred, struct proc *proc)
{
	int error;

	PROC_LOCK_ASSERT(proc, MA_OWNED);

	if (!mac_enforce_process)
		return (0);

	MAC_CHECK(check_proc_sched, cred, proc);

	return (error);
}

int
mac_check_proc_signal(struct ucred *cred, struct proc *proc, int signum)
{
	int error;

	PROC_LOCK_ASSERT(proc, MA_OWNED);

	if (!mac_enforce_process)
		return (0);

	MAC_CHECK(check_proc_signal, cred, proc, signum);

	return (error);
}

int
mac_check_socket_bind(struct ucred *ucred, struct socket *socket,
    struct sockaddr *sockaddr)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_bind, ucred, socket, &socket->so_label,
	    sockaddr);

	return (error);
}

int
mac_check_socket_connect(struct ucred *cred, struct socket *socket,
    struct sockaddr *sockaddr)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_connect, cred, socket, &socket->so_label,
	    sockaddr);

	return (error);
}

int
mac_check_socket_deliver(struct socket *socket, struct mbuf *mbuf)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_deliver, socket, &socket->so_label, mbuf,
	    &mbuf->m_pkthdr.label);

	return (error);
}

int
mac_check_socket_listen(struct ucred *cred, struct socket *socket)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_listen, cred, socket, &socket->so_label);
	return (error);
}

int
mac_check_socket_receive(struct ucred *cred, struct socket *so)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_receive, cred, so, &so->so_label);

	return (error);
}

static int
mac_check_socket_relabel(struct ucred *cred, struct socket *socket,
    struct label *newlabel)
{
	int error;

	MAC_CHECK(check_socket_relabel, cred, socket, &socket->so_label,
	    newlabel);

	return (error);
}

int
mac_check_socket_send(struct ucred *cred, struct socket *so)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_send, cred, so, &so->so_label);

	return (error);
}

int
mac_check_socket_visible(struct ucred *cred, struct socket *socket)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_visible, cred, socket, &socket->so_label);

	return (error);
}

int
mac_check_system_acct(struct ucred *cred, struct vnode *vp)
{
	int error;

	if (vp != NULL) {
		ASSERT_VOP_LOCKED(vp, "mac_check_system_acct");
	}

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_acct, cred, vp,
	    vp != NULL ? &vp->v_label : NULL);

	return (error);
}

int
mac_check_system_nfsd(struct ucred *cred)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_nfsd, cred);

	return (error);
}

int
mac_check_system_reboot(struct ucred *cred, int howto)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_reboot, cred, howto);

	return (error);
}

int
mac_check_system_settime(struct ucred *cred)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_settime, cred);

	return (error);
}

int
mac_check_system_swapon(struct ucred *cred, struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_system_swapon");

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_swapon, cred, vp, &vp->v_label);
	return (error);
}

int
mac_check_system_sysctl(struct ucred *cred, int *name, u_int namelen,
    void *old, size_t *oldlenp, int inkernel, void *new, size_t newlen)
{
	int error;

	/*
	 * XXXMAC: We're very much like to assert the SYSCTL_LOCK here,
	 * but since it's not exported from kern_sysctl.c, we can't.
	 */
	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_sysctl, cred, name, namelen, old, oldlenp,
	    inkernel, new, newlen);

	return (error);
}

int
mac_ioctl_ifnet_get(struct ucred *cred, struct ifreq *ifr,
    struct ifnet *ifnet)
{
	char *elements, *buffer;
	struct mac mac;
	int error;

	error = copyin(ifr->ifr_ifru.ifru_data, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_externalize_ifnet_label(&ifnet->if_label, elements,
	    buffer, mac.m_buflen, M_WAITOK);
	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}

int
mac_ioctl_ifnet_set(struct ucred *cred, struct ifreq *ifr,
    struct ifnet *ifnet)
{
	struct label intlabel;
	struct mac mac;
	char *buffer;
	int error;

	error = copyin(ifr->ifr_ifru.ifru_data, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, buffer, mac.m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	mac_init_ifnet_label(&intlabel);
	error = mac_internalize_ifnet_label(&intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error) {
		mac_destroy_ifnet_label(&intlabel);
		return (error);
	}

	/*
	 * XXX: Note that this is a redundant privilege check, since
	 * policies impose this check themselves if required by the
	 * policy.  Eventually, this should go away.
	 */
	error = suser_cred(cred, 0);
	if (error) {
		mac_destroy_ifnet_label(&intlabel);
		return (error);
	}

	MAC_CHECK(check_ifnet_relabel, cred, ifnet, &ifnet->if_label,
	    &intlabel);
	if (error) {
		mac_destroy_ifnet_label(&intlabel);
		return (error);
	}

	MAC_PERFORM(relabel_ifnet, cred, ifnet, &ifnet->if_label, &intlabel);

	mac_destroy_ifnet_label(&intlabel);
	return (0);
}

void
mac_create_devfs_vnode(struct devfs_dirent *de, struct vnode *vp)
{

	MAC_PERFORM(create_devfs_vnode, de, &de->de_label, vp, &vp->v_label);
}

void
mac_create_devfs_device(dev_t dev, struct devfs_dirent *de)
{

	MAC_PERFORM(create_devfs_device, dev, de, &de->de_label);
}

void
mac_create_devfs_symlink(struct ucred *cred, struct devfs_dirent *dd,
    struct devfs_dirent *de)
{

	MAC_PERFORM(create_devfs_symlink, cred, dd, &dd->de_label, de,
	    &de->de_label);
}

void
mac_create_devfs_directory(char *dirname, int dirnamelen,
    struct devfs_dirent *de)
{

	MAC_PERFORM(create_devfs_directory, dirname, dirnamelen, de,
	    &de->de_label);
}

int
mac_setsockopt_label_set(struct ucred *cred, struct socket *so,
    struct mac *mac)
{
	struct label intlabel;
	char *buffer;
	int error;

	error = mac_check_structmac_consistent(mac);
	if (error)
		return (error);

	buffer = malloc(mac->m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac->m_string, buffer, mac->m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	mac_init_socket_label(&intlabel, M_WAITOK);
	error = mac_internalize_socket_label(&intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error) {
		mac_destroy_socket_label(&intlabel);
		return (error);
	}

	mac_check_socket_relabel(cred, so, &intlabel);
	if (error) {
		mac_destroy_socket_label(&intlabel);
		return (error);
	}

	mac_relabel_socket(cred, so, &intlabel);

	mac_destroy_socket_label(&intlabel);
	return (0);
}

int
mac_pipe_label_set(struct ucred *cred, struct pipe *pipe, struct label *label)
{
	int error;

	PIPE_LOCK_ASSERT(pipe, MA_OWNED);

	error = mac_check_pipe_relabel(cred, pipe, label);
	if (error)
		return (error);

	mac_relabel_pipe(cred, pipe, label);

	return (0);
}

int
mac_getsockopt_label_get(struct ucred *cred, struct socket *so,
    struct mac *mac)
{
	char *buffer, *elements;
	int error;

	error = mac_check_structmac_consistent(mac);
	if (error)
		return (error);

	elements = malloc(mac->m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac->m_string, elements, mac->m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac->m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_externalize_socket_label(&so->so_label, elements,
	    buffer, mac->m_buflen, M_WAITOK);
	if (error == 0)
		error = copyout(buffer, mac->m_string, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}

int
mac_getsockopt_peerlabel_get(struct ucred *cred, struct socket *so,
    struct mac *mac)
{
	char *elements, *buffer;
	int error;

	error = mac_check_structmac_consistent(mac);
	if (error)
		return (error);

	elements = malloc(mac->m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac->m_string, elements, mac->m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac->m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_externalize_socket_peer_label(&so->so_peerlabel,
	    elements, buffer, mac->m_buflen, M_WAITOK);
	if (error == 0)
		error = copyout(buffer, mac->m_string, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}

/*
 * Implementation of VOP_SETLABEL() that relies on extended attributes
 * to store label data.  Can be referenced by filesystems supporting
 * extended attributes.
 */
int
vop_stdsetlabel_ea(struct vop_setlabel_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct label *intlabel = ap->a_label;
	int error;

	ASSERT_VOP_LOCKED(vp, "vop_stdsetlabel_ea");

	if ((vp->v_mount->mnt_flag & MNT_MULTILABEL) == 0)
		return (EOPNOTSUPP);

	error = mac_setlabel_vnode_extattr(ap->a_cred, vp, intlabel);
	if (error)
		return (error);

	mac_relabel_vnode(ap->a_cred, vp, intlabel);

	return (0);
}

static int
vn_setlabel(struct vnode *vp, struct label *intlabel, struct ucred *cred)
{
	int error;

	if (vp->v_mount == NULL) {
		/* printf("vn_setlabel: null v_mount\n"); */
		if (vp->v_type != VNON)
			printf("vn_setlabel: null v_mount with non-VNON\n");
		return (EBADF);
	}

	if ((vp->v_mount->mnt_flag & MNT_MULTILABEL) == 0)
		return (EOPNOTSUPP);

	/*
	 * Multi-phase commit.  First check the policies to confirm the
	 * change is OK.  Then commit via the filesystem.  Finally,
	 * update the actual vnode label.  Question: maybe the filesystem
	 * should update the vnode at the end as part of VOP_SETLABEL()?
	 */
	error = mac_check_vnode_relabel(cred, vp, intlabel);
	if (error)
		return (error);

	/*
	 * VADMIN provides the opportunity for the filesystem to make
	 * decisions about who is and is not able to modify labels
	 * and protections on files.  This might not be right.  We can't
	 * assume VOP_SETLABEL() will do it, because we might implement
	 * that as part of vop_stdsetlabel_ea().
	 */
	error = VOP_ACCESS(vp, VADMIN, cred, curthread);
	if (error)
		return (error);

	error = VOP_SETLABEL(vp, intlabel, cred, curthread);
	if (error)
		return (error);

	return (0);
}

int
__mac_get_pid(struct thread *td, struct __mac_get_pid_args *uap)
{
	char *elements, *buffer;
	struct mac mac;
	struct proc *tproc;
	struct ucred *tcred;
	int error;

	error = copyin(SCARG(uap, mac_p), &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	tproc = pfind(uap->pid);
	if (tproc == NULL)
		return (ESRCH);

	tcred = NULL;				/* Satisfy gcc. */
	error = p_cansee(td, tproc);
	if (error == 0)
		tcred = crhold(tproc->p_ucred);
	PROC_UNLOCK(tproc);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		crfree(tcred);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_externalize_cred_label(&tcred->cr_label, elements,
	    buffer, mac.m_buflen, M_WAITOK);
	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);
	crfree(tcred);
	return (error);
}

/*
 * MPSAFE
 */
int
__mac_get_proc(struct thread *td, struct __mac_get_proc_args *uap)
{
	char *elements, *buffer;
	struct mac mac;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_externalize_cred_label(&td->td_ucred->cr_label,
	    elements, buffer, mac.m_buflen, M_WAITOK);
	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);
	return (error);
}

/*
 * MPSAFE
 */
int
__mac_set_proc(struct thread *td, struct __mac_set_proc_args *uap)
{
	struct ucred *newcred, *oldcred;
	struct label intlabel;
	struct proc *p;
	struct mac mac;
	char *buffer;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, buffer, mac.m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	mac_init_cred_label(&intlabel);
	error = mac_internalize_cred_label(&intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error) {
		mac_destroy_cred_label(&intlabel);
		return (error);
	}

	newcred = crget();

	p = td->td_proc;
	PROC_LOCK(p);
	oldcred = p->p_ucred;

	error = mac_check_cred_relabel(oldcred, &intlabel);
	if (error) {
		PROC_UNLOCK(p);
		crfree(newcred);
		goto out;
	}

	setsugid(p);
	crcopy(newcred, oldcred);
	mac_relabel_cred(newcred, &intlabel);
	p->p_ucred = newcred;

	/*
	 * Grab additional reference for use while revoking mmaps, prior
	 * to releasing the proc lock and sharing the cred.
	 */
	crhold(newcred);
	PROC_UNLOCK(p);

	if (mac_enforce_vm) {
		mtx_lock(&Giant);
		mac_cred_mmapped_drop_perms(td, newcred);
		mtx_unlock(&Giant);
	}

	crfree(newcred);	/* Free revocation reference. */
	crfree(oldcred);

out:
	mac_destroy_cred_label(&intlabel);
	return (error);
}

/*
 * MPSAFE
 */
int
__mac_get_fd(struct thread *td, struct __mac_get_fd_args *uap)
{
	char *elements, *buffer;
	struct label intlabel;
	struct file *fp;
	struct mac mac;
	struct vnode *vp;
	struct pipe *pipe;
	short label_type;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	mtx_lock(&Giant);				/* VFS */
	error = fget(td, SCARG(uap, fd), &fp);
	if (error)
		goto out;

	label_type = fp->f_type;
	switch (fp->f_type) {
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		vp = (struct vnode *)fp->f_data;

		mac_init_vnode_label(&intlabel);

		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		mac_copy_vnode_label(&vp->v_label, &intlabel);
		VOP_UNLOCK(vp, 0, td);

		break;
	case DTYPE_PIPE:
		pipe = (struct pipe *)fp->f_data;

		mac_init_pipe_label(&intlabel);

		PIPE_LOCK(pipe);
		mac_copy_pipe_label(pipe->pipe_label, &intlabel);
		PIPE_UNLOCK(pipe);
		break;
	default:
		error = EINVAL;
		fdrop(fp, td);
		goto out;
	}
	fdrop(fp, td);

	switch (label_type) {
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		if (error == 0)
			error = mac_externalize_vnode_label(&intlabel,
			    elements, buffer, mac.m_buflen, M_WAITOK);
		mac_destroy_vnode_label(&intlabel);
		break;
	case DTYPE_PIPE:
		error = mac_externalize_pipe_label(&intlabel, elements,
		    buffer, mac.m_buflen, M_WAITOK);
		mac_destroy_pipe_label(&intlabel);
		break;
	default:
		panic("__mac_get_fd: corrupted label_type");
	}

	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

out:
	mtx_unlock(&Giant);				/* VFS */
	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}

/*
 * MPSAFE
 */
int
__mac_get_file(struct thread *td, struct __mac_get_file_args *uap)
{
	char *elements, *buffer;
	struct nameidata nd;
	struct label intlabel;
	struct mac mac;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	mtx_lock(&Giant);				/* VFS */
	NDINIT(&nd, LOOKUP, LOCKLEAF | FOLLOW, UIO_USERSPACE, uap->path_p,
	    td);
	error = namei(&nd);
	if (error)
		goto out;

	mac_init_vnode_label(&intlabel);
	mac_copy_vnode_label(&nd.ni_vp->v_label, &intlabel);
	error = mac_externalize_vnode_label(&intlabel, elements, buffer,
	    mac.m_buflen, M_WAITOK);

	NDFREE(&nd, 0);
	mac_destroy_vnode_label(&intlabel);

	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

out:
	mtx_unlock(&Giant);				/* VFS */

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}

/*
 * MPSAFE
 */
int
__mac_get_link(struct thread *td, struct __mac_get_link_args *uap)
{
	char *elements, *buffer;
	struct nameidata nd;
	struct label intlabel;
	struct mac mac;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	mtx_lock(&Giant);				/* VFS */
	NDINIT(&nd, LOOKUP, LOCKLEAF | NOFOLLOW, UIO_USERSPACE, uap->path_p,
	    td);
	error = namei(&nd);
	if (error)
		goto out;

	mac_init_vnode_label(&intlabel);
	mac_copy_vnode_label(&nd.ni_vp->v_label, &intlabel);
	error = mac_externalize_vnode_label(&intlabel, elements, buffer,
	    mac.m_buflen, M_WAITOK);
	NDFREE(&nd, 0);
	mac_destroy_vnode_label(&intlabel);

	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

out:
	mtx_unlock(&Giant);				/* VFS */

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}

/*
 * MPSAFE
 */
int
__mac_set_fd(struct thread *td, struct __mac_set_fd_args *uap)
{
	struct label intlabel;
	struct pipe *pipe;
	struct file *fp;
	struct mount *mp;
	struct vnode *vp;
	struct mac mac;
	char *buffer;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, buffer, mac.m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	mtx_lock(&Giant);				/* VFS */

	error = fget(td, SCARG(uap, fd), &fp);
	if (error)
		goto out;

	switch (fp->f_type) {
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		mac_init_vnode_label(&intlabel);
		error = mac_internalize_vnode_label(&intlabel, buffer);
		if (error) {
			mac_destroy_vnode_label(&intlabel);
			break;
		}

		vp = (struct vnode *)fp->f_data;
		error = vn_start_write(vp, &mp, V_WAIT | PCATCH);
		if (error != 0) {
			mac_destroy_vnode_label(&intlabel);
			break;
		}

		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		error = vn_setlabel(vp, &intlabel, td->td_ucred);
		VOP_UNLOCK(vp, 0, td);
		vn_finished_write(mp);

		mac_destroy_vnode_label(&intlabel);
		break;

	case DTYPE_PIPE:
		mac_init_pipe_label(&intlabel);
		error = mac_internalize_pipe_label(&intlabel, buffer);
		if (error == 0) {
			pipe = (struct pipe *)fp->f_data;
			PIPE_LOCK(pipe);
			error = mac_pipe_label_set(td->td_ucred, pipe,
			    &intlabel);
			PIPE_UNLOCK(pipe);
		}

		mac_destroy_pipe_label(&intlabel);
		break;

	default:
		error = EINVAL;
	}

	fdrop(fp, td);
out:
	mtx_unlock(&Giant);				/* VFS */

	free(buffer, M_MACTEMP);

	return (error);
}

/*
 * MPSAFE
 */
int
__mac_set_file(struct thread *td, struct __mac_set_file_args *uap)
{
	struct label intlabel;
	struct nameidata nd;
	struct mount *mp;
	struct mac mac;
	char *buffer;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, buffer, mac.m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	mac_init_vnode_label(&intlabel);
	error = mac_internalize_vnode_label(&intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error) {
		mac_destroy_vnode_label(&intlabel);
		return (error);
	}

	mtx_lock(&Giant);				/* VFS */

	NDINIT(&nd, LOOKUP, LOCKLEAF | FOLLOW, UIO_USERSPACE, uap->path_p,
	    td);
	error = namei(&nd);
	if (error == 0) {
		error = vn_start_write(nd.ni_vp, &mp, V_WAIT | PCATCH);
		if (error == 0)
			error = vn_setlabel(nd.ni_vp, &intlabel,
			    td->td_ucred);
		vn_finished_write(mp);
	}

	NDFREE(&nd, 0);
	mtx_unlock(&Giant);				/* VFS */
	mac_destroy_vnode_label(&intlabel);

	return (error);
}

/*
 * MPSAFE
 */
int
__mac_set_link(struct thread *td, struct __mac_set_link_args *uap)
{
	struct label intlabel;
	struct nameidata nd;
	struct mount *mp;
	struct mac mac;
	char *buffer;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, buffer, mac.m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	mac_init_vnode_label(&intlabel);
	error = mac_internalize_vnode_label(&intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error) {
		mac_destroy_vnode_label(&intlabel);
		return (error);
	}

	mtx_lock(&Giant);				/* VFS */

	NDINIT(&nd, LOOKUP, LOCKLEAF | NOFOLLOW, UIO_USERSPACE, uap->path_p,
	    td);
	error = namei(&nd);
	if (error == 0) {
		error = vn_start_write(nd.ni_vp, &mp, V_WAIT | PCATCH);
		if (error == 0)
			error = vn_setlabel(nd.ni_vp, &intlabel,
			    td->td_ucred);
		vn_finished_write(mp);
	}

	NDFREE(&nd, 0);
	mtx_unlock(&Giant);				/* VFS */
	mac_destroy_vnode_label(&intlabel);

	return (error);
}

/*
 * MPSAFE
 */
int
mac_syscall(struct thread *td, struct mac_syscall_args *uap)
{
	struct mac_policy_conf *mpc;
	char target[MAC_MAX_POLICY_NAME];
	int error;

	error = copyinstr(SCARG(uap, policy), target, sizeof(target), NULL);
	if (error)
		return (error);

	error = ENOSYS;
	MAC_POLICY_LIST_BUSY();
	LIST_FOREACH(mpc, &mac_policy_list, mpc_list) {
		if (strcmp(mpc->mpc_name, target) == 0 &&
		    mpc->mpc_ops->mpo_syscall != NULL) {
			error = mpc->mpc_ops->mpo_syscall(td,
			    SCARG(uap, call), SCARG(uap, arg));
			goto out;
		}
	}

out:
	MAC_POLICY_LIST_UNBUSY();
	return (error);
}

SYSINIT(mac, SI_SUB_MAC, SI_ORDER_FIRST, mac_init, NULL);
SYSINIT(mac_late, SI_SUB_MAC_LATE, SI_ORDER_FIRST, mac_late_init, NULL);

#else /* !MAC */

int
__mac_get_pid(struct thread *td, struct __mac_get_pid_args *uap)
{

	return (ENOSYS);
}

int
__mac_get_proc(struct thread *td, struct __mac_get_proc_args *uap)
{

	return (ENOSYS);
}

int
__mac_set_proc(struct thread *td, struct __mac_set_proc_args *uap)
{

	return (ENOSYS);
}

int
__mac_get_fd(struct thread *td, struct __mac_get_fd_args *uap)
{

	return (ENOSYS);
}

int
__mac_get_file(struct thread *td, struct __mac_get_file_args *uap)
{

	return (ENOSYS);
}

int
__mac_get_link(struct thread *td, struct __mac_get_link_args *uap)
{

	return (ENOSYS);
}

int
__mac_set_fd(struct thread *td, struct __mac_set_fd_args *uap)
{

	return (ENOSYS);
}

int
__mac_set_file(struct thread *td, struct __mac_set_file_args *uap)
{

	return (ENOSYS);
}

int
__mac_set_link(struct thread *td, struct __mac_set_link_args *uap)
{

	return (ENOSYS);
}

int
mac_syscall(struct thread *td, struct mac_syscall_args *uap)
{

	return (ENOSYS);
}

#endif
