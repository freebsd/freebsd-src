/*
 * Copyright (c) 1999 M. Warner Losh <imp@village.org> 
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
 *
 * $FreeBSD: src/sys/dev/sn/if_snvar.h,v 1.2 2000/01/22 17:24:16 hosokawa Exp $
 */

#ifndef _IF_SNVAR_H
#define _IF_SNVAR_H

#include <net/if_arp.h>

/*
 * Ethernet software status per interface.  The first element MUST
 * be the arpcom struct since the address of the arpcom struct is
 * used as a backdoor to obtain the address of this whole structure
 * in many cases.
 */
struct sn_softc {
	struct arpcom   arpcom;	/* Ethernet common part */
	short           sn_io_addr;	/* i/o bus address (BASE) */
	int             pages_wanted;	/* Size of outstanding MMU ALLOC */
	int             intr_mask;	/* Most recently set interrupt mask */
	device_t	dev;
	void		*intrhand;
	struct resource *irq_res;
	int		irq_rid;
	struct resource	*port_res;
	int		port_rid;
	int		pccard_enaddr;	/* MAC address in pccard CIS tupple */
};

int	sn_probe(device_t, int);
int	sn_attach(device_t);
void	sn_intr(void *);

int	sn_activate(device_t);
void	sn_deactivate(device_t);

#endif /* _IF_SNVAR_H */
