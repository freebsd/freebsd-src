/*-
 * Copyright (c) 2015 Nuxi, https://nuxi.nl/
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

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/sysproto.h>

#include <contrib/cloudabi/cloudabi_types_common.h>

#include <compat/cloudabi/cloudabi_proto.h>

/* Converts CloudABI's memory protection flags to FreeBSD's. */
static int
convert_mprot(cloudabi_mprot_t in, int *out)
{

	/* Unknown protection flags. */
	if ((in & ~(CLOUDABI_PROT_EXEC | CLOUDABI_PROT_WRITE |
	    CLOUDABI_PROT_READ)) != 0)
		return (ENOTSUP);
	/* W^X: Write and exec cannot be enabled at the same time. */
	if ((in & (CLOUDABI_PROT_EXEC | CLOUDABI_PROT_WRITE)) ==
	    (CLOUDABI_PROT_EXEC | CLOUDABI_PROT_WRITE))
		return (ENOTSUP);

	*out = 0;
	if (in & CLOUDABI_PROT_EXEC)
		*out |= PROT_EXEC;
	if (in & CLOUDABI_PROT_WRITE)
		*out |= PROT_WRITE;
	if (in & CLOUDABI_PROT_READ)
		*out |= PROT_READ;
	return (0);
}

int
cloudabi_sys_mem_advise(struct thread *td,
    struct cloudabi_sys_mem_advise_args *uap)
{
	struct madvise_args madvise_args = {
		.addr	= uap->addr,
		.len	= uap->len
	};

	switch (uap->advice) {
	case CLOUDABI_ADVICE_DONTNEED:
		madvise_args.behav = MADV_DONTNEED;
		break;
	case CLOUDABI_ADVICE_NORMAL:
		madvise_args.behav = MADV_NORMAL;
		break;
	case CLOUDABI_ADVICE_RANDOM:
		madvise_args.behav = MADV_RANDOM;
		break;
	case CLOUDABI_ADVICE_SEQUENTIAL:
		madvise_args.behav = MADV_SEQUENTIAL;
		break;
	case CLOUDABI_ADVICE_WILLNEED:
		madvise_args.behav = MADV_WILLNEED;
		break;
	default:
		return (EINVAL);
	}

	return (sys_madvise(td, &madvise_args));
}

int
cloudabi_sys_mem_lock(struct thread *td, struct cloudabi_sys_mem_lock_args *uap)
{
	struct mlock_args mlock_args = {
		.addr	= uap->addr,
		.len	= uap->len
	};

	return (sys_mlock(td, &mlock_args));
}

int
cloudabi_sys_mem_map(struct thread *td, struct cloudabi_sys_mem_map_args *uap)
{
	struct mmap_args mmap_args = {
		.addr	= uap->addr,
		.len	= uap->len,
		.fd	= uap->fd,
		.pos	= uap->off
	};
	int error;

	/* Translate flags. */
	if (uap->flags & CLOUDABI_MAP_ANON)
		mmap_args.flags |= MAP_ANON;
	if (uap->flags & CLOUDABI_MAP_FIXED)
		mmap_args.flags |= MAP_FIXED;
	if (uap->flags & CLOUDABI_MAP_PRIVATE)
		mmap_args.flags |= MAP_PRIVATE;
	if (uap->flags & CLOUDABI_MAP_SHARED)
		mmap_args.flags |= MAP_SHARED;

	/* Translate protection. */
	error = convert_mprot(uap->prot, &mmap_args.prot);
	if (error != 0)
		return (error);

	return (sys_mmap(td, &mmap_args));
}

int
cloudabi_sys_mem_protect(struct thread *td,
    struct cloudabi_sys_mem_protect_args *uap)
{
	struct mprotect_args mprotect_args = {
		.addr	= uap->addr,
		.len	= uap->len,
	};
	int error;

	/* Translate protection. */
	error = convert_mprot(uap->prot, &mprotect_args.prot);
	if (error != 0)
		return (error);

	return (sys_mprotect(td, &mprotect_args));
}

int
cloudabi_sys_mem_sync(struct thread *td, struct cloudabi_sys_mem_sync_args *uap)
{
	struct msync_args msync_args = {
		.addr	= uap->addr,
		.len	= uap->len,
	};

	/* Convert flags. */
	switch (uap->flags & (CLOUDABI_MS_ASYNC | CLOUDABI_MS_SYNC)) {
	case CLOUDABI_MS_ASYNC:
		msync_args.flags |= MS_ASYNC;
		break;
	case CLOUDABI_MS_SYNC:
		msync_args.flags |= MS_SYNC;
		break;
	default:
		return (EINVAL);
	}
	if ((uap->flags & CLOUDABI_MS_INVALIDATE) != 0)
		msync_args.flags |= MS_INVALIDATE;

	return (sys_msync(td, &msync_args));
}

int
cloudabi_sys_mem_unlock(struct thread *td,
    struct cloudabi_sys_mem_unlock_args *uap)
{
	struct munlock_args munlock_args = {
		.addr	= uap->addr,
		.len	= uap->len
	};

	return (sys_munlock(td, &munlock_args));
}

int
cloudabi_sys_mem_unmap(struct thread *td,
    struct cloudabi_sys_mem_unmap_args *uap)
{
	struct munmap_args munmap_args = {
		.addr	= uap->addr,
		.len	= uap->len
	};

	return (sys_munmap(td, &munmap_args));
}
