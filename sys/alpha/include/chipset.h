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

#ifndef _MACHINE_CHIPSET_H_
#define _MACHINE_CHIPSET_H_

typedef u_int8_t	alpha_chipset_inb_t(u_int32_t port);
typedef u_int16_t	alpha_chipset_inw_t(u_int32_t port);
typedef u_int32_t	alpha_chipset_inl_t(u_int32_t port);
typedef void		alpha_chipset_outb_t(u_int32_t port, u_int8_t data);
typedef void		alpha_chipset_outw_t(u_int32_t port, u_int16_t data);
typedef void		alpha_chipset_outl_t(u_int32_t port, u_int32_t data);

typedef u_int8_t	alpha_chipset_readb_t(u_int32_t pa);
typedef u_int16_t	alpha_chipset_readw_t(u_int32_t pa);
typedef u_int32_t	alpha_chipset_readl_t(u_int32_t pa);
typedef void		alpha_chipset_writeb_t(u_int32_t pa, u_int8_t data);
typedef void		alpha_chipset_writew_t(u_int32_t pa, u_int16_t data);
typedef void		alpha_chipset_writel_t(u_int32_t pa, u_int32_t data);

typedef int		alpha_chipset_maxdevs_t(u_int bus);
typedef u_int8_t	alpha_chipset_cfgreadb_t(u_int, u_int, u_int, u_int, u_int);
typedef u_int16_t	alpha_chipset_cfgreadw_t(u_int, u_int, u_int, u_int, u_int);
typedef u_int32_t	alpha_chipset_cfgreadl_t(u_int, u_int, u_int, u_int, u_int);
typedef void		alpha_chipset_cfgwriteb_t(u_int, u_int, u_int, u_int, u_int,
						  u_int8_t);
typedef void		alpha_chipset_cfgwritew_t(u_int, u_int, u_int, u_int, u_int,
						  u_int16_t);
typedef void		alpha_chipset_cfgwritel_t(u_int, u_int, u_int, u_int, u_int,
						  u_int32_t);
typedef vm_offset_t     alpha_chipset_addrcvt_t(vm_offset_t);
typedef u_int64_t	alpha_chipset_read_hae_t(void);
typedef void		alpha_chipset_write_hae_t(u_int64_t);

struct sgmap;

typedef struct alpha_chipset {
    /*
     * I/O port access
     */
    alpha_chipset_inb_t*	inb;
    alpha_chipset_inw_t*	inw;
    alpha_chipset_inl_t*	inl;
    alpha_chipset_outb_t*	outb;
    alpha_chipset_outw_t*	outw;
    alpha_chipset_outl_t*	outl;

    /*
     * Memory access
     */
    alpha_chipset_readb_t*	readb;
    alpha_chipset_readw_t*	readw;
    alpha_chipset_readl_t*	readl;
    alpha_chipset_writeb_t*	writeb;
    alpha_chipset_writew_t*	writew;
    alpha_chipset_writel_t*	writel;

    /*
     * PCI configuration access
     */
    alpha_chipset_maxdevs_t*	maxdevs;
    alpha_chipset_cfgreadb_t*	cfgreadb;
    alpha_chipset_cfgreadw_t*	cfgreadw;
    alpha_chipset_cfgreadl_t*	cfgreadl;
    alpha_chipset_cfgwriteb_t*	cfgwriteb;
    alpha_chipset_cfgwritew_t*	cfgwritew;
    alpha_chipset_cfgwritel_t*	cfgwritel;

    /*
     * PCI address space translation functions
     */
    alpha_chipset_addrcvt_t*	cvt_to_dense;
    alpha_chipset_addrcvt_t*	cvt_to_bwx;

    /*
     * Access the HAE register
     */
    alpha_chipset_read_hae_t*	read_hae;
    alpha_chipset_write_hae_t*	write_hae;

    /*
     * Scatter-Gather map for ISA dma.
     */
    struct sgmap*		sgmap;
} alpha_chipset_t;

extern alpha_chipset_t chipset;

/*
 * Exported sysctl variables describing the PCI chipset.
 */
extern char chipset_type[10];
extern int chipset_bwx;
extern long chipset_ports;
extern long chipset_memory;
extern long chipset_dense;
extern long chipset_hae_mask;

#endif /* !_MACHINE_CHIPSET_H_ */
