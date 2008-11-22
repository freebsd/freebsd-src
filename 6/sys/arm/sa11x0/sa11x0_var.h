/*      $NetBSD: sa11x0_var.h,v 1.4 2003/04/14 14:18:41 rjs Exp $        */

/*-
 * Copyright (c) 2001, The NetBSD Foundation, Inc.  All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by IWAMOTO Toshihiro and Ichiro FUKUHARA.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * $FreeBSD$
 *
 */

#ifndef _SA11X0_VAR_H
#define _SA11X0_VAR_H

#include <sys/conf.h>

#include <sys/bus.h>
#include <machine/bus.h>

struct sa11x0_softc {
	device_t sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_space_handle_t sc_gpioh;
	bus_space_handle_t sc_ppch;
	bus_space_handle_t sc_dmach;
	bus_space_handle_t sc_reseth;
	u_int32_t sc_intrmask;
};

/* Attach args all devices */

typedef void *sa11x0_chipset_tag_t;

extern struct bus_space sa11x0_bs_tag;
struct sa11x0_attach_args {
	sa11x0_chipset_tag_t	sa_sc;		
	bus_space_tag_t		sa_iot;		/* Bus tag */
	bus_addr_t		sa_addr;	/* i/o address  */
	bus_size_t		sa_size;

	int			sa_intr;
	int			sa_gpio;
};

void *sa11x0_intr_establish(sa11x0_chipset_tag_t, int, int, int, 
			    int (*)(void *), void *);
void sa11x0_intr_disestablish(sa11x0_chipset_tag_t, void *);

#endif /* _SA11X0_VAR_H */
