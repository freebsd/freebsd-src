/*-
 * Copyright (c) 2001 - 2003 by Thomas Moestl <tmm@FreeBSD.org>.
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
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sun4v/include/ofw_bus.h,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_MACHINE_OFW_BUS_H_
#define	_MACHINE_OFW_BUS_H_

#define	ORIP_NOINT	-1
#define	ORIR_NOTFOUND	0xffffffff

/*
 * Other than in Open Firmware calls, the size of a bus cell seems to be
 * always the same.
 */
typedef u_int32_t pcell_t;

struct ofw_bus_iinfo {
	u_int8_t		*opi_imap;
	u_int8_t		*opi_imapmsk;
	int			opi_imapsz;
	pcell_t			opi_addrc;
};

void ofw_bus_setup_iinfo(phandle_t, struct ofw_bus_iinfo *, int);
int ofw_bus_lookup_imap(phandle_t, struct ofw_bus_iinfo *, void *, int,
    void *, int, void *, int, void *);
int ofw_bus_search_intrmap(void *, int, void *, int, void *, int, void *,
    void *, void *, int);

#endif /* !_MACHINE_OFW_BUS_H_ */
