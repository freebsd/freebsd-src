/* $FreeBSD$ */
/*
 * Machine and OS Independent (well, as best as possible)
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
 * Local static data
 */
#if	defined(ISP2100_TARGET_MODE) || defined(ISP_TARGET_MODE)
static const char tgtiqd[36] = {
	0x03, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
	0x51, 0x4C, 0x4F, 0x47, 0x49, 0x43, 0x20, 0x20,
#ifdef	__NetBSD__
	0x4E, 0x45, 0x54, 0x42, 0x53, 0x44, 0x20, 0x20,
#else
# ifdef	__FreeBSD__
	0x46, 0x52, 0x45, 0x45, 0x42, 0x52, 0x44, 0x20,
# else
#  ifdef linux
	0x4C, 0x49, 0x4E, 0x55, 0x58, 0x20, 0x20, 0x20,
#  else
#  endif
# endif
#endif
	0x54, 0x41, 0x52, 0x47, 0x45, 0x54, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x31
};
#endif


/*
 * Local function prototypes.
 */
static int isp_parse_async __P((struct ispsoftc *, u_int16_t));
static int isp_handle_other_response
__P((struct ispsoftc *, ispstatusreq_t *, u_int8_t *));
#if	defined(ISP2100_TARGET_MODE) || defined(ISP_TARGET_MODE)
static int isp_modify_lun __P((struct ispsoftc *, int, int, int));
#endif
static void isp_parse_status
__P((struct ispsoftc *, ispstatusreq_t *, ISP_SCSI_XFER_T *));
static void isp_fibre_init __P((struct ispsoftc *));
static void isp_fw_state __P((struct ispsoftc *));
static void isp_dumpregs __P((struct ispsoftc *, const char *));
static void isp_dumpxflist __P((struct ispsoftc *));
static void isp_prtstst __P((ispstatusreq_t *));
static void isp_mboxcmd __P((struct ispsoftc *, mbreg_t *));

static void isp_update  __P((struct ispsoftc *));
static void isp_setdfltparm __P((struct ispsoftc *));
static int isp_read_nvram __P((struct ispsoftc *));
static void isp_rdnvram_word __P((struct ispsoftc *, int, u_int16_t *));

/*
 * Reset Hardware.
 *
 * Hit the chip over the head, download new f/w.
 *
 * Locking done elsewhere.
 */
void
isp_reset(isp)
	struct ispsoftc *isp;
{
	static char once = 1;
	mbreg_t mbs;
	int loops, i, dodnld = 1;
	char *revname;

	isp->isp_state = ISP_NILSTATE;

	/*
	 * Basic types (SCSI, FibreChannel and PCI or SBus)
	 * have been set in the MD code. We figure out more
	 * here.
	 */
	isp->isp_dblev = DFLT_DBLEVEL;
	if (isp->isp_type & ISP_HA_FC) {
		revname = "2100";
	} else {
		sdparam *sdp = isp->isp_param;

		int rev = ISP_READ(isp, BIU_CONF0) & BIU_CONF0_HW_MASK;
		switch (rev) {
		default:
			PRINTF("%s: unknown chip rev. 0x%x- assuming a 1020\n",
			    isp->isp_name, rev);
			/* FALLTHROUGH */
		case 1:
			revname = "1020";	
			isp->isp_type = ISP_HA_SCSI_1020;
			sdp->isp_clock = 40;
			break;
		case 2:
			/*
			 * Some 1020A chips are Ultra Capable, but don't
			 * run the clock rate up for that unless told to
			 * do so by the Ultra Capable bits being set.
			 */
			revname = "1020A";	
			isp->isp_type = ISP_HA_SCSI_1020A;
			sdp->isp_clock = 40;
			break;
		case 3:
			revname = "1040";
			isp->isp_type = ISP_HA_SCSI_1040;
			sdp->isp_clock = 60;
			break;
		case 4:
			revname = "1040A";
			isp->isp_type = ISP_HA_SCSI_1040A;
			sdp->isp_clock = 60;
			break;
		case 5:
			revname = "1040B";
			isp->isp_type = ISP_HA_SCSI_1040B;
			sdp->isp_clock = 60;
			break;
		}
		/*
		 * Try and figure out if we're connected to a differential bus.
		 * You have to pause the RISC processor to read SXP registers.
		 */
		ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);
		i = 100;
		while ((ISP_READ(isp, HCCR) & HCCR_PAUSE) == 0) {
			SYS_DELAY(20);
			if (--i == 0) {
				PRINTF("%s: unable to pause RISC processor\n",
				    isp->isp_name);
				i = -1;
				break;
			}
		}
		if (i > 0) {
			if (isp->isp_bustype != ISP_BT_SBUS) {
				ISP_SETBITS(isp, BIU_CONF1, BIU_PCI_CONF1_SXP);
			}
			if (ISP_READ(isp, SXP_PINS_DIFF) & SXP_PINS_DIFF_MODE) {
				IDPRINTF(2, ("%s: Differential Mode Set\n",
				    isp->isp_name));
				sdp->isp_diffmode = 1;
			} else {
				sdp->isp_diffmode = 0;
			}

			if (isp->isp_bustype != ISP_BT_SBUS) {
				ISP_CLRBITS(isp, BIU_CONF1, BIU_PCI_CONF1_SXP);
			}

			/*
			 * Figure out whether we're ultra capable.
			 */
			i = ISP_READ(isp, RISC_PSR);
			if (isp->isp_bustype != ISP_BT_SBUS) {
				i &= RISC_PSR_PCI_ULTRA;
			} else {
				i &= RISC_PSR_SBUS_ULTRA;
			}
			if (i) {
				IDPRINTF(2, ("%s: Ultra Mode Capable\n",
				    isp->isp_name));
				sdp->isp_clock = 60;
			} else {
				sdp->isp_clock = 40;
			}
			/*
			 * Restart processor
			 */
			ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE);
		}
		/*
		 * Machine dependent clock (if set) overrides
		 * our generic determinations.
		 */
		if (isp->isp_mdvec->dv_clock) {
			if (isp->isp_mdvec->dv_clock < sdp->isp_clock) {
				sdp->isp_clock = isp->isp_mdvec->dv_clock;
			}
		}
	}

	/*
	 * Do MD specific pre initialization
	 */
	ISP_RESET0(isp);

	if (once == 1) {
		once = 0;
		/*
		 * Get the current running firmware revision out of the
		 * chip before we hit it over the head (if this is our
		 * first time through). Note that we store this as the
		 * 'ROM' firmware revision- which it may not be. In any
		 * case, we don't really use this yet, but we may in
		 * the future.
		 */
		mbs.param[0] = MBOX_ABOUT_FIRMWARE;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			IDPRINTF(3, ("%s: initial ABOUT FIRMWARE command "
			    "failed\n", isp->isp_name));
		} else {
			isp->isp_romfw_rev =
			    (((u_int16_t) mbs.param[1]) << 10) + mbs.param[2];
		}
	}

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
		/*
		 * All 2100's are 60Mhz with fast rams onboard.
		 */
		ISP_WRITE(isp, RISC_MTR2100, 0x1212);
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
			return;
		}
		if (mbs.param[1] != 0xdead || mbs.param[2] != 0xbeef ||
		    mbs.param[3] != 0xffff || mbs.param[4] != 0x1111 ||
		    mbs.param[5] != 0xa5a5) {
			isp_dumpregs(isp, "Register Test Failed");
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
		sdparam *sdp = isp->isp_param;
		/*
		 * Set CLOCK RATE, but only if asked to.
		 */
		if (sdp->isp_clock) {
			mbs.param[0] = MBOX_SET_CLOCK_RATE;
			mbs.param[1] = sdp->isp_clock;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				isp_dumpregs(isp, "failed to set CLOCKRATE");
				/* but continue */
			} else {
				IDPRINTF(3, ("%s: setting input clock to %d\n",
				    isp->isp_name, sdp->isp_clock));
			}
		}
	}
	mbs.param[0] = MBOX_ABOUT_FIRMWARE;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "ABOUT FIRMWARE command failed");
		return;
	}
	PRINTF("%s: Board Revision %s, %s F/W Revision %d.%d\n",
		isp->isp_name, revname, dodnld? "loaded" : "resident",
		mbs.param[1], mbs.param[2]);
	isp->isp_fwrev = (((u_int16_t) mbs.param[1]) << 10) + mbs.param[2];
	if (isp->isp_romfw_rev && dodnld) {
		PRINTF("%s: Last F/W revision was %d.%d\n", isp->isp_name,
		    isp->isp_romfw_rev >> 10, isp->isp_romfw_rev & 0x3ff);
	}
	isp_fw_state(isp);
	isp->isp_state = ISP_RESETSTATE;
}

/*
 * Initialize Hardware to known state
 *
 * Locks are held before coming here.
 */

void
isp_init(isp)
	struct ispsoftc *isp;
{
	sdparam *sdp;
	mbreg_t mbs;
	int tgt;

	/*
	 * Must do first.
	 */
	isp_setdfltparm(isp);

	/*
	 * If we're fibre, we have a completely different
	 * initialization method.
	 */

	if (isp->isp_type & ISP_HA_FC) {
		isp_fibre_init(isp);
		return;
	}
	sdp = isp->isp_param;

	/*
	 * Set (possibly new) Initiator ID.
	 */
	mbs.param[0] = MBOX_SET_INIT_SCSI_ID;
	mbs.param[1] = sdp->isp_initiator_id;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "failed to set initiator id");
		return;
	}

	/*
	 * Set Retry Delay and Count
	 */
	mbs.param[0] = MBOX_SET_RETRY_COUNT;
	mbs.param[1] = sdp->isp_retry_count;
	mbs.param[2] = sdp->isp_retry_delay;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "failed to set retry count and delay");
		return;
	}

	/*
	 * Set ASYNC DATA SETUP time. This is very important.
	 */
	mbs.param[0] = MBOX_SET_ASYNC_DATA_SETUP_TIME;
	mbs.param[1] = sdp->isp_async_data_setup;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "failed to set async data setup time");
		return;
	}

	/*
	 * Set ACTIVE Negation State.
	 */
	mbs.param[0] = MBOX_SET_ACTIVE_NEG_STATE;
	mbs.param[1] =
	    (sdp->isp_req_ack_active_neg << 4) |
	    (sdp->isp_data_line_active_neg << 5);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "failed to set active neg state");
		return;
	}

	/*
	 * Set the Tag Aging limit
	 */

	mbs.param[0] = MBOX_SET_TAG_AGE_LIMIT;
	mbs.param[1] = sdp->isp_tag_aging;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "failed to set tag age limit");
		return;
	}

	/*
	 * Set selection timeout.
	 */

	mbs.param[0] = MBOX_SET_SELECT_TIMEOUT;
	mbs.param[1] = sdp->isp_selection_timeout;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "failed to set selection timeout");
		return;
	}

	/*
	 * Set per-target parameters to a safe minimum.
	 */

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		int maxlun, lun;

		if (sdp->isp_devparam[tgt].dev_enable == 0)
			continue;

		mbs.param[0] = MBOX_SET_TARGET_PARAMS;
		mbs.param[1] = tgt << 8;
		mbs.param[2] = DPARM_SAFE_DFLT;
		mbs.param[3] = 0;
		/*
		 * It is not quite clear when this changed over so that
		 * we could force narrow and async, so assume >= 7.55.
		 *
		 * Otherwise, a SCSI bus reset issued below will force
		 * the back to the narrow, async state (but see note
		 * below also). Technically we should also do without
		 * Parity.
		 */
		if (isp->isp_fwrev >= ISP_FW_REV(7, 55)) {
			mbs.param[2] |= DPARM_NARROW | DPARM_ASYNC;
		}
		sdp->isp_devparam[tgt].cur_dflags = mbs.param[2] >> 8;

		IDPRINTF(3, ("\n%s: tgt %d cflags %x offset %x period %x\n",
		    isp->isp_name, tgt, mbs.param[2], mbs.param[3] >> 8,
		    mbs.param[3] & 0xff));
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {

			PRINTF("%s: failed to set parameters for tgt %d\n",
				isp->isp_name, tgt);

			PRINTF("%s: flags %x offset %x period %x\n",
				isp->isp_name, sdp->isp_devparam[tgt].dev_flags,
				sdp->isp_devparam[tgt].sync_offset,
				sdp->isp_devparam[tgt].sync_period);

			mbs.param[0] = MBOX_SET_TARGET_PARAMS;
			mbs.param[1] = tgt << 8;
			mbs.param[2] = DPARM_SAFE_DFLT;
			mbs.param[3] = 0;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				PRINTF("%s: failed even to set defaults for "
				    "target %d\n", isp->isp_name, tgt);
				continue;
			}
		}

		maxlun = (isp->isp_fwrev >= ISP_FW_REV(7, 55))? 32 : 8;
		for (lun = 0; lun < maxlun; lun++) {
			mbs.param[0] = MBOX_SET_DEV_QUEUE_PARAMS;
			mbs.param[1] = (tgt << 8) | lun;
			mbs.param[2] = sdp->isp_max_queue_depth;
			mbs.param[3] = sdp->isp_devparam[tgt].exc_throttle;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				PRINTF("%s: failed to set device queue "
				    "parameters for target %d, lun %d\n",
				    isp->isp_name, tgt, lun);
				break;
			}
		}
	}

	/*
	 * Set up DMA for the request and result mailboxes.
	 */
	if (ISP_MBOXDMASETUP(isp) != 0) {
		PRINTF("%s: can't setup dma mailboxes\n", isp->isp_name);
		return;
	}
		
	mbs.param[0] = MBOX_INIT_RES_QUEUE;
	mbs.param[1] = RESULT_QUEUE_LEN;
	mbs.param[2] = (u_int16_t) (isp->isp_result_dma >> 16);
	mbs.param[3] = (u_int16_t) (isp->isp_result_dma & 0xffff);
	mbs.param[4] = 0;
	mbs.param[5] = 0;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "set of response queue failed");
		return;
	}
	isp->isp_residx = 0;

	mbs.param[0] = MBOX_INIT_REQ_QUEUE;
	mbs.param[1] = RQUEST_QUEUE_LEN;
	mbs.param[2] = (u_int16_t) (isp->isp_rquest_dma >> 16);
	mbs.param[3] = (u_int16_t) (isp->isp_rquest_dma & 0xffff);
	mbs.param[4] = 0;
	mbs.param[5] = 0;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "set of request queue failed");
		return;
	}
	isp->isp_reqidx = isp->isp_reqodx = 0;

	/*	
	 * XXX: See whether or not for 7.55 F/W or later we
	 * XXX: can do without this, and see whether we should
	 * XXX: honor the NVRAM SCSI_RESET_DISABLE token.
	 */
	mbs.param[0] = MBOX_BUS_RESET;
	mbs.param[1] = 3;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "SCSI bus reset failed");
	}
	/*
	 * This is really important to have set after a bus reset.
	 */
	isp->isp_sendmarker = 1;
	isp->isp_state = ISP_INITSTATE;
}

/*
 * Fibre Channel specific initialization.
 *
 * Locks are held before coming here.
 */
static void
isp_fibre_init(isp)
	struct ispsoftc *isp;
{
	fcparam *fcp;
	isp_icb_t *icbp;
	mbreg_t mbs;
	int count;
	u_int8_t lwfs;

	fcp = isp->isp_param;

	if (ISP_MBOXDMASETUP(isp) != 0) {
		PRINTF("%s: can't setup DMA for mailboxes\n", isp->isp_name);
		return;
	}

	icbp = (isp_icb_t *) fcp->isp_scratch;
	bzero(icbp, sizeof (*icbp));

	icbp->icb_version = ICB_VERSION1;

	fcp->isp_fwoptions = 0;
#ifdef	ISP2100_TARGET_MODE
	fcp->isp_fwoptions |= ICBOPT_TGT_ENABLE	| ICBOPT_INI_TGTTYPE;
	icbp->icb_iqdevtype = 0x23;	/* DPQ_SUPPORTED/PROCESSOR */
#endif
	icbp->icb_fwoptions = fcp->isp_fwoptions;
	icbp->icb_maxfrmlen = fcp->isp_maxfrmlen;
	if (icbp->icb_maxfrmlen < ICB_MIN_FRMLEN ||
	    icbp->icb_maxfrmlen > ICB_MAX_FRMLEN) {
		PRINTF("%s: bad frame length (%d) from NVRAM- using %d\n",
		    isp->isp_name, fcp->isp_maxfrmlen, ICB_DFLT_FRMLEN);
	}
	icbp->icb_maxalloc = fcp->isp_maxalloc;
	icbp->icb_execthrottle = fcp->isp_execthrottle;
	icbp->icb_retry_delay = fcp->isp_retry_delay;
	icbp->icb_retry_count = fcp->isp_retry_count;

	MAKE_NODE_NAME_FROM_WWN(icbp->icb_nodename, fcp->isp_wwn);

	icbp->icb_rqstqlen = RQUEST_QUEUE_LEN;
	icbp->icb_rsltqlen = RESULT_QUEUE_LEN;
	icbp->icb_rqstaddr[RQRSP_ADDR0015] =
	    (u_int16_t) (isp->isp_rquest_dma & 0xffff);
	icbp->icb_rqstaddr[RQRSP_ADDR1631] =
	    (u_int16_t) (isp->isp_rquest_dma >> 16);
	icbp->icb_respaddr[RQRSP_ADDR0015] =
	    (u_int16_t) (isp->isp_result_dma & 0xffff);
	icbp->icb_respaddr[RQRSP_ADDR1631] =
	    (u_int16_t) (isp->isp_result_dma >> 16);

	for (count = 0; count < 10; count++) {
		mbs.param[0] = MBOX_INIT_FIRMWARE;
		mbs.param[1] = 0;
		mbs.param[2] = (u_int16_t) (fcp->isp_scdma >> 16);
		mbs.param[3] = (u_int16_t) (fcp->isp_scdma & 0xffff);
		mbs.param[4] = 0;
		mbs.param[5] = 0;
		mbs.param[6] = 0;
		mbs.param[7] = 0;

		isp_mboxcmd(isp, &mbs);

		switch (mbs.param[0]) {
		case MBOX_COMMAND_COMPLETE:
			count = 10;
			break;
		case ASYNC_LIP_OCCURRED:
		case ASYNC_LOOP_UP:
		case ASYNC_LOOP_DOWN:
		case ASYNC_LOOP_RESET:
		case ASYNC_PDB_CHANGED:
		case ASYNC_CHANGE_NOTIFY:
			if (count > 9) {
				PRINTF("%s: too many retries to get going- "
				    "giving up\n", isp->isp_name);
				return;
			}
			break;
		default:
			isp_dumpregs(isp, "INIT FIRMWARE failed");
			return;
		}
	}
	isp->isp_reqidx = isp->isp_reqodx = 0;
	isp->isp_residx = 0;

	/*
	 * Wait up to 12 seconds for FW to go to READY state.
	 * This used to be 3 seconds, but that lost.
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
	for (count = 0; count < 12000; count++) {
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
			return;
		}
		fcp->isp_loopid = mbs.param[1];
		fcp->isp_alpa = mbs.param[2];
		PRINTF("%s: Loop ID 0x%x, ALPA 0x%x\n", isp->isp_name,
		    fcp->isp_loopid, fcp->isp_alpa);
		isp->isp_state = ISP_INITSTATE;
#if	defined(ISP2100_TARGET_MODE) || defined(ISP_TARGET_MODE)
		DISABLE_INTS(isp);
		if (isp->isp_fwrev >= ISP_FW_REV(1, 13)) {
			if (isp_modify_lun(isp, 0, 1, 1)) {
				PRINTF("%s: failed to establish target mode\n",
				    isp->isp_name);
			}
		}
		ENABLE_INTS(isp);
#endif
	} else {
		PRINTF("%s: failed to go to FW READY state- will not attach\n",
		    isp->isp_name);
	}
}

/*
 * Free any associated resources prior to decommissioning and
 * set the card to a known state (so it doesn't wake up and kick
 * us when we aren't expecting it to).
 *
 * Locks are held before coming here.
 */
void
isp_uninit(isp)
	struct ispsoftc *isp;
{
	/*
	 * Leave with interrupts disabled.
	 */
	DISABLE_INTS(isp);

	/*
	 * Stop the watchdog timer (if started).
	 */
	STOP_WATCHDOG(isp_watch, isp);
}


/*
 * Start a command. Locking is assumed done in the caller.
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

	XS_INITERR(xs);
	isp = XS_ISP(xs);

	if (isp->isp_state != ISP_RUNSTATE) {
		PRINTF("%s: adapter not ready\n", isp->isp_name);
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}

	/*
	 * We *could* do the different sequence type that has clos
	 * to the whole Queue Entry for the command,.
	 */
	if (XS_CDBLEN(xs) > ((isp->isp_type & ISP_HA_FC)? 16 : 12)) {
		PRINTF("%s: unsupported cdb length (%d)\n",
		    isp->isp_name, XS_CDBLEN(xs));
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}

	/*
	 * First check to see if any HBA or Device
	 * parameters need to be updated.
	 */
	if (isp->isp_update) {
		isp_update(isp);
	}

	optr = isp->isp_reqodx = ISP_READ(isp, OUTMAILBOX4);
	iptr = isp->isp_reqidx;

	reqp = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
	iptr = ISP_NXT_QENTRY(iptr, RQUEST_QUEUE_LEN);
	if (iptr == optr) {
		IDPRINTF(2, ("%s: Request Queue Overflow\n", isp->isp_name));
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	}
	if (isp->isp_type & ISP_HA_FC) {
		DISABLE_INTS(isp);
	}

	if (isp->isp_sendmarker) {
		u_int8_t niptr;
		ispmarkreq_t *marker = (ispmarkreq_t *) reqp;

		bzero((void *) marker, sizeof (*marker));
		marker->req_header.rqs_entry_count = 1;
		marker->req_header.rqs_entry_type = RQSTYPE_MARKER;
		marker->req_modifier = SYNC_ALL;

		isp->isp_sendmarker = 0;

		/*
		 * Unconditionally update the input pointer anyway.
		 */
		ISP_WRITE(isp, INMAILBOX4, iptr);
		isp->isp_reqidx = iptr;

		niptr = ISP_NXT_QENTRY(iptr, RQUEST_QUEUE_LEN);
		if (niptr == optr) {
			if (isp->isp_type & ISP_HA_FC) {
				ENABLE_INTS(isp);
			}
			IDPRINTF(2, ("%s: Request Queue Overflow+\n",
			    isp->isp_name));
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_EAGAIN);
		}
		reqp = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
		iptr = niptr;
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

	for (i = 0; i < RQUEST_QUEUE_LEN; i++) {
		if (isp->isp_xflist[i] == NULL)
			break;
	}
	if (i == RQUEST_QUEUE_LEN) {
		if (isp->isp_type & ISP_HA_FC)
			ENABLE_INTS(isp);
		IDPRINTF(2, ("%s: out of xflist pointers\n", isp->isp_name));
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
		 * If we're marked as "Can't Tag", just do simple
		 * instead of ordered tags. It's pretty clear to me
		 * that we shouldn't do head of queue tagging in
		 * this case.
		 */
		if (XS_CANTAG(xs)) {
			t2reqp->req_flags = XS_KINDOF_TAG(xs);
		} else {
 			t2reqp->req_flags = REQFLAG_STAG; 
		}
	} else {
		sdparam *sdp = (sdparam *)isp->isp_param;
		if ((sdp->isp_devparam[XS_TGT(xs)].cur_dflags & DPARM_TQING) &&
		    XS_CANTAG(xs)) {
			reqp->req_flags = XS_KINDOF_TAG(xs);
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

	IDPRINTF(5, ("%s(%d.%d): START%d cmd 0x%x datalen %d\n", isp->isp_name,
	    XS_TGT(xs), XS_LUN(xs), reqp->req_header.rqs_seqno,
	    reqp->req_cdb[0], XS_XFRLEN(xs)));

	reqp->req_time = XS_TIME(xs) / 1000;
	if (reqp->req_time == 0 && XS_TIME(xs))
		reqp->req_time = 1;
	i = ISP_DMASETUP(isp, xs, reqp, &iptr, optr);
	if (i != CMD_QUEUED) {
		if (isp->isp_type & ISP_HA_FC)
			ENABLE_INTS(isp);
		/*
		 * dmasetup sets actual error in packet, and
		 * return what we were given to return.
		 */
		return (i);
	}
	XS_SETERR(xs, HBA_NOERROR);
	ISP_WRITE(isp, INMAILBOX4, iptr);
	isp->isp_reqidx = iptr;
	if (isp->isp_type & ISP_HA_FC) {
		ENABLE_INTS(isp);
	}
	isp->isp_nactive++;
	return (CMD_QUEUED);
#undef	reqp
#undef	t2reqp
}

/*
 * isp control
 * Locks (ints blocked) assumed held.
 */

int
isp_control(isp, ctl, arg)
	struct ispsoftc *isp;
	ispctl_t ctl;
	void *arg;
{
	ISP_SCSI_XFER_T *xs;
	mbreg_t mbs;
	int i;

	switch (ctl) {
	default:
		PRINTF("%s: isp_control unknown control op %x\n",
		    isp->isp_name, ctl);
		break;

	case ISPCTL_RESET_BUS:
		mbs.param[0] = MBOX_BUS_RESET;
		mbs.param[1] = (isp->isp_type & ISP_HA_FC)? 5: 2;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_dumpregs(isp, "isp_control SCSI bus reset failed");
			break;
		}
		/*
		 * This is really important to have set after a bus reset.
		 */
		isp->isp_sendmarker = 1;
		PRINTF("%s: driver initiated bus reset\n", isp->isp_name);
		return (0);

        case ISPCTL_RESET_DEV:
		/*
		 * Note that under parallel SCSI, this issues a BDR message.
		 * Under FC, we could probably be using ABORT TASK SET
		 * command.
		 */

		mbs.param[0] = MBOX_ABORT_TARGET;
		mbs.param[1] = ((long)arg) << 8;
		mbs.param[2] = 2;	/* 'delay', in seconds */
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_dumpregs(isp, "SCSI Target  reset failed");
			break;
		}
		PRINTF("%s: Target %d Reset Succeeded\n", isp->isp_name,
		    (int) ((long) arg));
		isp->isp_sendmarker = 1;
		return (0);

        case ISPCTL_ABORT_CMD:
		xs = (ISP_SCSI_XFER_T *) arg;
		for (i = 0; i < RQUEST_QUEUE_LEN; i++) {
			if (xs == isp->isp_xflist[i]) {
				break;
			}
		}
		if (i == RQUEST_QUEUE_LEN) {
			PRINTF("%s: isp_control- cannot find command to abort "
			    "in active list\n", isp->isp_name);
			break;
		}
		mbs.param[0] = MBOX_ABORT;
		mbs.param[1] = XS_TGT(xs) | XS_LUN(xs);
		mbs.param[2] = (i+1) >> 16;
		mbs.param[3] = (i+1) & 0xffff;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			PRINTF("%s: isp_control MBOX_ABORT failure (code %x)\n",
			    isp->isp_name, mbs.param[0]);
			break;
		}
		PRINTF("%s: command for target %d lun %d was aborted\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		return (0);

	case ISPCTL_UPDATE_PARAMS:
		isp_update(isp);
		return(0);
	}
	return (-1);
}

/*
 * Interrupt Service Routine(s).
 *
 * External (OS) framework has done the appropriate locking,
 * and the locking will be held throughout this function.
 */

int
isp_intr(arg)
	void *arg;
{
	ISP_SCSI_XFER_T *complist[RESULT_QUEUE_LEN], *xs;
	struct ispsoftc *isp = arg;
	u_int8_t iptr, optr;
	u_int16_t isr;
	int i, ndone = 0;

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

	if (ISP_READ(isp, BIU_SEMA) & 1) {
		u_int16_t mbox = ISP_READ(isp, OUTMAILBOX0);
		if (isp_parse_async(isp, mbox))
			return (1);
		ISP_WRITE(isp, BIU_SEMA, 0);
	}

	ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);

	optr = isp->isp_residx;
	iptr = ISP_READ(isp, OUTMAILBOX5);

	if (optr == iptr) {
		IDPRINTF(4, ("why intr? isr %x iptr %x optr %x\n",
		    isr, optr, iptr));
	}
	ENABLE_INTS(isp);

	while (optr != iptr) {
		ispstatusreq_t *sp;
		u_int8_t oop;
		int buddaboom = 0;

		sp = (ispstatusreq_t *) ISP_QUEUE_ENTRY(isp->isp_result, optr);
		oop = optr;
		optr = ISP_NXT_QENTRY(optr, RESULT_QUEUE_LEN);

		if (sp->req_header.rqs_entry_type != RQSTYPE_RESPONSE) {
			if (isp_handle_other_response(isp, sp, &optr) == 0) {
				ISP_WRITE(isp, INMAILBOX5, optr);
				continue;
			}
			/*
			 * It really has to be a bounced request just copied
			 * from the request queue to the response queue.
			 */

			if (sp->req_header.rqs_entry_type != RQSTYPE_REQUEST) {
				ISP_WRITE(isp, INMAILBOX5, optr);
				continue;
			}
			PRINTF("%s: not RESPONSE in RESPONSE Queue "
			    "(type 0x%x) @ idx %d (next %d)\n", isp->isp_name,
			    sp->req_header.rqs_entry_type, oop, optr);
			buddaboom = 1;
		}

		if (sp->req_header.rqs_flags & 0xf) {
			if (sp->req_header.rqs_flags & RQSFLAG_CONTINUATION) {
				ISP_WRITE(isp, INMAILBOX5, optr);
				continue;
			}
			PRINTF("%s: rqs_flags=%x", isp->isp_name,
				sp->req_header.rqs_flags & 0xf);
			if (sp->req_header.rqs_flags & RQSFLAG_FULL) {
				PRINTF("%s: internal queues full\n",
				    isp->isp_name);
				/* XXXX: this command *could* get restarted */
				buddaboom++;
			}
			if (sp->req_header.rqs_flags & RQSFLAG_BADHEADER) {
				PRINTF("%s: bad header\n", isp->isp_name);
				buddaboom++;
			}
			if (sp->req_header.rqs_flags & RQSFLAG_BADPACKET) {
				PRINTF("%s: bad request packet\n",
				    isp->isp_name);
				buddaboom++;
			}
		}
		if (sp->req_handle > RQUEST_QUEUE_LEN || sp->req_handle < 1) {
			PRINTF("%s: bad request handle %d\n", isp->isp_name,
				sp->req_handle);
			ISP_WRITE(isp, INMAILBOX5, optr);
			continue;
		}
		xs = (ISP_SCSI_XFER_T *) isp->isp_xflist[sp->req_handle - 1];
		if (xs == NULL) {
			PRINTF("%s: NULL xs in xflist (handle %x)\n",
			    isp->isp_name, sp->req_handle);
			isp_dumpxflist(isp);
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
			if (XS_NOERR(xs)) {
			    if (sp->req_completion_status != RQCS_COMPLETE) {
				isp_parse_status(isp, sp, xs);
			    } else {
				XS_SETERR(xs, HBA_NOERROR);
			    }
			}
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
		/*
		 * XXX: If we have a check condition, but no Sense Data,
		 * XXX: mark it as an error (ARQ failed). We need to
		 * XXX: to do a more distinct job because there may
		 * XXX: cases where ARQ is disabled.
		 */
		if (XS_STS(xs) == SCSI_CHECK && !(XS_IS_SNS_VALID(xs))) {
			if (XS_NOERR(xs)) {
				PRINTF("%s: ARQ Failure\n", isp->isp_name);
				XS_SETERR(xs, HBA_ARQFAIL);
			}
		}
		if ((isp->isp_dblev >= 5) ||
		    (isp->isp_dblev > 2 && !XS_NOERR(xs))) {
			PRINTF("%s(%d.%d): FIN%d dl%d resid%d STS %x",
			    isp->isp_name, XS_TGT(xs), XS_LUN(xs),
			    sp->req_header.rqs_seqno, XS_XFRLEN(xs),
			    XS_RESID(xs), XS_STS(xs));
			if (sp->req_state_flags & RQSF_GOT_SENSE) {
				PRINTF(" Skey: %x", XS_SNSKEY(xs));
				if (!(XS_IS_SNS_VALID(xs))) {
					PRINTF(" BUT NOT SET");
				}
			}
			PRINTF(" XS_ERR=0x%x\n", (unsigned int) XS_ERR(xs));
		}

		ISP_WRITE(isp, INMAILBOX5, optr);
		isp->isp_nactive--;
		if (isp->isp_nactive < 0)
			isp->isp_nactive = 0;
		complist[ndone++] = xs;	/* defer completion call until later */
	}
	/*
	 * If we completed any commands, then it's valid to find out
	 * what the outpointer is.
	 */
	if (ndone) {
	 	isp->isp_reqodx = ISP_READ(isp, OUTMAILBOX4);
	}
	isp->isp_residx = optr;
	for (i = 0; i < ndone; i++) {
		xs = complist[i];
		if (xs) {
			XS_CMD_DONE(xs);
		}
	}
	return (1);
}

/*
 * Support routines.
 */

static int
isp_parse_async(isp, mbox)
	struct ispsoftc *isp;
	u_int16_t mbox;
{
	switch (mbox) {
	case ASYNC_BUS_RESET:
		PRINTF("%s: SCSI bus reset detected\n", isp->isp_name);
		isp->isp_sendmarker = 1;
		break;

	case ASYNC_SYSTEM_ERROR:
		mbox = ISP_READ(isp, OUTMAILBOX1);
		PRINTF("%s: Internal FW Error @ RISC Addr 0x%x\n",
		    isp->isp_name, mbox);
		isp_restart(isp);
		/* no point continuing after this */
		return (1);

	case ASYNC_RQS_XFER_ERR:
		PRINTF("%s: Request Queue Transfer Error\n", isp->isp_name);
		break;

	case ASYNC_RSP_XFER_ERR:
		PRINTF("%s: Response Queue Transfer Error\n", isp->isp_name);
		break;

	case ASYNC_QWAKEUP:
		/* don't need to be chatty */
		mbox = ISP_READ(isp, OUTMAILBOX4);
		break;

	case ASYNC_TIMEOUT_RESET:
		PRINTF("%s: timeout initiated SCSI bus reset\n", isp->isp_name);
		isp->isp_sendmarker = 1;
		break;

	case ASYNC_UNSPEC_TMODE:
		PRINTF("%s: mystery async target completion\n", isp->isp_name);
		break;

	case ASYNC_EXTMSG_UNDERRUN:
		PRINTF("%s: extended message underrun\n", isp->isp_name);
		break;

	case ASYNC_SCAM_INT:
		PRINTF("%s: SCAM interrupt\n", isp->isp_name);
		break;

	case ASYNC_HUNG_SCSI:
		PRINTF("%s: stalled SCSI Bus after DATA Overrun\n",
		    isp->isp_name);
		/* XXX: Need to issue SCSI reset at this point */
		break;

	case ASYNC_KILLED_BUS:
		PRINTF("%s: SCSI Bus reset after DATA Overrun\n",
		    isp->isp_name);
		break;

	case ASYNC_BUS_TRANSIT:
		PRINTF("%s: LBD->HVD Transition 0x%x\n",
		    isp->isp_name, ISP_READ(isp, OUTMAILBOX1));
		break;

	case ASYNC_CMD_CMPLT:
		PRINTF("%s: fast post completion\n", isp->isp_name);
#if	0
		fast_post_handle = (ISP_READ(isp, OUTMAILBOX1) << 16) |
		    ISP_READ(isp, OUTMAILBOX2);
#endif
		break;

	case ASYNC_CTIO_DONE:
		PRINTF("%s: CTIO done\n", isp->isp_name);
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

	case ASYNC_PDB_CHANGED:
		PRINTF("%s: Port Database Changed\n", isp->isp_name);
		break;

	case ASYNC_CHANGE_NOTIFY:
		PRINTF("%s: Name Server Database Changed\n", isp->isp_name);
		break;

	default:
		PRINTF("%s: async %x\n", isp->isp_name, mbox);
		break;
	}
	return (0);
}

static int
isp_handle_other_response(isp, sp, optrp)
	struct ispsoftc *isp;
	ispstatusreq_t *sp;
	u_int8_t *optrp;
{
	u_int8_t iptr, optr;
	int reqsize = 0;
	void *ireqp = NULL;

	switch (sp->req_header.rqs_entry_type) {
	case RQSTYPE_REQUEST:
		return (-1);
#if	defined(ISP2100_TARGET_MODE) || defined(ISP_TARGET_MODE)
	case RQSTYPE_NOTIFY_ACK:
	{
		ispnotify_t *spx = (ispnotify_t *) sp;
		PRINTF("%s: Immediate Notify Ack %d.%d Status 0x%x Sequence "
		    "0x%x\n", isp->isp_name, spx->req_initiator, spx->req_lun,
		    spx->req_status, spx->req_sequence);
		break;
	}
	case RQSTYPE_NOTIFY:
	{
		ispnotify_t *spx = (ispnotify_t *) sp;

		PRINTF("%s: Notify loopid %d to lun %d req_status 0x%x "
		    "req_task_flags 0x%x seq 0x%x\n", isp->isp_name, 				    spx->req_initiator, spx->req_lun, spx->req_status, 
		    spx->req_task_flags, spx->req_sequence);
		reqsize = sizeof (*spx);
		spx->req_header.rqs_entry_type = RQSTYPE_NOTIFY_ACK;
		spx->req_header.rqs_entry_count = 1;
		spx->req_header.rqs_flags = 0;
		spx->req_header.rqs_seqno = isp->isp_seqno++;
		spx->req_handle = (spx->req_initiator<<16) | RQSTYPE_NOTIFY_ACK;
		if (spx->req_status == IN_RSRC_UNAVAIL)
			spx->req_flags = LUN_INCR_CMD;
		else if (spx->req_status == IN_NOCAP)
			spx->req_flags = LUN_INCR_IMMED;
		else {
			reqsize = 0;
		}
		ireqp = spx;
		break;
	}
	case RQSTYPE_ENABLE_LUN:
	{
		isplun_t *ip = (isplun_t *) sp;
		if (ip->req_status != 1) {
		    PRINTF("%s: ENABLE LUN returned status 0x%x\n",
			isp->isp_name, ip->req_status);
		}
		break;
	}
	case RQSTYPE_ATIO2:
	{
		fcparam *fcp = isp->isp_param;
		ispctiot2_t local, *ct2 = NULL;
		ispatiot2_t *at2 = (ispatiot2_t *) sp;
		int s;

		PRINTF("%s: atio2 loopid %d for lun %d rxid 0x%x flags 0x%x "
		    "task flags 0x%x exec codes 0x%x\n", isp->isp_name,
		    at2->req_initiator, at2->req_lun, at2->req_rxid,
		    at2->req_flags, at2->req_taskflags, at2->req_execodes);

		switch (at2->req_status & ~ATIO_SENSEVALID) {
		case ATIO_PATH_INVALID:
			PRINTF("%s: ATIO2 Path Invalid\n", isp->isp_name);
			break;
		case ATIO_NOCAP:
			PRINTF("%s: ATIO2 No Cap\n", isp->isp_name);
			break;
		case ATIO_BDR_MSG:
			PRINTF("%s: ATIO2 BDR Received\n", isp->isp_name);
			break;
		case ATIO_CDB_RECEIVED:
			ct2 = &local;
			break;
		default:
			PRINTF("%s: unknown req_status 0x%x\n", isp->isp_name,
			    at2->req_status);
			break;
		}
		if (ct2 == NULL) {
			/*
			 * Just do an ACCEPT on this fellow.
			 */
			at2->req_header.rqs_entry_type = RQSTYPE_ATIO2;
			at2->req_header.rqs_flags = 0;
			at2->req_flags = 1;
			ireqp = at2;
			reqsize = sizeof (*at2);
			break;
		}
		PRINTF("%s: datalen %d cdb0=0x%x\n", isp->isp_name,
		    at2->req_datalen, at2->req_cdb[0]);
		bzero ((void *) ct2, sizeof (*ct2));
		ct2->req_header.rqs_entry_type = RQSTYPE_CTIO2;
		ct2->req_header.rqs_entry_count = 1;
		ct2->req_header.rqs_flags = 0;
		ct2->req_header.rqs_seqno = isp->isp_seqno++;
		ct2->req_handle = (at2->req_initiator << 16) | at2->req_lun;
		ct2->req_lun = at2->req_lun;
		ct2->req_initiator = at2->req_initiator;
		ct2->req_rxid = at2->req_rxid;

		ct2->req_flags = CTIO_SEND_STATUS;
		switch (at2->req_cdb[0]) {
		case 0x0:		/* TUR */
			ct2->req_flags |= CTIO_NODATA | CTIO2_SMODE0;
			ct2->req_m.mode0.req_scsi_status = CTIO2_STATUS_VALID;
			break;

		case 0x3:		/* REQUEST SENSE */
		case 0x12:		/* INQUIRE */
			ct2->req_flags |= CTIO_SEND_DATA | CTIO2_SMODE0;
			ct2->req_m.mode0.req_scsi_status = CTIO2_STATUS_VALID;
			ct2->req_seg_count = 1;
			if (at2->req_cdb[0] == 0x12) {
				s = sizeof(tgtiqd);
				bcopy((void *)tgtiqd, fcp->isp_scratch, s);
			} else {
				s = at2->req_datalen;
				bzero(fcp->isp_scratch, s);
			}
			ct2->req_m.mode0.req_dataseg[0].ds_base =
			    fcp->isp_scdma;
			ct2->req_m.mode0.req_dataseg[0].ds_count = s;
			ct2->req_m.mode0.req_datalen = s;
#if	0
			if (at2->req_datalen < s) {
				ct2->req_m.mode1.req_scsi_status |=
				    CTIO2_RESP_VALID|CTIO2_RSPOVERUN;
			} else if (at2->req_datalen > s) {
				ct2->req_m.mode1.req_scsi_status |=
				    CTIO2_RESP_VALID|CTIO2_RSPUNDERUN;
			}
#endif
			break;

		default:		/* ALL OTHERS */
			ct2->req_flags |= CTIO_NODATA | CTIO2_SMODE1;
			ct2->req_m.mode1.req_scsi_status = 0;
#if	0
			if (at2->req_datalen) {
				ct2->req_m.mode1.req_scsi_status |=
				    CTIO2_RSPUNDERUN;
#if	BYTE_ORDER == BIG_ENDIAN
				ct2->req_resid[1] = at2->req_datalen & 0xff;
				ct2->req_resid[0] =
					(at2->req_datalen >> 8) & 0xff;
				ct2->req_resid[3] =
					(at2->req_datalen >> 16) & 0xff;
				ct2->req_resid[2] =
					(at2->req_datalen >> 24) & 0xff;
#else
				ct2->req_resid[0] = at2->req_datalen & 0xff;
				ct2->req_resid[1] =
					(at2->req_datalen >> 8) & 0xff;
				ct2->req_resid[2] =
					(at2->req_datalen >> 16) & 0xff;
				ct2->req_resid[3] =
					(at2->req_datalen >> 24) & 0xff;
#endif
			}
#endif
			if ((at2->req_status & ATIO_SENSEVALID) == 0) {
				ct2->req_m.mode1.req_sense_len = 18;
				ct2->req_m.mode1.req_scsi_status |= 2;
				ct2->req_m.mode1.req_response[0] = 0x70;
				ct2->req_m.mode1.req_response[2] = 0x2;
			} else {
				ct2->req_m.mode1.req_sense_len = 18;
				ct2->req_m.mode1.req_scsi_status |=
				    at2->req_scsi_status;
				bcopy((void *)at2->req_sense,
				    (void *)ct2->req_m.mode1.req_response,
				    sizeof (at2->req_sense));
			}
			break;
		}
		reqsize = sizeof (*ct2);
		ireqp = ct2;
		break;
	}
	case RQSTYPE_CTIO2:
	{
		ispatiot2_t *at2;
		ispctiot2_t *ct2 = (ispctiot2_t *) sp;
		PRINTF("%s: CTIO2 returned status 0x%x\n", isp->isp_name,
		    ct2->req_status);
		/*
	 	 * Return the ATIO to the board.
		 */
		at2 = (ispatiot2_t *) sp;
		at2->req_header.rqs_entry_type = RQSTYPE_ATIO2;
		at2->req_header.rqs_entry_count = 1;
		at2->req_header.rqs_flags = 0;
		at2->req_header.rqs_seqno = isp->isp_seqno++;
		at2->req_status = 1;
		reqsize = sizeof (*at2);
		ireqp = at2;
		break;
	}
#endif
	default:
		PRINTF("%s: other response type %x\n", isp->isp_name,
		    sp->req_header.rqs_entry_type);
		break;
	}
	if (reqsize) {
		void *reqp;
		optr = isp->isp_reqodx = ISP_READ(isp, OUTMAILBOX4);
		iptr = isp->isp_reqidx;
		reqp = (void *) ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
		iptr = ISP_NXT_QENTRY(iptr, RQUEST_QUEUE_LEN);
		if (iptr == optr) {
			PRINTF("%s: Request Queue Overflow other response\n",
			    isp->isp_name);
		} else {
			bcopy(ireqp, reqp, reqsize);
			ISP_WRITE(isp, INMAILBOX4, iptr);
			isp->isp_reqidx = iptr;
		}
	}
	return (0);
}

#if	defined(ISP2100_TARGET_MODE) || defined(ISP_TARGET_MODE)
/*
 * Locks held, and ints disabled (if FC).
 *
 * XXX: SETUP ONLY FOR INITIAL ENABLING RIGHT NOW
 */
static int
isp_modify_lun(isp, lun, icnt, ccnt)
	struct ispsoftc *isp;
	int lun;	/* logical unit to enable, modify, or disable */
	int icnt;	/* immediate notify count */
	int ccnt;	/* command count */
{
	isplun_t *ip = NULL;
	u_int8_t iptr, optr;

	optr = isp->isp_reqodx = ISP_READ(isp, OUTMAILBOX4);
	iptr = isp->isp_reqidx;
	ip = (isplun_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
	iptr = ISP_NXT_QENTRY(iptr, RQUEST_QUEUE_LEN);
	if (iptr == optr) {
		PRINTF("%s: Request Queue Overflow in isp_modify_lun\n",
		    isp->isp_name);
		return (-1);
	}

	bzero((void *) ip, sizeof (*ip));
	ip->req_header.rqs_entry_type = RQSTYPE_ENABLE_LUN;
	ip->req_header.rqs_entry_count = 1;
	ip->req_header.rqs_flags = 0;
	ip->req_header.rqs_seqno = isp->isp_seqno++;
	ip->req_handle = RQSTYPE_ENABLE_LUN;
	ip->req_lun = lun;
	ip->req_cmdcount = ccnt;
	ip->req_imcount = icnt;
	ip->req_timeout = 0;	/* default 30 seconds */
	ISP_WRITE(isp, INMAILBOX4, iptr);
	isp->isp_reqidx = iptr;
	return (0);
}
#endif

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
			IDPRINTF(3, ("%s: Selection Timeout for target %d\n",
			    isp->isp_name, XS_TGT(xs)));
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return;
		}
		PRINTF("%s: command incomplete for target %d lun %d, state "
		    "0x%x\n", isp->isp_name, XS_TGT(xs), XS_LUN(xs),
		    sp->req_state_flags);
		break;

	case RQCS_DMA_ERROR:
		PRINTF("%s: DMA error for command on target %d, lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_TRANSPORT_ERROR:
		PRINTF("%s: transport error\n", isp->isp_name);
		isp_prtstst(sp);
		break;

	case RQCS_RESET_OCCURRED:
		IDPRINTF(2, ("%s: bus reset destroyed command for target %d "
		    "lun %d\n", isp->isp_name, XS_TGT(xs), XS_LUN(xs)));
		isp->isp_sendmarker = 1;
		XS_SETERR(xs, HBA_BUSRESET);
		return;

	case RQCS_ABORTED:
		PRINTF("%s: command aborted for target %d lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		isp->isp_sendmarker = 1;
		XS_SETERR(xs, HBA_ABORTED);
		return;

	case RQCS_TIMEOUT:
		IDPRINTF(2, ("%s: command timed out for target %d lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs)));
		XS_SETERR(xs, HBA_CMDTIMEOUT);
		return;

	case RQCS_DATA_OVERRUN:
		if (isp->isp_type & ISP_HA_FC) {
			XS_RESID(xs) = sp->req_resid;
			break;
		}
		XS_SETERR(xs, HBA_DATAOVR);
		return;

	case RQCS_COMMAND_OVERRUN:
		PRINTF("%s: command overrun for command on target %d, lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_STATUS_OVERRUN:
		PRINTF("%s: status overrun for command on target %d, lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_BAD_MESSAGE:
		PRINTF("%s: message not COMMAND COMPLETE after status on "
		    "target %d, lun %d\n", isp->isp_name, XS_TGT(xs),
		    XS_LUN(xs));
		break;

	case RQCS_NO_MESSAGE_OUT:
		PRINTF("%s: No MESSAGE OUT phase after selection on "
		    "target %d, lun %d\n", isp->isp_name, XS_TGT(xs),
		    XS_LUN(xs));
		break;

	case RQCS_EXT_ID_FAILED:
		PRINTF("%s: EXTENDED IDENTIFY failed on target %d, lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_IDE_MSG_FAILED:
		PRINTF("%s: target %d lun %d rejected INITIATOR DETECTED "
		    "ERROR message\n", isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_ABORT_MSG_FAILED:
		PRINTF("%s: target %d lun %d rejected ABORT message\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_REJECT_MSG_FAILED:
		PRINTF("%s: target %d lun %d rejected MESSAGE REJECT message\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_NOP_MSG_FAILED:
		PRINTF("%s: target %d lun %d rejected NOP message\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_PARITY_ERROR_MSG_FAILED:
		PRINTF("%s: target %d lun %d rejected MESSAGE PARITY ERROR "
		    "message\n", isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_DEVICE_RESET_MSG_FAILED:
		PRINTF("%s: target %d lun %d rejected BUS DEVICE RESET "
		    "message\n", isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_ID_MSG_FAILED:
		PRINTF("%s: target %d lun %d rejected IDENTIFY "
		    "message\n", isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_UNEXP_BUS_FREE:
		PRINTF("%s: target %d lun %d had unexeptected bus free\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_DATA_UNDERRUN:
		if (isp->isp_type & ISP_HA_FC) {
			XS_RESID(xs) = sp->req_resid;
			/* an UNDERRUN is not a botch ??? */
		}
		XS_SETERR(xs, HBA_NOERROR);
		return;

	case RQCS_XACT_ERR1:
		PRINTF("%s: HBA attempted queued transaction with disconnect "
		    "not set for target %d lun %d\n", isp->isp_name, XS_TGT(xs),
		    XS_LUN(xs));
		break;

	case RQCS_XACT_ERR2:
		PRINTF("%s: HBA attempted queued transaction to target "
		    "routine %d on target %d\n", isp->isp_name, XS_LUN(xs),
		    XS_TGT(xs));
		break;

	case RQCS_XACT_ERR3:
		PRINTF("%s: HBA attempted queued transaction for target %d lun "
		    "%d when queueing disabled\n", isp->isp_name, XS_TGT(xs),
		    XS_LUN(xs));
		break;

	case RQCS_BAD_ENTRY:
		PRINTF("%s: invalid IOCB entry type detected\n", isp->isp_name);
		break;

	case RQCS_QUEUE_FULL:
		PRINTF("%s: internal queues full for target %d lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_PHASE_SKIPPED:
		PRINTF("%s: SCSI phase skipped (e.g., COMMAND COMPLETE w/o "
		    "STATUS phase) for target %d lun %d\n", isp->isp_name,
		    XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_ARQS_FAILED:
		PRINTF("%s: Auto Request Sense failed for target %d lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		XS_SETERR(xs, HBA_ARQFAIL);
		return;

	case RQCS_WIDE_FAILED:
		PRINTF("%s: Wide Negotiation failed for target %d lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		if (isp->isp_type & ISP_HA_SCSI) {
			sdparam *sdp = isp->isp_param;
			isp->isp_update = 1;
			sdp->isp_devparam[XS_TGT(xs)].dev_update = 1;
			sdp->isp_devparam[XS_TGT(xs)].dev_flags &= ~DPARM_WIDE;
		}
		XS_SETERR(xs, HBA_NOERROR);
		return;

	case RQCS_SYNCXFER_FAILED:
		PRINTF("%s: SDTR Message failed for target %d lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		if (isp->isp_type & ISP_HA_SCSI) {
			sdparam *sdp = isp->isp_param;
			isp->isp_update = 1;
			sdp->isp_devparam[XS_TGT(xs)].dev_update = 1;
			sdp->isp_devparam[XS_TGT(xs)].dev_flags &= ~DPARM_SYNC;
		}
		break;

	case RQCS_LVD_BUSERR:
		PRINTF("%s: Bad LVD Bus condition while talking to target %d "
		    "lun %d\n", isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_PORT_UNAVAILABLE:
		/*
		 * No such port on the loop. Moral equivalent of SELTIMEO
		 */
		IDPRINTF(3, ("%s: Port Unavailable for target %d\n",
		    isp->isp_name, XS_TGT(xs)));
		XS_SETERR(xs, HBA_SELTIMEOUT);
		return;

	case RQCS_PORT_LOGGED_OUT:
		/*
		 * It was there (maybe)- treat as a selection timeout.
		 */
		PRINTF("%s: port logout for target %d\n",
			isp->isp_name, XS_TGT(xs));
		XS_SETERR(xs, HBA_SELTIMEOUT);
		return;

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
	MAKNIB(1, 3),	/* 0x20: MBOX_GET_INIT_SCSI_ID, MBOX_GET_LOOP_ID */
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
	int loops, dld = 0;
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
			if (dld++) {
				return;
			}
			PRINTF("%s: but we'll try again, isr=%x\n",
			    isp->isp_name, ISP_READ(isp, BIU_ISR));
			if (ISP_READ(isp, BIU_SEMA) & 1) {
				u_int16_t mbox = ISP_READ(isp, OUTMAILBOX0);
				if (isp_parse_async(isp, mbox))
					return;
				ISP_WRITE(isp, BIU_SEMA, 0);
			}
			ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
			goto command_known;
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
		IDPRINTF(2, ("%s: mbox cmd %x failed with INVALID_COMMAND\n",
		    isp->isp_name, opcode));
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

	case ASYNC_LOOP_UP:
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

void
isp_lostcmd(isp, xs)
	struct ispsoftc *isp;
	ISP_SCSI_XFER_T *xs;
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
	mbs.param[1] = (XS_TGT(xs) << 8) | XS_LUN(xs);
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
isp_dumpregs(isp, msg)
	struct ispsoftc *isp;
	const char *msg;
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
isp_dumpxflist(isp)
	struct ispsoftc *isp;
{
	volatile ISP_SCSI_XFER_T *xs;
	int i, hdp;

	for (hdp = i = 0; i < RQUEST_QUEUE_LEN; i++) {
		xs = isp->isp_xflist[i];
		if (xs == NULL) {
			continue;
		}
		if (hdp == 0) {
			PRINTF("%s: active requests\n", isp->isp_name);
			hdp++;
		}
		PRINTF(" Active Handle %d: tgt %d lun %d dlen %d\n",
		    i+1, XS_TGT(xs), XS_LUN(xs), XS_XFRLEN(xs));
	}
}

static void
isp_fw_state(isp)
	struct ispsoftc *isp;
{
	mbreg_t mbs;
	if (isp->isp_type & ISP_HA_FC) {
		int once = 0;
		fcparam *fcp = isp->isp_param;
again:
		mbs.param[0] = MBOX_GET_FW_STATE;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			if (mbs.param[0] == ASYNC_LIP_OCCURRED ||
			    mbs.param[0] == ASYNC_LOOP_UP) {
				if (once++ < 2) {
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
isp_update(isp)
	struct ispsoftc *isp;
{
	int tgt;
	mbreg_t mbs;
	sdparam *sdp;

	isp->isp_update = 0;

	if (isp->isp_type & ISP_HA_FC) {
		return;
	}

	sdp = isp->isp_param;
	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		if (sdp->isp_devparam[tgt].dev_enable == 0) {
			continue;
		}
		if (sdp->isp_devparam[tgt].dev_update == 0) {
			continue;
		}

		mbs.param[0] = MBOX_SET_TARGET_PARAMS;
		mbs.param[1] = tgt << 8;
		mbs.param[2] = sdp->isp_devparam[tgt].dev_flags;
		mbs.param[3] =
			(sdp->isp_devparam[tgt].sync_offset << 8) |
			(sdp->isp_devparam[tgt].sync_period);

		IDPRINTF(3, ("\n%s: tgt %d cflags %x offset %x period %x\n",
		    isp->isp_name, tgt, mbs.param[2], mbs.param[3] >> 8,
		    mbs.param[3] & 0xff));

		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			PRINTF("%s: failed to change SCSI parameters for "
			    "target %d\n", isp->isp_name, tgt);
		} else {
			char *wt;
			int x, flags;

			flags = sdp->isp_devparam[tgt].cur_dflags =
			    sdp->isp_devparam[tgt].dev_flags;

			x = sdp->isp_devparam[tgt].sync_period & 0xff;
			if (flags & DPARM_SYNC) {
				if (x == (ISP_20M_SYNCPARMS & 0xff)) {
					x = 20;
				} else if (x == (ISP_10M_SYNCPARMS & 0xff)) {
					x = 10;
				} else if (x == (ISP_08M_SYNCPARMS & 0xff)) {
					x = 8;
				} else if (x == (ISP_05M_SYNCPARMS & 0xff)) {
					x = 5;
				} else if (x == (ISP_04M_SYNCPARMS & 0xff)) {
					x = 4;
				} else {
					x = 0;
				}
			} else {
				x = 0;
			}
			switch (flags & (DPARM_WIDE|DPARM_TQING)) {
			case DPARM_WIDE:
				wt = ", 16 bit wide\n";
				break;
			case DPARM_TQING:
				wt = ", Tagged Queueing Enabled\n";
				break;
			case DPARM_WIDE|DPARM_TQING:
				wt = ", 16 bit wide, Tagged Queueing Enabled\n";
				break;

			default:
				wt = "\n";
				break;
			}
			if (x) {
				IDPRINTF(3, ("%s: Target %d maximum Sync Mode "
				    "at %dMHz%s", isp->isp_name, tgt, x, wt));
			} else {
				IDPRINTF(3, ("%s: Target %d Async Mode%s",
				    isp->isp_name, tgt, wt));
			}
		}
		sdp->isp_devparam[tgt].dev_update = 0;
	}
}

static void
isp_setdfltparm(isp)
	struct ispsoftc *isp;
{
	int i, use_nvram;
	mbreg_t mbs;
	sdparam *sdp;

	/*
	 * Been there, done that, got the T-shirt...
	 */
	if (isp->isp_gotdparms) {
		IDPRINTF(3, ("%s: already have dparms\n", isp->isp_name));
		return;
	}
	isp->isp_gotdparms = 1;

	use_nvram = (isp_read_nvram(isp) == 0);
	if (use_nvram) {
		return;
	}
	if (isp->isp_type & ISP_HA_FC) {
		fcparam *fcp = (fcparam *) isp->isp_param;
		fcp->isp_maxfrmlen = ICB_DFLT_FRMLEN;
		fcp->isp_maxalloc = 256;
		fcp->isp_execthrottle = 16;
		fcp->isp_retry_delay = 5;
		fcp->isp_retry_count = 0;
		/*
		 * It would be nice to fake up a WWN in case we don't
		 * get one out of NVRAM. Solaris does this for SOCAL
		 * cards that don't have SBus properties- it sets up
		 * a WWN based upon the system MAC Address.
		 */
		fcp->isp_wwn = 0;
		return;
	}

	sdp = (sdparam *) isp->isp_param;
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
			PRINTF("%s: can't get SCSI parameters for target %d\n",
			    isp->isp_name, i);
			sdp->isp_devparam[i].sync_period = 0;
			sdp->isp_devparam[i].sync_offset = 0;
			sdp->isp_devparam[i].dev_flags = DPARM_SAFE_DFLT;
			continue;
		}
		sdp->isp_devparam[i].dev_flags = mbs.param[2];

		/*
		 * The maximum period we can really see
		 * here is 100 (decimal), or 400 ns.
		 * For some unknown reason we sometimes
		 * get back wildass numbers from the
		 * boot device's parameters.
		 *
		 * XXX: Hmm- this may be based on a different
		 * XXX: clock rate.
		 */
		if ((mbs.param[3] & 0xff) <= 0x64) {
			sdp->isp_devparam[i].sync_period = mbs.param[3] & 0xff;
			sdp->isp_devparam[i].sync_offset = mbs.param[3] >> 8; 
		}

		/*
		 * It is not safe to run Ultra Mode with a clock < 60.
		 */
		if (((sdp->isp_clock && sdp->isp_clock < 60) ||
		    (isp->isp_type < ISP_HA_SCSI_1020A)) &&
		    (sdp->isp_devparam[i].sync_period ==
		    (ISP_20M_SYNCPARMS & 0xff))) {
			sdp->isp_devparam[i].sync_offset =
				ISP_10M_SYNCPARMS >> 8;
			sdp->isp_devparam[i].sync_period =
				ISP_10M_SYNCPARMS & 0xff;
		}

	}

	/*
	 * Set Default Host Adapter Parameters
	 */
	sdp->isp_cmd_dma_burst_enable = 1;
	sdp->isp_data_dma_burst_enabl = 1;
	sdp->isp_fifo_threshold = 0;
	sdp->isp_initiator_id = 7;
	if (isp->isp_type >= ISP_HA_SCSI_1040) {
		sdp->isp_async_data_setup = 9;
	} else {
		sdp->isp_async_data_setup = 6;
	}
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

/* 
 * Re-initialize the ISP and complete all orphaned commands
 * with a 'botched' notice.
 *
 * Locks held prior to coming here.
 */

void
isp_restart(isp)
	struct ispsoftc *isp;
{
	ISP_SCSI_XFER_T *tlist[RQUEST_QUEUE_LEN], *xs;
	int i;

	for (i = 0; i < RQUEST_QUEUE_LEN; i++) {
		tlist[i] = (ISP_SCSI_XFER_T *) isp->isp_xflist[i];
		isp->isp_xflist[i] = NULL;
	}
	isp_reset(isp);
	if (isp->isp_state == ISP_RESETSTATE) {
		isp_init(isp);
		if (isp->isp_state == ISP_INITSTATE) {
			isp->isp_state = ISP_RUNSTATE;
		}
	}
	if (isp->isp_state != ISP_RUNSTATE) {
		PRINTF("%s: isp_restart cannot restart ISP\n", isp->isp_name);
	}

	for (i = 0; i < RQUEST_QUEUE_LEN; i++) {
		xs = tlist[i];
		if (XS_NULL(xs)) {
			continue;
		}
		isp->isp_nactive--;
		if (isp->isp_nactive < 0) {
			isp->isp_nactive = 0;
		}
		XS_RESID(xs) = XS_XFRLEN(xs);
		XS_SETERR(xs, HBA_BUSRESET);
		XS_CMD_DONE(xs);
	}
}

void
isp_watch(arg)
	void *arg;
{
	int i;
	struct ispsoftc *isp = arg;
	ISP_SCSI_XFER_T *xs;
	ISP_LOCKVAL_DECL;

	/*
	 * Look for completely dead commands (but not polled ones).
	 */
	ISP_ILOCK(isp);
	for (i = 0; i < RQUEST_QUEUE_LEN; i++) {
		if ((xs = (ISP_SCSI_XFER_T *) isp->isp_xflist[i]) == NULL) {
			continue;
		}
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
		if (isp_control(isp, ISPCTL_ABORT_CMD, xs)) {
			PRINTF("%s: isp_watch failed to abort command\n",
			    isp->isp_name);
			isp_restart(isp);
			break;
		}
	}
	ISP_IUNLOCK(isp);
	RESTART_WATCHDOG(isp_watch, isp);
}

static void
isp_prtstst(sp)
	ispstatusreq_t *sp;
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

/*
 * NVRAM Routines
 */

static int
isp_read_nvram(isp)
	struct ispsoftc *isp;
{
	int i, amt;
	u_int8_t csum, minversion;
	union {
		u_int8_t _x[ISP2100_NVRAM_SIZE];
		u_int16_t _s[ISP2100_NVRAM_SIZE>>1];
	} _n;
#define	nvram_data	_n._x
#define	nvram_words	_n._s

	if (isp->isp_type & ISP_HA_FC) {
		amt = ISP2100_NVRAM_SIZE;
		minversion = 1;
	} else {
		amt = ISP_NVRAM_SIZE;
		minversion = 2;
	}

	/*
	 * Just read the first two words first to see if we have a valid
	 * NVRAM to continue reading the rest with.
	 */
	for (i = 0; i < 2; i++) {
		isp_rdnvram_word(isp, i, &nvram_words[i]);
	}
	if (nvram_data[0] != 'I' || nvram_data[1] != 'S' ||
	    nvram_data[2] != 'P') {
		if (isp->isp_bustype != ISP_BT_SBUS) {
			PRINTF("%s: invalid NVRAM header\n", isp->isp_name);
		}
		return (-1);
	}
	for (i = 2; i < amt>>1; i++) {
		isp_rdnvram_word(isp, i, &nvram_words[i]);
	}
	for (csum = 0, i = 0; i < amt; i++) {
		csum += nvram_data[i];
	}
	if (csum != 0) {
		PRINTF("%s: invalid NVRAM checksum\n", isp->isp_name);
		return (-1);
	}
	if (ISP_NVRAM_VERSION(nvram_data) < minversion) {
		PRINTF("%s: version %d NVRAM not understood\n", isp->isp_name,
		    ISP_NVRAM_VERSION(nvram_data));
		return (-1);
	}

	if (isp->isp_type & ISP_HA_SCSI) {
		sdparam *sdp = (sdparam *) isp->isp_param;

		/* XXX CHECK THIS FOR SANITY XXX */
		sdp->isp_fifo_threshold =
			ISP_NVRAM_FIFO_THRESHOLD(nvram_data);

		sdp->isp_initiator_id =
			ISP_NVRAM_INITIATOR_ID(nvram_data);

		sdp->isp_bus_reset_delay =
			ISP_NVRAM_BUS_RESET_DELAY(nvram_data);

		sdp->isp_retry_count =
			ISP_NVRAM_BUS_RETRY_COUNT(nvram_data);

		sdp->isp_retry_delay =
			ISP_NVRAM_BUS_RETRY_DELAY(nvram_data);

		sdp->isp_async_data_setup =
			ISP_NVRAM_ASYNC_DATA_SETUP_TIME(nvram_data);

		if (isp->isp_type >= ISP_HA_SCSI_1040) {
			if (sdp->isp_async_data_setup < 9) {
				sdp->isp_async_data_setup = 9;
			}
		} else {
			if (sdp->isp_async_data_setup != 6) {
				sdp->isp_async_data_setup = 6;
			}
		}
		
		sdp->isp_req_ack_active_neg =
			ISP_NVRAM_REQ_ACK_ACTIVE_NEGATION(nvram_data);

		sdp->isp_data_line_active_neg =
			ISP_NVRAM_DATA_LINE_ACTIVE_NEGATION(nvram_data);

		sdp->isp_data_dma_burst_enabl =
			ISP_NVRAM_DATA_DMA_BURST_ENABLE(nvram_data);

		sdp->isp_cmd_dma_burst_enable =
			ISP_NVRAM_CMD_DMA_BURST_ENABLE(nvram_data);

		sdp->isp_tag_aging =
			ISP_NVRAM_TAG_AGE_LIMIT(nvram_data);

		/* XXX ISP_NVRAM_FIFO_THRESHOLD_128 XXX */

		sdp->isp_selection_timeout =
			ISP_NVRAM_SELECTION_TIMEOUT(nvram_data);

		sdp->isp_max_queue_depth =
			ISP_NVRAM_MAX_QUEUE_DEPTH(nvram_data);

		sdp->isp_fast_mttr = ISP_NVRAM_FAST_MTTR_ENABLE(nvram_data);

		for (i = 0; i < 16; i++) {
			sdp->isp_devparam[i].dev_enable =
				ISP_NVRAM_TGT_DEVICE_ENABLE(nvram_data, i);
			sdp->isp_devparam[i].exc_throttle =
				ISP_NVRAM_TGT_EXEC_THROTTLE(nvram_data, i);
			sdp->isp_devparam[i].sync_offset =
				ISP_NVRAM_TGT_SYNC_OFFSET(nvram_data, i);
			sdp->isp_devparam[i].sync_period =
				ISP_NVRAM_TGT_SYNC_PERIOD(nvram_data, i);

			if (isp->isp_type < ISP_HA_SCSI_1040) {
				/*
				 * If we're not ultra, we can't possibly
				 * be a shorter period than this.
				 */
				if (sdp->isp_devparam[i].sync_period < 0x19) {
					sdp->isp_devparam[i].sync_period =
					    0x19;
				}
				if (sdp->isp_devparam[i].sync_offset > 0xc) {
					sdp->isp_devparam[i].sync_offset =
					    0x0c;
				}
			} else {
				if (sdp->isp_devparam[i].sync_offset > 0x8) {
					sdp->isp_devparam[i].sync_offset = 0x8;
				}
			}

			sdp->isp_devparam[i].dev_flags = 0;

			if (ISP_NVRAM_TGT_RENEG(nvram_data, i))
				sdp->isp_devparam[i].dev_flags |= DPARM_RENEG;
			if (ISP_NVRAM_TGT_QFRZ(nvram_data, i)) {
				PRINTF("%s: not supporting QFRZ option for "
				    "target %d\n", isp->isp_name, i);
			}
			sdp->isp_devparam[i].dev_flags |= DPARM_ARQ;
			if (ISP_NVRAM_TGT_ARQ(nvram_data, i) == 0) {
				PRINTF("%s: not disabling ARQ option for "
				    "target %d\n", isp->isp_name, i);
			}
			if (ISP_NVRAM_TGT_TQING(nvram_data, i))
				sdp->isp_devparam[i].dev_flags |= DPARM_TQING;
			if (ISP_NVRAM_TGT_SYNC(nvram_data, i))
				sdp->isp_devparam[i].dev_flags |= DPARM_SYNC;
			if (ISP_NVRAM_TGT_WIDE(nvram_data, i))
				sdp->isp_devparam[i].dev_flags |= DPARM_WIDE;
			if (ISP_NVRAM_TGT_PARITY(nvram_data, i))
				sdp->isp_devparam[i].dev_flags |= DPARM_PARITY;
			if (ISP_NVRAM_TGT_DISC(nvram_data, i))
				sdp->isp_devparam[i].dev_flags |= DPARM_DISC;
		}
	} else {
		fcparam *fcp = (fcparam *) isp->isp_param;
		union {
			struct {
#if	BYTE_ORDER == BIG_ENDIAN
				u_int32_t hi32;
				u_int32_t lo32;
#else
				u_int32_t lo32;
				u_int32_t hi32;
#endif
			} wds;
			u_int64_t full64;
		} wwnstore;

		wwnstore.full64 = ISP2100_NVRAM_NODE_NAME(nvram_data);
		PRINTF("%s: Adapter WWN 0x%08x%08x\n", isp->isp_name,
		    wwnstore.wds.hi32, wwnstore.wds.lo32);
		fcp->isp_wwn = wwnstore.full64;
		wwnstore.full64 = ISP2100_NVRAM_BOOT_NODE_NAME(nvram_data);
		if (wwnstore.full64 != 0) {
			PRINTF("%s: BOOT DEVICE WWN 0x%08x%08x\n", isp->isp_name,
			    wwnstore.wds.hi32, wwnstore.wds.lo32);
		}
		fcp->isp_maxalloc =
			ISP2100_NVRAM_MAXIOCBALLOCATION(nvram_data);
		fcp->isp_maxfrmlen =
			ISP2100_NVRAM_MAXFRAMELENGTH(nvram_data);
		fcp->isp_retry_delay =
			ISP2100_NVRAM_RETRY_DELAY(nvram_data);
		fcp->isp_retry_count =
			ISP2100_NVRAM_RETRY_COUNT(nvram_data);
		fcp->isp_loopid =
			ISP2100_NVRAM_HARDLOOPID(nvram_data);
		fcp->isp_execthrottle =
			ISP2100_NVRAM_EXECUTION_THROTTLE(nvram_data);
	}
	return (0);
}

static void
isp_rdnvram_word(isp, wo, rp)
	struct ispsoftc *isp;
	int wo;
	u_int16_t *rp;
{
	int i, cbits;
	u_int16_t bit, rqst;

	ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT);
	SYS_DELAY(2);
	ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT|BIU_NVRAM_CLOCK);
	SYS_DELAY(2);

	if (isp->isp_type & ISP_HA_FC) {
		wo &= ((ISP2100_NVRAM_SIZE >> 1) - 1);
		rqst = (ISP_NVRAM_READ << 8) | wo;
		cbits = 10;
	} else {
		wo &= ((ISP_NVRAM_SIZE >> 1) - 1);
		rqst = (ISP_NVRAM_READ << 6) | wo;
		cbits = 8;
	}

	/*
	 * Clock the word select request out...
	 */
	for (i = cbits; i >= 0; i--) {
		if ((rqst >> i) & 1) {
			bit = BIU_NVRAM_SELECT | BIU_NVRAM_DATAOUT;
		} else {
			bit = BIU_NVRAM_SELECT;
		}
		ISP_WRITE(isp, BIU_NVRAM, bit);
		SYS_DELAY(2);
		ISP_WRITE(isp, BIU_NVRAM, bit | BIU_NVRAM_CLOCK);
		SYS_DELAY(2);
		ISP_WRITE(isp, BIU_NVRAM, bit);
		SYS_DELAY(2);
	}
	/*
	 * Now read the result back in (bits come back in MSB format).
	 */
	*rp = 0;
	for (i = 0; i < 16; i++) {
		u_int16_t rv;
		*rp <<= 1;
		ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT|BIU_NVRAM_CLOCK);
		SYS_DELAY(2);
		rv = ISP_READ(isp, BIU_NVRAM);
		if (rv & BIU_NVRAM_DATAIN) {
			*rp |= 1;
		}
		SYS_DELAY(2);
		ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT);
		SYS_DELAY(2);
	}
	ISP_WRITE(isp, BIU_NVRAM, 0);
	SYS_DELAY(2);
#if	BYTE_ORDER == BIG_ENDIAN
	*rp = ((*rp >> 8) | ((*rp & 0xff) << 8));
#endif
}
