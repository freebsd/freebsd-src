/*-
 * Copyright (c) 2016 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

static inline int
CHERIABI_SYS_cheriabi_ioctl_fill_uap(struct thread *td,
    struct cheriabi_ioctl_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [0] int fd */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi_ioctl, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->fd, CHERI_CR_CTEMP0);

	/* [1] u_long com */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi_ioctl, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->com, CHERI_CR_CTEMP0);

	/* [2] _Inout_opt_ caddr_t data */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi_ioctl, 2);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		CHERI_CTOINT(uap->data, CHERI_CR_CTEMP0);
		if (uap->data != NULL)
			return (EPROT);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL|CHERI_PERM_LOAD|CHERI_PERM_STORE);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		if (offset => length)
			return (EPROT);
		length -= offset;
		if (length < sizeof(*uap->data))
			return (EPROT);

		CHERI_CTOPTR(uap->data, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

static inline int
CHERIABI_SYS_mincore_fill_uap(struct thread *td,
    struct mincore_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [1] size_t len */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_mincore, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->len, CHERI_CR_CTEMP0);

	/* [0] _In_pagerange_(len) const void * addr */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_mincore, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL|CHERI_PERM_LOAD);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		CHERI_CGETLEN(base, CHERI_CR_CTEMP0);
		if (rounddown2(base + offset, PAGE_SIZE) < base)
			return (EINVAL);
			size_t adjust;
		adjust = ((base + offset) & PAGE_MASK);
		length += adjust;
		if (length < roundup2(uap->len + adjust, PAGE_SIZE))
			return (EINVAL);

		CHERI_CTOPTR(uap->addr, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	/* [2] _Out_writes_bytes_(len/PAGE_SIZE) char * vec */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_mincore, 2);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL|CHERI_PERM_STORE);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->vec))
			return (EINVAL);

		CHERI_CTOPTR(uap->vec, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

static inline int
CHERIABI_SYS_fcntl_fill_uap(struct thread *td,
    struct fcntl_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [0] int fd */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_fcntl, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->fd, CHERI_CR_CTEMP0);

	/* [1] int cmd */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_fcntl, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->cmd, CHERI_CR_CTEMP0);

	/* [2] intptr_t arg */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_fcntl, 2);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->arg))
			return (EINVAL);

		CHERI_CTOPTR(uap->arg, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

static inline int
CHERIABI_SYS_cheriabi_sysarch_fill_uap(struct thread *td,
    struct cheriabi_sysarch_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [0] int op */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi_sysarch, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->op, CHERI_CR_CTEMP0);

	/* [1] char * parms */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi_sysarch, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->parms))
			return (EINVAL);

		CHERI_CTOPTR(uap->parms, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

static inline int
CHERIABI_SYS_cheriabi_shmat_fill_uap(struct thread *td,
    struct cheriabi_shmat_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [0] int shmid */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_shmat, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->shmid, CHERI_CR_CTEMP0);

	/* [2] int shmflg */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_shmat, 2);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->shmflg, CHERI_CR_CTEMP0);

	/* [1] _In_pagerange_opt_(1) void * shmaddr */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_shmat, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		CHERI_CTOINT(uap->shmaddr, CHERI_CR_CTEMP0);
		if (uap->shmaddr != NULL)
			return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL|CHERI_PERM_LOAD);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		CHERI_CGETLEN(base, CHERI_CR_CTEMP0);
		if (rounddown2(base + offset, PAGE_SIZE) < base)
			return (EINVAL);
			size_t adjust;
		adjust = ((base + offset) & PAGE_MASK);
		length += adjust;
		if (length < roundup2(1 + adjust, PAGE_SIZE))
			return (EINVAL);

		CHERI_CTOPTR(uap->shmaddr, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

static inline int
CHERIABI_SYS_cheriabi_shmdt_fill_uap(struct thread *td,
    struct shmdt_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [0] _In_pagerange_(1) void * shmaddr */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_shmdt, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL|CHERI_PERM_LOAD);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		CHERI_CGETLEN(base, CHERI_CR_CTEMP0);
		if (rounddown2(base + offset, PAGE_SIZE) < base)
			return (EINVAL);
			size_t adjust;
		adjust = ((base + offset) & PAGE_MASK);
		length += adjust;
		if (length < roundup2(1 + adjust, PAGE_SIZE))
			return (EINVAL);

		CHERI_CTOPTR(uap->shmaddr, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

static inline int
CHERIABI_SYS_mac_syscall_fill_uap(struct thread *td,
    struct mac_syscall_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [1] int call */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_mac_syscall, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->call, CHERI_CR_CTEMP0);

	/* [0] _In_z_ const char * policy */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_mac_syscall, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL|CHERI_PERM_LOAD);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->policy))
			return (EINVAL);

		CHERI_CTOPTR(uap->policy, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	/* [2] _In_opt_ void * arg */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_mac_syscall, 2);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		CHERI_CTOINT(uap->arg, CHERI_CR_CTEMP0);
		if (uap->arg != NULL)
			return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL|CHERI_PERM_LOAD);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->arg))
			return (EINVAL);

		CHERI_CTOPTR(uap->arg, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

static inline int
CHERIABI_SYS_auditon_fill_uap(struct thread *td,
    struct auditon_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [0] int cmd */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_auditon, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->cmd, CHERI_CR_CTEMP0);

	/* [2] u_int length */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_auditon, 2);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->length, CHERI_CR_CTEMP0);

	/* [1] _In_opt_ void * data */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_auditon, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		CHERI_CTOINT(uap->data, CHERI_CR_CTEMP0);
		if (uap->data != NULL)
			return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL|CHERI_PERM_LOAD);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->data))
			return (EINVAL);

		CHERI_CTOPTR(uap->data, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

static inline int
CHERIABI_SYS__umtx_op_fill_uap(struct thread *td,
    struct _umtx_op_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [1] int op */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS__umtx_op, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->op, CHERI_CR_CTEMP0);

	/* [2] u_long val */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS__umtx_op, 2);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->val, CHERI_CR_CTEMP0);

	/* [0] void * obj */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS__umtx_op, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->obj))
			return (EINVAL);

		CHERI_CTOPTR(uap->obj, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	/* [3] void * uaddr */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS__umtx_op, 3);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->uaddr))
			return (EINVAL);

		CHERI_CTOPTR(uap->uaddr, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	/* [4] void * uaddr2 */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS__umtx_op, 4);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->uaddr2))
			return (EINVAL);

		CHERI_CTOPTR(uap->uaddr2, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

static inline int
CHERIABI_SYS_sigqueue_fill_uap(struct thread *td,
    struct sigqueue_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [0] pid_t pid */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_sigqueue, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->pid, CHERI_CR_CTEMP0);

	/* [1] int signum */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_sigqueue, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->signum, CHERI_CR_CTEMP0);

	/* [2] void * value */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_sigqueue, 2);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->value))
			return (EINVAL);

		CHERI_CTOPTR(uap->value, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

static inline int
CHERIABI_SYS_cheriabi___semctl_fill_uap(struct thread *td,
    struct cheriabi___semctl_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [0] int semid */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi___semctl, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->semid, CHERI_CR_CTEMP0);

	/* [1] int semnum */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi___semctl, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->semnum, CHERI_CR_CTEMP0);

	/* [2] int cmd */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi___semctl, 2);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->cmd, CHERI_CR_CTEMP0);

	/* [3] union semun_c * arg */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi___semctl, 3);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->arg))
			return (EINVAL);

		CHERI_CTOPTR(uap->arg, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

CHERIABI_SYS_cheriabi_msgctl_fill_uap(struct thread *td,
    struct cheriabi_msgctl_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [0] int msqid */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi_msgctl, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->msqid, CHERI_CR_CTEMP0);

	/* [1] int cmd */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi_msgctl, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->cmd, CHERI_CR_CTEMP0);

	/* [2] struct msqid_ds_c * buf */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_cheriabi_msgctl, 2);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->buf))
			return (EINVAL);

		CHERI_CTOPTR(uap->buf, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}

static inline int
CHERIABI_SYS_procctl_fill_uap(struct thread *td,
    struct procctl_args *uap)
{
	struct chericap tmpcap;
	u_int tag;
	register_t perms, reqperms;
	register_t sealed;
	size_t base __unused, length, offset;

	/* [0] int idtype */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_procctl, 0);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->idtype, CHERI_CR_CTEMP0);

	/* [1] id_t id */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_procctl, 1);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->id, CHERI_CR_CTEMP0);

	/* [2] int com */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_procctl, 2);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CTOINT(uap->com, CHERI_CR_CTEMP0);

	/* [3] void * data */
	cheriabi_fetch_syscall_arg(td, &tmpcap, CHERIABI_SYS_procctl, 3);
	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &tmpcap, 0);
	CHERI_CGETTAG(tag, CHERI_CR_CTEMP0);
	if (!tag) {
		return (EINVAL);
	} else {
		CHERI_CGETPERM(perms, CHERI_CR_CTEMP0);
		reqperms = (CHERI_PERM_GLOBAL);
		if ((perms & reqperms) != reqperms)
			return (EPROT);

		CHERI_CGETSEALED(sealed, CHERI_CR_CTEMP0);
		if (sealed)
			return (EPROT);

		CHERI_CGETLEN(length, CHERI_CR_CTEMP0);
		CHERI_CGETLEN(offset, CHERI_CR_CTEMP0);
		length -= offset;
		if (length < sizeof(*uap->data))
			return (EINVAL);

		CHERI_CTOPTR(uap->data, CHERI_CR_CTEMP0, CHERI_CR_KDC);
	}

	return (0);
}
