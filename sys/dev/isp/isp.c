/* $FreeBSD$ */
/*
 * Machine and OS Independent (well, as best as possible)
 * code for the Qlogic ISP SCSI adapters.
 *
 * Copyright (c) 1997, 1998, 1999 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
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
#ifdef	__OpenBSD__
#include <dev/ic/isp_openbsd.h>
#endif
#ifdef	__linux__
#include "isp_linux.h"
#endif

/*
 * General defines
 */

#define	MBOX_DELAY_COUNT	1000000 / 100

/*
 * Local static data
 */

/*
 * Local function prototypes.
 */
static int isp_parse_async __P((struct ispsoftc *, int));
static int isp_handle_other_response
__P((struct ispsoftc *, ispstatusreq_t *, u_int8_t *));
static void isp_parse_status
__P((struct ispsoftc *, ispstatusreq_t *, ISP_SCSI_XFER_T *));
static void isp_fastpost_complete __P((struct ispsoftc *, u_int32_t));
static void isp_scsi_init __P((struct ispsoftc *));
static void isp_scsi_channel_init __P((struct ispsoftc *, int));
static void isp_fibre_init __P((struct ispsoftc *));
static void isp_mark_getpdb_all __P((struct ispsoftc *));
static int isp_getpdb __P((struct ispsoftc *, int, isp_pdb_t *));
static u_int64_t isp_get_portname __P((struct ispsoftc *, int, int));
static int isp_fclink_test __P((struct ispsoftc *, int));
static int isp_same_lportdb __P((struct lportdb *, struct lportdb *));
static int isp_pdb_sync __P((struct ispsoftc *, int));
#ifdef	ISP2100_FABRIC
static int isp_scan_fabric __P((struct ispsoftc *));
#endif
static void isp_fw_state __P((struct ispsoftc *));
static void isp_dumpregs __P((struct ispsoftc *, const char *));
static void isp_mboxcmd __P((struct ispsoftc *, mbreg_t *));

static void isp_update __P((struct ispsoftc *));
static void isp_update_bus __P((struct ispsoftc *, int));
static void isp_setdfltparm __P((struct ispsoftc *, int));
static int isp_read_nvram __P((struct ispsoftc *));
static void isp_rdnvram_word __P((struct ispsoftc *, int, u_int16_t *));

/*
 * Reset Hardware.
 *
 * Hit the chip over the head, download new f/w if available and set it running.
 *
 * Locking done elsewhere.
 */
void
isp_reset(isp)
	struct ispsoftc *isp;
{
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

	/*
	 * After we've fired this chip up, zero out the conf1 register
	 * for SCSI adapters and other settings for the 2100.
	 */

	/*
	 * Get the current running firmware revision out of the
	 * chip before we hit it over the head (if this is our
	 * first time through). Note that we store this as the
	 * 'ROM' firmware revision- which it may not be. In any
	 * case, we don't really use this yet, but we may in
	 * the future.
	 */
	if (isp->isp_used == 0) {
		/*
		 * Just in case it was paused...
		 */
		ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE);
		mbs.param[0] = MBOX_ABOUT_FIRMWARE;
		isp_mboxcmd(isp, &mbs);
		/*
		 * If this fails, it probably means we're running
		 * an old prom, if anything at all...
		 */
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			isp->isp_romfw_rev[0] = mbs.param[1];
			isp->isp_romfw_rev[1] = mbs.param[2];
			isp->isp_romfw_rev[2] = mbs.param[3];
		}
		isp->isp_used = 1;
	}

	DISABLE_INTS(isp);

	/*
	 * Put the board into PAUSE mode.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);

	if (IS_FC(isp)) {
		revname = "2X00";
		switch (isp->isp_type) {
		case ISP_HA_FC_2100:
			revname[1] = '1';
			break;
		case ISP_HA_FC_2200:
			revname[1] = '2';
			break;
		default:
			break;
		}
	} else if (IS_12X0(isp)) {
		revname = "12X0";
		isp->isp_clock = 60;
	} else if (IS_1080(isp)) {
		u_int16_t l;
		sdparam *sdp = isp->isp_param;
		revname = "1080";
		isp->isp_clock = 100;
		l = ISP_READ(isp, SXP_PINS_DIFF) & ISP1080_MODE_MASK;
		switch (l) {
		case ISP1080_LVD_MODE:
			sdp->isp_lvdmode = 1;
			PRINTF("%s: LVD Mode\n", isp->isp_name);
			break;
		case ISP1080_HVD_MODE:
			sdp->isp_diffmode = 1;
			PRINTF("%s: Differential Mode\n", isp->isp_name);
			break;
		case ISP1080_SE_MODE:
			sdp->isp_ultramode = 1;
			PRINTF("%s: Single-Ended Mode\n", isp->isp_name);
			break;
		default:
			/*
			 * Hmm. Up in a wierd mode. This means all SCSI I/O
			 * buffer lines are tristated, so we're in a lot of
			 * trouble if we don't set things up right.
			 */
			PRINTF("%s: Illegal Mode 0x%x\n", isp->isp_name, l);
			break;
		}
	} else {
		sdparam *sdp = isp->isp_param;
		i = ISP_READ(isp, BIU_CONF0) & BIU_CONF0_HW_MASK;
		switch (i) {
		default:
			PRINTF("%s: unknown chip rev. 0x%x- assuming a 1020\n",
			    isp->isp_name, i);
			/* FALLTHROUGH */
		case 1:
			revname = "1020";
			isp->isp_type = ISP_HA_SCSI_1020;
			isp->isp_clock = 40;
			break;
		case 2:
			/*
			 * Some 1020A chips are Ultra Capable, but don't
			 * run the clock rate up for that unless told to
			 * do so by the Ultra Capable bits being set.
			 */
			revname = "1020A";
			isp->isp_type = ISP_HA_SCSI_1020A;
			isp->isp_clock = 40;
			break;
		case 3:
			revname = "1040";
			isp->isp_type = ISP_HA_SCSI_1040;
			isp->isp_clock = 60;
			break;
		case 4:
			revname = "1040A";
			isp->isp_type = ISP_HA_SCSI_1040A;
			isp->isp_clock = 60;
			break;
		case 5:
			revname = "1040B";
			isp->isp_type = ISP_HA_SCSI_1040B;
			isp->isp_clock = 60;
			break;
		case 6: 
			revname = "1040C(?)";
			isp->isp_type = ISP_HA_SCSI_1040C;
			isp->isp_clock = 60;
                        break; 
		}
		/*
		 * Now, while we're at it, gather info about ultra
		 * and/or differential mode.
		 */
		if (ISP_READ(isp, SXP_PINS_DIFF) & SXP_PINS_DIFF_MODE) {
			PRINTF("%s: Differential Mode\n", isp->isp_name);
			sdp->isp_diffmode = 1;
		} else {
			sdp->isp_diffmode = 0;
		}
		i = ISP_READ(isp, RISC_PSR);
		if (isp->isp_bustype == ISP_BT_SBUS) {
			i &= RISC_PSR_SBUS_ULTRA;
		} else {
			i &= RISC_PSR_PCI_ULTRA;
		}
		if (i != 0) {
			PRINTF("%s: Ultra Mode Capable\n", isp->isp_name);
			sdp->isp_ultramode = 1;
			/*
			 * If we're in Ultra Mode, we have to be 60Mhz clock-
			 * even for the SBus version.
			 */
			isp->isp_clock = 60;
		} else {
			sdp->isp_ultramode = 0;
			/*
			 * Clock is known. Gronk.
			 */
		}

		/*
		 * Machine dependent clock (if set) overrides
		 * our generic determinations.
		 */
		if (isp->isp_mdvec->dv_clock) {
			if (isp->isp_mdvec->dv_clock < isp->isp_clock) {
				isp->isp_clock = isp->isp_mdvec->dv_clock;
			}
		}

	}

	/*
	 * Do MD specific pre initialization
	 */
	ISP_RESET0(isp);

again:

	/*
	 * Hit the chip over the head with hammer,
	 * and give the ISP a chance to recover.
	 */

	if (IS_SCSI(isp)) {
		ISP_WRITE(isp, BIU_ICR, BIU_ICR_SOFT_RESET);
		/*
		 * A slight delay...
		 */
		SYS_DELAY(100);

#if	0
		PRINTF("%s: mbox0-5: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		    isp->isp_name, ISP_READ(isp, OUTMAILBOX0),
		    ISP_READ(isp, OUTMAILBOX1), ISP_READ(isp, OUTMAILBOX2),
		    ISP_READ(isp, OUTMAILBOX3), ISP_READ(isp, OUTMAILBOX4),
		    ISP_READ(isp, OUTMAILBOX5));
#endif

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

		/*
		 * Clear data && control DMA engines.
		 */
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
		if (IS_SCSI(isp)) {
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
	 * After we've fired this chip up, zero out the conf1 register
	 * for SCSI adapters and other settings for the 2100.
	 */

	if (IS_SCSI(isp)) {
		ISP_WRITE(isp, BIU_CONF1, 0);
	} else {
		ISP_WRITE(isp, BIU2100_CSR, 0);
	}

	/*
	 * Reset RISC Processor
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_RESET);
	SYS_DELAY(100);

	/*
	 * Establish some initial burst rate stuff.
	 * (only for the 1XX0 boards). This really should
	 * be done later after fetching from NVRAM.
	 */
	if (IS_SCSI(isp)) {
		u_int16_t tmp = isp->isp_mdvec->dv_conf1;
		/*
		 * Busted FIFO. Turn off all but burst enables.
		 */
		if (isp->isp_type == ISP_HA_SCSI_1040A) {
			tmp &= BIU_BURST_ENABLE;
		}
		ISP_SETBITS(isp, BIU_CONF1, tmp);
		if (tmp & BIU_BURST_ENABLE) {
			ISP_SETBITS(isp, CDMA_CONF, DMA_ENABLE_BURST);
			ISP_SETBITS(isp, DDMA_CONF, DMA_ENABLE_BURST);
		}
#ifdef	PTI_CARDS
		if (((sdparam *) isp->isp_param)->isp_ultramode) {
			while (ISP_READ(isp, RISC_MTR) != 0x1313) {
				ISP_WRITE(isp, RISC_MTR, 0x1313);
				ISP_WRITE(isp, HCCR, HCCR_CMD_STEP);
			}
		} else {
			ISP_WRITE(isp, RISC_MTR, 0x1212);
		}
		/*
		 * PTI specific register
		 */
		ISP_WRITE(isp, RISC_EMB, DUAL_BANK)
#else
		ISP_WRITE(isp, RISC_MTR, 0x1212);
#endif
	} else {
		ISP_WRITE(isp, RISC_MTR2100, 0x1212);
	}

	ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE); /* release paused processor */

	/*
	 * Do MD specific post initialization
	 */
	ISP_RESET1(isp);

	/*
	 * Wait for everything to finish firing up...
	 */
	loops = MBOX_DELAY_COUNT;
	while (ISP_READ(isp, OUTMAILBOX0) == MBOX_BUSY) {
		SYS_DELAY(100);
		if (--loops < 0) {
			PRINTF("%s: MBOX_BUSY never cleared on reset\n",
			    isp->isp_name);
			return;
		}
	}

	/*
	 * Up until this point we've done everything by just reading or
	 * setting registers. From this point on we rely on at least *some*
	 * kind of firmware running in the card.
	 */

	/*
	 * Do some sanity checking.
	 */
	mbs.param[0] = MBOX_NO_OP;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "NOP test failed");
		return;
	}

	if (IS_SCSI(isp)) {
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
	if ((isp->isp_mdvec->dv_ispfw != NULL) ||
	    (isp->isp_confopts & ISP_CFG_NORELOAD)) {
		dodnld = 0;
	}

	if (dodnld && isp->isp_mdvec->dv_ispfw) {
		u_int16_t fwlen  = isp->isp_mdvec->dv_fwlen;
		if (fwlen == 0)
			fwlen = isp->isp_mdvec->dv_ispfw[3]; /* usually here */
		for (i = 0; i < fwlen; i++) {
			mbs.param[0] = MBOX_WRITE_RAM_WORD;
			mbs.param[1] = isp->isp_mdvec->dv_codeorg + i;
			mbs.param[2] = isp->isp_mdvec->dv_ispfw[i];
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				PRINTF("%s: F/W download failed at word %d\n",
				    isp->isp_name, i);
				dodnld = 0;
				goto again;
			}
		}

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
	if (isp->isp_mdvec->dv_codeorg)
		mbs.param[1] = isp->isp_mdvec->dv_codeorg;
	else
		mbs.param[1] = 0x1000;
	isp_mboxcmd(isp, &mbs);

	if (IS_SCSI(isp)) {
		/*
		 * Set CLOCK RATE, but only if asked to.
		 */
		if (isp->isp_clock) {
			mbs.param[0] = MBOX_SET_CLOCK_RATE;
			mbs.param[1] = isp->isp_clock;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				PRINTF("failed to set clockrate (0x%x)\n",
				    mbs.param[0]);
				/* but continue */
			}
		}
	}
	mbs.param[0] = MBOX_ABOUT_FIRMWARE;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("could not get f/w started (0x%x)\n", mbs.param[0]);
		return;
	}
	CFGPRINTF("%s: Board Revision %s, %s F/W Revision %d.%d.%d\n",
	    isp->isp_name, revname, dodnld? "loaded" : "resident",
	    mbs.param[1], mbs.param[2], mbs.param[3]);
	if (IS_FC(isp)) {
		if (ISP_READ(isp, BIU2100_CSR) & BIU2100_PCI64) {
			CFGPRINTF("%s: in 64-Bit PCI slot\n", isp->isp_name);
		}
	}

	isp->isp_fwrev[0] = mbs.param[1];
	isp->isp_fwrev[1] = mbs.param[2];
	isp->isp_fwrev[2] = mbs.param[3];
	if (isp->isp_romfw_rev[0] || isp->isp_romfw_rev[1] ||
	    isp->isp_romfw_rev[2]) {
		CFGPRINTF("%s: Last F/W revision was %d.%d.%d\n", isp->isp_name,
		    isp->isp_romfw_rev[0], isp->isp_romfw_rev[1],
		    isp->isp_romfw_rev[2]);
	}

	mbs.param[0] = MBOX_GET_FIRMWARE_STATUS;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("%s: could not GET FIRMWARE STATUS\n", isp->isp_name);
		return;
	}
	isp->isp_maxcmds = mbs.param[2];
	CFGPRINTF("%s: %d max I/O commands supported\n",
	    isp->isp_name, mbs.param[2]);
	isp_fw_state(isp);

	/*
	 * Set up DMA for the request and result mailboxes.
	 */
	if (ISP_MBOXDMASETUP(isp) != 0) {
		PRINTF("%s: can't setup dma mailboxes\n", isp->isp_name);
		return;
	}
	isp->isp_state = ISP_RESETSTATE;
}

/*
 * Initialize Parameters of Hardware to a known state.
 *
 * Locks are held before coming here.
 */

void
isp_init(isp)
	struct ispsoftc *isp;
{
	/*
	 * Must do this first to get defaults established.
	 */
	isp_setdfltparm(isp, 0);
	if (IS_12X0(isp)) {
		isp_setdfltparm(isp, 1);
	}

	if (IS_FC(isp)) {
		isp_fibre_init(isp);
	} else {
		isp_scsi_init(isp);
	}
}

static void
isp_scsi_init(isp)
	struct ispsoftc *isp;
{
	sdparam *sdp_chan0, *sdp_chan1;
	mbreg_t mbs;

	sdp_chan0 = isp->isp_param;
	sdp_chan1 = sdp_chan0;
	if (IS_12X0(isp)) {
		sdp_chan1++;
	}

	/* First do overall per-card settings. */

	/*
	 * If we have fast memory timing enabled, turn it on.
	 */
	if (isp->isp_fast_mttr) {
		ISP_WRITE(isp, RISC_MTR, 0x1313);
	}

	/*
	 * Set Retry Delay and Count.
	 * You set both channels at the same time.
	 */
	mbs.param[0] = MBOX_SET_RETRY_COUNT;
	mbs.param[1] = sdp_chan0->isp_retry_count;
	mbs.param[2] = sdp_chan0->isp_retry_delay;
	mbs.param[6] = sdp_chan1->isp_retry_count;
	mbs.param[7] = sdp_chan1->isp_retry_delay;

	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("%s: failed to set retry count and retry delay\n",
		    isp->isp_name);
		return;
	}

	/*
	 * Set ASYNC DATA SETUP time. This is very important.
	 */
	mbs.param[0] = MBOX_SET_ASYNC_DATA_SETUP_TIME;
	mbs.param[1] = sdp_chan0->isp_async_data_setup;
	mbs.param[2] = sdp_chan1->isp_async_data_setup;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("%s: failed to set asynchronous data setup time\n",
		    isp->isp_name);
		return;
	}

	/*
	 * Set ACTIVE Negation State.
	 */
	mbs.param[0] = MBOX_SET_ACT_NEG_STATE;
	mbs.param[1] =
	    (sdp_chan0->isp_req_ack_active_neg << 4) |
	    (sdp_chan0->isp_data_line_active_neg << 5);
	mbs.param[2] =
	    (sdp_chan1->isp_req_ack_active_neg << 4) |
	    (sdp_chan1->isp_data_line_active_neg << 5);

	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("%s: failed to set active negation state "
		    "(%d,%d),(%d,%d)\n", isp->isp_name,
		    sdp_chan0->isp_req_ack_active_neg,
		    sdp_chan0->isp_data_line_active_neg,
		    sdp_chan1->isp_req_ack_active_neg,
		    sdp_chan1->isp_data_line_active_neg);
		/*
		 * But don't return.
		 */
	}

	/*
	 * Set the Tag Aging limit
	 */
	mbs.param[0] = MBOX_SET_TAG_AGE_LIMIT;
	mbs.param[1] = sdp_chan0->isp_tag_aging;
	mbs.param[2] = sdp_chan1->isp_tag_aging;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("%s: failed to set tag age limit (%d,%d)\n",
		    isp->isp_name, sdp_chan0->isp_tag_aging,
		    sdp_chan1->isp_tag_aging);
		return;
	}

	/*
	 * Set selection timeout.
	 */
	mbs.param[0] = MBOX_SET_SELECT_TIMEOUT;
	mbs.param[1] = sdp_chan0->isp_selection_timeout;
	mbs.param[2] = sdp_chan1->isp_selection_timeout;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("%s: failed to set selection timeout\n", isp->isp_name);
		return;
	}

	/* now do per-channel settings */
	isp_scsi_channel_init(isp, 0);
	if (IS_12X0(isp))
		isp_scsi_channel_init(isp, 1);

	/*
	 * Now enable request/response queues
	 */

	mbs.param[0] = MBOX_INIT_RES_QUEUE;
	mbs.param[1] = RESULT_QUEUE_LEN;
	mbs.param[2] = DMA_MSW(isp->isp_result_dma);
	mbs.param[3] = DMA_LSW(isp->isp_result_dma);
	mbs.param[4] = 0;
	mbs.param[5] = 0;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("%s: set of response queue failed\n", isp->isp_name);
		return;
	}
	isp->isp_residx = 0;

	mbs.param[0] = MBOX_INIT_REQ_QUEUE;
	mbs.param[1] = RQUEST_QUEUE_LEN;
	mbs.param[2] = DMA_MSW(isp->isp_rquest_dma);
	mbs.param[3] = DMA_LSW(isp->isp_rquest_dma);
	mbs.param[4] = 0;
	mbs.param[5] = 0;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("%s: set of request queue failed\n", isp->isp_name);
		return;
	}
	isp->isp_reqidx = isp->isp_reqodx = 0;

	/*
	 *  Turn on Fast Posting, LVD transitions
	 */

	if (IS_1080(isp) ||
	    ISP_FW_REVX(isp->isp_fwrev) >= ISP_FW_REV(7, 55, 0)) {
		mbs.param[0] = MBOX_SET_FW_FEATURES;
#ifndef	ISP_NO_FASTPOST_SCSI
		mbs.param[1] |= FW_FEATURE_FAST_POST;
#else
		mbs.param[1] = 0;
#endif
		if (IS_1080(isp))
			mbs.param[1] |= FW_FEATURE_LVD_NOTIFY;
		if (mbs.param[1] != 0) {
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				PRINTF("%s: unable enable FW features\n",
				    isp->isp_name);
			}
		}
	}

	/*
	 * Let the outer layers decide whether to issue a SCSI bus reset.
	 */
	isp->isp_state = ISP_INITSTATE;
}

static void
isp_scsi_channel_init(isp, channel)
	struct ispsoftc *isp;
	int channel;
{
	sdparam *sdp;
	mbreg_t mbs;
	int tgt;

	sdp = isp->isp_param;
	sdp += channel;

	/*
	 * Set (possibly new) Initiator ID.
	 */
	mbs.param[0] = MBOX_SET_INIT_SCSI_ID;
	mbs.param[1] = (channel << 7) | sdp->isp_initiator_id;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("%s: cannot set initiator id on bus %d to %d\n",
		    isp->isp_name, channel, sdp->isp_initiator_id);
		return;
	}

	/*
	 * Set current per-target parameters to a safe minimum.
	 */
	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		int maxlun, lun;
		u_int16_t sdf;

		if (sdp->isp_devparam[tgt].dev_enable == 0) {
			IDPRINTF(1, ("%s: skipping target %d bus %d settings\n",
			    isp->isp_name, tgt, channel));
			continue;
		}

		/*
		 * If we're in LVD mode, then we pretty much should
		 * only disable tagged queuing.
		 */
		if (IS_1080(isp) && sdp->isp_lvdmode) {
			sdf = DPARM_DEFAULT & ~DPARM_TQING;
		} else {
			sdf = DPARM_SAFE_DFLT;
			/*
			 * It is not quite clear when this changed over so that
			 * we could force narrow and async, so assume >= 7.55.
			 */
			if (ISP_FW_REVX(isp->isp_fwrev) >=
			    ISP_FW_REV(7, 55, 0)) {
				sdf |= DPARM_NARROW | DPARM_ASYNC;
			}
		}
		mbs.param[0] = MBOX_SET_TARGET_PARAMS;
		mbs.param[1] = (tgt << 8) | (channel << 15);
		mbs.param[2] = sdf;
		mbs.param[3] =
		    (sdp->isp_devparam[tgt].sync_offset << 8) |
		    (sdp->isp_devparam[tgt].sync_period);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			sdf = DPARM_SAFE_DFLT;
			mbs.param[0] = MBOX_SET_TARGET_PARAMS;
			mbs.param[1] = (tgt << 8) | (channel << 15);
			mbs.param[2] = sdf;
			mbs.param[3] =
			    (sdp->isp_devparam[tgt].sync_offset << 8) |
			    (sdp->isp_devparam[tgt].sync_period);
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				PRINTF("%s: failed even to set defaults for "
				    "target %d\n", isp->isp_name, tgt);
				continue;
			}
		}

#if	0
		/*
		 * We don't update dev_flags with what we've set
		 * because that's not the ultimate goal setting.
		 * If we succeed with the command, we *do* update
		 * cur_dflags by getting target parameters.
		 */
		mbs.param[0] = MBOX_GET_TARGET_PARAMS;
		mbs.param[1] = (tgt << 8) | (channel << 15);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			/*
			 * Urrr.... We'll set cur_dflags to DPARM_SAFE_DFLT so
			 * we don't try and do tags if tags aren't enabled.
			 */
			sdp->isp_devparam[tgt].cur_dflags = DPARM_SAFE_DFLT;
		} else {
			sdp->isp_devparam[tgt].cur_dflags = mbs.param[2];
			sdp->isp_devparam[tgt].cur_offset = mbs.param[3] >> 8;
			sdp->isp_devparam[tgt].cur_period = mbs.param[3] & 0xff;
		}
		IDPRINTF(3, ("%s: set flags 0x%x got 0x%x back for target %d\n",
		    isp->isp_name, sdf, mbs.param[2], tgt));
#else
		/*
		 * We don't update any information because we need to run
		 * at least one command per target to cause a new state
		 * to be latched.
		 */
#endif
		/*
		 * Ensure that we don't believe tagged queuing is enabled yet.
		 * It turns out that sometimes the ISP just ignores our
		 * attempts to set parameters for devices that it hasn't
		 * seen yet.
		 */
		sdp->isp_devparam[tgt].cur_dflags &= ~DPARM_TQING;
		if (ISP_FW_REVX(isp->isp_fwrev) >= ISP_FW_REV(7, 55, 0))
			maxlun = 32;
		else
			maxlun = 8;
		for (lun = 0; lun < maxlun; lun++) {
			mbs.param[0] = MBOX_SET_DEV_QUEUE_PARAMS;
			mbs.param[1] = (channel << 15) | (tgt << 8) | lun;
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
	int loopid;

	fcp = isp->isp_param;

	/*
	 * For systems that don't have BIOS methods for which
	 * we can easily change the NVRAM based loopid, we'll
	 * override that here. Note that when we initialize
	 * the firmware we may get back a different loopid than
	 * we asked for anyway. XXX This is probably not the
	 * best way to figure this out XXX
	 */
#ifndef	__i386__
	loopid = DEFAULT_LOOPID(isp);
#else
	loopid = fcp->isp_loopid;
#endif

	icbp = (isp_icb_t *) fcp->isp_scratch;
	MEMZERO(icbp, sizeof (*icbp));

	icbp->icb_version = ICB_VERSION1;
#ifdef	ISP_TARGET_MODE
	fcp->isp_fwoptions = ICBOPT_TGT_ENABLE;
#else
	fcp->isp_fwoptions = 0;
#endif
	fcp->isp_fwoptions |= ICBOPT_FAIRNESS;
	fcp->isp_fwoptions |= ICBOPT_PDBCHANGE_AE;
	fcp->isp_fwoptions |= ICBOPT_HARD_ADDRESS;
	/*
	 * We have to use FULL LOGIN even though it resets the loop too much
	 * because otherwise port database entries don't get updated after
	 * a LIP- this is a known f/w bug.
	 */
	if (ISP_FW_REVX(isp->isp_fwrev) < ISP_FW_REV(1, 17, 0)) {
		fcp->isp_fwoptions |= ICBOPT_FULL_LOGIN;
	}
#ifndef	ISP_NO_FASTPOST_FC
	fcp->isp_fwoptions |= ICBOPT_FAST_POST;
#endif
	if (isp->isp_confopts & ISP_CFG_FULL_DUPLEX)
		fcp->isp_fwoptions |= ICBOPT_FULL_DUPLEX;

	/*
	 * We don't set ICBOPT_PORTNAME because we want our
	 * Node Name && Port Names to be distinct.
	 */

	icbp->icb_fwoptions = fcp->isp_fwoptions;
	icbp->icb_maxfrmlen = fcp->isp_maxfrmlen;
	if (icbp->icb_maxfrmlen < ICB_MIN_FRMLEN ||
	    icbp->icb_maxfrmlen > ICB_MAX_FRMLEN) {
		PRINTF("%s: bad frame length (%d) from NVRAM- using %d\n",
		    isp->isp_name, fcp->isp_maxfrmlen, ICB_DFLT_FRMLEN);
		icbp->icb_maxfrmlen = ICB_DFLT_FRMLEN;
	}
	icbp->icb_maxalloc = fcp->isp_maxalloc;
	if (icbp->icb_maxalloc < 1) {
		PRINTF("%s: bad maximum allocation (%d)- using 16\n",
		     isp->isp_name, fcp->isp_maxalloc);
		icbp->icb_maxalloc = 16;
	}
	icbp->icb_execthrottle = fcp->isp_execthrottle;
	if (icbp->icb_execthrottle < 1) {
		PRINTF("%s: bad execution throttle of %d- using 16\n",
		    isp->isp_name, fcp->isp_execthrottle);
		icbp->icb_execthrottle = ICB_DFLT_THROTTLE;
	}
	icbp->icb_retry_delay = fcp->isp_retry_delay;
	icbp->icb_retry_count = fcp->isp_retry_count;
	icbp->icb_hardaddr = loopid;
	icbp->icb_logintime = 30;	/* 30 second login timeout */

	if (fcp->isp_nodewwn) {
		u_int64_t pn;
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_nodename, fcp->isp_nodewwn);
		if (fcp->isp_portwwn) {
			pn = fcp->isp_portwwn;
		} else {
			pn = fcp->isp_nodewwn |
			    (((u_int64_t)(isp->isp_unit+1)) << 56);
		}
		/*
		 * If the top nibble is 2, we can construct a port name
		 * from the node name by setting a nonzero instance in
		 * bits 56..59. Otherwise, we need to make it identical
		 * to Node name...
		 */
		if ((fcp->isp_nodewwn >> 60) == 2) {
			MAKE_NODE_NAME_FROM_WWN(icbp->icb_portname, pn);
		} else {
			MAKE_NODE_NAME_FROM_WWN(icbp->icb_portname,
			    fcp->isp_nodewwn);
		}
	} else {
		fcp->isp_fwoptions &= ~(ICBOPT_USE_PORTNAME|ICBOPT_FULL_LOGIN);
	}
	icbp->icb_rqstqlen = RQUEST_QUEUE_LEN;
	icbp->icb_rsltqlen = RESULT_QUEUE_LEN;
	icbp->icb_rqstaddr[RQRSP_ADDR0015] = DMA_LSW(isp->isp_rquest_dma);
	icbp->icb_rqstaddr[RQRSP_ADDR1631] = DMA_MSW(isp->isp_rquest_dma);
	icbp->icb_respaddr[RQRSP_ADDR0015] = DMA_LSW(isp->isp_result_dma);
	icbp->icb_respaddr[RQRSP_ADDR1631] = DMA_MSW(isp->isp_result_dma);
	ISP_SWIZZLE_ICB(isp, icbp);

	/*
	 * Do this *before* initializing the firmware.
	 */
	isp_mark_getpdb_all(isp);
	fcp->isp_fwstate = FW_CONFIG_WAIT;
	fcp->isp_loopstate = LOOP_NIL;

	MemoryBarrier();
	for (;;) {
		mbs.param[0] = MBOX_INIT_FIRMWARE;
		mbs.param[1] = 0;
		mbs.param[2] = DMA_MSW(fcp->isp_scdma);
		mbs.param[3] = DMA_LSW(fcp->isp_scdma);
		mbs.param[4] = 0;
		mbs.param[5] = 0;
		mbs.param[6] = 0;
		mbs.param[7] = 0;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			PRINTF("%s: INIT FIRMWARE failed (code 0x%x)\n",
			    isp->isp_name, mbs.param[0]);
			if (mbs.param[0] & 0x8000) {
				SYS_DELAY(1000);
				continue;
			}
			return;
		}
		break;
	}

	isp->isp_reqidx = isp->isp_reqodx = 0;
	isp->isp_residx = 0;
	isp->isp_sendmarker = 1;

	/*
	 * Whatever happens, we're now committed to being here.
	 */
	isp->isp_state = ISP_INITSTATE;

#ifdef	ISP_TARGET_MODE
	if (isp_modify_lun(isp, 0, 1, 1)) {
		PRINTF("%s: failed to enable target mode\n", isp->isp_name);
	}
#endif
}

/*
 * Fibre Channel Support- get the port database for the id.
 *
 * Locks are held before coming here. Return 0 if success,
 * else failure.
 */

static void
isp_mark_getpdb_all(isp)
	struct ispsoftc *isp;
{
	fcparam *fcp = (fcparam *) isp->isp_param;
	int i;
	for (i = 0; i < MAX_FC_TARG; i++) {
		fcp->portdb[i].valid = 0;
	}
}

static int
isp_getpdb(isp, id, pdbp)
	struct ispsoftc *isp;
	int id;
	isp_pdb_t *pdbp;
{
	fcparam *fcp = (fcparam *) isp->isp_param;
	mbreg_t mbs;

	mbs.param[0] = MBOX_GET_PORT_DB;
	mbs.param[1] = id << 8;
	mbs.param[2] = DMA_MSW(fcp->isp_scdma);
	mbs.param[3] = DMA_LSW(fcp->isp_scdma);
	/*
	 * Unneeded. For the 2100, except for initializing f/w, registers
	 * 4/5 have to not be written to.
	 *	mbs.param[4] = 0;
	 *	mbs.param[5] = 0;
	 *
	 */
	mbs.param[6] = 0;
	mbs.param[7] = 0;
	isp_mboxcmd(isp, &mbs);
	switch (mbs.param[0]) {
	case MBOX_COMMAND_COMPLETE:
		MemoryBarrier();
		ISP_UNSWIZZLE_AND_COPY_PDBP(isp, pdbp, fcp->isp_scratch);
		break;
	case MBOX_HOST_INTERFACE_ERROR:
		PRINTF("%s: DMA error getting port database\n", isp->isp_name);
		return (-1);
	case MBOX_COMMAND_PARAM_ERROR:
		/* Not Logged In */
		IDPRINTF(3, ("%s: Param Error on Get Port Database for id %d\n",
		    isp->isp_name, id));
		return (-1);
	default:
		PRINTF("%s: error 0x%x getting port database for ID %d\n",
		    isp->isp_name, mbs.param[0], id);
		return (-1);
	}
	return (0);
}

static u_int64_t
isp_get_portname(isp, loopid, nodename)
	struct ispsoftc *isp;
	int loopid;
	int nodename;
{
	u_int64_t wwn = 0;
	mbreg_t mbs;

	mbs.param[0] = MBOX_GET_PORT_NAME;
	mbs.param[1] = loopid << 8;
	if (nodename)
		mbs.param[1] |= 1;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
		wwn =
		    (((u_int64_t)(mbs.param[2] & 0xff)) << 56) |
		    (((u_int64_t)(mbs.param[2] >> 8))	<< 48) |
		    (((u_int64_t)(mbs.param[3] & 0xff))	<< 40) |
		    (((u_int64_t)(mbs.param[3] >> 8))	<< 32) |
		    (((u_int64_t)(mbs.param[6] & 0xff))	<< 24) |
		    (((u_int64_t)(mbs.param[6] >> 8))	<< 16) |
		    (((u_int64_t)(mbs.param[7] & 0xff))	<<  8) |
		    (((u_int64_t)(mbs.param[7] >> 8)));
	}
	return (wwn);
}

/*
 * Make sure we have good FC link and know our Loop ID.
 */

static int
isp_fclink_test(isp, waitdelay)
	struct ispsoftc *isp;
	int waitdelay;
{
	static char *toponames[] = {
		"Private Loop",
		"FL Port",
		"N-Port to N-Port",
		"F Port"
	};
	char *tname;
	mbreg_t mbs;
	int count, topo = -1;
	u_int8_t lwfs;
	fcparam *fcp;
#if	defined(ISP2100_FABRIC)
	isp_pdb_t pdb;
#endif
	fcp = isp->isp_param;

	/*
	 * Wait up to N microseconds for F/W to go to a ready state.
	 */
	lwfs = FW_CONFIG_WAIT;
	for (count = 0; count < waitdelay; count += 100) {
		isp_fw_state(isp);
		if (lwfs != fcp->isp_fwstate) {
			PRINTF("%s: Firmware State %s -> %s\n",
			    isp->isp_name, isp2100_fw_statename((int)lwfs),
			    isp2100_fw_statename((int)fcp->isp_fwstate));
			lwfs = fcp->isp_fwstate;
		}
		if (fcp->isp_fwstate == FW_READY) {
			break;
		}
		SYS_DELAY(100);	/* wait 100 microseconds */
	}

	/*
	 * If we haven't gone to 'ready' state, return.
	 */
	if (fcp->isp_fwstate != FW_READY) {
		return (-1);
	}

	/*
	 * Get our Loop ID (if possible). We really need to have it.
	 */
	mbs.param[0] = MBOX_GET_LOOP_ID;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		PRINTF("%s: GET LOOP ID failed\n", isp->isp_name);
		return (-1);
	}
	fcp->isp_loopid = mbs.param[1];
	if (isp->isp_type == ISP_HA_FC_2100) {
		if (ISP_FW_REVX(isp->isp_fwrev) >= ISP_FW_REV(2, 0, 14)) {
			topo = (int) mbs.param[6];
		}
	} else if (isp->isp_type == ISP_HA_FC_2100) {
		if (ISP_FW_REVX(isp->isp_fwrev) >= ISP_FW_REV(1, 17, 26)) {
			topo = (int) mbs.param[6];
		}
	}
	if (topo < 0 || topo > 3)
		tname = "unknown";
	else
		tname = toponames[topo];

	/*
	 * If we're not on a fabric, the low 8 bits will be our AL_PA.
	 * If we're on a fabric, the low 8 bits will still be our AL_PA.
	 */
	fcp->isp_alpa = mbs.param[2];
#if	defined(ISP2100_FABRIC)
	fcp->isp_onfabric = 0;
	if (isp_getpdb(isp, FL_PORT_ID, &pdb) == 0) {
		fcp->isp_portid = mbs.param[2] | (((int)mbs.param[3]) << 16);
		fcp->isp_onfabric = 1;
		CFGPRINTF("%s: Loop ID %d, AL_PA 0x%x, Port ID 0x%x Loop State "
		    "0x%x topology %s\n", isp->isp_name, fcp->isp_loopid,
		    fcp->isp_alpa, fcp->isp_portid, fcp->isp_loopstate, tname);

		/*
		 * Make sure we're logged out of all fabric devices.
		 */
		for (count = FC_SNS_ID+1; count < MAX_FC_TARG; count++) {
			struct lportdb *lp = &fcp->portdb[count];
			if (lp->valid == 0 || lp->fabdev == 0)
				continue;
			PRINTF("%s: logging out target %d at Loop ID %d "
			    "(port id 0x%x)\n", isp->isp_name, count,
			    lp->loopid, lp->portid);
			mbs.param[0] = MBOX_FABRIC_LOGOUT;
			mbs.param[1] = lp->loopid << 8;
			mbs.param[2] = 0;
			mbs.param[3] = 0;
			isp_mboxcmd(isp, &mbs);
		}
	} else
#endif
	CFGPRINTF("%s: Loop ID %d, ALPA 0x%x Loop State 0x%x topology %s\n",
	    isp->isp_name, fcp->isp_loopid, fcp->isp_alpa, fcp->isp_loopstate,
	    tname);
	fcp->loop_seen_once = 1;
	return (0);
}

/*
 * Compare two local port db entities and return 1 if they're the same, else 0.
 */

static int
isp_same_lportdb(a, b)
	struct lportdb *a, *b;
{
	/*
	 * We decide two lports are the same if they have non-zero and
	 * identical port WWNs and identical loop IDs.
	 */

	if (a->port_wwn == 0 || a->port_wwn != b->port_wwn ||
	    a->loopid != b->loopid) {
		return (0);
	} else {
		return (1);
	}
}

/*
 * Synchronize our soft copy of the port database with what the f/w thinks
 * (with a view toward possibly for a specific target....)
 */

static int
isp_pdb_sync(isp, target)
	struct ispsoftc *isp;
	int target;
{
	struct lportdb *lp, *tport;
	fcparam *fcp = isp->isp_param;
	isp_pdb_t pdb;
	int loopid, lim;

#ifdef	ISP2100_FABRIC
	/*
	 * XXX: If we do this *after* building up our local port database,
	 * XXX: the commands simply don't work.
	 */
	/*
	 * (Re)discover all fabric devices
	 */
	if (fcp->isp_onfabric)
		(void) isp_scan_fabric(isp);
#endif


	/*
	 * Run through the local loop ports and get port database info
	 * for each loop ID.
	 *
	 * There's a somewhat unexplained situation where the f/w passes back
	 * the wrong database entity- if that happens, just restart (up to
	 * FL_PORT_ID times).
	 */
	tport = fcp->tport;
	MEMZERO((void *) tport, sizeof (tport));
	for (lim = loopid = 0; loopid < FL_PORT_ID; loopid++) {
		/*
		 * make sure the temp port database is clean...
		 */
		lp = &tport[loopid];
		lp->node_wwn = isp_get_portname(isp, loopid, 1);
		if (lp->node_wwn == 0)
			continue;
		lp->port_wwn = isp_get_portname(isp, loopid, 0);
		if (lp->port_wwn == 0) {
			lp->node_wwn = 0;
			continue;
		}

		/*
		 * Get an entry....
		 */
		if (isp_getpdb(isp, loopid, &pdb) != 0) {
			continue;
		}

		/*
		 * If the returned database element doesn't match what we
		 * asked for, restart the process entirely (up to a point...).
		 */
		if (pdb.pdb_loopid != loopid) {
			IDPRINTF(0, ("%s: wankage (%d != %d)\n",
			    isp->isp_name, pdb.pdb_loopid, loopid));
			loopid = 0;
			if (lim++ < FL_PORT_ID) {
				continue;
			}
			PRINTF("%s: giving up on synchronizing the port "
			    "database\n", isp->isp_name);
			return (-1);
		}

		/*
		 * Save the pertinent info locally.
		 */
		lp->node_wwn =
		    (((u_int64_t)pdb.pdb_nodename[0]) << 56) |
		    (((u_int64_t)pdb.pdb_nodename[1]) << 48) |
		    (((u_int64_t)pdb.pdb_nodename[2]) << 40) |
		    (((u_int64_t)pdb.pdb_nodename[3]) << 32) |
		    (((u_int64_t)pdb.pdb_nodename[4]) << 24) |
		    (((u_int64_t)pdb.pdb_nodename[5]) << 16) |
		    (((u_int64_t)pdb.pdb_nodename[6]) <<  8) |
		    (((u_int64_t)pdb.pdb_nodename[7]));
		lp->port_wwn =
		    (((u_int64_t)pdb.pdb_portname[0]) << 56) |
		    (((u_int64_t)pdb.pdb_portname[1]) << 48) |
		    (((u_int64_t)pdb.pdb_portname[2]) << 40) |
		    (((u_int64_t)pdb.pdb_portname[3]) << 32) |
		    (((u_int64_t)pdb.pdb_portname[4]) << 24) |
		    (((u_int64_t)pdb.pdb_portname[5]) << 16) |
		    (((u_int64_t)pdb.pdb_portname[6]) <<  8) |
		    (((u_int64_t)pdb.pdb_portname[7]));
		lp->roles =
		    (pdb.pdb_prli_svc3 & SVC3_ROLE_MASK) >> SVC3_ROLE_SHIFT;
		lp->portid = BITS2WORD(pdb.pdb_portid_bits);
		lp->loopid = pdb.pdb_loopid;
		/*
		 * Do a quick check to see whether this matches the saved port
		 * database for the same loopid. We do this here to save
		 * searching later (if possible). Note that this fails over
		 * time as things shuffle on the loop- we get the current
		 * loop state (where loop id as an index matches loop id in
		 * use) and then compare it to our saved database which
		 * never shifts.
		 */
		if (isp_same_lportdb(lp, &fcp->portdb[target])) {
			lp->valid = 1;
		}
	}

	/*
	 * If we get this far, we've settled our differences with the f/w
	 * and we can say that the loop state is ready.
	 */
	fcp->isp_loopstate = LOOP_READY;

	/*
	 * Mark all of the permanent local loop database entries as invalid.
	 */
	for (loopid = 0; loopid < FL_PORT_ID; loopid++) {
		fcp->portdb[loopid].valid = 0;
	}

	/*
	 * Now merge our local copy of the port database into our saved copy.
	 * Notify the outer layers of new devices arriving.
	 */
	for (loopid = 0; loopid < FL_PORT_ID; loopid++) {
		int i;

		/*
		 * If we don't have a non-zero Port WWN, we're not here.
		 */
		if (tport[loopid].port_wwn == 0) {
			continue;
		}

		/*
		 * If we've already marked our tmp copy as valid,
		 * this means that we've decided that it's the
		 * same as our saved data base. This didn't include
		 * the 'valid' marking so we have set that here.
		 */
		if (tport[loopid].valid) {
			fcp->portdb[loopid].valid = 1;
			continue;
		}

		/*
		 * For the purposes of deciding whether this is the
		 * 'same' device or not, we only search for an identical
		 * Port WWN. Node WWNs may or may not be the same as
		 * the Port WWN, and there may be multiple different
		 * Port WWNs with the same Node WWN. It would be chaos
		 * to have multiple identical Port WWNs, so we don't
		 * allow that.
		 */

		for (i = 0; i < FL_PORT_ID; i++) {
			int j;
			if (fcp->portdb[i].port_wwn == 0)
				continue;
			if (fcp->portdb[i].port_wwn != tport[loopid].port_wwn)
				continue;
			/*
			 * We found this WWN elsewhere- it's changed
			 * loopids then. We don't change it's actual
			 * position in our cached port database- we
			 * just change the actual loop ID we'd use.
			 */
			if (fcp->portdb[i].loopid != loopid) {
				PRINTF("%s: Target ID %d Loop 0x%x (Port 0x%x) "
				    "=> Loop 0x%x (Port 0x%x) \n",
				    isp->isp_name, i, fcp->portdb[i].loopid,
				    fcp->portdb[i].portid, loopid,
				    tport[loopid].portid);
			}
			fcp->portdb[i].portid = tport[loopid].portid;
			fcp->portdb[i].loopid = loopid;
			fcp->portdb[i].valid = 1;
			/*
			 * XXX: Should we also propagate roles in case they
			 * XXX: changed?
			 */

			/*
			 * Now make sure this Port WWN doesn't exist elsewhere
			 * in the port database.
			 */
			for (j = i+1; j < FL_PORT_ID; j++) {
				if (fcp->portdb[i].port_wwn !=
				    fcp->portdb[j].port_wwn) {
					continue;
				}
				PRINTF("%s: Target ID %d Duplicates Target ID "
				    "%d- killing off both\n",
				    isp->isp_name, j, i);
				/*
				 * Invalidate the 'old' *and* 'new' ones.
				 * This is really harsh and not quite right,
				 * but if this happens, we really don't know
				 * who is what at this point.
				 */
				fcp->portdb[i].valid = 0;
				fcp->portdb[j].valid = 0;
			}
			break;
		}

		/*
		 * If we didn't traverse the entire port database,
		 * then we found (and remapped) an existing entry.
		 * No need to notify anyone- go for the next one.
		 */
		if (i < FL_PORT_ID) {
			continue;
		}

		/*
		 * We've not found this Port WWN anywhere. It's a new entry.
		 * See if we can leave it where it is (with target == loopid).
		 */
		if (fcp->portdb[loopid].port_wwn != 0) {
			for (lim = 0; lim < FL_PORT_ID; lim++) {
				if (fcp->portdb[lim].port_wwn == 0)
					break;
			}
			/* "Cannot Happen" */
			if (lim == FL_PORT_ID) {
				PRINTF("%s: remap overflow?\n", isp->isp_name);
				continue;
			}
			i = lim;
		} else {
			i = loopid;
		}

		/*
		 * NB:	The actual loopid we use here is loopid- we may
		 *	in fact be at a completely different index (target).
		 */
		fcp->portdb[i].loopid = loopid;
		fcp->portdb[i].port_wwn = tport[loopid].port_wwn;
		fcp->portdb[i].node_wwn = tport[loopid].node_wwn;
		fcp->portdb[i].roles = tport[loopid].roles;
		fcp->portdb[i].portid = tport[loopid].portid;
		fcp->portdb[i].valid = 1;

		/*
		 * Tell the outside world we've arrived.
		 */
		(void) isp_async(isp, ISPASYNC_PDB_CHANGED, &i);
	}

	/*
	 * Now find all previously used targets that are now invalid and
	 * notify the outer layers that they're gone.
	 */
	for (lp = fcp->portdb; lp < &fcp->portdb[FL_PORT_ID]; lp++) {
		if (lp->valid || lp->port_wwn == 0)
			continue;

		/*
		 * Tell the outside world we've gone away;
		 */
		loopid = lp - fcp->portdb;
		(void) isp_async(isp, ISPASYNC_PDB_CHANGED, &loopid);
		MEMZERO((void *) lp, sizeof (*lp));
	}

#ifdef	ISP2100_FABRIC
	/*
	 * Now log in any fabric devices
	 */
	for (lp = &fcp->portdb[FC_SNS_ID+1];
	     lp < &fcp->portdb[MAX_FC_TARG]; lp++) {
		mbreg_t mbs;

		/*
		 * Nothing here?
		 */
		if (lp->port_wwn == 0)
			continue;
		/*
		 * Don't try to log into yourself.
		 */
		if (lp->portid == fcp->isp_portid)
			continue;

		/*
		 * Force a logout.
		 */
		lp->loopid = loopid = lp - fcp->portdb;
		mbs.param[0] = MBOX_FABRIC_LOGOUT;
		mbs.param[1] = lp->loopid << 8;
		mbs.param[2] = 0;
		mbs.param[3] = 0;
		isp_mboxcmd(isp, &mbs);

		/*
		 * And log in....
		 */
		mbs.param[0] = MBOX_FABRIC_LOGIN;
		mbs.param[1] = lp->loopid << 8;
		mbs.param[2] = lp->portid >> 16;
		mbs.param[3] = lp->portid & 0xffff;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			lp->valid = 1;
			lp->fabdev = 1;
			if (isp_getpdb(isp, loopid, &pdb) != 0) {
				/*
				 * Be kind...
				 */
				lp->roles = (SVC3_TGT_ROLE >> SVC3_ROLE_SHIFT);
				PRINTF("%s: Faked PortID 0x%x into LoopID %d\n",
				    isp->isp_name, lp->portid, lp->loopid);
			} else if (pdb.pdb_loopid != lp->loopid) {
				lp->roles = (SVC3_TGT_ROLE >> SVC3_ROLE_SHIFT);
				PRINTF("%s: Wanked PortID 0x%x to LoopID %d\n",
				    isp->isp_name, lp->portid, lp->loopid);
			} else {
				lp->roles =
				    (pdb.pdb_prli_svc3 & SVC3_ROLE_MASK) >>
				    SVC3_ROLE_SHIFT;
				lp->portid = BITS2WORD(pdb.pdb_portid_bits);
				lp->loopid = pdb.pdb_loopid;
				lp->node_wwn =
				    (((u_int64_t)pdb.pdb_nodename[0]) << 56) |
				    (((u_int64_t)pdb.pdb_nodename[1]) << 48) |
				    (((u_int64_t)pdb.pdb_nodename[2]) << 40) |
				    (((u_int64_t)pdb.pdb_nodename[3]) << 32) |
				    (((u_int64_t)pdb.pdb_nodename[4]) << 24) |
				    (((u_int64_t)pdb.pdb_nodename[5]) << 16) |
				    (((u_int64_t)pdb.pdb_nodename[6]) <<  8) |
				    (((u_int64_t)pdb.pdb_nodename[7]));
				lp->port_wwn =
				    (((u_int64_t)pdb.pdb_portname[0]) << 56) |
				    (((u_int64_t)pdb.pdb_portname[1]) << 48) |
				    (((u_int64_t)pdb.pdb_portname[2]) << 40) |
				    (((u_int64_t)pdb.pdb_portname[3]) << 32) |
				    (((u_int64_t)pdb.pdb_portname[4]) << 24) |
				    (((u_int64_t)pdb.pdb_portname[5]) << 16) |
				    (((u_int64_t)pdb.pdb_portname[6]) <<  8) |
				    (((u_int64_t)pdb.pdb_portname[7]));
				(void) isp_async(isp, ISPASYNC_PDB_CHANGED,
				    &loopid);
			}
		}
	}
#endif
	return (0);
}

#ifdef	ISP2100_FABRIC
static int
isp_scan_fabric(isp)
	struct ispsoftc *isp;
{
	fcparam *fcp = isp->isp_param;
	u_int32_t portid, first_nz_portid;
	sns_screq_t *reqp;
	sns_scrsp_t *resp;
	mbreg_t mbs;
	int hicap;

	reqp = (sns_screq_t *) fcp->isp_scratch;
	resp = (sns_scrsp_t *) (&((char *)fcp->isp_scratch)[0x100]);
	first_nz_portid = portid = fcp->isp_portid;

	for (hicap = 0; hicap < 1024; hicap++) {
		MEMZERO((void *) reqp, SNS_GAN_REQ_SIZE);
		reqp->snscb_rblen = SNS_GAN_RESP_SIZE >> 1;
		reqp->snscb_addr[RQRSP_ADDR0015] =
			DMA_LSW(fcp->isp_scdma + 0x100);
		reqp->snscb_addr[RQRSP_ADDR1631] =
			DMA_MSW(fcp->isp_scdma + 0x100);
		reqp->snscb_sblen = 6;
		reqp->snscb_data[0] = SNS_GAN;
		reqp->snscb_data[4] = portid & 0xffff;
		reqp->snscb_data[5] = (portid >> 16) & 0xff;
		ISP_SWIZZLE_SNS_REQ(isp, reqp);
		mbs.param[0] = MBOX_SEND_SNS;
		mbs.param[1] = SNS_GAN_REQ_SIZE >> 1;
		mbs.param[2] = DMA_MSW(fcp->isp_scdma);
		mbs.param[3] = DMA_LSW(fcp->isp_scdma);
		mbs.param[6] = 0;
		mbs.param[7] = 0;
		MemoryBarrier();
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return (-1);
		}
		ISP_UNSWIZZLE_SNS_RSP(isp, resp, SNS_GAN_RESP_SIZE >> 1);
		portid = (((u_int32_t) resp->snscb_port_id[0]) << 16) |
		    (((u_int32_t) resp->snscb_port_id[1]) << 8) |
		    (((u_int32_t) resp->snscb_port_id[2]));
		if (isp_async(isp, ISPASYNC_FABRIC_DEV, resp)) {
			return (-1);
		}
		if (first_nz_portid == 0 && portid) {
			first_nz_portid = portid;
		}
		if (first_nz_portid == portid) {
			return (0);
		}
	}
	/*
	 * We either have a broken name server or a huge fabric if we get here.
	 */
	return (0);
}
#endif
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
	int target, i;

	XS_INITERR(xs);
	isp = XS_ISP(xs);

	if (isp->isp_state != ISP_RUNSTATE) {
		PRINTF("%s: adapter not ready\n", isp->isp_name);
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}

	/*
	 * We *could* do the different sequence type that has close
	 * to the whole Queue Entry for the command...
	 */

	if (XS_CDBLEN(xs) > (IS_FC(isp) ? 16 : 12) || XS_CDBLEN(xs) == 0) {
		PRINTF("%s: unsupported cdb length (%d, CDB[0]=0x%x)\n",
		    isp->isp_name, XS_CDBLEN(xs), XS_CDBP(xs)[0]);
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}

	/*
	 * Check to see whether we have good firmware state still or
	 * need to refresh our port database for this target.
	 */
	target = XS_TGT(xs);
	if (IS_FC(isp)) {
		fcparam *fcp = isp->isp_param;
		struct lportdb *lp;
#if	defined(ISP2100_FABRIC)
		if (target >= FL_PORT_ID) {
			/*
			 * If we're not on a Fabric, we can't have a target
			 * above FL_PORT_ID-1. If we're on a fabric, we
			 * can't have a target less than FC_SNS_ID+1.
			 */
			if (fcp->isp_onfabric == 0 || target <= FC_SNS_ID) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				return (CMD_COMPLETE);
			}
		}
#endif
		/*
		 * Check for f/w being in ready state. If the f/w
		 * isn't in ready state, then we don't know our
		 * loop ID and the f/w hasn't completed logging
		 * into all targets on the loop. If this is the
		 * case, then bounce the command. We pretend this is
		 * a SELECTION TIMEOUT error if we've never gone to
		 * FW_READY state at all- in this case we may not
		 * be hooked to a loop at all and we shouldn't hang
		 * the machine for this. Otherwise, defer this command
		 * until later.
		 */
		if (fcp->isp_fwstate != FW_READY) {
			if (isp_fclink_test(isp, FC_FW_READY_DELAY)) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				if (fcp->loop_seen_once) {
					return (CMD_RQLATER);
				} else {
					return (CMD_COMPLETE);
				}
			}
		}

		/*
		 * If our loop state is such that we haven't yet received
		 * a "Port Database Changed" notification (after a LIP or
		 * a Loop Reset or firmware initialization), then defer
		 * sending commands for a little while.
		 */
		if (fcp->isp_loopstate < LOOP_PDB_RCVD) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_RQLATER);
		}

		/*
		 * If our loop state is now such that we've just now
		 * received a Port Database Change notification, then
		 * we have to go off and (re)synchronize our 
		 */
		if (fcp->isp_loopstate == LOOP_PDB_RCVD) {
			if (isp_pdb_sync(isp, target)) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				return (CMD_COMPLETE);
			}
		}

		/*
		 * Now check whether we should even think about pursuing this.
		 */
		lp = &fcp->portdb[target];
		if (lp->valid == 0) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_COMPLETE);
		}
		if ((lp->roles & (SVC3_TGT_ROLE >> SVC3_ROLE_SHIFT)) == 0) {
			IDPRINTF(3, ("%s: target %d is not a target\n",
			    isp->isp_name, target));
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_COMPLETE);
		}
		/*
		 * Now turn target into what the actual loop ID is.
		 */
		target = lp->loopid;
	}

	/*
	 * Next check to see if any HBA or Device
	 * parameters need to be updated.
	 */
	if (isp->isp_update != 0) {
		isp_update(isp);
	}

	optr = isp->isp_reqodx = ISP_READ(isp, OUTMAILBOX4);
	iptr = isp->isp_reqidx;

	reqp = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
	iptr = ISP_NXT_QENTRY(iptr, RQUEST_QUEUE_LEN);
	if (iptr == optr) {
		IDPRINTF(0, ("%s: Request Queue Overflow\n", isp->isp_name));
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	}

	/*
	 * Now see if we need to synchronize the ISP with respect to anything.
	 * We do dual duty here (cough) for synchronizing for busses other
	 * than which we got here to send a command to.
	 */
	if (isp->isp_sendmarker) {
		u_int8_t niptr, n = (IS_12X0(isp)? 2: 1);
		/*
		 * Check ports to send markers for...
		 */
		for (i = 0; i < n; i++) {
			if ((isp->isp_sendmarker & (1 << i)) == 0) {
				continue;
			}
			MEMZERO((void *) reqp, sizeof (*reqp));
			reqp->req_header.rqs_entry_count = 1;
			reqp->req_header.rqs_entry_type = RQSTYPE_MARKER;
			reqp->req_modifier = SYNC_ALL;
			reqp->req_target = i << 7;	/* insert bus number */
			ISP_SWIZZLE_REQUEST(isp, reqp);

			/*
			 * Unconditionally update the input pointer anyway.
			 */
			ISP_WRITE(isp, INMAILBOX4, iptr);
			isp->isp_reqidx = iptr;

			niptr = ISP_NXT_QENTRY(iptr, RQUEST_QUEUE_LEN);
			if (niptr == optr) {
				IDPRINTF(0, ("%s: Request Queue Overflow+\n",
				    isp->isp_name));
				XS_SETERR(xs, HBA_BOTCH);
				return (CMD_EAGAIN);
			}
			reqp = (ispreq_t *)
			    ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
			iptr = niptr;
		}
	}

	MEMZERO((void *) reqp, UZSIZE);
	reqp->req_header.rqs_entry_count = 1;
	if (IS_FC(isp)) {
		reqp->req_header.rqs_entry_type = RQSTYPE_T2RQS;
	} else {
		reqp->req_header.rqs_entry_type = RQSTYPE_REQUEST;
	}
	reqp->req_header.rqs_flags = 0;
	reqp->req_header.rqs_seqno = 0;
	if (IS_FC(isp)) {
		/*
		 * See comment in isp_intr
		 */
		XS_RESID(xs) = 0;

		/*
		 * Fibre Channel always requires some kind of tag.
		 * The Qlogic drivers seem be happy not to use a tag,
		 * but this breaks for some devices (IBM drives).
		 */
		if (XS_CANTAG(xs)) {
			t2reqp->req_flags = XS_KINDOF_TAG(xs);
		} else {
			if (XS_CDBP(xs)[0] == 0x3)	/* REQUEST SENSE */
				t2reqp->req_flags = REQFLAG_HTAG;
			else
				t2reqp->req_flags = REQFLAG_OTAG;
		}
	} else {
		sdparam *sdp = (sdparam *)isp->isp_param;
		if ((sdp->isp_devparam[target].cur_dflags & DPARM_TQING) &&
		    XS_CANTAG(xs)) {
			reqp->req_flags = XS_KINDOF_TAG(xs);
		}
	}
	reqp->req_target = target | (XS_CHANNEL(xs) << 7);
	if (IS_SCSI(isp)) {
		reqp->req_lun_trn = XS_LUN(xs);
		reqp->req_cdblen = XS_CDBLEN(xs);
	} else {
#ifdef	ISP2100_SCCLUN
		t2reqp->req_scclun = XS_LUN(xs);
#else
		t2reqp->req_lun_trn = XS_LUN(xs);
#endif
	}
	MEMCPY(reqp->req_cdb, XS_CDBP(xs), XS_CDBLEN(xs));

	reqp->req_time = XS_TIME(xs) / 1000;
	if (reqp->req_time == 0 && XS_TIME(xs))
		reqp->req_time = 1;

	/*
	 * Always give a bit more leeway to commands after a bus reset.
	 * XXX: DOES NOT DISTINGUISH WHICH PORT MAY HAVE BEEN SYNCED
	 */
	if (isp->isp_sendmarker && reqp->req_time < 5) {
		reqp->req_time = 5;
	}
	if (isp_save_xs(isp, xs, &reqp->req_handle)) {
		IDPRINTF(2, ("%s: out of xflist pointers\n", isp->isp_name));
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	}
	/*
	 * Set up DMA and/or do any bus swizzling of the request entry
	 * so that the Qlogic F/W understands what is being asked of it.
 	*/
	i = ISP_DMASETUP(isp, xs, reqp, &iptr, optr);
	if (i != CMD_QUEUED) {
		isp_destroy_handle(isp, reqp->req_handle);
		/*
		 * dmasetup sets actual error in packet, and
		 * return what we were given to return.
		 */
		return (i);
	}
	XS_SETERR(xs, HBA_NOERROR);
	IDPRINTF(5, ("%s(%d.%d.%d): START cmd 0x%x datalen %d\n",
	    isp->isp_name, XS_CHANNEL(xs), target, XS_LUN(xs),
	    reqp->req_cdb[0], XS_XFRLEN(xs)));
	MemoryBarrier();
	ISP_WRITE(isp, INMAILBOX4, iptr);
	isp->isp_reqidx = iptr;
	isp->isp_nactive++;
	if (isp->isp_sendmarker)
		isp->isp_sendmarker = 0;
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
	int bus, tgt;
	u_int32_t handle;

	switch (ctl) {
	default:
		PRINTF("%s: isp_control unknown control op %x\n",
		    isp->isp_name, ctl);
		break;

	case ISPCTL_RESET_BUS:
		/*
		 * Issue a bus reset.
		 */
		mbs.param[0] = MBOX_BUS_RESET;
		if (IS_SCSI(isp)) {
			mbs.param[1] =
			    ((sdparam *) isp->isp_param)->isp_bus_reset_delay;
			if (mbs.param[1] < 2)
				mbs.param[1] = 2;
			bus = *((int *) arg);
			mbs.param[2] = bus;
		} else {
			mbs.param[1] = 10;
			mbs.param[2] = 0;
			bus = 0;
		}
		isp->isp_sendmarker = 1 << bus;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_dumpregs(isp, "isp_control SCSI bus reset failed");
			break;
		}
		PRINTF("%s: driver initiated bus reset of bus %d\n",
		    isp->isp_name, bus);
		return (0);

	case ISPCTL_RESET_DEV:
		tgt = (*((int *) arg)) & 0xffff;
		bus = (*((int *) arg)) >> 16;
		mbs.param[0] = MBOX_ABORT_TARGET;
		mbs.param[1] = (tgt << 8) | (bus << 15);
		mbs.param[2] = 3;	/* 'delay', in seconds */
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			PRINTF("%s: isp_control MBOX_RESET_DEV failure (code "
			    "%x)\n", isp->isp_name, mbs.param[0]);
			break;
		}
		PRINTF("%s: Target %d on Bus %d Reset Succeeded\n",
		    isp->isp_name, tgt, bus);
		isp->isp_sendmarker = 1 << bus;
		return (0);

	case ISPCTL_ABORT_CMD:
		xs = (ISP_SCSI_XFER_T *) arg;
		handle = isp_find_handle(isp, xs);
		if (handle == 0) {
			PRINTF("%s: isp_control- cannot find command to abort "
			    "in active list\n", isp->isp_name);
			break;
		}
		bus = XS_CHANNEL(xs);
		mbs.param[0] = MBOX_ABORT;
		if (IS_FC(isp)) {
#ifdef	ISP2100_SCCLUN
			mbs.param[1] = XS_TGT(xs) << 8;
			mbs.param[4] = 0;
			mbs.param[5] = 0;
			mbs.param[6] = XS_LUN(xs);
#else
			mbs.param[1] = XS_TGT(xs) << 8 | XS_LUN(xs);
#endif
		} else {
			mbs.param[1] =
			    (bus << 15) | (XS_TGT(xs) << 8) | XS_LUN(xs);
		}
		mbs.param[2] = handle >> 16;
		mbs.param[3] = handle & 0xffff;
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
		return (0);

	case ISPCTL_FCLINK_TEST:
		return (isp_fclink_test(isp, FC_FW_READY_DELAY));
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
	u_int16_t isr, isrb, sema;
	int i, nlooked = 0, ndone = 0;

	/*
	 * Well, if we've disabled interrupts, we may get a case where
	 * isr isn't set, but sema is. In any case, debounce isr reads.
	 */
	do {
		isr = ISP_READ(isp, BIU_ISR);
		isrb = ISP_READ(isp, BIU_ISR);
	} while (isr != isrb);
	sema = ISP_READ(isp, BIU_SEMA) & 0x1;
	IDPRINTF(5, ("%s: isp_intr isr %x sem %x\n", isp->isp_name, isr, sema));
	if (isr == 0) {
		return (0);
	}
	if (!INT_PENDING(isp, isr)) {
		IDPRINTF(4, ("%s: isp_intr isr=%x\n", isp->isp_name, isr));
		return (0);
	}
	if (isp->isp_state != ISP_RUNSTATE) {
		IDPRINTF(3, ("%s: interrupt (isr=%x,sema=%x) when not ready\n",
		    isp->isp_name, isr, sema));
		ISP_WRITE(isp, INMAILBOX5, ISP_READ(isp, OUTMAILBOX5));
		ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
		ISP_WRITE(isp, BIU_SEMA, 0);
		ENABLE_INTS(isp);
		return (1);
	}

	if (sema) {
		u_int16_t mbox = ISP_READ(isp, OUTMAILBOX0);
		if (mbox & 0x4000) {
			IDPRINTF(3, ("%s: Command Mbox 0x%x\n",
			    isp->isp_name, mbox));
		} else {
			u_int32_t fhandle = isp_parse_async(isp, (int) mbox);
			IDPRINTF(3, ("%s: Async Mbox 0x%x\n",
			    isp->isp_name, mbox));
			if (fhandle > 0) {
				isp_fastpost_complete(isp, fhandle);
			}
		}
		ISP_WRITE(isp, BIU_SEMA, 0);
		ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
		ENABLE_INTS(isp);
		return (1);
	}

	/*
	 * You *must* read OUTMAILBOX5 prior to clearing the RISC interrupt.
	 */
	optr = isp->isp_residx;
	iptr = ISP_READ(isp, OUTMAILBOX5);
	ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
	if (optr == iptr) {
		IDPRINTF(4, ("why intr? isr %x iptr %x optr %x\n",
		    isr, optr, iptr));
	}

	while (optr != iptr) {
		ispstatusreq_t *sp;
		u_int8_t oop;
		int buddaboom = 0;

		sp = (ispstatusreq_t *) ISP_QUEUE_ENTRY(isp->isp_result, optr);
		oop = optr;
		optr = ISP_NXT_QENTRY(optr, RESULT_QUEUE_LEN);
		nlooked++;
		MemoryBarrier();
		/*
		 * Do any appropriate unswizzling of what the Qlogic f/w has
		 * written into memory so it makes sense to us.
		 */
		ISP_UNSWIZZLE_RESPONSE(isp, sp);
		if (sp->req_header.rqs_entry_type != RQSTYPE_RESPONSE) {
			if (isp_handle_other_response(isp, sp, &optr) == 0) {
				ISP_WRITE(isp, INMAILBOX5, optr);
				continue;
			}
			/*
			 * It really has to be a bounced request just copied
			 * from the request queue to the response queue. If
			 * not, something bad has happened.
			 */
			if (sp->req_header.rqs_entry_type != RQSTYPE_REQUEST) {
				ISP_WRITE(isp, INMAILBOX5, optr);
				PRINTF("%s: not RESPONSE in RESPONSE Queue "
				    "(type 0x%x) @ idx %d (next %d)\n",
				    isp->isp_name,
				    sp->req_header.rqs_entry_type, oop, optr);
				continue;
			}
			buddaboom = 1;
		}

		if (sp->req_header.rqs_flags & 0xf) {
#define	_RQS_OFLAGS	\
	~(RQSFLAG_CONTINUATION|RQSFLAG_FULL|RQSFLAG_BADHEADER|RQSFLAG_BADPACKET)
			if (sp->req_header.rqs_flags & RQSFLAG_CONTINUATION) {
				IDPRINTF(3, ("%s: continuation segment\n",
				    isp->isp_name));
				ISP_WRITE(isp, INMAILBOX5, optr);
				continue;
			}
			if (sp->req_header.rqs_flags & RQSFLAG_FULL) {
				IDPRINTF(2, ("%s: internal queues full\n",
				    isp->isp_name));
				/*
				 * We'll synthesize a QUEUE FULL message below.
				 */
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
			if (sp->req_header.rqs_flags & _RQS_OFLAGS) {
				PRINTF("%s: unknown flags in response (0x%x)\n",
				    isp->isp_name, sp->req_header.rqs_flags);
				buddaboom++;
			}
#undef	_RQS_OFLAGS
		}
		if (sp->req_handle > isp->isp_maxcmds || sp->req_handle < 1) {
			PRINTF("%s: bad request handle %d\n", isp->isp_name,
			    sp->req_handle);
			ISP_WRITE(isp, INMAILBOX5, optr);
			continue;
		}
		xs = isp_find_xs(isp, sp->req_handle);
		if (xs == NULL) {
			PRINTF("%s: NULL xs in xflist (handle 0x%x)\n",
			    isp->isp_name, sp->req_handle);
			ISP_WRITE(isp, INMAILBOX5, optr);
			continue;
		}
		isp_destroy_handle(isp, sp->req_handle);
		if (sp->req_status_flags & RQSTF_BUS_RESET) {
			isp->isp_sendmarker |= (1 << XS_CHANNEL(xs));
		}
		if (buddaboom) {
			XS_SETERR(xs, HBA_BOTCH);
		}
		XS_STS(xs) = sp->req_scsi_status & 0xff;
		if (IS_SCSI(isp)) {
			if (sp->req_state_flags & RQSF_GOT_SENSE) {
				MEMCPY(XS_SNSP(xs), sp->req_sense_data,
					XS_SNSLEN(xs));
				XS_SNS_IS_VALID(xs);
			}
			/*
			 * A new synchronous rate was negotiated for this
			 * target. Mark state such that we'll go look up
			 * that which has changed later.
			 */
			if (sp->req_status_flags & RQSTF_NEGOTIATION) {
				sdparam *sdp = isp->isp_param;
				sdp += XS_CHANNEL(xs);
				sdp->isp_devparam[XS_TGT(xs)].dev_refresh = 1;
				isp->isp_update |= (1 << XS_CHANNEL(xs));
			}
		} else {
			if (XS_STS(xs) == SCSI_CHECK) {
				XS_SNS_IS_VALID(xs);
				MEMCPY(XS_SNSP(xs), sp->req_sense_data,
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
		} else if (sp->req_header.rqs_entry_type == RQSTYPE_REQUEST) {
			if (sp->req_header.rqs_flags & RQSFLAG_FULL) {
				/*
				 * Force Queue Full status.
				 */
				XS_STS(xs) = SCSI_QFULL;
				XS_SETERR(xs, HBA_NOERROR);
			} else if (XS_NOERR(xs)) {
				XS_SETERR(xs, HBA_BOTCH);
			}
		} else {
			PRINTF("%s: unhandled respose queue type 0x%x\n",
			    isp->isp_name, sp->req_header.rqs_entry_type);
			if (XS_NOERR(xs)) {
				XS_SETERR(xs, HBA_BOTCH);
			}
		}
		if (IS_SCSI(isp)) {
			XS_RESID(xs) = sp->req_resid;
		} else if (sp->req_scsi_status & RQCS_RU) {
			XS_RESID(xs) = sp->req_resid;
			IDPRINTF(4, ("%s: cnt %d rsd %d\n", isp->isp_name,
				XS_XFRLEN(xs), sp->req_resid));
		}
		if (XS_XFRLEN(xs)) {
			ISP_DMAFREE(isp, xs, sp->req_handle);
		}
		/*
		 * XXX: If we have a check condition, but no Sense Data,
		 * XXX: mark it as an error (ARQ failed). We need to
		 * XXX: to do a more distinct job because there may
		 * XXX: cases where ARQ is disabled.
		 */
		if (XS_STS(xs) == SCSI_CHECK && !(XS_IS_SNS_VALID(xs))) {
			if (XS_NOERR(xs)) {
				PRINTF("%s: ARQ failure for target %d lun %d\n",
				    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
				XS_SETERR(xs, HBA_ARQFAIL);
			}
		}
		if ((isp->isp_dblev >= 5) ||
		    (isp->isp_dblev > 2 && !XS_NOERR(xs))) {
			PRINTF("%s(%d.%d): FIN dl%d resid%d STS %x",
			    isp->isp_name, XS_TGT(xs), XS_LUN(xs),
			    XS_XFRLEN(xs), XS_RESID(xs), XS_STS(xs));
			if (sp->req_state_flags & RQSF_GOT_SENSE) {
				PRINTF(" Skey: %x", XS_SNSKEY(xs));
				if (!(XS_IS_SNS_VALID(xs))) {
					PRINTF(" BUT NOT SET");
				}
			}
			PRINTF(" XS_ERR=0x%x\n", (unsigned int) XS_ERR(xs));
		}

		if (isp->isp_nactive > 0)
		    isp->isp_nactive--;
		complist[ndone++] = xs;	/* defer completion call until later */
	}

	/*
	 * If we looked at any commands, then it's valid to find out
	 * what the outpointer is. It also is a trigger to update the
	 * ISP's notion of what we've seen so far.
	 */
	if (nlooked) {
		ISP_WRITE(isp, INMAILBOX5, optr);
		isp->isp_reqodx = ISP_READ(isp, OUTMAILBOX4);
	}
	isp->isp_residx = optr;
	for (i = 0; i < ndone; i++) {
		xs = complist[i];
		if (xs) {
			XS_CMD_DONE(xs);
		}
	}
	ENABLE_INTS(isp);
	return (1);
}

/*
 * Support routines.
 */

static int
isp_parse_async(isp, mbox)
	struct ispsoftc *isp;
	int mbox;
{
	u_int32_t fast_post_handle = 0;

	switch (mbox) {
	case MBOX_COMMAND_COMPLETE:	/* sometimes these show up */
		break;
	case ASYNC_BUS_RESET:
	{
		int bus;
		if (IS_1080(isp) || IS_12X0(isp)) {
			bus = ISP_READ(isp, OUTMAILBOX6);
		} else {
			bus = 0;
		}
		isp->isp_sendmarker = (1 << bus);
		isp_async(isp, ISPASYNC_BUS_RESET, &bus);
#ifdef	ISP_TARGET_MODE
		isp_notify_ack(isp, NULL);
#endif
		break;
	}
	case ASYNC_SYSTEM_ERROR:
		mbox = ISP_READ(isp, OUTMAILBOX1);
		PRINTF("%s: Internal FW Error @ RISC Addr 0x%x\n",
		    isp->isp_name, mbox);
		isp_restart(isp);
		/* no point continuing after this */
		return (-1);

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
#ifdef	ISP_TARGET_MODE
		isp_notify_ack(isp, NULL);
#endif
		break;

	case ASYNC_DEVICE_RESET:
		/*
		 * XXX: WHICH BUS?
		 */
		isp->isp_sendmarker = 1;
		PRINTF("%s: device reset\n", isp->isp_name);
#ifdef	ISP_TARGET_MODE
		isp_notify_ack(isp, NULL);
#endif
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
		/*
		 * XXX: WHICH BUS?
		 */
		mbox = ISP_READ(isp, OUTMAILBOX2);
		switch (mbox & 0x1c00) {
		case SXP_PINS_LVD_MODE:
			PRINTF("%s: Transition to LVD mode\n", isp->isp_name);
			((sdparam *)isp->isp_param)->isp_diffmode = 0;
			((sdparam *)isp->isp_param)->isp_ultramode = 0;
			((sdparam *)isp->isp_param)->isp_lvdmode = 1;
			break;
		case SXP_PINS_HVD_MODE:
			PRINTF("%s: Transition to Differential mode\n",
			    isp->isp_name);
			((sdparam *)isp->isp_param)->isp_diffmode = 1;
			((sdparam *)isp->isp_param)->isp_ultramode = 0;
			((sdparam *)isp->isp_param)->isp_lvdmode = 0;
			break;
		case SXP_PINS_SE_MODE:
			PRINTF("%s: Transition to Single Ended mode\n",
			    isp->isp_name);
			((sdparam *)isp->isp_param)->isp_diffmode = 0;
			((sdparam *)isp->isp_param)->isp_ultramode = 1;
			((sdparam *)isp->isp_param)->isp_lvdmode = 0;
			break;
		default:
			PRINTF("%s: Transition to unknown mode 0x%x\n",
			    isp->isp_name, mbox);
			break;
		}
		/*
		 * XXX: Set up to renegotiate again!
		 */
		/* Can only be for a 1080... */
		isp->isp_sendmarker = (1 << ISP_READ(isp, OUTMAILBOX6));
		break;

	case ASYNC_CMD_CMPLT:
		fast_post_handle = (ISP_READ(isp, OUTMAILBOX2) << 16) |
		    ISP_READ(isp, OUTMAILBOX1);
		IDPRINTF(3, ("%s: fast post completion of %u\n", isp->isp_name,
		    fast_post_handle));
		break;

	case ASYNC_CTIO_DONE:
		/* Should only occur when Fast Posting Set for 2100s */
		PRINTF("%s: CTIO done\n", isp->isp_name);
		break;

	case ASYNC_LIP_OCCURRED:
		((fcparam *) isp->isp_param)->isp_lipseq =
		    ISP_READ(isp, OUTMAILBOX1);
		((fcparam *) isp->isp_param)->isp_fwstate = FW_CONFIG_WAIT;
		((fcparam *) isp->isp_param)->isp_loopstate = LOOP_LIP_RCVD;
		isp->isp_sendmarker = 1;
		isp_mark_getpdb_all(isp);
		IDPRINTF(1, ("%s: LIP occurred\n", isp->isp_name));
		break;

	case ASYNC_LOOP_UP:
		isp->isp_sendmarker = 1;
		((fcparam *) isp->isp_param)->isp_fwstate = FW_CONFIG_WAIT;
		((fcparam *) isp->isp_param)->isp_loopstate = LOOP_LIP_RCVD;
		isp_mark_getpdb_all(isp);
		isp_async(isp, ISPASYNC_LOOP_UP, NULL);
		break;

	case ASYNC_LOOP_DOWN:
		isp->isp_sendmarker = 1;
		((fcparam *) isp->isp_param)->isp_fwstate = FW_CONFIG_WAIT;
		((fcparam *) isp->isp_param)->isp_loopstate = LOOP_NIL;
		isp_mark_getpdb_all(isp);
		isp_async(isp, ISPASYNC_LOOP_DOWN, NULL);
		break;

	case ASYNC_LOOP_RESET:
		isp->isp_sendmarker = 1;
		((fcparam *) isp->isp_param)->isp_fwstate = FW_CONFIG_WAIT;
		((fcparam *) isp->isp_param)->isp_loopstate = LOOP_NIL;
		isp_mark_getpdb_all(isp);
		PRINTF("%s: Loop RESET\n", isp->isp_name);
#ifdef	ISP_TARGET_MODE
		isp_notify_ack(isp, NULL);
#endif
		break;

	case ASYNC_PDB_CHANGED:
		isp->isp_sendmarker = 1;
		((fcparam *) isp->isp_param)->isp_loopstate = LOOP_PDB_RCVD;
		isp_mark_getpdb_all(isp);
		IDPRINTF(2, ("%s: Port Database Changed\n", isp->isp_name));
		break;

	case ASYNC_CHANGE_NOTIFY:
		isp_mark_getpdb_all(isp);
		/*
		 * Not correct, but it will force us to rescan the loop.
		 */
		((fcparam *) isp->isp_param)->isp_loopstate = LOOP_PDB_RCVD;
		isp_async(isp, ISPASYNC_CHANGE_NOTIFY, NULL);
		break;

	default:
		PRINTF("%s: unknown async code 0x%x\n", isp->isp_name, mbox);
		break;
	}
	return (fast_post_handle);
}

static int
isp_handle_other_response(isp, sp, optrp)
	struct ispsoftc *isp;
	ispstatusreq_t *sp;
	u_int8_t *optrp;
{
	switch (sp->req_header.rqs_entry_type) {
	case RQSTYPE_ATIO:
	case RQSTYPE_CTIO0:
	case RQSTYPE_ENABLE_LUN:
	case RQSTYPE_MODIFY_LUN:
	case RQSTYPE_NOTIFY:
	case RQSTYPE_NOTIFY_ACK:
	case RQSTYPE_CTIO1:
	case RQSTYPE_ATIO2:
	case RQSTYPE_CTIO2:
	case RQSTYPE_CTIO3:
#ifdef	ISP_TARGET_MODE
		return(isp_target_notify(isp, sp, optrp));
#else
		/* FALLTHROUGH */
#endif
	case RQSTYPE_REQUEST:
	default:
		return (-1);
	}
}

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
		/*
		 * XXX: Get port number for bus
		 */
		isp->isp_sendmarker = 3;
		XS_SETERR(xs, HBA_BUSRESET);
		return;

	case RQCS_ABORTED:
		PRINTF("%s: command aborted for target %d lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		/*
		 * XXX: Get port number for bus
		 */
		isp->isp_sendmarker = 3;
		XS_SETERR(xs, HBA_ABORTED);
		return;

	case RQCS_TIMEOUT:
		IDPRINTF(2, ("%s: command timed out for target %d lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs)));
		XS_SETERR(xs, HBA_CMDTIMEOUT);
		return;

	case RQCS_DATA_OVERRUN:
		if (IS_FC(isp)) {
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
		PRINTF("%s: target %d lun %d had an unexpected bus free\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_DATA_UNDERRUN:
		if (IS_FC(isp)) {
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
		IDPRINTF(3, ("%s: internal queues full for target %d lun %d "
		    "status 0x%x\n", isp->isp_name, XS_TGT(xs), XS_LUN(xs),
		    XS_STS(xs)));
		/*
		 * If QFULL or some other status byte is set, then this
		 * isn't an error, per se.
		 */
		if (XS_STS(xs) != 0) {
			XS_SETERR(xs, HBA_NOERROR);
			return;
		}
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
		if (IS_SCSI(isp)) {
			sdparam *sdp = isp->isp_param;
			sdp += XS_CHANNEL(xs);
			sdp->isp_devparam[XS_TGT(xs)].dev_flags &= ~DPARM_WIDE;
			sdp->isp_devparam[XS_TGT(xs)].dev_update = 1;
			isp->isp_update = XS_CHANNEL(xs)+1;
		}
		XS_SETERR(xs, HBA_NOERROR);
		return;

	case RQCS_SYNCXFER_FAILED:
		PRINTF("%s: SDTR Message failed for target %d lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		if (IS_SCSI(isp)) {
			sdparam *sdp = isp->isp_param;
			sdp += XS_CHANNEL(xs);
			sdp->isp_devparam[XS_TGT(xs)].dev_flags &= ~DPARM_SYNC;
			sdp->isp_devparam[XS_TGT(xs)].dev_update = 1;
			isp->isp_update = XS_CHANNEL(xs)+1;
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
		IDPRINTF(2, ("%s: port logout for target %d\n",
			isp->isp_name, XS_TGT(xs)));
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

static void
isp_fastpost_complete(isp, fph)
	struct ispsoftc *isp;
	u_int32_t fph;
{
	ISP_SCSI_XFER_T *xs;

	if (fph < 1) {
		return;
	}
	xs = isp_find_xs(isp, fph);
	if (xs == NULL) {
		PRINTF("%s: command for fast posting handle 0x%x not found\n",
		    isp->isp_name, fph);
		return;
	}
	isp_destroy_handle(isp, fph);

	/*
	 * Since we don't have a result queue entry item,
	 * we must believe that SCSI status is zero and
	 * that all data transferred.
	 */
	XS_RESID(xs) = 0;
	XS_STS(xs) = 0;
	if (XS_XFRLEN(xs)) {
		ISP_DMAFREE(isp, xs, fph);
	}
	XS_CMD_DONE(xs);
	if (isp->isp_nactive)
		isp->isp_nactive--;
}

#define	HINIB(x)			((x) >> 0x4)
#define	LONIB(x)			((x)  & 0xf)
#define	MAKNIB(a, b)			(((a) << 4) | (b))
static u_int8_t mbpcnt[] = {
	MAKNIB(1, 1),	/* 0x00: MBOX_NO_OP */
	MAKNIB(5, 5),	/* 0x01: MBOX_LOAD_RAM */
	MAKNIB(2, 0),	/* 0x02: MBOX_EXEC_FIRMWARE */
	MAKNIB(5, 5),	/* 0x03: MBOX_DUMP_RAM */
	MAKNIB(3, 3),	/* 0x04: MBOX_WRITE_RAM_WORD */
	MAKNIB(2, 3),	/* 0x05: MBOX_READ_RAM_WORD */
	MAKNIB(6, 6),	/* 0x06: MBOX_MAILBOX_REG_TEST */
	MAKNIB(2, 3),	/* 0x07: MBOX_VERIFY_CHECKSUM	*/
	MAKNIB(1, 4),	/* 0x08: MBOX_ABOUT_FIRMWARE */
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
	MAKNIB(3, 1),	/* 0x18: MBOX_BUS_RESET */
	MAKNIB(2, 3),	/* 0x19: MBOX_STOP_QUEUE */
	MAKNIB(2, 3),	/* 0x1a: MBOX_START_QUEUE */
	MAKNIB(2, 3),	/* 0x1b: MBOX_SINGLE_STEP_QUEUE */
	MAKNIB(2, 3),	/* 0x1c: MBOX_ABORT_QUEUE */
	MAKNIB(2, 4),	/* 0x1d: MBOX_GET_DEV_QUEUE_STATUS */
	MAKNIB(0, 0),	/* 0x1e: */
	MAKNIB(1, 3),	/* 0x1f: MBOX_GET_FIRMWARE_STATUS */
	MAKNIB(1, 4),	/* 0x20: MBOX_GET_INIT_SCSI_ID, MBOX_GET_LOOP_ID */
	MAKNIB(1, 3),	/* 0x21: MBOX_GET_SELECT_TIMEOUT */
	MAKNIB(1, 3),	/* 0x22: MBOX_GET_RETRY_COUNT	*/
	MAKNIB(1, 2),	/* 0x23: MBOX_GET_TAG_AGE_LIMIT */
	MAKNIB(1, 2),	/* 0x24: MBOX_GET_CLOCK_RATE */
	MAKNIB(1, 2),	/* 0x25: MBOX_GET_ACT_NEG_STATE */
	MAKNIB(1, 2),	/* 0x26: MBOX_GET_ASYNC_DATA_SETUP_TIME */
	MAKNIB(1, 3),	/* 0x27: MBOX_GET_PCI_PARAMS */
	MAKNIB(2, 4),	/* 0x28: MBOX_GET_TARGET_PARAMS */
	MAKNIB(2, 4),	/* 0x29: MBOX_GET_DEV_QUEUE_PARAMS */
	MAKNIB(1, 2),	/* 0x2a: MBOX_GET_RESET_DELAY_PARAMS */
	MAKNIB(0, 0),	/* 0x2b: */
	MAKNIB(0, 0),	/* 0x2c: */
	MAKNIB(0, 0),	/* 0x2d: */
	MAKNIB(0, 0),	/* 0x2e: */
	MAKNIB(0, 0),	/* 0x2f: */
	MAKNIB(2, 2),	/* 0x30: MBOX_SET_INIT_SCSI_ID */
	MAKNIB(2, 3),	/* 0x31: MBOX_SET_SELECT_TIMEOUT */
	MAKNIB(3, 3),	/* 0x32: MBOX_SET_RETRY_COUNT	*/
	MAKNIB(2, 2),	/* 0x33: MBOX_SET_TAG_AGE_LIMIT */
	MAKNIB(2, 2),	/* 0x34: MBOX_SET_CLOCK_RATE */
	MAKNIB(2, 2),	/* 0x35: MBOX_SET_ACT_NEG_STATE */
	MAKNIB(2, 2),	/* 0x36: MBOX_SET_ASYNC_DATA_SETUP_TIME */
	MAKNIB(3, 3),	/* 0x37: MBOX_SET_PCI_CONTROL_PARAMS */
	MAKNIB(4, 4),	/* 0x38: MBOX_SET_TARGET_PARAMS */
	MAKNIB(4, 4),	/* 0x39: MBOX_SET_DEV_QUEUE_PARAMS */
	MAKNIB(1, 2),	/* 0x3a: MBOX_SET_RESET_DELAY_PARAMS */
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
	MAKNIB(2, 1),	/* 0x4a: MBOX_SET_FIRMWARE_FEATURES */
	MAKNIB(1, 2),	/* 0x4b: MBOX_GET_FIRMWARE_FEATURES */
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
	MAKNIB(0, 0),	/* 0x61: */
	MAKNIB(2, 1),	/* 0x62: MBOX_INIT_LIP */
	MAKNIB(8, 1),	/* 0x63: MBOX_GET_FC_AL_POSITION_MAP */
	MAKNIB(8, 1),	/* 0x64: MBOX_GET_PORT_DB */
	MAKNIB(3, 1),	/* 0x65: MBOX_CLEAR_ACA */
	MAKNIB(3, 1),	/* 0x66: MBOX_TARGET_RESET */
	MAKNIB(3, 1),	/* 0x67: MBOX_CLEAR_TASK_SET */
	MAKNIB(3, 1),	/* 0x68: MBOX_ABORT_TASK_SET */
	MAKNIB(1, 2),	/* 0x69: MBOX_GET_FW_STATE */
	MAKNIB(2, 8),	/* 0x6a: MBOX_GET_PORT_NAME */
	MAKNIB(8, 1),	/* 0x6b: MBOX_GET_LINK_STATUS */
	MAKNIB(4, 4),	/* 0x6c: MBOX_INIT_LIP_RESET */
	MAKNIB(0, 0),	/* 0x6d: */
	MAKNIB(8, 1),	/* 0x6e: MBOX_SEND_SNS */
	MAKNIB(4, 3),	/* 0x6f: MBOX_FABRIC_LOGIN */
	MAKNIB(2, 1),	/* 0x70: MBOX_SEND_CHANGE_REQUEST */
	MAKNIB(2, 1),	/* 0x71: MBOX_FABRIC_LOGOUT */
	MAKNIB(4, 1)	/* 0x72: MBOX_INIT_LIP_LOGIN */
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


	/*
	 * Check for variants
	 */
#ifdef	ISP2100_SCCLUN
	if (IS_FC(isp)) {
		switch (mbp->param[0]) {
		case MBOX_ABORT:
			inparam = 7;
			break;
		case MBOX_ABORT_DEVICE:
		case MBOX_START_QUEUE:
		case MBOX_STOP_QUEUE:
		case MBOX_SINGLE_STEP_QUEUE:
		case MBOX_ABORT_QUEUE:
		case MBOX_GET_DEV_QUEUE_STATUS:
			inparam = 3;
			break;
		case MBOX_BUS_RESET:
			inparam = 2;
			break;
		default:
			break;
		}
	}
#endif

command_known:

	/*
	 * Set semaphore on mailbox registers to win any races to acquire them.
	 */
	ISP_WRITE(isp, BIU_SEMA, 1);

	/*
	 * Qlogic Errata for the ISP2100 says that there is a necessary
	 * debounce between between writing the semaphore register
	 * and reading a mailbox register. I believe we're okay here.
	 */

	/*
	 * Make sure we can send some words.
	 * Check to see if there's an async mbox event pending.
	 */

	loops = MBOX_DELAY_COUNT;
	while ((ISP_READ(isp, HCCR) & HCCR_HOST_INT) != 0) {
		if (ISP_READ(isp, BIU_SEMA) & 1) {
			int fph;
			u_int16_t mbox = ISP_READ(isp, OUTMAILBOX0);
			/*
			 * We have a pending MBOX async event.
			 */
			if (mbox & 0x8000) {
				fph = isp_parse_async(isp, (int) mbox);
				IDPRINTF(5, ("%s: line %d, fph %d\n",
				    isp->isp_name, __LINE__, fph));
				ISP_WRITE(isp, BIU_SEMA, 0);
				ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
				if (fph < 0) {
					return;
				} else if (fph > 0) {
					isp_fastpost_complete(isp, fph);
				}
				SYS_DELAY(100);
				goto command_known;
			}
			/*
			 * We have a pending MBOX completion? Might be
			 * from a previous command. We can't (sometimes)
			 * just clear HOST INTERRUPT, so we'll just silently
			 * eat this here.
			 */
			if (mbox & 0x4000) {
				IDPRINTF(5, ("%s: line %d, mbox 0x%x\n",
				    isp->isp_name, __LINE__, mbox));
				ISP_WRITE(isp, BIU_SEMA, 0);
				ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
				SYS_DELAY(100);
				goto command_known;
			}
		}
		SYS_DELAY(100);
		if (--loops < 0) {
			if (dld++ > 10) {
				PRINTF("%s: isp_mboxcmd could not get command "
				    "started\n", isp->isp_name);
				return;
			}
			ISP_WRITE(isp, BIU_SEMA, 0);
			ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
			goto command_known;
		}
	}

	/*
	 * Write input parameters.
	 *
	 * Special case some of the setups for the dual port SCSI cards.
	 * XXX Eventually will be fixed by converting register write/read
	 * XXX counts to bitmasks.
	 */
	if (IS_12X0(isp)) {
		switch (opcode) {
		case MBOX_GET_RETRY_COUNT:
		case MBOX_SET_RETRY_COUNT:
			ISP_WRITE(isp, INMAILBOX7, mbp->param[7]);
			mbp->param[7] = 0;
			ISP_WRITE(isp, INMAILBOX6, mbp->param[6]);
			mbp->param[6] = 0;
			break;
		case MBOX_SET_ASYNC_DATA_SETUP_TIME:
		case MBOX_SET_ACT_NEG_STATE:
		case MBOX_SET_TAG_AGE_LIMIT:
		case MBOX_SET_SELECT_TIMEOUT:
			ISP_WRITE(isp, INMAILBOX2, mbp->param[2]);
			mbp->param[2] = 0;
			break;
		}
	}

	switch (inparam) {
	case 8: ISP_WRITE(isp, INMAILBOX7, mbp->param[7]); mbp->param[7] = 0;
	case 7: ISP_WRITE(isp, INMAILBOX6, mbp->param[6]); mbp->param[6] = 0;
	case 6:
		/*
		 * The Qlogic 2100 cannot have registers 4 and 5 written to
		 * after initialization or BAD THINGS HAPPEN (tm).
		 */
		if (IS_SCSI(isp) || mbp->param[0] == MBOX_INIT_FIRMWARE)
			ISP_WRITE(isp, INMAILBOX5, mbp->param[5]);
		mbp->param[5] = 0;
	case 5:
		if (IS_SCSI(isp) || mbp->param[0] == MBOX_INIT_FIRMWARE)
			ISP_WRITE(isp, INMAILBOX4, mbp->param[4]);
		mbp->param[4] = 0;
	case 4: ISP_WRITE(isp, INMAILBOX3, mbp->param[3]); mbp->param[3] = 0;
	case 3: ISP_WRITE(isp, INMAILBOX2, mbp->param[2]); mbp->param[2] = 0;
	case 2: ISP_WRITE(isp, INMAILBOX1, mbp->param[1]); mbp->param[1] = 0;
	case 1: ISP_WRITE(isp, INMAILBOX0, mbp->param[0]); mbp->param[0] = 0;
	}

	/*
	 * Clear RISC int condition.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);

	/*
	 * Clear semaphore on mailbox registers so that the Qlogic
	 * may update outgoing registers.
	 */
	ISP_WRITE(isp, BIU_SEMA, 0);

	/*
	 * Set Host Interrupt condition so that RISC will pick up mailbox regs.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_SET_HOST_INT);

	/*
	 * Wait until HOST INT has gone away (meaning that the Qlogic
	 * has picked up the mailbox command. Wait a long time.
	 */
	loops = MBOX_DELAY_COUNT * 5;
	while ((ISP_READ(isp, HCCR) & HCCR_CMD_CLEAR_RISC_INT) != 0) {
		SYS_DELAY(100);
		if (--loops < 0) {
			PRINTF("%s: isp_mboxcmd timeout #2\n", isp->isp_name);
			return;
		}
	}

	/*
	 * While the Semaphore registers isn't set, wait for the Qlogic
	 * to process the mailbox command. Again- wait a long time.
	 */
	loops = MBOX_DELAY_COUNT * 5;
	while ((ISP_READ(isp, BIU_SEMA) & 1) == 0) {
		SYS_DELAY(100);
		/*
		 * Wierd- I've seen the case where the semaphore register
		 * isn't getting set- sort of a violation of the protocol..
		 */
		if (ISP_READ(isp, OUTMAILBOX0) & 0x4000)
			break;
		if (--loops < 0) {
			PRINTF("%s: isp_mboxcmd timeout #3\n", isp->isp_name);
			return;
		}
	}

	/*
	 * Make sure that the MBOX_BUSY has gone away
	 */
	loops = MBOX_DELAY_COUNT;
	for (;;) {
		u_int16_t mbox = ISP_READ(isp, OUTMAILBOX0);
		if (mbox == MBOX_BUSY) {
			if (--loops < 0) {
				PRINTF("%s: isp_mboxcmd timeout #4\n",
				    isp->isp_name);
				return;
			}
			SYS_DELAY(100);
			continue;
		}
		/*
		 * We have a pending MBOX async event.
		 */
		if (mbox & 0x8000) {
			int fph = isp_parse_async(isp, (int) mbox);
			ISP_WRITE(isp, BIU_SEMA, 0);
			ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
			if (fph < 0) {
				return;
			} else if (fph > 0) {
				isp_fastpost_complete(isp, fph);
			}
			SYS_DELAY(100);
			continue;
		}
		break;
	}

	/*
	 * Pick up output parameters. Special case some of the readbacks
	 * for the dual port SCSI cards.
	 */
	if (IS_12X0(isp)) {
		switch (opcode) {
		case MBOX_GET_RETRY_COUNT:
		case MBOX_SET_RETRY_COUNT:
			mbp->param[7] = ISP_READ(isp, OUTMAILBOX7);
			mbp->param[6] = ISP_READ(isp, OUTMAILBOX6);
			break;
		case MBOX_GET_TAG_AGE_LIMIT:
		case MBOX_SET_TAG_AGE_LIMIT:
		case MBOX_GET_ACT_NEG_STATE:
		case MBOX_SET_ACT_NEG_STATE:
		case MBOX_SET_ASYNC_DATA_SETUP_TIME:
		case MBOX_GET_ASYNC_DATA_SETUP_TIME:
		case MBOX_GET_RESET_DELAY_PARAMS:
		case MBOX_SET_RESET_DELAY_PARAMS:
			mbp->param[2] = ISP_READ(isp, OUTMAILBOX2);
			break;
		}
	}

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
	switch (mbp->param[0]) {
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
		if (opcode != MBOX_ABOUT_FIRMWARE)
		    PRINTF("%s: mbox cmd %x failed with COMMAND_ERROR\n",
			isp->isp_name, opcode);
		break;
	case MBOX_COMMAND_PARAM_ERROR:
		switch (opcode) {
		case MBOX_GET_PORT_DB:
		case MBOX_GET_PORT_NAME:
		case MBOX_GET_DEV_QUEUE_PARAMS:
			break;
		default:
			PRINTF("%s: mbox cmd %x failed with "
			    "COMMAND_PARAM_ERROR\n", isp->isp_name, opcode);
		}
		break;

	/*
	 * Be silent about these...
	 */
	case ASYNC_PDB_CHANGED:
		((fcparam *) isp->isp_param)->isp_loopstate = LOOP_PDB_RCVD;
		break;

	case ASYNC_LIP_OCCURRED:
		((fcparam *) isp->isp_param)->isp_lipseq = mbp->param[1];
		/* FALLTHROUGH */
	case ASYNC_LOOP_UP:
		((fcparam *) isp->isp_param)->isp_fwstate = FW_CONFIG_WAIT;
		((fcparam *) isp->isp_param)->isp_loopstate = LOOP_LIP_RCVD;
		break;

	case ASYNC_LOOP_DOWN:
	case ASYNC_LOOP_RESET:
		((fcparam *) isp->isp_param)->isp_fwstate = FW_CONFIG_WAIT;
		((fcparam *) isp->isp_param)->isp_loopstate = LOOP_NIL;
		/* FALLTHROUGH */
	case ASYNC_CHANGE_NOTIFY:
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
	mbs.param[1] = (XS_TGT(xs) << 8) | XS_LUN(xs); /* XXX: WHICH BUS? */
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
	if (IS_SCSI(isp))
		PRINTF("    biu_conf1=%x", ISP_READ(isp, BIU_CONF1));
	else
		PRINTF("    biu_csr=%x", ISP_READ(isp, BIU2100_CSR));
	PRINTF(" biu_icr=%x biu_isr=%x biu_sema=%x ", ISP_READ(isp, BIU_ICR),
	    ISP_READ(isp, BIU_ISR), ISP_READ(isp, BIU_SEMA));
	PRINTF("risc_hccr=%x\n", ISP_READ(isp, HCCR));


	if (IS_SCSI(isp)) {
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
	PRINTF("    mbox regs: %x %x %x %x %x\n",
	    ISP_READ(isp, OUTMAILBOX0), ISP_READ(isp, OUTMAILBOX1),
	    ISP_READ(isp, OUTMAILBOX2), ISP_READ(isp, OUTMAILBOX3),
	    ISP_READ(isp, OUTMAILBOX4));
	ISP_DUMPREGS(isp);
}

static void
isp_fw_state(isp)
	struct ispsoftc *isp;
{
	mbreg_t mbs;
	if (IS_FC(isp)) {
		int once = 0;
		fcparam *fcp = isp->isp_param;
again:
		mbs.param[0] = MBOX_GET_FW_STATE;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			IDPRINTF(0, ("%s: isp_fw_state 0x%x\n", isp->isp_name,
			    mbs.param[0]));
			switch (mbs.param[0]) {
			case ASYNC_PDB_CHANGED:
				if (once++ < 10) {
					goto again;
				}
				fcp->isp_fwstate = FW_CONFIG_WAIT;
				fcp->isp_loopstate = LOOP_PDB_RCVD;
				goto again;
			case ASYNC_LIP_OCCURRED:
				fcp->isp_lipseq = mbs.param[1];
				/* FALLTHROUGH */
			case ASYNC_LOOP_UP:
				fcp->isp_fwstate = FW_CONFIG_WAIT;
				fcp->isp_loopstate = LOOP_LIP_RCVD;
				if (once++ < 10) {
					goto again;
				}
				break;
			case ASYNC_LOOP_RESET:
			case ASYNC_LOOP_DOWN:
				fcp->isp_fwstate = FW_CONFIG_WAIT;
				fcp->isp_loopstate = LOOP_NIL;
				/* FALLTHROUGH */
			case ASYNC_CHANGE_NOTIFY:
				if (once++ < 10) {
					goto again;
				}
				break;
			}
			PRINTF("%s: GET FIRMWARE STATE failed (0x%x)\n",
			    isp->isp_name, mbs.param[0]);
			return;
		}
		fcp->isp_fwstate = mbs.param[1];
	}
}

static void
isp_update(isp)
	struct ispsoftc *isp;
{
	int bus;

	for (bus = 0; isp->isp_update != 0; bus++) {
		if (isp->isp_update & (1 << bus)) {
			isp_update_bus(isp, bus);
			isp->isp_update ^= (1 << bus);
		}
	}
}

static void
isp_update_bus(isp, bus)
	struct ispsoftc *isp;
	int bus;
{
	int tgt;
	mbreg_t mbs;
	sdparam *sdp;

	if (IS_FC(isp)) {
		return;
	}

	sdp = isp->isp_param;
	sdp += bus;

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		u_int16_t flags, period, offset;
		int get;

		if (sdp->isp_devparam[tgt].dev_enable == 0) {
			IDPRINTF(1, ("%s: skipping target %d bus %d update\n",
			    isp->isp_name, tgt, bus));
			continue;
		}

		/*
		 * If the goal is to update the status of the device,
		 * take what's in dev_flags and try and set the device
		 * toward that. Otherwise, if we're just refreshing the
		 * current device state, get the current parameters.
		 */
		if (sdp->isp_devparam[tgt].dev_update) {
			mbs.param[0] = MBOX_SET_TARGET_PARAMS;
			mbs.param[2] = sdp->isp_devparam[tgt].dev_flags;
			/*
			 * Insist that PARITY must be enabled if SYNC
			 * is enabled.
			 */
			if (mbs.param[2] & DPARM_SYNC) {
				mbs.param[2] |= DPARM_PARITY;
			}
			mbs.param[3] =
				(sdp->isp_devparam[tgt].sync_offset << 8) |
				(sdp->isp_devparam[tgt].sync_period);
			sdp->isp_devparam[tgt].dev_update = 0;
			/*
			 * A command completion later that has
			 * RQSTF_NEGOTIATION set will cause
			 * the dev_refresh/announce cycle.
			 *
			 * Note: It is really important to update our current
			 * flags with at least the state of TAG capabilities-
			 * otherwise we might try and send a tagged command
			 * when we have it all turned off. So change it here
			 * to say that current already matches goal.
			 */
			sdp->isp_devparam[tgt].cur_dflags &= ~DPARM_TQING;
			sdp->isp_devparam[tgt].cur_dflags |=
			    (sdp->isp_devparam[tgt].dev_flags & DPARM_TQING);
			sdp->isp_devparam[tgt].dev_refresh = 1;
			IDPRINTF(3, ("%s: bus %d set tgt %d flags 0x%x off 0x%x"
			    " period 0x%x\n", isp->isp_name, bus, tgt,
			    mbs.param[2], mbs.param[3] >> 8,
			    mbs.param[3] & 0xff));
			get = 0;
		} else if (sdp->isp_devparam[tgt].dev_refresh) {
			mbs.param[0] = MBOX_GET_TARGET_PARAMS;
			sdp->isp_devparam[tgt].dev_refresh = 0;
			get = 1;
		} else {
			continue;
		}
		mbs.param[1] = (bus << 15) | (tgt << 8) ;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			PRINTF("%s: failed to %cet SCSI parameters for "
			    "target %d\n", isp->isp_name, (get)? 'g' : 's',
			    tgt);
			continue;
		}
		if (get == 0) {
			isp->isp_sendmarker |= (1 << bus);
			continue;
		}
		flags = mbs.param[2];
		period = mbs.param[3] & 0xff;
		offset = mbs.param[3] >> 8;
		sdp->isp_devparam[tgt].cur_dflags = flags;
		sdp->isp_devparam[tgt].cur_period = period;
		sdp->isp_devparam[tgt].cur_offset = offset;
		get = (bus << 16) | tgt;
		(void) isp_async(isp, ISPASYNC_NEW_TGT_PARAMS, &get);
	}
}

static void
isp_setdfltparm(isp, channel)
	struct ispsoftc *isp;
	int channel;
{
	int tgt;
	mbreg_t mbs;
	sdparam *sdp, *sdp_chan0, *sdp_chan1;

	if (IS_FC(isp)) {
		fcparam *fcp = (fcparam *) isp->isp_param;
		fcp += channel;
		if (fcp->isp_gotdparms) {
			return;
		}
		fcp->isp_gotdparms = 1;
		fcp->isp_maxfrmlen = ICB_DFLT_FRMLEN;
		fcp->isp_maxalloc = ICB_DFLT_ALLOC;
		fcp->isp_execthrottle = ICB_DFLT_THROTTLE;
		fcp->isp_retry_delay = ICB_DFLT_RDELAY;
		fcp->isp_retry_count = ICB_DFLT_RCOUNT;
		/* Platform specific.... */
		fcp->isp_loopid = DEFAULT_LOOPID(isp);
		fcp->isp_nodewwn = DEFAULT_WWN(isp);
		fcp->isp_portwwn = DEFAULT_WWN(isp);
		/*
		 * Now try and read NVRAM
		 */
		if ((isp->isp_confopts & ISP_CFG_NONVRAM) == 0) {
			if (isp_read_nvram(isp)) {
				PRINTF("%s: using default WWN 0x%08x%08x\n",
				    isp->isp_name,
				    (u_int32_t)(fcp->isp_portwwn >> 32),
				    (u_int32_t)(fcp->isp_portwwn & 0xffffffff));
			}
		}
		return;
	}

	sdp_chan0 = (sdparam *) isp->isp_param;
	sdp_chan1 = sdp_chan0 + 1;
	sdp = sdp_chan0 + channel;

	/*
	 * Been there, done that, got the T-shirt...
	 */
	if (sdp->isp_gotdparms) {
		return;
	}
	sdp->isp_gotdparms = 1;

	/*
	 * If we've not been told to avoid reading NVRAM, try and read it.
	 * If we're successful reading it, we can return since NVRAM will
	 * tell us the right thing to do. Otherwise, establish some reasonable
	 * defaults.
	 */
	if ((isp->isp_confopts & ISP_CFG_NONVRAM) == 0) {
		if (isp_read_nvram(isp) == 0) {
			return;
		}
	}

	/*
	 * Now try and see whether we have specific values for them.
	 */
	mbs.param[0] = MBOX_GET_ACT_NEG_STATE;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		IDPRINTF(2, ("could not GET ACT NEG STATE\n"));
		sdp_chan0->isp_req_ack_active_neg = 1;
		sdp_chan0->isp_data_line_active_neg = 1;
		if (IS_12X0(isp)) {
			sdp_chan1->isp_req_ack_active_neg = 1;
			sdp_chan1->isp_data_line_active_neg = 1;
		}
	} else {
		sdp_chan0->isp_req_ack_active_neg = (mbs.param[1] >> 4) & 0x1;
		sdp_chan0->isp_data_line_active_neg = (mbs.param[1] >> 5) & 0x1;
		if (IS_12X0(isp)) {
			sdp_chan1->isp_req_ack_active_neg =
			    (mbs.param[2] >> 4) & 0x1;
			sdp_chan1->isp_data_line_active_neg =
			    (mbs.param[2] >> 5) & 0x1;
		}
	}

	/*
	 * The trick here is to establish a default for the default (honk!)
	 * state (dev_flags). Then try and get the current status from
	 * the card to fill in the current state. We don't, in fact, set
	 * the default to the SAFE default state- that's not the goal state.
	 */
	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		sdp->isp_devparam[tgt].cur_offset = 0;
		sdp->isp_devparam[tgt].cur_period = 0;
		sdp->isp_devparam[tgt].dev_flags = DPARM_DEFAULT;
		sdp->isp_devparam[tgt].cur_dflags = 0;
		if (isp->isp_type < ISP_HA_SCSI_1040 ||
		    (isp->isp_clock && isp->isp_clock < 60)) {
			sdp->isp_devparam[tgt].sync_offset =
			    ISP_10M_SYNCPARMS >> 8;
			sdp->isp_devparam[tgt].sync_period =
			    ISP_10M_SYNCPARMS & 0xff;
		} else if (IS_1080(isp)) {
			sdp->isp_devparam[tgt].sync_offset =
			    ISP_40M_SYNCPARMS >> 8;
			sdp->isp_devparam[tgt].sync_period =
			    ISP_40M_SYNCPARMS & 0xff;
		} else {
			sdp->isp_devparam[tgt].sync_offset =
			    ISP_20M_SYNCPARMS >> 8;
			sdp->isp_devparam[tgt].sync_period =
			    ISP_20M_SYNCPARMS & 0xff;
		}

		/*
		 * Don't get current target parameters if we've been
		 * told not to use NVRAM- it's really the same thing.
		 */
		if (isp->isp_confopts & ISP_CFG_NONVRAM) {
			continue;
		}

		mbs.param[0] = MBOX_GET_TARGET_PARAMS;
		mbs.param[1] = tgt << 8;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			continue;
		}
		sdp->isp_devparam[tgt].cur_dflags = mbs.param[2];
		sdp->isp_devparam[tgt].dev_flags = mbs.param[2];
		sdp->isp_devparam[tgt].cur_period = mbs.param[3] & 0xff;
		sdp->isp_devparam[tgt].cur_offset = mbs.param[3] >> 8;

		/*
		 * The maximum period we can really see
		 * here is 100 (decimal), or 400 ns.
		 * For some unknown reason we sometimes
		 * get back wildass numbers from the
		 * boot device's parameters (alpha only).
		 */
		if ((mbs.param[3] & 0xff) <= 0x64) {
			sdp->isp_devparam[tgt].sync_period =
			    mbs.param[3] & 0xff;
			sdp->isp_devparam[tgt].sync_offset =
			    mbs.param[3] >> 8;
		}

		/*
		 * It is not safe to run Ultra Mode with a clock < 60.
		 */
		if (((isp->isp_clock && isp->isp_clock < 60) ||
		    (isp->isp_type < ISP_HA_SCSI_1020A)) &&
		    (sdp->isp_devparam[tgt].sync_period <=
		    (ISP_20M_SYNCPARMS & 0xff))) {
			sdp->isp_devparam[tgt].sync_offset =
			    ISP_10M_SYNCPARMS >> 8;
			sdp->isp_devparam[tgt].sync_period =
			    ISP_10M_SYNCPARMS & 0xff;
		}
	}

	/*
	 * Establish default some more default parameters.
	 */
	sdp->isp_cmd_dma_burst_enable = 1;
	sdp->isp_data_dma_burst_enabl = 1;
	sdp->isp_fifo_threshold = 0;
	sdp->isp_initiator_id = 7;
	/* XXXX This is probably based upon clock XXXX */
	if (isp->isp_type >= ISP_HA_SCSI_1040) {
		sdp->isp_async_data_setup = 9;
	} else {
		sdp->isp_async_data_setup = 6;
	}
	sdp->isp_selection_timeout = 250;
	sdp->isp_max_queue_depth = MAXISPREQUEST;
	sdp->isp_tag_aging = 8;
	sdp->isp_bus_reset_delay = 3;
	sdp->isp_retry_count = 2;
	sdp->isp_retry_delay = 2;

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		sdp->isp_devparam[tgt].exc_throttle = 16;
		sdp->isp_devparam[tgt].dev_enable = 1;
	}
}

/*
 * Re-initialize the ISP and complete all orphaned commands
 * with a 'botched' notice. The reset/init routines should
 * not disturb an already active list of commands.
 *
 * Locks held prior to coming here.
 */

void
isp_restart(isp)
	struct ispsoftc *isp;
{
	ISP_SCSI_XFER_T *xs;
	u_int32_t handle;

#if	0
	isp->isp_gotdparms = 0;
#endif
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
	isp->isp_nactive = 0;

	for (handle = 1; handle <= (int) isp->isp_maxcmds; handle++) {
		xs = isp_find_xs(isp, handle);
		if (xs == NULL) {
			continue;
		}
		isp_destroy_handle(isp, handle);
		if (XS_XFRLEN(xs)) {
			ISP_DMAFREE(isp, xs, handle);
			XS_RESID(xs) = XS_XFRLEN(xs);
		} else {
			XS_RESID(xs) = 0;
		}
		XS_SETERR(xs, HBA_BUSRESET);
		XS_CMD_DONE(xs);
	}
}

/*
 * NVRAM Routines
 */

static int
isp_read_nvram(isp)
	struct ispsoftc *isp;
{
	static char *tru = "true";
	static char *not = "false";
	int i, amt;
	u_int8_t csum, minversion;
	union {
		u_int8_t _x[ISP2100_NVRAM_SIZE];
		u_int16_t _s[ISP2100_NVRAM_SIZE>>1];
	} _n;
#define	nvram_data	_n._x
#define	nvram_words	_n._s

	if (IS_FC(isp)) {
		amt = ISP2100_NVRAM_SIZE;
		minversion = 1;
	} else if (IS_1080(isp) || IS_12X0(isp)) {
		amt = ISP1080_NVRAM_SIZE;
		minversion = 0;
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
			PRINTF("%s: invalid NVRAM header (%x,%x,%x,%x)\n",
			    isp->isp_name, nvram_data[0], nvram_data[1],
			    nvram_data[2], nvram_data[3]);
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

	if (IS_1080(isp) || IS_12X0(isp)) {
		int bus;
		sdparam *sdp = (sdparam *) isp->isp_param;
		for (bus = 0; bus < (IS_1080(isp)? 1 : 2); bus++, sdp++) {
			sdp->isp_fifo_threshold = 
			    ISP1080_NVRAM_FIFO_THRESHOLD(nvram_data);

			sdp->isp_initiator_id =
			    ISP1080_NVRAM_INITIATOR_ID(nvram_data, bus);

			sdp->isp_bus_reset_delay =
			    ISP1080_NVRAM_BUS_RESET_DELAY(nvram_data, bus);

			sdp->isp_retry_count =
			    ISP1080_NVRAM_BUS_RETRY_COUNT(nvram_data, bus);

			sdp->isp_retry_delay =
			    ISP1080_NVRAM_BUS_RETRY_DELAY(nvram_data, bus);

			sdp->isp_async_data_setup =
			    ISP1080_NVRAM_ASYNC_DATA_SETUP_TIME(nvram_data,
			    bus);

			sdp->isp_req_ack_active_neg =
			    ISP1080_NVRAM_REQ_ACK_ACTIVE_NEGATION(nvram_data,
			    bus);

			sdp->isp_data_line_active_neg =
			    ISP1080_NVRAM_DATA_LINE_ACTIVE_NEGATION(nvram_data,
			    bus);

			sdp->isp_data_dma_burst_enabl =
			    ISP1080_NVRAM_BURST_ENABLE(nvram_data);

			sdp->isp_cmd_dma_burst_enable =
			    ISP1080_NVRAM_BURST_ENABLE(nvram_data);

			sdp->isp_selection_timeout =
			    ISP1080_NVRAM_SELECTION_TIMEOUT(nvram_data, bus);

			sdp->isp_max_queue_depth =
			     ISP1080_NVRAM_MAX_QUEUE_DEPTH(nvram_data, bus);

			if (isp->isp_dblev >= 3) {
				PRINTF("%s: ISP1080 bus %d NVRAM values:\n",
				    isp->isp_name, bus);
				PRINTF("               Initiator ID = %d\n",
				    sdp->isp_initiator_id);
				PRINTF("             Fifo Threshold = 0x%x\n",
				    sdp->isp_fifo_threshold);
				PRINTF("            Bus Reset Delay = %d\n",
				    sdp->isp_bus_reset_delay);
				PRINTF("                Retry Count = %d\n",
				    sdp->isp_retry_count);
				PRINTF("                Retry Delay = %d\n",
				    sdp->isp_retry_delay);
				PRINTF("              Tag Age Limit = %d\n",
				    sdp->isp_tag_aging);
				PRINTF("          Selection Timeout = %d\n",
				    sdp->isp_selection_timeout);
				PRINTF("            Max Queue Depth = %d\n",
				    sdp->isp_max_queue_depth);
				PRINTF("           Async Data Setup = 0x%x\n",
				    sdp->isp_async_data_setup);
				PRINTF("    REQ/ACK Active Negation = %s\n",
				    sdp->isp_req_ack_active_neg? tru : not);
				PRINTF("  Data Line Active Negation = %s\n",
				    sdp->isp_data_line_active_neg? tru : not);
				PRINTF("       Cmd DMA Burst Enable = %s\n",
				    sdp->isp_cmd_dma_burst_enable? tru : not);
			}
			for (i = 0; i < MAX_TARGETS; i++) {
				sdp->isp_devparam[i].dev_enable =
				    ISP1080_NVRAM_TGT_DEVICE_ENABLE(nvram_data, i, bus);
				sdp->isp_devparam[i].exc_throttle =
					ISP1080_NVRAM_TGT_EXEC_THROTTLE(nvram_data, i, bus);
				sdp->isp_devparam[i].sync_offset =
					ISP1080_NVRAM_TGT_SYNC_OFFSET(nvram_data, i, bus);
				sdp->isp_devparam[i].sync_period =
					ISP1080_NVRAM_TGT_SYNC_PERIOD(nvram_data, i, bus);
				sdp->isp_devparam[i].dev_flags = 0;
				if (ISP1080_NVRAM_TGT_RENEG(nvram_data, i, bus))
					sdp->isp_devparam[i].dev_flags |= DPARM_RENEG;
				if (ISP1080_NVRAM_TGT_QFRZ(nvram_data, i, bus)) {
					PRINTF("%s: not supporting QFRZ option "
					    "for target %d bus %d\n",
					    isp->isp_name, i, bus);
				}
				sdp->isp_devparam[i].dev_flags |= DPARM_ARQ;
				if (ISP1080_NVRAM_TGT_ARQ(nvram_data, i, bus) == 0) {
					PRINTF("%s: not disabling ARQ option "
					    "for target %d bus %d\n",
					    isp->isp_name, i, bus);
				}
				if (ISP1080_NVRAM_TGT_TQING(nvram_data, i, bus))
					sdp->isp_devparam[i].dev_flags |= DPARM_TQING;
				if (ISP1080_NVRAM_TGT_SYNC(nvram_data, i, bus))
					sdp->isp_devparam[i].dev_flags |= DPARM_SYNC;
				if (ISP1080_NVRAM_TGT_WIDE(nvram_data, i, bus))
					sdp->isp_devparam[i].dev_flags |= DPARM_WIDE;
				if (ISP1080_NVRAM_TGT_PARITY(nvram_data, i, bus))
					sdp->isp_devparam[i].dev_flags |= DPARM_PARITY;
				if (ISP1080_NVRAM_TGT_DISC(nvram_data, i, bus))
					sdp->isp_devparam[i].dev_flags |= DPARM_DISC;
				sdp->isp_devparam[i].cur_dflags = 0;
				if (isp->isp_dblev >= 3) {
					PRINTF("   Target %d: Ena %d Throttle "
					    "%d Offset %d Period %d Flags "
					    "0x%x\n", i,
					    sdp->isp_devparam[i].dev_enable,
					    sdp->isp_devparam[i].exc_throttle,
					    sdp->isp_devparam[i].sync_offset,
					    sdp->isp_devparam[i].sync_period,
					    sdp->isp_devparam[i].dev_flags);
				}
			}
		}
	} else if (IS_SCSI(isp)) {
		sdparam *sdp = (sdparam *) isp->isp_param;

		sdp->isp_fifo_threshold =
			ISP_NVRAM_FIFO_THRESHOLD(nvram_data) |
			(ISP_NVRAM_FIFO_THRESHOLD_128(nvram_data) << 2);

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

		sdp->isp_selection_timeout =
			ISP_NVRAM_SELECTION_TIMEOUT(nvram_data);

		sdp->isp_max_queue_depth =
			ISP_NVRAM_MAX_QUEUE_DEPTH(nvram_data);

		isp->isp_fast_mttr = ISP_NVRAM_FAST_MTTR_ENABLE(nvram_data);
		if (isp->isp_dblev > 2) {
			PRINTF("%s: NVRAM values:\n", isp->isp_name);
			PRINTF("             Fifo Threshold = 0x%x\n",
			    sdp->isp_fifo_threshold);
			PRINTF("            Bus Reset Delay = %d\n",
			    sdp->isp_bus_reset_delay);
			PRINTF("                Retry Count = %d\n",
			    sdp->isp_retry_count);
			PRINTF("                Retry Delay = %d\n",
			    sdp->isp_retry_delay);
			PRINTF("              Tag Age Limit = %d\n",
			    sdp->isp_tag_aging);
			PRINTF("          Selection Timeout = %d\n",
			    sdp->isp_selection_timeout);
			PRINTF("            Max Queue Depth = %d\n",
			    sdp->isp_max_queue_depth);
			PRINTF("           Async Data Setup = 0x%x\n",
			    sdp->isp_async_data_setup);
			PRINTF("    REQ/ACK Active Negation = %s\n",
			    sdp->isp_req_ack_active_neg? tru : not);
			PRINTF("  Data Line Active Negation = %s\n",
			    sdp->isp_data_line_active_neg? tru : not);
			PRINTF("      Data DMA Burst Enable = %s\n",
			    sdp->isp_data_dma_burst_enabl? tru : not);
			PRINTF("       Cmd DMA Burst Enable = %s\n",
			    sdp->isp_cmd_dma_burst_enable? tru : not);
			PRINTF("                  Fast MTTR = %s\n",
			    isp->isp_fast_mttr? tru : not);
		}
		for (i = 0; i < MAX_TARGETS; i++) {
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
			sdp->isp_devparam[i].cur_dflags = 0; /* we don't know */
			if (isp->isp_dblev > 2) {
				PRINTF("   Target %d: Enabled %d Throttle %d "
				    "Offset %d Period %d Flags 0x%x\n", i,
				    sdp->isp_devparam[i].dev_enable,
				    sdp->isp_devparam[i].exc_throttle,
				    sdp->isp_devparam[i].sync_offset,
				    sdp->isp_devparam[i].sync_period,
				    sdp->isp_devparam[i].dev_flags);
			}
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
			} wd;
			u_int64_t full64;
		} wwnstore;

		wwnstore.full64 = ISP2100_NVRAM_NODE_NAME(nvram_data);
		/*
		 * Broken PTI cards with nothing in the top nibble. Pah.
		 */
		if ((wwnstore.wd.hi32 >> 28) == 0) {
			wwnstore.wd.hi32 |= (2 << 28);
			CFGPRINTF("%s: (corrected) Adapter WWN 0x%08x%08x\n",
			    isp->isp_name, wwnstore.wd.hi32, wwnstore.wd.lo32);
		} else {
			CFGPRINTF("%s: Adapter WWN 0x%08x%08x\n", isp->isp_name,
			    wwnstore.wd.hi32, wwnstore.wd.lo32);
		}
		fcp->isp_nodewwn = wwnstore.full64;

		/*
		 * If the Node WWN has 2 in the top nibble, we can
		 * authoritatively construct a Port WWN by adding
		 * our unit number (plus one to make it nonzero) and
		 * putting it into bits 59..56. If the top nibble isn't
		 * 2, then we just set them identically.
		 */
		if ((fcp->isp_nodewwn >> 60) == 2) {
			fcp->isp_portwwn = fcp->isp_nodewwn |
			    (((u_int64_t)(isp->isp_unit+1)) << 56);
		} else {
			fcp->isp_portwwn = fcp->isp_nodewwn;
		}
		wwnstore.full64 = ISP2100_NVRAM_BOOT_NODE_NAME(nvram_data);
		if (wwnstore.full64 != 0) {
			PRINTF("%s: BOOT DEVICE WWN 0x%08x%08x\n",
			    isp->isp_name, wwnstore.wd.hi32, wwnstore.wd.lo32);
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
		fcp->isp_fwoptions = ISP2100_NVRAM_OPTIONS(nvram_data);
		if (isp->isp_dblev > 2) {
			PRINTF("%s: NVRAM values:\n", isp->isp_name);
			PRINTF("  Max IOCB Allocation = %d\n",
			    fcp->isp_maxalloc);
			PRINTF("     Max Frame Length = %d\n",
			    fcp->isp_maxfrmlen);
			PRINTF("   Execution Throttle = %d\n",
			    fcp->isp_execthrottle);
			PRINTF("          Retry Count = %d\n",
			    fcp->isp_retry_count);
			PRINTF("          Retry Delay = %d\n",
			    fcp->isp_retry_delay);
			PRINTF("         Hard Loop ID = %d\n",
			    fcp->isp_loopid);
			PRINTF("              Options = 0x%x\n",
			    fcp->isp_fwoptions);
			PRINTF("          HBA Options = 0x%x\n",
			    ISP2100_NVRAM_HBA_OPTIONS(nvram_data));
		}
	}
	IDPRINTF(3, ("%s: NVRAM is valid\n", isp->isp_name));
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

	if (IS_FC(isp)) {
		wo &= ((ISP2100_NVRAM_SIZE >> 1) - 1);
		rqst = (ISP_NVRAM_READ << 8) | wo;
		cbits = 10;
	} else if (IS_1080(isp) || IS_12X0(isp)) {
		wo &= ((ISP1080_NVRAM_SIZE >> 1) - 1);
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
