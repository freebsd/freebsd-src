/*-
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)bpf.h	8.1 (Berkeley) 6/10/93
 *	@(#)bpf.h	1.34 (LBL)     6/16/96
 *
 * $FreeBSD$
 */

#ifndef _NET_BPF_H_
#define _NET_BPF_H_

/* BSD style release date */
#define	BPF_RELEASE 199606

typedef	int32_t	  bpf_int32;
typedef	u_int32_t bpf_u_int32;

/*
 * Alignment macros.  BPF_WORDALIGN rounds up to the next
 * even multiple of BPF_ALIGNMENT.
 */
#define BPF_ALIGNMENT sizeof(long)
#define BPF_WORDALIGN(x) (((x)+(BPF_ALIGNMENT-1))&~(BPF_ALIGNMENT-1))

#define BPF_MAXINSNS 512
#define BPF_MAXBUFSIZE 0x80000
#define BPF_MINBUFSIZE 32

/*
 *  Structure for BIOCSETF.
 */
struct bpf_program {
	u_int bf_len;
	struct bpf_insn *bf_insns;
};

/*
 * Struct returned by BIOCGSTATS.
 */
struct bpf_stat {
	u_int bs_recv;		/* number of packets received */
	u_int bs_drop;		/* number of packets dropped */
};

/*
 * Struct return by BIOCVERSION.  This represents the version number of
 * the filter language described by the instruction encodings below.
 * bpf understands a program iff kernel_major == filter_major &&
 * kernel_minor >= filter_minor, that is, if the value returned by the
 * running kernel has the same major number and a minor number equal
 * equal to or less than the filter being downloaded.  Otherwise, the
 * results are undefined, meaning an error may be returned or packets
 * may be accepted haphazardly.
 * It has nothing to do with the source code version.
 */
struct bpf_version {
	u_short bv_major;
	u_short bv_minor;
};
/* Current version number of filter architecture. */
#define BPF_MAJOR_VERSION 1
#define BPF_MINOR_VERSION 1

#define	BIOCGBLEN	_IOR('B',102, u_int)
#define	BIOCSBLEN	_IOWR('B',102, u_int)
#define	BIOCSETF	_IOW('B',103, struct bpf_program)
#define	BIOCFLUSH	_IO('B',104)
#define BIOCPROMISC	_IO('B',105)
#define	BIOCGDLT	_IOR('B',106, u_int)
#define BIOCGETIF	_IOR('B',107, struct ifreq)
#define BIOCSETIF	_IOW('B',108, struct ifreq)
#define BIOCSRTIMEOUT	_IOW('B',109, struct timeval)
#define BIOCGRTIMEOUT	_IOR('B',110, struct timeval)
#define BIOCGSTATS	_IOR('B',111, struct bpf_stat)
#define BIOCIMMEDIATE	_IOW('B',112, u_int)
#define BIOCVERSION	_IOR('B',113, struct bpf_version)
#define BIOCGRSIG	_IOR('B',114, u_int)
#define BIOCSRSIG	_IOW('B',115, u_int)
#define BIOCGHDRCMPLT	_IOR('B',116, u_int)
#define BIOCSHDRCMPLT	_IOW('B',117, u_int)
#define BIOCGSEESENT	_IOR('B',118, u_int)
#define BIOCSSEESENT	_IOW('B',119, u_int)
#define	BIOCSDLT	_IOW('B',120, u_int)
#define	BIOCGDLTLIST	_IOWR('B',121, struct bpf_dltlist)

/*
 * Structure prepended to each packet.
 */
struct bpf_hdr {
	struct timeval	bh_tstamp;	/* time stamp */
	bpf_u_int32	bh_caplen;	/* length of captured portion */
	bpf_u_int32	bh_datalen;	/* original length of packet */
	u_short		bh_hdrlen;	/* length of bpf header (this struct
					   plus alignment padding) */
};
/*
 * Because the structure above is not a multiple of 4 bytes, some compilers
 * will insist on inserting padding; hence, sizeof(struct bpf_hdr) won't work.
 * Only the kernel needs to know about it; applications use bh_hdrlen.
 */
#ifdef _KERNEL
#define	SIZEOF_BPF_HDR	(sizeof(struct bpf_hdr) <= 20 ? 18 : \
    sizeof(struct bpf_hdr))
#endif

/*
 * Data-link level type codes.
 */
#define DLT_NULL	0	/* BSD loopback encapsulation */
#define DLT_EN10MB	1	/* Ethernet (10Mb) */
#define DLT_EN3MB	2	/* Experimental Ethernet (3Mb) */
#define DLT_AX25	3	/* Amateur Radio AX.25 */
#define DLT_PRONET	4	/* Proteon ProNET Token Ring */
#define DLT_CHAOS	5	/* Chaos */
#define DLT_IEEE802	6	/* IEEE 802 Networks */
#define DLT_ARCNET	7	/* ARCNET */
#define DLT_SLIP	8	/* Serial Line IP */
#define DLT_PPP		9	/* Point-to-point Protocol */
#define DLT_FDDI	10	/* FDDI */
#define DLT_ATM_RFC1483	11	/* LLC/SNAP encapsulated atm */
#define DLT_RAW		12	/* raw IP */

/*
 * These are values from BSD/OS's "bpf.h".
 * These are not the same as the values from the traditional libpcap
 * "bpf.h"; however, these values shouldn't be generated by any
 * OS other than BSD/OS, so the correct values to use here are the
 * BSD/OS values.
 *
 * Platforms that have already assigned these values to other
 * DLT_ codes, however, should give these codes the values
 * from that platform, so that programs that use these codes will
 * continue to compile - even though they won't correctly read
 * files of these types.
 */
#define DLT_SLIP_BSDOS	15	/* BSD/OS Serial Line IP */
#define DLT_PPP_BSDOS	16	/* BSD/OS Point-to-point Protocol */

#define DLT_ATM_CLIP	19	/* Linux Classical-IP over ATM */

/*
 * These values are defined by NetBSD; other platforms should refrain from
 * using them for other purposes, so that NetBSD savefiles with link
 * types of 50 or 51 can be read as this type on all platforms.
 */
#define DLT_PPP_SERIAL	50	/* PPP over serial with HDLC encapsulation */
#define DLT_PPP_ETHER	51	/* PPP over Ethernet */

/*
 * Reserved for the Symantec Enterprise Firewall.
 */
#define DLT_SYMANTEC_FIREWALL	99


/*
 * This value was defined by libpcap 0.5; platforms that have defined
 * it with a different value should define it here with that value -
 * a link type of 104 in a save file will be mapped to DLT_C_HDLC,
 * whatever value that happens to be, so programs will correctly
 * handle files with that link type regardless of the value of
 * DLT_C_HDLC.
 *
 * The name DLT_C_HDLC was used by BSD/OS; we use that name for source
 * compatibility with programs written for BSD/OS.
 *
 * libpcap 0.5 defined it as DLT_CHDLC; we define DLT_CHDLC as well,
 * for source compatibility with programs written for libpcap 0.5.
 */
#define DLT_C_HDLC	104	/* Cisco HDLC */
#define DLT_CHDLC	DLT_C_HDLC

#define DLT_IEEE802_11	105	/* IEEE 802.11 wireless */

/*
 * Values between 106 and 107 are used in capture file headers as
 * link-layer types corresponding to DLT_ types that might differ
 * between platforms; don't use those values for new DLT_ new types.
 */

/*
 * Frame Relay; BSD/OS has a DLT_FR with a value of 11, but that collides
 * with other values.
 * DLT_FR and DLT_FRELAY packets start with the Q.922 Frame Relay header
 * (DLCI, etc.).
 */
#define DLT_FRELAY	107

/*
 * OpenBSD DLT_LOOP, for loopback devices; it's like DLT_NULL, except
 * that the AF_ type in the link-layer header is in network byte order.
 *
 * OpenBSD defines it as 12, but that collides with DLT_RAW, so we
 * define it as 108 here.  If OpenBSD picks up this file, it should
 * define DLT_LOOP as 12 in its version, as per the comment above -
 * and should not use 108 as a DLT_ value.
 */
#define DLT_LOOP	108

/*
 * Values between 109 and 112 are used in capture file headers as
 * link-layer types corresponding to DLT_ types that might differ
 * between platforms; don't use those values for new DLT_ new types.
 */

/*
 * Encapsulated packets for IPsec; DLT_ENC is 13 in OpenBSD, but that's
 * DLT_SLIP_BSDOS in NetBSD, so we don't use 13 for it in OSes other
 * than OpenBSD.
 */
#define DLT_ENC	109

/*
 * This is for Linux cooked sockets.
 */
#define DLT_LINUX_SLL	113

/*
 * Apple LocalTalk hardware.
 */
#define DLT_LTALK	114

/*
 * Acorn Econet.
 */
#define DLT_ECONET	115

/*
 * Reserved for use with OpenBSD ipfilter.
 */
#define DLT_IPFILTER	116

/*
 * Reserved for use in capture-file headers as a link-layer type
 * corresponding to OpenBSD DLT_PFLOG; DLT_PFLOG is 17 in OpenBSD,
 * but that's DLT_LANE8023 in SuSE 6.3, so we can't use 17 for it
 * in capture-file headers.
 */
#define DLT_PFLOG	117

/*
 * Registered for Cisco-internal use.
 */
#define DLT_CISCO_IOS	118

/*
 * Reserved for 802.11 cards using the Prism II chips, with a link-layer
 * header including Prism monitor mode information plus an 802.11
 * header.
 */
#define DLT_PRISM_HEADER	119

/*
 * Reserved for Aironet 802.11 cards, with an Aironet link-layer header
 * (see Doug Ambrisko's FreeBSD patches).
 */
#define DLT_AIRONET_HEADER	120

/*
 * Reserved for use by OpenBSD's pfsync device.
 */
#define DLT_PFSYNC	121

/*
 * Reserved for Siemens HiPath HDLC. XXX
 */
#define DLT_HHDLC	121

/*
 * Reserved for RFC 2625 IP-over-Fibre Channel.
 */
#define DLT_IP_OVER_FC	122

/*
 * Reserved for Full Frontal ATM on Solaris.
 */
#define DLT_SUNATM	123

/*
 * Reserved as per request from Kent Dahlgren <kent@praesum.com>
 * for private use.
 */
#define DLT_RIO		124	/* RapidIO */
#define DLT_PCI_EXP	125	/* PCI Express */
#define DLT_AURORA	126	/* Xilinx Aurora link layer */

/*
 * BSD header for 802.11 plus a number of bits of link-layer information
 * including radio information.
 */
#ifndef DLT_IEEE802_11_RADIO
#define DLT_IEEE802_11_RADIO	127
#endif

/*
 * Reserved for TZSP encapsulation.
 */
#define DLT_TZSP		128	/* Tazmen Sniffer Protocol */

/*
 * Reserved for Linux ARCNET.
 */
#define DLT_ARCNET_LINUX	129

/*
 * Juniper-private data link types.
 */
#define DLT_JUNIPER_MLPPP	130
#define DLT_JUNIPER_MLFR	131
#define DLT_JUNIPER_ES		132
#define DLT_JUNIPER_GGSN	133
#define DLT_JUNIPER_MFR		134
#define DLT_JUNIPER_ATM2	135
#define DLT_JUNIPER_SERVICES	136
#define DLT_JUNIPER_ATM1	137

/*
 * Apple IP-over-IEEE 1394, as per a request from Dieter Siegmund
 * <dieter@apple.com>.  The header that's presented is an Ethernet-like
 * header:
 *
 *	#define FIREWIRE_EUI64_LEN	8
 *	struct firewire_header {
 *		u_char  firewire_dhost[FIREWIRE_EUI64_LEN];
 *		u_char  firewire_shost[FIREWIRE_EUI64_LEN];
 *		u_short firewire_type;
 *	};
 *
 * with "firewire_type" being an Ethernet type value, rather than,
 * for example, raw GASP frames being handed up.
 */
#define DLT_APPLE_IP_OVER_IEEE1394	138

/*
 * Various SS7 encapsulations, as per a request from Jeff Morriss
 * <jeff.morriss[AT]ulticom.com> and subsequent discussions.
 */
#define DLT_MTP2_WITH_PHDR	139	/* pseudo-header with various info, followed by MTP2 */
#define DLT_MTP2		140	/* MTP2, without pseudo-header */
#define DLT_MTP3		141	/* MTP3, without pseudo-header or MTP2 */
#define DLT_SCCP		142	/* SCCP, without pseudo-header or MTP2 or MTP3 */

/*
 * Reserved for DOCSIS.
 */
#define DLT_DOCSIS	143

/*
 * Reserved for Linux IrDA.
 */
#define DLT_LINUX_IRDA	144

/*
 * Reserved for IBM SP switch and IBM Next Federation switch.
 */
#define DLT_IBM_SP	145
#define DLT_IBM_SN	146

/*
 * Reserved for private use.  If you have some link-layer header type
 * that you want to use within your organization, with the capture files
 * using that link-layer header type not ever be sent outside your
 * organization, you can use these values.
 *
 * No libpcap release will use these for any purpose, nor will any
 * tcpdump release use them, either.
 *
 * Do *NOT* use these in capture files that you expect anybody not using
 * your private versions of capture-file-reading tools to read; in
 * particular, do *NOT* use them in products, otherwise you may find that
 * people won't be able to use tcpdump, or snort, or Ethereal, or... to
 * read capture files from your firewall/intrusion detection/traffic
 * monitoring/etc. appliance, or whatever product uses that DLT_ value,
 * and you may also find that the developers of those applications will
 * not accept patches to let them read those files.
 *
 * Also, do not use them if somebody might send you a capture using them
 * for *their* private type and tools using them for *your* private type
 * would have to read them.
 *
 * Instead, ask "tcpdump-workers@tcpdump.org" for a new DLT_ value,
 * as per the comment above, and use the type you're given.
 */
#define DLT_USER0		147
#define DLT_USER1		148
#define DLT_USER2		149
#define DLT_USER3		150
#define DLT_USER4		151
#define DLT_USER5		152
#define DLT_USER6		153
#define DLT_USER7		154
#define DLT_USER8		155
#define DLT_USER9		156
#define DLT_USER10		157
#define DLT_USER11		158
#define DLT_USER12		159
#define DLT_USER13		160
#define DLT_USER14		161
#define DLT_USER15		162

/*
 * For future use with 802.11 captures - defined by AbsoluteValue
 * Systems to store a number of bits of link-layer information
 * including radio information:
 *
 *	http://www.shaftnet.org/~pizza/software/capturefrm.txt
 *
 * but it might be used by some non-AVS drivers now or in the
 * future.
 */
#define DLT_IEEE802_11_RADIO_AVS 163	/* 802.11 plus AVS radio header */

/*
 * Juniper-private data link type, as per request from
 * Hannes Gredler <hannes@juniper.net>.  The DLT_s are used
 * for passing on chassis-internal metainformation such as
 * QOS profiles, etc..
 */
#define DLT_JUNIPER_MONITOR     164

/*
 * Reserved for BACnet MS/TP.
 */
#define DLT_BACNET_MS_TP	165

/*
 * Another PPP variant as per request from Karsten Keil <kkeil@suse.de>.
 *
 * This is used in some OSes to allow a kernel socket filter to distinguish
 * between incoming and outgoing packets, on a socket intended to
 * supply pppd with outgoing packets so it can do dial-on-demand and
 * hangup-on-lack-of-demand; incoming packets are filtered out so they
 * don't cause pppd to hold the connection up (you don't want random
 * input packets such as port scans, packets from old lost connections,
 * etc. to force the connection to stay up).
 *
 * The first byte of the PPP header (0xff03) is modified to accomodate
 * the direction - 0x00 = IN, 0x01 = OUT.
 */
#define DLT_PPP_PPPD		166

/*
 * Names for backwards compatibility with older versions of some PPP
 * software; new software should use DLT_PPP_PPPD.
 */
#define DLT_PPP_WITH_DIRECTION	DLT_PPP_PPPD
#define DLT_LINUX_PPP_WITHDIRECTION	DLT_PPP_PPPD

/*
 * Juniper-private data link type, as per request from
 * Hannes Gredler <hannes@juniper.net>.  The DLT_s are used
 * for passing on chassis-internal metainformation such as
 * QOS profiles, cookies, etc..
 */
#define DLT_JUNIPER_PPPOE       167
#define DLT_JUNIPER_PPPOE_ATM   168

#define DLT_GPRS_LLC		169	/* GPRS LLC */
#define DLT_GPF_T		170	/* GPF-T (ITU-T G.7041/Y.1303) */
#define DLT_GPF_F		171	/* GPF-F (ITU-T G.7041/Y.1303) */

/*
 * Requested by Oolan Zimmer <oz@gcom.com> for use in Gcom's T1/E1 line
 * monitoring equipment.
 */
#define DLT_GCOM_T1E1		172
#define DLT_GCOM_SERIAL		173

/*
 * Juniper-private data link type, as per request from
 * Hannes Gredler <hannes@juniper.net>.  The DLT_ is used
 * for internal communication to Physical Interface Cards (PIC)
 */
#define DLT_JUNIPER_PIC_PEER    174

/*
 * Link types requested by Gregor Maier <gregor@endace.com> of Endace
 * Measurement Systems.  They add an ERF header (see
 * http://www.endace.com/support/EndaceRecordFormat.pdf) in front of
 * the link-layer header.
 */
#define DLT_ERF_ETH		175	/* Ethernet */
#define DLT_ERF_POS		176	/* Packet-over-SONET */

/*
 * Requested by Daniele Orlandi <daniele@orlandi.com> for raw LAPD
 * for vISDN (http://www.orlandi.com/visdn/).  Its link-layer header
 * includes additional information before the LAPD header, so it's
 * not necessarily a generic LAPD header.
 */
#define DLT_LINUX_LAPD		177

/*
 * The instruction encodings.
 */
/* instruction classes */
#define BPF_CLASS(code) ((code) & 0x07)
#define		BPF_LD		0x00
#define		BPF_LDX		0x01
#define		BPF_ST		0x02
#define		BPF_STX		0x03
#define		BPF_ALU		0x04
#define		BPF_JMP		0x05
#define		BPF_RET		0x06
#define		BPF_MISC	0x07

/* ld/ldx fields */
#define BPF_SIZE(code)	((code) & 0x18)
#define		BPF_W		0x00
#define		BPF_H		0x08
#define		BPF_B		0x10
#define BPF_MODE(code)	((code) & 0xe0)
#define		BPF_IMM 	0x00
#define		BPF_ABS		0x20
#define		BPF_IND		0x40
#define		BPF_MEM		0x60
#define		BPF_LEN		0x80
#define		BPF_MSH		0xa0

/* alu/jmp fields */
#define BPF_OP(code)	((code) & 0xf0)
#define		BPF_ADD		0x00
#define		BPF_SUB		0x10
#define		BPF_MUL		0x20
#define		BPF_DIV		0x30
#define		BPF_OR		0x40
#define		BPF_AND		0x50
#define		BPF_LSH		0x60
#define		BPF_RSH		0x70
#define		BPF_NEG		0x80
#define		BPF_JA		0x00
#define		BPF_JEQ		0x10
#define		BPF_JGT		0x20
#define		BPF_JGE		0x30
#define		BPF_JSET	0x40
#define BPF_SRC(code)	((code) & 0x08)
#define		BPF_K		0x00
#define		BPF_X		0x08

/* ret - BPF_K and BPF_X also apply */
#define BPF_RVAL(code)	((code) & 0x18)
#define		BPF_A		0x10

/* misc */
#define BPF_MISCOP(code) ((code) & 0xf8)
#define		BPF_TAX		0x00
#define		BPF_TXA		0x80

/*
 * The instruction data structure.
 */
struct bpf_insn {
	u_short		code;
	u_char		jt;
	u_char		jf;
	bpf_u_int32	k;
};

/*
 * Macros for insn array initializers.
 */
#define BPF_STMT(code, k) { (u_short)(code), 0, 0, k }
#define BPF_JUMP(code, k, jt, jf) { (u_short)(code), jt, jf, k }

/*
 * Structure to retrieve available DLTs for the interface.
 */
struct bpf_dltlist {
	u_int	bfl_len;	/* number of bfd_list array */
	u_int	*bfl_list;	/* array of DLTs */
};

#ifdef _KERNEL
struct bpf_if;
int	 bpf_validate(const struct bpf_insn *, int);
void	 bpf_tap(struct bpf_if *, u_char *, u_int);
void	 bpf_mtap(struct bpf_if *, struct mbuf *);
void	 bpf_mtap2(struct bpf_if *, void *, u_int, struct mbuf *);
void	 bpfattach(struct ifnet *, u_int, u_int);
void	 bpfattach2(struct ifnet *, u_int, u_int, struct bpf_if **);
void	 bpfdetach(struct ifnet *);

void	 bpfilterattach(int);
u_int	 bpf_filter(const struct bpf_insn *, u_char *, u_int, u_int);

#define	BPF_TAP(_ifp,_pkt,_pktlen) do {				\
	if ((_ifp)->if_bpf)					\
		bpf_tap((_ifp)->if_bpf, (_pkt), (_pktlen));	\
} while (0)
#define	BPF_MTAP(_ifp,_m) do {					\
	if ((_ifp)->if_bpf) {					\
		M_ASSERTVALID(_m);				\
		bpf_mtap((_ifp)->if_bpf, (_m));			\
	}							\
} while (0)
#define	BPF_MTAP2(_ifp,_data,_dlen,_m) do {			\
	if ((_ifp)->if_bpf) {					\
		M_ASSERTVALID(_m);				\
		bpf_mtap2((_ifp)->if_bpf,(_data),(_dlen),(_m));	\
	}							\
} while (0)
#endif

/*
 * Number of scratch memory words (for BPF_LD|BPF_MEM and BPF_ST).
 */
#define BPF_MEMWORDS 16

#endif /* _NET_BPF_H_ */
