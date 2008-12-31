/*-
 * Copyright (c) 2006 Sam Leffler.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/arm/xscale/ixp425/ixp425_npevar.h,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _IXP425_NPEVAR_H_
#define _IXP425_NPEVAR_H_

/*
 * Intel (R) IXP400 Software NPE Image ID Definition
 *
 * Firmware Id's for current firmware image.  These are typed by
 * NPE ID and the feature set.  Not all features are available
 * on all NPE's.  The Image ID has the following structure:
 *
 * Field		[Bit Location]
 * -----------------------------------
 * Device ID		[28..31]
 * NPE ID		[24..27]
 * NPE Functionality ID	[16..23]
 * Major Release Number	[8..15]
 * Minor Release Number	[0..7]
 *
 * The following "feature sets" are known to exist:
 *
 * HSS-0: supports 32 channelized and 4 packetized.
 * HSS-0 + ATM + SPHY:
 *    For HSS, 16/32 channelized and 4/0 packetized.
 *    For ATM, AAL5, AAL0 and OAM for UTOPIA SPHY, 1 logical port, 32 VCs.
 *    Fast Path support.
 * HSS-0 + ATM + MPHY:
 *    For HSS, 16/32 channelized and 4/0 packetized.
 *    For ATM, AAL5, AAL0 and OAM for UTOPIA MPHY, 1 logical port, 32 VCs.
 *    Fast Path support.
 * ATM-Only:
 *    AAL5, AAL0 and OAM for UTOPIA MPHY, 12 logical ports, 32 VCs.
 *    Fast Path support.
 * HSS-2:
 *    HSS-0 and HSS-1.
 *    Each HSS port supports 32 channelized and 4 packetized.
 * ETH: Ethernet Rx/Tx which includes:
 *    MAC_FILTERING, MAC_LEARNING, SPANNING_TREE, FIREWALL
 * ETH+VLAN Ethernet Rx/Tx which includes:
 *    MAC_FILTERING, MAC_LEARNING, SPANNING_TREE, FIREWALL, VLAN_QOS
 * ETH+VLAN+HDR: Ethernet Rx/Tx which includes:
 *    SPANNING_TREE, FIREWALL, VLAN_QOS, HEADER_CONVERSION
 */
#define NPEIMAGE_DEVID(id)	(((id) >> 28) & 0xf)
#define NPEIMAGE_NPEID(id)	(((id) >> 24) & 0xf)
#define NPEIMAGE_FUNCID(id) 	(((id) >> 16) & 0xff)
#define NPEIMAGE_MAJOR(id)	(((id) >> 8)  & 0xff)
#define NPEIMAGE_MINOR(id)	(((id) >> 0)  & 0xff)
#define	NPEIMAGE_MAKEID(dev, npe, func, maj, min) \
	((((dev) & 0xf) << 28) | (((npe) & 0xf) << 24) | \
	(((func) & 0xff) << 16) (((maj) & 0xff) << 8) | (((min) & 0xff) << 0))

/* XXX not right, revise */
/* NPE A Firmware Image Id's */
#define	NPEFW_A_HSS0		0x00010000 /* HSS-0: 32 chan+4 packet */
#define NPEFW_A_HSS0_ATM_S_1	0x00020000 /* HSS-0+ATM UTOPIA SPHY (1 port) */
#define NPEFW_A_HSS0_ATM_M_1	0x00020000 /* HSS-0+ATM UTOPIA MPHY (1 port) */
#define NPEFW_A_ATM_M_12	0x00040000 /* ATM UTOPIA MPHY (12 ports) */
#define NPEFW_A_DMA		0x00150100 /* DMA only */
#define	NPEFW_A_HSS2		0x00090000 /* HSS-0 + HSS-1 */
#define	NPEFW_A_ETH		0x10800200 /* Basic Ethernet */
#define	NPEFW_A_ETH_VLAN	0x10810200 /* NPEFW_A_ETH + VLAN QoS */
#define	NPEFW_A_ETH_VLAN_HDR	0x10820200 /* NPEFW_A_ETH_VLAN + Hdr conv */
/* XXX ... more not included */

/* NPE B Firmware Image Id's */
#define NPEFW_B_ETH		0x01000200 /* Basic Ethernet */
#define NPEFW_B_ETH_VLAN	0x01010200 /* NPEFW_B_ETH + VLAN QoS */
#define NPEFW_B_ETH_VLAN_HDR	0x01020201 /* NPEFW_B_ETH_VLAN + Hdr conv */
#define NPEFW_B_DMA		0x01020100 /* DMA only */
/* XXX ... more not include */

#define	IXP425_NPE_B_IMAGEID	0x01000200
#define	IXP425_NPE_C_IMAGEID	0x02000200

struct ixpnpe_softc;
struct ixpnpe_softc *ixpnpe_attach(device_t);
void	ixpnpe_detach(struct ixpnpe_softc *);
int	ixpnpe_stopandreset(struct ixpnpe_softc *);
int	ixpnpe_start(struct ixpnpe_softc *);
int	ixpnpe_stop(struct ixpnpe_softc *);
int	ixpnpe_init(struct ixpnpe_softc *,
		const char *imageName, uint32_t imageId);
int	ixpnpe_getfunctionality(struct ixpnpe_softc *sc);

int	ixpnpe_sendmsg(struct ixpnpe_softc *, const uint32_t msg[2]);
int	ixpnpe_recvmsg(struct ixpnpe_softc *, uint32_t msg[2]);
int	ixpnpe_sendandrecvmsg(struct ixpnpe_softc *, const uint32_t send[2],
		uint32_t recv[2]);
#endif /* _IXP425_NPEVAR_H_ */
