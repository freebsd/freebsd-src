/* upf.c

   Ultrix PacketFilter interface code. */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.   All rights reserved.
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
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: upf.c,v 1.3.2.5 1999/03/29 22:07:13 mellon Exp $ Copyright (c) 1995, 1996, 1997, 1998, 1999 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#if defined (USE_UPF_SEND) || defined (USE_UPF_RECEIVE)
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <net/pfilt.h>
#include <netinet/in_systm.h>
#include "includes/netinet/ip.h"
#include "includes/netinet/udp.h"
#include "includes/netinet/if_ether.h"

/* Reinitializes the specified interface after an address change.   This
   is not required for packet-filter APIs. */

#ifdef USE_UPF_SEND
void if_reinitialize_send (info)
	struct interface_info *info;
{
}
#endif

#ifdef USE_UPF_RECEIVE
void if_reinitialize_receive (info)
	struct interface_info *info;
{
}
#endif

/* Called by get_interface_list for each interface that's discovered.
   Opens a packet filter for each interface and adds it to the select
   mask. */

int if_register_upf (info)
	struct interface_info *info;
{
	int sock;
	char filename[50];
	int b;
	struct endevp param;

	/* Open a UPF device */
	for (b = 0; 1; b++) {
#ifndef NO_SNPRINTF
		snprintf(filename, sizeof(filename), "/dev/pf/pfilt%d", b);
#else
		sprintf(filename, "/dev/pf/pfilt%d", b);
#endif
		sock = open (filename, O_RDWR, 0);
		if (sock < 0) {
			if (errno == EBUSY) {
				continue;
			} else {
				error ("Can't find free upf: %m");
			}
		} else {
			break;
		}
	}

	/* Set the UPF device to point at this interface. */
	if (ioctl (sock, EIOCSETIF, info -> ifp) < 0)
		error ("Can't attach interface %s to upf device %s: %m",
		       info -> name, filename);

	/* Get the hardware address. */
	if (ioctl (sock, EIOCDEVP, &param) < 0)
		error ("Can't get interface %s hardware address: %m",
		       info -> name);

	/* We only know how to do ethernet. */
	if (param.end_dev_type != ENDT_10MB)	
		error ("Invalid device type on network interface %s: %d",
		       info -> name, param.end_dev_type);

	if (param.end_addr_len != 6)
		error ("Invalid hardware address length on %s: %d",
		       info -> name, param.end_addr_len);

	info -> hw_address.hlen = 6;
	info -> hw_address.htype = ARPHRD_ETHER;
	memcpy (&info -> hw_address.haddr [0], param.end_addr, 6);

	return sock;
}
#endif /* USE_UPF_SEND || USE_UPF_RECEIVE */

#ifdef USE_UPF_SEND
void if_register_send (info)
	struct interface_info *info;
{
	/* If we're using the upf API for sending and receiving,
	   we don't need to register this interface twice. */
#ifndef USE_UPF_RECEIVE
	info -> wfdesc = if_register_upf (info, interface);
#else
	info -> wfdesc = info -> rfdesc;
#endif
        if (!quiet_interface_discovery)
		note ("Sending on   UPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.htype,
				     info -> hw_address.hlen,
				     info -> hw_address.haddr),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}
#endif /* USE_UPF_SEND */

#ifdef USE_UPF_RECEIVE
/* Packet filter program...
   XXX Changes to the filter program may require changes to the constant
   offsets used in if_register_send to patch the UPF program! XXX */


void if_register_receive (info)
	struct interface_info *info;
{
	int flag = 1;
	u_int32_t addr;
	struct enfilter pf;
	u_int32_t bits;

	/* Open a UPF device and hang it on this interface... */
	info -> rfdesc = if_register_upf (info);

	/* Allow the copyall flag to be set... */
	if (ioctl(info -> rfdesc, EIOCALLOWCOPYALL, &flag) < 0)
		error ("Can't set ALLOWCOPYALL: %m");

	/* Clear all the packet filter mode bits first... */
	flag = (ENHOLDSIG | ENBATCH | ENTSTAMP | ENPROMISC |
		ENNONEXCL | ENCOPYALL);
	if (ioctl (info -> rfdesc, EIOCMBIC, &flag) < 0)
		error ("Can't clear pfilt bits: %m");

	/* Set the ENBATCH and ENCOPYALL bits... */
	bits = ENBATCH | ENCOPYALL;
	if (ioctl (info -> rfdesc, EIOCMBIS, &bits) < 0)
		error ("Can't set ENBATCH|ENCOPYALL: %m");

	/* Set up the UPF filter program. */
	/* XXX Unlike the BPF filter program, this one won't work if the
	   XXX IP packet is fragmented or if there are options on the IP
	   XXX header. */
	pf.enf_Priority = 0;
	pf.enf_FilterLen = 0;

	pf.enf_Filter [pf.enf_FilterLen++] = ENF_PUSHWORD + 6;
	pf.enf_Filter [pf.enf_FilterLen++] = ENF_PUSHLIT + ENF_CAND;
	pf.enf_Filter [pf.enf_FilterLen++] = htons (ETHERTYPE_IP);
	pf.enf_Filter [pf.enf_FilterLen++] = ENF_PUSHLIT;
	pf.enf_Filter [pf.enf_FilterLen++] = htons (IPPROTO_UDP);
	pf.enf_Filter [pf.enf_FilterLen++] = ENF_PUSHWORD + 11;
	pf.enf_Filter [pf.enf_FilterLen++] = ENF_PUSHLIT + ENF_AND;
	pf.enf_Filter [pf.enf_FilterLen++] = htons (0xFF);
	pf.enf_Filter [pf.enf_FilterLen++] = ENF_CAND;
	pf.enf_Filter [pf.enf_FilterLen++] = ENF_PUSHWORD + 18;
	pf.enf_Filter [pf.enf_FilterLen++] = ENF_PUSHLIT + ENF_CAND;
	pf.enf_Filter [pf.enf_FilterLen++] = local_port;

	if (ioctl (info -> rfdesc, EIOCSETF, &pf) < 0)
		error ("Can't install packet filter program: %m");
        if (!quiet_interface_discovery)
		note ("Listening on UPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.htype,
				     info -> hw_address.hlen,
				     info -> hw_address.haddr),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}
#endif /* USE_UPF_RECEIVE */

#ifdef USE_UPF_SEND
ssize_t send_packet (interface, packet, raw, len, from, to, hto)
	struct interface_info *interface;
	struct packet *packet;
	struct dhcp_packet *raw;
	size_t len;
	struct in_addr from;
	struct sockaddr_in *to;
	struct hardware *hto;
{
	int bufp = 0;
	unsigned char buf [256];
	struct iovec iov [2];
	int result;

	if (!strcmp (interface -> name, "fallback"))
		return send_fallback (interface, packet, raw,
				      len, from, to, hto);

	/* Assemble the headers... */
	assemble_hw_header (interface, buf, &bufp, hto);
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
		warn ("send_packet: %m");
	return result;
}
#endif /* USE_UPF_SEND */

#ifdef USE_UPF_RECEIVE
ssize_t receive_packet (interface, buf, len, from, hfrom)
	struct interface_info *interface;
	unsigned char *buf;
	size_t len;
	struct sockaddr_in *from;
	struct hardware *hfrom;
{
	int nread;
	int length = 0;
	int offset = 0;
	unsigned char ibuf [1500 + sizeof (struct enstamp)];
	int bufix = 0;

	length = read (interface -> rfdesc, ibuf, sizeof ibuf);
	if (length <= 0)
		return length;

	bufix = sizeof (struct enstamp);
	/* Decode the physical header... */
	offset = decode_hw_header (interface, ibuf, bufix, hfrom);

	/* If a physical layer checksum failed (dunno of any
	   physical layer that supports this, but WTH), skip this
	   packet. */
	if (offset < 0) {
		return 0;
	}

	bufix += offset;
	length -= offset;

	/* Decode the IP and UDP headers... */
	offset = decode_udp_ip_header (interface, ibuf, bufix,
				       from, (unsigned char *)0, length);

	/* If the IP or UDP checksum was bad, skip the packet... */
	if (offset < 0)
		return 0;

	bufix += offset;
	length -= offset;

	/* Copy out the data in the packet... */
	memcpy (buf, &ibuf [bufix], length);
	return length;
}

int can_unicast_without_arp ()
{
	return 1;
}

int can_receive_unicast_unconfigured (ip)
	struct interface_info *ip;
{
	return 1;
}

void maybe_setup_fallback ()
{
	struct interface_info *fbi;
	fbi = setup_fallback ();
	if (fbi) {
		if_register_fallback (fbi);
		add_protocol ("fallback", fallback_interface -> wfdesc,
			      fallback_discard, fallback_interface);
	}
}
#endif
