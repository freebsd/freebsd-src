/* $NetBSD: awireg.h,v 1.8 2003/01/20 05:30:06 simonb Exp $ */
/* $FreeBSD: src/sys/dev/awi/awireg.h,v 1.2.26.1 2008/11/25 02:59:29 kensmith Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_IC_AWIREG_H
#define	_DEV_IC_AWIREG_H

/*
 * The firmware typically loaded onto Am79C930-based 802.11 interfaces
 * uses a 32k or larger shared memory buffer to communicate with the
 * host.
 *
 * Depending on the exact configuration of the device, this buffer may
 * either be mapped into PCMCIA memory space, or accessible a byte at
 * a type through PCMCIA I/O space.
 *
 * This header defines offsets into this shared memory.
 */


/*
 * LAST_TXD block.  5 32-bit words.
 *
 * There are five different output queues; this defines pointers to
 * the last completed descriptor for each one.
 */
#define AWI_LAST_TXD		0x3ec	/* last completed Tx Descr */

#define AWI_LAST_BCAST_TXD		AWI_LAST_TXD+0
#define AWI_LAST_MGT_TXD		AWI_LAST_TXD+4
#define AWI_LAST_DATA_TXD		AWI_LAST_TXD+8
#define AWI_LAST_PS_POLL_TXD		AWI_LAST_TXD+12
#define AWI_LAST_CF_POLL_TXD		AWI_LAST_TXD+16

/*
 * Banner block; null-terminated string.
 *
 * The doc says it contains
 * "PCnetMobile:v2.00 mmddyy APIx.x\0"
 */

#define AWI_BANNER		0x480	/* Version string */
#define AWI_BANNER_LEN			0x20

/*
 * Command block protocol:
 * write command byte to a zero value.
 * write command status to a zero value.
 * write arguments to AWI_COMMAND_PARAMS
 * write command byte to a non-zero value.
 * wait for command status to be non-zero.
 * write command byte to a zero value.
 * write command status to a zero value.
 */

#define AWI_CMD			0x4a0	/* Command opcode byte */

#define AWI_CMD_IDLE			0x0
#define AWI_CMD_NOP			0x1

#define AWI_CMD_SET_MIB			0x2
#define AWI_CMD_GET_MIB			0x9
#define AWI_CA_MIB_TYPE			(AWI_CMD_PARAMS + 0x0)
#define AWI_CA_MIB_SIZE			(AWI_CMD_PARAMS + 0x1)
#define AWI_CA_MIB_INDEX		(AWI_CMD_PARAMS + 0x2)
#define AWI_CA_MIB_DATA			(AWI_CMD_PARAMS + 0x4)
#define AWI_MIB_LOCAL				0
#define AWI_MIB_ADDR				2
#define AWI_MIB_MAC				3
#define AWI_MIB_STAT				4
#define AWI_MIB_MGT				5
#define AWI_MIB_DRVR				6
#define AWI_MIB_PHY				7

#define AWI_CMD_INIT_TX			0x3
#define AWI_CA_TX_LEN			20
#define AWI_CA_TX_DATA			(AWI_CMD_PARAMS + 0x0)
#define AWI_CA_TX_MGT			(AWI_CMD_PARAMS + 0x4)
#define AWI_CA_TX_BCAST		       	(AWI_CMD_PARAMS + 0x8)
#define AWI_CA_TX_PS			(AWI_CMD_PARAMS + 0xc)
#define AWI_CA_TX_CF			(AWI_CMD_PARAMS + 0x10)

#define AWI_CMD_FLUSH_TX		0x4
#define AWI_CA_FTX_LEN			5
#define AWI_CA_FTX_DATA			(AWI_CMD_PARAMS + 0x0)
#define AWI_CA_FTX_MGT			(AWI_CMD_PARAMS + 0x1)
#define AWI_CA_FTX_BCAST		(AWI_CMD_PARAMS + 0x2)
#define AWI_CA_FTX_PS			(AWI_CMD_PARAMS + 0x3)
#define AWI_CA_FTX_CF			(AWI_CMD_PARAMS + 0x4)

#define AWI_CMD_INIT_RX			0x5
#define AWI_CA_IRX_LEN			0x8
#define AWI_CA_IRX_DATA_DESC		(AWI_CMD_PARAMS + 0x0)	/* return */
#define AWI_CA_IRX_PS_DESC		(AWI_CMD_PARAMS + 0x4)	/* return */

#define AWI_CMD_KILL_RX			0x6

#define AWI_CMD_SLEEP			0x7
#define AWI_CA_SLEEP_LEN		8
#define AWI_CA_WAKEUP			(AWI_CMD_PARAMS + 0x0)	/* uint64 */

#define AWI_CMD_WAKE			0x8

#define AWI_CMD_SCAN			0xa
#define AWI_CA_SCAN_LEN			6
#define AWI_CA_SCAN_DURATION		(AWI_CMD_PARAMS + 0x0)
#define AWI_CA_SCAN_SET			(AWI_CMD_PARAMS + 0x2)
#define AWI_CA_SCAN_PATTERN		(AWI_CMD_PARAMS + 0x3)
#define AWI_CA_SCAN_IDX			(AWI_CMD_PARAMS + 0x4)
#define AWI_CA_SCAN_SUSP		(AWI_CMD_PARAMS + 0x5)

#define AWI_CMD_SYNC			0xb
#define AWI_CA_SYNC_LEN			20
#define AWI_CA_SYNC_SET			(AWI_CMD_PARAMS + 0x0)
#define AWI_CA_SYNC_PATTERN		(AWI_CMD_PARAMS + 0x1)
#define AWI_CA_SYNC_IDX			(AWI_CMD_PARAMS + 0x2)
#define AWI_CA_SYNC_STARTBSS		(AWI_CMD_PARAMS + 0x3)
#define AWI_CA_SYNC_DWELL		(AWI_CMD_PARAMS + 0x4)
#define AWI_CA_SYNC_MBZ			(AWI_CMD_PARAMS + 0x6)
#define AWI_CA_SYNC_TIMESTAMP		(AWI_CMD_PARAMS + 0x8)
#define AWI_CA_SYNC_REFTIME		(AWI_CMD_PARAMS + 0x10)

#define AWI_CMD_RESUME			0xc

#define AWI_CMD_STATUS			0x4a1 	/* Command status */

#define AWI_STAT_IDLE			0x0
#define AWI_STAT_OK			0x1
#define AWI_STAT_BADCMD			0x2
#define AWI_STAT_BADPARM		0x3
#define AWI_STAT_NOTIMP			0x4
#define AWI_STAT_BADRES			0x5
#define AWI_STAT_BADMODE		0x6

#define AWI_ERROR_OFFSET	0x4a2 	/* Offset to erroneous parameter */
#define AWI_CMD_PARAMS		0x4a4 	/* Command parameters */

#define AWI_CSB			0x4f0 	/* Control/Status block */

#define AWI_SELFTEST		0x4f0

#define AWI_SELFTEST_INIT		0x00 /* initial */
#define AWI_SELFTEST_FIRMCKSUM		0x01 /* firmware cksum running */
#define AWI_SELFTEST_HARDWARE		0x02 /* hardware tests running */
#define AWI_SELFTEST_MIB		0x03 /* mib initializing */

#define AWI_SELFTEST_MIB_FAIL		0xfa
#define AWI_SELFTEST_RADIO_FAIL		0xfb
#define AWI_SELFTEST_MAC_FAIL		0xfc
#define AWI_SELFTEST_FLASH_FAIL		0xfd
#define AWI_SELFTEST_RAM_FAIL		0xfe
#define AWI_SELFTEST_PASSED		0xff

#define AWI_STA_STATE		0x4f1

#define AWI_STA_AP			0x20 /* acting as AP */
#define AWI_STA_NOPSP			0x10 /* Power Saving disabled */
#define AWI_STA_DOZE			0x08 /* about to go to sleep */
#define AWI_STA_PSP			0x04 /* enable PSP */
#define AWI_STA_RXEN			0x02 /* enable RX */
#define AWI_STA_TXEN			0x01 /* enable TX */
					      
#define AWI_INTSTAT		0x4f3
#define AWI_INTMASK		0x4f4

/* Bits in AWI_INTSTAT/AWI_INTMASK */

#define AWI_INT_GROGGY			0x80 /* about to wake up */
#define AWI_INT_CFP_ENDING		0x40 /* cont. free period ending */
#define AWI_INT_DTIM			0x20 /* beacon outgoing */
#define AWI_INT_CFP_START		0x10 /* cont. free period starting */
#define AWI_INT_SCAN_CMPLT		0x08 /* scan complete */
#define AWI_INT_TX			0x04 /* tx done */
#define AWI_INT_RX			0x02 /* rx done */
#define AWI_INT_CMD			0x01 /* cmd done */

/*
 * The following are used to implement a locking protocol between host
 * and MAC to protect the interrupt status and mask fields.
 *
 * driver: read lockout_host byte; if zero, set lockout_mac to non-zero,
 *	then reread lockout_host byte; if still zero, host has lock.
 *	if non-zero, clear lockout_mac, loop.
 */

#define AWI_LOCKOUT_MAC		0x4f5
#define AWI_LOCKOUT_HOST	0x4f6


#define AWI_INTSTAT2		0x4f7
#define AWI_INTMASK2		0x4fd

/* Bits in AWI_INTSTAT2/INTMASK2 */
#define AWI_INT2_RXMGT			0x80 		/* mgt/ps received */
#define AWI_INT2_RXDATA			0x40 		/* data received */
#define AWI_INT2_TXMGT			0x10		/* mgt tx done */
#define AWI_INT2_TXCF			0x08		/* CF tx done */
#define AWI_INT2_TXPS			0x04		/* PS tx done */
#define AWI_INT2_TXBCAST		0x02		/* Broadcast tx done */
#define AWI_INT2_TXDATA			0x01		/* data tx done */

#define AWI_DIS_PWRDN		0x4fc		/* disable powerdown if set */

#define AWI_DRIVERSTATE		0x4fe		/* driver state */

#define AWI_DRV_STATEMASK		0x0f

#define AWI_DRV_RESET			0x0
#define AWI_DRV_INFSY			0x1 /* inf synced */
#define AWI_DRV_ADHSC			0x2 /* adhoc scan */
#define AWI_DRV_ADHSY			0x3 /* adhoc synced */
#define AWI_DRV_INFSC			0x4 /* inf scanning */
#define AWI_DRV_INFAUTH			0x5 /* inf authed */
#define AWI_DRV_INFASSOC		0x6 /* inf associated */
#define AWI_DRV_INFTOSS			0x7 /* inf handoff */
#define AWI_DRV_APNONE			0x8 /* AP activity: no assoc */
#define AWI_DRV_APQUIET			0xc /* AP: >=one assoc, no traffic */
#define AWI_DRV_APLO			0xd /* AP: >=one assoc, light tfc */
#define AWI_DRV_APMED			0xe /* AP: >=one assoc, mod tfc */
#define AWI_DRV_APHIGH			0xf /* AP: >=one assoc, heavy tfc */

#define AWI_DRV_AUTORXLED		0x10
#define AWI_DRV_AUTOTXLED		0x20
#define AWI_DRV_RXLED			0x40
#define AWI_DRV_TXLED			0x80

#define AWI_VBM_OFFSET		0x500	/* Virtual Bit Map */
#define AWI_VBM_LENGTH		0x501
#define AWI_VBM_BITMAP		0x502

#define AWI_BUFFERS		0x600	/* Buffers */
#define	AWI_BUFFERS_END		0x6000

/*
 * Receive descriptors; there are a linked list of these chained
 * through the "NEXT" fields, starting from XXX
 */

#define AWI_RXD_SIZE			0x18

#define AWI_RXD_NEXT			0x4
#define AWI_RXD_NEXT_LAST		0x80000000


#define AWI_RXD_HOST_DESC_STATE		0x9

#define AWI_RXD_ST_OWN		0x80 /* host owns this */
#define AWI_RXD_ST_CONSUMED	0x40 /* host is done */
#define AWI_RXD_ST_LF		0x20 /* last frag */
#define AWI_RXD_ST_CRC		0x08 /* CRC error */
#define AWI_RXD_ST_OFLO		0x02 /* possible buffer overrun */
#define AWI_RXD_ST_RXERROR	0x01 /* this frame is borked; discard me */

#define AWI_RXD_RSSI		0xa /* 1 byte: radio strength indicator */
#define AWI_RXD_INDEX		0xb /* 1 byte: FH hop index or DS channel */
#define AWI_RXD_LOCALTIME	0xc /* 4 bytes: local time of RX */
#define AWI_RXD_START_FRAME	0x10 /* 4 bytes: ptr to first received byte */
#define AWI_RXD_LEN		0x14 /* 2 bytes: rx len in bytes */
#define AWI_RXD_RATE		0x16 /* 1 byte: rx rate in 1e5 bps */

/*
 * Transmit descriptors.
 */

#define AWI_TXD_SIZE		0x18

#define AWI_TXD_START		0x00 /* pointer to start of frame */
#define AWI_TXD_NEXT		0x04 /* pointer to next TXD */
#define AWI_TXD_LENGTH		0x08 /* length of frame */
#define AWI_TXD_STATE		0x0a /* state */

#define AWI_TXD_ST_OWN		0x80 /* MAC owns this  */
#define AWI_TXD_ST_DONE		0x40 /* MAC is done */
#define AWI_TXD_ST_REJ		0x20 /* MAC doesn't like */
#define AWI_TXD_ST_MSDU		0x10 /* MSDU timeout */
#define AWI_TXD_ST_ABRT		0x08 /* TX aborted */
#define AWI_TXD_ST_RETURNED	0x04 /* TX returned */
#define AWI_TXD_ST_RETRY	0x02 /* TX retries exceeded */
#define AWI_TXD_ST_ERROR	0x01 /* TX error */

#define AWI_TXD_RATE		0x0b /* rate */

#define AWI_RATE_1MBIT		10
#define AWI_RATE_2MBIT		20

#define AWI_TXD_NDA		0x0c /* num DIFS attempts */
#define AWI_TXD_NDF		0x0d /* num DIFS failures */
#define AWI_TXD_NSA		0x0e /* num SIFS attempts */
#define AWI_TXD_NSF		0x0f /* num SIFS failures */

#define AWI_TXD_NRA		0x14 /* num RTS attempts */
#define AWI_TXD_NDTA		0x15 /* num data attempts */
#define AWI_TXD_CTL		0x16 /* control */

#define AWI_TXD_CTL_PSN		0x80	/* preserve sequence in MAC frame */
#define AWI_TXD_CTL_BURST	0x02    /* host is doing 802.11 fragmt. */
#define AWI_TXD_CTL_FRAGS	0x01    /* override normal fragmentation */

/*
 * MIB structures.
 */

#define	AWI_ESS_ID_SIZE			(IEEE80211_NWID_LEN+2)
struct awi_mib_local {
	u_int8_t	Fragmentation_Dis;
	u_int8_t	Add_PLCP_Dis;
	u_int8_t	MAC_Hdr_Prsv;
	u_int8_t	Rx_Mgmt_Que_En;
	u_int8_t	Re_Assembly_Dis;
	u_int8_t	Strip_PLCP_Dis;
	u_int8_t	Rx_Error_Dis;
	u_int8_t	Power_Saving_Mode_Dis;
	u_int8_t	Accept_All_Multicast_Dis;
	u_int8_t	Check_Seq_Cntl_Dis;
	u_int8_t	Flush_CFP_Queue_On_CF_End;
	u_int8_t	Network_Mode;
	u_int8_t	PWD_Lvl;
	u_int8_t	CFP_Mode;
	u_int8_t	Tx_Buffer_Offset[4];
	u_int8_t	Tx_Buffer_Size[4];
	u_int8_t	Rx_Buffer_Offset[4];
	u_int8_t	Rx_Buffer_Size[4];
	u_int8_t	Acting_as_AP;
	u_int8_t	Fill_CFP;
} __attribute__((__packed__));

struct awi_mib_mac {
	u_int8_t	_Reserved1[2];
	u_int8_t	_Reserved2[2];
	u_int8_t	aRTS_Threshold[2];
	u_int8_t	aCW_max[2];
	u_int8_t	aCW_min[2];
	u_int8_t	aPromiscuous_Enable;
	u_int8_t	_Reserved3;
	u_int8_t	_Reserved4[4];
	u_int8_t	aShort_Retry_Limit;
	u_int8_t	aLong_Retry_Limit;
	u_int8_t	aMax_Frame_Length[2];
	u_int8_t	aFragmentation_Threshold[2];
	u_int8_t	aProbe_Delay[2];
	u_int8_t	aMin_Probe_Response_Time[2];
	u_int8_t	aMax_Probe_Response_Time[2];
	u_int8_t	aMax_Transmit_MSDU_Lifetime[4];
	u_int8_t	aMax_Receive_MSDU_Lifetime[4];
	u_int8_t	aStation_Basic_Rate[2];
	u_int8_t	aDesired_ESS_ID[AWI_ESS_ID_SIZE];
} __attribute__((__packed__));

struct awi_mib_stat {
	u_int8_t	aTransmitted_MPDU_Count[4];
	u_int8_t	aTransmitted_MSDU_Count[4];
	u_int8_t	aOctets_Transmitted_Cnt[4];
	u_int8_t	aMulticast_Transmitted_Frame_Count[2];
	u_int8_t	aBroadcast_Transmitted_Frame_Count[2];
	u_int8_t	aFailed_Count[4];
	u_int8_t	aRetry_Count[4];
	u_int8_t	aMultiple_Retry_Count[4];
	u_int8_t	aFrame_Duplicate_Count[4];
	u_int8_t	aRTS_Success_Count[4];
	u_int8_t	aRTS_Failure_Count[4];
	u_int8_t	aACK_Failure_Count[4];
	u_int8_t	aReceived_Frame_Count [4];
	u_int8_t	aOctets_Received_Count[4];
	u_int8_t	aMulticast_Received_Count[2];
	u_int8_t	aBroadcast_Received_Count[2];
	u_int8_t	aFCS_Error_Count[4];
	u_int8_t	aError_Count[4];
	u_int8_t	aWEP_Undecryptable_Count[4];
} __attribute__((__packed__));

struct awi_mib_mgt {
	u_int8_t	aPower_Mgt_Mode;
	u_int8_t	aScan_Mode;
#define	AWI_SCAN_PASSIVE		0x00
#define	AWI_SCAN_ACTIVE			0x01
#define	AWI_SCAN_BACKGROUND		0x02
	u_int8_t	aScan_State;
	u_int8_t	aDTIM_Period;
	u_int8_t	aATIM_Window[2];
	u_int8_t	Wep_Required;
#define	AWI_WEP_ON			0x10
#define	AWI_WEP_OFF			0x00
	u_int8_t	_Reserved1;
	u_int8_t	aBeacon_Period[2];
	u_int8_t	aPassive_Scan_Duration[2];
	u_int8_t	aListen_Interval[2];
	u_int8_t	aMedium_Occupancy_Limit[2];
	u_int8_t	aMax_MPDU_Time[2];
	u_int8_t	aCFP_Max_Duration[2];
	u_int8_t	aCFP_Rate;
	u_int8_t	Do_Not_Receive_DTIMs;
	u_int8_t	aStation_ID[2];
	u_int8_t	aCurrent_BSS_ID[ETHER_ADDR_LEN];
	u_int8_t	aCurrent_ESS_ID[AWI_ESS_ID_SIZE];
} __attribute__((__packed__));

#define	AWI_GROUP_ADDR_SIZE	4
struct awi_mib_addr {
	u_int8_t	aMAC_Address[ETHER_ADDR_LEN];
	u_int8_t	aGroup_Addresses[AWI_GROUP_ADDR_SIZE][ETHER_ADDR_LEN];
	u_int8_t	aTransmit_Enable_Status;
	u_int8_t	_Reserved1;
} __attribute__((__packed__));

#define AWI_PWR_LEVEL_SIZE 4
struct awi_mib_phy {
	u_int8_t	aSlot_Time[2];
	u_int8_t	aSIFS[2];
	u_int8_t	aMPDU_Maximum[2];
	u_int8_t	aHop_Time[2];
	u_int8_t	aSuprt_Data_Rates[4];
	u_int8_t	aCurrent_Reg_Domain;
#define	AWI_REG_DOMAIN_US	0x10
#define	AWI_REG_DOMAIN_CA	0x20
#define	AWI_REG_DOMAIN_EU	0x30
#define	AWI_REG_DOMAIN_ES	0x31
#define	AWI_REG_DOMAIN_FR	0x32
#define	AWI_REG_DOMAIN_JP	0x40
	u_int8_t	aPreamble_Lngth;
	u_int8_t	aPLCP_Hdr_Lngth;
	u_int8_t	Pwr_Up_Time[AWI_PWR_LEVEL_SIZE][2];
	u_int8_t	IEEE_PHY_Type;
#define	AWI_PHY_TYPE_FH		1
#define	AWI_PHY_TYPE_DS		2
#define	AWI_PHY_TYPE_IR		3
	u_int8_t	RCR_33A_Bits[8];
} __attribute__((__packed__));

#endif /* _DEV_IC_AWIREG_H */
