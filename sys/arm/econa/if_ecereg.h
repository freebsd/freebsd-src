/*-
 * Copyright (c) 2009, Yohanes Nugroho <yohanes@gmail.com>
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

#ifndef	_IF_ECEREG_H
#define	_IF_ECEREG_H

#define	ETH_CFG		0x08
#define	ETH_CFG_RMII		(1 << 15)
#define	PHY_CONTROL		0x00
#define	PHY_RW_OK		(1<<15)

#define	PHY_ADDRESS(x)		((x) & 0x1)
#define	PHY_REGISTER(r)	(((r) & 0x1F) << 8)
#define	PHY_WRITE_COMMAND	(1<<13)
#define	PHY_READ_COMMAND	(1<<14)
#define	PHY_GET_DATA(d)	(((d) >> 16) & 0xFFFF)
#define	PHY_DATA(d)		(((d) & 0xFFFF) << 16)

#define	PORT_0_CONFIG		0x08

#define	ARL_TABLE_ACCESS_CONTROL_0	0x050
#define	ARL_TABLE_ACCESS_CONTROL_1	0x054
#define	ARL_TABLE_ACCESS_CONTROL_2	0x058

#define	ARL_WRITE_COMMAND	(1<<3)
#define	ARL_LOOKUP_COMMAND	(1<<2)
#define	ARL_COMMAND_COMPLETE	(1)


#define	PORT0			(1 << 0)
#define	PORT1			(1 << 1)
#define	CPU_PORT		(1 << 2)


#define	VLAN0_GROUP_ID		(0)
#define	VLAN1_GROUP_ID		(1)
#define	VLAN2_GROUP_ID		(2)
#define	VLAN3_GROUP_ID		(3)
#define	VLAN4_GROUP_ID		(4)
#define	VLAN5_GROUP_ID		(5)
#define	VLAN6_GROUP_ID		(6)
#define	VLAN7_GROUP_ID		(7)

#define	PORT0_PVID		(VLAN1_GROUP_ID)
#define	PORT1_PVID		(VLAN2_GROUP_ID)
#define	CPU_PORT_PVID		(VLAN0_GROUP_ID)

#define	VLAN0_VID		(0x111)
#define	VLAN1_VID		(0x222)
#define	VLAN2_VID		(0x333)
#define	VLAN3_VID		(0x444)
#define	VLAN4_VID		(0x555)
#define	VLAN5_VID		(0x666)
#define	VLAN6_VID		(0x777)
#define	VLAN7_VID		(0x888)

#define	VLAN0_GROUP		(PORT0 | PORT1 | CPU_PORT)
#define	VLAN1_GROUP		(PORT0 | CPU_PORT)
#define	VLAN2_GROUP		(PORT1 | CPU_PORT)
#define	VLAN3_GROUP		(0)
#define	VLAN4_GROUP		(0)
#define	VLAN5_GROUP		(0)
#define	VLAN6_GROUP		(0)
#define	VLAN7_GROUP		(0)

#define	SWITCH_CONFIG		0x004
#define	MAC_PORT_0_CONFIG	0x008
#define	MAC_PORT_1_CONFIG	0x00C
#define	CPU_PORT_CONFIG	0x010
#define	BIST_RESULT_TEST_0	0x094

#define	FS_DMA_CONTROL		0x104
#define	TS_DMA_CONTROL		0x100

#define	INTERRUPT_MASK		0x08C
#define	INTERRUPT_STATUS	0x088

#define	TS_DESCRIPTOR_POINTER		0x108
#define	TS_DESCRIPTOR_BASE_ADDR	0x110
#define	FS_DESCRIPTOR_POINTER		0x10C
#define	FS_DESCRIPTOR_BASE_ADDR	0x114


#define	VLAN_VID_0_1		0x060
#define	VLAN_VID_2_3		0x064
#define	VLAN_VID_4_5		0x068
#define	VLAN_VID_6_7		0x06C

#define	VLAN_PORT_PVID		0x05C
#define	VLAN_MEMBER_PORT_MAP		0x070
#define	VLAN_TAG_PORT_MAP		0x074


#define	ASIX_GIGA_PHY		1
#define	TWO_SINGLE_PHY		2
#define	AGERE_GIGA_PHY		3
#define	VSC8601_GIGA_PHY	4
#define	IC_PLUS_PHY		5
#define	NOT_FOUND_PHY		(-1)

#define	MAX_PACKET_LEN		(1536)

#define	INVALID_ENTRY		0
#define	NEW_ENTRY		0x1
#define	STATIC_ENTRY		0x7

/*mask status except for link change*/
#define	ERROR_MASK		0xFFFFFF7F

/*hardware interface flags*/

#define	FAST_AGING		(0xf)
#define	IVL_LEARNING		(0x1 << 22)
/*hardware NAT accelerator*/
#define	HARDWARE_NAT		(0x1 << 23)
/*aging		time		setting*/

/*skip lookup*/
#define	SKIP_L2_LOOKUP_PORT_1	(1 << 29)
#define	SKIP_L2_LOOKUP_PORT_0	(1 << 28)

#define	NIC_MODE		(1 << 30)
#define	PORT_DISABLE		(1 << 18)
#define	SA_LEARNING_DISABLE		(1 << 19)
#define	DISABLE_BROADCAST_PACKET	(1 << 27)
#define	DISABLE_MULTICAST_PACKET	( 1 << 26)

#endif
