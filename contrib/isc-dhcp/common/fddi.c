/* fddi.c

   Packet assembly code, originally contributed by Archie Cobbs. */

/*
 * Copyright (c) 1996-2002 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: fddi.c,v 1.3.2.1 2002/11/17 02:26:58 dhankins Exp $ Copyright (c) 1996-2002 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

#if defined (DEC_FDDI)
#include <netinet/if_fddi.h>
#include <net/if_llc.h>

#if defined (PACKET_ASSEMBLY) || defined (PACKET_DECODING)
#include "includes/netinet/if_ether.h"
#endif /* PACKET_ASSEMBLY || PACKET_DECODING */

#if defined (PACKET_ASSEMBLY)
/* Assemble an hardware header... */

void assemble_fddi_header (interface, buf, bufix, to)
	struct interface_info *interface;
	unsigned char *buf;
	unsigned *bufix;
	struct hardware *to;
{
	struct fddi_header   fh;
	struct llc     lh;

	if (to && to -> hlen == 7)
		memcpy (fh.fddi_dhost, &to -> hbuf [1],
			sizeof (fh.fddi_dhost));
	memcpy (fh.fddi_shost,
		&interface -> hw_address.hbuf [1], sizeof (fh.fddi_shost));
	fh.fddi_fc = FDDIFC_LLC_ASYNC;
	memcpy (&buf [*bufix], &fh, sizeof fh);
	*bufix += sizeof fh;

	lh.llc_dsap = LLC_SNAP_LSAP;
	lh.llc_ssap = LLC_SNAP_LSAP;
	lh.llc_un.type_snap.control = LLC_UI;
	lh.llc_un.type_snap.ether_type = htons (ETHERTYPE_IP);
	memcpy (&buf [*bufix], &lh, LLC_SNAP_LEN);
	*bufix += LLC_SNAP_LEN;
}
#endif /* PACKET_ASSEMBLY */

#ifdef PACKET_DECODING
/* Decode a hardware header... */

ssize_t decode_fddi_header (interface, buf, bufix, from)
     struct interface_info *interface;
     unsigned char *buf;
     unsigned bufix;
     struct hardware *from;
{
	struct fddi_header   fh;
	struct llc     lh;
	
	from -> hbuf [0] = HTYPE_FDDI;
	memcpy (&from -> hbuf [1], fh.fddi_shost, sizeof fh.fddi_shost);
	return FDDI_HEADER_SIZE + LLC_SNAP_LEN;
}
#endif /* PACKET_DECODING */
#endif /* DEC_FDDI */
