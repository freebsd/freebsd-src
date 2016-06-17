/**************************************************************************
 * Initio 9100 device driver for Linux.
 *
 * Copyright (c) 1994-1998 Initio Corporation
 * Copyright (c) 1998 Bas Vermeulen <bvermeul@blackstar.xs4all.nl>
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
 * DESCRIPTION:
 *
 * This is the Linux low-level SCSI driver for Initio INI-9X00U/UW SCSI host
 * adapters
 *
 * 08/06/97 hc	- v1.01h
 *		- Support inic-940 and inic-935
 * 09/26/97 hc	- v1.01i
 *		- Make correction from J.W. Schultz suggestion
 * 10/13/97 hc	- Support reset function
 * 10/21/97 hc	- v1.01j
 *		- Support 32 LUN (SCSI 3)
 * 01/14/98 hc	- v1.01k
 *		- Fix memory allocation problem
 * 03/04/98 hc	- v1.01l
 *		- Fix tape rewind which will hang the system problem
 *		- Set can_queue to tul_num_scb
 * 06/25/98 hc	- v1.01m
 *		- Get it work for kernel version >= 2.1.75
 *		- Dynamic assign SCSI bus reset holding time in init_tulip()
 * 07/02/98 hc	- v1.01n
 *		- Support 0002134A
 * 08/07/98 hc  - v1.01o
 *		- Change the tul_abort_srb routine to use scsi_done. <01>
 * 09/07/98 hl  - v1.02
 *              - Change the INI9100U define and proc_dir_entry to
 *                reflect the newer Kernel 2.1.118, but the v1.o1o
 *                should work with Kernel 2.1.118.
 * 09/20/98 wh  - v1.02a
 *              - Support Abort command.
 *              - Handle reset routine.
 * 09/21/98 hl  - v1.03
 *              - remove comments.
 * 12/09/98 bv	- v1.03a
 *		- Removed unused code
 * 12/13/98 bv	- v1.03b
 *		- Remove cli() locking for kernels >= 2.1.95. This uses
 *		  spinlocks to serialize access to the pSRB_head and
 *		  pSRB_tail members of the HCS structure.
 * 09/01/99 bv	- v1.03d
 *		- Fixed a deadlock problem in SMP.
 * 21/01/99 bv	- v1.03e
 *		- Add support for the Domex 3192U PCI SCSI
 *		  This is a slightly modified patch by
 *		  Brian Macy <bmacy@sunshinecomputing.com>
 * 22/02/99 bv	- v1.03f
 *		- Didn't detect the INIC-950 in 2.0.x correctly.
 *		  Now fixed.
 * 05/07/99 bv	- v1.03g
 *		- Changed the assumption that HZ = 100
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
#include <linux/config.h>

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
#include "ini9100u.h"

#ifdef DEBUG_i91u
unsigned int i91u_debug = DEBUG_DEFAULT;
#endif

static Scsi_Host_Template driver_template = INI9100U;
#include "scsi_module.c"

char *i91uCopyright = "Copyright (C) 1996-98";
char *i91uInitioName = "by Initio Corporation";
char *i91uProductName = "INI-9X00U/UW";
char *i91uVersion = "v1.03g";

#define TULSZ(sz)     (sizeof(sz) / sizeof(sz[0]))
#define TUL_RDWORD(x,y)         (short)(inl((int)((ULONG)((ULONG)x+(UCHAR)y)) ))

/* set by i91_setup according to the command line */
static int setup_called = 0;

static int tul_num_ch = 4;	/* Maximum 4 adapters           */
static int tul_num_scb;
static int tul_tag_enable = 1;
static SCB *tul_scb;

#ifdef DEBUG_i91u
static int setup_debug = 0;
#endif

static char *setup_str = (char *) NULL;

static void i91u_intr0(int irq, void *dev_id, struct pt_regs *);
static void i91u_intr1(int irq, void *dev_id, struct pt_regs *);
static void i91u_intr2(int irq, void *dev_id, struct pt_regs *);
static void i91u_intr3(int irq, void *dev_id, struct pt_regs *);
static void i91u_intr4(int irq, void *dev_id, struct pt_regs *);
static void i91u_intr5(int irq, void *dev_id, struct pt_regs *);
static void i91u_intr6(int irq, void *dev_id, struct pt_regs *);
static void i91u_intr7(int irq, void *dev_id, struct pt_regs *);

static void i91u_panic(char *msg);

static void i91uSCBPost(BYTE * pHcb, BYTE * pScb);

				/* ---- EXTERNAL FUNCTIONS ---- */
					/* Get total number of adapters */
extern void init_i91uAdapter_table(void);
extern int Addi91u_into_Adapter_table(WORD, WORD, BYTE, BYTE, BYTE);
extern int tul_ReturnNumberOfAdapters(void);
extern void get_tulipPCIConfig(HCS * pHCB, int iChannel_index);
extern int init_tulip(HCS * pHCB, SCB * pSCB, int tul_num_scb, BYTE * pbBiosAdr, int reset_time);
extern SCB *tul_alloc_scb(HCS * pHCB);
extern int tul_abort_srb(HCS * pHCB, Scsi_Cmnd * pSRB);
extern void tul_exec_scb(HCS * pHCB, SCB * pSCB);
extern void tul_release_scb(HCS * pHCB, SCB * pSCB);
extern void tul_stop_bm(HCS * pHCB);
extern int tul_reset_scsi(HCS * pCurHcb, int seconds);
extern int tul_isr(HCS * pHCB);
extern int tul_reset(HCS * pHCB, Scsi_Cmnd * pSRB, unsigned char target);
extern int tul_reset_scsi_bus(HCS * pCurHcb);
extern int tul_device_reset(HCS * pCurHcb, ULONG pSrb, unsigned int target, unsigned int ResetFlags);
				/* ---- EXTERNAL VARIABLES ---- */
extern HCS tul_hcs[];

const PCI_ID i91u_pci_devices[] = {
	{ INI_VENDOR_ID, I950_DEVICE_ID },
	{ INI_VENDOR_ID, I940_DEVICE_ID },
	{ INI_VENDOR_ID, I935_DEVICE_ID },
	{ INI_VENDOR_ID, I920_DEVICE_ID },
	{ DMX_VENDOR_ID, I920_DEVICE_ID },
};

/*
 *  queue services:
 */
/*****************************************************************************
 Function name  : i91uAppendSRBToQueue
 Description    : This function will push current request into save list
 Input          : pSRB  -       Pointer to SCSI request block.
		  pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : None.
*****************************************************************************/
static void i91uAppendSRBToQueue(HCS * pHCB, Scsi_Cmnd * pSRB)
{
	ULONG flags;
	spin_lock_irqsave(&(pHCB->pSRB_lock), flags);

	pSRB->next = NULL;	/* Pointer to next              */

	if (pHCB->pSRB_head == NULL)
		pHCB->pSRB_head = pSRB;
	else
		pHCB->pSRB_tail->next = pSRB;	/* Pointer to next              */
	pHCB->pSRB_tail = pSRB;

	spin_unlock_irqrestore(&(pHCB->pSRB_lock), flags);
	return;
}

/*****************************************************************************
 Function name  : i91uPopSRBFromQueue
 Description    : This function will pop current request from save list
 Input          : pHCB  -       Pointer to host adapter structure
 Output         : None.
 Return         : pSRB  -       Pointer to SCSI request block.
*****************************************************************************/
static Scsi_Cmnd *i91uPopSRBFromQueue(HCS * pHCB)
{
	Scsi_Cmnd *pSRB;
	ULONG flags;

	spin_lock_irqsave(&(pHCB->pSRB_lock), flags);

	if ((pSRB = pHCB->pSRB_head) != NULL) {
		pHCB->pSRB_head = pHCB->pSRB_head->next;
		pSRB->next = NULL;
	}
	spin_unlock_irqrestore(&(pHCB->pSRB_lock), flags);

	return (pSRB);
}

/* called from init/main.c */

void i91u_setup(char *str, int *ints)
{
	if (setup_called)
		i91u_panic("i91u: i91u_setup called twice.\n");

	setup_called = ints[0];
	setup_str = str;

#ifdef DEBUG_i91u
	setup_debug = ints[0] >= 1 ? ints[1] : DEBUG_DEFAULT;
#endif
}

int tul_NewReturnNumberOfAdapters(void)
{
	struct pci_dev *pDev = NULL;	/* Start from none              */
	int iAdapters = 0;
	long dRegValue;
	WORD wBIOS;
	int i = 0;

	init_i91uAdapter_table();

	for (i = 0; i < TULSZ(i91u_pci_devices); i++)
	{
		while ((pDev = pci_find_device(i91u_pci_devices[i].vendor_id, i91u_pci_devices[i].device_id, pDev)) != NULL) {
			if (pci_enable_device(pDev))
				continue;
			pci_read_config_dword(pDev, 0x44, (u32 *) & dRegValue);
			wBIOS = (UWORD) (dRegValue & 0xFF);
			if (((dRegValue & 0xFF00) >> 8) == 0xFF)
				dRegValue = 0;
			wBIOS = (wBIOS << 8) + ((UWORD) ((dRegValue & 0xFF00) >> 8));
			if (Addi91u_into_Adapter_table(wBIOS,
							(pDev->resource[0].start),
						       	pDev->irq,
						       	pDev->bus->number,
					       		(pDev->devfn >> 3)
		    		) == 0)
				iAdapters++;
		}
	}

	return (iAdapters);
}

int i91u_detect(Scsi_Host_Template * tpnt)
{
	SCB *pSCB;
	HCS *pHCB;
	struct Scsi_Host *hreg;
	unsigned long i;	/* 01/14/98                     */
	int ok = 0, iAdapters;
	ULONG dBiosAdr;
	BYTE *pbBiosAdr;

	tpnt->proc_name = "INI9100U";

	if (setup_called) {	/* Setup by i91u_setup          */
		printk("i91u: processing commandline: ");

#ifdef DEBUG_i91u
		if (setup_called > 1) {
			printk("\ni91u: %s\n", setup_str);
			printk("i91u: usage: i91u[=<DEBUG>]\n");
			i91u_panic("i91u panics in line %d", __LINE__);
		}
		i91u_debug = setup_debug;
#endif
	}
	/* Get total number of adapters in the motherboard */
#ifdef CONFIG_PCI
	iAdapters = tul_NewReturnNumberOfAdapters();
#else
	iAdapters = tul_ReturnNumberOfAdapters();
#endif

	if (iAdapters == 0)	/* If no tulip founded, return */
		return (0);

	tul_num_ch = (iAdapters > tul_num_ch) ? tul_num_ch : iAdapters;
	/* Update actually channel number */
	if (tul_tag_enable) {	/* 1.01i                  */
		tul_num_scb = MAX_TARGETS * i91u_MAXQUEUE;
	} else {
		tul_num_scb = MAX_TARGETS + 3;	/* 1-tape, 1-CD_ROM, 1- extra */
	}			/* Update actually SCBs per adapter */

	/* Get total memory needed for HCS */
	i = tul_num_ch * sizeof(HCS);
	memset((unsigned char *) &tul_hcs[0], 0, i);	/* Initialize tul_hcs 0 */
	/* Get total memory needed for SCB */

	for (; tul_num_scb >= MAX_TARGETS + 3; tul_num_scb--) {
		i = tul_num_ch * tul_num_scb * sizeof(SCB);
		if ((tul_scb = (SCB *) kmalloc(i, GFP_ATOMIC | GFP_DMA)) != NULL)
			break;
	}
	if (tul_scb == NULL) {
		printk("i91u: SCB memory allocation error\n");
		return (0);
	}
	memset((unsigned char *) tul_scb, 0, i);

	pSCB = tul_scb;
	for (i = 0; i < tul_num_ch * tul_num_scb; i++, pSCB++) {
		pSCB->SCB_SGPAddr = (U32) VIRT_TO_BUS(&pSCB->SCB_SGList[0]);
	}

	for (i = 0, pHCB = &tul_hcs[0];		/* Get pointer for control block */
	     i < tul_num_ch;
	     i++, pHCB++) {
		pHCB->pSRB_head = NULL;		/* Initial SRB save queue       */
		pHCB->pSRB_tail = NULL;		/* Initial SRB save queue       */
		pHCB->pSRB_lock = SPIN_LOCK_UNLOCKED;	/* SRB save queue lock */
		get_tulipPCIConfig(pHCB, i);

		dBiosAdr = pHCB->HCS_BIOS;
		dBiosAdr = (dBiosAdr << 4);

		pbBiosAdr = phys_to_virt(dBiosAdr);

		init_tulip(pHCB, tul_scb + (i * tul_num_scb), tul_num_scb, pbBiosAdr, 10);
		request_region(pHCB->HCS_Base, 256, "i91u"); /* Register */ 

		pHCB->HCS_Index = i;	/* 7/29/98 */
		hreg = scsi_register(tpnt, sizeof(HCS));
		if(hreg == NULL)
		{
			release_region(pHCB->HCS_Base, 256);
			return 0;
		}
		hreg->io_port = pHCB->HCS_Base;
		hreg->n_io_port = 0xff;
		hreg->can_queue = tul_num_scb;	/* 03/05/98                      */
		hreg->unique_id = pHCB->HCS_Base;
		hreg->max_id = pHCB->HCS_MaxTar;
		hreg->max_lun = 32;	/* 10/21/97                     */
		hreg->irq = pHCB->HCS_Intr;
		hreg->this_id = pHCB->HCS_SCSI_ID;	/* Assign HCS index           */
		hreg->base = (unsigned long)pHCB;
		hreg->sg_tablesize = TOTAL_SG_ENTRY;	/* Maximun support is 32 */

		/* Initial tulip chip           */
		switch (i) {
		case 0:
			ok = request_irq(pHCB->HCS_Intr, i91u_intr0, SA_INTERRUPT | SA_SHIRQ, "i91u", hreg);
			break;
		case 1:
			ok = request_irq(pHCB->HCS_Intr, i91u_intr1, SA_INTERRUPT | SA_SHIRQ, "i91u", hreg);
			break;
		case 2:
			ok = request_irq(pHCB->HCS_Intr, i91u_intr2, SA_INTERRUPT | SA_SHIRQ, "i91u", hreg);
			break;
		case 3:
			ok = request_irq(pHCB->HCS_Intr, i91u_intr3, SA_INTERRUPT | SA_SHIRQ, "i91u", hreg);
			break;
		case 4:
			ok = request_irq(pHCB->HCS_Intr, i91u_intr4, SA_INTERRUPT | SA_SHIRQ, "i91u", hreg);
			break;
		case 5:
			ok = request_irq(pHCB->HCS_Intr, i91u_intr5, SA_INTERRUPT | SA_SHIRQ, "i91u", hreg);
			break;
		case 6:
			ok = request_irq(pHCB->HCS_Intr, i91u_intr6, SA_INTERRUPT | SA_SHIRQ, "i91u", hreg);
			break;
		case 7:
			ok = request_irq(pHCB->HCS_Intr, i91u_intr7, SA_INTERRUPT | SA_SHIRQ, "i91u", hreg);
			break;
		default:
			i91u_panic("i91u: Too many host adapters\n");
			break;
		}
		if (ok < 0) {
			if (ok == -EINVAL) {
				printk("i91u: bad IRQ %d.\n", pHCB->HCS_Intr);
				printk("         Contact author.\n");
			} else if (ok == -EBUSY)
				printk("i91u: IRQ %d already in use. Configure another.\n",
				       pHCB->HCS_Intr);
			else {
				printk("\ni91u: Unexpected error code on requesting IRQ %d.\n",
				       pHCB->HCS_Intr);
				printk("         Contact author.\n");
			}
			i91u_panic("i91u: driver needs an IRQ.\n");
		}
	}

	tpnt->this_id = -1;
	tpnt->can_queue = 1;

	return 1;
}

static void i91uBuildSCB(HCS * pHCB, SCB * pSCB, Scsi_Cmnd * SCpnt)
{				/* Create corresponding SCB     */
	struct scatterlist *pSrbSG;
	SG *pSG;		/* Pointer to SG list           */
	int i;
	long TotalLen;

	pSCB->SCB_Post = i91uSCBPost;	/* i91u's callback routine      */
	pSCB->SCB_Srb = SCpnt;
	pSCB->SCB_Opcode = ExecSCSI;
	pSCB->SCB_Flags = SCF_POST;	/* After SCSI done, call post routine */
	pSCB->SCB_Target = SCpnt->target;
	pSCB->SCB_Lun = SCpnt->lun;
	pSCB->SCB_Ident = SCpnt->lun | DISC_ALLOW;
	pSCB->SCB_Flags |= SCF_SENSE;	/* Turn on auto request sense   */

	pSCB->SCB_SensePtr = (U32) VIRT_TO_BUS(SCpnt->sense_buffer);

	pSCB->SCB_SenseLen = SENSE_SIZE;

	pSCB->SCB_CDBLen = SCpnt->cmd_len;
	pSCB->SCB_HaStat = 0;
	pSCB->SCB_TaStat = 0;
	memcpy(&pSCB->SCB_CDB[0], &SCpnt->cmnd, SCpnt->cmd_len);

	if (SCpnt->device->tagged_supported) {	/* Tag Support                  */
		pSCB->SCB_TagMsg = SIMPLE_QUEUE_TAG;	/* Do simple tag only   */
	} else {
		pSCB->SCB_TagMsg = 0;	/* No tag support               */
	}

	if (SCpnt->use_sg) {
		pSrbSG = (struct scatterlist *) SCpnt->request_buffer;
		if (SCpnt->use_sg == 1) {	/* If only one entry in the list *//*      treat it as regular I/O */
			pSCB->SCB_BufPtr = (U32) VIRT_TO_BUS(pSrbSG->address);
			TotalLen = pSrbSG->length;
			pSCB->SCB_SGLen = 0;
		} else {	/* Assign SG physical address   */
			pSCB->SCB_BufPtr = pSCB->SCB_SGPAddr;
			pSCB->SCB_Flags |= SCF_SG;	/* Turn on SG list flag       */
			for (i = 0, TotalLen = 0, pSG = &pSCB->SCB_SGList[0];	/* 1.01g */
			     i < SCpnt->use_sg;
			     i++, pSG++, pSrbSG++) {
				pSG->SG_Ptr = (U32) VIRT_TO_BUS(pSrbSG->address);
				TotalLen += pSG->SG_Len = pSrbSG->length;
			}
			pSCB->SCB_SGLen = i;
		}
		pSCB->SCB_BufLen = (SCpnt->request_bufflen > TotalLen) ?
		    TotalLen : SCpnt->request_bufflen;
	} else {		/* Non SG                       */
		pSCB->SCB_BufPtr = (U32) VIRT_TO_BUS(SCpnt->request_buffer);
		pSCB->SCB_BufLen = SCpnt->request_bufflen;
		pSCB->SCB_SGLen = 0;
	}

	return;
}

/* 
 *  Queue a command and setup interrupts for a free bus.
 */
int i91u_queue(Scsi_Cmnd * SCpnt, void (*done) (Scsi_Cmnd *))
{
	register SCB *pSCB;
	HCS *pHCB;		/* Point to Host adapter control block */

	if (SCpnt->lun > 16) {	/* 07/22/98 */

		SCpnt->result = (DID_TIME_OUT << 16);
		done(SCpnt);	/* Notify system DONE           */
		return (0);
	}
	pHCB = (HCS *) SCpnt->host->base;

	SCpnt->scsi_done = done;
	/* Get free SCSI control block  */
	if ((pSCB = tul_alloc_scb(pHCB)) == NULL) {
		i91uAppendSRBToQueue(pHCB, SCpnt);	/* Buffer this request  */
		return (0);
	}
	i91uBuildSCB(pHCB, pSCB, SCpnt);
	tul_exec_scb(pHCB, pSCB);	/* Start execute SCB            */
	return (0);
}

/*
 *  We only support command in interrupt-driven fashion
 */
int i91u_command(Scsi_Cmnd * SCpnt)
{
	printk("i91u: interrupt driven driver; use i91u_queue()\n");
	return -1;
}

/*
 *  Abort a queued command
 *  (commands that are on the bus can't be aborted easily)
 */
int i91u_abort(Scsi_Cmnd * SCpnt)
{
	HCS *pHCB;

	pHCB = (HCS *) SCpnt->host->base;
	return tul_abort_srb(pHCB, SCpnt);
}

/*
 *  Reset registers, reset a hanging bus and
 *  kill active and disconnected commands for target w/o soft reset
 */
int i91u_reset(Scsi_Cmnd * SCpnt, unsigned int reset_flags)
{				/* I need Host Control Block Information */
	HCS *pHCB;

	pHCB = (HCS *) SCpnt->host->base;

	if (reset_flags & (SCSI_RESET_SUGGEST_BUS_RESET | SCSI_RESET_SUGGEST_HOST_RESET))
		return tul_reset_scsi_bus(pHCB);
	else
		return tul_device_reset(pHCB, (ULONG) SCpnt, SCpnt->target, reset_flags);
}

/*
 * Return the "logical geometry"
 */
int i91u_biosparam(Scsi_Disk * disk, kdev_t dev, int *info_array)
{
	HCS *pHcb;		/* Point to Host adapter control block */
	TCS *pTcb;

	pHcb = (HCS *) disk->device->host->base;
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

#if defined(DEBUG_BIOSPARAM)
	if (i91u_debug & debug_biosparam) {
		printk("bios geometry: head=%d, sec=%d, cyl=%d\n",
		       info_array[0], info_array[1], info_array[2]);
		printk("WARNING: check, if the bios geometry is correct.\n");
	}
#endif

	return 0;
}

/*****************************************************************************
 Function name  : i91uSCBPost
 Description    : This is callback routine be called when tulip finish one
			SCSI command.
 Input          : pHCB  -       Pointer to host adapter control block.
		  pSCB  -       Pointer to SCSI control block.
 Output         : None.
 Return         : None.
*****************************************************************************/
static void i91uSCBPost(BYTE * pHcb, BYTE * pScb)
{
	Scsi_Cmnd *pSRB;	/* Pointer to SCSI request block */
	HCS *pHCB;
	SCB *pSCB;

	pHCB = (HCS *) pHcb;
	pSCB = (SCB *) pScb;
	if ((pSRB = pSCB->SCB_Srb) == 0) {
		printk("i91uSCBPost: SRB pointer is empty\n");

		tul_release_scb(pHCB, pSCB);	/* Release SCB for current channel */
		return;
	}
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
	case 0x16:		/* Invalid SCB Operation Code. */

	default:
		printk("ini9100u: %x %x\n", pSCB->SCB_HaStat, pSCB->SCB_TaStat);
		pSCB->SCB_HaStat = DID_ERROR;	/* Couldn't find any better */
		break;
	}

	pSRB->result = pSCB->SCB_TaStat | (pSCB->SCB_HaStat << 16);

	if (pSRB == NULL) {
		printk("pSRB is NULL\n");
	}
	pSRB->scsi_done(pSRB);	/* Notify system DONE           */
	if ((pSRB = i91uPopSRBFromQueue(pHCB)) != NULL)
		/* Find the next pending SRB    */
	{			/* Assume resend will success   */
		/* Reuse old SCB                */
		i91uBuildSCB(pHCB, pSCB, pSRB);		/* Create corresponding SCB     */

		tul_exec_scb(pHCB, pSCB);	/* Start execute SCB            */
	} else {		/* No Pending SRB               */
		tul_release_scb(pHCB, pSCB);	/* Release SCB for current channel */
	}
	return;
}

/*
 * Interrupts handler (main routine of the driver)
 */
static void i91u_intr0(int irqno, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	if (tul_hcs[0].HCS_Intr != irqno)
		return;

	spin_lock_irqsave(&io_request_lock, flags);

	tul_isr(&tul_hcs[0]);

	spin_unlock_irqrestore(&io_request_lock, flags);
}

static void i91u_intr1(int irqno, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	if (tul_hcs[1].HCS_Intr != irqno)
		return;

	spin_lock_irqsave(&io_request_lock, flags);

	tul_isr(&tul_hcs[1]);

	spin_unlock_irqrestore(&io_request_lock, flags);
}

static void i91u_intr2(int irqno, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	if (tul_hcs[2].HCS_Intr != irqno)
		return;

	spin_lock_irqsave(&io_request_lock, flags);

	tul_isr(&tul_hcs[2]);

	spin_unlock_irqrestore(&io_request_lock, flags);
}

static void i91u_intr3(int irqno, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	if (tul_hcs[3].HCS_Intr != irqno)
		return;

	spin_lock_irqsave(&io_request_lock, flags);

	tul_isr(&tul_hcs[3]);

	spin_unlock_irqrestore(&io_request_lock, flags);
}

static void i91u_intr4(int irqno, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	if (tul_hcs[4].HCS_Intr != irqno)
		return;

	spin_lock_irqsave(&io_request_lock, flags);

	tul_isr(&tul_hcs[4]);

	spin_unlock_irqrestore(&io_request_lock, flags);
}

static void i91u_intr5(int irqno, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	if (tul_hcs[5].HCS_Intr != irqno)
		return;

	spin_lock_irqsave(&io_request_lock, flags);

	tul_isr(&tul_hcs[5]);

	spin_unlock_irqrestore(&io_request_lock, flags);
}

static void i91u_intr6(int irqno, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	if (tul_hcs[6].HCS_Intr != irqno)
		return;

	spin_lock_irqsave(&io_request_lock, flags);

	tul_isr(&tul_hcs[6]);

	spin_unlock_irqrestore(&io_request_lock, flags);
}

static void i91u_intr7(int irqno, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	if (tul_hcs[7].HCS_Intr != irqno)
		return;

	spin_lock_irqsave(&io_request_lock, flags);

	tul_isr(&tul_hcs[7]);

	spin_unlock_irqrestore(&io_request_lock, flags);
}

/* 
 * Dump the current driver status and panic...
 */
static void i91u_panic(char *msg)
{
	printk("\ni91u_panic: %s\n", msg);
	panic("i91u panic");
}

/*
 * Release ressources
 */
int i91u_release(struct Scsi_Host *hreg)
{
	free_irq(hreg->irq, hreg);
	release_region(hreg->io_port, 256);
	return 0;
}
MODULE_LICENSE("Dual BSD/GPL");
