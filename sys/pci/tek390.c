/***********************************************************************
 *	FILE NAME : TEK390.C					       *
 *	     BY   : C.L. Huang	(ching@tekram.com.tw)		       *
 *	Description: Device Driver for Tekram DC-390(T) PCI SCSI       *
 *		     Bus Master Host Adapter			       *
 * (C)Copyright 1995-1996 Tekram Technology Co., Ltd.		       *
 ***********************************************************************/
/***********************************************************************
 *	HISTORY:						       *
 *								       *
 *	REV#	DATE	NAME	DESCRIPTION			       *
 *	1.00  07/02/96	CLH	First release for RELEASE-2.1.0        *
 *	1.01  08/20/96	CLH	Update for RELEASE-2.1.5	       *
 *								       *
 ***********************************************************************/

/**************************************************************************
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 **************************************************************************/

/**************************************************************************/
/* Imported into FreeBSD source repository, and updated to compile under  */
/* FreeBSD-3.0-DEVELOPMENT, by Stefan Esser <se@FreeBSD.Org>, 1996-12-17  */
/**************************************************************************/

/* #define REL_2_1_0 */
#define REL_2_1_5

#define DC390_DEBUG

#include <stddef.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>

#ifdef KERNEL
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>

#include <machine/clock.h>
#include <machine/cpu.h> /* bootverbose */

#include <vm/vm.h>
#include <vm/vm_extern.h>
#endif /* KERNEL */

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/clock.h>

#include "pci/tek390.h"

#define INT32	  int32
#define U_INT32   u_int32
#define TIMEOUT   (timeout_func_t)


#define OutB(val, port) 		outb(port, val)
#define OutW(val, port) 		outw(port, val)
#define OutL(val, port) 		outl(port, val)

#define PCI_DEVICE_ID_AMD53C974 	0x20201022ul
#define PCI_BASE_ADDR0			0x10


#ifdef	REL_2_1_0
static int  DC390_Interrupt (PACB pACB);
#endif
#ifdef	REL_2_1_5
static void DC390_Interrupt (PACB pACB);
#endif
static USHORT DC390_StartSCSI( PACB pACB, PDCB pDCB, PSRB pSRB );
static void DC390_DataOut_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_DataIn_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_Command_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_Status_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_MsgOut_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_MsgIn_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_DataOutPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_DataInPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_CommandPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_StatusPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_MsgOutPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_MsgInPhase( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_Nop_0( PACB pACB, PSRB pSRB, PUCHAR psstatus);
static void DC390_Nop_1( PACB pACB, PSRB pSRB, PUCHAR psstatus);

static void SetXferRate( PACB pACB, PDCB pDCB );
static void DC390_Disconnect( PACB pACB );
static void DC390_Reselect( PACB pACB );
static void SRBdone( PACB pACB, PDCB pDCB, PSRB pSRB );
static void DoingSRB_Done( PACB pACB );
static void DC390_ScsiRstDetect( PACB pACB );
static void DC390_ResetSCSIBus( PACB pACB );
static void RequestSense( PACB pACB, PDCB pDCB, PSRB pSRB );
static void EnableMsgOut2( PACB pACB, PSRB pSRB );
static void EnableMsgOut( PACB pACB, PSRB pSRB );
static void DC390_InvalidCmd( PACB pACB );

static void DC390_timeout( void *arg1);
static void DC390_reset (PACB pACB);
static PUCHAR  phystovirt( PSRB pSRB, ULONG xferCnt );

void   DC390_initDCB( PACB pACB, PDCB pDCB, PSCSICMD cmd );
void   DC390_initSRB( PSRB psrb );
void   DC390_linkSRB( PACB pACB );
void   DC390_initACB( PACB pACB, ULONG io_port, UCHAR Irq, USHORT index );
int    DC390_initAdapter( PACB pACB, ULONG io_port, UCHAR Irq, USHORT index,
			  pcici_t config_id );
void   DC390_EnableCfg( USHORT mechnum, UCHAR regval );
void   DC390_DisableCfg( USHORT mechnum );
UCHAR  DC390_inByte( USHORT mechnum, UCHAR regval );
USHORT DC390_inWord( USHORT mechnum, UCHAR regval );
ULONG  DC390_inDword(USHORT mechnum, UCHAR regval );
void   DC390_OutB(USHORT mechnum, UCHAR regval, UCHAR bval );
void   DC390_EnDisableCE( UCHAR mode, USHORT mechnum, PUCHAR regval );
void   DC390_EEpromOutDI( USHORT mechnum, PUCHAR regval, USHORT Carry );
UCHAR  DC390_EEpromInDO( USHORT mechnum );
USHORT EEpromGetData1( USHORT mechnum );
void   DC390_Prepare( USHORT mechnum, PUCHAR regval, UCHAR EEpromCmd );
void   DC390_ReadEEprom( USHORT mechnum, USHORT index );
USHORT DC390_DefaultEEprom( USHORT mechnum, USHORT index );
USHORT DC390_CheckEEpromCheckSum( USHORT MechNum, USHORT index );
USHORT DC390_ToMech( USHORT Mechnum, pcici_t config_id );


#ifdef KERNEL

static	char*	trmamd_probe( pcici_t tag, pcidi_t type);
static	void	trmamd_attach( pcici_t tag, int unit);

#ifdef	REL_2_1_0
static	int32	trmamd_scsi_cmd( struct scsi_xfer *sx);
#endif

#ifdef	REL_2_1_5
static	int32_t trmamd_scsi_cmd( struct scsi_xfer *sx);
#endif

static	void	trmamd_min_phys( struct buf *pbuf);

#ifdef	REL_2_1_0
static	u_int32 trmamd_info( int unit );
#endif

#endif /* KERNEL */


static u_long	trmamd_count;

struct	pci_device   trmamd_device = {
	"trmamd",
	trmamd_probe,
	trmamd_attach,
	&trmamd_count,
	NULL
};

DATA_SET (pcidevice_set, trmamd_device);



struct scsi_adapter trmamd_switch =
{
	trmamd_scsi_cmd,
	trmamd_min_phys,
	0,
	0,
#ifdef	REL_2_1_0
	trmamd_info,
#endif
#ifdef	REL_2_1_5
	0,
#endif
	"trmamd",
};

struct scsi_device trmamd_dev =
{
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
	"trmamd",
};


static PACB	pACB0[MAX_ADAPTER_NUM]={0};
static PACB	pACB_start= NULL;
static PACB	pACB_current = NULL;
static PDCB	pPrevDCB = NULL;
static USHORT	adapterCnt = 0;
static USHORT	CurrSyncOffset = 0;
static USHORT	mech2Agent;
static ULONG	mech1addr;
static UCHAR	mech2bus, mech2CfgSPenR, CurrentID, CurrentLUN;

static PVOID DC390_phase0[]={
       DC390_DataOut_0,
       DC390_DataIn_0,
       DC390_Command_0,
       DC390_Status_0,
       DC390_Nop_0,
       DC390_Nop_0,
       DC390_MsgOut_0,
       DC390_MsgIn_0,
       DC390_Nop_1
       };

static PVOID DC390_phase1[]={
       DC390_DataOutPhase,
       DC390_DataInPhase,
       DC390_CommandPhase,
       DC390_StatusPhase,
       DC390_Nop_0,
       DC390_Nop_0,
       DC390_MsgOutPhase,
       DC390_MsgInPhase,
       DC390_Nop_1,
       };

UCHAR  eepromBuf[MAX_ADAPTER_NUM][128];


UCHAR  clock_period1[] = {4, 5, 6, 7 ,8, 10, 13, 20};

UCHAR  baddevname1[2][28] ={
       "SEAGATE ST3390N  ???    9546",
       "HP      C3323-300       4269"};

#define BADDEVCNT	2


/***********************************************************************
 *
 *
 *
 **********************************************************************/
static PSRB
GetSRB( PACB pACB )
{
    int    flags;
    PSRB   pSRB;

    flags = splbio();

    pSRB = pACB->pFreeSRB;
    if( pSRB )
    {
	pACB->pFreeSRB = pSRB->pNextSRB;
	pSRB->pNextSRB = NULL;
    }
    splx(flags);
    return( pSRB );
}


static void
RewaitSRB0( PDCB pDCB, PSRB pSRB )
{
    PSRB   psrb1;
    int    flags;

    flags = splbio();

    if( (psrb1 = pDCB->pWaitingSRB) )
    {
	pSRB->pNextSRB = psrb1;
	pDCB->pWaitingSRB = pSRB;
    }
    else
    {
	pSRB->pNextSRB = NULL;
	pDCB->pWaitingSRB = pSRB;
	pDCB->pWaitLast = pSRB;
    }
    splx(flags);
}


static void
RewaitSRB( PDCB pDCB, PSRB pSRB )
{
    PSRB   psrb1;
    int    flags;
    UCHAR  bval;

    flags = splbio();

    pDCB->GoingSRBCnt--;
    psrb1 = pDCB->pGoingSRB;
    if( pSRB == psrb1 )
    {
	pDCB->pGoingSRB = psrb1->pNextSRB;
    }
    else
    {
	while( pSRB != psrb1->pNextSRB )
	    psrb1 = psrb1->pNextSRB;
	psrb1->pNextSRB = pSRB->pNextSRB;
	if( pSRB == pDCB->pGoingLast )
	    pDCB->pGoingLast = psrb1;
    }
    if( (psrb1 = pDCB->pWaitingSRB) )
    {
	pSRB->pNextSRB = psrb1;
	pDCB->pWaitingSRB = pSRB;
    }
    else
    {
	pSRB->pNextSRB = NULL;
	pDCB->pWaitingSRB = pSRB;
	pDCB->pWaitLast = pSRB;
    }

    bval = pSRB->TagNumber;
    pDCB->TagMask &= (~(1 << bval));	  /* Free TAG number */
    splx(flags);
}


static void
DoWaitingSRB( PACB pACB )
{
    int    flags;
    PDCB   ptr, ptr1;
    PSRB   pSRB;

    flags = splbio();

    if( !(pACB->pActiveDCB) && !(pACB->ACBFlag & (RESET_DETECT+RESET_DONE+RESET_DEV) ) )
    {
	ptr = pACB->pDCBRunRobin;
	if( !ptr )
	{
	    ptr = pACB->pLinkDCB;
	    pACB->pDCBRunRobin = ptr;
	}
	ptr1 = ptr;
	for( ;ptr1; )
	{
	    pACB->pDCBRunRobin = ptr1->pNextDCB;
	    if( !( ptr1->MaxCommand > ptr1->GoingSRBCnt ) ||
		!( pSRB = ptr1->pWaitingSRB ) )
	    {
		if(pACB->pDCBRunRobin == ptr)
		    break;
		ptr1 = ptr1->pNextDCB;
	    }
	    else
	    {
		if( !DC390_StartSCSI(pACB, ptr1, pSRB) )
		{
		    ptr1->GoingSRBCnt++;
		    if( ptr1->pWaitLast == pSRB )
		    {
			ptr1->pWaitingSRB = NULL;
			ptr1->pWaitLast = NULL;
		    }
		    else
		    {
			ptr1->pWaitingSRB = pSRB->pNextSRB;
		    }
		    pSRB->pNextSRB = NULL;

		    if( ptr1->pGoingSRB )
			ptr1->pGoingLast->pNextSRB = pSRB;
		    else
			ptr1->pGoingSRB = pSRB;
		    ptr1->pGoingLast = pSRB;
		}
		break;
	    }
	}
    }
    splx(flags);
    return;
}


static void
SRBwaiting( PDCB pDCB, PSRB pSRB)
{
    if( pDCB->pWaitingSRB )
    {
	pDCB->pWaitLast->pNextSRB = pSRB;
	pDCB->pWaitLast = pSRB;
	pSRB->pNextSRB = NULL;
    }
    else
    {
	pDCB->pWaitingSRB = pSRB;
	pDCB->pWaitLast = pSRB;
    }
}


static void
SendSRB( PSCSICMD pcmd, PACB pACB, PSRB pSRB )
{
    int    flags;
    PDCB   pDCB;

    flags = splbio();

    pDCB = pSRB->pSRBDCB;
    if( !(pDCB->MaxCommand > pDCB->GoingSRBCnt) || (pACB->pActiveDCB) ||
	(pACB->ACBFlag & (RESET_DETECT+RESET_DONE+RESET_DEV)) )
    {
	SRBwaiting(pDCB, pSRB);
	goto SND_EXIT;
    }

    if( pDCB->pWaitingSRB )
    {
	SRBwaiting(pDCB, pSRB);
/*	pSRB = GetWaitingSRB(pDCB); */
	pSRB = pDCB->pWaitingSRB;
	pDCB->pWaitingSRB = pSRB->pNextSRB;
	pSRB->pNextSRB = NULL;
    }

    if( !DC390_StartSCSI(pACB, pDCB, pSRB) )
    {
	pDCB->GoingSRBCnt++;
	if( pDCB->pGoingSRB )
	{
	    pDCB->pGoingLast->pNextSRB = pSRB;
	    pDCB->pGoingLast = pSRB;
	}
	else
	{
	    pDCB->pGoingSRB = pSRB;
	    pDCB->pGoingLast = pSRB;
	}
    }
    else
	RewaitSRB0( pDCB, pSRB );

SND_EXIT:
    splx(flags);
    return;
}


/***********************************************************************
 * Function : static int32 dc390_scsi_cmd (struct scsi_xfer *cmd)
 * Purpose : enqueues a SCSI command
 ***********************************************************************/

#ifdef	REL_2_1_0
int32
#endif
#ifdef	REL_2_1_5
int32_t
#endif
trmamd_scsi_cmd ( PSCSICMD cmd )
{
    USHORT ioport, i;
    PSCSICMD  pcmd;
    PSCLINK   plink;
    PACB   pACB;
    PDCB   pDCB;
    PSRB   pSRB;
    int    flags, cflags, unit, CurrPgVaddr;
    ULONG  sglen, pglen, datalen, CurrPgPaddr, NextPgPaddr;
    PUCHAR ptr,ptr1;
    PSEG   psg;
    UCHAR  sgc, sstatus;

    plink = cmd->sc_link;
    unit = plink->adapter_unit;
    pACB = pACB0[unit];
    ioport = pACB->IOPortBase;

#ifdef DC390_DEBUG0
	printf("Cmd=%2x,ID=%d,LUN=%d,",cmd->cmd->opcode,
		plink->target, plink->lun);
#endif

    if( pACB->scan_devices )
    {
	if( (plink->target >= CurrentID) && (plink->lun >= CurrentLUN) )
	{
	    CurrentID = plink->target;
	    CurrentLUN = plink->lun;
	}
	else
	{
	    pACB->scan_devices = 0;
	    pPrevDCB->pNextDCB = pACB->pLinkDCB;
	}
    }

    if ( ( plink->target > pACB->max_id ) || ( plink->lun > pACB->max_lun ) )
    {
#ifdef DC390_DEBUG0
	printf("DC390: Ignore target %d lun %d\n",
		plink->target, plink->lun);
#endif
	cmd->error = XS_DRIVER_STUFFUP;
	return( COMPLETE );
    }

    if( (pACB->scan_devices) && !(pACB->DCBmap[plink->target] & (1 << plink->lun)) )
    {
	if( pACB->DeviceCnt < MAX_DEVICES )
	{
	    pACB->DCBmap[plink->target] |= (1 << plink->lun);
	    pDCB = pACB->pDCB_free;
#ifdef DC390_DEBUG0
	    printf("pDCB=%8x,ID=%2x,", (UINT) pDCB, plink->target);
#endif
	    DC390_initDCB( pACB, pDCB, cmd );
	}
	else	/* ???? */
	{
#ifdef DC390_DEBUG0
	    printf("DC390: Ignore target %d lun %d\n",
		    plink->target, plink->lun);
#endif
	    cmd->error = XS_DRIVER_STUFFUP;
	    return( COMPLETE );
	}
    }
    else if( !(pACB->scan_devices) && !(pACB->DCBmap[plink->target] & (1 << plink->lun)) )
    {
#ifdef DC390_DEBUG0
	printf("DC390: Ignore target %d lun %d\n",
		plink->target, plink->lun);
#endif
	cmd->error = XS_DRIVER_STUFFUP;
	return( COMPLETE );
    }
    else
    {
	pDCB = pACB->pLinkDCB;
	while( (pDCB->UnitSCSIID != plink->target) ||
	       (pDCB->UnitSCSILUN != plink->lun) )
	{
	    pDCB = pDCB->pNextDCB;
	}
#ifdef DC390_DEBUG0
	    printf("pDCB=%8x,ID=%2x,", (UINT) pDCB, plink->target);
#endif
    }

    cflags = cmd->flags;
    if(cflags & SCSI_RESET)
    {
	DC390_reset (pACB);
	cmd->error = XS_NOERROR;
	return(COMPLETE);
    }

    if( cflags & ITSDONE )
    {
	printf("DC390: Is it done?\n");
	cmd->flags &= ~ITSDONE;
    }
    if( !(cflags & INUSE) )
    {
	printf("DC390: In Use?\n");
	cmd->flags |= INUSE;
    }

    cmd->error = 0;
    cmd->resid = 0;

    flags = splbio();

    pcmd = cmd;

    pSRB = GetSRB( pACB );

    if( !pSRB )
    {
	pcmd->error = XS_DRIVER_STUFFUP;
	splx(flags);
	return( TRY_AGAIN_LATER);
    }

/*  BuildSRB(pSRB); */

    pSRB->pSRBDCB = pDCB;
    pSRB->pcmd = pcmd;
    ptr = (PUCHAR) pSRB->CmdBlock;
    ptr1 = (PUCHAR) pcmd->cmd;
    pSRB->ScsiCmdLen = pcmd->cmdlen;
    for(i=0; i< pcmd->cmdlen; i++)
    {
	*ptr = *ptr1;
	ptr++;
	ptr1++;
    }
    if( pcmd->datalen )
    {
	psg = (PSEG) &pSRB->SGsegment[0];
	pSRB->pSegmentList = psg;
	sgc = 0;

	/* Set up the scatter gather list */
	datalen = pcmd->datalen;
	CurrPgVaddr = (int) pcmd->data;
	CurrPgPaddr = vtophys(CurrPgVaddr);

	while ((datalen) && (sgc < MAX_SG_ENTRY))
	{
	    sglen = 0;
	    psg->SGXPtr = CurrPgPaddr;
	    NextPgPaddr = CurrPgPaddr;
	    while ((datalen) && (CurrPgPaddr == NextPgPaddr))
	    {
		/*
		 * This page is contiguous (physically) with the the last,
		 * just extend the length
		 */

		NextPgPaddr = (CurrPgPaddr & (~(PAGELEN - 1))) + PAGELEN;
		pglen = NextPgPaddr - CurrPgPaddr;

		if( datalen < pglen )
		    pglen = datalen;
		sglen += pglen;
		datalen -= pglen;
		CurrPgVaddr = (CurrPgVaddr & (~(PAGELEN - 1))) + PAGELEN;
		if( datalen )
		    CurrPgPaddr = vtophys(CurrPgVaddr);
	    }
	    /*
	       next page isn't contiguous, finish this segment
	     */
	    psg->SGXLen = sglen;
	    psg++;
	    sgc++;
	}
	pSRB->SGcount = sgc;

	if (datalen)
	{
	    printf("DC390: Out Of Segment Buffer!\n");
	    pSRB->pNextSRB = pACB->pFreeSRB;
	    pACB->pFreeSRB = pSRB;
	    pcmd->error = XS_DRIVER_STUFFUP;
	    splx(flags);
	    return (HAD_ERROR);
	}
    }
    else
	pSRB->SGcount = 0;

    pSRB->SGIndex = 0;
    pSRB->AdaptStatus = 0;
    pSRB->TargetStatus = 0;
    pSRB->MsgCnt = 0;
    if( pDCB->DevType != SCSI_SEQACESS )
	pSRB->RetryCnt = 1;
    else
	pSRB->RetryCnt = 0;
    pSRB->SRBStatus = 0;
    pSRB->SRBFlag = 0;
    pSRB->SRBState = 0;
    pSRB->TotalXferredLen = 0;
    pSRB->SGPhysAddr = 0;
    pSRB->SGToBeXferLen = 0;
    pSRB->ScsiPhase = 0;
    pSRB->EndMessage = 0;
    splx(flags);

    if( !(cflags & SCSI_NOMASK) )
    {
	flags = splbio();
	SendSRB( pcmd, pACB, pSRB );
	timeout(DC390_timeout, (caddr_t)pSRB, (pcmd->timeout * hz)/1000);
	splx(flags);
	return( SUCCESSFULLY_QUEUED);
    }
    else
    {
	SendSRB( pcmd, pACB, pSRB );
	do
	{
	    while(--pcmd->timeout)
	    {
		DELAY(1000);
		sstatus = inb( ioport+Scsi_Status );
		if( sstatus & INTERRUPT )
		    break;
	    }
	    if( pcmd->timeout == 0 )
	    {
		return(HAD_ERROR);
	    }
	    else
	    {
		DC390_Interrupt( pACB );
	    }
	}
	while( !(pcmd->flags & ITSDONE) );
	if( pcmd->error == XS_TIMEOUT)
	    return(HAD_ERROR);
	else
	    return(COMPLETE);
    }
}


void
trmamd_min_phys( struct buf *bp )
{
    if (bp->b_bcount > ((MAX_SG_ENTRY - 1) * PAGELEN))
	bp->b_bcount = ((MAX_SG_ENTRY - 1) * PAGELEN);
}


#ifdef	REL_2_1_0
u_int32
trmamd_info( int unit )
{
	return (MAX_CMD_PER_LUN);	/* outstanding requests at a time per device */
}
#endif


static PUCHAR  phystovirt( PSRB pSRB, ULONG xferCnt )
{
    int  dataPtr;
    PSCSICMD  pcmd;
    UCHAR     i;
    PSEG      pseg;

    pcmd = pSRB->pcmd;
    dataPtr = (int) pcmd->data;
    pseg = pSRB->SGsegment;
    for(i=0; i < pSRB->SGIndex; i++)
    {
	dataPtr += (int) pseg->SGXLen;
	pseg++;
    }
    dataPtr += (int) xferCnt;
    return( (PUCHAR) dataPtr);
}


/***********************************************************************
 * Function : int DC390_abort (SCSICMD *cmd)
 *
 * Purpose : Abort an errant SCSI command
 *
 * Inputs : cmd - command to abort
 *
 * Returns : 0 on success, -1 on failure.
 ***********************************************************************/
/*
int
DC390_abort (SCSICMD *cmd)
{
    int   flags;
    PACB  pACB;
    PDCB  pDCB, pdcb;
    PSRB  pSRB, psrb;
    USHORT count, i;
    PSCSICMD  pcmd, pcmd1;
    int   status;


#ifdef DC390_DEBUG0
    printf("DC390 : Abort Cmd.");
#endif

    flags = splbio();

    pACB = (PACB) cmd->host->hostdata;
    pDCB = pACB->pLinkDCB;
    pdcb = pDCB;
    while( (pDCB->UnitSCSIID != cmd->sc_link->target) ||
	   (pDCB->UnitSCSILUN != cmd->sc_link->lun) )
    {
	pDCB = pDCB->pNextDCB;
	if( pDCB == pdcb )
	    goto  NOT_RUN;
    }


    pSRB = pDCB->pWaitingSRB;
    if( !pSRB )
	goto  ON_GOING;
    if( pSRB->pcmd == cmd )
    {
	pDCB->pWaitingSRB = pSRB->pNextSRB;
	goto  IN_WAIT;
    }
    else
    {
	psrb = pSRB;
	while( psrb->pNextSRB->pcmd != cmd )
	{
	    psrb = psrb->pNextSRB;
	    if( !psrb )
		goto ON_GOING;
	}
	pSRB = psrb->pNextSRB;
	psrb->pNextSRB = pSRB->pNextSRB;
	if( pSRB == pDCB->pWaitLast )
	    pDCB->pWaitLast = psrb;
IN_WAIT:
	pSRB->pNextSRB = pACB->pFreeSRB;
	pACB->pFreeSRB = pSRB;
	cmd->next = NULL;
	status = SCSI_ABORT_SUCCESS;
	goto  ABO_X;
    }

ON_GOING:
    pSRB = pDCB->pGoingSRB;
    for( count = pDCB->GoingSRBCnt, i=0; i<count; i++)
    {
	if( pSRB->pcmd != cmd )
	    pSRB = pSRB->pNextSRB;
	else
	{
	    if( (pACB->pActiveDCB == pDCB) && (pDCB->pActiveSRB == pSRB) )
	    {
		status = SCSI_ABORT_BUSY;
		goto  ABO_X;
	    }
	    else
	    {
		status = SCSI_ABORT_SNOOZE;
		goto  ABO_X;
	    }
	}
    }

NOT_RUN:
    status = SCSI_ABORT_NOT_RUNNING;

ABO_X:
    cmd->error = XS_NOERROR;
    scsi_done(cmd);
    splx(flags);
    return( status );
}
*/

static void
ResetDevParam( PACB pACB )
{
    PDCB   pDCB, pdcb;

    pDCB = pACB->pLinkDCB;
    if( pDCB == NULL )
	return;
    pdcb = pDCB;
    do
    {
	pDCB->SyncMode &= ~SYNC_NEGO_DONE;
	pDCB->SyncPeriod = 0;
	pDCB->SyncOffset = 0;
	pDCB->CtrlR3 = FAST_CLK;
	pDCB->CtrlR4 &= NEGATE_REQACKDATA;
	pDCB->CtrlR4 |= EATER_25NS;
	pDCB = pDCB->pNextDCB;
    }
    while( pdcb != pDCB );
}


static void
RecoverSRB( PACB pACB )
{
    PDCB   pDCB, pdcb;
    PSRB   psrb, psrb2;
    USHORT cnt, i;

    pDCB = pACB->pLinkDCB;
    if( pDCB == NULL )
	return;
    pdcb = pDCB;
    do
    {
	cnt = pdcb->GoingSRBCnt;
	psrb = pdcb->pGoingSRB;
	for (i=0; i<cnt; i++)
	{
	    psrb2 = psrb;
	    psrb = psrb->pNextSRB;
/*	    RewaitSRB( pDCB, psrb ); */
	    if( pdcb->pWaitingSRB )
	    {
		psrb2->pNextSRB = pdcb->pWaitingSRB;
		pdcb->pWaitingSRB = psrb2;
	    }
	    else
	    {
		pdcb->pWaitingSRB = psrb2;
		pdcb->pWaitLast = psrb2;
		psrb2->pNextSRB = NULL;
	    }
	}
	pdcb->GoingSRBCnt = 0;
	pdcb->pGoingSRB = NULL;
	pdcb->TagMask = 0;
	pdcb = pdcb->pNextDCB;
    }
    while( pdcb != pDCB );
}


/***********************************************************************
 * Function : DC390_reset (PACB pACB)
 *
 * Purpose : perform a hard reset on the SCSI bus( and AMD chip).
 *
 * Inputs : cmd - command which caused the SCSI RESET
 *
 ***********************************************************************/

static void
DC390_reset (PACB pACB)
{
    USHORT   ioport;
    int   flags;
    UCHAR    bval;
    USHORT  i;


#ifdef DC390_DEBUG0
    printf("DC390: RESET,");
#endif

    flags = splbio();

    ioport = pACB->IOPortBase;
    bval = inb(ioport+CtrlReg1);
    bval |= DIS_INT_ON_SCSI_RST;
    OutB(bval,ioport+CtrlReg1);  /* disable interrupt */
    DC390_ResetSCSIBus( pACB );
    for( i=0; i<500; i++ )
	DELAY(1000);
    bval = inb(ioport+CtrlReg1);
    bval &= ~DIS_INT_ON_SCSI_RST;
    OutB(bval,ioport+CtrlReg1); /* re-enable interrupt */

    bval = DMA_IDLE_CMD;
    OutB(bval,ioport+DMA_Cmd);
    bval = CLEAR_FIFO_CMD;
    OutB(bval,ioport+ScsiCmd);

    ResetDevParam( pACB );
    DoingSRB_Done( pACB );
    pACB->pActiveDCB = NULL;

    pACB->ACBFlag = 0;
    DoWaitingSRB( pACB );
    splx(flags);
    return;
}


void
DC390_timeout( void *arg1)
{
    PSRB  pSRB;

    pSRB = (PSRB) arg1;
}


#include "pci/scsiiom.c"


/***********************************************************************
 * Function : static void DC390_initDCB
 *
 * Purpose :  initialize the internal structures for a given DCB
 *
 * Inputs : cmd - pointer to this scsi cmd request block structure
 *
 ***********************************************************************/
void DC390_initDCB( PACB pACB, PDCB pDCB, PSCSICMD cmd )
{
    PEEprom	prom;
    UCHAR	bval;
    USHORT	index;
    PSCLINK	plink;

    if( pACB->DeviceCnt == 0 )
    {
	pACB->pLinkDCB = pDCB;
	pACB->pDCBRunRobin = pDCB;
	pDCB->pNextDCB = pDCB;
	pPrevDCB = pDCB;
    }
    else
	pPrevDCB->pNextDCB = pDCB;

    plink = cmd->sc_link;
    pDCB->pDCBACB = pACB;
    pDCB->UnitSCSIID = plink->target;
    pDCB->UnitSCSILUN = plink->lun;
    pDCB->pWaitingSRB = NULL;
    pDCB->pGoingSRB = NULL;
    pDCB->GoingSRBCnt = 0;
    pDCB->pActiveSRB = NULL;
    pDCB->TagMask = 0;
    pDCB->MaxCommand = 1;
    pDCB->AdaptIndex = pACB->AdapterIndex;
    index = pACB->AdapterIndex;
    pDCB->DCBFlag = 0;

    prom = (PEEprom) &eepromBuf[index][plink->target << 2];
    pDCB->DevMode = prom->EE_MODE1;
    pDCB->AdpMode = eepromBuf[index][EE_MODE2];

    if( pDCB->DevMode & EN_DISCONNECT_ )
	bval = 0xC0;
    else
	bval = 0x80;
    bval |= plink->lun;
    pDCB->IdentifyMsg = bval;

    pDCB->SyncMode = 0;
    if( pDCB->DevMode & SYNC_NEGO_ )
    {
	if( !(plink->lun) || CurrSyncOffset )
	    pDCB->SyncMode = SYNC_ENABLE;
    }

    pDCB->SyncPeriod = 0;
    pDCB->SyncOffset = 0;
    pDCB->NegoPeriod = (clock_period1[prom->EE_SPEED] * 25) >> 2;

    pDCB->CtrlR1 = pACB->AdaptSCSIID;
    if( pDCB->DevMode & PARITY_CHK_ )
	pDCB->CtrlR1 |= PARITY_ERR_REPO;

    pDCB->CtrlR3 = FAST_CLK;

    pDCB->CtrlR4 = EATER_25NS;
    if( pDCB->AdpMode & ACTIVE_NEGATION)
	pDCB->CtrlR4 |= NEGATE_REQACKDATA;
}


/***********************************************************************
 * Function : static void DC390_initSRB
 *
 * Purpose :  initialize the internal structures for a given SRB
 *
 * Inputs : psrb - pointer to this scsi request block structure
 *
 ***********************************************************************/
void DC390_initSRB( PSRB psrb )
{
	psrb->PhysSRB = vtophys( psrb );
}


void DC390_linkSRB( PACB pACB )
{
    USHORT  count, i;
    PSRB    psrb;

    count = pACB->SRBCount;

    for( i=0; i< count; i++)
    {
	if( i != count - 1)
	    pACB->SRB_array[i].pNextSRB = &pACB->SRB_array[i+1];
	else
	    pACB->SRB_array[i].pNextSRB = NULL;
	psrb = (PSRB) &pACB->SRB_array[i];
	DC390_initSRB( psrb );
    }
}


/***********************************************************************
 * Function : static void DC390_initACB
 *
 * Purpose :  initialize the internal structures for a given SCSI host
 *
 * Inputs : psh - pointer to this host adapter's structure
 *
 ***********************************************************************/
void DC390_initACB( PACB pACB, ULONG io_port, UCHAR Irq, USHORT index )
{
    USHORT  i;


    pACB->max_id = 7;
    if( pACB->max_id == eepromBuf[index][EE_ADAPT_SCSI_ID] )
	pACB->max_id--;
    if( eepromBuf[index][EE_MODE2] & LUN_CHECK )
	pACB->max_lun = 7;
    else
	pACB->max_lun = 0;

    pACB->IOPortBase = (USHORT) io_port;
    pACB->pLinkDCB = NULL;
    pACB->pDCBRunRobin = NULL;
    pACB->pActiveDCB = NULL;
    pACB->pFreeSRB = pACB->SRB_array;
    pACB->SRBCount = MAX_SRB_CNT;
    pACB->AdapterIndex = index;
    pACB->status = 0;
    pACB->AdaptSCSIID = eepromBuf[index][EE_ADAPT_SCSI_ID];
    pACB->HostID_Bit = (1 << pACB->AdaptSCSIID);
    pACB->AdaptSCSILUN = 0;
    pACB->DeviceCnt = 0;
    pACB->IRQLevel = Irq;
    pACB->TagMaxNum = (eepromBuf[index][EE_TAG_CMD_NUM]) << 2;
    pACB->ACBFlag = 0;
    pACB->scan_devices = 1;
    pACB->Gmode2 = eepromBuf[index][EE_MODE2];
    if( eepromBuf[index][EE_MODE2] & LUN_CHECK )
	pACB->LUNchk = 1;
    pACB->pDCB_free = &pACB->DCB_array[0];
    DC390_linkSRB( pACB );
    pACB->pTmpSRB = &pACB->TmpSRB;
    DC390_initSRB( pACB->pTmpSRB );
    for(i=0; i<MAX_SCSI_ID; i++)
	pACB->DCBmap[i] = 0;

    pACB->ScsiLink.adapter_unit = index;
    pACB->ScsiLink.adapter_targ = pACB->AdaptSCSIID;
    pACB->ScsiLink.fordriver	= 0;
    pACB->ScsiLink.opennings = 2;
    pACB->ScsiLink.adapter	= &trmamd_switch;
    pACB->ScsiLink.device	= &trmamd_dev;
    pACB->ScsiLink.flags	= 0;
}


/***********************************************************************
 * Function : static int DC390_initAdapter
 *
 * Purpose :  initialize the SCSI chip ctrl registers
 *
 * Inputs : psh - pointer to this host adapter's structure
 *
 ***********************************************************************/
int DC390_initAdapter( PACB pACB, ULONG io_port, UCHAR Irq, USHORT index,
		       pcici_t config_id )
{
    USHORT ioport;
    UCHAR  bval;

#ifdef	CHECK_SHARE_INT
    PACB   pacb;
    USHORT used_irq = 0;

    pacb = pACB_start;
    if( pacb != NULL )
    {
	for ( ; (pacb != (PACB) -1) ; )
	{
	    if( pacb->IRQLevel == Irq )
	    {
		used_irq = 1;
		break;
	    }
	    else
		pacb = pacb->pNextACB;
	}
    }

    if( !used_irq )
    {
#endif
	if( !pci_map_int (config_id, (PVOID)DC390_Interrupt, pACB, &bio_imask) )
	{
	    if(bootverbose)
		printf("DC390: register Interrupt handler error!\n");
	    return( -1 );
	}

#ifdef	CHECK_SHARE_INT
    }
#endif

    ioport = (USHORT) io_port;
    bval = 153; 			/* 250ms selection timeout */
    OutB(bval,ioport+Scsi_TimeOut);

    bval = CLK_FREQ_40MHZ;		/* Conversion factor = 0 , 40MHz clock */
    OutB(bval,ioport+Clk_Factor);

    bval = NOP_CMD;			/* NOP cmd - clear command register */
    OutB(bval,ioport+ScsiCmd);

    bval = EN_FEATURE+EN_SCSI2_CMD;	/* Enable Feature and SCSI-2 */
    OutB(bval,ioport+CtrlReg2);

    bval = FAST_CLK;			/* fast clock */
    OutB(bval,ioport+CtrlReg3);

    bval = EATER_25NS;
    if( eepromBuf[index][EE_MODE2] & ACTIVE_NEGATION )
	 bval |= NEGATE_REQACKDATA;
    OutB(bval,ioport+CtrlReg4);

    bval = DIS_INT_ON_SCSI_RST; 	/* Disable SCSI bus reset interrupt */
    OutB(bval,ioport+CtrlReg1);

    return(0);
}


void
DC390_EnableCfg( USHORT mechnum, UCHAR regval )
{
    ULONG wlval;

    if(mechnum == 2)
    {
	OutB(mech2bus, PCI_CFG2_FORWARD_REG);
	OutB(mech2CfgSPenR, PCI_CFG2_ENABLE_REG);
    }
    else
    {
	regval &= 0xFC;
	wlval = mech1addr;
	wlval |= (((ULONG)regval) & 0xff);
	OutL(wlval, PCI_CFG1_ADDRESS_REG);
    }
}


void
DC390_DisableCfg( USHORT mechnum )
{

    if(mechnum == 2)
	OutB(0, PCI_CFG2_ENABLE_REG);
    else
	OutL(0, PCI_CFG1_ADDRESS_REG);
}


UCHAR
DC390_inByte( USHORT mechnum, UCHAR regval )
{
    UCHAR bval;
    USHORT wval;
    int   flags;

    flags = splbio();
    DC390_EnableCfg( mechnum, regval );
    if(mechnum == 2)
    {
	wval = mech2Agent;
	wval <<= 8;
	wval |= ((USHORT) regval) & 0xff;
	bval = inb(wval);
    }
    else
    {
	regval &= 3;
	bval = inb(PCI_CFG1_DATA_REG | regval);
    }
    DC390_DisableCfg(mechnum);
    splx(flags);
    return(bval);
}


USHORT
DC390_inWord( USHORT mechnum, UCHAR regval )
{
    USHORT wval;
    int   flags;

    flags = splbio();
    DC390_EnableCfg(mechnum,regval);
    if(mechnum == 2)
    {
	wval = mech2Agent;
	wval <<= 8;
	wval |= regval;
	wval = inw(wval);
    }
    else
    {
	regval &= 3;
	wval = inw(PCI_CFG1_DATA_REG | regval);
    }
    DC390_DisableCfg(mechnum);
    splx(flags);
    return(wval);
}


ULONG
DC390_inDword(USHORT mechnum, UCHAR regval )
{
    ULONG wlval;
    int   flags;
    USHORT wval;

    flags = splbio();
    DC390_EnableCfg(mechnum,regval);
    if(mechnum == 2)
    {
	wval = mech2Agent;
	wval <<= 8;
	wval |= regval;
	wlval = inl(wval);
    }
    else
    {
	wlval = inl(PCI_CFG1_DATA_REG);
    }
    DC390_DisableCfg(mechnum);
    splx(flags);
    return(wlval);
}


void
DC390_OutB(USHORT mechnum, UCHAR regval, UCHAR bval )
{

    USHORT wval;
    int    flags;

    flags = splbio();
    DC390_EnableCfg(mechnum,regval);
    if(mechnum == 2)
    {
	wval = mech2Agent;
	wval <<= 8;
	wval |= regval;
	OutB(bval, wval);
    }
    else
    {
	regval &= 3;
	OutB(bval, PCI_CFG1_DATA_REG | regval);
    }
    DC390_DisableCfg(mechnum);
    splx(flags);
}


void
DC390_EnDisableCE( UCHAR mode, USHORT mechnum, PUCHAR regval )
{

    UCHAR bval;

    bval = 0;
    if(mode == ENABLE_CE)
	*regval = 0xc0;
    else
	*regval = 0x80;
    DC390_OutB(mechnum,*regval,bval);
    if(mode == DISABLE_CE)
	DC390_OutB(mechnum,*regval,bval);
    DELAY(160);
}


void
DC390_EEpromOutDI( USHORT mechnum, PUCHAR regval, USHORT Carry )
{
    UCHAR bval;

    bval = 0;
    if(Carry)
    {
	bval = 0x40;
	*regval = 0x80;
	DC390_OutB(mechnum,*regval,bval);
    }
    DELAY(160);
    bval |= 0x80;
    DC390_OutB(mechnum,*regval,bval);
    DELAY(160);
    bval = 0;
    DC390_OutB(mechnum,*regval,bval);
    DELAY(160);
}


UCHAR
DC390_EEpromInDO( USHORT mechnum )
{
    UCHAR bval,regval;

    regval = 0x80;
    bval = 0x80;
    DC390_OutB(mechnum,regval,bval);
    DELAY(160);
    bval = 0x40;
    DC390_OutB(mechnum,regval,bval);
    DELAY(160);
    regval = 0x0;
    bval = DC390_inByte(mechnum,regval);
    if(bval == 0x22)
	return(1);
    else
	return(0);
}


USHORT
EEpromGetData1( USHORT mechnum )
{
    UCHAR i;
    UCHAR carryFlag;
    USHORT wval;

    wval = 0;
    for(i=0; i<16; i++)
    {
	wval <<= 1;
	carryFlag = DC390_EEpromInDO(mechnum);
	wval |= carryFlag;
    }
    return(wval);
}


void
DC390_Prepare( USHORT mechnum, PUCHAR regval, UCHAR EEpromCmd )
{
    UCHAR i,j;
    USHORT carryFlag;

    carryFlag = 1;
    j = 0x80;
    for(i=0; i<9; i++)
    {
	DC390_EEpromOutDI(mechnum,regval,carryFlag);
	carryFlag = (EEpromCmd & j) ? 1 : 0;
	j >>= 1;
    }
}


void
DC390_ReadEEprom( USHORT mechnum, USHORT index )
{
    UCHAR   regval,cmd;
    PUSHORT ptr;
    USHORT  i;

    ptr = (PUSHORT) &eepromBuf[index][0];
    cmd = EEPROM_READ;
    for(i=0; i<0x40; i++)
    {
	DC390_EnDisableCE(ENABLE_CE, mechnum, &regval);
	DC390_Prepare(mechnum, &regval, cmd);
	*ptr = EEpromGetData1(mechnum);
	ptr++;
	cmd++;
	DC390_EnDisableCE(DISABLE_CE,mechnum,&regval);
    }
}


USHORT
DC390_DefaultEEprom( USHORT mechnum, USHORT index )
{
    PUCHAR  ptr;
    USHORT  i;

    ptr = (PUCHAR) &eepromBuf[index][0];
    bzero (ptr, sizeof eepromBuf[index]);
    for(i=0; i<0x40; i++)
    {
	*ptr = (TAG_QUEUING_|EN_DISCONNECT_|SYNC_NEGO_|PARITY_CHK_);
	ptr += 4;
    }
    ptr[EE_ADAPT_SCSI_ID] = 7;
    ptr[EE_MODE2] = (LUN_CHECK|ACTIVE_NEGATION);
    ptr[EE_TAG_CMD_NUM] = 4;
    return 0;
}


USHORT
DC390_CheckEEpromCheckSum( USHORT MechNum, USHORT index )
{
    USHORT wval, rc, *ptr;
    UCHAR  i;

    DC390_ReadEEprom( MechNum, index );
    wval = 0;
    ptr = (PUSHORT) &eepromBuf[index][0];
    for(i=0; i<128 ;i+=2, ptr++)
	wval += *ptr;
    if( wval == 0x1234 )
	rc = 0;
    else
	rc = DC390_DefaultEEprom( MechNum, index);
    return( rc );
}


USHORT
DC390_ToMech( USHORT Mechnum, pcici_t config_id )
{

    if(Mechnum == 2)
    {
	mech2bus = config_id.cfg2.forward;	/* Bus num */
	mech2Agent = config_id.cfg2.port >> 8;	/* Dev num */
	mech2CfgSPenR = config_id.cfg2.enable;	/* Fun num */
    }
    else	/* use mech #1 method */
    {
	mech1addr = config_id.cfg1;
    }
    return(0);
}

/***********************************************************************
 * Function : static int DC390_init (struct Scsi_Host *host)
 *
 * Purpose :  initialize the internal structures for a given SCSI host
 *
 * Inputs : host - pointer to this host adapter's structure/
 *
 * Preconditions : when this function is called, the chip_type
 *	field of the pACB structure MUST have been set.
 ***********************************************************************/

static int
DC390_init( ULONG io_port, UCHAR Irq, USHORT index, USHORT MechNum,
	     pcici_t config_id)
{
    PACB  pACB;

    if( !DC390_CheckEEpromCheckSum( MechNum, index) )
    {
	pACB = (PACB) malloc (sizeof (struct _ACB), M_DEVBUF, M_WAITOK);
	if( !pACB )
	{
	    printf("DC390%d: cannot allocate ACB !\n", index);
	    return( -1 );
	}
	bzero (pACB, sizeof (struct _ACB));
	DC390_initACB( pACB, io_port, Irq, index );
	if( !DC390_initAdapter( pACB, io_port, Irq, index, config_id) )
	{
	    if( !pACB_start )
	    {
		pACB_start = pACB;
		pACB_current = pACB;
		pACB->pNextACB = (PACB) -1;
	    }
	    else
	    {
		pACB_current->pNextACB = pACB;
		pACB_current = pACB;
		pACB->pNextACB = (PACB)  -1;
	    }
	    pACB0[index] = pACB;

#ifdef DC390_DEBUG0
	printf("DC390: pACB = %8x, pDCB_array = %8x, pSRB_array = %8x\n",
	      (UINT) pACB, (UINT) pACB->DCB_array, (UINT) pACB->SRB_array);
	printf("DC390: ACB size= %4x, DCB size= %4x, SRB size= %4x\n",
	      sizeof(DC390_ACB), sizeof(DC390_DCB), sizeof(DC390_SRB) );
#endif

	    return( 0 );
	}
	else
	{
	    free( pACB, M_DEVBUF);
	    return( -1 );
	}
    }
    else
    {
	printf("DC390_init: EEPROM reading error!\n");
	return( -1 );
    }
}



void
trmamd_attach (pcici_t config_id, int unit)
{
    struct scsibus_data *scbus;
    UCHAR   irq;
    USHORT  MechNum;
    ULONG   io_port, wlval;
    PACB    pACB = 0;
    int     flags;

    if( unit >= MAX_ADAPTER_NUM )
	return;

    if( pACB0[unit] )
	return;

    CurrentID = 0;
    CurrentLUN = 0;
    MechNum = pci_mechanism;

#ifdef DC390_DEBUG0
    if(bootverbose)
	printf("DC390: Mech=%2x,\n",(UCHAR) MechNum);
#endif

    if( !DC390_ToMech( MechNum, config_id ) )
    {
	wlval = DC390_inDword( MechNum, PCI_ID_REG);
	if(wlval == PCI_DEVICE_ID_AMD53C974 )
	{
	    io_port =DC390_inDword(MechNum,PCI_BASE_ADDR0) & 0xFFFE;
	    irq = DC390_inByte( MechNum, PCI_INTERRUPT_REG);
#ifdef DC390_DEBUG0
	    if(bootverbose)
		printf("DC390: IO_PORT=%4x,IRQ=%x,\n",(UINT) io_port, irq);
#endif
	    if( !DC390_init( io_port, irq, (USHORT) unit, MechNum, config_id) )
	    {
		adapterCnt++;
	    }
	    else
		return;
	}
    }

    pACB = pACB0[unit];

/*
  Now let the generic SCSI driver look for the SCSI devices on the bus
*/

    flags = splbio();

    scbus = scsi_alloc_bus();
    if(!scbus)
    {
	splx(flags);
	return;
    }
    scbus->adapter_link = &pACB->ScsiLink;
    scbus->maxtarg = pACB->max_id;

#ifdef	DC390_DEBUG
    if(bootverbose)
	printf("\nDC390: scanning for devices ...\n\n");
#endif

    scsi_attachdevs (scbus);
    scbus = NULL;   /* Upper-level SCSI code owns this now */

#ifdef	DC390_DEBUG
    if(bootverbose)
	printf("\n\nDC390: Attach devices return\n");
#endif

    splx(flags);
}


static	char*
trmamd_probe (pcici_t tag, pcidi_t type)
{
	if( type == PCI_DEVICE_ID_AMD53C974 )
	    return ("Tekram DC390(T) Adapter Driver v1.01 Aug-20-1996");
	else
	    return (NULL);
}

