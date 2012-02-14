/*
 * Copyright (C) 2004, 2005, 2007  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: lwaddr.h,v 1.8 2007-06-19 23:46:59 tbox Exp $ */

/*! \file */

#include <lwres/lwres.h>
#include <lwres/net.h>

isc_result_t
lwaddr_netaddr_fromlwresaddr(isc_netaddr_t *na, lwres_addr_t *la);

isc_result_t
lwaddr_sockaddr_fromlwresaddr(isc_sockaddr_t *sa, lwres_addr_t *la,
			      in_port_t port);

isc_result_t
lwaddr_lwresaddr_fromnetaddr(lwres_addr_t *la, isc_netaddr_t *na);

isc_result_t
lwaddr_lwresaddr_fromsockaddr(lwres_addr_t *la, isc_sockaddr_t *sa);
