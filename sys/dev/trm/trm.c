/*
 *       O.S   : FreeBSD CAM
 *	FILE NAME  : trm.c					      
 *	     BY    : C.L. Huang 	(ching@tekram.com.tw)
 *               Erich Chen     (erich@tekram.com.tw)
 *	Description: Device Driver for Tekram DC395U/UW/F ,DC315/U 
 *		         PCI SCSI Bus Master Host Adapter	
 *               (SCSI chip set used Tekram ASIC TRM-S1040)
 * (C)Copyright 1995-1999 Tekram Technology Co., Ltd.
 */

/*
 *	HISTORY:					
 *						
 *	REV#	DATE	NAME    	DESCRIPTION	
 *  1.05   05/01/1999  ERICH CHEN  First released for 3.x.x (CAM)
 *  1.06   07/29/1999  ERICH CHEN  Modify for NEW PCI
 *  1.07   12/12/1999  ERICH CHEN  Modify for 3.3.x ,DCB no free
 *  1.08   06/12/2000  ERICH CHEN  Modify for 4.x.x 
 */

/*
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
 */

/*
 * Imported into FreeBSD source repository, and updated to compile under  
 * FreeBSD-3.0-DEVELOPMENT, by Stefan Esser <se@FreeBSD.Org>, 1996-12-17  
 */

/*
 * Updated to compile under FreeBSD 5.0-CURRENT by Olivier Houchard
 * <doginou@ci0.org>, 2002-03-04
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#if __FreeBSD_version >= 500000
#include <sys/bio.h>
#endif
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <machine/resource.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>

#include <dev/trm/trm.h>

#define trm_reg_read8(reg)	bus_space_read_1(pACB->tag, pACB->bsh, reg)
#define trm_reg_read16(reg)	bus_space_read_2(pACB->tag, pACB->bsh, reg)
#define trm_reg_read32(reg)	bus_space_read_4(pACB->tag, pACB->bsh, reg)
#define trm_reg_write8(value,reg)	bus_space_write_1(pACB->tag, pACB->bsh,\
		reg, value)
#define trm_reg_write16(value,reg)	bus_space_write_2(pACB->tag, pACB->bsh,\
		reg, value)
#define trm_reg_write32(value,reg)	bus_space_write_4(pACB->tag, pACB->bsh,\
		reg, value)

#define PCI_Vendor_ID_TEKRAM	0x1DE1
#define PCI_Device_ID_TRM_S1040	0x0391
#define PCI_DEVICEID_TRMS1040	0x03911DE1

#ifdef trm_DEBUG1
#define TRM_DPRINTF(fmt, arg...) printf("trm: " fmt, ##arg)
#else
#define TRM_DPRINTF(fmt, arg...) {}
#endif /* TRM_DEBUG */

static void	trm_check_eeprom(PNVRAMTYPE pEEpromBuf,PACB pACB);
static void	TRM_read_all(PNVRAMTYPE pEEpromBuf,PACB pACB);
static u_int8_t	TRM_get_data(PACB pACB, u_int8_t bAddr);
static void	TRM_write_all(PNVRAMTYPE pEEpromBuf,PACB pACB);
static void	TRM_set_data(PACB pACB, u_int8_t bAddr, u_int8_t bData);
static void	TRM_write_cmd(PACB pACB, u_int8_t bCmd, u_int8_t bAddr);
static void	TRM_wait_30us(PACB pACB);

static void	trm_Interrupt(void *vpACB);
static void	trm_DataOutPhase0(PACB pACB, PSRB pSRB, 
					 u_int8_t * pscsi_status);
static void	trm_DataInPhase0(PACB pACB, PSRB pSRB, 
					u_int8_t * pscsi_status);
static void	trm_CommandPhase0(PACB pACB, PSRB pSRB, 
					 u_int8_t * pscsi_status);
static void	trm_StatusPhase0(PACB pACB, PSRB pSRB, 
					u_int8_t * pscsi_status);
static void	trm_MsgOutPhase0(PACB pACB, PSRB pSRB, 
					u_int8_t * pscsi_status);
static void	trm_MsgInPhase0(PACB pACB, PSRB pSRB, 
					u_int8_t * pscsi_status);
static void	trm_DataOutPhase1(PACB pACB, PSRB pSRB, 
					 u_int8_t * pscsi_status);
static void	trm_DataInPhase1(PACB pACB, PSRB pSRB, 
					u_int8_t * pscsi_status);
static void	trm_CommandPhase1(PACB pACB, PSRB pSRB, 
					 u_int8_t * pscsi_status);
static void	trm_StatusPhase1(PACB pACB, PSRB pSRB, 
					u_int8_t * pscsi_status);
static void	trm_MsgOutPhase1(PACB pACB, PSRB pSRB, 
					u_int8_t * pscsi_status);
static void	trm_MsgInPhase1(PACB pACB, PSRB pSRB, 
					u_int8_t * pscsi_status);
static void	trm_Nop0(PACB pACB, PSRB pSRB, u_int8_t * pscsi_status);
static void	trm_Nop1(PACB pACB, PSRB pSRB, u_int8_t * pscsi_status);
static void	trm_SetXferRate(PACB pACB, PSRB pSRB,PDCB pDCB);
static void	trm_DataIO_transfer(PACB pACB, PSRB pSRB, u_int16_t ioDir);
static void	trm_Disconnect(PACB pACB);
static void	trm_Reselect(PACB pACB);
static void	trm_SRBdone(PACB pACB, PDCB pDCB, PSRB pSRB);
static void	trm_DoingSRB_Done(PACB pACB);
static void	trm_ScsiRstDetect(PACB pACB);
static void	trm_ResetSCSIBus(PACB pACB);
static void	trm_RequestSense(PACB pACB, PDCB pDCB, PSRB pSRB);
static void	trm_EnableMsgOutAbort2(PACB pACB, PSRB pSRB);
static void	trm_EnableMsgOutAbort1(PACB pACB, PSRB pSRB);
static void	trm_SendSRB(PACB pACB, PSRB pSRB);
static int	trm_probe(device_t tag);
static int	trm_attach(device_t tag);
static void	trm_reset(PACB pACB);

static u_int16_t	trm_StartSCSI(PACB pACB, PDCB pDCB, PSRB pSRB);

static int	trm_initAdapter(PACB pACB, u_int16_t unit, 
    					device_t pci_config_id);
static void	trm_initDCB(PACB pACB, PDCB pDCB, u_int16_t unit, 
    					u_int32_t i, u_int32_t j);
static void	trm_initSRB(PSRB psrb);
static void	trm_linkSRB(PACB pACB);
static void	trm_initACB(PACB pACB, u_int16_t unit);
/* CAM SIM entry points */
#define ccb_trmsrb_ptr spriv_ptr0
#define ccb_trmacb_ptr spriv_ptr1
static void	trm_action(struct cam_sim *psim, union ccb *pccb);
static void	trm_poll(struct cam_sim *psim);


static void * trm_SCSI_phase0[] = {
	trm_DataOutPhase0,    /* phase:0 */
	trm_DataInPhase0,     /* phase:1 */
	trm_CommandPhase0,    /* phase:2 */
	trm_StatusPhase0,     /* phase:3 */
	trm_Nop0,             /* phase:4 */
	trm_Nop1,             /* phase:5 */
	trm_MsgOutPhase0,     /* phase:6 */
	trm_MsgInPhase0,      /* phase:7 */
};

/*
 *
 *          stateV = (void *) trm_SCSI_phase1[phase]
 *
 */
static void * trm_SCSI_phase1[] = {
	trm_DataOutPhase1,    /* phase:0 */
	trm_DataInPhase1,     /* phase:1 */
	trm_CommandPhase1,    /* phase:2 */
	trm_StatusPhase1,     /* phase:3 */
	trm_Nop0,             /* phase:4 */
	trm_Nop1,             /* phase:5 */
	trm_MsgOutPhase1,     /* phase:6 */
	trm_MsgInPhase1,      /* phase:7 */
};


NVRAMTYPE trm_eepromBuf[MAX_ADAPTER_NUM];
/*
 *Fast20:  000	 50ns, 20.0 Mbytes/s
 *	       001	 75ns, 13.3 Mbytes/s
 *	       010	100ns, 10.0 Mbytes/s
 *	       011	125ns,  8.0 Mbytes/s
 *	       100	150ns,  6.6 Mbytes/s
 *	       101	175ns,  5.7 Mbytes/s
 *	       110	200ns,  5.0 Mbytes/s
 *	       111	250ns,  4.0 Mbytes/s
 *
 *Fast40:  000	 25ns, 40.0 Mbytes/s
 *	       001	 50ns, 20.0 Mbytes/s
 *	       010	 75ns, 13.3 Mbytes/s
 *	       011	100ns, 10.0 Mbytes/s
 *	       100	125ns,  8.0 Mbytes/s
 *	       101	150ns,  6.6 Mbytes/s
 *	       110	175ns,  5.7 Mbytes/s
 *	       111	200ns,  5.0 Mbytes/s
 */
                                             /* real period: */
u_int8_t dc395x_trm_clock_period[] = {
	12,/*  48  ns 20   MB/sec */
	18,/*  72  ns 13.3 MB/sec */
	25,/* 100  ns 10.0 MB/sec */
	31,/* 124  ns  8.0 MB/sec */
	37,/* 148  ns  6.6 MB/sec */
	43,/* 172  ns  5.7 MB/sec */
	50,/* 200  ns  5.0 MB/sec */
	62 /* 248  ns  4.0 MB/sec */
};

u_int8_t dc395x_trm_tinfo_sync_period[] = { 
	12,/* 20.0 MB/sec */
	18,/* 13.3 MB/sec */
	25,/* 10.0 MB/sec */
	31,/*  8.0 MB/sec */
	37,/*  6.6 MB/sec */
	43,/*  5.7 MB/sec */
	50,/*  5.0 MB/sec */
	62,/*  4.0 MB/sec */
};

static PSRB
trm_GetSRB(PACB pACB)
{
	int	intflag;
	PSRB	pSRB;

	intflag = splcam();
    	pSRB = pACB->pFreeSRB;
	if (pSRB) {
		pACB->pFreeSRB = pSRB->pNextSRB;
		pSRB->pNextSRB = NULL;
	}
	splx(intflag);
    	return (pSRB);
}

static void
trm_RewaitSRB0(PDCB pDCB, PSRB pSRB)
{
	PSRB	psrb1;
	int	intflag;

	intflag = splcam();
    	if ((psrb1 = pDCB->pWaitingSRB)) {
		pSRB->pNextSRB = psrb1;
		pDCB->pWaitingSRB = pSRB;
	} else {
	  	pSRB->pNextSRB = NULL;
		pDCB->pWaitingSRB = pSRB;
		pDCB->pWaitLastSRB = pSRB;
	}
	splx(intflag);
}

static void
trm_RewaitSRB(PDCB pDCB, PSRB pSRB)
{
	PSRB	psrb1;
	int	intflag;
	u_int8_t	bval;

	intflag = splcam();
    	pDCB->GoingSRBCnt--;
	psrb1 = pDCB->pGoingSRB;
	if (pSRB == psrb1)
		pDCB->pGoingSRB = psrb1->pNextSRB;
	else {
		while (pSRB != psrb1->pNextSRB)
			psrb1 = psrb1->pNextSRB;
		psrb1->pNextSRB = pSRB->pNextSRB;
		if (pSRB == pDCB->pGoingLastSRB)
			pDCB->pGoingLastSRB = psrb1;
	}
	if ((psrb1 = pDCB->pWaitingSRB)) {
		pSRB->pNextSRB = psrb1;
		pDCB->pWaitingSRB = pSRB;
	} else {
		pSRB->pNextSRB = NULL;
		pDCB->pWaitingSRB = pSRB;
		pDCB->pWaitLastSRB = pSRB;
	}
	bval = pSRB->TagNumber;
	pDCB->TagMask &= (~(1 << bval));	  /* Free TAG number */
	splx(intflag);
}

static void
trm_DoWaitingSRB(PACB pACB)
{
	int	intflag;
	PDCB	ptr, ptr1;
	PSRB	pSRB;

	intflag = splcam();
    	if (!(pACB->pActiveDCB) && 
	    !(pACB->ACBFlag & (RESET_DETECT+RESET_DONE+RESET_DEV))) {
		ptr = pACB->pDCBRunRobin;
		if (!ptr) {
			ptr = pACB->pLinkDCB;
			pACB->pDCBRunRobin = ptr;
		}
		ptr1 = ptr;
		for (;ptr1 ;) {
			pACB->pDCBRunRobin = ptr1->pNextDCB;
			if (!(ptr1->MaxCommand > ptr1->GoingSRBCnt) 
			    || !(pSRB = ptr1->pWaitingSRB)) {
				if (pACB->pDCBRunRobin == ptr)
					break;
				ptr1 = ptr1->pNextDCB;
			} else {
				if (!trm_StartSCSI(pACB, ptr1, pSRB)) {
				/* 
				 * If trm_StartSCSI return 0 :
				 * current interrupt status is interrupt enable 
				 * It's said that SCSI processor is unoccupied 
				 */
					ptr1->GoingSRBCnt++;
					if (ptr1->pWaitLastSRB == pSRB) {
						ptr1->pWaitingSRB = NULL;
						ptr1->pWaitLastSRB = NULL;
					} else
						ptr1->pWaitingSRB = pSRB->pNextSRB;
					pSRB->pNextSRB = NULL;
					if (ptr1->pGoingSRB) 
						ptr1->pGoingLastSRB->pNextSRB = pSRB;
					else
						ptr1->pGoingSRB = pSRB;
					ptr1->pGoingLastSRB = pSRB;
				}
				break;
			}
		}
	}
	splx(intflag);
	return;
}

static void
trm_SRBwaiting(PDCB pDCB, PSRB pSRB)
{
  
	if (pDCB->pWaitingSRB) {
		pDCB->pWaitLastSRB->pNextSRB = pSRB;
		pDCB->pWaitLastSRB = pSRB;
		pSRB->pNextSRB = NULL;
	} else {
		pDCB->pWaitingSRB = pSRB;
		pDCB->pWaitLastSRB = pSRB;
	}
}

static void
trm_ExecuteSRB(void *arg, bus_dma_segment_t *dm_segs, int nseg, int vp)
{
	int		flags;
	PACB		pACB;
	PSRB		pSRB;
	union ccb	*ccb;
	u_long		totalxferlen=0;

	pSRB = (PSRB)arg;
	ccb = pSRB->pccb;
	pACB = (PACB)ccb->ccb_h.ccb_trmacb_ptr;
	TRM_DPRINTF("trm_ExecuteSRB..........\n");        
	if (nseg != 0) {
		PSEG			psg;
		bus_dma_segment_t	*end_seg;
		bus_dmasync_op_t	op;

		/* Copy the segments into our SG list */
		end_seg = dm_segs + nseg;
		psg = (PSEG) &pSRB->SegmentX[0];
		pSRB->SRBSGListPointer= psg;
		while (dm_segs < end_seg) {
			psg->address = vp?(u_long)vtophys(dm_segs->ds_addr)
			  :(u_long)dm_segs->ds_addr;
			psg->length = (u_long)dm_segs->ds_len;
			totalxferlen += dm_segs->ds_len;
			psg++;
			dm_segs++;
		}
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			op = BUS_DMASYNC_PREREAD;
		} else {
			op = BUS_DMASYNC_PREWRITE;
		}
		bus_dmamap_sync(pACB->buffer_dmat, pSRB->dmamap, op);
	}
	pSRB->RetryCnt = 0;
	pSRB->SRBTotalXferLength=totalxferlen;
	pSRB->SRBSGCount = nseg;
	pSRB->SRBSGIndex = 0;
	pSRB->AdaptStatus = 0;
	pSRB->TargetStatus = 0;
	pSRB->MsgCnt = 0;
	pSRB->SRBStatus = 0;
	pSRB->SRBFlag = 0;
	pSRB->SRBState = 0;
	pSRB->ScsiPhase = PH_BUS_FREE; /* SCSI bus free Phase */

	flags = splcam();
	if (ccb->ccb_h.status != CAM_REQ_INPROG) {
		if (nseg != 0)
			bus_dmamap_unload(pACB->buffer_dmat, pSRB->dmamap);
		pSRB->pNextSRB = pACB->pFreeSRB;
		pACB->pFreeSRB = pSRB;
		xpt_done(ccb);
		splx(flags);
		return;
	}
	ccb->ccb_h.status |= CAM_SIM_QUEUED;
#if 0
	/* XXX Need a timeout handler */
	ccb->ccb_h.timeout_ch = timeout(trmtimeout, (caddr_t)srb, (ccb->ccb_h.timeout * hz) / 1000);
#endif
	trm_SendSRB(pACB, pSRB);
	splx(flags);
	return;
}

static void
trm_SendSRB(PACB pACB, PSRB pSRB)
{
	int	intflag;
	PDCB	pDCB;

	intflag = splcam();
	pDCB = pSRB->pSRBDCB;
	if (!(pDCB->MaxCommand > pDCB->GoingSRBCnt) || (pACB->pActiveDCB)
	    || (pACB->ACBFlag & (RESET_DETECT+RESET_DONE+RESET_DEV))) {
		TRM_DPRINTF("pDCB->MaxCommand=%d \n",pDCB->MaxCommand);        
		TRM_DPRINTF("pDCB->GoingSRBCnt=%d \n",pDCB->GoingSRBCnt);
		TRM_DPRINTF("pACB->pActiveDCB=%8x \n",(u_int)pACB->pActiveDCB);
		TRM_DPRINTF("pACB->ACBFlag=%x \n",pACB->ACBFlag);
	    	trm_SRBwaiting(pDCB, pSRB);
		goto SND_EXIT;
	}

	if (pDCB->pWaitingSRB) {
		trm_SRBwaiting(pDCB, pSRB);
		pSRB = pDCB->pWaitingSRB;
		pDCB->pWaitingSRB = pSRB->pNextSRB;
		pSRB->pNextSRB = NULL;
	}

	if (!trm_StartSCSI(pACB, pDCB, pSRB)) { 
	/* 
	 * If trm_StartSCSI return 0 :
	 * current interrupt status is interrupt enable 
	 * It's said that SCSI processor is unoccupied 
	 */
		pDCB->GoingSRBCnt++; /* stack waiting SRB*/
		if (pDCB->pGoingSRB) {
			pDCB->pGoingLastSRB->pNextSRB = pSRB;
			pDCB->pGoingLastSRB = pSRB;
		} else {
			pDCB->pGoingSRB = pSRB;
			pDCB->pGoingLastSRB = pSRB;
		}
	} else {
	/* 
	 * If trm_StartSCSI return 1 :
	 * current interrupt status is interrupt disreenable 
	 * It's said that SCSI processor has more one SRB need to do
	 */
		trm_RewaitSRB0(pDCB, pSRB);
	}
SND_EXIT:
	splx(intflag);
	/*
	 *	enable interrupt
	 */
	return;
}


static void
trm_action(struct cam_sim *psim, union ccb *pccb) 
{
	PACB	pACB;
	u_int	target_id,target_lun;

	CAM_DEBUG(pccb->ccb_h.path, CAM_DEBUG_TRACE, ("trm_action\n"));

	pACB = (PACB) cam_sim_softc(psim);
    	target_id  = pccb->ccb_h.target_id;
	target_lun = pccb->ccb_h.target_lun;

	switch (pccb->ccb_h.func_code) {
		case XPT_NOOP:	        	
			TRM_DPRINTF(" XPT_NOOP \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Execute the requested I/O operation 
	 	 */
		case XPT_SCSI_IO: {
			PDCB			pDCB = NULL;
			PSRB			pSRB;
			struct ccb_scsiio	*pcsio;
     
			pcsio = &pccb->csio;
			TRM_DPRINTF(" XPT_SCSI_IO \n");
			TRM_DPRINTF("trm: target_id= %d target_lun= %d \n"
			     ,target_id, target_lun);
			TRM_DPRINTF(
			    "pACB->scan_devices[target_id][target_lun]= %d \n"
			    ,pACB->scan_devices[target_id][target_lun]);
			pDCB = pACB->pDCB[target_id][target_lun];
			/*
			 * Assign an SRB and connect it with this ccb.
			 */
			pSRB = trm_GetSRB(pACB);
			if (!pSRB) {
				/* Freeze SIMQ */
				pccb->ccb_h.status = CAM_RESRC_UNAVAIL;
				xpt_done(pccb);
				return;
			}
	    		pSRB->pSRBDCB = pDCB;
	    		pccb->ccb_h.ccb_trmsrb_ptr = pSRB;
	    		pccb->ccb_h.ccb_trmacb_ptr = pACB;
		    	pSRB->pccb = pccb;
			pSRB->ScsiCmdLen = pcsio->cdb_len;
			/* 
			 * move layer of CAM command block to layer of SCSI
			 * Request Block for SCSI processor command doing
			 */
			bcopy(pcsio->cdb_io.cdb_bytes,pSRB->CmdBlock
			    ,pcsio->cdb_len);
			if ((pccb->ccb_h.flags & CAM_DIR_MASK)
			    != CAM_DIR_NONE) {
				if ((pccb->ccb_h.flags &
				      CAM_SCATTER_VALID) == 0) {
					if ((pccb->ccb_h.flags 
					      & CAM_DATA_PHYS) == 0) {
						int flags;
						int error;

						flags = splsoftvm();
						error = bus_dmamap_load(
						    pACB->buffer_dmat,
						    pSRB->dmamap,
						    pcsio->data_ptr,
						    pcsio->dxfer_len,
						    trm_ExecuteSRB,
						    pSRB,
						    0);
						if (error == EINPROGRESS) {
							xpt_freeze_simq(
							    pACB->psim,
							    1);
							pccb->ccb_h.status |=
							  CAM_RELEASE_SIMQ;
						}
						splx(flags);
					} else {   
						struct bus_dma_segment seg;

						/* Pointer to physical buffer */
						seg.ds_addr = 
						  (bus_addr_t)pcsio->data_ptr;
						seg.ds_len = pcsio->dxfer_len;
						trm_ExecuteSRB(pSRB, &seg, 1,
						    0);
					}
				} else { 
					/*  CAM_SCATTER_VALID */
					struct bus_dma_segment *segs;

					if ((pccb->ccb_h.flags &
					     CAM_SG_LIST_PHYS) == 0 ||
					     (pccb->ccb_h.flags 
					     & CAM_DATA_PHYS) != 0) {
						pSRB->pNextSRB = pACB->pFreeSRB;
						pACB->pFreeSRB = pSRB;
						pccb->ccb_h.status = 
						  CAM_PROVIDE_FAIL;
						xpt_done(pccb);
						return;
					}

					/* cam SG list is physical,
					 *  cam data is virtual 
					 */
					segs = (struct bus_dma_segment *)
					    pcsio->data_ptr;
					trm_ExecuteSRB(pSRB, segs,
					    pcsio->sglist_cnt, 1);
				}   /*  CAM_SCATTER_VALID */
			} else
				trm_ExecuteSRB(pSRB, NULL, 0, 0);
				  }
			break;
		case XPT_GDEV_TYPE:		    
			TRM_DPRINTF(" XPT_GDEV_TYPE \n");
	    		pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		case XPT_GDEVLIST:		    
			TRM_DPRINTF(" XPT_GDEVLIST \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Path routing inquiry 
	       	 * Path Inquiry CCB 
		 */
		case XPT_PATH_INQ: {
			struct ccb_pathinq *cpi = &pccb->cpi;

			TRM_DPRINTF(" XPT_PATH_INQ \n");
			cpi->version_num = 1; 
			cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
			cpi->target_sprt = 0;
			cpi->hba_misc = 0;
			cpi->hba_eng_cnt = 0;
			cpi->max_target = 15 ; 
			cpi->max_lun = pACB->max_lun;        /* 7 or 0 */
			cpi->initiator_id = pACB->AdaptSCSIID;
			cpi->bus_id = cam_sim_bus(psim);
			strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
			strncpy(cpi->hba_vid, "Tekram_TRM", HBA_IDLEN);
			strncpy(cpi->dev_name, cam_sim_name(psim), DEV_IDLEN);
			cpi->unit_number = cam_sim_unit(psim);
			cpi->ccb_h.status = CAM_REQ_CMP;
			xpt_done(pccb);
				   }
			break;
		/*
		 * Release a frozen SIM queue 
		 * Release SIM Queue 
		 */
		case XPT_REL_SIMQ:		    
			TRM_DPRINTF(" XPT_REL_SIMQ \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Set Asynchronous Callback Parameters 
		 * Set Asynchronous Callback CCB
 		 */
		case XPT_SASYNC_CB:		    
			TRM_DPRINTF(" XPT_SASYNC_CB \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Set device type information 
		 * Set Device Type CCB 
 		 */
		case XPT_SDEV_TYPE:		    
			TRM_DPRINTF(" XPT_SDEV_TYPE \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * (Re)Scan the SCSI Bus 
	 	 * Rescan the given bus, or bus/target/lun
 		 */
		case XPT_SCAN_BUS:		    
			TRM_DPRINTF(" XPT_SCAN_BUS \n");
	    		pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Get EDT entries matching the given pattern 
 		 */
		case XPT_DEV_MATCH:	    	
			TRM_DPRINTF(" XPT_DEV_MATCH \n");
	    		pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Turn on debugging for a bus, target or lun 
      		 */
		case XPT_DEBUG:	    	    
			TRM_DPRINTF(" XPT_DEBUG \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
			/*
			 * XPT_ABORT = 0x10, Abort the specified CCB 
			 * Abort XPT request CCB 
			 */
		case XPT_ABORT:             
			TRM_DPRINTF(" XPT_ABORT \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Reset the specified SCSI bus 
		 * Reset SCSI Bus CCB 
 		 */
		case XPT_RESET_BUS: {		
			int i;

			TRM_DPRINTF(" XPT_RESET_BUS \n");
	    		trm_reset(pACB);
			pACB->ACBFlag=0;
			for (i=0; i<500; i++)
				DELAY(1000);
			pccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(pccb);
				    }
			break;
		/*
		 * Bus Device Reset the specified SCSI device 
		 * Reset SCSI Device CCB 
 		 */
		case XPT_RESET_DEV:	    	
		/*
		 * Don't (yet?) support vendor
		 * specific commands.
		 */
			TRM_DPRINTF(" XPT_RESET_DEV \n");
	    		pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Terminate the I/O process 
		 * Terminate I/O Process Request CCB 
 		 */
		case XPT_TERM_IO:	    	
			TRM_DPRINTF(" XPT_TERM_IO \n");
	    		pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Scan Logical Unit 
		 */
		case XPT_SCAN_LUN:		   
			TRM_DPRINTF(" XPT_SCAN_LUN \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;

		/*
		 * Get/Set transfer rate/width/disconnection/tag queueing 
		 * settings 
		 * (GET) default/user transfer settings for the target 
	 	 */
		case XPT_GET_TRAN_SETTINGS: {
			struct	ccb_trans_settings *cts;        
			int	intflag;
			struct	trm_transinfo *tinfo;
			PDCB	pDCB;	
			
			TRM_DPRINTF(" XPT_GET_TRAN_SETTINGS \n");
	    		cts = &pccb->cts;
			pDCB = pACB->pDCB[target_id][target_lun];
			intflag = splcam();
			/*
			 * disable interrupt
			 */
			if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0) {
				/* current transfer settings */
				if (pDCB->tinfo.disc_tag & TRM_CUR_DISCENB)
					cts->flags = CCB_TRANS_DISC_ENB;
				else
					cts->flags = 0;/* no tag & disconnect */
				if (pDCB->tinfo.disc_tag & TRM_CUR_TAGENB)
					cts->flags |= CCB_TRANS_TAG_ENB;
				tinfo = &pDCB->tinfo.current;
				TRM_DPRINTF("CURRENT:  cts->flags= %2x \n",
				    cts->flags);
			} else {
		  	  /* default(user) transfer settings */
				if (pDCB->tinfo.disc_tag & TRM_USR_DISCENB)
					cts->flags = CCB_TRANS_DISC_ENB;
				else
					cts->flags = 0;
				if (pDCB->tinfo.disc_tag & TRM_USR_TAGENB)
					cts->flags |= CCB_TRANS_TAG_ENB;
				tinfo = &pDCB->tinfo.user;
				TRM_DPRINTF("USER: cts->flags= %2x \n",
					cts->flags);
			}
			cts->sync_period = tinfo->period;
			cts->sync_offset = tinfo->offset;
			cts->bus_width   = tinfo->width;
			TRM_DPRINTF("pDCB->SyncPeriod: %d  \n", 
				pDCB->SyncPeriod);
			TRM_DPRINTF("period: %d  \n", tinfo->period);
			TRM_DPRINTF("offset: %d  \n", tinfo->offset);
			TRM_DPRINTF("width: %d  \n", tinfo->width);

	    		splx(intflag);
			cts->valid = CCB_TRANS_SYNC_RATE_VALID | 
			    CCB_TRANS_SYNC_OFFSET_VALID | 
			    CCB_TRANS_BUS_WIDTH_VALID | 
			    CCB_TRANS_DISC_VALID | 
			    CCB_TRANS_TQ_VALID;
			pccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(pccb);
					    }
			break;
		/* 
		 * Get/Set transfer rate/width/disconnection/tag queueing 
		 * settings
		 * (Set) transfer rate/width negotiation settings 
		 */
		case XPT_SET_TRAN_SETTINGS: {
			struct	ccb_trans_settings *cts;
			u_int	update_type;
			int	intflag;
			PDCB	pDCB;

			TRM_DPRINTF(" XPT_SET_TRAN_SETTINGS \n");
	    		cts = &pccb->cts;
			update_type = 0;
			if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0)
				update_type |= TRM_TRANS_GOAL;
			if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0)
				update_type |= TRM_TRANS_USER;
			intflag = splcam();
	    		pDCB = pACB->pDCB[target_id][target_lun];

			if ((cts->valid & CCB_TRANS_DISC_VALID) != 0) {
			  /*ccb disc enables */
				if (update_type & TRM_TRANS_GOAL) {
					if ((cts->flags & CCB_TRANS_DISC_ENB)
					    != 0) 
				    		pDCB->tinfo.disc_tag 
						    |= TRM_CUR_DISCENB;
					else
						pDCB->tinfo.disc_tag &=
						    ~TRM_CUR_DISCENB;
				}
				if (update_type & TRM_TRANS_USER) {
					if ((cts->flags & CCB_TRANS_DISC_ENB)
					    != 0)
						pDCB->tinfo.disc_tag 
						    |= TRM_USR_DISCENB;
					else
						pDCB->tinfo.disc_tag &=
						    ~TRM_USR_DISCENB;
				}
			}
			if ((cts->valid & CCB_TRANS_TQ_VALID) != 0) {
			  /* if ccb tag q active */
				if (update_type & TRM_TRANS_GOAL) {
					if ((cts->flags & CCB_TRANS_TAG_ENB)
					    != 0)
						pDCB->tinfo.disc_tag |= 
						    TRM_CUR_TAGENB;
					else
						pDCB->tinfo.disc_tag &= 
						    ~TRM_CUR_TAGENB;
				}
				if (update_type & TRM_TRANS_USER) {
					if ((cts->flags & CCB_TRANS_TAG_ENB)
					    != 0)
				  		pDCB->tinfo.disc_tag |= 
						    TRM_USR_TAGENB;
					else
						pDCB->tinfo.disc_tag &= 
						    ~TRM_USR_TAGENB;
				}	
			}
			/* Minimum sync period factor	*/

			if ((cts->valid & CCB_TRANS_SYNC_RATE_VALID) != 0) {
				/* if ccb sync active */
				/* TRM-S1040 MinSyncPeriod = 4 clocks/byte */
				if ((cts->sync_period != 0) &&
				    (cts->sync_period < 125))
					cts->sync_period = 125;
				/* 1/(125*4) minsync 2 MByte/sec */
				if ((cts->valid & CCB_TRANS_SYNC_OFFSET_VALID)
				    != 0) {
					if (cts->sync_offset == 0)
				 		cts->sync_period = 0;
					/* TRM-S1040 MaxSyncOffset = 15 bytes*/
					if (cts->sync_offset > 15) 
						cts->sync_offset = 15;
				}
			}
			if ((update_type & TRM_TRANS_USER) != 0) {
				pDCB->tinfo.user.period = cts->sync_period;
				pDCB->tinfo.user.offset = cts->sync_offset;
				pDCB->tinfo.user.width  = cts->bus_width;
			}
			if ((update_type & TRM_TRANS_GOAL) != 0) {
				pDCB->tinfo.goal.period = cts->sync_period;
				pDCB->tinfo.goal.offset = cts->sync_offset;
				pDCB->tinfo.goal.width  = cts->bus_width;
			}
			splx(intflag);
			pccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(pccb);
			break;
					    }
		/*
		 * Calculate the geometry parameters for a device give
		 * the sector size and volume size. 
   		 */
		case XPT_CALC_GEOMETRY:	{
			struct		ccb_calc_geometry *ccg;
			u_int32_t	size_mb;
			u_int32_t	secs_per_cylinder;
			int		extended;

			TRM_DPRINTF(" XPT_CALC_GEOMETRY \n");
			ccg = &pccb->ccg;
			size_mb = ccg->volume_size / 
			    ((1024L * 1024L) / ccg->block_size);
			extended =  1;		
			if (size_mb > 1024 && extended) {
				ccg->heads = 255;
				ccg->secs_per_track = 63;
			} else {
				ccg->heads = 64;
				ccg->secs_per_track = 32;
			}
			secs_per_cylinder = ccg->heads * ccg->secs_per_track;
			ccg->cylinders = ccg->volume_size / secs_per_cylinder;
			pccb->ccb_h.status = CAM_REQ_CMP;
			xpt_done(pccb);
					}
			break;
		case XPT_ENG_INQ:           
			TRM_DPRINTF(" XPT_ENG_INQ \n");
	    		pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * HBA execute engine request 
		 * This structure must match SCSIIO size 
		 */
		case XPT_ENG_EXEC:		    
			TRM_DPRINTF(" XPT_ENG_EXEC \n");
	    		pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * XPT_EN_LUN = 0x30, Enable LUN as a target 
		 * Target mode structures. 
	 	 */
		case XPT_EN_LUN:            
		/*
		 * Don't (yet?) support vendor
		 * specific commands.
		 */
			TRM_DPRINTF(" XPT_EN_LUN \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		* Execute target I/O request 
		*/
		case XPT_TARGET_IO:		    
		/*
		 * Don't (yet?) support vendor
		 * specific commands.
		 */
			TRM_DPRINTF(" XPT_TARGET_IO \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Accept Host Target Mode CDB 
       		 */
		case XPT_ACCEPT_TARGET_IO:	
		/*
		 * Don't (yet?) support vendor
		 * specific commands.
		 */
			TRM_DPRINTF(" XPT_ACCEPT_TARGET_IO \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Continue Host Target I/O Connection 
 		 */
		case XPT_CONT_TARGET_IO:  	
		/*
		 * Don't (yet?) support vendor
		 * specific commands.
		 */
			TRM_DPRINTF(" XPT_CONT_TARGET_IO \n");
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Notify Host Target driver of event 
 		 */
		case XPT_IMMED_NOTIFY:	    
			TRM_DPRINTF(" XPT_IMMED_NOTIFY \n");
	    		pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * Acknowledgement of event
       		 */
		case XPT_NOTIFY_ACK:	    
			TRM_DPRINTF(" XPT_NOTIFY_ACK \n");
	    		pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		/*
		 * XPT_VUNIQUE = 0x80
		 */
		case XPT_VUNIQUE:   
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
		default:
			pccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(pccb);
			break;
	}
}

static void 
trm_poll(struct cam_sim *psim)
{       
	trm_Interrupt(cam_sim_softc(psim)); 
}

static void
trm_ResetDevParam(PACB pACB)
{
	PDCB		pDCB, pdcb;
	PNVRAMTYPE 	pEEpromBuf;
	u_int8_t	PeriodIndex;

	pDCB = pACB->pLinkDCB;
	if (pDCB == NULL)
		return;
	pdcb = pDCB;
	do {
		pDCB->SyncMode  &= ~(SYNC_NEGO_DONE+ WIDE_NEGO_DONE);
		pDCB->SyncPeriod = 0;
		pDCB->SyncOffset = 0;
		pEEpromBuf = &trm_eepromBuf[pACB->AdapterUnit];
		pDCB->DevMode = 
		  pEEpromBuf->NvramTarget[pDCB->TargetID].NvmTarCfg0;
		pDCB->AdpMode = pEEpromBuf->NvramChannelCfg;
		PeriodIndex =
		   pEEpromBuf->NvramTarget[pDCB->TargetID].NvmTarPeriod & 0x07;
		pDCB->MaxNegoPeriod = dc395x_trm_clock_period[PeriodIndex];
		if ((pDCB->DevMode & NTC_DO_WIDE_NEGO) && 
		    (pACB->Config & HCC_WIDE_CARD))
			pDCB->SyncMode |= WIDE_NEGO_ENABLE;
		pDCB = pDCB->pNextDCB;
	}
	while (pdcb != pDCB);
}

static void
trm_RecoverSRB(PACB pACB)
{
	PDCB		pDCB, pdcb;
	PSRB		psrb, psrb2;
       	u_int16_t	cnt, i;

	pDCB = pACB->pLinkDCB;
	if (pDCB == NULL)
		return;
	pdcb = pDCB;
	do {
		cnt = pdcb->GoingSRBCnt;
		psrb = pdcb->pGoingSRB;
		for (i = 0; i < cnt; i++) {
			psrb2 = psrb;
			psrb = psrb->pNextSRB;
			if (pdcb->pWaitingSRB) {
				psrb2->pNextSRB = pdcb->pWaitingSRB;
				pdcb->pWaitingSRB = psrb2;
			} else {
				pdcb->pWaitingSRB = psrb2;
				pdcb->pWaitLastSRB = psrb2;
				psrb2->pNextSRB = NULL;
			}
		}
		pdcb->GoingSRBCnt = 0;
		pdcb->pGoingSRB = NULL;
		pdcb->TagMask = 0;
		pdcb = pdcb->pNextDCB;
	}
	while (pdcb != pDCB);
}

static void
trm_reset(PACB pACB)
{
	int		intflag;
	u_int16_t	i;

    	TRM_DPRINTF("trm: RESET");
    	intflag = splcam();
	trm_reg_write8(0x00, TRMREG_DMA_INTEN);
	trm_reg_write8(0x00, TRMREG_SCSI_INTEN);

	trm_ResetSCSIBus(pACB);
	for (i = 0; i < 500; i++)
		DELAY(1000);
    	trm_reg_write8(0x7F, TRMREG_SCSI_INTEN); 
	/* Enable DMA interrupt	*/
	trm_reg_write8(EN_SCSIINTR, TRMREG_DMA_INTEN);
	/* Clear DMA FIFO */
	trm_reg_write8(CLRXFIFO, TRMREG_DMA_CONTROL);
	/* Clear SCSI FIFO */
	trm_reg_write16(DO_CLRFIFO,TRMREG_SCSI_CONTROL);
	trm_ResetDevParam(pACB);
	trm_DoingSRB_Done(pACB);
	pACB->pActiveDCB = NULL;
	pACB->ACBFlag = 0;/* RESET_DETECT, RESET_DONE ,RESET_DEV */
	trm_DoWaitingSRB(pACB);
	/* Tell the XPT layer that a bus reset occured    */
	if (pACB->ppath != NULL)
		xpt_async(AC_BUS_RESET, pACB->ppath, NULL);
	splx(intflag);
    	return;
}

static u_int16_t
trm_StartSCSI(PACB pACB, PDCB pDCB, PSRB pSRB)
{
	u_int16_t	return_code;
	u_int8_t	tag_number, scsicommand, i,command,identify_message;
	u_int8_t *	ptr;
	u_long		tag_mask;
	union  ccb	*pccb;
	struct ccb_scsiio *pcsio;

	pccb  = pSRB->pccb;
	pcsio = &pccb->csio;
	pSRB->TagNumber = 31;

	trm_reg_write8(pACB->AdaptSCSIID, TRMREG_SCSI_HOSTID);
	trm_reg_write8(pDCB->TargetID, TRMREG_SCSI_TARGETID);
	trm_reg_write8(pDCB->SyncPeriod, TRMREG_SCSI_SYNC);
	trm_reg_write8(pDCB->SyncOffset, TRMREG_SCSI_OFFSET);
	pSRB->ScsiPhase = PH_BUS_FREE;/* initial phase */
	/* Flush FIFO */
	trm_reg_write16(DO_CLRFIFO, TRMREG_SCSI_CONTROL);

	identify_message = pDCB->IdentifyMsg;

   	if ((pSRB->CmdBlock[0] == INQUIRY) ||
	    (pSRB->CmdBlock[0] == REQUEST_SENSE) ||
	    (pSRB->SRBFlag & AUTO_REQSENSE)) {
		if (((pDCB->SyncMode & WIDE_NEGO_ENABLE) &&
		      !(pDCB->SyncMode & WIDE_NEGO_DONE)) \
		|| ((pDCB->SyncMode & SYNC_NEGO_ENABLE) &&
		  !(pDCB->SyncMode & SYNC_NEGO_DONE))) {
			if (!(pDCB->IdentifyMsg & 7) ||
			    (pSRB->CmdBlock[0] != INQUIRY)) {
				scsicommand = SCMD_SEL_ATNSTOP;
				pSRB->SRBState = SRB_MSGOUT;
				goto polling;
			}
		}
       	/* 
       	* Send identify message	
       	*/
		trm_reg_write8((identify_message & 0xBF) ,TRMREG_SCSI_FIFO); 
		scsicommand = SCMD_SEL_ATN;
		pSRB->SRBState = SRB_START_;
	} else {
		/* not inquiry,request sense,auto request sense */
		/* 
		 * Send identify message	
		 */
		trm_reg_write8(identify_message,TRMREG_SCSI_FIFO);
		scsicommand = SCMD_SEL_ATN;
		pSRB->SRBState = SRB_START_;
		if (pDCB->SyncMode & EN_TAG_QUEUING) {
		  /* Send Tag message */
	      	  /* 
	       	   * Get tag id
   		   */
			tag_mask = 1;
			tag_number = 0;
			while (tag_mask & pDCB->TagMask) {
				tag_mask = tag_mask << 1;
				tag_number++;
			}
			/* 
			 * Send Tag id
			 */
			trm_reg_write8(MSG_SIMPLE_QTAG, TRMREG_SCSI_FIFO);
			trm_reg_write8(tag_number, TRMREG_SCSI_FIFO);
			pDCB->TagMask |= tag_mask;
			pSRB->TagNumber = tag_number;
			scsicommand = SCMD_SEL_ATN3;
			pSRB->SRBState = SRB_START_;
		}
	}
polling:
	/*
	 * 	 Send CDB ..command block .........			
	 */
   	if (pSRB->SRBFlag & AUTO_REQSENSE) {
		trm_reg_write8(REQUEST_SENSE, TRMREG_SCSI_FIFO);
		trm_reg_write8((pDCB->IdentifyMsg << 5), TRMREG_SCSI_FIFO);
		trm_reg_write8(0, TRMREG_SCSI_FIFO);
		trm_reg_write8(0, TRMREG_SCSI_FIFO);
		trm_reg_write8(pcsio->sense_len, TRMREG_SCSI_FIFO);
		trm_reg_write8(0, TRMREG_SCSI_FIFO);
	} else {
		ptr = (u_int8_t *) pSRB->CmdBlock;
		for (i = 0; i < pSRB->ScsiCmdLen ; i++) {
			command = *ptr++;
			trm_reg_write8(command,TRMREG_SCSI_FIFO);
		}
	}
	if (trm_reg_read16(TRMREG_SCSI_STATUS) & SCSIINTERRUPT) { 
	    /* 
	     * If trm_StartSCSI return 1 :
	     * current interrupt status is interrupt disreenable 
	     * It's said that SCSI processor has more one SRB need to do,
     	     * SCSI processor has been occupied by one SRB.
	     */
		pSRB->SRBState = SRB_READY;
		pDCB->TagMask &= ~(1 << pSRB->TagNumber);
		return_code = 1;
	} else { 
	  /* 
	   * If trm_StartSCSI return 0 :
	   * current interrupt status is interrupt enable 
	   * It's said that SCSI processor is unoccupied 
	   */
		pSRB->ScsiPhase  = SCSI_NOP1; /* SCSI bus free Phase */
		pACB->pActiveDCB = pDCB;
		pDCB->pActiveSRB = pSRB;
		return_code = 0;
		trm_reg_write16(DO_DATALATCH | DO_HWRESELECT, 
		    TRMREG_SCSI_CONTROL);/* it's important for atn stop*/
		/*
		 * SCSI cammand 
		 */
		trm_reg_write8(scsicommand,TRMREG_SCSI_COMMAND);
	}
	return (return_code);	
}

static void 
trm_Interrupt(vpACB)
void *vpACB;
{
	PACB		pACB;
	PDCB		pDCB;
	PSRB		pSRB;
	u_int16_t	phase;
	void		(*stateV)(PACB, PSRB, u_int8_t *);
	u_int8_t	scsi_status=0, scsi_intstatus;

	pACB = vpACB;

	if (pACB == NULL) {
		TRM_DPRINTF("trm_Interrupt: pACB NULL return......");
	    	return;
	}

	scsi_status = trm_reg_read16(TRMREG_SCSI_STATUS);
	if (!(scsi_status & SCSIINTERRUPT)) {
		TRM_DPRINTF("trm_Interrupt: TRMREG_SCSI_STATUS scsi_status = NULL ,return......");
	    	return;
	}
	TRM_DPRINTF("scsi_status=%2x,",scsi_status);

    	scsi_intstatus = trm_reg_read8(TRMREG_SCSI_INTSTATUS);

	TRM_DPRINTF("scsi_intstatus=%2x,",scsi_intstatus);

    	if (scsi_intstatus & (INT_SELTIMEOUT | INT_DISCONNECT)) {
		trm_Disconnect(pACB);
		return;
	}

	if (scsi_intstatus & INT_RESELECTED) {
		trm_Reselect(pACB);
		return;
	}
	if (scsi_intstatus & INT_SCSIRESET) {
		trm_ScsiRstDetect(pACB);
		return;
	}

	if (scsi_intstatus & (INT_BUSSERVICE | INT_CMDDONE)) {
		pDCB = pACB->pActiveDCB;
		pSRB = pDCB->pActiveSRB;
		if (pDCB) {
			if (pDCB->DCBFlag & ABORT_DEV_)
				trm_EnableMsgOutAbort1(pACB, pSRB);
		}
		phase = (u_int16_t) pSRB->ScsiPhase;  /* phase: */
		stateV = (void *) trm_SCSI_phase0[phase];
		stateV(pACB, pSRB, &scsi_status);
		pSRB->ScsiPhase = scsi_status & PHASEMASK; 
		/* phase:0,1,2,3,4,5,6,7 */
		phase = (u_int16_t) scsi_status & PHASEMASK;       
		stateV = (void *) trm_SCSI_phase1[phase];
		stateV(pACB, pSRB, &scsi_status);  
	}
}

static void
trm_MsgOutPhase0(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{

	if (pSRB->SRBState & (SRB_UNEXPECT_RESEL+SRB_ABORT_SENT))
		*pscsi_status = PH_BUS_FREE;
	/*.. initial phase*/
}

static void
trm_MsgOutPhase1(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{
	u_int8_t	bval;
	u_int16_t	i, cnt;
	u_int8_t *	ptr;
	PDCB		pDCB;

	trm_reg_write16(DO_CLRFIFO, TRMREG_SCSI_CONTROL);
	pDCB = pACB->pActiveDCB;
	if (!(pSRB->SRBState & SRB_MSGOUT)) {
		cnt = pSRB->MsgCnt;
		if (cnt) {
			ptr = (u_int8_t *) pSRB->MsgOutBuf;
			for (i = 0; i < cnt; i++) {
				trm_reg_write8(*ptr, TRMREG_SCSI_FIFO);
				ptr++;
			}
			pSRB->MsgCnt = 0;
			if ((pDCB->DCBFlag & ABORT_DEV_) &&
			    (pSRB->MsgOutBuf[0] == MSG_ABORT)) {
				pSRB->SRBState = SRB_ABORT_SENT;
			}
		} else {
			bval = MSG_ABORT;	
			if ((pSRB->CmdBlock[0] == INQUIRY) ||
					(pSRB->CmdBlock[0] == REQUEST_SENSE) ||
					(pSRB->SRBFlag & AUTO_REQSENSE)) {
				if (pDCB->SyncMode & SYNC_NEGO_ENABLE) {
					goto  mop1;
				}
			}
			trm_reg_write8(bval, TRMREG_SCSI_FIFO);
		}
	} else {
mop1:   /* message out phase */
		if (!(pSRB->SRBState & SRB_DO_WIDE_NEGO)
		    && (pDCB->SyncMode & WIDE_NEGO_ENABLE)) {
		  /*
	   	   * WIDE DATA TRANSFER REQUEST code (03h)
		   */
			pDCB->SyncMode &= ~(SYNC_NEGO_DONE | EN_ATN_STOP);
			trm_reg_write8((pDCB->IdentifyMsg & 0xBF),
			    TRMREG_SCSI_FIFO); 
			trm_reg_write8(MSG_EXTENDED,TRMREG_SCSI_FIFO);
			/* (01h) */
			trm_reg_write8(2,TRMREG_SCSI_FIFO);	
			/* Message length (02h) */
			trm_reg_write8(3,TRMREG_SCSI_FIFO);
			/* wide data xfer (03h) */
			trm_reg_write8(1,TRMREG_SCSI_FIFO);
			/* width:0(8bit),1(16bit),2(32bit) */
			pSRB->SRBState |= SRB_DO_WIDE_NEGO; 
		} else if (!(pSRB->SRBState & SRB_DO_SYNC_NEGO) 
		    && (pDCB->SyncMode & SYNC_NEGO_ENABLE)) {
		  /*
	   	   * SYNCHRONOUS DATA TRANSFER REQUEST code (01h)
		   */
			if (!(pDCB->SyncMode & WIDE_NEGO_DONE))
				trm_reg_write8((pDCB->IdentifyMsg & 0xBF),
						TRMREG_SCSI_FIFO);
			trm_reg_write8(MSG_EXTENDED,TRMREG_SCSI_FIFO);
		  /* (01h) */
			trm_reg_write8(3,TRMREG_SCSI_FIFO); 
		  /* Message length (03h) */
			trm_reg_write8(1,TRMREG_SCSI_FIFO);
		  /* SYNCHRONOUS DATA TRANSFER REQUEST code (01h) */
			trm_reg_write8(pDCB->MaxNegoPeriod,TRMREG_SCSI_FIFO);
		  /* Transfer peeriod factor */
			trm_reg_write8(SYNC_NEGO_OFFSET,TRMREG_SCSI_FIFO); 
		  /* REQ/ACK offset */
			pSRB->SRBState |= SRB_DO_SYNC_NEGO;
		}
	}
	trm_reg_write16(DO_DATALATCH, TRMREG_SCSI_CONTROL);
	/* it's important for atn stop */
	/*
	 * SCSI cammand 
	 */
	trm_reg_write8(SCMD_FIFO_OUT, TRMREG_SCSI_COMMAND);
}

static void 
trm_CommandPhase0(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{

}

static void 
trm_CommandPhase1(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{
	PDCB			pDCB;
	u_int8_t *		ptr;
	u_int16_t		i, cnt;
	union  ccb		*pccb;
	struct ccb_scsiio	*pcsio;

	pccb  = pSRB->pccb;
	pcsio = &pccb->csio;

	trm_reg_write16(DO_CLRATN | DO_CLRFIFO , TRMREG_SCSI_CONTROL);
	if (!(pSRB->SRBFlag & AUTO_REQSENSE)) {
		cnt = (u_int16_t) pSRB->ScsiCmdLen;
		ptr = (u_int8_t *) pSRB->CmdBlock;
		for (i = 0; i < cnt; i++) {
			trm_reg_write8(*ptr, TRMREG_SCSI_FIFO);
			ptr++;
		}
	} else {
		trm_reg_write8(REQUEST_SENSE, TRMREG_SCSI_FIFO);
		pDCB = pACB->pActiveDCB;
		/* target id */
		trm_reg_write8((pDCB->IdentifyMsg << 5), TRMREG_SCSI_FIFO);
		trm_reg_write8(0, TRMREG_SCSI_FIFO);
		trm_reg_write8(0, TRMREG_SCSI_FIFO);
		/* sizeof(struct scsi_sense_data) */
		trm_reg_write8(pcsio->sense_len, TRMREG_SCSI_FIFO);
		trm_reg_write8(0, TRMREG_SCSI_FIFO);
	}
	pSRB->SRBState = SRB_COMMAND;
	trm_reg_write16(DO_DATALATCH, TRMREG_SCSI_CONTROL);
	/* it's important for atn stop*/
	/*
	 * SCSI cammand 
	 */
	trm_reg_write8(SCMD_FIFO_OUT, TRMREG_SCSI_COMMAND);
}

static void
trm_DataOutPhase0(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{
	PDCB		pDCB;
	u_int8_t	TempDMAstatus,SGIndexTemp;
	u_int16_t	scsi_status;
	PSEG		pseg;
	u_long		TempSRBXferredLength,dLeftCounter=0;

	pDCB = pSRB->pSRBDCB;
	scsi_status = *pscsi_status;

	if (!(pSRB->SRBState & SRB_XFERPAD)) {
		if (scsi_status & PARITYERROR)
			pSRB->SRBStatus |= PARITY_ERROR;
		if (!(scsi_status & SCSIXFERDONE)) {
      		  /*
		   * when data transfer from DMA FIFO to SCSI FIFO
		   * if there was some data left in SCSI FIFO
		   */
  			dLeftCounter = (u_long) 
			  (trm_reg_read8(TRMREG_SCSI_FIFOCNT) & 0x1F);
			if (pDCB->SyncPeriod & WIDE_SYNC) {
			  /*
		   	   * if WIDE scsi SCSI FIFOCNT unit is word
	   		   * so need to * 2
   			   */
				dLeftCounter <<= 1;
			}
		}
		/*
		 * caculate all the residue data that not yet tranfered
		 * SCSI transfer counter + left in SCSI FIFO data
		 *
		 * .....TRM_SCSI_COUNTER (24bits)
		 * The counter always decrement by one for every SCSI byte 
		 *transfer.
		 * .....TRM_SCSI_FIFOCNT (5bits)
		 * The counter is SCSI FIFO offset counter
		 */
		dLeftCounter += trm_reg_read32(TRMREG_SCSI_COUNTER);
		if (dLeftCounter == 1) {
			dLeftCounter = 0;
			trm_reg_write16(DO_CLRFIFO,TRMREG_SCSI_CONTROL);
		}
		if ((dLeftCounter == 0) || 
		    (scsi_status & SCSIXFERCNT_2_ZERO)) {   
			TempDMAstatus = trm_reg_read8(TRMREG_DMA_STATUS);
			while (!(TempDMAstatus & DMAXFERCOMP)) {
				TempDMAstatus = 
				  trm_reg_read8(TRMREG_DMA_STATUS);
			}
			pSRB->SRBTotalXferLength = 0;
		} else {
		  /* Update SG list		*/
		  /*
	   	   * if transfer not yet complete
   		   * there were some data residue in SCSI FIFO or
		   * SCSI transfer counter not empty
		   */
			if (pSRB->SRBTotalXferLength != dLeftCounter) {
			  /*
		  	   * data that had transferred length
	   		   */
				TempSRBXferredLength = 
				  pSRB->SRBTotalXferLength - dLeftCounter;
				/*
				 * next time to be transferred length
				 */
				pSRB->SRBTotalXferLength = dLeftCounter;
				/*
				 * parsing from last time disconnect SRBSGIndex
				 */
				pseg = 
				  pSRB->SRBSGListPointer + pSRB->SRBSGIndex;
				for (SGIndexTemp = pSRB->SRBSGIndex;
				    SGIndexTemp < pSRB->SRBSGCount; 
				    SGIndexTemp++) {
					/* 
					 * find last time which SG transfer be 
					 * disconnect 
					 */
					if (TempSRBXferredLength >= 
					    pseg->length) 
						TempSRBXferredLength -= 
						  pseg->length;
					else {
				  	  /*
			   		   * update last time disconnected SG 
					   * list
				   	   */
						pseg->length -= 
						  TempSRBXferredLength; 
						/* residue data length  */
						pseg->address += 
						  TempSRBXferredLength;
						/* residue data pointer */
						pSRB->SRBSGIndex = SGIndexTemp;
						break;
					}
					pseg++;
				}
			}
		}
	}
	trm_reg_write8(STOPDMAXFER ,TRMREG_DMA_CONTROL);
}


static void
trm_DataOutPhase1(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{
	u_int16_t	ioDir;
	/*
	 * do prepare befor transfer when data out phase
	 */

	ioDir = XFERDATAOUT;
	trm_DataIO_transfer(pACB, pSRB, ioDir);
}

static void 
trm_DataInPhase0(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{
	u_int8_t	bval,SGIndexTemp;
	u_int16_t	scsi_status;
	PSEG		pseg;
	u_long		TempSRBXferredLength,dLeftCounter = 0;

    	scsi_status = *pscsi_status;
	if (!(pSRB->SRBState & SRB_XFERPAD)) {
		if (scsi_status & PARITYERROR)
			pSRB->SRBStatus |= PARITY_ERROR;
		dLeftCounter += trm_reg_read32(TRMREG_SCSI_COUNTER);
		if ((dLeftCounter == 0) || (scsi_status & SCSIXFERCNT_2_ZERO)) {
			bval = trm_reg_read8(TRMREG_DMA_STATUS);
			while (!(bval & DMAXFERCOMP))
				bval = trm_reg_read8(TRMREG_DMA_STATUS);
			pSRB->SRBTotalXferLength = 0;
		} else {  
	  	  /*
   		   * parsing the case:
	   	   * when a transfer not yet complete 
	   	   * but be disconnected by uper layer
	   	   * if transfer not yet complete
	   	   * there were some data residue in SCSI FIFO or
	   	   * SCSI transfer counter not empty
	   	   */
		  if (pSRB->SRBTotalXferLength != dLeftCounter) {
				/*
				 * data that had transferred length
				 */
		  	TempSRBXferredLength = 
			  pSRB->SRBTotalXferLength - dLeftCounter;
				/*
			 	 * next time to be transferred length
				 */
			pSRB->SRBTotalXferLength = dLeftCounter;
				/*
				 * parsing from last time disconnect SRBSGIndex
				 */
			pseg = pSRB->SRBSGListPointer + pSRB->SRBSGIndex;
			for (SGIndexTemp = pSRB->SRBSGIndex; 
			    SGIndexTemp < pSRB->SRBSGCount;
			    SGIndexTemp++) {
			  /* 
	   		   * find last time which SG transfer be disconnect 
	   		   */
	 			if (TempSRBXferredLength >= pseg->length)
					TempSRBXferredLength -= pseg->length;
				else {
		  		  /*
   				   * update last time disconnected SG list
				   */
					pseg->length -= TempSRBXferredLength;
					/* residue data length  */
					pseg->address += TempSRBXferredLength;
					/* residue data pointer */
					pSRB->SRBSGIndex = SGIndexTemp;
					break;
				} 
				pseg++;
			}
	  	  }
		}
	}
}

static void
trm_DataInPhase1(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{
	u_int16_t	ioDir;
	/*
	 * do prepare befor transfer when data in phase
	 */
	
	ioDir = XFERDATAIN;
	trm_DataIO_transfer(pACB, pSRB, ioDir);
}

static void
trm_DataIO_transfer(PACB pACB, PSRB pSRB, u_int16_t ioDir)
{
	u_int8_t	bval;
	PDCB		pDCB;

	pDCB = pSRB->pSRBDCB;
	if (pSRB->SRBSGIndex < pSRB->SRBSGCount) {
		if (pSRB->SRBTotalXferLength != 0) {
			pSRB->SRBSGPhyAddr = vtophys(pSRB->SRBSGListPointer);
			/* 
			 * load what physical address of Scatter/Gather list 
			 table want to be transfer
			 */
			pSRB->SRBState = SRB_DATA_XFER;
			trm_reg_write32(0, TRMREG_DMA_XHIGHADDR);
			trm_reg_write32(
			    (pSRB->SRBSGPhyAddr + 
			     ((u_long)pSRB->SRBSGIndex << 3)),
			    TRMREG_DMA_XLOWADDR);
			/*
			 * load how many bytes in the Scatter/Gather 
			 * list table 
			 */
			trm_reg_write32(
			    ((u_long)(pSRB->SRBSGCount - pSRB->SRBSGIndex) << 3),
			    TRMREG_DMA_XCNT);			
			/*
			 * load total transfer length (24bits) max value
			 * 16Mbyte 
			 */
			trm_reg_write32(pSRB->SRBTotalXferLength,
			    TRMREG_SCSI_COUNTER);
			/* Start DMA transfer */
			trm_reg_write16(ioDir, TRMREG_DMA_COMMAND);
			/* Start SCSI transfer */
			trm_reg_write16(DO_DATALATCH, TRMREG_SCSI_CONTROL);
			/* it's important for atn stop */
			/*
			 * SCSI cammand 
			 */
			bval = (ioDir == XFERDATAOUT) ?
			  SCMD_DMA_OUT : SCMD_DMA_IN;
			trm_reg_write8(bval, TRMREG_SCSI_COMMAND);
		} else {
		  /* xfer pad */
			if (pSRB->SRBSGCount) {
				pSRB->AdaptStatus = H_OVER_UNDER_RUN;
				pSRB->SRBStatus |= OVER_RUN;
			}
			if (pDCB->SyncPeriod & WIDE_SYNC)
				trm_reg_write32(2,TRMREG_SCSI_COUNTER);
			else
				trm_reg_write32(1,TRMREG_SCSI_COUNTER);
			if (ioDir == XFERDATAOUT)
				trm_reg_write16(0, TRMREG_SCSI_FIFO);
			else
				trm_reg_read16(TRMREG_SCSI_FIFO);
			pSRB->SRBState |= SRB_XFERPAD;
			trm_reg_write16(DO_DATALATCH, TRMREG_SCSI_CONTROL);
			/* it's important for atn stop */
			/*
			 * SCSI cammand 
			 */
			bval = (ioDir == XFERDATAOUT) ? 
			  SCMD_FIFO_OUT : SCMD_FIFO_IN;
			trm_reg_write8(bval, TRMREG_SCSI_COMMAND);
		}
	}
}

static void
trm_StatusPhase0(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{

	pSRB->TargetStatus = trm_reg_read8(TRMREG_SCSI_FIFO);
	pSRB->SRBState = SRB_COMPLETED;
	*pscsi_status = PH_BUS_FREE;  
	/*.. initial phase*/
	trm_reg_write16(DO_DATALATCH, TRMREG_SCSI_CONTROL);
	/* it's important for atn stop */
	/*
	 * SCSI cammand 
	 */
	trm_reg_write8(SCMD_MSGACCEPT, TRMREG_SCSI_COMMAND);
}



static void
trm_StatusPhase1(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{

	if (trm_reg_read16(TRMREG_DMA_COMMAND) & 0x0001) {
		if (!(trm_reg_read8(TRMREG_SCSI_FIFOCNT) & 0x40))
	       		trm_reg_write16(DO_CLRFIFO, TRMREG_SCSI_CONTROL);
		if (!(trm_reg_read16(TRMREG_DMA_FIFOCNT) & 0x8000))
			trm_reg_write8(CLRXFIFO, TRMREG_DMA_CONTROL);
	} else {
		if (!(trm_reg_read16(TRMREG_DMA_FIFOCNT) & 0x8000))
			trm_reg_write8(CLRXFIFO, TRMREG_DMA_CONTROL);
		if (!(trm_reg_read8(TRMREG_SCSI_FIFOCNT) & 0x40))
			trm_reg_write16(DO_CLRFIFO, TRMREG_SCSI_CONTROL);
	}
	pSRB->SRBState = SRB_STATUS;
	trm_reg_write16(DO_DATALATCH, TRMREG_SCSI_CONTROL);
	/* it's important for atn stop */
	/*
	 * SCSI cammand 
	 */
	trm_reg_write8(SCMD_COMP, TRMREG_SCSI_COMMAND);
}

/*
 *scsiiom		 
 *       trm_MsgInPhase0: one of trm_SCSI_phase0[] vectors
 *            stateV = (void *) trm_SCSI_phase0[phase]
 *		           if phase =7    
 * extended message codes:
 *
 *   code        description
 *
 *    02h        Reserved
 *    00h        MODIFY DATA  POINTER
 *    01h        SYNCHRONOUS DATA TRANSFER REQUEST
 *    03h        WIDE DATA TRANSFER REQUEST
 * 04h - 7Fh     Reserved
 * 80h - FFh     Vendor specific  
 *		        
 */

static void
trm_MsgInPhase0(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{
	u_int8_t	message_in_code,bIndex,message_in_tag_id;
	PDCB		pDCB;
	PSRB		pSRBTemp;

	pDCB = pACB->pActiveDCB;

	message_in_code = trm_reg_read8(TRMREG_SCSI_FIFO);
	if (!(pSRB->SRBState & SRB_EXTEND_MSGIN)) {
		if (message_in_code == MSG_DISCONNECT) {
			pSRB->SRBState = SRB_DISCONNECT;
			goto  min6;
		} else if (message_in_code == MSG_SAVE_PTR) {
			goto  min6;
		} else if ((message_in_code == MSG_EXTENDED) ||
		    ((message_in_code >= MSG_SIMPLE_QTAG) &&
		     (message_in_code <= MSG_ORDER_QTAG))) {
			pSRB->SRBState |= SRB_EXTEND_MSGIN;
		    	pSRB->MsgInBuf[0] = message_in_code;
			/* extended message      (01h) */
			pSRB->MsgCnt = 1;
			pSRB->pMsgPtr = &pSRB->MsgInBuf[1];
			/* extended message length (n) */
			goto  min6;
		} else if (message_in_code == MSG_REJECT_) {
			/* Reject message */
			if (pDCB->SyncMode & WIDE_NEGO_ENABLE) {
			  /* do wide nego reject */
				pDCB = pSRB->pSRBDCB;
				pDCB->SyncMode |= WIDE_NEGO_DONE;
				pDCB->SyncMode &= ~(SYNC_NEGO_DONE | 
				    EN_ATN_STOP | WIDE_NEGO_ENABLE);
				pSRB->SRBState &= ~(SRB_DO_WIDE_NEGO+SRB_MSGIN);
				if ((pDCB->SyncMode & SYNC_NEGO_ENABLE) 
				    && !(pDCB->SyncMode & SYNC_NEGO_DONE)) {   
				  /* Set ATN, in case ATN was clear */
					pSRB->SRBState |= SRB_MSGOUT;
					trm_reg_write16(
					    DO_SETATN,
					    TRMREG_SCSI_CONTROL);
				} else {   
			  	  /* Clear ATN */
					trm_reg_write16(
					    DO_CLRATN,
					    TRMREG_SCSI_CONTROL);
				}
			} else if (pDCB->SyncMode & SYNC_NEGO_ENABLE) { 
			  /* do sync nego reject */
				trm_reg_write16(DO_CLRATN,TRMREG_SCSI_CONTROL);
				if (pSRB->SRBState & SRB_DO_SYNC_NEGO) {
					pDCB = pSRB->pSRBDCB;
					pDCB->SyncMode &= 
					  ~(SYNC_NEGO_ENABLE+SYNC_NEGO_DONE); 
					pDCB->SyncPeriod = 0;
					pDCB->SyncOffset = 0;
					goto  re_prog;
				}
			}
			goto  min6;
		} else if (message_in_code == MSG_IGNOREWIDE) {
			trm_reg_write32(1, TRMREG_SCSI_COUNTER);
			trm_reg_read8(TRMREG_SCSI_FIFO);
			goto  min6;
		} else {
	  	  /* Restore data pointer message */
  		  /* Save data pointer message	  */
		  /* Completion message		  */
		  /* NOP message       	          */
			goto  min6;
		}
	} else {	
	  /* 
   	   * Parsing incomming extented messages 
	   */
		*pSRB->pMsgPtr = message_in_code;
		pSRB->MsgCnt++;
		pSRB->pMsgPtr++;
		TRM_DPRINTF("pSRB->MsgInBuf[0]=%2x \n ",pSRB->MsgInBuf[0]);
		TRM_DPRINTF("pSRB->MsgInBuf[1]=%2x \n ",pSRB->MsgInBuf[1]);
		TRM_DPRINTF("pSRB->MsgInBuf[2]=%2x \n ",pSRB->MsgInBuf[2]);
		TRM_DPRINTF("pSRB->MsgInBuf[3]=%2x \n ",pSRB->MsgInBuf[3]);
		TRM_DPRINTF("pSRB->MsgInBuf[4]=%2x \n ",pSRB->MsgInBuf[4]);
	    	if ((pSRB->MsgInBuf[0] >= MSG_SIMPLE_QTAG)
		    && (pSRB->MsgInBuf[0] <= MSG_ORDER_QTAG)) {
		  /*
	   	   * is QUEUE tag message :
   		   *
	   	   * byte 0:
	   	   * HEAD    QUEUE TAG (20h)
	   	   * ORDERED QUEUE TAG (21h)
	   	   * SIMPLE  QUEUE TAG (22h)
	   	   * byte 1:
	   	   * Queue tag (00h - FFh)
	   	   */
			if (pSRB->MsgCnt == 2) {
				pSRB->SRBState = 0;
				message_in_tag_id = pSRB->MsgInBuf[1];
				pSRB = pDCB->pGoingSRB;
				pSRBTemp = pDCB->pGoingLastSRB;
				if (pSRB) {
					for (;;) {
						if (pSRB->TagNumber != 
						    message_in_tag_id) {
							if (pSRB == pSRBTemp) {
								goto  mingx0;
							}
							pSRB = pSRB->pNextSRB;
						} else
							break;
					}
					if (pDCB->DCBFlag & ABORT_DEV_) {
						pSRB->SRBState = SRB_ABORT_SENT;
						trm_EnableMsgOutAbort1(
						    pACB, pSRB);
					}
					if (!(pSRB->SRBState & SRB_DISCONNECT))
						goto  mingx0;
					pDCB->pActiveSRB = pSRB;
					pSRB->SRBState = SRB_DATA_XFER;
				} else {
mingx0:
	     				pSRB = pACB->pTmpSRB;
					pSRB->SRBState = SRB_UNEXPECT_RESEL;
					pDCB->pActiveSRB = pSRB;
					pSRB->MsgOutBuf[0] = MSG_ABORT_TAG;
					trm_EnableMsgOutAbort2(
					    pACB,
					    pSRB);
				}
			}
		} else if ((pSRB->MsgInBuf[0] == MSG_EXTENDED) &&
		    (pSRB->MsgInBuf[2] == 3) && (pSRB->MsgCnt == 4)) {
		  /*
	   	   * is Wide data xfer Extended message :
	   	   * ======================================
	   	   * WIDE DATA TRANSFER REQUEST
   		   * ======================================
		   * byte 0 :  Extended message (01h)
		   * byte 1 :  Extended message length (02h)
		   * byte 2 :  WIDE DATA TRANSFER code (03h)
		   * byte 3 :  Transfer width exponent 
		   */
			pDCB = pSRB->pSRBDCB;
			pSRB->SRBState &= ~(SRB_EXTEND_MSGIN+SRB_DO_WIDE_NEGO);
			if ((pSRB->MsgInBuf[1] != 2)) {
			  /* Length is wrong, reject it  */
				pDCB->SyncMode &=
				  ~(WIDE_NEGO_ENABLE+WIDE_NEGO_DONE); 
				pSRB->MsgCnt = 1;
				pSRB->MsgInBuf[0] = MSG_REJECT_;
				trm_reg_write16(DO_SETATN, TRMREG_SCSI_CONTROL);
				goto  min6;
			}
			if (pDCB->SyncMode & WIDE_NEGO_ENABLE) {		
			  /* Do wide negoniation */
				if (pSRB->MsgInBuf[3] > 2) {
				  /* > 32 bit	*/
				  /* reject_msg: */
					pDCB->SyncMode &= 
					  ~(WIDE_NEGO_ENABLE+WIDE_NEGO_DONE); 
					pSRB->MsgCnt = 1;
					pSRB->MsgInBuf[0] = MSG_REJECT_;
					trm_reg_write16(DO_SETATN,
					    TRMREG_SCSI_CONTROL);
					goto  min6;
				}
				if (pSRB->MsgInBuf[3] == 2) {
					pSRB->MsgInBuf[3] = 1;
					/* do 16 bits	*/
				} else {
					if (!(pDCB->SyncMode 
					      & WIDE_NEGO_DONE)) {
						pSRB->SRBState &=
						  ~(SRB_DO_WIDE_NEGO+SRB_MSGIN);
						pDCB->SyncMode |= 
						  WIDE_NEGO_DONE;
						pDCB->SyncMode &=
						  ~(SYNC_NEGO_DONE |
						      EN_ATN_STOP |
						      WIDE_NEGO_ENABLE);
						if (pSRB->MsgInBuf[3] != 0) {
						  /* is Wide data xfer */
							pDCB->SyncPeriod |=
							  WIDE_SYNC;
							pDCB->tinfo.current.width 
							  = MSG_EXT_WDTR_BUS_16_BIT;
							pDCB->tinfo.goal.width
							  = MSG_EXT_WDTR_BUS_16_BIT;
						}
					}
				}
			} else
				pSRB->MsgInBuf[3] = 0;
			pSRB->SRBState |= SRB_MSGOUT;
			trm_reg_write16(DO_SETATN,TRMREG_SCSI_CONTROL);
			goto  min6;
		} else if ((pSRB->MsgInBuf[0] == MSG_EXTENDED) &&
		    (pSRB->MsgInBuf[2] == 1) && (pSRB->MsgCnt == 5)) {
			/*
			 * is 8bit transfer Extended message :
			 * =================================
			 * SYNCHRONOUS DATA TRANSFER REQUEST
			 * =================================
			 * byte 0 :  Extended message (01h)
			 * byte 1 :  Extended message length (03)
			 * byte 2 :  SYNCHRONOUS DATA TRANSFER code (01h)
			 * byte 3 :  Transfer period factor 
			 * byte 4 :  REQ/ACK offset  
			 */
			pSRB->SRBState &= ~(SRB_EXTEND_MSGIN+SRB_DO_SYNC_NEGO);
			if ((pSRB->MsgInBuf[1] != 3) ||
			    (pSRB->MsgInBuf[2] != 1)) {
			  /* reject_msg: */
				pSRB->MsgCnt = 1;
				pSRB->MsgInBuf[0] = MSG_REJECT_;
				trm_reg_write16(DO_SETATN, TRMREG_SCSI_CONTROL);
			} else if (!(pSRB->MsgInBuf[3]) || !(pSRB->MsgInBuf[4])) {
				/* set async */
				pDCB = pSRB->pSRBDCB;
				/* disable sync & sync nego */
				pDCB->SyncMode &= 
				  ~(SYNC_NEGO_ENABLE+SYNC_NEGO_DONE);
				pDCB->SyncPeriod = 0;
				pDCB->SyncOffset = 0;
				pDCB->tinfo.goal.period = 0;
				pDCB->tinfo.goal.offset = 0;
				pDCB->tinfo.current.period = 0;
				pDCB->tinfo.current.offset = 0;
				pDCB->tinfo.current.width = 
				  MSG_EXT_WDTR_BUS_8_BIT;
				goto  re_prog;
			} else {
				/* set sync */
				pDCB = pSRB->pSRBDCB;
				pDCB->SyncMode |= 
				  SYNC_NEGO_ENABLE+SYNC_NEGO_DONE;
				pDCB->MaxNegoPeriod = pSRB->MsgInBuf[3];
				/* Transfer period factor */
				pDCB->SyncOffset = pSRB->MsgInBuf[4]; 
				/* REQ/ACK offset */
				for (bIndex = 0; bIndex < 7; bIndex++) {
				  if (pSRB->MsgInBuf[3] <=
				      dc395x_trm_clock_period[bIndex]) {
				  	break;
				  }
				}
				pDCB->tinfo.goal.period =
				  dc395x_trm_tinfo_sync_period[bIndex];
				pDCB->tinfo.current.period = 
				  dc395x_trm_tinfo_sync_period[bIndex];
				pDCB->tinfo.goal.offset = pDCB->SyncOffset;
				pDCB->tinfo.current.offset = pDCB->SyncOffset;
				pDCB->SyncPeriod |= (bIndex | ALT_SYNC);
re_prog:
				/*               
				 *
	 			 *   program SCSI control register
	 			 *
	 			 */
				trm_reg_write8(pDCB->SyncPeriod,
				    TRMREG_SCSI_SYNC);
				trm_reg_write8(pDCB->SyncOffset,
				    TRMREG_SCSI_OFFSET);
				trm_SetXferRate(pACB,pSRB,pDCB);
			}
		}
	}
min6:
	*pscsi_status = PH_BUS_FREE;
	/* .. initial phase */
	trm_reg_write16(DO_DATALATCH, TRMREG_SCSI_CONTROL);
	/* it's important for atn stop */
	/*
	 * SCSI cammand 
	 */
	trm_reg_write8(SCMD_MSGACCEPT, TRMREG_SCSI_COMMAND);
}

static void
trm_MsgInPhase1(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{

	trm_reg_write16(DO_CLRFIFO, TRMREG_SCSI_CONTROL);
	trm_reg_write32(1,TRMREG_SCSI_COUNTER);
	if (!(pSRB->SRBState & SRB_MSGIN)) {
		pSRB->SRBState &= SRB_DISCONNECT;
		pSRB->SRBState |= SRB_MSGIN;
	}
	trm_reg_write16(DO_DATALATCH, TRMREG_SCSI_CONTROL);
	/* it's important for atn stop*/
	/*
	 * SCSI cammand 
	 */
	trm_reg_write8(SCMD_FIFO_IN, TRMREG_SCSI_COMMAND);
}

static void
trm_Nop0(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{

}

static void
trm_Nop1(PACB pACB, PSRB pSRB, u_int8_t *pscsi_status)
{

}

static void
trm_SetXferRate(PACB pACB,PSRB pSRB, PDCB pDCB)
{
	u_int16_t	cnt, i;
	u_int8_t	bval;
	PDCB		pDCBTemp;
	u_int		target_id,target_lun;

	/*
	 * set all lun device's  period , offset
	 */
	target_id  = pSRB->pccb->ccb_h.target_id;
	target_lun = pSRB->pccb->ccb_h.target_lun;
	TRM_DPRINTF("trm_SetXferRate:target_id= %d ,target_lun= %d \n"
	    ,target_id,target_lun);
	if (!(pDCB->IdentifyMsg & 0x07)) {
		if (!pACB->scan_devices[target_id][target_lun]) {
			pDCBTemp = pACB->pLinkDCB;
			cnt = pACB->DeviceCnt;
			bval = pDCB->TargetID;
			for (i = 0; i < cnt; i++) {
				if (pDCBTemp->TargetID == bval) {
					pDCBTemp->SyncPeriod = pDCB->SyncPeriod;
					pDCBTemp->SyncOffset = pDCB->SyncOffset;
					pDCBTemp->SyncMode = pDCB->SyncMode;
				}
				pDCBTemp = pDCBTemp->pNextDCB;
			}
		}
	}
	return;
}

/*
 * scsiiom		
 *            trm_Interrupt       
 *		        
 *
 *    ---SCSI bus phase
 *
 * 	PH_DATA_OUT	        0x00	 Data out phase	              
 * 	PH_DATA_IN	        0x01	 Data in phase	            
 * 	PH_COMMAND	        0x02	 Command phase	 
 * 	PH_STATUS	        0x03	 Status phase
 *	PH_BUS_FREE	        0x04	 Invalid phase used as bus free	
 * 	PH_BUS_FREE	        0x05	 Invalid phase used as bus free	
 * 	PH_MSG_OUT	        0x06	 Message out phase
 * 	PH_MSG_IN	        0x07	 Message in phase
 *
 */
static void 
trm_Disconnect(PACB pACB)
{
	PDCB		pDCB;
	PSRB		pSRB, psrb;
	int		intflag;
	u_int16_t	i,j, cnt;
	u_int8_t	bval;
	u_int		target_id,target_lun;
	
	TRM_DPRINTF("trm_Disconnect...............\n ");
	
	intflag = splcam();
       	pDCB = pACB->pActiveDCB;
	if (!pDCB) {
		TRM_DPRINTF(" Exception Disconnect DCB=NULL..............\n ");
		j = 400;
    		while (--j) 
			DELAY(1);
		/* 1 msec */
		trm_reg_write16((DO_CLRFIFO | DO_HWRESELECT),
		    TRMREG_SCSI_CONTROL);
		return;
	}
	pSRB = pDCB->pActiveSRB; 
	/* bug pSRB=0 */
	target_id  = pSRB->pccb->ccb_h.target_id;
	target_lun = pSRB->pccb->ccb_h.target_lun;
	TRM_DPRINTF(":pDCB->pActiveSRB= %8x \n ",(u_int) pDCB->pActiveSRB);
	pACB->pActiveDCB = 0;
	pSRB->ScsiPhase = PH_BUS_FREE; 
	/* SCSI bus free Phase */
	trm_reg_write16((DO_CLRFIFO | DO_HWRESELECT), TRMREG_SCSI_CONTROL);
	if (pSRB->SRBState & SRB_UNEXPECT_RESEL) {
		pSRB->SRBState = 0;
		trm_DoWaitingSRB(pACB);
	} else if (pSRB->SRBState & SRB_ABORT_SENT) {
		pDCB->TagMask = 0;
		pDCB->DCBFlag = 0;
		cnt = pDCB->GoingSRBCnt;
		pDCB->GoingSRBCnt = 0;
		pSRB = pDCB->pGoingSRB;
		for (i = 0; i < cnt; i++) {
			psrb = pSRB->pNextSRB;
			pSRB->pNextSRB = pACB->pFreeSRB;
			pACB->pFreeSRB = pSRB;
			pSRB = psrb;
		}
		pDCB->pGoingSRB = 0;
		trm_DoWaitingSRB(pACB);
	} else {
		if ((pSRB->SRBState & (SRB_START_+SRB_MSGOUT)) || 
		    !(pSRB->SRBState & (SRB_DISCONNECT+SRB_COMPLETED))) {
		  /* Selection time out */
			if (!(pACB->scan_devices[target_id][target_lun])) {
				pSRB->SRBState = SRB_READY;
				trm_RewaitSRB(pDCB, pSRB);
			} else {
				pSRB->TargetStatus = SCSI_STAT_SEL_TIMEOUT;
				goto  disc1;
			}
		} else if (pSRB->SRBState & SRB_DISCONNECT) {
			/*
			 * SRB_DISCONNECT
			 */
			trm_DoWaitingSRB(pACB);
		} else if (pSRB->SRBState & SRB_COMPLETED) {
disc1:
		  /*
		   * SRB_COMPLETED
		   */
			if (pDCB->MaxCommand > 1) {
				bval = pSRB->TagNumber;
				pDCB->TagMask &= (~(1 << bval));
				/* free tag mask */
			}
			pDCB->pActiveSRB = 0;
			pSRB->SRBState = SRB_FREE;
			trm_SRBdone(pACB, pDCB, pSRB);
		}
	}
	splx(intflag);
	return;
}

static void
trm_Reselect(PACB pACB)
{
	PDCB		pDCB;
	PSRB		pSRB;
	u_int16_t	RselTarLunId;

	TRM_DPRINTF("trm_Reselect................. \n");
	pDCB = pACB->pActiveDCB;
	if (pDCB) {
	  /* Arbitration lost but Reselection win */
		pSRB = pDCB->pActiveSRB;
		pSRB->SRBState = SRB_READY;
		trm_RewaitSRB(pDCB, pSRB);
	}
	/* Read Reselected Target Id and LUN */
	RselTarLunId = trm_reg_read16(TRMREG_SCSI_TARGETID) & 0x1FFF;
	pDCB = pACB->pLinkDCB;
	while (RselTarLunId != *((u_int16_t *) &pDCB->TargetID)) {
	  /* get pDCB of the reselect id */
		pDCB = pDCB->pNextDCB;
	}

	pACB->pActiveDCB = pDCB;
	if (pDCB->SyncMode & EN_TAG_QUEUING) {
		pSRB = pACB->pTmpSRB;
		pDCB->pActiveSRB = pSRB;
	} else {
		pSRB = pDCB->pActiveSRB;
		if (!pSRB || !(pSRB->SRBState & SRB_DISCONNECT)) {
		  /*
	   	   * abort command
   		   */
			pSRB = pACB->pTmpSRB;
			pSRB->SRBState = SRB_UNEXPECT_RESEL;
			pDCB->pActiveSRB = pSRB;
			trm_EnableMsgOutAbort1(pACB, pSRB);
		} else {
			if (pDCB->DCBFlag & ABORT_DEV_) {
				pSRB->SRBState = SRB_ABORT_SENT;
				trm_EnableMsgOutAbort1(pACB, pSRB);
			} else 
				pSRB->SRBState = SRB_DATA_XFER;
		}
	}
	pSRB->ScsiPhase = PH_BUS_FREE;
	/* SCSI bus free Phase */
	/* 
	 * Program HA ID, target ID, period and offset
	 */
	trm_reg_write8((u_int8_t) RselTarLunId,TRMREG_SCSI_TARGETID);
	/* target ID */
	trm_reg_write8(pACB->AdaptSCSIID,TRMREG_SCSI_HOSTID);
	/* host   ID */
	trm_reg_write8(pDCB->SyncPeriod,TRMREG_SCSI_SYNC);
	/* period    */
	trm_reg_write8(pDCB->SyncOffset,TRMREG_SCSI_OFFSET); 
	/* offset    */
	trm_reg_write16(DO_DATALATCH, TRMREG_SCSI_CONTROL);
	/* it's important for atn stop*/
	/*
	 * SCSI cammand 
	 */
	trm_reg_write8(SCMD_MSGACCEPT, TRMREG_SCSI_COMMAND);
	/* to rls the /ACK signal */
}

static void
trm_SRBdone(PACB pACB, PDCB pDCB, PSRB pSRB)
{
	PSRB			psrb;
	u_int8_t		bval, bval1,status;
	union ccb		*pccb;
	struct ccb_scsiio	*pcsio;
	PSCSI_INQDATA		ptr;
	int			intflag;
	u_int			target_id,target_lun;
	PDCB			pTempDCB;

	pccb  = pSRB->pccb;
	if (pccb == NULL)
		return;
	pcsio = &pccb->csio;
	target_id  = pSRB->pccb->ccb_h.target_id;
	target_lun = pSRB->pccb->ccb_h.target_lun;
	if ((pccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		bus_dmasync_op_t op;
		if ((pccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;
		bus_dmamap_sync(pACB->buffer_dmat, pSRB->dmamap, op);
		bus_dmamap_unload(pACB->buffer_dmat, pSRB->dmamap);
	}
    	/*
	 *
	 * target status
	 *
	 */
	status = pSRB->TargetStatus;
	pcsio->scsi_status=SCSI_STAT_GOOD;
	pccb->ccb_h.status = CAM_REQ_CMP;
	if (pSRB->SRBFlag & AUTO_REQSENSE) {
	  /* 
   	   * status of auto request sense 
	   */
		pSRB->SRBFlag &= ~AUTO_REQSENSE;
		pSRB->AdaptStatus = 0;
		pSRB->TargetStatus = SCSI_STATUS_CHECK_COND;
		
		if (status == SCSI_STATUS_CHECK_COND) {
			pccb->ccb_h.status = CAM_SEL_TIMEOUT;
			goto ckc_e;
		}
		*((u_long *) &(pSRB->CmdBlock[0])) = pSRB->Segment0[0];
		*((u_long *) &(pSRB->CmdBlock[4])) = pSRB->Segment0[1];
		pSRB->SRBTotalXferLength = pSRB->Segment1[1];
		pSRB->SegmentX[0].address = pSRB->SgSenseTemp.address;
		pSRB->SegmentX[0].length = pSRB->SgSenseTemp.length;
		pcsio->scsi_status = SCSI_STATUS_CHECK_COND;
		pccb->ccb_h.status = CAM_AUTOSNS_VALID;
		goto ckc_e;
	}
	/*
	 * target status
	 */
	if (status) {
		if (status == SCSI_STATUS_CHECK_COND) {
			if ((pcsio->ccb_h.flags & CAM_DIS_AUTOSENSE) == 0) {
			  TRM_DPRINTF("trm_RequestSense..................\n");
			  trm_RequestSense(pACB, pDCB, pSRB);
			  return;
			}
			pcsio->scsi_status = SCSI_STATUS_CHECK_COND;
			pccb->ccb_h.status = CAM_AUTOSNS_VALID |
			  CAM_SCSI_STATUS_ERROR;
			goto ckc_e;
		} else if (status == SCSI_STAT_QUEUEFULL) {
			bval = (u_int8_t) pDCB->GoingSRBCnt;
			bval--;
			pDCB->MaxCommand = bval;
			trm_RewaitSRB(pDCB, pSRB);
			pSRB->AdaptStatus = 0;
			pSRB->TargetStatus = 0;
			pcsio->scsi_status = SCSI_STAT_QUEUEFULL;
			pccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;
			goto ckc_e;
		} else if (status == SCSI_STAT_SEL_TIMEOUT) {
			pSRB->AdaptStatus  = H_SEL_TIMEOUT;
			pSRB->TargetStatus = 0;
			pcsio->scsi_status = SCSI_STAT_SEL_TIMEOUT;
			pccb->ccb_h.status = CAM_SEL_TIMEOUT;
		} else if (status == SCSI_STAT_BUSY) {
			TRM_DPRINTF("trm: target busy at %s %d\n",
				__FILE__, __LINE__);
			pcsio->scsi_status = SCSI_STAT_BUSY;
			pccb->ccb_h.status = CAM_SCSI_BUSY;
		  /* The device busy, try again later?	  */
		} else if (status == SCSI_STAT_RESCONFLICT) {
			TRM_DPRINTF("trm: target reserved at %s %d\n",
				__FILE__, __LINE__);
			pcsio->scsi_status = SCSI_STAT_RESCONFLICT;
			pccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;	/*XXX*/
		} else {
			pSRB->AdaptStatus = 0;
			if (pSRB->RetryCnt) {
				pSRB->RetryCnt--;
				pSRB->TargetStatus = 0;
				pSRB->SRBSGIndex = 0;
				pSRB->SRBSGListPointer = (PSEG)
				  &pSRB->SegmentX[0];
				if (trm_StartSCSI(pACB, pDCB, pSRB)) {
				  /* 
				   * If trm_StartSCSI return 1 :
				   * current interrupt status is interrupt 
				   * disreenable 
				   * It's said that SCSI processor has more 
				   * one SRB need to do
				   */
					trm_RewaitSRB(pDCB, pSRB);
				}
				return;
			} else {
        			TRM_DPRINTF("trm: driver stuffup at %s %d\n",
					__FILE__, __LINE__);
		      		pccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;
			}
		}
	} else {
        /* 
 	 * process initiator status..........................
	 * Adapter (initiator) status
	 */
		status = pSRB->AdaptStatus;
		if (status & H_OVER_UNDER_RUN) {
			pSRB->TargetStatus = 0;
			pccb->ccb_h.status = CAM_DATA_RUN_ERR;
			/* Illegal length (over/under run) */
		} else if (pSRB->SRBStatus & PARITY_ERROR) {
			TRM_DPRINTF("trm: driver stuffup %s %d\n",
				__FILE__, __LINE__);
			pDCB->tinfo.goal.period = 0;
			pDCB->tinfo.goal.offset = 0;
			/* Driver failed to perform operation */
			pccb->ccb_h.status = CAM_UNCOR_PARITY;
		} else {
		  /* no error */
			pSRB->AdaptStatus = 0;
			pSRB->TargetStatus = 0;
			pccb->ccb_h.status = CAM_REQ_CMP;
			/* there is no error, (sense is invalid) */
		}
	}
ckc_e:
	if (pACB->scan_devices[target_id][target_lun]) {
	  /*
	   *   if SCSI command in "scan devices" duty
	   */
		if (pSRB->CmdBlock[0] == TEST_UNIT_READY) 
			pACB->scan_devices[target_id][target_lun] = 0;
		/* SCSI command phase :test unit ready */
		else if (pSRB->CmdBlock[0] == INQUIRY) {
		  /* 
		   * SCSI command phase :inquiry scsi device data 
		   * (type,capacity,manufacture.... 
		   */
			if (pccb->ccb_h.status == CAM_SEL_TIMEOUT)
				goto NO_DEV;
			ptr = (PSCSI_INQDATA) pcsio->data_ptr;
			/* page fault */
			TRM_DPRINTF("trm_SRBdone..PSCSI_INQDATA:%2x \n",
			    ptr->DevType);
		  	bval1 = ptr->DevType & SCSI_DEVTYPE; 
			if (bval1 == SCSI_NODEV) {
NO_DEV:
	  			TRM_DPRINTF("trm_SRBdone NO Device:target_id= %d ,target_lun= %d \n",
				    target_id,
				    target_lun);
		      		intflag = splcam();
				pACB->scan_devices[target_id][target_lun] = 0;
				/* no device set scan device flag =0*/
				/* pDCB Q link */
				/* move the head of DCB to tempDCB*/
				pTempDCB=pACB->pLinkDCB;  
				/* search current DCB for pass link */
				while (pTempDCB->pNextDCB != pDCB) {
					pTempDCB = pTempDCB->pNextDCB;
				}
				/*
				 * when the current DCB found than connect 
				 * current DCB tail 
				 */
				/* to the DCB tail that before current DCB */
				pTempDCB->pNextDCB = pDCB->pNextDCB;
				/*
				 * if there was only one DCB ,connect his tail
				 * to his head 
				 */
				if (pACB->pLinkDCB == pDCB)
					pACB->pLinkDCB = pTempDCB->pNextDCB;
				if (pACB->pDCBRunRobin == pDCB)
					pACB->pDCBRunRobin = pTempDCB->pNextDCB;
				pACB->DeviceCnt--;
				if (pACB->DeviceCnt == 0) {
					pACB->pLinkDCB = NULL;
					pACB->pDCBRunRobin = NULL;
				}
				splx(intflag);
			} else { 
#ifdef trm_DEBUG1
				int j;
				for (j = 0; j < 28; j++) {
					TRM_DPRINTF("ptr=%2x ", 
						((u_int8_t *)ptr)[j]);
				}
#endif
	      			pDCB->DevType = bval1;
				if (bval1 == SCSI_DASD ||
				    bval1 == SCSI_OPTICAL) {
					if ((((ptr->Vers & 0x07) >= 2) ||
					      ((ptr->RDF & 0x0F) == 2)) && 
					    (ptr->Flags & SCSI_INQ_CMDQUEUE) &&
					    (pDCB->DevMode & TAG_QUEUING_) &&
					    (pDCB->DevMode & EN_DISCONNECT_)) {
						if (pDCB->DevMode &
						    TAG_QUEUING_) {
							pDCB->MaxCommand = 
							  pACB->TagMaxNum;
							pDCB->SyncMode |= 
							  EN_TAG_QUEUING;
							pDCB->TagMask = 0;
							pDCB->tinfo.disc_tag |=
							  TRM_CUR_TAGENB;
						} else {
							pDCB->SyncMode |= 
							  EN_ATN_STOP;
							pDCB->tinfo.disc_tag &=
							  ~TRM_CUR_TAGENB;
						}
					}
				}
			}
			/* pSRB->CmdBlock[0] == INQUIRY */
		}
		/* pACB->scan_devices[target_id][target_lun] */
	}
    	intflag = splcam();
	/*  ReleaseSRB(pDCB, pSRB); */
	if (pSRB == pDCB->pGoingSRB)
		pDCB->pGoingSRB = pSRB->pNextSRB;
	else {
		psrb = pDCB->pGoingSRB;
		while (psrb->pNextSRB != pSRB) {
			psrb = psrb->pNextSRB;
		}
		psrb->pNextSRB = pSRB->pNextSRB;
		if (pSRB == pDCB->pGoingLastSRB) {
			pDCB->pGoingLastSRB = psrb;
		}
	}
	pSRB->pNextSRB = pACB->pFreeSRB;
	pACB->pFreeSRB = pSRB;
	pDCB->GoingSRBCnt--;
	trm_DoWaitingSRB(pACB);

	splx(intflag);
	/*  Notify cmd done */
	xpt_done (pccb);
}

static void
trm_DoingSRB_Done(PACB pACB)
{
	PDCB		pDCB, pdcb;
	PSRB		psrb, psrb2;
	u_int16_t	cnt, i;
	union ccb 	*pccb;

	pDCB = pACB->pLinkDCB;
	if (pDCB == NULL) 
  		return;
	pdcb = pDCB;
    	do {
		cnt = pdcb->GoingSRBCnt;
		psrb = pdcb->pGoingSRB;
		for (i = 0; i < cnt; i++) {
			psrb2 = psrb->pNextSRB;
		    	pccb = psrb->pccb;
			pccb->ccb_h.status = CAM_SEL_TIMEOUT;
			/*  ReleaseSRB(pDCB, pSRB); */
			psrb->pNextSRB = pACB->pFreeSRB;
			pACB->pFreeSRB = psrb;
			xpt_done(pccb);
			psrb  = psrb2;
		}
		pdcb->GoingSRBCnt = 0;;
		pdcb->pGoingSRB = NULL;
		pdcb->TagMask = 0;
		pdcb = pdcb->pNextDCB;
	}
	while (pdcb != pDCB);
}

static void 
trm_ResetSCSIBus(PACB pACB)
{
	int	intflag;

	intflag = splcam();
    	pACB->ACBFlag |= RESET_DEV;

	trm_reg_write16(DO_RSTSCSI,TRMREG_SCSI_CONTROL);
	while (!(trm_reg_read16(TRMREG_SCSI_INTSTATUS) & INT_SCSIRESET));
	splx(intflag);
	return;
}

static void 
trm_ScsiRstDetect(PACB pACB)
{
	int	intflag;
	u_long	wlval;

	TRM_DPRINTF("trm_ScsiRstDetect \n");
	wlval = 1000;
	while (--wlval)
		DELAY(1000);
	intflag = splcam();
    	trm_reg_write8(STOPDMAXFER,TRMREG_DMA_CONTROL);

	trm_reg_write16(DO_CLRFIFO,TRMREG_SCSI_CONTROL);

	if (pACB->ACBFlag & RESET_DEV)
		pACB->ACBFlag |= RESET_DONE;
	else {
		pACB->ACBFlag |= RESET_DETECT;
		trm_ResetDevParam(pACB);
		/*	trm_DoingSRB_Done(pACB); ???? */
		trm_RecoverSRB(pACB);
		pACB->pActiveDCB = NULL;
		pACB->ACBFlag = 0;
		trm_DoWaitingSRB(pACB);
	}
	splx(intflag);
    	return;
}

static void
trm_RequestSense(PACB pACB, PDCB pDCB, PSRB pSRB)	
{
	union ccb		*pccb;
	struct ccb_scsiio	*pcsio;

	pccb  = pSRB->pccb;
	pcsio = &pccb->csio;

	pSRB->SRBFlag |= AUTO_REQSENSE;
	pSRB->Segment0[0] = *((u_long *) &(pSRB->CmdBlock[0]));
	pSRB->Segment0[1] = *((u_long *) &(pSRB->CmdBlock[4]));
	pSRB->Segment1[0] = (u_long) ((pSRB->ScsiCmdLen << 8) + 
	    pSRB->SRBSGCount);
	pSRB->Segment1[1] = pSRB->SRBTotalXferLength; /* ?????????? */

	/* $$$$$$ Status of initiator/target $$$$$$$$ */
	pSRB->AdaptStatus = 0;
	pSRB->TargetStatus = 0;
	/* $$$$$$ Status of initiator/target $$$$$$$$ */
	
	pSRB->SRBTotalXferLength = sizeof(pcsio->sense_data);
	pSRB->SgSenseTemp.address = pSRB->SegmentX[0].address;
	pSRB->SgSenseTemp.length  = pSRB->SegmentX[0].length;
	pSRB->SegmentX[0].address = (u_long) vtophys(&pcsio->sense_data);
	pSRB->SegmentX[0].length = (u_long) pcsio->sense_len;
	pSRB->SRBSGListPointer = &pSRB->SegmentX[0];
	pSRB->SRBSGCount = 1;
	pSRB->SRBSGIndex = 0;
	
	*((u_long *) &(pSRB->CmdBlock[0])) = 0x00000003;
	pSRB->CmdBlock[1] = pDCB->IdentifyMsg << 5;
	*((u_int16_t *) &(pSRB->CmdBlock[4])) = pcsio->sense_len;
	pSRB->ScsiCmdLen = 6;
	
	if (trm_StartSCSI(pACB, pDCB, pSRB))
	   /* 
	    * If trm_StartSCSI return 1 :
	    * current interrupt status is interrupt disreenable 
	    * It's said that SCSI processor has more one SRB need to do
	    */
		trm_RewaitSRB(pDCB, pSRB);
}

static void 
trm_EnableMsgOutAbort2(PACB pACB, PSRB pSRB)
{

	pSRB->MsgCnt = 1;
	trm_reg_write16(DO_SETATN, TRMREG_SCSI_CONTROL);
}

static void
trm_EnableMsgOutAbort1(PACB pACB, PSRB pSRB)
{
  
	pSRB->MsgOutBuf[0] = MSG_ABORT;
	trm_EnableMsgOutAbort2(pACB, pSRB);
}

static void
trm_initDCB(PACB pACB, PDCB pDCB, u_int16_t unit,u_int32_t i,u_int32_t j)
{
	PNVRAMTYPE 	pEEpromBuf;
	u_int8_t	bval,PeriodIndex;
	u_int		target_id,target_lun;
	PDCB		pTempDCB;
	int		intflag;
    
    	target_id  = i;
	target_lun = j;

	intflag = splcam();
	if (pACB->pLinkDCB == 0) {
		pACB->pLinkDCB = pDCB;
		/* 
		 * RunRobin impersonate the role 
		 * that let each device had good proportion 
		 * about SCSI command proceeding 
		 */
		pACB->pDCBRunRobin = pDCB;
		pDCB->pNextDCB = pDCB;
	} else {
		pTempDCB=pACB->pLinkDCB;
		/* search the last nod of DCB link */
		while (pTempDCB->pNextDCB != pACB->pLinkDCB)
			pTempDCB = pTempDCB->pNextDCB;
		/* connect current DCB with last DCB tail */
		pTempDCB->pNextDCB = pDCB;
		/* connect current DCB tail to this DCB Q head */
		pDCB->pNextDCB=pACB->pLinkDCB;
	}
	splx(intflag);

	pACB->DeviceCnt++;
	pDCB->pDCBACB = pACB;
	pDCB->TargetID = target_id;
	pDCB->TargetLUN =  target_lun;
	pDCB->pWaitingSRB = NULL;
	pDCB->pGoingSRB = NULL;
	pDCB->GoingSRBCnt = 0;
	pDCB->pActiveSRB = NULL;
	pDCB->TagMask = 0;
	pDCB->MaxCommand = 1;
	pDCB->DCBFlag = 0;
	/* $$$$$$$ */
	pEEpromBuf = &trm_eepromBuf[unit];
	pDCB->DevMode = pEEpromBuf->NvramTarget[target_id].NvmTarCfg0;
	pDCB->AdpMode = pEEpromBuf->NvramChannelCfg;
	/* $$$$$$$ */
	/* 
	 * disconnect enable ?
	 */
	if (pDCB->DevMode & NTC_DO_DISCONNECT) {
		bval = 0xC0;
		pDCB->tinfo.disc_tag |= TRM_USR_DISCENB ;
	} else {
		bval = 0x80;
		pDCB->tinfo.disc_tag &= ~(TRM_USR_DISCENB);
	}
	bval |= target_lun;
	pDCB->IdentifyMsg = bval;
	/* $$$$$$$ */
	/*
	 * tag Qing enable ?
	 */
	if (pDCB->DevMode & TAG_QUEUING_) {
		pDCB->tinfo.disc_tag |= TRM_USR_TAGENB ;
	} else
		pDCB->tinfo.disc_tag &= ~(TRM_USR_TAGENB);
	/* $$$$$$$ */
	/*
	 * wide nego ,sync nego enable ?
	 */
	pDCB->SyncPeriod = 0;
	pDCB->SyncOffset = 0;
	PeriodIndex = pEEpromBuf->NvramTarget[target_id].NvmTarPeriod & 0x07;
	pDCB->MaxNegoPeriod = dc395x_trm_clock_period[ PeriodIndex ] ;
	pDCB->SyncMode = 0;
	if ((pDCB->DevMode & NTC_DO_WIDE_NEGO) && 
	    (pACB->Config & HCC_WIDE_CARD))
		pDCB->SyncMode |= WIDE_NEGO_ENABLE;
	/* enable wide nego */
   	if (pDCB->DevMode & NTC_DO_SYNC_NEGO)
		pDCB->SyncMode |= SYNC_NEGO_ENABLE;
	/* enable sync nego */
	/* $$$$$$$ */
	/*
	 *	Fill in tinfo structure.
	 */
	pDCB->tinfo.user.period = pDCB->MaxNegoPeriod;
	pDCB->tinfo.user.offset = (pDCB->SyncMode & SYNC_NEGO_ENABLE) ? 15 : 0;
	pDCB->tinfo.user.width  = (pDCB->SyncMode & WIDE_NEGO_ENABLE) ? 
	  MSG_EXT_WDTR_BUS_16_BIT : MSG_EXT_WDTR_BUS_8_BIT;

	pDCB->tinfo.current.period = 0;
	pDCB->tinfo.current.offset = 0;
	pDCB->tinfo.current.width = MSG_EXT_WDTR_BUS_8_BIT;
}

static void 
trm_initSRB(PSRB psrb)
{
  
	psrb->PhysSRB = vtophys(psrb);
}

static void
trm_linkSRB(PACB pACB)
{
	u_int16_t	i;

	for (i = 0; i < MAX_SRB_CNT; i++) {
		if (i != MAX_SRB_CNT - 1)
			/*
			 * link all SRB 
			 */
			pACB->SRB_array[i].pNextSRB = &pACB->SRB_array[i+1];
    	else
			/*
			 * load NULL to NextSRB of the last SRB
			 */
		pACB->SRB_array[i].pNextSRB = NULL;
	/*
 	 * convert and save physical address of SRB to pSRB->PhysSRB
	 */
	trm_initSRB((PSRB) &pACB->SRB_array[i]);
	}
}


static void
trm_initACB(PACB pACB, u_int16_t unit)
{
	PNVRAMTYPE	pEEpromBuf;
	u_int16_t		i,j;
    
	pEEpromBuf = &trm_eepromBuf[unit];
	pACB->max_id = 15;
	
	if (pEEpromBuf->NvramChannelCfg & NAC_SCANLUN)
  		pACB->max_lun = 7;
	else
		pACB->max_lun = 0;

	TRM_DPRINTF("trm: pACB->max_id= %d pACB->max_lun= %d \n",
	    pACB->max_id, pACB->max_lun);

	pACB->pLinkDCB = NULL;
	pACB->pDCBRunRobin = NULL;
	pACB->pActiveDCB = NULL;
	pACB->pFreeSRB = pACB->SRB_array;
	pACB->AdapterUnit = unit;
	pACB->AdaptSCSIID = pEEpromBuf->NvramScsiId;
	pACB->AdaptSCSILUN = 0;
	pACB->DeviceCnt = 0;
	pACB->TagMaxNum = 2 << pEEpromBuf->NvramMaxTag ;
	pACB->ACBFlag = 0;
	/* 
	 * link all device's SRB Q of this adapter 
	 */
	trm_linkSRB(pACB);
	/* 
	 * temp SRB for Q tag used or abord command used 
	 */
	pACB->pTmpSRB = &pACB->TmpSRB;
	/*
	 * convert and save physical address of SRB to pSRB->PhysSRB
	 */
	trm_initSRB(pACB->pTmpSRB);
	/* allocate DCB array for scan device */
	for (i = 0; i < (pACB->max_id +1); i++) {   
		if (pACB->AdaptSCSIID != i) {
			for (j = 0; j < (pACB->max_lun +1); j++) {
				pACB->scan_devices[i][j] = 1;
				pACB->pDCB[i][j]= (PDCB) malloc (
				    sizeof (struct _DCB), M_DEVBUF, M_WAITOK);
				trm_initDCB(pACB,
				    pACB->pDCB[i][j], unit, i, j);
				TRM_DPRINTF("pDCB= %8x \n",
					(u_int)pACB->pDCB[i][j]);
			}
		}
	}
    	TRM_DPRINTF("sizeof(struct _DCB)= %8x \n",sizeof(struct _DCB));
	TRM_DPRINTF("sizeof(struct _ACB)= %8x \n",sizeof(struct _ACB));
	TRM_DPRINTF("sizeof(struct _SRB)= %8x \n",sizeof(struct _SRB));
}

static void
TRM_write_all(PNVRAMTYPE pEEpromBuf,PACB pACB)
{
	u_int8_t	*bpEeprom = (u_int8_t *) pEEpromBuf;
	u_int8_t	bAddr;

	/* Enable SEEPROM */
	trm_reg_write8((trm_reg_read8(TRMREG_GEN_CONTROL) | EN_EEPROM),
	    TRMREG_GEN_CONTROL);
	/*
	 * Write enable
	 */
	TRM_write_cmd(pACB, 0x04, 0xFF);
	trm_reg_write8(0, TRMREG_GEN_NVRAM);
	TRM_wait_30us(pACB);
	for (bAddr = 0; bAddr < 128; bAddr++, bpEeprom++) { 
		TRM_set_data(pACB, bAddr, *bpEeprom);
	}
	/* 
	 * Write disable
	 */
	TRM_write_cmd(pACB, 0x04, 0x00);
	trm_reg_write8(0 , TRMREG_GEN_NVRAM);
	TRM_wait_30us(pACB);
	/* Disable SEEPROM */
	trm_reg_write8((trm_reg_read8(TRMREG_GEN_CONTROL) & ~EN_EEPROM),
	    TRMREG_GEN_CONTROL);
	return;
}

static void
TRM_set_data(PACB pACB, u_int8_t bAddr, u_int8_t bData)
{
	int		i;
	u_int8_t	bSendData;
	/* 
	 * Send write command & address	
	 */
	
	TRM_write_cmd(pACB, 0x05, bAddr);
	/* 
	 * Write data 
	 */
	for (i = 0; i < 8; i++, bData <<= 1) {
		bSendData = NVR_SELECT;
		if (bData & 0x80)
		  /* Start from bit 7	*/
			bSendData |= NVR_BITOUT;
		trm_reg_write8(bSendData , TRMREG_GEN_NVRAM);
		TRM_wait_30us(pACB);
		trm_reg_write8((bSendData | NVR_CLOCK), TRMREG_GEN_NVRAM);
		TRM_wait_30us(pACB);
	}
	trm_reg_write8(NVR_SELECT , TRMREG_GEN_NVRAM);
	TRM_wait_30us(pACB);
	/*
	 * Disable chip select 
	 */
	trm_reg_write8(0 , TRMREG_GEN_NVRAM);
	TRM_wait_30us(pACB);
	trm_reg_write8(NVR_SELECT ,TRMREG_GEN_NVRAM);
	TRM_wait_30us(pACB);
	/* 
	 * Wait for write ready	
	 */
	while (1) {
		trm_reg_write8((NVR_SELECT | NVR_CLOCK), TRMREG_GEN_NVRAM);
		TRM_wait_30us(pACB);
		trm_reg_write8(NVR_SELECT, TRMREG_GEN_NVRAM);
		TRM_wait_30us(pACB);
		if (trm_reg_read8(TRMREG_GEN_NVRAM) & NVR_BITIN) {
			break;
		}
	}
	/* 
	 * Disable chip select 
	 */
	trm_reg_write8(0, TRMREG_GEN_NVRAM);
	return;
}

static void 
TRM_read_all(PNVRAMTYPE pEEpromBuf, PACB pACB)
{
	u_int8_t	*bpEeprom = (u_int8_t*) pEEpromBuf;
	u_int8_t	bAddr;
    
	/*
	 * Enable SEEPROM 
	 */
	trm_reg_write8((trm_reg_read8(TRMREG_GEN_CONTROL) | EN_EEPROM),
	    TRMREG_GEN_CONTROL);
	for (bAddr = 0; bAddr < 128; bAddr++, bpEeprom++)
		*bpEeprom = TRM_get_data(pACB, bAddr);
	/* 
	 * Disable SEEPROM 
	 */
	trm_reg_write8((trm_reg_read8(TRMREG_GEN_CONTROL) & ~EN_EEPROM),
	    TRMREG_GEN_CONTROL);
	return;
}

static u_int8_t
TRM_get_data(PACB pACB, u_int8_t bAddr)
{
	int		i;
	u_int8_t	bReadData, bData = 0;
	/* 
	* Send read command & address
	*/
	
	TRM_write_cmd(pACB, 0x06, bAddr);
				
	for (i = 0; i < 8; i++) {
	  /* 
	   * Read data
	   */
		trm_reg_write8((NVR_SELECT | NVR_CLOCK) , TRMREG_GEN_NVRAM);
		TRM_wait_30us(pACB);
		trm_reg_write8(NVR_SELECT , TRMREG_GEN_NVRAM);
		/* 
		 * Get data bit while falling edge 
		 */
		bReadData = trm_reg_read8(TRMREG_GEN_NVRAM);
		bData <<= 1;
		if (bReadData & NVR_BITIN) {
			bData |= 1;
		}
		TRM_wait_30us(pACB);
	}
	/* 
	 * Disable chip select 
	 */
	trm_reg_write8(0, TRMREG_GEN_NVRAM);
	return (bData);
}

static void
TRM_wait_30us(PACB pACB)
{
  
	/*    ScsiPortStallExecution(30);	 wait 30 us	*/
	trm_reg_write8(5, TRMREG_GEN_TIMER);
	while (!(trm_reg_read8(TRMREG_GEN_STATUS) & GTIMEOUT));
	return;
}

static void
TRM_write_cmd(PACB pACB, u_int8_t bCmd, u_int8_t bAddr)
{
	int		i;
	u_int8_t	bSendData;
					
    	for (i = 0; i < 3; i++, bCmd <<= 1) {
	  /* 
   	   * Program SB+OP code		
   	   */
     		bSendData = NVR_SELECT;
		if (bCmd & 0x04)	
			bSendData |= NVR_BITOUT;
		/* start from bit 2 */
		trm_reg_write8(bSendData, TRMREG_GEN_NVRAM);
		TRM_wait_30us(pACB);
		trm_reg_write8((bSendData | NVR_CLOCK), TRMREG_GEN_NVRAM);
		TRM_wait_30us(pACB);
	}	
	for (i = 0; i < 7; i++, bAddr <<= 1) {
	  /* 
	   * Program address		
	   */
		bSendData = NVR_SELECT;
		if (bAddr & 0x40)	
		  /* Start from bit 6	*/
			bSendData |= NVR_BITOUT;
		trm_reg_write8(bSendData , TRMREG_GEN_NVRAM);
		TRM_wait_30us(pACB);
		trm_reg_write8((bSendData | NVR_CLOCK), TRMREG_GEN_NVRAM);
		TRM_wait_30us(pACB);
	}
	trm_reg_write8(NVR_SELECT, TRMREG_GEN_NVRAM);
	TRM_wait_30us(pACB);
}

static void
trm_check_eeprom(PNVRAMTYPE pEEpromBuf, PACB pACB)
{
	u_int16_t	*wpEeprom = (u_int16_t *) pEEpromBuf;
	u_int16_t	wAddr, wCheckSum;
	u_long	dAddr, *dpEeprom;

	TRM_read_all(pEEpromBuf,pACB);
	wCheckSum = 0;
	for (wAddr = 0, wpEeprom = (u_int16_t *) pEEpromBuf;
	    wAddr < 64; wAddr++, wpEeprom++) {
		wCheckSum += *wpEeprom;
	}
	if (wCheckSum != 0x1234) {
	  /* 
   	   * Checksum error, load default	
	   */
		pEEpromBuf->NvramSubVendorID[0]	= (u_int8_t) PCI_Vendor_ID_TEKRAM;
		pEEpromBuf->NvramSubVendorID[1]	=
		  (u_int8_t) (PCI_Vendor_ID_TEKRAM >> 8);
		pEEpromBuf->NvramSubSysID[0] = (u_int8_t) PCI_Device_ID_TRM_S1040;
		pEEpromBuf->NvramSubSysID[1] = 
		  (u_int8_t) (PCI_Device_ID_TRM_S1040 >> 8);
		pEEpromBuf->NvramSubClass = 0x00;
		pEEpromBuf->NvramVendorID[0] = (u_int8_t) PCI_Vendor_ID_TEKRAM;
		pEEpromBuf->NvramVendorID[1] =
		  (u_int8_t) (PCI_Vendor_ID_TEKRAM >> 8);
		pEEpromBuf->NvramDeviceID[0] = (u_int8_t) PCI_Device_ID_TRM_S1040;
		pEEpromBuf->NvramDeviceID[1] = 
		  (u_int8_t) (PCI_Device_ID_TRM_S1040 >> 8);
		pEEpromBuf->NvramReserved = 0x00;

		for (dAddr = 0, dpEeprom = (u_long *) pEEpromBuf->NvramTarget;
		    dAddr < 16; dAddr++, dpEeprom++) {
			*dpEeprom = 0x00000077;
			/* NvmTarCfg3,NvmTarCfg2,NvmTarPeriod,NvmTarCfg0 */
		}

		*dpEeprom++ = 0x04000F07;
		/* NvramMaxTag,NvramDelayTime,NvramChannelCfg,NvramScsiId */
		*dpEeprom++ = 0x00000015;
		/* NvramReserved1,NvramBootLun,NvramBootTarget,NvramReserved0 */
		for (dAddr = 0; dAddr < 12; dAddr++, dpEeprom++)
			*dpEeprom = 0x00;
		pEEpromBuf->NvramCheckSum = 0x00;
		for (wAddr = 0, wCheckSum = 0, wpEeprom = (u_int16_t *) pEEpromBuf;
		    wAddr < 63; wAddr++, wpEeprom++)
	      		wCheckSum += *wpEeprom;
		*wpEeprom = 0x1234 - wCheckSum;
		TRM_write_all(pEEpromBuf,pACB);
	}
	return;
}
static int
trm_initAdapter(PACB pACB, u_int16_t unit, device_t pci_config_id)
{
	PNVRAMTYPE	pEEpromBuf;
	u_int16_t	wval;
	u_int8_t	bval;

	pEEpromBuf = &trm_eepromBuf[unit];

	/* 250ms selection timeout */
	trm_reg_write8(SEL_TIMEOUT, TRMREG_SCSI_TIMEOUT);
	/* Mask all the interrupt */
	trm_reg_write8(0x00, TRMREG_DMA_INTEN);    
	trm_reg_write8(0x00, TRMREG_SCSI_INTEN);     
	/* Reset SCSI module */
	trm_reg_write16(DO_RSTMODULE, TRMREG_SCSI_CONTROL); 
	/* program configuration 0 */
	pACB->Config = HCC_AUTOTERM | HCC_PARITY;
	if (trm_reg_read8(TRMREG_GEN_STATUS) & WIDESCSI)
		pACB->Config |= HCC_WIDE_CARD;
	if (pEEpromBuf->NvramChannelCfg & NAC_POWERON_SCSI_RESET)
		pACB->Config |= HCC_SCSI_RESET;
	if (pACB->Config & HCC_PARITY)
		bval = PHASELATCH | INITIATOR | BLOCKRST | PARITYCHECK;
	else
		bval = PHASELATCH | INITIATOR | BLOCKRST ;
	trm_reg_write8(bval,TRMREG_SCSI_CONFIG0); 
	/* program configuration 1 */
	trm_reg_write8(0x13, TRMREG_SCSI_CONFIG1); 
	/* program Host ID */
	bval = pEEpromBuf->NvramScsiId;
	trm_reg_write8(bval, TRMREG_SCSI_HOSTID); 
	/* set ansynchronous transfer */
	trm_reg_write8(0x00, TRMREG_SCSI_OFFSET); 
	/* Trun LED control off*/
	wval = trm_reg_read16(TRMREG_GEN_CONTROL) & 0x7F;
	trm_reg_write16(wval, TRMREG_GEN_CONTROL); 
	/* DMA config */
	wval = trm_reg_read16(TRMREG_DMA_CONFIG) | DMA_ENHANCE;
	trm_reg_write16(wval, TRMREG_DMA_CONFIG); 
	/* Clear pending interrupt status */
	trm_reg_read8(TRMREG_SCSI_INTSTATUS);
	/* Enable SCSI interrupt */
	trm_reg_write8(0x7F, TRMREG_SCSI_INTEN); 
	trm_reg_write8(EN_SCSIINTR, TRMREG_DMA_INTEN); 
	return (0);
}

static PACB
trm_init(u_int16_t unit, device_t pci_config_id)
{
	PACB		pACB;
	int		rid = PCIR_MAPS;
    
 	pACB = (PACB) device_get_softc(pci_config_id);
   	if (!pACB) {
		printf("trm%d: cannot allocate ACB !\n", unit);
		return (NULL);
	}
	bzero (pACB, sizeof (struct _ACB));
	pACB->iores = bus_alloc_resource(pci_config_id, SYS_RES_IOPORT, 
	    &rid, 0, ~0, 1, RF_ACTIVE);
    	if (pACB->iores == NULL) {
		printf("trm_init: bus_alloc_resource failed!\n");
		return (NULL);
	}
	pACB->tag = rman_get_bustag(pACB->iores);
	pACB->bsh = rman_get_bushandle(pACB->iores);
	if (bus_dma_tag_create(/*parent_dmat*/                 NULL, 
	      /*alignment*/                      1,
	      /*boundary*/                       0,
	      /*lowaddr*/  BUS_SPACE_MAXADDR_32BIT,
	      /*highaddr*/       BUS_SPACE_MAXADDR,
	      /*filter*/                      NULL, 
	      /*filterarg*/                   NULL,
	      /*maxsize*/                 MAXBSIZE,
	      /*nsegments*/               TRM_NSEG,
	      /*maxsegsz*/    TRM_MAXTRANSFER_SIZE,
	      /*flags*/           BUS_DMA_ALLOCNOW,
	      &pACB->buffer_dmat) != 0) 
		goto bad;
	trm_check_eeprom(&trm_eepromBuf[unit],pACB);
	trm_initACB(pACB, unit);
   	if (trm_initAdapter(pACB, unit, pci_config_id)) {
		printf("trm_initAdapter: initial ERROR\n");
		goto bad;
	}
	return (pACB);
bad:
	if (pACB->iores)
		bus_release_resource(pci_config_id, SYS_RES_IOPORT, PCIR_MAPS,
		    pACB->iores);
	if (pACB->buffer_dmat)
		bus_dma_tag_destroy(pACB->buffer_dmat);
	return (NULL);
}

static int
trm_attach(device_t pci_config_id)
{
	struct	cam_devq *device_Q;
	u_long	device_id;
	PACB	pACB = 0;
	int	rid = 0;
	int unit = device_get_unit(pci_config_id);
	
	device_id = pci_get_devid(pci_config_id);
	/*
	 * These cards do not allow memory mapped accesses
	 */
	if (device_id == PCI_DEVICEID_TRMS1040) {
		if ((pACB=trm_init((u_int16_t) unit,
			pci_config_id)) == NULL) {
			printf("trm%d: trm_init error!\n",unit);
			return (ENXIO);
		}
	} else
		return (ENXIO);
	/* After setting up the adapter, map our interrupt */
	/*  
	 * Now let the CAM generic SCSI layer find the SCSI devices on the bus
	 * start queue to reset to the idle loop.
	 * Create device queue of SIM(s)
	 * (MAX_START_JOB - 1) : max_sim_transactions
	 */
	pACB->irq = bus_alloc_resource(pci_config_id, SYS_RES_IRQ, &rid, 0,
	    ~0, 1, RF_SHAREABLE | RF_ACTIVE);
    	if (pACB->irq == NULL ||
	    bus_setup_intr(pci_config_id, pACB->irq, 
	    INTR_TYPE_CAM, trm_Interrupt, pACB, &pACB->ih)) {
		printf("trm%d: register Interrupt handler error!\n", unit);
		goto bad;
	}
	device_Q = cam_simq_alloc(MAX_START_JOB);
	if (device_Q == NULL){ 
		printf("trm%d: device_Q == NULL !\n",unit);
		goto bad;
	}
	/*
	 * Now tell the generic SCSI layer
	 * about our bus.
	 * If this is the xpt layer creating a sim, then it's OK
	 * to wait for an allocation.
	 * XXX Should we pass in a flag to indicate that wait is OK?
	 *
	 *                    SIM allocation
	 *
	 *                 SCSI Interface Modules
	 * The sim driver creates a sim for each controller.  The sim device
	 * queue is separately created in order to allow resource sharing betwee
	 * sims.  For instance, a driver may create one sim for each channel of
	 * a multi-channel controller and use the same queue for each channel.
	 * In this way, the queue resources are shared across all the channels
	 * of the multi-channel controller.
	 * trm_action     : sim_action_func
	 * trm_poll       : sim_poll_func
	 * "trm"        : sim_name ,if sim_name =  "xpt" ..M_DEVBUF,M_WAITOK
	 * pACB         : *softc    if sim_name <> "xpt" ..M_DEVBUF,M_NOWAIT
	 * pACB->unit   : unit
	 * 1            : max_dev_transactions
	 * MAX_TAGS     : max_tagged_dev_transactions
	 *
	 *  *******Construct our first channel SIM entry
	 */
	pACB->psim = cam_sim_alloc(trm_action,
	    trm_poll,
	    "trm",
	    pACB,
	    unit,
	    1,
	    MAX_TAGS_CMD_QUEUE,
	    device_Q);
	if (pACB->psim == NULL) {
		printf("trm%d: SIM allocate fault !\n",unit);
		cam_simq_free(device_Q);  /* SIM allocate fault*/
		goto bad;
	}
	if (xpt_bus_register(pACB->psim, 0) != CAM_SUCCESS)  {
		printf("trm%d: xpt_bus_register fault !\n",unit);
		goto bad;
	}
	if (xpt_create_path(&pACB->ppath,
	      NULL,
	      cam_sim_path(pACB->psim),
	      CAM_TARGET_WILDCARD,
	      CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		printf("trm%d: xpt_create_path fault !\n",unit);
		xpt_bus_deregister(cam_sim_path(pACB->psim));
		goto bad;
		/* 
		 * cam_sim_free(pACB->psim, TRUE);  free_devq 
		 * pACB->psim = NULL;
		 */
		return (ENXIO);
	}
	return (0);
bad:
	if (pACB->iores)
		bus_release_resource(pci_config_id, SYS_RES_IOPORT, PCIR_MAPS,
		    pACB->iores);
	if (pACB->buffer_dmat)
		bus_dma_tag_destroy(pACB->buffer_dmat);
	if (pACB->ih)
		bus_teardown_intr(pci_config_id, pACB->irq, pACB->ih);
	if (pACB->irq)
		bus_release_resource(pci_config_id, SYS_RES_IRQ, 0, pACB->irq);
	if (pACB->psim)
		cam_sim_free(pACB->psim, TRUE);
	
	return (ENXIO);
	
}

/*
*                  pci_device
*         trm_probe (device_t tag, pcidi_t type)
*
*/
static int
trm_probe(device_t tag)
{
  
	if (pci_get_devid(tag) == PCI_DEVICEID_TRMS1040) { 
		device_set_desc(tag,
		    "Tekram DC395U/UW/F DC315/U Fast20 Wide SCSI Adapter");
		return (0);
	} else
		return (ENXIO);
}

static int
trm_detach(device_t dev)
{
	PACB pACB = device_get_softc(dev);

	bus_release_resource(dev, SYS_RES_IOPORT, PCIR_MAPS, pACB->iores);
	bus_dma_tag_destroy(pACB->buffer_dmat);	
	bus_teardown_intr(dev, pACB->irq, pACB->ih);
	bus_release_resource(dev, SYS_RES_IRQ, 0, pACB->irq);
	xpt_async(AC_LOST_DEVICE, pACB->ppath, NULL);
	xpt_free_path(pACB->ppath);
	xpt_bus_deregister(cam_sim_path(pACB->psim));
	cam_sim_free(pACB->psim, TRUE);
	return (0);
}
static device_method_t trm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		trm_probe),
	DEVMETHOD(device_attach,	trm_attach),
	DEVMETHOD(device_detach,	trm_detach),
	{ 0, 0 }
};

static driver_t trm_driver = {
	"trm", trm_methods, sizeof(struct _ACB)
};

static devclass_t trm_devclass;
DRIVER_MODULE(trm, pci, trm_driver, trm_devclass, 0, 0);
