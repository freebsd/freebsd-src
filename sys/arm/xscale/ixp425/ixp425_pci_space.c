/*	$NetBSD: ixp425_pci_space.c,v 1.6 2006/04/10 03:36:03 simonb Exp $ */

/*
 * Copyright (c) 2003
 *	Ichiro FUKUHARA <ichiro@ichiro.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ichiro FUKUHARA.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ICHIRO FUKUHARA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ICHIRO FUKUHARA OR THE VOICES IN HIS HEAD BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * bus_space PCI functions for ixp425
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <machine/pcb.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

/*
 * Macros to read/write registers
*/
#define CSR_READ_4(x)		*(volatile uint32_t *) \
	(IXP425_PCI_CSR_BASE + (x))
#define CSR_WRITE_4(x, v)	*(volatile uint32_t *) \
	(IXP425_PCI_CSR_BASE + (x)) = (v)

/* Proto types for all the bus_space structure functions */
bs_protos(ixp425_pci);
bs_protos(ixp425_pci_io);
bs_protos(ixp425_pci_mem);

/* special I/O functions */
static u_int8_t  _pci_io_bs_r_1(void *, bus_space_handle_t, bus_size_t);
static u_int16_t _pci_io_bs_r_2(void *, bus_space_handle_t, bus_size_t);
static u_int32_t _pci_io_bs_r_4(void *, bus_space_handle_t, bus_size_t);

static void _pci_io_bs_w_1(void *, bus_space_handle_t, bus_size_t, u_int8_t);
static void _pci_io_bs_w_2(void *, bus_space_handle_t, bus_size_t, u_int16_t);
static void _pci_io_bs_w_4(void *, bus_space_handle_t, bus_size_t, u_int32_t);

#ifdef __ARMEB__
static u_int8_t  _pci_io_bs_r_1_s(void *, bus_space_handle_t, bus_size_t);
static u_int16_t _pci_io_bs_r_2_s(void *, bus_space_handle_t, bus_size_t);
static u_int32_t _pci_io_bs_r_4_s(void *, bus_space_handle_t, bus_size_t);

static void _pci_io_bs_w_1_s(void *, bus_space_handle_t, bus_size_t, u_int8_t);
static void _pci_io_bs_w_2_s(void *, bus_space_handle_t, bus_size_t, u_int16_t);
static void _pci_io_bs_w_4_s(void *, bus_space_handle_t, bus_size_t, u_int32_t);

static u_int8_t  _pci_mem_bs_r_1(void *, bus_space_handle_t, bus_size_t);
static u_int16_t _pci_mem_bs_r_2(void *, bus_space_handle_t, bus_size_t);
static u_int32_t _pci_mem_bs_r_4(void *, bus_space_handle_t, bus_size_t);

static void _pci_mem_bs_w_1(void *, bus_space_handle_t, bus_size_t, u_int8_t);
static void _pci_mem_bs_w_2(void *, bus_space_handle_t, bus_size_t, u_int16_t);
static void _pci_mem_bs_w_4(void *, bus_space_handle_t, bus_size_t, u_int32_t);
#endif

struct bus_space ixp425_pci_io_bs_tag_template = {
	/* mapping/unmapping */
	.bs_map		= ixp425_pci_io_bs_map,
	.bs_unmap	= ixp425_pci_io_bs_unmap,
	.bs_subregion	= ixp425_pci_bs_subregion,

	.bs_alloc	= ixp425_pci_io_bs_alloc,
	.bs_free	= ixp425_pci_io_bs_free,

	/* barrier */
	.bs_barrier	= ixp425_pci_bs_barrier,

	/*
	 * IXP425 processor does not have PCI I/O windows
	 */
	/* read (single) */
	.bs_r_1		= _pci_io_bs_r_1,
	.bs_r_2		= _pci_io_bs_r_2,
	.bs_r_4		= _pci_io_bs_r_4,

	/* write (single) */
	.bs_w_1		= _pci_io_bs_w_1,
	.bs_w_2		= _pci_io_bs_w_2,
	.bs_w_4		= _pci_io_bs_w_4,

#ifdef __ARMEB__
	.bs_r_1_s	= _pci_io_bs_r_1_s,
	.bs_r_2_s	= _pci_io_bs_r_2_s,
	.bs_r_4_s	= _pci_io_bs_r_4_s,

	.bs_w_1_s	= _pci_io_bs_w_1_s,
	.bs_w_2_s	= _pci_io_bs_w_2_s,
	.bs_w_4_s	= _pci_io_bs_w_4_s,
#else
	.bs_r_1_s	= _pci_io_bs_r_1,
	.bs_r_2_s	= _pci_io_bs_r_2,
	.bs_r_4_s	= _pci_io_bs_r_4,

	.bs_w_1_s	= _pci_io_bs_w_1,
	.bs_w_2_s	= _pci_io_bs_w_2,
	.bs_w_4_s	= _pci_io_bs_w_4,
#endif
};

void
ixp425_io_bs_init(bus_space_tag_t bs, void *cookie)
{
	*bs = ixp425_pci_io_bs_tag_template;
	bs->bs_cookie = cookie;
}

struct bus_space ixp425_pci_mem_bs_tag_template = {
	/* mapping/unmapping */
	.bs_map		= ixp425_pci_mem_bs_map,
	.bs_unmap	= ixp425_pci_mem_bs_unmap,
	.bs_subregion	= ixp425_pci_bs_subregion,

	.bs_alloc	= ixp425_pci_mem_bs_alloc,
	.bs_free	= ixp425_pci_mem_bs_free,

	/* barrier */
	.bs_barrier	= ixp425_pci_bs_barrier,

#ifdef __ARMEB__
	/* read (single) */
	.bs_r_1_s	= _pci_mem_bs_r_1,
	.bs_r_2_s	= _pci_mem_bs_r_2,
	.bs_r_4_s	= _pci_mem_bs_r_4,

	.bs_r_1	= 	ixp425_pci_mem_bs_r_1,
	.bs_r_2	= 	ixp425_pci_mem_bs_r_2,
	.bs_r_4	= 	ixp425_pci_mem_bs_r_4,

	/* write (single) */
	.bs_w_1_s	= _pci_mem_bs_w_1,
	.bs_w_2_s	= _pci_mem_bs_w_2,
	.bs_w_4_s	= _pci_mem_bs_w_4,

	.bs_w_1	=	 ixp425_pci_mem_bs_w_1,
	.bs_w_2	= 	ixp425_pci_mem_bs_w_2,
	.bs_w_4	= 	ixp425_pci_mem_bs_w_4,
#else
	/* read (single) */
	.bs_r_1		= ixp425_pci_mem_bs_r_1,
	.bs_r_2		= ixp425_pci_mem_bs_r_2,
	.bs_r_4		= ixp425_pci_mem_bs_r_4,
	.bs_r_1_s	= ixp425_pci_mem_bs_r_1,
	.bs_r_2_s	= ixp425_pci_mem_bs_r_2,
	.bs_r_4_s	= ixp425_pci_mem_bs_r_4,

	/* write (single) */
	.bs_w_1		= ixp425_pci_mem_bs_w_1,
	.bs_w_2		= ixp425_pci_mem_bs_w_2,
	.bs_w_4		= ixp425_pci_mem_bs_w_4,
	.bs_w_1_s	= ixp425_pci_mem_bs_w_1,
	.bs_w_2_s	= ixp425_pci_mem_bs_w_2,
	.bs_w_4_s	= ixp425_pci_mem_bs_w_4,
#endif
};

void
ixp425_mem_bs_init(bus_space_tag_t bs, void *cookie)
{
	*bs = ixp425_pci_mem_bs_tag_template;
	bs->bs_cookie = cookie;
}

/* common routine */
int
ixp425_pci_bs_subregion(void *t, bus_space_handle_t bsh, bus_size_t offset,
	bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

void
ixp425_pci_bs_barrier(void *t, bus_space_handle_t bsh, bus_size_t offset,
    bus_size_t len, int flags)
{
	/* NULL */
}	

/* io bs */
int
ixp425_pci_io_bs_map(void *t, bus_addr_t bpa, bus_size_t size,
	int cacheable, bus_space_handle_t *bshp)
{
	*bshp = bpa;
	return (0);
}

void
ixp425_pci_io_bs_unmap(void *t, bus_space_handle_t h, bus_size_t size)
{
	/* Nothing to do. */
}

int
ixp425_pci_io_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
	bus_size_t size, bus_size_t alignment, bus_size_t boundary, int cacheable,
	bus_addr_t *bpap, bus_space_handle_t *bshp)
{
	panic("ixp425_pci_io_bs_alloc(): not implemented\n");
}

void
ixp425_pci_io_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{
	panic("ixp425_pci_io_bs_free(): not implemented\n");
}

/* special I/O functions */
static __inline u_int32_t
_bs_r(void *v, bus_space_handle_t ioh, bus_size_t off, u_int32_t be)
{
	u_int32_t data;

	CSR_WRITE_4(PCI_NP_AD, (ioh + off) & ~3);
	CSR_WRITE_4(PCI_NP_CBE, be | COMMAND_NP_IO_READ);
	data = CSR_READ_4(PCI_NP_RDATA);
	if (CSR_READ_4(PCI_ISR) & ISR_PFE)
		CSR_WRITE_4(PCI_ISR, ISR_PFE);

	return data;
}

static u_int8_t
_pci_io_bs_r_1(void *v, bus_space_handle_t ioh, bus_size_t off)
{
	u_int32_t data, n, be;

	n = (ioh + off) % 4;
	be = (0xf & ~(1U << n)) << NP_CBE_SHIFT;
	data = _bs_r(v, ioh, off, be);

	return data >> (8 * n);
}

static u_int16_t
_pci_io_bs_r_2(void *v, bus_space_handle_t ioh, bus_size_t off)
{
	u_int32_t data, n, be;

	n = (ioh + off) % 4;
	be = (0xf & ~((1U << n) | (1U << (n + 1)))) << NP_CBE_SHIFT;
	data = _bs_r(v, ioh, off, be);

	return data >> (8 * n);
}

static u_int32_t
_pci_io_bs_r_4(void *v, bus_space_handle_t ioh, bus_size_t off)
{
	u_int32_t data;

	data = _bs_r(v, ioh, off, 0);
	return data;
}

#ifdef __ARMEB__
static u_int8_t
_pci_io_bs_r_1_s(void *v, bus_space_handle_t ioh, bus_size_t off)
{
	u_int32_t data, n, be;

	n = (ioh + off) % 4;
	be = (0xf & ~(1U << n)) << NP_CBE_SHIFT;
	data = _bs_r(v, ioh, off, be);

	return data >> (8 * n);
}

static u_int16_t
_pci_io_bs_r_2_s(void *v, bus_space_handle_t ioh, bus_size_t off)
{
	u_int32_t data, n, be;

	n = (ioh + off) % 4;
	be = (0xf & ~((1U << n) | (1U << (n + 1)))) << NP_CBE_SHIFT;
	data = _bs_r(v, ioh, off, be);

	return data >> (8 * n);
}

static u_int32_t
_pci_io_bs_r_4_s(void *v, bus_space_handle_t ioh, bus_size_t off)
{
	u_int32_t data;

	data = _bs_r(v, ioh, off, 0);
	return le32toh(data);
}
#endif /* __ARMEB__ */

static __inline void
_bs_w(void *v, bus_space_handle_t ioh, bus_size_t off,
	u_int32_t be, u_int32_t data)
{
	CSR_WRITE_4(PCI_NP_AD, (ioh + off) & ~3);
	CSR_WRITE_4(PCI_NP_CBE, be | COMMAND_NP_IO_WRITE);
	CSR_WRITE_4(PCI_NP_WDATA, data);
	if (CSR_READ_4(PCI_ISR) & ISR_PFE)
		CSR_WRITE_4(PCI_ISR, ISR_PFE);
}

static void
_pci_io_bs_w_1(void *v, bus_space_handle_t ioh, bus_size_t off,
	u_int8_t val)
{
	u_int32_t data, n, be;

	n = (ioh + off) % 4;
	be = (0xf & ~(1U << n)) << NP_CBE_SHIFT;
	data = val << (8 * n);
	_bs_w(v, ioh, off, be, data);
}

static void
_pci_io_bs_w_2(void *v, bus_space_handle_t ioh, bus_size_t off,
	u_int16_t val)
{
	u_int32_t data, n, be;

	n = (ioh + off) % 4;
	be = (0xf & ~((1U << n) | (1U << (n + 1)))) << NP_CBE_SHIFT;
	data = val << (8 * n);
	_bs_w(v, ioh, off, be, data);
}

static void
_pci_io_bs_w_4(void *v, bus_space_handle_t ioh, bus_size_t off,
	u_int32_t val)
{
	_bs_w(v, ioh, off, 0, val);
}

#ifdef __ARMEB__
static void
_pci_io_bs_w_1_s(void *v, bus_space_handle_t ioh, bus_size_t off,
	u_int8_t val)
{
	u_int32_t data, n, be;

	n = (ioh + off) % 4;
	be = (0xf & ~(1U << n)) << NP_CBE_SHIFT;
	data = val << (8 * n);
	_bs_w(v, ioh, off, be, data);
}

static void
_pci_io_bs_w_2_s(void *v, bus_space_handle_t ioh, bus_size_t off,
	u_int16_t val)
{
	u_int32_t data, n, be;

	n = (ioh + off) % 4;
	be = (0xf & ~((1U << n) | (1U << (n + 1)))) << NP_CBE_SHIFT;
	data = val << (8 * n);
	_bs_w(v, ioh, off, be, data);
}

static void
_pci_io_bs_w_4_s(void *v, bus_space_handle_t ioh, bus_size_t off,
	u_int32_t val)
{
	_bs_w(v, ioh, off, 0, htole32(val));
}
#endif /* __ARMEB__ */

/* mem bs */
int
ixp425_pci_mem_bs_map(void *t, bus_addr_t bpa, bus_size_t size,
	      int cacheable, bus_space_handle_t *bshp)
{
	vm_paddr_t pa, endpa;

	pa = trunc_page(bpa);
	endpa = round_page(bpa + size);

	*bshp = (vm_offset_t)pmap_mapdev(pa, endpa - pa);

	return (0);
}

void
ixp425_pci_mem_bs_unmap(void *t, bus_space_handle_t h, bus_size_t size)
{
	vm_offset_t va, endva;

	va = trunc_page((vm_offset_t)t);
	endva = va + round_page(size);

	/* Free the kernel virtual mapping. */
	kmem_free(kernel_map, va, endva - va);
}

int
ixp425_pci_mem_bs_alloc(void *t, bus_addr_t rstart, bus_addr_t rend,
	bus_size_t size, bus_size_t alignment, bus_size_t boundary, int cacheable,
	bus_addr_t *bpap, bus_space_handle_t *bshp)
{
	panic("ixp425_mem_bs_alloc(): not implemented\n");
}

void
ixp425_pci_mem_bs_free(void *t, bus_space_handle_t bsh, bus_size_t size)
{
	panic("ixp425_mem_bs_free(): not implemented\n");
}

#ifdef __ARMEB__
static u_int8_t
_pci_mem_bs_r_1(void *v, bus_space_handle_t ioh, bus_size_t off)
{
	return ixp425_pci_mem_bs_r_1(v, ioh, off);
}

static u_int16_t
_pci_mem_bs_r_2(void *v, bus_space_handle_t ioh, bus_size_t off)
{
	return (ixp425_pci_mem_bs_r_2(v, ioh, off));
}

static u_int32_t
_pci_mem_bs_r_4(void *v, bus_space_handle_t ioh, bus_size_t off)
{
	u_int32_t data;

	data = ixp425_pci_mem_bs_r_4(v, ioh, off);
	return (le32toh(data));
}

static void
_pci_mem_bs_w_1(void *v, bus_space_handle_t ioh, bus_size_t off,
	u_int8_t val)
{
	ixp425_pci_mem_bs_w_1(v, ioh, off, val);
}

static void
_pci_mem_bs_w_2(void *v, bus_space_handle_t ioh, bus_size_t off,
	u_int16_t val)
{
	ixp425_pci_mem_bs_w_2(v, ioh, off, val);
}

static void
_pci_mem_bs_w_4(void *v, bus_space_handle_t ioh, bus_size_t off,
	u_int32_t val)
{
	ixp425_pci_mem_bs_w_4(v, ioh, off, htole32(val));
}
#endif /* __ARMEB__ */

/* End of ixp425_pci_space.c */
