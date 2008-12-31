/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 * This software is derived from software provide by Kwikbyte who specifically
 * disclaimed copyright on the code.
 *
 * $FreeBSD: src/sys/boot/arm/at91/libat91/mci_device.h,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

//*---------------------------------------------------------------------------
//*         ATMEL Microcontroller Software Support  -  ROUSSET  -
//*---------------------------------------------------------------------------
//* The software is delivered "AS IS" without warranty or condition of any
//* kind, either express, implied or statutory. This includes without
//* limitation any warranty or condition with respect to merchantability or
//* fitness for any particular purpose, or against the infringements of
//* intellectual property rights of others.
//*---------------------------------------------------------------------------
//* File Name           : AT91C_MCI_Device.h
//* Object              : Data Flash Atmel Description File
//* Translator          :
//*
//* 1.0 26/11/02 FB		: Creation
//*---------------------------------------------------------------------------

#ifndef __MCI_Device_h
#define __MCI_Device_h

#include <sys/types.h>

typedef unsigned int AT91S_MCIDeviceStatus;

///////////////////////////////////////////////////////////////////////////////

#define AT91C_CARD_REMOVED			0
#define AT91C_MMC_CARD_INSERTED		1
#define AT91C_SD_CARD_INSERTED		2

#define AT91C_NO_ARGUMENT			0x0

#define AT91C_FIRST_RCA				0xCAFE
#define AT91C_MAX_MCI_CARDS			10

#define AT91C_BUS_WIDTH_1BIT		0x00
#define AT91C_BUS_WIDTH_4BITS		0x02

/* Driver State */
#define AT91C_MCI_IDLE       		0x0
#define AT91C_MCI_TIMEOUT_ERROR		0x1
#define AT91C_MCI_RX_SINGLE_BLOCK	0x2
#define AT91C_MCI_RX_MULTIPLE_BLOCK	0x3
#define AT91C_MCI_RX_STREAM			0x4
#define AT91C_MCI_TX_SINGLE_BLOCK	0x5
#define AT91C_MCI_TX_MULTIPLE_BLOCK	0x6
#define AT91C_MCI_TX_STREAM 		0x7

/* TimeOut */
#define AT91C_TIMEOUT_CMDRDY		30



///////////////////////////////////////////////////////////////////////////////
// MMC & SDCard Structures 
///////////////////////////////////////////////////////////////////////////////

/*---------------------------------------------*/
/* MCI Device Structure Definition 			   */
/*---------------------------------------------*/
typedef struct _AT91S_MciDevice
{
	volatile unsigned char	state;
	unsigned char	SDCard_bus_width;
	unsigned int 	RCA;		// RCA
	unsigned int	READ_BL_LEN;
#ifdef REPORT_SIZE
	unsigned int	Memory_Capacity;
#endif
} AT91S_MciDevice;

#include <dev/mmc/mmcreg.h>

///////////////////////////////////////////////////////////////////////////////
// Functions returnals
///////////////////////////////////////////////////////////////////////////////
#define AT91C_CMD_SEND_OK		0		// Command ok
#define AT91C_CMD_SEND_ERROR		-1		// Command failed
#define AT91C_INIT_OK			2		// Init Successfull
#define AT91C_INIT_ERROR		3		// Init Failed
#define AT91C_READ_OK			4		// Read Successfull
#define AT91C_READ_ERROR		5		// Read Failed
#define AT91C_WRITE_OK			6		// Write Successfull
#define AT91C_WRITE_ERROR		7		// Write Failed
#define AT91C_ERASE_OK			8		// Erase Successfull
#define AT91C_ERASE_ERROR		9		// Erase Failed
#define AT91C_CARD_SELECTED_OK		10		// Card Selection Successfull
#define AT91C_CARD_SELECTED_ERROR	11		// Card Selection Failed

#define AT91C_MCI_SR_ERROR (AT91C_MCI_UNRE | AT91C_MCI_OVRE | AT91C_MCI_DTOE | \
	AT91C_MCI_DCRCE | AT91C_MCI_RTOE | AT91C_MCI_RENDE | AT91C_MCI_RCRCE | \
	AT91C_MCI_RDIRE | AT91C_MCI_RINDE)

#define	MMC_CMDNB       (0x1Fu <<  0)		// Command Number
#define	MMC_RSPTYP      (0x3u <<  6)		// Response Type
#define	    MMC_RSPTYP_NO      (0x0u <<  6)	// No response
#define	    MMC_RSPTYP_48      (0x1u <<  6)	// 48-bit response
#define	    MMC_RSPTYP_136     (0x2u <<  6)	// 136-bit response
#define	MMC_SPCMD       (0x7u <<  8)		// Special CMD
#define	    MMC_SPCMD_NONE     (0x0u <<  8)	// Not a special CMD
#define	    MMC_SPCMD_INIT     (0x1u <<  8)	// Initialization CMD
#define	    MMC_SPCMD_SYNC     (0x2u <<  8)	// Synchronized CMD
#define	    MMC_SPCMD_IT_CMD   (0x4u <<  8)	// Interrupt command
#define	    MMC_SPCMD_IT_REP   (0x5u <<  8)	// Interrupt response
#define	MMC_OPDCMD      (0x1u << 11)		// Open Drain Command
#define	MMC_MAXLAT      (0x1u << 12)		// Maximum Latency for Command to respond
#define	MMC_TRCMD       (0x3u << 16)		// Transfer CMD
#define	    MMC_TRCMD_NO       (0x0u << 16)	// No transfer
#define	    MMC_TRCMD_START    (0x1u << 16)	// Start transfer
#define	    MMC_TRCMD_STOP     (0x2u << 16)	// Stop transfer
#define	MMC_TRDIR       (0x1u << 18)		// Transfer Direction
#define	MMC_TRTYP       (0x3u << 19)		// Transfer Type
#define	    MMC_TRTYP_BLOCK    (0x0u << 19)	// Block Transfer type
#define	    MMC_TRTYP_MULTIPLE (0x1u << 19)	// Multiple Block transfer type
#define	    MMC_TRTYP_STREAM   (0x2u << 19)	// Stream transfer type

///////////////////////////////////////////////////////////////////////////////
// MCI_CMD Register Value 
///////////////////////////////////////////////////////////////////////////////
#define POWER_ON_INIT	\
    (0 | MMC_TRCMD_NO | MMC_SPCMD_INIT | MMC_OPDCMD)

/////////////////////////////////////////////////////////////////	
// Class 0 & 1 commands: Basic commands and Read Stream commands
/////////////////////////////////////////////////////////////////

#define GO_IDLE_STATE_CMD	\
    (0 | MMC_TRCMD_NO | MMC_SPCMD_NONE )
#define MMC_GO_IDLE_STATE_CMD \
    (0 | MMC_TRCMD_NO | MMC_SPCMD_NONE | MMC_OPDCMD)
#define MMC_SEND_OP_COND_CMD \
    (1 | MMC_TRCMD_NO | MMC_SPCMD_NONE | MMC_RSPTYP_48 | \
     MMC_OPDCMD)

#define ALL_SEND_CID_CMD \
    (2 | MMC_TRCMD_NO | MMC_SPCMD_NONE | MMC_RSPTYP_136)
#define MMC_ALL_SEND_CID_CMD \
    (2 | MMC_TRCMD_NO | MMC_SPCMD_NONE | MMC_RSPTYP_136 | \
    MMC_OPDCMD)

#define SET_RELATIVE_ADDR_CMD \
    (3 | MMC_TRCMD_NO | MMC_SPCMD_NONE | MMC_RSPTYP_48 | \
     MMC_MAXLAT)
#define MMC_SET_RELATIVE_ADDR_CMD \
    (3 | MMC_TRCMD_NO | MMC_SPCMD_NONE | MMC_RSPTYP_48 | \
     MMC_MAXLAT | MMC_OPDCMD)

#define SET_DSR_CMD \
    (4 | MMC_TRCMD_NO | MMC_SPCMD_NONE | MMC_RSPTYP_NO | \
     MMC_MAXLAT)	// no tested

#define SEL_DESEL_CARD_CMD \
    (7 | MMC_TRCMD_NO | MMC_SPCMD_NONE | MMC_RSPTYP_48 | \
     MMC_MAXLAT)
#define SEND_CSD_CMD \
    (9 | MMC_TRCMD_NO | MMC_SPCMD_NONE | MMC_RSPTYP_136 | \
     MMC_MAXLAT)
#define SEND_CID_CMD \
    (10 | MMC_TRCMD_NO | MMC_SPCMD_NONE | MMC_RSPTYP_136 | \
     MMC_MAXLAT)
#define MMC_READ_DAT_UNTIL_STOP_CMD \
    (11 | MMC_TRTYP_STREAM | MMC_SPCMD_NONE | \
     MMC_RSPTYP_48 | MMC_TRDIR | MMC_TRCMD_START | \
     MMC_MAXLAT)

#define STOP_TRANSMISSION_CMD \
    (12 | MMC_TRCMD_STOP | MMC_SPCMD_NONE | MMC_RSPTYP_48 | \
     MMC_MAXLAT)
#define STOP_TRANSMISSION_SYNC_CMD \
    (12 | MMC_TRCMD_STOP | MMC_SPCMD_SYNC | MMC_RSPTYP_48 | \
     MMC_MAXLAT)
#define SEND_STATUS_CMD \
    (13 | MMC_TRCMD_NO | MMC_SPCMD_NONE | MMC_RSPTYP_48 | \
     MMC_MAXLAT)
#define GO_INACTIVE_STATE_CMD \
     (15 | MMC_RSPTYP_NO)

//*------------------------------------------------
//* Class 2 commands: Block oriented Read commands
//*------------------------------------------------

#define SET_BLOCKLEN_CMD					(16 | MMC_TRCMD_NO 	| MMC_SPCMD_NONE	| MMC_RSPTYP_48		| MMC_MAXLAT )
#define READ_SINGLE_BLOCK_CMD				(17 | MMC_SPCMD_NONE	| MMC_RSPTYP_48 	| MMC_TRCMD_START	| MMC_TRTYP_BLOCK	| MMC_TRDIR	| MMC_MAXLAT)
#define READ_MULTIPLE_BLOCK_CMD			(18 | MMC_SPCMD_NONE	| MMC_RSPTYP_48 	| MMC_TRCMD_START	| MMC_TRTYP_MULTIPLE	| MMC_TRDIR	| MMC_MAXLAT)

//*--------------------------------------------
//* Class 3 commands: Sequential write commands
//*--------------------------------------------

#define MMC_WRITE_DAT_UNTIL_STOP_CMD		(20 | MMC_TRTYP_STREAM| MMC_SPCMD_NONE	| MMC_RSPTYP_48 & ~(MMC_TRDIR) | MMC_TRCMD_START | MMC_MAXLAT )	// MMC

//*------------------------------------------------
//* Class 4 commands: Block oriented write commands
//*------------------------------------------------
	
#define WRITE_BLOCK_CMD					(24 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_START	| (MMC_TRTYP_BLOCK 	&  ~(MMC_TRDIR))	| MMC_MAXLAT)
#define WRITE_MULTIPLE_BLOCK_CMD			(25 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_START	| (MMC_TRTYP_MULTIPLE	&  ~(MMC_TRDIR)) 	| MMC_MAXLAT)
#define PROGRAM_CSD_CMD					(27 | MMC_RSPTYP_48 )


//*----------------------------------------
//* Class 6 commands: Group Write protect
//*----------------------------------------

#define SET_WRITE_PROT_CMD				(28	| MMC_RSPTYP_48 )
#define CLR_WRITE_PROT_CMD				(29	| MMC_RSPTYP_48 )
#define SEND_WRITE_PROT_CMD				(30	| MMC_RSPTYP_48 )


//*----------------------------------------
//* Class 5 commands: Erase commands
//*----------------------------------------

#define TAG_SECTOR_START_CMD				(32 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO	| MMC_MAXLAT)
#define TAG_SECTOR_END_CMD  				(33 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO	| MMC_MAXLAT)
#define MMC_UNTAG_SECTOR_CMD				(34 | MMC_RSPTYP_48 )
#define MMC_TAG_ERASE_GROUP_START_CMD		(35 | MMC_RSPTYP_48 )
#define MMC_TAG_ERASE_GROUP_END_CMD		(36 | MMC_RSPTYP_48 )
#define MMC_UNTAG_ERASE_GROUP_CMD			(37 | MMC_RSPTYP_48 )
#define ERASE_CMD							(38 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO	| MMC_MAXLAT )

//*----------------------------------------
//* Class 7 commands: Lock commands
//*----------------------------------------

#define LOCK_UNLOCK						(42 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO	| MMC_MAXLAT)	// no tested

//*-----------------------------------------------
// Class 8 commands: Application specific commands
//*-----------------------------------------------

#define APP_CMD							(55 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO | MMC_MAXLAT)
#define GEN_CMD							(56 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO | MMC_MAXLAT)	// no tested

#define SDCARD_SET_BUS_WIDTH_CMD			(6 	| MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO	| MMC_MAXLAT)
#define SDCARD_STATUS_CMD					(13 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO	| MMC_MAXLAT)
#define SDCARD_SEND_NUM_WR_BLOCKS_CMD		(22 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO	| MMC_MAXLAT)
#define SDCARD_SET_WR_BLK_ERASE_COUNT_CMD	(23 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO	| MMC_MAXLAT)
#define SDCARD_APP_OP_COND_CMD			(41 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO )
#define SDCARD_SET_CLR_CARD_DETECT_CMD	(42 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO	| MMC_MAXLAT)
#define SDCARD_SEND_SCR_CMD				(51 | MMC_SPCMD_NONE	| MMC_RSPTYP_48	| MMC_TRCMD_NO	| MMC_MAXLAT)

#define SDCARD_APP_ALL_CMD				(SDCARD_SET_BUS_WIDTH_CMD +\
												SDCARD_STATUS_CMD +\
												SDCARD_SEND_NUM_WR_BLOCKS_CMD +\
												SDCARD_SET_WR_BLK_ERASE_COUNT_CMD +\
												SDCARD_APP_OP_COND_CMD +\
												SDCARD_SET_CLR_CARD_DETECT_CMD +\
												SDCARD_SEND_SCR_CMD)

//*----------------------------------------
//* Class 9 commands: IO Mode commands
//*----------------------------------------

#define MMC_FAST_IO_CMD					(39 | MMC_SPCMD_NONE | MMC_RSPTYP_48 | MMC_MAXLAT)
#define MMC_GO_IRQ_STATE_CMD				(40 | MMC_SPCMD_NONE | MMC_RSPTYP_48 | MMC_TRCMD_NO	| MMC_MAXLAT)

///////////////////////////////////////////////////////////////////////////////
// OCR Register
///////////////////////////////////////////////////////////////////////////////
#define AT91C_VDD_16_17					(1 << 4)
#define AT91C_VDD_17_18					(1 << 5)
#define AT91C_VDD_18_19					(1 << 6)
#define AT91C_VDD_19_20					(1 << 7)
#define AT91C_VDD_20_21					(1 << 8)
#define AT91C_VDD_21_22					(1 << 9)
#define AT91C_VDD_22_23					(1 << 10)
#define AT91C_VDD_23_24					(1 << 11)
#define AT91C_VDD_24_25					(1 << 12)
#define AT91C_VDD_25_26					(1 << 13)
#define AT91C_VDD_26_27					(1 << 14)
#define AT91C_VDD_27_28					(1 << 15)
#define AT91C_VDD_28_29					(1 << 16)
#define AT91C_VDD_29_30					(1 << 17)
#define AT91C_VDD_30_31					(1 << 18)
#define AT91C_VDD_31_32					(1 << 19)
#define AT91C_VDD_32_33					(1 << 20)
#define AT91C_VDD_33_34					(1 << 21)
#define AT91C_VDD_34_35					(1 << 22)
#define AT91C_VDD_35_36					(1 << 23)
#define AT91C_CARD_POWER_UP_BUSY		(1 << 31)

#define AT91C_MMC_HOST_VOLTAGE_RANGE	(AT91C_VDD_27_28 | AT91C_VDD_28_29  | \
    AT91C_VDD_29_30 | AT91C_VDD_30_31 | AT91C_VDD_31_32 | AT91C_VDD_32_33)

///////////////////////////////////////////////////////////////////////////////
// CURRENT_STATE & READY_FOR_DATA in SDCard Status Register definition (response type R1)
///////////////////////////////////////////////////////////////////////////////
#define AT91C_SR_READY_FOR_DATA				(1 << 8)	// corresponds to buffer empty signalling on the bus
#define AT91C_SR_IDLE						(0 << 9)
#define AT91C_SR_READY						(1 << 9)
#define AT91C_SR_IDENT						(2 << 9)
#define AT91C_SR_STBY						(3 << 9)
#define AT91C_SR_TRAN						(4 << 9)
#define AT91C_SR_DATA						(5 << 9)
#define AT91C_SR_RCV						(6 << 9)
#define AT91C_SR_PRG						(7 << 9)
#define AT91C_SR_DIS						(8 << 9)

#define AT91C_SR_CARD_SELECTED				(AT91C_SR_READY_FOR_DATA + AT91C_SR_TRAN)

#define MMC_FIRST_RCA				0xCAFE

///////////////////////////////////////////////////////////////////////////////
// MMC CSD register header File					
// CSD_x_xxx_S	for shift value for word x
// CSD_x_xxx_M	for mask  value for word x
///////////////////////////////////////////////////////////////////////////////

// First Response INT <=> CSD[3] : bits 0 to 31
#define	CSD_3_BIT0_S		0		// [0:0]			
#define	CSD_3_BIT0_M		0x01				
#define	CSD_3_CRC_S		1		// [7:1]
#define	CSD_3_CRC_M		0x7F
#define	CSD_3_MMC_ECC_S		8		// [9:8] reserved for MMC compatibility
#define	CSD_3_MMC_ECC_M		0x03
#define	CSD_3_FILE_FMT_S	10		// [11:10]
#define	CSD_3_FILE_FMT_M	0x03
#define	CSD_3_TMP_WP_S		12		// [12:12]
#define	CSD_3_TMP_WP_M		0x01
#define	CSD_3_PERM_WP_S 	13		// [13:13]
#define	CSD_3_PERM_WP_M 	0x01
#define	CSD_3_COPY_S		14		// [14:14]
#define	CSD_3_COPY_M 		0x01
#define	CSD_3_FILE_FMT_GRP_S	15		// [15:15]
#define	CSD_3_FILE_FMT_GRP_M	0x01
//	reserved		16		// [20:16]
//	reserved		0x1F
#define	CSD_3_WBLOCK_P_S	21		// [21:21]
#define	CSD_3_WBLOCK_P_M	0x01
#define	CSD_3_WBLEN_S 		22		// [25:22]
#define	CSD_3_WBLEN_M 		0x0F
#define	CSD_3_R2W_F_S 		26		// [28:26]
#define	CSD_3_R2W_F_M 		0x07
#define	CSD_3_MMC_DEF_ECC_S	29		// [30:29] reserved for MMC compatibility
#define	CSD_3_MMC_DEF_ECC_M	0x03
#define	CSD_3_WP_GRP_EN_S	31		// [31:31]
#define	CSD_3_WP_GRP_EN_M 	0x01

// Seconde Response INT <=> CSD[2] : bits 32 to 63
#define	CSD_2_v21_WP_GRP_SIZE_S	0		// [38:32]				
#define	CSD_2_v21_WP_GRP_SIZE_M	0x7F				
#define	CSD_2_v21_SECT_SIZE_S	7		// [45:39]
#define	CSD_2_v21_SECT_SIZE_M	0x7F
#define	CSD_2_v21_ER_BLEN_EN_S	14		// [46:46]
#define	CSD_2_v21_ER_BLEN_EN_M	0x01

#define	CSD_2_v22_WP_GRP_SIZE_S	0		// [36:32]				
#define	CSD_2_v22_WP_GRP_SIZE_M	0x1F				
#define	CSD_2_v22_ER_GRP_SIZE_S	5		// [41:37]
#define	CSD_2_v22_ER_GRP_SIZE_M	0x1F
#define	CSD_2_v22_SECT_SIZE_S	10		// [46:42]
#define	CSD_2_v22_SECT_SIZE_M	0x1F

#define	CSD_2_C_SIZE_M_S	15		// [49:47]
#define	CSD_2_C_SIZE_M_M	0x07
#define	CSD_2_VDD_WMAX_S	18		// [52:50]
#define	CSD_2_VDD_WMAX_M	0x07
#define	CSD_2_VDD_WMIN_S 	21		// [55:53]
#define	CSD_2_VDD_WMIN_M	0x07
#define	CSD_2_RCUR_MAX_S	24		// [58:56]
#define	CSD_2_RCUR_MAX_M	0x07
#define	CSD_2_RCUR_MIN_S	27		// [61:59]
#define	CSD_2_RCUR_MIN_M	0x07
#define	CSD_2_CSIZE_L_S		30		// [63:62] <=> 2 LSB of CSIZE
#define	CSD_2_CSIZE_L_M		0x03

// Third Response INT <=> CSD[1] : bits 64 to 95
#define	CSD_1_CSIZE_H_S		0	// [73:64]	<=> 10 MSB of CSIZE
#define	CSD_1_CSIZE_H_M		0x03FF
// reserved			10		// [75:74]
// reserved			0x03		
#define	CSD_1_DSR_I_S 		12		// [76:76]
#define	CSD_1_DSR_I_M 		0x01
#define	CSD_1_RD_B_MIS_S	13		// [77:77]
#define	CSD_1_RD_B_MIS_M	0x01
#define	CSD_1_WR_B_MIS_S	14		// [78:78]
#define	CSD_1_WR_B_MIS_M	0x01
#define	CSD_1_RD_B_PAR_S	15		// [79:79]
#define	CSD_1_RD_B_PAR_M	0x01
#define	CSD_1_RD_B_LEN_S	16		// [83:80]
#define	CSD_1_RD_B_LEN_M	0x0F
#define	CSD_1_CCC_S		20		// [95:84]
#define	CSD_1_CCC_M 		0x0FFF

// Fourth Response INT <=> CSD[0] : bits 96 to 127
#define	CSD_0_TRANS_SPEED_S 	0		// [103:96]
#define	CSD_0_TRANS_SPEED_M 	0xFF
#define	CSD_0_NSAC_S		8		// [111:104]
#define	CSD_0_NSAC_M		0xFF
#define	CSD_0_TAAC_S		16		// [119:112]
#define	CSD_0_TAAC_M 		0xFF
//	reserved		24		// [121:120]
//	reserved		0x03
#define	CSD_0_MMC_SPEC_VERS_S	26		// [125:122]	reserved for MMC compatibility
#define	CSD_0_MMC_SPEC_VERS_M	0x0F
#define	CSD_0_STRUCT_S		30	// [127:126]
#define	CSD_0_STRUCT_M 		0x03

///////////////////////////////////////////////////////////////////////////////
#endif
