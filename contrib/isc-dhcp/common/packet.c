/* packet.c

   Packet assembly code, originally contributed by Archie Cobbs. */

/*
 * Copyright (c) 1995, 1996, 1999 The Internet Software Consortium.
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
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#ifndef lint
static char copyright[] =
"$Id: packet.c,v 1.18.2.6 1999/06/10 00:58:16 mellon Exp $ Copyright (c) 1996 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

#if defined (PACKET_ASSEMBLY) || defined (PACKET_DECODING)
#include "includes/netinet/ip.h"
#include "includes/netinet/udp.h"
#endif /* PACKET_ASSEMBLY || PACKET_DECODING */

/* Compute the easy part of the checksum on a range of bytes. */

u_int32_t checksum (buf, nbytes, sum)
	unsigned char *buf;
	int nbytes;
	u_int32_t sum;
{
	int i;

#ifdef DEBUG_CHECKSUM
	debug ("checksum (%x %d %x)", buf, nbytes, sum);
#endif

	/* Checksum all the pairs of bytes first... */
	for (i = 0; i < (nbytes & ~1); i += 2) {
#ifdef DEBUG_CHECKSUM_VERBOSE
		debug ("sum = %x", sum);
#endif
		sum += (u_int16_t) ntohs(*((u_int16_t *)(buf + i)));
		/* Add carry. */
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}	

	/* If there's a single byte left over, checksum it, too.   Network
	   byte order is big-endian, so the remaining byte is the high byte. */
	if (i < nbytes) {
#ifdef DEBUG_CHECKSUM_VERBOSE
		debug ("sum = %x", sum);
#endif
		sum += buf [i] << 8;
		/* Add carry. */
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	
	return sum;
}

/* Finish computing the sum, and then put it into network byte order. */

u_int32_t wrapsum (sum)
	u_int32_t sum;
{
#ifdef DEBUG_CHECKSUM
	debug ("wrapsum (%x)", sum);
#endif

	sum = ~sum & 0xFFFF;
#ifdef DEBUG_CHECKSUM_VERBOSE
	debug ("sum = %x", sum);
#endif
	
#ifdef DEBUG_CHECKSUM
	debug ("wrapsum returns %x", htons (sum));
#endif
	return htons(sum);
}

#ifdef PACKET_ASSEMBLY
/* Assemble an hardware header... */
/* XXX currently only supports ethernet; doesn't check for other types. */

void assemble_hw_header (interface, buf, bufix, to)
	struct interface_info *interface;
	unsigned char *buf;
	int *bufix;
	struct hardware *to;
{
#if defined (HAVE_TR_SUPPORT)
	if (interface -> hw_address.htype == HTYPE_IEEE802)
		assemble_tr_header (interface, buf, bufix, to);
	else
#endif
		assemble_ethernet_header (interface, buf, bufix, to);

}

/* UDP header and IP header assembled together for convenience. */

void assemble_udp_ip_header (interface, buf, bufix,
			     from, to, port, data, len)
	struct interface_info *interface;
	unsigned char *buf;
	int *bufix;
	u_int32_t from;
	u_int32_t to;
	unsigned int port;
	unsigned char *data;
	int len;
{
	struct ip ip;
	struct udphdr udp;

	/* Fill out the IP header */
	ip.ip_v = 4;
	ip.ip_hl = 5;
	ip.ip_tos = IPTOS_LOWDELAY;
	ip.ip_len = htons(sizeof(ip) + sizeof(udp) + len);
	ip.ip_id = 0;
	ip.ip_off = 0;
	ip.ip_ttl = 16;
	ip.ip_p = IPPROTO_UDP;
	ip.ip_sum = 0;
	ip.ip_src.s_addr = from;
	ip.ip_dst.s_addr = to;
	
	/* Checksum the IP header... */
	ip.ip_sum = wrapsum (checksum ((unsigned char *)&ip, sizeof ip, 0));
	
	/* Copy the ip header into the buffer... */
	memcpy (&buf [*bufix], &ip, sizeof ip);
	*bufix += sizeof ip;

	/* Fill out the UDP header */
	udp.uh_sport = local_port;		/* XXX */
	udp.uh_dport = port;			/* XXX */
	udp.uh_ulen = htons(sizeof(udp) + len);
	memset (&udp.uh_sum, 0, sizeof udp.uh_sum);

	/* Compute UDP checksums, including the ``pseudo-header'', the UDP
	   header and the data. */

	udp.uh_sum =
		wrapsum (checksum ((unsigned char *)&udp, sizeof udp,
				   checksum (data, len, 
					     checksum ((unsigned char *)
						       &ip.ip_src,
						       2 * sizeof ip.ip_src,
						       IPPROTO_UDP +
						       (u_int32_t)
						       ntohs (udp.uh_ulen)))));

	/* Copy the udp header into the buffer... */
	memcpy (&buf [*bufix], &udp, sizeof udp);
	*bufix += sizeof udp;
}
#endif /* PACKET_ASSEMBLY */

#ifdef PACKET_DECODING
/* Decode a hardware header... */

ssize_t decode_hw_header (interface, buf, bufix, from)
     struct interface_info *interface;
     unsigned char *buf;
     int bufix;
     struct hardware *from;
{
#if defined (HAVE_TR_SUPPORT)
	if (interface -> hw_address.htype == HTYPE_IEEE802)
		return decode_tr_header (interface, buf, bufix, from);
	else
#endif
		return decode_ethernet_header (interface, buf, bufix, from);
}

/* UDP header and IP header decoded together for convenience. */

ssize_t decode_udp_ip_header (interface, buf, bufix, from, data, buflen)
	struct interface_info *interface;
	unsigned char *buf;
	int bufix;
	struct sockaddr_in *from;
	unsigned char *data;
	int buflen;
{
  struct ip *ip;
  struct udphdr *udp;
  u_int32_t ip_len = (buf [bufix] & 0xf) << 2;
  u_int32_t sum, usum;
  static int ip_packets_seen;
  static int ip_packets_bad_checksum;
  static int udp_packets_seen;
  static int udp_packets_bad_checksum;
  static int udp_packets_length_checked;
  static int udp_packets_length_overflow;
  int len;

  ip = (struct ip *)(buf + bufix);
  udp = (struct udphdr *)(buf + bufix + ip_len);

#ifdef USERLAND_FILTER
  /* Is it a UDP packet? */
  if (ip -> ip_p != IPPROTO_UDP)
	  return -1;

  /* Is it to the port we're serving? */
  if (udp -> uh_dport != local_port)
	  return -1;
#endif /* USERLAND_FILTER */

  /* Check the IP header checksum - it should be zero. */
  ++ip_packets_seen;
  if (wrapsum (checksum (buf + bufix, ip_len, 0))) {
	  ++ip_packets_bad_checksum;
	  if (ip_packets_seen > 4 &&
	      (ip_packets_seen / ip_packets_bad_checksum) < 2) {
		  note ("%d bad IP checksums seen in %d packets",
			ip_packets_bad_checksum, ip_packets_seen);
		  ip_packets_seen = ip_packets_bad_checksum = 0;
	  }
	  return -1;
  }

  /* Check the IP packet length. */
  if (ntohs (ip -> ip_len) != buflen)
	  debug ("ip length %d disagrees with bytes received %d.",
		 ntohs (ip -> ip_len), buflen);

  /* Copy out the IP source address... */
  memcpy (&from -> sin_addr, &ip -> ip_src, 4);

  /* Compute UDP checksums, including the ``pseudo-header'', the UDP
     header and the data.   If the UDP checksum field is zero, we're
     not supposed to do a checksum. */

  if (!data) {
	  data = buf + bufix + ip_len + sizeof *udp;
	  len = ntohs (udp -> uh_ulen) - sizeof *udp;
	  ++udp_packets_length_checked;
	  if (len + data > buf + bufix + buflen) {
		  ++udp_packets_length_overflow;
		  if (udp_packets_length_checked > 4 &&
		      (udp_packets_length_checked /
		       udp_packets_length_overflow) < 2) {
			  note ("%d udp packets in %d too long - dropped",
				udp_packets_length_overflow,
				udp_packets_length_checked);
			  udp_packets_length_overflow =
				  udp_packets_length_checked = 0;
		  }
		  return -1;
	  }
	  if (len + data != buf + bufix + buflen)
		  debug ("accepting packet with data after udp payload.");
  }

  usum = udp -> uh_sum;
  udp -> uh_sum = 0;

  sum = wrapsum (checksum ((unsigned char *)udp, sizeof *udp,
			   checksum (data, len,
				     checksum ((unsigned char *)
					       &ip -> ip_src,
					       2 * sizeof ip -> ip_src,
					       IPPROTO_UDP +
					       (u_int32_t)
					       ntohs (udp -> uh_ulen)))));

  udp_packets_seen++;
  if (usum && usum != sum) {
	  udp_packets_bad_checksum++;
	  if (udp_packets_seen > 4 &&
	      (udp_packets_seen / udp_packets_bad_checksum) < 2) {
		  note ("%d bad udp checksums in %d packets",
			udp_packets_bad_checksum, udp_packets_seen);
		  udp_packets_seen = udp_packets_bad_checksum = 0;
	  }
	  return -1;
  }

  /* Copy out the port... */
  memcpy (&from -> sin_port, &udp -> uh_sport, sizeof udp -> uh_sport);

  return ip_len + sizeof *udp;
}
#endif /* PACKET_DECODING */
