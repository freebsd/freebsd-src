/* bpf.c

   BPF socket interface code, originally contributed by Archie Cobbs. */

/*
 * Copyright (c) 1996-2000 Internet Software Consortium.
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
 * This software was contributed to the Internet Software Consortium
 * by Archie Cobbs, and is now maintained by Ted Lemon in cooperation
 * with Nominum, Inc.  To learn more about the Internet Software
 * Consortium, see ``http://www.isc.org/''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.  To learn more about
 * Nominum, Inc., see ``http://www.nominum.com''.
 *
 * Patches for FDDI support on Digital Unix were written by Bill
 * Stapleton, and maintained for a while by Mike Meredith before he
 * managed to get me to integrate them.
 */

#ifndef lint
static char copyright[] =
"$Id: bpf.c,v 1.48.2.2 2002/06/08 09:29:16 murray Exp $ Copyright (c) 1995-2000 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#if defined (USE_BPF_SEND) || defined (USE_BPF_RECEIVE)	\
				|| defined (USE_LPF_RECEIVE)
# if defined (USE_LPF_RECEIVE)
#  include <asm/types.h>
#  include <linux/filter.h>
#  define bpf_insn sock_filter /* Linux: dare to be gratuitously different. */
# else
#  include <sys/ioctl.h>
#  include <sys/uio.h>
#  include <net/bpf.h>
#  if defined (NEED_OSF_PFILT_HACKS)
#   include <net/pfilt.h>
#  endif
# endif

#include <netinet/in_systm.h>
#include "includes/netinet/ip.h"
#include "includes/netinet/udp.h"
#include "includes/netinet/if_ether.h"
#endif

/* Reinitializes the specified interface after an address change.   This
   is not required for packet-filter APIs. */

#ifdef USE_BPF_SEND
void if_reinitialize_send (info)
	struct interface_info *info;
{
}
#endif

#ifdef USE_BPF_RECEIVE
void if_reinitialize_receive (info)
	struct interface_info *info;
{
}
#endif

/* Called by get_interface_list for each interface that's discovered.
   Opens a packet filter for each interface and adds it to the select
   mask. */

#if defined (USE_BPF_SEND) || defined (USE_BPF_RECEIVE)
int if_register_bpf (info)
	struct interface_info *info;
{
	int sock;
	char filename[50];
	int b;

	/* Open a BPF device */
	for (b = 0; 1; b++) {
#ifndef NO_SNPRINTF
		snprintf(filename, sizeof(filename), BPF_FORMAT, b);
#else
		sprintf(filename, BPF_FORMAT, b);
#endif
		sock = open (filename, O_RDWR, 0);
		if (sock < 0) {
			if (errno == EBUSY) {
				continue;
			} else {
				if (!b)
					log_fatal ("No bpf devices.%s%s%s",
					       "   Please read the README",
					       " section for your operating",
					       " system.");
				log_fatal ("Can't find free bpf: %m");
			}
		} else {
			break;
		}
	}

	/* Set the BPF device to point at this interface. */
	if (ioctl (sock, BIOCSETIF, info -> ifp) < 0)
		log_fatal ("Can't attach interface %s to bpf device %s: %m",
		       info -> name, filename);

	return sock;
}
#endif /* USE_BPF_SEND || USE_BPF_RECEIVE */

#ifdef USE_BPF_SEND
void if_register_send (info)
	struct interface_info *info;
{
	/* If we're using the bpf API for sending and receiving,
	   we don't need to register this interface twice. */
#ifndef USE_BPF_RECEIVE
	info -> wfdesc = if_register_bpf (info, interface);
#else
	info -> wfdesc = info -> rfdesc;
#endif
	if (!quiet_interface_discovery)
		log_info ("Sending on   BPF/%s/%s%s%s",
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
	/* If we're using the bpf API for sending and receiving,
	   we don't need to register this interface twice. */
#ifndef USE_BPF_RECEIVE
	close (info -> wfdesc);
#endif
	info -> wfdesc = -1;

	if (!quiet_interface_discovery)
		log_info ("Disabling output on BPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}
#endif /* USE_BPF_SEND */

#if defined (USE_BPF_RECEIVE) || defined (USE_LPF_RECEIVE)
/* Packet filter program...
   XXX Changes to the filter program may require changes to the constant
   offsets used in if_register_send to patch the BPF program! XXX */

struct bpf_insn dhcp_bpf_filter [] = {
	/* Make sure this is an IP packet... */
	BPF_STMT (BPF_LD + BPF_H + BPF_ABS, 12),
	BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, ETHERTYPE_IP, 0, 8),

	/* Make sure it's a UDP packet... */
	BPF_STMT (BPF_LD + BPF_B + BPF_ABS, 23),
	BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, IPPROTO_UDP, 0, 6),

	/* Make sure this isn't a fragment... */
	BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 20),
	BPF_JUMP(BPF_JMP + BPF_JSET + BPF_K, 0x1fff, 4, 0),

	/* Get the IP header length... */
	BPF_STMT (BPF_LDX + BPF_B + BPF_MSH, 14),

	/* Make sure it's to the right port... */
	BPF_STMT (BPF_LD + BPF_H + BPF_IND, 16),
	BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, 67, 0, 1),             /* patch */

	/* If we passed all the tests, ask for the whole packet. */
	BPF_STMT(BPF_RET+BPF_K, (u_int)-1),

	/* Otherwise, drop it. */
	BPF_STMT(BPF_RET+BPF_K, 0),
};

#if defined (DEC_FDDI)
struct bpf_insn *bpf_fddi_filter;
#endif

int dhcp_bpf_filter_len = sizeof dhcp_bpf_filter / sizeof (struct bpf_insn);
#if defined (HAVE_TR_SUPPORT)
struct bpf_insn dhcp_bpf_tr_filter [] = {
        /* accept all token ring packets due to variable length header */
        /* if we want to get clever, insert the program here */

	/* If we passed all the tests, ask for the whole packet. */
	BPF_STMT(BPF_RET+BPF_K, (u_int)-1),

	/* Otherwise, drop it. */
	BPF_STMT(BPF_RET+BPF_K, 0),
};

int dhcp_bpf_tr_filter_len = (sizeof dhcp_bpf_tr_filter /
			      sizeof (struct bpf_insn));
#endif /* HAVE_TR_SUPPORT */
#endif /* USE_LPF_RECEIVE || USE_BPF_RECEIVE */

#if defined (USE_BPF_RECEIVE)
void if_register_receive (info)
	struct interface_info *info;
{
	int flag = 1;
	struct bpf_version v;
	u_int32_t addr;
	struct bpf_program p;
	u_int32_t bits;
#ifdef DEC_FDDI
	int link_layer;
#endif /* DEC_FDDI */

	/* Open a BPF device and hang it on this interface... */
	info -> rfdesc = if_register_bpf (info);

	/* Make sure the BPF version is in range... */
	if (ioctl (info -> rfdesc, BIOCVERSION, &v) < 0)
		log_fatal ("Can't get BPF version: %m");

	if (v.bv_major != BPF_MAJOR_VERSION ||
	    v.bv_minor < BPF_MINOR_VERSION)
		log_fatal ("BPF version mismatch - recompile DHCP!");

	/* Set immediate mode so that reads return as soon as a packet
	   comes in, rather than waiting for the input buffer to fill with
	   packets. */
	if (ioctl (info -> rfdesc, BIOCIMMEDIATE, &flag) < 0)
		log_fatal ("Can't set immediate mode on bpf device: %m");

#ifdef NEED_OSF_PFILT_HACKS
	/* Allow the copyall flag to be set... */
	if (ioctl(info -> rfdesc, EIOCALLOWCOPYALL, &flag) < 0)
		log_fatal ("Can't set ALLOWCOPYALL: %m");

	/* Clear all the packet filter mode bits first... */
	bits = 0;
	if (ioctl (info -> rfdesc, EIOCMBIS, &bits) < 0)
		log_fatal ("Can't clear pfilt bits: %m");

	/* Set the ENBATCH, ENCOPYALL, ENBPFHDR bits... */
	bits = ENBATCH | ENCOPYALL | ENBPFHDR;
	if (ioctl (info -> rfdesc, EIOCMBIS, &bits) < 0)
		log_fatal ("Can't set ENBATCH|ENCOPYALL|ENBPFHDR: %m");
#endif
	/* Get the required BPF buffer length from the kernel. */
	if (ioctl (info -> rfdesc, BIOCGBLEN, &info -> rbuf_max) < 0)
		log_fatal ("Can't get bpf buffer length: %m");
	info -> rbuf = dmalloc (info -> rbuf_max, MDL);
	if (!info -> rbuf)
		log_fatal ("Can't allocate %ld bytes for bpf input buffer.",
			   (long)(info -> rbuf_max));
	info -> rbuf_offset = 0;
	info -> rbuf_len = 0;

	/* Set up the bpf filter program structure. */
	p.bf_len = dhcp_bpf_filter_len;

#ifdef DEC_FDDI
	/* See if this is an FDDI interface, flag it for later. */
	if (ioctl(info -> rfdesc, BIOCGDLT, &link_layer) >= 0 &&
	    link_layer == DLT_FDDI) {
		if (!bpf_fddi_filter) {
			bpf_fddi_filter = dmalloc (sizeof bpf_fddi_filter,
						    MDL);
			if (!bpf_fddi_filter)
				log_fatal ("No memory for FDDI filter.");
			memcpy (bpf_fddi_filter,
				dhcp_bpf_filter, sizeof dhcp_bpf_filter);
			/* Patch the BPF program to account for the difference
			   in length between ethernet headers (14), FDDI and
			   802.2 headers (16 +8=24, +10).
			   XXX changes to filter program may require changes to
			   XXX the insn number(s) used below! */
			bpf_fddi_filter[0].k += 10;
			bpf_fddi_filter[2].k += 10;
			bpf_fddi_filter[4].k += 10;
			bpf_fddi_filter[6].k += 10;
			bpf_fddi_filter[7].k += 10;
		}
		p.bf_insns = bpf_fddi_filter;
	} else
#endif /* DEC_FDDI */
	p.bf_insns = dhcp_bpf_filter;

        /* Patch the server port into the BPF  program...
	   XXX changes to filter program may require changes
	   to the insn number(s) used below! XXX */
	dhcp_bpf_filter [8].k = ntohs (local_port);

	if (ioctl (info -> rfdesc, BIOCSETF, &p) < 0)
		log_fatal ("Can't install packet filter program: %m");
	if (!quiet_interface_discovery)
		log_info ("Listening on BPF/%s/%s%s%s",
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
		log_info ("Disabling input on BPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}
#endif /* USE_BPF_RECEIVE */

#ifdef USE_BPF_SEND
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
#endif /* USE_BPF_SEND */

#ifdef USE_BPF_RECEIVE
ssize_t receive_packet (interface, buf, len, from, hfrom)
	struct interface_info *interface;
	unsigned char *buf;
	size_t len;
	struct sockaddr_in *from;
	struct hardware *hfrom;
{
	int length = 0;
	int offset = 0;
	struct bpf_hdr hdr;

	/* All this complexity is because BPF doesn't guarantee
	   that only one packet will be returned at a time.   We're
	   getting what we deserve, though - this is a terrible abuse
	   of the BPF interface.   Sigh. */

	/* Process packets until we get one we can return or until we've
	   done a read and gotten nothing we can return... */

	do {
		/* If the buffer is empty, fill it. */
		if (interface -> rbuf_offset == interface -> rbuf_len) {
			length = read (interface -> rfdesc,
				       interface -> rbuf,
				       (size_t)interface -> rbuf_max);
			if (length <= 0) {
#ifdef __FreeBSD__
				if (errno == ENXIO) {
#else
				if (errno == EIO) {
#endif
					dhcp_interface_remove
						((omapi_object_t *)interface,
						 (omapi_object_t *)0);
				}
				return length;
			}
			interface -> rbuf_offset = 0;
			interface -> rbuf_len = BPF_WORDALIGN (length);
		}

		/* If there isn't room for a whole bpf header, something went
		   wrong, but we'll ignore it and hope it goes away... XXX */
		if (interface -> rbuf_len -
		    interface -> rbuf_offset < sizeof hdr) {
			interface -> rbuf_offset = interface -> rbuf_len;
			continue;
		}

		/* Copy out a bpf header... */
		memcpy (&hdr, &interface -> rbuf [interface -> rbuf_offset],
			sizeof hdr);

		/* If the bpf header plus data doesn't fit in what's left
		   of the buffer, stick head in sand yet again... */
		if (interface -> rbuf_offset +
		    hdr.bh_hdrlen + hdr.bh_caplen > interface -> rbuf_len) {
			interface -> rbuf_offset = interface -> rbuf_len;
			continue;
		}

		/* If the captured data wasn't the whole packet, or if
		   the packet won't fit in the input buffer, all we
		   can do is drop it. */
		if (hdr.bh_caplen != hdr.bh_datalen) {
			interface -> rbuf_offset =
				BPF_WORDALIGN (interface -> rbuf_offset +
					       hdr.bh_hdrlen + hdr.bh_caplen);
			continue;
		}

		/* Skip over the BPF header... */
		interface -> rbuf_offset += hdr.bh_hdrlen;

		/* Decode the physical header... */
		offset = decode_hw_header (interface,
					   interface -> rbuf,
					   interface -> rbuf_offset,
					   hfrom);

		/* If a physical layer checksum failed (dunno of any
		   physical layer that supports this, but WTH), skip this
		   packet. */
		if (offset < 0) {
			interface -> rbuf_offset = 
				BPF_WORDALIGN (interface -> rbuf_offset +
					       hdr.bh_caplen);
			continue;
		}
		interface -> rbuf_offset += offset;
		hdr.bh_caplen -= offset;

		/* Decode the IP and UDP headers... */
		offset = decode_udp_ip_header (interface,
					       interface -> rbuf,
					       interface -> rbuf_offset,
					       from,
					       (unsigned char *)0,
					       hdr.bh_caplen);

		/* If the IP or UDP checksum was bad, skip the packet... */
		if (offset < 0) {
			interface -> rbuf_offset = 
				BPF_WORDALIGN (interface -> rbuf_offset +
					       hdr.bh_caplen);
			continue;
		}
		interface -> rbuf_offset = interface -> rbuf_offset + offset;
		hdr.bh_caplen -= offset;

		/* If there's not enough room to stash the packet data,
		   we have to skip it (this shouldn't happen in real
		   life, though). */
		if (hdr.bh_caplen > len) {
			interface -> rbuf_offset =
				BPF_WORDALIGN (interface -> rbuf_offset +
					       hdr.bh_caplen);
			continue;
		}

		/* Copy out the data in the packet... */
		memcpy (buf, interface -> rbuf + interface -> rbuf_offset,
			hdr.bh_caplen);
		interface -> rbuf_offset =
			BPF_WORDALIGN (interface -> rbuf_offset +
				       hdr.bh_caplen);
		return hdr.bh_caplen;
	} while (!length);
	return 0;
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
