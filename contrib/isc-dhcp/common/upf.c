/* upf.c

   Ultrix PacketFilter interface code. */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
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
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: upf.c,v 1.21.2.4 2004/06/17 20:54:39 dhankins Exp $ Copyright (c) 2004 Internet Systems Consortium.  All rights reserved.\n";
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
		/* %Audit% Cannot exceed 36 bytes. %2004.06.17,Safe% */
		sprintf(filename, "/dev/pf/pfilt%d", b);

		sock = open (filename, O_RDWR, 0);
		if (sock < 0) {
			if (errno == EBUSY) {
				continue;
			} else {
				log_fatal ("Can't find free upf: %m");
			}
		} else {
			break;
		}
	}

	/* Set the UPF device to point at this interface. */
	if (ioctl (sock, EIOCSETIF, info -> ifp) < 0)
		log_fatal ("Can't attach interface %s to upf device %s: %m",
		       info -> name, filename);

	/* Get the hardware address. */
	if (ioctl (sock, EIOCDEVP, &param) < 0)
		log_fatal ("Can't get interface %s hardware address: %m",
		       info -> name);

	/* We only know how to do ethernet. */
	if (param.end_dev_type != ENDT_10MB)	
		log_fatal ("Invalid device type on network interface %s: %d",
		       info -> name, param.end_dev_type);

	if (param.end_addr_len != 6)
		log_fatal ("Invalid hardware address length on %s: %d",
		       info -> name, param.end_addr_len);

	info -> hw_address.hlen = 7;
	info -> hw_address.hbuf [0] = ARPHRD_ETHER;
	memcpy (&info -> hw_address.hbuf [1], param.end_addr, 6);

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
		log_info ("Sending on   UPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}

void if_deregister_send (info)
	struct interface_info *info;
{
#ifndef USE_UPF_RECEIVE
	close (info -> wfdesc);
#endif
	info -> wfdesc = -1;
        if (!quiet_interface_discovery)
		log_info ("Disabling output on UPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
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
		log_fatal ("Can't set ALLOWCOPYALL: %m");

	/* Clear all the packet filter mode bits first... */
	flag = (ENHOLDSIG | ENBATCH | ENTSTAMP | ENPROMISC |
		ENNONEXCL | ENCOPYALL);
	if (ioctl (info -> rfdesc, EIOCMBIC, &flag) < 0)
		log_fatal ("Can't clear pfilt bits: %m");

	/* Set the ENBATCH and ENCOPYALL bits... */
	bits = ENBATCH | ENCOPYALL;
	if (ioctl (info -> rfdesc, EIOCMBIS, &bits) < 0)
		log_fatal ("Can't set ENBATCH|ENCOPYALL: %m");

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
		log_fatal ("Can't install packet filter program: %m");
        if (!quiet_interface_discovery)
		log_info ("Listening on UPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}

void if_deregister_receive (info)
	struct interface_info *info;
{
	close (info -> rfdesc);
	info -> rfdesc = -1;
        if (!quiet_interface_discovery)
		log_info ("Disabling input on UPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
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
	unsigned hbufp = 0, ibufp = 0;
	double hw [4];
	double ip [32];
	struct iovec iov [3];
	int result;
	int fudge;

	if (!strcmp (interface -> name, "fallback"))
		return send_fallback (interface, packet, raw,
				      len, from, to, hto);

	/* Assemble the headers... */
	assemble_hw_header (interface, (unsigned char *)hw, &hbufp, hto);
	assemble_udp_ip_header (interface,
				(unsigned char *)ip, &ibufp, from.s_addr,
				to -> sin_addr.s_addr, to -> sin_port,
				(unsigned char *)raw, len);

	/* Fire it off */
	iov [0].iov_base = ((char *)hw);
	iov [0].iov_len = hbufp;
	iov [1].iov_base = ((char *)ip);
	iov [1].iov_len = ibufp;
	iov [2].iov_base = (char *)raw;
	iov [2].iov_len = len;

	result = writev(interface -> wfdesc, iov, 3);
	if (result < 0)
		log_error ("send_packet: %m");
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

int can_unicast_without_arp (ip)
	struct interface_info *ip;
{
	return 1;
}

int can_receive_unicast_unconfigured (ip)
	struct interface_info *ip;
{
	return 1;
}

int supports_multiple_interfaces (ip)
	struct interface_info *ip;
{
	return 1;
}

void maybe_setup_fallback ()
{
	isc_result_t status;
	struct interface_info *fbi = (struct interface_info *)0;
	if (setup_fallback (&fbi, MDL)) {
		if_register_fallback (fbi);
		status = omapi_register_io_object ((omapi_object_t *)fbi,
						   if_readsocket, 0,
						   fallback_discard, 0, 0);
		if (status != ISC_R_SUCCESS)
			log_fatal ("Can't register I/O handle for %s: %s",
				   fbi -> name, isc_result_totext (status));
		interface_dereference (&fbi, MDL);
	}
}
#endif
