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

typedef u_int64_t	alpha_chipset_read_hae_t(void);
typedef void		alpha_chipset_write_hae_t(u_int64_t);

struct sgmap;

typedef struct alpha_chipset {
    /*
     * Access the HAE register
     */
    alpha_chipset_read_hae_t*	read_hae;
    alpha_chipset_write_hae_t*	write_hae;

    /*
     * Scatter-Gather map for ISA dma.
     */
    struct sgmap*		sgmap;

    /*
     * Scatter-Gather map for PCI dma.
     */
    struct sgmap*		pci_sgmap;

    /*
     * direct map
     */
    long			dmsize;
    long			dmoffset;
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
