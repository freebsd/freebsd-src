/* 
 * Copyright (c) Comtrol Corporation <support@comtrol.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted prodived that the follwoing conditions
 * are met.
 * 1. Redistributions of source code must retain the above copyright 
 *    notive, this list of conditions and the following disclainer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials prodided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *       This product includes software developed by Comtrol Corporation.
 * 4. The name of Comtrol Corporation may not be used to endorse or 
 *    promote products derived from this software without specific 
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY COMTROL CORPORATION ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL COMTROL CORPORATION BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 
 * rp.c - for RocketPort FreeBSD
 */

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <i386/isa/isa_device.h>

#include <pci/pcivar.h>

#define ROCKET_C
#include <i386/isa/rpreg.h>
#include <i386/isa/rpvar.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static Byte_t RData[RDATASIZE] =
{
   0x00, 0x09, 0xf6, 0x82,
   0x02, 0x09, 0x86, 0xfb,
   0x04, 0x09, 0x00, 0x0a,
   0x06, 0x09, 0x01, 0x0a,
   0x08, 0x09, 0x8a, 0x13,
   0x0a, 0x09, 0xc5, 0x11,
   0x0c, 0x09, 0x86, 0x85,
   0x0e, 0x09, 0x20, 0x0a,
   0x10, 0x09, 0x21, 0x0a,
   0x12, 0x09, 0x41, 0xff,
   0x14, 0x09, 0x82, 0x00,
   0x16, 0x09, 0x82, 0x7b,
   0x18, 0x09, 0x8a, 0x7d,
   0x1a, 0x09, 0x88, 0x81,
   0x1c, 0x09, 0x86, 0x7a,
   0x1e, 0x09, 0x84, 0x81,
   0x20, 0x09, 0x82, 0x7c,
   0x22, 0x09, 0x0a, 0x0a
};

static Byte_t RRegData[RREGDATASIZE]=
{
   0x00, 0x09, 0xf6, 0x82,	       /* 00: Stop Rx processor */
   0x08, 0x09, 0x8a, 0x13,	       /* 04: Tx software flow control */
   0x0a, 0x09, 0xc5, 0x11,	       /* 08: XON char */
   0x0c, 0x09, 0x86, 0x85,	       /* 0c: XANY */
   0x12, 0x09, 0x41, 0xff,	       /* 10: Rx mask char */
   0x14, 0x09, 0x82, 0x00,	       /* 14: Compare/Ignore #0 */
   0x16, 0x09, 0x82, 0x7b,	       /* 18: Compare #1 */
   0x18, 0x09, 0x8a, 0x7d,	       /* 1c: Compare #2 */
   0x1a, 0x09, 0x88, 0x81,	       /* 20: Interrupt #1 */
   0x1c, 0x09, 0x86, 0x7a,	       /* 24: Ignore/Replace #1 */
   0x1e, 0x09, 0x84, 0x81,	       /* 28: Interrupt #2 */
   0x20, 0x09, 0x82, 0x7c,	       /* 2c: Ignore/Replace #2 */
   0x22, 0x09, 0x0a, 0x0a	       /* 30: Rx FIFO Enable */
};

static CONTROLLER_T sController[CTL_SIZE] =
{
   {-1,-1,0,0,0,0,0,0,0,0,0,{0,0,0,0},{0,0,0,0},{-1,-1,-1,-1},{0,0,0,0}},
   {-1,-1,0,0,0,0,0,0,0,0,0,{0,0,0,0},{0,0,0,0},{-1,-1,-1,-1},{0,0,0,0}},
   {-1,-1,0,0,0,0,0,0,0,0,0,{0,0,0,0},{0,0,0,0},{-1,-1,-1,-1},{0,0,0,0}},
   {-1,-1,0,0,0,0,0,0,0,0,0,{0,0,0,0},{0,0,0,0},{-1,-1,-1,-1},{0,0,0,0}}
};

#if 0
/* IRQ number to MUDBAC register 2 mapping */
Byte_t sIRQMap[16] =
{
   0,0,0,0x10,0x20,0x30,0,0,0,0x40,0x50,0x60,0x70,0,0,0x80
};
#endif

static Byte_t sBitMapClrTbl[8] =
{
   0xfe,0xfd,0xfb,0xf7,0xef,0xdf,0xbf,0x7f
};

static Byte_t sBitMapSetTbl[8] =
{
   0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80
};

/***************************************************************************
Function: sInitController
Purpose:  Initialization of controller global registers and controller
	  structure.
Call:	  sInitController(CtlP,CtlNum,MudbacIO,AiopIOList,AiopIOListSize,
			  IRQNum,Frequency,PeriodicOnly)
	  CONTROLLER_T *CtlP; Ptr to controller structure
	  int CtlNum; Controller number
	  ByteIO_t MudbacIO; Mudbac base I/O address.
	  ByteIO_t *AiopIOList; List of I/O addresses for each AIOP.
	     This list must be in the order the AIOPs will be found on the
	     controller.  Once an AIOP in the list is not found, it is
	     assumed that there are no more AIOPs on the controller.
	  int AiopIOListSize; Number of addresses in AiopIOList
	  int IRQNum; Interrupt Request number.  Can be any of the following:
			 0: Disable global interrupts
			 3: IRQ 3
			 4: IRQ 4
			 5: IRQ 5
			 9: IRQ 9
			 10: IRQ 10
			 11: IRQ 11
			 12: IRQ 12
			 15: IRQ 15
	  Byte_t Frequency: A flag identifying the frequency
		   of the periodic interrupt, can be any one of the following:
		      FREQ_DIS - periodic interrupt disabled
		      FREQ_137HZ - 137 Hertz
		      FREQ_69HZ - 69 Hertz
		      FREQ_34HZ - 34 Hertz
		      FREQ_17HZ - 17 Hertz
		      FREQ_9HZ - 9 Hertz
		      FREQ_4HZ - 4 Hertz
		   If IRQNum is set to 0 the Frequency parameter is
		   overidden, it is forced to a value of FREQ_DIS.
	  int PeriodicOnly: TRUE if all interrupts except the periodic
			       interrupt are to be blocked.
			    FALSE is both the periodic interrupt and
			       other channel interrupts are allowed.
			    If IRQNum is set to 0 the PeriodicOnly parameter is
			       overidden, it is forced to a value of FALSE.
Return:   int: Number of AIOPs on the controller, or CTLID_NULL if controller
	       initialization failed.

Comments:
	  If periodic interrupts are to be disabled but AIOP interrupts
	  are allowed, set Frequency to FREQ_DIS and PeriodicOnly to FALSE.

	  If interrupts are to be completely disabled set IRQNum to 0.

	  Setting Frequency to FREQ_DIS and PeriodicOnly to TRUE is an
	  invalid combination.

	  This function performs initialization of global interrupt modes,
	  but it does not actually enable global interrupts.  To enable
	  and disable global interrupts use functions sEnGlobalInt() and
	  sDisGlobalInt().  Enabling of global interrupts is normally not
	  done until all other initializations are complete.

	  Even if interrupts are globally enabled, they must also be
	  individually enabled for each channel that is to generate
	  interrupts.

Warnings: No range checking on any of the parameters is done.

	  No context switches are allowed while executing this function.

	  After this function all AIOPs on the controller are disabled,
	  they can be enabled with sEnAiop().
*/
int sInitController(	CONTROLLER_T *CtlP,
			int CtlNum,
			ByteIO_t MudbacIO,
			ByteIO_t *AiopIOList,
			int AiopIOListSize,
			int IRQNum,
			Byte_t Frequency,
			int PeriodicOnly)
{
	int		i;
	ByteIO_t	io;

   CtlP->CtlNum = CtlNum;
   CtlP->BusType = isISA;
   CtlP->CtlID = CTLID_0001;	    /* controller release 1 */

   CtlP->MBaseIO = MudbacIO;
   CtlP->MReg1IO = MudbacIO + 1;
   CtlP->MReg2IO = MudbacIO + 2;
   CtlP->MReg3IO = MudbacIO + 3;
#if 1
   CtlP->MReg2 = 0;		    /* interrupt disable */
   CtlP->MReg3 = 0;		    /* no periodic interrupts */
#else
   if(sIRQMap[IRQNum] == 0)	       /* interrupts globally disabled */
   {
      CtlP->MReg2 = 0;		       /* interrupt disable */
      CtlP->MReg3 = 0;		       /* no periodic interrupts */
   }
   else
   {
      CtlP->MReg2 = sIRQMap[IRQNum];   /* set IRQ number */
      CtlP->MReg3 = Frequency;	       /* set frequency */
      if(PeriodicOnly)		       /* periodic interrupt only */
      {
	 CtlP->MReg3 |= PERIODIC_ONLY;
      }
   }
#endif
   sOutB(CtlP->MReg2IO,CtlP->MReg2);
   sOutB(CtlP->MReg3IO,CtlP->MReg3);
   sControllerEOI(CtlP);	       /* clear EOI if warm init */

   /* Init AIOPs */
   CtlP->NumAiop = 0;
   for(i=0; i < AiopIOListSize; i++)
   {
      io = AiopIOList[i];
      CtlP->AiopIO[i] = (WordIO_t)io;
      CtlP->AiopIntChanIO[i] = io + _INT_CHAN;
      sOutB(CtlP->MReg2IO,CtlP->MReg2 | (i & 0x03)); /* AIOP index */
      sOutB(MudbacIO,(Byte_t)(io >> 6));	/* set up AIOP I/O in MUDBAC */
      sEnAiop(CtlP,i);			       /* enable the AIOP */

      CtlP->AiopID[i] = sReadAiopID(io);       /* read AIOP ID */
      if(CtlP->AiopID[i] == AIOPID_NULL)       /* if AIOP does not exist */
      {
	 sDisAiop(CtlP,i);		       /* disable AIOP */
	 break; 			       /* done looking for AIOPs */
      }

      CtlP->AiopNumChan[i] = sReadAiopNumChan((WordIO_t)io); /* num channels in AIOP */
      sOutW((WordIO_t)io + _INDX_ADDR,_CLK_PRE);      /* clock prescaler */
      sOutB(io + _INDX_DATA,CLOCK_PRESC);
      CtlP->NumAiop++;			       /* bump count of AIOPs */
      sDisAiop(CtlP,i); 		       /* disable AIOP */
   }

   if(CtlP->NumAiop == 0)
      return(-1);
   else
      return(CtlP->NumAiop);
}

int sPCIInitController( CONTROLLER_T *CtlP,
			int CtlNum,
			ByteIO_t *AiopIOList,
			int AiopIOListSize,
			int IRQNum,
			Byte_t Frequency,
			int PeriodicOnly)
{
	int		i;
	ByteIO_t	io;

   CtlP->CtlNum = CtlNum;
   CtlP->BusType = isPCI;
   CtlP->CtlID = CTLID_0001;	    /* controller release 1 */
   CtlP->PCIIO = (WordIO_t)((ByteIO_t)AiopIOList[0] + _PCI_INT_FUNC);

   sPCIControllerEOI(CtlP);

   /* Init AIOPs */
   CtlP->NumAiop = 0;
   for(i=0; i < AiopIOListSize; i++)
   {
      io = AiopIOList[i];
      CtlP->AiopIO[i] = (WordIO_t)io;
      CtlP->AiopIntChanIO[i] = io + _INT_CHAN;

      CtlP->AiopID[i] = sReadAiopID(io);       /* read AIOP ID */
      if(CtlP->AiopID[i] == AIOPID_NULL)       /* if AIOP does not exist */
      {
	 break; 			       /* done looking for AIOPs */
      }

      CtlP->AiopNumChan[i] = sReadAiopNumChan((WordIO_t)io); /* num channels in AIOP */
      sOutW((WordIO_t)io + _INDX_ADDR,_CLK_PRE);      /* clock prescaler */
      sOutB(io + _INDX_DATA,CLOCK_PRESC);
      CtlP->NumAiop++;			       /* bump count of AIOPs */
   }

   if(CtlP->NumAiop == 0)
      return(-1);
   else
      return(CtlP->NumAiop);
}

/***************************************************************************
Function: sReadAiopID
Purpose:  Read the AIOP idenfication number directly from an AIOP.
Call:	  sReadAiopID(io)
	  ByteIO_t io: AIOP base I/O address
Return:   int: Flag AIOPID_XXXX if a valid AIOP is found, where X
		 is replace by an identifying number.
	  Flag AIOPID_NULL if no valid AIOP is found
Warnings: No context switches are allowed while executing this function.

*/
int sReadAiopID(ByteIO_t io)
{
   Byte_t AiopID;		/* ID byte from AIOP */

   sOutB(io + _CMD_REG,RESET_ALL);     /* reset AIOP */
   sOutB(io + _CMD_REG,0x0);
   AiopID = sInB(io + _CHN_STAT0) & 0x07;
   if(AiopID == 0x06)
      return(1);
   else 			       /* AIOP does not exist */
      return(-1);
}

/***************************************************************************
Function: sReadAiopNumChan
Purpose:  Read the number of channels available in an AIOP directly from
	  an AIOP.
Call:	  sReadAiopNumChan(io)
	  WordIO_t io: AIOP base I/O address
Return:   int: The number of channels available
Comments: The number of channels is determined by write/reads from identical
	  offsets within the SRAM address spaces for channels 0 and 4.
	  If the channel 4 space is mirrored to channel 0 it is a 4 channel
	  AIOP, otherwise it is an 8 channel.
Warnings: No context switches are allowed while executing this function.
*/
int sReadAiopNumChan(WordIO_t io)
{
   Word_t x;

   sOutDW((DWordIO_t)io + _INDX_ADDR,0x12340000L); /* write to chan 0 SRAM */
   sOutW(io + _INDX_ADDR,0);	   /* read from SRAM, chan 0 */
   x = sInW(io + _INDX_DATA);
   sOutW(io + _INDX_ADDR,0x4000);  /* read from SRAM, chan 4 */
   if(x != sInW(io + _INDX_DATA))  /* if different must be 8 chan */
      return(8);
   else
      return(4);
}

/***************************************************************************
Function: sInitChan
Purpose:  Initialization of a channel and channel structure
Call:	  sInitChan(CtlP,ChP,AiopNum,ChanNum)
	  CONTROLLER_T *CtlP; Ptr to controller structure
	  CHANNEL_T *ChP; Ptr to channel structure
	  int AiopNum; AIOP number within controller
	  int ChanNum; Channel number within AIOP
Return:   int: TRUE if initialization succeeded, FALSE if it fails because channel
	       number exceeds number of channels available in AIOP.
Comments: This function must be called before a channel can be used.
Warnings: No range checking on any of the parameters is done.

	  No context switches are allowed while executing this function.
*/
int sInitChan(	CONTROLLER_T *CtlP,
		CHANNEL_T *ChP,
		int AiopNum,
		int ChanNum)
{
   int i;
   WordIO_t AiopIO;
   WordIO_t ChIOOff;
   Byte_t *ChR;
   Word_t ChOff;
   static Byte_t R[4];

   if(ChanNum >= CtlP->AiopNumChan[AiopNum])
      return(FALSE);		       /* exceeds num chans in AIOP */

   /* Channel, AIOP, and controller identifiers */
   ChP->CtlP = CtlP;
   ChP->ChanID = CtlP->AiopID[AiopNum];
   ChP->AiopNum = AiopNum;
   ChP->ChanNum = ChanNum;

   /* Global direct addresses */
   AiopIO = CtlP->AiopIO[AiopNum];
   ChP->Cmd = (ByteIO_t)AiopIO + _CMD_REG;
   ChP->IntChan = (ByteIO_t)AiopIO + _INT_CHAN;
   ChP->IntMask = (ByteIO_t)AiopIO + _INT_MASK;
   ChP->IndexAddr = (DWordIO_t)AiopIO + _INDX_ADDR;
   ChP->IndexData = AiopIO + _INDX_DATA;

   /* Channel direct addresses */
   ChIOOff = AiopIO + ChP->ChanNum * 2;
   ChP->TxRxData = ChIOOff + _TD0;
   ChP->ChanStat = ChIOOff + _CHN_STAT0;
   ChP->TxRxCount = ChIOOff + _FIFO_CNT0;
   ChP->IntID = (ByteIO_t)AiopIO + ChP->ChanNum + _INT_ID0;

   /* Initialize the channel from the RData array */
   for(i=0; i < RDATASIZE; i+=4)
   {
      R[0] = RData[i];
      R[1] = RData[i+1] + 0x10 * ChanNum;
      R[2] = RData[i+2];
      R[3] = RData[i+3];
      sOutDW(ChP->IndexAddr,*((DWord_t *)&R[0]));
   }

   ChR = ChP->R;
   for(i=0; i < RREGDATASIZE; i+=4)
   {
      ChR[i] = RRegData[i];
      ChR[i+1] = RRegData[i+1] + 0x10 * ChanNum;
      ChR[i+2] = RRegData[i+2];
      ChR[i+3] = RRegData[i+3];
   }

   /* Indexed registers */
   ChOff = (Word_t)ChanNum * 0x1000;

   ChP->BaudDiv[0] = (Byte_t)(ChOff + _BAUD);
   ChP->BaudDiv[1] = (Byte_t)((ChOff + _BAUD) >> 8);
   ChP->BaudDiv[2] = (Byte_t)BRD9600;
   ChP->BaudDiv[3] = (Byte_t)(BRD9600 >> 8);
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->BaudDiv[0]);

   ChP->TxControl[0] = (Byte_t)(ChOff + _TX_CTRL);
   ChP->TxControl[1] = (Byte_t)((ChOff + _TX_CTRL) >> 8);
   ChP->TxControl[2] = 0;
   ChP->TxControl[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxControl[0]);

   ChP->RxControl[0] = (Byte_t)(ChOff + _RX_CTRL);
   ChP->RxControl[1] = (Byte_t)((ChOff + _RX_CTRL) >> 8);
   ChP->RxControl[2] = 0;
   ChP->RxControl[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->RxControl[0]);

   ChP->TxEnables[0] = (Byte_t)(ChOff + _TX_ENBLS);
   ChP->TxEnables[1] = (Byte_t)((ChOff + _TX_ENBLS) >> 8);
   ChP->TxEnables[2] = 0;
   ChP->TxEnables[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxEnables[0]);

   ChP->TxCompare[0] = (Byte_t)(ChOff + _TXCMP1);
   ChP->TxCompare[1] = (Byte_t)((ChOff + _TXCMP1) >> 8);
   ChP->TxCompare[2] = 0;
   ChP->TxCompare[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxCompare[0]);

   ChP->TxReplace1[0] = (Byte_t)(ChOff + _TXREP1B1);
   ChP->TxReplace1[1] = (Byte_t)((ChOff + _TXREP1B1) >> 8);
   ChP->TxReplace1[2] = 0;
   ChP->TxReplace1[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxReplace1[0]);

   ChP->TxReplace2[0] = (Byte_t)(ChOff + _TXREP2);
   ChP->TxReplace2[1] = (Byte_t)((ChOff + _TXREP2) >> 8);
   ChP->TxReplace2[2] = 0;
   ChP->TxReplace2[3] = 0;
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxReplace2[0]);

   ChP->TxFIFOPtrs = ChOff + _TXF_OUTP;
   ChP->TxFIFO = ChOff + _TX_FIFO;

   sOutB(ChP->Cmd,(Byte_t)ChanNum | RESTXFCNT); /* apply reset Tx FIFO count */
   sOutB(ChP->Cmd,(Byte_t)ChanNum);  /* remove reset Tx FIFO count */
   sOutW((WordIO_t)ChP->IndexAddr,ChP->TxFIFOPtrs); /* clear Tx in/out ptrs */
   sOutW(ChP->IndexData,0);
   ChP->RxFIFOPtrs = ChOff + _RXF_OUTP;
   ChP->RxFIFO = ChOff + _RX_FIFO;

   sOutB(ChP->Cmd,(Byte_t)ChanNum | RESRXFCNT); /* apply reset Rx FIFO count */
   sOutB(ChP->Cmd,(Byte_t)ChanNum);  /* remove reset Rx FIFO count */
   sOutW((WordIO_t)ChP->IndexAddr,ChP->RxFIFOPtrs); /* clear Rx out ptr */
   sOutW(ChP->IndexData,0);
   sOutW((WordIO_t)ChP->IndexAddr,ChP->RxFIFOPtrs + 2); /* clear Rx in ptr */
   sOutW(ChP->IndexData,0);
   ChP->TxPrioCnt = ChOff + _TXP_CNT;
   sOutW((WordIO_t)ChP->IndexAddr,ChP->TxPrioCnt);
   sOutB(ChP->IndexData,0);
   ChP->TxPrioPtr = ChOff + _TXP_PNTR;
   sOutW((WordIO_t)ChP->IndexAddr,ChP->TxPrioPtr);
   sOutB(ChP->IndexData,0);
   ChP->TxPrioBuf = ChOff + _TXP_BUF;
   sEnRxProcessor(ChP); 	       /* start the Rx processor */

   return(TRUE);
}

/***************************************************************************
Function: sStopRxProcessor
Purpose:  Stop the receive processor from processing a channel.
Call:	  sStopRxProcessor(ChP)
	  CHANNEL_T *ChP; Ptr to channel structure

Comments: The receive processor can be started again with sStartRxProcessor().
	  This function causes the receive processor to skip over the
	  stopped channel.  It does not stop it from processing other channels.

Warnings: No context switches are allowed while executing this function.

	  Do not leave the receive processor stopped for more than one
	  character time.

	  After calling this function a delay of 4 uS is required to ensure
	  that the receive processor is no longer processing this channel.
*/
void sStopRxProcessor(CHANNEL_T *ChP)
{
   Byte_t R[4];

   R[0] = ChP->R[0];
   R[1] = ChP->R[1];
   R[2] = 0x0a;
   R[3] = ChP->R[3];
   sOutDW(ChP->IndexAddr,*(DWord_t *)&R[0]);
}

/***************************************************************************
Function: sFlushRxFIFO
Purpose:  Flush the Rx FIFO
Call:	  sFlushRxFIFO(ChP)
	  CHANNEL_T *ChP; Ptr to channel structure
Return:   void
Comments: To prevent data from being enqueued or dequeued in the Tx FIFO
	  while it is being flushed the receive processor is stopped
	  and the transmitter is disabled.  After these operations a
	  4 uS delay is done before clearing the pointers to allow
	  the receive processor to stop.  These items are handled inside
	  this function.
Warnings: No context switches are allowed while executing this function.
*/
void sFlushRxFIFO(CHANNEL_T *ChP)
{
   int i;
   Byte_t Ch;			/* channel number within AIOP */
   int RxFIFOEnabled;		       /* TRUE if Rx FIFO enabled */

   if(sGetRxCnt(ChP) == 0)	       /* Rx FIFO empty */
      return;			       /* don't need to flush */

   RxFIFOEnabled = FALSE;
   if(ChP->R[0x32] == 0x08) /* Rx FIFO is enabled */
   {
      RxFIFOEnabled = TRUE;
      sDisRxFIFO(ChP);		       /* disable it */
      for(i=0; i < 2000/200; i++)	/* delay 2 uS to allow proc to disable FIFO*/
	 sInB(ChP->IntChan);		/* depends on bus i/o timing */
   }
   sGetChanStatus(ChP); 	 /* clear any pending Rx errors in chan stat */
   Ch = (Byte_t)sGetChanNum(ChP);
   sOutB(ChP->Cmd,Ch | RESRXFCNT);     /* apply reset Rx FIFO count */
   sOutB(ChP->Cmd,Ch);		       /* remove reset Rx FIFO count */
   sOutW((WordIO_t)ChP->IndexAddr,ChP->RxFIFOPtrs); /* clear Rx out ptr */
   sOutW(ChP->IndexData,0);
   sOutW((WordIO_t)ChP->IndexAddr,ChP->RxFIFOPtrs + 2); /* clear Rx in ptr */
   sOutW(ChP->IndexData,0);
   if(RxFIFOEnabled)
      sEnRxFIFO(ChP);		       /* enable Rx FIFO */
}

/***************************************************************************
Function: sFlushTxFIFO
Purpose:  Flush the Tx FIFO
Call:	  sFlushTxFIFO(ChP)
	  CHANNEL_T *ChP; Ptr to channel structure
Return:   void
Comments: To prevent data from being enqueued or dequeued in the Tx FIFO
	  while it is being flushed the receive processor is stopped
	  and the transmitter is disabled.  After these operations a
	  4 uS delay is done before clearing the pointers to allow
	  the receive processor to stop.  These items are handled inside
	  this function.
Warnings: No context switches are allowed while executing this function.
*/
void sFlushTxFIFO(CHANNEL_T *ChP)
{
   int i;
   Byte_t Ch;			/* channel number within AIOP */
   int TxEnabled;		       /* TRUE if transmitter enabled */

   if(sGetTxCnt(ChP) == 0)	       /* Tx FIFO empty */
      return;			       /* don't need to flush */

   TxEnabled = FALSE;
   if(ChP->TxControl[3] & TX_ENABLE)
   {
      TxEnabled = TRUE;
      sDisTransmit(ChP);	       /* disable transmitter */
   }
   sStopRxProcessor(ChP);	       /* stop Rx processor */
   for(i = 0; i < 4000/200; i++)	 /* delay 4 uS to allow proc to stop */
      sInB(ChP->IntChan);	/* depends on bus i/o timing */
   Ch = (Byte_t)sGetChanNum(ChP);
   sOutB(ChP->Cmd,Ch | RESTXFCNT);     /* apply reset Tx FIFO count */
   sOutB(ChP->Cmd,Ch);		       /* remove reset Tx FIFO count */
   sOutW((WordIO_t)ChP->IndexAddr,ChP->TxFIFOPtrs); /* clear Tx in/out ptrs */
   sOutW(ChP->IndexData,0);
   if(TxEnabled)
      sEnTransmit(ChP); 	       /* enable transmitter */
   sStartRxProcessor(ChP);	       /* restart Rx processor */
}

/***************************************************************************
Function: sWriteTxPrioByte
Purpose:  Write a byte of priority transmit data to a channel
Call:	  sWriteTxPrioByte(ChP,Data)
	  CHANNEL_T *ChP; Ptr to channel structure
	  Byte_t Data; The transmit data byte

Return:   int: 1 if the bytes is successfully written, otherwise 0.

Comments: The priority byte is transmitted before any data in the Tx FIFO.

Warnings: No context switches are allowed while executing this function.
*/
int sWriteTxPrioByte(CHANNEL_T *ChP, Byte_t Data)
{
   Byte_t DWBuf[4];		/* buffer for double word writes */
   Word_t *WordPtr;	     /* must be far because Win SS != DS */
   register DWordIO_t IndexAddr;

   if(sGetTxCnt(ChP) > 1)	       /* write it to Tx priority buffer */
   {
      IndexAddr = ChP->IndexAddr;
      sOutW((WordIO_t)IndexAddr,ChP->TxPrioCnt); /* get priority buffer status */
      if(sInB((ByteIO_t)ChP->IndexData) & PRI_PEND) /* priority buffer busy */
	 return(0);		       /* nothing sent */

      WordPtr = (Word_t *)(&DWBuf[0]);
      *WordPtr = ChP->TxPrioBuf;       /* data byte address */

      DWBuf[2] = Data;		       /* data byte value */
      sOutDW(IndexAddr,*((DWord_t *)(&DWBuf[0]))); /* write it out */

      *WordPtr = ChP->TxPrioCnt;       /* Tx priority count address */

      DWBuf[2] = PRI_PEND + 1;	       /* indicate 1 byte pending */
      DWBuf[3] = 0;		       /* priority buffer pointer */
      sOutDW(IndexAddr,*((DWord_t *)(&DWBuf[0]))); /* write it out */
   }
   else 			       /* write it to Tx FIFO */
   {
      sWriteTxByte(sGetTxRxDataIO(ChP),Data);
   }
   return(1);			       /* 1 byte sent */
}

/***************************************************************************
Function: sEnInterrupts
Purpose:  Enable one or more interrupts for a channel
Call:	  sEnInterrupts(ChP,Flags)
	  CHANNEL_T *ChP; Ptr to channel structure
	  Word_t Flags: Interrupt enable flags, can be any combination
	     of the following flags:
		TXINT_EN:   Interrupt on Tx FIFO empty
		RXINT_EN:   Interrupt on Rx FIFO at trigger level (see
			    sSetRxTrigger())
		SRCINT_EN:  Interrupt on SRC (Special Rx Condition)
		MCINT_EN:   Interrupt on modem input change
		CHANINT_EN: Allow channel interrupt signal to the AIOP's
			    Interrupt Channel Register.
Return:   void
Comments: If an interrupt enable flag is set in Flags, that interrupt will be
	  enabled.  If an interrupt enable flag is not set in Flags, that
	  interrupt will not be changed.  Interrupts can be disabled with
	  function sDisInterrupts().

	  This function sets the appropriate bit for the channel in the AIOP's
	  Interrupt Mask Register if the CHANINT_EN flag is set.  This allows
	  this channel's bit to be set in the AIOP's Interrupt Channel Register.

	  Interrupts must also be globally enabled before channel interrupts
	  will be passed on to the host.  This is done with function
	  sEnGlobalInt().

	  In some cases it may be desirable to disable interrupts globally but
	  enable channel interrupts.  This would allow the global interrupt
	  status register to be used to determine which AIOPs need service.
*/
void sEnInterrupts(CHANNEL_T *ChP,Word_t Flags)
{
   Byte_t Mask; 		/* Interrupt Mask Register */

   ChP->RxControl[2] |=
      ((Byte_t)Flags & (RXINT_EN | SRCINT_EN | MCINT_EN));

   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->RxControl[0]);

   ChP->TxControl[2] |= ((Byte_t)Flags & TXINT_EN);

   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxControl[0]);

   if(Flags & CHANINT_EN)
   {
      Mask = sInB(ChP->IntMask) | sBitMapSetTbl[ChP->ChanNum];
      sOutB(ChP->IntMask,Mask);
   }
}

/***************************************************************************
Function: sDisInterrupts
Purpose:  Disable one or more interrupts for a channel
Call:	  sDisInterrupts(ChP,Flags)
	  CHANNEL_T *ChP; Ptr to channel structure
	  Word_t Flags: Interrupt flags, can be any combination
	     of the following flags:
		TXINT_EN:   Interrupt on Tx FIFO empty
		RXINT_EN:   Interrupt on Rx FIFO at trigger level (see
			    sSetRxTrigger())
		SRCINT_EN:  Interrupt on SRC (Special Rx Condition)
		MCINT_EN:   Interrupt on modem input change
		CHANINT_EN: Disable channel interrupt signal to the
			    AIOP's Interrupt Channel Register.
Return:   void
Comments: If an interrupt flag is set in Flags, that interrupt will be
	  disabled.  If an interrupt flag is not set in Flags, that
	  interrupt will not be changed.  Interrupts can be enabled with
	  function sEnInterrupts().

	  This function clears the appropriate bit for the channel in the AIOP's
	  Interrupt Mask Register if the CHANINT_EN flag is set.  This blocks
	  this channel's bit from being set in the AIOP's Interrupt Channel
	  Register.
*/
void sDisInterrupts(CHANNEL_T *ChP,Word_t Flags)
{
   Byte_t Mask; 		/* Interrupt Mask Register */

   ChP->RxControl[2] &=
	 ~((Byte_t)Flags & (RXINT_EN | SRCINT_EN | MCINT_EN));
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->RxControl[0]);
   ChP->TxControl[2] &= ~((Byte_t)Flags & TXINT_EN);
   sOutDW(ChP->IndexAddr,*(DWord_t *)&ChP->TxControl[0]);

   if(Flags & CHANINT_EN)
   {
      Mask = sInB(ChP->IntMask) & sBitMapClrTbl[ChP->ChanNum];
      sOutB(ChP->IntMask,Mask);
   }
}

/*********************************************************************
  Begin FreeBsd-specific driver code
**********************************************************************/

static int rpprobe __P((struct isa_device *));
static int rpattach __P((struct isa_device *));

static char* rp_pciprobe(pcici_t tag, pcidi_t type);
static void rp_pciattach(pcici_t tag, int unit);
static u_long	rp_pcicount;

static struct pci_device rp_pcidevice = {
	"rp",
	rp_pciprobe,
	rp_pciattach,
	&rp_pcicount,
	NULL
};

DATA_SET (pcidevice_set, rp_pcidevice);

static timeout_t rpdtrwakeup;

struct isa_driver rpdriver = {
	rpprobe, rpattach, "rp"
     };

#define CDEV_MAJOR	81

static	char	driver_name[] = "rp";

static	d_open_t	rpopen;
static	d_close_t	rpclose;
static	d_read_t	rpread;
static	d_write_t	rpwrite;
static	d_ioctl_t	rpioctl;
static	d_stop_t	rpstop;
static	d_devtotty_t	rpdevtotty;

static	struct cdevsw	rp_cdevsw =
		{ rpopen,	rpclose,	rpread, 	rpwrite,
		  rpioctl,	rpstop, 	noreset,	rpdevtotty,
		  ttpoll,	nommap, 	NULL,		driver_name,
		  NULL, 	-1};

static int rp_controller_port = 0;
static int rp_num_ports_open = 0;
static int rp_timeout;
static int	ndevs = 0;
static int	minor_to_unit[128];
static	struct	tty	rp_tty[128];

static int rp_num_ports[4];	/* Number of ports on each controller */

#define _INLINE_ __inline
#define POLL_INTERVAL 1

#define CALLOUT_MASK		0x80
#define CONTROL_MASK		0x60
#define CONTROL_INIT_STATE	0x20
#define CONTROL_LOCK_STATE	0x40
#define DEV_UNIT(dev)	(MINOR_TO_UNIT(minor(dev))
#define MINOR_MAGIC_MASK	(CALLOUT_MASK | CONTROL_MASK)
#define MINOR_MAGIC(dev)	((minor(dev)) & ~MINOR_MAGIC_MASK)
#define IS_CALLOUT(dev) 	(minor(dev) & CALLOUT_MASK)
#define IS_CONTROL(dev) 	(minor(dev) & CONTROL_MASK)

#define RP_ISMULTIPORT(dev)	((dev)->id_flags & 0x1)
#define RP_MPMASTER(dev)	(((dev)->id_flags >> 8) & 0xff)
#define RP_NOTAST4(dev) 	((dev)->id_flags & 0x04)

static	struct	rp_port *p_rp_addr[4];
static	struct	rp_port *p_rp_table[MAX_RP_PORTS];
#define rp_addr(unit)	(p_rp_addr[unit])
#define rp_table(port)	(p_rp_table[port])

/*
 * The top-level routines begin here
 */

int	rpselect __P((dev_t, int, struct proc *));

static	int	rpparam __P((struct tty *, struct termios *));
static	void	rpstart __P((struct tty *));
static	void	rphardclose	__P((struct rp_port *));
#define rpmap	nomap
#define rpreset noreset
#define rpstrategy	nostrategy
static	void	rp_disc_optim	__P((struct tty *tp, struct termios *t,
						struct rp_port	*rp));

static _INLINE_ void rp_do_receive(struct rp_port *rp, struct tty *tp,
			CHANNEL_t *cp, unsigned int ChanStatus)
{
	int	spl;
	unsigned	int	CharNStat;
	int	ToRecv, wRecv, ch;

	ToRecv = sGetRxCnt(cp);
	if(ToRecv == 0)
		return;

/*	If status indicates there are errored characters in the
	FIFO, then enter status mode (a word in FIFO holds
	characters and status)
*/

	if(ChanStatus & (RXFOVERFL | RXBREAK | RXFRAME | RXPARITY)) {
		if(!(ChanStatus & STATMODE)) {
			ChanStatus |= STATMODE;
			sEnRxStatusMode(cp);
		}
	}
/*
	if we previously entered status mode then read down the
	FIFO one word at a time, pulling apart the character and
	the status. Update error counters depending on status.
*/
	if(ChanStatus & STATMODE) {
		while(ToRecv) {
			if(tp->t_state & TS_TBLOCK) {
				break;
			}
			CharNStat = sInW(sGetTxRxDataIO(cp));
			ch = CharNStat & 0xff;

			if((CharNStat & STMBREAK) || (CharNStat & STMFRAMEH))
				ch |= TTY_FE;
			else if (CharNStat & STMPARITYH)
				ch |= TTY_PE;
			else if (CharNStat & STMRCVROVRH)
				rp->rp_overflows++;

			(*linesw[tp->t_line].l_rint)(ch, tp);
			ToRecv--;
		}
/*
	After emtying FIFO in status mode, turn off status mode
*/

	if(sGetRxCnt(cp) == 0)
		sDisRxStatusMode(cp);
	}
	else {
		while (ToRecv) {
			if(tp->t_state & TS_TBLOCK) {
				break;
			}
			ch = (u_char) sInB(sGetTxRxDataIO(cp));
			spl = spltty();
			(*linesw[tp->t_line].l_rint)(ch, tp);
			splx(spl);
			ToRecv--;
		}
	}
}

static _INLINE_ void rp_handle_port(struct rp_port *rp)
{
	CHANNEL_t	*cp;
	struct	tty	*tp;
	unsigned	int	IntMask, ChanStatus;
	int	oldcts, ToRecv;

	if(!rp)
		return;

	cp = &rp->rp_channel;
	tp = rp->rp_tty;
	IntMask = sGetChanIntID(cp);
	IntMask = IntMask & rp->rp_intmask;
	ChanStatus = sGetChanStatus(cp);
	if(IntMask & RXF_TRIG)
		if(!(tp->t_state & TS_TBLOCK) && (tp->t_state & TS_CARR_ON) && (tp->t_state & TS_ISOPEN)) {
			rp_do_receive(rp, tp, cp, ChanStatus);
		}
	if(IntMask & DELTA_CD) {
		if(ChanStatus & CD_ACT) {
			if(!(tp->t_state & TS_CARR_ON) ) {
				(void)(*linesw[tp->t_line].l_modem)(tp, 1);
			}
		} else {
			if((tp->t_state & TS_CARR_ON)) {
				(void)(*linesw[tp->t_line].l_modem)(tp, 0);
				if((*linesw[tp->t_line].l_modem)(tp, 0) == 0) {
					rphardclose(rp);
				}
			}
		}
	}
/*	oldcts = rp->rp_cts;
	rp->rp_cts = ((ChanStatus & CTS_ACT) != 0);
	if(oldcts != rp->rp_cts) {
		printf("CTS change (now %s)... on port %d\n", rp->rp_cts ? "on" : "off", rp->rp_port);
	}
*/
}

static void rp_do_poll(void *not_used)
{
	CONTROLLER_t	*ctl;
	struct rp_port	*rp;
	struct tty	*tp;
	int	unit, aiop, ch, line, count;
	unsigned char	CtlMask, AiopMask;

	for(unit = 0; unit <= ndevs; unit++) {
	rp = rp_addr(unit);
	ctl = rp->rp_ctlp;
	if(ctl->BusType == isPCI)
		CtlMask = sPCIGetControllerIntStatus(ctl);
	else
		CtlMask = sGetControllerIntStatus(ctl);
	for(aiop=0; CtlMask; CtlMask >>=1, aiop++) {
		if(CtlMask & 1) {
			AiopMask = sGetAiopIntStatus(ctl, aiop);
			for(ch = 0; AiopMask; AiopMask >>=1, ch++) {
				if(AiopMask & 1) {
					line = (unit << 5) | (aiop << 3) | ch;
					rp = rp_table(line);
					rp_handle_port(rp);
				}
			}
		}
	}

	for(line = 0, rp = rp_addr(unit); line < rp_num_ports[unit];
			line++, rp++) {
		tp = rp->rp_tty;
		if((tp->t_state & TS_BUSY) && (tp->t_state & TS_ISOPEN)) {
			count = sGetTxCnt(&rp->rp_channel);
			if(count == 0)
				tp->t_state &= ~(TS_BUSY);
			if(!(tp->t_state & TS_TTSTOP) &&
				(count <= rp->rp_restart)) {
				(*linesw[tp->t_line].l_start)(tp);
			}
		}
	}
	}
	if(rp_num_ports_open)
		timeout(rp_do_poll, (void *)NULL, POLL_INTERVAL);
}

static char*
rp_pciprobe(pcici_t tag, pcidi_t type)
{
	int	vendor_id;

	vendor_id = type & 0xffff;
	switch(vendor_id)
	case 0x11fe:
		return("rp");
	return(NULL);
}

static
int
rpprobe(dev)
struct isa_device *dev;
{
	struct	isa_device	*idev;
	int controller, unit;
	int i, aiop, num_aiops;
	unsigned int aiopio[MAX_AIOPS_PER_BOARD];
	CONTROLLER_t *ctlp;

	unit = dev->id_unit;
	if (dev->id_unit >= 4) {
		printf("rpprobe: unit number %d invalid.\n", dev->id_unit);
		return 1;
	}
	printf("probing for RocketPort(ISA) unit %d\n", unit);
	if (rp_controller_port)
		controller = rp_controller_port;
	else {
		controller = dev->id_iobase + 0x40;
	}

	for (aiop=0; aiop<MAX_AIOPS_PER_BOARD; aiop++)
		aiopio[aiop]= dev->id_iobase + (aiop * 0x400);

	ctlp = sCtlNumToCtlPtr(dev->id_unit);
	num_aiops = sInitController(ctlp, dev->id_unit,
				controller + ((unit-rp_pcicount)*0x400),
				aiopio, MAX_AIOPS_PER_BOARD, 0,
				FREQ_DIS, 0);
	if (num_aiops <= 0) {
		printf("board%d init failed\n", unit);
		return 0;
	}

	if (rp_controller_port) {
		dev->id_msize = 64;
	} else {
		dev->id_msize = 68;
		rp_controller_port = controller;
	}

	dev->id_irq = 0;

	return 1;
}

static void
rp_pciattach(pcici_t tag, int unit)
{
	dev_t	rp_dev;
	int	success, rpmajor, oldspl;
	u_short iobase;
	int	num_ports, num_chan, num_aiops;
	int	aiop, chan, port;
	int	ChanStatus, line, i, count;
	unsigned int	aiopio[MAX_AIOPS_PER_BOARD];
	struct	rp_port *rp;
	struct	tty	*tty;
	CONTROLLER_t	*ctlp;
	char	status;

	success = pci_map_port(tag, 0x10, &iobase);
	if(!success)
		printf("ioaddr mapping failed for RocketPort(PCI)\n");

	for(aiop=0; aiop < MAX_AIOPS_PER_BOARD; aiop++)
		aiopio[aiop] = iobase + (aiop * 0x40);

	ctlp = sCtlNumToCtlPtr(unit);
	num_aiops = sPCIInitController(ctlp, unit,
				aiopio, MAX_AIOPS_PER_BOARD, 0,
				FREQ_DIS, 0);

	num_ports = 0;
	for(aiop=0; aiop < num_aiops; aiop++) {
		sResetAiopByNum(ctlp, aiop);
		num_ports += sGetAiopNumChan(ctlp, aiop);
	}
	printf("RocketPort%d = %d ports\n", unit, num_ports);
	rp_num_ports[unit] = num_ports;

	rp = (struct rp_port *)
		malloc(sizeof(struct rp_port) * num_ports, M_TTYS, M_NOWAIT);
	if(rp == 0) {
		printf("rp_attach: Could not malloc rp_ports structures\n");
		return;
	}

	count = unit * 32;      /* board times max ports per card SG */
	for(i=count;i < (count + rp_num_ports[unit]);i++)
		minor_to_unit[i] = unit;

	bzero(rp, sizeof(struct rp_port) * num_ports);
	tty = (struct tty *)
		malloc(sizeof(struct tty) * num_ports, M_TTYS, M_NOWAIT);
	if(tty == 0) {
		printf("rp_attach: Could not malloc tty structures\n");
		return;
	}
	bzero(tty, sizeof(struct tty) * num_ports);

	oldspl = spltty();
	rp_addr(unit) = rp;
	splx(oldspl);

	rp_dev = makedev(CDEV_MAJOR, unit);
	cdevsw_add(&rp_dev, &rp_cdevsw, NULL);

	port = 0;
	for(aiop=0; aiop < num_aiops; aiop++) {
		num_chan = sGetAiopNumChan(ctlp, aiop);
		for(chan=0; chan < num_chan; chan++, port++, rp++, tty++) {
			rp->rp_tty = tty;
			rp->rp_port = port;
			rp->rp_ctlp = ctlp;
			rp->rp_unit = unit;
			rp->rp_chan = chan;
			rp->rp_aiop = aiop;

			tty->t_line = 0;
	/*		tty->t_termios = deftermios;
	*/
			rp->dtr_wait = 3 * hz;
			rp->it_in.c_iflag = 0;
			rp->it_in.c_oflag = 0;
			rp->it_in.c_cflag = TTYDEF_CFLAG;
			rp->it_in.c_lflag = 0;
			termioschars(&rp->it_in);
	/*		termioschars(&tty->t_termios);
	*/
			rp->it_in.c_ispeed = rp->it_in.c_ospeed = TTYDEF_SPEED;
			rp->it_out = rp->it_in;

			rp->rp_intmask = RXF_TRIG | TXFIFO_MT | SRC_INT |
				DELTA_CD | DELTA_CTS | DELTA_DSR;
			ChanStatus = sGetChanStatus(&rp->rp_channel);
			if(sInitChan(ctlp, &rp->rp_channel, aiop, chan) == 0) {
				printf("RocketPort sInitChan(%d, %d, %d) failed
					\n", unit, aiop, chan);
				return;
			}
			ChanStatus = sGetChanStatus(&rp->rp_channel);
			rp->rp_cts = (ChanStatus & CTS_ACT) != 0;
			line = (unit << 5) | (aiop << 3) | chan;
			rp_table(line) = rp;
/*			devfs_add_devswf(&rp_cdevsw,
				port, DV_CHR, UID_ROOT, GID_WHEEL, 0600,
				"ttyR%n", port);
			devfs_add_devswf(&rp_cdevsw,
				port | CONTROL_INIT_STATE, DV_CHR, UID_ROOT,
				GID_WHEEL, 0600, "ttyRi%n", port);
*/
		}
	}
}

static
int
rpattach(dev)
struct	isa_device	*dev;
{
	struct	isa_device	*idev;
	dev_t	rp_dev;
	int	iobase, unit, rpmajor, oldspl;
	int	num_ports, num_chan, num_aiops;
	int	aiop, chan, port;
	int	ChanStatus, line, i, count;
	unsigned int	aiopio[MAX_AIOPS_PER_BOARD];
	struct	rp_port *rp;
	struct	tty	*tty;
	CONTROLLER_t	*ctlp;
	char	status;

	iobase = dev->id_iobase;
	unit = dev->id_unit;
	ndevs = unit;

	for(aiop=0; aiop < MAX_AIOPS_PER_BOARD; aiop++)
		aiopio[aiop] = iobase + (aiop * 0x400);

	ctlp = sCtlNumToCtlPtr(unit);
	num_aiops = sInitController(ctlp, unit,
				rp_controller_port + ((unit-rp_pcicount) * 0x400),
				aiopio, MAX_AIOPS_PER_BOARD, 0,
				FREQ_DIS, 0);

	num_ports = 0;
	for(aiop=0; aiop < num_aiops; aiop++) {
		sResetAiopByNum(ctlp, aiop);
		sEnAiop(ctlp, aiop);
		num_ports += sGetAiopNumChan(ctlp, aiop);
	}
	printf("RocketPort%d = %d ports\n", unit, num_ports);
	rp_num_ports[unit] = num_ports;

	rp = (struct rp_port *)
		malloc(sizeof(struct rp_port) * num_ports, M_TTYS, M_NOWAIT);
	if(rp == 0) {
		printf("rp_attach: Could not malloc rp_ports structures\n");
		return(0);
	}

	count = unit * 32;    /* board # times max ports per card  SG */
	for(i=count;i < (count + rp_num_ports[unit]);i++)
		minor_to_unit[i] = unit;

	bzero(rp, sizeof(struct rp_port) * num_ports);
	tty = (struct tty *)
		malloc(sizeof(struct tty) * num_ports, M_TTYS, M_NOWAIT);
	if(tty == 0) {
		printf("rp_attach: Could not malloc tty structures\n");
		return(0);
	}
	bzero(tty, sizeof(struct tty) * num_ports);

	oldspl = spltty();
	rp_addr(unit) = rp;
	splx(oldspl);

	rp_dev = makedev(CDEV_MAJOR, unit);
	cdevsw_add(&rp_dev, &rp_cdevsw, NULL);

	port = 0;
	for(aiop=0; aiop < num_aiops; aiop++) {
		num_chan = sGetAiopNumChan(ctlp, aiop);
		for(chan=0; chan < num_chan; chan++, port++, rp++, tty++) {
			rp->rp_tty = tty;
			rp->rp_port = port;
			rp->rp_ctlp = ctlp;
			rp->rp_unit = unit;
			rp->rp_chan = chan;
			rp->rp_aiop = aiop;

			tty->t_line = 0;
	/*		tty->t_termios = deftermios;
	*/
			rp->dtr_wait = 3 * hz;
			rp->it_in.c_iflag = 0;
			rp->it_in.c_oflag = 0;
			rp->it_in.c_cflag = TTYDEF_CFLAG;
			rp->it_in.c_lflag = 0;
			termioschars(&rp->it_in);
	/*		termioschars(&tty->t_termios);
	*/
			rp->it_in.c_ispeed = rp->it_in.c_ospeed = TTYDEF_SPEED;
			rp->it_out = rp->it_in;

			rp->rp_intmask = RXF_TRIG | TXFIFO_MT | SRC_INT |
				DELTA_CD | DELTA_CTS | DELTA_DSR;
			ChanStatus = sGetChanStatus(&rp->rp_channel);
			if(sInitChan(ctlp, &rp->rp_channel, aiop, chan) == 0) {
				printf("RocketPort sInitChan(%d, %d, %d) failed
					\n", unit, aiop, chan);
				return(0);
			}
			ChanStatus = sGetChanStatus(&rp->rp_channel);
			rp->rp_cts = (ChanStatus & CTS_ACT) != 0;
			line = (unit << 5) | (aiop << 3) | chan;
			rp_table(line) = rp;
		}
	}

		idev = find_isadev(isa_devtab_tty, &rpdriver,
				RP_MPMASTER(dev) + rp_pcicount);
		if(idev == NULL) {
			printf("rp%d: master device %d not configured\n",
				dev->id_unit, RP_MPMASTER(dev));
		}
/*		printf("COOL!! Device is found!!\n");
	for(rpmajor=0;rpmajor<nchrdev;rpmajor++)
		if(cdevsw[rpmajor].d_open == rpopen)
			printf("From %d entries: Found entry at major = %d\n",nchrdev,rpmajor);
*/
	return(1);
}

int
rpopen(dev, flag, mode, p)
	dev_t	dev;
	int	flag, mode;
	struct	proc	*p;
{
	struct	rp_port *rp;
	int	unit, i, port, mynor, umynor, flags;  /* SG */
	struct	tty	*tp;
	int	oldspl, error;
	unsigned int	IntMask, ChanStatus;


   umynor = (((minor(dev) >> 16) -1) * 32);    /* SG */
	port  = (minor(dev) & 0x1f);                /* SG */
	mynor = (port + umynor);                    /* SG */
	unit = minor_to_unit[mynor];
	if(IS_CONTROL(dev))
		return(0);
	rp = rp_addr(unit) + port;
/*	rp->rp_tty = &rp_tty[rp->rp_port];
*/
	tp = rp->rp_tty;

	oldspl = spltty();

open_top:
	while(rp->state & ~SET_DTR) {
		error = tsleep(&rp->dtr_wait, TTIPRI | PCATCH, "rpdtr", 0);
		if(error != 0)
			goto out;
	}

	if(tp->t_state & TS_ISOPEN) {
		if(IS_CALLOUT(dev)) {
			if(!rp->active_out) {
				error = EBUSY;
				goto out;
			}
		} else {
			if(rp->active_out) {
				if(flag & O_NONBLOCK) {
					error = EBUSY;
					goto out;
				}
				error = tsleep(&rp->active_out,
					TTIPRI | PCATCH, "rpbi", 0);
				if(error != 0)
					goto out;
				goto open_top;
			}
		}
		if(tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
			splx(oldspl);
			return(EBUSY);
		}
	}
	else {
		tp->t_dev = dev;
		tp->t_param = rpparam;
		tp->t_oproc = rpstart;
		tp->t_line = 0;
		tp->t_termios = IS_CALLOUT(dev) ? rp->it_out : rp->it_in;
		flags = 0;
		flags |= SET_RTS;
		flags |= SET_DTR;
		rp->rp_channel.TxControl[3] =
			((rp->rp_channel.TxControl[3]
			& ~(SET_RTS | SET_DTR)) | flags);
		sOutDW(rp->rp_channel.IndexAddr,
			*(DWord_t *) &(rp->rp_channel.TxControl[0]));
		sSetRxTrigger(&rp->rp_channel, TRIG_1);
		sDisRxStatusMode(&rp->rp_channel);
		sFlushRxFIFO(&rp->rp_channel);
		sFlushTxFIFO(&rp->rp_channel);

		sEnInterrupts(&rp->rp_channel,
			(TXINT_EN|MCINT_EN|RXINT_EN|SRCINT_EN|CHANINT_EN));
		sSetRxTrigger(&rp->rp_channel, TRIG_1);

		sDisRxStatusMode(&rp->rp_channel);
		sClrTxXOFF(&rp->rp_channel);

/*		sDisRTSFlowCtl(&rp->rp_channel);
		sDisCTSFlowCtl(&rp->rp_channel);
*/
		sDisTxSoftFlowCtl(&rp->rp_channel);

		sStartRxProcessor(&rp->rp_channel);

		sEnRxFIFO(&rp->rp_channel);
		sEnTransmit(&rp->rp_channel);

/*		sSetDTR(&rp->rp_channel);
		sSetRTS(&rp->rp_channel);
*/

		++rp->wopeners;
		error = rpparam(tp, &tp->t_termios);
		--rp->wopeners;
		if(error != 0) {
			splx(oldspl);
			return(error);
		}

		ttsetwater(tp);

		rp_num_ports_open++;

		IntMask = sGetChanIntID(&rp->rp_channel);
		IntMask = IntMask & rp->rp_intmask;
		ChanStatus = sGetChanStatus(&rp->rp_channel);
		if((IntMask & DELTA_CD) || IS_CALLOUT(dev)) {
			if((ChanStatus & CD_ACT) || IS_CALLOUT(dev)) {
					(void)(*linesw[tp->t_line].l_modem)(tp, 1);
			}
		}

	if(rp_num_ports_open == 1)
		timeout(rp_do_poll, (void *)NULL, POLL_INTERVAL);

	}

	if(!(flag&O_NONBLOCK) && !(tp->t_cflag&CLOCAL) &&
		!(tp->t_state & TS_CARR_ON) && !(IS_CALLOUT(dev))) {
		++rp->wopeners;
		error = tsleep(TSA_CARR_ON(tp), TTIPRI | PCATCH,
				"rpdcd", 0);
		--rp->wopeners;
		if(error != 0)
			goto out;
		goto open_top;
	}
	error = (*linesw[tp->t_line].l_open)(dev, tp);

	rp_disc_optim(tp, &tp->t_termios, rp);
	if(tp->t_state & TS_ISOPEN && IS_CALLOUT(dev))
		rp->active_out = TRUE;

/*	if(rp_num_ports_open == 1)
		timeout(rp_do_poll, (void *)NULL, POLL_INTERVAL);
*/
out:
	splx(oldspl);
	if(!(tp->t_state & TS_ISOPEN) && rp->wopeners == 0) {
		rphardclose(rp);
	}
	return(error);
}

int
rpclose(dev, flag, mode, p)
	dev_t	dev;
	int	flag, mode;
	struct	proc	*p;
{
	int	oldspl, unit, mynor, umynor, port, status, i; /* SG */
	struct	rp_port *rp;
	struct	tty	*tp;
	CHANNEL_t	*cp;

   umynor = (((minor(dev) >> 16) -1) * 32);    /* SG */
	port  = (minor(dev) & 0x1f);                /* SG */
	mynor = (port + umynor);                    /* SG */
   unit = minor_to_unit[mynor];                /* SG */

	if(IS_CONTROL(dev))
		return(0);
	rp = rp_addr(unit) + port;
	cp = &rp->rp_channel;
	tp = rp->rp_tty;

	oldspl = spltty();
	(*linesw[tp->t_line].l_close)(tp, flag);
	rp_disc_optim(tp, &tp->t_termios, rp);
	rpstop(tp, FREAD | FWRITE);
	rphardclose(rp);

	tp->t_state &= ~TS_BUSY;
	ttyclose(tp);

	splx(oldspl);

	return(0);
}

static void
rphardclose(struct rp_port *rp)
{
	int	status, oldspl, mynor;
	struct	tty	*tp;
	CHANNEL_t	*cp;

	cp = &rp->rp_channel;
	tp = rp->rp_tty;
	mynor = MINOR_MAGIC(tp->t_dev);

	sFlushRxFIFO(cp);
	sFlushTxFIFO(cp);
	sDisTransmit(cp);
	sDisInterrupts(cp, TXINT_EN|MCINT_EN|RXINT_EN|SRCINT_EN|CHANINT_EN);
	sDisRTSFlowCtl(cp);
	sDisCTSFlowCtl(cp);
	sDisTxSoftFlowCtl(cp);
	sClrTxXOFF(cp);

	if(tp->t_cflag&HUPCL || !(tp->t_state&TS_ISOPEN) || !rp->active_out) {
		sClrDTR(cp);
	}
	if(IS_CALLOUT(tp->t_dev)) {
		sClrDTR(cp);
	}
	if(rp->dtr_wait != 0) {
		timeout(rpdtrwakeup, rp, rp->dtr_wait);
		rp->state |= ~SET_DTR;
	}

	rp->active_out = FALSE;
	wakeup(&rp->active_out);
	wakeup(TSA_CARR_ON(tp));
}

static
int
rpread(dev, uio, flag)
	dev_t	dev;
	struct	uio	*uio;
	int	flag;
{
	struct	rp_port *rp;
	struct	tty	*tp;
	int	unit, i, mynor, umynor, port, error = 0; /* SG */

   umynor = (((minor(dev) >> 16) -1) * 32);    /* SG */
	port  = (minor(dev) & 0x1f);                /* SG */
	mynor = (port + umynor);                    /* SG */
   unit = minor_to_unit[mynor];                /* SG */

	if(IS_CONTROL(dev))
		return(ENODEV);
	rp = rp_addr(unit) + port;
	tp = rp->rp_tty;
	error = (*linesw[tp->t_line].l_read)(tp, uio, flag);
	return(error);
}

static
int
rpwrite(dev, uio, flag)
	dev_t	dev;
	struct	uio	*uio;
	int	flag;
{
	struct	rp_port *rp;
	struct	tty	*tp;
	int	unit, i, mynor, port, umynor, error = 0; /* SG */

   umynor = (((minor(dev) >> 16) -1) * 32);    /* SG */
	port  = (minor(dev) & 0x1f);                /* SG */
	mynor = (port + umynor);                    /* SG */
   unit = minor_to_unit[mynor];                /* SG */

	if(IS_CONTROL(dev))
		return(ENODEV);
	rp = rp_addr(unit) + port;
	tp = rp->rp_tty;
	while(rp->rp_disable_writes) {
		rp->rp_waiting = 1;
		if(error = ttysleep(tp, (caddr_t)rp, TTOPRI|PCATCH,
					"rp_write", 0)) {
			return(error);
		}
	}

	error = (*linesw[tp->t_line].l_write)(tp, uio, flag);
	return error;
}

static void
rpdtrwakeup(void *chan)
{
	struct	rp_port *rp;

	rp = (struct rp_port *)chan;
	rp->state &= SET_DTR;
	wakeup(&rp->dtr_wait);
}

int
rpioctl(dev, cmd, data, flag, p)
	dev_t	dev;
	u_long	cmd;
	caddr_t data;
	int	flag;
	struct	proc	*p;
{
	struct rp_port	*rp;
	CHANNEL_t	*cp;
	struct tty	*tp;
	int	unit, mynor, port, umynor;            /* SG */
	int	oldspl, cflag, iflag, oflag, lflag;
	int	i, error = 0;
	char	status;
	int	arg, flags, result, ChanStatus;
	int	oldcmd;
	struct	termios term, *t;

   umynor = (((minor(dev) >> 16) -1) * 32);    /* SG */
	port  = (minor(dev) & 0x1f);                /* SG */
	mynor = (port + umynor);                    /* SG */
	unit = minor_to_unit[mynor];
	rp = rp_addr(unit) + port;

	if(IS_CONTROL(dev)) {
		struct	termios *ct;

		switch (IS_CONTROL(dev)) {
		case CONTROL_INIT_STATE:
			ct =  IS_CALLOUT(dev) ? &rp->it_out : &rp->it_in;
			break;
		case CONTROL_LOCK_STATE:
			ct =  IS_CALLOUT(dev) ? &rp->lt_out : &rp->lt_in;
			break;
		default:
			return(ENODEV); 	/* /dev/nodev */
		}
		switch (cmd) {
		case TIOCSETA:
			error = suser(p->p_ucred, &p->p_acflag);
			if(error != 0)
				return(error);
			*ct = *(struct termios *)data;
			return(0);
		case TIOCGETA:
			*(struct termios *)data = *ct;
			return(0);
		case TIOCGETD:
			*(int *)data = TTYDISC;
			return(0);
		case TIOCGWINSZ:
			bzero(data, sizeof(struct winsize));
			return(0);
		default:
			return(ENOTTY);
		}
	}

	tp = rp->rp_tty;
	cp = &rp->rp_channel;

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	term = tp->t_termios;
	oldcmd = cmd;
	error = ttsetcompat(tp, &cmd, data, &term);
	if(error != 0)
		return(error);
	if(cmd != oldcmd) {
		data = (caddr_t)&term;
	}
#endif
	if((cmd == TIOCSETA) || (cmd == TIOCSETAW) || (cmd == TIOCSETAF)) {
		int	cc;
		struct	termios *dt = (struct termios *)data;
		struct	termios *lt = IS_CALLOUT(dev)
					? &rp->lt_out : &rp->lt_in;

		dt->c_iflag = (tp->t_iflag & lt->c_iflag)
				| (dt->c_iflag & ~lt->c_iflag);
		dt->c_oflag = (tp->t_oflag & lt->c_oflag)
				| (dt->c_oflag & ~lt->c_oflag);
		dt->c_cflag = (tp->t_cflag & lt->c_cflag)
				| (dt->c_cflag & ~lt->c_cflag);
		dt->c_lflag = (tp->t_lflag & lt->c_lflag)
				| (dt->c_lflag & ~lt->c_lflag);
		for(cc = 0; cc < NCCS; ++cc)
			if(lt->c_cc[cc] = tp->t_cc[cc])
				dt->c_cc[cc] = tp->t_cc[cc];
		if(lt->c_ispeed != 0)
			dt->c_ispeed = tp->t_ispeed;
		if(lt->c_ospeed != 0)
			dt->c_ospeed = tp->t_ospeed;
	}

	t = &tp->t_termios;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if(error != ENOIOCTL) {
		return(error);
	}
	oldspl = spltty();

	flags = rp->rp_channel.TxControl[3];

	error = ttioctl(tp, cmd, data, flag);
	flags = rp->rp_channel.TxControl[3];
	rp_disc_optim(tp, &tp->t_termios, rp);
	if(error != ENOIOCTL) {
		splx(oldspl);
		return(error);
	}
	switch(cmd) {
	case TIOCSBRK:
		sSendBreak(&rp->rp_channel);
		break;

	case TIOCCBRK:
		sClrBreak(&rp->rp_channel);
		break;

	case TIOCSDTR:
		sSetDTR(&rp->rp_channel);
		sSetRTS(&rp->rp_channel);
		break;

	case TIOCCDTR:
		sClrDTR(&rp->rp_channel);
		break;

	case TIOCMSET:
		arg = *(int *) data;
		flags = 0;
		if(arg & TIOCM_RTS)
			flags |= SET_RTS;
		if(arg & TIOCM_DTR)
			flags |= SET_DTR;
		rp->rp_channel.TxControl[3] =
			((rp->rp_channel.TxControl[3]
			& ~(SET_RTS | SET_DTR)) | flags);
		sOutDW(rp->rp_channel.IndexAddr,
			*(DWord_t *) &(rp->rp_channel.TxControl[0]));
		break;
	case TIOCMBIS:
		arg = *(int *) data;
		flags = 0;
		if(arg & TIOCM_RTS)
			flags |= SET_RTS;
		if(arg & TIOCM_DTR)
			flags |= SET_DTR;
			rp->rp_channel.TxControl[3] |= flags;
		sOutDW(rp->rp_channel.IndexAddr,
			*(DWord_t *) &(rp->rp_channel.TxControl[0]));
		break;
	case TIOCMBIC:
		arg = *(int *) data;
		flags = 0;
		if(arg & TIOCM_RTS)
			flags |= SET_RTS;
		if(arg & TIOCM_DTR)
			flags |= SET_DTR;
		rp->rp_channel.TxControl[3] &= ~flags;
		sOutDW(rp->rp_channel.IndexAddr,
			*(DWord_t *) &(rp->rp_channel.TxControl[0]));
		break;


	case TIOCMGET:
		ChanStatus = sGetChanStatusLo(&rp->rp_channel);
		flags = rp->rp_channel.TxControl[3];
		result = TIOCM_LE; /* always on while open for some reason */
		result |= (((flags & SET_DTR) ? TIOCM_DTR : 0)
			| ((flags & SET_RTS) ? TIOCM_RTS : 0)
			| ((ChanStatus & CD_ACT) ? TIOCM_CAR : 0)
			| ((ChanStatus & DSR_ACT) ? TIOCM_DSR : 0)
			| ((ChanStatus & CTS_ACT) ? TIOCM_CTS : 0));

		if(rp->rp_channel.RxControl[2] & RTSFC_EN)
		{
			result |= TIOCM_RTS;
		}

		*(int *)data = result;
		break;
	case TIOCMSDTRWAIT:
		error = suser(p->p_ucred, &p->p_acflag);
		if(error != 0) {
			splx(oldspl);
			return(error);
		}
		rp->dtr_wait = *(int *)data * hz/100;
		break;
	case TIOCMGDTRWAIT:
		*(int *)data = rp->dtr_wait * 100/hz;
		break;
	default:
		splx(oldspl);
		return ENOTTY;
	}
	splx(oldspl);
	return(0);
}

static struct speedtab baud_table[] = {
	B0,	0,		B50,	BRD50,		B75,	BRD75,
	B110,	BRD110, 	B134,	BRD134, 	B150,	BRD150,
	B200,	BRD200, 	B300,	BRD300, 	B600,	BRD600,
	B1200,	BRD1200,	B1800,	BRD1800,	B2400,	BRD2400,
	B4800,	BRD4800,	B9600,	BRD9600,	B19200, BRD19200,
	B38400, BRD38400,	B7200,	BRD7200,	B14400, BRD14400,
				B57600, BRD57600,	B76800, BRD76800,
	B115200, BRD115200,	B230400, BRD230400,
	-1,	-1
};

static int
rpparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct rp_port	*rp;
	CHANNEL_t	*cp;
	int	unit, i, mynor, port, umynor;               /* SG */
	int	oldspl, cflag, iflag, oflag, lflag;
	int	ospeed, flags;


   umynor = (((minor(tp->t_dev) >> 16) -1) * 32);    /* SG */
	port  = (minor(tp->t_dev) & 0x1f);                /* SG */
	mynor = (port + umynor);                          /* SG */

	unit = minor_to_unit[mynor];
	rp = rp_addr(unit) + port;
	cp = &rp->rp_channel;
	oldspl = spltty();

	cflag = t->c_cflag;
	iflag = t->c_iflag;
	oflag = t->c_oflag;
	lflag = t->c_lflag;

	ospeed = ttspeedtab(t->c_ispeed, baud_table);
	if(ospeed < 0 || t->c_ispeed != t->c_ospeed)
		return(EINVAL);

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = cflag;
	tp->t_iflag = iflag;
	tp->t_oflag = oflag;
	tp->t_lflag = lflag;

	if(t->c_ospeed == 0) {
		sClrDTR(cp);
		return(0);
	}
	rp->rp_fifo_lw = ((t->c_ospeed*2) / 1000) +1;

	/* Set baud rate ----- we only pay attention to ispeed */
	sSetDTR(cp);
	sSetRTS(cp);
	sSetBaud(cp, ospeed);

	if(cflag & CSTOPB) {
		sSetStop2(cp);
	} else {
		sSetStop1(cp);
	}

	if(cflag & PARENB) {
		sEnParity(cp);
		if(cflag & PARODD) {
			sSetOddParity(cp);
		} else {
			sSetEvenParity(cp);
		}
	}
	else {
		sDisParity(cp);
	}
	if((cflag & CSIZE) == CS8) {
		sSetData8(cp);
		rp->rp_imask = 0xFF;
	} else {
		sSetData7(cp);
		rp->rp_imask = 0x7F;
	}

	if(iflag & ISTRIP) {
		rp->rp_imask &= 0x7F;
	}

	if(cflag & CLOCAL) {
		rp->rp_intmask &= ~DELTA_CD;
	} else {
		rp->rp_intmask |= DELTA_CD;
	}

	/* Put flow control stuff here */

	if(cflag & CCTS_OFLOW) {
		sEnCTSFlowCtl(cp);
	} else {
		sDisCTSFlowCtl(cp);
	}

	if(cflag & CRTS_IFLOW) {
		rp->rp_rts_iflow = 1;
	} else {
		rp->rp_rts_iflow = 0;
	}

	if(cflag & CRTS_IFLOW) {
		sEnRTSFlowCtl(cp);
	} else {
		sDisRTSFlowCtl(cp);
	}
	rp_disc_optim(tp, t, rp);

	if((cflag & CLOCAL) || (sGetChanStatusLo(cp) & CD_ACT)) {
		tp->t_state |= TS_CARR_ON;
		wakeup(TSA_CARR_ON(tp));
	}

/*	tp->t_state |= TS_CAN_BYPASS_L_RINT;
	flags = rp->rp_channel.TxControl[3];
	if(flags & SET_DTR)
	else
	if(flags & SET_RTS)
	else
*/
	splx(oldspl);

	return(0);
}

static void
rp_disc_optim(tp, t, rp)
struct	tty	*tp;
struct	termios *t;
struct	rp_port *rp;
{
	if(!(t->c_iflag & (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP | IXON))
		&&(!(t->c_iflag & BRKINT) || (t->c_iflag & IGNBRK))
		&&(!(t->c_iflag & PARMRK)
		  ||(t->c_iflag & (IGNPAR | IGNBRK)) == (IGNPAR | IGNBRK))
		&& !(t->c_lflag & (ECHO | ICANON | IEXTEN | ISIG | PENDIN))
		&& linesw[tp->t_line].l_rint == ttyinput)
		tp->t_state |= TS_CAN_BYPASS_L_RINT;
	else
		tp->t_state &= ~TS_CAN_BYPASS_L_RINT;
}

static void
rpstart(tp)
	struct tty *tp;
{
	struct rp_port	*rp;
	CHANNEL_t	*cp;
	struct	clist	*qp;
	int	unit, i, mynor, port, umynor;               /* SG */
	char	status, ch, flags;
	int	spl, xmit_fifo_room;
	int	count, ToRecv;


   umynor = (((minor(tp->t_dev) >> 16) -1) * 32);    /* SG */
	port  = (minor(tp->t_dev) & 0x1f);                /* SG */
	mynor = (port + umynor);                          /* SG */
	unit = minor_to_unit[mynor];
	rp = rp_addr(unit) + port;
	cp = &rp->rp_channel;
	flags = rp->rp_channel.TxControl[3];
	spl = spltty();

	if(tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
		splx(spl);
		return;
	}
	if(rp->rp_xmit_stopped) {
		sEnTransmit(cp);
		rp->rp_xmit_stopped = 0;
	}
	count = sGetTxCnt(cp);

	if(tp->t_outq.c_cc == 0) {
		if((tp->t_state & TS_BUSY) && (count == 0)) {
			tp->t_state &= ~TS_BUSY;
		}
		ttwwakeup(tp);
		splx(spl);
		return;
	}
	xmit_fifo_room = TXFIFO_SIZE - sGetTxCnt(cp);
	qp = &tp->t_outq;
	count = 0;
	if(xmit_fifo_room > 0 && qp->c_cc > 0) {
		tp->t_state |= TS_BUSY;
	}
	while(xmit_fifo_room > 0 && qp->c_cc > 0) {
		ch = getc(qp);
		sOutB(sGetTxRxDataIO(cp), ch);
		xmit_fifo_room--;
		count++;
	}
	rp->rp_restart = (qp->c_cc > 0) ? rp->rp_fifo_lw : 0;

	ttwwakeup(tp);
	splx(spl);
}

static
void
rpstop(tp, flag)
	register struct tty *tp;
	int	flag;
{
	struct rp_port	*rp;
	CHANNEL_t	*cp;
	struct	clist	*qp;
	int	unit, mynor, port, umynor;                  /* SG */
	char	status, ch;
	int	spl, xmit_fifo_room;
	int	i, count;

   umynor = (((minor(tp->t_dev) >> 16) -1) * 32);    /* SG */
	port  = (minor(tp->t_dev) & 0x1f);                /* SG */
	mynor = (port + umynor);                          /* SG */
	unit = minor_to_unit[mynor];
	rp = rp_addr(unit) + port;
	cp = &rp->rp_channel;

	spl = spltty();

	if(tp->t_state & TS_BUSY) {
		if((tp->t_state&TS_TTSTOP) == 0) {
			sFlushTxFIFO(cp);
		} else {
			if(rp->rp_xmit_stopped == 0) {
				sDisTransmit(cp);
				rp->rp_xmit_stopped = 1;
			}
		}
	}
	splx(spl);
	rpstart(tp);
}

int
rpselect(dev, flag, p)
	dev_t dev;
	int flag;
	struct proc *p;
{
	return(0);
}

struct tty *
rpdevtotty(dev_t dev)
{
	struct	rp_port *rp;
	int	unit, i, port, mynor, umynor;         /* SG */

   umynor = (((minor(dev) >> 16) -1) * 32);    /* SG */
	port  = (minor(dev) & 0x1f);                /* SG */
	mynor = (port + umynor);                    /* SG */
   unit = minor_to_unit[mynor];                /* SG */

	if(IS_CONTROL(dev))
		return(NULL);
	rp = rp_addr(unit) + port;
	return(rp->rp_tty);
}
