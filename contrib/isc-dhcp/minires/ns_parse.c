/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef lint
static const char rcsid[] = "$Id: ns_parse.c,v 1.2 2001/01/16 22:33:08 mellon Exp $";
#endif

/* Import. */

#include <sys/types.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include <errno.h>
#include <string.h>

#include "minires/minires.h"
#include "arpa/nameser.h"

/* Forward. */

static void	setsection(ns_msg *msg, ns_sect sect);

/* Macros. */

#define RETERR(err) do { errno = (err); return (-1); } while (0)

/* Public. */

/* These need to be in the same order as the nres.h:ns_flag enum. */
struct _ns_flagdata _ns_flagdata[16] = {
	{ 0x8000, 15 },		/* qr. */
	{ 0x7800, 11 },		/* opcode. */
	{ 0x0400, 10 },		/* aa. */
	{ 0x0200, 9 },		/* tc. */
	{ 0x0100, 8 },		/* rd. */
	{ 0x0080, 7 },		/* ra. */
	{ 0x0040, 6 },		/* z. */
	{ 0x0020, 5 },		/* ad. */
	{ 0x0010, 4 },		/* cd. */
	{ 0x000f, 0 },		/* rcode. */
	{ 0x0000, 0 },		/* expansion (1/6). */
	{ 0x0000, 0 },		/* expansion (2/6). */
	{ 0x0000, 0 },		/* expansion (3/6). */
	{ 0x0000, 0 },		/* expansion (4/6). */
	{ 0x0000, 0 },		/* expansion (5/6). */
	{ 0x0000, 0 },		/* expansion (6/6). */
};

int
ns_skiprr(const u_char *ptr, const u_char *eom, ns_sect section, int count) {
	const u_char *optr = ptr;

	for ((void)NULL; count > 0; count--) {
		int b, rdlength;

		b = dn_skipname(ptr, eom);
		if (b < 0)
			RETERR(EMSGSIZE);
		ptr += b/*Name*/ + NS_INT16SZ/*Type*/ + NS_INT16SZ/*Class*/;
		if (section != ns_s_qd) {
			if (ptr + NS_INT32SZ + NS_INT16SZ > eom)
				RETERR(EMSGSIZE);
			ptr += NS_INT32SZ/*TTL*/;
			rdlength = getUShort(ptr);
			ptr += 2;
			ptr += rdlength/*RData*/;
		}
	}
	if (ptr > eom)
		RETERR(EMSGSIZE);
	return (ptr - optr);
}

int
ns_initparse(const u_char *msg, unsigned msglen, ns_msg *handle) {
	const u_char *eom = msg + msglen;
	int i;

	memset(handle, 0x5e, sizeof *handle);
	handle->_msg = msg;
	handle->_eom = eom;
	if (msg + NS_INT16SZ > eom)
		RETERR(EMSGSIZE);
	handle->_id = getUShort (msg);
	msg += 2;
	if (msg + NS_INT16SZ > eom)
		RETERR(EMSGSIZE);
	handle->_flags = getUShort (msg);
	msg += 2;
	for (i = 0; i < ns_s_max; i++) {
		if (msg + NS_INT16SZ > eom)
			RETERR(EMSGSIZE);
		handle->_counts[i] = getUShort (msg);
		msg += 2;
	}
	for (i = 0; i < ns_s_max; i++)
		if (handle->_counts[i] == 0)
			handle->_sections[i] = NULL;
		else {
			int b = ns_skiprr(msg, eom, (ns_sect)i,
					  handle->_counts[i]);

			if (b < 0)
				return (-1);
			handle->_sections[i] = msg;
			msg += b;
		}
	if (msg != eom)
		RETERR(EMSGSIZE);
	setsection(handle, ns_s_max);
	return (0);
}

isc_result_t
ns_parserr(ns_msg *handle, ns_sect section, int rrnum, ns_rr *rr) {
	int b;

	/* Make section right. */
	if (section < 0 || section >= ns_s_max)
		return ISC_R_NOTIMPLEMENTED;
	if (section != handle->_sect)
		setsection(handle, section);

	/* Make rrnum right. */
	if (rrnum == -1)
		rrnum = handle->_rrnum;
	if (rrnum < 0 || rrnum >= handle->_counts[(int)section])
		RETERR(ENODEV);
	if (rrnum < handle->_rrnum)
		setsection(handle, section);
	if (rrnum > handle->_rrnum) {
		b = ns_skiprr(handle->_ptr, handle->_eom, section,
			      rrnum - handle->_rrnum);

		if (b < 0)
			return (-1);
		handle->_ptr += b;
		handle->_rrnum = rrnum;
	}

	/* Do the parse. */
	b = dn_expand(handle->_msg, handle->_eom,
		      handle->_ptr, rr->name, NS_MAXDNAME);
	if (b < 0)
		return (-1);
	handle->_ptr += b;
	if (handle->_ptr + NS_INT16SZ + NS_INT16SZ > handle->_eom)
		return ISC_R_NOSPACE;
	rr->type = getUShort (handle->_ptr);
	handle -> _ptr += 2;
	rr->rr_class = getUShort (handle->_ptr);
	handle -> _ptr += 2;
	if (section == ns_s_qd) {
		rr->ttl = 0;
		rr->rdlength = 0;
		rr->rdata = NULL;
	} else {
		if (handle->_ptr + NS_INT32SZ + NS_INT16SZ > handle->_eom)
			return ISC_R_NOSPACE;
		rr->ttl = getULong (handle->_ptr);
		handle -> _ptr += 4;
		rr->rdlength = getUShort (handle->_ptr);
		handle -> _ptr += 2;
		if (handle->_ptr + rr->rdlength > handle->_eom)
			return ISC_R_NOSPACE;
		rr->rdata = handle->_ptr;
		handle->_ptr += rr->rdlength;
	}
	if (++handle->_rrnum > handle->_counts[(int)section])
		setsection(handle, (ns_sect)((int)section + 1));

	/* All done. */
	return ISC_R_SUCCESS;
}

/* Private. */

static void
setsection(ns_msg *msg, ns_sect sect) {
	msg->_sect = sect;
	if (sect == ns_s_max) {
		msg->_rrnum = -1;
		msg->_ptr = NULL;
	} else {
		msg->_rrnum = 0;
		msg->_ptr = msg->_sections[(int)sect];
	}
}
