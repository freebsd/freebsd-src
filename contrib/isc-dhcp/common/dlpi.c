/* dlpi.c
 
   Data Link Provider Interface (DLPI) network interface code. */

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
 * This software was written for Internet Systems Consortium
 * by Eric James Negaard, <lmdejn@lmd.ericsson.se>.  To learn more about
 * Internet Systems Consortium, see ``http://www.isc.org''.
 *
 * Joost Mulders has also done considerable work in debugging the DLPI API
 * support on Solaris and getting this code to work properly on a variety
 * of different Solaris platforms.
 */

/*
 * Based largely in part to the existing NIT code in nit.c.
 *
 * This code has been developed and tested on sparc-based machines running
 * SunOS 5.5.1, with le and hme network interfaces.  It should be pretty
 * generic, though.
 */

/*
 * Implementation notes:
 *
 * I first tried to write this code to the "vanilla" DLPI 2.0 API.
 * It worked on a Sun Ultra-1 with a hme interface, but didn't work
 * on Sun SparcStation 5's with "le" interfaces (the packets sent out
 * via dlpiunitdatareq contained an Ethernet type of 0x0000 instead
 * of the expected 0x0800).
 *
 * Therefore I added the "DLPI_RAW" code which is a Sun extension to
 * the DLPI standard.  This code works on both of the above machines.
 * This is configurable in the OS-dependent include file by defining
 * USE_DLPI_RAW.
 *
 * It quickly became apparant that I should also use the "pfmod"
 * STREAMS module to cut down on the amount of user level packet
 * processing.  I don't know how widely available "pfmod" is, so it's
 * use is conditionally included. This is configurable in the
 * OS-dependent include file by defining USE_DLPI_PFMOD.
 *
 * A major quirk on the Sun's at least, is that no packets seem to get
 * sent out the interface until six seconds after the interface is
 * first "attached" to [per system reboot] (it's actually from when
 * the interface is attached, not when it is plumbed, so putting a
 * sleep into the dhclient-script at PREINIT time doesn't help).  I
 * HAVE tried, without success to poll the fd to see when it is ready
 * for writing.  This doesn't help at all. If the sleeps are not done,
 * the initial DHCPREQUEST or DHCPDISCOVER never gets sent out, so
 * I've put them here, when register_send and register_receive are
 * called (split up into two three-second sleeps between the notices,
 * so that it doesn't seem like so long when you're watching :-).  The
 * amount of time to sleep is configurable in the OS-dependent include
 * file by defining DLPI_FIRST_SEND_WAIT to be the number of seconds
 * to sleep.
 */

#ifndef lint
static char copyright[] =
"$Id: dlpi.c,v 1.28.2.2 2004/06/10 17:59:17 dhankins Exp $ Copyright (c) 2004 Internet Systems Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"

#if defined (USE_DLPI_SEND) || defined (USE_DLPI_RECEIVE)

# include <sys/ioctl.h>
# include <sys/time.h>
# include <sys/dlpi.h>
# include <stropts.h>
# ifdef USE_DLPI_PFMOD
#  include <sys/pfmod.h>
# endif
# ifdef USE_POLL
#  include <poll.h>
# endif

# include <netinet/in_systm.h>
# include "includes/netinet/ip.h"
# include "includes/netinet/udp.h"
# include "includes/netinet/if_ether.h"

# ifdef USE_DLPI_PFMOD
#  ifdef USE_DLPI_RAW
#   define DLPI_MODNAME "DLPI+RAW+PFMOD"
#  else
#   define DLPI_MODNAME "DLPI+PFMOD"
#  endif
# else
#  ifdef USE_DLPI_RAW
#   define DLPI_MODNAME "DLPI+RAW"
#  else
#   define DLPI_MODNAME "DLPI"
#  endif
# endif

# ifndef ABS
#  define ABS(x) ((x) >= 0 ? (x) : 0-(x))
# endif

static int strioctl PROTO ((int fd, int cmd, int timeout, int len, char *dp));

#define DLPI_MAXDLBUF		8192	/* Buffer size */
#define DLPI_MAXDLADDR		1024	/* Max address size */
#define DLPI_DEVDIR		"/dev/"	/* Device directory */

static int dlpiopen PROTO ((char *ifname));
static int dlpiunit PROTO ((char *ifname));
static int dlpiinforeq PROTO ((int fd));
static int dlpiphysaddrreq PROTO ((int fd, unsigned long addrtype));
static int dlpiattachreq PROTO ((int fd, unsigned long ppa));
static int dlpibindreq PROTO ((int fd, unsigned long sap, unsigned long max_conind,
			       unsigned long service_mode, unsigned long conn_mgmt,
			       unsigned long xidtest));
static int dlpidetachreq PROTO ((int fd));
static int dlpiunbindreq PROTO ((int fd));
static int dlpiokack PROTO ((int fd, char *bufp));
static int dlpiinfoack PROTO ((int fd, char *bufp));
static int dlpiphysaddrack PROTO ((int fd, char *bufp));
static int dlpibindack PROTO ((int fd, char *bufp));
static int dlpiunitdatareq PROTO ((int fd, unsigned char *addr,
				   int addrlen, unsigned long minpri,
				   unsigned long maxpri, unsigned char *data,
				   int datalen));
static int dlpiunitdataind PROTO ((int fd,
				   unsigned char *dstaddr,
				   unsigned long *dstaddrlen,
				   unsigned char *srcaddr,
				   unsigned long *srcaddrlen,
				   unsigned long *grpaddr,
				   unsigned char *data,
				   int datalen));

# ifndef USE_POLL
static void	sigalrm PROTO ((int sig));
# endif
static int	expected PROTO ((unsigned long prim, union DL_primitives *dlp,
				  int msgflags));
static int	strgetmsg PROTO ((int fd, struct strbuf *ctlp,
				  struct strbuf *datap, int *flagsp,
				  char *caller));

/* Reinitializes the specified interface after an address change.   This
   is not required for packet-filter APIs. */

#ifdef USE_DLPI_SEND
void if_reinitialize_send (info)
	struct interface_info *info;
{
}
#endif

#ifdef USE_DLPI_RECEIVE
void if_reinitialize_receive (info)
	struct interface_info *info;
{
}
#endif

/* Called by get_interface_list for each interface that's discovered.
   Opens a packet filter for each interface and adds it to the select
   mask. */

int if_register_dlpi (info)
	struct interface_info *info;
{
	int sock;
	int unit;
	long buf [DLPI_MAXDLBUF];
	union DL_primitives *dlp;

	dlp = (union DL_primitives *)buf;

	/* Open a DLPI device */
	if ((sock = dlpiopen (info -> name)) < 0) {
	    log_fatal ("Can't open DLPI device for %s: %m", info -> name);
	}


	/*
       * Submit a DL_INFO_REQ request, to find the dl_mac_type and 
         * dl_provider_style
	 */
	if (dlpiinforeq(sock) < 0 || dlpiinfoack(sock, (char *)buf) < 0) {
	    log_fatal ("Can't get DLPI MAC type for %s: %m", info -> name);
	} else {
	    switch (dlp -> info_ack.dl_mac_type) {
	      case DL_CSMACD: /* IEEE 802.3 */
	      case DL_ETHER:
		info -> hw_address.hbuf [0] = HTYPE_ETHER;
		break;
	      /* adding token ring 5/1999 - mayer@ping.at  */ 
	      case DL_TPR:
		info -> hw_address.hbuf [0] = HTYPE_IEEE802;
		break;
	      case DL_FDDI:
		info -> hw_address.hbuf [0] = HTYPE_FDDI;
		break;
	      default:
              log_fatal ("%s: unsupported DLPI MAC type %ld",
                     info -> name, dlp -> info_ack.dl_mac_type);
		break;
	    }
            /*
             * copy the sap length and broadcast address of this interface
             * to interface_info. This fixes nothing but seemed nicer than to
             * assume -2 and ffffff.
             */
            info -> dlpi_sap_length = dlp -> info_ack.dl_sap_length;
            info -> dlpi_broadcast_addr.hlen = 
             dlp -> info_ack.dl_brdcst_addr_length;
            memcpy (info -> dlpi_broadcast_addr.hbuf, 
             (char *)dlp + dlp -> info_ack.dl_brdcst_addr_offset, 
             dlp -> info_ack.dl_brdcst_addr_length);
	}

	if (dlp -> info_ack.dl_provider_style == DL_STYLE2) {
	    /*
	     * Attach to the device.  If this fails, the device
	     * does not exist.
	     */
	    unit = dlpiunit (info -> name);
	
	    if (dlpiattachreq (sock, unit) < 0
		|| dlpiokack (sock, (char *)buf) < 0) {
		log_fatal ("Can't attach DLPI device for %s: %m", info -> name);
	    }
	}

	/*
	 * Bind to the IP service access point (SAP), connectionless (CLDLS).
	 */
      if (dlpibindreq (sock, ETHERTYPE_IP, 0, DL_CLDLS, 0, 0) < 0
	    || dlpibindack (sock, (char *)buf) < 0) {
	    log_fatal ("Can't bind DLPI device for %s: %m", info -> name);
	}

	/*
	 * Submit a DL_PHYS_ADDR_REQ request, to find
	 * the hardware address
	 */
	if (dlpiphysaddrreq (sock, DL_CURR_PHYS_ADDR) < 0
	    || dlpiphysaddrack (sock, (char *)buf) < 0) {
	    log_fatal ("Can't get DLPI hardware address for %s: %m",
		   info -> name);
	}

	info -> hw_address.hlen = dlp -> physaddr_ack.dl_addr_length + 1;
	memcpy (&info -> hw_address.hbuf [1],
		(char *)buf + dlp -> physaddr_ack.dl_addr_offset,
		dlp -> physaddr_ack.dl_addr_length);

#ifdef USE_DLPI_RAW
	if (strioctl (sock, DLIOCRAW, INFTIM, 0, 0) < 0) {
	    log_fatal ("Can't set DLPI RAW mode for %s: %m",
		   info -> name);
	}
#endif

#ifdef USE_DLPI_PFMOD
	if (ioctl (sock, I_PUSH, "pfmod") < 0) {
	    log_fatal ("Can't push packet filter onto DLPI for %s: %m",
		   info -> name);
	}
#endif

	return sock;
}

static int
strioctl (fd, cmd, timeout, len, dp)
int fd;
int cmd;
int timeout;
int len;
char *dp;
{
    struct strioctl sio;
    int rslt;

    sio.ic_cmd = cmd;
    sio.ic_timout = timeout;
    sio.ic_len = len;
    sio.ic_dp = dp;

    if ((rslt = ioctl (fd, I_STR, &sio)) < 0) {
	return rslt;
    } else {
	return sio.ic_len;
    }
}

#ifdef USE_DLPI_SEND
void if_register_send (info)
	struct interface_info *info;
{
	/* If we're using the DLPI API for sending and receiving,
	   we don't need to register this interface twice. */
#ifndef USE_DLPI_RECEIVE
# ifdef USE_DLPI_PFMOD
	struct packetfilt pf;
# endif

	info -> wfdesc = if_register_dlpi (info);

# ifdef USE_DLPI_PFMOD
	/* Set up an PFMOD filter that rejects everything... */
	pf.Pf_Priority = 0;
	pf.Pf_FilterLen = 1;
	pf.Pf_Filter [0] = ENF_PUSHZERO;

	/* Install the filter */
	if (strioctl (info -> wfdesc, PFIOCSETF, INFTIM,
		      sizeof (pf), (char *)&pf) < 0) {
	    log_fatal ("Can't set PFMOD send filter on %s: %m", info -> name);
	}

# endif /* USE_DLPI_PFMOD */
#else /* !defined (USE_DLPI_RECEIVE) */
	/*
	 * If using DLPI for both send and receive, simply re-use
	 * the read file descriptor that was set up earlier.
	 */
	info -> wfdesc = info -> rfdesc;
#endif

        if (!quiet_interface_discovery)
		log_info ("Sending on   DLPI/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));

#ifdef DLPI_FIRST_SEND_WAIT
/* See the implementation notes at the beginning of this file */
# ifdef USE_DLPI_RECEIVE
	sleep (DLPI_FIRST_SEND_WAIT - (DLPI_FIRST_SEND_WAIT / 2));
# else
	sleep (DLPI_FIRST_SEND_WAIT);
# endif
#endif
}

void if_deregister_send (info)
	struct interface_info *info;
{
	/* If we're using the DLPI API for sending and receiving,
	   we don't need to register this interface twice. */
#ifndef USE_DLPI_RECEIVE
	close (info -> wfdesc);
#endif
	info -> wfdesc = -1;

        if (!quiet_interface_discovery)
		log_info ("Disabling output on DLPI/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}
#endif /* USE_DLPI_SEND */

#ifdef USE_DLPI_RECEIVE
/* Packet filter program...
   XXX Changes to the filter program may require changes to the constant
   offsets used in if_register_send to patch the NIT program! XXX */

void if_register_receive (info)
	struct interface_info *info;
{
#ifdef USE_DLPI_PFMOD
	struct packetfilt pf;
        struct ip iphdr;
        u_int16_t offset;
#endif

	/* Open a DLPI device and hang it on this interface... */
	info -> rfdesc = if_register_dlpi (info);

#ifdef USE_DLPI_PFMOD
	/* Set up the PFMOD filter program. */
	/* XXX Unlike the BPF filter program, this one won't work if the
	   XXX IP packet is fragmented or if there are options on the IP
	   XXX header. */
	pf.Pf_Priority = 0;
	pf.Pf_FilterLen = 0;

#if defined (USE_DLPI_RAW)
# define ETHER_H_PREFIX (14) /* sizeof (ethernet_header) */
    /*
     * ethertype == ETHERTYPE_IP
     */
    offset = 12;
    pf.Pf_Filter [pf.Pf_FilterLen++] = ENF_PUSHWORD + (offset / 2);
    pf.Pf_Filter [pf.Pf_FilterLen++] = ENF_PUSHLIT | ENF_CAND;
    pf.Pf_Filter [pf.Pf_FilterLen++] = htons (ETHERTYPE_IP);
# else
# define ETHER_H_PREFIX (0)
# endif /* USE_DLPI_RAW */
	/*
	 * The packets that will be received on this file descriptor
	 * will be IP packets (due to the SAP that was specified in
	 * the dlbind call).  There will be no ethernet header.
	 * Therefore, setup the packet filter to check the protocol
	 * field for UDP, and the destination port number equal
	 * to the local port.  All offsets are relative to the start
	 * of an IP packet.
	 */

        /*
         * BOOTPS destination port
         */
        offset = ETHER_H_PREFIX + sizeof (iphdr) + sizeof (u_int16_t);
        pf.Pf_Filter [pf.Pf_FilterLen++] = ENF_PUSHWORD + (offset / 2);
        pf.Pf_Filter [pf.Pf_FilterLen++] = ENF_PUSHLIT | ENF_CAND;
        pf.Pf_Filter [pf.Pf_FilterLen++] = local_port;

        /*
         * protocol should be udp. this is a byte compare, test for
         * endianess.
         */
        offset = ETHER_H_PREFIX + ((u_int8_t *)&(iphdr.ip_p) - (u_int8_t *)&iphdr);
        pf.Pf_Filter [pf.Pf_FilterLen++] = ENF_PUSHWORD + (offset / 2);
        pf.Pf_Filter [pf.Pf_FilterLen++] = ENF_PUSHLIT | ENF_AND;
        pf.Pf_Filter [pf.Pf_FilterLen++] = htons (0x00FF);
        pf.Pf_Filter [pf.Pf_FilterLen++] = ENF_PUSHLIT | ENF_CAND;
      pf.Pf_Filter [pf.Pf_FilterLen++] = htons (IPPROTO_UDP);

	/* Install the filter... */
	if (strioctl (info -> rfdesc, PFIOCSETF, INFTIM,
		      sizeof (pf), (char *)&pf) < 0) {
	    log_fatal ("Can't set PFMOD receive filter on %s: %m", info -> name);
	}
#endif /* USE_DLPI_PFMOD */

        if (!quiet_interface_discovery)
		log_info ("Listening on DLPI/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));

#ifdef DLPI_FIRST_SEND_WAIT
/* See the implementation notes at the beginning of this file */
# ifdef USE_DLPI_SEND
	sleep (DLPI_FIRST_SEND_WAIT / 2);
# else
	sleep (DLPI_FIRST_SEND_WAIT);
# endif
#endif
}

void if_deregister_receive (info)
	struct interface_info *info;
{
	/* If we're using the DLPI API for sending and receiving,
	   we don't need to register this interface twice. */
#ifndef USE_DLPI_SEND
	close (info -> rfdesc);
#endif
	info -> rfdesc = -1;

        if (!quiet_interface_discovery)
		log_info ("Disabling input on DLPI/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}
#endif /* USE_DLPI_RECEIVE */

#ifdef USE_DLPI_SEND
ssize_t send_packet (interface, packet, raw, len, from, to, hto)
	struct interface_info *interface;
	struct packet *packet;
	struct dhcp_packet *raw;
	size_t len;
	struct in_addr from;
	struct sockaddr_in *to;
	struct hardware *hto;
{
	unsigned hbufp = 0;
	double hh [32];
	double ih [1536 / sizeof (double)];
	unsigned char *dbuf = (unsigned char *)ih;
	unsigned dbuflen;
	unsigned char dstaddr [DLPI_MAXDLADDR];
	unsigned addrlen;
	int result;
	int fudge;

	if (!strcmp (interface -> name, "fallback"))
		return send_fallback (interface, packet, raw,
				      len, from, to, hto);

	dbuflen = 0;

	/* Assemble the headers... */
#ifdef USE_DLPI_RAW
	assemble_hw_header (interface, (unsigned char *)hh, &dbuflen, hto);
      if (dbuflen > sizeof hh)
              log_fatal ("send_packet: hh buffer too small.\n");
	fudge = dbuflen % 4; /* IP header must be word-aligned. */
	memcpy (dbuf + fudge, (unsigned char *)hh, dbuflen);
	dbuflen += fudge;
#else
	fudge = 0;
#endif
	assemble_udp_ip_header (interface, dbuf, &dbuflen, from.s_addr,
				to -> sin_addr.s_addr, to -> sin_port,
				(unsigned char *)raw, len);

	/* Copy the data into the buffer (yuk). */
	memcpy (dbuf + dbuflen, raw, len);
	dbuflen += len;

#ifdef USE_DLPI_RAW
	result = write (interface -> wfdesc, dbuf + fudge, dbuflen - fudge);
#else

	/*
         * Setup the destination address (DLSAP) in dstaddr 
         *
         * If sap_length < 0 we must deliver the DLSAP as phys+sap. 
         * If sap_length > 0 we must deliver the DLSAP as sap+phys.
         *
         * sap = Service Access Point == ETHERTYPE_IP
         * sap + datalink address is called DLSAP in dlpi speak.
         */
        { /* ENCODE DLSAP */
          unsigned char phys [DLPI_MAXDLADDR];
          unsigned char sap [4];
          int sap_len = interface -> dlpi_sap_length;
          int phys_len = interface -> hw_address.hlen - 1;

          /* sap = htons (ETHERTYPE_IP) kludge */
          memset (sap, 0, sizeof (sap));
# if (BYTE_ORDER == LITTLE_ENDIAN)
          sap [0] = 0x00;
          sap [1] = 0x08;
# else
          sap [0] = 0x08;
          sap [1] = 0x00;
# endif

        if (hto && hto -> hlen == interface -> hw_address.hlen)
             memcpy ( phys, (char *) &hto -> hbuf [1], phys_len);
          else 
             memcpy ( phys, interface -> dlpi_broadcast_addr.hbuf, 
              interface -> dlpi_broadcast_addr.hlen);
           
          if (sap_len < 0) { 
             memcpy ( dstaddr, phys, phys_len);
             memcpy ( (char *) &dstaddr [phys_len], sap, ABS (sap_len));
          }
          else {
             memcpy ( dstaddr, (void *) sap, sap_len);
             memcpy ( (char *) &dstaddr [sap_len], phys, phys_len);
          }
        addrlen = phys_len + ABS (sap_len);
      } /* ENCODE DLSAP */

	result = dlpiunitdatareq (interface -> wfdesc, dstaddr, addrlen,
				  0, 0, dbuf, dbuflen);
#endif /* USE_DLPI_RAW */
	if (result < 0)
		log_error ("send_packet: %m");
	return result;
}
#endif /* USE_DLPI_SEND */

#ifdef USE_DLPI_RECEIVE
ssize_t receive_packet (interface, buf, len, from, hfrom)
	struct interface_info *interface;
	unsigned char *buf;
	size_t len;
	struct sockaddr_in *from;
	struct hardware *hfrom;
{
	unsigned char dbuf [1536];
	unsigned char srcaddr [DLPI_MAXDLADDR];
	unsigned long srcaddrlen;
	int flags = 0;
	int length = 0;
	int offset = 0;
	int rslt;
	int bufix = 0;
	
#ifdef USE_DLPI_RAW
	length = read (interface -> rfdesc, dbuf, sizeof (dbuf));
#else	
	length = dlpiunitdataind (interface -> rfdesc, (unsigned char *)NULL,
				  (unsigned long *)NULL, srcaddr, &srcaddrlen,
				  (unsigned long *)NULL, dbuf, sizeof (dbuf));
#endif

	if (length <= 0) {
	    return length;
	}

# if !defined (USE_DLPI_RAW)
        /*
         * Copy the sender's hw address into hfrom
         * If sap_len < 0 the DLSAP is as phys+sap.
         * If sap_len > 0 the DLSAP is as sap+phys.
         *
         * sap is discarded here.
         */
        { /* DECODE DLSAP */
          int sap_len = interface -> dlpi_sap_length;
          int phys_len = interface -> hw_address.hlen - 1;

          if (hfrom && (srcaddrlen == ABS (sap_len) + phys_len )) {
            hfrom -> hbuf [0] = interface -> hw_address.hbuf [0];
            hfrom -> hlen = interface -> hw_address.hlen;
            
            if (sap_len < 0) {
              memcpy ((char *) &hfrom -> hbuf [1], srcaddr, phys_len);
            }
            else {
              memcpy ((char *) &hfrom -> hbuf [1], (char *) &srcaddr [phys_len],
                phys_len);
            }
          } 
          else if (hfrom) {
            memset (hfrom, '\0', sizeof *hfrom);
          }
        } /* DECODE_DLSAP */

# endif /* !defined (USE_DLPI_RAW) */

	/* Decode the IP and UDP headers... */
	bufix = 0;
#ifdef USE_DLPI_RAW
	/* Decode the physical header... */
	offset = decode_hw_header (interface, dbuf, bufix, hfrom);

	/* If a physical layer checksum failed (dunno of any
	   physical layer that supports this, but WTH), skip this
	   packet. */
	if (offset < 0) {
		return 0;
	}
	bufix += offset;
	length -= offset;
#endif
	offset = decode_udp_ip_header (interface, dbuf, bufix,
				       from, (unsigned char *)0, length);

	/* If the IP or UDP checksum was bad, skip the packet... */
	if (offset < 0) {
	    return 0;
	}

	bufix += offset;
	length -= offset;

	/* Copy out the data in the packet... */
	memcpy (buf, &dbuf [bufix], length);
	return length;
}
#endif

/* Common DLPI routines ...
 *
 * Written by Eric James Negaard, <lmdejn@lmd.ericsson.se>
 *
 * Based largely in part to the example code contained in the document
 * "How to Use the STREAMS Data Link Provider Interface (DLPI)", written
 * by Neal Nuckolls of SunSoft Internet Engineering.
 * 
 * This code has been developed and tested on sparc-based machines running
 * SunOS 5.5.1, with le and hme network interfaces.  It should be pretty
 * generic, though.
 * 
 * The usual disclaimers apply.  This code works for me.  Don't blame me
 * if it makes your machine or network go down in flames.  That taken
 * into consideration, use this code as you wish.  If you make usefull
 * modifications I'd appreciate hearing about it.
 */

#define DLPI_MAXWAIT		15	/* Max timeout */


/*
 * Parse an interface name and extract the unit number
 */

static int dlpiunit (ifname)
	char *ifname;
{
	int fd;
	char *cp, *dp, *ep;
	int unit;
	
	if (!ifname) {
		return 0;
	}
	
	/* Advance to the end of the name */
	cp = ifname;
	while (*cp) cp++;
	/* Back up to the start of the first digit */
	while ((*(cp-1) >= '0' && *(cp-1) <= '9') || *(cp - 1) == ':') cp--;
	
	/* Convert the unit number */
	unit = 0;
	while (*cp >= '0' && *cp <= '9') {
		unit *= 10;
		unit += (*cp++ - '0');
	}
	
	return unit;
}

/*
 * dlpiopen - open the DLPI device for a given interface name
 */
static int dlpiopen (ifname)
	char *ifname;
{
	char devname [50];
	char *cp, *dp, *ep;
	
	if (!ifname) {
		return -1;
	}
	
	/* Open a DLPI device */
	if (*ifname == '/') {
		dp = devname;
	} else {
		/* Prepend the device directory */
		memcpy (devname, DLPI_DEVDIR, strlen (DLPI_DEVDIR));
		dp = &devname [strlen (DLPI_DEVDIR)];
	}

	/* Find the end of the interface name */
	ep = cp = ifname;
	while (*ep)
		ep++;
	/* And back up to the first digit (unit number) */
	while ((*(ep - 1) >= '0' && *(ep - 1) <= '9') || *(ep - 1) == ':')
		ep--;
	
	/* Copy everything up to the unit number */
	while (cp < ep) {
		*dp++ = *cp++;
	}
	*dp = '\0';
	
	return open (devname, O_RDWR, 0);
}

/*
 * dlpiinforeq - request information about the data link provider.
 */

static int dlpiinforeq (fd)
	int fd;
{
	dl_info_req_t info_req;
	struct strbuf ctl;
	int flags;
	
	info_req.dl_primitive = DL_INFO_REQ;
	
	ctl.maxlen = 0;
	ctl.len = sizeof (info_req);
	ctl.buf = (char *)&info_req;
	
	flags = RS_HIPRI;
	
	return putmsg (fd, &ctl, (struct strbuf *)NULL, flags);
}

/*
 * dlpiphysaddrreq - request the current physical address.
 */
static int dlpiphysaddrreq (fd, addrtype)
	int fd;
	unsigned long addrtype;
{
	dl_phys_addr_req_t physaddr_req;
	struct strbuf ctl;
	int flags;
	
	physaddr_req.dl_primitive = DL_PHYS_ADDR_REQ;
	physaddr_req.dl_addr_type = addrtype;
	
	ctl.maxlen = 0;
	ctl.len = sizeof (physaddr_req);
	ctl.buf = (char *)&physaddr_req;
	
	flags = RS_HIPRI;
	
	return putmsg (fd, &ctl, (struct strbuf *)NULL, flags);
}

/*
 * dlpiattachreq - send a request to attach to a specific unit.
 */
static int dlpiattachreq (fd, ppa)
	unsigned long ppa;
	int fd;
{
	dl_attach_req_t	attach_req;
	struct strbuf ctl;
	int flags;
	
	attach_req.dl_primitive = DL_ATTACH_REQ;
	attach_req.dl_ppa = ppa;
	
	ctl.maxlen = 0;
	ctl.len = sizeof (attach_req);
	ctl.buf = (char *)&attach_req;
	
	flags = 0;
	
	return putmsg (fd, &ctl, (struct strbuf*)NULL, flags);
}

/*
 * dlpibindreq - send a request to bind to a specific SAP address.
 */
static int dlpibindreq (fd, sap, max_conind, service_mode, conn_mgmt, xidtest)
	unsigned long sap;
	unsigned long max_conind;
	unsigned long service_mode;
	unsigned long conn_mgmt;
	unsigned long xidtest;
	int fd;
{
	dl_bind_req_t bind_req;
	struct strbuf ctl;
	int flags;
	
	bind_req.dl_primitive = DL_BIND_REQ;
	bind_req.dl_sap = sap;
	bind_req.dl_max_conind = max_conind;
	bind_req.dl_service_mode = service_mode;
	bind_req.dl_conn_mgmt = conn_mgmt;
	bind_req.dl_xidtest_flg = xidtest;
	
	ctl.maxlen = 0;
	ctl.len = sizeof (bind_req);
	ctl.buf = (char *)&bind_req;
	
	flags = 0;
	
	return putmsg (fd, &ctl, (struct strbuf*)NULL, flags);
}

/*
 * dlpiunbindreq - send a request to unbind.
 */
static int dlpiunbindreq (fd)
	int fd;
{
	dl_unbind_req_t	unbind_req;
	struct strbuf ctl;
	int flags;
	
	unbind_req.dl_primitive = DL_UNBIND_REQ;
	
	ctl.maxlen = 0;
	ctl.len = sizeof (unbind_req);
	ctl.buf = (char *)&unbind_req;
	
	flags = 0;
	
	return putmsg (fd, &ctl, (struct strbuf*)NULL, flags);
}


/*
 * dlpidetachreq - send a request to detach.
 */
static int dlpidetachreq (fd)
	int fd;
{
	dl_detach_req_t	detach_req;
	struct strbuf ctl;
	int flags;
	
	detach_req.dl_primitive = DL_DETACH_REQ;
	
	ctl.maxlen = 0;
	ctl.len = sizeof (detach_req);
	ctl.buf = (char *)&detach_req;
	
	flags = 0;
	
	return putmsg (fd, &ctl, (struct strbuf*)NULL, flags);
}


/*
 * dlpibindack - receive an ack to a dlbindreq.
 */
static int dlpibindack (fd, bufp)
	char *bufp;
	int fd;
{
	union DL_primitives *dlp;
	struct strbuf ctl;
	int flags;
	
	ctl.maxlen = DLPI_MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;

	if (strgetmsg (fd, &ctl,
		       (struct strbuf*)NULL, &flags, "dlpibindack") < 0) {
		return -1;
	}
	
	dlp = (union DL_primitives *)ctl.buf;
	
	if (!expected (DL_BIND_ACK, dlp, flags) < 0) {
		return -1;
	}
	
	if (ctl.len < sizeof (dl_bind_ack_t)) {
		/* Returned structure is too short */
		return -1;
	}

	return 0;
}

/*
 * dlpiokack - general acknowledgement reception.
 */
static int dlpiokack (fd, bufp)
	char *bufp;
	int fd;
{
	union DL_primitives *dlp;
	struct strbuf ctl;
	int flags;
	
	ctl.maxlen = DLPI_MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;
	
	if (strgetmsg (fd, &ctl,
		       (struct strbuf*)NULL, &flags, "dlpiokack") < 0) {
		return -1;
	}
	
	dlp = (union DL_primitives *)ctl.buf;
	
	if (!expected (DL_OK_ACK, dlp, flags) < 0) {
		return -1;
	}
	
	if (ctl.len < sizeof (dl_ok_ack_t)) {
		/* Returned structure is too short */
		return -1;
	}
	
	return 0;
}

/*
 * dlpiinfoack - receive an ack to a dlinforeq.
 */
static int dlpiinfoack (fd, bufp)
	char *bufp;
	int fd;
{
	union DL_primitives *dlp;
	struct strbuf ctl;
	int flags;
	
	ctl.maxlen = DLPI_MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;
	
	if (strgetmsg (fd, &ctl, (struct strbuf *)NULL, &flags,
		       "dlpiinfoack") < 0) {
		return -1;
	}
	
	dlp = (union DL_primitives *) ctl.buf;
	
	if (!expected (DL_INFO_ACK, dlp, flags) < 0) {
		return -1;
	}
	
	if (ctl.len < sizeof (dl_info_ack_t)) {
		/* Returned structure is too short */
		return -1;
	}
	
	return 0;
}

/*
 * dlpiphysaddrack - receive an ack to a dlpiphysaddrreq.
 */
int dlpiphysaddrack (fd, bufp)
	char *bufp;
	int fd;
{
	union DL_primitives *dlp;
	struct strbuf ctl;
	int flags;
	
	ctl.maxlen = DLPI_MAXDLBUF;
	ctl.len = 0;
	ctl.buf = bufp;
	
	if (strgetmsg (fd, &ctl, (struct strbuf *)NULL, &flags,
		       "dlpiphysaddrack") < 0) {
		return -1;
	}

	dlp = (union DL_primitives *)ctl.buf;
	
	if (!expected (DL_PHYS_ADDR_ACK, dlp, flags) < 0) {
		return -1;
	}

	if (ctl.len < sizeof (dl_phys_addr_ack_t)) {
		/* Returned structure is too short */
		return -1;
	}
	
	return 0;
}

int dlpiunitdatareq (fd, addr, addrlen, minpri, maxpri, dbuf, dbuflen)
	int fd;
	unsigned char *addr;
	int addrlen;
	unsigned long minpri;
	unsigned long maxpri;
	unsigned char *dbuf;
	int dbuflen;
{
	long buf [DLPI_MAXDLBUF];
	union DL_primitives *dlp;
	struct strbuf ctl, data;
	
	/* Set up the control information... */
	dlp = (union DL_primitives *)buf;
	dlp -> unitdata_req.dl_primitive = DL_UNITDATA_REQ;
	dlp -> unitdata_req.dl_dest_addr_length = addrlen;
	dlp -> unitdata_req.dl_dest_addr_offset = sizeof (dl_unitdata_req_t);
	dlp -> unitdata_req.dl_priority.dl_min = minpri;
	dlp -> unitdata_req.dl_priority.dl_max = maxpri;

	/* Append the destination address */
	memcpy ((char *)buf + dlp -> unitdata_req.dl_dest_addr_offset,
		addr, addrlen);
	
	ctl.maxlen = 0;
	ctl.len = dlp -> unitdata_req.dl_dest_addr_offset + addrlen;
	ctl.buf = (char *)buf;

	data.maxlen = 0;
	data.buf = (char *)dbuf;
	data.len = dbuflen;

	/* Send the packet down the wire... */
	return putmsg (fd, &ctl, &data, 0);
}

static int dlpiunitdataind (fd, daddr, daddrlen,
			    saddr, saddrlen, grpaddr, dbuf, dlen)
	int fd;
	unsigned char *daddr;
	unsigned long *daddrlen;
	unsigned char *saddr;
	unsigned long *saddrlen;
	unsigned long *grpaddr;
	unsigned char *dbuf;
	int dlen;
{
	long buf [DLPI_MAXDLBUF];
	union DL_primitives *dlp;
	struct strbuf ctl, data;
	int flags = 0;
	int result;

	/* Set up the msg_buf structure... */
	dlp = (union DL_primitives *)buf;
	dlp -> unitdata_ind.dl_primitive = DL_UNITDATA_IND;

	ctl.maxlen = DLPI_MAXDLBUF;
	ctl.len = 0;
	ctl.buf = (char *)buf;
	
	data.maxlen = dlen;
	data.len = 0;
	data.buf = (char *)dbuf;
	
	result = getmsg (fd, &ctl, &data, &flags);
	
	if (result != 0) {
		return -1;
	}
	
	if (ctl.len < sizeof (dl_unitdata_ind_t) ||
	    dlp -> unitdata_ind.dl_primitive != DL_UNITDATA_IND) {
		return -1;
	}
	
	if (data.len <= 0) {
		return data.len;
	}

	/* Copy sender info */
	if (saddr) {
		memcpy (saddr,
			(char *)buf + dlp -> unitdata_ind.dl_src_addr_offset,
			dlp -> unitdata_ind.dl_src_addr_length);
	}
	if (saddrlen) {
		*saddrlen = dlp -> unitdata_ind.dl_src_addr_length;
	}

	/* Copy destination info */
	if (daddr) {
		memcpy (daddr,
			(char *)buf + dlp -> unitdata_ind.dl_dest_addr_offset,
			dlp -> unitdata_ind.dl_dest_addr_length);
	}
	if (daddrlen) {
		*daddrlen = dlp -> unitdata_ind.dl_dest_addr_length;
	}
	
	if (grpaddr) {
		*grpaddr = dlp -> unitdata_ind.dl_group_address;
	}
	
	return data.len;
}

/*
 * expected - see if we got what we wanted.
 */
static int expected (prim, dlp, msgflags)
	unsigned long prim;
	union DL_primitives *dlp;
	int msgflags;
{
	if (msgflags != RS_HIPRI) {
		/* Message was not M_PCPROTO */
		return 0;
	}

	if (dlp -> dl_primitive != prim) {
		/* Incorrect/unexpected return message */
		return 0;
	}
	
	return 1;
}

/*
 * strgetmsg - get a message from a stream, with timeout.
 */
static int strgetmsg (fd, ctlp, datap, flagsp, caller)
	struct strbuf *ctlp, *datap;
	char *caller;
	int *flagsp;
	int fd;
{
	int result;
#ifdef USE_POLL
	struct pollfd pfd;
	int count;
	time_t now;
	time_t starttime;
	int to_msec;
#endif
	
#ifdef USE_POLL
	pfd.fd = fd;
	pfd.events = POLLPRI;	/* We're only interested in knowing
				 * when we can receive the next high
				 * priority message.
				 */
	pfd.revents = 0;

	now = time (&starttime);
	while (now <= starttime + DLPI_MAXWAIT) {
		to_msec = ((starttime + DLPI_MAXWAIT) - now) * 1000;
		count = poll (&pfd, 1, to_msec);
		
		if (count == 0) {
			/* log_fatal ("strgetmsg: timeout"); */
			return -1;
		} else if (count < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				time (&now);
				continue;
			} else {
				/* log_fatal ("poll: %m"); */
				return -1;
			}
		} else {
			break;
		}
	}
#else  /* defined (USE_POLL) */
	/*
	 * Start timer.  Can't use select, since it might return true if there
	 * were non High-Priority data available on the stream.
	 */
	(void) sigset (SIGALRM, sigalrm);
	
	if (alarm (DLPI_MAXWAIT) < 0) {
		/* log_fatal ("alarm: %m"); */
		return -1;
	}
#endif /* !defined (USE_POLL) */

	/*
	 * Set flags argument and issue getmsg ().
	 */
	*flagsp = 0;
	if ((result = getmsg (fd, ctlp, datap, flagsp)) < 0) {
		return result;
	}

#ifndef USE_POLL
	/*
	 * Stop timer.
	 */	
	if (alarm (0) < 0) {
		/* log_fatal ("alarm: %m"); */
		return -1;
	}
#endif

	/*
	 * Check for MOREDATA and/or MORECTL.
	 */
	if (result & (MORECTL|MOREDATA)) {
		return -1;
	}

	/*
	 * Check for at least sizeof (long) control data portion.
	 */
	if (ctlp -> len < sizeof (long)) {
		return -1;
	}

	return 0;
}

#ifndef USE_POLL
/*
 * sigalrm - handle alarms.
 */
static void sigalrm (sig)
	int sig;
{
	fprintf (stderr, "strgetmsg: timeout");
	exit (1);
}
#endif /* !defined (USE_POLL) */

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
#endif /* USE_DLPI */
