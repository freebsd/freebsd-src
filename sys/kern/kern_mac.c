/*-
 * Copyright (c) 1999-2002 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001-2004 Networks Associates Technology, Inc.
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
 */

/*-
 * Framework for extensible kernel access control.  This file contains
 * Kernel and userland interface to the framework, policy registration
 * found in src/sys/security/mac/.  Sample policies may be found in
 * src/sys/security/mac_*.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mac.h"
#include "opt_devfs.h"

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/extattr.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mac.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
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

#include <security/mac/mac_internal.h>

#ifdef MAC

/*
 * Declare that the kernel provides MAC support, version 1.  This permits
 * modules to refuse to be loaded if the necessary support isn't present,
 * even if it's pre-boot.
 */
MODULE_VERSION(kernel_mac_support, 1);

SYSCTL_NODE(_security, OID_AUTO, mac, CTLFLAG_RW, 0,
    "TrustedBSD MAC policy controls");

#if MAC_MAX_SLOTS > 32
#error "MAC_MAX_SLOTS too large"
#endif

static unsigned int mac_max_slots = MAC_MAX_SLOTS;
static unsigned int mac_slot_offsets_free = (1 << MAC_MAX_SLOTS) - 1;
SYSCTL_UINT(_security_mac, OID_AUTO, max_slots, CTLFLAG_RD,
    &mac_max_slots, 0, "");

/*
 * Has the kernel started generating labeled objects yet?  All read/write
 * access to this variable is serialized during the boot process.  Following
 * the end of serialization, we don't update this flag; no locking.
 */
int	mac_late = 0;

/*
 * Flag to indicate whether or not we should allocate label storage for
 * new mbufs.  Since most dynamic policies we currently work with don't
 * rely on mbuf labeling, try to avoid paying the cost of mtag allocation
 * unless specifically notified of interest.  One result of this is
 * that if a dynamically loaded policy requests mbuf labels, it must
 * be able to deal with a NULL label being returned on any mbufs that
 * were already in flight when the policy was loaded.  Since the policy
 * already has to deal with uninitialized labels, this probably won't
 * be a problem.  Note: currently no locking.  Will this be a problem?
 */
#ifndef MAC_ALWAYS_LABEL_MBUF
int	mac_labelmbufs = 0;
#endif

#ifdef MAC_DEBUG
SYSCTL_NODE(_security_mac, OID_AUTO, debug, CTLFLAG_RW, 0,
    "TrustedBSD MAC debug info");
SYSCTL_NODE(_security_mac_debug, OID_AUTO, counters, CTLFLAG_RW, 0,
    "TrustedBSD MAC object counters");

static unsigned int nmactemp;
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, temp, CTLFLAG_RD,
    &nmactemp, 0, "number of temporary labels in use");
#endif

static int	mac_policy_register(struct mac_policy_conf *mpc);
static int	mac_policy_unregister(struct mac_policy_conf *mpc);

MALLOC_DEFINE(M_MACTEMP, "mactemp", "MAC temporary label storage");

/*
 * mac_static_policy_list holds a list of policy modules that are not
 * loaded while the system is "live", and cannot be unloaded.  These
 * policies can be invoked without holding the busy count.
 *
 * mac_policy_list stores the list of dynamic policies.  A busy count is
 * maintained for the list, stored in mac_policy_busy.  The busy count
 * is protected by mac_policy_mtx; the list may be modified only
 * while the busy count is 0, requiring that the lock be held to
 * prevent new references to the list from being acquired.  For almost
 * all operations, incrementing the busy count is sufficient to
 * guarantee consistency, as the list cannot be modified while the
 * busy count is elevated.  For a few special operations involving a
 * change to the list of active policies, the mtx itself must be held.
 * A condition variable, mac_policy_cv, is used to signal potential
 * exclusive consumers that they should try to acquire the lock if a
 * first attempt at exclusive access fails.
 */
#ifndef MAC_STATIC
static struct mtx mac_policy_mtx;
static struct cv mac_policy_cv;
static int mac_policy_count;
#endif
struct mac_policy_list_head mac_policy_list;
struct mac_policy_list_head mac_static_policy_list;

/*
 * We manually invoke WITNESS_WARN() to allow Witness to generate
 * warnings even if we don't end up ever triggering the wait at
 * run-time.  The consumer of the exclusive interface must not hold
 * any locks (other than potentially Giant) since we may sleep for
 * long (potentially indefinite) periods of time waiting for the
 * framework to become quiescent so that a policy list change may
 * be made.
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
	while (mac_policy_count != 0)
		cv_wait(&mac_policy_cv, &mac_policy_mtx);
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
	if (!mac_late)
		return;

	KASSERT(mac_policy_count == 0,
	    ("mac_policy_release_exclusive(): not exclusive"));
	mtx_unlock(&mac_policy_mtx);
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
	if (!mac_late)
		return (1);

	return (1);
#endif
}

void
mac_policy_list_unbusy(void)
{

#ifndef MAC_STATIC
	if (!mac_late)
		return;

	mtx_lock(&mac_policy_mtx);
	mac_policy_count--;
	KASSERT(mac_policy_count >= 0, ("MAC_POLICY_LIST_LOCK"));
	if (mac_policy_count == 0)
		cv_signal(&mac_policy_cv);
	mtx_unlock(&mac_policy_mtx);
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
 * After the policy list has changed, walk the list to update any global
 * flags.  Currently, we support only one flag, and it's conditionally
 * defined; as a result, the entire function is conditional.  Eventually,
 * the #else case might also iterate across the policies.
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

static int
mac_policy_register(struct mac_policy_conf *mpc)
{
	struct mac_policy_conf *tmpc;
	int error, slot, static_entry;

	error = 0;

	/*
	 * We don't technically need exclusive access while !mac_late,
	 * but hold it for assertion consistency.
	 */
	mac_policy_grab_exclusive();

	/*
	 * If the module can potentially be unloaded, or we're loading
	 * late, we have to stick it in the non-static list and pay
	 * an extra performance overhead.  Otherwise, we can pay a
	 * light locking cost and stick it in the static list.
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
	 * If we're loading a MAC module after the framework has
	 * initialized, it has to go into the dynamic list.  If
	 * we're loading it before we've finished initializing,
	 * it can go into the static list with weaker locker
	 * requirements.
	 */
	if (static_entry)
		LIST_INSERT_HEAD(&mac_static_policy_list, mpc, mpc_list);
	else
		LIST_INSERT_HEAD(&mac_policy_list, mpc, mpc_list);

	/* Per-policy initialization. */
	if (mpc->mpc_ops->mpo_init != NULL)
		(*(mpc->mpc_ops->mpo_init))(mpc);
	mac_policy_updateflags();

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
	 * If we fail the load, we may get a request to unload.  Check
	 * to see if we did the run-time registration, and if not,
	 * silently succeed.
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
	 * Only allow the unload to proceed if the module is unloadable
	 * by its own definition.
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

	printf("Security policy unload: %s (%s)\n", mpc->mpc_fullname,
	    mpc->mpc_name);

	return (0);
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

void
mac_init_label(struct label *label)
{

	bzero(label, sizeof(*label));
	label->l_flags = MAC_FLAG_INITIALIZED;
}

void
mac_destroy_label(struct label *label)
{

	KASSERT(label->l_flags & MAC_FLAG_INITIALIZED,
	    ("destroying uninitialized label"));

	bzero(label, sizeof(*label));
	/* implicit: label->l_flags &= ~MAC_FLAG_INITIALIZED; */
}

int
mac_check_structmac_consistent(struct mac *mac)
{

	if (mac->m_buflen < 0 ||
	    mac->m_buflen > MAC_MAX_LABEL_BUF_LEN)
		return (EINVAL);

	return (0);
}

/*
 * MPSAFE
 */
int
__mac_get_pid(struct thread *td, struct __mac_get_pid_args *uap)
{
	char *elements, *buffer;
	struct mac mac;
	struct proc *tproc;
	struct ucred *tcred;
	int error;

	error = copyin(uap->mac_p, &mac, sizeof(mac));
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
	error = mac_externalize_cred_label(tcred->cr_label, elements,
	    buffer, mac.m_buflen);
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
	error = mac_externalize_cred_label(td->td_ucred->cr_label,
	    elements, buffer, mac.m_buflen);
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
	struct label *intlabel;
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

	intlabel = mac_cred_label_alloc();
	error = mac_internalize_cred_label(intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error)
		goto out;

	newcred = crget();

	p = td->td_proc;
	PROC_LOCK(p);
	oldcred = p->p_ucred;

	error = mac_check_cred_relabel(oldcred, intlabel);
	if (error) {
		PROC_UNLOCK(p);
		crfree(newcred);
		goto out;
	}

	setsugid(p);
	crcopy(newcred, oldcred);
	mac_relabel_cred(newcred, intlabel);
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
	mac_cred_label_free(intlabel);
	return (error);
}

/*
 * MPSAFE
 */
int
__mac_get_fd(struct thread *td, struct __mac_get_fd_args *uap)
{
	char *elements, *buffer;
	struct label *intlabel;
	struct file *fp;
	struct mac mac;
	struct vnode *vp;
	struct pipe *pipe;
	struct socket *so;
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
	error = fget(td, uap->fd, &fp);
	if (error)
		goto out;

	label_type = fp->f_type;
	switch (fp->f_type) {
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		vp = fp->f_vnode;
		intlabel = mac_vnode_label_alloc();
		mtx_lock(&Giant);				/* VFS */
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		mac_copy_vnode_label(vp->v_label, intlabel);
		VOP_UNLOCK(vp, 0, td);
		mtx_unlock(&Giant);				/* VFS */
		error = mac_externalize_vnode_label(intlabel, elements,
		    buffer, mac.m_buflen);
		mac_vnode_label_free(intlabel);
		break;

	case DTYPE_PIPE:
		pipe = fp->f_data;
		intlabel = mac_pipe_label_alloc();
		PIPE_LOCK(pipe);
		mac_copy_pipe_label(pipe->pipe_pair->pp_label, intlabel);
		PIPE_UNLOCK(pipe);
		error = mac_externalize_pipe_label(intlabel, elements,
		    buffer, mac.m_buflen);
		mac_pipe_label_free(intlabel);
		break;

	case DTYPE_SOCKET:
		so = fp->f_data;
		intlabel = mac_socket_label_alloc(M_WAITOK);
		mtx_lock(&Giant);				/* Sockets */
		/* XXX: Socket lock here. */
		mac_copy_socket_label(so->so_label, intlabel);
		/* XXX: Socket unlock here. */
		mtx_unlock(&Giant);				/* Sockets */
		error = mac_externalize_socket_label(intlabel, elements,
		    buffer, mac.m_buflen);
		mac_socket_label_free(intlabel);
		break;

	default:
		error = EINVAL;
	}
	fdrop(fp, td);
	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

out:
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
	struct label *intlabel;
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

	intlabel = mac_vnode_label_alloc();
	mac_copy_vnode_label(nd.ni_vp->v_label, intlabel);
	error = mac_externalize_vnode_label(intlabel, elements, buffer,
	    mac.m_buflen);

	NDFREE(&nd, 0);
	mac_vnode_label_free(intlabel);

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
	struct label *intlabel;
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

	intlabel = mac_vnode_label_alloc();
	mac_copy_vnode_label(nd.ni_vp->v_label, intlabel);
	error = mac_externalize_vnode_label(intlabel, elements, buffer,
	    mac.m_buflen);
	NDFREE(&nd, 0);
	mac_vnode_label_free(intlabel);

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
	struct label *intlabel;
	struct pipe *pipe;
	struct socket *so;
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

	error = fget(td, uap->fd, &fp);
	if (error)
		goto out;

	switch (fp->f_type) {
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		intlabel = mac_vnode_label_alloc();
		error = mac_internalize_vnode_label(intlabel, buffer);
		if (error) {
			mac_vnode_label_free(intlabel);
			break;
		}
		vp = fp->f_vnode;
		mtx_lock(&Giant);				/* VFS */
		error = vn_start_write(vp, &mp, V_WAIT | PCATCH);
		if (error != 0) {
			mtx_unlock(&Giant);			/* VFS */
			mac_vnode_label_free(intlabel);
			break;
		}
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		error = vn_setlabel(vp, intlabel, td->td_ucred);
		VOP_UNLOCK(vp, 0, td);
		vn_finished_write(mp);
		mtx_unlock(&Giant);				/* VFS */
		mac_vnode_label_free(intlabel);
		break;

	case DTYPE_PIPE:
		intlabel = mac_pipe_label_alloc();
		error = mac_internalize_pipe_label(intlabel, buffer);
		if (error == 0) {
			pipe = fp->f_data;
			PIPE_LOCK(pipe);
			error = mac_pipe_label_set(td->td_ucred,
			    pipe->pipe_pair, intlabel);
			PIPE_UNLOCK(pipe);
		}
		mac_pipe_label_free(intlabel);
		break;

	case DTYPE_SOCKET:
		intlabel = mac_socket_label_alloc(M_WAITOK);
		error = mac_internalize_socket_label(intlabel, buffer);
		if (error == 0) {
			so = fp->f_data;
			mtx_lock(&Giant);			/* Sockets */
			/* XXX: Socket lock here. */
			error = mac_socket_label_set(td->td_ucred, so,
			    intlabel);
			/* XXX: Socket unlock here. */
			mtx_unlock(&Giant);			/* Sockets */
		}
		mac_socket_label_free(intlabel);
		break;

	default:
		error = EINVAL;
	}
	fdrop(fp, td);
out:
	free(buffer, M_MACTEMP);
	return (error);
}

/*
 * MPSAFE
 */
int
__mac_set_file(struct thread *td, struct __mac_set_file_args *uap)
{
	struct label *intlabel;
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

	intlabel = mac_vnode_label_alloc();
	error = mac_internalize_vnode_label(intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error)
		goto out;

	mtx_lock(&Giant);				/* VFS */

	NDINIT(&nd, LOOKUP, LOCKLEAF | FOLLOW, UIO_USERSPACE, uap->path_p,
	    td);
	error = namei(&nd);
	if (error == 0) {
		error = vn_start_write(nd.ni_vp, &mp, V_WAIT | PCATCH);
		if (error == 0)
			error = vn_setlabel(nd.ni_vp, intlabel,
			    td->td_ucred);
		vn_finished_write(mp);
	}

	NDFREE(&nd, 0);
	mtx_unlock(&Giant);				/* VFS */
out:
	mac_vnode_label_free(intlabel);
	return (error);
}

/*
 * MPSAFE
 */
int
__mac_set_link(struct thread *td, struct __mac_set_link_args *uap)
{
	struct label *intlabel;
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

	intlabel = mac_vnode_label_alloc();
	error = mac_internalize_vnode_label(intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error)
		goto out;

	mtx_lock(&Giant);				/* VFS */

	NDINIT(&nd, LOOKUP, LOCKLEAF | NOFOLLOW, UIO_USERSPACE, uap->path_p,
	    td);
	error = namei(&nd);
	if (error == 0) {
		error = vn_start_write(nd.ni_vp, &mp, V_WAIT | PCATCH);
		if (error == 0)
			error = vn_setlabel(nd.ni_vp, intlabel,
			    td->td_ucred);
		vn_finished_write(mp);
	}

	NDFREE(&nd, 0);
	mtx_unlock(&Giant);				/* VFS */
out:
	mac_vnode_label_free(intlabel);
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
	int entrycount, error;

	error = copyinstr(uap->policy, target, sizeof(target), NULL);
	if (error)
		return (error);

	error = ENOSYS;
	LIST_FOREACH(mpc, &mac_static_policy_list, mpc_list) {
		if (strcmp(mpc->mpc_name, target) == 0 &&
		    mpc->mpc_ops->mpo_syscall != NULL) {
			error = mpc->mpc_ops->mpo_syscall(td,
			    uap->call, uap->arg);
			goto out;
		}
	}

	if ((entrycount = mac_policy_list_conditional_busy()) != 0) {
		LIST_FOREACH(mpc, &mac_policy_list, mpc_list) {
			if (strcmp(mpc->mpc_name, target) == 0 &&
			    mpc->mpc_ops->mpo_syscall != NULL) {
				error = mpc->mpc_ops->mpo_syscall(td,
				    uap->call, uap->arg);
				break;
			}
		}
		mac_policy_list_unbusy();
	}
out:
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

#endif /* !MAC */
