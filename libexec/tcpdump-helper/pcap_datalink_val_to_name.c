/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1997, 1998
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 */

#include <sys/types.h>
#include <pcap/bpf.h>
#include <stdlib.h>

const char	*pcap_datalink_val_to_name(int dlt);

struct dlt_choice {
	const char *name;
	const char *description;
	int	dlt;
};

#define DLT_CHOICE(code, description) { #code, description, code }
#define DLT_CHOICE_SENTINEL { NULL, NULL, 0 }

static struct dlt_choice dlt_choices[] = {
	DLT_CHOICE(DLT_NULL, "BSD loopback"),
	DLT_CHOICE(DLT_EN10MB, "Ethernet"),
	DLT_CHOICE(DLT_IEEE802, "Token ring"),
	DLT_CHOICE(DLT_ARCNET, "BSD ARCNET"),
	DLT_CHOICE(DLT_SLIP, "SLIP"),
	DLT_CHOICE(DLT_PPP, "PPP"),
	DLT_CHOICE(DLT_FDDI, "FDDI"),
	DLT_CHOICE(DLT_ATM_RFC1483, "RFC 1483 LLC-encapsulated ATM"),
	DLT_CHOICE(DLT_RAW, "Raw IP"),
	DLT_CHOICE(DLT_SLIP_BSDOS, "BSD/OS SLIP"),
	DLT_CHOICE(DLT_PPP_BSDOS, "BSD/OS PPP"),
	DLT_CHOICE(DLT_ATM_CLIP, "Linux Classical IP-over-ATM"),
	DLT_CHOICE(DLT_PPP_SERIAL, "PPP over serial"),
	DLT_CHOICE(DLT_PPP_ETHER, "PPPoE"),
        DLT_CHOICE(DLT_SYMANTEC_FIREWALL, "Symantec Firewall"),
	DLT_CHOICE(DLT_C_HDLC, "Cisco HDLC"),
	DLT_CHOICE(DLT_IEEE802_11, "802.11"),
	DLT_CHOICE(DLT_FRELAY, "Frame Relay"),
	DLT_CHOICE(DLT_LOOP, "OpenBSD loopback"),
	DLT_CHOICE(DLT_ENC, "OpenBSD encapsulated IP"),
	DLT_CHOICE(DLT_LINUX_SLL, "Linux cooked"),
	DLT_CHOICE(DLT_LTALK, "Localtalk"),
	DLT_CHOICE(DLT_PFLOG, "OpenBSD pflog file"),
	DLT_CHOICE(DLT_PFSYNC, "Packet filter state syncing"),
	DLT_CHOICE(DLT_PRISM_HEADER, "802.11 plus Prism header"),
	DLT_CHOICE(DLT_IP_OVER_FC, "RFC 2625 IP-over-Fibre Channel"),
	DLT_CHOICE(DLT_SUNATM, "Sun raw ATM"),
	DLT_CHOICE(DLT_IEEE802_11_RADIO, "802.11 plus radiotap header"),
	DLT_CHOICE(DLT_ARCNET_LINUX, "Linux ARCNET"),
        DLT_CHOICE(DLT_JUNIPER_MLPPP, "Juniper Multi-Link PPP"),
	DLT_CHOICE(DLT_JUNIPER_MLFR, "Juniper Multi-Link Frame Relay"),
        DLT_CHOICE(DLT_JUNIPER_ES, "Juniper Encryption Services PIC"),
        DLT_CHOICE(DLT_JUNIPER_GGSN, "Juniper GGSN PIC"),
	DLT_CHOICE(DLT_JUNIPER_MFR, "Juniper FRF.16 Frame Relay"),
        DLT_CHOICE(DLT_JUNIPER_ATM2, "Juniper ATM2 PIC"),
        DLT_CHOICE(DLT_JUNIPER_SERVICES, "Juniper Advanced Services PIC"),
        DLT_CHOICE(DLT_JUNIPER_ATM1, "Juniper ATM1 PIC"),
	DLT_CHOICE(DLT_APPLE_IP_OVER_IEEE1394, "Apple IP-over-IEEE 1394"),
	DLT_CHOICE(DLT_MTP2_WITH_PHDR, "SS7 MTP2 with Pseudo-header"),
	DLT_CHOICE(DLT_MTP2, "SS7 MTP2"),
	DLT_CHOICE(DLT_MTP3, "SS7 MTP3"),
	DLT_CHOICE(DLT_SCCP, "SS7 SCCP"),
	DLT_CHOICE(DLT_DOCSIS, "DOCSIS"),
	DLT_CHOICE(DLT_LINUX_IRDA, "Linux IrDA"),
	DLT_CHOICE(DLT_IEEE802_11_RADIO_AVS, "802.11 plus AVS radio information header"),
        DLT_CHOICE(DLT_JUNIPER_MONITOR, "Juniper Passive Monitor PIC"),
	DLT_CHOICE(DLT_PPP_PPPD, "PPP for pppd, with direction flag"),
	DLT_CHOICE(DLT_JUNIPER_PPPOE, "Juniper PPPoE"),
	DLT_CHOICE(DLT_JUNIPER_PPPOE_ATM, "Juniper PPPoE/ATM"),
	DLT_CHOICE(DLT_GPRS_LLC, "GPRS LLC"),
	DLT_CHOICE(DLT_GPF_T, "GPF-T"),
	DLT_CHOICE(DLT_GPF_F, "GPF-F"),
	DLT_CHOICE(DLT_JUNIPER_PIC_PEER, "Juniper PIC Peer"),
	DLT_CHOICE(DLT_ERF_ETH,	"Ethernet with Endace ERF header"),
	DLT_CHOICE(DLT_ERF_POS, "Packet-over-SONET with Endace ERF header"),
	DLT_CHOICE(DLT_LINUX_LAPD, "Linux vISDN LAPD"),
	DLT_CHOICE(DLT_JUNIPER_ETHER, "Juniper Ethernet"),
	DLT_CHOICE(DLT_JUNIPER_PPP, "Juniper PPP"),
	DLT_CHOICE(DLT_JUNIPER_FRELAY, "Juniper Frame Relay"),
	DLT_CHOICE(DLT_JUNIPER_CHDLC, "Juniper C-HDLC"),
	DLT_CHOICE(DLT_MFR, "FRF.16 Frame Relay"),
	DLT_CHOICE(DLT_JUNIPER_VP, "Juniper Voice PIC"),
	DLT_CHOICE(DLT_A429, "Arinc 429"),
	DLT_CHOICE(DLT_A653_ICM, "Arinc 653 Interpartition Communication"),
	DLT_CHOICE(DLT_USB, "USB"),
	DLT_CHOICE(DLT_BLUETOOTH_HCI_H4, "Bluetooth HCI UART transport layer"),
	DLT_CHOICE(DLT_IEEE802_16_MAC_CPS, "IEEE 802.16 MAC Common Part Sublayer"),
	DLT_CHOICE(DLT_USB_LINUX, "USB with Linux header"),
	DLT_CHOICE(DLT_CAN20B, "Controller Area Network (CAN) v. 2.0B"),
	DLT_CHOICE(DLT_IEEE802_15_4_LINUX, "IEEE 802.15.4 with Linux padding"),
	DLT_CHOICE(DLT_PPI, "Per-Packet Information"),
	DLT_CHOICE(DLT_IEEE802_16_MAC_CPS_RADIO, "IEEE 802.16 MAC Common Part Sublayer plus radiotap header"),
	DLT_CHOICE(DLT_JUNIPER_ISM, "Juniper Integrated Service Module"),
	DLT_CHOICE(DLT_IEEE802_15_4, "IEEE 802.15.4 with FCS"),
	DLT_CHOICE(DLT_SITA, "SITA pseudo-header"),
	DLT_CHOICE(DLT_ERF, "Endace ERF header"),
	DLT_CHOICE(DLT_RAIF1, "Ethernet with u10 Networks pseudo-header"),
	DLT_CHOICE(DLT_IPMB, "IPMB"),
	DLT_CHOICE(DLT_JUNIPER_ST, "Juniper Secure Tunnel"),
	DLT_CHOICE(DLT_BLUETOOTH_HCI_H4_WITH_PHDR, "Bluetooth HCI UART transport layer plus pseudo-header"),
	DLT_CHOICE(DLT_AX25_KISS, "AX.25 with KISS header"),
	DLT_CHOICE(DLT_IEEE802_15_4_NONASK_PHY, "IEEE 802.15.4 with non-ASK PHY data"),
	DLT_CHOICE(DLT_MPLS, "MPLS with label as link-layer header"),
	DLT_CHOICE(DLT_USB_LINUX_MMAPPED, "USB with padded Linux header"),
	DLT_CHOICE(DLT_DECT, "DECT"),
	DLT_CHOICE(DLT_AOS, "AOS Space Data Link protocol"),
	DLT_CHOICE(DLT_WIHART, "Wireless HART"),
	DLT_CHOICE(DLT_FC_2, "Fibre Channel FC-2"),
	DLT_CHOICE(DLT_FC_2_WITH_FRAME_DELIMS, "Fibre Channel FC-2 with frame delimiters"),
	DLT_CHOICE(DLT_IPNET, "Solaris ipnet"),
	DLT_CHOICE(DLT_CAN_SOCKETCAN, "CAN-bus with SocketCAN headers"),
	DLT_CHOICE(DLT_IPV4, "Raw IPv4"),
	DLT_CHOICE(DLT_IPV6, "Raw IPv6"),
	DLT_CHOICE(DLT_IEEE802_15_4_NOFCS, "IEEE 802.15.4 without FCS"),
	DLT_CHOICE(DLT_JUNIPER_VS, "Juniper Virtual Server"),
	DLT_CHOICE(DLT_JUNIPER_SRX_E2E, "Juniper SRX E2E"),
	DLT_CHOICE(DLT_JUNIPER_FIBRECHANNEL, "Juniper Fibre Channel"),
	DLT_CHOICE(DLT_DVB_CI, "DVB-CI"),
	DLT_CHOICE(DLT_JUNIPER_ATM_CEMIC, "Juniper ATM CEMIC"),
	DLT_CHOICE(DLT_NFLOG, "Linux netfilter log messages"),
	DLT_CHOICE(DLT_NETANALYZER, "Ethernet with Hilscher netANALYZER pseudo-header"),
	DLT_CHOICE(DLT_NETANALYZER_TRANSPARENT, "Ethernet with Hilscher netANALYZER pseudo-header and with preamble and SFD"),
	DLT_CHOICE(DLT_IPOIB, "RFC 4391 IP-over-Infiniband"),
	DLT_CHOICE_SENTINEL
};

const char *
pcap_datalink_val_to_name(int dlt)
{
	int i;

	for (i = 0; dlt_choices[i].name != NULL; i++) {
		if (dlt_choices[i].dlt == dlt)
			return (dlt_choices[i].name + sizeof("DLT_") - 1);
	}
	return (NULL);
}
