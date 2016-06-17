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
 ************************************************************************
    Module: i91uscsi.c
    Description: PCI I/F for INI-910 SCSI Bus Master Controller
    Revision History:
	11/09/94 Tim Chen, Initiali Version 0.90A
	01/17/95 TC, release ver 1.01
	02/09/95 TC  modify ReadPCIConfig, try both mechanisms;
	02/15/95 TC  add support for INI-9100W
	06/04/96 HC, Change to fit LINUX from jaspci.c
	11/18/96 HC, Port for tulip
	07/08/98 hc, Support 0002134A
        07/23/98 wh, Change the abort_srb routine.
	09/16/98 hl, Support ALPHA, Rewrite the returnNumberAdapters	<01>
	12/09/98 bv, Removed unused code, changed tul_se2_wait to
		     use udelay(30) and tul_do_pause to enable 
		     interrupts for >= 2.1.95
	12/13/98 bv, Use spinlocks instead of cli() for serialized
		     access to HCS_Semaph, HCS_FirstAvail and HCS_LastAvail
		     members of the HCS structure.
	01/09/98 bv, Fix a deadlock on SMP system.
**********************************************************************/

#define DEBUG_INTERRUPT 0
#define DEBUG_QUEUE     0
#define DEBUG_STATE     0
#define INT_DISC	0


#ifndef CVT_LINUX_VERSION
#define	CVT_LINUX_VERSION(V,P,S)	(V * 65536 + P * 256 + S)
#endif

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/blk.h>
#include <asm/io.h>

#include "i91uscsi.h"

/*--- external functions --*/
static void tul_se2_wait(void);

/*--- forward refrence ---*/
static SCB *tul_find_busy_scb(HCS * pCurHcb, WORD tarlun);
static SCB *tul_find_done_scb(HCS * pCurHcb);

static int tulip_main(HCS * pCurHcb);

static int tul_next_state(HCS * pCurHcb);
static int tul_state_1(HCS * pCurHcb);
static int tul_state_2(HCS * pCurHcb);
static int tul_state_3(HCS * pCurHcb);
static int tul_state_4(HCS * pCurHcb);
static int tul_state_5(HCS * pCurHcb);
static int tul_state_6(HCS * pCurHcb);
static int tul_state_7(HCS * pCurHcb);
static int tul_xfer_data_in(HCS * pCurHcb);
static int tul_xfer_data_out(HCS * pCurHcb);
static int tul_xpad_in(HCS * pCurHcb);
static int tul_xpad_out(HCS * pCurHcb);
static int tul_status_msg(HCS * pCurHcb);

static int tul_msgin(HCS * pCurHcb);
static int tul_msgin_sync(HCS * pCurHcb);
static int tul_msgin_accept(HCS * pCurHcb);
static int tul_msgout_reject(HCS * pCurHcb);
static int tul_msgin_extend(HCS * pCurHcb);

static int tul_msgout_ide(HCS * pCurHcb);
static int tul_msgout_abort_targ(HCS * pCurHcb);
static int tul_msgout_abort_tag(HCS * pCurHcb);

static int tul_bus_device_reset(HCS * pCurHcb);
static void tul_select_atn(HCS * pCurHcb, SCB * pCurScb);
static void tul_select_atn3(HCS * pCurHcb, SCB * pCurScb);
static void tul_select_atn_stop(HCS * pCurHcb, SCB * pCurScb);
static int int_tul_busfree(HCS * pCurHcb);
int int_tul_scsi_rst(HCS * pCurHcb);
static int int_tul_bad_seq(HCS * pCurHcb);
static int int_tul_resel(HCS * pCurHcb);
static int tul_sync_done(HCS * pCurHcb);
static int wdtr_done(HCS * pCurHcb);
static int wait_tulip(HCS * pCurHcb);
static int tul_wait_done_disc(HCS * pCurHcb);
static int tul_wait_disc(HCS * pCurHcb);
static void tulip_scsi(HCS * pCurHcb);
static int tul_post_scsi_rst(HCS * pCurHcb);

static void tul_se2_ew_en(WORD CurBase);
static void tul_se2_ew_ds(WORD CurBase);
static int tul_se2_rd_all(WORD CurBase);
static void tul_se2_update_all(WORD CurBase);	/* setup default pattern */
static void tul_read_eeprom(WORD CurBase);

				/* ---- EXTERNAL VARIABLES ---- */
HCS tul_hcs[MAX_SUPPORTED_ADAPTERS];
				/* ---- INTERNAL VARIABLES ---- */
static INI_ADPT_STRUCT i91u_adpt[MAX_SUPPORTED_ADAPTERS];

/*NVRAM nvram, *nvramp = &nvram; */
static NVRAM i91unvram;
static NVRAM *i91unvramp;



static UCHAR i91udftNvRam[64] =
{
/*----------- header -----------*/
	0x25, 0xc9,		/* Signature    */
	0x40,			/* Size         */
	0x01,			/* Revision     */
	/* -- Host Adapter Structure -- */
	0x95,			/* ModelByte0   */
	0x00,			/* ModelByte1   */
	0x00,			/* ModelInfo    */
	0x01,			/* NumOfCh      */
	NBC1_DEFAULT,		/* BIOSConfig1  */
	0,			/* BIOSConfig2  */
	0,			/* HAConfig1    */
	0,			/* HAConfig2    */
	/* SCSI channel 0 and target Structure  */
	7,			/* SCSIid       */
	NCC1_DEFAULT,		/* SCSIconfig1  */
	0,			/* SCSIconfig2  */
	0x10,			/* NumSCSItarget */

	NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT,
	NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT,
	NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT,
	NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT,

	/* SCSI channel 1 and target Structure  */
	7,			/* SCSIid       */
	NCC1_DEFAULT,		/* SCSIconfig1  */
	0,			/* SCSIconfig2  */
	0x10,			/* NumSCSItarget */

	NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT,
	NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT,
	NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT,
	NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT, NTC_DEFAULT,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0};			/*      - CheckSum -            */


static UCHAR tul_rate_tbl[8] =	/* fast 20      */
{
				/* nanosecond devide by 4 */
	12,			/* 50ns,  20M   */
	18,			/* 75ns,  13.3M */
	25,			/* 100ns, 10M   */
	31,			/* 125ns, 8M    */
	37,			/* 150ns, 6.6M  */
	43,			/* 175ns, 5.7M  */
	50,			/* 200ns, 5M    */
	62			/* 250ns, 4M    */
};

extern int tul_num_ch;


static void tul_do_pause(unsigned amount)
{				/* Pause for amount jiffies */
	unsigned long the_time = jiffies + amount;

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	while (time_before_eq(jiffies, the_time));
#else
	while (jiffies < the_time);
#endif
}

/*-- forward reference --*/

/*******************************************************************
	Use memeory refresh time        ~ 15us * 2
********************************************************************/
void tul_se2_wait()
{
#if 1
	udelay(30);
#else
	UCHAR readByte;

	readByte = TUL_RD(0, 0x61);
	if ((readByte & 0x10) == 0x10) {
		for (;;) {
			readByte = TUL_RD(0, 0x61);
			if ((readByte & 0x10) == 0x10)
				break;
		}
		for (;;) {
			readByte = TUL_RD(0, 0x61);
			if ((readByte & 0x10) != 0x10)
				break;
		}
	} else {
		for (;;) {
			readByte = TUL_RD(0, 0x61);
			if ((readByte & 0x10) == 0x10)
				break;
		}
		for (;;) {
			readByte = TUL_RD(0, 0x61);
			if ((readByte & 0x10) != 0x10)
				break;
		}
	}
#endif
}


/******************************************************************
 Input: instruction for  Serial E2PROM

 EX: se2_rd(0 call se2_instr() to send address and read command

	 StartBit  OP_Code   Address                Data
	 --------- --------  ------------------     -------
	 1         1 , 0     A5,A4,A3,A2,A1,A0      D15-D0

		 +-----------------------------------------------------
		 |
 CS -----+
			+--+  +--+  +--+  +--+  +--+
			^  |  ^  |  ^  |  ^  |  ^  |
			|  |  |  |  |  |  |  |  |  |
 CLK -------+  +--+  +--+  +--+  +--+  +--
 (leading edge trigger)

		 +--1-----1--+
		 | SB    OP  |  OP    A5    A4
 DI  ----+           +--0------------------
 (address and cmd sent to nvram)

	 -------------------------------------------+
												|
 DO                                             +---
 (data sent from nvram)


******************************************************************/
void tul_se2_instr(WORD CurBase, UCHAR instr)
{
	int i;
	UCHAR b;

	TUL_WR(CurBase + TUL_NVRAM, SE2CS | SE2DO);	/* cs+start bit */
	tul_se2_wait();
	TUL_WR(CurBase + TUL_NVRAM, SE2CS | SE2CLK | SE2DO);	/* +CLK */
	tul_se2_wait();

	for (i = 0; i < 8; i++) {
		if (instr & 0x80)
			b = SE2CS | SE2DO;	/* -CLK+dataBit */
		else
			b = SE2CS;	/* -CLK */
		TUL_WR(CurBase + TUL_NVRAM, b);
		tul_se2_wait();
		TUL_WR(CurBase + TUL_NVRAM, b | SE2CLK);	/* +CLK */
		tul_se2_wait();
		instr <<= 1;
	}
	TUL_WR(CurBase + TUL_NVRAM, SE2CS);	/* -CLK */
	tul_se2_wait();
	return;
}


/******************************************************************
 Function name  : tul_se2_ew_en
 Description    : Enable erase/write state of serial EEPROM
******************************************************************/
void tul_se2_ew_en(WORD CurBase)
{
	tul_se2_instr(CurBase, 0x30);	/* EWEN */
	TUL_WR(CurBase + TUL_NVRAM, 0);		/* -CS  */
	tul_se2_wait();
	return;
}


/************************************************************************
 Disable erase/write state of serial EEPROM
*************************************************************************/
void tul_se2_ew_ds(WORD CurBase)
{
	tul_se2_instr(CurBase, 0);	/* EWDS */
	TUL_WR(CurBase + TUL_NVRAM, 0);		/* -CS  */
	tul_se2_wait();
	return;
}


/******************************************************************
	Input  :address of Serial E2PROM
	Output :value stored in  Serial E2PROM
*******************************************************************/
USHORT tul_se2_rd(WORD CurBase, ULONG adr)
{
	UCHAR instr, readByte;
	USHORT readWord;
	int i;

	instr = (UCHAR) (adr | 0x80);
	tul_se2_instr(CurBase, instr);	/* READ INSTR */
	readWord = 0;

	for (i = 15; i >= 0; i--) {
		TUL_WR(CurBase + TUL_NVRAM, SE2CS | SE2CLK);	/* +CLK */
		tul_se2_wait();
		TUL_WR(CurBase + TUL_NVRAM, SE2CS);	/* -CLK */

		/* sample data after the following edge of clock  */
		readByte = TUL_RD(CurBase, TUL_NVRAM);
		readByte &= SE2DI;
		readWord += (readByte << i);
		tul_se2_wait();	/* 6/20/95 */
	}

	TUL_WR(CurBase + TUL_NVRAM, 0);		/* no chip select */
	tul_se2_wait();
	return readWord;
}


/******************************************************************
 Input: new value in  Serial E2PROM, address of Serial E2PROM
*******************************************************************/
void tul_se2_wr(WORD CurBase, UCHAR adr, USHORT writeWord)
{
	UCHAR readByte;
	UCHAR instr;
	int i;

	instr = (UCHAR) (adr | 0x40);
	tul_se2_instr(CurBase, instr);	/* WRITE INSTR */
	for (i = 15; i >= 0; i--) {
		if (writeWord & 0x8000)
			TUL_WR(CurBase + TUL_NVRAM, SE2CS | SE2DO);	/* -CLK+dataBit 1 */
		else
			TUL_WR(CurBase + TUL_NVRAM, SE2CS);	/* -CLK+dataBit 0 */
		tul_se2_wait();
		TUL_WR(CurBase + TUL_NVRAM, SE2CS | SE2CLK);	/* +CLK */
		tul_se2_wait();
		writeWord <<= 1;
	}
	TUL_WR(CurBase + TUL_NVRAM, SE2CS);	/* -CLK */
	tul_se2_wait();
	TUL_WR(CurBase + TUL_NVRAM, 0);		/* -CS  */
	tul_se2_wait();

	TUL_WR(CurBase + TUL_NVRAM, SE2CS);	/* +CS  */
	tul_se2_wait();

	for (;;) {
		TUL_WR(CurBase + TUL_NVRAM, SE2CS | SE2CLK);	/* +CLK */
		tul_se2_wait();
		TUL_WR(CurBase + TUL_NVRAM, SE2CS);	/* -CLK */
		tul_se2_wait();
		if ((readByte = TUL_RD(CurBase, TUL_NVRAM)) & SE2DI)
			break;	/* write complete */
	}
	TUL_WR(CurBase + TUL_NVRAM, 0);		/* -CS */
	return;
}


/***********************************************************************
 Read SCSI H/A configuration parameters from serial EEPROM
************************************************************************/
int tul_se2_rd_all(WORD CurBase)
{
	int i;
	ULONG chksum = 0;
	USHORT *np;

	i91unvramp = &i91unvram;
	np = (USHORT *) i91unvramp;
	for (i = 0; i < 32; i++) {
		*np++ = tul_se2_rd(CurBase, i);
	}

/*--------------------Is signature "ini" ok ? ----------------*/
	if (i91unvramp->NVM_Signature != INI_SIGNATURE)
		return -1;
/*---------------------- Is ckecksum ok ? ----------------------*/
	np = (USHORT *) i91unvramp;
	for (i = 0; i < 31; i++)
		chksum += *np++;
	if (i91unvramp->NVM_CheckSum != (USHORT) chksum)
		return -1;
	return 1;
}


/***********************************************************************
 Update SCSI H/A configuration parameters from serial EEPROM
************************************************************************/
void tul_se2_update_all(WORD CurBase)
{				/* setup default pattern */
	int i;
	ULONG chksum = 0;
	USHORT *np, *np1;

	i91unvramp = &i91unvram;
	/* Calculate checksum first */
	np = (USHORT *) i91udftNvRam;
	for (i = 0; i < 31; i++)
		chksum += *np++;
	*np = (USHORT) chksum;
	tul_se2_ew_en(CurBase);	/* Enable write  */

	np = (USHORT *) i91udftNvRam;
	np1 = (USHORT *) i91unvramp;
	for (i = 0; i < 32; i++, np++, np1++) {
		if (*np != *np1) {
			tul_se2_wr(CurBase, i, *np);
		}
	}

	tul_se2_ew_ds(CurBase);	/* Disable write   */
	return;
}

/*************************************************************************
 Function name  : read_eeprom
**************************************************************************/
void tul_read_eeprom(WORD CurBase)
{
	UCHAR gctrl;

	i91unvramp = &i91unvram;
/*------Enable EEProm programming ---*/
	gctrl = TUL_RD(CurBase, TUL_GCTRL);
	TUL_WR(CurBase + TUL_GCTRL, gctrl | TUL_GCTRL_EEPROM_BIT);
	if (tul_se2_rd_all(CurBase) != 1) {
		tul_se2_update_all(CurBase);	/* setup default pattern */
		tul_se2_rd_all(CurBase);	/* load again  */
	}
/*------ Disable EEProm programming ---*/
	gctrl = TUL_RD(CurBase, TUL_GCTRL);
	TUL_WR(CurBase + TUL_GCTRL, gctrl & ~TUL_GCTRL_EEPROM_BIT);
}				/* read_eeprom */

int Addi91u_into_Adapter_table(WORD wBIOS, WORD wBASE, BYTE bInterrupt,
			       BYTE bBus, BYTE bDevice)
{
	int i, j;

	for (i = 0; i < MAX_SUPPORTED_ADAPTERS; i++) {
		if (i91u_adpt[i].ADPT_BIOS < wBIOS)
			continue;
		if (i91u_adpt[i].ADPT_BIOS == wBIOS) {
			if (i91u_adpt[i].ADPT_BASE == wBASE) {
				if (i91u_adpt[i].ADPT_Bus != 0xFF)
					return (FAILURE);
			} else if (i91u_adpt[i].ADPT_BASE < wBASE)
					continue;
		}
		for (j = MAX_SUPPORTED_ADAPTERS - 1; j > i; j--) {
			i91u_adpt[j].ADPT_BASE = i91u_adpt[j - 1].ADPT_BASE;
			i91u_adpt[j].ADPT_INTR = i91u_adpt[j - 1].ADPT_INTR;
			i91u_adpt[j].ADPT_BIOS = i91u_adpt[j - 1].ADPT_BIOS;
			i91u_adpt[j].ADPT_Bus = i91u_adpt[j - 1].ADPT_Bus;
			i91u_adpt[j].ADPT_Device = i91u_adpt[j - 1].ADPT_Device;
		}
		i91u_adpt[i].ADPT_BASE = wBASE;
		i91u_adpt[i].ADPT_INTR = bInterrupt;
		i91u_adpt[i].ADPT_BIOS = wBIOS;
		i91u_adpt[i].ADPT_Bus = bBus;
		i91u_adpt[i].ADPT_Device = bDevice;
		return (SUCCESSFUL);
	}
	return (FAILURE);
}

void init_i91uAdapter_table(void)
{
	int i;

	for (i = 0; i < MAX_SUPPORTED_ADAPTERS; i++) {	/* Initialize adapter structure */
		i91u_adpt[i].ADPT_BIOS = 0xffff;
		i91u_adpt[i].ADPT_BASE = 0xffff;
		i91u_adpt[i].ADPT_INTR = 0xff;
		i91u_adpt[i].ADPT_Bus = 0xff;
		i91u_adpt[i].ADPT_Device = 0xff;
	}
	return;
}

void tul_stop_bm(HCS * pCurHcb)
{

	if (TUL_RD(pCurHcb->HCS_Base, TUL_XStatus) & XPEND) {	/* if DMA xfer is pending, abort DMA xfer */
		TUL_WR(pCurHcb->HCS_Base + TUL_XCmd, TAX_X_ABT | TAX_X_CLR_FIFO);
		/* wait Abort DMA xfer done */
		while ((TUL_RD(pCurHcb->HCS_Base, TUL_Int) & XABT) == 0);
	}
	TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);
}

/***************************************************************************/
void get_tulipPCIConfig(HCS * pCurHcb, int ch_idx)
{
	pCurHcb->HCS_Base = i91u_adpt[ch_idx].ADPT_BASE;	/* Supply base address  */
	pCurHcb->HCS_BIOS = i91u_adpt[ch_idx].ADPT_BIOS;	/* Supply BIOS address  */
	pCurHcb->HCS_Intr = i91u_adpt[ch_idx].ADPT_INTR;	/* Supply interrupt line */
	return;
}

/***************************************************************************/
int tul_reset_scsi(HCS * pCurHcb, int seconds)
{
	TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_RST_BUS);

	while (!((pCurHcb->HCS_JSInt = TUL_RD(pCurHcb->HCS_Base, TUL_SInt)) & TSS_SCSIRST_INT));
	/* reset tulip chip */

	TUL_WR(pCurHcb->HCS_Base + TUL_SSignal, 0);

	/* Stall for a while, wait for target's firmware ready,make it 2 sec ! */
	/* SONY 5200 tape drive won't work if only stall for 1 sec */
	tul_do_pause(seconds * HZ);

	TUL_RD(pCurHcb->HCS_Base, TUL_SInt);

	return (SCSI_RESET_SUCCESS);
}

/***************************************************************************/
int init_tulip(HCS * pCurHcb, SCB * scbp, int tul_num_scb, BYTE * pbBiosAdr, int seconds)
{
	int i;
	BYTE *pwFlags;
	BYTE *pbHeads;
	SCB *pTmpScb, *pPrevScb = NULL;

	pCurHcb->HCS_NumScbs = tul_num_scb;
	pCurHcb->HCS_Semaph = 1;
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	pCurHcb->HCS_SemaphLock = SPIN_LOCK_UNLOCKED;
#endif
	pCurHcb->HCS_JSStatus0 = 0;
	pCurHcb->HCS_Scb = scbp;
	pCurHcb->HCS_NxtPend = scbp;
	pCurHcb->HCS_NxtAvail = scbp;
	for (i = 0, pTmpScb = scbp; i < tul_num_scb; i++, pTmpScb++) {
		pTmpScb->SCB_TagId = i;
		if (i != 0)
			pPrevScb->SCB_NxtScb = pTmpScb;
		pPrevScb = pTmpScb;
	}
	pPrevScb->SCB_NxtScb = NULL;
	pCurHcb->HCS_ScbEnd = pTmpScb;
	pCurHcb->HCS_FirstAvail = scbp;
	pCurHcb->HCS_LastAvail = pPrevScb;
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	pCurHcb->HCS_AvailLock = SPIN_LOCK_UNLOCKED;
#endif
	pCurHcb->HCS_FirstPend = NULL;
	pCurHcb->HCS_LastPend = NULL;
	pCurHcb->HCS_FirstBusy = NULL;
	pCurHcb->HCS_LastBusy = NULL;
	pCurHcb->HCS_FirstDone = NULL;
	pCurHcb->HCS_LastDone = NULL;
	pCurHcb->HCS_ActScb = NULL;
	pCurHcb->HCS_ActTcs = NULL;

	tul_read_eeprom(pCurHcb->HCS_Base);
/*---------- get H/A configuration -------------*/
	if (i91unvramp->NVM_SCSIInfo[0].NVM_NumOfTarg == 8)
		pCurHcb->HCS_MaxTar = 8;
	else
		pCurHcb->HCS_MaxTar = 16;

	pCurHcb->HCS_Config = i91unvramp->NVM_SCSIInfo[0].NVM_ChConfig1;

	pCurHcb->HCS_SCSI_ID = i91unvramp->NVM_SCSIInfo[0].NVM_ChSCSIID;
	pCurHcb->HCS_IdMask = ~(1 << pCurHcb->HCS_SCSI_ID);

#if CHK_PARITY
	/* Enable parity error response */
	TUL_WR(pCurHcb->HCS_Base + TUL_PCMD, TUL_RD(pCurHcb->HCS_Base, TUL_PCMD) | 0x40);
#endif

	/* Mask all the interrupt       */
	TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x1F);

	tul_stop_bm(pCurHcb);
	/* --- Initialize the tulip --- */
	TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_RST_CHIP);

	/* program HBA's SCSI ID        */
	TUL_WR(pCurHcb->HCS_Base + TUL_SScsiId, pCurHcb->HCS_SCSI_ID << 4);

	/* Enable Initiator Mode ,phase latch,alternate sync period mode,
	   disable SCSI reset */
	if (pCurHcb->HCS_Config & HCC_EN_PAR)
		pCurHcb->HCS_SConf1 = (TSC_INITDEFAULT | TSC_EN_SCSI_PAR);
	else
		pCurHcb->HCS_SConf1 = (TSC_INITDEFAULT);
	TUL_WR(pCurHcb->HCS_Base + TUL_SConfig, pCurHcb->HCS_SConf1);

	/* Enable HW reselect           */
	TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl1, TSC_HW_RESELECT);

	TUL_WR(pCurHcb->HCS_Base + TUL_SPeriod, 0);

	/* selection time out = 250 ms */
	TUL_WR(pCurHcb->HCS_Base + TUL_STimeOut, 153);

/*--------- Enable SCSI terminator -----*/
	TUL_WR(pCurHcb->HCS_Base + TUL_XCtrl, (pCurHcb->HCS_Config & (HCC_ACT_TERM1 | HCC_ACT_TERM2)));
	TUL_WR(pCurHcb->HCS_Base + TUL_GCTRL1,
	       ((pCurHcb->HCS_Config & HCC_AUTO_TERM) >> 4) | (TUL_RD(pCurHcb->HCS_Base, TUL_GCTRL1) & 0xFE));

	for (i = 0,
	     pwFlags = & (i91unvramp->NVM_SCSIInfo[0].NVM_Targ0Config),
	     pbHeads = pbBiosAdr + 0x180;
	     i < pCurHcb->HCS_MaxTar;
	     i++, pwFlags++) {
		pCurHcb->HCS_Tcs[i].TCS_Flags = *pwFlags & ~(TCF_SYNC_DONE | TCF_WDTR_DONE);
		if (pCurHcb->HCS_Tcs[i].TCS_Flags & TCF_EN_255)
			pCurHcb->HCS_Tcs[i].TCS_DrvFlags = TCF_DRV_255_63;
		else
			pCurHcb->HCS_Tcs[i].TCS_DrvFlags = 0;
		pCurHcb->HCS_Tcs[i].TCS_JS_Period = 0;
		pCurHcb->HCS_Tcs[i].TCS_SConfig0 = pCurHcb->HCS_SConf1;
		pCurHcb->HCS_Tcs[i].TCS_DrvHead = *pbHeads++;
		if (pCurHcb->HCS_Tcs[i].TCS_DrvHead == 255)
			pCurHcb->HCS_Tcs[i].TCS_DrvFlags = TCF_DRV_255_63;
		else
			pCurHcb->HCS_Tcs[i].TCS_DrvFlags = 0;
		pCurHcb->HCS_Tcs[i].TCS_DrvSector = *pbHeads++;
		pCurHcb->HCS_Tcs[i].TCS_Flags &= ~TCF_BUSY;
		pCurHcb->HCS_ActTags[i] = 0;
		pCurHcb->HCS_MaxTags[i] = 0xFF;
	}			/* for                          */
	printk("i91u: PCI Base=0x%04X, IRQ=%d, BIOS=0x%04X0, SCSI ID=%d\n",
	       pCurHcb->HCS_Base, pCurHcb->HCS_Intr,
	       pCurHcb->HCS_BIOS, pCurHcb->HCS_SCSI_ID);
/*------------------- reset SCSI Bus ---------------------------*/
	if (pCurHcb->HCS_Config & HCC_SCSI_RESET) {
		printk("i91u: Reset SCSI Bus ... \n");
		tul_reset_scsi(pCurHcb, seconds);
	}
	TUL_WR(pCurHcb->HCS_Base + TUL_SCFG1, 0x17);
	TUL_WR(pCurHcb->HCS_Base + TUL_SIntEnable, 0xE9);
	return (0);
}

/***************************************************************************/
SCB *tul_alloc_scb(HCS * hcsp)
{
	SCB *pTmpScb;
	ULONG flags;
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_lock_irqsave(&(hcsp->HCS_AvailLock), flags);
#else
	save_flags(flags);
	cli();
#endif
	if ((pTmpScb = hcsp->HCS_FirstAvail) != NULL) {
#if DEBUG_QUEUE
		printk("find scb at %08lx\n", (ULONG) pTmpScb);
#endif
		if ((hcsp->HCS_FirstAvail = pTmpScb->SCB_NxtScb) == NULL)
			hcsp->HCS_LastAvail = NULL;
		pTmpScb->SCB_NxtScb = NULL;
		pTmpScb->SCB_Status = SCB_RENT;
	}
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_unlock_irqrestore(&(hcsp->HCS_AvailLock), flags);
#else
	restore_flags(flags);
#endif
	return (pTmpScb);
}

/***************************************************************************/
void tul_release_scb(HCS * hcsp, SCB * scbp)
{
	ULONG flags;

#if DEBUG_QUEUE
	printk("Release SCB %lx; ", (ULONG) scbp);
#endif
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_lock_irqsave(&(hcsp->HCS_AvailLock), flags);
#else
	save_flags(flags);
	cli();
#endif
	scbp->SCB_Srb = 0;
	scbp->SCB_Status = 0;
	scbp->SCB_NxtScb = NULL;
	if (hcsp->HCS_LastAvail != NULL) {
		hcsp->HCS_LastAvail->SCB_NxtScb = scbp;
		hcsp->HCS_LastAvail = scbp;
	} else {
		hcsp->HCS_FirstAvail = scbp;
		hcsp->HCS_LastAvail = scbp;
	}
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_unlock_irqrestore(&(hcsp->HCS_AvailLock), flags);
#else
	restore_flags(flags);
#endif
}

/***************************************************************************/
void tul_append_pend_scb(HCS * pCurHcb, SCB * scbp)
{

#if DEBUG_QUEUE
	printk("Append pend SCB %lx; ", (ULONG) scbp);
#endif
	scbp->SCB_Status = SCB_PEND;
	scbp->SCB_NxtScb = NULL;
	if (pCurHcb->HCS_LastPend != NULL) {
		pCurHcb->HCS_LastPend->SCB_NxtScb = scbp;
		pCurHcb->HCS_LastPend = scbp;
	} else {
		pCurHcb->HCS_FirstPend = scbp;
		pCurHcb->HCS_LastPend = scbp;
	}
}

/***************************************************************************/
void tul_push_pend_scb(HCS * pCurHcb, SCB * scbp)
{

#if DEBUG_QUEUE
	printk("Push pend SCB %lx; ", (ULONG) scbp);
#endif
	scbp->SCB_Status = SCB_PEND;
	if ((scbp->SCB_NxtScb = pCurHcb->HCS_FirstPend) != NULL) {
		pCurHcb->HCS_FirstPend = scbp;
	} else {
		pCurHcb->HCS_FirstPend = scbp;
		pCurHcb->HCS_LastPend = scbp;
	}
}

/***************************************************************************/
SCB *tul_find_first_pend_scb(HCS * pCurHcb)
{
	SCB *pFirstPend;


	pFirstPend = pCurHcb->HCS_FirstPend;
	while (pFirstPend != NULL) {
		if (pFirstPend->SCB_Opcode != ExecSCSI) {
			return (pFirstPend);
		}
		if (pFirstPend->SCB_TagMsg == 0) {
			if ((pCurHcb->HCS_ActTags[pFirstPend->SCB_Target] == 0) &&
			    !(pCurHcb->HCS_Tcs[pFirstPend->SCB_Target].TCS_Flags & TCF_BUSY)) {
				return (pFirstPend);
			}
		} else {
			if ((pCurHcb->HCS_ActTags[pFirstPend->SCB_Target] >=
			  pCurHcb->HCS_MaxTags[pFirstPend->SCB_Target]) |
			    (pCurHcb->HCS_Tcs[pFirstPend->SCB_Target].TCS_Flags & TCF_BUSY)) {
				pFirstPend = pFirstPend->SCB_NxtScb;
				continue;
			}
			return (pFirstPend);
		}
		pFirstPend = pFirstPend->SCB_NxtScb;
	}


	return (pFirstPend);
}
/***************************************************************************/
SCB *tul_pop_pend_scb(HCS * pCurHcb)
{
	SCB *pTmpScb;

	if ((pTmpScb = pCurHcb->HCS_FirstPend) != NULL) {
		if ((pCurHcb->HCS_FirstPend = pTmpScb->SCB_NxtScb) == NULL)
			pCurHcb->HCS_LastPend = NULL;
		pTmpScb->SCB_NxtScb = NULL;
	}
#if DEBUG_QUEUE
	printk("Pop pend SCB %lx; ", (ULONG) pTmpScb);
#endif
	return (pTmpScb);
}


/***************************************************************************/
void tul_unlink_pend_scb(HCS * pCurHcb, SCB * pCurScb)
{
	SCB *pTmpScb, *pPrevScb;

#if DEBUG_QUEUE
	printk("unlink pend SCB %lx; ", (ULONG) pCurScb);
#endif

	pPrevScb = pTmpScb = pCurHcb->HCS_FirstPend;
	while (pTmpScb != NULL) {
		if (pCurScb == pTmpScb) {	/* Unlink this SCB              */
			if (pTmpScb == pCurHcb->HCS_FirstPend) {
				if ((pCurHcb->HCS_FirstPend = pTmpScb->SCB_NxtScb) == NULL)
					pCurHcb->HCS_LastPend = NULL;
			} else {
				pPrevScb->SCB_NxtScb = pTmpScb->SCB_NxtScb;
				if (pTmpScb == pCurHcb->HCS_LastPend)
					pCurHcb->HCS_LastPend = pPrevScb;
			}
			pTmpScb->SCB_NxtScb = NULL;
			break;
		}
		pPrevScb = pTmpScb;
		pTmpScb = pTmpScb->SCB_NxtScb;
	}
	return;
}
/***************************************************************************/
void tul_append_busy_scb(HCS * pCurHcb, SCB * scbp)
{

#if DEBUG_QUEUE
	printk("append busy SCB %lx; ", (ULONG) scbp);
#endif
	if (scbp->SCB_TagMsg)
		pCurHcb->HCS_ActTags[scbp->SCB_Target]++;
	else
		pCurHcb->HCS_Tcs[scbp->SCB_Target].TCS_Flags |= TCF_BUSY;
	scbp->SCB_Status = SCB_BUSY;
	scbp->SCB_NxtScb = NULL;
	if (pCurHcb->HCS_LastBusy != NULL) {
		pCurHcb->HCS_LastBusy->SCB_NxtScb = scbp;
		pCurHcb->HCS_LastBusy = scbp;
	} else {
		pCurHcb->HCS_FirstBusy = scbp;
		pCurHcb->HCS_LastBusy = scbp;
	}
}

/***************************************************************************/
SCB *tul_pop_busy_scb(HCS * pCurHcb)
{
	SCB *pTmpScb;


	if ((pTmpScb = pCurHcb->HCS_FirstBusy) != NULL) {
		if ((pCurHcb->HCS_FirstBusy = pTmpScb->SCB_NxtScb) == NULL)
			pCurHcb->HCS_LastBusy = NULL;
		pTmpScb->SCB_NxtScb = NULL;
		if (pTmpScb->SCB_TagMsg)
			pCurHcb->HCS_ActTags[pTmpScb->SCB_Target]--;
		else
			pCurHcb->HCS_Tcs[pTmpScb->SCB_Target].TCS_Flags &= ~TCF_BUSY;
	}
#if DEBUG_QUEUE
	printk("Pop busy SCB %lx; ", (ULONG) pTmpScb);
#endif
	return (pTmpScb);
}

/***************************************************************************/
void tul_unlink_busy_scb(HCS * pCurHcb, SCB * pCurScb)
{
	SCB *pTmpScb, *pPrevScb;

#if DEBUG_QUEUE
	printk("unlink busy SCB %lx; ", (ULONG) pCurScb);
#endif

	pPrevScb = pTmpScb = pCurHcb->HCS_FirstBusy;
	while (pTmpScb != NULL) {
		if (pCurScb == pTmpScb) {	/* Unlink this SCB              */
			if (pTmpScb == pCurHcb->HCS_FirstBusy) {
				if ((pCurHcb->HCS_FirstBusy = pTmpScb->SCB_NxtScb) == NULL)
					pCurHcb->HCS_LastBusy = NULL;
			} else {
				pPrevScb->SCB_NxtScb = pTmpScb->SCB_NxtScb;
				if (pTmpScb == pCurHcb->HCS_LastBusy)
					pCurHcb->HCS_LastBusy = pPrevScb;
			}
			pTmpScb->SCB_NxtScb = NULL;
			if (pTmpScb->SCB_TagMsg)
				pCurHcb->HCS_ActTags[pTmpScb->SCB_Target]--;
			else
				pCurHcb->HCS_Tcs[pTmpScb->SCB_Target].TCS_Flags &= ~TCF_BUSY;
			break;
		}
		pPrevScb = pTmpScb;
		pTmpScb = pTmpScb->SCB_NxtScb;
	}
	return;
}

/***************************************************************************/
SCB *tul_find_busy_scb(HCS * pCurHcb, WORD tarlun)
{
	SCB *pTmpScb, *pPrevScb;
	WORD scbp_tarlun;


	pPrevScb = pTmpScb = pCurHcb->HCS_FirstBusy;
	while (pTmpScb != NULL) {
		scbp_tarlun = (pTmpScb->SCB_Lun << 8) | (pTmpScb->SCB_Target);
		if (scbp_tarlun == tarlun) {	/* Unlink this SCB              */
			break;
		}
		pPrevScb = pTmpScb;
		pTmpScb = pTmpScb->SCB_NxtScb;
	}
#if DEBUG_QUEUE
	printk("find busy SCB %lx; ", (ULONG) pTmpScb);
#endif
	return (pTmpScb);
}

/***************************************************************************/
void tul_append_done_scb(HCS * pCurHcb, SCB * scbp)
{

#if DEBUG_QUEUE
	printk("append done SCB %lx; ", (ULONG) scbp);
#endif

	scbp->SCB_Status = SCB_DONE;
	scbp->SCB_NxtScb = NULL;
	if (pCurHcb->HCS_LastDone != NULL) {
		pCurHcb->HCS_LastDone->SCB_NxtScb = scbp;
		pCurHcb->HCS_LastDone = scbp;
	} else {
		pCurHcb->HCS_FirstDone = scbp;
		pCurHcb->HCS_LastDone = scbp;
	}
}

/***************************************************************************/
SCB *tul_find_done_scb(HCS * pCurHcb)
{
	SCB *pTmpScb;


	if ((pTmpScb = pCurHcb->HCS_FirstDone) != NULL) {
		if ((pCurHcb->HCS_FirstDone = pTmpScb->SCB_NxtScb) == NULL)
			pCurHcb->HCS_LastDone = NULL;
		pTmpScb->SCB_NxtScb = NULL;
	}
#if DEBUG_QUEUE
	printk("find done SCB %lx; ", (ULONG) pTmpScb);
#endif
	return (pTmpScb);
}

/***************************************************************************/
int tul_abort_srb(HCS * pCurHcb, ULONG srbp)
{
	ULONG flags;
	SCB *pTmpScb, *pPrevScb;

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_lock_irqsave(&(pCurHcb->HCS_SemaphLock), flags);
#else
	save_flags(flags);
	cli();
#endif

	if ((pCurHcb->HCS_Semaph == 0) && (pCurHcb->HCS_ActScb == NULL)) {
		TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x1F);
		/* disable Jasmin SCSI Int        */

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
                spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#endif

		tulip_main(pCurHcb);

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
        	spin_lock_irqsave(&(pCurHcb->HCS_SemaphLock), flags);
#endif

		pCurHcb->HCS_Semaph = 1;
		TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x0F);

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
		spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
		restore_flags(flags);
#endif

		return SCSI_ABORT_SNOOZE;
	}
	pPrevScb = pTmpScb = pCurHcb->HCS_FirstPend;	/* Check Pend queue */
	while (pTmpScb != NULL) {
		/* 07/27/98 */
		if (pTmpScb->SCB_Srb == (unsigned char *) srbp) {
			if (pTmpScb == pCurHcb->HCS_ActScb) {
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
				spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
				restore_flags(flags);
#endif
				return SCSI_ABORT_BUSY;
			} else if (pTmpScb == pCurHcb->HCS_FirstPend) {
				if ((pCurHcb->HCS_FirstPend = pTmpScb->SCB_NxtScb) == NULL)
					pCurHcb->HCS_LastPend = NULL;
			} else {
				pPrevScb->SCB_NxtScb = pTmpScb->SCB_NxtScb;
				if (pTmpScb == pCurHcb->HCS_LastPend)
					pCurHcb->HCS_LastPend = pPrevScb;
			}
			pTmpScb->SCB_HaStat = HOST_ABORTED;
			pTmpScb->SCB_Flags |= SCF_DONE;
			if (pTmpScb->SCB_Flags & SCF_POST)
				(*pTmpScb->SCB_Post) ((BYTE *) pCurHcb, (BYTE *) pTmpScb);
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
			spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
			restore_flags(flags);
#endif
			return SCSI_ABORT_SUCCESS;
		}
		pPrevScb = pTmpScb;
		pTmpScb = pTmpScb->SCB_NxtScb;
	}

	pPrevScb = pTmpScb = pCurHcb->HCS_FirstBusy;	/* Check Busy queue */
	while (pTmpScb != NULL) {

		if (pTmpScb->SCB_Srb == (unsigned char *) srbp) {

			if (pTmpScb == pCurHcb->HCS_ActScb) {
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
				spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
				restore_flags(flags);
#endif
				return SCSI_ABORT_BUSY;
			} else if (pTmpScb->SCB_TagMsg == 0) {
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
				spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
				restore_flags(flags);
#endif
				return SCSI_ABORT_BUSY;
			} else {
				pCurHcb->HCS_ActTags[pTmpScb->SCB_Target]--;
				if (pTmpScb == pCurHcb->HCS_FirstBusy) {
					if ((pCurHcb->HCS_FirstBusy = pTmpScb->SCB_NxtScb) == NULL)
						pCurHcb->HCS_LastBusy = NULL;
				} else {
					pPrevScb->SCB_NxtScb = pTmpScb->SCB_NxtScb;
					if (pTmpScb == pCurHcb->HCS_LastBusy)
						pCurHcb->HCS_LastBusy = pPrevScb;
				}
				pTmpScb->SCB_NxtScb = NULL;


				pTmpScb->SCB_HaStat = HOST_ABORTED;
				pTmpScb->SCB_Flags |= SCF_DONE;
				if (pTmpScb->SCB_Flags & SCF_POST)
					(*pTmpScb->SCB_Post) ((BYTE *) pCurHcb, (BYTE *) pTmpScb);
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
				spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
				restore_flags(flags);
#endif
				return SCSI_ABORT_SUCCESS;
			}
		}
		pPrevScb = pTmpScb;
		pTmpScb = pTmpScb->SCB_NxtScb;
	}
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
	restore_flags(flags);
#endif
	return (SCSI_ABORT_NOT_RUNNING);
}

/***************************************************************************/
int tul_bad_seq(HCS * pCurHcb)
{
	SCB *pCurScb;

	printk("tul_bad_seg c=%d\n", pCurHcb->HCS_Index);

	if ((pCurScb = pCurHcb->HCS_ActScb) != NULL) {
		tul_unlink_busy_scb(pCurHcb, pCurScb);
		pCurScb->SCB_HaStat = HOST_BAD_PHAS;
		pCurScb->SCB_TaStat = 0;
		tul_append_done_scb(pCurHcb, pCurScb);
	}
	tul_stop_bm(pCurHcb);

	tul_reset_scsi(pCurHcb, 8);	/* 7/29/98 */

	return (tul_post_scsi_rst(pCurHcb));
}

/************************************************************************/
int tul_device_reset(HCS * pCurHcb, ULONG pSrb, unsigned int target, unsigned int ResetFlags)
{
	ULONG flags;
	SCB *pScb;
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_lock_irqsave(&(pCurHcb->HCS_SemaphLock), flags);
#else
	save_flags(flags);
	cli();
#endif

	if (ResetFlags & SCSI_RESET_ASYNCHRONOUS) {

		if ((pCurHcb->HCS_Semaph == 0) && (pCurHcb->HCS_ActScb == NULL)) {
			TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x1F);
			/* disable Jasmin SCSI Int        */

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
        		spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#endif

			tulip_main(pCurHcb);

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
        		spin_lock_irqsave(&(pCurHcb->HCS_SemaphLock), flags);
#endif

			pCurHcb->HCS_Semaph = 1;
			TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x0F);

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
			spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
			restore_flags(flags);
#endif

			return SCSI_RESET_SNOOZE;
		}
		pScb = pCurHcb->HCS_FirstBusy;	/* Check Busy queue */
		while (pScb != NULL) {
			if (pScb->SCB_Srb == (unsigned char *) pSrb)
				break;
			pScb = pScb->SCB_NxtScb;
		}
		if (pScb == NULL) {
			printk("Unable to Reset - No SCB Found\n");

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
			spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
			restore_flags(flags);
#endif
			return SCSI_RESET_NOT_RUNNING;
		}
	}
	if ((pScb = tul_alloc_scb(pCurHcb)) == NULL) {
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
		spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
		restore_flags(flags);
#endif
		return SCSI_RESET_NOT_RUNNING;
	}
	pScb->SCB_Opcode = BusDevRst;
	pScb->SCB_Flags = SCF_POST;
	pScb->SCB_Target = target;
	pScb->SCB_Mode = 0;

	pScb->SCB_Srb = 0;
	if (ResetFlags & SCSI_RESET_SYNCHRONOUS) {
		pScb->SCB_Srb = (unsigned char *) pSrb;
	}
	tul_push_pend_scb(pCurHcb, pScb);	/* push this SCB to Pending queue */

	if (pCurHcb->HCS_Semaph == 1) {
		TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x1F);
		/* disable Jasmin SCSI Int        */
		pCurHcb->HCS_Semaph = 0;

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
        	spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#endif

		tulip_main(pCurHcb);

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
                spin_lock_irqsave(&(pCurHcb->HCS_SemaphLock), flags);
#endif

		pCurHcb->HCS_Semaph = 1;
		TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x0F);
	}
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
	restore_flags(flags);
#endif
	return SCSI_RESET_PENDING;
}

int tul_reset_scsi_bus(HCS * pCurHcb)
{
	ULONG flags;

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_lock_irqsave(&(pCurHcb->HCS_SemaphLock), flags);
#else
	save_flags(flags);
	cli();
#endif
	TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x1F);
	pCurHcb->HCS_Semaph = 0;

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
	restore_flags(flags);
#endif

	tul_stop_bm(pCurHcb);

	tul_reset_scsi(pCurHcb, 2);	/* 7/29/98 */

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_lock_irqsave(&(pCurHcb->HCS_SemaphLock), flags);
#else
	save_flags(flags);
	cli();
#endif
	tul_post_scsi_rst(pCurHcb);

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
        spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#endif

	tulip_main(pCurHcb);

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
        spin_lock_irqsave(&(pCurHcb->HCS_SemaphLock), flags);
#endif

	pCurHcb->HCS_Semaph = 1;
	TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x0F);
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
	restore_flags(flags);
#endif
	return (SCSI_RESET_SUCCESS | SCSI_RESET_HOST_RESET);
}

/************************************************************************/
void tul_exec_scb(HCS * pCurHcb, SCB * pCurScb)
{
	ULONG flags;

	pCurScb->SCB_Mode = 0;

	pCurScb->SCB_SGIdx = 0;
	pCurScb->SCB_SGMax = pCurScb->SCB_SGLen;

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_lock_irqsave(&(pCurHcb->HCS_SemaphLock), flags);
#else
	save_flags(flags);
	cli();
#endif

	tul_append_pend_scb(pCurHcb, pCurScb);	/* Append this SCB to Pending queue */

/* VVVVV 07/21/98 */
	if (pCurHcb->HCS_Semaph == 1) {
		TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x1F);
		/* disable Jasmin SCSI Int        */
		pCurHcb->HCS_Semaph = 0;

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
        	spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#endif

		tulip_main(pCurHcb);

#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
        	spin_lock_irqsave(&(pCurHcb->HCS_SemaphLock), flags);
#endif

		pCurHcb->HCS_Semaph = 1;
		TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x0F);
	}
#if LINUX_VERSION_CODE >= CVT_LINUX_VERSION(2,1,95)
	spin_unlock_irqrestore(&(pCurHcb->HCS_SemaphLock), flags);
#else
	restore_flags(flags);
#endif
	return;
}

/***************************************************************************/
int tul_isr(HCS * pCurHcb)
{
	/* Enter critical section       */

	if (TUL_RD(pCurHcb->HCS_Base, TUL_Int) & TSS_INT_PENDING) {
		if (pCurHcb->HCS_Semaph == 1) {
			TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x1F);
			/* Disable Tulip SCSI Int */
			pCurHcb->HCS_Semaph = 0;

			tulip_main(pCurHcb);

			pCurHcb->HCS_Semaph = 1;
			TUL_WR(pCurHcb->HCS_Base + TUL_Mask, 0x0F);
			return (1);
		}
	}
	return (0);
}

/***************************************************************************/
int tulip_main(HCS * pCurHcb)
{
	SCB *pCurScb;

	for (;;) {

		tulip_scsi(pCurHcb);	/* Call tulip_scsi              */

		while ((pCurScb = tul_find_done_scb(pCurHcb)) != NULL) {	/* find done entry */
			if (pCurScb->SCB_TaStat == QUEUE_FULL) {
				pCurHcb->HCS_MaxTags[pCurScb->SCB_Target] =
				    pCurHcb->HCS_ActTags[pCurScb->SCB_Target] - 1;
				pCurScb->SCB_TaStat = 0;
				tul_append_pend_scb(pCurHcb, pCurScb);
				continue;
			}
			if (!(pCurScb->SCB_Mode & SCM_RSENS)) {		/* not in auto req. sense mode */
				if (pCurScb->SCB_TaStat == 2) {

					/* clr sync. nego flag */

					if (pCurScb->SCB_Flags & SCF_SENSE) {
						BYTE len;
						len = pCurScb->SCB_SenseLen;
						if (len == 0)
							len = 1;
						pCurScb->SCB_BufLen = pCurScb->SCB_SenseLen;
						pCurScb->SCB_BufPtr = pCurScb->SCB_SensePtr;
						pCurScb->SCB_Flags &= ~(SCF_SG | SCF_DIR);	/* for xfer_data_in */
/*                      pCurScb->SCB_Flags |= SCF_NO_DCHK;      */
						/* so, we won't report worng direction in xfer_data_in,
						   and won't report HOST_DO_DU in state_6 */
						pCurScb->SCB_Mode = SCM_RSENS;
						pCurScb->SCB_Ident &= 0xBF;	/* Disable Disconnect */
						pCurScb->SCB_TagMsg = 0;
						pCurScb->SCB_TaStat = 0;
						pCurScb->SCB_CDBLen = 6;
						pCurScb->SCB_CDB[0] = SCSICMD_RequestSense;
						pCurScb->SCB_CDB[1] = 0;
						pCurScb->SCB_CDB[2] = 0;
						pCurScb->SCB_CDB[3] = 0;
						pCurScb->SCB_CDB[4] = len;
						pCurScb->SCB_CDB[5] = 0;
						tul_push_pend_scb(pCurHcb, pCurScb);
						break;
					}
				}
			} else {	/* in request sense mode */

				if (pCurScb->SCB_TaStat == 2) {		/* check contition status again after sending
									   requset sense cmd 0x3 */
					pCurScb->SCB_HaStat = HOST_BAD_PHAS;
				}
				pCurScb->SCB_TaStat = 2;
			}
			pCurScb->SCB_Flags |= SCF_DONE;
			if (pCurScb->SCB_Flags & SCF_POST) {
				(*pCurScb->SCB_Post) ((BYTE *) pCurHcb, (BYTE *) pCurScb);
			}
		}		/* while */

		/* find_active: */
		if (TUL_RD(pCurHcb->HCS_Base, TUL_SStatus0) & TSS_INT_PENDING)
			continue;

		if (pCurHcb->HCS_ActScb) {	/* return to OS and wait for xfer_done_ISR/Selected_ISR */
			return 1;	/* return to OS, enable interrupt */
		}
		/* Check pending SCB            */
		if (tul_find_first_pend_scb(pCurHcb) == NULL) {
			return 1;	/* return to OS, enable interrupt */
		}
	}			/* End of for loop */
	/* statement won't reach here */
}




/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

/***************************************************************************/
void tulip_scsi(HCS * pCurHcb)
{
	SCB *pCurScb;
	TCS *pCurTcb;

	/* make sure to service interrupt asap */

	if ((pCurHcb->HCS_JSStatus0 = TUL_RD(pCurHcb->HCS_Base, TUL_SStatus0)) & TSS_INT_PENDING) {

		pCurHcb->HCS_Phase = pCurHcb->HCS_JSStatus0 & TSS_PH_MASK;
		pCurHcb->HCS_JSStatus1 = TUL_RD(pCurHcb->HCS_Base, TUL_SStatus1);
		pCurHcb->HCS_JSInt = TUL_RD(pCurHcb->HCS_Base, TUL_SInt);
		if (pCurHcb->HCS_JSInt & TSS_SCSIRST_INT) {	/* SCSI bus reset detected      */
			int_tul_scsi_rst(pCurHcb);
			return;
		}
		if (pCurHcb->HCS_JSInt & TSS_RESEL_INT) {	/* if selected/reselected interrupt */
			if (int_tul_resel(pCurHcb) == 0)
				tul_next_state(pCurHcb);
			return;
		}
		if (pCurHcb->HCS_JSInt & TSS_SEL_TIMEOUT) {
			int_tul_busfree(pCurHcb);
			return;
		}
		if (pCurHcb->HCS_JSInt & TSS_DISC_INT) {	/* BUS disconnection            */
			int_tul_busfree(pCurHcb);	/* unexpected bus free or sel timeout */
			return;
		}
		if (pCurHcb->HCS_JSInt & (TSS_FUNC_COMP | TSS_BUS_SERV)) {	/* func complete or Bus service */
			if ((pCurScb = pCurHcb->HCS_ActScb) != NULL)
				tul_next_state(pCurHcb);
			return;
		}
	}
	if (pCurHcb->HCS_ActScb != NULL)
		return;

	if ((pCurScb = tul_find_first_pend_scb(pCurHcb)) == NULL)
		return;

	/* program HBA's SCSI ID & target SCSI ID */
	TUL_WR(pCurHcb->HCS_Base + TUL_SScsiId,
	     (pCurHcb->HCS_SCSI_ID << 4) | (pCurScb->SCB_Target & 0x0F));
	if (pCurScb->SCB_Opcode == ExecSCSI) {
		pCurTcb = &pCurHcb->HCS_Tcs[pCurScb->SCB_Target];

		if (pCurScb->SCB_TagMsg)
			pCurTcb->TCS_DrvFlags |= TCF_DRV_EN_TAG;
		else
			pCurTcb->TCS_DrvFlags &= ~TCF_DRV_EN_TAG;

		TUL_WR(pCurHcb->HCS_Base + TUL_SPeriod, pCurTcb->TCS_JS_Period);
		if ((pCurTcb->TCS_Flags & (TCF_WDTR_DONE | TCF_NO_WDTR)) == 0) {	/* do wdtr negotiation          */
			tul_select_atn_stop(pCurHcb, pCurScb);
		} else {
			if ((pCurTcb->TCS_Flags & (TCF_SYNC_DONE | TCF_NO_SYNC_NEGO)) == 0) {	/* do sync negotiation          */
				tul_select_atn_stop(pCurHcb, pCurScb);
			} else {
				if (pCurScb->SCB_TagMsg)
					tul_select_atn3(pCurHcb, pCurScb);
				else
					tul_select_atn(pCurHcb, pCurScb);
			}
		}
		if (pCurScb->SCB_Flags & SCF_POLL) {
			while (wait_tulip(pCurHcb) != -1) {
				if (tul_next_state(pCurHcb) == -1)
					break;
			}
		}
	} else if (pCurScb->SCB_Opcode == BusDevRst) {
		tul_select_atn_stop(pCurHcb, pCurScb);
		pCurScb->SCB_NxtStat = 8;
		if (pCurScb->SCB_Flags & SCF_POLL) {
			while (wait_tulip(pCurHcb) != -1) {
				if (tul_next_state(pCurHcb) == -1)
					break;
			}
		}
	} else if (pCurScb->SCB_Opcode == AbortCmd) {
		ULONG srbp;

		srbp = (ULONG) pCurScb->SCB_Srb;
/* 08/03/98 */
		if (tul_abort_srb(pCurHcb, srbp) != 0) {


			tul_unlink_pend_scb(pCurHcb, pCurScb);

			tul_release_scb(pCurHcb, pCurScb);
		} else {
			pCurScb->SCB_Opcode = BusDevRst;
			tul_select_atn_stop(pCurHcb, pCurScb);
			pCurScb->SCB_NxtStat = 8;
		}

/* 08/03/98 */
	} else {
		tul_unlink_pend_scb(pCurHcb, pCurScb);
		pCurScb->SCB_HaStat = 0x16;	/* bad command */
		tul_append_done_scb(pCurHcb, pCurScb);
	}
	return;
}


/***************************************************************************/
int tul_next_state(HCS * pCurHcb)
{
	int next;

	next = pCurHcb->HCS_ActScb->SCB_NxtStat;
	for (;;) {
		switch (next) {
		case 1:
			next = tul_state_1(pCurHcb);
			break;
		case 2:
			next = tul_state_2(pCurHcb);
			break;
		case 3:
			next = tul_state_3(pCurHcb);
			break;
		case 4:
			next = tul_state_4(pCurHcb);
			break;
		case 5:
			next = tul_state_5(pCurHcb);
			break;
		case 6:
			next = tul_state_6(pCurHcb);
			break;
		case 7:
			next = tul_state_7(pCurHcb);
			break;
		case 8:
			return (tul_bus_device_reset(pCurHcb));
		default:
			return (tul_bad_seq(pCurHcb));
		}
		if (next <= 0)
			return next;
	}
}


/***************************************************************************/
/* sTate after selection with attention & stop */
int tul_state_1(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;
	TCS *pCurTcb = pCurHcb->HCS_ActTcs;
#if DEBUG_STATE
	printk("-s1-");
#endif

	tul_unlink_pend_scb(pCurHcb, pCurScb);
	tul_append_busy_scb(pCurHcb, pCurScb);

	TUL_WR(pCurHcb->HCS_Base + TUL_SConfig, pCurTcb->TCS_SConfig0);
	/* ATN on */
	if (pCurHcb->HCS_Phase == MSG_OUT) {

		TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl1, (TSC_EN_BUS_IN | TSC_HW_RESELECT));

		TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurScb->SCB_Ident);

		if (pCurScb->SCB_TagMsg) {
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurScb->SCB_TagMsg);
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurScb->SCB_TagId);
		}
		if ((pCurTcb->TCS_Flags & (TCF_WDTR_DONE | TCF_NO_WDTR)) == 0) {

			pCurTcb->TCS_Flags |= TCF_WDTR_DONE;

			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_EXTEND);
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 2);	/* Extended msg length */
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 3);	/* Sync request */
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 1);	/* Start from 16 bits */
		} else if ((pCurTcb->TCS_Flags & (TCF_SYNC_DONE | TCF_NO_SYNC_NEGO)) == 0) {

			pCurTcb->TCS_Flags |= TCF_SYNC_DONE;

			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_EXTEND);
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 3);	/* extended msg length */
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 1);	/* sync request */
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, tul_rate_tbl[pCurTcb->TCS_Flags & TCF_SCSI_RATE]);
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MAX_OFFSET);	/* REQ/ACK offset */
		}
		TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
		if (wait_tulip(pCurHcb) == -1)
			return (-1);
	}
	TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);
	TUL_WR(pCurHcb->HCS_Base + TUL_SSignal, (TUL_RD(pCurHcb->HCS_Base, TUL_SSignal) & (TSC_SET_ACK | 7)));
	return (3);
}


/***************************************************************************/
/* state after selection with attention */
/* state after selection with attention3 */
int tul_state_2(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;
	TCS *pCurTcb = pCurHcb->HCS_ActTcs;
#if DEBUG_STATE
	printk("-s2-");
#endif

	tul_unlink_pend_scb(pCurHcb, pCurScb);
	tul_append_busy_scb(pCurHcb, pCurScb);

	TUL_WR(pCurHcb->HCS_Base + TUL_SConfig, pCurTcb->TCS_SConfig0);

	if (pCurHcb->HCS_JSStatus1 & TSS_CMD_PH_CMP) {
		return (4);
	}
	TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);
	TUL_WR(pCurHcb->HCS_Base + TUL_SSignal, (TUL_RD(pCurHcb->HCS_Base, TUL_SSignal) & (TSC_SET_ACK | 7)));
	return (3);
}

/***************************************************************************/
/* state before CDB xfer is done */
int tul_state_3(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;
	TCS *pCurTcb = pCurHcb->HCS_ActTcs;
	int i;

#if DEBUG_STATE
	printk("-s3-");
#endif
	for (;;) {
		switch (pCurHcb->HCS_Phase) {
		case CMD_OUT:	/* Command out phase            */
			for (i = 0; i < (int) pCurScb->SCB_CDBLen; i++)
				TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurScb->SCB_CDB[i]);
			TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
			if (wait_tulip(pCurHcb) == -1)
				return (-1);
			if (pCurHcb->HCS_Phase == CMD_OUT) {
				return (tul_bad_seq(pCurHcb));
			}
			return (4);

		case MSG_IN:	/* Message in phase             */
			pCurScb->SCB_NxtStat = 3;
			if (tul_msgin(pCurHcb) == -1)
				return (-1);
			break;

		case STATUS_IN:	/* Status phase                 */
			if (tul_status_msg(pCurHcb) == -1)
				return (-1);
			break;

		case MSG_OUT:	/* Message out phase            */
			if (pCurTcb->TCS_Flags & (TCF_SYNC_DONE | TCF_NO_SYNC_NEGO)) {

				TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_NOP);		/* msg nop */
				TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
				if (wait_tulip(pCurHcb) == -1)
					return (-1);

			} else {
				pCurTcb->TCS_Flags |= TCF_SYNC_DONE;

				TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_EXTEND);
				TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 3);	/* ext. msg len */
				TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 1);	/* sync request */
				TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, tul_rate_tbl[pCurTcb->TCS_Flags & TCF_SCSI_RATE]);
				TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MAX_OFFSET);	/* REQ/ACK offset */
				TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
				if (wait_tulip(pCurHcb) == -1)
					return (-1);
				TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);
				TUL_WR(pCurHcb->HCS_Base + TUL_SSignal, TUL_RD(pCurHcb->HCS_Base, TUL_SSignal) & (TSC_SET_ACK | 7));

			}
			break;

		default:
			return (tul_bad_seq(pCurHcb));
		}
	}
}


/***************************************************************************/
int tul_state_4(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;

#if DEBUG_STATE
	printk("-s4-");
#endif
	if ((pCurScb->SCB_Flags & SCF_DIR) == SCF_NO_XF) {
		return (6);	/* Go to state 6                */
	}
	for (;;) {
		if (pCurScb->SCB_BufLen == 0)
			return (6);	/* Go to state 6                */

		switch (pCurHcb->HCS_Phase) {

		case STATUS_IN:	/* Status phase                 */
			if ((pCurScb->SCB_Flags & SCF_DIR) != 0) {	/* if direction bit set then report data underrun */
				pCurScb->SCB_HaStat = HOST_DO_DU;
			}
			if ((tul_status_msg(pCurHcb)) == -1)
				return (-1);
			break;

		case MSG_IN:	/* Message in phase             */
			pCurScb->SCB_NxtStat = 0x4;
			if (tul_msgin(pCurHcb) == -1)
				return (-1);
			break;

		case MSG_OUT:	/* Message out phase            */
			if (pCurHcb->HCS_JSStatus0 & TSS_PAR_ERROR) {
				pCurScb->SCB_BufLen = 0;
				pCurScb->SCB_HaStat = HOST_DO_DU;
				if (tul_msgout_ide(pCurHcb) == -1)
					return (-1);
				return (6);	/* Go to state 6                */
			} else {
				TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_NOP);		/* msg nop */
				TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
				if (wait_tulip(pCurHcb) == -1)
					return (-1);
			}
			break;

		case DATA_IN:	/* Data in phase                */
			return (tul_xfer_data_in(pCurHcb));

		case DATA_OUT:	/* Data out phase               */
			return (tul_xfer_data_out(pCurHcb));

		default:
			return (tul_bad_seq(pCurHcb));
		}
	}
}


/***************************************************************************/
/* state after dma xfer done or phase change before xfer done */
int tul_state_5(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;
	long cnt, xcnt;		/* cannot use unsigned !! code: if (xcnt < 0) */

#if DEBUG_STATE
	printk("-s5-");
#endif
/*------ get remaining count -------*/

	cnt = TUL_RDLONG(pCurHcb->HCS_Base, TUL_SCnt0) & 0x0FFFFFF;

	if (TUL_RD(pCurHcb->HCS_Base, TUL_XCmd) & 0x20) {
		/* ----------------------- DATA_IN ----------------------------- */
		/* check scsi parity error */
		if (pCurHcb->HCS_JSStatus0 & TSS_PAR_ERROR) {
			pCurScb->SCB_HaStat = HOST_DO_DU;
		}
		if (TUL_RD(pCurHcb->HCS_Base, TUL_XStatus) & XPEND) {	/* DMA xfer pending, Send STOP  */
			/* tell Hardware  scsi xfer has been terminated */
			TUL_WR(pCurHcb->HCS_Base + TUL_XCtrl, TUL_RD(pCurHcb->HCS_Base, TUL_XCtrl) | 0x80);
			/* wait until DMA xfer not pending */
			while (TUL_RD(pCurHcb->HCS_Base, TUL_XStatus) & XPEND);
		}
	} else {
/*-------- DATA OUT -----------*/
		if ((TUL_RD(pCurHcb->HCS_Base, TUL_SStatus1) & TSS_XFER_CMP) == 0) {
			if (pCurHcb->HCS_ActTcs->TCS_JS_Period & TSC_WIDE_SCSI)
				cnt += (TUL_RD(pCurHcb->HCS_Base, TUL_SFifoCnt) & 0x1F) << 1;
			else
				cnt += (TUL_RD(pCurHcb->HCS_Base, TUL_SFifoCnt) & 0x1F);
		}
		if (TUL_RD(pCurHcb->HCS_Base, TUL_XStatus) & XPEND) {	/* if DMA xfer is pending, abort DMA xfer */
			TUL_WR(pCurHcb->HCS_Base + TUL_XCmd, TAX_X_ABT);
			/* wait Abort DMA xfer done */
			while ((TUL_RD(pCurHcb->HCS_Base, TUL_Int) & XABT) == 0);
		}
		if ((cnt == 1) && (pCurHcb->HCS_Phase == DATA_OUT)) {
			TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
			if (wait_tulip(pCurHcb) == -1) {
				return (-1);
			}
			cnt = 0;
		} else {
			if ((TUL_RD(pCurHcb->HCS_Base, TUL_SStatus1) & TSS_XFER_CMP) == 0)
				TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);
		}
	}

	if (cnt == 0) {
		pCurScb->SCB_BufLen = 0;
		return (6);	/* Go to state 6                */
	}
	/* Update active data pointer */
	xcnt = (long) pCurScb->SCB_BufLen - cnt;	/* xcnt== bytes already xferred */
	pCurScb->SCB_BufLen = (U32) cnt;	/* cnt == bytes left to be xferred */
	if (pCurScb->SCB_Flags & SCF_SG) {
		register SG *sgp;
		ULONG i;

		sgp = &pCurScb->SCB_SGList[pCurScb->SCB_SGIdx];
		for (i = pCurScb->SCB_SGIdx; i < pCurScb->SCB_SGMax; sgp++, i++) {
			xcnt -= (long) sgp->SG_Len;
			if (xcnt < 0) {		/* this sgp xfer half done */
				xcnt += (long) sgp->SG_Len;	/* xcnt == bytes xferred in this sgp */
				sgp->SG_Ptr += (U32) xcnt;	/* new ptr to be xfer */
				sgp->SG_Len -= (U32) xcnt;	/* new len to be xfer */
				pCurScb->SCB_BufPtr += ((U32) (i - pCurScb->SCB_SGIdx) << 3);
				/* new SG table ptr */
				pCurScb->SCB_SGLen = (BYTE) (pCurScb->SCB_SGMax - i);
				/* new SG table len */
				pCurScb->SCB_SGIdx = (WORD) i;
				/* for next disc and come in this loop */
				return (4);	/* Go to state 4                */
			}
			/* else (xcnt >= 0 , i.e. this sgp already xferred */
		}		/* for */
		return (6);	/* Go to state 6                */
	} else {
		pCurScb->SCB_BufPtr += (U32) xcnt;
	}
	return (4);		/* Go to state 4                */
}

/***************************************************************************/
/* state after Data phase */
int tul_state_6(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;

#if DEBUG_STATE
	printk("-s6-");
#endif
	for (;;) {
		switch (pCurHcb->HCS_Phase) {
		case STATUS_IN:	/* Status phase                 */
			if ((tul_status_msg(pCurHcb)) == -1)
				return (-1);
			break;

		case MSG_IN:	/* Message in phase             */
			pCurScb->SCB_NxtStat = 6;
			if ((tul_msgin(pCurHcb)) == -1)
				return (-1);
			break;

		case MSG_OUT:	/* Message out phase            */
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_NOP);		/* msg nop */
			TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
			if (wait_tulip(pCurHcb) == -1)
				return (-1);
			break;

		case DATA_IN:	/* Data in phase                */
			return (tul_xpad_in(pCurHcb));

		case DATA_OUT:	/* Data out phase               */
			return (tul_xpad_out(pCurHcb));

		default:
			return (tul_bad_seq(pCurHcb));
		}
	}
}

/***************************************************************************/
int tul_state_7(HCS * pCurHcb)
{
	int cnt, i;

#if DEBUG_STATE
	printk("-s7-");
#endif
	/* flush SCSI FIFO */
	cnt = TUL_RD(pCurHcb->HCS_Base, TUL_SFifoCnt) & 0x1F;
	if (cnt) {
		for (i = 0; i < cnt; i++)
			TUL_RD(pCurHcb->HCS_Base, TUL_SFifo);
	}
	switch (pCurHcb->HCS_Phase) {
	case DATA_IN:		/* Data in phase                */
	case DATA_OUT:		/* Data out phase               */
		return (tul_bad_seq(pCurHcb));
	default:
		return (6);	/* Go to state 6                */
	}
}

/***************************************************************************/
int tul_xfer_data_in(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;

	if ((pCurScb->SCB_Flags & SCF_DIR) == SCF_DOUT) {
		return (6);	/* wrong direction */
	}
	TUL_WRLONG(pCurHcb->HCS_Base + TUL_SCnt0, pCurScb->SCB_BufLen);

	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_DMA_IN);	/* 7/25/95 */

	if (pCurScb->SCB_Flags & SCF_SG) {	/* S/G xfer */
		TUL_WRLONG(pCurHcb->HCS_Base + TUL_XCntH, ((ULONG) pCurScb->SCB_SGLen) << 3);
		TUL_WRLONG(pCurHcb->HCS_Base + TUL_XAddH, pCurScb->SCB_BufPtr);
		TUL_WR(pCurHcb->HCS_Base + TUL_XCmd, TAX_SG_IN);
	} else {
		TUL_WRLONG(pCurHcb->HCS_Base + TUL_XCntH, pCurScb->SCB_BufLen);
		TUL_WRLONG(pCurHcb->HCS_Base + TUL_XAddH, pCurScb->SCB_BufPtr);
		TUL_WR(pCurHcb->HCS_Base + TUL_XCmd, TAX_X_IN);
	}
	pCurScb->SCB_NxtStat = 0x5;
	return (0);		/* return to OS, wait xfer done , let jas_isr come in */
}


/***************************************************************************/
int tul_xfer_data_out(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;

	if ((pCurScb->SCB_Flags & SCF_DIR) == SCF_DIN) {
		return (6);	/* wrong direction */
	}
	TUL_WRLONG(pCurHcb->HCS_Base + TUL_SCnt0, pCurScb->SCB_BufLen);
	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_DMA_OUT);

	if (pCurScb->SCB_Flags & SCF_SG) {	/* S/G xfer */
		TUL_WRLONG(pCurHcb->HCS_Base + TUL_XCntH, ((ULONG) pCurScb->SCB_SGLen) << 3);
		TUL_WRLONG(pCurHcb->HCS_Base + TUL_XAddH, pCurScb->SCB_BufPtr);
		TUL_WR(pCurHcb->HCS_Base + TUL_XCmd, TAX_SG_OUT);
	} else {
		TUL_WRLONG(pCurHcb->HCS_Base + TUL_XCntH, pCurScb->SCB_BufLen);
		TUL_WRLONG(pCurHcb->HCS_Base + TUL_XAddH, pCurScb->SCB_BufPtr);
		TUL_WR(pCurHcb->HCS_Base + TUL_XCmd, TAX_X_OUT);
	}

	pCurScb->SCB_NxtStat = 0x5;
	return (0);		/* return to OS, wait xfer done , let jas_isr come in */
}


/***************************************************************************/
int tul_xpad_in(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;
	TCS *pCurTcb = pCurHcb->HCS_ActTcs;

	if ((pCurScb->SCB_Flags & SCF_DIR) != SCF_NO_DCHK) {
		pCurScb->SCB_HaStat = HOST_DO_DU;	/* over run             */
	}
	for (;;) {
		if (pCurTcb->TCS_JS_Period & TSC_WIDE_SCSI)
			TUL_WRLONG(pCurHcb->HCS_Base + TUL_SCnt0, 2);
		else
			TUL_WRLONG(pCurHcb->HCS_Base + TUL_SCnt0, 1);

		TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_IN);
		if ((wait_tulip(pCurHcb)) == -1) {
			return (-1);
		}
		if (pCurHcb->HCS_Phase != DATA_IN) {
			TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);
			return (6);
		}
		TUL_RD(pCurHcb->HCS_Base, TUL_SFifo);
	}
}

int tul_xpad_out(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;
	TCS *pCurTcb = pCurHcb->HCS_ActTcs;

	if ((pCurScb->SCB_Flags & SCF_DIR) != SCF_NO_DCHK) {
		pCurScb->SCB_HaStat = HOST_DO_DU;	/* over run             */
	}
	for (;;) {
		if (pCurTcb->TCS_JS_Period & TSC_WIDE_SCSI)
			TUL_WRLONG(pCurHcb->HCS_Base + TUL_SCnt0, 2);
		else
			TUL_WRLONG(pCurHcb->HCS_Base + TUL_SCnt0, 1);

		TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 0);
		TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
		if ((wait_tulip(pCurHcb)) == -1) {
			return (-1);
		}
		if (pCurHcb->HCS_Phase != DATA_OUT) {	/* Disable wide CPU to allow read 16 bits */
			TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl1, TSC_HW_RESELECT);
			TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);
			return (6);
		}
	}
}


/***************************************************************************/
int tul_status_msg(HCS * pCurHcb)
{				/* status & MSG_IN */
	SCB *pCurScb = pCurHcb->HCS_ActScb;
	BYTE msg;

	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_CMD_COMP);
	if ((wait_tulip(pCurHcb)) == -1) {
		return (-1);
	}
	/* get status */
	pCurScb->SCB_TaStat = TUL_RD(pCurHcb->HCS_Base, TUL_SFifo);

	if (pCurHcb->HCS_Phase == MSG_OUT) {
		if (pCurHcb->HCS_JSStatus0 & TSS_PAR_ERROR) {
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_PARITY);
		} else {
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_NOP);
		}
		TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
		return (wait_tulip(pCurHcb));
	}
	if (pCurHcb->HCS_Phase == MSG_IN) {
		msg = TUL_RD(pCurHcb->HCS_Base, TUL_SFifo);
		if (pCurHcb->HCS_JSStatus0 & TSS_PAR_ERROR) {	/* Parity error                 */
			if ((tul_msgin_accept(pCurHcb)) == -1)
				return (-1);
			if (pCurHcb->HCS_Phase != MSG_OUT)
				return (tul_bad_seq(pCurHcb));
			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_PARITY);
			TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
			return (wait_tulip(pCurHcb));
		}
		if (msg == 0) {	/* Command complete             */

			if ((pCurScb->SCB_TaStat & 0x18) == 0x10) {	/* No link support              */
				return (tul_bad_seq(pCurHcb));
			}
			TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);
			TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_MSG_ACCEPT);
			return tul_wait_done_disc(pCurHcb);

		}
		if ((msg == MSG_LINK_COMP) || (msg == MSG_LINK_FLAG)) {
			if ((pCurScb->SCB_TaStat & 0x18) == 0x10)
				return (tul_msgin_accept(pCurHcb));
		}
	}
	return (tul_bad_seq(pCurHcb));
}


/***************************************************************************/
/* scsi bus free */
int int_tul_busfree(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;

	if (pCurScb != NULL) {
		if (pCurScb->SCB_Status & SCB_SELECT) {		/* selection timeout */
			tul_unlink_pend_scb(pCurHcb, pCurScb);
			pCurScb->SCB_HaStat = HOST_SEL_TOUT;
			tul_append_done_scb(pCurHcb, pCurScb);
		} else {	/* Unexpected bus free          */
			tul_unlink_busy_scb(pCurHcb, pCurScb);
			pCurScb->SCB_HaStat = HOST_BUS_FREE;
			tul_append_done_scb(pCurHcb, pCurScb);
		}
		pCurHcb->HCS_ActScb = NULL;
		pCurHcb->HCS_ActTcs = NULL;
	}
	TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);		/* Flush SCSI FIFO  */
	TUL_WR(pCurHcb->HCS_Base + TUL_SConfig, TSC_INITDEFAULT);
	TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl1, TSC_HW_RESELECT);	/* Enable HW reselect       */
	return (-1);
}


/***************************************************************************/
/* scsi bus reset */
int int_tul_scsi_rst(HCS * pCurHcb)
{
	SCB *pCurScb;
	int i;

	/* if DMA xfer is pending, abort DMA xfer */
	if (TUL_RD(pCurHcb->HCS_Base, TUL_XStatus) & 0x01) {
		TUL_WR(pCurHcb->HCS_Base + TUL_XCmd, TAX_X_ABT | TAX_X_CLR_FIFO);
		/* wait Abort DMA xfer done */
		while ((TUL_RD(pCurHcb->HCS_Base, TUL_Int) & 0x04) == 0);
		TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);
	}
	/* Abort all active & disconnected scb */
	while ((pCurScb = tul_pop_busy_scb(pCurHcb)) != NULL) {
		pCurScb->SCB_HaStat = HOST_BAD_PHAS;
		tul_append_done_scb(pCurHcb, pCurScb);
	}
	pCurHcb->HCS_ActScb = NULL;
	pCurHcb->HCS_ActTcs = NULL;

	/* clr sync nego. done flag */
	for (i = 0; i < pCurHcb->HCS_MaxTar; i++) {
		pCurHcb->HCS_Tcs[i].TCS_Flags &= ~(TCF_SYNC_DONE | TCF_WDTR_DONE);
	}
	return (-1);
}


/***************************************************************************/
/* scsi reselection */
int int_tul_resel(HCS * pCurHcb)
{
	SCB *pCurScb;
	TCS *pCurTcb;
	BYTE tag, msg = 0;
	BYTE tar, lun;

	if ((pCurScb = pCurHcb->HCS_ActScb) != NULL) {
		if (pCurScb->SCB_Status & SCB_SELECT) {		/* if waiting for selection complete */
			pCurScb->SCB_Status &= ~SCB_SELECT;
		}
		pCurHcb->HCS_ActScb = NULL;
	}
	/* --------- get target id---------------------- */
	tar = TUL_RD(pCurHcb->HCS_Base, TUL_SBusId);
	/* ------ get LUN from Identify message----------- */
	lun = TUL_RD(pCurHcb->HCS_Base, TUL_SIdent) & 0x0F;
	/* 07/22/98 from 0x1F -> 0x0F */
	pCurTcb = &pCurHcb->HCS_Tcs[tar];
	pCurHcb->HCS_ActTcs = pCurTcb;
	TUL_WR(pCurHcb->HCS_Base + TUL_SConfig, pCurTcb->TCS_SConfig0);
	TUL_WR(pCurHcb->HCS_Base + TUL_SPeriod, pCurTcb->TCS_JS_Period);


	/* ------------- tag queueing ? ------------------- */
	if (pCurTcb->TCS_DrvFlags & TCF_DRV_EN_TAG) {
		if ((tul_msgin_accept(pCurHcb)) == -1)
			return (-1);
		if (pCurHcb->HCS_Phase != MSG_IN)
			goto no_tag;
		TUL_WRLONG(pCurHcb->HCS_Base + TUL_SCnt0, 1);
		TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_IN);
		if ((wait_tulip(pCurHcb)) == -1)
			return (-1);
		msg = TUL_RD(pCurHcb->HCS_Base, TUL_SFifo);	/* Read Tag Message    */

		if ((msg < MSG_STAG) || (msg > MSG_OTAG))	/* Is simple Tag      */
			goto no_tag;

		if ((tul_msgin_accept(pCurHcb)) == -1)
			return (-1);

		if (pCurHcb->HCS_Phase != MSG_IN)
			goto no_tag;

		TUL_WRLONG(pCurHcb->HCS_Base + TUL_SCnt0, 1);
		TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_IN);
		if ((wait_tulip(pCurHcb)) == -1)
			return (-1);
		tag = TUL_RD(pCurHcb->HCS_Base, TUL_SFifo);	/* Read Tag ID       */
		pCurScb = pCurHcb->HCS_Scb + tag;
		if ((pCurScb->SCB_Target != tar) || (pCurScb->SCB_Lun != lun)) {
			return tul_msgout_abort_tag(pCurHcb);
		}
		if (pCurScb->SCB_Status != SCB_BUSY) {	/* 03/24/95             */
			return tul_msgout_abort_tag(pCurHcb);
		}
		pCurHcb->HCS_ActScb = pCurScb;
		if ((tul_msgin_accept(pCurHcb)) == -1)
			return (-1);
	} else {		/* No tag               */
	      no_tag:
		if ((pCurScb = tul_find_busy_scb(pCurHcb, tar | (lun << 8))) == NULL) {
			return tul_msgout_abort_targ(pCurHcb);
		}
		pCurHcb->HCS_ActScb = pCurScb;
		if (!(pCurTcb->TCS_DrvFlags & TCF_DRV_EN_TAG)) {
			if ((tul_msgin_accept(pCurHcb)) == -1)
				return (-1);
		}
	}
	return 0;
}


/***************************************************************************/
int int_tul_bad_seq(HCS * pCurHcb)
{				/* target wrong phase           */
	SCB *pCurScb;
	int i;

	tul_reset_scsi(pCurHcb, 10);

	while ((pCurScb = tul_pop_busy_scb(pCurHcb)) != NULL) {
		pCurScb->SCB_HaStat = HOST_BAD_PHAS;
		tul_append_done_scb(pCurHcb, pCurScb);
	}
	for (i = 0; i < pCurHcb->HCS_MaxTar; i++) {
		pCurHcb->HCS_Tcs[i].TCS_Flags &= ~(TCF_SYNC_DONE | TCF_WDTR_DONE);;
	}
	return (-1);
}


/***************************************************************************/
int tul_msgout_abort_targ(HCS * pCurHcb)
{

	TUL_WR(pCurHcb->HCS_Base + TUL_SSignal, ((TUL_RD(pCurHcb->HCS_Base, TUL_SSignal) & (TSC_SET_ACK | 7)) | TSC_SET_ATN));
	if (tul_msgin_accept(pCurHcb) == -1)
		return (-1);
	if (pCurHcb->HCS_Phase != MSG_OUT)
		return (tul_bad_seq(pCurHcb));

	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_ABORT);
	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);

	return tul_wait_disc(pCurHcb);
}

/***************************************************************************/
int tul_msgout_abort_tag(HCS * pCurHcb)
{

	TUL_WR(pCurHcb->HCS_Base + TUL_SSignal, ((TUL_RD(pCurHcb->HCS_Base, TUL_SSignal) & (TSC_SET_ACK | 7)) | TSC_SET_ATN));
	if (tul_msgin_accept(pCurHcb) == -1)
		return (-1);
	if (pCurHcb->HCS_Phase != MSG_OUT)
		return (tul_bad_seq(pCurHcb));

	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_ABORT_TAG);
	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);

	return tul_wait_disc(pCurHcb);

}

/***************************************************************************/
int tul_msgin(HCS * pCurHcb)
{
	TCS *pCurTcb;

	for (;;) {

		TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);

		TUL_WRLONG(pCurHcb->HCS_Base + TUL_SCnt0, 1);
		TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_IN);
		if ((wait_tulip(pCurHcb)) == -1)
			return (-1);

		switch (TUL_RD(pCurHcb->HCS_Base, TUL_SFifo)) {
		case MSG_DISC:	/* Disconnect msg */
			TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_MSG_ACCEPT);

			return tul_wait_disc(pCurHcb);

		case MSG_SDP:
		case MSG_RESTORE:
		case MSG_NOP:
			tul_msgin_accept(pCurHcb);
			break;

		case MSG_REJ:	/* Clear ATN first              */
			TUL_WR(pCurHcb->HCS_Base + TUL_SSignal,
			       (TUL_RD(pCurHcb->HCS_Base, TUL_SSignal) & (TSC_SET_ACK | 7)));
			pCurTcb = pCurHcb->HCS_ActTcs;
			if ((pCurTcb->TCS_Flags & (TCF_SYNC_DONE | TCF_NO_SYNC_NEGO)) == 0) {	/* do sync nego */
				TUL_WR(pCurHcb->HCS_Base + TUL_SSignal, ((TUL_RD(pCurHcb->HCS_Base, TUL_SSignal) & (TSC_SET_ACK | 7)) | TSC_SET_ATN));
			}
			tul_msgin_accept(pCurHcb);
			break;

		case MSG_EXTEND:	/* extended msg */
			tul_msgin_extend(pCurHcb);
			break;

		case MSG_IGNOREWIDE:
			tul_msgin_accept(pCurHcb);
			break;

			/* get */
			TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_IN);
			if (wait_tulip(pCurHcb) == -1)
				return -1;

			TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 0);	/* put pad  */
			TUL_RD(pCurHcb->HCS_Base, TUL_SFifo);	/* get IGNORE field */
			TUL_RD(pCurHcb->HCS_Base, TUL_SFifo);	/* get pad */

			tul_msgin_accept(pCurHcb);
			break;

		case MSG_COMP:
			{
				TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);
				TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_MSG_ACCEPT);
				return tul_wait_done_disc(pCurHcb);
			}
		default:
			tul_msgout_reject(pCurHcb);
			break;
		}
		if (pCurHcb->HCS_Phase != MSG_IN)
			return (pCurHcb->HCS_Phase);
	}
	/* statement won't reach here */
}




/***************************************************************************/
int tul_msgout_reject(HCS * pCurHcb)
{

	TUL_WR(pCurHcb->HCS_Base + TUL_SSignal, ((TUL_RD(pCurHcb->HCS_Base, TUL_SSignal) & (TSC_SET_ACK | 7)) | TSC_SET_ATN));

	if ((tul_msgin_accept(pCurHcb)) == -1)
		return (-1);

	if (pCurHcb->HCS_Phase == MSG_OUT) {
		TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_REJ);		/* Msg reject           */
		TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
		return (wait_tulip(pCurHcb));
	}
	return (pCurHcb->HCS_Phase);
}



/***************************************************************************/
int tul_msgout_ide(HCS * pCurHcb)
{
	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_IDE);		/* Initiator Detected Error */
	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
	return (wait_tulip(pCurHcb));
}


/***************************************************************************/
int tul_msgin_extend(HCS * pCurHcb)
{
	BYTE len, idx;

	if (tul_msgin_accept(pCurHcb) != MSG_IN)
		return (pCurHcb->HCS_Phase);

	/* Get extended msg length      */
	TUL_WRLONG(pCurHcb->HCS_Base + TUL_SCnt0, 1);
	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_IN);
	if (wait_tulip(pCurHcb) == -1)
		return (-1);

	len = TUL_RD(pCurHcb->HCS_Base, TUL_SFifo);
	pCurHcb->HCS_Msg[0] = len;
	for (idx = 1; len != 0; len--) {

		if ((tul_msgin_accept(pCurHcb)) != MSG_IN)
			return (pCurHcb->HCS_Phase);
		TUL_WRLONG(pCurHcb->HCS_Base + TUL_SCnt0, 1);
		TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_IN);
		if (wait_tulip(pCurHcb) == -1)
			return (-1);
		pCurHcb->HCS_Msg[idx++] = TUL_RD(pCurHcb->HCS_Base, TUL_SFifo);
	}
	if (pCurHcb->HCS_Msg[1] == 1) {		/* if it's synchronous data transfer request */
		if (pCurHcb->HCS_Msg[0] != 3)	/* if length is not right */
			return (tul_msgout_reject(pCurHcb));
		if (pCurHcb->HCS_ActTcs->TCS_Flags & TCF_NO_SYNC_NEGO) {	/* Set OFFSET=0 to do async, nego back */
			pCurHcb->HCS_Msg[3] = 0;
		} else {
			if ((tul_msgin_sync(pCurHcb) == 0) &&
			    (pCurHcb->HCS_ActTcs->TCS_Flags & TCF_SYNC_DONE)) {
				tul_sync_done(pCurHcb);
				return (tul_msgin_accept(pCurHcb));
			}
		}

		TUL_WR(pCurHcb->HCS_Base + TUL_SSignal, ((TUL_RD(pCurHcb->HCS_Base, TUL_SSignal) & (TSC_SET_ACK | 7)) | TSC_SET_ATN));
		if ((tul_msgin_accept(pCurHcb)) != MSG_OUT)
			return (pCurHcb->HCS_Phase);
		/* sync msg out */
		TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);

		tul_sync_done(pCurHcb);

		TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_EXTEND);
		TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 3);
		TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 1);
		TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurHcb->HCS_Msg[2]);
		TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurHcb->HCS_Msg[3]);

		TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
		return (wait_tulip(pCurHcb));
	}
	if ((pCurHcb->HCS_Msg[0] != 2) || (pCurHcb->HCS_Msg[1] != 3))
		return (tul_msgout_reject(pCurHcb));
	/* if it's WIDE DATA XFER REQ   */
	if (pCurHcb->HCS_ActTcs->TCS_Flags & TCF_NO_WDTR) {
		pCurHcb->HCS_Msg[2] = 0;
	} else {
		if (pCurHcb->HCS_Msg[2] > 2)	/* > 32 bits            */
			return (tul_msgout_reject(pCurHcb));
		if (pCurHcb->HCS_Msg[2] == 2) {		/* == 32                */
			pCurHcb->HCS_Msg[2] = 1;
		} else {
			if ((pCurHcb->HCS_ActTcs->TCS_Flags & TCF_NO_WDTR) == 0) {
				wdtr_done(pCurHcb);
				if ((pCurHcb->HCS_ActTcs->TCS_Flags & (TCF_SYNC_DONE | TCF_NO_SYNC_NEGO)) == 0)
					TUL_WR(pCurHcb->HCS_Base + TUL_SSignal, ((TUL_RD(pCurHcb->HCS_Base, TUL_SSignal) & (TSC_SET_ACK | 7)) | TSC_SET_ATN));
				return (tul_msgin_accept(pCurHcb));
			}
		}
	}
	TUL_WR(pCurHcb->HCS_Base + TUL_SSignal, ((TUL_RD(pCurHcb->HCS_Base, TUL_SSignal) & (TSC_SET_ACK | 7)) | TSC_SET_ATN));

	if (tul_msgin_accept(pCurHcb) != MSG_OUT)
		return (pCurHcb->HCS_Phase);
	/* WDTR msg out                 */
	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_EXTEND);
	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 2);
	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, 3);
	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurHcb->HCS_Msg[2]);
	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);
	return (wait_tulip(pCurHcb));
}

/***************************************************************************/
int tul_msgin_sync(HCS * pCurHcb)
{
	char default_period;

	default_period = tul_rate_tbl[pCurHcb->HCS_ActTcs->TCS_Flags & TCF_SCSI_RATE];
	if (pCurHcb->HCS_Msg[3] > MAX_OFFSET) {
		pCurHcb->HCS_Msg[3] = MAX_OFFSET;
		if (pCurHcb->HCS_Msg[2] < default_period) {
			pCurHcb->HCS_Msg[2] = default_period;
			return 1;
		}
		if (pCurHcb->HCS_Msg[2] >= 59) {	/* Change to async              */
			pCurHcb->HCS_Msg[3] = 0;
		}
		return 1;
	}
	/* offset requests asynchronous transfers ? */
	if (pCurHcb->HCS_Msg[3] == 0) {
		return 0;
	}
	if (pCurHcb->HCS_Msg[2] < default_period) {
		pCurHcb->HCS_Msg[2] = default_period;
		return 1;
	}
	if (pCurHcb->HCS_Msg[2] >= 59) {
		pCurHcb->HCS_Msg[3] = 0;
		return 1;
	}
	return 0;
}


/***************************************************************************/
int wdtr_done(HCS * pCurHcb)
{
	pCurHcb->HCS_ActTcs->TCS_Flags &= ~TCF_SYNC_DONE;
	pCurHcb->HCS_ActTcs->TCS_Flags |= TCF_WDTR_DONE;

	pCurHcb->HCS_ActTcs->TCS_JS_Period = 0;
	if (pCurHcb->HCS_Msg[2]) {	/* if 16 bit */
		pCurHcb->HCS_ActTcs->TCS_JS_Period |= TSC_WIDE_SCSI;
	}
	pCurHcb->HCS_ActTcs->TCS_SConfig0 &= ~TSC_ALT_PERIOD;
	TUL_WR(pCurHcb->HCS_Base + TUL_SConfig, pCurHcb->HCS_ActTcs->TCS_SConfig0);
	TUL_WR(pCurHcb->HCS_Base + TUL_SPeriod, pCurHcb->HCS_ActTcs->TCS_JS_Period);

	return 1;
}

/***************************************************************************/
int tul_sync_done(HCS * pCurHcb)
{
	int i;

	pCurHcb->HCS_ActTcs->TCS_Flags |= TCF_SYNC_DONE;

	if (pCurHcb->HCS_Msg[3]) {
		pCurHcb->HCS_ActTcs->TCS_JS_Period |= pCurHcb->HCS_Msg[3];
		for (i = 0; i < 8; i++) {
			if (tul_rate_tbl[i] >= pCurHcb->HCS_Msg[2])	/* pick the big one */
				break;
		}
		pCurHcb->HCS_ActTcs->TCS_JS_Period |= (i << 4);
		pCurHcb->HCS_ActTcs->TCS_SConfig0 |= TSC_ALT_PERIOD;
	}
	TUL_WR(pCurHcb->HCS_Base + TUL_SConfig, pCurHcb->HCS_ActTcs->TCS_SConfig0);
	TUL_WR(pCurHcb->HCS_Base + TUL_SPeriod, pCurHcb->HCS_ActTcs->TCS_JS_Period);

	return (-1);
}


int tul_post_scsi_rst(HCS * pCurHcb)
{
	SCB *pCurScb;
	TCS *pCurTcb;
	int i;

	pCurHcb->HCS_ActScb = 0;
	pCurHcb->HCS_ActTcs = 0;
	pCurHcb->HCS_Flags = 0;

	while ((pCurScb = tul_pop_busy_scb(pCurHcb)) != NULL) {
		pCurScb->SCB_HaStat = HOST_BAD_PHAS;
		tul_append_done_scb(pCurHcb, pCurScb);
	}
	/* clear sync done flag         */
	pCurTcb = &pCurHcb->HCS_Tcs[0];
	for (i = 0; i < pCurHcb->HCS_MaxTar; pCurTcb++, i++) {
		pCurTcb->TCS_Flags &= ~(TCF_SYNC_DONE | TCF_WDTR_DONE);
		/* Initialize the sync. xfer register values to an asyn xfer */
		pCurTcb->TCS_JS_Period = 0;
		pCurTcb->TCS_SConfig0 = pCurHcb->HCS_SConf1;
		pCurHcb->HCS_ActTags[0] = 0;	/* 07/22/98 */
		pCurHcb->HCS_Tcs[i].TCS_Flags &= ~TCF_BUSY;	/* 07/22/98 */
	}			/* for */

	return (-1);
}

/***************************************************************************/
void tul_select_atn_stop(HCS * pCurHcb, SCB * pCurScb)
{
	pCurScb->SCB_Status |= SCB_SELECT;
	pCurScb->SCB_NxtStat = 0x1;
	pCurHcb->HCS_ActScb = pCurScb;
	pCurHcb->HCS_ActTcs = &pCurHcb->HCS_Tcs[pCurScb->SCB_Target];
	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_SELATNSTOP);
	return;
}


/***************************************************************************/
void tul_select_atn(HCS * pCurHcb, SCB * pCurScb)
{
	int i;

	pCurScb->SCB_Status |= SCB_SELECT;
	pCurScb->SCB_NxtStat = 0x2;

	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurScb->SCB_Ident);
	for (i = 0; i < (int) pCurScb->SCB_CDBLen; i++)
		TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurScb->SCB_CDB[i]);
	pCurHcb->HCS_ActTcs = &pCurHcb->HCS_Tcs[pCurScb->SCB_Target];
	pCurHcb->HCS_ActScb = pCurScb;
	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_SEL_ATN);
	return;
}

/***************************************************************************/
void tul_select_atn3(HCS * pCurHcb, SCB * pCurScb)
{
	int i;

	pCurScb->SCB_Status |= SCB_SELECT;
	pCurScb->SCB_NxtStat = 0x2;

	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurScb->SCB_Ident);
	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurScb->SCB_TagMsg);
	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurScb->SCB_TagId);
	for (i = 0; i < (int) pCurScb->SCB_CDBLen; i++)
		TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, pCurScb->SCB_CDB[i]);
	pCurHcb->HCS_ActTcs = &pCurHcb->HCS_Tcs[pCurScb->SCB_Target];
	pCurHcb->HCS_ActScb = pCurScb;
	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_SEL_ATN3);
	return;
}

/***************************************************************************/
/* SCSI Bus Device Reset */
int tul_bus_device_reset(HCS * pCurHcb)
{
	SCB *pCurScb = pCurHcb->HCS_ActScb;
	TCS *pCurTcb = pCurHcb->HCS_ActTcs;
	SCB *pTmpScb, *pPrevScb;
	BYTE tar;

	if (pCurHcb->HCS_Phase != MSG_OUT) {
		return (int_tul_bad_seq(pCurHcb));	/* Unexpected phase             */
	}
	tul_unlink_pend_scb(pCurHcb, pCurScb);
	tul_release_scb(pCurHcb, pCurScb);


	tar = pCurScb->SCB_Target;	/* target                       */
	pCurTcb->TCS_Flags &= ~(TCF_SYNC_DONE | TCF_WDTR_DONE | TCF_BUSY);
	/* clr sync. nego & WDTR flags  07/22/98 */

	/* abort all SCB with same target */
	pPrevScb = pTmpScb = pCurHcb->HCS_FirstBusy;	/* Check Busy queue */
	while (pTmpScb != NULL) {

		if (pTmpScb->SCB_Target == tar) {
			/* unlink it */
			if (pTmpScb == pCurHcb->HCS_FirstBusy) {
				if ((pCurHcb->HCS_FirstBusy = pTmpScb->SCB_NxtScb) == NULL)
					pCurHcb->HCS_LastBusy = NULL;
			} else {
				pPrevScb->SCB_NxtScb = pTmpScb->SCB_NxtScb;
				if (pTmpScb == pCurHcb->HCS_LastBusy)
					pCurHcb->HCS_LastBusy = pPrevScb;
			}
			pTmpScb->SCB_HaStat = HOST_ABORTED;
			tul_append_done_scb(pCurHcb, pTmpScb);
		}
		/* Previous haven't change      */
		else {
			pPrevScb = pTmpScb;
		}
		pTmpScb = pTmpScb->SCB_NxtScb;
	}

	TUL_WR(pCurHcb->HCS_Base + TUL_SFifo, MSG_DEVRST);
	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_XF_FIFO_OUT);

	return tul_wait_disc(pCurHcb);

}

/***************************************************************************/
int tul_msgin_accept(HCS * pCurHcb)
{
	TUL_WR(pCurHcb->HCS_Base + TUL_SCmd, TSC_MSG_ACCEPT);
	return (wait_tulip(pCurHcb));
}

/***************************************************************************/
int wait_tulip(HCS * pCurHcb)
{

	while (!((pCurHcb->HCS_JSStatus0 = TUL_RD(pCurHcb->HCS_Base, TUL_SStatus0))
		 & TSS_INT_PENDING));

	pCurHcb->HCS_JSInt = TUL_RD(pCurHcb->HCS_Base, TUL_SInt);
	pCurHcb->HCS_Phase = pCurHcb->HCS_JSStatus0 & TSS_PH_MASK;
	pCurHcb->HCS_JSStatus1 = TUL_RD(pCurHcb->HCS_Base, TUL_SStatus1);

	if (pCurHcb->HCS_JSInt & TSS_RESEL_INT) {	/* if SCSI bus reset detected   */
		return (int_tul_resel(pCurHcb));
	}
	if (pCurHcb->HCS_JSInt & TSS_SEL_TIMEOUT) {	/* if selected/reselected timeout interrupt */
		return (int_tul_busfree(pCurHcb));
	}
	if (pCurHcb->HCS_JSInt & TSS_SCSIRST_INT) {	/* if SCSI bus reset detected   */
		return (int_tul_scsi_rst(pCurHcb));
	}
	if (pCurHcb->HCS_JSInt & TSS_DISC_INT) {	/* BUS disconnection            */
		if (pCurHcb->HCS_Flags & HCF_EXPECT_DONE_DISC) {
			TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);		/* Flush SCSI FIFO  */
			tul_unlink_busy_scb(pCurHcb, pCurHcb->HCS_ActScb);
			pCurHcb->HCS_ActScb->SCB_HaStat = 0;
			tul_append_done_scb(pCurHcb, pCurHcb->HCS_ActScb);
			pCurHcb->HCS_ActScb = NULL;
			pCurHcb->HCS_ActTcs = NULL;
			pCurHcb->HCS_Flags &= ~HCF_EXPECT_DONE_DISC;
			TUL_WR(pCurHcb->HCS_Base + TUL_SConfig, TSC_INITDEFAULT);
			TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl1, TSC_HW_RESELECT);	/* Enable HW reselect       */
			return (-1);
		}
		if (pCurHcb->HCS_Flags & HCF_EXPECT_DISC) {
			TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);		/* Flush SCSI FIFO  */
			pCurHcb->HCS_ActScb = NULL;
			pCurHcb->HCS_ActTcs = NULL;
			pCurHcb->HCS_Flags &= ~HCF_EXPECT_DISC;
			TUL_WR(pCurHcb->HCS_Base + TUL_SConfig, TSC_INITDEFAULT);
			TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl1, TSC_HW_RESELECT);	/* Enable HW reselect       */
			return (-1);
		}
		return (int_tul_busfree(pCurHcb));
	}
	if (pCurHcb->HCS_JSInt & (TSS_FUNC_COMP | TSS_BUS_SERV)) {
		return (pCurHcb->HCS_Phase);
	}
	return (pCurHcb->HCS_Phase);
}
/***************************************************************************/
int tul_wait_disc(HCS * pCurHcb)
{

	while (!((pCurHcb->HCS_JSStatus0 = TUL_RD(pCurHcb->HCS_Base, TUL_SStatus0))
		 & TSS_INT_PENDING));


	pCurHcb->HCS_JSInt = TUL_RD(pCurHcb->HCS_Base, TUL_SInt);

	if (pCurHcb->HCS_JSInt & TSS_SCSIRST_INT) {	/* if SCSI bus reset detected   */
		return (int_tul_scsi_rst(pCurHcb));
	}
	if (pCurHcb->HCS_JSInt & TSS_DISC_INT) {	/* BUS disconnection            */
		TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);		/* Flush SCSI FIFO  */
		TUL_WR(pCurHcb->HCS_Base + TUL_SConfig, TSC_INITDEFAULT);
		TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl1, TSC_HW_RESELECT);	/* Enable HW reselect       */
		pCurHcb->HCS_ActScb = NULL;
		return (-1);
	}
	return (tul_bad_seq(pCurHcb));
}

/***************************************************************************/
int tul_wait_done_disc(HCS * pCurHcb)
{


	while (!((pCurHcb->HCS_JSStatus0 = TUL_RD(pCurHcb->HCS_Base, TUL_SStatus0))
		 & TSS_INT_PENDING));

	pCurHcb->HCS_JSInt = TUL_RD(pCurHcb->HCS_Base, TUL_SInt);


	if (pCurHcb->HCS_JSInt & TSS_SCSIRST_INT) {	/* if SCSI bus reset detected   */
		return (int_tul_scsi_rst(pCurHcb));
	}
	if (pCurHcb->HCS_JSInt & TSS_DISC_INT) {	/* BUS disconnection            */
		TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl0, TSC_FLUSH_FIFO);		/* Flush SCSI FIFO  */
		TUL_WR(pCurHcb->HCS_Base + TUL_SConfig, TSC_INITDEFAULT);
		TUL_WR(pCurHcb->HCS_Base + TUL_SCtrl1, TSC_HW_RESELECT);	/* Enable HW reselect       */
		tul_unlink_busy_scb(pCurHcb, pCurHcb->HCS_ActScb);

		tul_append_done_scb(pCurHcb, pCurHcb->HCS_ActScb);
		pCurHcb->HCS_ActScb = NULL;
		return (-1);
	}
	return (tul_bad_seq(pCurHcb));
}

/**************************** EOF *********************************/
