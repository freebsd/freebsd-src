/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2004 Alan L. Cox <alc@cs.rice.edu>
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <machine/vmparam.h>

/*
 * Implement uiomove(9) from physical memory using the direct map to
 * avoid the creation and destruction of ephemeral mappings.
 */
int
uiomove_fromphys(vm_page_t ma[], vm_offset_t offset, int n, struct uio *uio)
{
	struct thread *td = curthread;
	struct iovec *iov;
	void *cp;
	vm_offset_t page_offset, vaddr;
	size_t cnt;
	int error = 0;
	int save = 0;
	bool mapped;

	KASSERT(uio->uio_rw == UIO_READ || uio->uio_rw == UIO_WRITE,
	    ("uiomove_fromphys: mode"));
	KASSERT(uio->uio_segflg != UIO_USERSPACE || uio->uio_td == curthread,
	    ("uiomove_fromphys proc"));
	KASSERT(uio->uio_resid >= 0,
	    ("%s: uio %p resid underflow", __func__, uio));

	save = td->td_pflags & TDP_DEADLKTREAT;
	td->td_pflags |= TDP_DEADLKTREAT;
	mapped = false;
	while (n > 0 && uio->uio_resid) {
		KASSERT(uio->uio_iovcnt > 0,
		    ("%s: uio %p iovcnt underflow", __func__, uio));

		iov = uio->uio_iov;
		cnt = iov->iov_len;
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		if (cnt > n)
			cnt = n;
		page_offset = offset & PAGE_MASK;
		cnt = min(cnt, PAGE_SIZE - page_offset);
		if (uio->uio_segflg != UIO_NOCOPY) {
			mapped = pmap_map_io_transient(
			    &ma[offset >> PAGE_SHIFT], &vaddr, 1, true);
			cp = (char *)vaddr + page_offset;
		}
		switch (uio->uio_segflg) {
		case UIO_USERSPACE:
			maybe_yield();
			switch (uio->uio_rw) {
			case UIO_READ:
				error = copyout(cp, iov->iov_base, cnt);
				break;
			case UIO_WRITE:
				error = copyin(iov->iov_base, cp, cnt);
				break;
			}
			if (error)
				goto out;
			break;
		case UIO_SYSSPACE:
			switch (uio->uio_rw) {
			case UIO_READ:
				bcopy(cp, iov->iov_base, cnt);
				break;
			case UIO_WRITE:
				bcopy(iov->iov_base, cp, cnt);
				break;
			}
			break;
		case UIO_NOCOPY:
			break;
		}
		if (__predict_false(mapped)) {
			pmap_unmap_io_transient(&ma[offset >> PAGE_SHIFT],
			    &vaddr, 1, true);
			mapped = false;
		}
		iov->iov_base = (char *)iov->iov_base + cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_offset += cnt;
		offset += cnt;
		n -= cnt;
	}
out:
	if (__predict_false(mapped))
		pmap_unmap_io_transient(&ma[offset >> PAGE_SHIFT], &vaddr, 1,
		    true);
	if (save == 0)
		td->td_pflags &= ~TDP_DEADLKTREAT;
	return (error);
}
