/* $FreeBSD$ */
/*
 * Machine and OS Independent (well, as best as possible)
 * code for the Qlogic ISP SCSI adapters.
 *
 * Copyright (c) 1997, 1998, 1999, 2000, 2001 by Matthew Jacob
 * Feral Software
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
#ifdef	__svr4__
#include "isp_solaris.h"
#endif

/*
 * General defines
 */

#define	MBOX_DELAY_COUNT	1000000 / 100

/*
 * Local static data
 */
static const char portshift[] =
    "Target %d Loop ID 0x%x (Port 0x%x) => Loop 0x%x (Port 0x%x)";
static const char portdup[] =
    "Target %d duplicates Target %d- killing off both";
static const char retained[] =
    "Retaining Loop ID 0x%x for Target %d (Port 0x%x)";
static const char lretained[] =
    "Retained login of Target %d (Loop ID 0x%x) Port 0x%x";
static const char plogout[] =
    "Logging out Target %d at Loop ID 0x%x (Port 0x%x)";
static const char plogierr[] =
    "Command Error in PLOGI for Port 0x%x (0x%x)";
static const char nopdb[] =
    "Could not get PDB for Device @ Port 0x%x";
static const char pdbmfail1[] =
    "PDB Loop ID info for Device @ Port 0x%x does not match up (0x%x)";
static const char pdbmfail2[] =
    "PDB Port info for Device @ Port 0x%x does not match up (0x%x)";
static const char ldumped[] =
    "Target %d (Loop ID 0x%x) Port 0x%x dumped after login info mismatch";
static const char notresp[] =
  "Not RESPONSE in RESPONSE Queue (type 0x%x) @ idx %d (next %d) nlooked %d";
static const char xact1[] =
    "HBA attempted queued transaction with disconnect not set for %d.%d.%d";
static const char xact2[] =
    "HBA attempted queued transaction to target routine %d on target %d bus %d";
static const char xact3[] =
    "HBA attempted queued cmd for %d.%d.%d when queueing disabled";
static const char pskip[] =
    "SCSI phase skipped for target %d.%d.%d";
static const char topology[] =
    "Loop ID %d, AL_PA 0x%x, Port ID 0x%x, Loop State 0x%x, Topology '%s'";
static const char swrej[] =
    "Fabric Nameserver rejected %s (Reason=0x%x Expl=0x%x) for Port ID 0x%x";
static const char finmsg[] =
    "(%d.%d.%d): FIN dl%d resid %d STS 0x%x SKEY %c XS_ERR=0x%x";
static const char sc0[] =
    "%s CHAN %d FTHRSH %d IID %d RESETD %d RETRYC %d RETRYD %d ASD 0x%x";
static const char sc1[] =
    "%s RAAN 0x%x DLAN 0x%x DDMAB 0x%x CDMAB 0x%x SELTIME %d MQD %d";
static const char sc2[] = "%s CHAN %d TGT %d FLAGS 0x%x 0x%x/0x%x";
static const char sc3[] = "Generated";
static const char sc4[] = "NVRAM";
static const char bun[] =
    "bad underrun for %d.%d (count %d, resid %d, status %s)";

/*
 * Local function prototypes.
 */
static int isp_parse_async(struct ispsoftc *, u_int16_t);
static int isp_handle_other_response(struct ispsoftc *, int, isphdr_t *,
    u_int16_t *);
static void
isp_parse_status(struct ispsoftc *, ispstatusreq_t *, XS_T *);
static void isp_fastpost_complete(struct ispsoftc *, u_int16_t);
static int isp_mbox_continue(struct ispsoftc *);
static void isp_scsi_init(struct ispsoftc *);
static void isp_scsi_channel_init(struct ispsoftc *, int);
static void isp_fibre_init(struct ispsoftc *);
static void isp_mark_getpdb_all(struct ispsoftc *);
static int isp_getmap(struct ispsoftc *, fcpos_map_t *);
static int isp_getpdb(struct ispsoftc *, int, isp_pdb_t *);
static u_int64_t isp_get_portname(struct ispsoftc *, int, int);
static int isp_fclink_test(struct ispsoftc *, int);
static char *isp2100_fw_statename(int);
static int isp_pdb_sync(struct ispsoftc *);
static int isp_scan_loop(struct ispsoftc *);
static int isp_fabric_mbox_cmd(struct ispsoftc *, mbreg_t *);
static int isp_scan_fabric(struct ispsoftc *, int);
static void isp_register_fc4_type(struct ispsoftc *);
static void isp_fw_state(struct ispsoftc *);
static void isp_mboxcmd_qnw(struct ispsoftc *, mbreg_t *, int);
static void isp_mboxcmd(struct ispsoftc *, mbreg_t *, int);

static void isp_update(struct ispsoftc *);
static void isp_update_bus(struct ispsoftc *, int);
static void isp_setdfltparm(struct ispsoftc *, int);
static int isp_read_nvram(struct ispsoftc *);
static void isp_rdnvram_word(struct ispsoftc *, int, u_int16_t *);
static void isp_parse_nvram_1020(struct ispsoftc *, u_int8_t *);
static void isp_parse_nvram_1080(struct ispsoftc *, int, u_int8_t *);
static void isp_parse_nvram_12160(struct ispsoftc *, int, u_int8_t *);
static void isp_parse_nvram_2100(struct ispsoftc *, u_int8_t *);

/*
 * Reset Hardware.
 *
 * Hit the chip over the head, download new f/w if available and set it running.
 *
 * Locking done elsewhere.
 */

void
isp_reset(struct ispsoftc *isp)
{
	mbreg_t mbs;
	u_int16_t code_org;
	int loops, i, dodnld = 1;
	char *btype = "????";

	isp->isp_state = ISP_NILSTATE;

	/*
	 * Basic types (SCSI, FibreChannel and PCI or SBus)
	 * have been set in the MD code. We figure out more
	 * here. Possibly more refined types based upon PCI
	 * identification. Chip revision has been gathered.
	 *
	 * After we've fired this chip up, zero out the conf1 register
	 * for SCSI adapters and do other settings for the 2100.
	 */

	/*
	 * Get the current running firmware revision out of the
	 * chip before we hit it over the head (if this is our
	 * first time through). Note that we store this as the
	 * 'ROM' firmware revision- which it may not be. In any
	 * case, we don't really use this yet, but we may in
	 * the future.
	 */
	if (isp->isp_touched == 0) {
		/*
		 * First see whether or not we're sitting in the ISP PROM.
		 * If we've just been reset, we'll have the string "ISP   "
		 * spread through outgoing mailbox registers 1-3. We do
		 * this for PCI cards because otherwise we really don't
		 * know what state the card is in and we could hang if
		 * we try this command otherwise.
		 *
		 * For SBus cards, we just do this because they almost
		 * certainly will be running firmware by now.
		 */
		if (ISP_READ(isp, OUTMAILBOX1) != 0x4953 ||
		    ISP_READ(isp, OUTMAILBOX2) != 0x5020 ||
		    ISP_READ(isp, OUTMAILBOX3) != 0x2020) {
			/*
			 * Just in case it was paused...
			 */
			ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE);
			mbs.param[0] = MBOX_ABOUT_FIRMWARE;
			isp_mboxcmd(isp, &mbs, MBLOGNONE);
			if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
				isp->isp_romfw_rev[0] = mbs.param[1];
				isp->isp_romfw_rev[1] = mbs.param[2];
				isp->isp_romfw_rev[2] = mbs.param[3];
			}
		}
		isp->isp_touched = 1;
	}

	DISABLE_INTS(isp);

	/*
	 * Set up default request/response queue in-pointer/out-pointer
	 * register indices.
	 */
	if (IS_23XX(isp)) {
		isp->isp_rqstinrp = BIU_REQINP;
		isp->isp_rqstoutrp = BIU_REQOUTP;
		isp->isp_respinrp = BIU_RSPINP;
		isp->isp_respoutrp = BIU_RSPOUTP;
	} else {
		isp->isp_rqstinrp = INMAILBOX4;
		isp->isp_rqstoutrp = OUTMAILBOX4;
		isp->isp_respinrp = OUTMAILBOX5;
		isp->isp_respoutrp = INMAILBOX5;
	}

	/*
	 * Put the board into PAUSE mode (so we can read the SXP registers
	 * or write FPM/FBM registers).
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);

	if (IS_FC(isp)) {
		switch (isp->isp_type) {
		case ISP_HA_FC_2100:
			btype = "2100";
			break;
		case ISP_HA_FC_2200:
			btype = "2200";
			break;
		case ISP_HA_FC_2300:
			btype = "2300";
			break;
		case ISP_HA_FC_2312:
			btype = "2312";
			break;
		default:
			break;
		}
		/*
		 * While we're paused, reset the FPM module and FBM fifos.
		 */
		ISP_WRITE(isp, BIU2100_CSR, BIU2100_FPM0_REGS);
		ISP_WRITE(isp, FPM_DIAG_CONFIG, FPM_SOFT_RESET);
		ISP_WRITE(isp, BIU2100_CSR, BIU2100_FB_REGS);
		ISP_WRITE(isp, FBM_CMD, FBMCMD_FIFO_RESET_ALL);
		ISP_WRITE(isp, BIU2100_CSR, BIU2100_RISC_REGS);
	} else if (IS_1240(isp)) {
		sdparam *sdp = isp->isp_param;
		btype = "1240";
		isp->isp_clock = 60;
		sdp->isp_ultramode = 1;
		sdp++;
		sdp->isp_ultramode = 1;
		/*
		 * XXX: Should probably do some bus sensing.
		 */
	} else if (IS_ULTRA2(isp)) {
		static const char m[] = "bus %d is in %s Mode";
		u_int16_t l;
		sdparam *sdp = isp->isp_param;

		isp->isp_clock = 100;

		if (IS_1280(isp))
			btype = "1280";
		else if (IS_1080(isp))
			btype = "1080";
		else if (IS_10160(isp))
			btype = "10160";
		else if (IS_12160(isp))
			btype = "12160";
		else
			btype = "<UNKLVD>";

		l = ISP_READ(isp, SXP_PINS_DIFF) & ISP1080_MODE_MASK;
		switch (l) {
		case ISP1080_LVD_MODE:
			sdp->isp_lvdmode = 1;
			isp_prt(isp, ISP_LOGCONFIG, m, 0, "LVD");
			break;
		case ISP1080_HVD_MODE:
			sdp->isp_diffmode = 1;
			isp_prt(isp, ISP_LOGCONFIG, m, 0, "Differential");
			break;
		case ISP1080_SE_MODE:
			sdp->isp_ultramode = 1;
			isp_prt(isp, ISP_LOGCONFIG, m, 0, "Single-Ended");
			break;
		default:
			isp_prt(isp, ISP_LOGERR,
			    "unknown mode on bus %d (0x%x)", 0, l);
			break;
		}

		if (IS_DUALBUS(isp)) {
			sdp++;
			l = ISP_READ(isp, SXP_PINS_DIFF|SXP_BANK1_SELECT);
			l &= ISP1080_MODE_MASK;
			switch(l) {
			case ISP1080_LVD_MODE:
				sdp->isp_lvdmode = 1;
				isp_prt(isp, ISP_LOGCONFIG, m, 1, "LVD");
				break;
			case ISP1080_HVD_MODE:
				sdp->isp_diffmode = 1;
				isp_prt(isp, ISP_LOGCONFIG,
				    m, 1, "Differential");
				break;
			case ISP1080_SE_MODE:
				sdp->isp_ultramode = 1;
				isp_prt(isp, ISP_LOGCONFIG,
				    m, 1, "Single-Ended");
				break;
			default:
				isp_prt(isp, ISP_LOGERR,
				    "unknown mode on bus %d (0x%x)", 1, l);
				break;
			}
		}
	} else {
		sdparam *sdp = isp->isp_param;
		i = ISP_READ(isp, BIU_CONF0) & BIU_CONF0_HW_MASK;
		switch (i) {
		default:
			isp_prt(isp, ISP_LOGALL, "Unknown Chip Type 0x%x", i);
			/* FALLTHROUGH */
		case 1:
			btype = "1020";
			isp->isp_type = ISP_HA_SCSI_1020;
			isp->isp_clock = 40;
			break;
		case 2:
			/*
			 * Some 1020A chips are Ultra Capable, but don't
			 * run the clock rate up for that unless told to
			 * do so by the Ultra Capable bits being set.
			 */
			btype = "1020A";
			isp->isp_type = ISP_HA_SCSI_1020A;
			isp->isp_clock = 40;
			break;
		case 3:
			btype = "1040";
			isp->isp_type = ISP_HA_SCSI_1040;
			isp->isp_clock = 60;
			break;
		case 4:
			btype = "1040A";
			isp->isp_type = ISP_HA_SCSI_1040A;
			isp->isp_clock = 60;
			break;
		case 5:
			btype = "1040B";
			isp->isp_type = ISP_HA_SCSI_1040B;
			isp->isp_clock = 60;
			break;
		case 6:
			btype = "1040C";
			isp->isp_type = ISP_HA_SCSI_1040C;
			isp->isp_clock = 60;
                        break;
		}
		/*
		 * Now, while we're at it, gather info about ultra
		 * and/or differential mode.
		 */
		if (ISP_READ(isp, SXP_PINS_DIFF) & SXP_PINS_DIFF_MODE) {
			isp_prt(isp, ISP_LOGCONFIG, "Differential Mode");
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
			isp_prt(isp, ISP_LOGCONFIG, "Ultra Mode Capable");
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
	 * Clear instrumentation
	 */
	isp->isp_intcnt = isp->isp_intbogus = 0;

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
		USEC_DELAY(100);

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
		USEC_DELAY(100);

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
		USEC_DELAY(100);
		if (--loops < 0) {
			ISP_DUMPREGS(isp, "chip reset timed out");
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
	USEC_DELAY(100);
	/* Clear semaphore register (just to be sure) */
	ISP_WRITE(isp, BIU_SEMA, 0);

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
		if (IS_2200(isp) || IS_23XX(isp)) {
			ISP_WRITE(isp, HCCR, HCCR_2X00_DISABLE_PARITY_PAUSE);
		}
	}

	ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE); /* release paused processor */

	/*
	 * Do MD specific post initialization
	 */
	ISP_RESET1(isp);

	/*
	 * Wait for everything to finish firing up.
	 *
	 * Avoid doing this on the 2312 because you can generate a PCI
	 * parity error (chip breakage).
	 */
	if (IS_23XX(isp)) {
		USEC_DELAY(5);
	} else {
		loops = MBOX_DELAY_COUNT;
		while (ISP_READ(isp, OUTMAILBOX0) == MBOX_BUSY) {
			USEC_DELAY(100);
			if (--loops < 0) {
				isp_prt(isp, ISP_LOGERR,
				    "MBOX_BUSY never cleared on reset");
				return;
			}
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
	isp_mboxcmd(isp, &mbs, MBLOGALL);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}

	if (IS_SCSI(isp)) {
		mbs.param[0] = MBOX_MAILBOX_REG_TEST;
		mbs.param[1] = 0xdead;
		mbs.param[2] = 0xbeef;
		mbs.param[3] = 0xffff;
		mbs.param[4] = 0x1111;
		mbs.param[5] = 0xa5a5;
		isp_mboxcmd(isp, &mbs, MBLOGALL);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		if (mbs.param[1] != 0xdead || mbs.param[2] != 0xbeef ||
		    mbs.param[3] != 0xffff || mbs.param[4] != 0x1111 ||
		    mbs.param[5] != 0xa5a5) {
			isp_prt(isp, ISP_LOGERR,
			    "Register Test Failed (0x%x 0x%x 0x%x 0x%x 0x%x)",
			    mbs.param[1], mbs.param[2], mbs.param[3],
			    mbs.param[4], mbs.param[5]);
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
	if ((isp->isp_mdvec->dv_ispfw == NULL) ||
	    (isp->isp_confopts & ISP_CFG_NORELOAD)) {
		dodnld = 0;
	}

	if (IS_23XX(isp))
		code_org = ISP_CODE_ORG_2300;
	else
		code_org = ISP_CODE_ORG;

	if (dodnld) {
		isp->isp_mbxworkp = (void *) &isp->isp_mdvec->dv_ispfw[1];
		isp->isp_mbxwrk0 = isp->isp_mdvec->dv_ispfw[3] - 1;
		isp->isp_mbxwrk1 = code_org + 1;
		mbs.param[0] = MBOX_WRITE_RAM_WORD;
		mbs.param[1] = code_org;
		mbs.param[2] = isp->isp_mdvec->dv_ispfw[0];
		isp_mboxcmd(isp, &mbs, MBLOGNONE);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGERR,
			    "F/W download failed at word %d",
			    isp->isp_mbxwrk1 - code_org);
			dodnld = 0;
			goto again;
		}
		/*
		 * Verify that it downloaded correctly.
		 */
		mbs.param[0] = MBOX_VERIFY_CHECKSUM;
		mbs.param[1] = code_org;
		isp_mboxcmd(isp, &mbs, MBLOGNONE);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGERR, "Ram Checksum Failure");
			return;
		}
		isp->isp_loaded_fw = 1;
	} else {
		isp->isp_loaded_fw = 0;
		isp_prt(isp, ISP_LOGDEBUG2, "skipping f/w download");
	}

	/*
	 * Now start it rolling.
	 *
	 * If we didn't actually download f/w,
	 * we still need to (re)start it.
	 */


	mbs.param[0] = MBOX_EXEC_FIRMWARE;
	mbs.param[1] = code_org;
	isp_mboxcmd(isp, &mbs, MBLOGNONE);
	/*
	 * Give it a chance to start.
	 */
	USEC_DELAY(500);

	if (IS_SCSI(isp)) {
		/*
		 * Set CLOCK RATE, but only if asked to.
		 */
		if (isp->isp_clock) {
			mbs.param[0] = MBOX_SET_CLOCK_RATE;
			mbs.param[1] = isp->isp_clock;
			isp_mboxcmd(isp, &mbs, MBLOGALL);
			/* we will try not to care if this fails */
		}
	}

	mbs.param[0] = MBOX_ABOUT_FIRMWARE;
	isp_mboxcmd(isp, &mbs, MBLOGALL);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}

	/*
	 * The SBus firmware that we are using apparently does not return
	 * major, minor, micro revisions in the mailbox registers, which
	 * is really, really, annoying.
	 */
	if (ISP_SBUS_SUPPORTED && isp->isp_bustype == ISP_BT_SBUS) {
		if (dodnld) {
#ifdef	ISP_TARGET_MODE
			isp->isp_fwrev[0] = 7;
			isp->isp_fwrev[1] = 55;
#else
			isp->isp_fwrev[0] = 1;
			isp->isp_fwrev[1] = 37;
#endif
			isp->isp_fwrev[2] = 0;
		} 
	} else {
		isp->isp_fwrev[0] = mbs.param[1];
		isp->isp_fwrev[1] = mbs.param[2];
		isp->isp_fwrev[2] = mbs.param[3];
	}
	isp_prt(isp, ISP_LOGCONFIG,
	    "Board Type %s, Chip Revision 0x%x, %s F/W Revision %d.%d.%d",
	    btype, isp->isp_revision, dodnld? "loaded" : "resident",
	    isp->isp_fwrev[0], isp->isp_fwrev[1], isp->isp_fwrev[2]);

	if (IS_FC(isp)) {
		/*
		 * We do not believe firmware attributes for 2100 code less
		 * than 1.17.0, unless it's the firmware we specifically
		 * are loading.
		 *
		 * Note that all 22XX and 23XX f/w is greater than 1.X.0.
		 */
		if (!(ISP_FW_NEWER_THAN(isp, 1, 17, 0))) {
#ifdef	USE_SMALLER_2100_FIRMWARE
			FCPARAM(isp)->isp_fwattr = ISP_FW_ATTR_SCCLUN;
#else
			FCPARAM(isp)->isp_fwattr = 0;
#endif
		} else {
			FCPARAM(isp)->isp_fwattr = mbs.param[6];
			isp_prt(isp, ISP_LOGDEBUG0,
			    "Firmware Attributes = 0x%x", mbs.param[6]);
		}
		if (ISP_READ(isp, BIU2100_CSR) & BIU2100_PCI64) {
			isp_prt(isp, ISP_LOGCONFIG,
			    "Installed in 64-Bit PCI slot");
		}
	}

	if (isp->isp_romfw_rev[0] || isp->isp_romfw_rev[1] ||
	    isp->isp_romfw_rev[2]) {
		isp_prt(isp, ISP_LOGCONFIG, "Last F/W revision was %d.%d.%d",
		    isp->isp_romfw_rev[0], isp->isp_romfw_rev[1],
		    isp->isp_romfw_rev[2]);
	}

	mbs.param[0] = MBOX_GET_FIRMWARE_STATUS;
	isp_mboxcmd(isp, &mbs, MBLOGALL);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}
	isp->isp_maxcmds = mbs.param[2];
	isp_prt(isp, ISP_LOGINFO,
	    "%d max I/O commands supported", mbs.param[2]);
	isp_fw_state(isp);

	/*
	 * Set up DMA for the request and result mailboxes.
	 */
	if (ISP_MBOXDMASETUP(isp) != 0) {
		isp_prt(isp, ISP_LOGERR, "Cannot setup DMA");
		return;
	}
	isp->isp_state = ISP_RESETSTATE;

	/*
	 * Okay- now that we have new firmware running, we now (re)set our
	 * notion of how many luns we support. This is somewhat tricky because
	 * if we haven't loaded firmware, we sometimes do not have an easy way
	 * of knowing how many luns we support.
	 *
	 * Expanded lun firmware gives you 32 luns for SCSI cards and
	 * 16384 luns for Fibre Channel cards.
	 *
	 * It turns out that even for QLogic 2100s with ROM 1.10 and above
	 * we do get a firmware attributes word returned in mailbox register 6.
	 *
	 * Because the lun is in a different position in the Request Queue
	 * Entry structure for Fibre Channel with expanded lun firmware, we
	 * can only support one lun (lun zero) when we don't know what kind
	 * of firmware we're running.
	 */
	if (IS_SCSI(isp)) {
		if (dodnld) {
			if (IS_ULTRA2(isp) || IS_ULTRA3(isp)) {
				isp->isp_maxluns = 32;
			} else {
				isp->isp_maxluns = 8;
			}
		} else {
			isp->isp_maxluns = 8;
		}
	} else {
		if (FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_SCCLUN) {
			isp->isp_maxluns = 16384;
		} else {
			isp->isp_maxluns = 16;
		}
	}
}

/*
 * Initialize Parameters of Hardware to a known state.
 *
 * Locks are held before coming here.
 */

void
isp_init(struct ispsoftc *isp)
{
	/*
	 * Must do this first to get defaults established.
	 */
	isp_setdfltparm(isp, 0);
	if (IS_DUALBUS(isp)) {
		isp_setdfltparm(isp, 1);
	}
	if (IS_FC(isp)) {
		isp_fibre_init(isp);
	} else {
		isp_scsi_init(isp);
	}
}

static void
isp_scsi_init(struct ispsoftc *isp)
{
	sdparam *sdp_chan0, *sdp_chan1;
	mbreg_t mbs;

	sdp_chan0 = isp->isp_param;
	sdp_chan1 = sdp_chan0;
	if (IS_DUALBUS(isp)) {
		sdp_chan1++;
	}

	/*
	 * If we have no role (neither target nor initiator), return.
	 */
	if (isp->isp_role == ISP_ROLE_NONE) {
		return;
	}

	/* First do overall per-card settings. */

	/*
	 * If we have fast memory timing enabled, turn it on.
	 */
	if (sdp_chan0->isp_fast_mttr) {
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

	isp_mboxcmd(isp, &mbs, MBLOGALL);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}

	/*
	 * Set ASYNC DATA SETUP time. This is very important.
	 */
	mbs.param[0] = MBOX_SET_ASYNC_DATA_SETUP_TIME;
	mbs.param[1] = sdp_chan0->isp_async_data_setup;
	mbs.param[2] = sdp_chan1->isp_async_data_setup;
	isp_mboxcmd(isp, &mbs, MBLOGALL);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
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

	isp_mboxcmd(isp, &mbs, MBLOGNONE);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_prt(isp, ISP_LOGERR,
		    "failed to set active negation state (%d,%d), (%d,%d)",
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
	isp_mboxcmd(isp, &mbs, MBLOGALL);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_prt(isp, ISP_LOGERR, "failed to set tag age limit (%d,%d)",
		    sdp_chan0->isp_tag_aging, sdp_chan1->isp_tag_aging);
		return;
	}

	/*
	 * Set selection timeout.
	 */
	mbs.param[0] = MBOX_SET_SELECT_TIMEOUT;
	mbs.param[1] = sdp_chan0->isp_selection_timeout;
	mbs.param[2] = sdp_chan1->isp_selection_timeout;
	isp_mboxcmd(isp, &mbs, MBLOGALL);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}

	/* now do per-channel settings */
	isp_scsi_channel_init(isp, 0);
	if (IS_DUALBUS(isp))
		isp_scsi_channel_init(isp, 1);

	/*
	 * Now enable request/response queues
	 */

	if (IS_ULTRA2(isp) || IS_1240(isp)) {
		mbs.param[0] = MBOX_INIT_RES_QUEUE_A64;
		mbs.param[1] = RESULT_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_result_dma);
		mbs.param[3] = DMA_WD0(isp->isp_result_dma);
		mbs.param[4] = 0;
		mbs.param[6] = DMA_WD3(isp->isp_result_dma);
		mbs.param[7] = DMA_WD2(isp->isp_result_dma);
		isp_mboxcmd(isp, &mbs, MBLOGALL);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		isp->isp_residx = mbs.param[5];

		mbs.param[0] = MBOX_INIT_REQ_QUEUE_A64;
		mbs.param[1] = RQUEST_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
		mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
		mbs.param[5] = 0;
		mbs.param[6] = DMA_WD3(isp->isp_result_dma);
		mbs.param[7] = DMA_WD2(isp->isp_result_dma);
		isp_mboxcmd(isp, &mbs, MBLOGALL);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		isp->isp_reqidx = isp->isp_reqodx = mbs.param[4];
	} else {
		mbs.param[0] = MBOX_INIT_RES_QUEUE;
		mbs.param[1] = RESULT_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_result_dma);
		mbs.param[3] = DMA_WD0(isp->isp_result_dma);
		mbs.param[4] = 0;
		isp_mboxcmd(isp, &mbs, MBLOGALL);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		isp->isp_residx = mbs.param[5];

		mbs.param[0] = MBOX_INIT_REQ_QUEUE;
		mbs.param[1] = RQUEST_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
		mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
		mbs.param[5] = 0;
		isp_mboxcmd(isp, &mbs, MBLOGALL);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		isp->isp_reqidx = isp->isp_reqodx = mbs.param[4];
	}

	/*
	 * Turn on Fast Posting, LVD transitions
	 *
	 * Ultra2 F/W always has had fast posting (and LVD transitions)
	 *
	 * Ultra and older (i.e., SBus) cards may not. It's just safer
	 * to assume not for them.
	 */

	mbs.param[0] = MBOX_SET_FW_FEATURES;
	mbs.param[1] = 0;
	if (IS_ULTRA2(isp))
		mbs.param[1] |= FW_FEATURE_LVD_NOTIFY;
#ifndef	ISP_NO_RIO
	if (IS_ULTRA2(isp) || IS_1240(isp))
		mbs.param[1] |= FW_FEATURE_RIO_16BIT;
#else
#ifndef	ISP_NO_FASTPOST
	if (IS_ULTRA2(isp) || IS_1240(isp))
		mbs.param[1] |= FW_FEATURE_FAST_POST;
#endif
#endif
	if (mbs.param[1] != 0) {
		u_int16_t sfeat = mbs.param[1];
		isp_mboxcmd(isp, &mbs, MBLOGALL);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGINFO,
			    "Enabled FW features (0x%x)", sfeat);
		}
	}

	/*
	 * Let the outer layers decide whether to issue a SCSI bus reset.
	 */
	isp->isp_state = ISP_INITSTATE;
}

static void
isp_scsi_channel_init(struct ispsoftc *isp, int channel)
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
	isp_mboxcmd(isp, &mbs, MBLOGALL);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}
	isp_prt(isp, ISP_LOGINFO, "Initiator ID is %d on Channel %d",
	    sdp->isp_initiator_id, channel);


	/*
	 * Set current per-target parameters to an initial safe minimum.
	 */
	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		int lun;
		u_int16_t sdf;

		if (sdp->isp_devparam[tgt].dev_enable == 0) {
			continue;
		}
#ifndef	ISP_TARGET_MODE
		sdf = sdp->isp_devparam[tgt].goal_flags;
		sdf &= DPARM_SAFE_DFLT;
		/*
		 * It is not quite clear when this changed over so that
		 * we could force narrow and async for 1000/1020 cards,
		 * but assume that this is only the case for loaded
		 * firmware.
		 */
		if (isp->isp_loaded_fw) {
			sdf |= DPARM_NARROW | DPARM_ASYNC;
		}
#else
		/*
		 * The !$*!)$!$)* f/w uses the same index into some
		 * internal table to decide how to respond to negotiations,
		 * so if we've said "let's be safe" for ID X, and ID X
		 * selects *us*, the negotiations will back to 'safe'
		 * (as in narrow/async). What the f/w *should* do is
		 * use the initiator id settings to decide how to respond.
		 */
		sdp->isp_devparam[tgt].goal_flags = sdf = DPARM_DEFAULT;
#endif
		mbs.param[0] = MBOX_SET_TARGET_PARAMS;
		mbs.param[1] = (channel << 15) | (tgt << 8);
		mbs.param[2] = sdf;
		if ((sdf & DPARM_SYNC) == 0) {
			mbs.param[3] = 0;
		} else {
			mbs.param[3] =
			    (sdp->isp_devparam[tgt].goal_offset << 8) |
			    (sdp->isp_devparam[tgt].goal_period);
		}
		isp_prt(isp, ISP_LOGDEBUG0,
		    "Initial Settings bus%d tgt%d flags 0x%x off 0x%x per 0x%x",
		    channel, tgt, mbs.param[2], mbs.param[3] >> 8,
		    mbs.param[3] & 0xff);
		isp_mboxcmd(isp, &mbs, MBLOGNONE);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			sdf = DPARM_SAFE_DFLT;
			mbs.param[0] = MBOX_SET_TARGET_PARAMS;
			mbs.param[1] = (tgt << 8) | (channel << 15);
			mbs.param[2] = sdf;
			mbs.param[3] = 0;
			isp_mboxcmd(isp, &mbs, MBLOGALL);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				continue;
			}
		}

		/*
		 * We don't update any information directly from the f/w
		 * because we need to run at least one command to cause a
		 * new state to be latched up. So, we just assume that we
		 * converge to the values we just had set.
		 *
		 * Ensure that we don't believe tagged queuing is enabled yet.
		 * It turns out that sometimes the ISP just ignores our
		 * attempts to set parameters for devices that it hasn't
		 * seen yet.
		 */
		sdp->isp_devparam[tgt].actv_flags = sdf & ~DPARM_TQING;
		for (lun = 0; lun < (int) isp->isp_maxluns; lun++) {
			mbs.param[0] = MBOX_SET_DEV_QUEUE_PARAMS;
			mbs.param[1] = (channel << 15) | (tgt << 8) | lun;
			mbs.param[2] = sdp->isp_max_queue_depth;
			mbs.param[3] = sdp->isp_devparam[tgt].exc_throttle;
			isp_mboxcmd(isp, &mbs, MBLOGALL);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				break;
			}
		}
	}
	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		if (sdp->isp_devparam[tgt].dev_refresh) {
			isp->isp_sendmarker |= (1 << channel);
			isp->isp_update |= (1 << channel);
			break;
		}
	}
}

/*
 * Fibre Channel specific initialization.
 *
 * Locks are held before coming here.
 */
static void
isp_fibre_init(struct ispsoftc *isp)
{
	fcparam *fcp;
	isp_icb_t local, *icbp = &local;
	mbreg_t mbs;
	int loopid;
	u_int64_t nwwn, pwwn;

	fcp = isp->isp_param;

	/*
	 * Do this *before* initializing the firmware.
	 */
	isp_mark_getpdb_all(isp);
	fcp->isp_fwstate = FW_CONFIG_WAIT;
	fcp->isp_loopstate = LOOP_NIL;

	/*
	 * If we have no role (neither target nor initiator), return.
	 */
	if (isp->isp_role == ISP_ROLE_NONE) {
		return;
	}

	loopid = fcp->isp_loopid;
	MEMZERO(icbp, sizeof (*icbp));
	icbp->icb_version = ICB_VERSION1;

	/*
	 * Firmware Options are either retrieved from NVRAM or
	 * are patched elsewhere. We check them for sanity here
	 * and make changes based on board revision, but otherwise
	 * let others decide policy.
	 */

	/*
	 * If this is a 2100 < revision 5, we have to turn off FAIRNESS.
	 */
	if ((isp->isp_type == ISP_HA_FC_2100) && isp->isp_revision < 5) {
		fcp->isp_fwoptions &= ~ICBOPT_FAIRNESS;
	}

	/*
	 * We have to use FULL LOGIN even though it resets the loop too much
	 * because otherwise port database entries don't get updated after
	 * a LIP- this is a known f/w bug for 2100 f/w less than 1.17.0.
	 */
	if (!ISP_FW_NEWER_THAN(isp, 1, 17, 0)) {
		fcp->isp_fwoptions |= ICBOPT_FULL_LOGIN;
	}

	/*
	 * Insist on Port Database Update Async notifications
	 */
	fcp->isp_fwoptions |= ICBOPT_PDBCHANGE_AE;

	/*
	 * Make sure that target role reflects into fwoptions.
	 */
	if (isp->isp_role & ISP_ROLE_TARGET) {
		fcp->isp_fwoptions |= ICBOPT_TGT_ENABLE;
	} else {
		fcp->isp_fwoptions &= ~ICBOPT_TGT_ENABLE;
	}

	/*
	 * Propagate all of this into the ICB structure.
	 */
	icbp->icb_fwoptions = fcp->isp_fwoptions;
	icbp->icb_maxfrmlen = fcp->isp_maxfrmlen;
	if (icbp->icb_maxfrmlen < ICB_MIN_FRMLEN ||
	    icbp->icb_maxfrmlen > ICB_MAX_FRMLEN) {
		isp_prt(isp, ISP_LOGERR,
		    "bad frame length (%d) from NVRAM- using %d",
		    fcp->isp_maxfrmlen, ICB_DFLT_FRMLEN);
		icbp->icb_maxfrmlen = ICB_DFLT_FRMLEN;
	}
	icbp->icb_maxalloc = fcp->isp_maxalloc;
	if (icbp->icb_maxalloc < 1) {
		isp_prt(isp, ISP_LOGERR,
		    "bad maximum allocation (%d)- using 16", fcp->isp_maxalloc);
		icbp->icb_maxalloc = 16;
	}
	icbp->icb_execthrottle = fcp->isp_execthrottle;
	if (icbp->icb_execthrottle < 1) {
		isp_prt(isp, ISP_LOGERR,
		    "bad execution throttle of %d- using 16",
		    fcp->isp_execthrottle);
		icbp->icb_execthrottle = ICB_DFLT_THROTTLE;
	}
	icbp->icb_retry_delay = fcp->isp_retry_delay;
	icbp->icb_retry_count = fcp->isp_retry_count;
	icbp->icb_hardaddr = loopid;
	/*
	 * Right now we just set extended options to prefer point-to-point
	 * over loop based upon some soft config options.
	 * 
	 * NB: for the 2300, ICBOPT_EXTENDED is required.
	 */
	if (IS_2200(isp) || IS_23XX(isp)) {
		icbp->icb_fwoptions |= ICBOPT_EXTENDED;
		/*
		 * Prefer or force Point-To-Point instead Loop?
		 */
		switch(isp->isp_confopts & ISP_CFG_PORT_PREF) {
		case ISP_CFG_NPORT:
			icbp->icb_xfwoptions |= ICBXOPT_PTP_2_LOOP;
			break;
		case ISP_CFG_NPORT_ONLY:
			icbp->icb_xfwoptions |= ICBXOPT_PTP_ONLY;
			break;
		case ISP_CFG_LPORT_ONLY:
			icbp->icb_xfwoptions |= ICBXOPT_LOOP_ONLY;
			break;
		default:
			icbp->icb_xfwoptions |= ICBXOPT_LOOP_2_PTP;
			break;
		}
		if (IS_23XX(isp)) {
			/*
			 * QLogic recommends that FAST Posting be turned
			 * off for 23XX cards and instead allow the HBA
			 * to write response queue entries and interrupt
			 * after a delay (ZIO).
			 *
			 * If we set ZIO, it will disable fast posting,
			 * so we don't need to clear it in fwoptions.
			 */
			icbp->icb_xfwoptions |= ICBXOPT_ZIO;

			if (isp->isp_confopts & ISP_CFG_ONEGB) {
				icbp->icb_zfwoptions |= ICBZOPT_RATE_ONEGB;
			} else if (isp->isp_confopts & ISP_CFG_TWOGB) {
				icbp->icb_zfwoptions |= ICBZOPT_RATE_TWOGB;
			} else {
				icbp->icb_zfwoptions |= ICBZOPT_RATE_AUTO;
			}
		}
	}

#ifndef	ISP_NO_RIO_FC
	/*
	 * RIO seems to be enabled in 2100s for fw >= 1.17.0.
	 *
	 * I've had some questionable problems with RIO on 2200.
	 * More specifically, on a 2204 I had problems with RIO
	 * on a Linux system where I was dropping commands right
	 * and left. It's not clear to me what the actual problem
	 * was.
	 *
	 * 23XX Cards do not support RIO. Instead they support ZIO.
	 */
#if	0
	if (!IS_23XX(isp) && ISP_FW_NEWER_THAN(isp, 1, 17, 0)) {
		icbp->icb_xfwoptions |= ICBXOPT_RIO_16BIT;
		icbp->icb_racctimer = 4;
		icbp->icb_idelaytimer = 8;
	}
#endif
#endif

	/*
	 * For 22XX > 2.1.26 && 23XX, set someoptions.
	 * XXX: Probably okay for newer 2100 f/w too.
	 */
	if (ISP_FW_NEWER_THAN(isp, 2, 26, 0)) {
		/*
		 * Turn on LIP F8 async event (1)
		 * Turn on generate AE 8013 on all LIP Resets (2)
		 * Disable LIP F7 switching (8)
		 */
		mbs.param[0] = MBOX_SET_FIRMWARE_OPTIONS;
		mbs.param[1] = 0xb;
		mbs.param[2] = 0;
		mbs.param[3] = 0;
		isp_mboxcmd(isp, &mbs, MBLOGALL);
	}
	icbp->icb_logintime = 30;	/* 30 second login timeout */

	if (IS_23XX(isp)) {
		ISP_WRITE(isp, isp->isp_rqstinrp, 0);
        	ISP_WRITE(isp, isp->isp_rqstoutrp, 0);
        	ISP_WRITE(isp, isp->isp_respinrp, 0);
		ISP_WRITE(isp, isp->isp_respoutrp, 0);
	}

	nwwn = ISP_NODEWWN(isp);
	pwwn = ISP_PORTWWN(isp);
	if (nwwn && pwwn) {
		icbp->icb_fwoptions |= ICBOPT_BOTH_WWNS;
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_nodename, nwwn);
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_portname, pwwn);
		isp_prt(isp, ISP_LOGDEBUG1,
		    "Setting ICB Node 0x%08x%08x Port 0x%08x%08x",
		    ((u_int32_t) (nwwn >> 32)),
		    ((u_int32_t) (nwwn & 0xffffffff)),
		    ((u_int32_t) (pwwn >> 32)),
		    ((u_int32_t) (pwwn & 0xffffffff)));
	} else {
		isp_prt(isp, ISP_LOGDEBUG1, "Not using any WWNs");
		icbp->icb_fwoptions &= ~(ICBOPT_BOTH_WWNS|ICBOPT_FULL_LOGIN);
	}
	icbp->icb_rqstqlen = RQUEST_QUEUE_LEN(isp);
	icbp->icb_rsltqlen = RESULT_QUEUE_LEN(isp);
	icbp->icb_rqstaddr[RQRSP_ADDR0015] = DMA_WD0(isp->isp_rquest_dma);
	icbp->icb_rqstaddr[RQRSP_ADDR1631] = DMA_WD1(isp->isp_rquest_dma);
	icbp->icb_rqstaddr[RQRSP_ADDR3247] = DMA_WD2(isp->isp_rquest_dma);
	icbp->icb_rqstaddr[RQRSP_ADDR4863] = DMA_WD3(isp->isp_rquest_dma);
	icbp->icb_respaddr[RQRSP_ADDR0015] = DMA_WD0(isp->isp_result_dma);
	icbp->icb_respaddr[RQRSP_ADDR1631] = DMA_WD1(isp->isp_result_dma);
	icbp->icb_respaddr[RQRSP_ADDR3247] = DMA_WD2(isp->isp_result_dma);
	icbp->icb_respaddr[RQRSP_ADDR4863] = DMA_WD3(isp->isp_result_dma);
	isp_prt(isp, ISP_LOGDEBUG0,
	    "isp_fibre_init: fwopt 0x%x xfwopt 0x%x zfwopt 0x%x",
	    icbp->icb_fwoptions, icbp->icb_xfwoptions, icbp->icb_zfwoptions);

	FC_SCRATCH_ACQUIRE(isp);
	isp_put_icb(isp, icbp, (isp_icb_t *)fcp->isp_scratch);

	/*
	 * Init the firmware
	 */
	mbs.param[0] = MBOX_INIT_FIRMWARE;
	mbs.param[1] = 0;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[4] = 0;
	mbs.param[5] = 0;
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	isp_mboxcmd(isp, &mbs, MBLOGALL);
	FC_SCRATCH_RELEASE(isp);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}
	isp->isp_reqidx = isp->isp_reqodx = 0;
	isp->isp_residx = 0;
	isp->isp_sendmarker = 1;

	/*
	 * Whatever happens, we're now committed to being here.
	 */
	isp->isp_state = ISP_INITSTATE;
}

/*
 * Fibre Channel Support- get the port database for the id.
 *
 * Locks are held before coming here. Return 0 if success,
 * else failure.
 */

static int
isp_getmap(struct ispsoftc *isp, fcpos_map_t *map)
{
	fcparam *fcp = (fcparam *) isp->isp_param;
	mbreg_t mbs;

	mbs.param[0] = MBOX_GET_FC_AL_POSITION_MAP;
	mbs.param[1] = 0;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	/*
	 * Unneeded. For the 2100, except for initializing f/w, registers
	 * 4/5 have to not be written to.
	 *	mbs.param[4] = 0;
	 *	mbs.param[5] = 0;
	 *
	 */
	mbs.param[6] = 0;
	mbs.param[7] = 0;
	FC_SCRATCH_ACQUIRE(isp);
	isp_mboxcmd(isp, &mbs, MBLOGALL & ~MBOX_COMMAND_PARAM_ERROR);
	if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
		MEMCPY(map, fcp->isp_scratch, sizeof (fcpos_map_t));
		map->fwmap = mbs.param[1] != 0;
		FC_SCRATCH_RELEASE(isp);
		return (0);
	}
	FC_SCRATCH_RELEASE(isp);
	return (-1);
}

static void
isp_mark_getpdb_all(struct ispsoftc *isp)
{
	fcparam *fcp = (fcparam *) isp->isp_param;
	int i;
	for (i = 0; i < MAX_FC_TARG; i++) {
		fcp->portdb[i].valid = fcp->portdb[i].fabric_dev = 0;
	}
}

static int
isp_getpdb(struct ispsoftc *isp, int id, isp_pdb_t *pdbp)
{
	fcparam *fcp = (fcparam *) isp->isp_param;
	mbreg_t mbs;

	mbs.param[0] = MBOX_GET_PORT_DB;
	mbs.param[1] = id << 8;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	/*
	 * Unneeded. For the 2100, except for initializing f/w, registers
	 * 4/5 have to not be written to.
	 *	mbs.param[4] = 0;
	 *	mbs.param[5] = 0;
	 *
	 */
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	FC_SCRATCH_ACQUIRE(isp);
	isp_mboxcmd(isp, &mbs, MBLOGALL & ~MBOX_COMMAND_PARAM_ERROR);
	if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
		isp_get_pdb(isp, (isp_pdb_t *)fcp->isp_scratch, pdbp);
		FC_SCRATCH_RELEASE(isp);
		return (0);
	}
	FC_SCRATCH_RELEASE(isp);
	return (-1);
}

static u_int64_t
isp_get_portname(struct ispsoftc *isp, int loopid, int nodename)
{
	u_int64_t wwn = 0;
	mbreg_t mbs;

	mbs.param[0] = MBOX_GET_PORT_NAME;
	mbs.param[1] = loopid << 8;
	if (nodename)
		mbs.param[1] |= 1;
	isp_mboxcmd(isp, &mbs, MBLOGALL & ~MBOX_COMMAND_PARAM_ERROR);
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
isp_fclink_test(struct ispsoftc *isp, int usdelay)
{
	static char *toponames[] = {
		"Private Loop",
		"FL Port",
		"N-Port to N-Port",
		"F Port",
		"F Port (no FLOGI_ACC response)"
	};
	mbreg_t mbs;
	int count, check_for_fabric;
	u_int8_t lwfs;
	fcparam *fcp;
	struct lportdb *lp;
	isp_pdb_t pdb;

	fcp = isp->isp_param;

	/*
	 * XXX: Here is where we would start a 'loop dead' timeout
	 */

	/*
	 * Wait up to N microseconds for F/W to go to a ready state.
	 */
	lwfs = FW_CONFIG_WAIT;
	count = 0;
	while (count < usdelay) {
		u_int64_t enano;
		u_int32_t wrk;
		NANOTIME_T hra, hrb;

		GET_NANOTIME(&hra);
		isp_fw_state(isp);
		if (lwfs != fcp->isp_fwstate) {
			isp_prt(isp, ISP_LOGINFO, "Firmware State <%s->%s>",
			    isp2100_fw_statename((int)lwfs),
			    isp2100_fw_statename((int)fcp->isp_fwstate));
			lwfs = fcp->isp_fwstate;
		}
		if (fcp->isp_fwstate == FW_READY) {
			break;
		}
		GET_NANOTIME(&hrb);

		/*
		 * Get the elapsed time in nanoseconds.
		 * Always guaranteed to be non-zero.
		 */
		enano = NANOTIME_SUB(&hrb, &hra);

		isp_prt(isp, ISP_LOGDEBUG1,
		    "usec%d: 0x%lx->0x%lx enano 0x%x%08x",
		    count, (long) GET_NANOSEC(&hra), (long) GET_NANOSEC(&hrb),
		    (u_int32_t)(enano >> 32), (u_int32_t)(enano & 0xffffffff));

		/*
		 * If the elapsed time is less than 1 millisecond,
		 * delay a period of time up to that millisecond of
		 * waiting.
		 *
		 * This peculiar code is an attempt to try and avoid
		 * invoking u_int64_t math support functions for some
		 * platforms where linkage is a problem.
		 */
		if (enano < (1000 * 1000)) {
			count += 1000;
			enano = (1000 * 1000) - enano;
			while (enano > (u_int64_t) 4000000000U) {
				USEC_SLEEP(isp, 4000000);
				enano -= (u_int64_t) 4000000000U;
			}
			wrk = enano;
			wrk /= 1000;
			USEC_SLEEP(isp, wrk);
		} else {
			while (enano > (u_int64_t) 4000000000U) {
				count += 4000000;
				enano -= (u_int64_t) 4000000000U;
			}
			wrk = enano;
			count += (wrk / 1000);
		}
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
	isp_mboxcmd(isp, &mbs, MBLOGALL);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return (-1);
	}
	fcp->isp_loopid = mbs.param[1];
	if (IS_2200(isp) || IS_23XX(isp)) {
		int topo = (int) mbs.param[6];
		if (topo < TOPO_NL_PORT || topo > TOPO_PTP_STUB)
			topo = TOPO_PTP_STUB;
		fcp->isp_topo = topo;
	} else {
		fcp->isp_topo = TOPO_NL_PORT;
	}
	fcp->isp_portid = fcp->isp_alpa = mbs.param[2] & 0xff;

	/*
	 * Check to see if we're on a fabric by trying to see if we
	 * can talk to the fabric name server. This can be a bit
	 * tricky because if we're a 2100, we should check always
	 * (in case we're connected to a server doing aliasing).
	 */
	fcp->isp_onfabric = 0;

	if (IS_2100(isp)) {
		/*
		 * Don't bother with fabric if we are using really old
		 * 2100 firmware. It's just not worth it.
		 */
		if (ISP_FW_NEWER_THAN(isp, 1, 15, 37)) {
			check_for_fabric = 1;
		} else {
			check_for_fabric = 0;
		}
	} else if (fcp->isp_topo == TOPO_FL_PORT ||
	    fcp->isp_topo == TOPO_F_PORT) {
		check_for_fabric = 1;
	} else
		check_for_fabric = 0;

	if (check_for_fabric && isp_getpdb(isp, FL_PORT_ID, &pdb) == 0) {
		int loopid = FL_PORT_ID;
		if (IS_2100(isp)) {
			fcp->isp_topo = TOPO_FL_PORT;
		}

		if (BITS2WORD(pdb.pdb_portid_bits) == 0) {
			/*
			 * Crock.
			 */
			fcp->isp_topo = TOPO_NL_PORT;
			goto not_on_fabric;
		}
		fcp->isp_portid = mbs.param[2] | ((int) mbs.param[3] << 16);

		/*
		 * Save the Fabric controller's port database entry.
		 */
		lp = &fcp->portdb[loopid];
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
		lp->loggedin = lp->valid = 1;
		fcp->isp_onfabric = 1;
		(void) isp_async(isp, ISPASYNC_PROMENADE, &loopid);
		isp_register_fc4_type(isp);
	} else {
not_on_fabric:
		fcp->isp_onfabric = 0;
		fcp->portdb[FL_PORT_ID].valid = 0;
	}

	fcp->isp_gbspeed = 1;
	if (IS_23XX(isp)) {
		mbs.param[0] = MBOX_GET_SET_DATA_RATE;
		mbs.param[1] = MBGSD_GET_RATE;
		/* mbs.param[2] undefined if we're just getting rate */
		isp_mboxcmd(isp, &mbs, MBLOGALL);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			if (mbs.param[1] == MBGSD_TWOGB) {
				isp_prt(isp, ISP_LOGINFO, "2Gb link speed/s");
				fcp->isp_gbspeed = 2;
			}
		}
	}

	isp_prt(isp, ISP_LOGCONFIG, topology, fcp->isp_loopid, fcp->isp_alpa,
	    fcp->isp_portid, fcp->isp_loopstate, toponames[fcp->isp_topo]);

	/*
	 * Announce ourselves, too. This involves synthesizing an entry.
	 */
	if (fcp->isp_iid_set == 0) {
		fcp->isp_iid_set = 1;
		fcp->isp_iid = fcp->isp_loopid;
		lp = &fcp->portdb[fcp->isp_iid];
	} else {
		lp = &fcp->portdb[fcp->isp_iid];
		if (fcp->isp_portid != lp->portid ||
		    fcp->isp_loopid != lp->loopid ||
		    fcp->isp_nodewwn != ISP_NODEWWN(isp) ||
		    fcp->isp_portwwn != ISP_PORTWWN(isp)) {
			lp->valid = 0;
			count = fcp->isp_iid;
			(void) isp_async(isp, ISPASYNC_PROMENADE, &count);
		}
	}
	lp->loopid = fcp->isp_loopid;
	lp->portid = fcp->isp_portid;
	lp->node_wwn = ISP_NODEWWN(isp);
	lp->port_wwn = ISP_PORTWWN(isp);
	switch (isp->isp_role) {
	case ISP_ROLE_NONE:
		lp->roles = 0;
		break;
	case ISP_ROLE_TARGET:
		lp->roles = SVC3_TGT_ROLE >> SVC3_ROLE_SHIFT;
		break;
	case ISP_ROLE_INITIATOR:
		lp->roles = SVC3_INI_ROLE >> SVC3_ROLE_SHIFT;
		break;
	case ISP_ROLE_BOTH:
		lp->roles = (SVC3_INI_ROLE|SVC3_TGT_ROLE) >> SVC3_ROLE_SHIFT;
		break;
	}
	lp->loggedin = lp->valid = 1;
	count = fcp->isp_iid;
	(void) isp_async(isp, ISPASYNC_PROMENADE, &count);
	return (0);
}

static char *
isp2100_fw_statename(int state)
{
	switch(state) {
	case FW_CONFIG_WAIT:	return "Config Wait";
	case FW_WAIT_AL_PA:	return "Waiting for AL_PA";
	case FW_WAIT_LOGIN:	return "Wait Login";
	case FW_READY:		return "Ready";
	case FW_LOSS_OF_SYNC:	return "Loss Of Sync";
	case FW_ERROR:		return "Error";
	case FW_REINIT:		return "Re-Init";
	case FW_NON_PART:	return "Nonparticipating";
	default:		return "?????";
	}
}

/*
 * Synchronize our soft copy of the port database with what the f/w thinks
 * (with a view toward possibly for a specific target....)
 */

static int
isp_pdb_sync(struct ispsoftc *isp)
{
	struct lportdb *lp;
	fcparam *fcp = isp->isp_param;
	isp_pdb_t pdb;
	int loopid, base, lim;

	/*
	 * Make sure we're okay for doing this right now.
	 */
	if (fcp->isp_loopstate != LOOP_PDB_RCVD &&
	    fcp->isp_loopstate != LOOP_FSCAN_DONE &&
	    fcp->isp_loopstate != LOOP_LSCAN_DONE) {
		return (-1);
	}

	if (fcp->isp_topo == TOPO_FL_PORT || fcp->isp_topo == TOPO_NL_PORT ||
	    fcp->isp_topo == TOPO_N_PORT) {
		if (fcp->isp_loopstate < LOOP_LSCAN_DONE) {
			if (isp_scan_loop(isp) != 0) {
				return (-1);
			}
		}
	}
	fcp->isp_loopstate = LOOP_SYNCING_PDB;

	/*
	 * If we get this far, we've settled our differences with the f/w
	 * (for local loop device) and we can say that the loop state is ready.
	 */

	if (fcp->isp_topo == TOPO_NL_PORT) {
		fcp->loop_seen_once = 1;
		fcp->isp_loopstate = LOOP_READY;
		return (0);
	}

	/*
	 * Find all Fabric Entities that didn't make it from one scan to the
	 * next and let the world know they went away. Scan the whole database.
	 */
	for (lp = &fcp->portdb[0]; lp < &fcp->portdb[MAX_FC_TARG]; lp++) {
		if (lp->was_fabric_dev && lp->fabric_dev == 0) {
			loopid = lp - fcp->portdb;
			lp->valid = 0;	/* should already be set */
			(void) isp_async(isp, ISPASYNC_PROMENADE, &loopid);
			MEMZERO((void *) lp, sizeof (*lp));
			continue;
		}
		lp->was_fabric_dev = lp->fabric_dev;
	}

	if (fcp->isp_topo == TOPO_FL_PORT)
		base = FC_SNS_ID+1;
	else
		base = 0;

	if (fcp->isp_topo == TOPO_N_PORT)
		lim = 1;
	else
		lim = MAX_FC_TARG;

	/*
	 * Now log in any fabric devices that the outer layer has
	 * left for us to see. This seems the most sane policy
	 * for the moment.
	 */
	for (lp = &fcp->portdb[base]; lp < &fcp->portdb[lim]; lp++) {
		u_int32_t portid;
		mbreg_t mbs;

		loopid = lp - fcp->portdb;
		if (loopid >= FL_PORT_ID && loopid <= FC_SNS_ID) {
			continue;
		}

		/*
		 * Anything here?
		 */
		if (lp->port_wwn == 0) {
			continue;
		}

		/*
		 * Don't try to log into yourself.
		 */
		if ((portid = lp->portid) == fcp->isp_portid) {
			continue;
		}


		/*
		 * If we'd been logged in- see if we still are and we haven't
		 * changed. If so, no need to log ourselves out, etc..
		 *
		 * Unfortunately, our charming Qlogic f/w has decided to
		 * return a valid port database entry for a fabric device
		 * that has, in fact, gone away. And it hangs trying to
		 * log it out.
		 */
		if (lp->loggedin && lp->force_logout == 0 &&
		    isp_getpdb(isp, lp->loopid, &pdb) == 0) {
			int nrole;
			u_int64_t nwwnn, nwwpn;
			nwwnn =
			    (((u_int64_t)pdb.pdb_nodename[0]) << 56) |
			    (((u_int64_t)pdb.pdb_nodename[1]) << 48) |
			    (((u_int64_t)pdb.pdb_nodename[2]) << 40) |
			    (((u_int64_t)pdb.pdb_nodename[3]) << 32) |
			    (((u_int64_t)pdb.pdb_nodename[4]) << 24) |
			    (((u_int64_t)pdb.pdb_nodename[5]) << 16) |
			    (((u_int64_t)pdb.pdb_nodename[6]) <<  8) |
			    (((u_int64_t)pdb.pdb_nodename[7]));
			nwwpn =
			    (((u_int64_t)pdb.pdb_portname[0]) << 56) |
			    (((u_int64_t)pdb.pdb_portname[1]) << 48) |
			    (((u_int64_t)pdb.pdb_portname[2]) << 40) |
			    (((u_int64_t)pdb.pdb_portname[3]) << 32) |
			    (((u_int64_t)pdb.pdb_portname[4]) << 24) |
			    (((u_int64_t)pdb.pdb_portname[5]) << 16) |
			    (((u_int64_t)pdb.pdb_portname[6]) <<  8) |
			    (((u_int64_t)pdb.pdb_portname[7]));
			nrole = (pdb.pdb_prli_svc3 & SVC3_ROLE_MASK) >>
			    SVC3_ROLE_SHIFT;
			if (pdb.pdb_loopid == lp->loopid && lp->portid ==
			    (u_int32_t) BITS2WORD(pdb.pdb_portid_bits) &&
			    nwwnn == lp->node_wwn && nwwpn == lp->port_wwn &&
			    lp->roles == nrole && lp->force_logout == 0) {
				lp->loggedin = lp->valid = 1;
				isp_prt(isp, ISP_LOGCONFIG, lretained,
				    (int) (lp - fcp->portdb),
				    (int) lp->loopid, lp->portid);
				continue;
			}
		}

		if (fcp->isp_fwstate != FW_READY ||
		    fcp->isp_loopstate != LOOP_SYNCING_PDB) {
			return (-1);
		}

		/*
		 * Force a logout if we were logged in.
		 */
		if (lp->loggedin) {
			if (lp->force_logout ||
			    isp_getpdb(isp, lp->loopid, &pdb) == 0) {
				mbs.param[0] = MBOX_FABRIC_LOGOUT;
				mbs.param[1] = lp->loopid << 8;
				mbs.param[2] = 0;
				mbs.param[3] = 0;
				isp_mboxcmd(isp, &mbs, MBLOGNONE);
				isp_prt(isp, ISP_LOGINFO, plogout,
				    (int) (lp - fcp->portdb), lp->loopid,
				    lp->portid);
			}
			lp->force_logout = lp->loggedin = 0;
			if (fcp->isp_fwstate != FW_READY ||
			    fcp->isp_loopstate != LOOP_SYNCING_PDB) {
				return (-1);
			}
		}

		/*
		 * And log in....
		 */
		loopid = lp - fcp->portdb;
		lp->loopid = FL_PORT_ID;
		do {
			mbs.param[0] = MBOX_FABRIC_LOGIN;
			mbs.param[1] = loopid << 8;
			mbs.param[2] = portid >> 16;
			mbs.param[3] = portid & 0xffff;
			isp_mboxcmd(isp, &mbs, MBLOGALL & ~(MBOX_LOOP_ID_USED |
			    MBOX_PORT_ID_USED | MBOX_COMMAND_ERROR));
			if (fcp->isp_fwstate != FW_READY ||
			    fcp->isp_loopstate != LOOP_SYNCING_PDB) {
				return (-1);
			}
			switch (mbs.param[0]) {
			case MBOX_LOOP_ID_USED:
				/*
				 * Try the next available loop id.
				 */
				loopid++;
				break;
			case MBOX_PORT_ID_USED:
				/*
				 * This port is already logged in.
				 * Snaffle the loop id it's using if it's
				 * nonzero, otherwise we're hosed.
				 */
				if (mbs.param[1] != 0) {
					loopid = mbs.param[1];
					isp_prt(isp, ISP_LOGINFO, retained,
					    loopid, (int) (lp - fcp->portdb),
					    lp->portid);
				} else {
					loopid = MAX_FC_TARG;
					break;
				}
				/* FALLTHROUGH */
			case MBOX_COMMAND_COMPLETE:
				lp->loggedin = 1;
				lp->loopid = loopid;
				break;
			case MBOX_COMMAND_ERROR:
				isp_prt(isp, ISP_LOGINFO, plogierr,
				    portid, mbs.param[1]);
				/* FALLTHROUGH */
			case MBOX_ALL_IDS_USED: /* We're outta IDs */
			default:
				loopid = MAX_FC_TARG;
				break;
			}
		} while (lp->loopid == FL_PORT_ID && loopid < MAX_FC_TARG);

		/*
		 * If we get here and we haven't set a Loop ID,
		 * we failed to log into this device.
		 */

		if (lp->loopid == FL_PORT_ID) {
			lp->loopid = 0;
			continue;
		}

		/*
		 * Make sure we can get the approriate port information.
		 */
		if (isp_getpdb(isp, lp->loopid, &pdb) != 0) {
			isp_prt(isp, ISP_LOGWARN, nopdb, lp->portid);
			goto dump_em;
		}

		if (fcp->isp_fwstate != FW_READY ||
		    fcp->isp_loopstate != LOOP_SYNCING_PDB) {
			return (-1);
		}

		if (pdb.pdb_loopid != lp->loopid) {
			isp_prt(isp, ISP_LOGWARN, pdbmfail1,
			    lp->portid, pdb.pdb_loopid);
			goto dump_em;
		}

		if (lp->portid != (u_int32_t) BITS2WORD(pdb.pdb_portid_bits)) {
			isp_prt(isp, ISP_LOGWARN, pdbmfail2,
			    lp->portid, BITS2WORD(pdb.pdb_portid_bits));
			goto dump_em;
		}

		lp->roles =
		    (pdb.pdb_prli_svc3 & SVC3_ROLE_MASK) >> SVC3_ROLE_SHIFT;
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
		/*
		 * Check to make sure this all makes sense.
		 */
		if (lp->node_wwn && lp->port_wwn) {
			lp->valid = 1;
			loopid = lp - fcp->portdb;
			(void) isp_async(isp, ISPASYNC_PROMENADE, &loopid);
			continue;
		}
dump_em:
		lp->valid = 0;
		isp_prt(isp, ISP_LOGINFO,
		    ldumped, loopid, lp->loopid, lp->portid);
		mbs.param[0] = MBOX_FABRIC_LOGOUT;
		mbs.param[1] = lp->loopid << 8;
		mbs.param[2] = 0;
		mbs.param[3] = 0;
		isp_mboxcmd(isp, &mbs, MBLOGNONE);
		if (fcp->isp_fwstate != FW_READY ||
		    fcp->isp_loopstate != LOOP_SYNCING_PDB) {
			return (-1);
		}
	}
	/*
	 * If we get here, we've for sure seen not only a valid loop
	 * but know what is or isn't on it, so mark this for usage
	 * in isp_start.
	 */
	fcp->loop_seen_once = 1;
	fcp->isp_loopstate = LOOP_READY;
	return (0);
}

static int
isp_scan_loop(struct ispsoftc *isp)
{
	struct lportdb *lp;
	fcparam *fcp = isp->isp_param;
	isp_pdb_t pdb;
	int loopid, lim, hival;

	switch (fcp->isp_topo) {
	case TOPO_NL_PORT:
		hival = FL_PORT_ID;
		break;
	case TOPO_N_PORT:
		hival = 2;
		break;
	case TOPO_FL_PORT:
		hival = FC_PORT_ID;
		break;
	default:
		fcp->isp_loopstate = LOOP_LSCAN_DONE;
		return (0);
	}
	fcp->isp_loopstate = LOOP_SCANNING_LOOP;

	/*
	 * make sure the temp port database is clean...
	 */
	MEMZERO((void *)fcp->tport, sizeof (fcp->tport));

	/*
	 * Run through the local loop ports and get port database info
	 * for each loop ID.
	 *
	 * There's a somewhat unexplained situation where the f/w passes back
	 * the wrong database entity- if that happens, just restart (up to
	 * FL_PORT_ID times).
	 */
	for (lim = loopid = 0; loopid < hival; loopid++) {
		lp = &fcp->tport[loopid];

		/*
		 * Don't even try for ourselves...
	 	 */
		if (loopid == fcp->isp_loopid)
			continue;

		lp->node_wwn = isp_get_portname(isp, loopid, 1);
		if (fcp->isp_loopstate < LOOP_SCANNING_LOOP)
			return (-1);
		if (lp->node_wwn == 0)
			continue;
		lp->port_wwn = isp_get_portname(isp, loopid, 0);
		if (fcp->isp_loopstate < LOOP_SCANNING_LOOP)
			return (-1);
		if (lp->port_wwn == 0) {
			lp->node_wwn = 0;
			continue;
		}

		/*
		 * Get an entry....
		 */
		if (isp_getpdb(isp, loopid, &pdb) != 0) {
			if (fcp->isp_loopstate < LOOP_SCANNING_LOOP)
				return (-1);
			continue;
		}
		if (fcp->isp_loopstate < LOOP_SCANNING_LOOP) {
			return (-1);
		}

		/*
		 * If the returned database element doesn't match what we
		 * asked for, restart the process entirely (up to a point...).
		 */
		if (pdb.pdb_loopid != loopid) {
			loopid = 0;
			if (lim++ < hival) {
				continue;
			}
			isp_prt(isp, ISP_LOGWARN,
			    "giving up on synchronizing the port database");
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
	}

	/*
	 * Mark all of the permanent local loop database entries as invalid
	 * (except our own entry).
	 */
	for (loopid = 0; loopid < hival; loopid++) {
		if (loopid == fcp->isp_iid) {
			fcp->portdb[loopid].valid = 1;
			fcp->portdb[loopid].loopid = fcp->isp_loopid;
			continue;
		}
		fcp->portdb[loopid].valid = 0;
	}

	/*
	 * Now merge our local copy of the port database into our saved copy.
	 * Notify the outer layers of new devices arriving.
	 */
	for (loopid = 0; loopid < hival; loopid++) {
		int i;

		/*
		 * If we don't have a non-zero Port WWN, we're not here.
		 */
		if (fcp->tport[loopid].port_wwn == 0) {
			continue;
		}

		/*
		 * Skip ourselves.
		 */
		if (loopid == fcp->isp_iid) {
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

		for (i = 0; i < hival; i++) {
			int j;
			if (fcp->portdb[i].port_wwn == 0)
				continue;
			if (fcp->portdb[i].port_wwn !=
			    fcp->tport[loopid].port_wwn)
				continue;
			/*
			 * We found this WWN elsewhere- it's changed
			 * loopids then. We don't change it's actual
			 * position in our cached port database- we
			 * just change the actual loop ID we'd use.
			 */
			if (fcp->portdb[i].loopid != loopid) {
				isp_prt(isp, ISP_LOGINFO, portshift, i,
				    fcp->portdb[i].loopid,
				    fcp->portdb[i].portid, loopid,
				    fcp->tport[loopid].portid);
			}
			fcp->portdb[i].portid = fcp->tport[loopid].portid;
			fcp->portdb[i].loopid = loopid;
			fcp->portdb[i].valid = 1;
			fcp->portdb[i].roles = fcp->tport[loopid].roles;

			/*
			 * Now make sure this Port WWN doesn't exist elsewhere
			 * in the port database.
			 */
			for (j = i+1; j < hival; j++) {
				if (fcp->portdb[i].port_wwn !=
				    fcp->portdb[j].port_wwn) {
					continue;
				}
				isp_prt(isp, ISP_LOGWARN, portdup, j, i);
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
		if (i < hival) {
			isp_prt(isp, ISP_LOGINFO, retained,
			    fcp->portdb[i].loopid, i, fcp->portdb[i].portid);
			continue;
		}

		/*
		 * We've not found this Port WWN anywhere. It's a new entry.
		 * See if we can leave it where it is (with target == loopid).
		 */
		if (fcp->portdb[loopid].port_wwn != 0) {
			for (lim = 0; lim < hival; lim++) {
				if (fcp->portdb[lim].port_wwn == 0)
					break;
			}
			/* "Cannot Happen" */
			if (lim == hival) {
				isp_prt(isp, ISP_LOGWARN, "Remap Overflow");
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
		fcp->portdb[i].port_wwn = fcp->tport[loopid].port_wwn;
		fcp->portdb[i].node_wwn = fcp->tport[loopid].node_wwn;
		fcp->portdb[i].roles = fcp->tport[loopid].roles;
		fcp->portdb[i].portid = fcp->tport[loopid].portid;
		fcp->portdb[i].valid = 1;

		/*
		 * Tell the outside world we've arrived.
		 */
		(void) isp_async(isp, ISPASYNC_PROMENADE, &i);
	}

	/*
	 * Now find all previously used targets that are now invalid and
	 * notify the outer layers that they're gone.
	 */
	for (lp = &fcp->portdb[0]; lp < &fcp->portdb[hival]; lp++) {
		if (lp->valid || lp->port_wwn == 0) {
			continue;
		}

		/*
		 * Tell the outside world we've gone
		 * away and erase our pdb entry.
		 *
		 */
		loopid = lp - fcp->portdb;
		(void) isp_async(isp, ISPASYNC_PROMENADE, &loopid);
		MEMZERO((void *) lp, sizeof (*lp));
	}
	fcp->isp_loopstate = LOOP_LSCAN_DONE;
	return (0);
}


static int
isp_fabric_mbox_cmd(struct ispsoftc *isp, mbreg_t *mbp)
{
	isp_mboxcmd(isp, mbp, MBLOGNONE);
	if (mbp->param[0] != MBOX_COMMAND_COMPLETE) {
		if (FCPARAM(isp)->isp_loopstate == LOOP_SCANNING_FABRIC) {
			FCPARAM(isp)->isp_loopstate = LOOP_PDB_RCVD;
		}
		if (mbp->param[0] == MBOX_COMMAND_ERROR) {
			char tbuf[16];
			char *m;
			switch (mbp->param[1]) {
			case 1:
				m = "No Loop";
				break;
			case 2:
				m = "Failed to allocate IOCB buffer";
				break;
			case 3:
				m = "Failed to allocate XCB buffer";
				break;
			case 4:
				m = "timeout or transmit failed";
				break;
			case 5:
				m = "no fabric loop";
				break;
			case 6:
				m = "remote device not a target";
				break;
			default:
				SNPRINTF(tbuf, sizeof tbuf, "%x",
				    mbp->param[1]);
				m = tbuf;
				break;
			}
			isp_prt(isp, ISP_LOGERR, "SNS Failed- %s", m);
		}
		return (-1);
	}

	if (FCPARAM(isp)->isp_fwstate != FW_READY ||
	    FCPARAM(isp)->isp_loopstate < LOOP_SCANNING_FABRIC) {
		return (-1);
	}
	return(0);
}

#ifdef	ISP_USE_GA_NXT
static int
isp_scan_fabric(struct ispsoftc *isp, int ftype)
{
	fcparam *fcp = isp->isp_param;
	u_int32_t portid, first_portid, last_portid;
	int hicap, last_port_same;

	if (fcp->isp_onfabric == 0) {
		fcp->isp_loopstate = LOOP_FSCAN_DONE;
		return (0);
	}

	FC_SCRATCH_ACQUIRE(isp);

	/*
	 * Since Port IDs are 24 bits, we can check against having seen
	 * anything yet with this value.
	 */
	last_port_same = 0;
	last_portid = 0xffffffff;	/* not a port */
	first_portid = portid = fcp->isp_portid;
	fcp->isp_loopstate = LOOP_SCANNING_FABRIC;

	for (hicap = 0; hicap < GA_NXT_MAX; hicap++) {
		mbreg_t mbs;
		sns_screq_t *rq;
		sns_ga_nxt_rsp_t *rs0, *rs1;
		struct lportdb lcl;
		u_int8_t sc[SNS_GA_NXT_RESP_SIZE];

		rq = (sns_screq_t *)sc;
		MEMZERO((void *) rq, SNS_GA_NXT_REQ_SIZE);
		rq->snscb_rblen = SNS_GA_NXT_RESP_SIZE >> 1;
		rq->snscb_addr[RQRSP_ADDR0015] = DMA_WD0(fcp->isp_scdma+0x100);
		rq->snscb_addr[RQRSP_ADDR1631] = DMA_WD1(fcp->isp_scdma+0x100);
		rq->snscb_addr[RQRSP_ADDR3247] = DMA_WD2(fcp->isp_scdma+0x100);
		rq->snscb_addr[RQRSP_ADDR4863] = DMA_WD3(fcp->isp_scdma+0x100);
		rq->snscb_sblen = 6;
		rq->snscb_data[0] = SNS_GA_NXT;
		rq->snscb_data[4] = portid & 0xffff;
		rq->snscb_data[5] = (portid >> 16) & 0xff;
		isp_put_sns_request(isp, rq, (sns_screq_t *) fcp->isp_scratch);
		MEMORYBARRIER(isp, SYNC_SFORDEV, 0, SNS_GA_NXT_REQ_SIZE);
		mbs.param[0] = MBOX_SEND_SNS;
		mbs.param[1] = SNS_GA_NXT_REQ_SIZE >> 1;
		mbs.param[2] = DMA_WD1(fcp->isp_scdma);
		mbs.param[3] = DMA_WD0(fcp->isp_scdma);
		/*
		 * Leave 4 and 5 alone
		 */
		mbs.param[6] = DMA_WD3(fcp->isp_scdma);
		mbs.param[7] = DMA_WD2(fcp->isp_scdma);
		if (isp_fabric_mbox_cmd(isp, &mbs)) {
			if (fcp->isp_loopstate >= LOOP_SCANNING_FABRIC) {
				fcp->isp_loopstate = LOOP_PDB_RCVD;
			}
			FC_SCRATCH_RELEASE(isp);
			return (-1);
		}
		MEMORYBARRIER(isp, SYNC_SFORCPU, 0x100, SNS_GA_NXT_RESP_SIZE);
		rs1 = (sns_ga_nxt_rsp_t *) sc;
		rs0 = (sns_ga_nxt_rsp_t *) ((u_int8_t *)fcp->isp_scratch+0x100);
		isp_get_ga_nxt_response(isp, rs0, rs1);
		if (rs1->snscb_cthdr.ct_response != FS_ACC) {
			int level;
			if (rs1->snscb_cthdr.ct_reason == 9 &&
			    rs1->snscb_cthdr.ct_explanation == 7)
				level = ISP_LOGDEBUG0;
			else
				level = ISP_LOGWARN;
			isp_prt(isp, level, swrej, "GA_NXT",
			    rs1->snscb_cthdr.ct_reason,
			    rs1->snscb_cthdr.ct_explanation, portid);
			FC_SCRATCH_RELEASE(isp);
			fcp->isp_loopstate = LOOP_FSCAN_DONE;
			return (0);
		}
		portid =
		    (((u_int32_t) rs1->snscb_port_id[0]) << 16) |
		    (((u_int32_t) rs1->snscb_port_id[1]) << 8) |
		    (((u_int32_t) rs1->snscb_port_id[2]));

		/*
		 * XXX: We should check to make sure that this entry
		 * XXX: supports the type(s) we are interested in.
		 */
		/*
		 * Okay, we now have information about a fabric object.
		 * If it is the type we're interested in, tell the outer layers
		 * about it. The outer layer needs to  know: Port ID, WWNN,
		 * WWPN, FC4 type, and port type.
		 *
		 * The lportdb structure is adequate for this.
		 */
		MEMZERO(&lcl, sizeof (lcl));
		lcl.port_type = rs1->snscb_port_type;
		lcl.fc4_type = ftype;
		lcl.portid = portid;
		lcl.node_wwn =
		    (((u_int64_t)rs1->snscb_nodename[0]) << 56) |
		    (((u_int64_t)rs1->snscb_nodename[1]) << 48) |
		    (((u_int64_t)rs1->snscb_nodename[2]) << 40) |
		    (((u_int64_t)rs1->snscb_nodename[3]) << 32) |
		    (((u_int64_t)rs1->snscb_nodename[4]) << 24) |
		    (((u_int64_t)rs1->snscb_nodename[5]) << 16) |
		    (((u_int64_t)rs1->snscb_nodename[6]) <<  8) |
		    (((u_int64_t)rs1->snscb_nodename[7]));
		lcl.port_wwn =
		    (((u_int64_t)rs1->snscb_portname[0]) << 56) |
		    (((u_int64_t)rs1->snscb_portname[1]) << 48) |
		    (((u_int64_t)rs1->snscb_portname[2]) << 40) |
		    (((u_int64_t)rs1->snscb_portname[3]) << 32) |
		    (((u_int64_t)rs1->snscb_portname[4]) << 24) |
		    (((u_int64_t)rs1->snscb_portname[5]) << 16) |
		    (((u_int64_t)rs1->snscb_portname[6]) <<  8) |
		    (((u_int64_t)rs1->snscb_portname[7]));

		/*
		 * Does this fabric object support the type we want?
		 * If not, skip it.
		 */
		if (rs1->snscb_fc4_types[ftype >> 5] & (1 << (ftype & 0x1f))) {
			if (first_portid == portid) {
				lcl.last_fabric_dev = 1;
			} else {
				lcl.last_fabric_dev = 0;
			}
			(void) isp_async(isp, ISPASYNC_FABRIC_DEV, &lcl);
		} else {
			isp_prt(isp, ISP_LOGDEBUG0,
			    "PortID 0x%x doesn't support FC4 type 0x%x",
			    portid, ftype);
		}
		if (first_portid == portid) {
			fcp->isp_loopstate = LOOP_FSCAN_DONE;
			FC_SCRATCH_RELEASE(isp);
			return (0);
		}
		if (portid == last_portid) {
			if (last_port_same++ > 20) {
				isp_prt(isp, ISP_LOGWARN,
				    "tangled fabric database detected");
				break;
			}
		} else {
			last_port_same = 0 ;
			last_portid = portid;
		}
	}
	FC_SCRATCH_RELEASE(isp);
	if (hicap >= GA_NXT_MAX) {
		isp_prt(isp, ISP_LOGWARN, "fabric too big (> %d)", GA_NXT_MAX);
	}
	fcp->isp_loopstate = LOOP_FSCAN_DONE;
	return (0);
}
#else
#define	GIDLEN	((ISP2100_SCRLEN >> 1) + 16)
#define	NGENT	((GIDLEN - 16) >> 2)

#define	IGPOFF	(ISP2100_SCRLEN - GIDLEN)
#define	GXOFF	(256)

static int
isp_scan_fabric(struct ispsoftc *isp, int ftype)
{
	fcparam *fcp = FCPARAM(isp);
	mbreg_t mbs;
	int i;
	sns_gid_ft_req_t *rq;
	sns_gid_ft_rsp_t *rs0, *rs1;

	if (fcp->isp_onfabric == 0) {
		fcp->isp_loopstate = LOOP_FSCAN_DONE;
		return (0);
	}

	FC_SCRATCH_ACQUIRE(isp);
	fcp->isp_loopstate = LOOP_SCANNING_FABRIC;

	rq = (sns_gid_ft_req_t *)fcp->tport;
	MEMZERO((void *) rq, SNS_GID_FT_REQ_SIZE);
	rq->snscb_rblen = GIDLEN >> 1;
	rq->snscb_addr[RQRSP_ADDR0015] = DMA_WD0(fcp->isp_scdma+IGPOFF);
	rq->snscb_addr[RQRSP_ADDR1631] = DMA_WD1(fcp->isp_scdma+IGPOFF);
	rq->snscb_addr[RQRSP_ADDR3247] = DMA_WD2(fcp->isp_scdma+IGPOFF);
	rq->snscb_addr[RQRSP_ADDR4863] = DMA_WD3(fcp->isp_scdma+IGPOFF);
	rq->snscb_sblen = 6;
	rq->snscb_cmd = SNS_GID_FT;
	rq->snscb_mword_div_2 = NGENT;
	rq->snscb_fc4_type = ftype;
	isp_put_gid_ft_request(isp, rq, (sns_gid_ft_req_t *) fcp->isp_scratch);
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, SNS_GID_FT_REQ_SIZE);
	mbs.param[0] = MBOX_SEND_SNS;
	mbs.param[1] = SNS_GID_FT_REQ_SIZE >> 1;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);

	/*
	 * Leave 4 and 5 alone
	 */
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	if (isp_fabric_mbox_cmd(isp, &mbs)) {
		if (fcp->isp_loopstate >= LOOP_SCANNING_FABRIC) {
			fcp->isp_loopstate = LOOP_PDB_RCVD;
		}
		FC_SCRATCH_RELEASE(isp);
		return (-1);
	}
	if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
		FC_SCRATCH_RELEASE(isp);
		return (-1);
	}
	MEMORYBARRIER(isp, SYNC_SFORCPU, IGPOFF, GIDLEN);
	rs1 = (sns_gid_ft_rsp_t *) fcp->tport;
	rs0 = (sns_gid_ft_rsp_t *) ((u_int8_t *)fcp->isp_scratch+IGPOFF);
	isp_get_gid_ft_response(isp, rs0, rs1, NGENT);
	if (rs1->snscb_cthdr.ct_response != FS_ACC) {
		int level;
		if (rs1->snscb_cthdr.ct_reason == 9 &&
		    rs1->snscb_cthdr.ct_explanation == 7)
			level = ISP_LOGDEBUG0;
		else
			level = ISP_LOGWARN;
		isp_prt(isp, level, swrej, "GID_FT",
		    rs1->snscb_cthdr.ct_reason,
		    rs1->snscb_cthdr.ct_explanation, 0);
		FC_SCRATCH_RELEASE(isp);
		fcp->isp_loopstate = LOOP_FSCAN_DONE;
		return (0);
	}

	/*
	 * Okay, we now have a list of Port IDs for this class of device.
	 * Go through the list and for each one get the WWPN/WWNN for it
	 * and tell the outer layers about it. The outer layer needs to
	 * know: Port ID, WWNN, WWPN, FC4 type, and (possibly) port type.
	 *
	 * The lportdb structure is adequate for this.
	 */
	i = -1;
	do {
		sns_gxn_id_req_t grqbuf, *gq = &grqbuf;
		sns_gxn_id_rsp_t *gs0, grsbuf, *gs1 = &grsbuf;
		struct lportdb lcl;
#if	0
		sns_gff_id_rsp_t *fs0, ffsbuf, *fs1 = &ffsbuf;
#endif

		i++;
		MEMZERO(&lcl, sizeof (lcl));
		lcl.fc4_type = ftype;
		lcl.portid =
		    (((u_int32_t) rs1->snscb_ports[i].portid[0]) << 16) |
		    (((u_int32_t) rs1->snscb_ports[i].portid[1]) << 8) |
		    (((u_int32_t) rs1->snscb_ports[i].portid[2]));

		MEMZERO((void *) gq, sizeof (sns_gxn_id_req_t));
		gq->snscb_rblen = SNS_GXN_ID_RESP_SIZE >> 1;
		gq->snscb_addr[RQRSP_ADDR0015] = DMA_WD0(fcp->isp_scdma+GXOFF);
		gq->snscb_addr[RQRSP_ADDR1631] = DMA_WD1(fcp->isp_scdma+GXOFF);
		gq->snscb_addr[RQRSP_ADDR3247] = DMA_WD2(fcp->isp_scdma+GXOFF);
		gq->snscb_addr[RQRSP_ADDR4863] = DMA_WD3(fcp->isp_scdma+GXOFF);
		gq->snscb_sblen = 6;
		gq->snscb_cmd = SNS_GPN_ID;
		gq->snscb_portid = lcl.portid;
		isp_put_gxn_id_request(isp, gq,
		    (sns_gxn_id_req_t *) fcp->isp_scratch);
		MEMORYBARRIER(isp, SYNC_SFORDEV, 0, SNS_GXN_ID_REQ_SIZE);
		mbs.param[0] = MBOX_SEND_SNS;
		mbs.param[1] = SNS_GXN_ID_REQ_SIZE >> 1;
		mbs.param[2] = DMA_WD1(fcp->isp_scdma);
		mbs.param[3] = DMA_WD0(fcp->isp_scdma);
		/*
		 * Leave 4 and 5 alone
		 */
		mbs.param[6] = DMA_WD3(fcp->isp_scdma);
		mbs.param[7] = DMA_WD2(fcp->isp_scdma);
		if (isp_fabric_mbox_cmd(isp, &mbs)) {
			if (fcp->isp_loopstate >= LOOP_SCANNING_FABRIC) {
				fcp->isp_loopstate = LOOP_PDB_RCVD;
			}
			FC_SCRATCH_RELEASE(isp);
			return (-1);
		}
		if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
			FC_SCRATCH_RELEASE(isp);
			return (-1);
		}
		MEMORYBARRIER(isp, SYNC_SFORCPU, GXOFF, SNS_GXN_ID_RESP_SIZE);
		gs0 = (sns_gxn_id_rsp_t *) ((u_int8_t *)fcp->isp_scratch+GXOFF);
		isp_get_gxn_id_response(isp, gs0, gs1);
		if (gs1->snscb_cthdr.ct_response != FS_ACC) {
			isp_prt(isp, ISP_LOGWARN, swrej, "GPN_ID",
			    gs1->snscb_cthdr.ct_reason,
			    gs1->snscb_cthdr.ct_explanation, lcl.portid);
			if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
				FC_SCRATCH_RELEASE(isp);
				return (-1);
			}
			continue;
		}
		lcl.port_wwn = 
		    (((u_int64_t)gs1->snscb_wwn[0]) << 56) |
		    (((u_int64_t)gs1->snscb_wwn[1]) << 48) |
		    (((u_int64_t)gs1->snscb_wwn[2]) << 40) |
		    (((u_int64_t)gs1->snscb_wwn[3]) << 32) |
		    (((u_int64_t)gs1->snscb_wwn[4]) << 24) |
		    (((u_int64_t)gs1->snscb_wwn[5]) << 16) |
		    (((u_int64_t)gs1->snscb_wwn[6]) <<  8) |
		    (((u_int64_t)gs1->snscb_wwn[7]));

		MEMZERO((void *) gq, sizeof (sns_gxn_id_req_t));
		gq->snscb_rblen = SNS_GXN_ID_RESP_SIZE >> 1;
		gq->snscb_addr[RQRSP_ADDR0015] = DMA_WD0(fcp->isp_scdma+GXOFF);
		gq->snscb_addr[RQRSP_ADDR1631] = DMA_WD1(fcp->isp_scdma+GXOFF);
		gq->snscb_addr[RQRSP_ADDR3247] = DMA_WD2(fcp->isp_scdma+GXOFF);
		gq->snscb_addr[RQRSP_ADDR4863] = DMA_WD3(fcp->isp_scdma+GXOFF);
		gq->snscb_sblen = 6;
		gq->snscb_cmd = SNS_GNN_ID;
		gq->snscb_portid = lcl.portid;
		isp_put_gxn_id_request(isp, gq,
		    (sns_gxn_id_req_t *) fcp->isp_scratch);
		MEMORYBARRIER(isp, SYNC_SFORDEV, 0, SNS_GXN_ID_REQ_SIZE);
		mbs.param[0] = MBOX_SEND_SNS;
		mbs.param[1] = SNS_GXN_ID_REQ_SIZE >> 1;
		mbs.param[2] = DMA_WD1(fcp->isp_scdma);
		mbs.param[3] = DMA_WD0(fcp->isp_scdma);
		/*
		 * Leave 4 and 5 alone
		 */
		mbs.param[6] = DMA_WD3(fcp->isp_scdma);
		mbs.param[7] = DMA_WD2(fcp->isp_scdma);
		if (isp_fabric_mbox_cmd(isp, &mbs)) {
			if (fcp->isp_loopstate >= LOOP_SCANNING_FABRIC) {
				fcp->isp_loopstate = LOOP_PDB_RCVD;
			}
			FC_SCRATCH_RELEASE(isp);
			return (-1);
		}
		if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
			FC_SCRATCH_RELEASE(isp);
			return (-1);
		}
		MEMORYBARRIER(isp, SYNC_SFORCPU, GXOFF, SNS_GXN_ID_RESP_SIZE);
		gs0 = (sns_gxn_id_rsp_t *) ((u_int8_t *)fcp->isp_scratch+GXOFF);
		isp_get_gxn_id_response(isp, gs0, gs1);
		if (gs1->snscb_cthdr.ct_response != FS_ACC) {
			isp_prt(isp, ISP_LOGWARN, swrej, "GNN_ID",
			    gs1->snscb_cthdr.ct_reason,
			    gs1->snscb_cthdr.ct_explanation, lcl.portid);
			if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
				FC_SCRATCH_RELEASE(isp);
				return (-1);
			}
			continue;
		}
		lcl.node_wwn = 
		    (((u_int64_t)gs1->snscb_wwn[0]) << 56) |
		    (((u_int64_t)gs1->snscb_wwn[1]) << 48) |
		    (((u_int64_t)gs1->snscb_wwn[2]) << 40) |
		    (((u_int64_t)gs1->snscb_wwn[3]) << 32) |
		    (((u_int64_t)gs1->snscb_wwn[4]) << 24) |
		    (((u_int64_t)gs1->snscb_wwn[5]) << 16) |
		    (((u_int64_t)gs1->snscb_wwn[6]) <<  8) |
		    (((u_int64_t)gs1->snscb_wwn[7]));

		/*
		 * The QLogic f/w is bouncing this with a parameter error.
		 */
#if	0
		/*
		 * Try and get FC4 Features (FC-GS-3 only).
		 * We can use the sns_gxn_id_req_t for this request.
		 */
		MEMZERO((void *) gq, sizeof (sns_gxn_id_req_t));
		gq->snscb_rblen = SNS_GFF_ID_RESP_SIZE >> 1;
		gq->snscb_addr[RQRSP_ADDR0015] = DMA_WD0(fcp->isp_scdma+GXOFF);
		gq->snscb_addr[RQRSP_ADDR1631] = DMA_WD1(fcp->isp_scdma+GXOFF);
		gq->snscb_addr[RQRSP_ADDR3247] = DMA_WD2(fcp->isp_scdma+GXOFF);
		gq->snscb_addr[RQRSP_ADDR4863] = DMA_WD3(fcp->isp_scdma+GXOFF);
		gq->snscb_sblen = 6;
		gq->snscb_cmd = SNS_GFF_ID;
		gq->snscb_portid = lcl.portid;
		isp_put_gxn_id_request(isp, gq,
		    (sns_gxn_id_req_t *) fcp->isp_scratch);
		MEMORYBARRIER(isp, SYNC_SFORDEV, 0, SNS_GXN_ID_REQ_SIZE);
		mbs.param[0] = MBOX_SEND_SNS;
		mbs.param[1] = SNS_GXN_ID_REQ_SIZE >> 1;
		mbs.param[2] = DMA_WD1(fcp->isp_scdma);
		mbs.param[3] = DMA_WD0(fcp->isp_scdma);
		/*
		 * Leave 4 and 5 alone
		 */
		mbs.param[6] = DMA_WD3(fcp->isp_scdma);
		mbs.param[7] = DMA_WD2(fcp->isp_scdma);
		if (isp_fabric_mbox_cmd(isp, &mbs)) {
			if (fcp->isp_loopstate >= LOOP_SCANNING_FABRIC) {
				fcp->isp_loopstate = LOOP_PDB_RCVD;
			}
			FC_SCRATCH_RELEASE(isp);
			return (-1);
		}
		if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
			FC_SCRATCH_RELEASE(isp);
			return (-1);
		}
		MEMORYBARRIER(isp, SYNC_SFORCPU, GXOFF, SNS_GFF_ID_RESP_SIZE);
		fs0 = (sns_gff_id_rsp_t *) ((u_int8_t *)fcp->isp_scratch+GXOFF);
		isp_get_gff_id_response(isp, fs0, fs1);
		if (fs1->snscb_cthdr.ct_response != FS_ACC) {
			isp_prt(isp, /* ISP_LOGDEBUG0 */ ISP_LOGWARN,
			    swrej, "GFF_ID",
			    fs1->snscb_cthdr.ct_reason,
			    fs1->snscb_cthdr.ct_explanation, lcl.portid);
			if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
				FC_SCRATCH_RELEASE(isp);
				return (-1);
			}
		} else {
			int index = (ftype >> 3);
			int bshft = (ftype & 0x7) * 4;
			int fc4_fval =
			    (fs1->snscb_fc4_features[index] >> bshft) & 0xf;
			if (fc4_fval & 0x1) {
				lcl.roles |=
				    (SVC3_INI_ROLE >> SVC3_ROLE_SHIFT);
			}
			if (fc4_fval & 0x2) {
				lcl.roles |=
				    (SVC3_TGT_ROLE >> SVC3_ROLE_SHIFT);
			}
		}
#endif

		/*
		 * If we really want to know what kind of port type this is,
		 * we have to run another CT command. Otherwise, we'll leave
		 * it as undefined.
		 *
		lcl.port_type = 0;
		 */
		if (rs1->snscb_ports[i].control & 0x80) {
			lcl.last_fabric_dev = 1;
		} else {
			lcl.last_fabric_dev = 0;
		}
		(void) isp_async(isp, ISPASYNC_FABRIC_DEV, &lcl);

	} while ((rs1->snscb_ports[i].control & 0x80) == 0 && i < NGENT-1);

	/*
	 * If we're not at the last entry, our list isn't big enough.
	 */
	if ((rs1->snscb_ports[i].control & 0x80) == 0) {
		isp_prt(isp, ISP_LOGWARN, "fabric too big for scratch area");
	}

	FC_SCRATCH_RELEASE(isp);
	fcp->isp_loopstate = LOOP_FSCAN_DONE;
	return (0);
}
#endif

static void
isp_register_fc4_type(struct ispsoftc *isp)
{
	fcparam *fcp = isp->isp_param;
	u_int8_t local[SNS_RFT_ID_REQ_SIZE];
	sns_screq_t *reqp = (sns_screq_t *) local;
	mbreg_t mbs;

	MEMZERO((void *) reqp, SNS_RFT_ID_REQ_SIZE);
	reqp->snscb_rblen = SNS_RFT_ID_RESP_SIZE >> 1;
	reqp->snscb_addr[RQRSP_ADDR0015] = DMA_WD0(fcp->isp_scdma + 0x100);
	reqp->snscb_addr[RQRSP_ADDR1631] = DMA_WD1(fcp->isp_scdma + 0x100);
	reqp->snscb_addr[RQRSP_ADDR3247] = DMA_WD2(fcp->isp_scdma + 0x100);
	reqp->snscb_addr[RQRSP_ADDR4863] = DMA_WD3(fcp->isp_scdma + 0x100);
	reqp->snscb_sblen = 22;
	reqp->snscb_data[0] = SNS_RFT_ID;
	reqp->snscb_data[4] = fcp->isp_portid & 0xffff;
	reqp->snscb_data[5] = (fcp->isp_portid >> 16) & 0xff;
	reqp->snscb_data[6] = (1 << FC4_SCSI);
#if	0
	reqp->snscb_data[6] |= (1 << FC4_IP);	/* ISO/IEC 8802-2 LLC/SNAP */
#endif
	FC_SCRATCH_ACQUIRE(isp);
	isp_put_sns_request(isp, reqp, (sns_screq_t *) fcp->isp_scratch);
	mbs.param[0] = MBOX_SEND_SNS;
	mbs.param[1] = SNS_RFT_ID_REQ_SIZE >> 1;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	/*
	 * Leave 4 and 5 alone
	 */
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	isp_mboxcmd(isp, &mbs, MBLOGALL);
	FC_SCRATCH_RELEASE(isp);
	if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
		isp_prt(isp, ISP_LOGDEBUG0, "Register FC4 types succeeded");
	}
}

/*
 * Start a command. Locking is assumed done in the caller.
 */

int
isp_start(XS_T *xs)
{
	struct ispsoftc *isp;
	u_int16_t nxti, optr, handle;
	u_int8_t local[QENTRY_LEN];
	ispreq_t *reqp, *qep;
	int target, i;

	XS_INITERR(xs);
	isp = XS_ISP(xs);

	/*
	 * Check to make sure we're supporting initiator role.
	 */
	if ((isp->isp_role & ISP_ROLE_INITIATOR) == 0) {
		XS_SETERR(xs, HBA_SELTIMEOUT);
		return (CMD_COMPLETE);
	}

	/*
	 * Now make sure we're running.
	 */

	if (isp->isp_state != ISP_RUNSTATE) {
		isp_prt(isp, ISP_LOGERR, "Adapter not at RUNSTATE");
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}

	/*
	 * Check command CDB length, etc.. We really are limited to 16 bytes
	 * for Fibre Channel, but can do up to 44 bytes in parallel SCSI,
	 * but probably only if we're running fairly new firmware (we'll
	 * let the old f/w choke on an extended command queue entry).
	 */

	if (XS_CDBLEN(xs) > (IS_FC(isp)? 16 : 44) || XS_CDBLEN(xs) == 0) {
		isp_prt(isp, ISP_LOGERR,
		    "unsupported cdb length (%d, CDB[0]=0x%x)",
		    XS_CDBLEN(xs), XS_CDBP(xs)[0] & 0xff);
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
#ifdef	HANDLE_LOOPSTATE_IN_OUTER_LAYERS
		if (fcp->isp_fwstate != FW_READY ||
		    fcp->isp_loopstate != LOOP_READY) {
			return (CMD_RQLATER);
		}

		/*
		 * If we're not on a Fabric, we can't have a target
		 * above FL_PORT_ID-1.
		 *
		 * If we're on a fabric and *not* connected as an F-port,
		 * we can't have a target less than FC_SNS_ID+1. This
		 * keeps us from having to sort out the difference between
		 * local public loop devices and those which we might get
		 * from a switch's database.
		 */
		if (fcp->isp_onfabric == 0) {
			if (target >= FL_PORT_ID) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				return (CMD_COMPLETE);
			}
		} else {
			if (target >= FL_PORT_ID && target <= FC_SNS_ID) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				return (CMD_COMPLETE);
			}
			/*
			 * We used to exclude having local loop ports
			 * at the same time that we have fabric ports.
			 * That is, we used to exclude having ports
			 * at < FL_PORT_ID if we're FL-port.
			 *
			 * That's wrong. The only thing that could be
			 * dicey is if the switch you're connected to
			 * has these local loop ports appear on the
			 * fabric and we somehow attach them twice.
			 */
		}
#else
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
			/*
			 * Give ourselves at most a 250ms delay.
			 */
			if (isp_fclink_test(isp, 250000)) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				if (fcp->loop_seen_once) {
					return (CMD_RQLATER);
				} else {
					return (CMD_COMPLETE);
				}
			}
		}

		/*
		 * If we're not on a Fabric, we can't have a target
		 * above FL_PORT_ID-1.
		 *
		 * If we're on a fabric and *not* connected as an F-port,
		 * we can't have a target less than FC_SNS_ID+1. This
		 * keeps us from having to sort out the difference between
		 * local public loop devices and those which we might get
		 * from a switch's database.
		 */
		if (fcp->isp_onfabric == 0) {
			if (target >= FL_PORT_ID) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				return (CMD_COMPLETE);
			}
		} else {
			if (target >= FL_PORT_ID && target <= FC_SNS_ID) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				return (CMD_COMPLETE);
			}
			if (fcp->isp_topo != TOPO_F_PORT &&
			    target < FL_PORT_ID) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				return (CMD_COMPLETE);
			}
		}

		/*
		 * If our loop state is such that we haven't yet received
		 * a "Port Database Changed" notification (after a LIP or
		 * a Loop Reset or firmware initialization), then defer
		 * sending commands for a little while, but only if we've
		 * seen a valid loop at one point (otherwise we can get
		 * stuck at initialization time).
		 */
		if (fcp->isp_loopstate < LOOP_PDB_RCVD) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
			if (fcp->loop_seen_once) {
				return (CMD_RQLATER);
			} else {
				return (CMD_COMPLETE);
			}
		}

		/*
		 * If we're in the middle of loop or fabric scanning
		 * or merging the port databases, retry this command later.
		 */
		if (fcp->isp_loopstate == LOOP_SCANNING_FABRIC ||
		    fcp->isp_loopstate == LOOP_SCANNING_LOOP ||
		    fcp->isp_loopstate == LOOP_SYNCING_PDB) {
			return (CMD_RQLATER);
		}

		/*
		 * If our loop state is now such that we've just now
		 * received a Port Database Change notification, then
		 * we have to go off and (re)scan the fabric. We back
		 * out and try again later if this doesn't work.
		 */
		if (fcp->isp_loopstate == LOOP_PDB_RCVD && fcp->isp_onfabric) {
			if (isp_scan_fabric(isp, FC4_SCSI)) {
				return (CMD_RQLATER);
			}
			if (fcp->isp_fwstate != FW_READY ||
			    fcp->isp_loopstate < LOOP_FSCAN_DONE) {
				return (CMD_RQLATER);
			}
		}

		/*
		 * If our loop state is now such that we've just now
		 * received a Port Database Change notification, then
		 * we have to go off and (re)synchronize our port
		 * database.
		 */
		if (fcp->isp_loopstate < LOOP_READY) {
			if (isp_pdb_sync(isp)) {
				return (CMD_RQLATER);
			}
			if (fcp->isp_fwstate != FW_READY ||
			    fcp->isp_loopstate != LOOP_READY) {
				return (CMD_RQLATER);
			}
		}

		/*
		 * XXX: Here's were we would cancel any loop_dead flag
		 * XXX: also cancel in dead_loop timeout that's running
		 */
#endif

		/*
		 * Now check whether we should even think about pursuing this.
		 */
		lp = &fcp->portdb[target];
		if (lp->valid == 0) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_COMPLETE);
		}
		if ((lp->roles & (SVC3_TGT_ROLE >> SVC3_ROLE_SHIFT)) == 0) {
			isp_prt(isp, ISP_LOGDEBUG2,
			    "Target %d does not have target service", target);
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_COMPLETE);
		}
		/*
		 * Now turn target into what the actual Loop ID is.
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

	if (isp_getrqentry(isp, &nxti, &optr, (void *)&qep)) {
		isp_prt(isp, ISP_LOGDEBUG0, "Request Queue Overflow");
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	}

	/*
	 * Now see if we need to synchronize the ISP with respect to anything.
	 * We do dual duty here (cough) for synchronizing for busses other
	 * than which we got here to send a command to.
	 */
	reqp = (ispreq_t *) local;
	if (isp->isp_sendmarker) {
		u_int8_t n = (IS_DUALBUS(isp)? 2: 1);
		/*
		 * Check ports to send markers for...
		 */
		for (i = 0; i < n; i++) {
			if ((isp->isp_sendmarker & (1 << i)) == 0) {
				continue;
			}
			MEMZERO((void *) reqp, QENTRY_LEN);
			reqp->req_header.rqs_entry_count = 1;
			reqp->req_header.rqs_entry_type = RQSTYPE_MARKER;
			reqp->req_modifier = SYNC_ALL;
			reqp->req_target = i << 7;	/* insert bus number */
			isp_put_request(isp, reqp, qep);
			ISP_ADD_REQUEST(isp, nxti);
			isp->isp_sendmarker &= ~(1 << i);
			if (isp_getrqentry(isp, &nxti, &optr, (void *) &qep)) {
				isp_prt(isp, ISP_LOGDEBUG0,
				    "Request Queue Overflow+");
				XS_SETERR(xs, HBA_BOTCH);
				return (CMD_EAGAIN);
			}
		}
	}

	MEMZERO((void *)reqp, QENTRY_LEN);
	reqp->req_header.rqs_entry_count = 1;
	if (IS_FC(isp)) {
		reqp->req_header.rqs_entry_type = RQSTYPE_T2RQS;
	} else {
		if (XS_CDBLEN(xs) > 12)
			reqp->req_header.rqs_entry_type = RQSTYPE_CMDONLY;
		else
			reqp->req_header.rqs_entry_type = RQSTYPE_REQUEST;
	}
	/* reqp->req_header.rqs_flags = 0; */
	/* reqp->req_header.rqs_seqno = 0; */
	if (IS_FC(isp)) {
		/*
		 * See comment in isp_intr
		 */
		/* XS_RESID(xs) = 0; */

		/*
		 * Fibre Channel always requires some kind of tag.
		 * The Qlogic drivers seem be happy not to use a tag,
		 * but this breaks for some devices (IBM drives).
		 */
		if (XS_TAG_P(xs)) {
			((ispreqt2_t *)reqp)->req_flags = XS_TAG_TYPE(xs);
		} else {
			/*
			 * If we don't know what tag to use, use HEAD OF QUEUE
			 * for Request Sense or Simple.
			 */
			if (XS_CDBP(xs)[0] == 0x3)	/* REQUEST SENSE */
				((ispreqt2_t *)reqp)->req_flags = REQFLAG_HTAG;
			else
				((ispreqt2_t *)reqp)->req_flags = REQFLAG_STAG;
		}
	} else {
		sdparam *sdp = (sdparam *)isp->isp_param;
		sdp += XS_CHANNEL(xs);
		if ((sdp->isp_devparam[target].actv_flags & DPARM_TQING) &&
		    XS_TAG_P(xs)) {
			reqp->req_flags = XS_TAG_TYPE(xs);
		}
	}
	reqp->req_target = target | (XS_CHANNEL(xs) << 7);
	if (IS_SCSI(isp)) {
		reqp->req_lun_trn = XS_LUN(xs);
		reqp->req_cdblen = XS_CDBLEN(xs);
	} else {
		if (FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_SCCLUN)
			((ispreqt2_t *)reqp)->req_scclun = XS_LUN(xs);
		else
			((ispreqt2_t *)reqp)->req_lun_trn = XS_LUN(xs);
	}
	MEMCPY(reqp->req_cdb, XS_CDBP(xs), XS_CDBLEN(xs));

	reqp->req_time = XS_TIME(xs) / 1000;
	if (reqp->req_time == 0 && XS_TIME(xs)) {
		reqp->req_time = 1;
	}

	if (isp_save_xs(isp, xs, &handle)) {
		isp_prt(isp, ISP_LOGDEBUG0, "out of xflist pointers");
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	}
	reqp->req_handle = handle;

	/*
	 * Set up DMA and/or do any bus swizzling of the request entry
	 * so that the Qlogic F/W understands what is being asked of it.
	 */
	i = ISP_DMASETUP(isp, xs, reqp, &nxti, optr);
	if (i != CMD_QUEUED) {
		isp_destroy_handle(isp, handle);
		/*
		 * dmasetup sets actual error in packet, and
		 * return what we were given to return.
		 */
		return (i);
	}
	XS_SETERR(xs, HBA_NOERROR);
	isp_prt(isp, ISP_LOGDEBUG2,
	    "START cmd for %d.%d.%d cmd 0x%x datalen %ld",
	    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs), XS_CDBP(xs)[0],
	    (long) XS_XFRLEN(xs));
	ISP_ADD_REQUEST(isp, nxti);
	isp->isp_nactive++;
	return (CMD_QUEUED);
}

/*
 * isp control
 * Locks (ints blocked) assumed held.
 */

int
isp_control(struct ispsoftc *isp, ispctl_t ctl, void *arg)
{
	XS_T *xs;
	mbreg_t mbs;
	int bus, tgt;
	u_int16_t handle;

	switch (ctl) {
	default:
		isp_prt(isp, ISP_LOGERR, "Unknown Control Opcode 0x%x", ctl);
		break;

	case ISPCTL_RESET_BUS:
		/*
		 * Issue a bus reset.
		 */
		mbs.param[0] = MBOX_BUS_RESET;
		mbs.param[2] = 0;
		if (IS_SCSI(isp)) {
			mbs.param[1] =
			    ((sdparam *) isp->isp_param)->isp_bus_reset_delay;
			if (mbs.param[1] < 2)
				mbs.param[1] = 2;
			bus = *((int *) arg);
			if (IS_DUALBUS(isp))
				mbs.param[2] = bus;
		} else {
			mbs.param[1] = 10;
			bus = 0;
		}
		isp->isp_sendmarker |= (1 << bus);
		isp_mboxcmd(isp, &mbs, MBLOGALL);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			break;
		}
		isp_prt(isp, ISP_LOGINFO,
		    "driver initiated bus reset of bus %d", bus);
		return (0);

	case ISPCTL_RESET_DEV:
		tgt = (*((int *) arg)) & 0xffff;
		bus = (*((int *) arg)) >> 16;
		mbs.param[0] = MBOX_ABORT_TARGET;
		mbs.param[1] = (tgt << 8) | (bus << 15);
		mbs.param[2] = 3;	/* 'delay', in seconds */
		isp_mboxcmd(isp, &mbs, MBLOGALL);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			break;
		}
		isp_prt(isp, ISP_LOGINFO,
		    "Target %d on Bus %d Reset Succeeded", tgt, bus);
		isp->isp_sendmarker |= (1 << bus);
		return (0);

	case ISPCTL_ABORT_CMD:
		xs = (XS_T *) arg;
		tgt = XS_TGT(xs);
		handle = isp_find_handle(isp, xs);
		if (handle == 0) {
			isp_prt(isp, ISP_LOGWARN,
			    "cannot find handle for command to abort");
			break;
		}
		bus = XS_CHANNEL(xs);
		mbs.param[0] = MBOX_ABORT;
		if (IS_FC(isp)) {
			if (FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_SCCLUN)  {
				mbs.param[1] = tgt << 8;
				mbs.param[4] = 0;
				mbs.param[5] = 0;
				mbs.param[6] = XS_LUN(xs);
			} else {
				mbs.param[1] = tgt << 8 | XS_LUN(xs);
			}
		} else {
			mbs.param[1] =
			    (bus << 15) | (XS_TGT(xs) << 8) | XS_LUN(xs);
		}
		mbs.param[3] = 0;
		mbs.param[2] = handle;
		isp_mboxcmd(isp, &mbs, MBLOGALL & ~MBOX_COMMAND_ERROR);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			return (0);
		}
		/*
		 * XXX: Look for command in the REQUEST QUEUE. That is,
		 * XXX: It hasen't been picked up by firmware yet.
		 */
		break;

	case ISPCTL_UPDATE_PARAMS:

		isp_update(isp);
		return (0);

	case ISPCTL_FCLINK_TEST:

		if (IS_FC(isp)) {
			int usdelay = (arg)? *((int *) arg) : 250000;
			return (isp_fclink_test(isp, usdelay));
		}
		break;

	case ISPCTL_SCAN_FABRIC:

		if (IS_FC(isp)) {
			int ftype = (arg)? *((int *) arg) : FC4_SCSI;
			return (isp_scan_fabric(isp, ftype));
		}
		break;

	case ISPCTL_SCAN_LOOP:

		if (IS_FC(isp)) {
			return (isp_scan_loop(isp));
		}
		break;

	case ISPCTL_PDB_SYNC:

		if (IS_FC(isp)) {
			return (isp_pdb_sync(isp));
		}
		break;

	case ISPCTL_SEND_LIP:

		if (IS_FC(isp)) {
			mbs.param[0] = MBOX_INIT_LIP;
			isp_mboxcmd(isp, &mbs, MBLOGALL);
			if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
				return (0);
			}
		}
		break;

	case ISPCTL_GET_POSMAP:

		if (IS_FC(isp) && arg) {
			return (isp_getmap(isp, arg));
		}
		break;

	case ISPCTL_RUN_MBOXCMD:

		isp_mboxcmd(isp, arg, MBLOGALL);
		return(0);

#ifdef	ISP_TARGET_MODE
	case ISPCTL_TOGGLE_TMODE:
	{

		/*
		 * We don't check/set against role here- that's the
		 * responsibility for the outer layer to coordinate.
		 */
		if (IS_SCSI(isp)) {
			int param = *(int *)arg;
			mbs.param[0] = MBOX_ENABLE_TARGET_MODE;
			mbs.param[1] = param & 0xffff;
			mbs.param[2] = param >> 16;
			isp_mboxcmd(isp, &mbs, MBLOGALL);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				break;
			}
		}
		return (0);
	}
#endif
	}
	return (-1);
}

/*
 * Interrupt Service Routine(s).
 *
 * External (OS) framework has done the appropriate locking,
 * and the locking will be held throughout this function.
 */

/*
 * Limit our stack depth by sticking with the max likely number
 * of completions on a request queue at any one time.
 */
#ifndef	MAX_REQUESTQ_COMPLETIONS
#define	MAX_REQUESTQ_COMPLETIONS	64
#endif

void
isp_intr(struct ispsoftc *isp, u_int16_t isr, u_int16_t sema, u_int16_t mbox)
{
	XS_T *complist[MAX_REQUESTQ_COMPLETIONS], *xs;
	u_int16_t iptr, optr, junk;
	int i, nlooked = 0, ndone = 0;

again:
	/*
	 * Is this a mailbox related interrupt?
	 * The mailbox semaphore will be nonzero if so.
	 */
	if (sema) {
		if (mbox & 0x4000) {
			isp->isp_intmboxc++;
			if (isp->isp_mboxbsy) {
				int i = 0, obits = isp->isp_obits;
				isp->isp_mboxtmp[i++] = mbox;
				for (i = 1; i < MAX_MAILBOX; i++) {
					if ((obits & (1 << i)) == 0) {
						continue;
					}
					isp->isp_mboxtmp[i] =
					    ISP_READ(isp, MBOX_OFF(i));
				}
				if (isp->isp_mbxwrk0) {
					if (isp_mbox_continue(isp) == 0) {
						return;
					}
				}
				MBOX_NOTIFY_COMPLETE(isp);
			} else {
				isp_prt(isp, ISP_LOGWARN,
				    "Mbox Command Async (0x%x) with no waiters",
				    mbox);
			}
		} else if (isp_parse_async(isp, mbox) < 0) {
			return;
		}
		if ((IS_FC(isp) && mbox != ASYNC_RIO_RESP) ||
		    isp->isp_state != ISP_RUNSTATE) {
			ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
			ISP_WRITE(isp, BIU_SEMA, 0);
			return;
		}
	}

	/*
	 * We can't be getting this now.
	 */
	if (isp->isp_state != ISP_RUNSTATE) {
		isp_prt(isp, ISP_LOGWARN,
		    "interrupt (ISR=%x SEMA=%x) when not ready", isr, sema);
		/*
		 * Thank you very much!  *Burrrp*!
		 */
		WRITE_RESPONSE_QUEUE_OUT_POINTER(isp,
		    READ_RESPONSE_QUEUE_IN_POINTER(isp));

		ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
		ISP_WRITE(isp, BIU_SEMA, 0);
		return;
	}

	/*
	 * Get the current Response Queue Out Pointer.
	 *
	 * If we're a 2300, we can ask what hardware what it thinks.
	 */
	if (IS_23XX(isp)) {
		optr = ISP_READ(isp, isp->isp_respoutrp);
		/*
		 * Debug: to be taken out eventually
		 */
		if (isp->isp_residx != optr) {
			isp_prt(isp, ISP_LOGWARN, "optr %x soft optr %x",
			    optr, isp->isp_residx);
		}
	} else {
		optr = isp->isp_residx;
	}

	/*
	 * You *must* read the Response Queue In Pointer
	 * prior to clearing the RISC interrupt.
	 *
	 * Debounce the 2300 if revision less than 2.
	 */
	if (IS_2100(isp) || (IS_2300(isp) && isp->isp_revision < 2)) {
		i = 0;
		do {
			iptr = READ_RESPONSE_QUEUE_IN_POINTER(isp);
			junk = READ_RESPONSE_QUEUE_IN_POINTER(isp);
		} while (junk != iptr && ++i < 1000);

		if (iptr != junk) {
			ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
			isp_prt(isp, ISP_LOGWARN,
			    "Response Queue Out Pointer Unstable (%x, %x)",
			    iptr, junk);
			return;
		}
	} else {
		iptr = READ_RESPONSE_QUEUE_IN_POINTER(isp);
	}
	isp->isp_resodx = iptr;


	if (optr == iptr && sema == 0) {
		/*
		 * There are a lot of these- reasons unknown- mostly on
		 * faster Alpha machines.
		 *
		 * I tried delaying after writing HCCR_CMD_CLEAR_RISC_INT to
		 * make sure the old interrupt went away (to avoid 'ringing'
		 * effects), but that didn't stop this from occurring.
		 */
		if (IS_23XX(isp)) {
			USEC_DELAY(100);
			iptr = READ_RESPONSE_QUEUE_IN_POINTER(isp);
			junk = ISP_READ(isp, BIU_R2HSTSLO);
		} else {
			junk = ISP_READ(isp, BIU_ISR);
		}
		if (optr == iptr) {
			if (IS_23XX(isp)) {
				;
			} else {
				sema = ISP_READ(isp, BIU_SEMA);
				mbox = ISP_READ(isp, OUTMAILBOX0);
				if ((sema & 0x3) && (mbox & 0x8000)) {
					goto again;
				}
			}
			isp->isp_intbogus++;
			isp_prt(isp, ISP_LOGDEBUG1,
			    "bogus intr- isr %x (%x) iptr %x optr %x",
			    isr, junk, iptr, optr);
		}
	}
	isp->isp_resodx = iptr;
	ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
	ISP_WRITE(isp, BIU_SEMA, 0);

	if (isp->isp_rspbsy) {
		return;
	}
	isp->isp_rspbsy = 1;

	while (optr != iptr) {
		ispstatusreq_t local, *sp = &local;
		isphdr_t *hp;
		int type;
		u_int16_t oop;
		int buddaboom = 0;

		hp = (isphdr_t *) ISP_QUEUE_ENTRY(isp->isp_result, optr);
		oop = optr;
		optr = ISP_NXT_QENTRY(optr, RESULT_QUEUE_LEN(isp));
		nlooked++;
		/*
		 * Synchronize our view of this response queue entry.
		 */
		MEMORYBARRIER(isp, SYNC_RESULT, oop, QENTRY_LEN);

		type = isp_get_response_type(isp, hp);

		if (type == RQSTYPE_RESPONSE) {
			isp_get_response(isp, (ispstatusreq_t *) hp, sp);
		} else if (type == RQSTYPE_RIO2) {
			isp_rio2_t rio;
			isp_get_rio2(isp, (isp_rio2_t *) hp, &rio);
			for (i = 0; i < rio.req_header.rqs_seqno; i++) {
				isp_fastpost_complete(isp, rio.req_handles[i]);
			}
			if (isp->isp_fpcchiwater < rio.req_header.rqs_seqno)
				isp->isp_fpcchiwater = rio.req_header.rqs_seqno;
			MEMZERO(hp, QENTRY_LEN);	/* PERF */
			continue;
		} else {
			/*
			 * Somebody reachable via isp_handle_other_response
			 * may have updated the response queue pointers for
			 * us, so we reload our goal index.
			 */
			if (isp_handle_other_response(isp, type, hp, &optr)) {
				iptr = isp->isp_resodx;
				MEMZERO(hp, QENTRY_LEN);	/* PERF */
				continue;
			}

			/*
			 * After this point, we'll just look at the header as
			 * we don't know how to deal with the rest of the
			 * response.
			 */
			isp_get_response(isp, (ispstatusreq_t *) hp, sp);

			/*
			 * It really has to be a bounced request just copied
			 * from the request queue to the response queue. If
			 * not, something bad has happened.
			 */
			if (sp->req_header.rqs_entry_type != RQSTYPE_REQUEST) {
				isp_prt(isp, ISP_LOGERR, notresp,
				    sp->req_header.rqs_entry_type, oop, optr,
				    nlooked);
				if (isp->isp_dblev & ISP_LOGDEBUG0) {
					isp_print_bytes(isp, "Queue Entry",
					    QENTRY_LEN, sp);
				}
				MEMZERO(hp, QENTRY_LEN);	/* PERF */
				continue;
			}
			buddaboom = 1;
		}

		if (sp->req_header.rqs_flags & 0xf) {
#define	_RQS_OFLAGS	\
	~(RQSFLAG_CONTINUATION|RQSFLAG_FULL|RQSFLAG_BADHEADER|RQSFLAG_BADPACKET)
			if (sp->req_header.rqs_flags & RQSFLAG_CONTINUATION) {
				isp_prt(isp, ISP_LOGWARN,
				    "continuation segment");
				WRITE_RESPONSE_QUEUE_OUT_POINTER(isp, optr);
				continue;
			}
			if (sp->req_header.rqs_flags & RQSFLAG_FULL) {
				isp_prt(isp, ISP_LOGDEBUG1,
				    "internal queues full");
				/*
				 * We'll synthesize a QUEUE FULL message below.
				 */
			}
			if (sp->req_header.rqs_flags & RQSFLAG_BADHEADER) {
				isp_prt(isp, ISP_LOGERR,  "bad header flag");
				buddaboom++;
			}
			if (sp->req_header.rqs_flags & RQSFLAG_BADPACKET) {
				isp_prt(isp, ISP_LOGERR, "bad request packet");
				buddaboom++;
			}
			if (sp->req_header.rqs_flags & _RQS_OFLAGS) {
				isp_prt(isp, ISP_LOGERR,
				    "unknown flags (0x%x) in response",
				    sp->req_header.rqs_flags);
				buddaboom++;
			}
#undef	_RQS_OFLAGS
		}
		if (sp->req_handle > isp->isp_maxcmds || sp->req_handle < 1) {
			MEMZERO(hp, QENTRY_LEN);	/* PERF */
			isp_prt(isp, ISP_LOGERR,
			    "bad request handle %d (type 0x%x, flags 0x%x)",
			    sp->req_handle, sp->req_header.rqs_entry_type,
			    sp->req_header.rqs_flags);
			WRITE_RESPONSE_QUEUE_OUT_POINTER(isp, optr);
			continue;
		}
		xs = isp_find_xs(isp, sp->req_handle);
		if (xs == NULL) {
			u_int8_t ts = sp->req_completion_status & 0xff;
			MEMZERO(hp, QENTRY_LEN);	/* PERF */
			/*
			 * Only whine if this isn't the expected fallout of
			 * aborting the command.
			 */
			if (sp->req_header.rqs_entry_type != RQSTYPE_RESPONSE) {
				isp_prt(isp, ISP_LOGERR,
				    "cannot find handle 0x%x (type 0x%x)",
				    sp->req_handle,
				    sp->req_header.rqs_entry_type);
			} else if (ts != RQCS_ABORTED) {
				isp_prt(isp, ISP_LOGERR,
				    "cannot find handle 0x%x (status 0x%x)",
				    sp->req_handle, ts);
			}
			WRITE_RESPONSE_QUEUE_OUT_POINTER(isp, optr);
			continue;
		}
		isp_destroy_handle(isp, sp->req_handle);
		if (sp->req_status_flags & RQSTF_BUS_RESET) {
			XS_SETERR(xs, HBA_BUSRESET);
			isp->isp_sendmarker |= (1 << XS_CHANNEL(xs));
		}
		if (buddaboom) {
			XS_SETERR(xs, HBA_BOTCH);
		}

		if (IS_FC(isp) && (sp->req_scsi_status & RQCS_SV)) {
			/*
			 * Fibre Channel F/W doesn't say we got status
			 * if there's Sense Data instead. I guess they
			 * think it goes w/o saying.
			 */
			sp->req_state_flags |= RQSF_GOT_STATUS;
		}
		if (sp->req_state_flags & RQSF_GOT_STATUS) {
			*XS_STSP(xs) = sp->req_scsi_status & 0xff;
		}

		switch (sp->req_header.rqs_entry_type) {
		case RQSTYPE_RESPONSE:
			XS_SET_STATE_STAT(isp, xs, sp);
			isp_parse_status(isp, sp, xs);
			if ((XS_NOERR(xs) || XS_ERR(xs) == HBA_NOERROR) &&
			    (*XS_STSP(xs) == SCSI_BUSY)) {
				XS_SETERR(xs, HBA_TGTBSY);
			}
			if (IS_SCSI(isp)) {
				XS_RESID(xs) = sp->req_resid;
				if ((sp->req_state_flags & RQSF_GOT_STATUS) &&
				    (*XS_STSP(xs) == SCSI_CHECK) &&
				    (sp->req_state_flags & RQSF_GOT_SENSE)) {
					XS_SAVE_SENSE(xs, sp);
				}
				/*
				 * A new synchronous rate was negotiated for
				 * this target. Mark state such that we'll go
				 * look up that which has changed later.
				 */
				if (sp->req_status_flags & RQSTF_NEGOTIATION) {
					int t = XS_TGT(xs);
					sdparam *sdp = isp->isp_param;
					sdp += XS_CHANNEL(xs);
					sdp->isp_devparam[t].dev_refresh = 1;
					isp->isp_update |=
					    (1 << XS_CHANNEL(xs));
				}
			} else {
				if (sp->req_status_flags & RQSF_XFER_COMPLETE) {
					XS_RESID(xs) = 0;
				} else if (sp->req_scsi_status & RQCS_RESID) {
					XS_RESID(xs) = sp->req_resid;
				} else {
					XS_RESID(xs) = 0;
				}
				if ((sp->req_state_flags & RQSF_GOT_STATUS) &&
				    (*XS_STSP(xs) == SCSI_CHECK) &&
				    (sp->req_scsi_status & RQCS_SV)) {
					XS_SAVE_SENSE(xs, sp);
					/* solely for the benefit of debug */
					sp->req_state_flags |= RQSF_GOT_SENSE;
				}
			}
			isp_prt(isp, ISP_LOGDEBUG2,
			   "asked for %ld got resid %ld", (long) XS_XFRLEN(xs),
			   (long) sp->req_resid);
			break;
		case RQSTYPE_REQUEST:
			if (sp->req_header.rqs_flags & RQSFLAG_FULL) {
				/*
				 * Force Queue Full status.
				 */
				*XS_STSP(xs) = SCSI_QFULL;
				XS_SETERR(xs, HBA_NOERROR);
			} else if (XS_NOERR(xs)) {
				/*
				 * ????
				 */
				isp_prt(isp, ISP_LOGDEBUG0,
				    "Request Queue Entry bounced back");
				XS_SETERR(xs, HBA_BOTCH);
			}
			XS_RESID(xs) = XS_XFRLEN(xs);
			break;
		default:
			isp_prt(isp, ISP_LOGWARN,
			    "unhandled response queue type 0x%x",
			    sp->req_header.rqs_entry_type);
			if (XS_NOERR(xs)) {
				XS_SETERR(xs, HBA_BOTCH);
			}
			break;
		}

		/*
		 * Free any dma resources. As a side effect, this may
		 * also do any cache flushing necessary for data coherence.			 */
		if (XS_XFRLEN(xs)) {
			ISP_DMAFREE(isp, xs, sp->req_handle);
		}

		if (((isp->isp_dblev & (ISP_LOGDEBUG2|ISP_LOGDEBUG3))) ||
		    ((isp->isp_dblev & ISP_LOGDEBUG1) && ((!XS_NOERR(xs)) ||
		    (*XS_STSP(xs) != SCSI_GOOD)))) {
			char skey;
			if (sp->req_state_flags & RQSF_GOT_SENSE) {
				skey = XS_SNSKEY(xs) & 0xf;
				if (skey < 10)
					skey += '0';
				else
					skey += 'a' - 10;
			} else if (*XS_STSP(xs) == SCSI_CHECK) {
				skey = '?';
			} else {
				skey = '.';
			}
			isp_prt(isp, ISP_LOGALL, finmsg, XS_CHANNEL(xs),
			    XS_TGT(xs), XS_LUN(xs), XS_XFRLEN(xs), XS_RESID(xs),
			    *XS_STSP(xs), skey, XS_ERR(xs));
		}

		if (isp->isp_nactive > 0)
		    isp->isp_nactive--;
		complist[ndone++] = xs;	/* defer completion call until later */
		MEMZERO(hp, QENTRY_LEN);	/* PERF */
		if (ndone == MAX_REQUESTQ_COMPLETIONS) {
			break;
		}
	}

	/*
	 * If we looked at any commands, then it's valid to find out
	 * what the outpointer is. It also is a trigger to update the
	 * ISP's notion of what we've seen so far.
	 */
	if (nlooked) {
		WRITE_RESPONSE_QUEUE_OUT_POINTER(isp, optr);
		/*
		 * While we're at it, read the requst queue out pointer.
		 */
		isp->isp_reqodx = READ_REQUEST_QUEUE_OUT_POINTER(isp);
		if (isp->isp_rscchiwater < ndone)
			isp->isp_rscchiwater = ndone;
	}

	isp->isp_residx = optr;
	isp->isp_rspbsy = 0;
	for (i = 0; i < ndone; i++) {
		xs = complist[i];
		if (xs) {
			isp->isp_rsltccmplt++;
			isp_done(xs);
		}
	}
}

/*
 * Support routines.
 */

static int
isp_parse_async(struct ispsoftc *isp, u_int16_t mbox)
{
	int rval = 0;
	int bus;

	if (IS_DUALBUS(isp)) {
		bus = ISP_READ(isp, OUTMAILBOX6);
	} else {
		bus = 0;
	}
	isp_prt(isp, ISP_LOGDEBUG2, "Async Mbox 0x%x", mbox);

	switch (mbox) {
	case ASYNC_BUS_RESET:
		isp->isp_sendmarker |= (1 << bus);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox))
			rval = -1;
#endif
		isp_async(isp, ISPASYNC_BUS_RESET, &bus);
		break;
	case ASYNC_SYSTEM_ERROR:
#ifdef	ISP_FW_CRASH_DUMP
		/*
		 * If we have crash dumps enabled, it's up to the handler
		 * for isp_async to reinit stuff and restart the firmware
		 * after performing the crash dump. The reason we do things
		 * this way is that we may need to activate a kernel thread
		 * to do all the crash dump goop.
		 */
		isp_async(isp, ISPASYNC_FW_CRASH, NULL);
#else
		isp_async(isp, ISPASYNC_FW_CRASH, NULL);
		isp_reinit(isp);
		isp_async(isp, ISPASYNC_FW_RESTARTED, NULL);
#endif
		rval = -1;
		break;

	case ASYNC_RQS_XFER_ERR:
		isp_prt(isp, ISP_LOGERR, "Request Queue Transfer Error");
		break;

	case ASYNC_RSP_XFER_ERR:
		isp_prt(isp, ISP_LOGERR, "Response Queue Transfer Error");
		break;

	case ASYNC_QWAKEUP:
		/*
		 * We've just been notified that the Queue has woken up.
		 * We don't need to be chatty about this- just unlatch things
		 * and move on.
		 */
		mbox = READ_REQUEST_QUEUE_OUT_POINTER(isp);
		break;

	case ASYNC_TIMEOUT_RESET:
		isp_prt(isp, ISP_LOGWARN,
		    "timeout initiated SCSI bus reset of bus %d", bus);
		isp->isp_sendmarker |= (1 << bus);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox))
			rval = -1;
#endif
		break;

	case ASYNC_DEVICE_RESET:
		isp_prt(isp, ISP_LOGINFO, "device reset on bus %d", bus);
		isp->isp_sendmarker |= (1 << bus);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox))
			rval = -1;
#endif
		break;

	case ASYNC_EXTMSG_UNDERRUN:
		isp_prt(isp, ISP_LOGWARN, "extended message underrun");
		break;

	case ASYNC_SCAM_INT:
		isp_prt(isp, ISP_LOGINFO, "SCAM interrupt");
		break;

	case ASYNC_HUNG_SCSI:
		isp_prt(isp, ISP_LOGERR,
		    "stalled SCSI Bus after DATA Overrun");
		/* XXX: Need to issue SCSI reset at this point */
		break;

	case ASYNC_KILLED_BUS:
		isp_prt(isp, ISP_LOGERR, "SCSI Bus reset after DATA Overrun");
		break;

	case ASYNC_BUS_TRANSIT:
		mbox = ISP_READ(isp, OUTMAILBOX2);
		switch (mbox & 0x1c00) {
		case SXP_PINS_LVD_MODE:
			isp_prt(isp, ISP_LOGINFO, "Transition to LVD mode");
			SDPARAM(isp)->isp_diffmode = 0;
			SDPARAM(isp)->isp_ultramode = 0;
			SDPARAM(isp)->isp_lvdmode = 1;
			break;
		case SXP_PINS_HVD_MODE:
			isp_prt(isp, ISP_LOGINFO,
			    "Transition to Differential mode");
			SDPARAM(isp)->isp_diffmode = 1;
			SDPARAM(isp)->isp_ultramode = 0;
			SDPARAM(isp)->isp_lvdmode = 0;
			break;
		case SXP_PINS_SE_MODE:
			isp_prt(isp, ISP_LOGINFO,
			    "Transition to Single Ended mode");
			SDPARAM(isp)->isp_diffmode = 0;
			SDPARAM(isp)->isp_ultramode = 1;
			SDPARAM(isp)->isp_lvdmode = 0;
			break;
		default:
			isp_prt(isp, ISP_LOGWARN,
			    "Transition to Unknown Mode 0x%x", mbox);
			break;
		}
		/*
		 * XXX: Set up to renegotiate again!
		 */
		/* Can only be for a 1080... */
		isp->isp_sendmarker |= (1 << bus);
		break;

	/*
	 * We can use bus, which will always be zero for FC cards,
	 * as a mailbox pattern accumulator to be checked below.
	 */
	case ASYNC_RIO5:
		bus = 0x1ce;	/* outgoing mailbox regs 1-3, 6-7 */
		break;

	case ASYNC_RIO4:
		bus = 0x14e;	/* outgoing mailbox regs 1-3, 6 */
		break;

	case ASYNC_RIO3:
		bus = 0x10e;	/* outgoing mailbox regs 1-3 */
		break;

	case ASYNC_RIO2:
		bus = 0x106;	/* outgoing mailbox regs 1-2 */
		break;

	case ASYNC_RIO1:
	case ASYNC_CMD_CMPLT:
		bus = 0x102;	/* outgoing mailbox regs 1 */
		break;

	case ASYNC_RIO_RESP:
		return (rval);

	case ASYNC_CTIO_DONE:
	{
#ifdef	ISP_TARGET_MODE
		int handle =
		    (ISP_READ(isp, OUTMAILBOX2) << 16) | 
		    (ISP_READ(isp, OUTMAILBOX1));
		if (isp_target_async(isp, handle, mbox))
			rval = -1;
#else
		isp_prt(isp, ISP_LOGINFO, "Fast Posting CTIO done");
#endif
		isp->isp_fphccmplt++;	/* count it as a fast posting intr */
		break;
	}
	case ASYNC_LIP_F8:
	case ASYNC_LIP_OCCURRED:
		FCPARAM(isp)->isp_lipseq =
		    ISP_READ(isp, OUTMAILBOX1);
		FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp)->isp_loopstate = LOOP_LIP_RCVD;
		isp->isp_sendmarker = 1;
		isp_mark_getpdb_all(isp);
		isp_async(isp, ISPASYNC_LIP, NULL);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox))
			rval = -1;
#endif
		/*
		 * We've had problems with data corruption occuring on
		 * commands that complete (with no apparent error) after
		 * we receive a LIP. This has been observed mostly on
		 * Local Loop topologies. To be safe, let's just mark
		 * all active commands as dead.
		 */
		if (FCPARAM(isp)->isp_topo == TOPO_NL_PORT ||
		    FCPARAM(isp)->isp_topo == TOPO_FL_PORT) {
			int i, j;
			for (i = j = 0; i < isp->isp_maxcmds; i++) {
				XS_T *xs;
				xs = isp->isp_xflist[i];
				if (xs != NULL) {
					j++;
					XS_SETERR(xs, HBA_BUSRESET);
				}
			}
			if (j) {
				isp_prt(isp, ISP_LOGERR,
				    "LIP destroyed %d active commands", j);
			}
		}
		break;

	case ASYNC_LOOP_UP:
		isp->isp_sendmarker = 1;
		FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp)->isp_loopstate = LOOP_LIP_RCVD;
		isp_mark_getpdb_all(isp);
		isp_async(isp, ISPASYNC_LOOP_UP, NULL);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox))
			rval = -1;
#endif
		break;

	case ASYNC_LOOP_DOWN:
		isp->isp_sendmarker = 1;
		FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp)->isp_loopstate = LOOP_NIL;
		isp_mark_getpdb_all(isp);
		isp_async(isp, ISPASYNC_LOOP_DOWN, NULL);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox))
			rval = -1;
#endif
		break;

	case ASYNC_LOOP_RESET:
		isp->isp_sendmarker = 1;
		FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp)->isp_loopstate = LOOP_NIL;
		isp_mark_getpdb_all(isp);
		isp_async(isp, ISPASYNC_LOOP_RESET, NULL);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox))
			rval = -1;
#endif
		break;

	case ASYNC_PDB_CHANGED:
		isp->isp_sendmarker = 1;
		FCPARAM(isp)->isp_loopstate = LOOP_PDB_RCVD;
		isp_mark_getpdb_all(isp);
		isp_async(isp, ISPASYNC_CHANGE_NOTIFY, ISPASYNC_CHANGE_PDB);
		break;

	case ASYNC_CHANGE_NOTIFY:
		/*
		 * Not correct, but it will force us to rescan the loop.
		 */
		FCPARAM(isp)->isp_loopstate = LOOP_PDB_RCVD;
		isp_mark_getpdb_all(isp);
		isp_async(isp, ISPASYNC_CHANGE_NOTIFY, ISPASYNC_CHANGE_SNS);
		break;

	case ASYNC_PTPMODE:
		if (FCPARAM(isp)->isp_onfabric)
			FCPARAM(isp)->isp_topo = TOPO_F_PORT;
		else
			FCPARAM(isp)->isp_topo = TOPO_N_PORT;
		isp_mark_getpdb_all(isp);
		isp->isp_sendmarker = 1;
		FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp)->isp_loopstate = LOOP_LIP_RCVD;
		isp_async(isp, ISPASYNC_CHANGE_NOTIFY, ISPASYNC_CHANGE_OTHER);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox))
			rval = -1;
#endif
		isp_prt(isp, ISP_LOGINFO, "Point-to-Point mode");
		break;

	case ASYNC_CONNMODE:
		mbox = ISP_READ(isp, OUTMAILBOX1);
		isp_mark_getpdb_all(isp);
		switch (mbox) {
		case ISP_CONN_LOOP:
			isp_prt(isp, ISP_LOGINFO,
			    "Point-to-Point -> Loop mode");
			break;
		case ISP_CONN_PTP:
			isp_prt(isp, ISP_LOGINFO,
			    "Loop -> Point-to-Point mode");
			break;
		case ISP_CONN_BADLIP:
			isp_prt(isp, ISP_LOGWARN,
			    "Point-to-Point -> Loop mode (BAD LIP)");
			break;
		case ISP_CONN_FATAL:
			isp_prt(isp, ISP_LOGERR, "FATAL CONNECTION ERROR");
#ifdef	ISP_FW_CRASH_DUMP
			isp_async(isp, ISPASYNC_FW_CRASH, NULL);
#else
			isp_async(isp, ISPASYNC_FW_CRASH, NULL);
			isp_reinit(isp);
			isp_async(isp, ISPASYNC_FW_RESTARTED, NULL);
#endif
			return (-1);
		case ISP_CONN_LOOPBACK:
			isp_prt(isp, ISP_LOGWARN,
			    "Looped Back in Point-to-Point mode");
			break;
		default:
			isp_prt(isp, ISP_LOGWARN,
			    "Unknown connection mode (0x%x)", mbox);
			break;
		}
		isp_async(isp, ISPASYNC_CHANGE_NOTIFY, ISPASYNC_CHANGE_OTHER);
		isp->isp_sendmarker = 1;
		FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp)->isp_loopstate = LOOP_LIP_RCVD;
		break;

	default:
		isp_prt(isp, ISP_LOGWARN, "Unknown Async Code 0x%x", mbox);
		break;
	}

	if (bus & 0x100) {
		int i, nh;
		u_int16_t handles[5];

		for (nh = 0, i = 1; i < MAX_MAILBOX; i++) {
			if ((bus & (1 << i)) == 0) {
				continue;
			}
			handles[nh++] = ISP_READ(isp, MBOX_OFF(i));
		}
		for (i = 0; i < nh; i++) {
			isp_fastpost_complete(isp, handles[i]);
			isp_prt(isp,  ISP_LOGDEBUG3,
			    "fast post completion of %u", handles[i]);
		}
		if (isp->isp_fpcchiwater < nh)
			isp->isp_fpcchiwater = nh;
	} else {
		isp->isp_intoasync++;
	}
	return (rval);
}

/*
 * Handle other response entries. A pointer to the request queue output
 * index is here in case we want to eat several entries at once, although
 * this is not used currently.
 */

static int
isp_handle_other_response(struct ispsoftc *isp, int type,
    isphdr_t *hp, u_int16_t *optrp)
{
	switch (type) {
	case RQSTYPE_STATUS_CONT:
		isp_prt(isp, ISP_LOGINFO, "Ignored Continuation Response");
		return (1);
	case RQSTYPE_ATIO:
	case RQSTYPE_CTIO:
	case RQSTYPE_ENABLE_LUN:
	case RQSTYPE_MODIFY_LUN:
	case RQSTYPE_NOTIFY:
	case RQSTYPE_NOTIFY_ACK:
	case RQSTYPE_CTIO1:
	case RQSTYPE_ATIO2:
	case RQSTYPE_CTIO2:
	case RQSTYPE_CTIO3:
		isp->isp_rsltccmplt++;	/* count as a response completion */
#ifdef	ISP_TARGET_MODE
		if (isp_target_notify(isp, (ispstatusreq_t *) hp, optrp)) {
			return (1);
		}
#else
		optrp = optrp;
		/* FALLTHROUGH */
#endif
	case RQSTYPE_REQUEST:
	default:
		if (isp_async(isp, ISPASYNC_UNHANDLED_RESPONSE, hp)) {
			return (1);
		}
		isp_prt(isp, ISP_LOGWARN, "Unhandled Response Type 0x%x",
		    isp_get_response_type(isp, hp));
		return (0);
	}
}

static void
isp_parse_status(struct ispsoftc *isp, ispstatusreq_t *sp, XS_T *xs)
{
	switch (sp->req_completion_status & 0xff) {
	case RQCS_COMPLETE:
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_NOERROR);
		}
		return;

	case RQCS_INCOMPLETE:
		if ((sp->req_state_flags & RQSF_GOT_TARGET) == 0) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "Selection Timeout for %d.%d.%d",
			    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
			if (XS_NOERR(xs)) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
			}
			return;
		}
		isp_prt(isp, ISP_LOGERR,
		    "command incomplete for %d.%d.%d, state 0x%x",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs),
		    sp->req_state_flags);
		break;

	case RQCS_DMA_ERROR:
		isp_prt(isp, ISP_LOGERR, "DMA error for command on %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_TRANSPORT_ERROR:
	{
		char buf[172];
		SNPRINTF(buf, sizeof (buf), "states=>");
		if (sp->req_state_flags & RQSF_GOT_BUS) {
			SNPRINTF(buf, sizeof (buf), "%s GOT_BUS", buf);
		}
		if (sp->req_state_flags & RQSF_GOT_TARGET) {
			SNPRINTF(buf, sizeof (buf), "%s GOT_TGT", buf);
		}
		if (sp->req_state_flags & RQSF_SENT_CDB) {
			SNPRINTF(buf, sizeof (buf), "%s SENT_CDB", buf);
		}
		if (sp->req_state_flags & RQSF_XFRD_DATA) {
			SNPRINTF(buf, sizeof (buf), "%s XFRD_DATA", buf);
		}
		if (sp->req_state_flags & RQSF_GOT_STATUS) {
			SNPRINTF(buf, sizeof (buf), "%s GOT_STS", buf);
		}
		if (sp->req_state_flags & RQSF_GOT_SENSE) {
			SNPRINTF(buf, sizeof (buf), "%s GOT_SNS", buf);
		}
		if (sp->req_state_flags & RQSF_XFER_COMPLETE) {
			SNPRINTF(buf, sizeof (buf), "%s XFR_CMPLT", buf);
		}
		SNPRINTF(buf, sizeof (buf), "%s\nstatus=>", buf);
		if (sp->req_status_flags & RQSTF_DISCONNECT) {
			SNPRINTF(buf, sizeof (buf), "%s Disconnect", buf);
		}
		if (sp->req_status_flags & RQSTF_SYNCHRONOUS) {
			SNPRINTF(buf, sizeof (buf), "%s Sync_xfr", buf);
		}
		if (sp->req_status_flags & RQSTF_PARITY_ERROR) {
			SNPRINTF(buf, sizeof (buf), "%s Parity", buf);
		}
		if (sp->req_status_flags & RQSTF_BUS_RESET) {
			SNPRINTF(buf, sizeof (buf), "%s Bus_Reset", buf);
		}
		if (sp->req_status_flags & RQSTF_DEVICE_RESET) {
			SNPRINTF(buf, sizeof (buf), "%s Device_Reset", buf);
		}
		if (sp->req_status_flags & RQSTF_ABORTED) {
			SNPRINTF(buf, sizeof (buf), "%s Aborted", buf);
		}
		if (sp->req_status_flags & RQSTF_TIMEOUT) {
			SNPRINTF(buf, sizeof (buf), "%s Timeout", buf);
		}
		if (sp->req_status_flags & RQSTF_NEGOTIATION) {
			SNPRINTF(buf, sizeof (buf), "%s Negotiation", buf);
		}
		isp_prt(isp, ISP_LOGERR, "%s", buf);
		isp_prt(isp, ISP_LOGERR, "transport error for %d.%d.%d:\n%s",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs), buf);
		break;
	}
	case RQCS_RESET_OCCURRED:
		isp_prt(isp, ISP_LOGWARN,
		    "bus reset destroyed command for %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		isp->isp_sendmarker |= (1 << XS_CHANNEL(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_BUSRESET);
		}
		return;

	case RQCS_ABORTED:
		isp_prt(isp, ISP_LOGERR, "command aborted for %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		isp->isp_sendmarker |= (1 << XS_CHANNEL(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_ABORTED);
		}
		return;

	case RQCS_TIMEOUT:
		isp_prt(isp, ISP_LOGWARN, "command timed out for %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		/*
	 	 * Check to see if we logged out the device.
		 */
		if (IS_FC(isp)) {
			if ((sp->req_completion_status & RQSTF_LOGOUT) &&
			    FCPARAM(isp)->portdb[XS_TGT(xs)].valid &&
			    FCPARAM(isp)->portdb[XS_TGT(xs)].fabric_dev) {
				FCPARAM(isp)->portdb[XS_TGT(xs)].relogin = 1;
			}
		}
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_CMDTIMEOUT);
		}
		return;

	case RQCS_DATA_OVERRUN:
		XS_RESID(xs) = sp->req_resid;
		isp_prt(isp, ISP_LOGERR, "data overrun for command on %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_DATAOVR);
		}
		return;

	case RQCS_COMMAND_OVERRUN:
		isp_prt(isp, ISP_LOGERR,
		    "command overrun for command on %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_STATUS_OVERRUN:
		isp_prt(isp, ISP_LOGERR,
		    "status overrun for command on %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_BAD_MESSAGE:
		isp_prt(isp, ISP_LOGERR,
		    "msg not COMMAND COMPLETE after status %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_NO_MESSAGE_OUT:
		isp_prt(isp, ISP_LOGERR,
		    "No MESSAGE OUT phase after selection on %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_EXT_ID_FAILED:
		isp_prt(isp, ISP_LOGERR, "EXTENDED IDENTIFY failed %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_IDE_MSG_FAILED:
		isp_prt(isp, ISP_LOGERR,
		    "INITIATOR DETECTED ERROR rejected by %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_ABORT_MSG_FAILED:
		isp_prt(isp, ISP_LOGERR, "ABORT OPERATION rejected by %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_REJECT_MSG_FAILED:
		isp_prt(isp, ISP_LOGERR, "MESSAGE REJECT rejected by %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_NOP_MSG_FAILED:
		isp_prt(isp, ISP_LOGERR, "NOP rejected by %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_PARITY_ERROR_MSG_FAILED:
		isp_prt(isp, ISP_LOGERR,
		    "MESSAGE PARITY ERROR rejected by %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_DEVICE_RESET_MSG_FAILED:
		isp_prt(isp, ISP_LOGWARN,
		    "BUS DEVICE RESET rejected by %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_ID_MSG_FAILED:
		isp_prt(isp, ISP_LOGERR, "IDENTIFY rejected by %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_UNEXP_BUS_FREE:
		isp_prt(isp, ISP_LOGERR, "%d.%d.%d had an unexpected bus free",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_DATA_UNDERRUN:
	{
		if (IS_FC(isp)) {
			int ru_marked = (sp->req_scsi_status & RQCS_RU) != 0;
			if (!ru_marked || sp->req_resid > XS_XFRLEN(xs)) {
				isp_prt(isp, ISP_LOGWARN, bun, XS_TGT(xs),
				    XS_LUN(xs), XS_XFRLEN(xs), sp->req_resid,
				    (ru_marked)? "marked" : "not marked");
				if (XS_NOERR(xs)) {
					XS_SETERR(xs, HBA_BOTCH);
				}
				return;
			}
		}
		XS_RESID(xs) = sp->req_resid;
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_NOERROR);
		}
		return;
	}

	case RQCS_XACT_ERR1:
		isp_prt(isp, ISP_LOGERR, xact1, XS_CHANNEL(xs),
		    XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_XACT_ERR2:
		isp_prt(isp, ISP_LOGERR, xact2,
		    XS_LUN(xs), XS_TGT(xs), XS_CHANNEL(xs));
		break;

	case RQCS_XACT_ERR3:
		isp_prt(isp, ISP_LOGERR, xact3,
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_BAD_ENTRY:
		isp_prt(isp, ISP_LOGERR, "Invalid IOCB entry type detected");
		break;

	case RQCS_QUEUE_FULL:
		isp_prt(isp, ISP_LOGDEBUG0,
		    "internal queues full for %d.%d.%d status 0x%x",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs), *XS_STSP(xs));

		/*
		 * If QFULL or some other status byte is set, then this
		 * isn't an error, per se.
		 *
		 * Unfortunately, some QLogic f/w writers have, in
		 * some cases, ommitted to *set* status to QFULL.
		 *

		if (*XS_STSP(xs) != SCSI_GOOD && XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_NOERROR);
			return;
		}

		 *
		 *
		 */

		*XS_STSP(xs) = SCSI_QFULL;
		XS_SETERR(xs, HBA_NOERROR);
		return;

	case RQCS_PHASE_SKIPPED:
		isp_prt(isp, ISP_LOGERR, pskip, XS_CHANNEL(xs),
		    XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_ARQS_FAILED:
		isp_prt(isp, ISP_LOGERR,
		    "Auto Request Sense failed for %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_ARQFAIL);
		}
		return;

	case RQCS_WIDE_FAILED:
		isp_prt(isp, ISP_LOGERR,
		    "Wide Negotiation failed for %d.%d.%d",
		    XS_TGT(xs), XS_LUN(xs), XS_CHANNEL(xs));
		if (IS_SCSI(isp)) {
			sdparam *sdp = isp->isp_param;
			sdp += XS_CHANNEL(xs);
			sdp->isp_devparam[XS_TGT(xs)].goal_flags &= ~DPARM_WIDE;
			sdp->isp_devparam[XS_TGT(xs)].dev_update = 1;
			isp->isp_update |= (1 << XS_CHANNEL(xs));
		}
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_NOERROR);
		}
		return;

	case RQCS_SYNCXFER_FAILED:
		isp_prt(isp, ISP_LOGERR,
		    "SDTR Message failed for target %d.%d.%d",
		    XS_TGT(xs), XS_LUN(xs), XS_CHANNEL(xs));
		if (IS_SCSI(isp)) {
			sdparam *sdp = isp->isp_param;
			sdp += XS_CHANNEL(xs);
			sdp->isp_devparam[XS_TGT(xs)].goal_flags &= ~DPARM_SYNC;
			sdp->isp_devparam[XS_TGT(xs)].dev_update = 1;
			isp->isp_update |= (1 << XS_CHANNEL(xs));
		}
		break;

	case RQCS_LVD_BUSERR:
		isp_prt(isp, ISP_LOGERR,
		    "Bad LVD condition while talking to %d.%d.%d",
		    XS_TGT(xs), XS_LUN(xs), XS_CHANNEL(xs));
		break;

	case RQCS_PORT_UNAVAILABLE:
		/*
		 * No such port on the loop. Moral equivalent of SELTIMEO
		 */
	case RQCS_PORT_LOGGED_OUT:
		/*
		 * It was there (maybe)- treat as a selection timeout.
		 */
		if ((sp->req_completion_status & 0xff) == RQCS_PORT_UNAVAILABLE)
			isp_prt(isp, ISP_LOGINFO,
			    "port unavailable for target %d", XS_TGT(xs));
		else
			isp_prt(isp, ISP_LOGINFO,
			    "port logout for target %d", XS_TGT(xs));
		/*
		 * If we're on a local loop, force a LIP (which is overkill)
		 * to force a re-login of this unit. If we're on fabric,
		 * then we'll have to relogin as a matter of course.
		 */
		if (FCPARAM(isp)->isp_topo == TOPO_NL_PORT ||
		    FCPARAM(isp)->isp_topo == TOPO_FL_PORT) {
			mbreg_t mbs;
			mbs.param[0] = MBOX_INIT_LIP;
			isp_mboxcmd_qnw(isp, &mbs, 1);
		}

		/*
		 * Probably overkill.
		 */
		isp->isp_sendmarker = 1;
		FCPARAM(isp)->isp_loopstate = LOOP_PDB_RCVD;
		isp_mark_getpdb_all(isp);
		isp_async(isp, ISPASYNC_CHANGE_NOTIFY, ISPASYNC_CHANGE_OTHER);
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
		}
		return;

	case RQCS_PORT_CHANGED:
		isp_prt(isp, ISP_LOGWARN,
		    "port changed for target %d", XS_TGT(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
		}
		return;

	case RQCS_PORT_BUSY:
		isp_prt(isp, ISP_LOGWARN,
		    "port busy for target %d", XS_TGT(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_TGTBSY);
		}
		return;

	default:
		isp_prt(isp, ISP_LOGERR, "Unknown Completion Status 0x%x",
		    sp->req_completion_status);
		break;
	}
	if (XS_NOERR(xs)) {
		XS_SETERR(xs, HBA_BOTCH);
	}
}

static void
isp_fastpost_complete(struct ispsoftc *isp, u_int16_t fph)
{
	XS_T *xs;

	if (fph == 0) {
		return;
	}
	xs = isp_find_xs(isp, fph);
	if (xs == NULL) {
		isp_prt(isp, ISP_LOGWARN,
		    "Command for fast post handle 0x%x not found", fph);
		return;
	}
	isp_destroy_handle(isp, fph);

	/*
	 * Since we don't have a result queue entry item,
	 * we must believe that SCSI status is zero and
	 * that all data transferred.
	 */
	XS_SET_STATE_STAT(isp, xs, NULL);
	XS_RESID(xs) = 0;
	*XS_STSP(xs) = SCSI_GOOD;
	if (XS_XFRLEN(xs)) {
		ISP_DMAFREE(isp, xs, fph);
	}
	if (isp->isp_nactive)
		isp->isp_nactive--;
	isp->isp_fphccmplt++;
	isp_done(xs);
}

static int
isp_mbox_continue(struct ispsoftc *isp)
{
	mbreg_t mbs;
	u_int16_t *ptr;

	switch (isp->isp_lastmbxcmd) {
	case MBOX_WRITE_RAM_WORD:
	case MBOX_READ_RAM_WORD:
	case MBOX_READ_RAM_WORD_EXTENDED:
		break;
	default:
		return (1);
	}
	if (isp->isp_mboxtmp[0] != MBOX_COMMAND_COMPLETE) {
		isp->isp_mbxwrk0 = 0;
		return (-1);
	}


	/*
	 * Clear the previous interrupt.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
	ISP_WRITE(isp, BIU_SEMA, 0);

	/*
	 * Continue with next word.
	 */
	ptr = isp->isp_mbxworkp;
	switch (isp->isp_lastmbxcmd) {
	case MBOX_WRITE_RAM_WORD:
		mbs.param[2] = *ptr++;
		mbs.param[1] = isp->isp_mbxwrk1++;
		break;
	case MBOX_READ_RAM_WORD:
	case MBOX_READ_RAM_WORD_EXTENDED:
		*ptr++ = isp->isp_mboxtmp[2];
		mbs.param[1] = isp->isp_mbxwrk1++;
		break;
	}
	isp->isp_mbxworkp = ptr;
	mbs.param[0] = isp->isp_lastmbxcmd;
	isp->isp_mbxwrk0 -= 1;
	isp_mboxcmd_qnw(isp, &mbs, 0);
	return (0);
}


#define	HIBYT(x)			((x) >> 0x8)
#define	LOBYT(x)			((x)  & 0xff)
#define	ISPOPMAP(a, b)			(((a) << 8) | (b))
static u_int16_t mbpscsi[] = {
	ISPOPMAP(0x01, 0x01),	/* 0x00: MBOX_NO_OP */
	ISPOPMAP(0x1f, 0x01),	/* 0x01: MBOX_LOAD_RAM */
	ISPOPMAP(0x03, 0x01),	/* 0x02: MBOX_EXEC_FIRMWARE */
	ISPOPMAP(0x1f, 0x01),	/* 0x03: MBOX_DUMP_RAM */
	ISPOPMAP(0x07, 0x07),	/* 0x04: MBOX_WRITE_RAM_WORD */
	ISPOPMAP(0x03, 0x07),	/* 0x05: MBOX_READ_RAM_WORD */
	ISPOPMAP(0x3f, 0x3f),	/* 0x06: MBOX_MAILBOX_REG_TEST */
	ISPOPMAP(0x03, 0x07),	/* 0x07: MBOX_VERIFY_CHECKSUM	*/
	ISPOPMAP(0x01, 0x0f),	/* 0x08: MBOX_ABOUT_FIRMWARE */
	ISPOPMAP(0x00, 0x00),	/* 0x09: */
	ISPOPMAP(0x00, 0x00),	/* 0x0a: */
	ISPOPMAP(0x00, 0x00),	/* 0x0b: */
	ISPOPMAP(0x00, 0x00),	/* 0x0c: */
	ISPOPMAP(0x00, 0x00),	/* 0x0d: */
	ISPOPMAP(0x01, 0x05),	/* 0x0e: MBOX_CHECK_FIRMWARE */
	ISPOPMAP(0x00, 0x00),	/* 0x0f: */
	ISPOPMAP(0x1f, 0x1f),	/* 0x10: MBOX_INIT_REQ_QUEUE */
	ISPOPMAP(0x3f, 0x3f),	/* 0x11: MBOX_INIT_RES_QUEUE */
	ISPOPMAP(0x0f, 0x0f),	/* 0x12: MBOX_EXECUTE_IOCB */
	ISPOPMAP(0x03, 0x03),	/* 0x13: MBOX_WAKE_UP	*/
	ISPOPMAP(0x01, 0x3f),	/* 0x14: MBOX_STOP_FIRMWARE */
	ISPOPMAP(0x0f, 0x0f),	/* 0x15: MBOX_ABORT */
	ISPOPMAP(0x03, 0x03),	/* 0x16: MBOX_ABORT_DEVICE */
	ISPOPMAP(0x07, 0x07),	/* 0x17: MBOX_ABORT_TARGET */
	ISPOPMAP(0x07, 0x07),	/* 0x18: MBOX_BUS_RESET */
	ISPOPMAP(0x03, 0x07),	/* 0x19: MBOX_STOP_QUEUE */
	ISPOPMAP(0x03, 0x07),	/* 0x1a: MBOX_START_QUEUE */
	ISPOPMAP(0x03, 0x07),	/* 0x1b: MBOX_SINGLE_STEP_QUEUE */
	ISPOPMAP(0x03, 0x07),	/* 0x1c: MBOX_ABORT_QUEUE */
	ISPOPMAP(0x03, 0x4f),	/* 0x1d: MBOX_GET_DEV_QUEUE_STATUS */
	ISPOPMAP(0x00, 0x00),	/* 0x1e: */
	ISPOPMAP(0x01, 0x07),	/* 0x1f: MBOX_GET_FIRMWARE_STATUS */
	ISPOPMAP(0x01, 0x07),	/* 0x20: MBOX_GET_INIT_SCSI_ID */
	ISPOPMAP(0x01, 0x07),	/* 0x21: MBOX_GET_SELECT_TIMEOUT */
	ISPOPMAP(0x01, 0xc7),	/* 0x22: MBOX_GET_RETRY_COUNT	*/
	ISPOPMAP(0x01, 0x07),	/* 0x23: MBOX_GET_TAG_AGE_LIMIT */
	ISPOPMAP(0x01, 0x03),	/* 0x24: MBOX_GET_CLOCK_RATE */
	ISPOPMAP(0x01, 0x07),	/* 0x25: MBOX_GET_ACT_NEG_STATE */
	ISPOPMAP(0x01, 0x07),	/* 0x26: MBOX_GET_ASYNC_DATA_SETUP_TIME */
	ISPOPMAP(0x01, 0x07),	/* 0x27: MBOX_GET_PCI_PARAMS */
	ISPOPMAP(0x03, 0x4f),	/* 0x28: MBOX_GET_TARGET_PARAMS */
	ISPOPMAP(0x03, 0x0f),	/* 0x29: MBOX_GET_DEV_QUEUE_PARAMS */
	ISPOPMAP(0x01, 0x07),	/* 0x2a: MBOX_GET_RESET_DELAY_PARAMS */
	ISPOPMAP(0x00, 0x00),	/* 0x2b: */
	ISPOPMAP(0x00, 0x00),	/* 0x2c: */
	ISPOPMAP(0x00, 0x00),	/* 0x2d: */
	ISPOPMAP(0x00, 0x00),	/* 0x2e: */
	ISPOPMAP(0x00, 0x00),	/* 0x2f: */
	ISPOPMAP(0x03, 0x03),	/* 0x30: MBOX_SET_INIT_SCSI_ID */
	ISPOPMAP(0x07, 0x07),	/* 0x31: MBOX_SET_SELECT_TIMEOUT */
	ISPOPMAP(0xc7, 0xc7),	/* 0x32: MBOX_SET_RETRY_COUNT	*/
	ISPOPMAP(0x07, 0x07),	/* 0x33: MBOX_SET_TAG_AGE_LIMIT */
	ISPOPMAP(0x03, 0x03),	/* 0x34: MBOX_SET_CLOCK_RATE */
	ISPOPMAP(0x07, 0x07),	/* 0x35: MBOX_SET_ACT_NEG_STATE */
	ISPOPMAP(0x07, 0x07),	/* 0x36: MBOX_SET_ASYNC_DATA_SETUP_TIME */
	ISPOPMAP(0x07, 0x07),	/* 0x37: MBOX_SET_PCI_CONTROL_PARAMS */
	ISPOPMAP(0x4f, 0x4f),	/* 0x38: MBOX_SET_TARGET_PARAMS */
	ISPOPMAP(0x0f, 0x0f),	/* 0x39: MBOX_SET_DEV_QUEUE_PARAMS */
	ISPOPMAP(0x07, 0x07),	/* 0x3a: MBOX_SET_RESET_DELAY_PARAMS */
	ISPOPMAP(0x00, 0x00),	/* 0x3b: */
	ISPOPMAP(0x00, 0x00),	/* 0x3c: */
	ISPOPMAP(0x00, 0x00),	/* 0x3d: */
	ISPOPMAP(0x00, 0x00),	/* 0x3e: */
	ISPOPMAP(0x00, 0x00),	/* 0x3f: */
	ISPOPMAP(0x01, 0x03),	/* 0x40: MBOX_RETURN_BIOS_BLOCK_ADDR */
	ISPOPMAP(0x3f, 0x01),	/* 0x41: MBOX_WRITE_FOUR_RAM_WORDS */
	ISPOPMAP(0x03, 0x07),	/* 0x42: MBOX_EXEC_BIOS_IOCB */
	ISPOPMAP(0x00, 0x00),	/* 0x43: */
	ISPOPMAP(0x00, 0x00),	/* 0x44: */
	ISPOPMAP(0x03, 0x03),	/* 0x45: SET SYSTEM PARAMETER */
	ISPOPMAP(0x01, 0x03),	/* 0x46: GET SYSTEM PARAMETER */
	ISPOPMAP(0x00, 0x00),	/* 0x47: */
	ISPOPMAP(0x01, 0xcf),	/* 0x48: GET SCAM CONFIGURATION */
	ISPOPMAP(0xcf, 0xcf),	/* 0x49: SET SCAM CONFIGURATION */
	ISPOPMAP(0x03, 0x03),	/* 0x4a: MBOX_SET_FIRMWARE_FEATURES */
	ISPOPMAP(0x01, 0x03),	/* 0x4b: MBOX_GET_FIRMWARE_FEATURES */
	ISPOPMAP(0x00, 0x00),	/* 0x4c: */
	ISPOPMAP(0x00, 0x00),	/* 0x4d: */
	ISPOPMAP(0x00, 0x00),	/* 0x4e: */
	ISPOPMAP(0x00, 0x00),	/* 0x4f: */
	ISPOPMAP(0xdf, 0xdf),	/* 0x50: LOAD RAM A64 */
	ISPOPMAP(0xdf, 0xdf),	/* 0x51: DUMP RAM A64 */
	ISPOPMAP(0xdf, 0xff),	/* 0x52: INITIALIZE REQUEST QUEUE A64 */
	ISPOPMAP(0xef, 0xff),	/* 0x53: INITIALIZE RESPONSE QUEUE A64 */
	ISPOPMAP(0xcf, 0x01),	/* 0x54: EXECUTE IOCB A64 */
	ISPOPMAP(0x07, 0x01),	/* 0x55: ENABLE TARGET MODE */
	ISPOPMAP(0x03, 0x0f),	/* 0x56: GET TARGET STATUS */
	ISPOPMAP(0x00, 0x00),	/* 0x57: */
	ISPOPMAP(0x00, 0x00),	/* 0x58: */
	ISPOPMAP(0x00, 0x00),	/* 0x59: */
	ISPOPMAP(0x03, 0x03),	/* 0x5a: SET DATA OVERRUN RECOVERY MODE */
	ISPOPMAP(0x01, 0x03),	/* 0x5b: GET DATA OVERRUN RECOVERY MODE */
	ISPOPMAP(0x0f, 0x0f),	/* 0x5c: SET HOST DATA */
	ISPOPMAP(0x01, 0x01)	/* 0x5d: GET NOST DATA */
};

#ifndef	ISP_STRIPPED
static char *scsi_mbcmd_names[] = {
	"NO-OP",
	"LOAD RAM",
	"EXEC FIRMWARE",
	"DUMP RAM",
	"WRITE RAM WORD",
	"READ RAM WORD",
	"MAILBOX REG TEST",
	"VERIFY CHECKSUM",
	"ABOUT FIRMWARE",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"CHECK FIRMWARE",
	NULL,
	"INIT REQUEST QUEUE",
	"INIT RESULT QUEUE",
	"EXECUTE IOCB",
	"WAKE UP",
	"STOP FIRMWARE",
	"ABORT",
	"ABORT DEVICE",
	"ABORT TARGET",
	"BUS RESET",
	"STOP QUEUE",
	"START QUEUE",
	"SINGLE STEP QUEUE",
	"ABORT QUEUE",
	"GET DEV QUEUE STATUS",
	NULL,
	"GET FIRMWARE STATUS",
	"GET INIT SCSI ID",
	"GET SELECT TIMEOUT",
	"GET RETRY COUNT",
	"GET TAG AGE LIMIT",
	"GET CLOCK RATE",
	"GET ACT NEG STATE",
	"GET ASYNC DATA SETUP TIME",
	"GET PCI PARAMS",
	"GET TARGET PARAMS",
	"GET DEV QUEUE PARAMS",
	"GET RESET DELAY PARAMS",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"SET INIT SCSI ID",
	"SET SELECT TIMEOUT",
	"SET RETRY COUNT",
	"SET TAG AGE LIMIT",
	"SET CLOCK RATE",
	"SET ACT NEG STATE",
	"SET ASYNC DATA SETUP TIME",
	"SET PCI CONTROL PARAMS",
	"SET TARGET PARAMS",
	"SET DEV QUEUE PARAMS",
	"SET RESET DELAY PARAMS",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"RETURN BIOS BLOCK ADDR",
	"WRITE FOUR RAM WORDS",
	"EXEC BIOS IOCB",
	NULL,
	NULL,
	"SET SYSTEM PARAMETER",
	"GET SYSTEM PARAMETER",
	NULL,
	"GET SCAM CONFIGURATION",
	"SET SCAM CONFIGURATION",
	"SET FIRMWARE FEATURES",
	"GET FIRMWARE FEATURES",
	NULL,
	NULL,
	NULL,
	NULL,
	"LOAD RAM A64",
	"DUMP RAM A64",
	"INITIALIZE REQUEST QUEUE A64",
	"INITIALIZE RESPONSE QUEUE A64",
	"EXECUTE IOCB A64",
	"ENABLE TARGET MODE",
	"GET TARGET MODE STATE",
	NULL,
	NULL,
	NULL,
	"SET DATA OVERRUN RECOVERY MODE",
	"GET DATA OVERRUN RECOVERY MODE",
	"SET HOST DATA",
	"GET NOST DATA",
};
#endif

static u_int16_t mbpfc[] = {
	ISPOPMAP(0x01, 0x01),	/* 0x00: MBOX_NO_OP */
	ISPOPMAP(0x1f, 0x01),	/* 0x01: MBOX_LOAD_RAM */
	ISPOPMAP(0x03, 0x01),	/* 0x02: MBOX_EXEC_FIRMWARE */
	ISPOPMAP(0xdf, 0x01),	/* 0x03: MBOX_DUMP_RAM */
	ISPOPMAP(0x07, 0x07),	/* 0x04: MBOX_WRITE_RAM_WORD */
	ISPOPMAP(0x03, 0x07),	/* 0x05: MBOX_READ_RAM_WORD */
	ISPOPMAP(0xff, 0xff),	/* 0x06: MBOX_MAILBOX_REG_TEST */
	ISPOPMAP(0x03, 0x05),	/* 0x07: MBOX_VERIFY_CHECKSUM	*/
	ISPOPMAP(0x01, 0x4f),	/* 0x08: MBOX_ABOUT_FIRMWARE */
	ISPOPMAP(0xdf, 0x01),	/* 0x09: LOAD RAM */
	ISPOPMAP(0xdf, 0x01),	/* 0x0a: DUMP RAM */
	ISPOPMAP(0x00, 0x00),	/* 0x0b: */
	ISPOPMAP(0x00, 0x00),	/* 0x0c: */
	ISPOPMAP(0x00, 0x00),	/* 0x0d: */
	ISPOPMAP(0x01, 0x05),	/* 0x0e: MBOX_CHECK_FIRMWARE */
	ISPOPMAP(0x03, 0x07),	/* 0x0f: MBOX_READ_RAM_WORD_EXTENDED(1) */
	ISPOPMAP(0x1f, 0x11),	/* 0x10: MBOX_INIT_REQ_QUEUE */
	ISPOPMAP(0x2f, 0x21),	/* 0x11: MBOX_INIT_RES_QUEUE */
	ISPOPMAP(0x0f, 0x01),	/* 0x12: MBOX_EXECUTE_IOCB */
	ISPOPMAP(0x03, 0x03),	/* 0x13: MBOX_WAKE_UP	*/
	ISPOPMAP(0x01, 0xff),	/* 0x14: MBOX_STOP_FIRMWARE */
	ISPOPMAP(0x4f, 0x01),	/* 0x15: MBOX_ABORT */
	ISPOPMAP(0x07, 0x01),	/* 0x16: MBOX_ABORT_DEVICE */
	ISPOPMAP(0x07, 0x01),	/* 0x17: MBOX_ABORT_TARGET */
	ISPOPMAP(0x03, 0x03),	/* 0x18: MBOX_BUS_RESET */
	ISPOPMAP(0x07, 0x05),	/* 0x19: MBOX_STOP_QUEUE */
	ISPOPMAP(0x07, 0x05),	/* 0x1a: MBOX_START_QUEUE */
	ISPOPMAP(0x07, 0x05),	/* 0x1b: MBOX_SINGLE_STEP_QUEUE */
	ISPOPMAP(0x07, 0x05),	/* 0x1c: MBOX_ABORT_QUEUE */
	ISPOPMAP(0x07, 0x03),	/* 0x1d: MBOX_GET_DEV_QUEUE_STATUS */
	ISPOPMAP(0x00, 0x00),	/* 0x1e: */
	ISPOPMAP(0x01, 0x07),	/* 0x1f: MBOX_GET_FIRMWARE_STATUS */
	ISPOPMAP(0x01, 0x4f),	/* 0x20: MBOX_GET_LOOP_ID */
	ISPOPMAP(0x00, 0x00),	/* 0x21: */
	ISPOPMAP(0x01, 0x07),	/* 0x22: MBOX_GET_RETRY_COUNT	*/
	ISPOPMAP(0x00, 0x00),	/* 0x23: */
	ISPOPMAP(0x00, 0x00),	/* 0x24: */
	ISPOPMAP(0x00, 0x00),	/* 0x25: */
	ISPOPMAP(0x00, 0x00),	/* 0x26: */
	ISPOPMAP(0x00, 0x00),	/* 0x27: */
	ISPOPMAP(0x01, 0x03),	/* 0x28: MBOX_GET_FIRMWARE_OPTIONS */
	ISPOPMAP(0x03, 0x07),	/* 0x29: MBOX_GET_PORT_QUEUE_PARAMS */
	ISPOPMAP(0x00, 0x00),	/* 0x2a: */
	ISPOPMAP(0x00, 0x00),	/* 0x2b: */
	ISPOPMAP(0x00, 0x00),	/* 0x2c: */
	ISPOPMAP(0x00, 0x00),	/* 0x2d: */
	ISPOPMAP(0x00, 0x00),	/* 0x2e: */
	ISPOPMAP(0x00, 0x00),	/* 0x2f: */
	ISPOPMAP(0x00, 0x00),	/* 0x30: */
	ISPOPMAP(0x00, 0x00),	/* 0x31: */
	ISPOPMAP(0x07, 0x07),	/* 0x32: MBOX_SET_RETRY_COUNT	*/
	ISPOPMAP(0x00, 0x00),	/* 0x33: */
	ISPOPMAP(0x00, 0x00),	/* 0x34: */
	ISPOPMAP(0x00, 0x00),	/* 0x35: */
	ISPOPMAP(0x00, 0x00),	/* 0x36: */
	ISPOPMAP(0x00, 0x00),	/* 0x37: */
	ISPOPMAP(0x0f, 0x01),	/* 0x38: MBOX_SET_FIRMWARE_OPTIONS */
	ISPOPMAP(0x0f, 0x07),	/* 0x39: MBOX_SET_PORT_QUEUE_PARAMS */
	ISPOPMAP(0x00, 0x00),	/* 0x3a: */
	ISPOPMAP(0x00, 0x00),	/* 0x3b: */
	ISPOPMAP(0x00, 0x00),	/* 0x3c: */
	ISPOPMAP(0x00, 0x00),	/* 0x3d: */
	ISPOPMAP(0x00, 0x00),	/* 0x3e: */
	ISPOPMAP(0x00, 0x00),	/* 0x3f: */
	ISPOPMAP(0x03, 0x01),	/* 0x40: MBOX_LOOP_PORT_BYPASS */
	ISPOPMAP(0x03, 0x01),	/* 0x41: MBOX_LOOP_PORT_ENABLE */
	ISPOPMAP(0x03, 0x07),	/* 0x42: MBOX_GET_RESOURCE_COUNTS */
	ISPOPMAP(0x01, 0x01),	/* 0x43: MBOX_REQUEST_NON_PARTICIPATING_MODE */
	ISPOPMAP(0x00, 0x00),	/* 0x44: */
	ISPOPMAP(0x00, 0x00),	/* 0x45: */
	ISPOPMAP(0x00, 0x00),	/* 0x46: */
	ISPOPMAP(0xcf, 0x03),	/* 0x47: GET PORT_DATABASE ENHANCED */
	ISPOPMAP(0x00, 0x00),	/* 0x48: */
	ISPOPMAP(0x00, 0x00),	/* 0x49: */
	ISPOPMAP(0x00, 0x00),	/* 0x4a: */
	ISPOPMAP(0x00, 0x00),	/* 0x4b: */
	ISPOPMAP(0x00, 0x00),	/* 0x4c: */
	ISPOPMAP(0x00, 0x00),	/* 0x4d: */
	ISPOPMAP(0x00, 0x00),	/* 0x4e: */
	ISPOPMAP(0x00, 0x00),	/* 0x4f: */
	ISPOPMAP(0x00, 0x00),	/* 0x50: */
	ISPOPMAP(0x00, 0x00),	/* 0x51: */
	ISPOPMAP(0x00, 0x00),	/* 0x52: */
	ISPOPMAP(0x00, 0x00),	/* 0x53: */
	ISPOPMAP(0xcf, 0x01),	/* 0x54: EXECUTE IOCB A64 */
	ISPOPMAP(0x00, 0x00),	/* 0x55: */
	ISPOPMAP(0x00, 0x00),	/* 0x56: */
	ISPOPMAP(0x00, 0x00),	/* 0x57: */
	ISPOPMAP(0x00, 0x00),	/* 0x58: */
	ISPOPMAP(0x00, 0x00),	/* 0x59: */
	ISPOPMAP(0x00, 0x00),	/* 0x5a: */
	ISPOPMAP(0x03, 0x01),	/* 0x5b: MBOX_DRIVER_HEARTBEAT */
	ISPOPMAP(0xcf, 0x01),	/* 0x5c: MBOX_FW_HEARTBEAT */
	ISPOPMAP(0x07, 0x03),	/* 0x5d: MBOX_GET_SET_DATA_RATE */
	ISPOPMAP(0x00, 0x00),	/* 0x5e: */
	ISPOPMAP(0x00, 0x00),	/* 0x5f: */
	ISPOPMAP(0xfd, 0x31),	/* 0x60: MBOX_INIT_FIRMWARE */
	ISPOPMAP(0x00, 0x00),	/* 0x61: */
	ISPOPMAP(0x01, 0x01),	/* 0x62: MBOX_INIT_LIP */
	ISPOPMAP(0xcd, 0x03),	/* 0x63: MBOX_GET_FC_AL_POSITION_MAP */
	ISPOPMAP(0xcf, 0x01),	/* 0x64: MBOX_GET_PORT_DB */
	ISPOPMAP(0x07, 0x01),	/* 0x65: MBOX_CLEAR_ACA */
	ISPOPMAP(0x07, 0x01),	/* 0x66: MBOX_TARGET_RESET */
	ISPOPMAP(0x07, 0x01),	/* 0x67: MBOX_CLEAR_TASK_SET */
	ISPOPMAP(0x07, 0x01),	/* 0x68: MBOX_ABORT_TASK_SET */
	ISPOPMAP(0x01, 0x07),	/* 0x69: MBOX_GET_FW_STATE */
	ISPOPMAP(0x03, 0xcf),	/* 0x6a: MBOX_GET_PORT_NAME */
	ISPOPMAP(0xcf, 0x01),	/* 0x6b: MBOX_GET_LINK_STATUS */
	ISPOPMAP(0x0f, 0x01),	/* 0x6c: MBOX_INIT_LIP_RESET */
	ISPOPMAP(0x00, 0x00),	/* 0x6d: */
	ISPOPMAP(0xcf, 0x03),	/* 0x6e: MBOX_SEND_SNS */
	ISPOPMAP(0x0f, 0x07),	/* 0x6f: MBOX_FABRIC_LOGIN */
	ISPOPMAP(0x03, 0x01),	/* 0x70: MBOX_SEND_CHANGE_REQUEST */
	ISPOPMAP(0x03, 0x03),	/* 0x71: MBOX_FABRIC_LOGOUT */
	ISPOPMAP(0x0f, 0x0f),	/* 0x72: MBOX_INIT_LIP_LOGIN */
	ISPOPMAP(0x00, 0x00),	/* 0x73: */
	ISPOPMAP(0x07, 0x01),	/* 0x74: LOGIN LOOP PORT */
	ISPOPMAP(0xcf, 0x03),	/* 0x75: GET PORT/NODE NAME LIST */
	ISPOPMAP(0x4f, 0x01),	/* 0x76: SET VENDOR ID */
	ISPOPMAP(0xcd, 0x01),	/* 0x77: INITIALIZE IP MAILBOX */
	ISPOPMAP(0x00, 0x00),	/* 0x78: */
	ISPOPMAP(0x00, 0x00),	/* 0x79: */
	ISPOPMAP(0x00, 0x00),	/* 0x7a: */
	ISPOPMAP(0x00, 0x00),	/* 0x7b: */
	ISPOPMAP(0x4f, 0x03),	/* 0x7c: Get ID List */
	ISPOPMAP(0xcf, 0x01),	/* 0x7d: SEND LFA */
	ISPOPMAP(0x07, 0x01)	/* 0x7e: Lun RESET */
};
/*
 * Footnotes
 *
 * (1): this sets bits 21..16 in mailbox register #8, which we nominally 
 *	do not access at this time in the core driver. The caller is
 *	responsible for setting this register first (Gross!).
 */

#ifndef	ISP_STRIPPED
static char *fc_mbcmd_names[] = {
	"NO-OP",
	"LOAD RAM",
	"EXEC FIRMWARE",
	"DUMP RAM",
	"WRITE RAM WORD",
	"READ RAM WORD",
	"MAILBOX REG TEST",
	"VERIFY CHECKSUM",
	"ABOUT FIRMWARE",
	"LOAD RAM",
	"DUMP RAM",
	NULL,
	NULL,
	"READ RAM WORD EXTENDED",
	"CHECK FIRMWARE",
	NULL,
	"INIT REQUEST QUEUE",
	"INIT RESULT QUEUE",
	"EXECUTE IOCB",
	"WAKE UP",
	"STOP FIRMWARE",
	"ABORT",
	"ABORT DEVICE",
	"ABORT TARGET",
	"BUS RESET",
	"STOP QUEUE",
	"START QUEUE",
	"SINGLE STEP QUEUE",
	"ABORT QUEUE",
	"GET DEV QUEUE STATUS",
	NULL,
	"GET FIRMWARE STATUS",
	"GET LOOP ID",
	NULL,
	"GET RETRY COUNT",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"GET FIRMWARE OPTIONS",
	"GET PORT QUEUE PARAMS",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"SET RETRY COUNT",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"SET FIRMWARE OPTIONS",
	"SET PORT QUEUE PARAMS",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"LOOP PORT BYPASS",
	"LOOP PORT ENABLE",
	"GET RESOURCE COUNTS",
	"REQUEST NON PARTICIPATING MODE",
	NULL,
	NULL,
	NULL,
	"GET PORT DATABASE,, ENHANCED",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"EXECUTE IOCB A64",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"DRIVER HEARTBEAT",
	NULL,
	"GET/SET DATA RATE",
	NULL,
	NULL,
	"INIT FIRMWARE",
	NULL,
	"INIT LIP",
	"GET FC-AL POSITION MAP",
	"GET PORT DATABASE",
	"CLEAR ACA",
	"TARGET RESET",
	"CLEAR TASK SET",
	"ABORT TASK SET",
	"GET FW STATE",
	"GET PORT NAME",
	"GET LINK STATUS",
	"INIT LIP RESET",
	NULL,
	"SEND SNS",
	"FABRIC LOGIN",
	"SEND CHANGE REQUEST",
	"FABRIC LOGOUT",
	"INIT LIP LOGIN",
	NULL,
	"LOGIN LOOP PORT",
	"GET PORT/NODE NAME LIST",
	"SET VENDOR ID",
	"INITIALIZE IP MAILBOX",
	NULL,
	NULL,
	NULL,
	NULL,
	"Get ID List",
	"SEND LFA",
	"Lun RESET"
};
#endif

static void
isp_mboxcmd_qnw(struct ispsoftc *isp, mbreg_t *mbp, int nodelay)
{
	unsigned int lim, ibits, obits, box, opcode;
	u_int16_t *mcp;

	if (IS_FC(isp)) {
		mcp = mbpfc;
		lim = (sizeof (mbpfc) / sizeof (mbpfc[0]));
	} else {
		mcp = mbpscsi;
		lim = (sizeof (mbpscsi) / sizeof (mbpscsi[0]));
	}
	opcode = mbp->param[0];
	ibits = HIBYT(mcp[opcode]) & NMBOX_BMASK(isp);
	obits = LOBYT(mcp[opcode]) & NMBOX_BMASK(isp);
	for (box = 0; box < MAX_MAILBOX; box++) {
		if (ibits & (1 << box)) {
			ISP_WRITE(isp, MBOX_OFF(box), mbp->param[box]);
		}
		if (nodelay == 0) {
			isp->isp_mboxtmp[box] = mbp->param[box] = 0;
		}
	}
	if (nodelay == 0) {
		isp->isp_lastmbxcmd = opcode;
		isp->isp_obits = obits;
		isp->isp_mboxbsy = 1;
	}
	ISP_WRITE(isp, HCCR, HCCR_CMD_SET_HOST_INT);
	/*
	 * Oddly enough, if we're not delaying for an answer,
	 * delay a bit to give the f/w a chance to pick up the
	 * command.
	 */
	if (nodelay) {
		USEC_DELAY(1000);
	}
}

static void
isp_mboxcmd(struct ispsoftc *isp, mbreg_t *mbp, int logmask)
{
	char *cname, *xname, tname[16], mname[16];
	unsigned int lim, ibits, obits, box, opcode;
	u_int16_t *mcp;

	if (IS_FC(isp)) {
		mcp = mbpfc;
		lim = (sizeof (mbpfc) / sizeof (mbpfc[0]));
	} else {
		mcp = mbpscsi;
		lim = (sizeof (mbpscsi) / sizeof (mbpscsi[0]));
	}

	if ((opcode = mbp->param[0]) >= lim) {
		mbp->param[0] = MBOX_INVALID_COMMAND;
		isp_prt(isp, ISP_LOGERR, "Unknown Command 0x%x", opcode);
		return;
	}

	ibits = HIBYT(mcp[opcode]) & NMBOX_BMASK(isp);
	obits = LOBYT(mcp[opcode]) & NMBOX_BMASK(isp);

	if (ibits == 0 && obits == 0) {
		mbp->param[0] = MBOX_COMMAND_PARAM_ERROR;
		isp_prt(isp, ISP_LOGERR, "no parameters for 0x%x", opcode);
		return;
	}

	/*
	 * Get exclusive usage of mailbox registers.
	 */
	MBOX_ACQUIRE(isp);

	for (box = 0; box < MAX_MAILBOX; box++) {
		if (ibits & (1 << box)) {
			ISP_WRITE(isp, MBOX_OFF(box), mbp->param[box]);
		}
		isp->isp_mboxtmp[box] = mbp->param[box] = 0;
	}

	isp->isp_lastmbxcmd = opcode;

	/*
	 * We assume that we can't overwrite a previous command.
	 */
	isp->isp_obits = obits;
	isp->isp_mboxbsy = 1;

	/*
	 * Set Host Interrupt condition so that RISC will pick up mailbox regs.
	 */
	ISP_WRITE(isp, HCCR, HCCR_CMD_SET_HOST_INT);

	/*
	 * While we haven't finished the command, spin our wheels here.
	 */
	MBOX_WAIT_COMPLETE(isp);

	if (isp->isp_mboxbsy) {
		/*
		 * Command timed out.
		 */
		isp->isp_mboxbsy = 0;
		MBOX_RELEASE(isp);
		return;
	}

	/*
	 * Copy back output registers.
	 */
	for (box = 0; box < MAX_MAILBOX; box++) {
		if (obits & (1 << box)) {
			mbp->param[box] = isp->isp_mboxtmp[box];
		}
	}

	MBOX_RELEASE(isp);

	if (logmask == 0 || opcode == MBOX_EXEC_FIRMWARE) {
		return;
	}
#ifdef	ISP_STRIPPED
	cname = NULL;
#else
	cname = (IS_FC(isp))? fc_mbcmd_names[opcode] : scsi_mbcmd_names[opcode];
#endif
	if (cname == NULL) {
		cname = tname;
		SNPRINTF(tname, sizeof tname, "opcode %x", opcode);
	}

	/*
	 * Just to be chatty here...
	 */
	xname = NULL;
	switch (mbp->param[0]) {
	case MBOX_COMMAND_COMPLETE:
		break;
	case MBOX_INVALID_COMMAND:
		if (logmask & MBLOGMASK(MBOX_COMMAND_COMPLETE))
			xname = "INVALID COMMAND";
		break;
	case MBOX_HOST_INTERFACE_ERROR:
		if (logmask & MBLOGMASK(MBOX_HOST_INTERFACE_ERROR))
			xname = "HOST INTERFACE ERROR";
		break;
	case MBOX_TEST_FAILED:
		if (logmask & MBLOGMASK(MBOX_TEST_FAILED))
			xname = "TEST FAILED";
		break;
	case MBOX_COMMAND_ERROR:
		if (logmask & MBLOGMASK(MBOX_COMMAND_ERROR))
			xname = "COMMAND ERROR";
		break;
	case MBOX_COMMAND_PARAM_ERROR:
		if (logmask & MBLOGMASK(MBOX_COMMAND_PARAM_ERROR))
			xname = "COMMAND PARAMETER ERROR";
		break;
	case MBOX_LOOP_ID_USED:
		if (logmask & MBLOGMASK(MBOX_LOOP_ID_USED))
			xname = "LOOP ID ALREADY IN USE";
		break;
	case MBOX_PORT_ID_USED:
		if (logmask & MBLOGMASK(MBOX_PORT_ID_USED))
			xname = "PORT ID ALREADY IN USE";
		break;
	case MBOX_ALL_IDS_USED:
		if (logmask & MBLOGMASK(MBOX_ALL_IDS_USED))
			xname = "ALL LOOP IDS IN USE";
		break;
	case 0:		/* special case */
		xname = "TIMEOUT";
		break;
	default:
		SNPRINTF(mname, sizeof mname, "error 0x%x", mbp->param[0]);
		xname = mname;
		break;
	}
	if (xname)
		isp_prt(isp, ISP_LOGALL, "Mailbox Command '%s' failed (%s)",
		    cname, xname);
}

static void
isp_fw_state(struct ispsoftc *isp)
{
	if (IS_FC(isp)) {
		mbreg_t mbs;
		fcparam *fcp = isp->isp_param;

		mbs.param[0] = MBOX_GET_FW_STATE;
		isp_mboxcmd(isp, &mbs, MBLOGALL);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			fcp->isp_fwstate = mbs.param[1];
		}
	}
}

static void
isp_update(struct ispsoftc *isp)
{
	int bus, upmask;

	for (bus = 0, upmask = isp->isp_update; upmask != 0; bus++) {
		if (upmask & (1 << bus)) {
			isp_update_bus(isp, bus);
		}
		upmask &= ~(1 << bus);
	}
}

static void
isp_update_bus(struct ispsoftc *isp, int bus)
{
	int tgt;
	mbreg_t mbs;
	sdparam *sdp;

	isp->isp_update &= ~(1 << bus);
	if (IS_FC(isp)) {
		/*
		 * There are no 'per-bus' settings for Fibre Channel.
		 */
		return;
	}
	sdp = isp->isp_param;
	sdp += bus;

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		u_int16_t flags, period, offset;
		int get;

		if (sdp->isp_devparam[tgt].dev_enable == 0) {
			sdp->isp_devparam[tgt].dev_update = 0;
			sdp->isp_devparam[tgt].dev_refresh = 0;
			isp_prt(isp, ISP_LOGDEBUG0,
	 		    "skipping target %d bus %d update", tgt, bus);
			continue;
		}
		/*
		 * If the goal is to update the status of the device,
		 * take what's in goal_flags and try and set the device
		 * toward that. Otherwise, if we're just refreshing the
		 * current device state, get the current parameters.
		 */

		/*
		 * Refresh overrides set
		 */
		if (sdp->isp_devparam[tgt].dev_refresh) {
			mbs.param[0] = MBOX_GET_TARGET_PARAMS;
			sdp->isp_devparam[tgt].dev_refresh = 0;
			get = 1;
		} else if (sdp->isp_devparam[tgt].dev_update) {
			mbs.param[0] = MBOX_SET_TARGET_PARAMS;
			/*
			 * Make sure goal_flags has "Renegotiate on Error"
			 * on and "Freeze Queue on Error" off.
			 */
			sdp->isp_devparam[tgt].goal_flags |= DPARM_RENEG;
			sdp->isp_devparam[tgt].goal_flags &= ~DPARM_QFRZ;

			mbs.param[2] = sdp->isp_devparam[tgt].goal_flags;

			/*
			 * Insist that PARITY must be enabled
			 * if SYNC or WIDE is enabled.
			 */
			if ((mbs.param[2] & (DPARM_SYNC|DPARM_WIDE)) != 0) {
				mbs.param[2] |= DPARM_PARITY;
			}

			if ((mbs.param[2] & DPARM_SYNC) == 0) {
				mbs.param[3] = 0;
			} else {
				mbs.param[3] =
				    (sdp->isp_devparam[tgt].goal_offset << 8) |
				    (sdp->isp_devparam[tgt].goal_period);
			}
			/*
			 * A command completion later that has
			 * RQSTF_NEGOTIATION set can cause
			 * the dev_refresh/announce cycle also.
			 *
			 * Note: It is really important to update our current
			 * flags with at least the state of TAG capabilities-
			 * otherwise we might try and send a tagged command
			 * when we have it all turned off. So change it here
			 * to say that current already matches goal.
			 */
			sdp->isp_devparam[tgt].actv_flags &= ~DPARM_TQING;
			sdp->isp_devparam[tgt].actv_flags |=
			    (sdp->isp_devparam[tgt].goal_flags & DPARM_TQING);
			isp_prt(isp, ISP_LOGDEBUG0,
			    "bus %d set tgt %d flags 0x%x off 0x%x period 0x%x",
			    bus, tgt, mbs.param[2], mbs.param[3] >> 8,
			    mbs.param[3] & 0xff);
			sdp->isp_devparam[tgt].dev_update = 0;
			sdp->isp_devparam[tgt].dev_refresh = 1;
			get = 0;
		} else {
			continue;
		}
		mbs.param[1] = (bus << 15) | (tgt << 8);
		isp_mboxcmd(isp, &mbs, MBLOGALL);
		if (get == 0) {
			isp->isp_sendmarker |= (1 << bus);
			continue;
		}
		flags = mbs.param[2];
		period = mbs.param[3] & 0xff;
		offset = mbs.param[3] >> 8;
		sdp->isp_devparam[tgt].actv_flags = flags;
		sdp->isp_devparam[tgt].actv_period = period;
		sdp->isp_devparam[tgt].actv_offset = offset;
		get = (bus << 16) | tgt;
		(void) isp_async(isp, ISPASYNC_NEW_TGT_PARAMS, &get);
	}

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		if (sdp->isp_devparam[tgt].dev_update ||
		    sdp->isp_devparam[tgt].dev_refresh) {
			isp->isp_update |= (1 << bus);
			break;
		}
	}
}

#ifndef	DEFAULT_FRAMESIZE
#define	DEFAULT_FRAMESIZE(isp)		ICB_DFLT_FRMLEN
#endif
#ifndef	DEFAULT_EXEC_THROTTLE
#define	DEFAULT_EXEC_THROTTLE(isp)	ISP_EXEC_THROTTLE
#endif

static void
isp_setdfltparm(struct ispsoftc *isp, int channel)
{
	int tgt;
	mbreg_t mbs;
	sdparam *sdp;

	if (IS_FC(isp)) {
		fcparam *fcp = (fcparam *) isp->isp_param;
		int nvfail;

		fcp += channel;
		if (fcp->isp_gotdparms) {
			return;
		}
		fcp->isp_gotdparms = 1;
		fcp->isp_maxfrmlen = DEFAULT_FRAMESIZE(isp);
		fcp->isp_maxalloc = ICB_DFLT_ALLOC;
		fcp->isp_execthrottle = DEFAULT_EXEC_THROTTLE(isp);
		fcp->isp_retry_delay = ICB_DFLT_RDELAY;
		fcp->isp_retry_count = ICB_DFLT_RCOUNT;
		/* Platform specific.... */
		fcp->isp_loopid = DEFAULT_LOOPID(isp);
		fcp->isp_nodewwn = DEFAULT_NODEWWN(isp);
		fcp->isp_portwwn = DEFAULT_PORTWWN(isp);
		fcp->isp_fwoptions = 0;
		fcp->isp_fwoptions |= ICBOPT_FAIRNESS;
		fcp->isp_fwoptions |= ICBOPT_PDBCHANGE_AE;
		fcp->isp_fwoptions |= ICBOPT_HARD_ADDRESS;
#ifndef	ISP_NO_FASTPOST_FC
		fcp->isp_fwoptions |= ICBOPT_FAST_POST;
#endif
		if (isp->isp_confopts & ISP_CFG_FULL_DUPLEX)
			fcp->isp_fwoptions |= ICBOPT_FULL_DUPLEX;

		/*
		 * Make sure this is turned off now until we get
		 * extended options from NVRAM
		 */
		fcp->isp_fwoptions &= ~ICBOPT_EXTENDED;

		/*
		 * Now try and read NVRAM unless told to not do so.
		 * This will set fcparam's isp_nodewwn && isp_portwwn.
		 */
		if ((isp->isp_confopts & ISP_CFG_NONVRAM) == 0) {
		    	nvfail = isp_read_nvram(isp);
			if (nvfail)
				isp->isp_confopts |= ISP_CFG_NONVRAM;
		} else {
			nvfail = 1;
		}
		/*
		 * Set node && port to override platform set defaults
		 * unless the nvram read failed (or none was done),
		 * or the platform code wants to use what had been
		 * set in the defaults.
		 */
		if (nvfail) {
			isp->isp_confopts |= ISP_CFG_OWNWWPN|ISP_CFG_OWNWWNN;
		}
		if (isp->isp_confopts & ISP_CFG_OWNWWNN) {
			isp_prt(isp, ISP_LOGCONFIG, "Using Node WWN 0x%08x%08x",
			    (u_int32_t) (DEFAULT_NODEWWN(isp) >> 32),
			    (u_int32_t) (DEFAULT_NODEWWN(isp) & 0xffffffff));
			ISP_NODEWWN(isp) = DEFAULT_NODEWWN(isp);
		} else {
			/*
			 * We always start out with values derived
			 * from NVRAM or our platform default.
			 */
			ISP_NODEWWN(isp) = fcp->isp_nodewwn;
		}
		if (isp->isp_confopts & ISP_CFG_OWNWWPN) {
			isp_prt(isp, ISP_LOGCONFIG, "Using Port WWN 0x%08x%08x",
			    (u_int32_t) (DEFAULT_PORTWWN(isp) >> 32),
			    (u_int32_t) (DEFAULT_PORTWWN(isp) & 0xffffffff));
			ISP_PORTWWN(isp) = DEFAULT_PORTWWN(isp);
		} else {
			/*
			 * We always start out with values derived
			 * from NVRAM or our platform default.
			 */
			ISP_PORTWWN(isp) = fcp->isp_portwwn;
		}
		return;
	}

	sdp = (sdparam *) isp->isp_param;
	sdp += channel;

	/*
	 * Been there, done that, got the T-shirt...
	 */
	if (sdp->isp_gotdparms) {
		return;
	}
	sdp->isp_gotdparms = 1;

	/*
	 * Establish some default parameters.
	 */
	sdp->isp_cmd_dma_burst_enable = 0;
	sdp->isp_data_dma_burst_enabl = 1;
	sdp->isp_fifo_threshold = 0;
	sdp->isp_initiator_id = DEFAULT_IID(isp);
	if (isp->isp_type >= ISP_HA_SCSI_1040) {
		sdp->isp_async_data_setup = 9;
	} else {
		sdp->isp_async_data_setup = 6;
	}
	sdp->isp_selection_timeout = 250;
	sdp->isp_max_queue_depth = MAXISPREQUEST(isp);
	sdp->isp_tag_aging = 8;
	sdp->isp_bus_reset_delay = 5;
	/*
	 * Don't retry selection, busy or queue full automatically- reflect
	 * these back to us.
	 */
	sdp->isp_retry_count = 0;
	sdp->isp_retry_delay = 0;

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		sdp->isp_devparam[tgt].exc_throttle = ISP_EXEC_THROTTLE;
		sdp->isp_devparam[tgt].dev_enable = 1;
	}

	/*
	 * If we've not been told to avoid reading NVRAM, try and read it.
	 * If we're successful reading it, we can then return because NVRAM
	 * will tell us what the desired settings are. Otherwise, we establish
	 * some reasonable 'fake' nvram and goal defaults.
	 */

	if ((isp->isp_confopts & ISP_CFG_NONVRAM) == 0) {
		if (isp_read_nvram(isp) == 0) {
			return;
		}
	}

	/*
	 * Now try and see whether we have specific values for them.
	 */
	if ((isp->isp_confopts & ISP_CFG_NONVRAM) == 0) {
		mbs.param[0] = MBOX_GET_ACT_NEG_STATE;
		isp_mboxcmd(isp, &mbs, MBLOGNONE);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			sdp->isp_req_ack_active_neg = 1;
			sdp->isp_data_line_active_neg = 1;
		} else {
			sdp->isp_req_ack_active_neg =
			    (mbs.param[1+channel] >> 4) & 0x1;
			sdp->isp_data_line_active_neg =
			    (mbs.param[1+channel] >> 5) & 0x1;
		}
	}

	isp_prt(isp, ISP_LOGDEBUG0, sc0, sc3,
	    0, sdp->isp_fifo_threshold, sdp->isp_initiator_id,
	    sdp->isp_bus_reset_delay, sdp->isp_retry_count,
	    sdp->isp_retry_delay, sdp->isp_async_data_setup);
	isp_prt(isp, ISP_LOGDEBUG0, sc1, sc3,
	    sdp->isp_req_ack_active_neg, sdp->isp_data_line_active_neg,
	    sdp->isp_data_dma_burst_enabl, sdp->isp_cmd_dma_burst_enable,
	    sdp->isp_selection_timeout, sdp->isp_max_queue_depth);

	/*
	 * The trick here is to establish a default for the default (honk!)
	 * state (goal_flags). Then try and get the current status from
	 * the card to fill in the current state. We don't, in fact, set
	 * the default to the SAFE default state- that's not the goal state.
	 */
	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		u_int8_t off, per;
		sdp->isp_devparam[tgt].actv_offset = 0;
		sdp->isp_devparam[tgt].actv_period = 0;
		sdp->isp_devparam[tgt].actv_flags = 0;

		sdp->isp_devparam[tgt].goal_flags =
		    sdp->isp_devparam[tgt].nvrm_flags = DPARM_DEFAULT;

		/*
		 * We default to Wide/Fast for versions less than a 1040
		 * (unless it's SBus).
		 */
		if (IS_ULTRA3(isp)) {
			off = ISP_80M_SYNCPARMS >> 8;
			per = ISP_80M_SYNCPARMS & 0xff;
		} else if (IS_ULTRA2(isp)) {
			off = ISP_40M_SYNCPARMS >> 8;
			per = ISP_40M_SYNCPARMS & 0xff;
		} else if (IS_1240(isp)) {
			off = ISP_20M_SYNCPARMS >> 8;
			per = ISP_20M_SYNCPARMS & 0xff;
		} else if ((isp->isp_bustype == ISP_BT_SBUS &&
		    isp->isp_type < ISP_HA_SCSI_1020A) ||
		    (isp->isp_bustype == ISP_BT_PCI &&
		    isp->isp_type < ISP_HA_SCSI_1040) ||
		    (isp->isp_clock && isp->isp_clock < 60) ||
		    (sdp->isp_ultramode == 0)) {
			off = ISP_10M_SYNCPARMS >> 8;
			per = ISP_10M_SYNCPARMS & 0xff;
		} else {
			off = ISP_20M_SYNCPARMS_1040 >> 8;
			per = ISP_20M_SYNCPARMS_1040 & 0xff;
		}
		sdp->isp_devparam[tgt].goal_offset =
		    sdp->isp_devparam[tgt].nvrm_offset = off;
		sdp->isp_devparam[tgt].goal_period =
		    sdp->isp_devparam[tgt].nvrm_period = per;

		isp_prt(isp, ISP_LOGDEBUG0, sc2, sc3,
		    channel, tgt, sdp->isp_devparam[tgt].nvrm_flags,
		    sdp->isp_devparam[tgt].nvrm_offset,
		    sdp->isp_devparam[tgt].nvrm_period);
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
isp_reinit(struct ispsoftc *isp)
{
	XS_T *xs;
	u_int16_t handle;

	isp_reset(isp);
	if (isp->isp_state != ISP_RESETSTATE) {
		isp_prt(isp, ISP_LOGERR, "isp_reinit cannot reset card");
	} else if (isp->isp_role != ISP_ROLE_NONE) {
		isp_init(isp);
		if (isp->isp_state == ISP_INITSTATE) {
			isp->isp_state = ISP_RUNSTATE;
		}
		if (isp->isp_state != ISP_RUNSTATE) {
			isp_prt(isp, ISP_LOGERR,
			    "isp_reinit cannot restart card");
		}
	}
	isp->isp_nactive = 0;

	for (handle = 1; (int) handle <= isp->isp_maxcmds; handle++) {
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
		isp_done(xs);
	}
}

/*
 * NVRAM Routines
 */
static int
isp_read_nvram(struct ispsoftc *isp)
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
	} else if (IS_ULTRA2(isp)) {
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
			isp_prt(isp, ISP_LOGWARN, "invalid NVRAM header");
			isp_prt(isp, ISP_LOGDEBUG0, "%x %x %x",
			    nvram_data[0], nvram_data[1], nvram_data[2]);
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
		isp_prt(isp, ISP_LOGWARN, "invalid NVRAM checksum");
		return (-1);
	}
	if (ISP_NVRAM_VERSION(nvram_data) < minversion) {
		isp_prt(isp, ISP_LOGWARN, "version %d NVRAM not understood",
		    ISP_NVRAM_VERSION(nvram_data));
		return (-1);
	}

	if (IS_ULTRA3(isp)) {
		isp_parse_nvram_12160(isp, 0, nvram_data);
		if (IS_12160(isp))
			isp_parse_nvram_12160(isp, 1, nvram_data);
	} else if (IS_1080(isp)) {
		isp_parse_nvram_1080(isp, 0, nvram_data);
	} else if (IS_1280(isp) || IS_1240(isp)) {
		isp_parse_nvram_1080(isp, 0, nvram_data);
		isp_parse_nvram_1080(isp, 1, nvram_data);
	} else if (IS_SCSI(isp)) {
		isp_parse_nvram_1020(isp, nvram_data);
	} else {
		isp_parse_nvram_2100(isp, nvram_data);
	}
	return (0);
#undef	nvram_data
#undef	nvram_words
}

static void
isp_rdnvram_word(struct ispsoftc *isp, int wo, u_int16_t *rp)
{
	int i, cbits;
	u_int16_t bit, rqst;

	ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT);
	USEC_DELAY(2);
	ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT|BIU_NVRAM_CLOCK);
	USEC_DELAY(2);

	if (IS_FC(isp)) {
		wo &= ((ISP2100_NVRAM_SIZE >> 1) - 1);
		if (IS_2312(isp) && isp->isp_port) {
			wo += 128;
		}
		rqst = (ISP_NVRAM_READ << 8) | wo;
		cbits = 10;
	} else if (IS_ULTRA2(isp)) {
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
		USEC_DELAY(2);
		ISP_WRITE(isp, BIU_NVRAM, bit | BIU_NVRAM_CLOCK);
		USEC_DELAY(2);
		ISP_WRITE(isp, BIU_NVRAM, bit);
		USEC_DELAY(2);
	}
	/*
	 * Now read the result back in (bits come back in MSB format).
	 */
	*rp = 0;
	for (i = 0; i < 16; i++) {
		u_int16_t rv;
		*rp <<= 1;
		ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT|BIU_NVRAM_CLOCK);
		USEC_DELAY(2);
		rv = ISP_READ(isp, BIU_NVRAM);
		if (rv & BIU_NVRAM_DATAIN) {
			*rp |= 1;
		}
		USEC_DELAY(2);
		ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT);
		USEC_DELAY(2);
	}
	ISP_WRITE(isp, BIU_NVRAM, 0);
	USEC_DELAY(2);
	ISP_SWIZZLE_NVRAM_WORD(isp, rp);
}

static void
isp_parse_nvram_1020(struct ispsoftc *isp, u_int8_t *nvram_data)
{
	sdparam *sdp = (sdparam *) isp->isp_param;
	int tgt;

	sdp->isp_fifo_threshold =
		ISP_NVRAM_FIFO_THRESHOLD(nvram_data) |
		(ISP_NVRAM_FIFO_THRESHOLD_128(nvram_data) << 2);

	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0)
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

	isp_prt(isp, ISP_LOGDEBUG0, sc0, sc4,
	    0, sdp->isp_fifo_threshold, sdp->isp_initiator_id,
	    sdp->isp_bus_reset_delay, sdp->isp_retry_count,
	    sdp->isp_retry_delay, sdp->isp_async_data_setup);
	isp_prt(isp, ISP_LOGDEBUG0, sc1, sc4,
	    sdp->isp_req_ack_active_neg, sdp->isp_data_line_active_neg,
	    sdp->isp_data_dma_burst_enabl, sdp->isp_cmd_dma_burst_enable,
	    sdp->isp_selection_timeout, sdp->isp_max_queue_depth);

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		sdp->isp_devparam[tgt].dev_enable =
			ISP_NVRAM_TGT_DEVICE_ENABLE(nvram_data, tgt);
		sdp->isp_devparam[tgt].exc_throttle =
			ISP_NVRAM_TGT_EXEC_THROTTLE(nvram_data, tgt);
		sdp->isp_devparam[tgt].nvrm_offset =
			ISP_NVRAM_TGT_SYNC_OFFSET(nvram_data, tgt);
		sdp->isp_devparam[tgt].nvrm_period =
			ISP_NVRAM_TGT_SYNC_PERIOD(nvram_data, tgt);
		/*
		 * We probably shouldn't lie about this, but it
		 * it makes it much safer if we limit NVRAM values
		 * to sanity.
		 */
		if (isp->isp_type < ISP_HA_SCSI_1040) {
			/*
			 * If we're not ultra, we can't possibly
			 * be a shorter period than this.
			 */
			if (sdp->isp_devparam[tgt].nvrm_period < 0x19) {
				sdp->isp_devparam[tgt].nvrm_period = 0x19;
			}
			if (sdp->isp_devparam[tgt].nvrm_offset > 0xc) {
				sdp->isp_devparam[tgt].nvrm_offset = 0x0c;
			}
		} else {
			if (sdp->isp_devparam[tgt].nvrm_offset > 0x8) {
				sdp->isp_devparam[tgt].nvrm_offset = 0x8;
			}
		}
		sdp->isp_devparam[tgt].nvrm_flags = 0;
		if (ISP_NVRAM_TGT_RENEG(nvram_data, tgt))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_RENEG;
		sdp->isp_devparam[tgt].nvrm_flags |= DPARM_ARQ;
		if (ISP_NVRAM_TGT_TQING(nvram_data, tgt))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_TQING;
		if (ISP_NVRAM_TGT_SYNC(nvram_data, tgt))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_SYNC;
		if (ISP_NVRAM_TGT_WIDE(nvram_data, tgt))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_WIDE;
		if (ISP_NVRAM_TGT_PARITY(nvram_data, tgt))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_PARITY;
		if (ISP_NVRAM_TGT_DISC(nvram_data, tgt))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_DISC;
		sdp->isp_devparam[tgt].actv_flags = 0; /* we don't know */
		isp_prt(isp, ISP_LOGDEBUG0, sc2, sc4,
		    0, tgt, sdp->isp_devparam[tgt].nvrm_flags,
		    sdp->isp_devparam[tgt].nvrm_offset,
		    sdp->isp_devparam[tgt].nvrm_period);
		sdp->isp_devparam[tgt].goal_offset =
		    sdp->isp_devparam[tgt].nvrm_offset;
		sdp->isp_devparam[tgt].goal_period =
		    sdp->isp_devparam[tgt].nvrm_period;
		sdp->isp_devparam[tgt].goal_flags =
		    sdp->isp_devparam[tgt].nvrm_flags;
	}
}

static void
isp_parse_nvram_1080(struct ispsoftc *isp, int bus, u_int8_t *nvram_data)
{
	sdparam *sdp = (sdparam *) isp->isp_param;
	int tgt;

	sdp += bus;

	sdp->isp_fifo_threshold =
	    ISP1080_NVRAM_FIFO_THRESHOLD(nvram_data);

	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0)
		sdp->isp_initiator_id =
		    ISP1080_NVRAM_INITIATOR_ID(nvram_data, bus);

	sdp->isp_bus_reset_delay =
	    ISP1080_NVRAM_BUS_RESET_DELAY(nvram_data, bus);

	sdp->isp_retry_count =
	    ISP1080_NVRAM_BUS_RETRY_COUNT(nvram_data, bus);

	sdp->isp_retry_delay =
	    ISP1080_NVRAM_BUS_RETRY_DELAY(nvram_data, bus);

	sdp->isp_async_data_setup =
	    ISP1080_NVRAM_ASYNC_DATA_SETUP_TIME(nvram_data, bus);

	sdp->isp_req_ack_active_neg =
	    ISP1080_NVRAM_REQ_ACK_ACTIVE_NEGATION(nvram_data, bus);

	sdp->isp_data_line_active_neg =
	    ISP1080_NVRAM_DATA_LINE_ACTIVE_NEGATION(nvram_data, bus);

	sdp->isp_data_dma_burst_enabl =
	    ISP1080_NVRAM_BURST_ENABLE(nvram_data);

	sdp->isp_cmd_dma_burst_enable =
	    ISP1080_NVRAM_BURST_ENABLE(nvram_data);

	sdp->isp_selection_timeout =
	    ISP1080_NVRAM_SELECTION_TIMEOUT(nvram_data, bus);

	sdp->isp_max_queue_depth =
	     ISP1080_NVRAM_MAX_QUEUE_DEPTH(nvram_data, bus);

	isp_prt(isp, ISP_LOGDEBUG0, sc0, sc4,
	    bus, sdp->isp_fifo_threshold, sdp->isp_initiator_id,
	    sdp->isp_bus_reset_delay, sdp->isp_retry_count,
	    sdp->isp_retry_delay, sdp->isp_async_data_setup);
	isp_prt(isp, ISP_LOGDEBUG0, sc1, sc4,
	    sdp->isp_req_ack_active_neg, sdp->isp_data_line_active_neg,
	    sdp->isp_data_dma_burst_enabl, sdp->isp_cmd_dma_burst_enable,
	    sdp->isp_selection_timeout, sdp->isp_max_queue_depth);


	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		sdp->isp_devparam[tgt].dev_enable =
		    ISP1080_NVRAM_TGT_DEVICE_ENABLE(nvram_data, tgt, bus);
		sdp->isp_devparam[tgt].exc_throttle =
			ISP1080_NVRAM_TGT_EXEC_THROTTLE(nvram_data, tgt, bus);
		sdp->isp_devparam[tgt].nvrm_offset =
			ISP1080_NVRAM_TGT_SYNC_OFFSET(nvram_data, tgt, bus);
		sdp->isp_devparam[tgt].nvrm_period =
			ISP1080_NVRAM_TGT_SYNC_PERIOD(nvram_data, tgt, bus);
		sdp->isp_devparam[tgt].nvrm_flags = 0;
		if (ISP1080_NVRAM_TGT_RENEG(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_RENEG;
		sdp->isp_devparam[tgt].nvrm_flags |= DPARM_ARQ;
		if (ISP1080_NVRAM_TGT_TQING(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_TQING;
		if (ISP1080_NVRAM_TGT_SYNC(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_SYNC;
		if (ISP1080_NVRAM_TGT_WIDE(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_WIDE;
		if (ISP1080_NVRAM_TGT_PARITY(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_PARITY;
		if (ISP1080_NVRAM_TGT_DISC(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_DISC;
		sdp->isp_devparam[tgt].actv_flags = 0;
		isp_prt(isp, ISP_LOGDEBUG0, sc2, sc4,
		    bus, tgt, sdp->isp_devparam[tgt].nvrm_flags,
		    sdp->isp_devparam[tgt].nvrm_offset,
		    sdp->isp_devparam[tgt].nvrm_period);
		sdp->isp_devparam[tgt].goal_offset =
		    sdp->isp_devparam[tgt].nvrm_offset;
		sdp->isp_devparam[tgt].goal_period =
		    sdp->isp_devparam[tgt].nvrm_period;
		sdp->isp_devparam[tgt].goal_flags =
		    sdp->isp_devparam[tgt].nvrm_flags;
	}
}

static void
isp_parse_nvram_12160(struct ispsoftc *isp, int bus, u_int8_t *nvram_data)
{
	sdparam *sdp = (sdparam *) isp->isp_param;
	int tgt;

	sdp += bus;

	sdp->isp_fifo_threshold =
	    ISP12160_NVRAM_FIFO_THRESHOLD(nvram_data);

	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0)
		sdp->isp_initiator_id =
		    ISP12160_NVRAM_INITIATOR_ID(nvram_data, bus);

	sdp->isp_bus_reset_delay =
	    ISP12160_NVRAM_BUS_RESET_DELAY(nvram_data, bus);

	sdp->isp_retry_count =
	    ISP12160_NVRAM_BUS_RETRY_COUNT(nvram_data, bus);

	sdp->isp_retry_delay =
	    ISP12160_NVRAM_BUS_RETRY_DELAY(nvram_data, bus);

	sdp->isp_async_data_setup =
	    ISP12160_NVRAM_ASYNC_DATA_SETUP_TIME(nvram_data, bus);

	sdp->isp_req_ack_active_neg =
	    ISP12160_NVRAM_REQ_ACK_ACTIVE_NEGATION(nvram_data, bus);

	sdp->isp_data_line_active_neg =
	    ISP12160_NVRAM_DATA_LINE_ACTIVE_NEGATION(nvram_data, bus);

	sdp->isp_data_dma_burst_enabl =
	    ISP12160_NVRAM_BURST_ENABLE(nvram_data);

	sdp->isp_cmd_dma_burst_enable =
	    ISP12160_NVRAM_BURST_ENABLE(nvram_data);

	sdp->isp_selection_timeout =
	    ISP12160_NVRAM_SELECTION_TIMEOUT(nvram_data, bus);

	sdp->isp_max_queue_depth =
	     ISP12160_NVRAM_MAX_QUEUE_DEPTH(nvram_data, bus);

	isp_prt(isp, ISP_LOGDEBUG0, sc0, sc4,
	    bus, sdp->isp_fifo_threshold, sdp->isp_initiator_id,
	    sdp->isp_bus_reset_delay, sdp->isp_retry_count,
	    sdp->isp_retry_delay, sdp->isp_async_data_setup);
	isp_prt(isp, ISP_LOGDEBUG0, sc1, sc4,
	    sdp->isp_req_ack_active_neg, sdp->isp_data_line_active_neg,
	    sdp->isp_data_dma_burst_enabl, sdp->isp_cmd_dma_burst_enable,
	    sdp->isp_selection_timeout, sdp->isp_max_queue_depth);

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		sdp->isp_devparam[tgt].dev_enable =
		    ISP12160_NVRAM_TGT_DEVICE_ENABLE(nvram_data, tgt, bus);
		sdp->isp_devparam[tgt].exc_throttle =
			ISP12160_NVRAM_TGT_EXEC_THROTTLE(nvram_data, tgt, bus);
		sdp->isp_devparam[tgt].nvrm_offset =
			ISP12160_NVRAM_TGT_SYNC_OFFSET(nvram_data, tgt, bus);
		sdp->isp_devparam[tgt].nvrm_period =
			ISP12160_NVRAM_TGT_SYNC_PERIOD(nvram_data, tgt, bus);
		sdp->isp_devparam[tgt].nvrm_flags = 0;
		if (ISP12160_NVRAM_TGT_RENEG(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_RENEG;
		sdp->isp_devparam[tgt].nvrm_flags |= DPARM_ARQ;
		if (ISP12160_NVRAM_TGT_TQING(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_TQING;
		if (ISP12160_NVRAM_TGT_SYNC(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_SYNC;
		if (ISP12160_NVRAM_TGT_WIDE(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_WIDE;
		if (ISP12160_NVRAM_TGT_PARITY(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_PARITY;
		if (ISP12160_NVRAM_TGT_DISC(nvram_data, tgt, bus))
			sdp->isp_devparam[tgt].nvrm_flags |= DPARM_DISC;
		sdp->isp_devparam[tgt].actv_flags = 0;
		isp_prt(isp, ISP_LOGDEBUG0, sc2, sc4,
		    bus, tgt, sdp->isp_devparam[tgt].nvrm_flags,
		    sdp->isp_devparam[tgt].nvrm_offset,
		    sdp->isp_devparam[tgt].nvrm_period);
		sdp->isp_devparam[tgt].goal_offset =
		    sdp->isp_devparam[tgt].nvrm_offset;
		sdp->isp_devparam[tgt].goal_period =
		    sdp->isp_devparam[tgt].nvrm_period;
		sdp->isp_devparam[tgt].goal_flags =
		    sdp->isp_devparam[tgt].nvrm_flags;
	}
}

static void
isp_parse_nvram_2100(struct ispsoftc *isp, u_int8_t *nvram_data)
{
	fcparam *fcp = (fcparam *) isp->isp_param;
	u_int64_t wwn;

	/*
	 * There is NVRAM storage for both Port and Node entities-
	 * but the Node entity appears to be unused on all the cards
	 * I can find. However, we should account for this being set
	 * at some point in the future.
	 *
	 * Qlogic WWNs have an NAA of 2, but usually nothing shows up in
	 * bits 48..60. In the case of the 2202, it appears that they do
	 * use bit 48 to distinguish between the two instances on the card.
	 * The 2204, which I've never seen, *probably* extends this method.
	 */
	wwn = ISP2100_NVRAM_PORT_NAME(nvram_data);
	if (wwn) {
		isp_prt(isp, ISP_LOGCONFIG, "NVRAM Port WWN 0x%08x%08x",
		    (u_int32_t) (wwn >> 32), (u_int32_t) (wwn & 0xffffffff));
		if ((wwn >> 60) == 0) {
			wwn |= (((u_int64_t) 2)<< 60);
		}
	}
	fcp->isp_portwwn = wwn;
	if (IS_2200(isp) || IS_23XX(isp)) {
		wwn = ISP2200_NVRAM_NODE_NAME(nvram_data);
		if (wwn) {
			isp_prt(isp, ISP_LOGCONFIG, "NVRAM Node WWN 0x%08x%08x",
			    (u_int32_t) (wwn >> 32),
			    (u_int32_t) (wwn & 0xffffffff));
			if ((wwn >> 60) == 0) {
				wwn |= (((u_int64_t) 2)<< 60);
			}
		}
	} else {
		wwn &= ~((u_int64_t) 0xfff << 48);
	}
	fcp->isp_nodewwn = wwn;

	/*
	 * Make sure we have both Node and Port as non-zero values.
	 */
	if (fcp->isp_nodewwn != 0 && fcp->isp_portwwn == 0) {
		fcp->isp_portwwn = fcp->isp_nodewwn;
	} else if (fcp->isp_nodewwn == 0 && fcp->isp_portwwn != 0) {
		fcp->isp_nodewwn = fcp->isp_portwwn;
	}

	/*
	 * Make the Node and Port values sane if they're NAA == 2.
	 * This means to clear bits 48..56 for the Node WWN and
	 * make sure that there's some non-zero value in 48..56
	 * for the Port WWN.
	 */
	if (fcp->isp_nodewwn && fcp->isp_portwwn) {
		if ((fcp->isp_nodewwn & (((u_int64_t) 0xfff) << 48)) != 0 &&
		    (fcp->isp_nodewwn >> 60) == 2) {
			fcp->isp_nodewwn &= ~((u_int64_t) 0xfff << 48);
		}
		if ((fcp->isp_portwwn & (((u_int64_t) 0xfff) << 48)) == 0 &&
		    (fcp->isp_portwwn >> 60) == 2) {
			fcp->isp_portwwn |= ((u_int64_t) 1 << 56);
		}
	}

	isp_prt(isp, ISP_LOGDEBUG0,
	    "NVRAM: maxfrmlen %d execthrottle %d fwoptions 0x%x loopid %x",
	    ISP2100_NVRAM_MAXFRAMELENGTH(nvram_data),
	    ISP2100_NVRAM_EXECUTION_THROTTLE(nvram_data),
	    ISP2100_NVRAM_OPTIONS(nvram_data),
	    ISP2100_NVRAM_HARDLOOPID(nvram_data));

	fcp->isp_maxalloc =
		ISP2100_NVRAM_MAXIOCBALLOCATION(nvram_data);
	if ((isp->isp_confopts & ISP_CFG_OWNFSZ) == 0)
		fcp->isp_maxfrmlen =
			ISP2100_NVRAM_MAXFRAMELENGTH(nvram_data);
	fcp->isp_retry_delay =
		ISP2100_NVRAM_RETRY_DELAY(nvram_data);
	fcp->isp_retry_count =
		ISP2100_NVRAM_RETRY_COUNT(nvram_data);
	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0)
		fcp->isp_loopid =
			ISP2100_NVRAM_HARDLOOPID(nvram_data);
	if ((isp->isp_confopts & ISP_CFG_OWNEXCTHROTTLE) == 0)
		fcp->isp_execthrottle =
			ISP2100_NVRAM_EXECUTION_THROTTLE(nvram_data);
	fcp->isp_fwoptions = ISP2100_NVRAM_OPTIONS(nvram_data);
}

#ifdef	ISP_FW_CRASH_DUMP
static void isp2200_fw_dump(struct ispsoftc *);
static void isp2300_fw_dump(struct ispsoftc *);

static void
isp2200_fw_dump(struct ispsoftc *isp)
{
	int i, j;
	mbreg_t mbs;
	u_int16_t *ptr;

	ptr = FCPARAM(isp)->isp_dump_data;
	if (ptr == NULL) {
		isp_prt(isp, ISP_LOGERR,
		   "No place to dump RISC registers and SRAM");
		return;
	}
	if (*ptr++) {
		isp_prt(isp, ISP_LOGERR,
		   "dump area for RISC registers and SRAM already used");
		return;
	}
	ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);
	for (i = 0; i < 100; i++) {
		USEC_DELAY(100);
		if (ISP_READ(isp, HCCR) & HCCR_PAUSE) {
			break;
		}
	}
	if (ISP_READ(isp, HCCR) & HCCR_PAUSE) {
		/*
		 * PBIU Registers
		 */
		for (i = 0; i < 8; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + (i << 1));
		}

		/*
		 * Mailbox Registers
		 */
		for (i = 0; i < 8; i++) {
			*ptr++ = ISP_READ(isp, MBOX_BLOCK + (i << 1));
		}

		/*
		 * DMA Registers
		 */
		for (i = 0; i < 48; i++) {
			*ptr++ = ISP_READ(isp, DMA_BLOCK + 0x20 + (i << 1));
		}

		/*
		 * RISC H/W Registers
		 */
		ISP_WRITE(isp, BIU2100_CSR, 0);
		for (i = 0; i < 16; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + 0xA0 + (i << 1));
		}

		/*
		 * RISC GP Registers
		 */
		for (j = 0; j < 8; j++) {
			ISP_WRITE(isp, BIU_BLOCK + 0xA4, 0x2000 + (j << 8));
			for (i = 0; i < 16; i++) {
				*ptr++ =
				    ISP_READ(isp, BIU_BLOCK + 0x80 + (i << 1));
			}
		}

		/*
		 * Frame Buffer Hardware Registers
		 */
		ISP_WRITE(isp, BIU2100_CSR, 0x10);
		for (i = 0; i < 16; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + 0x80 + (i << 1));
		}

		/*
		 * Fibre Protocol Module 0 Hardware Registers
		 */
		ISP_WRITE(isp, BIU2100_CSR, 0x20);
		for (i = 0; i < 64; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + 0x80 + (i << 1));
		}

		/*
		 * Fibre Protocol Module 1 Hardware Registers
		 */
		ISP_WRITE(isp, BIU2100_CSR, 0x30);
		for (i = 0; i < 64; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + 0x80 + (i << 1));
		}
	} else {
		isp_prt(isp, ISP_LOGERR, "RISC Would Not Pause");
		return;
	}
	isp_prt(isp, ISP_LOGALL,
	   "isp_fw_dump: RISC registers dumped successfully");
	ISP_WRITE(isp, BIU2100_CSR, BIU2100_SOFT_RESET);
	for (i = 0; i < 100; i++) {
		USEC_DELAY(100);
		if (ISP_READ(isp, OUTMAILBOX0) == 0) {
			break;
		}
	}
	if (ISP_READ(isp, OUTMAILBOX0) != 0) {
		isp_prt(isp, ISP_LOGERR, "Board Would Not Reset");
		return;
	}
	ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);
	for (i = 0; i < 100; i++) {
		USEC_DELAY(100);
		if (ISP_READ(isp, HCCR) & HCCR_PAUSE) {
			break;
		}
	}
	if ((ISP_READ(isp, HCCR) & HCCR_PAUSE) == 0) {
		isp_prt(isp, ISP_LOGERR, "RISC Would Not Pause After Reset");
		return;
	}
	ISP_WRITE(isp, RISC_EMB, 0xf2);
	ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE);
	for (i = 0; i < 100; i++) {
		USEC_DELAY(100);
		if ((ISP_READ(isp, HCCR) & HCCR_PAUSE) == 0) {
			break;
		}
	}
	ENABLE_INTS(isp);
	mbs.param[0] = MBOX_READ_RAM_WORD;
	mbs.param[1] = 0x1000;
	isp->isp_mbxworkp = (void *) ptr;
	isp->isp_mbxwrk0 = 0xefff;	/* continuation count */
	isp->isp_mbxwrk1 = 0x1001;	/* next SRAM address */
	isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_prt(isp, ISP_LOGWARN,
		    "RAM DUMP FAILED @ WORD %x", isp->isp_mbxwrk1);
		return;
	}
	ptr = isp->isp_mbxworkp;	/* finish fetch of final word */
	*ptr++ = isp->isp_mboxtmp[2];
	isp_prt(isp, ISP_LOGALL, "isp_fw_dump: SRAM dumped succesfully");
	FCPARAM(isp)->isp_dump_data[0] = isp->isp_type; /* now used */
	(void) isp_async(isp, ISPASYNC_FW_DUMPED, 0);
}

static void
isp2300_fw_dump(struct ispsoftc *isp)
{
	int i, j;
	mbreg_t mbs;
	u_int16_t *ptr;

	ptr = FCPARAM(isp)->isp_dump_data;
	if (ptr == NULL) {
		isp_prt(isp, ISP_LOGERR,
		   "No place to dump RISC registers and SRAM");
		return;
	}
	if (*ptr++) {
		isp_prt(isp, ISP_LOGERR,
		   "dump area for RISC registers and SRAM already used");
		return;
	}
	ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);
	for (i = 0; i < 100; i++) {
		USEC_DELAY(100);
		if (ISP_READ(isp, HCCR) & HCCR_PAUSE) {
			break;
		}
	}
	if (ISP_READ(isp, HCCR) & HCCR_PAUSE) {
		/*
		 * PBIU registers
		 */
		for (i = 0; i < 8; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + (i << 1));
		}

		/*
		 * ReqQ-RspQ-Risc2Host Status registers
		 */
		for (i = 0; i < 8; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + 0x10 + (i << 1));
		}

		/*
		 * Mailbox Registers
		 */
		for (i = 0; i < 32; i++) {
			*ptr++ =
			    ISP_READ(isp, PCI_MBOX_REGS2300_OFF + (i << 1));
		}

		/*
		 * Auto Request Response DMA registers
		 */
		ISP_WRITE(isp, BIU2100_CSR, 0x40);
		for (i = 0; i < 32; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + 0x80 + (i << 1));
		}

		/*
		 * DMA registers
		 */
		ISP_WRITE(isp, BIU2100_CSR, 0x50);
		for (i = 0; i < 48; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + 0x80 + (i << 1));
		}

		/*
		 * RISC hardware registers
		 */
		ISP_WRITE(isp, BIU2100_CSR, 0);
		for (i = 0; i < 16; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + 0xA0 + (i << 1));
		}

		/*
		 * RISC GP? registers
		 */
		for (j = 0; j < 8; j++) {
			ISP_WRITE(isp, BIU_BLOCK + 0xA4, 0x2000 + (j << 9));
			for (i = 0; i < 16; i++) {
				*ptr++ =
				    ISP_READ(isp, BIU_BLOCK + 0x80 + (i << 1));
			}
		}

		/*
		 * frame buffer hardware registers
		 */
		ISP_WRITE(isp, BIU2100_CSR, 0x10);
		for (i = 0; i < 64; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + 0x80 + (i << 1));
		}

		/*
		 * FPM B0 hardware registers
		 */
		ISP_WRITE(isp, BIU2100_CSR, 0x20);
		for (i = 0; i < 64; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + 0x80 + (i << 1));
		}

		/*
		 * FPM B1 hardware registers
		 */
		ISP_WRITE(isp, BIU2100_CSR, 0x30);
		for (i = 0; i < 64; i++) {
			*ptr++ = ISP_READ(isp, BIU_BLOCK + 0x80 + (i << 1));
		}
	} else {
		isp_prt(isp, ISP_LOGERR, "RISC Would Not Pause");
		return;
	}
	isp_prt(isp, ISP_LOGALL,
	   "isp_fw_dump: RISC registers dumped successfully");
	ISP_WRITE(isp, BIU2100_CSR, BIU2100_SOFT_RESET);
	for (i = 0; i < 100; i++) {
		USEC_DELAY(100);
		if (ISP_READ(isp, OUTMAILBOX0) == 0) {
			break;
		}
	}
	if (ISP_READ(isp, OUTMAILBOX0) != 0) {
		isp_prt(isp, ISP_LOGERR, "Board Would Not Reset");
		return;
	}
	ENABLE_INTS(isp);
	mbs.param[0] = MBOX_READ_RAM_WORD;
	mbs.param[1] = 0x800;
	isp->isp_mbxworkp = (void *) ptr;
	isp->isp_mbxwrk0 = 0xf7ff;	/* continuation count */
	isp->isp_mbxwrk1 = 0x801;	/* next SRAM address */
	isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_prt(isp, ISP_LOGWARN,
		    "RAM DUMP FAILED @ WORD %x", isp->isp_mbxwrk1);
		return;
	}
	ptr = isp->isp_mbxworkp;	/* finish fetch of final word */
	*ptr++ = isp->isp_mboxtmp[2];

	/*
	 * We don't have access to mailbox registers 8.. onward
	 * in our 'common' device model- so we have to set it
	 * here and hope it stays the same!
	 */
	ISP_WRITE(isp, PCI_MBOX_REGS2300_OFF + (8 << 1), 0x1);

	mbs.param[0] = MBOX_READ_RAM_WORD_EXTENDED;
	mbs.param[1] = 0;
	isp->isp_mbxworkp = (void *) ptr;
	isp->isp_mbxwrk0 = 0xffff;	/* continuation count */
	isp->isp_mbxwrk1 = 0x1;		/* next SRAM address */
	isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_prt(isp, ISP_LOGWARN,
		    "RAM DUMP FAILED @ WORD %x", 0x10000 + isp->isp_mbxwrk1);
		return;
	}
	ptr = isp->isp_mbxworkp;	/* finish final word */
	*ptr++ = mbs.param[2];
	isp_prt(isp, ISP_LOGALL, "isp_fw_dump: SRAM dumped succesfully");
	FCPARAM(isp)->isp_dump_data[0] = isp->isp_type; /* now used */
	(void) isp_async(isp, ISPASYNC_FW_DUMPED, 0);
}

void
isp_fw_dump(struct ispsoftc *isp)
{
	if (IS_2200(isp))
		isp2200_fw_dump(isp);
	else if (IS_23XX(isp))
		isp2300_fw_dump(isp);
}
#endif
