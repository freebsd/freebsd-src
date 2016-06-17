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
 * module: inia100.c
 * DESCRIPTION:
 * 	This is the Linux low-level SCSI driver for Initio INIA100 SCSI host
 * 	adapters
 * 09/24/98 hl - v1.02 initial production release.
 * 12/19/98 bv - v1.02a Use spinlocks for 2.1.95 and up.
 **************************************************************************/

#define CVT_LINUX_VERSION(V,P,S)        (V * 65536 + P * 256 + S)

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#include <linux/module.h>

#include <stdarg.h>
#include <asm/irq.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blk.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include "scsi.h"
#include "sd.h"
#include "hosts.h"
#include <linux/slab.h>
#include "inia100.h"

static Scsi_Host_Template driver_template = INIA100;
#include "scsi_module.c"

#define ORC_RDWORD(x,y)         (short)(inl((int)((ULONG)((ULONG)x+(UCHAR)y)) ))

char *inia100_Copyright = "Copyright (C) 1998-99";
char *inia100_InitioName = "by Initio Corporation";
char *inia100_ProductName = "INI-A100U2W";
char *inia100_Version = "v1.02c";

/* set by inia100_setup according to the command line */
static int setup_called = 0;
static int orc_num_ch = MAX_SUPPORTED_ADAPTERS;		/* Maximum 4 adapters           */

/* ---- INTERNAL VARIABLES ---- */
#define NUMBER(arr)     (sizeof(arr) / sizeof(arr[0]))
static char *setup_str = (char *) NULL;

static void inia100_intr0(int irq, void *dev_id, struct pt_regs *);
static void inia100_intr1(int irq, void *dev_id, struct pt_regs *);
static void inia100_intr2(int irq, void *dev_id, struct pt_regs *);
static void inia100_intr3(int irq, void *dev_id, struct pt_regs *);
static void inia100_intr4(int irq, void *dev_id, struct pt_regs *);
static void inia100_intr5(int irq, void *dev_id, struct pt_regs *);
static void inia100_intr6(int irq, void *dev_id, struct pt_regs *);
static void inia100_intr7(int irq, void *dev_id, struct pt_regs *);

static void inia100_panic(char *msg);
void inia100SCBPost(BYTE * pHcb, BYTE * pScb);

/* ---- EXTERNAL VARIABLES ---- */
extern int Addinia100_into_Adapter_table(WORD, WORD, BYTE, BYTE, BYTE);
extern void init_inia100Adapter_table(void);
extern ORC_SCB *orc_alloc_scb(ORC_HCS * hcsp);
extern void orc_exec_scb(ORC_HCS * hcsp, ORC_SCB * scbp);
extern void orc_release_scb(ORC_HCS * hcsp, ORC_SCB * scbp);
extern void orc_interrupt(ORC_HCS * hcsp);
extern int orc_device_reset(ORC_HCS * pHCB, ULONG SCpnt, unsigned int target, unsigned int ResetFlags);
extern int orc_reset_scsi_bus(ORC_HCS * pHCB);
extern int abort_SCB(ORC_HCS * hcsp, ORC_SCB * pScb);
extern int orc_abort_srb(ORC_HCS * hcsp, ULONG SCpnt);
extern void get_orcPCIConfig(ORC_HCS * pCurHcb, int ch_idx);
extern int init_orchid(ORC_HCS * hcsp);

extern int orc_num_scb;
extern ORC_HCS orc_hcs[];

/*****************************************************************************
 Function name  : inia100AppendSRBToQueue
 Description    : This function will push current request into save list
 Input          : pSRB  -       Pointer to SCSI request block.
		  pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : None.
*****************************************************************************/
static void inia100AppendSRBToQueue(ORC_HCS * pHCB, Scsi_Cmnd * pSRB)
{
	ULONG flags;

	spin_lock_irqsave(&(pHCB->pSRB_lock), flags);

	pSRB->next = NULL;	/* Pointer to next */
	if (pHCB->pSRB_head == NULL)
		pHCB->pSRB_head = pSRB;
	else
		pHCB->pSRB_tail->next = pSRB;	/* Pointer to next */
	pHCB->pSRB_tail = pSRB;
	spin_unlock_irqrestore(&(pHCB->pSRB_lock), flags);
	return;
}

/*****************************************************************************
 Function name  : inia100PopSRBFromQueue
 Description    : This function will pop current request from save list
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
static Scsi_Cmnd *inia100PopSRBFromQueue(ORC_HCS * pHCB)
{
	Scsi_Cmnd *pSRB;
	ULONG flags;
	spin_lock_irqsave(&(pHCB->pSRB_lock), flags);
	if ((pSRB = (Scsi_Cmnd *) pHCB->pSRB_head) != NULL) {
		pHCB->pSRB_head = pHCB->pSRB_head->next;
		pSRB->next = NULL;
	}
	spin_unlock_irqrestore(&(pHCB->pSRB_lock), flags);
	return (pSRB);
}

/*****************************************************************************
 Function name  : inia100_setup
 Description    : 
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
void inia100_setup(char *str, int *ints)
{
	if (setup_called)
		inia100_panic("inia100: inia100_setup called twice.\n");

	setup_called = ints[0];
	setup_str = str;
}

/*****************************************************************************
 Function name	: orc_ReturnNumberOfAdapters
 Description	: This function will scan PCI bus to get all Orchid card
 Input		: None.
 Output		: None.
 Return		: SUCCESSFUL	- Successful scan
		  ohterwise	- No drives founded
*****************************************************************************/
int orc_ReturnNumberOfAdapters(void)
{
	unsigned int i, iAdapters;

	iAdapters = 0;
	/*
	 * PCI-bus probe.
	 */
	if (pcibios_present()) {
		struct {
			unsigned short vendor_id;
			unsigned short device_id;
		} const inia100_pci_devices[] =
		{
			{ORC_VENDOR_ID, I920_DEVICE_ID},
			{ORC_VENDOR_ID, ORC_DEVICE_ID}
		};

		unsigned int dRegValue;
		WORD wBIOS, wBASE;
		BYTE bPCIBusNum, bInterrupt, bPCIDeviceNum;

#ifdef MMAPIO
		unsigned long page_offset, base;
#endif

		struct pci_dev *pdev = NULL;

		bPCIBusNum = 0;
		bPCIDeviceNum = 0;
		init_inia100Adapter_table();
		for (i = 0; i < NUMBER(inia100_pci_devices); i++) {
			pdev = NULL;
			while ((pdev = pci_find_device(inia100_pci_devices[i].vendor_id,
					inia100_pci_devices[i].device_id,
						       pdev)))
			{
				if (pci_enable_device(pdev))
					continue;
				if (iAdapters >= MAX_SUPPORTED_ADAPTERS)
					break;	/* Never greater than maximum   */

				if (i == 0) {
					/*
					   printk("inia100: The RAID controller is not supported by\n");
					   printk("inia100:         this driver, we are ignoring it.\n");
					 */
				} else {
					/*
					 * Read sundry information from PCI BIOS.
					 */
					bPCIBusNum = pdev->bus->number;
					bPCIDeviceNum = pdev->devfn;
					dRegValue = pci_resource_start(pdev, 0);
					if (dRegValue == -1) {	/* Check return code            */
						printk("\n\rinia100: orchid read configuration error.\n");
						return (0);	/* Read configuration space error  */
					}

					/* <02> read from base address + 0x50 offset to get the wBIOS balue. */
					wBASE = (WORD) dRegValue;

					/* Now read the interrupt line value */
					dRegValue = pdev->irq;
					bInterrupt = dRegValue;		/* Assign interrupt line      */

					wBIOS = ORC_RDWORD(wBASE, 0x50);

					pci_set_master(pdev);

#ifdef MMAPIO
					base = wBASE & PAGE_MASK;
					page_offset = wBASE - base;

					/*
					 * replace the next line with this one if you are using 2.1.x:
					 * temp_p->maddr = ioremap(base, page_offset + 256);
					 */
					wBASE = ioremap(base, page_offset + 256);
					if (wBASE) {
						wBASE += page_offset;
					}
#endif

					if (Addinia100_into_Adapter_table(wBIOS, wBASE, bInterrupt, bPCIBusNum,
					    bPCIDeviceNum) == SUCCESSFUL)
						iAdapters++;
				}
			}	/* while(pdev=....) */
		}		/* for PCI_DEVICES */
	}			/* PCI BIOS present */
	return (iAdapters);
}

/*****************************************************************************
 Function name  : inia100_detect
 Description    : 
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
int inia100_detect(Scsi_Host_Template * tpnt)
{
	ORC_HCS *pHCB;
	struct Scsi_Host *hreg;
	U32 sz;
	U32 i;			/* 01/14/98                     */
	int ok = 0, iAdapters;
	ULONG dBiosAdr;
	BYTE *pbBiosAdr;

	tpnt->proc_name = "INIA100";
	if (setup_called) {
		/* Setup by inia100_setup          */
		printk("inia100: processing commandline: ");
	}
	/* Get total number of adapters in the motherboard */
	iAdapters = orc_ReturnNumberOfAdapters();

	/* printk("inia100: Total Initio Adapters = %d\n", iAdapters); */
	if (iAdapters == 0)	/* If no orc founded, return */
		return (0);

	orc_num_ch = (iAdapters > orc_num_ch) ? orc_num_ch : iAdapters;
	orc_num_scb = ORC_MAXQUEUE;

	/* clear the memory needed for HCS */
	i = orc_num_ch * sizeof(ORC_HCS);
	memset((unsigned char *) &orc_hcs[0], 0, i);	/* Initialize orc_hcs 0   */

#if 0
	printk("orc_num_scb= %x orc_num_ch= %x hcsize= %x scbsize= %x escbsize= %x\n",
	       orc_num_scb, orc_num_ch, sizeof(ORC_HCS), sizeof(ORC_SCB), sizeof(ESCB));
#endif

	for (i = 0, pHCB = &orc_hcs[0];		/* Get pointer for control block */
	     i < orc_num_ch;
	     i++, pHCB++) {

		pHCB->pSRB_head = NULL;		/* Initial SRB save queue       */
		pHCB->pSRB_tail = NULL;		/* Initial SRB save queue       */
		pHCB->pSRB_lock = SPIN_LOCK_UNLOCKED; /* SRB save queue lock */
		pHCB->BitAllocFlagLock = SPIN_LOCK_UNLOCKED;
		/* Get total memory needed for SCB */
		sz = orc_num_scb * sizeof(ORC_SCB);
		if ((pHCB->HCS_virScbArray = (PVOID) kmalloc(sz, GFP_ATOMIC | GFP_DMA)) == NULL) {
			printk("inia100: SCB memory allocation error\n");
			return (0);
		}
		memset((unsigned char *) pHCB->HCS_virScbArray, 0, sz);
		pHCB->HCS_physScbArray = (U32) VIRT_TO_BUS(pHCB->HCS_virScbArray);

		/* Get total memory needed for ESCB */
		sz = orc_num_scb * sizeof(ESCB);
		if ((pHCB->HCS_virEscbArray = (PVOID) kmalloc(sz, GFP_ATOMIC | GFP_DMA)) == NULL) {
			printk("inia100: ESCB memory allocation error\n");
			/* ?? does pHCB->HCS_virtScbArray leak ??*/
			return (0);
		}
		memset((unsigned char *) pHCB->HCS_virEscbArray, 0, sz);
		pHCB->HCS_physEscbArray = (U32) VIRT_TO_BUS(pHCB->HCS_virEscbArray);

		get_orcPCIConfig(pHCB, i);

		dBiosAdr = pHCB->HCS_BIOS;
		dBiosAdr = (dBiosAdr << 4);

		pbBiosAdr = phys_to_virt(dBiosAdr);

		if (init_orchid(pHCB)) {	/* Initial orchid chip    */
			printk("inia100: initial orchid fail!!\n");
			return (0);
		}
		request_region(pHCB->HCS_Base, 256, "inia100");	/* Register */

		hreg = scsi_register(tpnt, sizeof(ORC_HCS));
		if (hreg == NULL) {
			release_region(pHCB->HCS_Base, 256);	/* Register */
			return 0;
		}
		hreg->io_port = pHCB->HCS_Base;
		hreg->n_io_port = 0xff;
		hreg->can_queue = orc_num_scb;	/* 03/05/98                   */

		hreg->unique_id = pHCB->HCS_Base;
		hreg->max_id = pHCB->HCS_MaxTar;

		hreg->max_lun = 32;	/* 10/21/97                     */
/*
   hreg->max_lun = 8;
   hreg->max_channel = 1;
 */
		hreg->irq = pHCB->HCS_Intr;
		hreg->this_id = pHCB->HCS_SCSI_ID;	/* Assign HCS index           */
		hreg->base = (unsigned long)pHCB;

#if 1
		hreg->sg_tablesize = TOTAL_SG_ENTRY;	/* Maximun support is 32 */
#else
		hreg->sg_tablesize = SG_NONE;	/* No SG                        */
#endif

		/* Initial orc chip           */
		switch (i) {
		case 0:
			ok = request_irq(pHCB->HCS_Intr, inia100_intr0, SA_INTERRUPT | SA_SHIRQ, "inia100", hreg);
			break;
		case 1:
			ok = request_irq(pHCB->HCS_Intr, inia100_intr1, SA_INTERRUPT | SA_SHIRQ, "inia100", hreg);
			break;
		case 2:
			ok = request_irq(pHCB->HCS_Intr, inia100_intr2, SA_INTERRUPT | SA_SHIRQ, "inia100", hreg);
			break;
		case 3:
			ok = request_irq(pHCB->HCS_Intr, inia100_intr3, SA_INTERRUPT | SA_SHIRQ, "inia100", hreg);
			break;
		case 4:
			ok = request_irq(pHCB->HCS_Intr, inia100_intr4, SA_INTERRUPT | SA_SHIRQ, "inia100", hreg);
			break;
		case 5:
			ok = request_irq(pHCB->HCS_Intr, inia100_intr5, SA_INTERRUPT | SA_SHIRQ, "inia100", hreg);
			break;
		case 6:
			ok = request_irq(pHCB->HCS_Intr, inia100_intr6, SA_INTERRUPT | SA_SHIRQ, "inia100", hreg);
			break;
		case 7:
			ok = request_irq(pHCB->HCS_Intr, inia100_intr7, SA_INTERRUPT | SA_SHIRQ, "inia100", hreg);
			break;
		default:
			inia100_panic("inia100: Too many host adapters\n");
			break;
		}

		if (ok < 0) {
			if (ok == -EINVAL) {
				printk("inia100: bad IRQ %d.\n", pHCB->HCS_Intr);
				printk("         Contact author.\n");
			} else {
				if (ok == -EBUSY)
					printk("inia100: IRQ %d already in use. Configure another.\n", pHCB->HCS_Intr);
				else {
					printk("\ninia100: Unexpected error code on requesting IRQ %d.\n",
					       pHCB->HCS_Intr);
					printk("         Contact author.\n");
				}
			}
			inia100_panic("inia100: driver needs an IRQ.\n");
		}
	}

	tpnt->this_id = -1;
	tpnt->can_queue = 1;
	return 1;
}

/*****************************************************************************
 Function name  : inia100BuildSCB
 Description    : 
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
static void inia100BuildSCB(ORC_HCS * pHCB, ORC_SCB * pSCB, Scsi_Cmnd * SCpnt)
{				/* Create corresponding SCB     */
	struct scatterlist *pSrbSG;
	ORC_SG *pSG;		/* Pointer to SG list           */
	int i;
	U32 TotalLen;
	ESCB *pEScb;

	pEScb = pSCB->SCB_EScb;
	pEScb->SCB_Srb = SCpnt;
	pSG = NULL;

	pSCB->SCB_Opcode = ORC_EXECSCSI;
	pSCB->SCB_Flags = SCF_NO_DCHK;	/* Clear done bit               */
	pSCB->SCB_Target = SCpnt->target;
	pSCB->SCB_Lun = SCpnt->lun;
	pSCB->SCB_Reserved0 = 0;
	pSCB->SCB_Reserved1 = 0;
	pSCB->SCB_SGLen = 0;

	if ((pSCB->SCB_XferLen = (U32) SCpnt->request_bufflen)) {
		pSG = (ORC_SG *) & pEScb->ESCB_SGList[0];
		if (SCpnt->use_sg) {
			TotalLen = 0;
			pSCB->SCB_SGLen = (U32) (SCpnt->use_sg * 8);
			pSrbSG = (struct scatterlist *) SCpnt->request_buffer;
			for (i = 0; i < SCpnt->use_sg; i++, pSG++, pSrbSG++) {
				pSG->SG_Ptr = (U32) (VIRT_TO_BUS(pSrbSG->address));
				pSG->SG_Len = (U32) pSrbSG->length;
				TotalLen += (U32) pSrbSG->length;
			}
		} else {	/* Non SG                       */
			pSCB->SCB_SGLen = 0x8;
			pSG->SG_Ptr = (U32) (VIRT_TO_BUS(SCpnt->request_buffer));
			pSG->SG_Len = (U32) SCpnt->request_bufflen;
		}
	}
	pSCB->SCB_SGPAddr = (U32) pSCB->SCB_SensePAddr;
	pSCB->SCB_HaStat = 0;
	pSCB->SCB_TaStat = 0;
	pSCB->SCB_Link = 0xFF;
	pSCB->SCB_SenseLen = SENSE_SIZE;
	pSCB->SCB_CDBLen = SCpnt->cmd_len;
	if (pSCB->SCB_CDBLen >= IMAX_CDB) {
		printk("max cdb length= %x\b", SCpnt->cmd_len);
		pSCB->SCB_CDBLen = IMAX_CDB;
	}
	pSCB->SCB_Ident = SCpnt->lun | DISC_ALLOW;
	if (SCpnt->device->tagged_supported) {	/* Tag Support                  */
		pSCB->SCB_TagMsg = SIMPLE_QUEUE_TAG;	/* Do simple tag only   */
	} else {
		pSCB->SCB_TagMsg = 0;	/* No tag support               */
	}
	memcpy(&pSCB->SCB_CDB[0], &SCpnt->cmnd, pSCB->SCB_CDBLen);
	return;
}

/*****************************************************************************
 Function name  : inia100_queue
 Description    : Queue a command and setup interrupts for a free bus.
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
int inia100_queue(Scsi_Cmnd * SCpnt, void (*done) (Scsi_Cmnd *))
{
	register ORC_SCB *pSCB;
	ORC_HCS *pHCB;		/* Point to Host adapter control block */

	if (SCpnt->lun > 16) {
		SCpnt->result = (DID_TIME_OUT << 16);
		done(SCpnt);	/* Notify system DONE           */
		return (0);
	}
	pHCB = (ORC_HCS *) SCpnt->host->base;
	SCpnt->scsi_done = done;
	/* Get free SCSI control block  */
	if ((pSCB = orc_alloc_scb(pHCB)) == NULL) {
		inia100AppendSRBToQueue(pHCB, SCpnt);	/* Buffer this request  */
		/* printk("inia100_entry: can't allocate SCB\n"); */
		return (0);
	}
	inia100BuildSCB(pHCB, pSCB, SCpnt);
	orc_exec_scb(pHCB, pSCB);	/* Start execute SCB            */

	return (0);
}

/*****************************************************************************
 Function name  : inia100_command
 Description    : We only support command in interrupt-driven fashion
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
int inia100_command(Scsi_Cmnd * SCpnt)
{
	printk("inia100: interrupt driven driver; use inia100_queue()\n");
	return -1;
}

/*****************************************************************************
 Function name  : inia100_abort
 Description    : Abort a queued command.
	                 (commands that are on the bus can't be aborted easily)
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
int inia100_abort(Scsi_Cmnd * SCpnt)
{
	ORC_HCS *hcsp;

	hcsp = (ORC_HCS *) SCpnt->host->base;
	return orc_abort_srb(hcsp, (ULONG) SCpnt);
}

/*****************************************************************************
 Function name  : inia100_reset
 Description    : Reset registers, reset a hanging bus and
                  kill active and disconnected commands for target w/o soft reset
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
int inia100_reset(Scsi_Cmnd * SCpnt, unsigned int reset_flags)
{				/* I need Host Control Block Information */
	ORC_HCS *pHCB;
	pHCB = (ORC_HCS *) SCpnt->host->base;

	if (reset_flags & (SCSI_RESET_SUGGEST_BUS_RESET | SCSI_RESET_SUGGEST_HOST_RESET))
		return orc_reset_scsi_bus(pHCB);
	else
		return orc_device_reset(pHCB, (ULONG) SCpnt, SCpnt->target, reset_flags);

}

/*****************************************************************************
 Function name  : inia100SCBPost
 Description    : This is callback routine be called when orc finish one
			SCSI command.
 Input          : pHCB  -       Pointer to host adapter control block.
		  pSCB  -       Pointer to SCSI control block.
 Output         : None.
 Return         : None.
*****************************************************************************/
void inia100SCBPost(BYTE * pHcb, BYTE * pScb)
{
	Scsi_Cmnd *pSRB;	/* Pointer to SCSI request block */
	ORC_HCS *pHCB;
	ORC_SCB *pSCB;
	ESCB *pEScb;

	pHCB = (ORC_HCS *) pHcb;
	pSCB = (ORC_SCB *) pScb;
	pEScb = pSCB->SCB_EScb;
	if ((pSRB = (Scsi_Cmnd *) pEScb->SCB_Srb) == 0) {
		printk("inia100SCBPost: SRB pointer is empty\n");
		orc_release_scb(pHCB, pSCB);	/* Release SCB for current channel */
		return;
	}
	pEScb->SCB_Srb = NULL;

	switch (pSCB->SCB_HaStat) {
	case 0x0:
	case 0xa:		/* Linked command complete without error and linked normally */
	case 0xb:		/* Linked command complete without error interrupt generated */
		pSCB->SCB_HaStat = 0;
		break;

	case 0x11:		/* Selection time out-The initiator selection or target
				   reselection was not complete within the SCSI Time out period */
		pSCB->SCB_HaStat = DID_TIME_OUT;
		break;

	case 0x14:		/* Target bus phase sequence failure-An invalid bus phase or bus
				   phase sequence was requested by the target. The host adapter
				   will generate a SCSI Reset Condition, notifying the host with
				   a SCRD interrupt */
		pSCB->SCB_HaStat = DID_RESET;
		break;

	case 0x1a:		/* SCB Aborted. 07/21/98 */
		pSCB->SCB_HaStat = DID_ABORT;
		break;

	case 0x12:		/* Data overrun/underrun-The target attempted to transfer more data
				   than was allocated by the Data Length field or the sum of the
				   Scatter / Gather Data Length fields. */
	case 0x13:		/* Unexpected bus free-The target dropped the SCSI BSY at an unexpected time. */
	case 0x16:		/* Invalid CCB Operation Code-The first byte of the CCB was invalid. */

	default:
		printk("inia100: %x %x\n", pSCB->SCB_HaStat, pSCB->SCB_TaStat);
		pSCB->SCB_HaStat = DID_ERROR;	/* Couldn't find any better */
		break;
	}

	if (pSCB->SCB_TaStat == 2) {	/* Check condition              */
		memcpy((unsigned char *) &pSRB->sense_buffer[0],
		   (unsigned char *) &pEScb->ESCB_SGList[0], SENSE_SIZE);
	}
	pSRB->result = pSCB->SCB_TaStat | (pSCB->SCB_HaStat << 16);
	pSRB->scsi_done(pSRB);	/* Notify system DONE           */

	/* Find the next pending SRB    */
	if ((pSRB = inia100PopSRBFromQueue(pHCB)) != NULL) {	/* Assume resend will success   */
		/* Reuse old SCB                */
		inia100BuildSCB(pHCB, pSCB, pSRB);	/* Create corresponding SCB     */
		orc_exec_scb(pHCB, pSCB);	/* Start execute SCB            */
	} else {		/* No Pending SRB               */
		orc_release_scb(pHCB, pSCB);	/* Release SCB for current channel */
	}
	return;
}

/*****************************************************************************
 Function name  : inia100_biosparam
 Description    : Return the "logical geometry"
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
int inia100_biosparam(Scsi_Disk * disk, kdev_t dev, int *info_array)
{
	ORC_HCS *pHcb;		/* Point to Host adapter control block */
	ORC_TCS *pTcb;

	pHcb = (ORC_HCS *) disk->device->host->base;
	pTcb = &pHcb->HCS_Tcs[disk->device->id];

	if (pTcb->TCS_DrvHead) {
		info_array[0] = pTcb->TCS_DrvHead;
		info_array[1] = pTcb->TCS_DrvSector;
		info_array[2] = disk->capacity / pTcb->TCS_DrvHead / pTcb->TCS_DrvSector;
	} else {
		if (pTcb->TCS_DrvFlags & TCF_DRV_255_63) {
			info_array[0] = 255;
			info_array[1] = 63;
			info_array[2] = disk->capacity / 255 / 63;
		} else {
			info_array[0] = 64;
			info_array[1] = 32;
			info_array[2] = disk->capacity >> 11;
		}
	}
	return 0;
}


static void subIntr(ORC_HCS * pHCB, int irqno)
{
	unsigned long flags;

	spin_lock_irqsave(&io_request_lock, flags);

	if (pHCB->HCS_Intr != irqno) {
		spin_unlock_irqrestore(&io_request_lock, flags);
		return;
	}
	orc_interrupt(pHCB);

	spin_unlock_irqrestore(&io_request_lock, flags);
}

/*
 * Interrupts handler (main routine of the driver)
 */
static void inia100_intr0(int irqno, void *dev_id, struct pt_regs *regs)
{
	subIntr(&orc_hcs[0], irqno);
}

static void inia100_intr1(int irqno, void *dev_id, struct pt_regs *regs)
{
	subIntr(&orc_hcs[1], irqno);
}

static void inia100_intr2(int irqno, void *dev_id, struct pt_regs *regs)
{
	subIntr(&orc_hcs[2], irqno);
}

static void inia100_intr3(int irqno, void *dev_id, struct pt_regs *regs)
{
	subIntr(&orc_hcs[3], irqno);
}

static void inia100_intr4(int irqno, void *dev_id, struct pt_regs *regs)
{
	subIntr(&orc_hcs[4], irqno);
}

static void inia100_intr5(int irqno, void *dev_id, struct pt_regs *regs)
{
	subIntr(&orc_hcs[5], irqno);
}

static void inia100_intr6(int irqno, void *dev_id, struct pt_regs *regs)
{
	subIntr(&orc_hcs[6], irqno);
}

static void inia100_intr7(int irqno, void *dev_id, struct pt_regs *regs)
{
	subIntr(&orc_hcs[7], irqno);
}

/* 
 * Dump the current driver status and panic...
 */
static void inia100_panic(char *msg)
{
	printk("\ninia100_panic: %s\n", msg);
	panic("inia100 panic");
}

/*
 * Release ressources
 */
int inia100_release(struct Scsi_Host *hreg)
{
        free_irq(hreg->irq, hreg);
        release_region(hreg->io_port, 256);
        return 0;
} 

MODULE_LICENSE("Dual BSD/GPL");
/*#include "inia100scsi.c" */
