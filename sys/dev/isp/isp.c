/* $Id: isp.c,v 1.6 1998/04/14 17:43:45 mjacob Exp $ */
/*
 * Machine Independent (well, as best as possible)
 * code for the Qlogic ISP SCSI adapters.
 *
 *---------------------------------------
 * Copyright (c) 1997, 1998 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
 *---------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * Inspiration and ideas about this driver are from Erik Moe's Linux driver
 * (qlogicisp.c) and Dave Miller's SBus version of same (qlogicisp.c). Some
 * ideas dredged from the Solaris driver.
 */

/*
 * Include header file appropriate for platform we're building on.
 */

#ifdef	__NetBSD__
#include <dev/ic/isp_netbsd.h>
#endif
#ifdef	__FreeBSD__
#include <dev/isp/isp_freebsd.h>
#endif
#ifdef	__linux__
#include <isp_linux.h>
#endif

/*
 * General defines
 */

#define	MBOX_DELAY_COUNT	1000000 / 100

/*
 * Function prototypes.
 */
static int isp_poll __P((struct ispsoftc *, ISP_SCSI_XFER_T *, int));
static void isp_parse_status
__P((struct ispsoftc *, ispstatusreq_t *, ISP_SCSI_XFER_T *));
static void isp_lostcmd
__P((struct ispsoftc *, ISP_SCSI_XFER_T *, ispreq_t *));
static void isp_fibre_init __P((struct ispsoftc *));
static void isp_fw_state __P((struct ispsoftc *));
static void isp_dumpregs __P((struct ispsoftc *, const char *));
static void isp_setdparm __P((struct ispsoftc *));
static void isp_prtstst __P((ispstatusreq_t *));
static void isp_mboxcmd __P((struct ispsoftc *, mbreg_t *));

/*
 * Reset Hardware.
 */
void
isp_reset(isp)
	struct ispsoftc *isp;
{
	mbreg_t mbs;
	int loops, i, dodnld = 1;
	char *revname;
	ISP_LOCKVAL_DECL;

	revname = "(unknown)";

	isp->isp_state = ISP_NILSTATE;

	/*
	 * Basic types have been set in the MD code.
	 * See if we can't figure out more here.
	 */
	isp->isp_dblev = DFLT_DBLEVEL;
	if (isp->isp_type & ISP_HA_FC) {
		revname = "2100";
	} else {
		i = ISP_READ(isp, BIU_CONF0) & BIU_CONF0_HW_MASK;
		switch (i) {
		default:
			PRINTF("%s: unknown ISP type %x- assuming 1020\n",
			    isp->isp_name, i);
			isp->isp_type = ISP_HA_SCSI_1020;
			break;
		case 1:
		case 2:
			revname = "1020";
			isp->isp_type = ISP_HA_SCSI_1020;
			break;
		case 3:
			revname = "1040A";
			isp->isp_type = ISP_HA_SCSI_1040A;
			break;
		case 5:
			revname = "1040B";
			isp->isp_type = ISP_HA_SCSI_1040B;
			break;
		}
	}

	/*
	 * Do MD specific pre initialization
	 */
	ISP_LOCK;
	ISP_RESET0(isp);
	isp_setdparm(isp);

	/*
	 * Hit the chip over the head with hammer,
	 * and give the ISP a chance to recover.
	 */

	if (isp->isp_type & ISP_HA_SCSI) {
		ISP_WRITE(isp, BIU_ICR, BIU_ICR_SOFT_RESET);
		/*
		 * A slight delay...
		 */
		SYS_DELAY(100);

		/*
		 * Clear data && control DMA engines.
		 */
		ISP_WRITE(isp, CDMA_CONTROL,
		      DMA_CNTRL_CLEAR_CHAN | DMA_CNTRL_RESET_INT);
		ISP_WRITE(isp, DDMA_CONTROL,
		      DMA_CNTRL_CLEAR_CHAN | DMA_CNTRL_RESET_INT);
	} else {
		ISP_WRITE(isp, BIU2100_CSR, BIU2100_SOFT_RESET);
		/*
		 * A slight delay...
		 */
		SYS_DELAY(100);
		ISP_WRITE(isp, CDMA2100_CONTROL,
			DMA_CNTRL2100_CLEAR_CHAN | DMA_CNTRL2100_RESET_INT);
		ISP_WRITE(isp, TDMA2100_CONTROL,
			DMA_CNTRL2100_CLEAR_CHAN | DMA_CNTRL2100_RESET_INT);
		ISP_WRITE(isp, RDMA2100_CONTROL,
			DMA_CNTRL2100_CLEAR_CHAN | DMA_CNTRL2100_RESET_INT);
	}

	/*
	 * Wait for ISP to be ready to go...
	 */
	loops = MBOX_DELAY_COUNT;
	for (;;) {
		if (isp->isp_type & ISP_HA_SCSI) {
			if (!(ISP_READ(isp, BIU_ICR) & BIU_ICR_SOFT_RESET))
				break;
		} else {
			if (!(ISP_READ(isp, BIU2100_CSR) & BIU2100_SOFT_RESET))
				break;
		}
		SYS_DELAY(100);
		if (--loops < 0) {
			isp_dumpregs(isp, "chip reset timed out");
			ISP_UNLOCK;
			return;
		}
	}
	/*
	 * More initialization
	 */
	if (isp->isp_type & ISP_HA_SCSI) {      
		ISP_WRITE(isp, BIU_CONF1, 0);
	} else {
		ISP_WRITE(isp, BIU2100_CSR, 0);
		ISP_WRITE(isp, RISC_MTR2100, 0x1212);	/* FM */
	}

	ISP_WRITE(isp, HCCR, HCCR_CMD_RESET);
	SYS_DELAY(100);

	if (isp->isp_type & ISP_HA_SCSI) {
		ISP_SETBITS(isp, BIU_CONF1, isp->isp_mdvec->dv_conf1);
		if (isp->isp_mdvec->dv_conf1 & BIU_BURST_ENABLE) {
			ISP_SETBITS(isp, CDMA_CONF, DMA_ENABLE_BURST);
			ISP_SETBITS(isp, DDMA_CONF, DMA_ENABLE_BURST);
		}
	}
	ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE); /* release paused processor */

	/*
	 * Do MD specific post initialization
	 */
	ISP_RESET1(isp);

	/*
	 * Enable interrupts
	 */
	ENABLE_INTS(isp);

	/*
	 * Do some sanity checking.
	 */
	mbs.param[0] = MBOX_NO_OP;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "NOP test failed");
		ISP_UNLOCK;
		return;
	}

	if (isp->isp_type & ISP_HA_SCSI) {
		mbs.param[0] = MBOX_MAILBOX_REG_TEST;
		mbs.param[1] = 0xdead;
		mbs.param[2] = 0xbeef;
		mbs.param[3] = 0xffff;
		mbs.param[4] = 0x1111;
		mbs.param[5] = 0xa5a5;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_dumpregs(isp,
				"Mailbox Register test didn't complete");
			ISP_UNLOCK;
			return;
		}
		if (mbs.param[1] != 0xdead || mbs.param[2] != 0xbeef ||
		    mbs.param[3] != 0xffff || mbs.param[4] != 0x1111 ||
		    mbs.param[5] != 0xa5a5) {
			isp_dumpregs(isp, "Register Test Failed");
			ISP_UNLOCK;
			return;
		}

	}

	/*
	 * Download new Firmware, unless requested not to do so.
	 * This is made slightly trickier in some cases where the
	 * firmware of the ROM revision is newer than the revision
	 * compiled into the driver. So, where we used to compare
	 * versions of our f/w and the ROM f/w, now we just see
	 * whether we have f/w at all and whether a config flag
	 * has disabled our download.
	 */
	if ((isp->isp_mdvec->dv_fwlen == 0) ||
	    (isp->isp_confopts & ISP_CFG_NORELOAD)) {
		dodnld = 0;
	}

	if (dodnld) {
		for (i = 0; i < isp->isp_mdvec->dv_fwlen; i++) {
			mbs.param[0] = MBOX_WRITE_RAM_WORD;
			mbs.param[1] = isp->isp_mdvec->dv_codeorg + i;
			mbs.param[2] = isp->isp_mdvec->dv_ispfw[i];
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				isp_dumpregs(isp, "f/w download failed");

				ISP_UNLOCK;
				return;
			}
		}

		if (isp->isp_mdvec->dv_fwlen) {
			/*
			 * Verify that it downloaded correctly.
			 */
			mbs.param[0] = MBOX_VERIFY_CHECKSUM;
			mbs.param[1] = isp->isp_mdvec->dv_codeorg;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				isp_dumpregs(isp, "ram checksum failure");
				ISP_UNLOCK;
				return;
			}
		}
	} else {
		IDPRINTF(3, ("%s: skipping f/w download\n", isp->isp_name));
	}

	/*
	 * Now start it rolling.
	 *
	 * If we didn't actually download f/w,
	 * we still need to (re)start it.
	 */

	mbs.param[0] = MBOX_EXEC_FIRMWARE;
	mbs.param[1] = isp->isp_mdvec->dv_codeorg;
	isp_mboxcmd(isp, &mbs);

	if (isp->isp_type & ISP_HA_SCSI) {
		/*
		 * Set CLOCK RATE
		 */
		if (((sdparam *)isp->isp_param)->isp_clock) {
			mbs.param[0] = MBOX_SET_CLOCK_RATE;
			mbs.param[1] = ((sdparam *)isp->isp_param)->isp_clock;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				isp_dumpregs(isp, "failed to set CLOCKRATE");
				ISP_UNLOCK;
				return;
			}
		}
	}
	mbs.param[0] = MBOX_ABOUT_FIRMWARE;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "ABOUT FIRMWARE command failed");
		ISP_UNLOCK;
		return;
	}
	PRINTF("%s: Board Revision %s, %s F/W Revision %d.%d\n",
		isp->isp_name, revname, dodnld? "loaded" : "ROM",
		mbs.param[1], mbs.param[2]);
	isp_fw_state(isp);
	isp->isp_state = ISP_RESETSTATE;
	ISP_UNLOCK;
}

/*
 * Abort an executing command.
 * Locks (ints blocked) assumed held.
 */

int
isp_abortcmd(isp, xidx)
	struct ispsoftc *isp;
	int xidx;
{
	mbreg_t mbs;
	ISP_SCSI_XFER_T *xs;

	xs = (ISP_SCSI_XFER_T *) isp->isp_xflist[xidx];
	if (xs == NULL) {
		PRINTF("%s: isp_abortcmd - NULL xs\n", isp->isp_name);
		return (0);
	}
	mbs.param[0] = MBOX_ABORT;
	mbs.param[1] = XS_TGT(xs) | XS_LUN(xs);
	mbs.param[2] = (xidx+1) >> 16;
	mbs.param[3] = (xidx+1) & 0xffff;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("%s: isp_abort failure (code %x)\n",
		       isp->isp_name, mbs.param[0]);
		return (0);
	} else {
		return (1);
	}
}

/*
 * Initialize Hardware to known state
 */
void
isp_init(isp)
	struct ispsoftc *isp;
{
	sdparam *sdp;
	mbreg_t mbs;
	int i, l;
	ISP_LOCKVAL_DECL;

	if (isp->isp_type & ISP_HA_FC) {
		isp_fibre_init(isp);
		return;
	}

	sdp = isp->isp_param;

	/*
	 * Try and figure out if we're connected to a differential bus.
	 * You have to pause the RISC processor to read SXP registers.
	 *
	 * This, by the way, is likely broken in that it should be
	 * getting this info from NVRAM settings too.
	 */
	ISP_LOCK;
	ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);
	if (ISP_READ(isp, SXP_PINS_DIFF) & SXP_PINS_DIFF_SENSE) {
		sdp->isp_diffmode = 1;
		PRINTF("%s: Differential Mode\n", isp->isp_name);
	} else {
		/*
		 * Force pullups on.
		 */
		sdp->isp_req_ack_active_neg = 1;
		sdp->isp_data_line_active_neg = 1;
		sdp->isp_diffmode = 0;
	}
	ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE); /* release paused processor */

	mbs.param[0] = MBOX_GET_INIT_SCSI_ID;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_UNLOCK;
		isp_dumpregs(isp, "failed to get initiator id");
		return;
	}
	if (mbs.param[1] != sdp->isp_initiator_id) {
		PRINTF("%s: setting Initiator ID to %d\n", isp->isp_name,
			sdp->isp_initiator_id);
		mbs.param[0] = MBOX_SET_INIT_SCSI_ID;
		mbs.param[1] = sdp->isp_initiator_id;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			ISP_UNLOCK;
			isp_dumpregs(isp, "failed to set initiator id");
			return;
		}
	} else {
		IDPRINTF(3, ("%s: leaving Initiator ID at %d\n", isp->isp_name,
			sdp->isp_initiator_id));
	}

	mbs.param[0] = MBOX_SET_RETRY_COUNT;
	mbs.param[1] = sdp->isp_retry_count;
	mbs.param[2] = sdp->isp_retry_delay;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_UNLOCK;
		isp_dumpregs(isp, "failed to set retry count and delay");
		return;
	}

	mbs.param[0] = MBOX_SET_ASYNC_DATA_SETUP_TIME;
	mbs.param[1] = sdp->isp_async_data_setup;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_UNLOCK;
		isp_dumpregs(isp, "failed to set async data setup time");
		return;
	}

	mbs.param[0] = MBOX_SET_ACTIVE_NEG_STATE;
	mbs.param[1] =	(sdp->isp_req_ack_active_neg << 4) |
			(sdp->isp_data_line_active_neg << 5);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_UNLOCK;
		isp_dumpregs(isp, "failed to set active neg state");
		return;
	}

	mbs.param[0] = MBOX_SET_TAG_AGE_LIMIT;
	mbs.param[1] = sdp->isp_tag_aging;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_UNLOCK;
		isp_dumpregs(isp, "failed to set tag age limit");
		return;
	}

	mbs.param[0] = MBOX_SET_SELECT_TIMEOUT;
	mbs.param[1] = sdp->isp_selection_timeout;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_UNLOCK;
		isp_dumpregs(isp, "failed to set selection timeout");
		return;
	}

	IDPRINTF(2, ("%s: devparm, W=wide, S=sync, T=Tag\n", isp->isp_name));
	for (i = 0; i < MAX_TARGETS; i++) {
		char bz[9];
		u_int16_t cj = sdp->isp_devparam[i].sync_period;

		if (sdp->isp_devparam[i].dev_flags & DPARM_SYNC) {
			u_int16_t x;
			if (cj == (ISP_20M_SYNCPARMS & 0xff)) {
				x = 20;
			} else if (cj == (ISP_10M_SYNCPARMS & 0xff)) {
				x = 10;
			} else if (cj == (ISP_08M_SYNCPARMS & 0xff)) {
				x = 8;
			} else if (cj == (ISP_05M_SYNCPARMS & 0xff)) {
				x = 5;
			} else if (cj == (ISP_04M_SYNCPARMS & 0xff)) {
				x = 4;
			} else {
				x = 0;
			}
			if (x)
				sprintf(bz, "%02dMHz:", x);
			else
				sprintf(bz, "?%04x:", cj);
		} else {
			sprintf(bz, "Async:");
		}
		if (sdp->isp_devparam[i].dev_flags & DPARM_WIDE)
			bz[6] = 'W';
		else
			bz[6] = ' ';
		if (sdp->isp_devparam[i].dev_flags & DPARM_TQING)
			bz[7] = 'T';
		else
			bz[7] = ' ';
		bz[8] = 0;
		IDPRINTF(2, (" id%x:%s", i, bz));
		if (((i+1) & 0x3) == 0)
			IDPRINTF(2, ("\n"));
		if (sdp->isp_devparam[i].dev_enable == 0)
			continue;

		/*
		 * It is not safe to run the 1020 in ultra mode.
		 */
		if (isp->isp_type == ISP_HA_SCSI_1020 &&
		    cj == (ISP_20M_SYNCPARMS & 0xff)) {
			PRINTF("%s: an ISP1020 set to Ultra Speed- derating.\n",
				isp->isp_name);
			sdp->isp_devparam[i].sync_offset =
				ISP_10M_SYNCPARMS >> 8;
			sdp->isp_devparam[i].sync_period =
				ISP_10M_SYNCPARMS & 0xff;
		}
		mbs.param[0] = MBOX_SET_TARGET_PARAMS;
		mbs.param[1] = i << 8;
		mbs.param[2] = sdp->isp_devparam[i].dev_flags << 8;
		mbs.param[3] =
			(sdp->isp_devparam[i].sync_offset << 8) |
			(sdp->isp_devparam[i].sync_period);

		IDPRINTF(3, ("\n%s: target %d flags %x offset %x period %x\n",
			     isp->isp_name, i, sdp->isp_devparam[i].dev_flags,
			     sdp->isp_devparam[i].sync_offset,
			     sdp->isp_devparam[i].sync_period));
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			PRINTF("%s: failed to set parameters for target %d\n",
				isp->isp_name, i);
			PRINTF("%s: flags %x offset %x period %x\n",
				isp->isp_name, sdp->isp_devparam[i].dev_flags,
				sdp->isp_devparam[i].sync_offset,
				sdp->isp_devparam[i].sync_period);
			mbs.param[0] = MBOX_SET_TARGET_PARAMS;
			mbs.param[1] = i << 8;
			mbs.param[2] = DPARM_DEFAULT << 8;
			mbs.param[3] = ISP_10M_SYNCPARMS;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				ISP_UNLOCK;
				PRINTF("%s: failed even to set defaults\n",
					isp->isp_name);
				return;
			}
		}
		for (l = 0; l < MAX_LUNS; l++) {
			mbs.param[0] = MBOX_SET_DEV_QUEUE_PARAMS;
			mbs.param[1] = (i << 8) | l;
			mbs.param[2] = sdp->isp_max_queue_depth;
			mbs.param[3] = sdp->isp_devparam[i].exc_throttle;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				ISP_UNLOCK;
				isp_dumpregs(isp, "failed to set device queue "
				       "parameters");
				return;
			}
		}
	}

	/*
	 * Set up DMA for the request and result mailboxes.
	 */
	if (ISP_MBOXDMASETUP(isp)) {
		ISP_UNLOCK;
		PRINTF("%s: can't setup dma mailboxes\n", isp->isp_name);
		return;
	}
		
	mbs.param[0] = MBOX_INIT_RES_QUEUE;
	mbs.param[1] = RESULT_QUEUE_LEN(isp);
	mbs.param[2] = (u_int16_t) (isp->isp_result_dma >> 16);
	mbs.param[3] = (u_int16_t) (isp->isp_result_dma & 0xffff);
	mbs.param[4] = 0;
	mbs.param[5] = 0;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_UNLOCK;
		isp_dumpregs(isp, "set of response queue failed");
		return;
	}
	isp->isp_residx = 0;

	mbs.param[0] = MBOX_INIT_REQ_QUEUE;
	mbs.param[1] = RQUEST_QUEUE_LEN(isp);
	mbs.param[2] = (u_int16_t) (isp->isp_rquest_dma >> 16);
	mbs.param[3] = (u_int16_t) (isp->isp_rquest_dma & 0xffff);
	mbs.param[4] = 0;
	mbs.param[5] = 0;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_UNLOCK;
		isp_dumpregs(isp, "set of request queue failed");
		return;
	}
	isp->isp_reqidx = 0;

	/*	
	 * Unfortunately, this is the only way right now for
	 * forcing a sync renegotiation. If we boot off of
	 * an Alpha, it's put the chip in SYNC mode, but we
	 * haven't necessarily set up the parameters the
	 * same, so we'll have to yank the reset line to
	 * get everyone to renegotiate.
	 */

	mbs.param[0] = MBOX_BUS_RESET;
	mbs.param[1] = 2;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_UNLOCK;
		isp_dumpregs(isp, "SCSI bus reset failed");
	}
	/*
	 * This is really important to have set after a bus reset.
	 */
	isp->isp_sendmarker = 1;
	ISP_UNLOCK;
	isp->isp_state = ISP_INITSTATE;
}

static void
isp_fibre_init(isp)
	struct ispsoftc *isp;
{
	fcparam *fcp;
	isp_icb_t *icbp;
	mbreg_t mbs;
	int count;
	u_int8_t lwfs;
	ISP_LOCKVAL_DECL;

	fcp = isp->isp_param;

	fcp->isp_retry_count = 0;
	fcp->isp_retry_delay = 1;

	ISP_LOCK;
	mbs.param[0] = MBOX_SET_RETRY_COUNT;
	mbs.param[1] = fcp->isp_retry_count;
	mbs.param[2] = fcp->isp_retry_delay;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_UNLOCK;
		isp_dumpregs(isp, "failed to set retry count and delay");
		return;
	}

	if (ISP_MBOXDMASETUP(isp)) {
		ISP_UNLOCK;
		PRINTF("%s: can't setup DMA for mailboxes\n", isp->isp_name);
		return;
	}

	icbp = (isp_icb_t *) fcp->isp_scratch;
	bzero(icbp, sizeof (*icbp));
#if 0
	icbp->icb_maxfrmlen = ICB_DFLT_FRMLEN;
	MAKE_NODE_NAME(isp, icbp);
	icbp->icb_rqstqlen = RQUEST_QUEUE_LEN(isp);
	icbp->icb_rsltqlen = RESULT_QUEUE_LEN(isp);
	icbp->icb_rqstaddr[0] = (u_int16_t) (isp->isp_rquest_dma & 0xffff);
	icbp->icb_rqstaddr[1] = (u_int16_t) (isp->isp_rquest_dma >> 16);
	icbp->icb_respaddr[0] = (u_int16_t) (isp->isp_result_dma & 0xffff);
	icbp->icb_respaddr[1] = (u_int16_t) (isp->isp_result_dma >> 16);
#endif
	icbp->icb_version = 1;
	icbp->icb_maxfrmlen = ICB_DFLT_FRMLEN;
	icbp->icb_maxalloc = 256;
	icbp->icb_execthrottle = 16;
	icbp->icb_retry_delay = 5;
	icbp->icb_retry_count = 0;
	MAKE_NODE_NAME(isp, icbp);
	icbp->icb_rqstqlen = RQUEST_QUEUE_LEN(isp);
	icbp->icb_rsltqlen = RESULT_QUEUE_LEN(isp);
	icbp->icb_rqstaddr[0] = (u_int16_t) (isp->isp_rquest_dma & 0xffff);
	icbp->icb_rqstaddr[1] = (u_int16_t) (isp->isp_rquest_dma >> 16);
	icbp->icb_respaddr[0] = (u_int16_t) (isp->isp_result_dma & 0xffff);
	icbp->icb_respaddr[1] = (u_int16_t) (isp->isp_result_dma >> 16);

	mbs.param[0] = MBOX_INIT_FIRMWARE;
	mbs.param[1] = 0;
	mbs.param[2] = (u_int16_t) (fcp->isp_scdma >> 16);
	mbs.param[3] = (u_int16_t) (fcp->isp_scdma & 0xffff);
	mbs.param[4] = 0;
	mbs.param[5] = 0;
	mbs.param[6] = 0;
	mbs.param[7] = 0;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_UNLOCK;
		isp_dumpregs(isp, "INIT FIRMWARE failed");
		return;
	}
	isp->isp_reqidx = 0;
	isp->isp_residx = 0;

	/*
	 * Wait up to 3 seconds for FW to go to READY state.
	 *
	 * This is all very much not right. The problem here
	 * is that the cable may not be plugged in, or there
	 * may be many many members of the loop that haven't
	 * been logged into.
	 *
	 * This model of doing things doesn't support dynamic
	 * attachment, so we just plain lose (for now).
	 */
	lwfs = FW_CONFIG_WAIT;
	for (count = 0; count < 3000; count++) {
		isp_fw_state(isp);
		if (lwfs != fcp->isp_fwstate) {
			PRINTF("%s: Firmware State %s -> %s\n", isp->isp_name, 
			    fw_statename(lwfs), fw_statename(fcp->isp_fwstate));
			lwfs = fcp->isp_fwstate;
		}
		if (fcp->isp_fwstate == FW_READY) {
			break;
		}
		SYS_DELAY(1000);	/* wait one millisecond */
	}
	isp->isp_sendmarker = 1;

	/*
	 * Get our Loop ID
	 * (if possible)
	 */
	if (fcp->isp_fwstate == FW_READY) {
		mbs.param[0] = MBOX_GET_LOOP_ID;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_dumpregs(isp, "GET LOOP ID failed");
			ISP_UNLOCK;
			return;
		}
		fcp->isp_loopid = mbs.param[1];
		if (fcp->isp_loopid) {
			PRINTF("%s: Loop ID 0x%x\n", isp->isp_name,
				fcp->isp_loopid);
		}
		isp->isp_state = ISP_INITSTATE;
	}
	ISP_UNLOCK;
}

/*
 * Free any associated resources prior to decommissioning.
 */
void
isp_uninit(isp)
	struct ispsoftc *isp;
{
	STOP_WATCHDOG(isp_watch, isp);
}


/*
 * start an xfer
 */
int32_t
ispscsicmd(xs)
	ISP_SCSI_XFER_T *xs;
{
	struct ispsoftc *isp;
	u_int8_t iptr, optr;
	union {
		ispreq_t *_reqp;
		ispreqt2_t *_t2reqp;
	} _u;
#define	reqp	_u._reqp
#define	t2reqp	_u._t2reqp
#define	UZSIZE	max(sizeof (ispreq_t), sizeof (ispreqt2_t))
	int i;
	ISP_LOCKVAL_DECL;

	XS_INITERR(xs);
	isp = XS_ISP(xs);

	if (isp->isp_type & ISP_HA_FC) {
		if (XS_CDBLEN(xs) > 12) {
			PRINTF("%s: unsupported cdb length for fibre (%d)\n", 
				isp->isp_name, XS_CDBLEN(xs));
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
		}
	}
	optr = ISP_READ(isp, OUTMAILBOX4);
	iptr = isp->isp_reqidx;

	reqp = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
	iptr = (iptr + 1) & (RQUEST_QUEUE_LEN(isp) - 1);
	if (iptr == optr) {
		PRINTF("%s: Request Queue Overflow\n", isp->isp_name);
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	}

	ISP_LOCK;
	if (isp->isp_type & ISP_HA_FC)
		DISABLE_INTS(isp);

	if (isp->isp_sendmarker) {
		ispmarkreq_t *marker = (ispmarkreq_t *) reqp;

		bzero((void *) marker, sizeof (*marker));
		marker->req_header.rqs_entry_count = 1;
		marker->req_header.rqs_entry_type = RQSTYPE_MARKER;
		marker->req_modifier = SYNC_ALL;

		isp->isp_sendmarker = 0;

		if (((iptr + 1) & (RQUEST_QUEUE_LEN(isp) - 1)) == optr) {
			ISP_WRITE(isp, INMAILBOX4, iptr);
			isp->isp_reqidx = iptr;

			if (isp->isp_type & ISP_HA_FC)
				ENABLE_INTS(isp);
			ISP_UNLOCK;
			PRINTF("%s: Request Queue Overflow+\n", isp->isp_name);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_EAGAIN);
		}
		reqp = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
		iptr = (iptr + 1) & (RQUEST_QUEUE_LEN(isp) - 1);
	}

	bzero((void *) reqp, UZSIZE);
	reqp->req_header.rqs_entry_count = 1;
	if (isp->isp_type & ISP_HA_FC) {
		reqp->req_header.rqs_entry_type = RQSTYPE_T2RQS;
	} else {
		reqp->req_header.rqs_entry_type = RQSTYPE_REQUEST;
	}
	reqp->req_header.rqs_flags = 0;
	reqp->req_header.rqs_seqno = isp->isp_seqno++;

	for (i = 0; i < RQUEST_QUEUE_LEN(isp); i++) {
		if (isp->isp_xflist[i] == NULL)
			break;
	}
	if (i == RQUEST_QUEUE_LEN(isp)) {
		if (isp->isp_type & ISP_HA_FC)
			ENABLE_INTS(isp);
		ISP_UNLOCK;
		PRINTF("%s: ran out of xflist pointers?????\n", isp->isp_name);
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	} else {
		/*
		 * Never have a handle that is zero, so
		 * set req_handle off by one.
		 */
		isp->isp_xflist[i] = xs;
		reqp->req_handle = i+1;
	}

	if (isp->isp_type & ISP_HA_FC) {
		/*
		 * See comment in isp_intr
		 */
		XS_RESID(xs) = 0;
		/*
		 * Fibre Channel always requires some kind of tag.
		 */
		if (XS_POLLDCMD(xs)) {
			t2reqp->req_flags = REQFLAG_STAG;
		} else {
			t2reqp->req_flags = REQFLAG_OTAG;
		}
	} else {
		sdparam *sdp = (sdparam *)isp->isp_param;
		if ((sdp->isp_devparam[XS_TGT(xs)].dev_flags & DPARM_TQING) &&
		    (XS_POLLDCMD(xs) == 0)) {
			reqp->req_flags = REQFLAG_OTAG;
		} else {
			reqp->req_flags = 0;
		}
	}
	reqp->req_lun_trn = XS_LUN(xs);
	reqp->req_target = XS_TGT(xs);
	if (isp->isp_type & ISP_HA_SCSI) {
		reqp->req_cdblen = XS_CDBLEN(xs);
	}
	bcopy((void *)XS_CDBP(xs), reqp->req_cdb, XS_CDBLEN(xs));

	IDPRINTF(6, ("%s(%d.%d): START%d cmd 0x%x datalen %d\n", isp->isp_name,
	    XS_TGT(xs), XS_LUN(xs), reqp->req_header.rqs_seqno,
	    *(u_char *) XS_CDBP(xs), XS_XFRLEN(xs)));

	reqp->req_time = XS_TIME(xs) / 1000;
	if (reqp->req_time == 0 && XS_TIME(xs))
		reqp->req_time = 1;
	if (ISP_DMASETUP(isp, xs, reqp, &iptr, optr)) {
		if (isp->isp_type & ISP_HA_FC)
			ENABLE_INTS(isp);
		ISP_UNLOCK;
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}
	XS_SETERR(xs, HBA_NOERROR);
	ISP_WRITE(isp, INMAILBOX4, iptr);
	isp->isp_reqidx = iptr;
	if (isp->isp_type & ISP_HA_FC)
		ENABLE_INTS(isp);
	isp->isp_nactive++;
	if (XS_POLLDCMD(xs) == 0) {
		ISP_UNLOCK;
		return (CMD_QUEUED);
	}

	/*
	 * If we can't use interrupts, poll on completion.
	 */
	if (isp_poll(isp, xs, XS_TIME(xs))) {
		/*
		 * If no other error occurred but we didn't finish,
		 * something bad happened.
		 */
		if (XS_IS_CMD_DONE(xs) == 0) {
			isp->isp_nactive--;
			if (isp->isp_nactive < 0)
				isp->isp_nactive = 0;
			if (XS_NOERR(xs)) {
				isp_lostcmd(isp, xs, reqp);
				XS_SETERR(xs, HBA_BOTCH);
			}
		}
	}
	ISP_UNLOCK;
	return (CMD_COMPLETE);
#undef	reqp
#undef	t2reqp
}

/*
 * Interrupt Service Routine(s)
 */

int
isp_poll(isp, xs, mswait)
	struct ispsoftc *isp;
	ISP_SCSI_XFER_T *xs;
	int mswait;
{

	while (mswait) {
		/* Try the interrupt handling routine */
		(void)isp_intr((void *)isp);

		/* See if the xs is now done */
		if (XS_IS_CMD_DONE(xs))
			return (0);
		SYS_DELAY(1000);	/* wait one millisecond */
		mswait--;
	}
	return (1);
}

int
isp_intr(arg)
	void *arg;
{
	ISP_SCSI_XFER_T *xs;
	struct ispsoftc *isp = arg;
	u_int16_t iptr, optr, isr;

	isr = ISP_READ(isp, BIU_ISR);
	if (isp->isp_type & ISP_HA_FC) {
		if (isr == 0 || (isr & BIU2100_ISR_RISC_INT) == 0) {
			if (isr) {
				IDPRINTF(4, ("%s: isp_intr isr=%x\n",
					     isp->isp_name, isr));
			}
			return (0);
		}
	} else {
		if (isr == 0 || (isr & BIU_ISR_RISC_INT) == 0) {
			if (isr) {
				IDPRINTF(4, ("%s: isp_intr isr=%x\n",
					     isp->isp_name, isr));
			}
			return (0);
		}
	}

	optr = isp->isp_residx;
	if (ISP_READ(isp, BIU_SEMA) & 1) {
		u_int16_t mbox0 = ISP_READ(isp, OUTMAILBOX0);
		switch (mbox0) {
		case ASYNC_BUS_RESET:
		case ASYNC_TIMEOUT_RESET:
			PRINTF("%s: bus or timeout reset\n", isp->isp_name);
			isp->isp_sendmarker = 1;
			break;
		case ASYNC_LIP_OCCURRED:
			PRINTF("%s: LIP occurred\n", isp->isp_name);
			break;
		case ASYNC_LOOP_UP:
			PRINTF("%s: Loop UP\n", isp->isp_name);
			break;
		case ASYNC_LOOP_DOWN:
			PRINTF("%s: Loop DOWN\n", isp->isp_name);
			break;
		case ASYNC_LOOP_RESET:
			PRINTF("%s: Loop RESET\n", isp->isp_name);
			break;
		default:
			PRINTF("%s: async %x\n", isp->isp_name, mbox0);
			break;
		}
		ISP_WRITE(isp, BIU_SEMA, 0);
	}

	ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
	iptr = ISP_READ(isp, OUTMAILBOX5);
	if (optr == iptr) {
		IDPRINTF(4, ("why intr? isr %x iptr %x optr %x\n",
			isr, optr, iptr));
	}
	ENABLE_INTS(isp);

	while (optr != iptr) {
		ispstatusreq_t *sp;
		int buddaboom = 0;

		sp = (ispstatusreq_t *) ISP_QUEUE_ENTRY(isp->isp_result, optr);

		optr = (optr + 1) & (RESULT_QUEUE_LEN(isp)-1);
		if (sp->req_header.rqs_entry_type != RQSTYPE_RESPONSE) {
			PRINTF("%s: not RESPONSE in RESPONSE Queue (0x%x)\n",
				isp->isp_name, sp->req_header.rqs_entry_type);
			if (sp->req_header.rqs_entry_type != RQSTYPE_REQUEST) {
				ISP_WRITE(isp, INMAILBOX5, optr);
				continue;
			}
			buddaboom = 1;
		}

		if (sp->req_header.rqs_flags & 0xf) {
			if (sp->req_header.rqs_flags & RQSFLAG_CONTINUATION) {
				ISP_WRITE(isp, INMAILBOX5, optr);
				continue;
			}
			PRINTF("%s: rqs_flags=%x\n", isp->isp_name,
				sp->req_header.rqs_flags & 0xf);
		}
		if (sp->req_handle > RQUEST_QUEUE_LEN(isp) ||
		    sp->req_handle < 1) {
			PRINTF("%s: bad request handle %d\n", isp->isp_name,
				sp->req_handle);
			ISP_WRITE(isp, INMAILBOX5, optr);
			continue;
		}
		xs = (ISP_SCSI_XFER_T *) isp->isp_xflist[sp->req_handle - 1];
		if (xs == NULL) {
			PRINTF("%s: NULL xs in xflist\n", isp->isp_name);
			ISP_WRITE(isp, INMAILBOX5, optr);
			continue;
		}
		isp->isp_xflist[sp->req_handle - 1] = NULL;
		if (sp->req_status_flags & RQSTF_BUS_RESET) {
			isp->isp_sendmarker = 1;
		}
		if (buddaboom) {
			XS_SETERR(xs, HBA_BOTCH);
		}
		XS_STS(xs) = sp->req_scsi_status & 0xff;
		if (isp->isp_type & ISP_HA_SCSI) {
			if (sp->req_state_flags & RQSF_GOT_SENSE) {
				bcopy(sp->req_sense_data, XS_SNSP(xs),
					XS_SNSLEN(xs));
				XS_SNS_IS_VALID(xs);
			}
		} else {
			if (XS_STS(xs) == SCSI_CHECK) {
				XS_SNS_IS_VALID(xs);
				bcopy(sp->req_sense_data, XS_SNSP(xs),
					XS_SNSLEN(xs));
				sp->req_state_flags |= RQSF_GOT_SENSE;
			}
		}
		if (XS_NOERR(xs) && XS_STS(xs) == SCSI_BUSY) {
			XS_SETERR(xs, HBA_TGTBSY);
		}

		if (sp->req_header.rqs_entry_type == RQSTYPE_RESPONSE) {
			if (XS_NOERR(xs) && sp->req_completion_status)
				isp_parse_status(isp, sp, xs);
		} else {
			PRINTF("%s: unknown return %x\n", isp->isp_name,
				sp->req_header.rqs_entry_type);
			if (XS_NOERR(xs))
				XS_SETERR(xs, HBA_BOTCH);
		}
		if (isp->isp_type & ISP_HA_SCSI) {
			XS_RESID(xs) = sp->req_resid;
		} else if (sp->req_scsi_status & RQCS_RU) {
			XS_RESID(xs) = sp->req_resid;
			IDPRINTF(4, ("%s: cnt %d rsd %d\n", isp->isp_name,
				XS_XFRLEN(xs), sp->req_resid));
		}
		if (XS_XFRLEN(xs)) {
			ISP_DMAFREE(isp, xs, sp->req_handle - 1);
		}
		if ((isp->isp_dblev >= 5) ||
		    (isp->isp_dblev > 2 && !XS_NOERR(xs))) {
			PRINTF("%s(%d.%d): FIN%d cmd0x%x len%d resid%d STS %x",
			    isp->isp_name, XS_TGT(xs), XS_LUN(xs),
			    sp->req_header.rqs_seqno, *(u_char *) XS_CDBP(xs),
			    XS_XFRLEN(xs), XS_RESID(xs), XS_STS(xs));
			if (sp->req_state_flags & RQSF_GOT_SENSE) {
				PRINTF(" Skey: %x", XS_SNSKEY(xs));
				if (!(XS_IS_SNS_VALID(xs))) {
					PRINTF(" BUT NOT SET");
				}
			}
			PRINTF(" XS_ERR(xs) %d\n", XS_ERR(xs));
		}
		ISP_WRITE(isp, INMAILBOX5, optr);
		isp->isp_nactive--;
		if (isp->isp_nactive < 0)
			isp->isp_nactive = 0;
		XS_CMD_DONE(xs);
	}
	isp->isp_residx = optr;
	return (1);
}

/*
 * Support routines.
 */

static void
isp_parse_status(isp, sp, xs)
	struct ispsoftc *isp;
	ispstatusreq_t *sp;
	ISP_SCSI_XFER_T *xs;
{
	switch (sp->req_completion_status) {
	case RQCS_COMPLETE:
		XS_SETERR(xs, HBA_NOERROR);
		return;

	case RQCS_INCOMPLETE:
		if ((sp->req_state_flags & RQSF_GOT_TARGET) == 0) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return;
		}
		PRINTF("%s: incomplete, state %x\n",
			isp->isp_name, sp->req_state_flags);
		break;

	case RQCS_TRANSPORT_ERROR:
		PRINTF("%s: transport error\n", isp->isp_name);
		isp_prtstst(sp);
		break;

	case RQCS_DATA_OVERRUN:
		if (isp->isp_type & ISP_HA_FC) {
			XS_RESID(xs) = sp->req_resid;
			break;
		}
		XS_SETERR(xs, HBA_NOERROR);
		return;

	case RQCS_DATA_UNDERRUN:
		if (isp->isp_type & ISP_HA_FC) {
			XS_RESID(xs) = sp->req_resid;
			/* an UNDERRUN is not a botch ??? */
		}
		XS_SETERR(xs, HBA_NOERROR);
		return;

	case RQCS_TIMEOUT:
		XS_SETERR(xs, HBA_CMDTIMEOUT);
		return;

	case RQCS_RESET_OCCURRED:
		PRINTF("%s: reset occurred, %d active\n", isp->isp_name,
			isp->isp_nactive);
		isp->isp_sendmarker = 1;
		XS_SETERR(xs, HBA_BUSRESET);
		return;

	case RQCS_ABORTED:
		PRINTF("%s: command aborted\n", isp->isp_name);
		isp->isp_sendmarker = 1;
		XS_SETERR(xs, HBA_ABORTED);
		return;

	case RQCS_PORT_UNAVAILABLE:
		/*
		 * No such port on the loop. Moral equivalent of SELTIMEO
		 */
		XS_SETERR(xs, HBA_SELTIMEOUT);
		return;

	case RQCS_PORT_LOGGED_OUT:
		PRINTF("%s: port logout for target %d\n",
			isp->isp_name, XS_TGT(xs));
		break;

	case RQCS_PORT_CHANGED:
		PRINTF("%s: port changed for target %d\n",
			isp->isp_name, XS_TGT(xs));
		break;

	case RQCS_PORT_BUSY:
		PRINTF("%s: port busy for target %d\n",
			isp->isp_name, XS_TGT(xs));
		XS_SETERR(xs, HBA_TGTBSY);
		return;

	default:
		PRINTF("%s: comp status %x\n", isp->isp_name,
		       sp->req_completion_status);
		break;
	}
	XS_SETERR(xs, HBA_BOTCH);
}

#define	HINIB(x)			((x) >> 0x4)
#define	LONIB(x)			((x)  & 0xf)
#define MAKNIB(a, b)			(((a) << 4) | (b))
static u_int8_t mbpcnt[] = {
	MAKNIB(1, 1),	/* 0x00: MBOX_NO_OP */
	MAKNIB(5, 5),	/* 0x01: MBOX_LOAD_RAM */
	MAKNIB(2, 0),	/* 0x02: MBOX_EXEC_FIRMWARE */
	MAKNIB(5, 5),	/* 0x03: MBOX_DUMP_RAM */
	MAKNIB(3, 3),	/* 0x04: MBOX_WRITE_RAM_WORD */
	MAKNIB(2, 3),	/* 0x05: MBOX_READ_RAM_WORD */
	MAKNIB(6, 6),	/* 0x06: MBOX_MAILBOX_REG_TEST */
	MAKNIB(2, 3),	/* 0x07: MBOX_VERIFY_CHECKSUM	*/
	MAKNIB(1, 3),	/* 0x08: MBOX_ABOUT_FIRMWARE */
	MAKNIB(0, 0),	/* 0x09: */
	MAKNIB(0, 0),	/* 0x0a: */
	MAKNIB(0, 0),	/* 0x0b: */
	MAKNIB(0, 0),	/* 0x0c: */
	MAKNIB(0, 0),	/* 0x0d: */
	MAKNIB(1, 2),	/* 0x0e: MBOX_CHECK_FIRMWARE */
	MAKNIB(0, 0),	/* 0x0f: */
	MAKNIB(5, 5),	/* 0x10: MBOX_INIT_REQ_QUEUE */
	MAKNIB(6, 6),	/* 0x11: MBOX_INIT_RES_QUEUE */
	MAKNIB(4, 4),	/* 0x12: MBOX_EXECUTE_IOCB */
	MAKNIB(2, 2),	/* 0x13: MBOX_WAKE_UP	*/
	MAKNIB(1, 6),	/* 0x14: MBOX_STOP_FIRMWARE */
	MAKNIB(4, 4),	/* 0x15: MBOX_ABORT */
	MAKNIB(2, 2),	/* 0x16: MBOX_ABORT_DEVICE */
	MAKNIB(3, 3),	/* 0x17: MBOX_ABORT_TARGET */
	MAKNIB(2, 2),	/* 0x18: MBOX_BUS_RESET */
	MAKNIB(2, 3),	/* 0x19: MBOX_STOP_QUEUE */
	MAKNIB(2, 3),	/* 0x1a: MBOX_START_QUEUE */
	MAKNIB(2, 3),	/* 0x1b: MBOX_SINGLE_STEP_QUEUE */
	MAKNIB(2, 3),	/* 0x1c: MBOX_ABORT_QUEUE */
	MAKNIB(2, 4),	/* 0x1d: MBOX_GET_DEV_QUEUE_STATUS */
	MAKNIB(0, 0),	/* 0x1e: */
	MAKNIB(1, 3),	/* 0x1f: MBOX_GET_FIRMWARE_STATUS */
	MAKNIB(1, 2),	/* 0x20: MBOX_GET_INIT_SCSI_ID */
	MAKNIB(1, 2),	/* 0x21: MBOX_GET_SELECT_TIMEOUT */
	MAKNIB(1, 3),	/* 0x22: MBOX_GET_RETRY_COUNT	*/
	MAKNIB(1, 2),	/* 0x23: MBOX_GET_TAG_AGE_LIMIT */
	MAKNIB(1, 2),	/* 0x24: MBOX_GET_CLOCK_RATE */
	MAKNIB(1, 2),	/* 0x25: MBOX_GET_ACT_NEG_STATE */
	MAKNIB(1, 2),	/* 0x26: MBOX_GET_ASYNC_DATA_SETUP_TIME */
	MAKNIB(1, 3),	/* 0x27: MBOX_GET_PCI_PARAMS */
	MAKNIB(2, 4),	/* 0x28: MBOX_GET_TARGET_PARAMS */
	MAKNIB(2, 4),	/* 0x29: MBOX_GET_DEV_QUEUE_PARAMS */
	MAKNIB(0, 0),	/* 0x2a: */
	MAKNIB(0, 0),	/* 0x2b: */
	MAKNIB(0, 0),	/* 0x2c: */
	MAKNIB(0, 0),	/* 0x2d: */
	MAKNIB(0, 0),	/* 0x2e: */
	MAKNIB(0, 0),	/* 0x2f: */
	MAKNIB(2, 2),	/* 0x30: MBOX_SET_INIT_SCSI_ID */
	MAKNIB(2, 2),	/* 0x31: MBOX_SET_SELECT_TIMEOUT */
	MAKNIB(3, 3),	/* 0x32: MBOX_SET_RETRY_COUNT	*/
	MAKNIB(2, 2),	/* 0x33: MBOX_SET_TAG_AGE_LIMIT */
	MAKNIB(2, 2),	/* 0x34: MBOX_SET_CLOCK_RATE */
	MAKNIB(2, 2),	/* 0x35: MBOX_SET_ACTIVE_NEG_STATE */
	MAKNIB(2, 2),	/* 0x36: MBOX_SET_ASYNC_DATA_SETUP_TIME */
	MAKNIB(3, 3),	/* 0x37: MBOX_SET_PCI_CONTROL_PARAMS */
	MAKNIB(4, 4),	/* 0x38: MBOX_SET_TARGET_PARAMS */
	MAKNIB(4, 4),	/* 0x39: MBOX_SET_DEV_QUEUE_PARAMS */
	MAKNIB(0, 0),	/* 0x3a: */
	MAKNIB(0, 0),	/* 0x3b: */
	MAKNIB(0, 0),	/* 0x3c: */
	MAKNIB(0, 0),	/* 0x3d: */
	MAKNIB(0, 0),	/* 0x3e: */
	MAKNIB(0, 0),	/* 0x3f: */
	MAKNIB(1, 2),	/* 0x40: MBOX_RETURN_BIOS_BLOCK_ADDR */
	MAKNIB(6, 1),	/* 0x41: MBOX_WRITE_FOUR_RAM_WORDS */
	MAKNIB(2, 3),	/* 0x42: MBOX_EXEC_BIOS_IOCB */
	MAKNIB(0, 0),	/* 0x43: */
	MAKNIB(0, 0),	/* 0x44: */
	MAKNIB(0, 0),	/* 0x45: */
	MAKNIB(0, 0),	/* 0x46: */
	MAKNIB(0, 0),	/* 0x47: */
	MAKNIB(0, 0),	/* 0x48: */
	MAKNIB(0, 0),	/* 0x49: */
	MAKNIB(0, 0),	/* 0x4a: */
	MAKNIB(0, 0),	/* 0x4b: */
	MAKNIB(0, 0),	/* 0x4c: */
	MAKNIB(0, 0),	/* 0x4d: */
	MAKNIB(0, 0),	/* 0x4e: */
	MAKNIB(0, 0),	/* 0x4f: */
	MAKNIB(0, 0),	/* 0x50: */
	MAKNIB(0, 0),	/* 0x51: */
	MAKNIB(0, 0),	/* 0x52: */
	MAKNIB(0, 0),	/* 0x53: */
	MAKNIB(8, 0),	/* 0x54: MBOX_EXEC_COMMAND_IOCB_A64 */
	MAKNIB(0, 0),	/* 0x55: */
	MAKNIB(0, 0),	/* 0x56: */
	MAKNIB(0, 0),	/* 0x57: */
	MAKNIB(0, 0),	/* 0x58: */
	MAKNIB(0, 0),	/* 0x59: */
	MAKNIB(0, 0),	/* 0x5a: */
	MAKNIB(0, 0),	/* 0x5b: */
	MAKNIB(0, 0),	/* 0x5c: */
	MAKNIB(0, 0),	/* 0x5d: */
	MAKNIB(0, 0),	/* 0x5e: */
	MAKNIB(0, 0),	/* 0x5f: */
	MAKNIB(8, 6),	/* 0x60: MBOX_INIT_FIRMWARE */
	MAKNIB(0, 0),	/* 0x60: MBOX_GET_INIT_CONTROL_BLOCK  (FORMAT?) */
	MAKNIB(2, 1),	/* 0x62: MBOX_INIT_LIP */
	MAKNIB(8, 1),	/* 0x63: MBOX_GET_FC_AL_POSITION_MAP */
	MAKNIB(8, 1),	/* 0x64: MBOX_GET_PORT_DB */
	MAKNIB(3, 1),	/* 0x65: MBOX_CLEAR_ACA */
	MAKNIB(3, 1),	/* 0x66: MBOX_TARGET_RESET */
	MAKNIB(3, 1),	/* 0x67: MBOX_CLEAR_TASK_SET */
	MAKNIB(3, 1),	/* 0x69: MBOX_ABORT_TASK_SET */
	MAKNIB(1, 2)	/* 0x69: MBOX_GET_FW_STATE */
};
#define	NMBCOM	(sizeof (mbpcnt) / sizeof (mbpcnt[0]))

static void
isp_mboxcmd(isp, mbp)
	struct ispsoftc *isp;
	mbreg_t *mbp;
{
	int outparam, inparam;
	int loops;
	u_int8_t opcode;

	if (mbp->param[0] == ISP2100_SET_PCI_PARAM) {
		opcode = mbp->param[0] = MBOX_SET_PCI_PARAMETERS;
		inparam = 4;
		outparam = 4;
		goto command_known;
	} else if (mbp->param[0] > NMBCOM) {
		PRINTF("%s: bad command %x\n", isp->isp_name, mbp->param[0]);
		return;
	}

	opcode = mbp->param[0];
	inparam = HINIB(mbpcnt[mbp->param[0]]);
	outparam =  LONIB(mbpcnt[mbp->param[0]]);

	if (inparam == 0 && outparam == 0) {
		PRINTF("%s: no parameters for %x\n", isp->isp_name,
			mbp->param[0]);
		return;
	}


command_known:
	/*
	 * Make sure we can send some words..
	 */

	loops = MBOX_DELAY_COUNT;
	while ((ISP_READ(isp, HCCR) & HCCR_HOST_INT) != 0) {
		SYS_DELAY(100);
		if (--loops < 0) {
			PRINTF("%s: isp_mboxcmd timeout #1\n", isp->isp_name);
			return;
		}
	}

	/*
	 * Write input parameters
	 */
	switch (inparam) {
	case 8: ISP_WRITE(isp, INMAILBOX7, mbp->param[7]); mbp->param[7] = 0;
	case 7: ISP_WRITE(isp, INMAILBOX6, mbp->param[6]); mbp->param[6] = 0;
	case 6: ISP_WRITE(isp, INMAILBOX5, mbp->param[5]); mbp->param[5] = 0;
	case 5: ISP_WRITE(isp, INMAILBOX4, mbp->param[4]); mbp->param[4] = 0;
	case 4: ISP_WRITE(isp, INMAILBOX3, mbp->param[3]); mbp->param[3] = 0;
	case 3: ISP_WRITE(isp, INMAILBOX2, mbp->param[2]); mbp->param[2] = 0;
	case 2: ISP_WRITE(isp, INMAILBOX1, mbp->param[1]); mbp->param[1] = 0;
	case 1: ISP_WRITE(isp, INMAILBOX0, mbp->param[0]); mbp->param[0] = 0;
	}

	/*
	 * Clear semaphore on mailbox registers
	 */
	ISP_WRITE(isp, BIU_SEMA, 0);

	/*
	 * Clear RISC int condition.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);

	/*
	 * Set Host Interrupt condition so that RISC will pick up mailbox regs.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_SET_HOST_INT);

	/*
	 * Wait until RISC int is set, except 2100
	 */
	if ((isp->isp_type & ISP_HA_FC) == 0) {
		loops = MBOX_DELAY_COUNT;
		while ((ISP_READ(isp, BIU_ISR) & BIU_ISR_RISC_INT) == 0) {
			SYS_DELAY(100);
			if (--loops < 0) {
				PRINTF("%s: isp_mboxcmd timeout #2\n",
				    isp->isp_name);
				return;
			}
		}
	}

	/*
	 * Check to make sure that the semaphore has been set.
	 */
	loops = MBOX_DELAY_COUNT;
	while ((ISP_READ(isp, BIU_SEMA) & 1) == 0) {
		SYS_DELAY(100);
		if (--loops < 0) {
			PRINTF("%s: isp_mboxcmd timeout #3\n", isp->isp_name);
			return;
		}
	}

	/*
	 * Make sure that the MBOX_BUSY has gone away
	 */
	loops = MBOX_DELAY_COUNT;
	while (ISP_READ(isp, OUTMAILBOX0) == MBOX_BUSY) {
		SYS_DELAY(100);
		if (--loops < 0) {
			PRINTF("%s: isp_mboxcmd timeout #4\n", isp->isp_name);
			return;
		}
	}


	/*
	 * Pick up output parameters.
	 */
	switch (outparam) {
	case 8: mbp->param[7] = ISP_READ(isp, OUTMAILBOX7);
	case 7: mbp->param[6] = ISP_READ(isp, OUTMAILBOX6);
	case 6: mbp->param[5] = ISP_READ(isp, OUTMAILBOX5);
	case 5: mbp->param[4] = ISP_READ(isp, OUTMAILBOX4);
	case 4: mbp->param[3] = ISP_READ(isp, OUTMAILBOX3);
	case 3: mbp->param[2] = ISP_READ(isp, OUTMAILBOX2);
	case 2: mbp->param[1] = ISP_READ(isp, OUTMAILBOX1);
	case 1: mbp->param[0] = ISP_READ(isp, OUTMAILBOX0);
	}

	/*
	 * Clear RISC int.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);

	/*
	 * Release semaphore on mailbox registers
	 */
	ISP_WRITE(isp, BIU_SEMA, 0);

	/*
	 * Just to be chatty here...
	 */
	switch(mbp->param[0]) {
	case MBOX_COMMAND_COMPLETE:
		break;
	case MBOX_INVALID_COMMAND:
		/*
		 * GET_CLOCK_RATE can fail a lot
		 * So can a couple of other commands.
		 */
		if (isp->isp_dblev > 2  && opcode != MBOX_GET_CLOCK_RATE) {
			PRINTF("%s: mbox cmd %x failed with INVALID_COMMAND\n",
				isp->isp_name, opcode);
		}
		break;
	case MBOX_HOST_INTERFACE_ERROR:
		PRINTF("%s: mbox cmd %x failed with HOST_INTERFACE_ERROR\n",
			isp->isp_name, opcode);
		break;
	case MBOX_TEST_FAILED:
		PRINTF("%s: mbox cmd %x failed with TEST_FAILED\n",
			isp->isp_name, opcode);
		break;
	case MBOX_COMMAND_ERROR:
		PRINTF("%s: mbox cmd %x failed with COMMAND_ERROR\n",
			isp->isp_name, opcode);
		break;
	case MBOX_COMMAND_PARAM_ERROR:
		PRINTF("%s: mbox cmd %x failed with COMMAND_PARAM_ERROR\n",
			isp->isp_name, opcode);
		break;

	case ASYNC_LIP_OCCURRED:
		break;

	default:
		/*
		 * The expected return of EXEC_FIRMWARE is zero.
		 */
		if ((opcode == MBOX_EXEC_FIRMWARE && mbp->param[0] != 0) ||
		    (opcode != MBOX_EXEC_FIRMWARE)) {
			PRINTF("%s: mbox cmd %x failed with error %x\n",
				isp->isp_name, opcode, mbp->param[0]);
		}
		break;
	}
}

static void
isp_lostcmd(struct ispsoftc *isp, ISP_SCSI_XFER_T *xs, ispreq_t *req)
{
	mbreg_t mbs;

	mbs.param[0] = MBOX_GET_FIRMWARE_STATUS;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "couldn't GET FIRMWARE STATUS");
		return;
	}
	if (mbs.param[1]) {
		PRINTF("%s: %d commands on completion queue\n",
		       isp->isp_name, mbs.param[1]);
	}
	if (XS_NULL(xs))
		return;

	mbs.param[0] = MBOX_GET_DEV_QUEUE_STATUS;
	mbs.param[1] = XS_TGT(xs) << 8 | XS_LUN(xs);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "couldn't GET DEVICE QUEUE STATUS");
		return;
	}
	PRINTF("%s: lost command for target %d lun %d, %d active of %d, "
		"Queue State: %x\n", isp->isp_name, XS_TGT(xs),
		XS_LUN(xs), mbs.param[2], mbs.param[3], mbs.param[1]);

	isp_dumpregs(isp, "lost command");
	/*
	 * XXX: Need to try and do something to recover.
	 */
}

static void
isp_dumpregs(struct ispsoftc *isp, const char *msg)
{
	PRINTF("%s: %s\n", isp->isp_name, msg);
	if (isp->isp_type & ISP_HA_SCSI)
		PRINTF("    biu_conf1=%x", ISP_READ(isp, BIU_CONF1));
	else
		PRINTF("    biu_csr=%x", ISP_READ(isp, BIU2100_CSR));
	PRINTF(" biu_icr=%x biu_isr=%x biu_sema=%x ", ISP_READ(isp, BIU_ICR),
	       ISP_READ(isp, BIU_ISR), ISP_READ(isp, BIU_SEMA));
	PRINTF("risc_hccr=%x\n", ISP_READ(isp, HCCR));

	if (isp->isp_type & ISP_HA_SCSI) {
		ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);
		PRINTF("    cdma_conf=%x cdma_sts=%x cdma_fifostat=%x\n",
			ISP_READ(isp, CDMA_CONF), ISP_READ(isp, CDMA_STATUS),
			ISP_READ(isp, CDMA_FIFO_STS));
		PRINTF("    ddma_conf=%x ddma_sts=%x ddma_fifostat=%x\n",
			ISP_READ(isp, DDMA_CONF), ISP_READ(isp, DDMA_STATUS),
			ISP_READ(isp, DDMA_FIFO_STS));
		PRINTF("    sxp_int=%x sxp_gross=%x sxp(scsi_ctrl)=%x\n",
			ISP_READ(isp, SXP_INTERRUPT),
			ISP_READ(isp, SXP_GROSS_ERR),
			ISP_READ(isp, SXP_PINS_CONTROL));
		ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE);
	}
	ISP_DUMPREGS(isp);
}

static void
isp_fw_state(struct ispsoftc *isp)
{
	mbreg_t mbs;
	if (isp->isp_type & ISP_HA_FC) {
		int once = 0;
		fcparam *fcp = isp->isp_param;
again:
		mbs.param[0] = MBOX_GET_FW_STATE;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			if (mbs.param[0] == ASYNC_LIP_OCCURRED) {
				if (!once++) {
					goto again;
				}
			}
			isp_dumpregs(isp, "GET FIRMWARE STATE failed");
			return;
		}
		fcp->isp_fwstate = mbs.param[1];
	}
}

static void
isp_setdparm(struct ispsoftc *isp)
{
	int i;
	mbreg_t mbs;
	sdparam *sdp;

	isp->isp_fwrev = 0;
	if (isp->isp_type & ISP_HA_FC) {
		/*
		 * ROM in 2100 doesn't appear to support ABOUT_FIRMWARE
		 */
		return;
	}

	mbs.param[0] = MBOX_ABOUT_FIRMWARE;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		IDPRINTF(3, ("1st ABOUT FIRMWARE command failed"));
	} else {
		isp->isp_fwrev =
			(((u_int16_t) mbs.param[1]) << 10) + mbs.param[2];
	}


	sdp = (sdparam *) isp->isp_param;
	/*
	 * Try and get old clock rate out before we hit the
	 * chip over the head- but if and only if we don't
	 * know our desired clock rate.
	 */
	if (isp->isp_mdvec->dv_clock == 0) {
		mbs.param[0] = MBOX_GET_CLOCK_RATE;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			sdp->isp_clock = mbs.param[1];
			PRINTF("%s: using board clock 0x%x\n",
				isp->isp_name, sdp->isp_clock);
		}
	} else {
		sdp->isp_clock = isp->isp_mdvec->dv_clock;
	}

	mbs.param[0] = MBOX_GET_ACT_NEG_STATE;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		IDPRINTF(2, ("could not GET ACT NEG STATE\n"));
		sdp->isp_req_ack_active_neg = 1;
		sdp->isp_data_line_active_neg = 1;
	} else {
		sdp->isp_req_ack_active_neg = (mbs.param[1] >> 4) & 0x1;
		sdp->isp_data_line_active_neg = (mbs.param[1] >> 5) & 0x1;
	}
	for (i = 0; i < MAX_TARGETS; i++) {
		mbs.param[0] = MBOX_GET_TARGET_PARAMS;
		mbs.param[1] = i << 8;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			IDPRINTF(2, ("cannot get params for target %d\n", i));
			sdp->isp_devparam[i].sync_period =
				ISP_10M_SYNCPARMS & 0xff;
			sdp->isp_devparam[i].sync_offset =
				ISP_10M_SYNCPARMS >> 8;
			sdp->isp_devparam[i].dev_flags = DPARM_DEFAULT;
		} else {
			IDPRINTF(3, ("\%s: target %d - flags 0x%x, sync %x\n",
			       isp->isp_name, i, mbs.param[2], mbs.param[3]));
			sdp->isp_devparam[i].dev_flags = mbs.param[2] >> 8;
			/*
			 * The maximum period we can really see
			 * here is 100 (decimal), or 400 ns.
			 * For some unknown reason we sometimes
			 * get back wildass numbers from the
			 * boot device's paramaters.
			 */
			if ((mbs.param[3] & 0xff) <= 0x64) {
				sdp->isp_devparam[i].sync_period = 
					mbs.param[3] & 0xff;
				sdp->isp_devparam[i].sync_offset =
					mbs.param[3] >> 8; 
			}
		}
	}

	/*
	 * Set Default Host Adapter Parameters
	 * XXX: Should try and get them out of NVRAM
	 */
	sdp->isp_adapter_enabled = 1;
	sdp->isp_cmd_dma_burst_enable = 1;
	sdp->isp_data_dma_burst_enabl = 1;
	sdp->isp_fifo_threshold = 2;
	sdp->isp_initiator_id = 7;
	sdp->isp_async_data_setup = 6;
	sdp->isp_selection_timeout = 250;
	sdp->isp_max_queue_depth = 128;
	sdp->isp_tag_aging = 8;
	sdp->isp_bus_reset_delay = 3;
	sdp->isp_retry_count = 0;
	sdp->isp_retry_delay = 1;

	for (i = 0; i < MAX_TARGETS; i++) {
		sdp->isp_devparam[i].exc_throttle = 16;
		sdp->isp_devparam[i].dev_enable = 1;
	}
}

static void
isp_phoenix(struct ispsoftc *isp)
{
	ISP_SCSI_XFER_T *tlist[MAXISPREQUEST], *xs;
	int i;

	for (i = 0; i < RQUEST_QUEUE_LEN(isp); i++) {
		tlist[i] = (ISP_SCSI_XFER_T *) isp->isp_xflist[i];
	}
	isp_reset(isp);
	isp_init(isp);
	isp->isp_state = ISP_RUNSTATE;

	for (i = 0; i < RQUEST_QUEUE_LEN(isp); i++) {
		xs = tlist[i];
		if (XS_NULL(xs))
			continue;
		isp->isp_nactive--;
		if (isp->isp_nactive < 0)
			isp->isp_nactive = 0;
		XS_RESID(xs) = XS_XFRLEN(xs);
		XS_SETERR(xs, HBA_BOTCH);
		XS_CMD_DONE(xs);
	}
}

void
isp_watch(void *arg)
{
	int i;
	struct ispsoftc *isp = arg;
	ISP_SCSI_XFER_T *xs;
	ISP_LOCKVAL_DECL;

	/*
	 * Look for completely dead commands (but not polled ones).
	 */
	ISP_LOCK;
	for (i = 0; i < RQUEST_QUEUE_LEN(isp); i++) {
		if ((xs = (ISP_SCSI_XFER_T *) isp->isp_xflist[i]) == NULL) {
			continue;
		}
		if (XS_POLLDCMD(xs))
			continue;
		if (XS_TIME(xs) == 0) {
			continue;
		}
		XS_TIME(xs) -= (WATCH_INTERVAL * 1000);
		/*
		 * Avoid later thinking that this
		 * transaction is not being timed.
		 * Then give ourselves to watchdog
		 * periods of grace.
		 */
		if (XS_TIME(xs) == 0)
			XS_TIME(xs) = 1;
		else if (XS_TIME(xs) > -(2 * WATCH_INTERVAL * 1000)) {
			continue;
		}
		if (isp_abortcmd(isp, i)) {
			PRINTF("%s: isp_watch failed to abort command\n",
			       isp->isp_name);
			isp_phoenix(isp);
			break;
		}
	}
	ISP_UNLOCK;
	RESTART_WATCHDOG(isp_watch, isp);
}

static void
isp_prtstst(ispstatusreq_t *sp)
{
	PRINTF("states->");
	if (sp->req_state_flags & RQSF_GOT_BUS)
		PRINTF("GOT_BUS ");
	if (sp->req_state_flags & RQSF_GOT_TARGET)
		PRINTF("GOT_TGT ");
	if (sp->req_state_flags & RQSF_SENT_CDB)
		PRINTF("SENT_CDB ");
	if (sp->req_state_flags & RQSF_XFRD_DATA)
		PRINTF("XFRD_DATA ");
	if (sp->req_state_flags & RQSF_GOT_STATUS)
		PRINTF("GOT_STS ");
	if (sp->req_state_flags & RQSF_GOT_SENSE)
		PRINTF("GOT_SNS ");
	if (sp->req_state_flags & RQSF_XFER_COMPLETE)
		PRINTF("XFR_CMPLT ");
	PRINTF("\n");
	PRINTF("status->");
	if (sp->req_status_flags & RQSTF_DISCONNECT)
		PRINTF("Disconnect ");
	if (sp->req_status_flags & RQSTF_SYNCHRONOUS)
		PRINTF("Sync_xfr ");
	if (sp->req_status_flags & RQSTF_PARITY_ERROR)
		PRINTF("Parity ");
	if (sp->req_status_flags & RQSTF_BUS_RESET)
		PRINTF("Bus_Reset ");
	if (sp->req_status_flags & RQSTF_DEVICE_RESET)
		PRINTF("Device_Reset ");
	if (sp->req_status_flags & RQSTF_ABORTED)
		PRINTF("Aborted ");
	if (sp->req_status_flags & RQSTF_TIMEOUT)
		PRINTF("Timeout ");
	if (sp->req_status_flags & RQSTF_NEGOTIATION)
		PRINTF("Negotiation ");
	PRINTF("\n");
}
