/**************************************************************************
 * Initio 9100 device driver for Linux.
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
 *************************************************************************
 *
 * Module: ini9100u.h
 * Description: INI-9100U/UW LINUX device driver header
 * Revision History:
 * 06/18/96 Harry Chen, Initial Version 1.00A (Beta)
 * 06/23/98 hc	- v1.01k
 *		- Get it work for kernel version >= 2.1.75
 * 12/09/98 bv	- v1.03a
 *		- Removed unused code
 * 12/13/98 bv	- v1.03b
 *		- Add spinlocks to HCS structure.
 * 21/01/99 bv	- v1.03e
 *		- Added PCI_ID structure
 **************************************************************************/

#ifndef	CVT_LINUX_VERSION
#define	CVT_LINUX_VERSION(V,P,S)	(((V) * 65536) + ((P) * 256) + (S))
#endif

#ifndef	LINUX_VERSION_CODE
#include <linux/version.h>
#endif
#include <linux/types.h>

#include "sd.h"

extern int i91u_detect(Scsi_Host_Template *);
extern int i91u_release(struct Scsi_Host *);
extern int i91u_command(Scsi_Cmnd *);
extern int i91u_queue(Scsi_Cmnd *, void (*done) (Scsi_Cmnd *));
extern int i91u_abort(Scsi_Cmnd *);
extern int i91u_reset(Scsi_Cmnd *, unsigned int);
extern int i91u_biosparam(Scsi_Disk *, kdev_t, int *);	/*for linux v2.0 */

#define i91u_REVID "Initio INI-9X00U/UW SCSI device driver; Revision: 1.03g"

#define INI9100U	{ \
	next:		NULL,						\
	module:		NULL,						\
	proc_name:	"INI9100U", \
	proc_info:	NULL,				\
	name:		i91u_REVID, \
	detect:		i91u_detect, \
	release:	i91u_release, \
	info:		NULL,					\
	command:	i91u_command, \
	queuecommand:	i91u_queue, \
 	eh_strategy_handler: NULL, \
 	eh_abort_handler: NULL, \
 	eh_device_reset_handler: NULL, \
 	eh_bus_reset_handler: NULL, \
 	eh_host_reset_handler: NULL, \
	abort:		i91u_abort, \
	reset:		i91u_reset, \
	slave_attach:	NULL, \
	bios_param:	i91u_biosparam, \
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
#define USHORT  unsigned short
#define UCHAR   unsigned char
#define BYTE    u8
#define WORD    unsigned short
#define DWORD   unsigned long
#define UBYTE   u8
#define UWORD   unsigned short
#define UDWORD  unsigned long
#define U32   u32

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

#define i91u_MAXQUEUE		2
#define TOTAL_SG_ENTRY		32
#define MAX_TARGETS		16
#define SENSE_SIZE		14

#define INI_VENDOR_ID   0x1101	/* Initio's PCI vendor ID       */
#define DMX_VENDOR_ID	0x134a	/* Domex's PCI vendor ID	*/
#define I950_DEVICE_ID	0x9500	/* Initio's inic-950 product ID   */
#define I940_DEVICE_ID	0x9400	/* Initio's inic-940 product ID   */
#define I935_DEVICE_ID	0x9401	/* Initio's inic-935 product ID   */
#define I920_DEVICE_ID	0x0002	/* Initio's other product ID      */

/************************************************************************/
/*              Vendor ID/Device ID Pair Structure			*/
/************************************************************************/
typedef struct PCI_ID_Struc {
	unsigned short vendor_id;
	unsigned short device_id;
} PCI_ID;

/************************************************************************/
/*              Scatter-Gather Element Structure                        */
/************************************************************************/
typedef struct SG_Struc {
	U32 SG_Ptr;		/* Data Pointer */
	U32 SG_Len;		/* Data Length */
} SG;

/***********************************************************************
		SCSI Control Block
************************************************************************/
typedef struct Scsi_Ctrl_Blk {
	U32 SCB_InitioReserved[9];	/* 0 */

	UBYTE SCB_Opcode;	/*24 SCB command code */
	UBYTE SCB_Flags;	/*25 SCB Flags */
	UBYTE SCB_Target;	/*26 Target Id */
	UBYTE SCB_Lun;		/*27 Lun */
	U32 SCB_BufPtr;		/*28 Data Buffer Pointer */
	U32 SCB_BufLen;		/*2C Data Allocation Length */
	UBYTE SCB_SGLen;	/*30 SG list # */
	UBYTE SCB_SenseLen;	/*31 Sense Allocation Length */
	UBYTE SCB_HaStat;	/*32 */
	UBYTE SCB_TaStat;	/*33 */
	UBYTE SCB_CDBLen;	/*34 CDB Length */
	UBYTE SCB_Ident;	/*35 Identify */
	UBYTE SCB_TagMsg;	/*36 Tag Message */
	UBYTE SCB_TagId;	/*37 Queue Tag */
	UBYTE SCB_CDB[12];	/*38 */
	U32 SCB_SGPAddr;	/*44 SG List/Sense Buf phy. Addr. */
	U32 SCB_SensePtr;	/*48 Sense data pointer */
	void (*SCB_Post) (BYTE *, BYTE *);	/*4C POST routine */
	Scsi_Cmnd *SCB_Srb;	/*50 SRB Pointer */
	SG SCB_SGList[TOTAL_SG_ENTRY];	/*54 Start of SG list */
} SCB;

/* Opcodes of SCB_Opcode */
#define ExecSCSI        0x1
#define BusDevRst       0x2
#define AbortCmd        0x3

/* Bit Definition for SCB_Flags */
#define SCF_DONE        0x01
#define SCF_POST        0x02
#define SCF_SENSE       0x04
#define SCF_DIR         0x18
#define SCF_NO_DCHK     0x00
#define SCF_DIN         0x08
#define SCF_DOUT        0x10
#define SCF_NO_XF       0x18
#define SCF_POLL        0x40
#define SCF_SG          0x80

/* Error Codes for SCB_HaStat */
#define HOST_SEL_TOUT   0x11
#define HOST_DO_DU      0x12
#define HOST_BUS_FREE   0x13
#define HOST_BAD_PHAS   0x14
#define HOST_INV_CMD    0x16
#define HOST_SCSI_RST   0x1B
#define HOST_DEV_RST    0x1C

/* Error Codes for SCB_TaStat */
#define TARGET_CHKCOND  0x02
#define TARGET_BUSY     0x08

/* Queue tag msg: Simple_quque_tag, Head_of_queue_tag, Ordered_queue_tag */
#define MSG_STAG        0x20
#define MSG_HTAG        0x21
#define MSG_OTAG        0x22

/***********************************************************************
		Target Device Control Structure
**********************************************************************/

typedef struct Tar_Ctrl_Struc {
	ULONG TCS_InitioReserved;	/* 0 */

	UWORD TCS_DrvFlags;	/* 4 */
	UBYTE TCS_DrvHead;	/* 6 */
	UBYTE TCS_DrvSector;	/* 7 */
} TCS;

/***********************************************************************
		Target Device Control Structure
**********************************************************************/
/* Bit Definition for TCF_DrvFlags */
#define TCF_DRV_255_63          0x0400

/***********************************************************************
	      Host Adapter Control Structure
************************************************************************/
typedef struct Ha_Ctrl_Struc {
	UWORD HCS_Base;		/* 00 */
	UWORD HCS_BIOS;		/* 02 */
	UBYTE HCS_Intr;		/* 04 */
	UBYTE HCS_SCSI_ID;	/* 05 */
	UBYTE HCS_MaxTar;	/* 06 */
	UBYTE HCS_NumScbs;	/* 07 */

	UBYTE HCS_Flags;	/* 08 */
	UBYTE HCS_Index;	/* 09 */
	UBYTE HCS_Reserved[2];	/* 0a */
	ULONG HCS_InitioReserved[27];	/* 0C */
	TCS HCS_Tcs[16];	/* 78 -> 16 Targets */
	Scsi_Cmnd *pSRB_head;	/* SRB save queue header     */
	Scsi_Cmnd *pSRB_tail;	/* SRB save queue tail       */
	spinlock_t HCS_AvailLock;
	spinlock_t HCS_SemaphLock;
	spinlock_t pSRB_lock;
	struct pci_dev *pci_dev;
} HCS;

/* Bit Definition for HCB_Flags */
#define HCF_EXPECT_RESET        0x10

/* SCSI related definition                                              */
#define DISC_NOT_ALLOW          0x80	/* Disconnect is not allowed    */
#define DISC_ALLOW              0xC0	/* Disconnect is allowed        */
