/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001, 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson and Ilmar Habibulin for the
 * TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by NAI Labs,
 * the Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#ifndef MAC_MAX_POLICIES
#define	MAC_MAX_POLICIES	8
#endif
#if MAC_MAX_POLICIES > 32
#error "MAC_MAX_POLICIES too large"
#endif
static unsigned int mac_max_policies = MAC_MAX_POLICIES;
static unsigned int mac_policy_offsets_free = (1 << MAC_MAX_POLICIES) - 1;
SYSCTL_UINT(_security_mac, OID_AUTO, max_policies, CTLFLAG_RD,
    &mac_max_policies, 0, "");

static int	mac_late = 0;

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

static int     mac_enforce_vm = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_vm, CTLFLAG_RW,
    &mac_enforce_vm, 0, "Enforce MAC policy on vm operations");
TUNABLE_INT("security.mac.enforce_vm", &mac_enforce_vm);

static int	mac_label_size = sizeof(struct mac);
SYSCTL_INT(_security_mac, OID_AUTO, label_size, CTLFLAG_RD,
    &mac_label_size, 0, "Pre-compiled MAC label size");

static int	mac_cache_fslabel_in_vnode = 1;
SYSCTL_INT(_security_mac, OID_AUTO, cache_fslabel_in_vnode, CTLFLAG_RW,
    &mac_cache_fslabel_in_vnode, 0, "Cache mount fslabel in vnode");
TUNABLE_INT("security.mac.cache_fslabel_in_vnode",
    &mac_cache_fslabel_in_vnode);

static int	mac_vnode_label_cache_hits = 0;
SYSCTL_INT(_security_mac, OID_AUTO, vnode_label_cache_hits, CTLFLAG_RD,
    &mac_vnode_label_cache_hits, 0, "Cache hits on vnode labels");
static int	mac_vnode_label_cache_misses = 0;
SYSCTL_INT(_security_mac, OID_AUTO, vnode_label_cache_misses, CTLFLAG_RD,
    &mac_vnode_label_cache_misses, 0, "Cache misses on vnode labels");

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
static int	mac_externalize(struct label *label, struct mac *mac);
static int	mac_policy_register(struct mac_policy_conf *mpc);
static int	mac_policy_unregister(struct mac_policy_conf *mpc);

static int	mac_stdcreatevnode_ea(struct vnode *vp);
static void	mac_check_vnode_mmap_downgrade(struct ucred *cred,
		    struct vnode *vp, int *prot);
static void	mac_cred_mmapped_drop_perms_recurse(struct thread *td,
		    struct ucred *cred, struct vm_map *map);

static void	mac_destroy_socket_label(struct label *label);

MALLOC_DEFINE(M_MACOPVEC, "macopvec", "MAC policy operation vector");
MALLOC_DEFINE(M_MACPIPELABEL, "macpipelabel", "MAC labels for pipes");

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
	struct mac_policy_op_entry *mpe;
	int slot;

	MALLOC(mpc->mpc_ops, struct mac_policy_ops *, sizeof(*mpc->mpc_ops),
	    M_MACOPVEC, M_WAITOK | M_ZERO);
	for (mpe = mpc->mpc_entries; mpe->mpe_constant != MAC_OP_LAST; mpe++) {
		switch (mpe->mpe_constant) {
		case MAC_OP_LAST:
			/*
			 * Doesn't actually happen, but this allows checking
			 * that all enumerated values are handled.
			 */
			break;
		case MAC_DESTROY:
			mpc->mpc_ops->mpo_destroy =
			    mpe->mpe_function;
			break;
		case MAC_INIT:
			mpc->mpc_ops->mpo_init =
			    mpe->mpe_function;
			break;
		case MAC_SYSCALL:
			mpc->mpc_ops->mpo_syscall =
			    mpe->mpe_function;
			break;
		case MAC_INIT_BPFDESC_LABEL:
			mpc->mpc_ops->mpo_init_bpfdesc_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_CRED_LABEL:
			mpc->mpc_ops->mpo_init_cred_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_DEVFSDIRENT_LABEL:
			mpc->mpc_ops->mpo_init_devfsdirent_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_IFNET_LABEL:
			mpc->mpc_ops->mpo_init_ifnet_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_IPQ_LABEL:
			mpc->mpc_ops->mpo_init_ipq_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_MBUF_LABEL:
			mpc->mpc_ops->mpo_init_mbuf_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_MOUNT_LABEL:
			mpc->mpc_ops->mpo_init_mount_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_MOUNT_FS_LABEL:
			mpc->mpc_ops->mpo_init_mount_fs_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_PIPE_LABEL:
			mpc->mpc_ops->mpo_init_pipe_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_SOCKET_LABEL:
			mpc->mpc_ops->mpo_init_socket_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_SOCKET_PEER_LABEL:
			mpc->mpc_ops->mpo_init_socket_peer_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_TEMP_LABEL:
			mpc->mpc_ops->mpo_init_temp_label =
			    mpe->mpe_function;
			break;
		case MAC_INIT_VNODE_LABEL:
			mpc->mpc_ops->mpo_init_vnode_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_BPFDESC_LABEL:
			mpc->mpc_ops->mpo_destroy_bpfdesc_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_CRED_LABEL:
			mpc->mpc_ops->mpo_destroy_cred_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_DEVFSDIRENT_LABEL:
			mpc->mpc_ops->mpo_destroy_devfsdirent_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_IFNET_LABEL:
			mpc->mpc_ops->mpo_destroy_ifnet_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_IPQ_LABEL:
			mpc->mpc_ops->mpo_destroy_ipq_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_MBUF_LABEL:
			mpc->mpc_ops->mpo_destroy_mbuf_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_MOUNT_LABEL:
			mpc->mpc_ops->mpo_destroy_mount_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_MOUNT_FS_LABEL:
			mpc->mpc_ops->mpo_destroy_mount_fs_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_PIPE_LABEL:
			mpc->mpc_ops->mpo_destroy_pipe_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_SOCKET_LABEL:
			mpc->mpc_ops->mpo_destroy_socket_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_SOCKET_PEER_LABEL:
			mpc->mpc_ops->mpo_destroy_socket_peer_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_TEMP_LABEL:
			mpc->mpc_ops->mpo_destroy_temp_label =
			    mpe->mpe_function;
			break;
		case MAC_DESTROY_VNODE_LABEL:
			mpc->mpc_ops->mpo_destroy_vnode_label =
			    mpe->mpe_function;
			break;
		case MAC_EXTERNALIZE:
			mpc->mpc_ops->mpo_externalize =
			    mpe->mpe_function;
			break;
		case MAC_INTERNALIZE:
			mpc->mpc_ops->mpo_internalize =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_DEVFS_DEVICE:
			mpc->mpc_ops->mpo_create_devfs_device =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_DEVFS_DIRECTORY:
			mpc->mpc_ops->mpo_create_devfs_directory =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_DEVFS_SYMLINK:
			mpc->mpc_ops->mpo_create_devfs_symlink =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_DEVFS_VNODE:
			mpc->mpc_ops->mpo_create_devfs_vnode =
			    mpe->mpe_function;
			break;
		case MAC_STDCREATEVNODE_EA:
			mpc->mpc_ops->mpo_stdcreatevnode_ea =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_VNODE:
			mpc->mpc_ops->mpo_create_vnode =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_MOUNT:
			mpc->mpc_ops->mpo_create_mount =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_ROOT_MOUNT:
			mpc->mpc_ops->mpo_create_root_mount =
			    mpe->mpe_function;
			break;
		case MAC_RELABEL_VNODE:
			mpc->mpc_ops->mpo_relabel_vnode =
			    mpe->mpe_function;
			break;
		case MAC_UPDATE_DEVFSDIRENT:
			mpc->mpc_ops->mpo_update_devfsdirent =
			    mpe->mpe_function;
			break;
		case MAC_UPDATE_PROCFSVNODE:
			mpc->mpc_ops->mpo_update_procfsvnode =
			    mpe->mpe_function;
			break;
		case MAC_UPDATE_VNODE_FROM_EXTATTR:
			mpc->mpc_ops->mpo_update_vnode_from_extattr =
			    mpe->mpe_function;
			break;
		case MAC_UPDATE_VNODE_FROM_EXTERNALIZED:
			mpc->mpc_ops->mpo_update_vnode_from_externalized =
			    mpe->mpe_function;
			break;
		case MAC_UPDATE_VNODE_FROM_MOUNT:
			mpc->mpc_ops->mpo_update_vnode_from_mount =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_MBUF_FROM_SOCKET:
			mpc->mpc_ops->mpo_create_mbuf_from_socket =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_PIPE:
			mpc->mpc_ops->mpo_create_pipe =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_SOCKET:
			mpc->mpc_ops->mpo_create_socket =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_SOCKET_FROM_SOCKET:
			mpc->mpc_ops->mpo_create_socket_from_socket =
			    mpe->mpe_function;
			break;
		case MAC_RELABEL_PIPE:
			mpc->mpc_ops->mpo_relabel_pipe =
			    mpe->mpe_function;
			break;
		case MAC_RELABEL_SOCKET:
			mpc->mpc_ops->mpo_relabel_socket =
			    mpe->mpe_function;
			break;
		case MAC_SET_SOCKET_PEER_FROM_MBUF:
			mpc->mpc_ops->mpo_set_socket_peer_from_mbuf =
			    mpe->mpe_function;
			break;
		case MAC_SET_SOCKET_PEER_FROM_SOCKET:
			mpc->mpc_ops->mpo_set_socket_peer_from_socket =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_BPFDESC:
			mpc->mpc_ops->mpo_create_bpfdesc =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_DATAGRAM_FROM_IPQ:
			mpc->mpc_ops->mpo_create_datagram_from_ipq =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_FRAGMENT:
			mpc->mpc_ops->mpo_create_fragment =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_IFNET:
			mpc->mpc_ops->mpo_create_ifnet =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_IPQ:
			mpc->mpc_ops->mpo_create_ipq =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_MBUF_FROM_MBUF:
			mpc->mpc_ops->mpo_create_mbuf_from_mbuf =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_MBUF_LINKLAYER:
			mpc->mpc_ops->mpo_create_mbuf_linklayer =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_MBUF_FROM_BPFDESC:
			mpc->mpc_ops->mpo_create_mbuf_from_bpfdesc =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_MBUF_FROM_IFNET:
			mpc->mpc_ops->mpo_create_mbuf_from_ifnet =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_MBUF_MULTICAST_ENCAP:
			mpc->mpc_ops->mpo_create_mbuf_multicast_encap =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_MBUF_NETLAYER:
			mpc->mpc_ops->mpo_create_mbuf_netlayer =
			    mpe->mpe_function;
			break;
		case MAC_FRAGMENT_MATCH:
			mpc->mpc_ops->mpo_fragment_match =
			    mpe->mpe_function;
			break;
		case MAC_RELABEL_IFNET:
			mpc->mpc_ops->mpo_relabel_ifnet =
			    mpe->mpe_function;
			break;
		case MAC_UPDATE_IPQ:
			mpc->mpc_ops->mpo_update_ipq =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_CRED:
			mpc->mpc_ops->mpo_create_cred =
			    mpe->mpe_function;
			break;
		case MAC_EXECVE_TRANSITION:
			mpc->mpc_ops->mpo_execve_transition =
			    mpe->mpe_function;
			break;
		case MAC_EXECVE_WILL_TRANSITION:
			mpc->mpc_ops->mpo_execve_will_transition =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_PROC0:
			mpc->mpc_ops->mpo_create_proc0 =
			    mpe->mpe_function;
			break;
		case MAC_CREATE_PROC1:
			mpc->mpc_ops->mpo_create_proc1 =
			    mpe->mpe_function;
			break;
		case MAC_RELABEL_CRED:
			mpc->mpc_ops->mpo_relabel_cred =
			    mpe->mpe_function;
			break;
		case MAC_THREAD_USERRET:
			mpc->mpc_ops->mpo_thread_userret =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_BPFDESC_RECEIVE:
			mpc->mpc_ops->mpo_check_bpfdesc_receive =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_CRED_RELABEL:
			mpc->mpc_ops->mpo_check_cred_relabel =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_CRED_VISIBLE:
			mpc->mpc_ops->mpo_check_cred_visible =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_IFNET_RELABEL:
			mpc->mpc_ops->mpo_check_ifnet_relabel =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_IFNET_TRANSMIT:
			mpc->mpc_ops->mpo_check_ifnet_transmit =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_MOUNT_STAT:
			mpc->mpc_ops->mpo_check_mount_stat =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_PIPE_IOCTL:
			mpc->mpc_ops->mpo_check_pipe_ioctl =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_PIPE_POLL:
			mpc->mpc_ops->mpo_check_pipe_poll =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_PIPE_READ:
			mpc->mpc_ops->mpo_check_pipe_read =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_PIPE_RELABEL:
			mpc->mpc_ops->mpo_check_pipe_relabel =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_PIPE_STAT:
			mpc->mpc_ops->mpo_check_pipe_stat =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_PIPE_WRITE:
			mpc->mpc_ops->mpo_check_pipe_write =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_PROC_DEBUG:
			mpc->mpc_ops->mpo_check_proc_debug =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_PROC_SCHED:
			mpc->mpc_ops->mpo_check_proc_sched =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_PROC_SIGNAL:
			mpc->mpc_ops->mpo_check_proc_signal =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_SOCKET_BIND:
			mpc->mpc_ops->mpo_check_socket_bind =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_SOCKET_CONNECT:
			mpc->mpc_ops->mpo_check_socket_connect =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_SOCKET_DELIVER:
			mpc->mpc_ops->mpo_check_socket_deliver =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_SOCKET_LISTEN:
			mpc->mpc_ops->mpo_check_socket_listen =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_SOCKET_RELABEL:
			mpc->mpc_ops->mpo_check_socket_relabel =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_SOCKET_VISIBLE:
			mpc->mpc_ops->mpo_check_socket_visible =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_ACCESS:
			mpc->mpc_ops->mpo_check_vnode_access =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_CHDIR:
			mpc->mpc_ops->mpo_check_vnode_chdir =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_CHROOT:
			mpc->mpc_ops->mpo_check_vnode_chroot =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_CREATE:
			mpc->mpc_ops->mpo_check_vnode_create =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_DELETE:
			mpc->mpc_ops->mpo_check_vnode_delete =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_DELETEACL:
			mpc->mpc_ops->mpo_check_vnode_deleteacl =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_EXEC:
			mpc->mpc_ops->mpo_check_vnode_exec =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_GETACL:
			mpc->mpc_ops->mpo_check_vnode_getacl =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_GETEXTATTR:
			mpc->mpc_ops->mpo_check_vnode_getextattr =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_LINK:
			mpc->mpc_ops->mpo_check_vnode_link =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_LOOKUP:
			mpc->mpc_ops->mpo_check_vnode_lookup =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_MMAP:
			mpc->mpc_ops->mpo_check_vnode_mmap =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_MMAP_DOWNGRADE:
			mpc->mpc_ops->mpo_check_vnode_mmap_downgrade =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_MPROTECT:
			mpc->mpc_ops->mpo_check_vnode_mprotect =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_OPEN:
			mpc->mpc_ops->mpo_check_vnode_open =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_POLL:
			mpc->mpc_ops->mpo_check_vnode_poll =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_READ:
			mpc->mpc_ops->mpo_check_vnode_read =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_READDIR:
			mpc->mpc_ops->mpo_check_vnode_readdir =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_READLINK:
			mpc->mpc_ops->mpo_check_vnode_readlink =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_RELABEL:
			mpc->mpc_ops->mpo_check_vnode_relabel =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_RENAME_FROM:
			mpc->mpc_ops->mpo_check_vnode_rename_from =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_RENAME_TO:
			mpc->mpc_ops->mpo_check_vnode_rename_to =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_REVOKE:
			mpc->mpc_ops->mpo_check_vnode_revoke =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_SETACL:
			mpc->mpc_ops->mpo_check_vnode_setacl =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_SETEXTATTR:
			mpc->mpc_ops->mpo_check_vnode_setextattr =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_SETFLAGS:
			mpc->mpc_ops->mpo_check_vnode_setflags =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_SETMODE:
			mpc->mpc_ops->mpo_check_vnode_setmode =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_SETOWNER:
			mpc->mpc_ops->mpo_check_vnode_setowner =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_SETUTIMES:
			mpc->mpc_ops->mpo_check_vnode_setutimes =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_STAT:
			mpc->mpc_ops->mpo_check_vnode_stat =
			    mpe->mpe_function;
			break;
		case MAC_CHECK_VNODE_WRITE:
			mpc->mpc_ops->mpo_check_vnode_write =
			    mpe->mpe_function;
			break;
/*
		default:
			printf("MAC policy `%s': unknown operation %d\n",
			    mpc->mpc_name, mpe->mpe_constant);
			return (EINVAL);
*/
		}
	}
	MAC_POLICY_LIST_LOCK();
	if (mac_policy_list_busy > 0) {
		MAC_POLICY_LIST_UNLOCK();
		FREE(mpc->mpc_ops, M_MACOPVEC);
		mpc->mpc_ops = NULL;
		return (EBUSY);
	}
	LIST_FOREACH(tmpc, &mac_policy_list, mpc_list) {
		if (strcmp(tmpc->mpc_name, mpc->mpc_name) == 0) {
			MAC_POLICY_LIST_UNLOCK();
			FREE(mpc->mpc_ops, M_MACOPVEC);
			mpc->mpc_ops = NULL;
			return (EEXIST);
		}
	}
	if (mpc->mpc_field_off != NULL) {
		slot = ffs(mac_policy_offsets_free);
		if (slot == 0) {
			MAC_POLICY_LIST_UNLOCK();
			FREE(mpc->mpc_ops, M_MACOPVEC);
			mpc->mpc_ops = NULL;
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

	FREE(mpc->mpc_ops, M_MACOPVEC);
	mpc->mpc_ops = NULL;

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

static void
mac_init_structmac(struct mac *mac)
{

	bzero(mac, sizeof(*mac));
	mac->m_macflags = MAC_FLAG_INITIALIZED;
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

void
mac_init_cred(struct ucred *cr)
{

	mac_init_label(&cr->cr_label);
	MAC_PERFORM(init_cred_label, &cr->cr_label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmaccreds, 1);
#endif
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

void
mac_init_ifnet(struct ifnet *ifp)
{

	mac_init_label(&ifp->if_label);
	MAC_PERFORM(init_ifnet_label, &ifp->if_label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmacifnets, 1);
#endif
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

void
mac_init_pipe(struct pipe *pipe)
{
	struct label *label;

	label = malloc(sizeof(struct label), M_MACPIPELABEL, M_ZERO|M_WAITOK);
	mac_init_label(label);
	pipe->pipe_label = label;
	pipe->pipe_peer->pipe_label = label;
	MAC_PERFORM(init_pipe_label, pipe->pipe_label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmacpipes, 1);
#endif
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

static void
mac_init_temp(struct label *label)
{

	mac_init_label(label);
	MAC_PERFORM(init_temp_label, label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmactemp, 1);
#endif
}

void
mac_init_vnode(struct vnode *vp)
{

	mac_init_label(&vp->v_label);
	MAC_PERFORM(init_vnode_label, &vp->v_label);
#ifdef MAC_DEBUG
	atomic_add_int(&nmacvnodes, 1);
#endif
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

void
mac_destroy_cred(struct ucred *cr)
{

	MAC_PERFORM(destroy_cred_label, &cr->cr_label);
	mac_destroy_label(&cr->cr_label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmaccreds, 1);
#endif
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

void
mac_destroy_ifnet(struct ifnet *ifp)
{

	MAC_PERFORM(destroy_ifnet_label, &ifp->if_label);
	mac_destroy_label(&ifp->if_label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacifnets, 1);
#endif
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

void
mac_destroy_pipe(struct pipe *pipe)
{

	MAC_PERFORM(destroy_pipe_label, pipe->pipe_label);
	mac_destroy_label(pipe->pipe_label);
	free(pipe->pipe_label, M_MACPIPELABEL);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacpipes, 1);
#endif
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

static void
mac_destroy_temp(struct label *label)
{

	MAC_PERFORM(destroy_temp_label, label);
	mac_destroy_label(label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmactemp, 1);
#endif
}

void
mac_destroy_vnode(struct vnode *vp)
{

	MAC_PERFORM(destroy_vnode_label, &vp->v_label);
	mac_destroy_label(&vp->v_label);
#ifdef MAC_DEBUG
	atomic_subtract_int(&nmacvnodes, 1);
#endif
}

static int
mac_externalize(struct label *label, struct mac *mac)
{
	int error;

	mac_init_structmac(mac);
	MAC_CHECK(externalize, label, mac);

	return (error);
}

static int
mac_internalize(struct label *label, struct mac *mac)
{
	int error;

	mac_init_temp(label);
	MAC_CHECK(internalize, label, mac);
	if (error)
		mac_destroy_temp(label);

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
mac_update_procfsvnode(struct vnode *vp, struct ucred *cred)
{

	MAC_PERFORM(update_procfsvnode, vp, &vp->v_label, cred);
}

/*
 * Support callout for policies that manage their own externalization
 * using extended attributes.
 */
static int
mac_update_vnode_from_extattr(struct vnode *vp, struct mount *mp)
{
	int error;

	MAC_CHECK(update_vnode_from_extattr, vp, &vp->v_label, mp,
	    &mp->mnt_fslabel);

	return (error);
}

/*
 * Given an externalized mac label, internalize it and stamp it on a
 * vnode.
 */
static int
mac_update_vnode_from_externalized(struct vnode *vp, struct mac *extmac)
{
	int error;

	MAC_CHECK(update_vnode_from_externalized, vp, &vp->v_label, extmac);

	return (error);
}

/*
 * Call out to individual policies to update the label in a vnode from
 * the mountpoint.
 */
void
mac_update_vnode_from_mount(struct vnode *vp, struct mount *mp)
{

	MAC_PERFORM(update_vnode_from_mount, vp, &vp->v_label, mp,
	    &mp->mnt_fslabel);

	ASSERT_VOP_LOCKED(vp, "mac_update_vnode_from_mount");
	if (mac_cache_fslabel_in_vnode)
		vp->v_vflag |= VV_CACHEDLABEL;
}

/*
 * Implementation of VOP_REFRESHLABEL() that relies on extended attributes
 * to store label data.  Can be referenced by filesystems supporting
 * extended attributes.
 */
int
vop_stdrefreshlabel_ea(struct vop_refreshlabel_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct mac extmac;
	int buflen, error;

	ASSERT_VOP_LOCKED(vp, "vop_stdrefreshlabel_ea");

	/*
	 * Call out to external policies first.  Order doesn't really
	 * matter, as long as failure of one assures failure of all.
	 */
	error = mac_update_vnode_from_extattr(vp, vp->v_mount);
	if (error)
		return (error);

	buflen = sizeof(extmac);
	error = vn_extattr_get(vp, IO_NODELOCKED,
	    FREEBSD_MAC_EXTATTR_NAMESPACE, FREEBSD_MAC_EXTATTR_NAME, &buflen,
	    (char *)&extmac, curthread);
	switch (error) {
	case 0:
		/* Got it */
		break;

	case ENOATTR:
		/*
		 * Use the label from the mount point.
		 */
		mac_update_vnode_from_mount(vp, vp->v_mount);
		return (0);

	case EOPNOTSUPP:
	default:
		/* Fail horribly. */
		return (error);
	}

	if (buflen != sizeof(extmac))
		error = EPERM;		/* Fail very closed. */
	if (error == 0)
		error = mac_update_vnode_from_externalized(vp, &extmac);
	if (error == 0)
		vp->v_vflag |= VV_CACHEDLABEL;
	else {
		struct vattr va;

		printf("Corrupted label on %s",
		    vp->v_mount->mnt_stat.f_mntonname);
		if (VOP_GETATTR(vp, &va, curthread->td_ucred, curthread) == 0)
			printf(" inum %ld", va.va_fileid);
#ifdef MAC_DEBUG
		if (mac_debug_label_fallback) {
			printf(", falling back.\n");
			mac_update_vnode_from_mount(vp, vp->v_mount);
			error = 0;
		} else {
#endif
			printf(".\n");
			error = EPERM;
#ifdef MAC_DEBUG
		}
#endif
	}

	return (error);
}

/*
 * Make sure the vnode label is up-to-date.  If EOPNOTSUPP, then we handle
 * the labeling activity outselves.  Filesystems should be careful not
 * to change their minds regarding whether they support vop_refreshlabel()
 * for a vnode or not.  Don't cache the vnode here, allow the file
 * system code to determine if it's safe to cache.  If we update from
 * the mount, don't cache since a change to the mount label should affect
 * all vnodes.
 */
static int
vn_refreshlabel(struct vnode *vp, struct ucred *cred)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "vn_refreshlabel");

	if (vp->v_mount == NULL) {
/*
		Eventually, we probably want to special-case refreshing
		of deadfs vnodes, and if there's a lock-free race somewhere,
		that case might be handled here.

		mac_update_vnode_deadfs(vp);
		return (0);
 */
		/* printf("vn_refreshlabel: null v_mount\n"); */
		if (vp->v_type != VNON)
			printf(
			    "vn_refreshlabel: null v_mount with non-VNON\n");
		return (EBADF);
	}

	if (vp->v_vflag & VV_CACHEDLABEL) {
		mac_vnode_label_cache_hits++;
		return (0);
	} else
		mac_vnode_label_cache_misses++;

	if ((vp->v_mount->mnt_flag & MNT_MULTILABEL) == 0) {
		mac_update_vnode_from_mount(vp, vp->v_mount);
		return (0);
	}

	error = VOP_REFRESHLABEL(vp, cred, curthread);
	switch (error) {
	case EOPNOTSUPP:
		/*
		 * If labels are not supported on this vnode, fall back to
		 * the label in the mount and propagate it to the vnode.
		 * There should probably be some sort of policy/flag/decision
		 * about doing this.
		 */
		mac_update_vnode_from_mount(vp, vp->v_mount);
		error = 0;
	default:
		return (error);
	}
}

/*
 * Helper function for file systems using the vop_std*_ea() calls.  This
 * function must be called after EA service is available for the vnode,
 * but before it's hooked up to the namespace so that the node persists
 * if there's a crash, or before it can be accessed.  On successful
 * commit of the label to disk (etc), do cache the label.
 */
int
vop_stdcreatevnode_ea(struct vnode *dvp, struct vnode *tvp, struct ucred *cred)
{
	struct mac extmac;
	int error;

	ASSERT_VOP_LOCKED(tvp, "vop_stdcreatevnode_ea");
	if ((dvp->v_mount->mnt_flag & MNT_MULTILABEL) == 0) {
		mac_update_vnode_from_mount(tvp, tvp->v_mount);
	} else {
		error = vn_refreshlabel(dvp, cred);
		if (error)
			return (error);

		/*
		 * Stick the label in the vnode.  Then try to write to
		 * disk.  If we fail, return a failure to abort the
		 * create operation.  Really, this failure shouldn't
		 * happen except in fairly unusual circumstances (out
		 * of disk, etc).
		 */
		mac_create_vnode(cred, dvp, tvp);

		error = mac_stdcreatevnode_ea(tvp);
		if (error)
			return (error);

		/*
		 * XXX: Eventually this will go away and all policies will
		 * directly manage their extended attributes.
		 */
		error = mac_externalize(&tvp->v_label, &extmac);
		if (error)
			return (error);

		error = vn_extattr_set(tvp, IO_NODELOCKED,
		    FREEBSD_MAC_EXTATTR_NAMESPACE, FREEBSD_MAC_EXTATTR_NAME,
		    sizeof(extmac), (char *)&extmac, curthread);
		if (error == 0)
			tvp->v_vflag |= VV_CACHEDLABEL;
		else {
#if 0
			/*
			 * In theory, we could have fall-back behavior here.
			 * It would probably be incorrect.
			 */
#endif
			return (error);
		}
	}

	return (0);
}

void
mac_execve_transition(struct ucred *old, struct ucred *new, struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_execve_transition");

	error = vn_refreshlabel(vp, old);
	if (error) {
		printf("mac_execve_transition: vn_refreshlabel returned %d\n",
		    error);
		printf("mac_execve_transition: using old vnode label\n");
	}

	MAC_PERFORM(execve_transition, old, new, vp, &vp->v_label);
}

int
mac_execve_will_transition(struct ucred *old, struct vnode *vp)
{
	int error, result;

	error = vn_refreshlabel(vp, old);
	if (error)
		return (error);

	result = 0;
	MAC_BOOLEAN(execve_will_transition, ||, old, vp, &vp->v_label);

	return (result);
}

int
mac_check_vnode_access(struct ucred *cred, struct vnode *vp, int flags)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_access");

	if (!mac_enforce_fs)
		return (0);

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

	MAC_CHECK(check_vnode_access, cred, vp, &vp->v_label, flags);
	return (error);
}

int
mac_check_vnode_chdir(struct ucred *cred, struct vnode *dvp)
{
	int error;

	ASSERT_VOP_LOCKED(dvp, "mac_check_vnode_chdir");

	if (!mac_enforce_fs)
		return (0);

	error = vn_refreshlabel(dvp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(dvp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(dvp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(dvp, cred);
	if (error)
		return (error);
	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);
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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(dvp, cred);
	if (error)
		return (error);

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(dvp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

	MAC_CHECK(check_vnode_mprotect, cred, vp, &vp->v_label, prot);
	return (error);
}

int
mac_check_vnode_open(struct ucred *cred, struct vnode *vp, mode_t acc_mode)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_open");

	if (!mac_enforce_fs)
		return (0);

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, active_cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, active_cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(dvp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

	MAC_CHECK(check_vnode_readlink, cred, vp, &vp->v_label);
	return (error);
}

static int
mac_check_vnode_relabel(struct ucred *cred, struct vnode *vp,
    struct label *newlabel)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_vnode_relabel");

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(dvp, cred);
	if (error)
		return (error);
	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(dvp, cred);
	if (error)
		return (error);
	if (vp != NULL) {
		error = vn_refreshlabel(vp, cred);
		if (error)
			return (error);
	}
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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, active_cred);
	if (error)
		return (error);

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

	error = vn_refreshlabel(vp, active_cred);
	if (error)
		return (error);

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
		printf("%s%d: not initialized\n", ifnet->if_name,
		    ifnet->if_unit);

	MAC_CHECK(check_ifnet_transmit, ifnet, &ifnet->if_label, mbuf,
	    &mbuf->m_pkthdr.label);

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
mac_check_socket_visible(struct ucred *cred, struct socket *socket)
{
	int error;

	if (!mac_enforce_socket)
		return (0);
                
	MAC_CHECK(check_socket_visible, cred, socket, &socket->so_label);
                            
	return (error);
}

int
mac_ioctl_ifnet_get(struct ucred *cred, struct ifreq *ifr,
    struct ifnet *ifnet)
{
	struct mac label;
	int error;

	error = mac_externalize(&ifnet->if_label, &label);
	if (error)
		return (error);

	return (copyout(&label, ifr->ifr_ifru.ifru_data, sizeof(label)));
}

int
mac_ioctl_ifnet_set(struct ucred *cred, struct ifreq *ifr,
    struct ifnet *ifnet)
{
	struct mac newlabel;
	struct label intlabel;
	int error;

	error = copyin(ifr->ifr_ifru.ifru_data, &newlabel, sizeof(newlabel));
	if (error)
		return (error);

	error = mac_internalize(&intlabel, &newlabel);
	if (error)
		return (error);

	/*
	 * XXX: Note that this is a redundant privilege check, since
	 * policies impose this check themselves if required by the
	 * policy.  Eventually, this should go away.
	 */
	error = suser_cred(cred, 0);
	if (error)
		goto out;

	MAC_CHECK(check_ifnet_relabel, cred, ifnet, &ifnet->if_label,
	    &intlabel);
	if (error)
		goto out;

	MAC_PERFORM(relabel_ifnet, cred, ifnet, &ifnet->if_label, &intlabel);

out:
	mac_destroy_temp(&intlabel);
	return (error);
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

static int
mac_stdcreatevnode_ea(struct vnode *vp)
{
	int error;

	MAC_CHECK(stdcreatevnode_ea, vp, &vp->v_label);

	return (error);
}

void
mac_create_devfs_directory(char *dirname, int dirnamelen,
    struct devfs_dirent *de)
{

	MAC_PERFORM(create_devfs_directory, dirname, dirnamelen, de,
	    &de->de_label);
}

/*
 * When a new vnode is created, this call will initialize its label.
 */
void
mac_create_vnode(struct ucred *cred, struct vnode *parent,
    struct vnode *child)
{
	int error;

	ASSERT_VOP_LOCKED(parent, "mac_create_vnode");
	ASSERT_VOP_LOCKED(child, "mac_create_vnode");

	error = vn_refreshlabel(parent, cred);
	if (error) {
		printf("mac_create_vnode: vn_refreshlabel returned %d\n",
		    error);
		printf("mac_create_vnode: using old vnode label\n");
	}

	MAC_PERFORM(create_vnode, cred, parent, &parent->v_label, child,
	    &child->v_label);
}

int
mac_setsockopt_label_set(struct ucred *cred, struct socket *so,
    struct mac *extmac)
{
	struct label intlabel;
	int error;

	error = mac_internalize(&intlabel, extmac);
	if (error)
		return (error);

	mac_check_socket_relabel(cred, so, &intlabel);
	if (error) {
		mac_destroy_temp(&intlabel);
		return (error);
	}

	mac_relabel_socket(cred, so, &intlabel);

	mac_destroy_temp(&intlabel);
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
    struct mac *extmac)
{

	return (mac_externalize(&so->so_label, extmac));
}

int
mac_getsockopt_peerlabel_get(struct ucred *cred, struct socket *so,
    struct mac *extmac)
{

	return (mac_externalize(&so->so_peerlabel, extmac));
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
	struct mac extmac;
	int error;

	ASSERT_VOP_LOCKED(vp, "vop_stdsetlabel_ea");

	/*
	 * XXX: Eventually call out to EA check/set calls here.
	 * Be particularly careful to avoid race conditions,
	 * consistency problems, and stability problems when
	 * dealing with multiple EAs.  In particular, we require
	 * the ability to write multiple EAs on the same file in
	 * a single transaction, which the current EA interface
	 * does not provide.
	 */

	error = mac_externalize(intlabel, &extmac);
	if (error)
		return (error);

	error = vn_extattr_set(vp, IO_NODELOCKED,
	    FREEBSD_MAC_EXTATTR_NAMESPACE, FREEBSD_MAC_EXTATTR_NAME,
	    sizeof(extmac), (char *)&extmac, curthread);
	if (error)
		return (error);

	mac_relabel_vnode(ap->a_cred, vp, intlabel);

	vp->v_vflag |= VV_CACHEDLABEL;

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

/*
 * MPSAFE
 */
int
__mac_get_proc(struct thread *td, struct __mac_get_proc_args *uap)
{
	struct mac extmac;
	int error;

	error = mac_externalize(&td->td_ucred->cr_label, &extmac);
	if (error == 0)
		error = copyout(&extmac, SCARG(uap, mac_p), sizeof(extmac));

	return (error);
}

/*
 * MPSAFE
 */
int
__mac_set_proc(struct thread *td, struct __mac_set_proc_args *uap)
{
	struct ucred *newcred, *oldcred;
	struct proc *p;
	struct mac extmac;
	struct label intlabel;
	int error;

	error = copyin(SCARG(uap, mac_p), &extmac, sizeof(extmac));
	if (error)
		return (error);

	error = mac_internalize(&intlabel, &extmac);
	if (error)
		return (error);

	newcred = crget();

	p = td->td_proc;
	PROC_LOCK(p);
	oldcred = p->p_ucred;

	error = mac_check_cred_relabel(oldcred, &intlabel);
	if (error) {
		PROC_UNLOCK(p);
		mac_destroy_temp(&intlabel);
		crfree(newcred);
		return (error);
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

	mtx_lock(&Giant);
	mac_cred_mmapped_drop_perms(td, newcred);
	mtx_unlock(&Giant);

	crfree(newcred);	/* Free revocation reference. */
	crfree(oldcred);
	mac_destroy_temp(&intlabel);
	return (0);
}

/*
 * MPSAFE
 */
int
__mac_get_fd(struct thread *td, struct __mac_get_fd_args *uap)
{
	struct file *fp;
	struct mac extmac;
	struct vnode *vp;
	struct pipe *pipe;
	int error;

	mtx_lock(&Giant);

	error = fget(td, SCARG(uap, fd), &fp);
	if (error)
		goto out;

	switch (fp->f_type) {
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		vp = (struct vnode *)fp->f_data;

		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		error = vn_refreshlabel(vp, td->td_ucred);
		if (error == 0)
			error = mac_externalize(&vp->v_label, &extmac);
		VOP_UNLOCK(vp, 0, td);
		break;
	case DTYPE_PIPE:
		pipe = (struct pipe *)fp->f_data;
		error = mac_externalize(pipe->pipe_label, &extmac);
		break;
	default:
		error = EINVAL;
	}

	if (error == 0)
		error = copyout(&extmac, SCARG(uap, mac_p), sizeof(extmac));

	fdrop(fp, td);

out:
	mtx_unlock(&Giant);
	return (error);
}

/*
 * MPSAFE
 */
int
__mac_get_file(struct thread *td, struct __mac_get_file_args *uap)
{
	struct nameidata nd;
	struct mac extmac;
	int error;

	mtx_lock(&Giant);
	NDINIT(&nd, LOOKUP, LOCKLEAF | FOLLOW, UIO_USERSPACE,
	    SCARG(uap, path_p), td);
	error = namei(&nd);
	if (error)
		goto out;

	error = vn_refreshlabel(nd.ni_vp, td->td_ucred);
	if (error == 0)
		error = mac_externalize(&nd.ni_vp->v_label, &extmac);
	NDFREE(&nd, 0);
	if (error)
		goto out;

	error = copyout(&extmac, SCARG(uap, mac_p), sizeof(extmac));

out:
	mtx_unlock(&Giant);
	return (error);
}

/*
 * MPSAFE
 */
int
__mac_set_fd(struct thread *td, struct __mac_set_fd_args *uap)
{
	struct file *fp;
	struct mac extmac;
	struct label intlabel;
	struct mount *mp;
	struct vnode *vp;
	struct pipe *pipe;
	int error;

	mtx_lock(&Giant);
	error = fget(td, SCARG(uap, fd), &fp);
	if (error)
		goto out1;

	error = copyin(SCARG(uap, mac_p), &extmac, sizeof(extmac));
	if (error)
		goto out2;

	error = mac_internalize(&intlabel, &extmac);
	if (error)
		goto out2;

	switch (fp->f_type) {
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		vp = (struct vnode *)fp->f_data;
		error = vn_start_write(vp, &mp, V_WAIT | PCATCH);
		if (error != 0)
			break;

		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		error = vn_setlabel(vp, &intlabel, td->td_ucred);
		VOP_UNLOCK(vp, 0, td);
		vn_finished_write(mp);
		mac_destroy_temp(&intlabel);
		break;
	case DTYPE_PIPE:
		pipe = (struct pipe *)fp->f_data;
		PIPE_LOCK(pipe);
		error = mac_pipe_label_set(td->td_ucred, pipe, &intlabel);
		PIPE_UNLOCK(pipe);
		break;
	default:
		error = EINVAL;
	}

out2:
	fdrop(fp, td);
out1:
	mtx_unlock(&Giant);
	return (error);
}

/*
 * MPSAFE
 */
int
__mac_set_file(struct thread *td, struct __mac_set_file_args *uap)
{
	struct nameidata nd;
	struct mac extmac;
	struct label intlabel;
	struct mount *mp;
	int error;

	mtx_lock(&Giant);

	error = copyin(SCARG(uap, mac_p), &extmac, sizeof(extmac));
	if (error)
		goto out;

	error = mac_internalize(&intlabel, &extmac);
	if (error)
		goto out;

	NDINIT(&nd, LOOKUP, LOCKLEAF | FOLLOW, UIO_USERSPACE,
	    SCARG(uap, path_p), td);
	error = namei(&nd);
	if (error)
		goto out2;
	error = vn_start_write(nd.ni_vp, &mp, V_WAIT | PCATCH);
	if (error)
		goto out2;

	error = vn_setlabel(nd.ni_vp, &intlabel, td->td_ucred);

	vn_finished_write(mp);
out2:
	mac_destroy_temp(&intlabel);
	NDFREE(&nd, 0);
out:
	mtx_unlock(&Giant);
	return (error);
}

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
mac_syscall(struct thread *td, struct mac_syscall_args *uap)
{

	return (ENOSYS);
}

#endif /* !MAC */
