/* $NetBSD: s3c2xx0var.h,v 1.3 2003/08/05 11:26:54 bsh Exp $ */

/*-
 * Copyright (c) 2002 Fujitsu Component Limited
 * Copyright (c) 2002 Genetec Corporation
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
 * 3. Neither the name of The Fujitsu Component Limited nor the name of
 *    Genetec corporation may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY FUJITSU COMPONENT LIMITED AND GENETEC
 * CORPORATION ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL FUJITSU COMPONENT LIMITED OR GENETEC
 * CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _ARM_S3C2XX0VAR_H_
#define _ARM_S3C2XX0VAR_H_

#include <machine/bus.h>
#include <sys/rman.h>

typedef enum {
	CPU_S3C2410,
	CPU_S3C2440,
} s3c2xx0_cpu;

struct s3c2xx0_softc {
	device_t	   	sc_dev;

	s3c2xx0_cpu		sc_cpu;

	bus_space_tag_t  	sc_iot;

	bus_space_handle_t	sc_intctl_ioh;
	bus_space_handle_t	sc_gpio_ioh;  	/* GPIO */
	bus_space_handle_t	sc_clkman_ioh; 	/* Clock manager */
	bus_space_handle_t	sc_wdt_ioh; 	/* Watchdog Timer */

	bus_dma_tag_t  		sc_dmat;

	/* clock frequency */
	int sc_fclk;			/* CPU clock */
	int sc_hclk;			/* AHB bus clock */
	int sc_pclk;			/* peripheral clock */

	struct rman s3c2xx0_irq_rman;
	struct rman s3c2xx0_mem_rman;
};

struct s3c2xx0_ivar {
	struct resource_list resources;
};

typedef void *s3c2xx0_chipset_tag_t;

extern struct bus_space s3c2xx0_bs_tag;
extern struct s3c2xx0_softc *s3c2xx0_softc;
extern struct arm32_bus_dma_tag s3c2xx0_bus_dma;

/* Platform needs to provide this */
bus_dma_tag_t s3c2xx0_bus_dma_init(struct arm32_bus_dma_tag *);

#endif /* _ARM_S3C2XX0VAR_H_ */
