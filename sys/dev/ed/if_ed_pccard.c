/*-
 * Copyright (c) 2005, M. Warner Losh
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Notes for adding media support.  Each chipset is somewhat different
 * from the others.  Linux has a table of OIDs that it uses to see what
 * supports the misc register of the NS83903.  But a sampling of datasheets
 * I could dig up on cards I own paints a different picture.
 *
 * Chipset specific details:
 * NS 83903/902A paired
 *    ccr base 0x1020
 *    id register at 0x1000: 7-3 = 0, 2-0 = 1.
 *	(maybe this test is too week)
 *    misc register at 0x018:
 *	6 WAIT_TOUTENABLE enable watchdog timeout
 *	3 AUI/TPI 1 AUX, 0 TPI
 *	2 loopback
 *      1 gdlink (tpi mode only) 1 tp good, 0 tp bad
 *	0 0-no mam, 1 mam connected
 *
 * NS83926 appears to be a NS pcmcia glue chip used on the IBM Ethernet II
 * and the NEC PC9801N-J12 ccr base 0x2000!
 *
 * winbond 289c926
 *    ccr base 0xfd0
 *    cfb (am 0xff2):
 *	0-1 PHY01	00 TPI, 01 10B2, 10 10B5, 11 TPI (reduced squ)
 *	2 LNKEN		0 - enable link and auto switch, 1 disable
 *	3 LNKSTS	TPI + LNKEN=0 + link good == 1, else 0
 *    sr (am 0xff4)
 *	88 00 88 00 88 00, etc
 *
 * TMI tc3299a (cr PHY01 == 0)
 *    ccr base 0x3f8
 *    cra (io 0xa)
 *    crb (io 0xb)
 *	0-1 PHY01	00 auto, 01 res, 10 10B5, 11 TPI
 *	2 GDLINK	1 disable checking of link
 *	6 LINK		0 bad link, 1 good link
 *
 * EN5017A, EN5020	no data, but very popular
 * Other chips?
 * NetBSD supports RTL8019, but none have surfaced that I can see
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_mib.h>
#include <net/if_media.h>

#include <dev/ed/if_edreg.h>
#include <dev/ed/if_edvar.h>
#include <dev/ed/ax88x90reg.h>
#include <dev/ed/dl100xxreg.h>
#include <dev/ed/tc5299jreg.h>
#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccard_cis.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "card_if.h"
/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"
#include "pccarddevs.h"

/*
 * NE-2000 based PC Cards have a number of ways to get the MAC address.
 * Some cards encode this as a FUNCE.  Others have this in the ROMs the
 * same way that ISA cards do.  Some have it encoded in the attribute
 * memory somewhere that isn't in the CIS.  Some new chipsets have it
 * in special registers in the ASIC part of the chip.
 *
 * For those cards that have the MAC adress stored in attribute memory
 * outside of a FUNCE entry in the CIS, nearly all of them have it at
 * a fixed offset (0xff0).  We use that offset as a source of last
 * resource if other offsets have failed.  This is the address of the
 * National Semiconductor DP83903A, which is the only chip's datasheet
 * I've found.
 */
#define ED_DEFAULT_MAC_OFFSET	0xff0

static const struct ed_product {
	struct pccard_product	prod;
	int flags;
#define	NE2000DVF_DL100XX	0x0001		/* chip is D-Link DL10019/22 */
#define	NE2000DVF_AX88X90	0x0002		/* chip is ASIX AX88[17]90 */
#define NE2000DVF_TC5299J	0x0004		/* chip is Tamarack TC5299J */
#define NE2000DVF_TOSHIBA	0x0008		/* Toshiba DP83902A */
#define NE2000DVF_ENADDR	0x0100		/* Get MAC from attr mem */
#define NE2000DVF_ANYFUNC	0x0200		/* Allow any function type */
#define NE2000DVF_MODEM		0x0400		/* Has a modem/serial */
	int enoff;
} ed_pccard_products[] = {
	{ PCMCIA_CARD(ACCTON, EN2212), 0},
	{ PCMCIA_CARD(ACCTON, EN2216), 0},
	{ PCMCIA_CARD(ALLIEDTELESIS, LA_PCM), 0},
	{ PCMCIA_CARD(AMBICOM, AMB8002T), 0},
	{ PCMCIA_CARD(BILLIONTON, CFLT10N), 0},
	{ PCMCIA_CARD(BILLIONTON, LNA100B), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(BILLIONTON, LNT10TN), 0},
	{ PCMCIA_CARD(BROMAX, AXNET), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(BROMAX, IPORT), 0},
	{ PCMCIA_CARD(BROMAX, IPORT2), 0},
	{ PCMCIA_CARD(BUFFALO, LPC2_CLT), 0},
	{ PCMCIA_CARD(BUFFALO, LPC3_CLT), 0},
	{ PCMCIA_CARD(BUFFALO, LPC3_CLX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(BUFFALO, LPC4_TX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(BUFFALO, LPC4_CLX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(BUFFALO, LPC_CF_CLT), 0},
	{ PCMCIA_CARD(CNET, NE2000), 0},
	{ PCMCIA_CARD(COMPEX, AX88190), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(COMPEX, LANMODEM), 0},
	{ PCMCIA_CARD(COMPEX, LINKPORT_ENET_B), 0},
	{ PCMCIA_CARD(COREGA, ETHER_II_PCC_T), 0},
	{ PCMCIA_CARD(COREGA, ETHER_II_PCC_TD), 0},
	{ PCMCIA_CARD(COREGA, ETHER_PCC_T), 0},
	{ PCMCIA_CARD(COREGA, ETHER_PCC_TD), 0},
	{ PCMCIA_CARD(COREGA, FAST_ETHER_PCC_TX), NE2000DVF_DL100XX},
	{ PCMCIA_CARD(COREGA, FETHER_PCC_TXD), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(COREGA, FETHER_PCC_TXF), NE2000DVF_DL100XX},
	{ PCMCIA_CARD(COREGA, FETHER_II_PCC_TXD), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(COREGA, LAPCCTXD), 0},
	{ PCMCIA_CARD(DAYNA, COMMUNICARD_E_1), 0},
	{ PCMCIA_CARD(DAYNA, COMMUNICARD_E_2), 0},
	{ PCMCIA_CARD(DLINK, DE650), NE2000DVF_ANYFUNC },
	{ PCMCIA_CARD(DLINK, DE660), 0 },
	{ PCMCIA_CARD(DLINK, DE660PLUS), 0},
	{ PCMCIA_CARD(DYNALINK, L10C), 0},
	{ PCMCIA_CARD(EDIMAX, EP4000A), 0},
	{ PCMCIA_CARD(EPSON, EEN10B), 0},
	{ PCMCIA_CARD(EXP, THINLANCOMBO), 0},
	{ PCMCIA_CARD(GLOBALVILLAGE, LANMODEM), 0},
	{ PCMCIA_CARD(GREY_CELL, TDK3000), 0},
	{ PCMCIA_CARD(GREY_CELL, DMF650TX),
	    NE2000DVF_ANYFUNC | NE2000DVF_DL100XX | NE2000DVF_MODEM},
	{ PCMCIA_CARD(GVC, NIC_2000P), 0},
	{ PCMCIA_CARD(IBM, HOME_AND_AWAY), 0},
	{ PCMCIA_CARD(IBM, INFOMOVER), 0},
	{ PCMCIA_CARD(IODATA3, PCLAT), 0},
	{ PCMCIA_CARD(KINGSTON, CIO10T), 0},
	{ PCMCIA_CARD(KINGSTON, KNE2), 0},
	{ PCMCIA_CARD(LANTECH, FASTNETTX), NE2000DVF_AX88X90},
	/* Same ID for many different cards, including generic NE2000 */
	{ PCMCIA_CARD(LINKSYS, COMBO_ECARD),
	    NE2000DVF_DL100XX | NE2000DVF_AX88X90},
	{ PCMCIA_CARD(LINKSYS, ECARD_1), 0},
	{ PCMCIA_CARD(LINKSYS, ECARD_2), 0},
	{ PCMCIA_CARD(LINKSYS, ETHERFAST), NE2000DVF_DL100XX},
	{ PCMCIA_CARD(LINKSYS, TRUST_COMBO_ECARD), 0},
	{ PCMCIA_CARD(MACNICA, ME1_JEIDA), 0},
	{ PCMCIA_CARD(MAGICRAM, ETHER), 0},
	{ PCMCIA_CARD(MELCO, LPC3_CLX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(MELCO, LPC3_TX), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(MITSUBISHI, B8895), NE2000DVF_ANYFUNC}, /* NG */
	{ PCMCIA_CARD(MICRORESEARCH, MR10TPC), 0},
	{ PCMCIA_CARD(NDC, ND5100_E), 0},
	{ PCMCIA_CARD(NETGEAR, FA410TXC), NE2000DVF_DL100XX},
	/* Same ID as DLINK DFE-670TXD.  670 has DL10022, fa411 has ax88790 */
	{ PCMCIA_CARD(NETGEAR, FA411), NE2000DVF_AX88X90 | NE2000DVF_DL100XX},
	{ PCMCIA_CARD(NEXTCOM, NEXTHAWK), 0},
	{ PCMCIA_CARD(NEWMEDIA, LANSURFER), NE2000DVF_ANYFUNC},
	{ PCMCIA_CARD(NEWMEDIA, LIVEWIRE), 0},
	{ PCMCIA_CARD(OEM2, ETHERNET), 0},
	{ PCMCIA_CARD(OEM2, FAST_ETHERNET), NE2000DVF_AX88X90 },
	{ PCMCIA_CARD(OEM2, NE2000), 0},
	{ PCMCIA_CARD(PLANET, SMARTCOM2000), 0 },
	{ PCMCIA_CARD(PREMAX, PE200), 0},
	{ PCMCIA_CARD(PSION, LANGLOBAL),
	    NE2000DVF_ANYFUNC | NE2000DVF_AX88X90 | NE2000DVF_MODEM},
	{ PCMCIA_CARD(RACORE, ETHERNET), 0},
	{ PCMCIA_CARD(RACORE, FASTENET), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(RACORE, 8041TX), NE2000DVF_AX88X90 | NE2000DVF_TC5299J},
	{ PCMCIA_CARD(RELIA, COMBO), 0},
	{ PCMCIA_CARD(RIOS, PCCARD3), 0},
	{ PCMCIA_CARD(RPTI, EP400), 0},
	{ PCMCIA_CARD(RPTI, EP401), 0},
	{ PCMCIA_CARD(SMC, EZCARD), 0},
	{ PCMCIA_CARD(SOCKET, EA_ETHER), 0},
	{ PCMCIA_CARD(SOCKET, ES_1000), 0},
	{ PCMCIA_CARD(SOCKET, LP_ETHER), 0},
	{ PCMCIA_CARD(SOCKET, LP_ETHER_CF), 0},
	{ PCMCIA_CARD(SOCKET, LP_ETH_10_100_CF), NE2000DVF_DL100XX},
	{ PCMCIA_CARD(SVEC, COMBOCARD), 0},
	{ PCMCIA_CARD(SVEC, LANCARD), 0},
	{ PCMCIA_CARD(TAMARACK, ETHERNET), 0},
	{ PCMCIA_CARD(TDK, CFE_10), 0},
	{ PCMCIA_CARD(TDK, LAK_CD031), 0},
	{ PCMCIA_CARD(TDK, DFL5610WS), 0},
	{ PCMCIA_CARD(TELECOMDEVICE, LM5LT), 0 },
	{ PCMCIA_CARD(TELECOMDEVICE, TCD_HPC100), NE2000DVF_AX88X90},
	{ PCMCIA_CARD(TJ, PTJ_LAN_T), 0 },
	{ PCMCIA_CARD(TOSHIBA2, LANCT00A), NE2000DVF_ANYFUNC | NE2000DVF_TOSHIBA},
	{ PCMCIA_CARD(ZONET, ZEN), 0},
	{ { NULL } }
};

/*
	PCMCIA_PFC_DEVICE_MANF_CARD(0, 0x08a1, 0xc0ab),
	PCMCIA_DEVICE_MANF_CARD(0x0213, 0x2452),

	PCMCIA_PFC_DEVICE_PROD_ID12(0, "AnyCom", "Fast Ethernet + 56K COMBO", 0x578ba6e7, 0xb0ac62c4),
	PCMCIA_PFC_DEVICE_PROD_ID12(0, "D-Link", "DME336T", 0x1a424a1c, 0xb23897ff),
	PCMCIA_PFC_DEVICE_PROD_ID12(0, "Grey Cell", "GCS3000", 0x2a151fac, 0x48b932ae),
	PCMCIA_PFC_DEVICE_PROD_ID12(0, "Linksys", "EtherFast 10&100 + 56K PC Card (PCMLM56)", 0x0733cc81, 0xb3765033),
	PCMCIA_PFC_DEVICE_PROD_ID12(0, "LINKSYS", "PCMLM336", 0xf7cb0b07, 0x7a821b58),
	PCMCIA_PFC_DEVICE_PROD_ID12(0, "PCMCIAs", "ComboCard", 0xdcfe12d3, 0xcd8906cc),
	PCMCIA_PFC_DEVICE_PROD_ID12(0, "PCMCIAs", "LanModem", 0xdcfe12d3, 0xc67c648f),
	PCMCIA_MFC_DEVICE_PROD_ID123(0, "APEX DATA", "MULTICARD", "ETHERNET-MODEM", 0x11c2da09, 0x7289dc5d, 0xaad95e1f),
	PCMCIA_MFC_DEVICE_PROD_ID2(0, "FAX/Modem/Ethernet Combo Card ", 0x1ed59302),
	PCMCIA_DEVICE_PROD_ID12("2408LAN", "Ethernet", 0x352fff7f, 0x00b2e941),
	PCMCIA_DEVICE_PROD_ID1234("Socket", "CF 10/100 Ethernet Card", "Revision B", "05/11/06", 0xb38bcc2e, 0x4de88352, 0xeaca6c8d, 0x7e57c22e),
	PCMCIA_DEVICE_PROD_ID123("Cardwell", "PCMCIA", "ETHERNET", 0x9533672e, 0x281f1c5d, 0x3ff7175b),
	PCMCIA_DEVICE_PROD_ID123("CNet  ", "CN30BC", "ETHERNET", 0x9fe55d3d, 0x85601198, 0x3ff7175b),
	PCMCIA_DEVICE_PROD_ID123("Digital", "Ethernet", "Adapter", 0x9999ab35, 0x00b2e941, 0x4b0d829e),
	PCMCIA_DEVICE_PROD_ID123("Edimax Technology Inc.", "PCMCIA", "Ethernet Card", 0x738a0019, 0x281f1c5d, 0x5e9d92c0),
	PCMCIA_DEVICE_PROD_ID123("EFA   ", "EFA207", "ETHERNET", 0x3d294be4, 0xeb9aab6c, 0x3ff7175b),
	PCMCIA_DEVICE_PROD_ID123("I-O DATA", "PCLA", "ETHERNET", 0x1d55d7ec, 0xe4c64d34, 0x3ff7175b),
	PCMCIA_DEVICE_PROD_ID123("IO DATA", "PCLATE", "ETHERNET", 0x547e66dc, 0x6b260753, 0x3ff7175b),
	PCMCIA_DEVICE_PROD_ID123("KingMax Technology Inc.", "EN10-T2", "PCMCIA Ethernet Card", 0x932b7189, 0x699e4436, 0x6f6652e0),
	PCMCIA_DEVICE_PROD_ID123("PCMCIA", "PCMCIA-ETHERNET-CARD", "UE2216", 0x281f1c5d, 0xd4cd2f20, 0xb87add82),
	PCMCIA_DEVICE_PROD_ID123("PCMCIA", "PCMCIA-ETHERNET-CARD", "UE2620", 0x281f1c5d, 0xd4cd2f20, 0x7d3d83a8),
	PCMCIA_DEVICE_PROD_ID1("2412LAN", 0x67f236ab),
	PCMCIA_DEVICE_PROD_ID12("Allied Telesis, K.K.", "CentreCOM LA100-PCM-T V2 100/10M LAN PC Card", 0xbb7fbdd7, 0xcd91cc68),
	PCMCIA_DEVICE_PROD_ID12("Allied Telesis K.K.", "LA100-PCM V2", 0x36634a66, 0xc6d05997),
  	PCMCIA_DEVICE_PROD_ID12("Allied Telesis, K.K.", "CentreCOM LA-PCM_V2", 0xbb7fBdd7, 0x28e299f8),
	PCMCIA_DEVICE_PROD_ID12("Allied Telesis K.K.", "LA-PCM V3", 0x36634a66, 0x62241d96),
	PCMCIA_DEVICE_PROD_ID12("AmbiCom", "AMB8010", 0x5070a7f9, 0x82f96e96),
	PCMCIA_DEVICE_PROD_ID12("AmbiCom", "AMB8610", 0x5070a7f9, 0x86741224),
	PCMCIA_DEVICE_PROD_ID12("AmbiCom Inc", "AMB8002", 0x93b15570, 0x75ec3efb),
	PCMCIA_DEVICE_PROD_ID12("AmbiCom Inc", "AMB8002T", 0x93b15570, 0x461c5247),
	PCMCIA_DEVICE_PROD_ID12("AmbiCom Inc", "AMB8010", 0x93b15570, 0x82f96e96),
	PCMCIA_DEVICE_PROD_ID12("AnyCom", "ECO Ethernet", 0x578ba6e7, 0x0a9888c1),
	PCMCIA_DEVICE_PROD_ID12("AnyCom", "ECO Ethernet 10/100", 0x578ba6e7, 0x939fedbd),
	PCMCIA_DEVICE_PROD_ID12("AROWANA", "PCMCIA Ethernet LAN Card", 0x313adbc8, 0x08d9f190),
	PCMCIA_DEVICE_PROD_ID12("ASANTE", "FriendlyNet PC Card", 0x3a7ade0f, 0x41c64504),
	PCMCIA_DEVICE_PROD_ID12("Billionton", "LNT-10TB", 0x552ab682, 0xeeb1ba6a),
	PCMCIA_DEVICE_PROD_ID12("CF", "10Base-Ethernet", 0x44ebf863, 0x93ae4d79),
	PCMCIA_DEVICE_PROD_ID12("COMPU-SHACK", "BASEline PCMCIA 10 MBit Ethernetadapter", 0xfa2e424d, 0xe9190d8a),
	PCMCIA_DEVICE_PROD_ID12("COMPU-SHACK", "FASTline PCMCIA 10/100 Fast-Ethernet", 0xfa2e424d, 0x3953d9b9),
	PCMCIA_DEVICE_PROD_ID12("CONTEC", "C-NET(PC)C-10L", 0x21cab552, 0xf6f90722),
	PCMCIA_DEVICE_PROD_ID12("Corega,K.K.", "Ethernet LAN Card", 0x110d26d9, 0x9fd2f0a2),
	PCMCIA_DEVICE_PROD_ID12("corega,K.K.", "Ethernet LAN Card", 0x9791a90e, 0x9fd2f0a2),
	PCMCIA_DEVICE_PROD_ID12("CouplerlessPCMCIA", "100BASE", 0xee5af0ad, 0x7c2add04),
	PCMCIA_DEVICE_PROD_ID12("CyQ've", "ELA-110E 10/100M LAN Card", 0x77008979, 0xfd184814),
	PCMCIA_DEVICE_PROD_ID12("DataTrek.", "NetCard ", 0x5cd66d9d, 0x84697ce0),
	PCMCIA_DEVICE_PROD_ID12("Dayna Communications, Inc.", "CommuniCard E", 0x0c629325, 0xb4e7dbaf),
	PCMCIA_DEVICE_PROD_ID12("Digicom", "Palladio LAN 10/100", 0x697403d8, 0xe160b995),
	PCMCIA_DEVICE_PROD_ID12("Digicom", "Palladio LAN 10/100 Dongless", 0x697403d8, 0xa6d3b233),
	PCMCIA_DEVICE_PROD_ID12("D-Link", "DFE-650", 0x1a424a1c, 0x0f0073f9),
	PCMCIA_DEVICE_PROD_ID12("Dual Speed", "10/100 PC Card", 0x725b842d, 0xf1efee84),
	PCMCIA_DEVICE_PROD_ID12("Dual Speed", "10/100 Port Attached PC Card", 0x725b842d, 0x2db1f8e9),
	PCMCIA_DEVICE_PROD_ID12("Dynalink", "L10BC", 0x55632fd5, 0xdc65f2b1),
	PCMCIA_DEVICE_PROD_ID12("DYNALINK", "L10BC", 0x6a26d1cf, 0xdc65f2b1),
	PCMCIA_DEVICE_PROD_ID12("E-CARD", "E-CARD", 0x6701da11, 0x6701da11),
	PCMCIA_DEVICE_PROD_ID12("EIGER Labs Inc.", "Ethernet 10BaseT card", 0x53c864c6, 0xedd059f6),
	PCMCIA_DEVICE_PROD_ID12("EIGER Labs Inc.", "Ethernet Combo card", 0x53c864c6, 0x929c486c),
	PCMCIA_DEVICE_PROD_ID12("Ethernet", "Adapter", 0x00b2e941, 0x4b0d829e),
	PCMCIA_DEVICE_PROD_ID12("Ethernet Adapter", "E2000 PCMCIA Ethernet", 0x96767301, 0x71fbbc61),
	PCMCIA_DEVICE_PROD_ID12("Ethernet PCMCIA adapter", "EP-210", 0x8dd86181, 0xf2b52517),
	PCMCIA_DEVICE_PROD_ID12("Fast Ethernet", "Adapter", 0xb4be14e3, 0x4b0d829e),
	PCMCIA_DEVICE_PROD_ID12("Grey Cell", "GCS2000", 0x2a151fac, 0xf00555cb),
	PCMCIA_DEVICE_PROD_ID12("Grey Cell", "GCS2220", 0x2a151fac, 0xc1b7e327),
	PCMCIA_DEVICE_PROD_ID12("GVC", "NIC-2000p", 0x76e171bd, 0x6eb1c947),
	PCMCIA_DEVICE_PROD_ID12("IBM Corp.", "Ethernet", 0xe3736c88, 0x00b2e941),
	PCMCIA_DEVICE_PROD_ID12("IC-CARD", "IC-CARD", 0x60cb09a6, 0x60cb09a6),
	PCMCIA_DEVICE_PROD_ID12("IC-CARD+", "IC-CARD+", 0x93693494, 0x93693494),
	PCMCIA_DEVICE_PROD_ID12("IO DATA", "PCETTX", 0x547e66dc, 0x6fc5459b),
	PCMCIA_DEVICE_PROD_ID12("iPort", "10/100 Ethernet Card", 0x56c538d2, 0x11b0ffc0),
	PCMCIA_DEVICE_PROD_ID12("KANSAI ELECTRIC CO.,LTD", "KLA-PCM/T", 0xb18dc3b4, 0xcc51a956),
	PCMCIA_DEVICE_PROD_ID12("KCI", "PE520 PCMCIA Ethernet Adapter", 0xa89b87d3, 0x1eb88e64),
	PCMCIA_DEVICE_PROD_ID12("KINGMAX", "EN10T2T", 0x7bcb459a, 0xa5c81fa5),
	PCMCIA_DEVICE_PROD_ID12("Kingston", "KNE-PC2", 0x1128e633, 0xce2a89b3),
	PCMCIA_DEVICE_PROD_ID12("Kingston Technology Corp.", "EtheRx PC Card Ethernet Adapter", 0x313c7be3, 0x0afb54a2),
	PCMCIA_DEVICE_PROD_ID12("Laneed", "LD-10/100CD", 0x1b7827b2, 0xcda71d1c),
	PCMCIA_DEVICE_PROD_ID12("Laneed", "LD-CDF", 0x1b7827b2, 0xfec71e40),
	PCMCIA_DEVICE_PROD_ID12("Laneed", "LD-CDL/T", 0x1b7827b2, 0x79fba4f7),
	PCMCIA_DEVICE_PROD_ID12("Laneed", "LD-CDS", 0x1b7827b2, 0x931afaab),
	PCMCIA_DEVICE_PROD_ID12("LEMEL", "LM-N89TX PRO", 0xbbefb52f, 0xd2897a97),
	PCMCIA_DEVICE_PROD_ID12("Linksys", "Combo PCMCIA EthernetCard (EC2T)", 0x0733cc81, 0x32ee8c78),
	PCMCIA_DEVICE_PROD_ID12("Linksys", "EtherFast 10/100 Integrated PC Card (PCM100)", 0x0733cc81, 0x453c3f9d),
	PCMCIA_DEVICE_PROD_ID12("Linksys", "EtherFast 10/100 PC Card (PCMPC100)", 0x0733cc81, 0x66c5a389),
	PCMCIA_DEVICE_PROD_ID12("Linksys", "EtherFast 10/100 PC Card (PCMPC100 V2)", 0x0733cc81, 0x3a3b28e9),
	PCMCIA_DEVICE_PROD_ID12("Linksys", "HomeLink Phoneline + 10/100 Network PC Card (PCM100H1)", 0x733cc81, 0x7a3e5c3a),
	PCMCIA_DEVICE_PROD_ID12("Logitec", "LPM-LN100TX", 0x88fcdeda, 0x6d772737),
	PCMCIA_DEVICE_PROD_ID12("Logitec", "LPM-LN100TE", 0x88fcdeda, 0x0e714bee),
	PCMCIA_DEVICE_PROD_ID12("Logitec", "LPM-LN20T", 0x88fcdeda, 0x81090922),
	PCMCIA_DEVICE_PROD_ID12("Logitec", "LPM-LN10TE", 0x88fcdeda, 0xc1e2521c),
	PCMCIA_DEVICE_PROD_ID12("LONGSHINE", "PCMCIA Ethernet Card", 0xf866b0b0, 0x6f6652e0),
	PCMCIA_DEVICE_PROD_ID12("MACNICA", "ME1-JEIDA", 0x20841b68, 0xaf8a3578),
	PCMCIA_DEVICE_PROD_ID12("Macsense", "MPC-10", 0xd830297f, 0xd265c307),
	PCMCIA_DEVICE_PROD_ID12("Matsushita Electric Industrial Co.,LTD.", "CF-VEL211", 0x44445376, 0x8ded41d4),
	PCMCIA_DEVICE_PROD_ID12("MAXTECH", "PCN2000", 0x78d64bc0, 0xca0ca4b8),
	PCMCIA_DEVICE_PROD_ID12("MELCO", "LPC2-T", 0x481e0094, 0xa2eb0cf3),
	PCMCIA_DEVICE_PROD_ID12("Microcom C.E.", "Travel Card LAN 10/100", 0x4b91cec7, 0xe70220d6),
	PCMCIA_DEVICE_PROD_ID12("Microdyne", "NE4200", 0x2e6da59b, 0x0478e472),
	PCMCIA_DEVICE_PROD_ID12("MIDORI ELEC.", "LT-PCMT", 0x648d55c1, 0xbde526c7),
	PCMCIA_DEVICE_PROD_ID12("National Semiconductor", "InfoMover 4100", 0x36e1191f, 0x60c229b9),
	PCMCIA_DEVICE_PROD_ID12("National Semiconductor", "InfoMover NE4100", 0x36e1191f, 0xa6617ec8),
	PCMCIA_DEVICE_PROD_ID12("Network Everywhere", "Fast Ethernet 10/100 PC Card", 0x820a67b6, 0x31ed1a5f),
	PCMCIA_DEVICE_PROD_ID12("NextCom K.K.", "Next Hawk", 0xaedaec74, 0xad050ef1),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA", "10/100Mbps Ethernet Card", 0x281f1c5d, 0x6e41773b),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA", "ETHERNET", 0x281f1c5d, 0x3ff7175b),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA", "Ethernet 10BaseT Card", 0x281f1c5d, 0x4de2f6c8),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA", "Ethernet Card", 0x281f1c5d, 0x5e9d92c0),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA", "Ethernet Combo card", 0x281f1c5d, 0x929c486c),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA", "ETHERNET V1.0", 0x281f1c5d, 0x4d8817c8),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA", "FastEthernet", 0x281f1c5d, 0xfe871eeb),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA", "Fast-Ethernet", 0x281f1c5d, 0x45f1f3b4),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA LAN", "Ethernet", 0x7500e246, 0x00b2e941),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA", "LNT-10TN", 0x281f1c5d, 0xe707f641),
	PCMCIA_DEVICE_PROD_ID12("PCMCIAs", "ComboCard", 0xdcfe12d3, 0xcd8906cc),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA", "UE2212", 0x281f1c5d, 0xbf17199b),
	PCMCIA_DEVICE_PROD_ID12("PCMCIA", "    Ethernet NE2000 Compatible", 0x281f1c5d, 0x42d5d7e1),
	PCMCIA_DEVICE_PROD_ID12("PRETEC", "Ethernet CompactLAN 10baseT 3.3V", 0xebf91155, 0x30074c80),
	PCMCIA_DEVICE_PROD_ID12("PRETEC", "Ethernet CompactLAN 10BaseT 3.3V", 0xebf91155, 0x7f5a4f50),
	PCMCIA_DEVICE_PROD_ID12("Psion Dacom", "Gold Card Ethernet", 0xf5f025c2, 0x3a30e110),
	PCMCIA_DEVICE_PROD_ID12("=RELIA==", "Ethernet", 0xcdd0644a, 0x00b2e941),
	PCMCIA_DEVICE_PROD_ID12("RP", "1625B Ethernet NE2000 Compatible", 0xe3e66e22, 0xb96150df),
	PCMCIA_DEVICE_PROD_ID12("RPTI", "EP400 Ethernet NE2000 Compatible", 0xdc6f88fd, 0x4a7e2ae0),
	PCMCIA_DEVICE_PROD_ID12("RPTI", "EP401 Ethernet NE2000 Compatible", 0xdc6f88fd, 0x4bcbd7fd),
	PCMCIA_DEVICE_PROD_ID12("RPTI LTD.", "EP400", 0xc53ac515, 0x81e39388),
	PCMCIA_DEVICE_PROD_ID12("SCM", "Ethernet Combo card", 0xbdc3b102, 0x929c486c),
	PCMCIA_DEVICE_PROD_ID12("Seiko Epson Corp.", "Ethernet", 0x09928730, 0x00b2e941),
	PCMCIA_DEVICE_PROD_ID12("SMC", "EZCard-10-PCMCIA", 0xc4f8b18b, 0xfb21d265),
	PCMCIA_DEVICE_PROD_ID12("Telecom Device K.K.", "SuperSocket RE450T", 0x466b05f0, 0x8b74bc4f),
	PCMCIA_DEVICE_PROD_ID12("Telecom Device K.K.", "SuperSocket RE550T", 0x466b05f0, 0x33c8db2a),
	PCMCIA_DEVICE_PROD_ID13("Hypertec",  "EP401", 0x8787bec7, 0xf6e4a31e),
	PCMCIA_DEVICE_PROD_ID13("KingMax Technology Inc.", "Ethernet Card", 0x932b7189, 0x5e9d92c0),
	PCMCIA_DEVICE_PROD_ID13("LONGSHINE", "EP401", 0xf866b0b0, 0xf6e4a31e),
	PCMCIA_DEVICE_PROD_ID1("CyQ've 10 Base-T LAN CARD", 0x94faf360),
	PCMCIA_DEVICE_PROD_ID1("EP-210 PCMCIA LAN CARD.", 0x8850b4de),
	PCMCIA_DEVICE_PROD_ID1("ETHER-C16", 0x06a8514f),
	PCMCIA_DEVICE_PROD_ID1("NE2000 Compatible", 0x75b8ad5a),
	PCMCIA_DEVICE_PROD_ID2("EN-6200P2", 0xa996d078),


	PCMCIA_PFC_DEVICE_PROD_ID12(0, "MICRO RESEARCH", "COMBO-L/M-336", 0xb2ced065, 0x3ced0555),
 */

/*
 *      PC Card (PCMCIA) specific code.
 */
static int	ed_pccard_probe(device_t);
static int	ed_pccard_attach(device_t);
static void	ed_pccard_tick(void *);

static int	ed_pccard_dl100xx(device_t dev, const struct ed_product *);
static void	ed_pccard_dl100xx_mii_reset(struct ed_softc *sc);
static u_int	ed_pccard_dl100xx_mii_readbits(struct ed_softc *sc, int nbits);
static void	ed_pccard_dl100xx_mii_writebits(struct ed_softc *sc, u_int val,
    int nbits);

static int	ed_pccard_ax88x90(device_t dev, const struct ed_product *);
static u_int	ed_pccard_ax88x90_mii_readbits(struct ed_softc *sc, int nbits);
static void	ed_pccard_ax88x90_mii_writebits(struct ed_softc *sc, u_int val,
    int nbits);

static int	ed_miibus_readreg(device_t dev, int phy, int reg);
static int	ed_ifmedia_upd(struct ifnet *);
static void	ed_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static int	ed_pccard_tc5299j(device_t dev, const struct ed_product *);
static u_int	ed_pccard_tc5299j_mii_readbits(struct ed_softc *sc, int nbits);
static void	ed_pccard_tc5299j_mii_writebits(struct ed_softc *sc, u_int val,
    int nbits);

static void
ed_pccard_print_entry(const struct ed_product *pp)
{
	int i;

	printf("Product entry: ");
	if (pp->prod.pp_name)
		printf("name='%s',", pp->prod.pp_name);
	printf("vendor=%#x,product=%#x", pp->prod.pp_vendor,
	    pp->prod.pp_product);
	for (i = 0; i < 4; i++)
		if (pp->prod.pp_cis[i])
			printf(",CIS%d='%s'", i, pp->prod.pp_cis[i]);
	printf("\n");
}

static int
ed_pccard_probe(device_t dev)
{
	const struct ed_product *pp, *pp2;
	int		error, first = 1;
	uint32_t	fcn = PCCARD_FUNCTION_UNSPEC;

	/* Make sure we're a network function */
	error = pccard_get_function(dev, &fcn);
	if (error != 0)
		return (error);

	if ((pp = (const struct ed_product *) pccard_product_lookup(dev, 
	    (const struct pccard_product *) ed_pccard_products,
	    sizeof(ed_pccard_products[0]), NULL)) != NULL) {
		if (pp->prod.pp_name != NULL)
			device_set_desc(dev, pp->prod.pp_name);
		/*
		 * Some devices don't ID themselves as network, but
		 * that's OK if the flags say so.
		 */
		if (!(pp->flags & NE2000DVF_ANYFUNC) &&
		    fcn != PCCARD_FUNCTION_NETWORK)
			return (ENXIO);
		/*
		 * Some devices match multiple entries.  Report that
		 * as a warning to help cull the table
		 */
		pp2 = pp;
		while ((pp2 = (const struct ed_product *)pccard_product_lookup(
		    dev, (const struct pccard_product *)(pp2 + 1),
		    sizeof(ed_pccard_products[0]), NULL)) != NULL) {
			if (first) {
				device_printf(dev,
    "Warning: card matches multiple entries.  Report to imp@freebsd.org\n");
				ed_pccard_print_entry(pp);
				first = 0;
			}
			ed_pccard_print_entry(pp2);
		}
		
		return (0);
	}
	return (ENXIO);
}

static int
ed_pccard_rom_mac(device_t dev, uint8_t *enaddr)
{
	struct ed_softc *sc = device_get_softc(dev);
	uint8_t romdata[32], sum;
	int i;

	/*
	 * Read in the rom data at location 0.  Since there are no
	 * NE-1000 based PC Card devices, we'll assume we're 16-bit.
	 *
	 * In researching what format this takes, I've found that the
	 * following appears to be true for multiple cards based on
	 * observation as well as datasheet digging.
	 *
	 * Data is stored in some ROM and is copied out 8 bits at a time
	 * into 16-bit wide locations.  This means that the odd locations
	 * of the ROM are not used (and can be either 0 or ff).
	 *
	 * The contents appears to be as follows:
	 * PROM   RAM
	 * Offset Offset	What
	 *  0      0	ENETADDR 0
	 *  1      2	ENETADDR 1
	 *  2      4	ENETADDR 2
	 *  3      6	ENETADDR 3
	 *  4      8	ENETADDR 4
	 *  5     10	ENETADDR 5
	 *  6-13  12-26 Reserved (varies by manufacturer)
	 * 14     28	0x57
	 * 15     30    0x57
	 *
	 * Some manufacturers have another image of enetaddr from
	 * PROM offset 0x10 to 0x15 with 0x42 in 0x1e and 0x1f, but
	 * this doesn't appear to be universally documented in the
	 * datasheets.  Some manufactuers have a card type, card config
	 * checksums, etc encoded into PROM offset 6-13, but deciphering it
	 * requires more knowledge about the exact underlying chipset than
	 * we possess (and maybe can possess).
	 */
	ed_pio_readmem(sc, 0, romdata, 32);
	if (bootverbose)
		device_printf(dev, "ROM DATA: %32D\n", romdata, " ");
	if (romdata[28] != 0x57 || romdata[30] != 0x57)
		return (0);
	for (i = 0, sum = 0; i < ETHER_ADDR_LEN; i++)
		sum |= romdata[i * 2];
	if (sum == 0)
		return (0);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		enaddr[i] = romdata[i * 2];
	return (1);
}

static int
ed_pccard_add_modem(device_t dev)
{
	device_printf(dev, "Need to write this code\n");
	return 0;
}

static int
ed_pccard_kick_phy(struct ed_softc *sc)
{
	struct mii_softc *miisc;
	struct mii_data *mii;

	/*
	 * Many of the PHYs that wind up on PC Cards are weird in
	 * this way.  Generally, we don't need to worry so much about
	 * the Isolation protocol since there's only one PHY in
	 * these designs, so this workaround is reasonable.
	 */
	mii = device_get_softc(sc->miibus);
	LIST_FOREACH(miisc, &mii->mii_phys, mii_list) {
		miisc->mii_flags |= MIIF_FORCEANEG;
		mii_phy_reset(miisc);
	}
	return (mii_mediachg(mii));
}

static int
ed_pccard_media_ioctl(struct ed_softc *sc, struct ifreq *ifr, u_long command)
{
	struct mii_data *mii;

	if (sc->miibus == NULL)
		return (EINVAL);
	mii = device_get_softc(sc->miibus);
	return (ifmedia_ioctl(sc->ifp, ifr, &mii->mii_media, command));
}


static void
ed_pccard_mediachg(struct ed_softc *sc)
{
	struct mii_data *mii;

	if (sc->miibus == NULL)
		return;
	mii = device_get_softc(sc->miibus);
	mii_mediachg(mii);
}

static int
ed_pccard_attach(device_t dev)
{
	u_char sum;
	u_char enaddr[ETHER_ADDR_LEN];
	const struct ed_product *pp;
	int	error, i, flags, port_rid, modem_rid;
	struct ed_softc *sc = device_get_softc(dev);
	u_long size;
	static uint16_t *intr_vals[] = {NULL, NULL};

	sc->dev = dev;
	if ((pp = (const struct ed_product *) pccard_product_lookup(dev, 
	    (const struct pccard_product *) ed_pccard_products,
		 sizeof(ed_pccard_products[0]), NULL)) == NULL) {
		printf("Can't find\n");
		return (ENXIO);
	}
	modem_rid = port_rid = -1;
	if (pp->flags & NE2000DVF_MODEM) {
		for (i = 0; i < 4; i++) {
			size = bus_get_resource_count(dev, SYS_RES_IOPORT, i);
			if (size == ED_NOVELL_IO_PORTS)
				port_rid = i;
			else if (size == 8)
				modem_rid = i;
		}
		if (port_rid == -1) {
			device_printf(dev, "Cannot locate my ports!\n");
			return (ENXIO);
		}
	} else {
		port_rid = 0;
	}
	/* Allocate the port resource during setup. */
	error = ed_alloc_port(dev, port_rid, ED_NOVELL_IO_PORTS);
	if (error) {
		printf("alloc_port failed\n");
		return (error);
	}
	if (rman_get_size(sc->port_res) == ED_NOVELL_IO_PORTS / 2) {
		port_rid++;
		sc->port_res2 = bus_alloc_resource(dev, SYS_RES_IOPORT,
		    &port_rid, 0ul, ~0ul, 1, RF_ACTIVE);
		if (sc->port_res2 == NULL ||
		    rman_get_size(sc->port_res2) != ED_NOVELL_IO_PORTS / 2) {
			error = ENXIO;
			goto bad;
		}
	}
	error = ed_alloc_irq(dev, 0, 0);
	if (error)
		goto bad;

	/*
	 * Determine which chipset we are.  Almost all the PC Card chipsets
	 * have the Novel ASIC and NIC offsets.  There's 2 known cards that
	 * follow the WD80x3 conventions, which are handled as a special case.
	 */
	sc->asic_offset = ED_NOVELL_ASIC_OFFSET;
	sc->nic_offset  = ED_NOVELL_NIC_OFFSET;
	error = ENXIO;
	flags = device_get_flags(dev);
	if (error != 0)
		error = ed_pccard_dl100xx(dev, pp);
	if (error != 0)
		error = ed_pccard_ax88x90(dev, pp);
	if (error != 0)
		error = ed_pccard_tc5299j(dev, pp);
	if (error != 0) {
		error = ed_probe_Novell_generic(dev, flags);
		printf("Novell probe generic %d\n", error);
	}
	if (error != 0 && (pp->flags & NE2000DVF_TOSHIBA)) {
		flags |= ED_FLAGS_TOSH_ETHER;
		flags |= ED_FLAGS_PCCARD;
		sc->asic_offset = ED_WD_ASIC_OFFSET;
		sc->nic_offset  = ED_WD_NIC_OFFSET;
		error = ed_probe_WD80x3_generic(dev, flags, intr_vals);
	}
	if (error)
		goto bad;

	/*
	 * There are several ways to get the MAC address for the card.
	 * Some of the above probe routines can fill in the enaddr.  If
	 * not, we run through a number of 'well known' locations:
	 *	(1) From the PC Card FUNCE
	 *	(2) From offset 0 in the shared memory
	 *	(3) From a hinted offset in attribute memory
	 *	(4) From 0xff0 in attribute memory
	 * If we can't get a non-zero MAC address from this list, we fail.
	 */
	for (i = 0, sum = 0; i < ETHER_ADDR_LEN; i++)
		sum |= sc->enaddr[i];
	if (sum == 0) {
		pccard_get_ether(dev, enaddr);
		if (bootverbose)
			device_printf(dev, "CIS MAC %6D\n", enaddr, ":");
		for (i = 0, sum = 0; i < ETHER_ADDR_LEN; i++)
			sum |= enaddr[i];
		if (sum == 0 && ed_pccard_rom_mac(dev, enaddr)) {
			if (bootverbose)
				device_printf(dev, "ROM mac %6D\n", enaddr,
				    ":");
			sum++;
		}
		if (sum == 0 && pp->flags & NE2000DVF_ENADDR) {
			for (i = 0; i < ETHER_ADDR_LEN; i++) {
				pccard_attr_read_1(dev, pp->enoff + i * 2,
				    enaddr + i);
				sum |= enaddr[i];
			}
			if (bootverbose)
				device_printf(dev, "Hint %x MAC %6D\n",
				    pp->enoff, enaddr, ":");
		}
		if (sum == 0) {
			for (i = 0; i < ETHER_ADDR_LEN; i++) {
				pccard_attr_read_1(dev, ED_DEFAULT_MAC_OFFSET +
				    i * 2, enaddr + i);
				sum |= enaddr[i];
			}
			if (bootverbose)
				device_printf(dev, "Fallback MAC %6D\n",
				    enaddr, ":");
		}
		if (sum == 0) {
			device_printf(dev, "Cannot extract MAC address.\n");
			ed_release_resources(dev);
			return (ENXIO);
		}
		bcopy(enaddr, sc->enaddr, ETHER_ADDR_LEN);
	}

	error = ed_attach(dev);
	if (error)
		goto bad;
 	if (sc->chip_type == ED_CHIP_TYPE_DL10019 ||
	    sc->chip_type == ED_CHIP_TYPE_DL10022) {
		/* Probe for an MII bus, but ignore errors. */
		ed_pccard_dl100xx_mii_reset(sc);
		(void)mii_phy_probe(dev, &sc->miibus, ed_ifmedia_upd,
		    ed_ifmedia_sts);
	} else if (sc->chip_type == ED_CHIP_TYPE_AX88190 ||
	    sc->chip_type == ED_CHIP_TYPE_AX88790) {
		if ((error = mii_phy_probe(dev, &sc->miibus, ed_ifmedia_upd,
		     ed_ifmedia_sts)) != 0) {
			device_printf(dev, "Missing mii %d!\n", error);
			goto bad;
		}
		    
	} else if (sc->chip_type == ED_CHIP_TYPE_TC5299J) {
		if ((error = mii_phy_probe(dev, &sc->miibus, ed_ifmedia_upd,
		     ed_ifmedia_sts)) != 0) {
			device_printf(dev, "Missing mii!\n");
			goto bad;
		}
		    
	}
	if (sc->miibus != NULL) {
		sc->sc_tick = ed_pccard_tick;
		sc->sc_mediachg = ed_pccard_mediachg;
		sc->sc_media_ioctl = ed_pccard_media_ioctl;
		ed_pccard_kick_phy(sc);
	} else {
		ed_gen_ifmedia_init(sc);
	}
	if (modem_rid != -1)
		ed_pccard_add_modem(dev);

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, edintr, sc, &sc->irq_handle);
	if (error) {
		device_printf(dev, "setup intr failed %d \n", error);
		goto bad;
	}	      

	return (0);
bad:
	ed_detach(dev);
	return (error);
}

/*
 * Probe the Ethernet MAC addrees for PCMCIA Linksys EtherFast 10/100 
 * and compatible cards (DL10019C Ethernet controller).
 */
static int
ed_pccard_dl100xx(device_t dev, const struct ed_product *pp)
{
	struct ed_softc *sc = device_get_softc(dev);
	u_char sum;
	uint8_t id;
	u_int   memsize;
	int i, error;

	if (!(pp->flags & NE2000DVF_DL100XX))
		return (ENXIO);
	if (bootverbose)
		device_printf(dev, "Trying DL100xx probing\n");
	error = ed_probe_Novell_generic(dev, device_get_flags(dev));
	if (bootverbose && error)
		device_printf(dev, "Novell generic probe failed: %d\n", error);
	if (error != 0)
		return (error);

	/*
	 * Linksys registers(offset from ASIC base)
	 *
	 * 0x04-0x09 : Physical Address Register 0-5 (PAR0-PAR5)
	 * 0x0A      : Card ID Register (CIR)
	 * 0x0B      : Check Sum Register (SR)
	 */
	for (sum = 0, i = 0x04; i < 0x0c; i++)
		sum += ed_asic_inb(sc, i);
	if (sum != 0xff) {
		if (bootverbose)
			device_printf(dev, "Bad checksum %#x\n", sum);
		return (ENXIO);		/* invalid DL10019C */
	}
	if (bootverbose)
		device_printf(dev, "CIR is %d\n", ed_asic_inb(sc, 0xa));
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->enaddr[i] = ed_asic_inb(sc, 0x04 + i);
	ed_nic_outb(sc, ED_P0_DCR, ED_DCR_WTS | ED_DCR_FT1 | ED_DCR_LS);
	id = ed_asic_inb(sc, 0xf);
	sc->isa16bit = 1;
	/*
	 * Hard code values based on the datasheet.  We're NE-2000 compatible
	 * NIC with 24kb of packet memory starting at 24k offset.  These
	 * cards also work with 16k at 16k, but don't work with 24k at 16k
	 * or 32k at 16k.
	 */
	sc->type = ED_TYPE_NE2000;
	sc->mem_start = 24 * 1024;
	memsize = sc->mem_size = 24 * 1024;
	sc->mem_end = sc->mem_start + memsize;
	sc->tx_page_start = memsize / ED_PAGE_SIZE;
	sc->txb_cnt = 3;
	sc->rec_page_start = sc->tx_page_start + sc->txb_cnt * ED_TXBUF_SIZE;
	sc->rec_page_stop = sc->tx_page_start + memsize / ED_PAGE_SIZE;

	sc->mem_ring = sc->mem_start + sc->txb_cnt * ED_PAGE_SIZE * ED_TXBUF_SIZE;

	ed_nic_outb(sc, ED_P0_PSTART, sc->mem_start / ED_PAGE_SIZE);
	ed_nic_outb(sc, ED_P0_PSTOP, sc->mem_end / ED_PAGE_SIZE);
	sc->vendor = ED_VENDOR_NOVELL;
	sc->chip_type = (id & 0x90) == 0x90 ?
	    ED_CHIP_TYPE_DL10022 : ED_CHIP_TYPE_DL10019;
	sc->type_str = ((id & 0x90) == 0x90) ? "DL10022" : "DL10019";
	sc->mii_readbits = ed_pccard_dl100xx_mii_readbits;
	sc->mii_writebits = ed_pccard_dl100xx_mii_writebits;
	return (0);
}

/* MII bit-twiddling routines for cards using Dlink chipset */
#define DL100XX_MIISET(sc, x) ed_asic_outb(sc, ED_DL100XX_MIIBUS, \
    ed_asic_inb(sc, ED_DL100XX_MIIBUS) | (x))
#define DL100XX_MIICLR(sc, x) ed_asic_outb(sc, ED_DL100XX_MIIBUS, \
    ed_asic_inb(sc, ED_DL100XX_MIIBUS) & ~(x))

static void
ed_pccard_dl100xx_mii_reset(struct ed_softc *sc)
{
	if (sc->chip_type != ED_CHIP_TYPE_DL10022)
		return;

	ed_asic_outb(sc, ED_DL100XX_MIIBUS, ED_DL10022_MII_RESET2);
	DELAY(10);
	ed_asic_outb(sc, ED_DL100XX_MIIBUS,
	    ED_DL10022_MII_RESET2 | ED_DL10022_MII_RESET1);
	DELAY(10);
	ed_asic_outb(sc, ED_DL100XX_MIIBUS, ED_DL10022_MII_RESET2);
	DELAY(10);
	ed_asic_outb(sc, ED_DL100XX_MIIBUS,
	    ED_DL10022_MII_RESET2 | ED_DL10022_MII_RESET1);
	DELAY(10);
	ed_asic_outb(sc, ED_DL100XX_MIIBUS, 0);
}

static void
ed_pccard_dl100xx_mii_writebits(struct ed_softc *sc, u_int val, int nbits)
{
	int i;

	DL100XX_MIISET(sc, ED_DL100XX_MII_DIROUT);
	for (i = nbits - 1; i >= 0; i--) {
		if ((val >> i) & 1)
			DL100XX_MIISET(sc, ED_DL100XX_MII_DATAOUT);
		else
			DL100XX_MIICLR(sc, ED_DL100XX_MII_DATAOUT);
		DL100XX_MIISET(sc, ED_DL100XX_MII_CLK);
		DL100XX_MIICLR(sc, ED_DL100XX_MII_CLK);
	}
}

static u_int
ed_pccard_dl100xx_mii_readbits(struct ed_softc *sc, int nbits)
{
	int i;
	u_int val = 0;

	DL100XX_MIICLR(sc, ED_DL100XX_MII_DIROUT);
	for (i = nbits - 1; i >= 0; i--) {
		DL100XX_MIISET(sc, ED_DL100XX_MII_CLK);
		val <<= 1;
		if (ed_asic_inb(sc, ED_DL100XX_MIIBUS) & ED_DL100XX_MII_DATAIN)
			val++;
		DL100XX_MIICLR(sc, ED_DL100XX_MII_CLK);
	}
	return val;
}

static void
ed_pccard_ax88x90_reset(struct ed_softc *sc)
{
	int i;

	/* Reset Card */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP | ED_CR_PAGE_0);
	ed_asic_outb(sc, ED_NOVELL_RESET, ed_asic_inb(sc, ED_NOVELL_RESET));

	/* Wait for the RST bit to assert, but cap it at 10ms */
	for (i = 10000; !(ed_nic_inb(sc, ED_P0_ISR) & ED_ISR_RST) && i > 0;
	     i--)
		continue;
	ed_nic_outb(sc, ED_P0_ISR, ED_ISR_RST);	/* ACK INTR */
	if (i == 0)
		device_printf(sc->dev, "Reset didn't finish\n");
}

/*
 * Probe and vendor-specific initialization routine for ax88x90 boards
 */
static int
ed_probe_ax88x90_generic(device_t dev, int flags)
{
	struct ed_softc *sc = device_get_softc(dev);
	u_int   memsize;
	static char test_pattern[32] = "THIS is A memory TEST pattern";
	char    test_buffer[32];

	ed_pccard_ax88x90_reset(sc);
	DELAY(10*1000);

	/* Make sure that we really have an 8390 based board */
	if (!ed_probe_generic8390(sc))
		return (ENXIO);

	sc->vendor = ED_VENDOR_NOVELL;
	sc->mem_shared = 0;
	sc->cr_proto = ED_CR_RD2;

	/*
	 * This prevents packets from being stored in the NIC memory when the
	 * readmem routine turns on the start bit in the CR.  We write some
	 * bytes in word mode and verify we can read them back.  If we can't
	 * then we don't have an AX88x90 chip here.
	 */
	sc->isa16bit = 1;
	ed_nic_outb(sc, ED_P0_RCR, ED_RCR_MON);
	ed_nic_outb(sc, ED_P0_DCR, ED_DCR_WTS | ED_DCR_FT1 | ED_DCR_LS);
	ed_pio_writemem(sc, test_pattern, 16384, sizeof(test_pattern));
	ed_pio_readmem(sc, 16384, test_buffer, sizeof(test_pattern));
	if (bcmp(test_pattern, test_buffer, sizeof(test_pattern)) != 0)
		return (ENXIO);

	/*
	 * Hard code values based on the datasheet.  We're NE-2000 compatible
	 * NIC with 16kb of packet memory starting at 16k offset.
	 */
	sc->type = ED_TYPE_NE2000;
	memsize = sc->mem_size = 16*1024;
	sc->mem_start = 16 * 1024;
	if (ed_asic_inb(sc, ED_AX88X90_TEST) != 0)
		sc->chip_type = ED_CHIP_TYPE_AX88790;
	else {
		sc->chip_type = ED_CHIP_TYPE_AX88190;
		/*
		 * The AX88190 (not A) has external 64k SRAM.  Probe for this
		 * here.  Most of the cards I have either use the AX88190A
		 * part, or have only 32k SRAM for some reason, so I don't
		 * know if this works or not.
		 */
		ed_pio_writemem(sc, test_pattern, 32768, sizeof(test_pattern));
		ed_pio_readmem(sc, 32768, test_buffer, sizeof(test_pattern));
		if (bcmp(test_pattern, test_buffer, sizeof(test_pattern)) == 0) {
			sc->mem_start = 2*1024;
			memsize = sc->mem_size = 62 * 1024;
		}
	}
	sc->mem_end = sc->mem_start + memsize;
	sc->tx_page_start = memsize / ED_PAGE_SIZE;
	if (sc->mem_size > 16 * 1024)
		sc->txb_cnt = 3;
	else
		sc->txb_cnt = 2;
	sc->rec_page_start = sc->tx_page_start + sc->txb_cnt * ED_TXBUF_SIZE;
	sc->rec_page_stop = sc->tx_page_start + memsize / ED_PAGE_SIZE;

	sc->mem_ring = sc->mem_start + sc->txb_cnt * ED_PAGE_SIZE * ED_TXBUF_SIZE;

	ed_nic_outb(sc, ED_P0_PSTART, sc->mem_start / ED_PAGE_SIZE);
	ed_nic_outb(sc, ED_P0_PSTOP, sc->mem_end / ED_PAGE_SIZE);

	/* Get the mac before we go -- It's just at 0x400 in "SRAM" */
	ed_pio_readmem(sc, 0x400, sc->enaddr, ETHER_ADDR_LEN);

	/* clear any pending interrupts that might have occurred above */
	ed_nic_outb(sc, ED_P0_ISR, 0xff);
	sc->sc_write_mbufs = ed_pio_write_mbufs;
	return (0);
}

static int
ed_pccard_ax88x90_check_mii(device_t dev, struct ed_softc *sc)
{
	int	i, id;

	/*
	 * All AX88x90 devices have MII and a PHY, so we use this to weed out
	 * chips that would otherwise make it through the tests we have after
	 * this point.
	 */
	for (i = 0; i < 32; i++) {
		id = ed_miibus_readreg(dev, i, MII_BMSR);
		if (id != 0 && id != 0xffff)
			break;
	}
	/*
	 * Found one, we're good.
	 */
	if (i != 32)
		return (0);
	/*
	 * Didn't find anything, so try to power up and try again.  The PHY
	 * may be not responding because we're in power down mode.
	 */
	if (sc->chip_type == ED_CHIP_TYPE_AX88190)
		return (ENXIO);
	pccard_ccr_write_1(dev, PCCARD_CCR_STATUS, PCCARD_CCR_STATUS_PWRDWN);
	for (i = 0; i < 32; i++) {
		id = ed_miibus_readreg(dev, i, MII_BMSR);
		if (id != 0 && id != 0xffff)
			break;
	}
	/*
	 * Still no joy?  We're AFU, punt.
	 */
	if (i == 32)
		return (ENXIO);
	return (0);
	
}

/*
 * Special setup for AX88[17]90
 */
static int
ed_pccard_ax88x90(device_t dev, const struct ed_product *pp)
{
	int	error;
	int iobase;
	struct	ed_softc *sc = device_get_softc(dev);

	if (!(pp->flags & NE2000DVF_AX88X90))
		return (ENXIO);

	if (bootverbose)
		device_printf(dev, "Checking AX88x90\n");

	/*
	 * Set the IOBASE Register.  The AX88x90 cards are potentially
	 * multifunction cards, and thus requires a slight workaround.
	 * We write the address the card is at, on the off chance that this
	 * card is not MFC.
	 * XXX I'm not sure that this is still needed...
	 */
	iobase = rman_get_start(sc->port_res);
	pccard_ccr_write_1(dev, PCCARD_CCR_IOBASE0, iobase & 0xff);
	pccard_ccr_write_1(dev, PCCARD_CCR_IOBASE1, (iobase >> 8) & 0xff);

	sc->mii_readbits = ed_pccard_ax88x90_mii_readbits;
	sc->mii_writebits = ed_pccard_ax88x90_mii_writebits;
	error = ed_probe_ax88x90_generic(dev, device_get_flags(dev));
	if (error) {
		if (bootverbose)
			device_printf(dev, "probe ax88x90 failed %d\n",
			    error);
		goto fail;
	}
	error = ed_pccard_ax88x90_check_mii(dev, sc);
	if (error)
		goto fail;
	sc->vendor = ED_VENDOR_NOVELL;
	sc->type = ED_TYPE_NE2000;
	if (sc->chip_type == ED_CHIP_TYPE_AX88190)
		sc->type_str = "AX88190";
	else
		sc->type_str = "AX88790";
	return (0);
fail:;
	sc->mii_readbits = 0;
	sc->mii_writebits = 0;
	return (error);
}

static void
ed_pccard_ax88x90_mii_writebits(struct ed_softc *sc, u_int val, int nbits)
{
	int i, data;

	for (i = nbits - 1; i >= 0; i--) {
		data = (val >> i) & 1 ? ED_AX88X90_MII_DATAOUT : 0;
		ed_asic_outb(sc, ED_AX88X90_MIIBUS, data);
		ed_asic_outb(sc, ED_AX88X90_MIIBUS, data | ED_AX88X90_MII_CLK);
	}
}

static u_int
ed_pccard_ax88x90_mii_readbits(struct ed_softc *sc, int nbits)
{
	int i;
	u_int val = 0;
	uint8_t mdio;

	mdio = ED_AX88X90_MII_DIRIN;
	for (i = nbits - 1; i >= 0; i--) {
		ed_asic_outb(sc, ED_AX88X90_MIIBUS, mdio);
		val <<= 1;
		if (ed_asic_inb(sc, ED_AX88X90_MIIBUS) & ED_AX88X90_MII_DATAIN)
			val++;
		ed_asic_outb(sc, ED_AX88X90_MIIBUS, mdio | ED_AX88X90_MII_CLK);
	}
	return val;
}

/*
 * Special setup for TC5299J
 */
static int
ed_pccard_tc5299j(device_t dev, const struct ed_product *pp)
{
	int	error, i, id;
	char *ts;
	struct	ed_softc *sc = device_get_softc(dev);

	if (!(pp->flags & NE2000DVF_TC5299J))
		return (ENXIO);

	if (bootverbose)
		device_printf(dev, "Checking Tc5299j\n");

	error = ed_probe_Novell_generic(dev, device_get_flags(dev));
	if (bootverbose)
		device_printf(dev, "probe novel returns %d\n", error);
	if (error != 0)
		return (error);

	/*
	 * Check to see if we have a MII PHY ID at any address.  All TC5299J
	 * devices have MII and a PHY, so we use this to weed out chips that
	 * would otherwise make it through the tests we have after this point.
	 */
	sc->mii_readbits = ed_pccard_tc5299j_mii_readbits;
	sc->mii_writebits = ed_pccard_tc5299j_mii_writebits;
	for (i = 0; i < 32; i++) {
		id = ed_miibus_readreg(dev, i, MII_PHYIDR1);
		if (id != 0 && id != 0xffff)
			break;
	}
	if (i == 32) {
		sc->mii_readbits = 0;
		sc->mii_writebits = 0;
		return (ENXIO);
	}
	ts = "TC5299J";
	if (ed_pccard_rom_mac(dev, sc->enaddr) == 0) {
		sc->mii_readbits = 0;
		sc->mii_writebits = 0;
		return (ENXIO);
	}
	sc->vendor = ED_VENDOR_NOVELL;
	sc->type = ED_TYPE_NE2000;
	sc->chip_type = ED_CHIP_TYPE_TC5299J;
	sc->type_str = ts;
	return (0);
}

static void
ed_pccard_tc5299j_mii_writebits(struct ed_softc *sc, u_int val, int nbits)
{
	int i;
	uint8_t cr, data;

	/* Select page 3 */
	cr = ed_nic_inb(sc, ED_P0_CR);
	ed_nic_outb(sc, ED_P0_CR, cr | ED_CR_PAGE_3);

	for (i = nbits - 1; i >= 0; i--) {
		data = (val >> i) & 1 ? ED_TC5299J_MII_DATAOUT : 0;
		ed_nic_outb(sc, ED_TC5299J_MIIBUS, data);
		ed_nic_outb(sc, ED_TC5299J_MIIBUS, data | ED_TC5299J_MII_CLK);
	}
	ed_nic_outb(sc, ED_TC5299J_MIIBUS, 0);
		
	/* Restore prior page */
	ed_nic_outb(sc, ED_P0_CR, cr);
}

static u_int
ed_pccard_tc5299j_mii_readbits(struct ed_softc *sc, int nbits)
{
	int i;
	u_int val = 0;
	uint8_t cr;

	/* Select page 3 */
	cr = ed_nic_inb(sc, ED_P0_CR);
	ed_nic_outb(sc, ED_P0_CR, cr | ED_CR_PAGE_3);

	ed_asic_outb(sc, ED_TC5299J_MIIBUS, ED_TC5299J_MII_DIROUT);
	for (i = nbits - 1; i >= 0; i--) {
		ed_nic_outb(sc, ED_TC5299J_MIIBUS,
		    ED_TC5299J_MII_CLK | ED_TC5299J_MII_DIROUT);
		val <<= 1;
		if (ed_nic_inb(sc, ED_TC5299J_MIIBUS) & ED_TC5299J_MII_DATAIN)
			val++;
		ed_nic_outb(sc, ED_TC5299J_MIIBUS, ED_TC5299J_MII_DIROUT);
	}

	/* Restore prior page */
	ed_nic_outb(sc, ED_P0_CR, cr);
	return val;
}

/*
 * MII bus support routines.
 */
static int
ed_miibus_readreg(device_t dev, int phy, int reg)
{
	struct ed_softc *sc;
	int failed, val;

	sc = device_get_softc(dev);
	/*
	 * The AX88790 has an interesting quirk.  It has an internal phy that
	 * needs a special bit set to access, but can also have additional
	 * external PHYs set for things like HomeNET media.  When accessing
	 * the internal PHY, a bit has to be set, when accessing the external
	 * PHYs, it must be clear.  See Errata 1, page 51, in the AX88790
	 * datasheet for more details.
	 *
	 * Also, PHYs above 16 appear to be phantoms on some cards, but not
	 * others.  Registers read for this are often the same as prior values
	 * read.  Filter all register requests to 17-31.
	 *
	 * I can't explain it, since I don't have the DL100xx data sheets, but
	 * the DL100xx chips do 13-bits before the 'ACK' but, but the AX88x90
	 * chips have 14.  The linux pcnet and axnet drivers confirm this.
	 */
	if (sc->chip_type == ED_CHIP_TYPE_AX88790) {
		if (phy > 0x10)
			return (0);
		if (phy == 0x10)
			ed_asic_outb(sc, ED_AX88X90_GPIO,
			    ED_AX88X90_GPIO_INT_PHY);
		else
			ed_asic_outb(sc, ED_AX88X90_GPIO, 0);
	}

	(*sc->mii_writebits)(sc, 0xffffffff, 32);
	(*sc->mii_writebits)(sc, ED_MII_STARTDELIM, ED_MII_STARTDELIM_BITS);
	(*sc->mii_writebits)(sc, ED_MII_READOP, ED_MII_OP_BITS);
	(*sc->mii_writebits)(sc, phy, ED_MII_PHY_BITS);
	(*sc->mii_writebits)(sc, reg, ED_MII_REG_BITS);
	if (sc->chip_type == ED_CHIP_TYPE_AX88790 ||
	    sc->chip_type == ED_CHIP_TYPE_AX88190)
		(*sc->mii_readbits)(sc, ED_MII_ACK_BITS);
	failed = (*sc->mii_readbits)(sc, ED_MII_ACK_BITS);
	val = (*sc->mii_readbits)(sc, ED_MII_DATA_BITS);
	(*sc->mii_writebits)(sc, ED_MII_IDLE, ED_MII_IDLE_BITS);
/*	printf("Reading phy %d reg %#x returning %#x (valid %d)\n", phy, reg, val, !failed); */
	return (failed ? 0 : val);
}

static int
ed_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct ed_softc *sc;

/*	printf("Writing phy %d reg %#x data %#x\n", phy, reg, data); */
	sc = device_get_softc(dev);
	/* See ed_miibus_readreg for details */
	if (sc->chip_type == ED_CHIP_TYPE_AX88790) {
		if (phy > 0x10)
			return (0);
		if (phy == 0x10)
			ed_asic_outb(sc, ED_AX88X90_GPIO,
			    ED_AX88X90_GPIO_INT_PHY);
		else
			ed_asic_outb(sc, ED_AX88X90_GPIO, 0);
	}
	(*sc->mii_writebits)(sc, 0xffffffff, 32);
	(*sc->mii_writebits)(sc, ED_MII_STARTDELIM, ED_MII_STARTDELIM_BITS);
	(*sc->mii_writebits)(sc, ED_MII_WRITEOP, ED_MII_OP_BITS);
	(*sc->mii_writebits)(sc, phy, ED_MII_PHY_BITS);
	(*sc->mii_writebits)(sc, reg, ED_MII_REG_BITS);
	(*sc->mii_writebits)(sc, ED_MII_TURNAROUND, ED_MII_TURNAROUND_BITS);
	(*sc->mii_writebits)(sc, data, ED_MII_DATA_BITS);
	(*sc->mii_writebits)(sc, ED_MII_IDLE, ED_MII_IDLE_BITS);
	return (0);
}

static int
ed_ifmedia_upd(struct ifnet *ifp)
{
	struct ed_softc *sc;
	int error;

	sc = ifp->if_softc;
	if (sc->miibus == NULL)
		return (ENXIO);
	ED_LOCK(sc);
	error = ed_pccard_kick_phy(sc);
	ED_UNLOCK(sc);
	return (error);
}

static void
ed_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct ed_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;
	if (sc->miibus == NULL)
		return;

	mii = device_get_softc(sc->miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static void
ed_child_detached(device_t dev, device_t child)
{
	struct ed_softc *sc;

	sc = device_get_softc(dev);
	if (child == sc->miibus)
		sc->miibus = NULL;
}

static void
ed_pccard_tick(void *arg)
{
	struct ed_softc *sc = arg;
	struct mii_data *mii;
	int media = 0;

	ED_ASSERT_LOCKED(sc);
	if (sc->miibus != NULL) {
		mii = device_get_softc(sc->miibus);
		media = mii->mii_media_status;
		mii_tick(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		    media != mii->mii_media_status) {
			if (sc->chip_type == ED_CHIP_TYPE_DL10022) {
				ed_asic_outb(sc, ED_DL10022_DIAG,
				    (mii->mii_media_active & IFM_FDX) ?
				    ED_DL10022_COLLISON_DIS : 0);
#ifdef notyet
			} else if (sc->chip_type == ED_CHIP_TYPE_DL10019) {
				write_asic(sc, ED_DL10019_MAGIC,
				    (mii->mii_media_active & IFM_FDX) ?
				    DL19FDUPLX : 0);
#endif
			}
		}
		
	}
	callout_reset(&sc->tick_ch, hz, ed_pccard_tick, sc);
}

static device_method_t ed_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ed_pccard_probe),
	DEVMETHOD(device_attach,	ed_pccard_attach),
	DEVMETHOD(device_detach,	ed_detach),

	/* Bus interface */
	DEVMETHOD(bus_child_detached,	ed_child_detached),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	ed_miibus_readreg),
	DEVMETHOD(miibus_writereg,	ed_miibus_writereg),

	{ 0, 0 }
};

static driver_t ed_pccard_driver = {
	"ed",
	ed_pccard_methods,
	sizeof(struct ed_softc)
};

DRIVER_MODULE(ed, pccard, ed_pccard_driver, ed_devclass, 0, 0);
DRIVER_MODULE(miibus, ed, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(ed, miibus, 1, 1, 1);
MODULE_DEPEND(ed, ether, 1, 1, 1);
