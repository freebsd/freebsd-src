/*-
 * Copyright (c) 1998 Doug Rabson
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

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/sysctl.h>
#include <err.h>
#include <paths.h>
#include <machine/swiz.h>
#include <machine/sysarch.h>
#include <stdlib.h>
#include "io.h"

#define mb()	__asm__ __volatile__("mb"  : : : "memory")
#define wmb()	__asm__ __volatile__("wmb" : : : "memory")

static int		mem_fd;		/* file descriptor to /dev/mem */
static void	       *swiz_ports;	/* mapped io ports */
static u_int64_t	swiz_io_base;	/* physical address of ports */
static u_int64_t	swiz_mem_base;	/* physical address of sparse mem */
static u_int64_t	swiz_dense_base; /* physical address of dense mem */
static u_int64_t	swiz_hae_mask;	/* mask address bits for hae */
static u_int32_t	swiz_hae;	/* cache of current hae */

static void
swiz_init()
{

    size_t len = sizeof(u_int64_t);
    int error;

    mem_fd = open(_PATH_MEM, O_RDWR);
    if (mem_fd < 0)
	err(1, _PATH_MEM);
    swiz_ports = mmap(0, 1L<<32, PROT_READ, MAP_ANON, -1, 0);

    if ((error = sysctlbyname("hw.chipset.ports", &swiz_io_base, &len,
                              0, 0)) < 0)
	    err(1, "hw.chipset.ports");
    if ((error = sysctlbyname("hw.chipset.memory", &swiz_mem_base, &len,
                              0, 0)) < 0)
	    err(1, "hw.chipset.memory");
    if ((error = sysctlbyname("hw.chipset.dense", &swiz_dense_base, &len,
                              0, 0)) < 0)
	    err(1, "hw.chipset.memory");
    if ((error = sysctlbyname("hw.chipset.hae_mask", &swiz_hae_mask, &len,
                              0, 0)) < 0)
	    err(1, "hw.chipset.memory");

}

static int
swiz_ioperm(u_int32_t from, u_int32_t num, int on)
{
    u_int64_t start, end;
    void *addr;

    if (!swiz_ports)
	swiz_init();

    if (!on)
	return -1;		/* XXX can't unmap yet */

    start = trunc_page(from << 5);
    end = round_page((from + num) << 5);
    addr = swiz_ports + start;
    munmap(addr, end - start);
    mmap(addr, end - start, PROT_READ|PROT_WRITE, MAP_SHARED,
	 mem_fd, swiz_io_base + start);
    return 0;
}

static u_int8_t
swiz_inb(u_int32_t port)
{
    mb();
    return SPARSE_READ_BYTE(swiz_ports, port);
}

static u_int16_t
swiz_inw(u_int32_t port)
{
    mb();
    return SPARSE_READ_WORD(swiz_ports, port);
}

static u_int32_t
swiz_inl(u_int32_t port)
{
    mb();
    return SPARSE_READ_LONG(swiz_ports, port);
}

static void
swiz_outb(u_int32_t port, u_int8_t val)
{
    SPARSE_WRITE_BYTE(swiz_ports, port, val);
    wmb();
}

static void
swiz_outw(u_int32_t port, u_int16_t val)
{
    SPARSE_WRITE_WORD(swiz_ports, port, val);
    wmb();
}

static void
swiz_outl(u_int32_t port, u_int32_t val)
{
    SPARSE_WRITE_LONG(swiz_ports, port, val);
    wmb();
}

struct swiz_mem_handle {
    u_int32_t	phys;		/* address in PCI address-space */
    void	*virt;		/* address in user address-space */
    u_int32_t	size;		/* size of mapped region */
};

static void *
swiz_map_memory(u_int32_t address, u_int32_t size)
{
    struct swiz_mem_handle *h;
    h = malloc(sizeof(struct swiz_mem_handle));
    if (!h) return 0;
    h->phys = address;
    h->virt = mmap(0, size << 5, PROT_READ|PROT_WRITE, MAP_SHARED,
		   mem_fd,
		   swiz_mem_base + ((address & ~swiz_hae_mask) << 5));
    if ((long) h->virt == -1) {
	free(h);
	return 0;
    }
    h->size = size << 5;
    return h;
}

static void
swiz_unmap_memory(void *handle, u_int32_t size)
{
    struct swiz_mem_handle *h = handle;
    munmap(h->virt, h->size);
    free(h);
}

static void
swiz_sethae(vm_offset_t phys)
{
    u_int32_t hae = phys & swiz_hae_mask;
    if (hae != swiz_hae) {
	alpha_sethae(hae);
	swiz_hae = hae;
    }
}

static u_int8_t
swiz_readb(void *handle, u_int32_t offset)
{
    struct swiz_mem_handle *h = handle;
    swiz_sethae(h->phys + offset);
    return SPARSE_READ_BYTE(h->virt, offset);
}

static u_int16_t
swiz_readw(void *handle, u_int32_t offset)
{
    struct swiz_mem_handle *h = handle;
    swiz_sethae(h->phys + offset);
    return SPARSE_READ_WORD(h->virt, offset);
}

static u_int32_t
swiz_readl(void *handle, u_int32_t offset)
{
    struct swiz_mem_handle *h = handle;
    swiz_sethae(h->phys + offset);
    return SPARSE_READ_LONG(h->virt, offset);
}

static void
swiz_writeb(void *handle, u_int32_t offset, u_int8_t val)
{
    struct swiz_mem_handle *h = handle;
    swiz_sethae(h->phys + offset);
    SPARSE_WRITE_BYTE(h->virt, offset, val);
}

static void
swiz_writew(void *handle, u_int32_t offset, u_int16_t val)
{
    struct swiz_mem_handle *h = handle;
    swiz_sethae(h->phys + offset);
    SPARSE_WRITE_WORD(h->virt, offset, val);
}

static void
swiz_writel(void *handle, u_int32_t offset, u_int32_t val)
{
    struct swiz_mem_handle *h = handle;
    swiz_sethae(h->phys + offset);
    SPARSE_WRITE_LONG(h->virt, offset, val);
}

struct io_ops swiz_io_ops = {
    swiz_ioperm,
    swiz_inb,
    swiz_inw,
    swiz_inl,
    swiz_outb,
    swiz_outw,
    swiz_outl,
    swiz_map_memory,
    swiz_unmap_memory,
    swiz_readb,
    swiz_readw,
    swiz_readl,
    swiz_writeb,
    swiz_writew,
    swiz_writel,
};
