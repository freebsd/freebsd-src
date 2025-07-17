/*-
 * Copyright (c) 2004 Tim J. Robbins
 * Copyright (c) 2002 Doug Rabson
 * Copyright (c) 2000 Marcel Moolenaar
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>

#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>

#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_mmap.h>
#include <compat/linux/linux_persona.h>
#include <compat/linux/linux_util.h>

#define STACK_SIZE  (2 * 1024 * 1024)
#define GUARD_SIZE  (4 * PAGE_SIZE)

#if defined(__amd64__)
static void linux_fixup_prot(struct thread *td, int *prot);
#endif

static int
linux_mmap_check_fp(struct file *fp, int flags, int prot, int maxprot)
{

	/* Linux mmap() just fails for O_WRONLY files */
	if ((fp->f_flag & FREAD) == 0)
		return (EACCES);

	return (0);
}

int
linux_mmap_common(struct thread *td, uintptr_t addr, size_t len, int prot,
    int flags, int fd, off_t pos)
{
	struct mmap_req mr, mr_fixed;
	struct proc *p = td->td_proc;
	struct vmspace *vms = td->td_proc->p_vmspace;
	int bsd_flags, error;

	LINUX_CTR6(mmap2, "0x%lx, %ld, %ld, 0x%08lx, %ld, 0x%lx",
	    addr, len, prot, flags, fd, pos);

	error = 0;
	bsd_flags = 0;

	/*
	 * Linux mmap(2):
	 * You must specify exactly one of MAP_SHARED and MAP_PRIVATE
	 */
	if (!((flags & LINUX_MAP_SHARED) ^ (flags & LINUX_MAP_PRIVATE)))
		return (EINVAL);

	if (flags & LINUX_MAP_SHARED)
		bsd_flags |= MAP_SHARED;
	if (flags & LINUX_MAP_PRIVATE)
		bsd_flags |= MAP_PRIVATE;
	if (flags & LINUX_MAP_FIXED)
		bsd_flags |= MAP_FIXED;
	if (flags & LINUX_MAP_ANON) {
		/* Enforce pos to be on page boundary, then ignore. */
		if ((pos & PAGE_MASK) != 0)
			return (EINVAL);
		pos = 0;
		bsd_flags |= MAP_ANON;
	} else
		bsd_flags |= MAP_NOSYNC;
	if (flags & LINUX_MAP_GROWSDOWN)
		bsd_flags |= MAP_STACK;

#if defined(__amd64__)
	/*
	 * According to the Linux mmap(2) man page, "MAP_32BIT flag
	 * is ignored when MAP_FIXED is set."
	 */
	if ((flags & LINUX_MAP_32BIT) && (flags & LINUX_MAP_FIXED) == 0)
		bsd_flags |= MAP_32BIT;

	/*
	 * PROT_READ, PROT_WRITE, or PROT_EXEC implies PROT_READ and PROT_EXEC
	 * on Linux/i386 if the binary requires executable stack.
	 * We do this only for IA32 emulation as on native i386 this is does not
	 * make sense without PAE.
	 *
	 * XXX. Linux checks that the file system is not mounted with noexec.
	 */
	linux_fixup_prot(td, &prot);
#endif

	/* Linux does not check file descriptor when MAP_ANONYMOUS is set. */
	fd = (bsd_flags & MAP_ANON) ? -1 : fd;
	if (flags & LINUX_MAP_GROWSDOWN) {
		/*
		 * The Linux MAP_GROWSDOWN option does not limit auto
		 * growth of the region.  Linux mmap with this option
		 * takes as addr the initial BOS, and as len, the initial
		 * region size.  It can then grow down from addr without
		 * limit.  However, Linux threads has an implicit internal
		 * limit to stack size of STACK_SIZE.  Its just not
		 * enforced explicitly in Linux.  But, here we impose
		 * a limit of (STACK_SIZE - GUARD_SIZE) on the stack
		 * region, since we can do this with our mmap.
		 *
		 * Our mmap with MAP_STACK takes addr as the maximum
		 * downsize limit on BOS, and as len the max size of
		 * the region.  It then maps the top SGROWSIZ bytes,
		 * and auto grows the region down, up to the limit
		 * in addr.
		 *
		 * If we don't use the MAP_STACK option, the effect
		 * of this code is to allocate a stack region of a
		 * fixed size of (STACK_SIZE - GUARD_SIZE).
		 */

		if ((caddr_t)addr + len > vms->vm_maxsaddr) {
			/*
			 * Some Linux apps will attempt to mmap
			 * thread stacks near the top of their
			 * address space.  If their TOS is greater
			 * than vm_maxsaddr, vm_map_growstack()
			 * will confuse the thread stack with the
			 * process stack and deliver a SEGV if they
			 * attempt to grow the thread stack past their
			 * current stacksize rlimit.  To avoid this,
			 * adjust vm_maxsaddr upwards to reflect
			 * the current stacksize rlimit rather
			 * than the maximum possible stacksize.
			 * It would be better to adjust the
			 * mmap'ed region, but some apps do not check
			 * mmap's return value.
			 */
			PROC_LOCK(p);
			vms->vm_maxsaddr = (char *)round_page(vms->vm_stacktop) -
			    lim_cur_proc(p, RLIMIT_STACK);
			PROC_UNLOCK(p);
		}

		/*
		 * This gives us our maximum stack size and a new BOS.
		 * If we're using VM_STACK, then mmap will just map
		 * the top SGROWSIZ bytes, and let the stack grow down
		 * to the limit at BOS.  If we're not using VM_STACK
		 * we map the full stack, since we don't have a way
		 * to autogrow it.
		 */
		if (len <= STACK_SIZE - GUARD_SIZE) {
			addr = addr - (STACK_SIZE - GUARD_SIZE - len);
			len = STACK_SIZE - GUARD_SIZE;
		}
	}

	/*
	 * FreeBSD is free to ignore the address hint if MAP_FIXED wasn't
	 * passed.  However, some Linux applications, like the ART runtime,
	 * depend on the hint.  If the MAP_FIXED wasn't passed, but the
	 * address is not zero, try with MAP_FIXED and MAP_EXCL first,
	 * and fall back to the normal behaviour if that fails.
	 */
	mr = (struct mmap_req) {
		.mr_hint = addr,
		.mr_len = len,
		.mr_prot = prot,
		.mr_flags = bsd_flags,
		.mr_fd = fd,
		.mr_pos = pos,
		.mr_check_fp_fn = linux_mmap_check_fp,
	};
	if (addr != 0 && (bsd_flags & MAP_FIXED) == 0 &&
	    (bsd_flags & MAP_EXCL) == 0) {
		mr_fixed = mr;
		mr_fixed.mr_flags |= MAP_FIXED | MAP_EXCL;
		error = kern_mmap(td, &mr_fixed);
		if (error == 0)
			goto out;
	}

	error = kern_mmap(td, &mr);
out:
	LINUX_CTR2(mmap2, "return: %d (%p)", error, td->td_retval[0]);

	return (error);
}

int
linux_mprotect_common(struct thread *td, uintptr_t addr, size_t len, int prot)
{
	int flags = 0;

	/* XXX Ignore PROT_GROWSUP for now. */
	prot &= ~LINUX_PROT_GROWSUP;
	if ((prot & ~(LINUX_PROT_GROWSDOWN | PROT_READ | PROT_WRITE |
	    PROT_EXEC)) != 0)
		return (EINVAL);
	if ((prot & LINUX_PROT_GROWSDOWN) != 0) {
		prot &= ~LINUX_PROT_GROWSDOWN;
		flags |= VM_MAP_PROTECT_GROWSDOWN;
	}

#if defined(__amd64__)
	linux_fixup_prot(td, &prot);
#endif
	return (kern_mprotect(td, addr, len, prot, flags));
}

/*
 * Implement Linux madvise(MADV_DONTNEED), which has unusual semantics: for
 * anonymous memory, pages in the range are immediately discarded.
 */
static int
linux_madvise_dontneed(struct thread *td, vm_offset_t start, vm_offset_t end)
{
	vm_map_t map;
	vm_map_entry_t entry;
	vm_object_t backing_object, object;
	vm_offset_t estart, eend;
	vm_pindex_t pstart, pend;
	int error;

	map = &td->td_proc->p_vmspace->vm_map;

	if (!vm_map_range_valid(map, start, end))
		return (EINVAL);
	start = trunc_page(start);
	end = round_page(end);

	error = 0;
	vm_map_lock_read(map);
	if (!vm_map_lookup_entry(map, start, &entry))
		entry = vm_map_entry_succ(entry);
	for (; entry->start < end; entry = vm_map_entry_succ(entry)) {
		if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) != 0)
			continue;

		if (entry->wired_count != 0) {
			error = EINVAL;
			break;
		}

		object = entry->object.vm_object;
		if (object == NULL)
			continue;
		if ((object->flags & (OBJ_UNMANAGED | OBJ_FICTITIOUS)) != 0)
			continue;

		pstart = OFF_TO_IDX(entry->offset);
		if (start > entry->start) {
			pstart += atop(start - entry->start);
			estart = start;
		} else {
			estart = entry->start;
		}
		pend = OFF_TO_IDX(entry->offset) +
		    atop(entry->end - entry->start);
		if (entry->end > end) {
			pend -= atop(entry->end - end);
			eend = end;
		} else {
			eend = entry->end;
		}

		if ((object->flags & (OBJ_ANON | OBJ_ONEMAPPING)) ==
		    (OBJ_ANON | OBJ_ONEMAPPING)) {
			/*
			 * Singly-mapped anonymous memory is discarded.  This
			 * does not match Linux's semantics when the object
			 * belongs to a shadow chain of length > 1, since
			 * subsequent faults may retrieve pages from an
			 * intermediate anonymous object.  However, handling
			 * this case correctly introduces a fair bit of
			 * complexity.
			 */
			VM_OBJECT_WLOCK(object);
			if ((object->flags & OBJ_ONEMAPPING) != 0) {
				vm_object_collapse(object);
				vm_object_page_remove(object, pstart, pend, 0);
				backing_object = object->backing_object;
				if (backing_object != NULL &&
				    (backing_object->flags & OBJ_ANON) != 0)
					linux_msg(td,
					    "possibly incorrect MADV_DONTNEED");
				VM_OBJECT_WUNLOCK(object);
				continue;
			}
			VM_OBJECT_WUNLOCK(object);
		}

		/*
		 * Handle shared mappings.  Remove them outright instead of
		 * calling pmap_advise(), for consistency with Linux.
		 */
		pmap_remove(map->pmap, estart, eend);
		vm_object_madvise(object, pstart, pend, MADV_DONTNEED);
	}
	vm_map_unlock_read(map);

	return (error);
}

int
linux_madvise_common(struct thread *td, uintptr_t addr, size_t len, int behav)
{

	switch (behav) {
	case LINUX_MADV_NORMAL:
		return (kern_madvise(td, addr, len, MADV_NORMAL));
	case LINUX_MADV_RANDOM:
		return (kern_madvise(td, addr, len, MADV_RANDOM));
	case LINUX_MADV_SEQUENTIAL:
		return (kern_madvise(td, addr, len, MADV_SEQUENTIAL));
	case LINUX_MADV_WILLNEED:
		return (kern_madvise(td, addr, len, MADV_WILLNEED));
	case LINUX_MADV_DONTNEED:
		return (linux_madvise_dontneed(td, addr, addr + len));
	case LINUX_MADV_FREE:
		return (kern_madvise(td, addr, len, MADV_FREE));
	case LINUX_MADV_REMOVE:
		linux_msg(curthread, "unsupported madvise MADV_REMOVE");
		return (EINVAL);
	case LINUX_MADV_DONTFORK:
		return (kern_minherit(td, addr, len, INHERIT_NONE));
	case LINUX_MADV_DOFORK:
		return (kern_minherit(td, addr, len, INHERIT_COPY));
	case LINUX_MADV_MERGEABLE:
		linux_msg(curthread, "unsupported madvise MADV_MERGEABLE");
		return (EINVAL);
	case LINUX_MADV_UNMERGEABLE:
		/* We don't merge anyway. */
		return (0);
	case LINUX_MADV_HUGEPAGE:
		/* Ignored; on FreeBSD huge pages are always on. */
		return (0);
	case LINUX_MADV_NOHUGEPAGE:
#if 0
		/*
		 * Don't warn - Firefox uses it a lot, and in real Linux it's
		 * an optional feature.
		 */
		linux_msg(curthread, "unsupported madvise MADV_NOHUGEPAGE");
#endif
		return (EINVAL);
	case LINUX_MADV_DONTDUMP:
		return (kern_madvise(td, addr, len, MADV_NOCORE));
	case LINUX_MADV_DODUMP:
		return (kern_madvise(td, addr, len, MADV_CORE));
	case LINUX_MADV_WIPEONFORK:
		return (kern_minherit(td, addr, len, INHERIT_ZERO));
	case LINUX_MADV_KEEPONFORK:
		return (kern_minherit(td, addr, len, INHERIT_COPY));
	case LINUX_MADV_HWPOISON:
		linux_msg(curthread, "unsupported madvise MADV_HWPOISON");
		return (EINVAL);
	case LINUX_MADV_SOFT_OFFLINE:
		linux_msg(curthread, "unsupported madvise MADV_SOFT_OFFLINE");
		return (EINVAL);
	case -1:
		/*
		 * -1 is sometimes used as a dummy value to detect simplistic
		 * madvise(2) stub implementations.  This safeguard is used by
		 * BoringSSL, for example, before assuming MADV_WIPEONFORK is
		 * safe to use.  Don't produce an "unsupported" error message
		 * for this special dummy value, which is unlikely to be used
		 * by any new advisory behavior feature.
		 */
		return (EINVAL);
	default:
		linux_msg(curthread, "unsupported madvise behav %d", behav);
		return (EINVAL);
	}
}

#if defined(__amd64__)
static void
linux_fixup_prot(struct thread *td, int *prot)
{
	struct linux_pemuldata *pem;

	if (SV_PROC_FLAG(td->td_proc, SV_ILP32) && *prot & PROT_READ) {
		pem = pem_find(td->td_proc);
		if (pem->persona & LINUX_READ_IMPLIES_EXEC)
			*prot |= PROT_EXEC;
	}

}
#endif
