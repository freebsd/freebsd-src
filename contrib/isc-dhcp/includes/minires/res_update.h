/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1999-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 */

/*
 *	$Id: res_update.h,v 1.2.2.2 2004/06/10 17:59:37 dhankins Exp $
 */

#ifndef __RES_UPDATE_H
#define __RES_UPDATE_H

#include <sys/types.h>
#include "arpa/nameser.h"
#include <isc-dhcp/list.h>

/*
 * This RR-like structure is particular to UPDATE.
 */
typedef struct ns_updrec {
	ISC_LINK(struct ns_updrec) r_link, r_glink;
	ns_sect r_section;		/* ZONE/PREREQUISITE/UPDATE */
	char *r_dname;			/* owner of the RR */
	ns_class r_class;		/* class number */
	ns_type r_type;			/* type number */
	u_int32_t r_ttl;		/* time to live */
	const unsigned char *r_data;	/* rdata fields as text string */
	unsigned char *r_data_ephem;	/* pointer to freeable r_data */
	unsigned int r_size;		/* size of r_data field */
	int r_opcode;			/* type of operation */
		/* following fields for private use by the resolver/server
		   routines */
	struct databuf *r_dp;		/* databuf to process */
	struct databuf *r_deldp;	/* databuf's deleted/overwritten */
	unsigned int r_zone;		/* zone number on server */
} ns_updrec;
typedef	ISC_LIST(ns_updrec) ns_updque;

#endif /*__RES_UPDATE_H*/
