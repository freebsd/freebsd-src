/*
 * Copyright (c) 1988-1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1998-2012  Michael Richardson <mcr@tcpdump.org>
 *      The TCPDUMP project
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
 */

#ifndef netdissect_h
#define netdissect_h

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif
#include <sys/types.h>
#include <setjmp.h>
#include "status-exit-codes.h"
#include "funcattrs.h" /* for PRINTFLIKE_FUNCPTR() */
#include "diag-control.h" /* for ND_UNREACHABLE */

/*
 * Data types corresponding to multi-byte integral values within data
 * structures.  These are defined as arrays of octets, so that they're
 * not aligned on their "natural" boundaries, and so that you *must*
 * use the EXTRACT_ macros to extract them (which you should be doing
 * *anyway*, so as not to assume a particular byte order or alignment
 * in your code).
 *
 * We even want EXTRACT_U_1 used for 8-bit integral values, so we
 * define nd_uint8_t and nd_int8_t as arrays as well.
 */
typedef unsigned char nd_uint8_t[1];
typedef unsigned char nd_uint16_t[2];
typedef unsigned char nd_uint24_t[3];
typedef unsigned char nd_uint32_t[4];
typedef unsigned char nd_uint40_t[5];
typedef unsigned char nd_uint48_t[6];
typedef unsigned char nd_uint56_t[7];
typedef unsigned char nd_uint64_t[8];

typedef signed char nd_int8_t[1];

/*
 * "unsigned char" so that sign extension isn't done on the
 * individual bytes while they're being assembled.
 */
typedef unsigned char nd_int32_t[4];
typedef unsigned char nd_int64_t[8];

#define	FMAXINT	(4294967296.0)	/* floating point rep. of MAXINT */

/*
 * Use this for IPv4 addresses and netmasks.
 *
 * It's defined as an array of octets, so that it's not guaranteed to
 * be aligned on its "natural" boundary (in some packet formats, it
 * *isn't* so aligned).  We have separate EXTRACT_ calls for them;
 * sometimes you want the host-byte-order value, other times you want
 * the network-byte-order value.
 *
 * Don't use EXTRACT_BE_U_4() on them, use EXTRACT_IPV4_TO_HOST_ORDER()
 * if you want them in host byte order and EXTRACT_IPV4_TO_NETWORK_ORDER()
 * if you want them in network byte order (which you want with system APIs
 * that expect network-order IPv4 addresses, such as inet_ntop()).
 *
 * If, on your little-endian machine (e.g., an "IBM-compatible PC", no matter
 * what the OS, or an Intel Mac, no matter what the OS), you get the wrong
 * answer, and you've used EXTRACT_BE_U_4(), do *N*O*T* "fix" this by using
 * EXTRACT_LE_U_4(), fix it by using EXTRACT_IPV4_TO_NETWORK_ORDER(),
 * otherwise you're breaking the result on big-endian machines (e.g.,
 * most PowerPC/Power ISA machines, System/390 and z/Architecture, SPARC,
 * etc.).
 *
 * Yes, people do this; that's why Wireshark has tvb_get_ipv4(), to extract
 * an IPv4 address from a packet data buffer; it was introduced in reaction
 * to somebody who *had* done that.
 */
typedef unsigned char nd_ipv4[4];

/*
 * Use this for IPv6 addresses and netmasks.
 */
typedef unsigned char nd_ipv6[16];

/*
 * Use this for MAC addresses.
 */
#define MAC_ADDR_LEN	6U		/* length of MAC addresses */
typedef unsigned char nd_mac_addr[MAC_ADDR_LEN];

/*
 * Use this for blobs of bytes; make them arrays of nd_byte.
 */
typedef unsigned char nd_byte;

/*
 * Round up x to a multiple of y; y must be a power of 2.
 */
#ifndef roundup2
#define	roundup2(x, y)	(((x)+((u_int)((y)-1)))&(~((u_int)((y)-1))))
#endif

#include <stdarg.h>
#include <pcap.h>

#include "ip.h" /* struct ip for nextproto4_cksum() */
#include "ip6.h" /* struct ip6 for nextproto6_cksum() */

#ifndef HAVE_STRLCAT
extern size_t strlcat (char *, const char *, size_t);
#endif
#ifndef HAVE_STRLCPY
extern size_t strlcpy (char *, const char *, size_t);
#endif

#ifndef HAVE_STRDUP
extern char *strdup (const char *str);
#endif

#ifndef HAVE_STRSEP
extern char *strsep(char **, const char *);
#endif

struct tok {
	u_int v;		/* value */
	const char *s;		/* string */
};

/* tok2str is deprecated */
extern const char *tok2str(const struct tok *, const char *, u_int);
extern char *bittok2str(const struct tok *, const char *, u_int);
extern char *bittok2str_nosep(const struct tok *, const char *, u_int);

/* Initialize netdissect. */
extern int nd_init(char *, size_t);
/* Clean up netdissect. */
extern void nd_cleanup(void);

/* Do we have libsmi support? */
extern int nd_have_smi_support(void);
/* Load an SMI module. */
extern int nd_load_smi_module(const char *, char *, size_t);
/* Flag indicating whether an SMI module has been loaded. */
extern int nd_smi_module_loaded;
/* Version number of the SMI library, or NULL if we don't have libsmi support. */
extern const char *nd_smi_version_string(void);

typedef struct netdissect_options netdissect_options;

#define IF_PRINTER_ARGS (netdissect_options *, const struct pcap_pkthdr *, const u_char *)

typedef void (*if_printer) IF_PRINTER_ARGS;

/*
 * In case the data in a buffer needs to be processed by being decrypted,
 * decompressed, etc. before it's dissected, we can't process it in place,
 * we have to allocate a new buffer for the processed data.
 *
 * We keep a stack of those buffers; when we allocate a new buffer, we
 * push the current one onto a stack, and when we're done with the new
 * buffer, we free the current buffer and pop the previous one off the
 * stack.
 *
 * A buffer has a beginning and end pointer, and a link to the previous
 * buffer on the stack.
 *
 * In other cases, we temporarily adjust the snapshot end to reflect a
 * packet-length field in the packet data and, when finished dissecting
 * that part of the packet, restore the old snapshot end.  We keep that
 * on the stack with null buffer pointer, meaning there's nothing to
 * free.
 */
struct netdissect_saved_packet_info {
  u_char *ndspi_buffer;					/* pointer to allocated buffer data */
  const u_char *ndspi_packetp;				/* saved beginning of data */
  const u_char *ndspi_snapend;				/* saved end of data */
  struct netdissect_saved_packet_info *ndspi_prev;	/* previous buffer on the stack */
};

/* 'val' value(s) for longjmp */
#define ND_TRUNCATED 1

struct netdissect_options {
  int ndo_bflag;		/* print 4 byte ASes in ASDOT notation */
  int ndo_eflag;		/* print ethernet header */
  int ndo_fflag;		/* don't translate "foreign" IP address */
  int ndo_Kflag;		/* don't check IP, TCP or UDP checksums */
  int ndo_nflag;		/* leave addresses as numbers */
  int ndo_Nflag;		/* remove domains from printed host names */
  int ndo_qflag;		/* quick (shorter) output */
  int ndo_Sflag;		/* print raw TCP sequence numbers */
  int ndo_tflag;		/* print packet arrival time */
  int ndo_uflag;		/* Print undecoded NFS handles */
  int ndo_vflag;		/* verbosity level */
  int ndo_xflag;		/* print packet in hex */
  int ndo_Xflag;		/* print packet in hex/ASCII */
  int ndo_Aflag;		/* print packet only in ASCII observing TAB,
				 * LF, CR and SPACE as graphical chars
				 */
  int ndo_Hflag;		/* dissect 802.11s draft mesh standard */
  const char *ndo_protocol;	/* protocol */
  jmp_buf ndo_early_end;	/* jmp_buf for setjmp()/longjmp() */
  void *ndo_last_mem_p;		/* pointer to the last allocated memory chunk */
  int ndo_packet_number;	/* print a packet number in the beginning of line */
  int ndo_suppress_default_print; /* don't use default_print() for unknown packet types */
  int ndo_tstamp_precision;	/* requested time stamp precision */
  const char *program_name;	/* Name of the program using the library */

  char *ndo_espsecret;
  struct sa_list *ndo_sa_list_head;  /* used by print-esp.c */
  struct sa_list *ndo_sa_default;

  char *ndo_sigsecret;		/* Signature verification secret key */

  int   ndo_packettype;	/* as specified by -T */

  int   ndo_snaplen;
  int   ndo_ll_hdr_len;	/* link-layer header length */

  /*global pointers to beginning and end of current packet (during printing) */
  const u_char *ndo_packetp;
  const u_char *ndo_snapend;

  /* stack of saved packet boundary and buffer information */
  struct netdissect_saved_packet_info *ndo_packet_info_stack;

  /* pointer to the if_printer function */
  if_printer ndo_if_printer;

  /* pointer to void function to output stuff */
  void (*ndo_default_print)(netdissect_options *,
			    const u_char *bp, u_int length);

  /* pointer to function to do regular output */
  int  (*ndo_printf)(netdissect_options *,
		     const char *fmt, ...)
		     PRINTFLIKE_FUNCPTR(2, 3);
  /* pointer to function to output errors */
  void NORETURN_FUNCPTR (*ndo_error)(netdissect_options *,
				     status_exit_codes_t status,
				     const char *fmt, ...)
				     PRINTFLIKE_FUNCPTR(3, 4);
  /* pointer to function to output warnings */
  void (*ndo_warning)(netdissect_options *,
		      const char *fmt, ...)
		      PRINTFLIKE_FUNCPTR(2, 3);
};

extern WARN_UNUSED_RESULT int nd_push_buffer(netdissect_options *, u_char *, const u_char *, const u_int);
extern WARN_UNUSED_RESULT int nd_push_snaplen(netdissect_options *, const u_char *, const u_int);
extern void nd_change_snaplen(netdissect_options *, const u_char *, const u_int);
extern void nd_pop_packet_info(netdissect_options *);
extern void nd_pop_all_packet_info(netdissect_options *);

static inline NORETURN void
nd_trunc_longjmp(netdissect_options *ndo)
{
	longjmp(ndo->ndo_early_end, ND_TRUNCATED);
#ifdef _AIX
	/*
	 * In AIX <setjmp.h> decorates longjmp() with "#pragma leaves", which tells
	 * XL C that the function is noreturn, but GCC remains unaware of that and
	 * yields a "'noreturn' function does return" warning.
	 */
	ND_UNREACHABLE
#endif /* _AIX */
}

#define PT_VAT		1	/* Visual Audio Tool */
#define PT_WB		2	/* distributed White Board */
#define PT_RPC		3	/* Remote Procedure Call */
#define PT_RTP		4	/* Real-Time Applications protocol */
#define PT_RTCP		5	/* Real-Time Applications control protocol */
#define PT_SNMP		6	/* Simple Network Management Protocol */
#define PT_CNFP		7	/* Cisco NetFlow protocol */
#define PT_TFTP		8	/* trivial file transfer protocol */
#define PT_AODV		9	/* Ad-hoc On-demand Distance Vector Protocol */
#define PT_CARP		10	/* Common Address Redundancy Protocol */
#define PT_RADIUS	11	/* RADIUS authentication Protocol */
#define PT_ZMTP1	12	/* ZeroMQ Message Transport Protocol 1.0 */
#define PT_VXLAN	13	/* Virtual eXtensible Local Area Network */
#define PT_PGM		14	/* [UDP-encapsulated] Pragmatic General Multicast */
#define PT_PGM_ZMTP1	15	/* ZMTP/1.0 inside PGM (native or UDP-encapsulated) */
#define PT_LMP		16	/* Link Management Protocol */
#define PT_RESP		17	/* RESP */
#define PT_PTP		18	/* PTP */
#define PT_SOMEIP	19	/* Autosar SOME/IP Protocol */
#define PT_DOMAIN	20	/* Domain Name System (DNS) */

#define ND_MIN(a,b) ((a)>(b)?(b):(a))
#define ND_MAX(a,b) ((b)>(a)?(b):(a))

/* For source or destination ports tests (UDP, TCP, ...) */
#define IS_SRC_OR_DST_PORT(p) (sport == (p) || dport == (p))

/*
 * Maximum snapshot length.  This should be enough to capture the full
 * packet on most network interfaces.
 *
 *
 * Somewhat arbitrary, but chosen to be:
 *
 *    1) big enough for maximum-size Linux loopback packets (65549)
 *       and some USB packets captured with USBPcap:
 *
 *           https://desowin.org/usbpcap/
 *
 *       (> 131072, < 262144)
 *
 * and
 *
 *    2) small enough not to cause attempts to allocate huge amounts of
 *       memory; some applications might use the snapshot length in a
 *       savefile header to control the size of the buffer they allocate,
 *       so a size of, say, 2^31-1 might not work well.
 *
 * XXX - does it need to be bigger still?  Note that, for versions of
 * libpcap with pcap_create()/pcap_activate(), if no -s flag is specified
 * or -s 0 is specified, we won't set the snapshot length at all, and will
 * let libpcap choose a snapshot length; newer versions may choose a bigger
 * value than 262144 for D-Bus, for example.
 */
#define MAXIMUM_SNAPLEN	262144

/*
 * True if "l" bytes from "p" were captured.
 *
 * The "ndo->ndo_snapend - (l) <= ndo->ndo_snapend" checks to make sure
 * "l" isn't so large that "ndo->ndo_snapend - (l)" underflows.
 *
 * The check is for <= rather than < because "l" might be 0.
 *
 * We cast the pointers to uintptr_t to make sure that the compiler
 * doesn't optimize away any of these tests (which it is allowed to
 * do, as adding an integer to, or subtracting an integer from, a
 * pointer assumes that the pointer is a pointer to an element of an
 * array and that the result of the addition or subtraction yields a
 * pointer to another member of the array, so that, for example, if
 * you subtract a positive integer from a pointer, the result is
 * guaranteed to be less than the original pointer value). See
 *
 *	https://www.kb.cert.org/vuls/id/162289
 */

/*
 * Test in two parts to avoid these warnings:
 * comparison of unsigned expression >= 0 is always true [-Wtype-limits],
 * comparison is always true due to limited range of data type [-Wtype-limits].
 */
#define IS_NOT_NEGATIVE(x) (((x) > 0) || ((x) == 0))

#define ND_TTEST_LEN(p, l) \
  (IS_NOT_NEGATIVE(l) && \
	((uintptr_t)ndo->ndo_snapend - (l) <= (uintptr_t)ndo->ndo_snapend && \
         (uintptr_t)(p) <= (uintptr_t)ndo->ndo_snapend - (l)))

/* True if "*(p)" was captured */
#define ND_TTEST_SIZE(p) ND_TTEST_LEN(p, sizeof(*(p)))

/* Bail out if "l" bytes from "p" were not captured */
#ifdef ND_LONGJMP_FROM_TCHECK
#define ND_TCHECK_LEN(p, l) if (!ND_TTEST_LEN(p, l)) nd_trunc_longjmp(ndo)
#else
#define ND_TCHECK_LEN(p, l) if (!ND_TTEST_LEN(p, l)) goto trunc
#endif

/* Bail out if "*(p)" was not captured */
#define ND_TCHECK_SIZE(p) ND_TCHECK_LEN(p, sizeof(*(p)))

/*
 * Number of bytes between two pointers.
 */
#define ND_BYTES_BETWEEN(p1, p2) ((u_int)(((const uint8_t *)(p1)) - (const uint8_t *)(p2)))

/*
 * Number of bytes remaining in the captured data, starting at the
 * byte pointed to by the argument.
 */
#define ND_BYTES_AVAILABLE_AFTER(p) ND_BYTES_BETWEEN(ndo->ndo_snapend, (p))

/* Check length < minimum for invalid packet with a custom message, format %u */
#define ND_LCHECKMSG_U(length, minimum, what) \
if ((length) < (minimum)) { \
ND_PRINT(" [%s %u < %u]", (what), (length), (minimum)); \
goto invalid; \
}

/* Check length < minimum for invalid packet with #length message, format %u */
#define ND_LCHECK_U(length, minimum) \
ND_LCHECKMSG_U((length), (minimum), (#length))

/* Check length < minimum for invalid packet with a custom message, format %zu */
#define ND_LCHECKMSG_ZU(length, minimum, what) \
if ((length) < (minimum)) { \
ND_PRINT(" [%s %u < %zu]", (what), (length), (minimum)); \
goto invalid; \
}

/* Check length < minimum for invalid packet with #length message, format %zu */
#define ND_LCHECK_ZU(length, minimum) \
ND_LCHECKMSG_ZU((length), (minimum), (#length))

#define ND_PRINT(...) (ndo->ndo_printf)(ndo, __VA_ARGS__)
#define ND_DEFAULTPRINT(ap, length) (*ndo->ndo_default_print)(ndo, ap, length)

extern void ts_print(netdissect_options *, const struct timeval *);
extern void signed_relts_print(netdissect_options *, int32_t);
extern void unsigned_relts_print(netdissect_options *, uint32_t);

extern const char *nd_format_time(char *buf, size_t bufsize,
    const char *format, const struct tm *timeptr);

extern void fn_print_char(netdissect_options *, u_char);
extern void fn_print_str(netdissect_options *, const u_char *);
extern u_int nd_printztn(netdissect_options *, const u_char *, u_int, const u_char *);
extern int nd_printn(netdissect_options *, const u_char *, u_int, const u_char *);
extern void nd_printjnp(netdissect_options *, const u_char *, u_int);

/*
 * Flags for txtproto_print().
 */
#define RESP_CODE_SECOND_TOKEN	0x00000001	/* response code is second token in response line */

extern void txtproto_print(netdissect_options *, const u_char *, u_int,
			   const char **, u_int);

#if (defined(__i386__) || defined(_M_IX86) || defined(__X86__) || defined(__x86_64__) || defined(_M_X64)) || \
    (defined(__arm__) || defined(_M_ARM) || defined(__aarch64__)) || \
    (defined(__m68k__) && (!defined(__mc68000__) && !defined(__mc68010__))) || \
    (defined(__ppc__) || defined(__ppc64__) || defined(_M_PPC) || defined(_ARCH_PPC) || defined(_ARCH_PPC64)) || \
    (defined(__s390__) || defined(__s390x__) || defined(__zarch__)) || \
    defined(__vax__)
/*
 * The processor natively handles unaligned loads, so just use memcpy()
 * and memcmp(), to enable those optimizations.
 *
 * XXX - are those all the x86 tests we need?
 * XXX - do we need to worry about ARMv1 through ARMv5, which didn't
 * support unaligned loads, and, if so, do we need to worry about all
 * of them, or just some of them, e.g. ARMv5?
 * XXX - are those the only 68k tests we need not to generated
 * unaligned accesses if the target is the 68000 or 68010?
 * XXX - are there any tests we don't need, because some definitions are for
 * compilers that also predefine the GCC symbols?
 * XXX - do we need to test for both 32-bit and 64-bit versions of those
 * architectures in all cases?
 */
#define UNALIGNED_MEMCPY(p, q, l)	memcpy((p), (q), (l))
#define UNALIGNED_MEMCMP(p, q, l)	memcmp((p), (q), (l))
#else
/*
 * The processor doesn't natively handle unaligned loads,
 * and the compiler might "helpfully" optimize memcpy()
 * and memcmp(), when handed pointers that would normally
 * be properly aligned, into sequences that assume proper
 * alignment.
 *
 * Do copies and compares of possibly-unaligned data by
 * calling routines that wrap memcpy() and memcmp(), to
 * prevent that optimization.
 */
extern void unaligned_memcpy(void *, const void *, size_t);
extern int unaligned_memcmp(const void *, const void *, size_t);
#define UNALIGNED_MEMCPY(p, q, l)	unaligned_memcpy((p), (q), (l))
#define UNALIGNED_MEMCMP(p, q, l)	unaligned_memcmp((p), (q), (l))
#endif

#define PLURAL_SUFFIX(n) \
	(((n) != 1) ? "s" : "")

extern const char *tok2strary_internal(const char **, int, const char *, int);
#define	tok2strary(a,f,i) tok2strary_internal(a, sizeof(a)/sizeof(a[0]),f,i)

struct uint_tokary
{
	u_int uintval;
	const struct tok *tokary;
};

extern const struct tok *uint2tokary_internal(const struct uint_tokary[], const size_t, const u_int);
#define uint2tokary(a, i) uint2tokary_internal(a, sizeof(a)/sizeof(a[0]), i)

extern if_printer lookup_printer(int);

#define ND_DEBUG {printf(" [%s:%d %s] ", __FILE__, __LINE__, __func__); fflush(stdout);}

/* The DLT printer routines */

extern void ap1394_if_print IF_PRINTER_ARGS;
extern void arcnet_if_print IF_PRINTER_ARGS;
extern void arcnet_linux_if_print IF_PRINTER_ARGS;
extern void atm_if_print IF_PRINTER_ARGS;
extern void brcm_tag_if_print IF_PRINTER_ARGS;
extern void brcm_tag_prepend_if_print IF_PRINTER_ARGS;
extern void bt_if_print IF_PRINTER_ARGS;
extern void chdlc_if_print IF_PRINTER_ARGS;
extern void cip_if_print IF_PRINTER_ARGS;
extern void dsa_if_print IF_PRINTER_ARGS;
extern void edsa_if_print IF_PRINTER_ARGS;
extern void enc_if_print IF_PRINTER_ARGS;
extern void ether_if_print IF_PRINTER_ARGS;
extern void fddi_if_print IF_PRINTER_ARGS;
extern void fr_if_print IF_PRINTER_ARGS;
extern void ieee802_11_if_print IF_PRINTER_ARGS;
extern void ieee802_11_radio_avs_if_print IF_PRINTER_ARGS;
extern void ieee802_11_radio_if_print IF_PRINTER_ARGS;
extern void ieee802_15_4_if_print IF_PRINTER_ARGS;
extern void ieee802_15_4_tap_if_print IF_PRINTER_ARGS;
extern void ipfc_if_print IF_PRINTER_ARGS;
extern void ipnet_if_print IF_PRINTER_ARGS;
extern void ipoib_if_print IF_PRINTER_ARGS;
extern void juniper_atm1_if_print IF_PRINTER_ARGS;
extern void juniper_atm2_if_print IF_PRINTER_ARGS;
extern void juniper_chdlc_if_print IF_PRINTER_ARGS;
extern void juniper_es_if_print IF_PRINTER_ARGS;
extern void juniper_ether_if_print IF_PRINTER_ARGS;
extern void juniper_frelay_if_print IF_PRINTER_ARGS;
extern void juniper_ggsn_if_print IF_PRINTER_ARGS;
extern void juniper_mfr_if_print IF_PRINTER_ARGS;
extern void juniper_mlfr_if_print IF_PRINTER_ARGS;
extern void juniper_mlppp_if_print IF_PRINTER_ARGS;
extern void juniper_monitor_if_print IF_PRINTER_ARGS;
extern void juniper_ppp_if_print IF_PRINTER_ARGS;
extern void juniper_pppoe_atm_if_print IF_PRINTER_ARGS;
extern void juniper_pppoe_if_print IF_PRINTER_ARGS;
extern void juniper_services_if_print IF_PRINTER_ARGS;
extern void ltalk_if_print IF_PRINTER_ARGS;
extern void mfr_if_print IF_PRINTER_ARGS;
extern void netanalyzer_if_print IF_PRINTER_ARGS;
extern void netanalyzer_transparent_if_print IF_PRINTER_ARGS;
extern void nflog_if_print IF_PRINTER_ARGS;
extern void null_if_print IF_PRINTER_ARGS;
extern void pflog_if_print IF_PRINTER_ARGS;
extern void pktap_if_print IF_PRINTER_ARGS;
extern void ppi_if_print IF_PRINTER_ARGS;
extern void ppp_bsdos_if_print IF_PRINTER_ARGS;
extern void ppp_hdlc_if_print IF_PRINTER_ARGS;
extern void ppp_if_print IF_PRINTER_ARGS;
extern void pppoe_if_print IF_PRINTER_ARGS;
extern void prism_if_print IF_PRINTER_ARGS;
extern void raw_if_print IF_PRINTER_ARGS;
extern void sl_bsdos_if_print IF_PRINTER_ARGS;
extern void sl_if_print IF_PRINTER_ARGS;
extern void sll2_if_print IF_PRINTER_ARGS;
extern void sll_if_print IF_PRINTER_ARGS;
extern void sunatm_if_print IF_PRINTER_ARGS;
extern void symantec_if_print IF_PRINTER_ARGS;
extern void token_if_print IF_PRINTER_ARGS;
extern void unsupported_if_print IF_PRINTER_ARGS;
extern void usb_linux_48_byte_if_print IF_PRINTER_ARGS;
extern void usb_linux_64_byte_if_print IF_PRINTER_ARGS;
extern void vsock_if_print IF_PRINTER_ARGS;

/*
 * Structure passed to some printers to allow them to print
 * link-layer address information if ndo_eflag isn't set
 * (because they are for protocols that don't have their
 * own addresses, so that we'd want to report link-layer
 * address information).
 *
 * This contains a pointer to an address and a pointer to a routine
 * to which we pass that pointer in order to get a string.
 */
struct lladdr_info {
	const char *(*addr_string)(netdissect_options *, const u_char *);
	const u_char *addr;
};

/* The printer routines. */

extern void aarp_print(netdissect_options *, const u_char *, u_int);
extern int ah_print(netdissect_options *, const u_char *);
extern void ahcp_print(netdissect_options *, const u_char *, u_int);
extern void aodv_print(netdissect_options *, const u_char *, u_int, int);
extern void aoe_print(netdissect_options *, const u_char *, const u_int);
extern int  arista_ethertype_print(netdissect_options *,const u_char *, u_int);
extern void arp_print(netdissect_options *, const u_char *, u_int, u_int);
extern void ascii_print(netdissect_options *, const u_char *, u_int);
extern void atalk_print(netdissect_options *, const u_char *, u_int);
extern void atm_print(netdissect_options *, u_int, u_int, u_int, const u_char *, u_int, u_int);
extern void babel_print(netdissect_options *, const u_char *, u_int);
extern void bcm_li_print(netdissect_options *, const u_char *, u_int);
extern void beep_print(netdissect_options *, const u_char *, u_int);
extern void bfd_print(netdissect_options *, const u_char *, u_int, u_int);
extern void bgp_print(netdissect_options *, const u_char *, u_int);
extern const char *bgp_vpn_rd_print(netdissect_options *, const u_char *);
extern void bootp_print(netdissect_options *, const u_char *, u_int);
extern void calm_fast_print(netdissect_options *, const u_char *, u_int, const struct lladdr_info *);
extern void carp_print(netdissect_options *, const u_char *, u_int, u_int);
extern void cdp_print(netdissect_options *, const u_char *, u_int);
extern void cfm_print(netdissect_options *, const u_char *, u_int);
extern u_int chdlc_print(netdissect_options *, const u_char *, u_int);
extern void cisco_autorp_print(netdissect_options *, const u_char *, u_int);
extern void cnfp_print(netdissect_options *, const u_char *);
extern void dccp_print(netdissect_options *, const u_char *, const u_char *, u_int);
extern void decnet_print(netdissect_options *, const u_char *, u_int, u_int);
extern void dhcp6_print(netdissect_options *, const u_char *, u_int);
extern int dstopt_process(netdissect_options *, const u_char *);
extern void dtp_print(netdissect_options *, const u_char *, u_int);
extern void dvmrp_print(netdissect_options *, const u_char *, u_int);
extern void eap_print(netdissect_options *, const u_char *, u_int);
extern void eapol_print(netdissect_options *, const u_char *);
extern void egp_print(netdissect_options *, const u_char *, u_int);
extern void eigrp_print(netdissect_options *, const u_char *, u_int);
extern void esp_print(netdissect_options *, const u_char *, u_int, const u_char *, u_int, int, u_int);
extern u_int ether_print(netdissect_options *, const u_char *, u_int, u_int, void (*)(netdissect_options *, const u_char *), const u_char *);
extern u_int ether_switch_tag_print(netdissect_options *, const u_char *, u_int, u_int, void (*)(netdissect_options *, const u_char *), u_int);
extern int ethertype_print(netdissect_options *, u_short, const u_char *, u_int, u_int, const struct lladdr_info *, const struct lladdr_info *);
extern u_int fddi_print(netdissect_options *, const u_char *, u_int, u_int);
extern void forces_print(netdissect_options *, const u_char *, u_int);
extern u_int fr_print(netdissect_options *, const u_char *, u_int);
extern int frag6_print(netdissect_options *, const u_char *, const u_char *);
extern void ftp_print(netdissect_options *, const u_char *, u_int);
extern void geneve_print(netdissect_options *, const u_char *, u_int);
extern void geonet_print(netdissect_options *, const u_char *, u_int, const struct lladdr_info *);
extern void gre_print(netdissect_options *, const u_char *, u_int);
extern int hbhopt_process(netdissect_options *, const u_char *, int *, uint32_t *);
extern void hex_and_ascii_print(netdissect_options *, const char *, const u_char *, u_int);
extern void hex_print(netdissect_options *, const char *ident, const u_char *cp, u_int);
extern void hex_print_with_offset(netdissect_options *, const char *ident, const u_char *cp, u_int, u_int);
extern void hncp_print(netdissect_options *, const u_char *, u_int);
extern void hsrp_print(netdissect_options *, const u_char *, u_int);
extern void http_print(netdissect_options *, const u_char *, u_int);
extern void icmp6_print(netdissect_options *, const u_char *, u_int, const u_char *, int);
extern void icmp_print(netdissect_options *, const u_char *, u_int, const u_char *, int);
extern u_int ieee802_15_4_print(netdissect_options *, const u_char *, u_int);
extern u_int ieee802_11_radio_print(netdissect_options *, const u_char *, u_int, u_int);
extern void igmp_print(netdissect_options *, const u_char *, u_int);
extern void igrp_print(netdissect_options *, const u_char *, u_int);
extern void ip6_print(netdissect_options *, const u_char *, u_int);
extern void ipN_print(netdissect_options *, const u_char *, u_int);
extern void ip_print(netdissect_options *, const u_char *, u_int);
extern void ipcomp_print(netdissect_options *, const u_char *);
extern void ipx_netbios_print(netdissect_options *, const u_char *, u_int);
extern void ipx_print(netdissect_options *, const u_char *, u_int);
extern void isakmp_print(netdissect_options *, const u_char *, u_int, const u_char *);
extern void isakmp_rfc3948_print(netdissect_options *, const u_char *, u_int, const u_char *, int, int, u_int);
extern void isoclns_print(netdissect_options *, const u_char *, u_int);
extern void krb_print(netdissect_options *, const u_char *);
extern void l2tp_print(netdissect_options *, const u_char *, u_int);
extern void lane_print(netdissect_options *, const u_char *, u_int, u_int);
extern void ldp_print(netdissect_options *, const u_char *, u_int);
extern void lisp_print(netdissect_options *, const u_char *, u_int);
extern u_int llap_print(netdissect_options *, const u_char *, u_int);
extern int llc_print(netdissect_options *, const u_char *, u_int, u_int, const struct lladdr_info *, const struct lladdr_info *);
extern void lldp_print(netdissect_options *, const u_char *, u_int);
extern void lmp_print(netdissect_options *, const u_char *, u_int);
extern void loopback_print(netdissect_options *, const u_char *, u_int);
extern void lspping_print(netdissect_options *, const u_char *, u_int);
extern void lwapp_control_print(netdissect_options *, const u_char *, u_int, int);
extern void lwapp_data_print(netdissect_options *, const u_char *, u_int);
extern void lwres_print(netdissect_options *, const u_char *, u_int);
extern void m3ua_print(netdissect_options *, const u_char *, const u_int);
extern int macsec_print(netdissect_options *, const u_char **,
			 u_int *, u_int *, u_int *, const struct lladdr_info *,
			 const struct lladdr_info *);
extern u_int mfr_print(netdissect_options *, const u_char *, u_int);
extern void mobile_print(netdissect_options *, const u_char *, u_int);
extern int mobility_print(netdissect_options *, const u_char *, const u_char *);
extern void mpcp_print(netdissect_options *, const u_char *, u_int);
extern void mpls_print(netdissect_options *, const u_char *, u_int);
extern int mptcp_print(netdissect_options *, const u_char *, u_int, u_char);
extern void msdp_print(netdissect_options *, const u_char *, u_int);
extern void msnlb_print(netdissect_options *, const u_char *);
extern void nbt_tcp_print(netdissect_options *, const u_char *, u_int);
extern void nbt_udp137_print(netdissect_options *, const u_char *, u_int);
extern void nbt_udp138_print(netdissect_options *, const u_char *, u_int);
extern void netbeui_print(netdissect_options *, u_short, const u_char *, u_int);
extern void nfsreply_print(netdissect_options *, const u_char *, u_int, const u_char *);
extern void nfsreply_noaddr_print(netdissect_options *, const u_char *, u_int, const u_char *);
extern void nfsreq_noaddr_print(netdissect_options *, const u_char *, u_int, const u_char *);
extern const u_char *fqdn_print(netdissect_options *, const u_char *, const u_char *);
extern void domain_print(netdissect_options *, const u_char *, u_int, int, int);
extern void nsh_print(netdissect_options *, const u_char *, u_int);
extern void ntp_print(netdissect_options *, const u_char *, u_int);
extern void oam_print(netdissect_options *, const u_char *, u_int, u_int);
extern void olsr_print(netdissect_options *, const u_char *, u_int, int);
extern void openflow_print(netdissect_options *, const u_char *, u_int);
extern void ospf6_print(netdissect_options *, const u_char *, u_int);
extern void ospf_print(netdissect_options *, const u_char *, u_int, const u_char *);
extern int ospf_grace_lsa_print(netdissect_options *, const u_char *, u_int);
extern int ospf_te_lsa_print(netdissect_options *, const u_char *, u_int);
extern void otv_print(netdissect_options *, const u_char *, u_int);
extern void pfsync_ip_print(netdissect_options *, const u_char *, u_int);
extern u_int pfsync_if_print(netdissect_options *, const struct pcap_pkthdr *, const u_char *);
extern void pgm_print(netdissect_options *, const u_char *, u_int, const u_char *);
extern void pim_print(netdissect_options *, const u_char *, u_int, const u_char *);
extern void pimv1_print(netdissect_options *, const u_char *, u_int);
extern u_int ppp_print(netdissect_options *, const u_char *, u_int);
extern u_int pppoe_print(netdissect_options *, const u_char *, u_int);
extern void pptp_print(netdissect_options *, const u_char *);
extern void ptp_print(netdissect_options *, const u_char *, u_int);
extern int print_unknown_data(netdissect_options *, const u_char *, const char *, u_int);
extern const char *q922_string(netdissect_options *, const u_char *, u_int);
extern void q933_print(netdissect_options *, const u_char *, u_int);
extern void radius_print(netdissect_options *, const u_char *, u_int);
extern void resp_print(netdissect_options *, const u_char *, u_int);
extern void rip_print(netdissect_options *, const u_char *, u_int);
extern void ripng_print(netdissect_options *, const u_char *, unsigned int);
extern void rpki_rtr_print(netdissect_options *, const u_char *, u_int);
extern void rsvp_print(netdissect_options *, const u_char *, u_int);
extern int rt6_print(netdissect_options *, const u_char *, const u_char *);
extern void rtl_print(netdissect_options *, const u_char *, u_int, const struct lladdr_info *, const struct lladdr_info *);
extern void rtsp_print(netdissect_options *, const u_char *, u_int);
extern void rx_print(netdissect_options *, const u_char *, u_int, uint16_t, uint16_t, const u_char *);
extern void sctp_print(netdissect_options *, const u_char *, const u_char *, u_int);
extern void sflow_print(netdissect_options *, const u_char *, u_int);
extern void ssh_print(netdissect_options *, const u_char *, u_int);
extern void sip_print(netdissect_options *, const u_char *, u_int);
extern void slow_print(netdissect_options *, const u_char *, u_int);
extern void smb_tcp_print(netdissect_options *, const u_char *, u_int);
extern void smtp_print(netdissect_options *, const u_char *, u_int);
extern int snap_print(netdissect_options *, const u_char *, u_int, u_int, const struct lladdr_info *, const struct lladdr_info *, u_int);
extern void snmp_print(netdissect_options *, const u_char *, u_int);
extern void stp_print(netdissect_options *, const u_char *, u_int);
extern void sunrpc_print(netdissect_options *, const u_char *, u_int, const u_char *);
extern void syslog_print(netdissect_options *, const u_char *, u_int);
extern void tcp_print(netdissect_options *, const u_char *, u_int, const u_char *, int);
extern void telnet_print(netdissect_options *, const u_char *, u_int);
extern void tftp_print(netdissect_options *, const u_char *, u_int);
extern void timed_print(netdissect_options *, const u_char *);
extern void tipc_print(netdissect_options *, const u_char *, u_int, u_int);
extern u_int token_print(netdissect_options *, const u_char *, u_int, u_int);
extern void udld_print(netdissect_options *, const u_char *, u_int);
extern void udp_print(netdissect_options *, const u_char *, u_int, const u_char *, int, u_int);
extern int vjc_print(netdissect_options *, const u_char *, u_short);
extern void vqp_print(netdissect_options *, const u_char *, u_int);
extern void vrrp_print(netdissect_options *, const u_char *, u_int, const u_char *, int, int);
extern void vtp_print(netdissect_options *, const u_char *, const u_int);
extern void vxlan_gpe_print(netdissect_options *, const u_char *, u_int);
extern void vxlan_print(netdissect_options *, const u_char *, u_int);
extern void wb_print(netdissect_options *, const u_char *, u_int);
extern void whois_print(netdissect_options *, const u_char *, u_int);
extern void zep_print(netdissect_options *, const u_char *, u_int);
extern void zephyr_print(netdissect_options *, const u_char *, u_int);
extern void zmtp1_print(netdissect_options *, const u_char *, u_int);
extern void zmtp1_datagram_print(netdissect_options *, const u_char *, const u_int);
extern void someip_print(netdissect_options *, const u_char *, const u_int);

/* checksum routines */
extern void init_checksum(void);
extern uint16_t verify_crc10_cksum(uint16_t, const u_char *, int);
extern uint16_t create_osi_cksum(const uint8_t *, int, int);

struct cksum_vec {
	const uint8_t	*ptr;
	int		len;
};
extern uint16_t in_cksum(const struct cksum_vec *, int);
extern uint16_t in_cksum_shouldbe(uint16_t, uint16_t);

/* IP protocol demuxing routines */
extern void ip_demux_print(netdissect_options *, const u_char *, u_int, u_int, int, u_int, uint8_t, const u_char *);

extern uint16_t nextproto4_cksum(netdissect_options *, const struct ip *, const uint8_t *, u_int, u_int, uint8_t);

/* in print-ip6.c */
extern uint16_t nextproto6_cksum(netdissect_options *, const struct ip6_hdr *, const uint8_t *, u_int, u_int, uint8_t);

/* Utilities */
extern void nd_print_trunc(netdissect_options *);
extern void nd_print_protocol(netdissect_options *);
extern void nd_print_protocol_caps(netdissect_options *);
extern void nd_print_invalid(netdissect_options *);

extern int mask2plen(uint32_t);
extern int mask62plen(const u_char *);

extern const char *dnnum_string(netdissect_options *, u_short);

extern int decode_prefix4(netdissect_options *, const u_char *, u_int, char *, size_t);
extern int decode_prefix6(netdissect_options *, const u_char *, u_int, char *, size_t);

extern void esp_decodesecret_print(netdissect_options *);
extern int esp_decrypt_buffer_by_ikev2_print(netdissect_options *, int,
					     const u_char spii[8],
					     const u_char spir[8],
					     const u_char *, const u_char *);

#endif  /* netdissect_h */
