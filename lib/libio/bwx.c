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
#include <machine/bwx.h>
#include <machine/sysarch.h>
#include <stdlib.h>
#include "io.h"

#define mb()	__asm__ __volatile__("mb"  : : : "memory")
#define wmb()	__asm__ __volatile__("wmb" : : : "memory")

static int		mem_fd;		/* file descriptor to /dev/mem */
static void	       *bwx_int1_ports;	/* mapped int1 io ports */
static void	       *bwx_int2_ports;	/* mapped int2 io ports */
static void	       *bwx_int4_ports;	/* mapped int4 io ports */
static u_int64_t	bwx_io_base;	/* physical address of ports */
static u_int64_t	bwx_mem_base;	/* physical address of bwx mem */

static void
bwx_init()
{
    size_t len = sizeof(u_int64_t);
    int error;

    mem_fd = open(_PATH_MEM, O_RDWR);
    if (mem_fd < 0)
	err(1, _PATH_MEM);
    bwx_int1_ports = mmap(0, 1L<<32, PROT_READ, MAP_ANON, -1, 0);
    bwx_int2_ports = mmap(0, 1L<<32, PROT_READ, MAP_ANON, -1, 0);
    bwx_int4_ports = mmap(0, 1L<<32, PROT_READ, MAP_ANON, -1, 0);

    if ((error = sysctlbyname("hw.chipset.ports", &bwx_io_base, &len,
			      0, 0)) < 0)
	err(1, "hw.chipset.ports");
    if ((error = sysctlbyname("hw.chipset.memory", &bwx_mem_base, &len,
			      0, 0)) < 0)
	err(1, "hw.chipset.memory");
}

static int
bwx_ioperm(u_int32_t from, u_int32_t num, int on)
{
    u_int32_t start, end;

    if (!bwx_int1_ports)
	bwx_init();

    if (!on)
	return -1;		/* XXX can't unmap yet */
   
    start = trunc_page(from);
    end = round_page(from + num);
    
    munmap(bwx_int1_ports + start, end-start);
    munmap(bwx_int2_ports + start, end-start);
    munmap(bwx_int4_ports + start, end-start);
    mmap(bwx_int1_ports + start, end-start, PROT_READ|PROT_WRITE, MAP_SHARED,
	 mem_fd, bwx_io_base + BWX_EV56_INT1 + start);
    mmap(bwx_int2_ports + start, end-start, PROT_READ|PROT_WRITE, MAP_SHARED,
	 mem_fd, bwx_io_base + BWX_EV56_INT2 + start);
    mmap(bwx_int4_ports + start, end-start, PROT_READ|PROT_WRITE, MAP_SHARED,
	 mem_fd, bwx_io_base + BWX_EV56_INT4 + start);
    return 0;
}

static u_int8_t
bwx_inb(u_int32_t port)
{
    mb();
    return ldbu((vm_offset_t)bwx_int1_ports + port);
}

static u_int16_t
bwx_inw(u_int32_t port)
{
    mb();
    return ldwu((vm_offset_t)bwx_int2_ports + port);
}

static u_int32_t
bwx_inl(u_int32_t port)
{
    mb();
    return ldl((vm_offset_t)bwx_int4_ports + port);
}

static void
bwx_outb(u_int32_t port, u_int8_t val)
{
    stb((vm_offset_t)bwx_int1_ports + port, val);
    wmb();
}

static void
bwx_outw(u_int32_t port, u_int16_t val)
{
    stw((vm_offset_t)bwx_int2_ports + port, val);
    wmb();
}

static void
bwx_outl(u_int32_t port, u_int32_t val)
{
    stl((vm_offset_t)bwx_int4_ports + port, val);
    wmb();
}

struct bwx_mem_handle {
    void	*virt1;		/* int1 address in user address-space */
    void	*virt2;		/* int2 address in user address-space */
    void	*virt4;		/* int4 address in user address-space */
};

static void *
bwx_map_memory(u_int32_t address, u_int32_t size)
{
    struct bwx_mem_handle *h;
    h = malloc(sizeof(struct bwx_mem_handle));
    if (!h) return 0;
    h->virt1 = mmap(0, size << 5, PROT_READ|PROT_WRITE, MAP_SHARED,
		    mem_fd, bwx_mem_base + BWX_EV56_INT1 + address);
    if ((long) h->virt1 == -1) {
	free(h);
	return 0;
    }
    h->virt2 = mmap(0, size << 5, PROT_READ|PROT_WRITE, MAP_SHARED,
		    mem_fd, bwx_mem_base + BWX_EV56_INT2 + address);
    if ((long) h->virt2 == -1) {
	munmap(h->virt1, size);
	free(h);
	return 0;
    }
    h->virt4 = mmap(0, size << 5, PROT_READ|PROT_WRITE, MAP_SHARED,
		    mem_fd, bwx_mem_base + BWX_EV56_INT4 + address);
    if ((long) h->virt4 == -1) {
	munmap(h->virt1, size);
	munmap(h->virt2, size);
	free(h);
	return 0;
    }
    return h;
}

static void
bwx_unmap_memory(void *handle, u_int32_t size)
{
    struct bwx_mem_handle *h = handle;
    munmap(h->virt1, size);
    munmap(h->virt2, size);
    munmap(h->virt4, size);
    free(h);
}

static u_int8_t
bwx_readb(void *handle, u_int32_t offset)
{
    struct bwx_mem_handle *h = handle;
    return ldbu((vm_offset_t)h->virt1 + offset);
}

static u_int16_t
bwx_readw(void *handle, u_int32_t offset)
{
    struct bwx_mem_handle *h = handle;
    return ldwu((vm_offset_t)h->virt2 + offset);
}

static u_int32_t
bwx_readl(void *handle, u_int32_t offset)
{
    struct bwx_mem_handle *h = handle;
    return ldl((vm_offset_t)h->virt4 + offset);
}

static void
bwx_writeb(void *handle, u_int32_t offset, u_int8_t val)
{
    struct bwx_mem_handle *h = handle;
    stb_nb((vm_offset_t)h->virt1 + offset, val);
}

static void
bwx_writew(void *handle, u_int32_t offset, u_int16_t val)
{
    struct bwx_mem_handle *h = handle;
    stw_nb((vm_offset_t)h->virt2 + offset, val);
}

static void
bwx_writel(void *handle, u_int32_t offset, u_int32_t val)
{
    struct bwx_mem_handle *h = handle;
    stl_nb((vm_offset_t)h->virt4 + offset, val);
}

struct io_ops bwx_io_ops = {
    bwx_ioperm,
    bwx_inb,
    bwx_inw,
    bwx_inl,
    bwx_outb,
    bwx_outw,
    bwx_outl,
    bwx_map_memory,
    bwx_unmap_memory,
    bwx_readb,
    bwx_readw,
    bwx_readl,
    bwx_writeb,
    bwx_writew,
    bwx_writel,
};
