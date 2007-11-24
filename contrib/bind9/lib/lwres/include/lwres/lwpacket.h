/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: lwpacket.h,v 1.17.206.1 2004/03/06 08:15:35 marka Exp $ */

#ifndef LWRES_LWPACKET_H
#define LWRES_LWPACKET_H 1

#include <lwres/lang.h>
#include <lwres/lwbuffer.h>
#include <lwres/result.h>

typedef struct lwres_lwpacket lwres_lwpacket_t;

struct lwres_lwpacket {
	lwres_uint32_t		length;
	lwres_uint16_t		version;
	lwres_uint16_t		pktflags;
	lwres_uint32_t		serial;
	lwres_uint32_t		opcode;
	lwres_uint32_t		result;
	lwres_uint32_t		recvlength;
	lwres_uint16_t		authtype;
	lwres_uint16_t		authlength;
};

#define LWRES_LWPACKET_LENGTH		(4 * 5 + 2 * 4)

#define LWRES_LWPACKETFLAG_RESPONSE	0x0001U	/* if set, pkt is a response */


#define LWRES_LWPACKETVERSION_0		0

/*
 * "length" is the overall packet length, including the entire packet header.
 *
 * "version" specifies the header format.  Currently, there is only one
 * format, LWRES_LWPACKETVERSION_0.
 *
 * "flags" specifies library-defined flags for this packet.  None of these
 * are definable by the caller, but library-defined values can be set by
 * the caller.  For example, one bit in this field indicates if the packet
 * is a request or a response.
 *
 * "serial" is set by the requestor and is returned in all replies.  If two
 * packets from the same source have the same serial number and are from
 * the same source, they are assumed to be duplicates and the latter ones
 * may be dropped.  (The library does not do this by default on replies, but
 * does so on requests.)
 *
 * "opcode" is application defined.  Opcodes between 0x04000000 and 0xffffffff
 * are application defined.  Opcodes between 0x00000000 and 0x03ffffff are
 * reserved for library use.
 *
 * "result" is application defined, and valid only on replies.
 * Results between 0x04000000 and 0xffffffff are application defined.
 * Results between 0x00000000 and 0x03ffffff are reserved for library use.
 * (This is the same reserved range defined in <isc/resultclass.h>, so it
 * would be trivial to map ISC_R_* result codes into packet result codes
 * when appropriate.)
 *
 * "recvlength" is set to the maximum buffer size that the receiver can
 * handle on requests, and the size of the buffer needed to satisfy a request
 * when the buffer is too large for replies.
 *
 * "authtype" is the packet level auth type used.
 * Authtypes between 0x1000 and 0xffff are application defined.  Authtypes
 * between 0x0000 and 0x0fff are reserved for library use.  This is currently
 * unused and MUST be set to zero.
 *
 * "authlen" is the length of the authentication data.  See the specific
 * authtypes for more information on what is contained in this field.  This
 * is currently unused, and MUST be set to zero.
 *
 * The remainder of the packet consists of two regions, one described by
 * "authlen" and one of "length - authlen - sizeof(lwres_lwpacket_t)".
 *
 * That is:
 *
 *	pkt header
 *	authlen bytes of auth information
 *	data bytes
 */

/*
 * Currently defined opcodes:
 *
 *	NOOP.  Success is always returned, with the packet contents echoed.
 *
 *	GETADDRSBYNAME.  Return all known addresses for a given name.
 *		This may return NIS or /etc/hosts info as well as DNS
 *		information.  Flags will be provided to indicate ip4/ip6
 *		addresses are desired.
 *
 *	GETNAMEBYADDR.	Return the hostname for the given address.  Once
 *		again, it will return data from multiple sources.
 */

LWRES_LANG_BEGINDECLS

/* XXXMLG document */
lwres_result_t
lwres_lwpacket_renderheader(lwres_buffer_t *b, lwres_lwpacket_t *pkt);

lwres_result_t
lwres_lwpacket_parseheader(lwres_buffer_t *b, lwres_lwpacket_t *pkt);

LWRES_LANG_ENDDECLS

#endif /* LWRES_LWPACKET_H */
