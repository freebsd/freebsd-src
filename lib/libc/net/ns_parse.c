/*
 * Copyright (c) 1996 by Internet Software Consortium.
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
static char rcsid[] = "$Id: ns_parse.c,v 8.8 1998/02/17 17:20:33 vixie Exp $";
#endif

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <resolv.h>
#include <string.h>

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

static int
skiprr(const u_char *ptr, const u_char *eom, ns_sect section, int count) {
	const u_char *optr = ptr;

	for ((void)NULL; count > 0; count--) {
		int b, rdlength;

		b = dn_skipname(ptr, eom);
		if (b < 0)
			goto emsgsize;
		ptr += b/*Name*/ + NS_INT16SZ/*Type*/ + NS_INT16SZ/*Class*/;
		if (section != ns_s_qd) {
			if (ptr + NS_INT32SZ > eom)
				goto emsgsize;
			ptr += NS_INT32SZ/*TTL*/;
			if (ptr + NS_INT16SZ > eom)
				goto emsgsize;
			NS_GET16(rdlength, ptr);
			ptr += rdlength/*RData*/;
		}
	}
	if (ptr > eom)
		goto emsgsize;
	return (ptr - optr);
 emsgsize:
	errno = EMSGSIZE;
	return (-1);
}

int
ns_initparse(const u_char *msg, int msglen, ns_msg *handle) {
	const u_char *eom = msg + msglen;
	int i;

	memset(handle, 0x5e, sizeof *handle);
	handle->_msg = msg;
	handle->_eom = eom;
	if (msg + NS_INT16SZ > eom)
		goto emsgsize;
	NS_GET16(handle->_id, msg);
	if (msg + NS_INT16SZ > eom)
		goto emsgsize;
	NS_GET16(handle->_flags, msg);
	for (i = 0; i < ns_s_max; i++) {
		if (msg + NS_INT16SZ > eom)
			goto emsgsize;
		NS_GET16(handle->_counts[i], msg);
	}
	for (i = 0; i < ns_s_max; i++)
		if (handle->_counts[i] == 0)
			handle->_sections[i] = NULL;
		else {
			int b = skiprr(msg, eom, (ns_sect)i,
				       handle->_counts[i]);

			if (b < 0)
				return (-1);
			handle->_sections[i] = msg;
			msg += b;
		}
	if (msg != eom)
		goto emsgsize;
	handle->_sect = ns_s_max;
	handle->_rrnum = -1;
	handle->_ptr = NULL;
	return (0);
 emsgsize:
	errno = EMSGSIZE;
	return (-1);
}

int
ns_parserr(ns_msg *handle, ns_sect section, int rrnum, ns_rr *rr) {
	int b;

	/* Make section right. */
	if (section < 0 || section >= ns_s_max)
		goto enodev;
	if ((int)section != (int)handle->_sect) {
		handle->_sect = section;
		handle->_rrnum = 0;
		handle->_ptr = handle->_sections[(int)section];
	}

	/* Make rrnum right. */
	if (rrnum == -1)
		rrnum = handle->_rrnum;
	if (rrnum < 0 || rrnum >= handle->_counts[(int)section])
		goto enodev;
	if (rrnum < handle->_rrnum) {
		handle->_rrnum = 0;
		handle->_ptr = handle->_sections[(int)section];
	}
	
	b = skiprr(handle->_msg, handle->_eom, section,
		   rrnum - handle->_rrnum);
	if (b < 0)
		return (-1);
	handle->_ptr += b;
	handle->_rrnum = rrnum;

	/* Do the parse. */
	b = dn_expand(handle->_msg, handle->_eom,
		      handle->_ptr, rr->name, NS_MAXDNAME);
	if (b < 0)
		return (-1);
	handle->_ptr += b;
	if (handle->_ptr + NS_INT16SZ > handle->_eom)
		goto emsgsize;
	NS_GET16(rr->type, handle->_ptr);
	if (handle->_ptr + NS_INT16SZ > handle->_eom)
		goto emsgsize;
	NS_GET16(rr->class, handle->_ptr);
	if (section == ns_s_qd) {
		rr->ttl = 0;
		rr->rdlength = 0;
		rr->rdata = NULL;
	} else {
		if (handle->_ptr + NS_INT32SZ > handle->_eom)
			goto emsgsize;
		NS_GET32(rr->ttl, handle->_ptr);
		if (handle->_ptr + NS_INT16SZ > handle->_eom)
			goto emsgsize;
		NS_GET16(rr->rdlength, handle->_ptr);
		if (handle->_ptr + rr->rdlength > handle->_eom)
			goto emsgsize;
		rr->rdata = handle->_ptr;
		handle->_ptr += rr->rdlength;
	}
	handle->_rrnum++;

	/* All done. */
	return (0);
 enodev:
	errno = ENODEV;
	return (-1);
 emsgsize:
	errno = EMSGSIZE;
	return (-1);
}
