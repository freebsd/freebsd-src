/**************************************************************************
 * Initio A100 device driver for Linux.
 *
 * Copyright (c) 1994-1998 Initio Corporation
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * --------------------------------------------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Where this Software is combined with software released under the terms of 
 * the GNU General Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under the terms of the GPL, the terms
 * and conditions of this License will apply in addition to those of the
 * GPL with the exception of any terms or conditions of this License that
 * conflict with, or are expressly prohibited by, the GPL.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 **************************************************************************
 *
 * Module: inia100.h
 * Description: INI-A100U2W LINUX device driver header
 * Revision History:
 *	06/18/98 HL, Initial production Version 1.02
 *	12/19/98 bv, Use spinlocks for 2.1.95 and up
 ****************************************************************************/

#ifndef	CVT_LINUX_VERSION
#define	CVT_LINUX_VERSION(V,P,S)	(((V) * 65536) + ((P) * 256) + (S))
#endif

#ifndef	LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#include <linux/types.h>

#include "sd.h"

extern int inia100_detect(Scsi_Host_Template *);
extern int inia100_release(struct Scsi_Host *);
extern int inia100_command(Scsi_Cmnd *);
extern int inia100_queue(Scsi_Cmnd *, void (*done) (Scsi_Cmnd *));
extern int inia100_abort(Scsi_Cmnd *);
extern int inia100_reset(Scsi_Cmnd *, unsigned int);

extern int inia100_biosparam(Scsi_Disk *, kdev_t, int *);	/*for linux v2.0 */

#define inia100_REVID "Initio INI-A100U2W SCSI device driver; Revision: 1.02c"

#define INIA100	{ \
	next:		NULL,						\
	module:		NULL,						\
	proc_name:	"INIA100", \
	proc_info:	NULL,				\
	name:		inia100_REVID, \
	detect:		inia100_detect, \
	release:	inia100_release, \
	info:		NULL,					\
	command:	inia100_command, \
	queuecommand:	inia100_queue, \
 	eh_strategy_handler: NULL, \
 	eh_abort_handler: NULL, \
 	eh_device_reset_handler: NULL, \
 	eh_bus_reset_handler: NULL, \
 	eh_host_reset_handler: NULL, \
	abort:		inia100_abort, \
	reset:		inia100_reset, \
	slave_attach:	NULL, \
	bios_param:	inia100_biosparam, \
	can_queue:	1, \
	this_id:	1, \
	sg_tablesize:	SG_ALL, \
	cmd_per_lun: 	1, \
	present:	0, \
	unchecked_isa_dma: 0, \
	use_clustering:	ENABLE_CLUSTERING, \
 use_new_eh_code: 0 \
}

#define VIRT_TO_BUS(i)  (unsigned int) virt_to_bus((void *)(i))
#define ULONG   unsigned long
#define PVOID   void *
#define USHORT  unsigned short
#define UCHAR   unsigned char
#define BYTE    unsigned char
#define WORD    unsigned short
#define DWORD   unsigned long
#define UBYTE   unsigned char
#define UWORD   unsigned short
#define UDWORD  unsigned long
#define U32     u32

#ifndef NULL
#define NULL     0		/* zero          */
#endif
#ifndef TRUE
#define TRUE     (1)		/* boolean true  */
#endif
#ifndef FALSE
#define FALSE    (0)		/* boolean false */
#endif
#ifndef FAILURE
#define FAILURE  (-1)
#endif
#if 1
#define ORC_MAXQUEUE		245
#else
#define ORC_MAXQUEUE		25
#endif

#define TOTAL_SG_ENTRY		32
#define MAX_TARGETS		16
#define IMAX_CDB			15
#define SENSE_SIZE		14
#define MAX_SUPPORTED_ADAPTERS  4
#define SUCCESSFUL              0x00

#define I920_DEVICE_ID	0x0002	/* Initio's inic-950 product ID   */

/************************************************************************/
/*              Scatter-Gather Element Structure                        */
/************************************************************************/
typedef struct ORC_SG_Struc {
	U32 SG_Ptr;		/* Data Pointer */
	U32 SG_Len;		/* Data Length */
} ORC_SG;


/* SCSI related definition                                              */
#define DISC_NOT_ALLOW          0x80	/* Disconnect is not allowed    */
#define DISC_ALLOW              0xC0	/* Disconnect is allowed        */


#define ORC_OFFSET_SCB			16
#define ORC_MAX_SCBS		    250
#define MAX_CHANNELS       2
#define MAX_ESCB_ELE				64
#define TCF_DRV_255_63     0x0400

/********************************************************/
/*      Orchid Configuration Register Set               */
/********************************************************/
#define ORC_PVID	0x00	/* Vendor ID                      */
#define ORC_VENDOR_ID	0x1101	/* Orchid vendor ID               */
#define ORC_PDID        0x02	/* Device ID                    */
#define ORC_DEVICE_ID	0x1060	/* Orchid device ID               */
#define ORC_COMMAND	0x04	/* Command                        */
#define BUSMS		0x04	/* BUS MASTER Enable              */
#define IOSPA		0x01	/* IO Space Enable                */
#define ORC_STATUS	0x06	/* Status register                */
#define ORC_REVISION	0x08	/* Revision number                */
#define ORC_BASE	0x10	/* Base address                   */
#define ORC_BIOS	0x50	/* Expansion ROM base address     */
#define ORC_INT_NUM	0x3C	/* Interrupt line         */
#define ORC_INT_PIN	0x3D	/* Interrupt pin          */

/********************************************************/
/*      Orchid Host Command Set                         */
/********************************************************/
#define ORC_CMD_NOP		0x00	/* Host command - NOP             */
#define ORC_CMD_VERSION		0x01	/* Host command - Get F/W version */
#define ORC_CMD_ECHO		0x02	/* Host command - ECHO            */
#define ORC_CMD_SET_NVM		0x03	/* Host command - Set NVRAM       */
#define ORC_CMD_GET_NVM		0x04	/* Host command - Get NVRAM       */
#define ORC_CMD_GET_BUS_STATUS	0x05	/* Host command - Get SCSI bus status */
#define ORC_CMD_ABORT_SCB	0x06	/* Host command - Abort SCB       */
#define ORC_CMD_ISSUE_SCB	0x07	/* Host command - Issue SCB       */

/********************************************************/
/*              Orchid Register Set                     */
/********************************************************/
#define ORC_GINTS	0xA0	/* Global Interrupt Status        */
#define QINT		0x04	/* Reply Queue Interrupt  */
#define ORC_GIMSK	0xA1	/* Global Interrupt MASK  */
#define MQINT		0x04	/* Mask Reply Queue Interrupt     */
#define	ORC_GCFG	0xA2	/* Global Configure               */
#define EEPRG		0x01	/* Enable EEPROM programming */
#define	ORC_GSTAT	0xA3	/* Global status          */
#define WIDEBUS		0x10	/* Wide SCSI Devices connected    */
#define ORC_HDATA	0xA4	/* Host Data                      */
#define ORC_HCTRL	0xA5	/* Host Control                   */
#define SCSIRST		0x80	/* SCSI bus reset         */
#define HDO			0x40	/* Host data out          */
#define HOSTSTOP		0x02	/* Host stop RISC engine  */
#define DEVRST		0x01	/* Device reset                   */
#define ORC_HSTUS	0xA6	/* Host Status                    */
#define HDI			0x02	/* Host data in                   */
#define RREADY		0x01	/* RISC engine is ready to receive */
#define	ORC_NVRAM	0xA7	/* Nvram port address             */
#define SE2CS		0x008
#define SE2CLK		0x004
#define SE2DO		0x002
#define SE2DI		0x001
#define ORC_PQUEUE	0xA8	/* Posting queue FIFO             */
#define ORC_PQCNT	0xA9	/* Posting queue FIFO Cnt */
#define ORC_RQUEUE	0xAA	/* Reply queue FIFO               */
#define ORC_RQUEUECNT	0xAB	/* Reply queue FIFO Cnt           */
#define	ORC_FWBASEADR	0xAC	/* Firmware base address  */

#define	ORC_EBIOSADR0 0xB0	/* External Bios address */
#define	ORC_EBIOSADR1 0xB1	/* External Bios address */
#define	ORC_EBIOSADR2 0xB2	/* External Bios address */
#define	ORC_EBIOSDATA 0xB3	/* External Bios address */

#define	ORC_SCBSIZE	0xB7	/* SCB size register              */
#define	ORC_SCBBASE0	0xB8	/* SCB base address 0             */
#define	ORC_SCBBASE1	0xBC	/* SCB base address 1             */

#define	ORC_RISCCTL	0xE0	/* RISC Control                   */
#define PRGMRST		0x002
#define DOWNLOAD		0x001
#define	ORC_PRGMCTR0	0xE2	/* RISC program counter           */
#define	ORC_PRGMCTR1	0xE3	/* RISC program counter           */
#define	ORC_RISCRAM	0xEC	/* RISC RAM data port 4 bytes     */

typedef struct orc_extended_scb {	/* Extended SCB                 */
	ORC_SG ESCB_SGList[TOTAL_SG_ENTRY];	/*0 Start of SG list              */
	Scsi_Cmnd *SCB_Srb;	/*50 SRB Pointer */
} ESCB;

/***********************************************************************
		SCSI Control Block
************************************************************************/
typedef struct orc_scb {	/* Scsi_Ctrl_Blk                */
	UBYTE SCB_Opcode;	/*00 SCB command code&residual  */
	UBYTE SCB_Flags;	/*01 SCB Flags                  */
	UBYTE SCB_Target;	/*02 Target Id                  */
	UBYTE SCB_Lun;		/*03 Lun                        */
	U32 SCB_Reserved0;	/*04 Reserved for ORCHID must 0 */
	U32 SCB_XferLen;	/*08 Data Transfer Length       */
	U32 SCB_Reserved1;	/*0C Reserved for ORCHID must 0 */
	U32 SCB_SGLen;		/*10 SG list # * 8              */
	U32 SCB_SGPAddr;	/*14 SG List Buf physical Addr  */
	U32 SCB_SGPAddrHigh;	/*18 SG Buffer high physical Addr */
	UBYTE SCB_HaStat;	/*1C Host Status                */
	UBYTE SCB_TaStat;	/*1D Target Status              */
	UBYTE SCB_Status;	/*1E SCB status                 */
	UBYTE SCB_Link;		/*1F Link pointer, default 0xFF */
	UBYTE SCB_SenseLen;	/*20 Sense Allocation Length    */
	UBYTE SCB_CDBLen;	/*21 CDB Length                 */
	UBYTE SCB_Ident;	/*22 Identify                   */
	UBYTE SCB_TagMsg;	/*23 Tag Message                */
	UBYTE SCB_CDB[IMAX_CDB];	/*24 SCSI CDBs                  */
	UBYTE SCB_ScbIdx;	/*3C Index for this ORCSCB      */
	U32 SCB_SensePAddr;	/*34 Sense Buffer physical Addr */

	ESCB *SCB_EScb;		/*38 Extended SCB Pointer       */
#ifndef ALPHA
	UBYTE SCB_Reserved2[4];	/*3E Reserved for Driver use    */
#endif
} ORC_SCB;

/* Opcodes of ORCSCB_Opcode */
#define ORC_EXECSCSI	0x00	/* SCSI initiator command with residual */
#define ORC_BUSDEVRST	0x01	/* SCSI Bus Device Reset  */

/* Status of ORCSCB_Status */
#define ORCSCB_COMPLETE	0x00	/* SCB request completed  */
#define ORCSCB_POST	0x01	/* SCB is posted by the HOST      */

/* Bit Definition for ORCSCB_Flags */
#define SCF_DISINT	0x01	/* Disable HOST interrupt */
#define SCF_DIR		0x18	/* Direction bits         */
#define SCF_NO_DCHK	0x00	/* Direction determined by SCSI   */
#define SCF_DIN		0x08	/* From Target to Initiator       */
#define SCF_DOUT	0x10	/* From Initiator to Target       */
#define SCF_NO_XF	0x18	/* No data transfer               */
#define SCF_POLL   0x40

/* Error Codes for ORCSCB_HaStat */
#define HOST_SEL_TOUT	0x11
#define HOST_DO_DU	0x12
#define HOST_BUS_FREE	0x13
#define HOST_BAD_PHAS	0x14
#define HOST_INV_CMD	0x16
#define HOST_SCSI_RST	0x1B
#define HOST_DEV_RST	0x1C


/* Error Codes for ORCSCB_TaStat */
#define TARGET_CHK_COND	0x02
#define TARGET_BUSY	0x08
#define TARGET_TAG_FULL	0x28


/* Queue tag msg: Simple_quque_tag, Head_of_queue_tag, Ordered_queue_tag */
#define MSG_STAG	0x20
#define MSG_HTAG	0x21
#define MSG_OTAG	0x22

#define MSG_IGNOREWIDE	0x23

#define MSG_IDENT	0x80
#define MSG_DISC	0x40	/* Disconnect allowed             */


/* SCSI MESSAGE */
#define	MSG_EXTEND	0x01
#define	MSG_SDP		0x02
#define	MSG_ABORT	0x06
#define	MSG_REJ		0x07
#define	MSG_NOP		0x08
#define	MSG_PARITY	0x09
#define	MSG_DEVRST	0x0C
#define	MSG_STAG	0x20

/***********************************************************************
		Target Device Control Structure
**********************************************************************/

typedef struct ORC_Tar_Ctrl_Struc {
	UBYTE TCS_DrvDASD;	/* 6 */
	UBYTE TCS_DrvSCSI;	/* 7 */
	UBYTE TCS_DrvHead;	/* 8 */
	UWORD TCS_DrvFlags;	/* 4 */
	UBYTE TCS_DrvSector;	/* 7 */
} ORC_TCS, *PORC_TCS;

/* Bit Definition for TCF_DrvFlags */
#define	TCS_DF_NODASD_SUPT	0x20	/* Suppress OS/2 DASD Mgr support */
#define	TCS_DF_NOSCSI_SUPT	0x40	/* Suppress OS/2 SCSI Mgr support */


/***********************************************************************
              Host Adapter Control Structure
************************************************************************/
typedef struct ORC_Ha_Ctrl_Struc {
	USHORT HCS_Base;	/* 00 */
	UBYTE HCS_Index;	/* 02 */
	UBYTE HCS_Intr;		/* 04 */
	UBYTE HCS_SCSI_ID;	/* 06    H/A SCSI ID */
	UBYTE HCS_BIOS;		/* 07    BIOS configuration */

	UBYTE HCS_Flags;	/* 0B */
	UBYTE HCS_HAConfig1;	/* 1B    SCSI0MAXTags */
	UBYTE HCS_MaxTar;	/* 1B    SCSI0MAXTags */

	USHORT HCS_Units;	/* Number of units this adapter  */
	USHORT HCS_AFlags;	/* Adapter info. defined flags   */
	ULONG HCS_Timeout;	/* Adapter timeout value   */
	PVOID HCS_virScbArray;	/* 28 Virtual Pointer to SCB array     */
	U32 HCS_physScbArray;	/* Scb Physical address */
	PVOID HCS_virEscbArray;	/* Virtual pointer to ESCB Scatter list */
	U32 HCS_physEscbArray;	/* scatter list Physical address */
	UBYTE TargetFlag[16];	/* 30  target configuration, TCF_EN_TAG */
	UBYTE MaximumTags[16];	/* 40  ORC_MAX_SCBS */
	UBYTE ActiveTags[16][16];	/* 50 */
	ORC_TCS HCS_Tcs[16];	/* 28 */
	U32 BitAllocFlag[MAX_CHANNELS][8];	/* Max STB is 256, So 256/32 */
	spinlock_t BitAllocFlagLock;
	Scsi_Cmnd *pSRB_head;
	Scsi_Cmnd *pSRB_tail;
	spinlock_t pSRB_lock;
} ORC_HCS;

/* Bit Definition for HCS_Flags */

#define HCF_SCSI_RESET	0x01	/* SCSI BUS RESET         */
#define HCF_PARITY    	0x02	/* parity card                    */
#define HCF_LVDS     	0x10	/* parity card                    */

/* Bit Definition for TargetFlag */

#define TCF_EN_255	    0x08
#define TCF_EN_TAG	    0x10
#define TCF_BUSY	      0x20
#define TCF_DISCONNECT	0x40
#define TCF_SPIN_UP	  0x80

/* Bit Definition for HCS_AFlags */
#define	HCS_AF_IGNORE		0x01	/* Adapter ignore         */
#define	HCS_AF_DISABLE_RESET	0x10	/* Adapter disable reset  */
#define	HCS_AF_DISABLE_ADPT	0x80	/* Adapter disable                */


/*---------------------------------------*/
/* TimeOut for RESET to complete (30s)   */
/*                                       */
/* After a RESET the drive is checked    */
/* every 200ms.                          */
/*---------------------------------------*/
#define DELAYED_RESET_MAX       (30*1000L)
#define DELAYED_RESET_INTERVAL  200L

/*----------------------------------------------*/
/* TimeOut for IRQ from last interrupt (5s)     */
/*----------------------------------------------*/
#define IRQ_TIMEOUT_INTERVAL    (5*1000L)

/*----------------------------------------------*/
/* Retry Delay interval (200ms)                 */
/*----------------------------------------------*/
#define DELAYED_RETRY_INTERVAL  200L

#define	INQUIRY_SIZE		36
#define	CAPACITY_SIZE		8
#define	DEFAULT_SENSE_LEN	14

#define	DEVICE_NOT_FOUND	0x86

/*----------------------------------------------*/
/* Definition for PCI device                    */
/*----------------------------------------------*/
#define	MAX_PCI_DEVICES	21
#define	MAX_PCI_BUSES	8
