/* socket.c

   BSD raw socket interface code... */

/* XXX

   It's not clear how this should work, and that lack of clarity is
   terribly detrimental to the NetBSD 1.1 kernel - it crashes and
   burns.

   Using raw sockets ought to be a big win over using BPF or something
   like it, because you don't need to deal with the complexities of
   the physical layer, but it appears not to be possible with existing
   raw socket implementations.  This may be worth revisiting in the
   future.  For now, this code can probably be considered a curiosity.
   Sigh. */

/*
 * Copyright (c) 1995-2000 Internet Software Consortium.
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
"$Id: raw.c,v 1.17 2000/03/17 03:59:01 mellon Exp $ Copyright (c) 1995, 1996 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

#if defined (USE_RAW_SEND)
#include <sys/uio.h>

/* Generic interface registration routine... */
void if_register_send (info)
	struct interface_info *info;
{
	struct sockaddr_in name;
	int sock;
	struct socklist *tmp;
	int flag;

	/* Set up the address we're going to connect to. */
	name.sin_family = AF_INET;
	name.sin_port = local_port;
	name.sin_addr.s_addr = htonl (INADDR_BROADCAST);
	memset (name.sin_zero, 0, sizeof (name.sin_zero));

	/* List addresses on which we're listening. */
        if (!quiet_interface_discovery)
		log_info ("Sending on %s, port %d",
		      piaddr (info -> address), htons (local_port));
	if ((sock = socket (AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		log_fatal ("Can't create dhcp socket: %m");

	/* Set the BROADCAST option so that we can broadcast DHCP responses. */
	flag = 1;
	if (setsockopt (sock, SOL_SOCKET, SO_BROADCAST,
			&flag, sizeof flag) < 0)
		log_fatal ("Can't set SO_BROADCAST option on dhcp socket: %m");

	/* Set the IP_HDRINCL flag so that we can supply our own IP
	   headers... */
	if (setsockopt (sock, IPPROTO_IP, IP_HDRINCL, &flag, sizeof flag) < 0)
		log_fatal ("Can't set IP_HDRINCL flag: %m");

	info -> wfdesc = sock;
        if (!quiet_interface_discovery)
		log_info ("Sending on   Raw/%s%s%s",
		      info -> name,
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}

void if_deregister_send (info)
	struct interface_info *info;
{
	close (info -> wfdesc);
	info -> wfdesc = -1;

        if (!quiet_interface_discovery)
		log_info ("Disabling output on Raw/%s%s%s",
		      info -> name,
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}

size_t send_packet (interface, packet, raw, len, from, to, hto)
	struct interface_info *interface;
	struct packet *packet;
	struct dhcp_packet *raw;
	size_t len;
	struct in_addr from;
	struct sockaddr_in *to;
	struct hardware *hto;
{
	unsigned char buf [256];
	int bufp = 0;
	struct iovec iov [2];
	int result;

	/* Assemble the headers... */
	assemble_udp_ip_header (interface, buf, &bufp, from.s_addr,
				to -> sin_addr.s_addr, to -> sin_port,
				(unsigned char *)raw, len);

	/* Fire it off */
	iov [0].iov_base = (char *)buf;
	iov [0].iov_len = bufp;
	iov [1].iov_base = (char *)raw;
	iov [1].iov_len = len;

	result = writev(interface -> wfdesc, iov, 2);
	if (result < 0)
		log_error ("send_packet: %m");
	return result;
}
#endif /* USE_SOCKET_SEND */
