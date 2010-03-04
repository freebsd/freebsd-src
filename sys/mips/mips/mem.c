/*	$OpenBSD: mem.c,v 1.2 1998/08/31 17:42:34 millert Exp $ */
/*	$NetBSD: mem.c,v 1.6 1995/04/10 11:55:03 mycroft Exp $	*/
/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)mem.c	8.3 (Berkeley) 1/12/94
 *	JNPR: mem.c,v 1.3 2007/08/09 11:23:32 katta Exp $
 */

/*
 * Memory special file
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <sys/msgbuf.h>
#include <sys/systm.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/sched.h>
#include <sys/malloc.h>
#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/atomic.h>
#include <machine/memdev.h>


extern struct sysmaps sysmaps_pcpu[];
/*ARGSUSED*/
int
memrw(dev, uio, flags)
	struct cdev *dev;
	struct uio *uio;
	int flags;
{
	register vm_offset_t v;
	register int c;
	register struct iovec *iov;
	int error = 0;

	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("mmrw");
			continue;
		}

		/* minor device 0 is physical memory */
		if (dev2unit(dev) == CDEV_MINOR_MEM) {
			v = uio->uio_offset;
			c = iov->iov_len;

			vm_offset_t va;
			vm_paddr_t pa;
			register int o;

			if (is_cacheable_mem(v) &&
			    is_cacheable_mem(v + c - 1)) {
				struct fpage *fp;
				struct sysmaps *sysmaps;

				sysmaps = &sysmaps_pcpu[PCPU_GET(cpuid)];
				mtx_lock(&sysmaps->lock);
				sched_pin();

				fp = &sysmaps->fp[PMAP_FPAGE1];
				pa = uio->uio_offset & ~PAGE_MASK;
				va = pmap_map_fpage(pa, fp, FALSE);
				o = (int)uio->uio_offset & PAGE_MASK;
				c = (u_int)(PAGE_SIZE -
					    ((uintptr_t)iov->iov_base & PAGE_MASK));
				c = min(c, (u_int)(PAGE_SIZE - o));
				c = min(c, (u_int)iov->iov_len);
				error = uiomove((caddr_t)(va + o), (int)c, uio);
				pmap_unmap_fpage(pa, fp);
				sched_unpin();
				mtx_unlock(&sysmaps->lock);
			} else
				return (EFAULT);
			continue;
		}

		/* minor device 1 is kernel memory */
		else if (dev2unit(dev) == CDEV_MINOR_KMEM) {
			v = uio->uio_offset;
			c = min(iov->iov_len, MAXPHYS);

			vm_offset_t addr, eaddr;
			vm_offset_t wired_tlb_virtmem_end;

			/*
			 * Make sure that all of the pages are currently
			 * resident so that we don't create any zero-fill pages.
			 */
			addr = trunc_page(uio->uio_offset);
			eaddr = round_page(uio->uio_offset + c);

			if (addr > (vm_offset_t) VM_MIN_KERNEL_ADDRESS) {
				wired_tlb_virtmem_end = VM_MIN_KERNEL_ADDRESS +
				    VM_KERNEL_ALLOC_OFFSET;
				if ((addr < wired_tlb_virtmem_end) &&
				    (eaddr >= wired_tlb_virtmem_end))
					addr = wired_tlb_virtmem_end;

				if (addr >= wired_tlb_virtmem_end) {
					for (; addr < eaddr; addr += PAGE_SIZE) 
						if (pmap_extract(kernel_pmap,
						    addr) == 0)
							return EFAULT;

					if (!kernacc(
					    (caddr_t)(uintptr_t)uio->uio_offset, c,
					    uio->uio_rw == UIO_READ ?
					    VM_PROT_READ : VM_PROT_WRITE))
						return (EFAULT);
				}
			}
			else if (MIPS_IS_KSEG0_ADDR(v)) {
				if (MIPS_KSEG0_TO_PHYS(v + c) >= ctob(physmem))
					return (EFAULT);
			}
			else if (MIPS_IS_KSEG1_ADDR(v)) {
				if (MIPS_KSEG1_TO_PHYS(v + c) >= ctob(physmem))
					return (EFAULT);
			}
			else
				return (EFAULT);


			error = uiomove((caddr_t)v, c, uio);
			continue;
		}

	}
	return (error);
}

/*ARGSUSED*/
int
memmmap(struct cdev *dev, vm_ooffset_t off, vm_paddr_t *paddr,
    int prot, vm_memattr_t *memattr)
{

	return (EOPNOTSUPP);
}

void
dev_mem_md_init(void)
{
}
