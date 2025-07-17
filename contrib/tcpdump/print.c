/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Support for splitting captures into multiple files with a maximum
 * file size:
 *
 * Copyright (c) 2001
 *	Seth Webster <swebster@sst.ll.mit.edu>
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "print.h"
#include "netdissect-alloc.h"

#include "pcap-missing.h"

struct printer {
	if_printer f;
	int type;
};

static const struct printer printers[] = {
#ifdef DLT_APPLE_IP_OVER_IEEE1394
	{ ap1394_if_print,	DLT_APPLE_IP_OVER_IEEE1394 },
#endif
	{ arcnet_if_print,	DLT_ARCNET },
#ifdef DLT_ARCNET_LINUX
	{ arcnet_linux_if_print, DLT_ARCNET_LINUX },
#endif
	{ atm_if_print,		DLT_ATM_RFC1483 },
#ifdef DLT_DSA_TAG_BRCM
	{ brcm_tag_if_print,	DLT_DSA_TAG_BRCM },
#endif
#ifdef DLT_DSA_TAG_BRCM_PREPEND
	{ brcm_tag_prepend_if_print, DLT_DSA_TAG_BRCM_PREPEND },
#endif
#ifdef DLT_BLUETOOTH_HCI_H4_WITH_PHDR
	{ bt_if_print,		DLT_BLUETOOTH_HCI_H4_WITH_PHDR},
#endif
#ifdef DLT_C_HDLC
	{ chdlc_if_print,	DLT_C_HDLC },
#endif
#ifdef DLT_HDLC
	{ chdlc_if_print,	DLT_HDLC },
#endif
#ifdef DLT_ATM_CLIP
	{ cip_if_print,		DLT_ATM_CLIP },
#endif
#ifdef DLT_CIP
	{ cip_if_print,		DLT_CIP },
#endif
#ifdef DLT_DSA_TAG_DSA
	{ dsa_if_print,		DLT_DSA_TAG_DSA },
#endif
#ifdef DLT_DSA_TAG_EDSA
	{ edsa_if_print,	DLT_DSA_TAG_EDSA },
#endif
#ifdef DLT_ENC
	{ enc_if_print,		DLT_ENC },
#endif
	{ ether_if_print,	DLT_EN10MB },
	{ fddi_if_print,	DLT_FDDI },
#ifdef DLT_FR
	{ fr_if_print,		DLT_FR },
#endif
#ifdef DLT_FRELAY
	{ fr_if_print,		DLT_FRELAY },
#endif
#ifdef DLT_IEEE802_11
	{ ieee802_11_if_print,	DLT_IEEE802_11},
#endif
#ifdef DLT_IEEE802_11_RADIO_AVS
	{ ieee802_11_radio_avs_if_print, DLT_IEEE802_11_RADIO_AVS },
#endif
#ifdef DLT_IEEE802_11_RADIO
	{ ieee802_11_radio_if_print,	DLT_IEEE802_11_RADIO },
#endif
#ifdef DLT_IEEE802_15_4
	{ ieee802_15_4_if_print, DLT_IEEE802_15_4 },
#endif
#ifdef DLT_IEEE802_15_4_NOFCS
	{ ieee802_15_4_if_print, DLT_IEEE802_15_4_NOFCS },
#endif
#ifdef DLT_IEEE802_15_4_TAP
	{ ieee802_15_4_tap_if_print, DLT_IEEE802_15_4_TAP },
#endif
#ifdef DLT_IP_OVER_FC
	{ ipfc_if_print,	DLT_IP_OVER_FC },
#endif
#ifdef DLT_IPNET
	{ ipnet_if_print,	DLT_IPNET },
#endif
#ifdef DLT_IPOIB
	{ ipoib_if_print,       DLT_IPOIB },
#endif
#ifdef DLT_JUNIPER_ATM1
	{ juniper_atm1_if_print, DLT_JUNIPER_ATM1 },
#endif
#ifdef DLT_JUNIPER_ATM2
	{ juniper_atm2_if_print, DLT_JUNIPER_ATM2 },
#endif
#ifdef DLT_JUNIPER_CHDLC
	{ juniper_chdlc_if_print,	DLT_JUNIPER_CHDLC },
#endif
#ifdef DLT_JUNIPER_ES
	{ juniper_es_if_print,	DLT_JUNIPER_ES },
#endif
#ifdef DLT_JUNIPER_ETHER
	{ juniper_ether_if_print,	DLT_JUNIPER_ETHER },
#endif
#ifdef DLT_JUNIPER_FRELAY
	{ juniper_frelay_if_print,	DLT_JUNIPER_FRELAY },
#endif
#ifdef DLT_JUNIPER_GGSN
	{ juniper_ggsn_if_print, DLT_JUNIPER_GGSN },
#endif
#ifdef DLT_JUNIPER_MFR
	{ juniper_mfr_if_print,	DLT_JUNIPER_MFR },
#endif
#ifdef DLT_JUNIPER_MLFR
	{ juniper_mlfr_if_print, DLT_JUNIPER_MLFR },
#endif
#ifdef DLT_JUNIPER_MLPPP
	{ juniper_mlppp_if_print, DLT_JUNIPER_MLPPP },
#endif
#ifdef DLT_JUNIPER_MONITOR
	{ juniper_monitor_if_print, DLT_JUNIPER_MONITOR },
#endif
#ifdef DLT_JUNIPER_PPP
	{ juniper_ppp_if_print,	DLT_JUNIPER_PPP },
#endif
#ifdef DLT_JUNIPER_PPPOE_ATM
	{ juniper_pppoe_atm_if_print, DLT_JUNIPER_PPPOE_ATM },
#endif
#ifdef DLT_JUNIPER_PPPOE
	{ juniper_pppoe_if_print, DLT_JUNIPER_PPPOE },
#endif
#ifdef DLT_JUNIPER_SERVICES
	{ juniper_services_if_print, DLT_JUNIPER_SERVICES },
#endif
#ifdef DLT_LTALK
	{ ltalk_if_print,	DLT_LTALK },
#endif
#ifdef DLT_MFR
	{ mfr_if_print,		DLT_MFR },
#endif
#ifdef DLT_NETANALYZER
	{ netanalyzer_if_print, DLT_NETANALYZER },
#endif
#ifdef DLT_NETANALYZER_TRANSPARENT
	{ netanalyzer_transparent_if_print, DLT_NETANALYZER_TRANSPARENT },
#endif
#ifdef DLT_NFLOG
	{ nflog_if_print,	DLT_NFLOG},
#endif
	{ null_if_print,	DLT_NULL },
#ifdef DLT_LOOP
	{ null_if_print,	DLT_LOOP },
#endif
#if defined(DLT_PFLOG) && defined(HAVE_NET_IF_PFLOG_H)
	{ pflog_if_print,	DLT_PFLOG },
#endif
#if defined(DLT_PFSYNC) && defined(HAVE_NET_PFVAR_H)
	{ pfsync_if_print,	DLT_PFSYNC},
#endif
#ifdef DLT_PKTAP
	{ pktap_if_print,	DLT_PKTAP },
#endif
#ifdef DLT_PPI
	{ ppi_if_print,		DLT_PPI },
#endif
#ifdef DLT_PPP_BSDOS
	{ ppp_bsdos_if_print,	DLT_PPP_BSDOS },
#endif
#ifdef DLT_PPP_SERIAL
	{ ppp_hdlc_if_print,	DLT_PPP_SERIAL },
#endif
	{ ppp_if_print,		DLT_PPP },
#ifdef DLT_PPP_PPPD
	{ ppp_if_print,		DLT_PPP_PPPD },
#endif
#ifdef DLT_PPP_ETHER
	{ pppoe_if_print,	DLT_PPP_ETHER },
#endif
#ifdef DLT_PRISM_HEADER
	{ prism_if_print,	DLT_PRISM_HEADER },
#endif
	{ raw_if_print,		DLT_RAW },
#ifdef DLT_IPV4
	{ raw_if_print,		DLT_IPV4 },
#endif
#ifdef DLT_IPV6
	{ raw_if_print,		DLT_IPV6 },
#endif
#ifdef DLT_SLIP_BSDOS
	{ sl_bsdos_if_print,	DLT_SLIP_BSDOS },
#endif
	{ sl_if_print,		DLT_SLIP },
#ifdef DLT_LINUX_SLL
	{ sll_if_print,		DLT_LINUX_SLL },
#endif
#ifdef DLT_LINUX_SLL2
	{ sll2_if_print,	DLT_LINUX_SLL2 },
#endif
#ifdef DLT_SUNATM
	{ sunatm_if_print,	DLT_SUNATM },
#endif
#ifdef DLT_SYMANTEC_FIREWALL
	{ symantec_if_print,	DLT_SYMANTEC_FIREWALL },
#endif
	{ token_if_print,	DLT_IEEE802 },
#ifdef DLT_USB_LINUX
	{ usb_linux_48_byte_if_print, DLT_USB_LINUX},
#endif /* DLT_USB_LINUX */
#ifdef DLT_USB_LINUX_MMAPPED
	{ usb_linux_64_byte_if_print, DLT_USB_LINUX_MMAPPED},
#endif /* DLT_USB_LINUX_MMAPPED */
#ifdef DLT_VSOCK
	{ vsock_if_print,	DLT_VSOCK },
#endif
	{ NULL,                 0 },
};

static void	ndo_default_print(netdissect_options *ndo, const u_char *bp,
		    u_int length);

static void NORETURN ndo_error(netdissect_options *ndo,
		     status_exit_codes_t status,
		     FORMAT_STRING(const char *fmt), ...)
		     PRINTFLIKE(3, 4);
static void	ndo_warning(netdissect_options *ndo,
		    FORMAT_STRING(const char *fmt), ...)
		    PRINTFLIKE(2, 3);

static int	ndo_printf(netdissect_options *ndo,
		     FORMAT_STRING(const char *fmt), ...)
		     PRINTFLIKE(2, 3);

void
init_print(netdissect_options *ndo, uint32_t localnet, uint32_t mask)
{
	init_addrtoname(ndo, localnet, mask);
}

if_printer
lookup_printer(int type)
{
	const struct printer *p;

	for (p = printers; p->f; ++p)
		if (type == p->type)
			return p->f;

#if defined(DLT_USER2) && defined(DLT_PKTAP)
	/*
	 * Apple incorrectly chose to use DLT_USER2 for their PKTAP
	 * header.
	 *
	 * We map DLT_PKTAP, whether it's DLT_USER2 as it is on Darwin-
	 * based OSes or the same value as LINKTYPE_PKTAP as it is on
	 * other OSes, to LINKTYPE_PKTAP, so files written with
	 * this version of libpcap for a DLT_PKTAP capture have a link-
	 * layer header type of LINKTYPE_PKTAP.
	 *
	 * However, files written on OS X Mavericks for a DLT_PKTAP
	 * capture have a link-layer header type of LINKTYPE_USER2.
	 * If we don't have a printer for DLT_USER2, and type is
	 * DLT_USER2, we look up the printer for DLT_PKTAP and use
	 * that.
	 */
	if (type == DLT_USER2) {
		for (p = printers; p->f; ++p)
			if (DLT_PKTAP == p->type)
				return p->f;
	}
#endif

	return NULL;
	/* NOTREACHED */
}

int
has_printer(int type)
{
	return (lookup_printer(type) != NULL);
}

if_printer
get_if_printer(int type)
{
	if_printer printer;

	printer = lookup_printer(type);
	if (printer == NULL)
		printer = unsupported_if_print;
	return printer;
}

#ifdef ENABLE_INSTRUMENT_FUNCTIONS
extern int profile_func_level;
static int pretty_print_packet_level = -1;
#endif

void
pretty_print_packet(netdissect_options *ndo, const struct pcap_pkthdr *h,
		    const u_char *sp, u_int packets_captured)
{
	u_int hdrlen = 0;
	int invalid_header = 0;

#ifdef ENABLE_INSTRUMENT_FUNCTIONS
	if (pretty_print_packet_level == -1)
		pretty_print_packet_level = profile_func_level;
#endif

	if (ndo->ndo_packet_number)
		ND_PRINT("%5u  ", packets_captured);

	/* Sanity checks on packet length / capture length */
	if (h->caplen == 0) {
		invalid_header = 1;
		ND_PRINT("[Invalid header: caplen==0");
	}
	if (h->len == 0) {
		if (!invalid_header) {
			invalid_header = 1;
			ND_PRINT("[Invalid header:");
		} else
			ND_PRINT(",");
		ND_PRINT(" len==0");
	} else if (h->len < h->caplen) {
		if (!invalid_header) {
			invalid_header = 1;
			ND_PRINT("[Invalid header:");
		} else
			ND_PRINT(",");
		ND_PRINT(" len(%u) < caplen(%u)", h->len, h->caplen);
	}
	if (h->caplen > MAXIMUM_SNAPLEN) {
		if (!invalid_header) {
			invalid_header = 1;
			ND_PRINT("[Invalid header:");
		} else
			ND_PRINT(",");
		ND_PRINT(" caplen(%u) > %u", h->caplen, MAXIMUM_SNAPLEN);
	}
	if (h->len > MAXIMUM_SNAPLEN) {
		if (!invalid_header) {
			invalid_header = 1;
			ND_PRINT("[Invalid header:");
		} else
			ND_PRINT(",");
		ND_PRINT(" len(%u) > %u", h->len, MAXIMUM_SNAPLEN);
	}
	if (invalid_header) {
		ND_PRINT("]\n");
		return;
	}

	/*
	 * At this point:
	 *   capture length != 0,
	 *   packet length != 0,
	 *   capture length <= MAXIMUM_SNAPLEN,
	 *   packet length <= MAXIMUM_SNAPLEN,
	 *   packet length >= capture length.
	 *
	 * Currently, there is no D-Bus printer, thus no need for
	 * bigger lengths.
	 */

	/*
	 * The header /usr/include/pcap/pcap.h in OpenBSD declares h->ts as
	 * struct bpf_timeval, not struct timeval. The former comes from
	 * /usr/include/net/bpf.h and uses 32-bit unsigned types instead of
	 * the types used in struct timeval.
	 */
	struct timeval tvbuf;
	tvbuf.tv_sec = h->ts.tv_sec;
	tvbuf.tv_usec = h->ts.tv_usec;
	ts_print(ndo, &tvbuf);

	/*
	 * Printers must check that they're not walking off the end of
	 * the packet.
	 * Rather than pass it all the way down, we set this member
	 * of the netdissect_options structure.
	 */
	ndo->ndo_snapend = sp + h->caplen;
	ndo->ndo_packetp = sp;

	ndo->ndo_protocol = "";
	ndo->ndo_ll_hdr_len = 0;
	switch (setjmp(ndo->ndo_early_end)) {
	case 0:
		/* Print the packet. */
		(ndo->ndo_if_printer)(ndo, h, sp);
		break;
	case ND_TRUNCATED:
		/* A printer quit because the packet was truncated; report it */
		nd_print_trunc(ndo);
		/* Print the full packet */
		ndo->ndo_ll_hdr_len = 0;
#ifdef ENABLE_INSTRUMENT_FUNCTIONS
		/* truncation => reassignment */
		profile_func_level = pretty_print_packet_level;
#endif
		break;
	}
	hdrlen = ndo->ndo_ll_hdr_len;

	/*
	 * Empty the stack of packet information, freeing all pushed buffers;
	 * if we got here by a printer quitting, we need to release anything
	 * that didn't get released because we longjmped out of the code
	 * before it popped the packet information.
	 */
	nd_pop_all_packet_info(ndo);

	/*
	 * Restore the originals snapend and packetp, as a printer
	 * might have changed them.
	 *
	 * XXX - nd_pop_all_packet_info() should have restored the
	 * original values, but, just in case....
	 */
	ndo->ndo_snapend = sp + h->caplen;
	ndo->ndo_packetp = sp;
	if (ndo->ndo_Xflag) {
		/*
		 * Print the raw packet data in hex and ASCII.
		 */
		if (ndo->ndo_Xflag > 1) {
			/*
			 * Include the link-layer header.
			 */
			hex_and_ascii_print(ndo, "\n\t", sp, h->caplen);
		} else {
			/*
			 * Don't include the link-layer header - and if
			 * we have nothing past the link-layer header,
			 * print nothing.
			 */
			if (h->caplen > hdrlen)
				hex_and_ascii_print(ndo, "\n\t", sp + hdrlen,
						    h->caplen - hdrlen);
		}
	} else if (ndo->ndo_xflag) {
		/*
		 * Print the raw packet data in hex.
		 */
		if (ndo->ndo_xflag > 1) {
			/*
			 * Include the link-layer header.
			 */
			hex_print(ndo, "\n\t", sp, h->caplen);
		} else {
			/*
			 * Don't include the link-layer header - and if
			 * we have nothing past the link-layer header,
			 * print nothing.
			 */
			if (h->caplen > hdrlen)
				hex_print(ndo, "\n\t", sp + hdrlen,
					  h->caplen - hdrlen);
		}
	} else if (ndo->ndo_Aflag) {
		/*
		 * Print the raw packet data in ASCII.
		 */
		if (ndo->ndo_Aflag > 1) {
			/*
			 * Include the link-layer header.
			 */
			ascii_print(ndo, sp, h->caplen);
		} else {
			/*
			 * Don't include the link-layer header - and if
			 * we have nothing past the link-layer header,
			 * print nothing.
			 */
			if (h->caplen > hdrlen)
				ascii_print(ndo, sp + hdrlen, h->caplen - hdrlen);
		}
	}

	ND_PRINT("\n");
	nd_free_all(ndo);
}

/*
 * By default, print the specified data out in hex and ASCII.
 */
static void
ndo_default_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
	hex_and_ascii_print(ndo, "\n\t", bp, length); /* pass on lf and indentation string */
}

/* VARARGS */
static void
ndo_error(netdissect_options *ndo, status_exit_codes_t status,
	  const char *fmt, ...)
{
	va_list ap;

	if (ndo->program_name)
		(void)fprintf(stderr, "%s: ", ndo->program_name);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
	nd_cleanup();
	exit(status);
	/* NOTREACHED */
}

/* VARARGS */
static void
ndo_warning(netdissect_options *ndo, const char *fmt, ...)
{
	va_list ap;

	if (ndo->program_name)
		(void)fprintf(stderr, "%s: ", ndo->program_name);
	(void)fprintf(stderr, "WARNING: ");
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
}

static int
ndo_printf(netdissect_options *ndo, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vfprintf(stdout, fmt, args);
	va_end(args);

	if (ret < 0)
		ndo_error(ndo, S_ERR_ND_WRITE_FILE,
			  "Unable to write output: %s", pcap_strerror(errno));
	return (ret);
}

void
ndo_set_function_pointers(netdissect_options *ndo)
{
	ndo->ndo_default_print=ndo_default_print;
	ndo->ndo_printf=ndo_printf;
	ndo->ndo_error=ndo_error;
	ndo->ndo_warning=ndo_warning;
}
