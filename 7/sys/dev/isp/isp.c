/*-
 *  Copyright (c) 1997-2007 by Matthew Jacob
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 *  THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */

/*
 * Machine and OS Independent (well, as best as possible)
 * code for the Qlogic ISP SCSI and FC-SCSI adapters.
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
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");
#include <dev/ic/isp_netbsd.h>
#endif
#ifdef	__FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
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
#define	ISP_MARK_PORTDB(a, b)	\
    isp_prt(isp, ISP_LOGSANCFG, "line %d: markportdb", __LINE__); \
    isp_mark_portdb(a, b)

/*
 * Local static data
 */
static const char fconf[] =
    "PortDB[%d] changed:\n current =(0x%x@0x%06x 0x%08x%08x 0x%08x%08x)\n"
    " database=(0x%x@0x%06x 0x%08x%08x 0x%08x%08x)";
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
    "HBA PortID 0x%06x N-Port Handle %d, Connection Topology '%s'";
static const char ourwwn[] =
    "HBA WWNN 0x%08x%08x HBA WWPN 0x%08x%08x";
static const char finmsg[] =
    "%d.%d.%d: FIN dl%d resid %d STS 0x%x SKEY %c XS_ERR=0x%x";
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
static int isp_parse_async(ispsoftc_t *, uint16_t);
static int isp_handle_other_response(ispsoftc_t *, int, isphdr_t *,
    uint32_t *);
static void
isp_parse_status(ispsoftc_t *, ispstatusreq_t *, XS_T *, long *);
static void
isp_parse_status_24xx(ispsoftc_t *, isp24xx_statusreq_t *, XS_T *, long *);
static void isp_fastpost_complete(ispsoftc_t *, uint16_t);
static int isp_mbox_continue(ispsoftc_t *);
static void isp_scsi_init(ispsoftc_t *);
static void isp_scsi_channel_init(ispsoftc_t *, int);
static void isp_fibre_init(ispsoftc_t *);
static void isp_fibre_init_2400(ispsoftc_t *);
static void isp_mark_portdb(ispsoftc_t *, int);
static int isp_plogx(ispsoftc_t *, uint16_t, uint32_t, int, int);
static int isp_port_login(ispsoftc_t *, uint16_t, uint32_t);
static int isp_port_logout(ispsoftc_t *, uint16_t, uint32_t);
static int isp_getpdb(ispsoftc_t *, uint16_t, isp_pdb_t *, int);
static uint64_t isp_get_portname(ispsoftc_t *, int, int);
static int isp_fclink_test(ispsoftc_t *, int);
static const char *ispfc_fw_statename(int);
static int isp_pdb_sync(ispsoftc_t *);
static int isp_scan_loop(ispsoftc_t *);
static int isp_gid_ft_sns(ispsoftc_t *);
static int isp_gid_ft_ct_passthru(ispsoftc_t *);
static int isp_scan_fabric(ispsoftc_t *);
static int isp_login_device(ispsoftc_t *, uint32_t, isp_pdb_t *, uint16_t *);
static int isp_register_fc4_type(ispsoftc_t *);
static int isp_register_fc4_type_24xx(ispsoftc_t *);
static uint16_t isp_nxt_handle(ispsoftc_t *, uint16_t);
static void isp_fw_state(ispsoftc_t *);
static void isp_mboxcmd_qnw(ispsoftc_t *, mbreg_t *, int);
static void isp_mboxcmd(ispsoftc_t *, mbreg_t *);

static void isp_update(ispsoftc_t *);
static void isp_update_bus(ispsoftc_t *, int);
static void isp_setdfltparm(ispsoftc_t *, int);
static void isp_setdfltfcparm(ispsoftc_t *);
static int isp_read_nvram(ispsoftc_t *);
static int isp_read_nvram_2400(ispsoftc_t *);
static void isp_rdnvram_word(ispsoftc_t *, int, uint16_t *);
static void isp_rd_2400_nvram(ispsoftc_t *, uint32_t, uint32_t *);
static void isp_parse_nvram_1020(ispsoftc_t *, uint8_t *);
static void isp_parse_nvram_1080(ispsoftc_t *, int, uint8_t *);
static void isp_parse_nvram_12160(ispsoftc_t *, int, uint8_t *);
static void isp_fix_nvram_wwns(ispsoftc_t *);
static void isp_parse_nvram_2100(ispsoftc_t *, uint8_t *);
static void isp_parse_nvram_2400(ispsoftc_t *, uint8_t *);

/*
 * Reset Hardware.
 *
 * Hit the chip over the head, download new f/w if available and set it running.
 *
 * Locking done elsewhere.
 */

void
isp_reset(ispsoftc_t *isp)
{
	mbreg_t mbs;
	uint32_t code_org, val;
	int loops, i, dodnld = 1;
	static const char *btype = "????";
	static const char dcrc[] = "Downloaded RISC Code Checksum Failure";

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
			if (IS_24XX(isp)) {
				ISP_WRITE(isp, BIU2400_HCCR,
				    HCCR_2400_CMD_RELEASE);
			} else {
				ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE);
			}
			MEMZERO(&mbs, sizeof (mbs));
			mbs.param[0] = MBOX_ABOUT_FIRMWARE;
			mbs.logval = MBLOGNONE;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
				isp->isp_romfw_rev[0] = mbs.param[1];
				isp->isp_romfw_rev[1] = mbs.param[2];
				isp->isp_romfw_rev[2] = mbs.param[3];
			}
		}
		isp->isp_touched = 1;
	}

	ISP_DISABLE_INTS(isp);

	/*
	 * Pick an initial maxcmds value which will be used
	 * to allocate xflist pointer space. It may be changed
	 * later by the firmware.
	 */
	if (IS_24XX(isp)) {
		isp->isp_maxcmds = 4096;
	} else if (IS_2322(isp)) {
		isp->isp_maxcmds = 2048;
	} else if (IS_23XX(isp) || IS_2200(isp)) {
		isp->isp_maxcmds = 1024;
 	} else {
		isp->isp_maxcmds = 512;
	}

	/*
	 * Set up DMA for the request and result queues.
	 *
	 * We do this now so we can use the request queue
	 * for a dma
	 */
	if (ISP_MBOXDMASETUP(isp) != 0) {
		isp_prt(isp, ISP_LOGERR, "Cannot setup DMA");
		return;
	}


	/*
	 * Set up default request/response queue in-pointer/out-pointer
	 * register indices.
	 */
	if (IS_24XX(isp)) {
		isp->isp_rqstinrp = BIU2400_REQINP;
		isp->isp_rqstoutrp = BIU2400_REQOUTP;
		isp->isp_respinrp = BIU2400_RSPINP;
		isp->isp_respoutrp = BIU2400_RSPOUTP;
		isp->isp_atioinrp = BIU2400_ATIO_RSPINP;
		isp->isp_atiooutrp = BIU2400_ATIO_REQINP;
	} else if (IS_23XX(isp)) {
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
	if (IS_24XX(isp)) {
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_HOST_INT);
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_RISC_INT);
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_PAUSE);
	} else {
		ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);
	}

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
		case ISP_HA_FC_2322:
			btype = "2322";
			break;
		case ISP_HA_FC_2400:
			btype = "2422";
			break;
		default:
			break;
		}

		if (!IS_24XX(isp)) {
			/*
			 * While we're paused, reset the FPM module and FBM
			 * fifos.
			 */
			ISP_WRITE(isp, BIU2100_CSR, BIU2100_FPM0_REGS);
			ISP_WRITE(isp, FPM_DIAG_CONFIG, FPM_SOFT_RESET);
			ISP_WRITE(isp, BIU2100_CSR, BIU2100_FB_REGS);
			ISP_WRITE(isp, FBM_CMD, FBMCMD_FIFO_RESET_ALL);
			ISP_WRITE(isp, BIU2100_CSR, BIU2100_RISC_REGS);
		}
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
		uint16_t l;
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
			 * If we're in Ultra Mode, we have to be 60MHz clock-
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


	} else if (IS_24XX(isp)) {
		/*
		 * Stop DMA and wait for it to stop.
		 */
		ISP_WRITE(isp, BIU2400_CSR, BIU2400_DMA_STOP|(3 << 4));
		for (val = loops = 0; loops < 30000; loops++) {
			USEC_DELAY(10);
			val = ISP_READ(isp, BIU2400_CSR);
			if ((val & BIU2400_DMA_ACTIVE) == 0) {
				break;
			}
		} 
		if (val & BIU2400_DMA_ACTIVE) {
			ISP_RESET0(isp);
			isp_prt(isp, ISP_LOGERR, "DMA Failed to Stop on Reset");
			return;
		}
		/*
		 * Hold it in SOFT_RESET and STOP state for 100us.
		 */
		ISP_WRITE(isp, BIU2400_CSR,
		    BIU2400_SOFT_RESET|BIU2400_DMA_STOP|(3 << 4));
		USEC_DELAY(100);
		for (loops = 0; loops < 10000; loops++) {
			USEC_DELAY(5);
			val = ISP_READ(isp, OUTMAILBOX0);
		}
		for (val = loops = 0; loops < 500000; loops ++) {
			val = ISP_READ(isp, BIU2400_CSR);
			if ((val & BIU2400_SOFT_RESET) == 0) {
				break;
			}
		}
		if (val & BIU2400_SOFT_RESET) {
			ISP_RESET0(isp);
			isp_prt(isp, ISP_LOGERR, "Failed to come out of reset");
			return;
		}
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
			if (!(ISP_READ(isp, BIU_ICR) & BIU_ICR_SOFT_RESET)) {
				break;
			}
		} else if (IS_24XX(isp)) {
			if (ISP_READ(isp, OUTMAILBOX0) == 0) {
				break;
			}
		} else {
			if (!(ISP_READ(isp, BIU2100_CSR) & BIU2100_SOFT_RESET))
				break;
		}
		USEC_DELAY(100);
		if (--loops < 0) {
			ISP_DUMPREGS(isp, "chip reset timed out");
			ISP_RESET0(isp);
			return;
		}
	}

	/*
	 * After we've fired this chip up, zero out the conf1 register
	 * for SCSI adapters and other settings for the 2100.
	 */

	if (IS_SCSI(isp)) {
		ISP_WRITE(isp, BIU_CONF1, 0);
	} else if (!IS_24XX(isp)) {
		ISP_WRITE(isp, BIU2100_CSR, 0);
	}

	/*
	 * Reset RISC Processor
	 */
	if (IS_24XX(isp)) {
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_RESET);
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_RELEASE);
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_RESET);
	} else {
		ISP_WRITE(isp, HCCR, HCCR_CMD_RESET);
		USEC_DELAY(100);
		ISP_WRITE(isp, BIU_SEMA, 0);
	}

	
	/*
	 * Post-RISC Reset stuff.
	 */
	if (IS_24XX(isp)) {
		for (val = loops = 0; loops < 5000000; loops++) {
			USEC_DELAY(5);
			val = ISP_READ(isp, OUTMAILBOX0);
			if (val == 0) {
				break;
			}
		}
		if (val != 0) {
			ISP_RESET0(isp);
			isp_prt(isp, ISP_LOGERR, "reset didn't clear");
			return;
		}
	} else if (IS_SCSI(isp)) {
		uint16_t tmp = isp->isp_mdvec->dv_conf1;
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
		if (SDPARAM(isp)->isp_ptisp) {
			if (SDPARAM(isp)->isp_ultramode) {
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
			ISP_WRITE(isp, RISC_EMB, DUAL_BANK);
		} else {
			ISP_WRITE(isp, RISC_MTR, 0x1212);
		}
		ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE);
	} else {
		ISP_WRITE(isp, RISC_MTR2100, 0x1212);
		if (IS_2200(isp) || IS_23XX(isp)) {
			ISP_WRITE(isp, HCCR, HCCR_2X00_DISABLE_PARITY_PAUSE);
		}
		ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE);
	}

	ISP_WRITE(isp, isp->isp_rqstinrp, 0);
	ISP_WRITE(isp, isp->isp_rqstoutrp, 0);
	ISP_WRITE(isp, isp->isp_respinrp, 0);
	ISP_WRITE(isp, isp->isp_respoutrp, 0);


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
	if (IS_2312(isp)) {
		USEC_DELAY(100);
	} else {
		loops = MBOX_DELAY_COUNT;
		while (ISP_READ(isp, OUTMAILBOX0) == MBOX_BUSY) {
			USEC_DELAY(100);
			if (--loops < 0) {
				ISP_RESET0(isp);
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
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_NO_OP;
	mbs.logval = MBLOGALL;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_RESET0(isp);
		return;
	}

	if (IS_SCSI(isp) || IS_24XX(isp)) {
		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_MAILBOX_REG_TEST;
		mbs.param[1] = 0xdead;
		mbs.param[2] = 0xbeef;
		mbs.param[3] = 0xffff;
		mbs.param[4] = 0x1111;
		mbs.param[5] = 0xa5a5;
		mbs.param[6] = 0x0000;
		mbs.param[7] = 0x0000;
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			ISP_RESET0(isp);
			return;
		}
		if (mbs.param[1] != 0xdead || mbs.param[2] != 0xbeef ||
		    mbs.param[3] != 0xffff || mbs.param[4] != 0x1111 ||
		    mbs.param[5] != 0xa5a5) {
			ISP_RESET0(isp);
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

	if (IS_24XX(isp)) {
		code_org = ISP_CODE_ORG_2400;
	} else if (IS_23XX(isp)) {
		code_org = ISP_CODE_ORG_2300;
	} else {
		code_org = ISP_CODE_ORG;
	}

	if (dodnld && IS_24XX(isp)) {
		const uint32_t *ptr = isp->isp_mdvec->dv_ispfw;

		/*
		 * NB: Whatever you do do, do *not* issue the VERIFY FIRMWARE
		 * NB: command to the 2400 while loading new firmware. This
		 * NB: causes the new f/w to start and immediately crash back
		 * NB: to the ROM.
		 */

		/*
		 * Keep loading until we run out of f/w.
		 */
		code_org = ptr[2];	/* 1st load address is our start addr */

		for (;;) {
			uint32_t la, wi, wl;

			isp_prt(isp, ISP_LOGDEBUG0,
			    "load 0x%x words of code at load address 0x%x",
			    ptr[3], ptr[2]);

			wi = 0;
			la = ptr[2];
			wl = ptr[3];

			while (wi < ptr[3]) {
				uint32_t *cp;
				uint32_t nw;

				nw = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp)) >> 2;
				if (nw > wl) {
					nw = wl;
				}
				cp = isp->isp_rquest;
				for (i = 0; i < nw; i++) {
					ISP_IOXPUT_32(isp,  ptr[wi++], &cp[i]);
					wl--;
				}
				MEMORYBARRIER(isp, SYNC_REQUEST,
				    0, ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp)));
				MEMZERO(&mbs, sizeof (mbs));
				mbs.param[0] = MBOX_LOAD_RISC_RAM;
				mbs.param[1] = la;
				mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
				mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
				mbs.param[4] = nw >> 16;
				mbs.param[5] = nw;
				mbs.param[6] = DMA_WD3(isp->isp_rquest_dma);
				mbs.param[7] = DMA_WD2(isp->isp_rquest_dma);
				mbs.param[8] = la >> 16;
				mbs.logval = MBLOGALL;
				isp_mboxcmd(isp, &mbs);
				if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
					isp_prt(isp, ISP_LOGERR,
					    "F/W Risc Ram Load Failed");
					ISP_RESET0(isp);
					return;
				}
				la += nw;
			}

			if (ptr[1] == 0) {
				break;
			}
			ptr += ptr[3];
		} 
		isp->isp_loaded_fw = 1;
	} else if (dodnld && IS_23XX(isp)) {
		const uint16_t *ptr = isp->isp_mdvec->dv_ispfw;
		uint16_t wi, wl, segno;
		uint32_t la;

		la = code_org;
		segno = 0;

		for (;;) {
			uint32_t nxtaddr;

			isp_prt(isp, ISP_LOGDEBUG0,
			    "load 0x%x words of code at load address 0x%x",
			    ptr[3], la);

			wi = 0;
			wl = ptr[3];

			while (wi < ptr[3]) {
				uint16_t *cp;
				uint32_t nw;
				
				nw = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp)) >> 1;
				if (nw > wl) {
					nw = wl;
				}
				if (nw > (1 << 15)) {
					nw = 1 << 15;
				}
				cp = isp->isp_rquest;
				for (i = 0; i < nw; i++) {
					ISP_IOXPUT_16(isp,  ptr[wi++], &cp[i]);
					wl--;
				}
				MEMORYBARRIER(isp, SYNC_REQUEST,
				    0, ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp)));
				MEMZERO(&mbs, sizeof (mbs));
				mbs.param[0] = MBOX_LOAD_RISC_RAM;
				mbs.param[1] = la;
				mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
				mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
				mbs.param[4] = nw;
				mbs.param[6] = DMA_WD3(isp->isp_rquest_dma);
				mbs.param[7] = DMA_WD2(isp->isp_rquest_dma);
				mbs.param[8] = la >> 16;
				mbs.logval = MBLOGALL;
				isp_mboxcmd(isp, &mbs);
				if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
					isp_prt(isp, ISP_LOGERR,
					    "F/W Risc Ram Load Failed");
					ISP_RESET0(isp);
					return;
				}
				la += nw;
			}

			if (!IS_2322(isp)) {
				/*
				 * Verify that it downloaded correctly.
				 */
				MEMZERO(&mbs, sizeof (mbs));
				mbs.param[0] = MBOX_VERIFY_CHECKSUM;
				mbs.param[1] = code_org;
				mbs.logval = MBLOGNONE;
				isp_mboxcmd(isp, &mbs);
				if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
					isp_prt(isp, ISP_LOGERR, dcrc);
					ISP_RESET0(isp);
					return;
				}
				break;
			}

			if (++segno == 3) {
				break;
			}

			/*
			 * If we're a 2322, the firmware actually comes in
			 * three chunks. We loaded the first at the code_org
			 * address. The other two chunks, which follow right
			 * after each other in memory here, get loaded at
			 * addresses specfied at offset 0x9..0xB.
			 */

			nxtaddr = ptr[3];
			ptr = &ptr[nxtaddr];
			la = ptr[5] | ((ptr[4] & 0x3f) << 16);
		}
		isp->isp_loaded_fw = 1;
	} else if (dodnld) {
		union {
			const uint16_t *cp;
			uint16_t *np;
		} u;
		u.cp = isp->isp_mdvec->dv_ispfw;
		isp->isp_mbxworkp = &u.np[1];
		isp->isp_mbxwrk0 = u.np[3] - 1;
		isp->isp_mbxwrk1 = code_org + 1;
		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_WRITE_RAM_WORD;
		mbs.param[1] = code_org;
		mbs.param[2] = u.np[0];
		mbs.logval = MBLOGNONE;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGERR,
			    "F/W download failed at word %d",
			    isp->isp_mbxwrk1 - code_org);
			ISP_RESET0(isp);
			return;
		}
		/*
		 * Verify that it downloaded correctly.
		 */
		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_VERIFY_CHECKSUM;
		mbs.param[1] = code_org;
		mbs.logval = MBLOGNONE;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGERR, dcrc);
			ISP_RESET0(isp);
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


	MEMZERO(&mbs, sizeof (mbs));
	mbs.timeout = 1000000;
	mbs.param[0] = MBOX_EXEC_FIRMWARE;
	if (IS_24XX(isp)) {
		mbs.param[1] = code_org >> 16;
		mbs.param[2] = code_org;
		if (isp->isp_loaded_fw) {
			mbs.param[3] = 0;
		} else {
			mbs.param[3] = 1;
		}
	} else if (IS_2322(isp)) {
		mbs.param[1] = code_org;
		if (isp->isp_loaded_fw) {
			mbs.param[2] = 0;
		} else {
			mbs.param[2] = 1;
		}
	} else {
		mbs.param[1] = code_org;
	}

	mbs.logval = MBLOGALL;
	isp_mboxcmd(isp, &mbs);
	if (IS_2322(isp) || IS_24XX(isp)) {
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			ISP_RESET0(isp);
			return;
		}
	}

	/*
	 * Give it a chance to finish starting up.
	 */
	USEC_DELAY(250000);

	if (IS_SCSI(isp)) {
		/*
		 * Set CLOCK RATE, but only if asked to.
		 */
		if (isp->isp_clock) {
			mbs.param[0] = MBOX_SET_CLOCK_RATE;
			mbs.param[1] = isp->isp_clock;
			mbs.logval = MBLOGNONE;
			isp_mboxcmd(isp, &mbs);
			/* we will try not to care if this fails */
		}
	}

	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_ABOUT_FIRMWARE;
	mbs.logval = MBLOGALL;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		ISP_RESET0(isp);
		return;
	}

	if (IS_24XX(isp) && mbs.param[1] == 0xdead) {
		isp_prt(isp, ISP_LOGERR, "f/w didn't *really* start");
		ISP_RESET0(isp);
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

	isp_prt(isp, ISP_LOGALL,
	    "Board Type %s, Chip Revision 0x%x, %s F/W Revision %d.%d.%d",
	    btype, isp->isp_revision, dodnld? "loaded" : "resident",
	    isp->isp_fwrev[0], isp->isp_fwrev[1], isp->isp_fwrev[2]);

	if (IS_FC(isp)) {
		/*
		 * We do not believe firmware attributes for 2100 code less
		 * than 1.17.0, unless it's the firmware we specifically
		 * are loading.
		 *
		 * Note that all 22XX and later f/w is greater than 1.X.0.
		 */
		if ((ISP_FW_OLDER_THAN(isp, 1, 17, 1))) {
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
		FCPARAM(isp)->isp_2klogin = 0;
		FCPARAM(isp)->isp_sccfw = 0;
		FCPARAM(isp)->isp_tmode = 0;
		if (IS_24XX(isp)) {
			FCPARAM(isp)->isp_2klogin = 1;
			FCPARAM(isp)->isp_sccfw = 1;
			FCPARAM(isp)->isp_tmode = 1;
		} else {
			if (FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_SCCLUN) {
				FCPARAM(isp)->isp_sccfw = 1;
			}
			if (FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_2KLOGINS) {
				FCPARAM(isp)->isp_2klogin = 1;
				FCPARAM(isp)->isp_sccfw = 1;
			}
			if (FCPARAM(isp)->isp_fwattr & ISP_FW_ATTR_TMODE) {
				FCPARAM(isp)->isp_tmode = 1;
			}
		}
		if (FCPARAM(isp)->isp_2klogin) {
			isp_prt(isp, ISP_LOGCONFIG, "2K Logins Supported");
		}
	}

	if (isp->isp_romfw_rev[0] || isp->isp_romfw_rev[1] ||
	    isp->isp_romfw_rev[2]) {
		isp_prt(isp, ISP_LOGCONFIG, "Last F/W revision was %d.%d.%d",
		    isp->isp_romfw_rev[0], isp->isp_romfw_rev[1],
		    isp->isp_romfw_rev[2]);
	}

	if (!IS_24XX(isp)) {
		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_GET_FIRMWARE_STATUS;
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			ISP_RESET0(isp);
			return;
		}
		if (isp->isp_maxcmds >= mbs.param[2]) {
			isp->isp_maxcmds = mbs.param[2];
		}
	}
	isp_prt(isp, ISP_LOGCONFIG,
	    "%d max I/O command limit set", isp->isp_maxcmds);
	isp_fw_state(isp);

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
		if (FCPARAM(isp)->isp_sccfw) {
			isp->isp_maxluns = 16384;
		} else {
			isp->isp_maxluns = 16;
		}
	}
	/*
	 * Must do this first to get defaults established.
	 */
	if (IS_SCSI(isp)) {
		isp_setdfltparm(isp, 0);
		if (IS_DUALBUS(isp)) {
			isp_setdfltparm(isp, 1);
		}
	} else {
		isp_setdfltfcparm(isp);
	}

}

/*
 * Initialize Parameters of Hardware to a known state.
 *
 * Locks are held before coming here.
 */

void
isp_init(ispsoftc_t *isp)
{
	if (IS_FC(isp)) {
		/*
		 * Do this *before* initializing the firmware.
		 */
		ISP_MARK_PORTDB(isp, 0);
		FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp)->isp_loopstate = LOOP_NIL;

		if (isp->isp_role != ISP_ROLE_NONE) {
			if (IS_24XX(isp)) {
				isp_fibre_init_2400(isp);
			} else {
				isp_fibre_init(isp);
			}
		}
	} else {
		isp_scsi_init(isp);
	}
}

static void
isp_scsi_init(ispsoftc_t *isp)
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
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_SET_RETRY_COUNT;
	mbs.param[1] = sdp_chan0->isp_retry_count;
	mbs.param[2] = sdp_chan0->isp_retry_delay;
	mbs.param[6] = sdp_chan1->isp_retry_count;
	mbs.param[7] = sdp_chan1->isp_retry_delay;
	mbs.logval = MBLOGALL;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}

	/*
	 * Set ASYNC DATA SETUP time. This is very important.
	 */
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_SET_ASYNC_DATA_SETUP_TIME;
	mbs.param[1] = sdp_chan0->isp_async_data_setup;
	mbs.param[2] = sdp_chan1->isp_async_data_setup;
	mbs.logval = MBLOGALL;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}

	/*
	 * Set ACTIVE Negation State.
	 */
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_SET_ACT_NEG_STATE;
	mbs.param[1] =
	    (sdp_chan0->isp_req_ack_active_neg << 4) |
	    (sdp_chan0->isp_data_line_active_neg << 5);
	mbs.param[2] =
	    (sdp_chan1->isp_req_ack_active_neg << 4) |
	    (sdp_chan1->isp_data_line_active_neg << 5);
	mbs.logval = MBLOGNONE;
	isp_mboxcmd(isp, &mbs);
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
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_SET_TAG_AGE_LIMIT;
	mbs.param[1] = sdp_chan0->isp_tag_aging;
	mbs.param[2] = sdp_chan1->isp_tag_aging;
	mbs.logval = MBLOGALL;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_prt(isp, ISP_LOGERR, "failed to set tag age limit (%d,%d)",
		    sdp_chan0->isp_tag_aging, sdp_chan1->isp_tag_aging);
		return;
	}

	/*
	 * Set selection timeout.
	 */
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_SET_SELECT_TIMEOUT;
	mbs.param[1] = sdp_chan0->isp_selection_timeout;
	mbs.param[2] = sdp_chan1->isp_selection_timeout;
	mbs.logval = MBLOGALL;
	isp_mboxcmd(isp, &mbs);
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
		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_INIT_RES_QUEUE_A64;
		mbs.param[1] = RESULT_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_result_dma);
		mbs.param[3] = DMA_WD0(isp->isp_result_dma);
		mbs.param[4] = 0;
		mbs.param[6] = DMA_WD3(isp->isp_result_dma);
		mbs.param[7] = DMA_WD2(isp->isp_result_dma);
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		isp->isp_residx = mbs.param[5];

		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_INIT_REQ_QUEUE_A64;
		mbs.param[1] = RQUEST_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
		mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
		mbs.param[5] = 0;
		mbs.param[6] = DMA_WD3(isp->isp_result_dma);
		mbs.param[7] = DMA_WD2(isp->isp_result_dma);
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		isp->isp_reqidx = isp->isp_reqodx = mbs.param[4];
	} else {
		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_INIT_RES_QUEUE;
		mbs.param[1] = RESULT_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_result_dma);
		mbs.param[3] = DMA_WD0(isp->isp_result_dma);
		mbs.param[4] = 0;
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		isp->isp_residx = mbs.param[5];

		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_INIT_REQ_QUEUE;
		mbs.param[1] = RQUEST_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
		mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
		mbs.param[5] = 0;
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
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

	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_SET_FW_FEATURES;
	mbs.param[1] = 0;
	if (IS_ULTRA2(isp))
		mbs.param[1] |= FW_FEATURE_LVD_NOTIFY;
#ifndef	ISP_NO_RIO
	if (IS_ULTRA2(isp) || IS_1240(isp))
		mbs.param[1] |= FW_FEATURE_RIO_16BIT;
#else
	if (IS_ULTRA2(isp) || IS_1240(isp))
		mbs.param[1] |= FW_FEATURE_FAST_POST;
#endif
	if (mbs.param[1] != 0) {
		uint16_t sfeat = mbs.param[1];
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
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
isp_scsi_channel_init(ispsoftc_t *isp, int channel)
{
	sdparam *sdp;
	mbreg_t mbs;
	int tgt;

	sdp = isp->isp_param;
	sdp += channel;

	/*
	 * Set (possibly new) Initiator ID.
	 */
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_SET_INIT_SCSI_ID;
	mbs.param[1] = (channel << 7) | sdp->isp_initiator_id;
	mbs.logval = MBLOGALL;
	isp_mboxcmd(isp, &mbs);
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
		uint16_t sdf;

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
		MEMZERO(&mbs, sizeof (mbs));
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
		mbs.logval = MBLOGNONE;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			sdf = DPARM_SAFE_DFLT;
			MEMZERO(&mbs, sizeof (mbs));
			mbs.param[0] = MBOX_SET_TARGET_PARAMS;
			mbs.param[1] = (tgt << 8) | (channel << 15);
			mbs.param[2] = sdf;
			mbs.param[3] = 0;
			mbs.logval = MBLOGALL;
			isp_mboxcmd(isp, &mbs);
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
			MEMZERO(&mbs, sizeof (mbs));
			mbs.param[0] = MBOX_SET_DEV_QUEUE_PARAMS;
			mbs.param[1] = (channel << 15) | (tgt << 8) | lun;
			mbs.param[2] = sdp->isp_max_queue_depth;
			mbs.param[3] = sdp->isp_devparam[tgt].exc_throttle;
			mbs.logval = MBLOGALL;
			isp_mboxcmd(isp, &mbs);
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
 */
static void
isp_fibre_init(ispsoftc_t *isp)
{
	fcparam *fcp;
	isp_icb_t local, *icbp = &local;
	mbreg_t mbs;
	int ownloopid;
	uint64_t nwwn, pwwn;

	fcp = isp->isp_param;

	MEMZERO(icbp, sizeof (*icbp));
	icbp->icb_version = ICB_VERSION1;
	icbp->icb_fwoptions = fcp->isp_fwoptions;

	/*
	 * Firmware Options are either retrieved from NVRAM or
	 * are patched elsewhere. We check them for sanity here
	 * and make changes based on board revision, but otherwise
	 * let others decide policy.
	 */

	/*
	 * If this is a 2100 < revision 5, we have to turn off FAIRNESS.
	 */
	if (IS_2100(isp) && isp->isp_revision < 5) {
		icbp->icb_fwoptions &= ~ICBOPT_FAIRNESS;
	}

	/*
	 * We have to use FULL LOGIN even though it resets the loop too much
	 * because otherwise port database entries don't get updated after
	 * a LIP- this is a known f/w bug for 2100 f/w less than 1.17.0.
	 */
	if (!ISP_FW_NEWER_THAN(isp, 1, 17, 0)) {
		icbp->icb_fwoptions |= ICBOPT_FULL_LOGIN;
	}

	/*
	 * Insist on Port Database Update Async notifications
	 */
	icbp->icb_fwoptions |= ICBOPT_PDBCHANGE_AE;

	/*
	 * Make sure that target role reflects into fwoptions.
	 */
	if (isp->isp_role & ISP_ROLE_TARGET) {
		icbp->icb_fwoptions |= ICBOPT_TGT_ENABLE;
	} else {
		icbp->icb_fwoptions &= ~ICBOPT_TGT_ENABLE;
	}

	if (isp->isp_role & ISP_ROLE_INITIATOR) {
		icbp->icb_fwoptions &= ~ICBOPT_INI_DISABLE;
	} else {
		icbp->icb_fwoptions |= ICBOPT_INI_DISABLE;
	}

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
	icbp->icb_hardaddr = fcp->isp_loopid;
	ownloopid = (isp->isp_confopts & ISP_CFG_OWNLOOPID) != 0;
	if (icbp->icb_hardaddr > 125) {
		icbp->icb_hardaddr = 0;
		ownloopid = 0;
	}

	/*
	 * Our life seems so much better with 2200s and later with
	 * the latest f/w if we set Hard Address.
	 */
	if (ownloopid || ISP_FW_NEWER_THAN(isp, 2, 2, 5)) {
		icbp->icb_fwoptions |= ICBOPT_HARD_ADDRESS;
	}

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
		if (IS_2200(isp)) {
			icbp->icb_fwoptions &= ~ICBOPT_FAST_POST;
		} else {
			/*
			 * QLogic recommends that FAST Posting be turned
			 * off for 23XX cards and instead allow the HBA
			 * to write response queue entries and interrupt
			 * after a delay (ZIO).
			 */
			icbp->icb_fwoptions &= ~ICBOPT_FAST_POST;
			if ((fcp->isp_xfwoptions & ICBXOPT_TIMER_MASK) ==
			    ICBXOPT_ZIO) {
				icbp->icb_xfwoptions |= ICBXOPT_ZIO;
				icbp->icb_idelaytimer = 10;
			}
			if (isp->isp_confopts & ISP_CFG_ONEGB) {
				icbp->icb_zfwoptions |= ICBZOPT_RATE_ONEGB;
			} else if (isp->isp_confopts & ISP_CFG_TWOGB) {
				icbp->icb_zfwoptions |= ICBZOPT_RATE_TWOGB;
			} else {
				icbp->icb_zfwoptions |= ICBZOPT_RATE_AUTO;
			}
			if (fcp->isp_zfwoptions & ICBZOPT_50_OHM) {
				icbp->icb_zfwoptions |= ICBZOPT_50_OHM;
			}
		}
	}


	/*
	 * For 22XX > 2.1.26 && 23XX, set some options.
	 * XXX: Probably okay for newer 2100 f/w too.
	 */
	if (ISP_FW_NEWER_THAN(isp, 2, 26, 0)) {
		/*
		 * Turn on LIP F8 async event (1)
		 * Turn on generate AE 8013 on all LIP Resets (2)
		 * Disable LIP F7 switching (8)
		 */
		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_SET_FIRMWARE_OPTIONS;
		mbs.param[1] = 0xb;
		mbs.param[2] = 0;
		mbs.param[3] = 0;
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
	}
	icbp->icb_logintime = ICB_LOGIN_TOV;
	icbp->icb_lunetimeout = ICB_LUN_ENABLE_TOV;

	nwwn = ISP_NODEWWN(isp);
	pwwn = ISP_PORTWWN(isp);
	if (nwwn && pwwn) {
		icbp->icb_fwoptions |= ICBOPT_BOTH_WWNS;
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_nodename, nwwn);
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_portname, pwwn);
		isp_prt(isp, ISP_LOGDEBUG1,
		    "Setting ICB Node 0x%08x%08x Port 0x%08x%08x",
		    ((uint32_t) (nwwn >> 32)),
		    ((uint32_t) (nwwn & 0xffffffff)),
		    ((uint32_t) (pwwn >> 32)),
		    ((uint32_t) (pwwn & 0xffffffff)));
	} else if (pwwn) {
		icbp->icb_fwoptions &= ~ICBOPT_BOTH_WWNS;
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_portname, pwwn);
		isp_prt(isp, ISP_LOGDEBUG1,
		    "Setting ICB Port 0x%08x%08x",
		    ((uint32_t) (pwwn >> 32)),
		    ((uint32_t) (pwwn & 0xffffffff)));
	} else {
		isp_prt(isp, ISP_LOGERR, "No valid WWNs to use");
		return;
	}
	icbp->icb_rqstqlen = RQUEST_QUEUE_LEN(isp);
	if (icbp->icb_rqstqlen < 1) {
		isp_prt(isp, ISP_LOGERR, "bad request queue length");
	}
	icbp->icb_rsltqlen = RESULT_QUEUE_LEN(isp);
	if (icbp->icb_rsltqlen < 1) {
		isp_prt(isp, ISP_LOGERR, "bad result queue length");
	}
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
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_INIT_FIRMWARE;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	mbs.logval = MBLOGALL;
	mbs.timeout = 30 * 1000000;
	isp_prt(isp, ISP_LOGDEBUG0, "INIT F/W from %p (%08x%08x)",
	    fcp->isp_scratch, (uint32_t) ((uint64_t)fcp->isp_scdma >> 32),
	    (uint32_t) fcp->isp_scdma);
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, sizeof (*icbp));
	isp_mboxcmd(isp, &mbs);
	FC_SCRATCH_RELEASE(isp);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_print_bytes(isp, "isp_fibre_init", sizeof (*icbp), icbp);
		return;
	}
	isp->isp_reqidx = 0;
	isp->isp_reqodx = 0;
	isp->isp_residx = 0;

	/*
	 * Whatever happens, we're now committed to being here.
	 */
	isp->isp_state = ISP_INITSTATE;
}

static void
isp_fibre_init_2400(ispsoftc_t *isp)
{
	fcparam *fcp;
	isp_icb_2400_t local, *icbp = &local;
	mbreg_t mbs;
	int ownloopid;
	uint64_t nwwn, pwwn;

	fcp = isp->isp_param;

	/*
	 * Turn on LIP F8 async event (1)
	 */
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_SET_FIRMWARE_OPTIONS;
	mbs.param[1] = 1;
	mbs.logval = MBLOGALL;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}

	/*
	 * XXX: This should be applied to icb- not fwoptions
	 */
	if (isp->isp_role & ISP_ROLE_TARGET) {
		fcp->isp_fwoptions |= ICB2400_OPT1_TGT_ENABLE;
	} else {
		fcp->isp_fwoptions &= ~ICB2400_OPT1_TGT_ENABLE;
	}

	if (isp->isp_role & ISP_ROLE_INITIATOR) {
		fcp->isp_fwoptions &= ~ICB2400_OPT1_INI_DISABLE;
	} else {
		fcp->isp_fwoptions |= ICB2400_OPT1_INI_DISABLE;
	}

	MEMZERO(icbp, sizeof (*icbp));
	icbp->icb_version = ICB_VERSION1;
	icbp->icb_maxfrmlen = fcp->isp_maxfrmlen;
	if (icbp->icb_maxfrmlen < ICB_MIN_FRMLEN ||
	    icbp->icb_maxfrmlen > ICB_MAX_FRMLEN) {
		isp_prt(isp, ISP_LOGERR,
		    "bad frame length (%d) from NVRAM- using %d",
		    fcp->isp_maxfrmlen, ICB_DFLT_FRMLEN);
		icbp->icb_maxfrmlen = ICB_DFLT_FRMLEN;
	}

	icbp->icb_execthrottle = fcp->isp_execthrottle;
	if (icbp->icb_execthrottle < 1) {
		isp_prt(isp, ISP_LOGERR,
		    "bad execution throttle of %d- using 16",
		    fcp->isp_execthrottle);
		icbp->icb_execthrottle = ICB_DFLT_THROTTLE;
	}

	if (isp->isp_role & ISP_ROLE_TARGET) {
		/*
		 * Get current resource count
		 */
		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_GET_RESOURCE_COUNT;
		mbs.obits = 0x4cf;
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		icbp->icb_xchgcnt = mbs.param[3];
	}

	icbp->icb_fwoptions1 = fcp->isp_fwoptions;

	icbp->icb_hardaddr = fcp->isp_loopid;
	ownloopid = (isp->isp_confopts & ISP_CFG_OWNLOOPID) != 0;
	if (icbp->icb_hardaddr > 125) {
		icbp->icb_hardaddr = 0;
		ownloopid = 0;
	}
	if (ownloopid) {
		icbp->icb_fwoptions1 |= ICB2400_OPT1_HARD_ADDRESS;
	}

	icbp->icb_fwoptions2 = fcp->isp_xfwoptions;
	switch(isp->isp_confopts & ISP_CFG_PORT_PREF) {
	case ISP_CFG_NPORT:
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TOPO_MASK;
		icbp->icb_fwoptions2 |= ICB2400_OPT2_PTP_2_LOOP;
		break;
	case ISP_CFG_NPORT_ONLY:
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TOPO_MASK;
		icbp->icb_fwoptions2 |= ICB2400_OPT2_PTP_ONLY;
		break;
	case ISP_CFG_LPORT_ONLY:
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TOPO_MASK;
		icbp->icb_fwoptions2 |= ICB2400_OPT2_LOOP_ONLY;
		break;
	default:
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TOPO_MASK;
		icbp->icb_fwoptions2 |= ICB2400_OPT2_LOOP_2_PTP;
		break;
	}

	switch (icbp->icb_fwoptions2 & ICB2400_OPT2_TIMER_MASK) {
	case ICB2400_OPT2_ZIO:
	case ICB2400_OPT2_ZIO1:
		icbp->icb_idelaytimer = 0;
		break;
	case 0:
		break;
	default:
		isp_prt(isp, ISP_LOGWARN, "bad value %x in fwopt2 timer field",
		    icbp->icb_fwoptions2 & ICB2400_OPT2_TIMER_MASK);
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TIMER_MASK;
		break;
	}

	icbp->icb_fwoptions3 = fcp->isp_zfwoptions;
	icbp->icb_fwoptions3 &= ~ICB2400_OPT3_RATE_AUTO;
	if (isp->isp_confopts & ISP_CFG_ONEGB) {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_ONEGB;
	} else if (isp->isp_confopts & ISP_CFG_TWOGB) {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_TWOGB;
	} else if (isp->isp_confopts & ISP_CFG_FOURGB) {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_FOURGB;
	} else {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_AUTO;
	}

	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0) {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_SOFTID;
	}
	icbp->icb_logintime = ICB_LOGIN_TOV;

	nwwn = ISP_NODEWWN(isp);
	pwwn = ISP_PORTWWN(isp);

	if (nwwn && pwwn) {
		icbp->icb_fwoptions1 |= ICB2400_OPT1_BOTH_WWNS;
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_nodename, nwwn);
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_portname, pwwn);
		isp_prt(isp, ISP_LOGDEBUG1,
		    "Setting ICB Node 0x%08x%08x Port 0x%08x%08x",
		    ((uint32_t) (nwwn >> 32)),
		    ((uint32_t) (nwwn & 0xffffffff)),
		    ((uint32_t) (pwwn >> 32)),
		    ((uint32_t) (pwwn & 0xffffffff)));
	} else if (pwwn) {
		icbp->icb_fwoptions1 &= ~ICB2400_OPT1_BOTH_WWNS;
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_portname, pwwn);
		isp_prt(isp, ISP_LOGDEBUG1,
		    "Setting ICB Port 0x%08x%08x",
		    ((uint32_t) (pwwn >> 32)),
		    ((uint32_t) (pwwn & 0xffffffff)));
	} else {
		isp_prt(isp, ISP_LOGERR, "No valid WWNs to use");
		return;
	}
	icbp->icb_retry_count = fcp->isp_retry_count;

	icbp->icb_rqstqlen = RQUEST_QUEUE_LEN(isp);
	if (icbp->icb_rqstqlen < 8) {
		isp_prt(isp, ISP_LOGERR, "bad request queue length %d",
		    icbp->icb_rqstqlen);
		return;
	}
	icbp->icb_rsltqlen = RESULT_QUEUE_LEN(isp);
	if (icbp->icb_rsltqlen < 8) {
		isp_prt(isp, ISP_LOGERR, "bad result queue length %d",
		    icbp->icb_rsltqlen);
		return;
	}
	icbp->icb_rqstaddr[RQRSP_ADDR0015] = DMA_WD0(isp->isp_rquest_dma);
	icbp->icb_rqstaddr[RQRSP_ADDR1631] = DMA_WD1(isp->isp_rquest_dma);
	icbp->icb_rqstaddr[RQRSP_ADDR3247] = DMA_WD2(isp->isp_rquest_dma);
	icbp->icb_rqstaddr[RQRSP_ADDR4863] = DMA_WD3(isp->isp_rquest_dma);

	icbp->icb_respaddr[RQRSP_ADDR0015] = DMA_WD0(isp->isp_result_dma);
	icbp->icb_respaddr[RQRSP_ADDR1631] = DMA_WD1(isp->isp_result_dma);
	icbp->icb_respaddr[RQRSP_ADDR3247] = DMA_WD2(isp->isp_result_dma);
	icbp->icb_respaddr[RQRSP_ADDR4863] = DMA_WD3(isp->isp_result_dma);

#ifdef	ISP_TARGET_MODE
	if (isp->isp_role & ISP_ROLE_TARGET) {
		icbp->icb_atioqlen = RESULT_QUEUE_LEN(isp);
		if (icbp->icb_atioqlen < 8) {
			isp_prt(isp, ISP_LOGERR, "bad ATIO queue length %d",
			    icbp->icb_atioqlen);
			return;
		}
		icbp->icb_atioqaddr[RQRSP_ADDR0015] =
		    DMA_WD0(isp->isp_atioq_dma);
		icbp->icb_atioqaddr[RQRSP_ADDR1631] =
		    DMA_WD1(isp->isp_atioq_dma);
		icbp->icb_atioqaddr[RQRSP_ADDR3247] =
		    DMA_WD2(isp->isp_atioq_dma);
		icbp->icb_atioqaddr[RQRSP_ADDR4863] =
		    DMA_WD3(isp->isp_atioq_dma);
		isp_prt(isp, ISP_LOGDEBUG0,
		    "isp_fibre_init_2400: atioq %04x%04x%04x%04x",
		    DMA_WD3(isp->isp_atioq_dma), DMA_WD2(isp->isp_atioq_dma),
		    DMA_WD1(isp->isp_atioq_dma), DMA_WD0(isp->isp_atioq_dma));
	}
#endif

	isp_prt(isp, ISP_LOGDEBUG0,
	    "isp_fibre_init_2400: fwopt1 0x%x fwopt2 0x%x fwopt3 0x%x",
	    icbp->icb_fwoptions1, icbp->icb_fwoptions2, icbp->icb_fwoptions3);

	isp_prt(isp, ISP_LOGDEBUG0,
	    "isp_fibre_init_2400: rqst %04x%04x%04x%04x rsp %04x%04x%04x%04x",
	    DMA_WD3(isp->isp_rquest_dma), DMA_WD2(isp->isp_rquest_dma),
	    DMA_WD1(isp->isp_rquest_dma), DMA_WD0(isp->isp_rquest_dma),
	    DMA_WD3(isp->isp_result_dma), DMA_WD2(isp->isp_result_dma),
	    DMA_WD1(isp->isp_result_dma), DMA_WD0(isp->isp_result_dma));

	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "isp_fibre_init_2400", sizeof (*icbp),
		    icbp);
	}
	FC_SCRATCH_ACQUIRE(isp);
	isp_put_icb_2400(isp, icbp, fcp->isp_scratch);


	/*
	 * Init the firmware
	 */
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_INIT_FIRMWARE;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	mbs.logval = MBLOGALL;
	mbs.timeout = 30 * 1000000;
	isp_prt(isp, ISP_LOGDEBUG0, "INIT F/W from %04x%04x%04x%04x",
	    DMA_WD3(fcp->isp_scdma), DMA_WD2(fcp->isp_scdma),
	    DMA_WD1(fcp->isp_scdma), DMA_WD0(fcp->isp_scdma));
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, sizeof (*icbp));
	isp_mboxcmd(isp, &mbs);
	FC_SCRATCH_RELEASE(isp);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}
	isp->isp_reqidx = 0;
	isp->isp_reqodx = 0;
	isp->isp_residx = 0;

	/*
	 * Whatever happens, we're now committed to being here.
	 */
	isp->isp_state = ISP_INITSTATE;
}

static void
isp_mark_portdb(ispsoftc_t *isp, int onprobation)
{
	fcparam *fcp = (fcparam *) isp->isp_param;
	int i;

	for (i = 0; i < MAX_FC_TARG; i++) {
		if (onprobation == 0) {
			MEMZERO(&fcp->portdb[i], sizeof (fcportdb_t));
		} else {
			switch (fcp->portdb[i].state) {
			case FC_PORTDB_STATE_CHANGED:
			case FC_PORTDB_STATE_PENDING_VALID:
			case FC_PORTDB_STATE_VALID:
			case FC_PORTDB_STATE_PROBATIONAL:
				fcp->portdb[i].state =
					FC_PORTDB_STATE_PROBATIONAL;
				break;
			case FC_PORTDB_STATE_ZOMBIE:
				break;
			case FC_PORTDB_STATE_NIL:
			default:
				MEMZERO(&fcp->portdb[i], sizeof (fcportdb_t));
				fcp->portdb[i].state =
					FC_PORTDB_STATE_NIL;
				break;
			}
		}
	}
}

/*
 * Perform an IOCB PLOGI or LOGO via EXECUTE IOCB A64 for 24XX cards
 * or via FABRIC LOGIN/FABRIC LOGOUT for other cards.
 */
static int
isp_plogx(ispsoftc_t *isp, uint16_t handle, uint32_t portid, int flags, int gs)
{
	mbreg_t mbs;
	uint8_t q[QENTRY_LEN];
	isp_plogx_t *plp;
	uint8_t *scp;
	uint32_t sst, parm1;
	int rval;

	if (!IS_24XX(isp)) {
		int action = flags & PLOGX_FLG_CMD_MASK;
		if (action == PLOGX_FLG_CMD_PLOGI) {
			return (isp_port_login(isp, handle, portid));
		} else if (action == PLOGX_FLG_CMD_LOGO) {
			return (isp_port_logout(isp, handle, portid));
		} else {
			return (MBOX_INVALID_COMMAND);
		}
	}

	MEMZERO(q, QENTRY_LEN);
	plp = (isp_plogx_t *) q;
	plp->plogx_header.rqs_entry_count = 1;
	plp->plogx_header.rqs_entry_type = RQSTYPE_LOGIN;
	plp->plogx_handle = 0xffffffff;
	plp->plogx_nphdl = handle;
	plp->plogx_portlo = portid;
	plp->plogx_rspsz_porthi = (portid >> 16) & 0xff;
	plp->plogx_flags = flags;

	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "IOCB LOGX", QENTRY_LEN, plp);
	}

	if (gs == 0) {
		FC_SCRATCH_ACQUIRE(isp);
	}
	scp = FCPARAM(isp)->isp_scratch;
	isp_put_plogx(isp, plp, (isp_plogx_t *) scp);


	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_EXEC_COMMAND_IOCB_A64;
	mbs.param[1] = QENTRY_LEN;
	mbs.param[2] = DMA_WD1(FCPARAM(isp)->isp_scdma);
	mbs.param[3] = DMA_WD0(FCPARAM(isp)->isp_scdma);
	mbs.param[6] = DMA_WD3(FCPARAM(isp)->isp_scdma);
	mbs.param[7] = DMA_WD2(FCPARAM(isp)->isp_scdma);
	mbs.timeout = 500000;
	mbs.logval = MBLOGALL;
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, QENTRY_LEN);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		rval = mbs.param[0];
		goto out;
	}
	MEMORYBARRIER(isp, SYNC_SFORCPU, QENTRY_LEN, QENTRY_LEN);
	scp += QENTRY_LEN;
	isp_get_plogx(isp, (isp_plogx_t *) scp, plp);
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "IOCB LOGX response", QENTRY_LEN, plp);
	}

	if (plp->plogx_status == PLOGX_STATUS_OK) {
		rval = 0;
		goto out;
	} else if (plp->plogx_status != PLOGX_STATUS_IOCBERR) {
		isp_prt(isp, ISP_LOGWARN, "status 0x%x on port login IOCB",
		    plp->plogx_status);
		rval = -1;
		goto out;
	}

	sst = plp->plogx_ioparm[0].lo16 | (plp->plogx_ioparm[0].hi16 << 16);
	parm1 = plp->plogx_ioparm[1].lo16 | (plp->plogx_ioparm[1].hi16 << 16);

	rval = -1;

	switch (sst) {
	case PLOGX_IOCBERR_NOLINK:
		isp_prt(isp, ISP_LOGERR, "PLOGX failed- no link");
		break;
	case PLOGX_IOCBERR_NOIOCB:
		isp_prt(isp, ISP_LOGERR, "PLOGX failed- no IOCB buffer");
		break;
	case PLOGX_IOCBERR_NOXGHG:
		isp_prt(isp, ISP_LOGERR,
		    "PLOGX failed- no Exchange Control Block");
		break;
	case PLOGX_IOCBERR_FAILED:
		isp_prt(isp, ISP_LOGERR,
		    "PLOGX(0x%x) of Port 0x%06x failed: reason 0x%x (last LOGIN"
		    " state 0x%x)", flags, portid, parm1 & 0xff,
		    (parm1 >> 8) & 0xff);
		break;
	case PLOGX_IOCBERR_NOFABRIC:
		isp_prt(isp, ISP_LOGERR, "PLOGX failed- no fabric");
		break;
	case PLOGX_IOCBERR_NOTREADY:
		isp_prt(isp, ISP_LOGERR, "PLOGX failed- f/w not ready");
		break;
	case PLOGX_IOCBERR_NOLOGIN:
		isp_prt(isp, ISP_LOGERR,
		    "PLOGX failed- not logged in (last LOGIN state 0x%x)",
		    parm1);
		rval = MBOX_NOT_LOGGED_IN;
		break;
	case PLOGX_IOCBERR_REJECT:
		isp_prt(isp, ISP_LOGERR, "PLOGX failed: LS_RJT = 0x%x", parm1);
		break;
	case PLOGX_IOCBERR_NOPCB:
		isp_prt(isp, ISP_LOGERR, "PLOGX failed- no PCB allocated");
		break;
	case PLOGX_IOCBERR_EINVAL:
		isp_prt(isp, ISP_LOGERR,
		    "PLOGX failed: invalid parameter at offset 0x%x", parm1);
		break;
	case PLOGX_IOCBERR_PORTUSED:
		isp_prt(isp, ISP_LOGDEBUG0,
		    "portid 0x%x already logged in with N-port handle 0x%x",
		    portid, parm1);
		rval = MBOX_PORT_ID_USED | (handle << 16);
		break;
	case PLOGX_IOCBERR_HNDLUSED:
		isp_prt(isp, ISP_LOGDEBUG0,
		    "N-port handle 0x%x already used for portid 0x%x",
		    handle, parm1);
		rval = MBOX_LOOP_ID_USED;
		break;
	case PLOGX_IOCBERR_NOHANDLE:
		isp_prt(isp, ISP_LOGERR, "PLOGX failed- no handle allocated");
		break;
	case PLOGX_IOCBERR_NOFLOGI:
		isp_prt(isp, ISP_LOGERR, "PLOGX failed- no FLOGI_ACC");
		break;
	default:
		isp_prt(isp, ISP_LOGERR, "status %x from %x", plp->plogx_status,
		    flags);
		rval = -1;
		break;
	}
out:
	if (gs == 0) {
		FC_SCRATCH_RELEASE(isp);
	}
	return (rval);
}

static int
isp_port_login(ispsoftc_t *isp, uint16_t handle, uint32_t portid)
{
	mbreg_t mbs;

	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_FABRIC_LOGIN;
	if (FCPARAM(isp)->isp_2klogin) {
		mbs.param[1] = handle;
		mbs.ibits = (1 << 10);
	} else {
		mbs.param[1] = handle << 8;
	}
	mbs.param[2] = portid >> 16;
	mbs.param[3] = portid;
	mbs.logval = MBLOGNONE;
	mbs.timeout = 500000;
	isp_mboxcmd(isp, &mbs);

	switch (mbs.param[0]) {
	case MBOX_PORT_ID_USED:
		isp_prt(isp, ISP_LOGDEBUG0,
		    "isp_plogi_old: portid 0x%06x already logged in as %u",
		    portid, mbs.param[1]);
		return (MBOX_PORT_ID_USED | (mbs.param[1] << 16));

	case MBOX_LOOP_ID_USED:
		isp_prt(isp, ISP_LOGDEBUG0,
		    "isp_plogi_old: handle %u in use for port id 0x%02xXXXX",
		    handle, mbs.param[1] & 0xff);
		return (MBOX_LOOP_ID_USED);

	case MBOX_COMMAND_COMPLETE:
		return (0);

	case MBOX_COMMAND_ERROR:
		isp_prt(isp, ISP_LOGINFO,
		    "isp_plogi_old: error 0x%x in PLOGI to port 0x%06x",
		    mbs.param[1], portid);
		return (MBOX_COMMAND_ERROR);

	case MBOX_ALL_IDS_USED:
		isp_prt(isp, ISP_LOGINFO,
		    "isp_plogi_old: all IDs used for fabric login");
		return (MBOX_ALL_IDS_USED);

	default:
		isp_prt(isp, ISP_LOGINFO,
		    "isp_plogi_old: error 0x%x on port login of 0x%06x@0x%0x",
		    mbs.param[0], portid, handle);
		return (mbs.param[0]);
	}
}

static int
isp_port_logout(ispsoftc_t *isp, uint16_t handle, uint32_t portid)
{
	mbreg_t mbs;

	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_FABRIC_LOGOUT;
	if (FCPARAM(isp)->isp_2klogin) {
		mbs.param[1] = handle;
		mbs.ibits = (1 << 10);
	} else {
		mbs.param[1] = handle << 8;
	}
	mbs.logval = MBLOGNONE;
	mbs.timeout = 100000;
	isp_mboxcmd(isp, &mbs);
	return (mbs.param[0] == MBOX_COMMAND_COMPLETE? 0 : mbs.param[0]);
}

static int
isp_getpdb(ispsoftc_t *isp, uint16_t id, isp_pdb_t *pdb, int dolock)
{
	fcparam *fcp = (fcparam *) isp->isp_param;
	mbreg_t mbs;
	union {
		isp_pdb_21xx_t fred;
		isp_pdb_24xx_t bill;
	} un;

	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_GET_PORT_DB;
	if (IS_24XX(isp)) {
		mbs.ibits = 0x3ff;
		mbs.param[1] = id;
	} else if (FCPARAM(isp)->isp_2klogin) {
		mbs.param[1] = id;
		mbs.ibits = (1 << 10);
	} else {
		mbs.param[1] = id << 8;
	}
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	mbs.timeout = 250000;
	mbs.logval = MBLOGALL & ~MBOX_COMMAND_PARAM_ERROR;
	if (dolock) {
		FC_SCRATCH_ACQUIRE(isp);
	}
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, sizeof (un));
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		if (dolock) {
			FC_SCRATCH_RELEASE(isp);
		}
		return (-1);
	}
	if (IS_24XX(isp)) {
		isp_get_pdb_24xx(isp, fcp->isp_scratch, &un.bill);
		pdb->handle = un.bill.pdb_handle;
		pdb->s3_role = un.bill.pdb_prli_svc3;
		pdb->portid = BITS2WORD_24XX(un.bill.pdb_portid_bits);
		MEMCPY(pdb->portname, un.bill.pdb_portname, 8);
		MEMCPY(pdb->nodename, un.bill.pdb_nodename, 8);
	} else {
		isp_get_pdb_21xx(isp, fcp->isp_scratch, &un.fred);
		pdb->handle = un.fred.pdb_loopid;
		pdb->s3_role = un.fred.pdb_prli_svc3;
		pdb->portid = BITS2WORD(un.fred.pdb_portid_bits);
		MEMCPY(pdb->portname, un.fred.pdb_portname, 8);
		MEMCPY(pdb->nodename, un.fred.pdb_nodename, 8);
	}
	if (dolock) {
		FC_SCRATCH_RELEASE(isp);
	}
	return (0);
}

static uint64_t
isp_get_portname(ispsoftc_t *isp, int loopid, int nodename)
{
	uint64_t wwn = (uint64_t) -1;
	mbreg_t mbs;

	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_GET_PORT_NAME;
	if (FCPARAM(isp)->isp_2klogin || IS_24XX(isp)) {
		mbs.param[1] = loopid;
		mbs.ibits = (1 << 10);
		if (nodename) {
			mbs.param[10] = 1;
		}
	} else {
		mbs.param[1] = loopid << 8;
		if (nodename) {
			mbs.param[1] |= 1;
		}
	}
	mbs.logval = MBLOGALL & ~MBOX_COMMAND_PARAM_ERROR;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return (wwn);
	}
	if (IS_24XX(isp)) {
		wwn =
		    (((uint64_t)(mbs.param[2] >> 8))  << 56) |
		    (((uint64_t)(mbs.param[2] & 0xff))	<< 48) |
		    (((uint64_t)(mbs.param[3] >> 8))	<< 40) |
		    (((uint64_t)(mbs.param[3] & 0xff))	<< 32) |
		    (((uint64_t)(mbs.param[6] >> 8))	<< 24) |
		    (((uint64_t)(mbs.param[6] & 0xff))	<< 16) |
		    (((uint64_t)(mbs.param[7] >> 8))	<<  8) |
		    (((uint64_t)(mbs.param[7] & 0xff)));
	} else {
		wwn =
		    (((uint64_t)(mbs.param[2] & 0xff))  << 56) |
		    (((uint64_t)(mbs.param[2] >> 8))	<< 48) |
		    (((uint64_t)(mbs.param[3] & 0xff))	<< 40) |
		    (((uint64_t)(mbs.param[3] >> 8))	<< 32) |
		    (((uint64_t)(mbs.param[6] & 0xff))	<< 24) |
		    (((uint64_t)(mbs.param[6] >> 8))	<< 16) |
		    (((uint64_t)(mbs.param[7] & 0xff))	<<  8) |
		    (((uint64_t)(mbs.param[7] >> 8)));
	}
	return (wwn);
}

/*
 * Make sure we have good FC link.
 */

static int
isp_fclink_test(ispsoftc_t *isp, int usdelay)
{
	static const char *toponames[] = {
		"Private Loop",
		"FL Port",
		"N-Port to N-Port",
		"F Port",
		"F Port (no FLOGI_ACC response)"
	};
	mbreg_t mbs;
	int count, check_for_fabric;
	uint8_t lwfs;
	int loopid;
	fcparam *fcp;
	fcportdb_t *lp;
	isp_pdb_t pdb;

	fcp = isp->isp_param;

	isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0, "FC Link Test Entry");
	ISP_MARK_PORTDB(isp, 1);

	/*
	 * Wait up to N microseconds for F/W to go to a ready state.
	 */
	lwfs = FW_CONFIG_WAIT;
	count = 0;
	while (count < usdelay) {
		uint64_t enano;
		uint32_t wrk;
		NANOTIME_T hra, hrb;

		GET_NANOTIME(&hra);
		isp_fw_state(isp);
		if (lwfs != fcp->isp_fwstate) {
			isp_prt(isp, ISP_LOGCONFIG|ISP_LOGSANCFG,
			    "Firmware State <%s->%s>",
			    ispfc_fw_statename((int)lwfs),
			    ispfc_fw_statename((int)fcp->isp_fwstate));
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
		    (uint32_t)(enano >> 32), (uint32_t)(enano & 0xffffffff));

		/*
		 * If the elapsed time is less than 1 millisecond,
		 * delay a period of time up to that millisecond of
		 * waiting.
		 *
		 * This peculiar code is an attempt to try and avoid
		 * invoking uint64_t math support functions for some
		 * platforms where linkage is a problem.
		 */
		if (enano < (1000 * 1000)) {
			count += 1000;
			enano = (1000 * 1000) - enano;
			while (enano > (uint64_t) 4000000000U) {
				USEC_SLEEP(isp, 4000000);
				enano -= (uint64_t) 4000000000U;
			}
			wrk = enano;
			wrk /= 1000;
			USEC_SLEEP(isp, wrk);
		} else {
			while (enano > (uint64_t) 4000000000U) {
				count += 4000000;
				enano -= (uint64_t) 4000000000U;
			}
			wrk = enano;
			count += (wrk / 1000);
		}
	}

	/*
	 * If we haven't gone to 'ready' state, return.
	 */
	if (fcp->isp_fwstate != FW_READY) {
		isp_prt(isp, ISP_LOGSANCFG,
		    "isp_fclink_test: not at FW_READY state");
		return (-1);
	}

	/*
	 * Get our Loop ID and Port ID.
	 */
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_GET_LOOP_ID;
	mbs.logval = MBLOGALL;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return (-1);
	}

	if (FCPARAM(isp)->isp_2klogin) {
		fcp->isp_loopid = mbs.param[1];
	} else {
		fcp->isp_loopid = mbs.param[1] & 0xff;
	}

	if (IS_2100(isp)) {
		fcp->isp_topo = TOPO_NL_PORT;
	} else {
		int topo = (int) mbs.param[6];
		if (topo < TOPO_NL_PORT || topo > TOPO_PTP_STUB) {
			topo = TOPO_PTP_STUB;
		}
		fcp->isp_topo = topo;
	}
	fcp->isp_portid = mbs.param[2] | (mbs.param[3] << 16);

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
	} else {
		check_for_fabric = 0;
	}

	if (IS_24XX(isp)) {
		loopid = NPH_FL_ID;
	} else {
		loopid = FL_ID;
	}

	if (check_for_fabric && isp_getpdb(isp, loopid, &pdb, 1) == 0) {
		int r;
		if (IS_2100(isp)) {
			fcp->isp_topo = TOPO_FL_PORT;
		}
		if (pdb.portid == 0) {
			/*
			 * Crock.
			 */
			fcp->isp_topo = TOPO_NL_PORT;
			goto not_on_fabric;
		}

		/*
		 * Save the Fabric controller's port database entry.
		 */
		lp = &fcp->portdb[FL_ID];
		lp->state = FC_PORTDB_STATE_PENDING_VALID;
		MAKE_WWN_FROM_NODE_NAME(lp->node_wwn, pdb.nodename);
		MAKE_WWN_FROM_NODE_NAME(lp->port_wwn, pdb.portname);
		lp->roles = (pdb.s3_role & SVC3_ROLE_MASK) >> SVC3_ROLE_SHIFT;
		lp->portid = pdb.portid;
		lp->handle = pdb.handle;
		lp->new_portid = lp->portid;
		lp->new_roles = lp->roles;
		if (IS_24XX(isp)) {
			r = isp_register_fc4_type_24xx(isp);
		} else {
			r = isp_register_fc4_type(isp);
		}
		if (r) {
			isp_prt(isp, ISP_LOGSANCFG,
			    "isp_fclink_test: register fc4 type failed");
			return (-1);
		}
	} else {
not_on_fabric:
		fcp->portdb[FL_ID].state = FC_PORTDB_STATE_NIL;
	}

	fcp->isp_gbspeed = 1;
	if (IS_23XX(isp) || IS_24XX(isp)) {
		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_GET_SET_DATA_RATE;
		mbs.param[1] = MBGSD_GET_RATE;
		/* mbs.param[2] undefined if we're just getting rate */
		mbs.logval = MBLOGALL;
		mbs.timeout = 3000000;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			if (mbs.param[1] == MBGSD_FOURGB) {
				isp_prt(isp, ISP_LOGINFO, "4Gb link speed/s");
				fcp->isp_gbspeed = 4;
			} if (mbs.param[1] == MBGSD_TWOGB) {
				isp_prt(isp, ISP_LOGINFO, "2Gb link speed/s");
				fcp->isp_gbspeed = 2;
			}
		}
	}

	/*
	 * Announce ourselves, too.
	 */
	isp_prt(isp, ISP_LOGSANCFG|ISP_LOGCONFIG, topology, fcp->isp_portid,
	    fcp->isp_loopid, toponames[fcp->isp_topo]);
	isp_prt(isp, ISP_LOGSANCFG|ISP_LOGCONFIG, ourwwn,
	    (uint32_t) (ISP_NODEWWN(isp) >> 32),
	    (uint32_t) ISP_NODEWWN(isp),
	    (uint32_t) (ISP_PORTWWN(isp) >> 32),
	    (uint32_t) ISP_PORTWWN(isp));
	isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0, "FC Link Test Complete");
	return (0);
}

static const char *
ispfc_fw_statename(int state)
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
 * Complete the synchronization of our Port Database.
 *
 * At this point, we've scanned the local loop (if any) and the fabric
 * and performed fabric logins on all new devices.
 *
 * Our task here is to go through our port database and remove any entities
 * that are still marked probational (issuing PLOGO for ones which we had
 * PLOGI'd into) or are dead.
 *
 * Our task here is to also check policy to decide whether devices which
 * have *changed* in some way should still be kept active. For example,
 * if a device has just changed PortID, we can either elect to treat it
 * as an old device or as a newly arrived device (and notify the outer
 * layer appropriately).
 *
 * We also do initiator map target id assignment here for new initiator
 * devices and refresh old ones ot make sure that they point to the corret
 * entities.
 */
static int
isp_pdb_sync(ispsoftc_t *isp)
{
	fcparam *fcp = isp->isp_param;
	fcportdb_t *lp;
	uint16_t dbidx;

	if (fcp->isp_loopstate == LOOP_READY) {
		return (0);
	}

	/*
	 * Make sure we're okay for doing this right now.
	 */
	if (fcp->isp_loopstate != LOOP_PDB_RCVD &&
	    fcp->isp_loopstate != LOOP_FSCAN_DONE &&
	    fcp->isp_loopstate != LOOP_LSCAN_DONE) {
		isp_prt(isp, ISP_LOGWARN, "isp_pdb_sync: bad loopstate %d",
		    fcp->isp_loopstate);
		return (-1);
	}

	if (fcp->isp_topo == TOPO_FL_PORT ||
	    fcp->isp_topo == TOPO_NL_PORT ||
	    fcp->isp_topo == TOPO_N_PORT) {
		if (fcp->isp_loopstate < LOOP_LSCAN_DONE) {
			if (isp_scan_loop(isp) != 0) {
				isp_prt(isp, ISP_LOGWARN,
				    "isp_pdb_sync: isp_scan_loop failed");
				return (-1);
			}
		}
	}

	if (fcp->isp_topo == TOPO_F_PORT || fcp->isp_topo == TOPO_FL_PORT) {
		if (fcp->isp_loopstate < LOOP_FSCAN_DONE) {
			if (isp_scan_fabric(isp) != 0) {
				isp_prt(isp, ISP_LOGWARN,
				    "isp_pdb_sync: isp_scan_fabric failed");
				return (-1);
			}
		}
	}

	isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0, "Synchronizing PDBs");

	fcp->isp_loopstate = LOOP_SYNCING_PDB;

	for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
		lp = &fcp->portdb[dbidx];

		if (lp->state == FC_PORTDB_STATE_NIL) {
			continue;
		}

		if (lp->state == FC_PORTDB_STATE_VALID) {
			if (dbidx != FL_ID) {
				isp_prt(isp,
				    ISP_LOGERR, "portdb idx %d already valid",
			    	    dbidx);
			}
			continue;
		}

		switch (lp->state) {
		case FC_PORTDB_STATE_PROBATIONAL:
		case FC_PORTDB_STATE_DEAD:
			/*
			 * It's up to the outer layers to clear isp_ini_map.
			 */
			lp->state = FC_PORTDB_STATE_NIL;
			isp_async(isp, ISPASYNC_DEV_GONE, lp);
			if (lp->autologin == 0) {
				(void) isp_plogx(isp, lp->handle, lp->portid,
				    PLOGX_FLG_CMD_LOGO |
				    PLOGX_FLG_IMPLICIT |
				    PLOGX_FLG_FREE_NPHDL, 0);
			} else {
				lp->autologin = 0;
			}
			lp->new_roles = 0;
			lp->new_portid = 0;
			/*
			 * Note that we might come out of this with our state
			 * set to FC_PORTDB_STATE_ZOMBIE.
			 */
			break;
		case FC_PORTDB_STATE_NEW:
			/*
			 * It's up to the outer layers to assign a virtual
			 * target id in isp_ini_map (if any).
			 */
			lp->portid = lp->new_portid;
			lp->roles = lp->new_roles;
			lp->state = FC_PORTDB_STATE_VALID;
			isp_async(isp, ISPASYNC_DEV_ARRIVED, lp);
			lp->new_roles = 0;
			lp->new_portid = 0;
			lp->reserved = 0;
			lp->new_reserved = 0;
			break;
		case FC_PORTDB_STATE_CHANGED:
/*
 * XXXX FIX THIS
 */
			lp->state = FC_PORTDB_STATE_VALID;
			isp_async(isp, ISPASYNC_DEV_CHANGED, lp);
			lp->new_roles = 0;
			lp->new_portid = 0;
			lp->reserved = 0;
			lp->new_reserved = 0;
			break;
		case FC_PORTDB_STATE_PENDING_VALID:
			lp->portid = lp->new_portid;
			lp->roles = lp->new_roles;
			if (lp->ini_map_idx) {
				int t = lp->ini_map_idx - 1;
				fcp->isp_ini_map[t] = dbidx + 1;
			}
			lp->state = FC_PORTDB_STATE_VALID;
			isp_async(isp, ISPASYNC_DEV_STAYED, lp);
			if (dbidx != FL_ID) {
				lp->new_roles = 0;
				lp->new_portid = 0;
			}
			lp->reserved = 0;
			lp->new_reserved = 0;
			break;
		case FC_PORTDB_STATE_ZOMBIE:
			break;
		default:
			isp_prt(isp, ISP_LOGWARN,
			    "isp_scan_loop: state %d for idx %d",
			    lp->state, dbidx);
			isp_dump_portdb(isp);
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

/*
 * Scan local loop for devices.
 */
static int
isp_scan_loop(ispsoftc_t *isp)
{
	fcportdb_t *lp, tmp;
	fcparam *fcp = isp->isp_param;
	int i;
	isp_pdb_t pdb;
	uint16_t handle, lim = 0;

	if (fcp->isp_fwstate < FW_READY ||
	    fcp->isp_loopstate < LOOP_PDB_RCVD) {
		return (-1);
	}

	if (fcp->isp_loopstate > LOOP_SCANNING_LOOP) {
		return (0);
	}

	/*
	 * Check our connection topology.
	 *
	 * If we're a public or private loop, we scan 0..125 as handle values.
	 * The firmware has (typically) peformed a PLOGI for us.
	 *
	 * If we're a N-port connection, we treat this is a short loop (0..1).
	 *
	 * If we're in target mode, we can all possible handles to see who
	 * might have logged into us.
	 */
	switch (fcp->isp_topo) {
	case TOPO_NL_PORT:
	case TOPO_FL_PORT:
		lim = LOCAL_LOOP_LIM;
		break;
	case TOPO_N_PORT:
		lim = 2;
		break;
	default:
		isp_prt(isp, ISP_LOGDEBUG0, "no loop topology to scan");
		fcp->isp_loopstate = LOOP_LSCAN_DONE;
		return (0);
	}

	fcp->isp_loopstate = LOOP_SCANNING_LOOP;

	isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0, "FC scan loop 0..%d", lim-1);


	/*
	 * Run through the list and get the port database info for each one.
	 */
	for (handle = 0; handle < lim; handle++) {
		/*
		 * But don't even try for ourselves...
	 	 */
		if (handle == fcp->isp_loopid) {
			continue;
		}

		/*
		 * In older cards with older f/w GET_PORT_DATABASE has been
		 * known to hang. This trick gets around that problem.
		 */
		if (IS_2100(isp) || IS_2200(isp)) {
			uint64_t node_wwn = isp_get_portname(isp, handle, 1);
			if (fcp->isp_loopstate < LOOP_SCANNING_LOOP) {
				return (-1);
			}
			if (node_wwn == 0) {
				continue;
			}
		}

		/*
		 * Get the port database entity for this index.
		 */
		if (isp_getpdb(isp, handle, &pdb, 1) != 0) {
			if (fcp->isp_loopstate < LOOP_SCANNING_LOOP) {
				ISP_MARK_PORTDB(isp, 1);
				return (-1);
			}
			continue;
		}

		if (fcp->isp_loopstate < LOOP_SCANNING_LOOP) {
			ISP_MARK_PORTDB(isp, 1);
			return (-1);
		}

		/*
		 * On *very* old 2100 firmware we would end up sometimes
		 * with the firmware returning the port database entry
		 * for something else. We used to restart this, but
		 * now we just punt.
		 */
		if (IS_2100(isp) && pdb.handle != handle) {
			isp_prt(isp, ISP_LOGWARN,
			    "giving up on synchronizing the port database");
			ISP_MARK_PORTDB(isp, 1);
			return (-1);
		}

		/*
		 * Save the pertinent info locally.
		 */
		MAKE_WWN_FROM_NODE_NAME(tmp.node_wwn, pdb.nodename);
		MAKE_WWN_FROM_NODE_NAME(tmp.port_wwn, pdb.portname);
		tmp.roles = (pdb.s3_role & SVC3_ROLE_MASK) >> SVC3_ROLE_SHIFT;
		tmp.portid = pdb.portid;
		tmp.handle = pdb.handle;

		/*
		 * Check to make sure it's still a valid entry. The 24XX seems
		 * to return a portid but not a WWPN/WWNN or role for devices
		 * which shift on a loop.
		 */
		if (tmp.node_wwn == 0 || tmp.port_wwn == 0 || tmp.portid == 0) {
			int a, b, c;
			a = (tmp.node_wwn == 0);
			b = (tmp.port_wwn == 0);
			c = (tmp.portid == 0);
			isp_prt(isp, ISP_LOGWARN,
			    "bad pdb (%1d%1d%1d) @ handle 0x%x", a, b, c,
			    handle);
			isp_dump_portdb(isp);
			continue;
		}

		/*
		 * Now search the entire port database
		 * for the same Port and Node WWN.
		 */
		for (i = 0; i < MAX_FC_TARG; i++) {
			lp = &fcp->portdb[i];
			if (lp->state == FC_PORTDB_STATE_NIL) {
				continue;
			}
			if (lp->node_wwn != tmp.node_wwn) {
				continue;
			}
			if (lp->port_wwn != tmp.port_wwn) {
				continue;
			}

			/*
			 * Okay- we've found a non-nil entry that matches.
			 * Check to make sure it's probational or a zombie.
			 */
			if (lp->state != FC_PORTDB_STATE_PROBATIONAL &&
			    lp->state != FC_PORTDB_STATE_ZOMBIE) {
				isp_prt(isp, ISP_LOGERR,
				    "[%d] not probational/zombie (0x%x)",
				    i, lp->state);
				isp_dump_portdb(isp);
				ISP_MARK_PORTDB(isp, 1);
				return (-1);
			}

			/*
			 * Mark the device as something the f/w logs into
			 * automatically.
			 */
			lp->autologin = 1;

			/*
			 * Check to make see if really still the same
			 * device. If it is, we mark it pending valid.
			 */
			if (lp->portid == tmp.portid &&
			    lp->handle == tmp.handle &&
			    lp->roles == tmp.roles) {
				lp->new_portid = tmp.portid;
				lp->new_roles = tmp.roles;
				lp->state = FC_PORTDB_STATE_PENDING_VALID;
				isp_prt(isp, ISP_LOGSANCFG,
				    "Loop Port 0x%02x@0x%x Pending Valid",
				    tmp.portid, tmp.handle);
				break;
			}
		
			/*
			 * We can wipe out the old handle value
			 * here because it's no longer valid.
			 */
			lp->handle = tmp.handle;

			/*
			 * Claim that this has changed and let somebody else
			 * decide what to do.
			 */
			isp_prt(isp, ISP_LOGSANCFG,
			    "Loop Port 0x%02x@0x%x changed",
			    tmp.portid, tmp.handle);
			lp->state = FC_PORTDB_STATE_CHANGED;
			lp->new_portid = tmp.portid;
			lp->new_roles = tmp.roles;
			break;
		}

		/*
		 * Did we find and update an old entry?
		 */
		if (i < MAX_FC_TARG) {
			continue;
		}

		/*
		 * Ah. A new device entry. Find an empty slot
		 * for it and save info for later disposition.
		 */
		for (i = 0; i < MAX_FC_TARG; i++) {
			if (fcp->portdb[i].state == FC_PORTDB_STATE_NIL) {
				break;
			}
		}
		if (i == MAX_FC_TARG) {
			isp_prt(isp, ISP_LOGERR, "out of portdb entries");
			continue;
		}
		lp = &fcp->portdb[i];

		MEMZERO(lp, sizeof (fcportdb_t));
		lp->autologin = 1;
		lp->state = FC_PORTDB_STATE_NEW;
		lp->new_portid = tmp.portid;
		lp->new_roles = tmp.roles;
		lp->handle = tmp.handle;
		lp->port_wwn = tmp.port_wwn;
		lp->node_wwn = tmp.node_wwn;
		isp_prt(isp, ISP_LOGSANCFG,
		    "Loop Port 0x%02x@0x%x is New Entry",
		    tmp.portid, tmp.handle);
	}
	fcp->isp_loopstate = LOOP_LSCAN_DONE;
	return (0);
}

/*
 * Scan the fabric for devices and add them to our port database.
 *
 * Use the GID_FT command to get all Port IDs for FC4 SCSI devices it knows.
 *
 * For 2100-23XX cards, we can use the SNS mailbox command to pass simple
 * name server commands to the switch management server via the QLogic f/w.
 *
 * For the 24XX card, we have to use CT-Pass through run via the Execute IOCB
 * mailbox command.
 *
 * The net result is to leave the list of Port IDs setting untranslated in
 * offset IGPOFF of the FC scratch area, whereupon we'll canonicalize it to
 * host order at OGPOFF.
 */

/*
 * Take less than half of our scratch area to store Port IDs 
 */
#define	GIDLEN	((ISP2100_SCRLEN >> 1) - 16 - SNS_GID_FT_REQ_SIZE)
#define	NGENT	((GIDLEN - 16) >> 2)

#define	IGPOFF	(2 * QENTRY_LEN)
#define	OGPOFF	(ISP2100_SCRLEN >> 1)
#define	ZTXOFF	(ISP2100_SCRLEN - (1 * QENTRY_LEN))
#define	CTXOFF	(ISP2100_SCRLEN - (2 * QENTRY_LEN))
#define	XTXOFF	(ISP2100_SCRLEN - (3 * QENTRY_LEN))

static int
isp_gid_ft_sns(ispsoftc_t *isp)
{
	union {
		sns_gid_ft_req_t _x;
		uint8_t _y[SNS_GID_FT_REQ_SIZE];
	} un;
	fcparam *fcp = FCPARAM(isp);
	sns_gid_ft_req_t *rq = &un._x;
	mbreg_t mbs;

	isp_prt(isp, ISP_LOGDEBUG0, "scanning fabric (GID_FT) via SNS");

	MEMZERO(rq, SNS_GID_FT_REQ_SIZE);
	rq->snscb_rblen = GIDLEN >> 1;
	rq->snscb_addr[RQRSP_ADDR0015] = DMA_WD0(fcp->isp_scdma + IGPOFF);
	rq->snscb_addr[RQRSP_ADDR1631] = DMA_WD1(fcp->isp_scdma + IGPOFF);
	rq->snscb_addr[RQRSP_ADDR3247] = DMA_WD2(fcp->isp_scdma + IGPOFF);
	rq->snscb_addr[RQRSP_ADDR4863] = DMA_WD3(fcp->isp_scdma + IGPOFF);
	rq->snscb_sblen = 6;
	rq->snscb_cmd = SNS_GID_FT;
	rq->snscb_mword_div_2 = NGENT;
	rq->snscb_fc4_type = FC4_SCSI;

	isp_put_gid_ft_request(isp, rq, fcp->isp_scratch);
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, SNS_GID_FT_REQ_SIZE);

	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_SEND_SNS;
	mbs.param[1] = SNS_GID_FT_REQ_SIZE >> 1;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	mbs.logval = MBLOGALL;
	mbs.timeout = 10000000;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		if (mbs.param[0] == MBOX_INVALID_COMMAND) {
			return (1);
		} else {
			return (-1);
		}
	}
	return (0);
}

static int
isp_gid_ft_ct_passthru(ispsoftc_t *isp)
{
	mbreg_t mbs;
	fcparam *fcp = FCPARAM(isp);
	union {
		isp_ct_pt_t plocal;
		ct_hdr_t clocal;
		uint8_t q[QENTRY_LEN];
	} un;
	isp_ct_pt_t *pt;
	ct_hdr_t *ct;
	uint32_t *rp;
	uint8_t *scp = fcp->isp_scratch;

	isp_prt(isp, ISP_LOGDEBUG0, "scanning fabric (GID_FT) via CT");

	if (!IS_24XX(isp)) {
		return (1);
	}

	/*
	 * Build a Passthrough IOCB in memory.
	 */
	pt = &un.plocal;
	MEMZERO(un.q, QENTRY_LEN);
	pt->ctp_header.rqs_entry_count = 1;
	pt->ctp_header.rqs_entry_type = RQSTYPE_CT_PASSTHRU;
	pt->ctp_handle = 0xffffffff;
	pt->ctp_nphdl = NPH_SNS_ID;
	pt->ctp_cmd_cnt = 1;
	pt->ctp_time = 30;
	pt->ctp_rsp_cnt = 1;
	pt->ctp_rsp_bcnt = GIDLEN;
	pt->ctp_cmd_bcnt = sizeof (*ct) + sizeof (uint32_t);
	pt->ctp_dataseg[0].ds_base = DMA_LO32(fcp->isp_scdma+XTXOFF);
	pt->ctp_dataseg[0].ds_basehi = DMA_HI32(fcp->isp_scdma+XTXOFF);
	pt->ctp_dataseg[0].ds_count = sizeof (*ct) + sizeof (uint32_t);
	pt->ctp_dataseg[1].ds_base = DMA_LO32(fcp->isp_scdma+IGPOFF);
	pt->ctp_dataseg[1].ds_basehi = DMA_HI32(fcp->isp_scdma+IGPOFF);
	pt->ctp_dataseg[1].ds_count = GIDLEN;
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "ct IOCB", QENTRY_LEN, pt);
	}
	isp_put_ct_pt(isp, pt, (isp_ct_pt_t *) &scp[CTXOFF]);

	/*
	 * Build the CT header and command in memory.
	 *
	 * Note that the CT header has to end up as Big Endian format in memory.
	 */
	ct = &un.clocal;
	MEMZERO(ct, sizeof (*ct));
	ct->ct_revision = CT_REVISION;
	ct->ct_fcs_type = CT_FC_TYPE_FC;
	ct->ct_fcs_subtype = CT_FC_SUBTYPE_NS;
	ct->ct_cmd_resp = SNS_GID_FT;
	ct->ct_bcnt_resid = (GIDLEN - 16) >> 2;

	isp_put_ct_hdr(isp, ct, (ct_hdr_t *) &scp[XTXOFF]);
	rp = (uint32_t *) &scp[XTXOFF+sizeof (*ct)];
	ISP_IOZPUT_32(isp, FC4_SCSI, rp);
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "CT HDR + payload after put",
		    sizeof (*ct) + sizeof (uint32_t), &scp[XTXOFF]);
	}
	MEMZERO(&scp[ZTXOFF], QENTRY_LEN);
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_EXEC_COMMAND_IOCB_A64;
	mbs.param[1] = QENTRY_LEN;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma + CTXOFF);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma + CTXOFF);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma + CTXOFF);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma + CTXOFF);
	mbs.timeout = 500000;
	mbs.logval = MBLOGALL;
	MEMORYBARRIER(isp, SYNC_SFORDEV, XTXOFF, 2 * QENTRY_LEN);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return (-1);
	}
	MEMORYBARRIER(isp, SYNC_SFORCPU, ZTXOFF, QENTRY_LEN);
	pt = &un.plocal;
	isp_get_ct_pt(isp, (isp_ct_pt_t *) &scp[ZTXOFF], pt);
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "IOCB response", QENTRY_LEN, pt);
	}

	if (pt->ctp_status && pt->ctp_status != RQCS_DATA_UNDERRUN) {
		isp_prt(isp, ISP_LOGWARN, "CT Passthrough returned 0x%x",
		    pt->ctp_status);
		return (-1);
	}
	MEMORYBARRIER(isp, SYNC_SFORCPU, IGPOFF, GIDLEN + 16);
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "CT response", GIDLEN+16, &scp[IGPOFF]);
	}
	return (0);
}

static int
isp_scan_fabric(ispsoftc_t *isp)
{
	fcparam *fcp = FCPARAM(isp);
	uint32_t portid;
	uint16_t handle, oldhandle;
	int portidx, portlim, r;
	sns_gid_ft_rsp_t *rs0, *rs1;

	isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0, "FC Scan Fabric");
	if (fcp->isp_fwstate != FW_READY ||
	    fcp->isp_loopstate < LOOP_LSCAN_DONE) {
		return (-1);
	}
	if (fcp->isp_loopstate > LOOP_SCANNING_FABRIC) {
		return (0);
	}
	if (fcp->isp_topo != TOPO_FL_PORT && fcp->isp_topo != TOPO_F_PORT) {
		fcp->isp_loopstate = LOOP_FSCAN_DONE;
		isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
		    "FC Scan Fabric Done (no fabric)");
		return (0);
	}

	FC_SCRATCH_ACQUIRE(isp);
	fcp->isp_loopstate = LOOP_SCANNING_FABRIC;

	if (IS_24XX(isp)) {
		r = isp_gid_ft_ct_passthru(isp);
	} else {
		r = isp_gid_ft_sns(isp);
	}

	if (r > 0) {
		fcp->isp_loopstate = LOOP_FSCAN_DONE;
		FC_SCRATCH_RELEASE(isp);
		return (0);
	} else if (r < 0) {
		fcp->isp_loopstate = LOOP_PDB_RCVD;	/* try again */
		FC_SCRATCH_RELEASE(isp);
		return (0);
	}
	if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
		FC_SCRATCH_RELEASE(isp);
		return (-1);
	}

	MEMORYBARRIER(isp, SYNC_SFORCPU, IGPOFF, GIDLEN);
	rs0 = (sns_gid_ft_rsp_t *) ((uint8_t *)fcp->isp_scratch+IGPOFF);
	rs1 = (sns_gid_ft_rsp_t *) ((uint8_t *)fcp->isp_scratch+OGPOFF);
	isp_get_gid_ft_response(isp, rs0, rs1, NGENT);
	if (rs1->snscb_cthdr.ct_cmd_resp != LS_ACC) {
		int level;
		if (rs1->snscb_cthdr.ct_reason == 9 &&
		    rs1->snscb_cthdr.ct_explanation == 7) {
			level = ISP_LOGSANCFG|ISP_LOGDEBUG0;
		} else {
			level = ISP_LOGWARN;
		}
		isp_prt(isp, level, "Fabric Nameserver rejected GID_FT "
		    "(Reason=0x%x Expl=0x%x)", rs1->snscb_cthdr.ct_reason,
		    rs1->snscb_cthdr.ct_explanation);
		FC_SCRATCH_RELEASE(isp);
		fcp->isp_loopstate = LOOP_FSCAN_DONE;
		return (0);
	}


	/*
	 * If we get this far, we certainly still have the fabric controller.
	 */
	fcp->portdb[FL_ID].state = FC_PORTDB_STATE_PENDING_VALID;

	/*
	 * Prime the handle we will start using.
	 */
	oldhandle = NIL_HANDLE;

	/*
	 * Okay, we now have a list of Port IDs for all FC4 SCSI devices
	 * that the Fabric Name server knows about. Go through the list
	 * and remove duplicate port ids.
	 */

	portlim = 0;
	portidx = 0;
	for (portidx = 0; portidx < NGENT-1; portidx++) {
		if (rs1->snscb_ports[portidx].control & 0x80) {
			break;
		}
	}

	/*
	 * If we're not at the last entry, our list wasn't big enough.
	 */
	if ((rs1->snscb_ports[portidx].control & 0x80) == 0) {
		isp_prt(isp, ISP_LOGWARN,
		    "fabric too big for scratch area: increase ISP2100_SCRLEN");
	}
	portlim = portidx + 1;
	isp_prt(isp, ISP_LOGSANCFG,
	    "got %d ports back from name server", portlim);

	for (portidx = 0; portidx < portlim; portidx++) {
		int npidx;

		portid =
		    ((rs1->snscb_ports[portidx].portid[0]) << 16) |
		    ((rs1->snscb_ports[portidx].portid[1]) << 8) |
		    ((rs1->snscb_ports[portidx].portid[2]));

		for (npidx = portidx + 1; npidx < portlim; npidx++) {
			uint32_t new_portid =
			    ((rs1->snscb_ports[npidx].portid[0]) << 16) |
			    ((rs1->snscb_ports[npidx].portid[1]) << 8) |
			    ((rs1->snscb_ports[npidx].portid[2]));
			if (new_portid == portid) {
				break;
			}
		}

		if (npidx < portlim) {
			rs1->snscb_ports[npidx].portid[0] = 0;
			rs1->snscb_ports[npidx].portid[1] = 0;
			rs1->snscb_ports[npidx].portid[2] = 0;
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "removing duplicate PortID 0x%x entry from list",
			    portid);
		}
	}

	/*
	 * Okay, we now have a list of Port IDs for all FC4 SCSI devices
	 * that the Fabric Name server knows about.
	 *
	 * For each entry on this list go through our port database looking
	 * for probational entries- if we find one, then an old entry is
	 * is maybe still this one. We get some information to find out.
	 *
	 * Otherwise, it's a new fabric device, and we log into it
	 * (unconditionally). After searching the entire database
	 * again to make sure that we never ever ever ever have more
	 * than one entry that has the same PortID or the same
	 * WWNN/WWPN duple, we enter the device into our database.
	 */

	for (portidx = 0; portidx < portlim; portidx++) {
		fcportdb_t *lp;
		isp_pdb_t pdb;
		uint64_t wwnn, wwpn;
		int dbidx, nr;

		portid =
		    ((rs1->snscb_ports[portidx].portid[0]) << 16) |
		    ((rs1->snscb_ports[portidx].portid[1]) << 8) |
		    ((rs1->snscb_ports[portidx].portid[2]));

		if (portid == 0) {
			isp_prt(isp, ISP_LOGSANCFG,
			    "skipping null PortID at idx %d", portidx);
			continue;
		}

		/*
		 * Skip ourselves...
		 */
		if (portid == fcp->isp_portid) {
			isp_prt(isp, ISP_LOGSANCFG,
			    "skip ourselves @ PortID 0x%06x", portid);
			continue;
		}
		isp_prt(isp, ISP_LOGSANCFG,
		    "Checking Fabric Port 0x%06x", portid);

		/*
		 * We now search our Port Database for any
		 * probational entries with this PortID. We don't
		 * look for zombies here- only probational
		 * entries (we've already logged out of zombies).
		 */
		for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
			lp = &fcp->portdb[dbidx];

			if (lp->state != FC_PORTDB_STATE_PROBATIONAL) {
				continue;
			}
			if (lp->portid == portid) {
				break;
			}
		}

		/*
		 * We found a probational entry with this Port ID.
		 */
		if (dbidx < MAX_FC_TARG) {
			int handle_changed = 0;

			lp = &fcp->portdb[dbidx];

			/*
			 * See if we're still logged into it.
			 *
			 * If we aren't, mark it as a dead device and
			 * leave the new portid in the database entry
			 * for somebody further along to decide what to
			 * do (policy choice).
			 *
			 * If we are, check to see if it's the same
			 * device still (it should be). If for some
			 * reason it isn't, mark it as a changed device
			 * and leave the new portid and role in the
			 * database entry for somebody further along to
			 * decide what to do (policy choice).
			 *
			 */

			r = isp_getpdb(isp, lp->handle, &pdb, 0);
			if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
				FC_SCRATCH_RELEASE(isp);
				ISP_MARK_PORTDB(isp, 1);
				return (-1);
			}
			if (r != 0) {
				lp->new_portid = portid;
				lp->state = FC_PORTDB_STATE_DEAD;
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				    "Fabric Port 0x%06x considered dead",
				    portid);
				continue;
			}


			/*
			 * Check to make sure that handle, portid, WWPN and
			 * WWNN agree. If they don't, then the association
			 * between this PortID and the stated handle has been
			 * broken by the firmware.
			 */
			MAKE_WWN_FROM_NODE_NAME(wwnn, pdb.nodename);
			MAKE_WWN_FROM_NODE_NAME(wwpn, pdb.portname);
			if (pdb.handle != lp->handle ||
			    pdb.portid != portid ||
			    wwpn != lp->port_wwn ||
			    wwnn != lp->node_wwn) {
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				    fconf, dbidx, pdb.handle, pdb.portid,
				    (uint32_t) (wwnn >> 32), (uint32_t) wwnn,
				    (uint32_t) (wwpn >> 32), (uint32_t) wwpn,
				    lp->handle, portid,
				    (uint32_t) (lp->node_wwn >> 32),
				    (uint32_t) lp->node_wwn,
				    (uint32_t) (lp->port_wwn >> 32),
				    (uint32_t) lp->port_wwn);
				/*
				 * Try to re-login to this device using a
				 * new handle. If that fails, mark it dead.
				 * 
				 * isp_login_device will check for handle and
				 * portid consistency after re-login.
				 * 
				 */
				if (isp_login_device(isp, portid, &pdb,
				    &oldhandle)) {
					lp->new_portid = portid;
					lp->state = FC_PORTDB_STATE_DEAD;
					if (fcp->isp_loopstate !=
					    LOOP_SCANNING_FABRIC) {
						FC_SCRATCH_RELEASE(isp);
						ISP_MARK_PORTDB(isp, 1);
						return (-1);
					}
					continue;
				}
				MAKE_WWN_FROM_NODE_NAME(wwnn, pdb.nodename);
				MAKE_WWN_FROM_NODE_NAME(wwpn, pdb.portname);
				if (wwpn != lp->port_wwn ||
				    wwnn != lp->node_wwn) {
					isp_prt(isp, ISP_LOGWARN, "changed WWN"
					    " after relogin");
					lp->new_portid = portid;
					lp->state = FC_PORTDB_STATE_DEAD;
					continue;
				}

				lp->handle = pdb.handle;
				handle_changed++;
			}

			nr = (pdb.s3_role & SVC3_ROLE_MASK) >> SVC3_ROLE_SHIFT;

			/*
			 * Check to see whether the portid and roles have
			 * stayed the same. If they have stayed the same,
			 * we believe that this is the same device and it
			 * hasn't become disconnected and reconnected, so
			 * mark it as pending valid.
			 *
			 * If they aren't the same, mark the device as a
			 * changed device and save the new port id and role
			 * and let somebody else decide.
			 */

			lp->new_portid = portid;
			lp->new_roles = nr;
			if (pdb.portid != lp->portid || nr != lp->roles ||
			    handle_changed) {
				isp_prt(isp, ISP_LOGSANCFG,
				    "Fabric Port 0x%06x changed", portid);
				lp->state = FC_PORTDB_STATE_CHANGED;
			} else {
				isp_prt(isp, ISP_LOGSANCFG,
				    "Fabric Port 0x%06x Now Pending Valid",
				    portid);
				lp->state = FC_PORTDB_STATE_PENDING_VALID;
			}
			continue;
		}

		/*
		 * Ah- a new entry. Search the database again for all non-NIL
		 * entries to make sure we never ever make a new database entry
		 * with the same port id. While we're at it, mark where the
		 * last free entry was.
		 */
	
		dbidx = MAX_FC_TARG;
		for (lp = fcp->portdb; lp < &fcp->portdb[MAX_FC_TARG]; lp++) {
			if (lp >= &fcp->portdb[FL_ID] &&
			    lp <= &fcp->portdb[SNS_ID]) {
				continue;
			}
			if (lp->state == FC_PORTDB_STATE_NIL) {
				if (dbidx == MAX_FC_TARG) {
					dbidx = lp - fcp->portdb;
				}
				continue;
			}
			if (lp->state == FC_PORTDB_STATE_ZOMBIE) {
				continue;
			}
			if (lp->portid == portid) {
				break;
			}
		}

		if (lp < &fcp->portdb[MAX_FC_TARG]) {
			isp_prt(isp, ISP_LOGWARN,
			    "PortID 0x%06x already at %d handle %d state %d",
			    portid, dbidx, lp->handle, lp->state);
			continue;
		}

		/*
		 * We should have the index of the first free entry seen.
		 */
		if (dbidx == MAX_FC_TARG) {
			isp_prt(isp, ISP_LOGERR,
			    "port database too small to login PortID 0x%06x"
			    "- increase MAX_FC_TARG", portid);
			continue;
		}

		/*
		 * Otherwise, point to our new home.
		 */
		lp = &fcp->portdb[dbidx];

		/*
		 * Try to see if we are logged into this device,
		 * and maybe log into it.
		 *
		 * isp_login_device will check for handle and
		 * portid consistency after login.
		 */
		if (isp_login_device(isp, portid, &pdb, &oldhandle)) {
			if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
				FC_SCRATCH_RELEASE(isp);
				ISP_MARK_PORTDB(isp, 1);
				return (-1);
			}
			continue;
		}

		handle = pdb.handle;
		MAKE_WWN_FROM_NODE_NAME(wwnn, pdb.nodename);
		MAKE_WWN_FROM_NODE_NAME(wwpn, pdb.portname);
		nr = (pdb.s3_role & SVC3_ROLE_MASK) >> SVC3_ROLE_SHIFT;

		/*
		 * And go through the database *one* more time to make sure
		 * that we do not make more than one entry that has the same
		 * WWNN/WWPN duple
		 */
		for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
			if (dbidx >= FL_ID && dbidx <= SNS_ID) {
				continue;
			}
			if (fcp->portdb[dbidx].state == FC_PORTDB_STATE_NIL) {
				continue;
			}
			if (fcp->portdb[dbidx].node_wwn == wwnn &&
			    fcp->portdb[dbidx].port_wwn == wwpn) {
				break;
			}
		}

		if (dbidx == MAX_FC_TARG) {
			MEMZERO(lp, sizeof (fcportdb_t));
			lp->handle = handle;
			lp->node_wwn = wwnn;
			lp->port_wwn = wwpn;
			lp->new_portid = portid;
			lp->new_roles = nr;
			lp->state = FC_PORTDB_STATE_NEW;
			isp_prt(isp, ISP_LOGSANCFG,
			    "Fabric Port 0x%06x is New Entry", portid);
			continue;
		}

    		if (fcp->portdb[dbidx].state != FC_PORTDB_STATE_ZOMBIE) {
			isp_prt(isp, ISP_LOGWARN,
			    "PortID 0x%x 0x%08x%08x/0x%08x%08x %ld already at "
			    "idx %d, state 0x%x", portid,
			    (uint32_t) (wwnn >> 32), (uint32_t) wwnn,
			    (uint32_t) (wwpn >> 32), (uint32_t) wwpn,
			    (long) (lp - fcp->portdb), dbidx,
			    fcp->portdb[dbidx].state);
			continue;
		}

		/*
		 * We found a zombie entry that matches us.
		 * Revive it. We know that WWN and WWPN
		 * are the same. For fabric devices, we
		 * don't care that handle is different
		 * as we assign that. If role or portid
		 * are different, it maybe a changed device.
		 */
		lp = &fcp->portdb[dbidx];
		lp->handle = handle;
		lp->new_portid = portid;
		lp->new_roles = nr;
		if (lp->portid != portid || lp->roles != nr) {
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "Zombie Fabric Port 0x%06x Now Changed", portid);
			lp->state = FC_PORTDB_STATE_CHANGED;
		} else {
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "Zombie Fabric Port 0x%06x Now Pending Valid",
			    portid);
			lp->state = FC_PORTDB_STATE_PENDING_VALID;
		}
	}

	FC_SCRATCH_RELEASE(isp);
	if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
		ISP_MARK_PORTDB(isp, 1);
		return (-1);
	}
	fcp->isp_loopstate = LOOP_FSCAN_DONE;
	isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0, "FC Scan Fabric Done");
	return (0);
}

/*
 * Find an unused handle and try and use to login to a port.
 */
static int
isp_login_device(ispsoftc_t *isp, uint32_t portid, isp_pdb_t *p, uint16_t *ohp)
{
	int lim, i, r;
	uint16_t handle;

	if (FCPARAM(isp)->isp_2klogin) {
		lim = NPH_MAX_2K;
	} else {
		lim = NPH_MAX;
	}

	handle = isp_nxt_handle(isp, *ohp);
	for (i = 0; i < lim; i++) {
		/*
		 * See if we're still logged into something with
		 * this handle and that something agrees with this
		 * port id.
		 */
		r = isp_getpdb(isp, handle, p, 0);
		if (r == 0 && p->portid != portid) {
			(void) isp_plogx(isp, handle, portid,
			    PLOGX_FLG_CMD_LOGO | PLOGX_FLG_IMPLICIT, 1);
		} else if (r == 0) {
			break;
		}
		if (FCPARAM(isp)->isp_loopstate != LOOP_SCANNING_FABRIC) {
			return (-1);
		}
		/*
		 * Now try and log into the device
		 */
		r = isp_plogx(isp, handle, portid, PLOGX_FLG_CMD_PLOGI, 1);
		if (FCPARAM(isp)->isp_loopstate != LOOP_SCANNING_FABRIC) {
			return (-1);
		}
		if (r == 0) {
			*ohp = handle;
			break;
		} else if ((r & 0xffff) == MBOX_PORT_ID_USED) {
			handle = r >> 16;
			break;
		} else if (r != MBOX_LOOP_ID_USED) {
			i = lim;
			break;
		} else {
			*ohp = handle;
			handle = isp_nxt_handle(isp, *ohp);
		}
	}

	if (i == lim) {
		isp_prt(isp, ISP_LOGWARN, "PLOGI 0x%06x failed", portid);
		return (-1);
	}

	/*
	 * If we successfully logged into it, get the PDB for it
	 * so we can crosscheck that it is still what we think it
	 * is and that we also have the role it plays
	 */
	r = isp_getpdb(isp, handle, p, 0);
	if (FCPARAM(isp)->isp_loopstate != LOOP_SCANNING_FABRIC) {
		return (-1);
	}
	if (r != 0) {
		isp_prt(isp, ISP_LOGERR, "new device 0x%06x@0x%x disappeared",
		    portid, handle);
		return (-1);
	}

	if (p->handle != handle || p->portid != portid) {
		isp_prt(isp, ISP_LOGERR,
		    "new device 0x%06x@0x%x changed (0x%06x@0x%0x)",
		    portid, handle, p->portid, p->handle);
		return (-1);
	}
	return (0);
}

static int
isp_register_fc4_type(ispsoftc_t *isp)
{
	fcparam *fcp = isp->isp_param;
	uint8_t local[SNS_RFT_ID_REQ_SIZE];
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
	FC_SCRATCH_ACQUIRE(isp);
	isp_put_sns_request(isp, reqp, (sns_screq_t *) fcp->isp_scratch);
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_SEND_SNS;
	mbs.param[1] = SNS_RFT_ID_REQ_SIZE >> 1;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	mbs.logval = MBLOGALL;
	mbs.timeout = 10000000;
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, SNS_RFT_ID_REQ_SIZE);
	isp_mboxcmd(isp, &mbs);
	FC_SCRATCH_RELEASE(isp);
	if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
		return (0);
	} else {
		return (-1);
	}
}

static int
isp_register_fc4_type_24xx(ispsoftc_t *isp)
{
	mbreg_t mbs;
	fcparam *fcp = FCPARAM(isp);
	union {
		isp_ct_pt_t plocal;
		rft_id_t clocal;
		uint8_t q[QENTRY_LEN];
	} un;
	isp_ct_pt_t *pt;
	ct_hdr_t *ct;
	rft_id_t *rp;
	uint8_t *scp = fcp->isp_scratch;

	FC_SCRATCH_ACQUIRE(isp);
	/*
	 * Build a Passthrough IOCB in memory.
	 */
	MEMZERO(un.q, QENTRY_LEN);
	pt = &un.plocal;
	pt->ctp_header.rqs_entry_count = 1;
	pt->ctp_header.rqs_entry_type = RQSTYPE_CT_PASSTHRU;
	pt->ctp_handle = 0xffffffff;
	pt->ctp_nphdl = NPH_SNS_ID;
	pt->ctp_cmd_cnt = 1;
	pt->ctp_time = 1;
	pt->ctp_rsp_cnt = 1;
	pt->ctp_rsp_bcnt = sizeof (ct_hdr_t);
	pt->ctp_cmd_bcnt = sizeof (rft_id_t);
	pt->ctp_dataseg[0].ds_base = DMA_LO32(fcp->isp_scdma+XTXOFF);
	pt->ctp_dataseg[0].ds_basehi = DMA_HI32(fcp->isp_scdma+XTXOFF);
	pt->ctp_dataseg[0].ds_count = sizeof (rft_id_t);
	pt->ctp_dataseg[1].ds_base = DMA_LO32(fcp->isp_scdma+IGPOFF);
	pt->ctp_dataseg[1].ds_basehi = DMA_HI32(fcp->isp_scdma+IGPOFF);
	pt->ctp_dataseg[1].ds_count = sizeof (ct_hdr_t);
	isp_put_ct_pt(isp, pt, (isp_ct_pt_t *) &scp[CTXOFF]);

	/*
	 * Build the CT header and command in memory.
	 *
	 * Note that the CT header has to end up as Big Endian format in memory.
	 */
	MEMZERO(&un.clocal, sizeof (un.clocal));
	ct = &un.clocal.rftid_hdr;
	ct->ct_revision = CT_REVISION;
	ct->ct_fcs_type = CT_FC_TYPE_FC;
	ct->ct_fcs_subtype = CT_FC_SUBTYPE_NS;
	ct->ct_cmd_resp = SNS_RFT_ID;
	ct->ct_bcnt_resid = (sizeof (rft_id_t) - sizeof (ct_hdr_t)) >> 2;
	rp = &un.clocal;
	rp->rftid_portid[0] = fcp->isp_portid >> 16;
	rp->rftid_portid[1] = fcp->isp_portid >> 8;
	rp->rftid_portid[2] = fcp->isp_portid;
	rp->rftid_fc4types[FC4_SCSI >> 5] = 1 << (FC4_SCSI & 0x1f);
	isp_put_rft_id(isp, rp, (rft_id_t *) &scp[XTXOFF]);

	MEMZERO(&scp[ZTXOFF], sizeof (ct_hdr_t));

	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_EXEC_COMMAND_IOCB_A64;
	mbs.param[1] = QENTRY_LEN;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma + CTXOFF);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma + CTXOFF);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma + CTXOFF);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma + CTXOFF);
	mbs.timeout = 500000;
	mbs.logval = MBLOGALL;
	MEMORYBARRIER(isp, SYNC_SFORDEV, XTXOFF, 2 * QENTRY_LEN);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		FC_SCRATCH_RELEASE(isp);
		return (-1);
	}
	MEMORYBARRIER(isp, SYNC_SFORCPU, ZTXOFF, QENTRY_LEN);
	pt = &un.plocal;
	isp_get_ct_pt(isp, (isp_ct_pt_t *) &scp[ZTXOFF], pt);
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "IOCB response", QENTRY_LEN, pt);
	}
	if (pt->ctp_status) {
		FC_SCRATCH_RELEASE(isp);
		isp_prt(isp, ISP_LOGWARN, "CT Passthrough returned 0x%x",
		    pt->ctp_status);
		return (-1);
	}

	isp_get_ct_hdr(isp, (ct_hdr_t *) &scp[IGPOFF], ct);
	FC_SCRATCH_RELEASE(isp);

	if (ct->ct_cmd_resp == LS_RJT) {
		isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
		    "Register FC4 Type rejected");
		return (-1);
	} else if (ct->ct_cmd_resp == LS_ACC) {
		isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
		    "Register FC4 Type accepted");
		return(0);
	} else {
		isp_prt(isp, ISP_LOGWARN,
		    "Register FC4 Type: 0x%x", ct->ct_cmd_resp);
		return (-1);
	}
}

static uint16_t
isp_nxt_handle(ispsoftc_t *isp, uint16_t handle)
{
	int i;
	if (handle == NIL_HANDLE) {
		if (FCPARAM(isp)->isp_topo == TOPO_F_PORT) {
			handle = 0;
		} else {
			handle = SNS_ID+1;
		}
	} else {
		handle += 1;
		if (handle >= FL_ID && handle <= SNS_ID) {
			handle = SNS_ID+1;
		}
		if (handle >= NPH_RESERVED && handle <= NPH_FL_ID) {
			handle = NPH_FL_ID+1;
		}
		if (FCPARAM(isp)->isp_2klogin) {
			if (handle == NPH_MAX_2K) {
				handle = 0;
			}
		} else {
			if (handle == NPH_MAX) {
				handle = 0;
			}
		}
	}
	if (handle == FCPARAM(isp)->isp_loopid) {
		return (isp_nxt_handle(isp, handle));
	}
	for (i = 0; i < MAX_FC_TARG; i++) {
		if (FCPARAM(isp)->portdb[i].state == FC_PORTDB_STATE_NIL) {
			continue;
		}
		if (FCPARAM(isp)->portdb[i].handle == handle) {
			return (isp_nxt_handle(isp, handle));
		}
	}
	return (handle);
}

/*
 * Start a command. Locking is assumed done in the caller.
 */

int
isp_start(XS_T *xs)
{
	ispsoftc_t *isp;
	uint32_t nxti, optr, handle;
	uint8_t local[QENTRY_LEN];
	ispreq_t *reqp, *qep;
	void *cdbp;
	uint16_t *tptr;
	int target, i, hdlidx = 0;

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
	 * Translate the target to device handle as appropriate, checking
	 * for correct device state as well.
	 */
	target = XS_TGT(xs);
	if (IS_FC(isp)) {
		fcparam *fcp = isp->isp_param;

		/*
		 * Try again later.
		 */
		if (fcp->isp_fwstate != FW_READY ||
		    fcp->isp_loopstate != LOOP_READY) {
			return (CMD_RQLATER);
		}

		if (XS_TGT(xs) >= MAX_FC_TARG) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_COMPLETE);
		}

		hdlidx = fcp->isp_ini_map[XS_TGT(xs)] - 1;
		isp_prt(isp, ISP_LOGDEBUG1, "XS_TGT(xs)=%d- hdlidx value %d",
		    XS_TGT(xs), hdlidx);
		if (hdlidx < 0 || hdlidx >= MAX_FC_TARG) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_COMPLETE);
		}
		if (fcp->portdb[hdlidx].state == FC_PORTDB_STATE_ZOMBIE) {
			return (CMD_RQLATER);
		}
		if (fcp->portdb[hdlidx].state != FC_PORTDB_STATE_VALID) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_COMPLETE);
		}
		target = fcp->portdb[hdlidx].handle;
	}

	/*
	 * Next check to see if any HBA or Device parameters need to be updated.
	 */
	if (isp->isp_update != 0) {
		isp_update(isp);
	}

 start_again:

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
		if (IS_24XX(isp)) {
			isp_marker_24xx_t *m = (isp_marker_24xx_t *) qep;
			MEMZERO(m, QENTRY_LEN);
			m->mrk_header.rqs_entry_count = 1;
			m->mrk_header.rqs_entry_type = RQSTYPE_MARKER;
			m->mrk_modifier = SYNC_ALL;
			isp_put_marker_24xx(isp, m, (isp_marker_24xx_t *)qep);
			ISP_ADD_REQUEST(isp, nxti);
			isp->isp_sendmarker = 0;
			goto start_again;
		} else {
			for (i = 0; i < (IS_DUALBUS(isp)? 2: 1); i++) {
				isp_marker_t *m = (isp_marker_t *) qep;
				if ((isp->isp_sendmarker & (1 << i)) == 0) {
					continue;
				}
				MEMZERO(m, QENTRY_LEN);
				m->mrk_header.rqs_entry_count = 1;
				m->mrk_header.rqs_entry_type = RQSTYPE_MARKER;
				m->mrk_target = (i << 7);	/* bus # */
				m->mrk_modifier = SYNC_ALL;
				isp_put_marker(isp, m, (isp_marker_t *) qep);
				ISP_ADD_REQUEST(isp, nxti);
				isp->isp_sendmarker &= ~(1 << i);
				goto start_again;
			}
		}
	}

	MEMZERO((void *)reqp, QENTRY_LEN);
	reqp->req_header.rqs_entry_count = 1;
	if (IS_24XX(isp)) {
		reqp->req_header.rqs_entry_type = RQSTYPE_T7RQS;
	} else if (IS_FC(isp)) {
		reqp->req_header.rqs_entry_type = RQSTYPE_T2RQS;
	} else {
		if (XS_CDBLEN(xs) > 12)
			reqp->req_header.rqs_entry_type = RQSTYPE_CMDONLY;
		else
			reqp->req_header.rqs_entry_type = RQSTYPE_REQUEST;
	}
	/* reqp->req_header.rqs_flags = 0; */
	/* reqp->req_header.rqs_seqno = 0; */
	if (IS_24XX(isp)) {
		int ttype;
		if (XS_TAG_P(xs)) {
			ttype = XS_TAG_TYPE(xs);
		} else {
			if (XS_CDBP(xs)[0] == 0x3) {
				ttype = REQFLAG_HTAG;
			} else {
				ttype = REQFLAG_STAG;
			}
		}
		if (ttype == REQFLAG_OTAG) {
			ttype = FCP_CMND_TASK_ATTR_ORDERED;
		} else if (ttype == REQFLAG_HTAG) {
			ttype = FCP_CMND_TASK_ATTR_HEAD;
		} else {
			ttype = FCP_CMND_TASK_ATTR_SIMPLE;
		}
		((ispreqt7_t *)reqp)->req_task_attribute = ttype;
	} else if (IS_FC(isp)) {
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
	cdbp = reqp->req_cdb;
	tptr = &reqp->req_time;

	if (IS_SCSI(isp)) {
		reqp->req_target = target | (XS_CHANNEL(xs) << 7);
		reqp->req_lun_trn = XS_LUN(xs);
		reqp->req_cdblen = XS_CDBLEN(xs);
	} else if (IS_24XX(isp)) {
		fcportdb_t *lp;

		lp = &FCPARAM(isp)->portdb[hdlidx];
		((ispreqt7_t *)reqp)->req_nphdl = target;
		((ispreqt7_t *)reqp)->req_tidlo = lp->portid;
		((ispreqt7_t *)reqp)->req_tidhi = lp->portid >> 16;
		if (XS_LUN(xs) > 256) {
			((ispreqt7_t *)reqp)->req_lun[0] = XS_LUN(xs) >> 8;
			((ispreqt7_t *)reqp)->req_lun[0] |= 0x40;
		}
		((ispreqt7_t *)reqp)->req_lun[1] = XS_LUN(xs);
		cdbp = ((ispreqt7_t *)reqp)->req_cdb;
		tptr = &((ispreqt7_t *)reqp)->req_time;
	} else if (FCPARAM(isp)->isp_2klogin) {
		((ispreqt2e_t *)reqp)->req_target = target;
		((ispreqt2e_t *)reqp)->req_scclun = XS_LUN(xs);
	} else if (FCPARAM(isp)->isp_sccfw) {
		((ispreqt2_t *)reqp)->req_target = target;
		((ispreqt2_t *)reqp)->req_scclun = XS_LUN(xs);
	} else {
		((ispreqt2_t *)reqp)->req_target = target;
		((ispreqt2_t *)reqp)->req_lun_trn = XS_LUN(xs);
	}
	MEMCPY(cdbp, XS_CDBP(xs), XS_CDBLEN(xs));

	*tptr = XS_TIME(xs) / 1000;
	if (*tptr == 0 && XS_TIME(xs)) {
		*tptr = 1;
	}
	if (IS_24XX(isp) && *tptr > 0x1999) {
		*tptr = 0x1999;
	}

	if (isp_save_xs(isp, xs, &handle)) {
		isp_prt(isp, ISP_LOGDEBUG0, "out of xflist pointers");
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	}
	/* Whew. Thankfully the same for type 7 requests */
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
	isp_prt(isp, ISP_LOGDEBUG0,
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
isp_control(ispsoftc_t *isp, ispctl_t ctl, void *arg)
{
	XS_T *xs;
	mbreg_t mbs;
	int bus, tgt;
	uint32_t handle;

	MEMZERO(&mbs, sizeof (mbs));

	switch (ctl) {
	default:
		isp_prt(isp, ISP_LOGERR, "Unknown Control Opcode 0x%x", ctl);
		break;

	case ISPCTL_RESET_BUS:
		/*
		 * Issue a bus reset.
		 */
		if (IS_24XX(isp)) {
			isp_prt(isp, ISP_LOGWARN, "RESET BUS NOT IMPLEMENTED");
			break;
		} else if (IS_FC(isp)) {
			mbs.param[1] = 10;
			bus = 0;
		} else {
			mbs.param[1] = SDPARAM(isp)->isp_bus_reset_delay;
			if (mbs.param[1] < 2) {
				mbs.param[1] = 2;
			}
			bus = *((int *) arg);
			if (IS_DUALBUS(isp)) {
				mbs.param[2] = bus;
			}
		}
		mbs.param[0] = MBOX_BUS_RESET;
		isp->isp_sendmarker |= (1 << bus);
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			break;
		}
		isp_prt(isp, ISP_LOGINFO,
		    "driver initiated bus reset of bus %d", bus);
		return (0);

	case ISPCTL_RESET_DEV:
		tgt = (*((int *) arg)) & 0xffff;
		if (IS_24XX(isp)) {
			isp_prt(isp, ISP_LOGWARN, "RESET DEV NOT IMPLEMENTED");
			break;
		} else if (IS_FC(isp)) {
			if (FCPARAM(isp)->isp_2klogin) {
				mbs.param[1] = tgt;
				mbs.ibits = (1 << 10);
			} else {
				mbs.param[1] = (tgt << 8);
			}
			bus = 0;
		} else {
			bus = (*((int *) arg)) >> 16;
			mbs.param[1] = (bus << 15) | (tgt << 8);
		}
		mbs.param[0] = MBOX_ABORT_TARGET;
		mbs.param[2] = 3;	/* 'delay', in seconds */
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
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
		if (IS_24XX(isp)) {
			isp_prt(isp, ISP_LOGWARN, "ABORT CMD NOT IMPLEMENTED");
			break;
		} else if (IS_FC(isp)) {
			if (FCPARAM(isp)->isp_sccfw) {
				if (FCPARAM(isp)->isp_2klogin) {
					mbs.param[1] = tgt;
				} else {
					mbs.param[1] = tgt << 8;
				}
				mbs.param[6] = XS_LUN(xs);
			} else {
				mbs.param[1] = tgt << 8 | XS_LUN(xs);
			}
		} else {
			bus = XS_CHANNEL(xs);
			mbs.param[1] = (bus << 15) | (tgt << 8) | XS_LUN(xs);
		}
		mbs.param[0] = MBOX_ABORT;
		mbs.param[2] = handle;
		mbs.logval = MBLOGALL & ~MBOX_COMMAND_ERROR;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			break;
		}
		return (0);

	case ISPCTL_UPDATE_PARAMS:

		isp_update(isp);
		return (0);

	case ISPCTL_FCLINK_TEST:

		if (IS_FC(isp)) {
			int usdelay = *((int *) arg);
			if (usdelay == 0) {
				usdelay =  250000;
			}
			return (isp_fclink_test(isp, usdelay));
		}
		break;

	case ISPCTL_SCAN_FABRIC:

		if (IS_FC(isp)) {
			return (isp_scan_fabric(isp));
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

		if (IS_FC(isp) && !IS_24XX(isp)) {
			mbs.param[0] = MBOX_INIT_LIP;
			if (FCPARAM(isp)->isp_2klogin) {
				mbs.ibits = (1 << 10);
			}
			mbs.logval = MBLOGALL;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
				return (0);
			}
		}
		break;

	case ISPCTL_GET_PDB:
		if (IS_FC(isp) && arg) {
			int id = *((int *)arg);
			isp_pdb_t *pdb = arg;
			return (isp_getpdb(isp, id, pdb, 1));
		}
		break;

	case ISPCTL_GET_PORTNAME:
	{
		uint64_t *wwnp = arg;
		int loopid = *wwnp;
		*wwnp = isp_get_portname(isp, loopid, 0);
		if (*wwnp == (uint64_t) -1) {
			break;
		} else {
			return (0);
		}
	}
	case ISPCTL_RUN_MBOXCMD:

		isp_mboxcmd(isp, arg);
		return(0);

	case ISPCTL_PLOGX:
	{
		isp_plcmd_t *p = arg;
		int r;

		if ((p->flags & PLOGX_FLG_CMD_MASK) != PLOGX_FLG_CMD_PLOGI ||
		    (p->handle != NIL_HANDLE)) {
			return (isp_plogx(isp, p->handle, p->portid,
			    p->flags, 0));
		}
		do {
			p->handle = isp_nxt_handle(isp, p->handle);
			r = isp_plogx(isp, p->handle, p->portid, p->flags, 0);
			if ((r & 0xffff) == MBOX_PORT_ID_USED) {
				p->handle = r >> 16;
				r = 0;
				break;
			}
		} while ((r & 0xffff) == MBOX_LOOP_ID_USED);
		return (r);
	}
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
			mbs.logval = MBLOGALL;
			isp_mboxcmd(isp, &mbs);
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
#define	MAX_REQUESTQ_COMPLETIONS	32
#endif

void
isp_intr(ispsoftc_t *isp, uint32_t isr, uint16_t sema, uint16_t mbox)
{
	XS_T *complist[MAX_REQUESTQ_COMPLETIONS], *xs;
	uint32_t iptr, optr, junk;
	int i, nlooked = 0, ndone = 0;

again:
	optr = isp->isp_residx;
	/*
	 * Is this a mailbox related interrupt?
	 * The mailbox semaphore will be nonzero if so.
	 */
	if (sema) {
		if (mbox & 0x4000) {
			isp->isp_intmboxc++;
			if (isp->isp_mboxbsy) {
				int obits = isp->isp_obits;
				isp->isp_mboxtmp[0] = mbox;
				for (i = 1; i < MAX_MAILBOX(isp); i++) {
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
				    "mailbox cmd (0x%x) with no waiters", mbox);
			}
		} else if (isp_parse_async(isp, mbox) < 0) {
			return;
		}
		if ((IS_FC(isp) && mbox != ASYNC_RIO_RESP) ||
		    isp->isp_state != ISP_RUNSTATE) {
			goto out;
		}
	}

	/*
	 * We can't be getting this now.
	 */
	if (isp->isp_state != ISP_RUNSTATE) {
		isp_prt(isp, ISP_LOGINFO,
		    "interrupt (ISR=%x SEMA=%x) when not ready", isr, sema);
		/*
		 * Thank you very much!  *Burrrp*!
		 */
		ISP_WRITE(isp, isp->isp_respoutrp,
		    ISP_READ(isp, isp->isp_respinrp));
		if (IS_24XX(isp)) {
			ISP_DISABLE_INTS(isp);
		}
		goto out;
	}

#ifdef	ISP_TARGET_MODE
	/*
	 * Check for ATIO Queue entries.
	 */
	if (isp->isp_rspbsy == 0 && (isp->isp_role & ISP_ROLE_TARGET) &&
	    IS_24XX(isp)) {
		iptr = ISP_READ(isp, isp->isp_atioinrp);
		optr = ISP_READ(isp, isp->isp_atiooutrp);

		isp->isp_rspbsy = 1;
		while (optr != iptr) {
			uint8_t qe[QENTRY_LEN];
			isphdr_t *hp;
			uint32_t oop;
			void *addr;

			oop = optr;
			MEMORYBARRIER(isp, SYNC_ATIOQ, oop, QENTRY_LEN);
			addr = ISP_QUEUE_ENTRY(isp->isp_atioq, oop);
			isp_get_hdr(isp, addr, (isphdr_t *)qe);
			hp = (isphdr_t *)qe;
			switch (hp->rqs_entry_type) {
			case RQSTYPE_NOTIFY:
			case RQSTYPE_ATIO:
				(void) isp_target_notify(isp, addr, &oop);
				break;
			default:
				isp_print_qentry(isp, "?ATIOQ entry?",
				    oop, addr);
				break;
			}
			optr = ISP_NXT_QENTRY(oop, RESULT_QUEUE_LEN(isp));
			ISP_WRITE(isp, isp->isp_atiooutrp, optr);
		}
		isp->isp_rspbsy = 0;
		optr = isp->isp_residx;
	}
#endif

	/*
	 * Get the current Response Queue Out Pointer.
	 *
	 * If we're a 2300 or 2400, we can ask what hardware what it thinks.
	 */
	if (IS_23XX(isp) || IS_24XX(isp)) {
		optr = ISP_READ(isp, isp->isp_respoutrp);
		/*
		 * Debug: to be taken out eventually
		 */
		if (isp->isp_residx != optr) {
			isp_prt(isp, ISP_LOGINFO,
			    "isp_intr: hard optr=%x, soft optr %x",
			    optr, isp->isp_residx);
			isp->isp_residx = optr;
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
			iptr = ISP_READ(isp, isp->isp_respinrp);
			junk = ISP_READ(isp, isp->isp_respinrp);
		} while (junk != iptr && ++i < 1000);

		if (iptr != junk) {
			isp_prt(isp, ISP_LOGWARN,
			    "Response Queue Out Pointer Unstable (%x, %x)",
			    iptr, junk);
			goto out;
		}
	} else {
		iptr = ISP_READ(isp, isp->isp_respinrp);
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
		if (IS_24XX(isp)) {
			junk = 0;
		} else if (IS_23XX(isp)) {
			USEC_DELAY(100);
			iptr = ISP_READ(isp, isp->isp_respinrp);
			junk = ISP_READ(isp, BIU_R2HSTSLO);
		} else {
			junk = ISP_READ(isp, BIU_ISR);
		}
		if (optr == iptr) {
			if (IS_23XX(isp) || IS_24XX(isp)) {
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


	if (isp->isp_rspbsy) {
		goto out;
	}
	isp->isp_rspbsy = 1;
	while (optr != iptr) {
		uint8_t qe[QENTRY_LEN];
		ispstatusreq_t *sp = (ispstatusreq_t *) qe;
		isphdr_t *hp;
		int buddaboom, etype, scsi_status, completion_status;
		int req_status_flags, req_state_flags;
		uint8_t *snsp, *resp;
		uint32_t rlen, slen;
		long resid;
		uint16_t oop;

		hp = (isphdr_t *) ISP_QUEUE_ENTRY(isp->isp_result, optr);
		oop = optr;
		optr = ISP_NXT_QENTRY(optr, RESULT_QUEUE_LEN(isp));
		nlooked++;
 read_again:
		buddaboom = req_status_flags = req_state_flags = 0;
		resid = 0L;

		/*
		 * Synchronize our view of this response queue entry.
		 */
		MEMORYBARRIER(isp, SYNC_RESULT, oop, QENTRY_LEN);
		isp_get_hdr(isp, hp, &sp->req_header);
		etype = sp->req_header.rqs_entry_type;

		if (IS_24XX(isp) && etype == RQSTYPE_RESPONSE) {
			isp24xx_statusreq_t *sp2 = (isp24xx_statusreq_t *)qe;
			isp_get_24xx_response(isp,
			    (isp24xx_statusreq_t *)hp, sp2);
			if (isp->isp_dblev & ISP_LOGDEBUG1) {
				isp_print_bytes(isp,
				    "Response Queue Entry", QENTRY_LEN, sp2);
			}
			scsi_status = sp2->req_scsi_status;
			completion_status = sp2->req_completion_status;
			req_state_flags = 0;
			resid = sp2->req_resid;
		} else if (etype == RQSTYPE_RESPONSE) {
			isp_get_response(isp, (ispstatusreq_t *) hp, sp);
			if (isp->isp_dblev & ISP_LOGDEBUG1) {
				isp_print_bytes(isp,
				    "Response Queue Entry", QENTRY_LEN, sp);
			}
			scsi_status = sp->req_scsi_status;
			completion_status = sp->req_completion_status;
			req_status_flags = sp->req_status_flags;
			req_state_flags = sp->req_state_flags;
			resid = sp->req_resid;
		} else if (etype == RQSTYPE_RIO2) {
			isp_rio2_t *rio = (isp_rio2_t *)qe;
			isp_get_rio2(isp, (isp_rio2_t *) hp, rio);
			if (isp->isp_dblev & ISP_LOGDEBUG1) {
				isp_print_bytes(isp,
				    "Response Queue Entry", QENTRY_LEN, rio);
			}
			for (i = 0; i < rio->req_header.rqs_seqno; i++) {
				isp_fastpost_complete(isp, rio->req_handles[i]);
			}
			if (isp->isp_fpcchiwater < rio->req_header.rqs_seqno) {
				isp->isp_fpcchiwater =
				    rio->req_header.rqs_seqno;
			}
			MEMZERO(hp, QENTRY_LEN);	/* PERF */
			continue;
		} else {
			/*
			 * Somebody reachable via isp_handle_other_response
			 * may have updated the response queue pointers for
			 * us, so we reload our goal index.
			 */
			int r;
			r = isp_handle_other_response(isp, etype, hp, &optr);
			if (r < 0) {
				goto read_again;
			}
			if (r > 0) {
				iptr = isp->isp_resodx;
				MEMZERO(hp, QENTRY_LEN);	/* PERF */
				continue;
			}

			/*
			 * After this point, we'll just look at the header as
			 * we don't know how to deal with the rest of the
			 * response.
			 */

			/*
			 * It really has to be a bounced request just copied
			 * from the request queue to the response queue. If
			 * not, something bad has happened.
			 */
			if (etype != RQSTYPE_REQUEST) {
				isp_prt(isp, ISP_LOGERR, notresp,
				    etype, oop, optr, nlooked);
				isp_print_bytes(isp,
				    "Request Queue Entry", QENTRY_LEN, sp);
				MEMZERO(hp, QENTRY_LEN);	/* PERF */
				continue;
			}
			buddaboom = 1;
			scsi_status = sp->req_scsi_status;
			completion_status = sp->req_completion_status;
			req_status_flags = sp->req_status_flags;
			req_state_flags = sp->req_state_flags;
			resid = sp->req_resid;
		}

		if (sp->req_header.rqs_flags & RQSFLAG_MASK) {
			if (sp->req_header.rqs_flags & RQSFLAG_CONTINUATION) {
				isp_prt(isp, ISP_LOGWARN,
				    "continuation segment");
				ISP_WRITE(isp, isp->isp_respoutrp, optr);
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
				isp_print_bytes(isp, "bad header flag",
				    QENTRY_LEN, sp);
				buddaboom++;
			}
			if (sp->req_header.rqs_flags & RQSFLAG_BADPACKET) {
				isp_print_bytes(isp, "bad request packet",
				    QENTRY_LEN, sp);
				buddaboom++;
			}
		}

		if (sp->req_handle > isp->isp_maxcmds || sp->req_handle < 1) {
			isp_prt(isp, ISP_LOGERR,
			    "bad request handle %d (type 0x%x)",
			    sp->req_handle, etype);
			MEMZERO(hp, QENTRY_LEN);	/* PERF */
			ISP_WRITE(isp, isp->isp_respoutrp, optr);
			continue;
		}
		xs = isp_find_xs(isp, sp->req_handle);
		if (xs == NULL) {
			uint8_t ts = completion_status & 0xff;
			/*
			 * Only whine if this isn't the expected fallout of
			 * aborting the command.
			 */
			if (etype != RQSTYPE_RESPONSE) {
				isp_prt(isp, ISP_LOGERR,
				    "cannot find handle 0x%x (type 0x%x)",
				    sp->req_handle, etype);
			} else if (ts != RQCS_ABORTED) {
				isp_prt(isp, ISP_LOGERR,
				    "cannot find handle 0x%x (status 0x%x)",
				    sp->req_handle, ts);
			}
			MEMZERO(hp, QENTRY_LEN);	/* PERF */
			ISP_WRITE(isp, isp->isp_respoutrp, optr);
			continue;
		}
		isp_destroy_handle(isp, sp->req_handle);
		if (req_status_flags & RQSTF_BUS_RESET) {
			XS_SETERR(xs, HBA_BUSRESET);
			isp->isp_sendmarker |= (1 << XS_CHANNEL(xs));
		}
		if (buddaboom) {
			XS_SETERR(xs, HBA_BOTCH);
		}

		resp = NULL;
		rlen = 0;
		snsp = NULL;
		slen = 0;
		if (IS_24XX(isp) && (scsi_status & (RQCS_RV|RQCS_SV)) != 0) {
			resp = ((isp24xx_statusreq_t *)sp)->req_rsp_sense;
			rlen = ((isp24xx_statusreq_t *)sp)->req_response_len;
		} else if (IS_FC(isp) && (scsi_status & RQCS_RV) != 0) {
			resp = sp->req_response;
			rlen = sp->req_response_len;
		}
		if (IS_FC(isp) && (scsi_status & RQCS_SV) != 0) {
			/*
			 * Fibre Channel F/W doesn't say we got status
			 * if there's Sense Data instead. I guess they
			 * think it goes w/o saying.
			 */
			req_state_flags |= RQSF_GOT_STATUS|RQSF_GOT_SENSE;
			if (IS_24XX(isp)) {
				snsp =
				    ((isp24xx_statusreq_t *)sp)->req_rsp_sense;
				snsp += rlen;
				slen =
				    ((isp24xx_statusreq_t *)sp)->req_sense_len;
			} else {
				snsp = sp->req_sense_data;
				slen = sp->req_sense_len;
			}
		} else if (IS_SCSI(isp) && (req_state_flags & RQSF_GOT_SENSE)) {
			snsp = sp->req_sense_data;
			slen = sp->req_sense_len;
		}
		if (req_state_flags & RQSF_GOT_STATUS) {
			*XS_STSP(xs) = scsi_status & 0xff;
		}

		switch (etype) {
		case RQSTYPE_RESPONSE:
			XS_SET_STATE_STAT(isp, xs, sp);
			if (resp && rlen >= 4 &&
			    resp[FCP_RSPNS_CODE_OFFSET] != 0) {
				isp_prt(isp, ISP_LOGWARN,
				    "%d.%d.%d FCP RESPONSE: 0x%x",
				    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs),
				    resp[FCP_RSPNS_CODE_OFFSET]);
				XS_SETERR(xs, HBA_BOTCH);
			}
			if (IS_24XX(isp)) {
				isp_parse_status_24xx(isp,
				    (isp24xx_statusreq_t *)sp, xs, &resid);
			} else {
				isp_parse_status(isp, (void *)sp, xs, &resid);
			}
			if ((XS_NOERR(xs) || XS_ERR(xs) == HBA_NOERROR) &&
			    (*XS_STSP(xs) == SCSI_BUSY)) {
				XS_SETERR(xs, HBA_TGTBSY);
			}
			if (IS_SCSI(isp)) {
				XS_RESID(xs) = resid;
				/*
				 * A new synchronous rate was negotiated for
				 * this target. Mark state such that we'll go
				 * look up that which has changed later.
				 */
				if (req_status_flags & RQSTF_NEGOTIATION) {
					int t = XS_TGT(xs);
					sdparam *sdp = isp->isp_param;
					sdp += XS_CHANNEL(xs);
					sdp->isp_devparam[t].dev_refresh = 1;
					isp->isp_update |=
					    (1 << XS_CHANNEL(xs));
				}
			} else {
				if (req_status_flags & RQSF_XFER_COMPLETE) {
					XS_RESID(xs) = 0;
				} else if (scsi_status & RQCS_RESID) {
					XS_RESID(xs) = resid;
				} else {
					XS_RESID(xs) = 0;
				}
			}
			if (snsp && slen) {
				XS_SAVE_SENSE(xs, snsp, slen);
			}
			isp_prt(isp, ISP_LOGDEBUG2,
			   "asked for %ld got raw resid %ld settled for %ld",
			    (long) XS_XFRLEN(xs), resid, (long) XS_RESID(xs));
			break;
		case RQSTYPE_REQUEST:
		case RQSTYPE_A64:
		case RQSTYPE_T2RQS:
		case RQSTYPE_T3RQS:
		case RQSTYPE_T7RQS:
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
				XS_SETERR(xs, HBA_BOTCH);
				isp_prt(isp, ISP_LOGDEBUG0,
				    "Request Queue Entry bounced back");
				if ((isp->isp_dblev & ISP_LOGDEBUG1) == 0) {
					isp_print_bytes(isp, "Bounced Request",
					    QENTRY_LEN, qe);
				}
			}
			XS_RESID(xs) = XS_XFRLEN(xs);
			break;
		default:
			isp_print_bytes(isp, "Unhandled Response Type",
			    QENTRY_LEN, qe);
			if (XS_NOERR(xs)) {
				XS_SETERR(xs, HBA_BOTCH);
			}
			break;
		}

		/*
		 * Free any DMA resources. As a side effect, this may
		 * also do any cache flushing necessary for data coherence.
		 */
		if (XS_XFRLEN(xs)) {
			ISP_DMAFREE(isp, xs, sp->req_handle);
		}

		if (((isp->isp_dblev & (ISP_LOGDEBUG2|ISP_LOGDEBUG3))) ||
		    ((isp->isp_dblev & ISP_LOGDEBUG0) && ((!XS_NOERR(xs)) ||
		    (*XS_STSP(xs) != SCSI_GOOD)))) {
			char skey;
			if (req_state_flags & RQSF_GOT_SENSE) {
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
		ISP_WRITE(isp, isp->isp_respoutrp, optr);
		/*
		 * While we're at it, read the requst queue out pointer.
		 */
		isp->isp_reqodx = ISP_READ(isp, isp->isp_rqstoutrp);
		if (isp->isp_rscchiwater < ndone) {
			isp->isp_rscchiwater = ndone;
		}
	}

out:

	if (IS_24XX(isp)) {
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_RISC_INT);
	} else {
		ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
		ISP_WRITE(isp, BIU_SEMA, 0);
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
isp_parse_async(ispsoftc_t *isp, uint16_t mbox)
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
		if (isp_target_async(isp, bus, mbox)) {
			rval = -1;
		}
#endif
		isp_async(isp, ISPASYNC_BUS_RESET, &bus);
		break;
	case ASYNC_SYSTEM_ERROR:
		isp->isp_state = ISP_CRASHED;
		if (IS_FC(isp)) {
			FCPARAM(isp)->isp_loopstate = LOOP_NIL;
			FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		}
		/*
		 * Were we waiting for a mailbox command to complete?
		 * If so, it's dead, so wake up the waiter.
		 */
		if (isp->isp_mboxbsy) {
			isp->isp_obits = 1;
			isp->isp_mboxtmp[0] = MBOX_HOST_INTERFACE_ERROR;
			MBOX_NOTIFY_COMPLETE(isp);
		}
		/*
		 * It's up to the handler for isp_async to reinit stuff and
		 * restart the firmware
		 */
		isp_async(isp, ISPASYNC_FW_CRASH, NULL);
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
		mbox = ISP_READ(isp, isp->isp_rqstoutrp);
		break;

	case ASYNC_TIMEOUT_RESET:
		isp_prt(isp, ISP_LOGWARN,
		    "timeout initiated SCSI bus reset of bus %d", bus);
		isp->isp_sendmarker |= (1 << bus);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox)) {
			rval = -1;
		}
#endif
		break;

	case ASYNC_DEVICE_RESET:
		isp_prt(isp, ISP_LOGINFO, "device reset on bus %d", bus);
		isp->isp_sendmarker |= (1 << bus);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox)) {
			rval = -1;
		}
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
		if (isp_target_async(isp, handle, mbox)) {
			rval = -1;
		} else {
			/* count it as a fast posting intr */
			isp->isp_fphccmplt++;
		}
#else
		isp_prt(isp, ISP_LOGINFO, "Fast Posting CTIO done");
		isp->isp_fphccmplt++;	/* count it as a fast posting intr */
#endif
		break;
	}
	case ASYNC_LIP_ERROR:
	case ASYNC_LIP_F8:
	case ASYNC_LIP_OCCURRED:
		FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp)->isp_loopstate = LOOP_LIP_RCVD;
		isp->isp_sendmarker = 1;
		ISP_MARK_PORTDB(isp, 1);
		isp_async(isp, ISPASYNC_LIP, NULL);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox)) {
			rval = -1;
		}
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
		ISP_MARK_PORTDB(isp, 1);
		isp_async(isp, ISPASYNC_LOOP_UP, NULL);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox)) {
			rval = -1;
		}
#endif
		break;

	case ASYNC_LOOP_DOWN:
		isp->isp_sendmarker = 1;
		FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp)->isp_loopstate = LOOP_NIL;
		ISP_MARK_PORTDB(isp, 1);
		isp_async(isp, ISPASYNC_LOOP_DOWN, NULL);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox)) {
			rval = -1;
		}
#endif
		break;

	case ASYNC_LOOP_RESET:
		isp->isp_sendmarker = 1;
		FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp)->isp_loopstate = LOOP_NIL;
		ISP_MARK_PORTDB(isp, 1);
		isp_async(isp, ISPASYNC_LOOP_RESET, NULL);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox)) {
			rval = -1;
		}
#endif
		break;

	case ASYNC_PDB_CHANGED:
		isp->isp_sendmarker = 1;
		FCPARAM(isp)->isp_loopstate = LOOP_PDB_RCVD;
		ISP_MARK_PORTDB(isp, 1);
		isp_async(isp, ISPASYNC_CHANGE_NOTIFY, ISPASYNC_CHANGE_PDB);
		break;

	case ASYNC_CHANGE_NOTIFY:
	    	if (FCPARAM(isp)->isp_topo == TOPO_F_PORT) {
			FCPARAM(isp)->isp_loopstate = LOOP_LSCAN_DONE;
		} else {
			FCPARAM(isp)->isp_loopstate = LOOP_PDB_RCVD;
		}
		ISP_MARK_PORTDB(isp, 1);
		isp_async(isp, ISPASYNC_CHANGE_NOTIFY, ISPASYNC_CHANGE_SNS);
		break;

	case ASYNC_PTPMODE:
		ISP_MARK_PORTDB(isp, 1);
		isp->isp_sendmarker = 1;
		FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp)->isp_loopstate = LOOP_LIP_RCVD;
		isp_async(isp, ISPASYNC_CHANGE_NOTIFY, ISPASYNC_CHANGE_OTHER);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, bus, mbox)) {
			rval = -1;
		}
#endif
		isp_prt(isp, ISP_LOGINFO, "Point-to-Point mode");
		break;

	case ASYNC_CONNMODE:
		mbox = ISP_READ(isp, OUTMAILBOX1);
		ISP_MARK_PORTDB(isp, 1);
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
			isp_async(isp, ISPASYNC_FW_CRASH, NULL);
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

	case ASYNC_RJT_SENT:	/* same as ASYNC_QFULL_SENT */
		if (IS_24XX(isp)) {
			isp_prt(isp, ISP_LOGTDEBUG0, "LS_RJT sent");
			break;
		} else if (IS_2200(isp)) {
			isp_prt(isp, ISP_LOGTDEBUG0, "QFULL sent");
			break;
		}
		/* FALLTHROUGH */
	default:
		isp_prt(isp, ISP_LOGWARN, "Unknown Async Code 0x%x", mbox);
		break;
	}

	if (bus & 0x100) {
		int i, nh;
		uint16_t handles[16];

		for (nh = 0, i = 1; i < MAX_MAILBOX(isp); i++) {
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
		if (isp->isp_fpcchiwater < nh) {
			isp->isp_fpcchiwater = nh;
		}
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
isp_handle_other_response(ispsoftc_t *isp, int type,
    isphdr_t *hp, uint32_t *optrp)
{
	switch (type) {
	case RQSTYPE_STATUS_CONT:
		isp_prt(isp, ISP_LOGDEBUG0, "Ignored Continuation Response");
		return (1);
	case RQSTYPE_MARKER:
		isp_prt(isp, ISP_LOGDEBUG0, "Marker Response");
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
	case RQSTYPE_CTIO7:
	case RQSTYPE_ABTS_RCVD:
	case RQSTYPE_ABTS_RSP:
		isp->isp_rsltccmplt++;	/* count as a response completion */
#ifdef	ISP_TARGET_MODE
		if (isp_target_notify(isp, (ispstatusreq_t *) hp, optrp)) {
			return (1);
		}
#endif
		/* FALLTHROUGH */
	case RQSTYPE_REQUEST:
	default:
		USEC_DELAY(100);
		if (type != isp_get_response_type(isp, hp)) {
			/*
			 * This is questionable- we're just papering over
			 * something we've seen on SMP linux in target
			 * mode- we don't really know what's happening
			 * here that causes us to think we've gotten
			 * an entry, but that either the entry isn't
			 * filled out yet or our CPU read data is stale.
			 */
			isp_prt(isp, ISP_LOGINFO,
				"unstable type in response queue");
			return (-1);
		}
		isp_prt(isp, ISP_LOGWARN, "Unhandled Response Type 0x%x",
		    isp_get_response_type(isp, hp));
		if (isp_async(isp, ISPASYNC_UNHANDLED_RESPONSE, hp)) {
			return (1);
		}
		return (0);
	}
}

static void
isp_parse_status(ispsoftc_t *isp, ispstatusreq_t *sp, XS_T *xs, long *rp)
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
				*rp = XS_XFRLEN(xs);
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
		*rp = XS_XFRLEN(xs);
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
		*rp = XS_XFRLEN(xs);
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
		*rp = XS_XFRLEN(xs);
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
	 	 * XXX: Check to see if we logged out of the device.
		 */
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
	{
		const char *reason;
		uint8_t sts = sp->req_completion_status & 0xff;

		/*
		 * It was there (maybe)- treat as a selection timeout.
		 */
		if (sts == RQCS_PORT_UNAVAILABLE) {
			reason = "unavailable";
		} else {
			reason = "logout";
		}

		isp_prt(isp, ISP_LOGINFO, "port %s for target %d",
		    reason, XS_TGT(xs));

		/*
		 * If we're on a local loop, force a LIP (which is overkill)
		 * to force a re-login of this unit. If we're on fabric,
		 * then we'll have to log in again as a matter of course.
		 */
		if (FCPARAM(isp)->isp_topo == TOPO_NL_PORT ||
		    FCPARAM(isp)->isp_topo == TOPO_FL_PORT) {
			mbreg_t mbs;
			MEMZERO(&mbs, sizeof (mbs));
			mbs.param[0] = MBOX_INIT_LIP;
			if (FCPARAM(isp)->isp_2klogin) {
				mbs.ibits = (1 << 10);
			}
			mbs.logval = MBLOGALL;
			isp_mboxcmd_qnw(isp, &mbs, 1);
		}
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
		}
		return;
	}
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
isp_parse_status_24xx(ispsoftc_t *isp, isp24xx_statusreq_t *sp,
    XS_T *xs, long *rp)
{
	int ru_marked, sv_marked;
	switch (sp->req_completion_status) {
	case RQCS_COMPLETE:
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_NOERROR);
		}
		return;

	case RQCS_DMA_ERROR:
		isp_prt(isp, ISP_LOGERR, "DMA error for command on %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

	case RQCS_TRANSPORT_ERROR:
		isp_prt(isp, ISP_LOGERR, "transport error for %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		break;

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
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_CMDTIMEOUT);
		}
		return;

	case RQCS_DATA_OVERRUN:
		XS_RESID(xs) = sp->req_resid;
		isp_prt(isp, ISP_LOGERR,
		    "data overrun for command on %d.%d.%d",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_DATAOVR);
		}
		return;

	case RQCS_24XX_DRE:	/* data reassembly error */
		isp_prt(isp, ISP_LOGERR, "data reassembly error for target %d",
		    XS_TGT(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_ABORTED);
		}
		*rp = XS_XFRLEN(xs);
		return;

	case RQCS_24XX_TABORT:	/* aborted by target */
		isp_prt(isp, ISP_LOGERR, "target %d sent ABTS",
		    XS_TGT(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_ABORTED);
		}
		return;

	case RQCS_DATA_UNDERRUN:
		ru_marked = (sp->req_scsi_status & RQCS_RU) != 0;
		/*
		 * We can get an underrun w/o things being marked 
		 * if we got a non-zero status.
		 */
		sv_marked = (sp->req_scsi_status & (RQCS_SV|RQCS_RV)) != 0;
		if ((ru_marked == 0 && sv_marked == 0) ||
		    (sp->req_resid > XS_XFRLEN(xs))) {
			isp_prt(isp, ISP_LOGWARN, bun, XS_TGT(xs),
			    XS_LUN(xs), XS_XFRLEN(xs), sp->req_resid,
			    (ru_marked)? "marked" : "not marked");
			if (XS_NOERR(xs)) {
				XS_SETERR(xs, HBA_BOTCH);
			}
			return;
		}
		XS_RESID(xs) = sp->req_resid;
		isp_prt(isp, ISP_LOGDEBUG0,
		    "%d.%d.%d data underrun (%d) for command 0x%x",
		    XS_CHANNEL(xs), XS_TGT(xs), XS_LUN(xs),
		    sp->req_resid, XS_CDBP(xs)[0] & 0xff);
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_NOERROR);
		}
		return;

	case RQCS_PORT_UNAVAILABLE:
		/*
		 * No such port on the loop. Moral equivalent of SELTIMEO
		 */
	case RQCS_PORT_LOGGED_OUT:
	{
		const char *reason;
		uint8_t sts = sp->req_completion_status & 0xff;

		/*
		 * It was there (maybe)- treat as a selection timeout.
		 */
		if (sts == RQCS_PORT_UNAVAILABLE) {
			reason = "unavailable";
		} else {
			reason = "logout";
		}

		isp_prt(isp, ISP_LOGINFO, "port %s for target %d",
		    reason, XS_TGT(xs));

		/*
		 * If we're on a local loop, force a LIP (which is overkill)
		 * to force a re-login of this unit. If we're on fabric,
		 * then we'll have to log in again as a matter of course.
		 */
		if (FCPARAM(isp)->isp_topo == TOPO_NL_PORT ||
		    FCPARAM(isp)->isp_topo == TOPO_FL_PORT) {
			mbreg_t mbs;
			MEMZERO(&mbs, sizeof (mbs));
			mbs.param[0] = MBOX_INIT_LIP;
			if (FCPARAM(isp)->isp_2klogin) {
				mbs.ibits = (1 << 10);
			}
			mbs.logval = MBLOGALL;
			isp_mboxcmd_qnw(isp, &mbs, 1);
		}
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
		}
		return;
	}
	case RQCS_PORT_CHANGED:
		isp_prt(isp, ISP_LOGWARN,
		    "port changed for target %d", XS_TGT(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
		}
		return;


	case RQCS_24XX_ENOMEM:	/* f/w resource unavailable */
		isp_prt(isp, ISP_LOGWARN,
		    "f/w resource unavailable for target %d", XS_TGT(xs));
		if (XS_NOERR(xs)) {
			*XS_STSP(xs) = SCSI_BUSY;
			XS_SETERR(xs, HBA_TGTBSY);
		}
		return;

	case RQCS_24XX_TMO:	/* task management overrun */
		isp_prt(isp, ISP_LOGWARN,
		    "command for target %d overlapped task management",
		    XS_TGT(xs));
		if (XS_NOERR(xs)) {
			*XS_STSP(xs) = SCSI_BUSY;
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
isp_fastpost_complete(ispsoftc_t *isp, uint16_t fph)
{
	XS_T *xs;

	if (fph == 0) {
		return;
	}
	xs = isp_find_xs(isp, fph);
	if (xs == NULL) {
		isp_prt(isp, ISP_LOGDEBUG1,
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
isp_mbox_continue(ispsoftc_t *isp)
{
	mbreg_t mbs;
	uint16_t *ptr;
	uint32_t offset;

	switch (isp->isp_lastmbxcmd) {
	case MBOX_WRITE_RAM_WORD:
	case MBOX_READ_RAM_WORD:
	case MBOX_WRITE_RAM_WORD_EXTENDED:
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
	if (IS_24XX(isp)) {
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_RISC_INT);
	} else {
		ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
		ISP_WRITE(isp, BIU_SEMA, 0);
	}

	/*
	 * Continue with next word.
	 */
	MEMZERO(&mbs, sizeof (mbs));
	ptr = isp->isp_mbxworkp;
	switch (isp->isp_lastmbxcmd) {
	case MBOX_WRITE_RAM_WORD:
		mbs.param[1] = isp->isp_mbxwrk1++;;
		mbs.param[2] = *ptr++;;
		break;
	case MBOX_READ_RAM_WORD:
		*ptr++ = isp->isp_mboxtmp[2];
		mbs.param[1] = isp->isp_mbxwrk1++;
		break;
	case MBOX_WRITE_RAM_WORD_EXTENDED:
		offset = isp->isp_mbxwrk1;
		offset |= isp->isp_mbxwrk8 << 16;

		mbs.param[2] = *ptr++;;
		mbs.param[1] = offset;
		mbs.param[8] = offset >> 16;
		isp->isp_mbxwrk1 = ++offset;
		isp->isp_mbxwrk8 = offset >> 16;
		break;
	case MBOX_READ_RAM_WORD_EXTENDED:
		offset = isp->isp_mbxwrk1;
		offset |= isp->isp_mbxwrk8 << 16;

		*ptr++ = isp->isp_mboxtmp[2];
		mbs.param[1] = offset;
		mbs.param[8] = offset >> 16;
		isp->isp_mbxwrk1 = ++offset;
		isp->isp_mbxwrk8 = offset >> 16;
		break;
	}
	isp->isp_mbxworkp = ptr;
	isp->isp_mbxwrk0--;
	mbs.param[0] = isp->isp_lastmbxcmd;
	mbs.logval = MBLOGALL;
	isp_mboxcmd_qnw(isp, &mbs, 0);
	return (0);
}

#define	HIWRD(x)			((x) >> 16)
#define	LOWRD(x)			((x)  & 0xffff)
#define	ISPOPMAP(a, b)			(((a) << 16) | (b))
static const uint32_t mbpscsi[] = {
	ISPOPMAP(0x01, 0x01),	/* 0x00: MBOX_NO_OP */
	ISPOPMAP(0x1f, 0x01),	/* 0x01: MBOX_LOAD_RAM */
	ISPOPMAP(0x03, 0x01),	/* 0x02: MBOX_EXEC_FIRMWARE */
	ISPOPMAP(0x1f, 0x01),	/* 0x03: MBOX_DUMP_RAM */
	ISPOPMAP(0x07, 0x07),	/* 0x04: MBOX_WRITE_RAM_WORD */
	ISPOPMAP(0x03, 0x07),	/* 0x05: MBOX_READ_RAM_WORD */
	ISPOPMAP(0x3f, 0x3f),	/* 0x06: MBOX_MAILBOX_REG_TEST */
	ISPOPMAP(0x07, 0x07),	/* 0x07: MBOX_VERIFY_CHECKSUM	*/
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
	ISPOPMAP(0xcf, 0x01),	/* 0x54: EXECUCUTE COMMAND IOCB A64 */
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

static const char *scsi_mbcmd_names[] = {
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

static const uint32_t mbpfc[] = {
	ISPOPMAP(0x01, 0x01),	/* 0x00: MBOX_NO_OP */
	ISPOPMAP(0x1f, 0x01),	/* 0x01: MBOX_LOAD_RAM */
	ISPOPMAP(0x0f, 0x01),	/* 0x02: MBOX_EXEC_FIRMWARE */
	ISPOPMAP(0xdf, 0x01),	/* 0x03: MBOX_DUMP_RAM */
	ISPOPMAP(0x07, 0x07),	/* 0x04: MBOX_WRITE_RAM_WORD */
	ISPOPMAP(0x03, 0x07),	/* 0x05: MBOX_READ_RAM_WORD */
	ISPOPMAP(0xff, 0xff),	/* 0x06: MBOX_MAILBOX_REG_TEST */
	ISPOPMAP(0x03, 0x07),	/* 0x07: MBOX_VERIFY_CHECKSUM	*/
	ISPOPMAP(0x01, 0x4f),	/* 0x08: MBOX_ABOUT_FIRMWARE */
	ISPOPMAP(0xdf, 0x01),	/* 0x09: MBOX_LOAD_RISC_RAM_2100 */
	ISPOPMAP(0xdf, 0x01),	/* 0x0a: DUMP RAM */
	ISPOPMAP(0x1ff, 0x01),	/* 0x0b: MBOX_LOAD_RISC_RAM */
	ISPOPMAP(0x00, 0x00),	/* 0x0c: */
	ISPOPMAP(0x10f, 0x01),	/* 0x0d: MBOX_WRITE_RAM_WORD_EXTENDED */
	ISPOPMAP(0x01, 0x05),	/* 0x0e: MBOX_CHECK_FIRMWARE */
	ISPOPMAP(0x10f, 0x05),	/* 0x0f: MBOX_READ_RAM_WORD_EXTENDED */
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
	ISPOPMAP(0x03, 0x07),	/* 0x42: MBOX_GET_RESOURCE_COUNT */
	ISPOPMAP(0x01, 0x01),	/* 0x43: MBOX_REQUEST_OFFLINE_MODE */
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
	ISPOPMAP(0xcd, 0x01),	/* 0x60: MBOX_INIT_FIRMWARE */
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
	ISPOPMAP(0x0f, 0x01)	/* 0x7e: LUN RESET */
};
/*
 * Footnotes
 *
 * (1): this sets bits 21..16 in mailbox register #8, which we nominally 
 *	do not access at this time in the core driver. The caller is
 *	responsible for setting this register first (Gross!). The assumption
 *	is that we won't overflow.
 */

static const char *fc_mbcmd_names[] = {
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
	"WRITE RAM WORD EXTENDED",
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
	"GET RESOURCE COUNT",
	"REQUEST NON PARTICIPATING MODE",
	NULL,
	NULL,
	NULL,
	"GET PORT DATABASE ENHANCED",
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

static void
isp_mboxcmd_qnw(ispsoftc_t *isp, mbreg_t *mbp, int nodelay)
{
	unsigned int ibits, obits, box, opcode;
	const uint32_t *mcp;

	if (IS_FC(isp)) {
		mcp = mbpfc;
	} else {
		mcp = mbpscsi;
	}
	opcode = mbp->param[0];
	ibits = HIWRD(mcp[opcode]) & NMBOX_BMASK(isp);
	obits = LOWRD(mcp[opcode]) & NMBOX_BMASK(isp);
	ibits |= mbp->ibits;
	obits |= mbp->obits;
	for (box = 0; box < MAX_MAILBOX(isp); box++) {
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
	if (IS_24XX(isp)) {
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_SET_HOST_INT);
	} else {
		ISP_WRITE(isp, HCCR, HCCR_CMD_SET_HOST_INT);
	}
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
isp_mboxcmd(ispsoftc_t *isp, mbreg_t *mbp)
{
	const char *cname, *xname;
	char tname[16], mname[16];
	unsigned int lim, ibits, obits, box, opcode;
	const uint32_t *mcp;

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

	ibits = HIWRD(mcp[opcode]) & NMBOX_BMASK(isp);
	obits = LOWRD(mcp[opcode]) & NMBOX_BMASK(isp);

	/*
	 * Pick up any additional bits that the caller might have set.
	 */
	ibits |= mbp->ibits;
	obits |= mbp->obits;

	if (ibits == 0 && obits == 0) {
		mbp->param[0] = MBOX_COMMAND_PARAM_ERROR;
		isp_prt(isp, ISP_LOGERR, "no parameters for 0x%x", opcode);
		return;
	}

	/*
	 * Get exclusive usage of mailbox registers.
	 */
	if (MBOX_ACQUIRE(isp)) {
		mbp->param[0] = MBOX_REGS_BUSY;
		goto out;
	}

	for (box = 0; box < MAX_MAILBOX(isp); box++) {
		if (ibits & (1 << box)) {
			isp_prt(isp, ISP_LOGDEBUG1, "IN mbox %d = 0x%04x", box,
			    mbp->param[box]);
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
	if (IS_24XX(isp)) {
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_SET_HOST_INT);
	} else {
		ISP_WRITE(isp, HCCR, HCCR_CMD_SET_HOST_INT);
	}

	/*
	 * While we haven't finished the command, spin our wheels here.
	 */
	MBOX_WAIT_COMPLETE(isp, mbp);

	/*
	 * Did the command time out?
	 */
	if (mbp->param[0] == MBOX_TIMEOUT) {
		MBOX_RELEASE(isp);
		goto out;
	}

	/*
	 * Copy back output registers.
	 */
	for (box = 0; box < MAX_MAILBOX(isp); box++) {
		if (obits & (1 << box)) {
			mbp->param[box] = isp->isp_mboxtmp[box];
			isp_prt(isp, ISP_LOGDEBUG1, "OUT mbox %d = 0x%04x", box,
			    mbp->param[box]);
		}
	}

	MBOX_RELEASE(isp);
 out:
	isp->isp_mboxbsy = 0;
	if (mbp->logval == 0 || opcode == MBOX_EXEC_FIRMWARE) {
		return;
	}
	cname = (IS_FC(isp))? fc_mbcmd_names[opcode] : scsi_mbcmd_names[opcode];
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
		if (mbp->logval & MBLOGMASK(MBOX_COMMAND_COMPLETE)) {
			xname = "INVALID COMMAND";
		}
		break;
	case MBOX_HOST_INTERFACE_ERROR:
		if (mbp->logval & MBLOGMASK(MBOX_HOST_INTERFACE_ERROR)) {
			xname = "HOST INTERFACE ERROR";
		}
		break;
	case MBOX_TEST_FAILED:
		if (mbp->logval & MBLOGMASK(MBOX_TEST_FAILED)) {
			xname = "TEST FAILED";
		}
		break;
	case MBOX_COMMAND_ERROR:
		if (mbp->logval & MBLOGMASK(MBOX_COMMAND_ERROR)) {
			xname = "COMMAND ERROR";
		}
		break;
	case MBOX_COMMAND_PARAM_ERROR:
		if (mbp->logval & MBLOGMASK(MBOX_COMMAND_PARAM_ERROR)) {
			xname = "COMMAND PARAMETER ERROR";
		}
		break;
	case MBOX_LOOP_ID_USED:
		if (mbp->logval & MBLOGMASK(MBOX_LOOP_ID_USED)) {
			xname = "LOOP ID ALREADY IN USE";
		}
		break;
	case MBOX_PORT_ID_USED:
		if (mbp->logval & MBLOGMASK(MBOX_PORT_ID_USED)) {
			xname = "PORT ID ALREADY IN USE";
		}
		break;
	case MBOX_ALL_IDS_USED:
		if (mbp->logval & MBLOGMASK(MBOX_ALL_IDS_USED)) {
			xname = "ALL LOOP IDS IN USE";
		}
		break;
	case MBOX_REGS_BUSY:
		xname = "REGISTERS BUSY";
		break;
	case MBOX_TIMEOUT:
		xname = "TIMEOUT";
		break;
	default:
		SNPRINTF(mname, sizeof mname, "error 0x%x", mbp->param[0]);
		xname = mname;
		break;
	}
	if (xname) {
		isp_prt(isp, ISP_LOGALL, "Mailbox Command '%s' failed (%s)",
		    cname, xname);
	}
}

static void
isp_fw_state(ispsoftc_t *isp)
{
	if (IS_FC(isp)) {
		mbreg_t mbs;
		fcparam *fcp = isp->isp_param;

		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_GET_FW_STATE;
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			fcp->isp_fwstate = mbs.param[1];
		}
	}
}

static void
isp_update(ispsoftc_t *isp)
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
isp_update_bus(ispsoftc_t *isp, int bus)
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
		uint16_t flags, period, offset;
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

		MEMZERO(&mbs, sizeof (mbs));

		/*
		 * Refresh overrides set
		 */
		if (sdp->isp_devparam[tgt].dev_refresh) {
			mbs.param[0] = MBOX_GET_TARGET_PARAMS;
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

			if (mbs.param[2] & DPARM_SYNC) {
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
			get = 0;
		} else {
			continue;
		}
		mbs.param[1] = (bus << 15) | (tgt << 8);
		mbs.logval = MBLOGALL;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			continue;
		}
		if (get == 0) {
			isp->isp_sendmarker |= (1 << bus);
			sdp->isp_devparam[tgt].dev_update = 0;
			sdp->isp_devparam[tgt].dev_refresh = 1;
		} else {
			sdp->isp_devparam[tgt].dev_refresh = 0;
			flags = mbs.param[2];
			period = mbs.param[3] & 0xff;
			offset = mbs.param[3] >> 8;
			sdp->isp_devparam[tgt].actv_flags = flags;
			sdp->isp_devparam[tgt].actv_period = period;
			sdp->isp_devparam[tgt].actv_offset = offset;
			get = (bus << 16) | tgt;
			(void) isp_async(isp, ISPASYNC_NEW_TGT_PARAMS, &get);
		}
	}

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		if (sdp->isp_devparam[tgt].dev_update ||
		    sdp->isp_devparam[tgt].dev_refresh) {
			isp->isp_update |= (1 << bus);
			break;
		}
	}
}

#ifndef	DEFAULT_EXEC_THROTTLE
#define	DEFAULT_EXEC_THROTTLE(isp)	ISP_EXEC_THROTTLE
#endif

static void
isp_setdfltparm(ispsoftc_t *isp, int channel)
{
	int tgt;
	sdparam *sdp;

	sdp = (sdparam *) isp->isp_param;
	sdp += channel;

	/*
	 * Been there, done that, got the T-shirt...
	 */
	if (sdp->isp_gotdparms) {
		return;
	}
	sdp->isp_gotdparms = 1;
	sdp->isp_bad_nvram = 0;
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
		sdp->isp_bad_nvram = 1;
	}

	/*
	 * Now try and see whether we have specific values for them.
	 */
	if ((isp->isp_confopts & ISP_CFG_NONVRAM) == 0) {
		mbreg_t mbs;

		MEMZERO(&mbs, sizeof (mbs));
		mbs.param[0] = MBOX_GET_ACT_NEG_STATE;
		mbs.logval = MBLOGNONE;
		isp_mboxcmd(isp, &mbs);
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
		uint8_t off, per;
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

#ifndef	DEFAULT_FRAMESIZE
#define	DEFAULT_FRAMESIZE(isp)		ICB_DFLT_FRMLEN
#endif
static void
isp_setdfltfcparm(ispsoftc_t *isp)
{
	fcparam *fcp = FCPARAM(isp);

	if (fcp->isp_gotdparms) {
		return;
	}
	fcp->isp_gotdparms = 1;
	fcp->isp_bad_nvram = 0;
	fcp->isp_maxfrmlen = DEFAULT_FRAMESIZE(isp);
	fcp->isp_maxalloc = ICB_DFLT_ALLOC;
	fcp->isp_execthrottle = DEFAULT_EXEC_THROTTLE(isp);
	fcp->isp_retry_delay = ICB_DFLT_RDELAY;
	fcp->isp_retry_count = ICB_DFLT_RCOUNT;
	/* Platform specific.... */
	fcp->isp_loopid = DEFAULT_LOOPID(isp);
	fcp->isp_wwnn_nvram = DEFAULT_NODEWWN(isp);
	fcp->isp_wwpn_nvram = DEFAULT_PORTWWN(isp);
	fcp->isp_fwoptions = 0;
	fcp->isp_fwoptions |= ICBOPT_FAIRNESS;
	fcp->isp_fwoptions |= ICBOPT_PDBCHANGE_AE;
	fcp->isp_fwoptions |= ICBOPT_HARD_ADDRESS;
	fcp->isp_fwoptions |= ICBOPT_FAST_POST;
	if (isp->isp_confopts & ISP_CFG_FULL_DUPLEX) {
		fcp->isp_fwoptions |= ICBOPT_FULL_DUPLEX;
	}

	/*
	 * Make sure this is turned off now until we get
	 * extended options from NVRAM
	 */
	fcp->isp_fwoptions &= ~ICBOPT_EXTENDED;

	/*
	 * Now try and read NVRAM unless told to not do so.
	 * This will set fcparam's isp_wwnn_nvram && isp_wwpn_nvram.
	 */
	if ((isp->isp_confopts & ISP_CFG_NONVRAM) == 0) {
		int i, j = 0;
		/*
		 * Give a couple of tries at reading NVRAM.
		 */
		for (i = 0; i < 2; i++) {
			j = isp_read_nvram(isp);
			if (j == 0) {
				break;
			}
		}
		if (j) {
			fcp->isp_bad_nvram = 1;
			isp->isp_confopts |= ISP_CFG_NONVRAM;
			isp->isp_confopts |= ISP_CFG_OWNWWPN;
			isp->isp_confopts |= ISP_CFG_OWNWWNN;
		}
	} else {
		isp->isp_confopts |= ISP_CFG_OWNWWPN|ISP_CFG_OWNWWNN;
	}

	/*
	 * Set node && port to override platform set defaults
	 * unless the nvram read failed (or none was done),
	 * or the platform code wants to use what had been
	 * set in the defaults.
	 */
	if (isp->isp_confopts & ISP_CFG_OWNWWNN) {
		isp_prt(isp, ISP_LOGCONFIG, "Using Node WWN 0x%08x%08x",
		    (uint32_t) (DEFAULT_NODEWWN(isp) >> 32),
		    (uint32_t) (DEFAULT_NODEWWN(isp) & 0xffffffff));
		ISP_NODEWWN(isp) = DEFAULT_NODEWWN(isp);
	} else {
		/*
		 * We always start out with values derived
		 * from NVRAM or our platform default.
		 */
		ISP_NODEWWN(isp) = fcp->isp_wwnn_nvram;
		if (fcp->isp_wwnn_nvram == 0) {
			isp_prt(isp, ISP_LOGCONFIG,
			    "bad WWNN- using default");
			ISP_NODEWWN(isp) = DEFAULT_NODEWWN(isp);
		}
	}
	if (isp->isp_confopts & ISP_CFG_OWNWWPN) {
		isp_prt(isp, ISP_LOGCONFIG, "Using Port WWN 0x%08x%08x",
		    (uint32_t) (DEFAULT_PORTWWN(isp) >> 32),
		    (uint32_t) (DEFAULT_PORTWWN(isp) & 0xffffffff));
		ISP_PORTWWN(isp) = DEFAULT_PORTWWN(isp);
	} else {
		/*
		 * We always start out with values derived
		 * from NVRAM or our platform default.
		 */
		ISP_PORTWWN(isp) = fcp->isp_wwpn_nvram;
		if (fcp->isp_wwpn_nvram == 0) {
			isp_prt(isp, ISP_LOGCONFIG,
			    "bad WWPN- using default");
			ISP_PORTWWN(isp) = DEFAULT_PORTWWN(isp);
		}
	}
}

/*
 * Re-initialize the ISP and complete all orphaned commands
 * with a 'botched' notice. The reset/init routines should
 * not disturb an already active list of commands.
 */

void
isp_reinit(ispsoftc_t *isp)
{
	XS_T *xs;
	uint32_t tmp;

	if (IS_FC(isp)) {
		ISP_MARK_PORTDB(isp, 0);
	}
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
			ISP_DISABLE_INTS(isp);
		}
	} else {
		ISP_DISABLE_INTS(isp);
		if (IS_FC(isp)) {
			/*
			 * If we're in ISP_ROLE_NONE, turn off the lasers.
			 */
			if (!IS_24XX(isp)) {
				ISP_WRITE(isp, BIU2100_CSR, BIU2100_FPM0_REGS);
				ISP_WRITE(isp, FPM_DIAG_CONFIG, FPM_SOFT_RESET);
				ISP_WRITE(isp, BIU2100_CSR, BIU2100_FB_REGS);
				ISP_WRITE(isp, FBM_CMD, FBMCMD_FIFO_RESET_ALL);
				ISP_WRITE(isp, BIU2100_CSR, BIU2100_RISC_REGS);
			}
		}
 	}
	isp->isp_nactive = 0;

	for (tmp = 0; tmp < isp->isp_maxcmds; tmp++) {
		uint32_t handle;

		xs = isp->isp_xflist[tmp];
		if (xs == NULL) {
			continue;
		}
		handle = isp_find_handle(isp, xs);
		if (handle == 0) {
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
#ifdef	ISP_TARGET_MODE
	MEMZERO(isp->isp_tgtlist, isp->isp_maxcmds * sizeof (void **));
#endif
}

/*
 * NVRAM Routines
 */
static int
isp_read_nvram(ispsoftc_t *isp)
{
	int i, amt, retval;
	uint8_t csum, minversion;
	union {
		uint8_t _x[ISP2100_NVRAM_SIZE];
		uint16_t _s[ISP2100_NVRAM_SIZE>>1];
	} _n;
#define	nvram_data	_n._x
#define	nvram_words	_n._s

	if (IS_24XX(isp)) {
		return (isp_read_nvram_2400(isp));
	} else if (IS_FC(isp)) {
		amt = ISP2100_NVRAM_SIZE;
		minversion = 1;
	} else if (IS_ULTRA2(isp)) {
		amt = ISP1080_NVRAM_SIZE;
		minversion = 0;
	} else {
		amt = ISP_NVRAM_SIZE;
		minversion = 2;
	}

	for (i = 0; i < amt>>1; i++) {
		isp_rdnvram_word(isp, i, &nvram_words[i]);
	}

	if (nvram_data[0] != 'I' || nvram_data[1] != 'S' ||
	    nvram_data[2] != 'P') {
		if (isp->isp_bustype != ISP_BT_SBUS) {
			isp_prt(isp, ISP_LOGWARN, "invalid NVRAM header");
			isp_prt(isp, ISP_LOGDEBUG0, "%x %x %x",
			    nvram_data[0], nvram_data[1], nvram_data[2]);
		}
		retval = -1;
		goto out;
	}

	for (csum = 0, i = 0; i < amt; i++) {
		csum += nvram_data[i];
	}
	if (csum != 0) {
		isp_prt(isp, ISP_LOGWARN, "invalid NVRAM checksum");
		retval = -1;
		goto out;
	}

	if (ISP_NVRAM_VERSION(nvram_data) < minversion) {
		isp_prt(isp, ISP_LOGWARN, "version %d NVRAM not understood",
		    ISP_NVRAM_VERSION(nvram_data));
		retval = -1;
		goto out;
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
	retval = 0;
out:
	return (retval);
#undef	nvram_data
#undef	nvram_words
}

static int
isp_read_nvram_2400(ispsoftc_t *isp)
{
	uint8_t *nvram_data = FCPARAM(isp)->isp_scratch;
	int retval = 0;
	uint32_t addr, csum, lwrds, *dptr;
	
	if (isp->isp_port) {
		addr = ISP2400_NVRAM_PORT1_ADDR;
	} else {
		addr = ISP2400_NVRAM_PORT0_ADDR;
	}
	
	dptr = (uint32_t *) nvram_data;
	for (lwrds = 0; lwrds < ISP2400_NVRAM_SIZE >> 2; lwrds++) {
		isp_rd_2400_nvram(isp, addr++, dptr++);
	}
	if (nvram_data[0] != 'I' || nvram_data[1] != 'S' ||
	    nvram_data[2] != 'P') {
		isp_prt(isp, ISP_LOGWARN, "invalid NVRAM header (%x %x %x)",
		    nvram_data[0], nvram_data[1], nvram_data[2]);
		retval = -1;
		goto out;
	}
	dptr = (uint32_t *) nvram_data;
	for (csum = 0, lwrds = 0; lwrds < ISP2400_NVRAM_SIZE >> 2; lwrds++) {
		uint32_t tmp;
		ISP_IOXGET_32(isp, &dptr[lwrds], tmp);
		csum += tmp;
	}
	if (csum != 0) {
		isp_prt(isp, ISP_LOGWARN, "invalid NVRAM checksum");
		retval = -1;
		goto out;
	}
	isp_parse_nvram_2400(isp, nvram_data);
out:
	return (retval);
}

static void
isp_rdnvram_word(ispsoftc_t *isp, int wo, uint16_t *rp)
{
	int i, cbits;
	uint16_t bit, rqst, junk;

	ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT);
	USEC_DELAY(10);
	ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT|BIU_NVRAM_CLOCK);
	USEC_DELAY(10);

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
		USEC_DELAY(10);
		junk = ISP_READ(isp, BIU_NVRAM);	/* force PCI flush */
		ISP_WRITE(isp, BIU_NVRAM, bit | BIU_NVRAM_CLOCK);
		USEC_DELAY(10);
		junk = ISP_READ(isp, BIU_NVRAM);	/* force PCI flush */
		ISP_WRITE(isp, BIU_NVRAM, bit);
		USEC_DELAY(10);
		junk = ISP_READ(isp, BIU_NVRAM);	/* force PCI flush */
	}
	/*
	 * Now read the result back in (bits come back in MSB format).
	 */
	*rp = 0;
	for (i = 0; i < 16; i++) {
		uint16_t rv;
		*rp <<= 1;
		ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT|BIU_NVRAM_CLOCK);
		USEC_DELAY(10);
		rv = ISP_READ(isp, BIU_NVRAM);
		if (rv & BIU_NVRAM_DATAIN) {
			*rp |= 1;
		}
		USEC_DELAY(10);
		ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT);
		USEC_DELAY(10);
		junk = ISP_READ(isp, BIU_NVRAM);	/* force PCI flush */
	}
	ISP_WRITE(isp, BIU_NVRAM, 0);
	USEC_DELAY(10);
	junk = ISP_READ(isp, BIU_NVRAM);	/* force PCI flush */
	ISP_SWIZZLE_NVRAM_WORD(isp, rp);
}

static void
isp_rd_2400_nvram(ispsoftc_t *isp, uint32_t addr, uint32_t *rp)
{
	int loops = 0;
	const uint32_t base = 0x7ffe0000;
	uint32_t tmp = 0;

	ISP_WRITE(isp, BIU2400_FLASH_ADDR, base | addr);
	for (loops = 0; loops < 5000; loops++) {
		USEC_DELAY(10);
		tmp = ISP_READ(isp, BIU2400_FLASH_ADDR);
		if ((tmp & (1U << 31)) != 0) {
			break;
		}
	}
	if (tmp & (1U << 31)) {
		*rp = ISP_READ(isp, BIU2400_FLASH_DATA);
		ISP_SWIZZLE_NVRAM_LONG(isp, rp);
	} else {
		*rp = 0xffffffff;
	}
}

static void
isp_parse_nvram_1020(ispsoftc_t *isp, uint8_t *nvram_data)
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
isp_parse_nvram_1080(ispsoftc_t *isp, int bus, uint8_t *nvram_data)
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
isp_parse_nvram_12160(ispsoftc_t *isp, int bus, uint8_t *nvram_data)
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
isp_fix_nvram_wwns(ispsoftc_t *isp)
{
	fcparam *fcp = FCPARAM(isp);

	/*
	 * Make sure we have both Node and Port as non-zero values.
	 */
	if (fcp->isp_wwnn_nvram != 0 && fcp->isp_wwpn_nvram == 0) {
		fcp->isp_wwpn_nvram = fcp->isp_wwnn_nvram;
	} else if (fcp->isp_wwnn_nvram == 0 && fcp->isp_wwpn_nvram != 0) {
		fcp->isp_wwnn_nvram = fcp->isp_wwpn_nvram;
	}

	/*
	 * Make the Node and Port values sane if they're NAA == 2.
	 * This means to clear bits 48..56 for the Node WWN and
	 * make sure that there's some non-zero value in 48..56
	 * for the Port WWN.
	 */
	if (fcp->isp_wwnn_nvram && fcp->isp_wwpn_nvram) {
		if ((fcp->isp_wwnn_nvram & (((uint64_t) 0xfff) << 48)) != 0 &&
		    (fcp->isp_wwnn_nvram >> 60) == 2) {
			fcp->isp_wwnn_nvram &= ~((uint64_t) 0xfff << 48);
		}
		if ((fcp->isp_wwpn_nvram & (((uint64_t) 0xfff) << 48)) == 0 &&
		    (fcp->isp_wwpn_nvram >> 60) == 2) {
			fcp->isp_wwpn_nvram |= ((uint64_t) 1 << 56);
		}
	}
}

static void
isp_parse_nvram_2100(ispsoftc_t *isp, uint8_t *nvram_data)
{
	fcparam *fcp = FCPARAM(isp);
	uint64_t wwn;

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
		    (uint32_t) (wwn >> 32), (uint32_t) (wwn & 0xffffffff));
		if ((wwn >> 60) == 0) {
			wwn |= (((uint64_t) 2)<< 60);
		}
	}
	fcp->isp_wwpn_nvram = wwn;
	if (IS_2200(isp) || IS_23XX(isp)) {
		wwn = ISP2100_NVRAM_NODE_NAME(nvram_data);
		if (wwn) {
			isp_prt(isp, ISP_LOGCONFIG, "NVRAM Node WWN 0x%08x%08x",
			    (uint32_t) (wwn >> 32),
			    (uint32_t) (wwn & 0xffffffff));
			if ((wwn >> 60) == 0) {
				wwn |= (((uint64_t) 2)<< 60);
			}
		}
	} else {
		wwn &= ~((uint64_t) 0xfff << 48);
	}
	fcp->isp_wwnn_nvram = wwn;

	isp_fix_nvram_wwns(isp);

	fcp->isp_maxalloc = ISP2100_NVRAM_MAXIOCBALLOCATION(nvram_data);
	if ((isp->isp_confopts & ISP_CFG_OWNFSZ) == 0) {
		fcp->isp_maxfrmlen = ISP2100_NVRAM_MAXFRAMELENGTH(nvram_data);
	}
	fcp->isp_retry_delay = ISP2100_NVRAM_RETRY_DELAY(nvram_data);
	fcp->isp_retry_count = ISP2100_NVRAM_RETRY_COUNT(nvram_data);
	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0) {
		fcp->isp_loopid = ISP2100_NVRAM_HARDLOOPID(nvram_data);
	}
	if ((isp->isp_confopts & ISP_CFG_OWNEXCTHROTTLE) == 0) {
		fcp->isp_execthrottle =
			ISP2100_NVRAM_EXECUTION_THROTTLE(nvram_data);
	}
	fcp->isp_fwoptions = ISP2100_NVRAM_OPTIONS(nvram_data);
	isp_prt(isp, ISP_LOGDEBUG0,
	    "NVRAM 0x%08x%08x 0x%08x%08x maxalloc %d maxframelen %d",
	    (uint32_t) (fcp->isp_wwnn_nvram >> 32), (uint32_t) fcp->isp_wwnn_nvram,
	    (uint32_t) (fcp->isp_wwpn_nvram >> 32), (uint32_t) fcp->isp_wwpn_nvram,
	    ISP2100_NVRAM_MAXIOCBALLOCATION(nvram_data),
	    ISP2100_NVRAM_MAXFRAMELENGTH(nvram_data));
	isp_prt(isp, ISP_LOGDEBUG0,
	    "execthrottle %d fwoptions 0x%x hardloop %d tov %d",
	    ISP2100_NVRAM_EXECUTION_THROTTLE(nvram_data),
	    ISP2100_NVRAM_OPTIONS(nvram_data),
	    ISP2100_NVRAM_HARDLOOPID(nvram_data),
	    ISP2100_NVRAM_TOV(nvram_data));
	fcp->isp_xfwoptions = ISP2100_XFW_OPTIONS(nvram_data);
	fcp->isp_zfwoptions = ISP2100_ZFW_OPTIONS(nvram_data);
	isp_prt(isp, ISP_LOGDEBUG0,
	    "xfwoptions 0x%x zfw options 0x%x",
	    ISP2100_XFW_OPTIONS(nvram_data), ISP2100_ZFW_OPTIONS(nvram_data));
}

static void
isp_parse_nvram_2400(ispsoftc_t *isp, uint8_t *nvram_data)
{
	fcparam *fcp = FCPARAM(isp);
	uint64_t wwn;

	isp_prt(isp, ISP_LOGDEBUG0,
	    "NVRAM 0x%08x%08x 0x%08x%08x exchg_cnt %d maxframelen %d",
	    (uint32_t) (ISP2400_NVRAM_NODE_NAME(nvram_data) >> 32),
	    (uint32_t) (ISP2400_NVRAM_NODE_NAME(nvram_data)),
	    (uint32_t) (ISP2400_NVRAM_PORT_NAME(nvram_data) >> 32),
	    (uint32_t) (ISP2400_NVRAM_PORT_NAME(nvram_data)),
	    ISP2400_NVRAM_EXCHANGE_COUNT(nvram_data),
	    ISP2400_NVRAM_MAXFRAMELENGTH(nvram_data));
	isp_prt(isp, ISP_LOGDEBUG0,
	    "NVRAM execthr %d loopid %d fwopt1 0x%x fwopt2 0x%x fwopt3 0x%x",
	    ISP2400_NVRAM_EXECUTION_THROTTLE(nvram_data),
	    ISP2400_NVRAM_HARDLOOPID(nvram_data),
	    ISP2400_NVRAM_FIRMWARE_OPTIONS1(nvram_data),
	    ISP2400_NVRAM_FIRMWARE_OPTIONS2(nvram_data),
	    ISP2400_NVRAM_FIRMWARE_OPTIONS3(nvram_data));

	wwn = ISP2400_NVRAM_PORT_NAME(nvram_data);
	if (wwn) {
		if ((wwn >> 60) != 2 && (wwn >> 60) != 5) {
			wwn = 0;
		}
	}
	fcp->isp_wwpn_nvram = wwn;

	wwn = ISP2400_NVRAM_NODE_NAME(nvram_data);
	if (wwn) {
		if ((wwn >> 60) != 2 && (wwn >> 60) != 5) {
			wwn = 0;
		}
	}
	fcp->isp_wwnn_nvram = wwn;

	isp_fix_nvram_wwns(isp);

	if (ISP2400_NVRAM_EXCHANGE_COUNT(nvram_data)) {
		fcp->isp_maxalloc = ISP2400_NVRAM_EXCHANGE_COUNT(nvram_data);
	}
	if ((isp->isp_confopts & ISP_CFG_OWNFSZ) == 0) {
		fcp->isp_maxfrmlen = ISP2400_NVRAM_MAXFRAMELENGTH(nvram_data);
	}
	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0) {
		fcp->isp_loopid = ISP2400_NVRAM_HARDLOOPID(nvram_data);
	}
	if ((isp->isp_confopts & ISP_CFG_OWNEXCTHROTTLE) == 0) {
		fcp->isp_execthrottle =
			ISP2400_NVRAM_EXECUTION_THROTTLE(nvram_data);
	}
	fcp->isp_fwoptions = ISP2400_NVRAM_FIRMWARE_OPTIONS1(nvram_data);
	fcp->isp_xfwoptions = ISP2400_NVRAM_FIRMWARE_OPTIONS2(nvram_data);
	fcp->isp_zfwoptions = ISP2400_NVRAM_FIRMWARE_OPTIONS3(nvram_data);
}

#ifdef	ISP_FW_CRASH_DUMP
static void isp2200_fw_dump(ispsoftc_t *);
static void isp2300_fw_dump(ispsoftc_t *);

static void
isp2200_fw_dump(ispsoftc_t *isp)
{
	int i, j;
	mbreg_t mbs;
	uint16_t *ptr;

	MEMZERO(&mbs, sizeof (mbs));
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
	ISP_ENABLE_INTS(isp);
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
	isp_prt(isp, ISP_LOGALL, "isp_fw_dump: SRAM dumped successfully");
	FCPARAM(isp)->isp_dump_data[0] = isp->isp_type; /* now used */
	(void) isp_async(isp, ISPASYNC_FW_DUMPED, 0);
}

static void
isp2300_fw_dump(ispsoftc_t *isp)
{
	int i, j;
	mbreg_t mbs;
	uint16_t *ptr;

	MEMZERO(&mbs, sizeof (mbs));
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
	ISP_ENABLE_INTS(isp);
	MEMZERO(&mbs, sizeof (mbs));
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
	MEMZERO(&mbs, sizeof (mbs));
	mbs.param[0] = MBOX_READ_RAM_WORD_EXTENDED;
	mbs.param[8] = 1;
	isp->isp_mbxworkp = (void *) ptr;
	isp->isp_mbxwrk0 = 0xffff;	/* continuation count */
	isp->isp_mbxwrk1 = 0x1;		/* next SRAM address */
	isp->isp_mbxwrk8 = 0x1;
	isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_prt(isp, ISP_LOGWARN,
		    "RAM DUMP FAILED @ WORD %x", 0x10000 + isp->isp_mbxwrk1);
		return;
	}
	ptr = isp->isp_mbxworkp;	/* finish final word */
	*ptr++ = mbs.param[2];
	isp_prt(isp, ISP_LOGALL, "isp_fw_dump: SRAM dumped successfully");
	FCPARAM(isp)->isp_dump_data[0] = isp->isp_type; /* now used */
	(void) isp_async(isp, ISPASYNC_FW_DUMPED, 0);
}

void
isp_fw_dump(ispsoftc_t *isp)
{
	if (IS_2200(isp))
		isp2200_fw_dump(isp);
	else if (IS_23XX(isp))
		isp2300_fw_dump(isp);
	else if (IS_24XX(isp))
		isp_prt(isp, ISP_LOGERR, "24XX dump method undefined");

}
#endif
