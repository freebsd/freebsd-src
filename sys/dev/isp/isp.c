/* $Id: isp.c,v 1.16 1999/03/26 00:33:13 mjacob Exp $ */
/* release_4_3_99 */
/*
 * Machine and OS Independent (well, as best as possible)
 * code for the Qlogic ISP SCSI adapters.
 *
 * Copyright (c) 1997, 1998 by Matthew Jacob
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
#ifdef	ISP_TARGET_MODE
static const char tgtiqd[36] = {
	0x03, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
	0x51, 0x4C, 0x4F, 0x47, 0x49, 0x43, 0x20, 0x20,
#ifdef	__NetBSD__
	0x4E, 0x45, 0x54, 0x42, 0x53, 0x44, 0x20, 0x20,
#else
# ifdef	__FreeBSD__
	0x46, 0x52, 0x45, 0x45, 0x42, 0x52, 0x44, 0x20,
# else
#  ifdef __OpenBSD__
	0x4F, 0x50, 0x45, 0x4E, 0x42, 0x52, 0x44, 0x20,
#  else
#   ifdef linux
	0x4C, 0x49, 0x4E, 0x55, 0x58, 0x20, 0x20, 0x20,
#   else
#   endif
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
static int isp_parse_async __P((struct ispsoftc *, int));
static int isp_handle_other_response
__P((struct ispsoftc *, ispstatusreq_t *, u_int8_t *));
#ifdef	ISP_TARGET_MODE
static int isp_modify_lun __P((struct ispsoftc *, int, int, int));
static void isp_notify_ack __P((struct ispsoftc *, void *));
static void isp_handle_atio __P((struct ispsoftc *, void *));
static void isp_handle_atio2 __P((struct ispsoftc *, void *));
static void isp_handle_ctio __P((struct ispsoftc *, void *));
static void isp_handle_ctio2 __P((struct ispsoftc *, void *));
#endif
static void isp_parse_status
__P((struct ispsoftc *, ispstatusreq_t *, ISP_SCSI_XFER_T *));
static void isp_fastpost_complete __P((struct ispsoftc *, int));
static void isp_fibre_init __P((struct ispsoftc *));
static void isp_mark_getpdb_all __P((struct ispsoftc *));
static int isp_getpdb __P((struct ispsoftc *, int, isp_pdb_t *));
static int isp_fclink_test __P((struct ispsoftc *, int));
static void isp_fw_state __P((struct ispsoftc *));
static void isp_dumpregs __P((struct ispsoftc *, const char *));
static void isp_dumpxflist __P((struct ispsoftc *));
static void isp_mboxcmd __P((struct ispsoftc *, mbreg_t *));

static void isp_update  __P((struct ispsoftc *));
static void isp_setdfltparm __P((struct ispsoftc *));
static int isp_read_nvram __P((struct ispsoftc *));
static void isp_rdnvram_word __P((struct ispsoftc *, int, u_int16_t *));

/*
 * Reset Hardware.
 *
 * Hit the chip over the head, download new f/w and set it running.
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
	 * Put it into PAUSE mode.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);

#if	0
	/*
	 * Do a little register testing.
	 */
	ISP_WRITE(isp, CDMA_COUNT, 0);
	ISP_WRITE(isp, CDMA_ADDR0, 0xdead);
	ISP_WRITE(isp, CDMA_ADDR1, 0xbeef);
	ISP_WRITE(isp, CDMA_ADDR2, 0xffff);
	ISP_WRITE(isp, CDMA_ADDR3, 0x1111);
	PRINTF("%s: (0,dead,beef,ffff,1111):\n", isp->isp_name);
	PRINTF("0x%x 0x%x 0x%x 0x%x 0x%x\n", ISP_READ(isp, CDMA_COUNT),
	    ISP_READ(isp, CDMA_ADDR0), ISP_READ(isp, CDMA_ADDR1),
	    ISP_READ(isp, CDMA_ADDR2), ISP_READ(isp, CDMA_ADDR3));
#endif

	if (IS_FC(isp)) {
		revname = "2100";
	} else if (IS_1080(isp)) {
		u_int16_t l;
		sdparam *sdp = isp->isp_param;
		revname = "1080";
		sdp->isp_clock = 100;
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
		case 6: 
			revname = "1040C(?)";
			isp->isp_type = ISP_HA_SCSI_1040C;
			sdp->isp_clock = 60;
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
			sdp->isp_clock = 60;
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
			if (isp->isp_mdvec->dv_clock < sdp->isp_clock) {
				sdp->isp_clock = isp->isp_mdvec->dv_clock;
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

#if	0
	/*
	 * Enable interrupts
	 */
	ENABLE_INTS(isp);
#endif

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

	if (dodnld && isp->isp_mdvec->dv_fwlen) {
		for (i = 0; i < isp->isp_mdvec->dv_fwlen; i++) {
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
	PRINTF("%s: Board Revision %s, %s F/W Revision %d.%d.%d\n",
		isp->isp_name, revname, dodnld? "loaded" : "resident",
		mbs.param[1], mbs.param[2], mbs.param[3]);
	if (IS_FC(isp)) {
		if (ISP_READ(isp, BIU2100_CSR) & BIU2100_PCI64) {
			PRINTF("%s: in 64-Bit PCI slot\n", isp->isp_name);
		}
	}
	isp->isp_fwrev[0] = mbs.param[1];
	isp->isp_fwrev[1] = mbs.param[2];
	isp->isp_fwrev[2] = mbs.param[3];
	if (isp->isp_romfw_rev[0] || isp->isp_romfw_rev[1] ||
	    isp->isp_romfw_rev[2]) {
		PRINTF("%s: Last F/W revision was %d.%d.%d\n", isp->isp_name,
		    isp->isp_romfw_rev[0], isp->isp_romfw_rev[1],
		    isp->isp_romfw_rev[2]);
	}
	isp_fw_state(isp);
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
	sdparam *sdp;
	mbreg_t mbs;
	int tgt;

	/*
	 * Must do first.
	 */
	isp_setdfltparm(isp);

	/*
	 * Set up DMA for the request and result mailboxes.
	 */
	if (ISP_MBOXDMASETUP(isp) != 0) {
		PRINTF("%s: can't setup dma mailboxes\n", isp->isp_name);
		return;
	}

	/*
	 * If we're fibre, we have a completely different
	 * initialization method.
	 */
	if (IS_FC(isp)) {
		isp_fibre_init(isp);
		return;
	}
	sdp = isp->isp_param;

	/*
	 * If we have fast memory timing enabled, turn it on.
	 */
	if (sdp->isp_fast_mttr) {
		ISP_WRITE(isp, RISC_MTR, 0x1313);
	}

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
	 * Set current per-target parameters to a safe minimum.
	 */
	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		int maxlun, lun;
		u_int16_t sdf;

		if (sdp->isp_devparam[tgt].dev_enable == 0)
			continue;

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
		mbs.param[1] = tgt << 8;
		mbs.param[2] = sdf;
		mbs.param[3] =
		    (sdp->isp_devparam[tgt].sync_offset << 8) |
		    (sdp->isp_devparam[tgt].sync_period);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			sdf = DPARM_SAFE_DFLT;
			mbs.param[0] = MBOX_SET_TARGET_PARAMS;
			mbs.param[1] = tgt << 8;
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
		/*
		 * We don't update dev_flags with what we've set
		 * because that's not the ultimate goal setting.
		 * If we succeed with the command, we *do* update
		 * cur_dflags by getting target parameters.
		 */
		mbs.param[0] = MBOX_GET_TARGET_PARAMS;
		mbs.param[1] = (tgt << 8);
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
		/*
		 * And mark this as an unannounced device
		 */
		sdp->isp_devparam[tgt].dev_announced = 0;
	}

	mbs.param[0] = MBOX_INIT_RES_QUEUE;
	mbs.param[1] = RESULT_QUEUE_LEN;
	mbs.param[2] = DMA_MSW(isp->isp_result_dma);
	mbs.param[3] = DMA_LSW(isp->isp_result_dma);
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
	mbs.param[2] = DMA_MSW(isp->isp_rquest_dma);
	mbs.param[3] = DMA_LSW(isp->isp_rquest_dma);
	mbs.param[4] = 0;
	mbs.param[5] = 0;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_dumpregs(isp, "set of request queue failed");
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
#if	0
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
#endif
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
	int count, loopid;

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
	loopid = DEFAULT_LOOPID;
#else
	loopid = fcp->isp_loopid;
#endif

#if	defined(ISP2100_FABRIC) && defined(ISP2100_SCCLUN)
	PRINTF("%s: Fabric Support, Expanded Lun Support\n", isp->isp_name);
#endif
#if	defined(ISP2100_FABRIC) && !defined(ISP2100_SCCLUN)
	PRINTF("%s: Fabric Support\n", isp->isp_name);
#endif
#if	!defined(ISP2100_FABRIC) && defined(ISP2100_SCCLUN)
	PRINTF("%s: Expanded Lun Support\n", isp->isp_name);
#endif

	icbp = (isp_icb_t *) fcp->isp_scratch;
	MEMZERO(icbp, sizeof (*icbp));

	icbp->icb_version = ICB_VERSION1;
#ifdef	ISP_TARGET_MODE
	fcp->isp_fwoptions = ICBOPT_TGT_ENABLE|ICBOPT_INI_TGTTYPE;
#else
	fcp->isp_fwoptions = 0;
#endif
	fcp->isp_fwoptions |= ICBOPT_INI_ADISC|ICBOPT_FAIRNESS;
	fcp->isp_fwoptions |= ICBOPT_PDBCHANGE_AE;
	fcp->isp_fwoptions |= ICBOPT_HARD_ADDRESS;
#ifndef	ISP_NO_FASTPOST_FC
	fcp->isp_fwoptions |= ICBOPT_FAST_POST;
#endif
#ifdef	CHECKME
	fcp->isp_fwoptions |= ICBOPT_USE_PORTNAME;
#endif
#ifdef	ISP2100_FABRIC
	fcp->isp_fwoptions |= ICBOPT_FULL_LOGIN;
#endif

	icbp->icb_fwoptions = fcp->isp_fwoptions;
	icbp->icb_maxfrmlen = fcp->isp_maxfrmlen;
	if (icbp->icb_maxfrmlen < ICB_MIN_FRMLEN ||
	    icbp->icb_maxfrmlen > ICB_MAX_FRMLEN) {
		PRINTF("%s: bad frame length (%d) from NVRAM- using %d\n",
		    isp->isp_name, fcp->isp_maxfrmlen, ICB_DFLT_FRMLEN);
		icbp->icb_maxfrmlen = ICB_DFLT_FRMLEN;
	}
	icbp->icb_maxalloc = fcp->isp_maxalloc;
	if (icbp->icb_maxalloc < 16) {
		PRINTF("%s: bad maximum allocation (%d)- using 16\n",
		     isp->isp_name, fcp->isp_maxalloc);
		icbp->icb_maxalloc = 16;
	}
	icbp->icb_execthrottle = fcp->isp_execthrottle;
	if (icbp->icb_execthrottle < 1) {
		PRINTF("%s: bad execution throttle of %d- using 16\n",
		    isp->isp_name, fcp->isp_execthrottle);
		icbp->icb_execthrottle = 16;
	}
	icbp->icb_retry_delay = fcp->isp_retry_delay;
	icbp->icb_retry_count = fcp->isp_retry_count;
	icbp->icb_hardaddr = loopid;

	if (fcp->isp_wwn) {
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_nodename, fcp->isp_wwn);
		if (icbp->icb_fwoptions & ICBOPT_USE_PORTNAME) {
			u_int64_t portname = fcp->isp_wwn | (2LL << 56);
			MAKE_NODE_NAME_FROM_WWN(icbp->icb_nodename, portname);
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
	MemoryBarrier();

	for (count = 0; count < 10; count++) {
		mbs.param[0] = MBOX_INIT_FIRMWARE;
		mbs.param[1] = 0;
		mbs.param[2] = DMA_MSW(fcp->isp_scdma);
		mbs.param[3] = DMA_LSW(fcp->isp_scdma);
		mbs.param[4] = 0;
		mbs.param[5] = 0;
		mbs.param[6] = 0;
		mbs.param[7] = 0;

		isp_mboxcmd(isp, &mbs);

		switch (mbs.param[0]) {
		case MBOX_COMMAND_COMPLETE:
			count = 10;
			break;
		case ASYNC_PDB_CHANGED:
			isp_mark_getpdb_all(isp);
			/* FALL THROUGH */
		case ASYNC_LIP_OCCURRED:
		case ASYNC_LOOP_UP:
		case ASYNC_LOOP_DOWN:
		case ASYNC_LOOP_RESET:
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
	isp->isp_sendmarker = 1;

	/*
	 * Whatever happens, we're now committed to being here.
	 */
	isp->isp_state = ISP_INITSTATE;
	fcp->isp_fwstate = FW_CONFIG_WAIT;

	isp_mark_getpdb_all(isp);

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
	isp_pdb_t *p;
	fcparam *fcp = (fcparam *) isp->isp_param;
	for (p = &fcp->isp_pdb[0]; p < &fcp->isp_pdb[MAX_FC_TARG]; p++) {
		p->pdb_options = INVALID_PDB_OPTIONS;
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
		MEMCPY(pdbp, fcp->isp_scratch, sizeof (isp_pdb_t));
		break;
	case MBOX_HOST_INTERFACE_ERROR:
		PRINTF("%s: DMA error getting port database\n", isp->isp_name);
		return (-1);
	case MBOX_COMMAND_PARAM_ERROR:
		/* Not Logged In */
		IDPRINTF(3, ("%s: Comand Param Error on Get Port Database\n",
		    isp->isp_name));
		return (-1);
	default:
		PRINTF("%s: error 0x%x getting port database for ID %d\n",
		    isp->isp_name, mbs.param[0], id);
		return (-1);
	}
	return (0);
}

/*
 * Make sure we have good FC link and know our Loop ID.
 */

static int
isp_fclink_test(isp, waitdelay)
	struct ispsoftc *isp;
	int waitdelay;
{
	mbreg_t mbs;
	int count;
	u_int8_t lwfs;
	fcparam *fcp;

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
	fcp->isp_alpa = mbs.param[2];
	PRINTF("%s: Loop ID %d, ALPA 0x%x\n", isp->isp_name,
	    fcp->isp_loopid, fcp->isp_alpa);
	return (0);

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
	int i, rqidx;

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

	if (XS_CDBLEN(xs) > ((isp->isp_type & ISP_HA_FC)? 16 : 12)) {
		PRINTF("%s: unsupported cdb length (%d)\n",
		    isp->isp_name, XS_CDBLEN(xs));
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}

	/*
	 * Check to see whether we have good firmware state still or
	 * need to refresh our port database for this target.
	 */
	if (IS_FC(isp)) {
		fcparam *fcp = isp->isp_param;
		isp_pdb_t *pdbp = &fcp->isp_pdb[XS_TGT(xs)];

		/*
		 * Check for f/w being in ready state. Well, okay,
		 * our cached copy of it...
		 */
		if (fcp->isp_fwstate != FW_READY) {
			if (isp_fclink_test(isp, FC_FW_READY_DELAY)) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				return (CMD_COMPLETE);
			}
		}
		/*
		 * Refresh our port database if needed.
		 */
		if (pdbp->pdb_options == INVALID_PDB_OPTIONS) {
			if (isp_getpdb(isp, XS_TGT(xs), pdbp) == 0) {
				isp_async(isp, ISPASYNC_PDB_CHANGE_COMPLETE,
				    (void *) (long) XS_TGT(xs));
			}
		}
	}

	/*
	 * Next check to see if any HBA or Device
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

	if (isp->isp_sendmarker) {
		u_int8_t niptr;

		MEMZERO((void *) reqp, sizeof (*reqp));
		reqp->req_header.rqs_entry_count = 1;
		reqp->req_header.rqs_entry_type = RQSTYPE_MARKER;
		reqp->req_modifier = SYNC_ALL;
		ISP_SBUSIFY_ISPHDR(isp, &reqp->req_header);

		/*
		 * Unconditionally update the input pointer anyway.
		 */
		ISP_WRITE(isp, INMAILBOX4, iptr);
		isp->isp_reqidx = iptr;

		niptr = ISP_NXT_QENTRY(iptr, RQUEST_QUEUE_LEN);
		if (niptr == optr) {
			IDPRINTF(2, ("%s: Request Queue Overflow+\n",
			    isp->isp_name));
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_EAGAIN);
		}
		reqp = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
		iptr = niptr;
	}

	MEMZERO((void *) reqp, UZSIZE);
	reqp->req_header.rqs_entry_count = 1;
	if (isp->isp_type & ISP_HA_FC) {
		reqp->req_header.rqs_entry_type = RQSTYPE_T2RQS;
	} else {
		reqp->req_header.rqs_entry_type = RQSTYPE_REQUEST;
	}
	reqp->req_header.rqs_flags = 0;
	reqp->req_header.rqs_seqno = isp->isp_seqno++;
	ISP_SBUSIFY_ISPHDR(isp, &reqp->req_header);

	for (rqidx = 0; rqidx < RQUEST_QUEUE_LEN; rqidx++) {
		if (isp->isp_xflist[rqidx] == NULL)
			break;
	}
	if (rqidx == RQUEST_QUEUE_LEN) {
		IDPRINTF(2, ("%s: out of xflist pointers\n", isp->isp_name));
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	} else {
		/*
		 * Never have a handle that is zero, so
		 * set req_handle off by one.
		 */
		isp->isp_xflist[rqidx] = xs;
		reqp->req_handle = rqidx+1;
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
	reqp->req_target = XS_TGT(xs);
	if (isp->isp_type & ISP_HA_SCSI) {
		reqp->req_lun_trn = XS_LUN(xs);
		reqp->req_cdblen = XS_CDBLEN(xs);
	} else {
#ifdef	ISP2100_SCCLUN
		reqp->req_scclun = XS_LUN(xs);
#else
		reqp->req_lun_trn = XS_LUN(xs);
#endif

	}
	MEMCPY(reqp->req_cdb, XS_CDBP(xs), XS_CDBLEN(xs));

	IDPRINTF(5, ("%s(%d.%d): START%d cmd 0x%x datalen %d\n", isp->isp_name,
	    XS_TGT(xs), XS_LUN(xs), reqp->req_header.rqs_seqno,
	    reqp->req_cdb[0], XS_XFRLEN(xs)));

	reqp->req_time = XS_TIME(xs) / 1000;
	if (reqp->req_time == 0 && XS_TIME(xs))
		reqp->req_time = 1;

	/*
	 * Always give a bit more leeway to commands after a bus reset.
	 */
	if (isp->isp_sendmarker && reqp->req_time < 5)
		reqp->req_time = 5;

	i = ISP_DMASETUP(isp, xs, reqp, &iptr, optr);
	if (i != CMD_QUEUED) {
		/*
		 * Take memory of it away...
		 */
		isp->isp_xflist[rqidx] = NULL;
		/*
		 * dmasetup sets actual error in packet, and
		 * return what we were given to return.
		 */
		return (i);
	}
	XS_SETERR(xs, HBA_NOERROR);
	ISP_SBUSIFY_ISPREQ(isp, reqp);
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
	int i;

	switch (ctl) {
	default:
		PRINTF("%s: isp_control unknown control op %x\n",
		    isp->isp_name, ctl);
		break;

	case ISPCTL_RESET_BUS:
		/*
		 * This is really important to have set after a bus reset.
		 */
		isp->isp_sendmarker = 1;

		/*
		 * Issue a bus reset.
		 */
		mbs.param[0] = MBOX_BUS_RESET;
		if (isp->isp_type & ISP_HA_SCSI) {
			mbs.param[1] =
			    ((sdparam *) isp->isp_param)->isp_bus_reset_delay;
			if (mbs.param[1] < 2)
				mbs.param[1] = 2;
		} else {
			/*
			 * Unparameterized.
			 */
			mbs.param[1] = 5;
		}
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_dumpregs(isp, "isp_control SCSI bus reset failed");
			break;
		}
		PRINTF("%s: driver initiated bus reset\n", isp->isp_name);
		return (0);

	case ISPCTL_RESET_DEV:
		mbs.param[0] = MBOX_ABORT_TARGET;
		mbs.param[1] = ((long)arg) << 8;
		mbs.param[2] = 3;	/* 'delay', in seconds */
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_dumpregs(isp, "Target Reset Failed");
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
#ifdef	ISP2100_SCCLUN
		if (isp->isp_type & ISP_HA_FC) {
			mbs.param[1] = XS_TGT(xs) << 8;
			mbs.param[4] = 0;
			mbs.param[5] = 0;
			mbs.param[6] = XS_LUN(xs);
		} else {
			mbs.param[1] = XS_TGT(xs) << 8 | XS_LUN(xs);
		}
#else
		mbs.param[1] = XS_TGT(xs) << 8 | XS_LUN(xs);
#endif
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
	u_int16_t isr;
	int i, nlooked = 0, ndone = 0;

	isr = ISP_READ(isp, BIU_ISR);
	IDPRINTF(5, ("%s: isp_intr isr %x sema 0x%x\n", isp->isp_name, isr,
	    ISP_READ(isp, BIU_SEMA)));
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
	if (isp->isp_state != ISP_RUNSTATE) {
		PRINTF("%s: interrupt (isr=0x%x,sema=0x%x) when not ready\n",
		    isp->isp_name, isr, ISP_READ(isp, BIU_SEMA));
		ISP_WRITE(isp, INMAILBOX5, ISP_READ(isp, OUTMAILBOX5));
		ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
		ISP_WRITE(isp, BIU_SEMA, 0);
		ENABLE_INTS(isp);
		return (1);
	}

	if (ISP_READ(isp, BIU_SEMA) & 1) {
		u_int16_t mbox = ISP_READ(isp, OUTMAILBOX0);
		if (mbox & 0x4000) {
			IDPRINTF(3, ("%s: isp_intr sees 0x%x\n",
			    isp->isp_name, mbox));
		} else {
			u_int32_t fhandle = isp_parse_async(isp, (int) mbox);
			if (fhandle > 0) {
				xs = (void *)isp->isp_xflist[fhandle - 1];
				isp->isp_xflist[fhandle - 1] = NULL;
				/*
				 * Since we don't have a result queue entry
				 * item, we must believe that SCSI status is
				 * zero and that all data transferred.
				 */
				XS_RESID(xs) = 0;
				XS_STS(xs) = 0;
				if (XS_XFRLEN(xs)) {
					ISP_DMAFREE(isp, xs, fhandle - 1);
				}
				if (isp->isp_nactive > 0)
				    isp->isp_nactive--;
				XS_CMD_DONE(xs);
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
		ISP_SBUSIFY_ISPHDR(isp, &sp->req_header);
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
		xs = (void *) isp->isp_xflist[sp->req_handle - 1];
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
				isp->isp_update = 1;
				sdp->isp_devparam[XS_TGT(xs)].dev_refresh = 1;
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
		} else {
			PRINTF("%s: unknown return %x\n", isp->isp_name,
				sp->req_header.rqs_entry_type);
			if (XS_NOERR(xs)) {
				XS_SETERR(xs, HBA_BOTCH);
			}
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
				PRINTF("%s: ARQ failure for target %d lun %d\n",
				    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
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
		isp_async(isp, ISPASYNC_BUS_RESET, NULL);
		isp->isp_sendmarker = 1;
#ifdef	ISP_TARGET_MODE
		isp_notify_ack(isp, NULL);
#endif
		break;

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
		isp->isp_sendmarker = 1;
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
		((fcparam *) isp->isp_param)->isp_fwstate = FW_CONFIG_WAIT;
		isp->isp_sendmarker = 1;
		isp_mark_getpdb_all(isp);
		PRINTF("%s: LIP occurred\n", isp->isp_name);
		break;

	case ASYNC_LOOP_UP:
		((fcparam *) isp->isp_param)->isp_fwstate = FW_CONFIG_WAIT;
		isp->isp_sendmarker = 1;
		isp_mark_getpdb_all(isp);
		isp_async(isp, ISPASYNC_LOOP_UP, NULL);
		break;

	case ASYNC_LOOP_DOWN:
		((fcparam *) isp->isp_param)->isp_fwstate = FW_CONFIG_WAIT;
		isp->isp_sendmarker = 1;
		isp_mark_getpdb_all(isp);
		isp_async(isp, ISPASYNC_LOOP_DOWN, NULL);
		break;

	case ASYNC_LOOP_RESET:
		((fcparam *) isp->isp_param)->isp_fwstate = FW_CONFIG_WAIT;
		isp->isp_sendmarker = 1;
		isp_mark_getpdb_all(isp);
		PRINTF("%s: Loop RESET\n", isp->isp_name);
#ifdef	ISP_TARGET_MODE
		isp_notify_ack(isp, NULL);
#endif
		break;

	case ASYNC_PDB_CHANGED:
		isp->isp_sendmarker = 1;
		isp_mark_getpdb_all(isp);
		PRINTF("%s: Port Database Changed\n", isp->isp_name);
		break;

	case ASYNC_CHANGE_NOTIFY:
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
	u_int8_t iptr, optr;
	int reqsize = 0;
	void *ireqp = NULL;
#ifdef	ISP_TARGET_MODE
	union {
		at_entry_t	*atio;
		at2_entry_t	*at2io;
		ct_entry_t	*ctio;
		ct2_entry_t	*ct2io;
		lun_entry_t	*lunen;
		in_entry_t	*inot;
		in_fcentry_t	*inot_fc;
		na_entry_t	*nack;
		na_fcentry_t	*nack_fc;
		void		*voidp;
#define	atio	un.atio
#define	at2io	un.at2io
#define	ctio	un.ctio
#define	ct2io	un.ct2io
#define	lunen	un.lunen
#define	inot	un.inot
#define	inot_fc	un.inot_fc
#define	nack	un.nack
#define	nack_fc	un.nack_fc
	} un;

	un.voidp = sp;
#endif


	switch (sp->req_header.rqs_entry_type) {
	case RQSTYPE_REQUEST:
		return (-1);
#ifdef	ISP_TARGET_MODE
	case RQSTYPE_NOTIFY_ACK:
	{
		static const char *f =
			"%s: Notify Ack Status 0x%x Sequence Id 0x%x\n"
		/*
		 * The ISP is acknowleding our ack of an Immediate Notify.
		 */
		if (isp->isp_type & ISP_HA_FC) {
			PRINTF(f, isp->isp_name,
			    nack_fc->na-status, nack_fc->na_seqid);
		} else {
			PRINTF(f, isp->isp_name,
			    nack->na_status, nack->na_seqid);
		}
		break;
	}
	case RQSTYPE_NOTIFY:
	{
		u_int16_t seqid, status;

		/*
		 * Either the ISP received a SCSI message it cannot handle
		 * or some other out of band condition (e.g., Port Logout)
		 * or it is returning an Immediate Notify entry we sent.
		 */
		if (isp->isp_type & ISP_HA_FC) {
			status = inot_fc->status;
			seqid = inot_fc->in_seqid;
		} else {
			status = inot->status;
			seqid = inot->seqid & 0xff;
		}
		PRINTF("%s: Immediate Notify Status 0x%x Sequence Id 0x%x\n",
		    isp->isp_name, status, seqid);

		switch (status) {
		case IN_MSG_RECEIVED:
		case IN_IDE_RECEIVED:
			ptisp_got_msg(ptp, &inot);
			break;
		case IN_RSRC_UNAVAIL:
			PRINTF("%s: Firmware out of ATIOs\n", isp->isp_name);
			break;
		case IN_ABORT_TASK:
			PRINTF("%s: Abort Task iid %d rx_id 0x%x\n",
			    inot_fc->in_iid, seqid);
			break;
		case IN_PORT_LOGOUT:
			PRINTF("%s: Port Logout for Initiator %d\n",
			    isp->isp_name, inot_fc->in_iid);
			break;
		default:
			PRINTF("%s: bad status (0x%x) in Immediate Notify\n",
			    isp->isp_name, status);
			break;

		}
		isp_notify_ack(isp, un.voidp);
		reqsize = 0;
		break;
	}
	case RQSTYPE_ENABLE_LUN:
	case RQSTYPE_MODIFY_LUN:
		if (lunen->req_status != 1) {
		    PRINTF("%s: ENABLE/MODIFY LUN returned status 0x%x\n",
			isp->isp_name, lunen->req_status);
		}
		break;
	case RQSTYPE_ATIO2:
	{
		fcparam *fcp = isp->isp_param;
		ispctiot2_t local, *ct2 = NULL;
		ispatiot2_t *at2 = (ispatiot2_t *) sp;
		int s, lun;

#ifdef	ISP2100_SCCLUN
		lun = at2->req_scclun;
#else
		lun = at2->req_lun;
#endif
		PRINTF("%s: atio2 loopid %d for lun %d rxid 0x%x flags0x%x "
		    "tflags0x%x ecodes0x%x rqstatus0x%x\n", isp->isp_name,
		    at2->req_initiator, lun, at2->req_rxid,
		    at2->req_flags, at2->req_taskflags, at2->req_execodes,
		    at2->req_status);

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
		MEMZERO((void *) ct2, sizeof (*ct2));
		ct2->req_header.rqs_entry_type = RQSTYPE_CTIO2;
		ct2->req_header.rqs_entry_count = 1;
		ct2->req_header.rqs_flags = 0;
		ct2->req_header.rqs_seqno = isp->isp_seqno++;
		ct2->req_handle = (at2->req_initiator << 16) | lun;
#ifndef	ISP2100_SCCLUN
		ct2->req_lun = lun;
#endif
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
				s = sizeof (tgtiqd);
				MEMCPY(fcp->isp_scratch, tgtiqd, s);
			} else {
				s = at2->req_datalen;
				MEMZERO(fcp->isp_scratch, s);
			}
			ct2->req_m.mode0.req_dataseg[0].ds_base =
			    fcp->isp_scdma;
			ct2->req_m.mode0.req_dataseg[0].ds_count = s;
			ct2->req_m.mode0.req_datalen = s;
#if	1
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
#if	1
			if (at2->req_datalen) {
				ct2->req_m.mode1.req_scsi_status |=
				    CTIO2_RSPUNDERUN;
				ct2->req_resid[0] = at2->req_datalen & 0xff;
				ct2->req_resid[1] =
					(at2->req_datalen >> 8) & 0xff;
				ct2->req_resid[2] =
					(at2->req_datalen >> 16) & 0xff;
				ct2->req_resid[3] =
					(at2->req_datalen >> 24) & 0xff;
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
				MEMCPY(ct2->req_m.mode1.req_response,
				    at2->req_sense, sizeof (at2->req_sense));
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
#undef	atio
#undef	at2io
#undef	ctio
#undef	ct2io
#undef	lunen
#undef	inot
#undef	inot_fc
#undef	nack
#undef	nack_fc
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
			MEMCPY(reqp, ireqp, reqsize);
			ISP_WRITE(isp, INMAILBOX4, iptr);
			isp->isp_reqidx = iptr;
		}
	}
	return (0);
}

#ifdef	ISP_TARGET_MODE

static void isp_tmd_newcmd_dflt __P((void *, tmd_cmd_t *));
static void isp_tmd_event_dflt __P((void *, int));
static void isp_tmd_notify_dflt __P((void *, tmd_notify_t *));

static void isp_tgt_data_xfer __P ((tmd_cmd_t *));
static void isp_tgt_endcmd __P ((tmd_cmd_t *, u_int8_t));
static void isp_tgt_done __P ((tmd_cmd_t *));

static void
isp_tmd_newcmd_dflt(arg0, cmdp)
	void *arg0;
	tmd_cmd_t *cmdp;
{
}

static void
isp_tmd_event_dflt(arg0, event)
	void *arg0;
	int event;
{
}

static void
isp_tmd_notify_dflt(arg0, npt)
	void *arg0;
	tmd_notify_t *npt;
{
}

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

	MEMZERO((void *) ip, sizeof (*ip));
	ip->req_header.rqs_entry_type = RQSTYPE_ENABLE_LUN;
	ip->req_header.rqs_entry_count = 1;
	ip->req_header.rqs_seqno = isp->isp_seqno++;
	ip->req_handle = RQSTYPE_ENABLE_LUN;
	if (isp->isp_type & ISP_HA_SCSI) {
		ip->req_lun = lun;
	}
	ip->req_cmdcount = ccnt;
	ip->req_imcount = icnt;
	ip->req_timeout = 0;	/* default 30 seconds */
	ISP_WRITE(isp, INMAILBOX4, iptr);
	isp->isp_reqidx = iptr;
	return (0);
}

static void
isp_notify_ack(isp, ptrp)
	struct ispsoftc *isp;
	void *ptrp;
{
	void *reqp;
	u_int8_t iptr, optr;
	union {
		na_fcentry_t _naf;
		na_entry_t _nas;
	} un;

	MEMZERO((caddr_t)&un, sizeof (un));
	un._nas.na_header.rqs_entry_type = RQSTYPE_NOTIFY_ACK;
	un._nas.na_header.rqs_entry_count = 1;

	if (isp->isp_type & ISP_HA_FC) {
		na_fcentry_t *na = &un._nas;
		if (ptrp) {
			in_fcentry_t *inp = ptrp;
			na->na_iid = inp->in_iid;
			na->na_lun = inp->in_lun;
			na->na_task_flags = inp->in_task_flags;
			na->na_seqid = inp->in_seqid;
			na->na_status = inp->in_status;
		} else {
			na->na_flags = NAFC_RST_CLRD;
		}
	} else {
		na_entry_t *na = &un._nas;
		if (ptrp) {
			in_entry_t *inp = ptrp;
			na->na_iid = inp->in_iid;
			na->na_lun = inp->in_lun;
			na->na_tgt = inp->in_tgt;
			na->na_seqid = inp->in_seqid;
		} else {
			na->na_flags = NA_RST_CLRD;
		}
	}
	optr = isp->isp_reqodx = ISP_READ(isp, OUTMAILBOX4);
	iptr = isp->isp_reqidx;
	reqp = (void *) ISP_QUEUE_ENTRY(isp->isp_rquest, iptr);
	iptr = ISP_NXT_QENTRY(iptr, RQUEST_QUEUE_LEN);
	if (iptr == optr) {
		PRINTF("%s: Request Queue Overflow For isp_notify_ack\n",
		    isp->isp_name);
	} else {
		MEMCPY(reqp, ireqp, sizeof (un));
		ISP_WRITE(isp, INMAILBOX4, iptr);
		isp->isp_reqidx = iptr;
	}
}

/*
 * These are dummy stubs for now until the outside framework is plugged in.
 */

static void
isp_handle_atio (isp, aep)
	struct ispsoftc *isp;
	at_entry_t *aep;
{
	int status, connected;
	tmd_cmd_t local, *cdp = &local;

	/*
	 * Get the ATIO status and see if we're still connected.
	 */
	status = aep->at_status;
	connected = ((aep->at_flags & AT_NODISC) != 0);

	PRINTF("%s: ATIO status=0x%x, connected=%d\n", isp->isp_name,
	    status, connected);

	/*
	 * The firmware status (except for the SenseValid bit) indicates
	 * why this ATIO was sent to us.
	 * If SenseValid is set, the firware has recommended Sense Data.
	 * If the Disconnects Disabled bit is set in the flags field,
	 * we're still connected on the SCSI bus - i.e. the initiator
	 * did not set DiscPriv in the identify message. We don't care
	 * about this so it's ignored.
	 */
	switch (status & ~TGTSVALID) {
	case AT_PATH_INVALID:
		/*
		 * ATIO rejected by the firmware due to disabled lun.
		 */
		PRINTF("%s: Firmware rejected ATIO for disabled lun %d\n",
		    isp->isp_name, aep->at_lun);
		break;

	case AT_PHASE_ERROR:
		/*
		 * Bus Pase Sequence error.
		 *
		 * The firmware should have filled in the correct
		 * sense data.
		 */


		if (status & TGTSVALID) {
			MEMCPY(&cdp->cd_sensedata, aep->at_sense,
			    sizeof (cdp->cd_sensedata));
			PRINTF("%s: Bus Phase Sequence error key 0x%x\n",
			    isp->isp_name, cdp->cd_sensedata[2] & 0xf);
		} else {
			PRINTF("%s: Bus Phase Sequence With No Sense\n",
			    isp->isp_name);
		}
		(*isp->isp_tmd_newcmd)(isp, cdp);
		break;

	case AT_NOCAP:
		/*
		 * Requested Capability not available
		 * We sent an ATIO that overflowed the firmware's
		 * command resource count.
		 */
		PRINTF("%s: Firmware rejected ATIO, command count overflow\n",
		    isp->isp_name);
		break;

	case AT_BDR_MSG:
		/*
		 * If we send an ATIO to the firmware to increment
		 * its command resource count, and the firmware is
		 * recovering from a Bus Device Reset, it returns
		 * the ATIO with this status.
		 */
		PRINTF("%s: ATIO returned with BDR received\n", isp->isp_name);
		break;

	case AT_CDB:
		/*
		 * New CDB
		 */
		cdp->cd_hba = isp;
		cdp->cd_iid = aep->at_iid;
		cdp->cd_tgt = aep->at_tgt;
		cdp->cd_lun = aep->at_lun;
		cdp->cd_tagtype = aep->at_tag_type;
		cdp->cd_tagval = aep->at_tag_val;
		MEMCPY(cdp->cd_cdb, aep->at_cdb, 16);
		PRINTF("%s: CDB 0x%x itl %d/%d/%d\n", isp->isp_name,
		    cdp->cd_cdb[0], cdp->cd_iid, cdp->cd_tgt, cdp->cd_lun);
		(*isp->isp_tmd_newcmd)(isp, cdp);
		break;

	default:
		PRINTF("%s: Unknown status (0x%x) in ATIO\n",
		    isp->isp_name, status);
		cdp->cd_hba = isp;
		cdp->cd_iid = aep->at_iid;
		cdp->cd_tgt = aep->at_tgt;
		cdp->cd_lun = aep->at_lun;
		cdp->cd_tagtype = aep->at_tag_type;
		cdp->cd_tagval = aep->at_tag_val;
		isp_tgtcmd_done(cdp);
		break;
	}
}

static void
isp_handle_atio2(isp, aep)
	struct ispsoftc *isp;
	at2_entry_t *aep;
{
	int status;
	tmd_cmd_t local, *cdp = &local;

	/*
	 * Get the ATIO2 status.
	 */
	status = aep->at_status;
	PRINTD("%s: ATIO2 status=0x%x\n", status);

	/*
	 * The firmware status (except for the SenseValid bit) indicates
	 * why this ATIO was sent to us.
	 * If SenseValid is set, the firware has recommended Sense Data.
	 */
	switch (status & ~TGTSVALID) {
	case AT_PATH_INVALID:
		/*
		 * ATIO rejected by the firmware due to disabled lun.
		 */
		PRINTF("%s: Firmware rejected ATIO2 for disabled lun %d\n",
		    isp->isp_name, aep->at_lun);
		break;

	case AT_NOCAP:
		/*
		 * Requested Capability not available
		 * We sent an ATIO that overflowed the firmware's
		 * command resource count.
		 */
		PRINTF("%s: Firmware rejected ATIO2, command count overflow\n",
		    isp->isp_name);
		break;

	case AT_BDR_MSG:
		/*
		 * If we send an ATIO to the firmware to increment
		 * its command resource count, and the firmware is
		 * recovering from a Bus Device Reset, it returns
		 * the ATIO with this status.
		 */
		PRINTF("%s: ATIO2 returned with BDR rcvd\n", isp->isp_name);
		break;

	case AT_CDB:
		/*
		 * New CDB
		 */
		cdp->cd_hba = isp;
		cdp->cd_iid = aep->at_iid;
		cdp->cd_tgt = 0;
		cdp->cd_lun = aep->at_lun;
		MEMCPY(cdp->cd_cdb, aep->at_cdb, 16);
		cdp->cd_rxid = aep->at_rxid;
		cdp->cp_origdlen = aep->at_datalen;
		cdp->cp_totbytes = 0;
		PRINTF("%s: CDB 0x%x rx_id 0x%x itl %d/%d/%d dlen %d\n",
		    isp->isp_name, cdp->cd_cdb[0], cdp->cd_tagval, cdp->cd_iid,
		    cdp->cd_tgt, cdp->cd_lun, aep->at_datalen);
		(*isp->isp_tmd_newcmd)(isp, cdp);
		break;

	default:
		PRINTF("%s: Unknown status (0x%x) in ATIO2\n",
		    isp->isp_name, status);
		cdp->cd_hba = isp;
		cdp->cd_iid = aep->at_iid;
		cdp->cd_tgt = aep->at_tgt;
		cdp->cd_lun = aep->at_lun;
		cdp->cp_rxid = aep->at_rxid;
		isp_tgtcmd_done(cdp);
		break;
	}
}

static void
isp_handle_ctio(isp, cep)
	struct ispsoftc *isp;
	ct_entry_t *aep;
{
}

static void
isp_handle_ctio2(isp, cep)
	struct ispsoftc *isp;
	at2_entry_t *aep;
{
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
		PRINTF("%s: target %d lun %d had an unexpected bus free\n",
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
		PRINTF("%s: internal queues full for target %d lun %d "
		    "status 0x%x\n", isp->isp_name, XS_TGT(xs), XS_LUN(xs),
		    XS_STS(xs));
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
		if (isp->isp_type & ISP_HA_SCSI) {
			sdparam *sdp = isp->isp_param;
			isp->isp_update = 1;
			sdp->isp_devparam[XS_TGT(xs)].dev_flags &= ~DPARM_WIDE;
			sdp->isp_devparam[XS_TGT(xs)].dev_update = 1;
		}
		XS_SETERR(xs, HBA_NOERROR);
		return;

	case RQCS_SYNCXFER_FAILED:
		PRINTF("%s: SDTR Message failed for target %d lun %d\n",
		    isp->isp_name, XS_TGT(xs), XS_LUN(xs));
		if (isp->isp_type & ISP_HA_SCSI) {
			sdparam *sdp = isp->isp_param;
			isp->isp_update = 1;
			sdp->isp_devparam[XS_TGT(xs)].dev_flags &= ~DPARM_SYNC;
			sdp->isp_devparam[XS_TGT(xs)].dev_update = 1;
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

static void
isp_fastpost_complete(isp, fph)
	struct ispsoftc *isp;
	int fph;
{
	ISP_SCSI_XFER_T *xs;

	if (fph < 1)
		return;
	xs = (ISP_SCSI_XFER_T *) isp->isp_xflist[fph - 1];
	isp->isp_xflist[fph - 1] = NULL;
	if (xs == NULL) {
		PRINTF("%s: fast posting handle 0x%x not found\n",
		    isp->isp_name, fph - 1);
		return;
	}
	/*
	 * Since we don't have a result queue entry item,
	 * we must believe that SCSI status is zero and
	 * that all data transferred.
	 */
	XS_RESID(xs) = 0;
	XS_STS(xs) = 0;
	if (XS_XFRLEN(xs)) {
		ISP_DMAFREE(isp, xs, fph - 1);
	}
	XS_CMD_DONE(xs);
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
	MAKNIB(0, 0),	/* 0x60: MBOX_GET_INIT_CONTROL_BLOCK  (FORMAT?) */
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
	MAKNIB(0, 0),	/* 0x6e: */
	MAKNIB(0, 0),	/* 0x6f: */
	MAKNIB(0, 0),	/* 0x70: */
	MAKNIB(0, 0),	/* 0x71: */
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
	if (isp->isp_type & ISP_HA_FC) {
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
	 * Make sure we can send some words. Check to see id there's
	 * an async mbox event pending.
	 */

	loops = MBOX_DELAY_COUNT;
	while ((ISP_READ(isp, HCCR) & HCCR_HOST_INT) != 0) {
		SYS_DELAY(100);
		if (ISP_READ(isp, BIU_SEMA) & 1) {
			int fph;
			u_int16_t mbox = ISP_READ(isp, OUTMAILBOX0);
			/*
			 * We have a pending MBOX async event.
			 */
			if (mbox & 0x8000) {
				fph = isp_parse_async(isp, (int) mbox);
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
			if (mbox == MBOX_COMMAND_COMPLETE) {
				ISP_WRITE(isp, BIU_SEMA, 0);
				ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
				SYS_DELAY(100);
				goto command_known;
			}
		}
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
	 * If we're a 1080 or a 1240, make sure that for a couple of commands
	 * the port parameter is set. This is sort of a temporary solution
	 * to do it here rather than every place a mailbox command is formed.
	 */
	if (IS_1080(isp) || IS_12X0(isp)) {
		switch (mbp->param[0]) {
		case MBOX_BUS_RESET:
			mbp->param[2] = isp->isp_port;
			break;
		default:
			break;
		}
	}

	/*
	 * Write input parameters.
	 */
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

	ENABLE_INTS(isp);
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

	case ASYNC_LIP_OCCURRED:
	case ASYNC_LOOP_UP:
	case ASYNC_LOOP_DOWN:
	case ASYNC_LOOP_RESET:
	case ASYNC_CHANGE_NOTIFY:
		break;
	case ASYNC_PDB_CHANGED:
		isp_mark_getpdb_all(isp);
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
	PRINTF("    mbox regs: %x %x %x %x %x\n",
	    ISP_READ(isp, OUTMAILBOX0), ISP_READ(isp, OUTMAILBOX1),
	    ISP_READ(isp, OUTMAILBOX2), ISP_READ(isp, OUTMAILBOX3),
	    ISP_READ(isp, OUTMAILBOX4));
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
			switch (mbs.param[0]) {
			case ASYNC_PDB_CHANGED:
				isp_mark_getpdb_all(isp);
				/* FALL THROUGH */
			case ASYNC_LIP_OCCURRED:
			case ASYNC_LOOP_UP:
			case ASYNC_LOOP_DOWN:
			case ASYNC_LOOP_RESET:
			case ASYNC_CHANGE_NOTIFY:
				if (once++ < 2) {
					goto again;
				}
				break;
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
		u_int16_t flags, period, offset, changed;
		int get;

		if (sdp->isp_devparam[tgt].dev_enable == 0) {
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
			get = 0;
		} else if (sdp->isp_devparam[tgt].dev_refresh) {
			mbs.param[0] = MBOX_GET_TARGET_PARAMS;
			sdp->isp_devparam[tgt].dev_refresh = 0;
			get = 1;
		} else {
			continue;
		}
		mbs.param[1] = tgt << 8;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			PRINTF("%s: failed to %cet SCSI parameters for "
			    "target %d\n", isp->isp_name, (get)? 'g' : 's',
			    tgt);
			continue;
		}

		if (get == 0) {
			/*
			 * XXX: Need a SYNC_TARGET for efficiency...
			 */
			isp->isp_sendmarker = 1;
			continue;
		}
		flags = mbs.param[2];
		period = mbs.param[3] & 0xff;
		offset = mbs.param[3] >> 8;
		if (sdp->isp_devparam[tgt].cur_dflags != flags ||
		    sdp->isp_devparam[tgt].cur_period != period ||
		    sdp->isp_devparam[tgt].cur_offset != offset) {
			IDPRINTF(3, ("%s: tgt %d flags 0x%x period %d "
			    "off %d\n", isp->isp_name, tgt, flags,
			    period, offset));
			changed = 1;
		} else {
			changed = 0;
		}
		sdp->isp_devparam[tgt].cur_dflags = flags;
		sdp->isp_devparam[tgt].cur_period = period;
		sdp->isp_devparam[tgt].cur_offset = offset;
		if (sdp->isp_devparam[tgt].dev_announced == 0 || changed) {
			if (isp_async(isp, ISPASYNC_NEW_TGT_PARAMS, &tgt))
				sdp->isp_devparam[tgt].dev_announced = 0;
			else
				sdp->isp_devparam[tgt].dev_announced = 1;
		}
	}
}

static void
isp_setdfltparm(isp)
	struct ispsoftc *isp;
{
	int tgt;
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
	if (IS_FC(isp)) {
		fcparam *fcp = (fcparam *) isp->isp_param;
		fcp->isp_maxfrmlen = ICB_DFLT_FRMLEN;
		fcp->isp_maxalloc = 256;
		fcp->isp_execthrottle = 16;
		fcp->isp_retry_delay = 5;
		fcp->isp_retry_count = 3;
		fcp->isp_loopid = DEFAULT_LOOPID;
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
		    (sdp->isp_clock && sdp->isp_clock < 60)) {
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
		if (((sdp->isp_clock && sdp->isp_clock < 60) ||
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

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		sdp->isp_devparam[tgt].exc_throttle = 16;
		sdp->isp_devparam[tgt].dev_enable = 1;
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
	isp->isp_gotdparms = 0;
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
		if (isp->isp_nactive > 0)
		    isp->isp_nactive--;
		XS_RESID(xs) = XS_XFRLEN(xs);
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

		sdp->isp_fast_mttr = ISP_NVRAM_FAST_MTTR_ENABLE(nvram_data);
		if (isp->isp_dblev > 2) {
			static char *true = "true";
			static char *false = "false";
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
			    sdp->isp_req_ack_active_neg? true : false);
			PRINTF("  Data Line Active Negation = %s\n",
			    sdp->isp_data_line_active_neg? true : false);
			PRINTF("      Data DMA Burst Enable = %s\n",
			    sdp->isp_data_dma_burst_enabl? true : false);
			PRINTF("       Cmd DMA Burst Enable = %s\n",
			    sdp->isp_cmd_dma_burst_enable? true : false);
			PRINTF("                  Fast MTTR = %s\n",
			    sdp->isp_fast_mttr? true : false);
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
			} wds;
			u_int64_t full64;
		} wwnstore;

		wwnstore.full64 = ISP2100_NVRAM_NODE_NAME(nvram_data);
		PRINTF("%s: Adapter WWN 0x%08x%08x\n", isp->isp_name,
		    wwnstore.wds.hi32, wwnstore.wds.lo32);
		fcp->isp_wwn = wwnstore.full64;
		wwnstore.full64 = ISP2100_NVRAM_BOOT_NODE_NAME(nvram_data);
		if (wwnstore.full64 != 0) {
			PRINTF("%s: BOOT DEVICE WWN 0x%08x%08x\n",
			    isp->isp_name, wwnstore.wds.hi32,
			    wwnstore.wds.lo32);
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
