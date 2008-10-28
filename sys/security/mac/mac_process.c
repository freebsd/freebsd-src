/*-
 * Copyright (c) 1999-2002, 2008 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001-2003 Networks Associates Technology, Inc.
 * Copyright (c) 2005 Samy Al Bahra
 * Copyright (c) 2006 SPARTA, Inc.
 * Copyright (c) 2008 Apple Inc.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mac.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>

#include <security/mac/mac_framework.h>
#include <security/mac/mac_internal.h>
#include <security/mac/mac_policy.h>

static int	mac_mmap_revocation = 1;
SYSCTL_INT(_security_mac, OID_AUTO, mmap_revocation, CTLFLAG_RW,
    &mac_mmap_revocation, 0, "Revoke mmap access to files on subject "
    "relabel");

static int	mac_mmap_revocation_via_cow = 0;
SYSCTL_INT(_security_mac, OID_AUTO, mmap_revocation_via_cow, CTLFLAG_RW,
    &mac_mmap_revocation_via_cow, 0, "Revoke mmap access to files via "
    "copy-on-write semantics, or by removing all write access");

static void	mac_cred_mmapped_drop_perms_recurse(struct thread *td,
		    struct ucred *cred, struct vm_map *map);

struct label *
mac_cred_label_alloc(void)
{
	struct label *label;

	label = mac_labelzone_alloc(M_WAITOK);
	MAC_PERFORM(cred_init_label, label);
	return (label);
}

void
mac_cred_init(struct ucred *cred)
{

	if (mac_labeled & MPC_OBJECT_CRED)
		cred->cr_label = mac_cred_label_alloc();
	else
		cred->cr_label = NULL;
}

static struct label *
mac_proc_label_alloc(void)
{
	struct label *label;

	label = mac_labelzone_alloc(M_WAITOK);
	MAC_PERFORM(proc_init_label, label);
	return (label);
}

void
mac_proc_init(struct proc *p)
{

	if (mac_labeled & MPC_OBJECT_PROC)
		p->p_label = mac_proc_label_alloc();
	else
		p->p_label = NULL;
}

void
mac_cred_label_free(struct label *label)
{

	MAC_PERFORM(cred_destroy_label, label);
	mac_labelzone_free(label);
}

void
mac_cred_destroy(struct ucred *cred)
{

	if (cred->cr_label != NULL) {
		mac_cred_label_free(cred->cr_label);
		cred->cr_label = NULL;
	}
}

static void
mac_proc_label_free(struct label *label)
{

	MAC_PERFORM(proc_destroy_label, label);
	mac_labelzone_free(label);
}

void
mac_proc_destroy(struct proc *p)
{

	if (p->p_label != NULL) {
		mac_proc_label_free(p->p_label);
		p->p_label = NULL;
	}
}

/*
 * When a thread becomes an NFS server daemon, its credential may need to be
 * updated to reflect this so that policies can recognize when file system
 * operations originate from the network.
 *
 * At some point, it would be desirable if the credential used for each NFS
 * RPC could be set based on the RPC context (i.e., source system, etc) to
 * provide more fine-grained access control.
 */
void
mac_cred_associate_nfsd(struct ucred *cred)
{

	MAC_PERFORM(cred_associate_nfsd, cred);
}

/*
 * Initialize MAC label for the first kernel process, from which other kernel
 * processes and threads are spawned.
 */
void
mac_cred_create_swapper(struct ucred *cred)
{

	MAC_PERFORM(cred_create_swapper, cred);
}

/*
 * Initialize MAC label for the first userland process, from which other
 * userland processes and threads are spawned.
 */
void
mac_cred_create_init(struct ucred *cred)
{

	MAC_PERFORM(cred_create_init, cred);
}

int
mac_cred_externalize_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen)
{
	int error;

	MAC_EXTERNALIZE(cred, label, elements, outbuf, outbuflen);

	return (error);
}

int
mac_cred_internalize_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(cred, label, string);

	return (error);
}

void
mac_thread_userret(struct thread *td)
{

	MAC_PERFORM(thread_userret, td);
}

/*
 * When a new process is created, its label must be initialized.  Generally,
 * this involves inheritence from the parent process, modulo possible deltas.
 * This function allows that processing to take place.
 */
void
mac_cred_copy(struct ucred *src, struct ucred *dest)
{

	MAC_PERFORM(cred_copy_label, src->cr_label, dest->cr_label);
}

int
mac_execve_enter(struct image_params *imgp, struct mac *mac_p)
{
	struct label *label;
	struct mac mac;
	char *buffer;
	int error;

	if (mac_p == NULL)
		return (0);

	if (!(mac_labeled & MPC_OBJECT_CRED))
		return (EINVAL);

	error = copyin(mac_p, &mac, sizeof(mac));
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

	label = mac_cred_label_alloc();
	error = mac_cred_internalize_label(label, buffer);
	free(buffer, M_MACTEMP);
	if (error) {
		mac_cred_label_free(label);
		return (error);
	}
	imgp->execlabel = label;
	return (0);
}

void
mac_execve_exit(struct image_params *imgp)
{
	if (imgp->execlabel != NULL) {
		mac_cred_label_free(imgp->execlabel);
		imgp->execlabel = NULL;
	}
}

void
mac_execve_interpreter_enter(struct vnode *interpvp,
    struct label **interpvplabel)
{

	if (mac_labeled & MPC_OBJECT_VNODE) {
		*interpvplabel = mac_vnode_label_alloc();
		mac_vnode_copy_label(interpvp->v_label, *interpvplabel);
	} else
		*interpvplabel = NULL;
}

void
mac_execve_interpreter_exit(struct label *interpvplabel)
{

	if (interpvplabel != NULL)
		mac_vnode_label_free(interpvplabel);
}

/*
 * When relabeling a process, call out to the policies for the maximum
 * permission allowed for each object type we know about in its memory space,
 * and revoke access (in the least surprising ways we know) when necessary.
 * The process lock is not held here.
 */
void
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
	int vfslocked, result;
	vm_prot_t revokeperms;
	vm_object_t backing_object, object;
	vm_ooffset_t offset;
	struct vnode *vp;
	struct mount *mp;

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
		VM_OBJECT_LOCK(object);
		while ((backing_object = object->backing_object) != NULL) {
			VM_OBJECT_LOCK(backing_object);
			offset += object->backing_object_offset;
			VM_OBJECT_UNLOCK(object);
			object = backing_object;
		}
		VM_OBJECT_UNLOCK(object);
		/*
		 * At the moment, vm_maps and objects aren't considered by
		 * the MAC system, so only things with backing by a normal
		 * object (read: vnodes) are checked.
		 */
		if (object->type != OBJT_VNODE)
			continue;
		vp = (struct vnode *)object->handle;
		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		result = vme->max_protection;
		mac_vnode_check_mmap_downgrade(cred, vp, &result);
		VOP_UNLOCK(vp, 0);
		/*
		 * Find out what maximum protection we may be allowing now
		 * but a policy needs to get removed.
		 */
		revokeperms = vme->max_protection & ~result;
		if (!revokeperms) {
			VFS_UNLOCK_GIANT(vfslocked);
			continue;
		}
		printf("pid %ld: revoking %s perms from %#lx:%ld "
		    "(max %s/cur %s)\n", (long)td->td_proc->p_pid,
		    prot2str(revokeperms), (u_long)vme->start,
		    (long)(vme->end - vme->start),
		    prot2str(vme->max_protection), prot2str(vme->protection));
		vm_map_lock_upgrade(map);
		/*
		 * This is the really simple case: if a map has more
		 * max_protection than is allowed, but it's not being
		 * actually used (that is, the current protection is still
		 * allowed), we can just wipe it out and do nothing more.
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
				(void) vn_start_write(vp, &mp, V_WAIT);
				vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
				VM_OBJECT_LOCK(object);
				vm_object_page_clean(object,
				    OFF_TO_IDX(offset),
				    OFF_TO_IDX(offset + vme->end - vme->start +
					PAGE_MASK),
				    OBJPC_SYNC);
				VM_OBJECT_UNLOCK(object);
				VOP_UNLOCK(vp, 0);
				vn_finished_write(mp);
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
		VFS_UNLOCK_GIANT(vfslocked);
	}
	vm_map_unlock_read(map);
}

/*
 * When the subject's label changes, it may require revocation of privilege
 * to mapped objects.  This can't be done on-the-fly later with a unified
 * buffer cache.
 */
void
mac_cred_relabel(struct ucred *cred, struct label *newlabel)
{

	MAC_PERFORM(cred_relabel, cred, newlabel);
}

int
mac_cred_check_relabel(struct ucred *cred, struct label *newlabel)
{
	int error;

	MAC_CHECK(cred_check_relabel, cred, newlabel);

	return (error);
}

int
mac_cred_check_visible(struct ucred *cr1, struct ucred *cr2)
{
	int error;

	MAC_CHECK(cred_check_visible, cr1, cr2);

	return (error);
}

int
mac_proc_check_debug(struct ucred *cred, struct proc *p)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_debug, cred, p);

	return (error);
}

int
mac_proc_check_sched(struct ucred *cred, struct proc *p)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_sched, cred, p);

	return (error);
}

int
mac_proc_check_signal(struct ucred *cred, struct proc *p, int signum)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_signal, cred, p, signum);

	return (error);
}

int
mac_proc_check_setuid(struct proc *p, struct ucred *cred, uid_t uid)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_setuid, cred, uid);
	return (error);
}

int
mac_proc_check_seteuid(struct proc *p, struct ucred *cred, uid_t euid)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_seteuid, cred, euid);
	return (error);
}

int
mac_proc_check_setgid(struct proc *p, struct ucred *cred, gid_t gid)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_setgid, cred, gid);

	return (error);
}

int
mac_proc_check_setegid(struct proc *p, struct ucred *cred, gid_t egid)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_setegid, cred, egid);

	return (error);
}

int
mac_proc_check_setgroups(struct proc *p, struct ucred *cred, int ngroups,
    gid_t *gidset)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_setgroups, cred, ngroups, gidset);
	return (error);
}

int
mac_proc_check_setreuid(struct proc *p, struct ucred *cred, uid_t ruid,
    uid_t euid)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_setreuid, cred, ruid, euid);

	return (error);
}

int
mac_proc_check_setregid(struct proc *proc, struct ucred *cred, gid_t rgid,
    gid_t egid)
{
	int error;

	PROC_LOCK_ASSERT(proc, MA_OWNED);

	MAC_CHECK(proc_check_setregid, cred, rgid, egid);

	return (error);
}

int
mac_proc_check_setresuid(struct proc *p, struct ucred *cred, uid_t ruid,
    uid_t euid, uid_t suid)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_setresuid, cred, ruid, euid, suid);
	return (error);
}

int
mac_proc_check_setresgid(struct proc *p, struct ucred *cred, gid_t rgid,
    gid_t egid, gid_t sgid)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_setresgid, cred, rgid, egid, sgid);

	return (error);
}

int
mac_proc_check_wait(struct ucred *cred, struct proc *p)
{
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	MAC_CHECK(proc_check_wait, cred, p);

	return (error);
}
