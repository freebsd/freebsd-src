/*-
 *  Copyright (c) 1997-2009 by Matthew Jacob
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
 *
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
#define	ISP_MARK_PORTDB(a, b, c)				\
	do {								\
		isp_prt(isp, ISP_LOG_SANCFG, 				\
		    "Chan %d ISP_MARK_PORTDB@LINE %d", (b), __LINE__);	\
		isp_mark_portdb((a), (b), (c));				\
	} while (0)

/*
 * Local static data
 */
static const char fconf[] = "Chan %d PortDB[%d] changed:\n current =(0x%x@0x%06x 0x%08x%08x 0x%08x%08x)\n database=(0x%x@0x%06x 0x%08x%08x 0x%08x%08x)";
static const char notresp[] = "Not RESPONSE in RESPONSE Queue (type 0x%x) @ idx %d (next %d) nlooked %d";
static const char topology[] = "Chan %d WWPN 0x%08x%08x PortID 0x%06x handle 0x%x, Connection '%s'";
static const char bun[] = "bad underrun (count %d, resid %d, status %s)";
static const char lipd[] = "Chan %d LIP destroyed %d active commands";
static const char sacq[] = "unable to acquire scratch area";

static const uint8_t alpa_map[] = {
	0xef, 0xe8, 0xe4, 0xe2, 0xe1, 0xe0, 0xdc, 0xda,
	0xd9, 0xd6, 0xd5, 0xd4, 0xd3, 0xd2, 0xd1, 0xce,
	0xcd, 0xcc, 0xcb, 0xca, 0xc9, 0xc7, 0xc6, 0xc5,
	0xc3, 0xbc, 0xba, 0xb9, 0xb6, 0xb5, 0xb4, 0xb3,
	0xb2, 0xb1, 0xae, 0xad, 0xac, 0xab, 0xaa, 0xa9,
	0xa7, 0xa6, 0xa5, 0xa3, 0x9f, 0x9e, 0x9d, 0x9b,
	0x98, 0x97, 0x90, 0x8f, 0x88, 0x84, 0x82, 0x81,
	0x80, 0x7c, 0x7a, 0x79, 0x76, 0x75, 0x74, 0x73,
	0x72, 0x71, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x69,
	0x67, 0x66, 0x65, 0x63, 0x5c, 0x5a, 0x59, 0x56,
	0x55, 0x54, 0x53, 0x52, 0x51, 0x4e, 0x4d, 0x4c,
	0x4b, 0x4a, 0x49, 0x47, 0x46, 0x45, 0x43, 0x3c,
	0x3a, 0x39, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31,
	0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x29, 0x27, 0x26,
	0x25, 0x23, 0x1f, 0x1e, 0x1d, 0x1b, 0x18, 0x17,
	0x10, 0x0f, 0x08, 0x04, 0x02, 0x01, 0x00
};

/*
 * Local function prototypes.
 */
static int isp_parse_async(ispsoftc_t *, uint16_t);
static int isp_parse_async_fc(ispsoftc_t *, uint16_t);
static int isp_handle_other_response(ispsoftc_t *, int, isphdr_t *, uint32_t *);
static void isp_parse_status(ispsoftc_t *, ispstatusreq_t *, XS_T *, long *); static void
isp_parse_status_24xx(ispsoftc_t *, isp24xx_statusreq_t *, XS_T *, long *);
static void isp_fastpost_complete(ispsoftc_t *, uint32_t);
static int isp_mbox_continue(ispsoftc_t *);
static void isp_scsi_init(ispsoftc_t *);
static void isp_scsi_channel_init(ispsoftc_t *, int);
static void isp_fibre_init(ispsoftc_t *);
static void isp_fibre_init_2400(ispsoftc_t *);
static void isp_mark_portdb(ispsoftc_t *, int, int);
static int isp_plogx(ispsoftc_t *, int, uint16_t, uint32_t, int, int);
static int isp_port_login(ispsoftc_t *, uint16_t, uint32_t);
static int isp_port_logout(ispsoftc_t *, uint16_t, uint32_t);
static int isp_getpdb(ispsoftc_t *, int, uint16_t, isp_pdb_t *, int);
static void isp_dump_chip_portdb(ispsoftc_t *, int, int);
static uint64_t isp_get_wwn(ispsoftc_t *, int, int, int);
static int isp_fclink_test(ispsoftc_t *, int, int);
static int isp_pdb_sync(ispsoftc_t *, int);
static int isp_scan_loop(ispsoftc_t *, int);
static int isp_gid_ft_sns(ispsoftc_t *, int);
static int isp_gid_ft_ct_passthru(ispsoftc_t *, int);
static int isp_scan_fabric(ispsoftc_t *, int);
static int isp_login_device(ispsoftc_t *, int, uint32_t, isp_pdb_t *, uint16_t *);
static int isp_register_fc4_type(ispsoftc_t *, int);
static int isp_register_fc4_type_24xx(ispsoftc_t *, int);
static uint16_t isp_nxt_handle(ispsoftc_t *, int, uint16_t);
static void isp_fw_state(ispsoftc_t *, int);
static void isp_mboxcmd_qnw(ispsoftc_t *, mbreg_t *, int);
static void isp_mboxcmd(ispsoftc_t *, mbreg_t *);

static void isp_spi_update(ispsoftc_t *, int);
static void isp_setdfltsdparm(ispsoftc_t *);
static void isp_setdfltfcparm(ispsoftc_t *, int);
static int isp_read_nvram(ispsoftc_t *, int);
static int isp_read_nvram_2400(ispsoftc_t *, uint8_t *);
static void isp_rdnvram_word(ispsoftc_t *, int, uint16_t *);
static void isp_rd_2400_nvram(ispsoftc_t *, uint32_t, uint32_t *);
static void isp_parse_nvram_1020(ispsoftc_t *, uint8_t *);
static void isp_parse_nvram_1080(ispsoftc_t *, int, uint8_t *);
static void isp_parse_nvram_12160(ispsoftc_t *, int, uint8_t *);
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
isp_reset(ispsoftc_t *isp, int do_load_defaults)
{
	mbreg_t mbs;
	char *buf;
	uint64_t fwt;
	uint32_t code_org, val;
	int loops, i, dodnld = 1;
	const char *btype = "????";
	static const char dcrc[] = "Downloaded RISC Code Checksum Failure";

	isp->isp_state = ISP_NILSTATE;
	if (isp->isp_dead) {
		isp_shutdown(isp);
		ISP_DISABLE_INTS(isp);
		return;
	}

	/*
	 * Basic types (SCSI, FibreChannel and PCI or SBus)
	 * have been set in the MD code. We figure out more
	 * here. Possibly more refined types based upon PCI
	 * identification. Chip revision has been gathered.
	 *
	 * After we've fired this chip up, zero out the conf1 register
	 * for SCSI adapters and do other settings for the 2100.
	 */

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
	 * Set up DMA for the request and response queues.
	 *
	 * We do this now so we can use the request queue
	 * for dma to load firmware from.
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
		case ISP_HA_FC_2500:
			btype = "2532";
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
		sdparam *sdp;

		btype = "1240";
		isp->isp_clock = 60;
		sdp = SDPARAM(isp, 0);
		sdp->isp_ultramode = 1;
		sdp = SDPARAM(isp, 1);
		sdp->isp_ultramode = 1;
		/*
		 * XXX: Should probably do some bus sensing.
		 */
	} else if (IS_ULTRA3(isp)) {
		sdparam *sdp = isp->isp_param;

		isp->isp_clock = 100;

		if (IS_10160(isp))
			btype = "10160";
		else if (IS_12160(isp))
			btype = "12160";
		else
			btype = "<UNKLVD>";
		sdp->isp_lvdmode = 1;

		if (IS_DUALBUS(isp)) {
			sdp++;
			sdp->isp_lvdmode = 1;
		}
	} else if (IS_ULTRA2(isp)) {
		static const char m[] = "bus %d is in %s Mode";
		uint16_t l;
		sdparam *sdp = SDPARAM(isp, 0);

		isp->isp_clock = 100;

		if (IS_1280(isp))
			btype = "1280";
		else if (IS_1080(isp))
			btype = "1080";
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
			sdp = SDPARAM(isp, 1);
			l = ISP_READ(isp, SXP_PINS_DIFF|SXP_BANK1_SELECT);
			l &= ISP1080_MODE_MASK;
			switch (l) {
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
		sdparam *sdp = SDPARAM(isp, 0);
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
	 * and give it a chance to recover.
	 */

	if (IS_SCSI(isp)) {
		ISP_WRITE(isp, BIU_ICR, BIU_ICR_SOFT_RESET);
		/*
		 * A slight delay...
		 */
		ISP_DELAY(100);

		/*
		 * Clear data && control DMA engines.
		 */
		ISP_WRITE(isp, CDMA_CONTROL, DMA_CNTRL_CLEAR_CHAN | DMA_CNTRL_RESET_INT);
		ISP_WRITE(isp, DDMA_CONTROL, DMA_CNTRL_CLEAR_CHAN | DMA_CNTRL_RESET_INT);


	} else if (IS_24XX(isp)) {
		/*
		 * Stop DMA and wait for it to stop.
		 */
		ISP_WRITE(isp, BIU2400_CSR, BIU2400_DMA_STOP|(3 << 4));
		for (val = loops = 0; loops < 30000; loops++) {
			ISP_DELAY(10);
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
		ISP_WRITE(isp, BIU2400_CSR, BIU2400_SOFT_RESET|BIU2400_DMA_STOP|(3 << 4));
		ISP_DELAY(100);
		for (loops = 0; loops < 10000; loops++) {
			ISP_DELAY(5);
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
		ISP_DELAY(100);

		/*
		 * Clear data && control DMA engines.
		 */
		ISP_WRITE(isp, CDMA2100_CONTROL, DMA_CNTRL2100_CLEAR_CHAN | DMA_CNTRL2100_RESET_INT);
		ISP_WRITE(isp, TDMA2100_CONTROL, DMA_CNTRL2100_CLEAR_CHAN | DMA_CNTRL2100_RESET_INT);
		ISP_WRITE(isp, RDMA2100_CONTROL, DMA_CNTRL2100_CLEAR_CHAN | DMA_CNTRL2100_RESET_INT);
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
		ISP_DELAY(100);
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
		ISP_DELAY(100);
		ISP_WRITE(isp, BIU_SEMA, 0);
	}

	/*
	 * Post-RISC Reset stuff.
	 */
	if (IS_24XX(isp)) {
		for (val = loops = 0; loops < 5000000; loops++) {
			ISP_DELAY(5);
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
		if (SDPARAM(isp, 0)->isp_ptisp) {
			if (SDPARAM(isp, 0)->isp_ultramode) {
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
	if (IS_24XX(isp)) {
		ISP_WRITE(isp, BIU2400_PRI_REQINP, 0);
		ISP_WRITE(isp, BIU2400_PRI_REQOUTP, 0);
		ISP_WRITE(isp, BIU2400_ATIO_RSPINP, 0);
		ISP_WRITE(isp, BIU2400_ATIO_RSPOUTP, 0);
	}

	/*
	 * Do MD specific post initialization
	 */
	ISP_RESET1(isp);

	/*
	 * Wait for everything to finish firing up.
	 *
	 * Avoid doing this on early 2312s because you can generate a PCI
	 * parity error (chip breakage).
	 */
	if (IS_2312(isp) && isp->isp_revision < 2) {
		ISP_DELAY(100);
	} else {
		loops = MBOX_DELAY_COUNT;
		while (ISP_READ(isp, OUTMAILBOX0) == MBOX_BUSY) {
			ISP_DELAY(100);
			if (--loops < 0) {
				ISP_RESET0(isp);
				isp_prt(isp, ISP_LOGERR, "MBOX_BUSY never cleared on reset");
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
	 * Do some sanity checking by running a NOP command.
	 * If it succeeds, the ROM firmware is now running.
	 */
	MBSINIT(&mbs, MBOX_NO_OP, MBLOGALL, 0);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_prt(isp, ISP_LOGERR, "NOP command failed (%x)", mbs.param[0]);
		ISP_RESET0(isp);
		return;
	}

	/*
	 * Do some operational tests
	 */

	if (IS_SCSI(isp) || IS_24XX(isp)) {
		static const uint16_t patterns[MAX_MAILBOX] = {
			0x0000, 0xdead, 0xbeef, 0xffff,
			0xa5a5, 0x5a5a, 0x7f7f, 0x7ff7,
			0x3421, 0xabcd, 0xdcba, 0xfeef,
			0xbead, 0xdebe, 0x2222, 0x3333,
			0x5555, 0x6666, 0x7777, 0xaaaa,
			0xffff, 0xdddd, 0x9999, 0x1fbc,
			0x6666, 0x6677, 0x1122, 0x33ff,
			0x0000, 0x0001, 0x1000, 0x1010,
		};
		int nmbox = ISP_NMBOX(isp);
		if (IS_SCSI(isp))
			nmbox = 6;
		MBSINIT(&mbs, MBOX_MAILBOX_REG_TEST, MBLOGALL, 0);
		for (i = 1; i < nmbox; i++) {
			mbs.param[i] = patterns[i];
		}
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			ISP_RESET0(isp);
			return;
		}
		for (i = 1; i < nmbox; i++) {
			if (mbs.param[i] != patterns[i]) {
				ISP_RESET0(isp);
				isp_prt(isp, ISP_LOGERR, "Register Test Failed at Register %d: should have 0x%04x but got 0x%04x", i, patterns[i], mbs.param[i]);
				return;
			}
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
	if ((isp->isp_mdvec->dv_ispfw == NULL) || (isp->isp_confopts & ISP_CFG_NORELOAD)) {
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
		int wordload;

		/*
		 * Keep loading until we run out of f/w.
		 */
		code_org = ptr[2];	/* 1st load address is our start addr */
		wordload = 0;

		for (;;) {
			uint32_t la, wi, wl;

			isp_prt(isp, ISP_LOGDEBUG0, "load 0x%x words of code at load address 0x%x", ptr[3], ptr[2]);

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
				MEMORYBARRIER(isp, SYNC_REQUEST, 0, ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp)), -1);
	again:
				MBSINIT(&mbs, 0, MBLOGALL, 0);
				if (la < 0x10000 && nw < 0x10000) {
					mbs.param[0] = MBOX_LOAD_RISC_RAM_2100;
					mbs.param[1] = la;
					mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
					mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
					mbs.param[4] = nw;
					mbs.param[6] = DMA_WD3(isp->isp_rquest_dma);
					mbs.param[7] = DMA_WD2(isp->isp_rquest_dma);
					isp_prt(isp, ISP_LOGDEBUG0, "LOAD RISC RAM 2100 %u words at load address 0x%x", nw, la);
				} else if (wordload) {
					union {
						const uint32_t *cp;
						uint32_t *np;
					} ucd;
					ucd.cp = (const uint32_t *)cp;
					mbs.param[0] = MBOX_WRITE_RAM_WORD_EXTENDED;
					mbs.param[1] = la;
					mbs.param[2] = (*ucd.np);
					mbs.param[3] = (*ucd.np) >> 16;
					mbs.param[8] = la >> 16;
					isp->isp_mbxwrk0 = nw - 1;
					isp->isp_mbxworkp = ucd.np+1;
					isp->isp_mbxwrk1 = (la + 1);
					isp->isp_mbxwrk8 = (la + 1) >> 16;
					isp_prt(isp, ISP_LOGDEBUG0, "WRITE RAM WORD EXTENDED %u words at load address 0x%x", nw, la);
				} else {
					mbs.param[0] = MBOX_LOAD_RISC_RAM;
					mbs.param[1] = la;
					mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
					mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
					mbs.param[4] = nw >> 16;
					mbs.param[5] = nw;
					mbs.param[6] = DMA_WD3(isp->isp_rquest_dma);
					mbs.param[7] = DMA_WD2(isp->isp_rquest_dma);
					mbs.param[8] = la >> 16;
					isp_prt(isp, ISP_LOGDEBUG0, "LOAD RISC RAM %u words at load address 0x%x", nw, la);
				}
				isp_mboxcmd(isp, &mbs);
				if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
					if (mbs.param[0] == MBOX_HOST_INTERFACE_ERROR) {
						isp_prt(isp, ISP_LOGERR, "switching to word load");
						wordload = 1;
						goto again;
					}
					isp_prt(isp, ISP_LOGERR, "F/W Risc Ram Load Failed");
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

			isp_prt(isp, ISP_LOGDEBUG0, "load 0x%x words of code at load address 0x%x", ptr[3], la);

			wi = 0;
			wl = ptr[3];

			while (wi < ptr[3]) {
				uint16_t *cp;
				uint16_t nw;

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
				MEMORYBARRIER(isp, SYNC_REQUEST, 0, ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp)), -1);
				MBSINIT(&mbs, 0, MBLOGALL, 0);
				if (la < 0x10000) {
					mbs.param[0] = MBOX_LOAD_RISC_RAM_2100;
					mbs.param[1] = la;
					mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
					mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
					mbs.param[4] = nw;
					mbs.param[6] = DMA_WD3(isp->isp_rquest_dma);
					mbs.param[7] = DMA_WD2(isp->isp_rquest_dma);
					isp_prt(isp, ISP_LOGDEBUG1, "LOAD RISC RAM 2100 %u words at load address 0x%x\n", nw, la);
				} else {
					mbs.param[0] = MBOX_LOAD_RISC_RAM;
					mbs.param[1] = la;
					mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
					mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
					mbs.param[4] = nw;
					mbs.param[6] = DMA_WD3(isp->isp_rquest_dma);
					mbs.param[7] = DMA_WD2(isp->isp_rquest_dma);
					mbs.param[8] = la >> 16;
					isp_prt(isp, ISP_LOGDEBUG1, "LOAD RISC RAM %u words at load address 0x%x\n", nw, la);
				}
				isp_mboxcmd(isp, &mbs);
				if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
					isp_prt(isp, ISP_LOGERR, "F/W Risc Ram Load Failed");
					ISP_RESET0(isp);
					return;
				}
				la += nw;
			}

			if (!IS_2322(isp)) {
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
		} ucd;
		ucd.cp = isp->isp_mdvec->dv_ispfw;
		isp->isp_mbxworkp = &ucd.np[1];
		isp->isp_mbxwrk0 = ucd.np[3] - 1;
		isp->isp_mbxwrk1 = code_org + 1;
		MBSINIT(&mbs, MBOX_WRITE_RAM_WORD, MBLOGNONE, 0);
		mbs.param[1] = code_org;
		mbs.param[2] = ucd.np[0];
		isp_prt(isp, ISP_LOGDEBUG1, "WRITE RAM %u words at load address 0x%x", ucd.np[3], code_org);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGERR, "F/W download failed at word %d", isp->isp_mbxwrk1 - code_org);
			ISP_RESET0(isp);
			return;
		}
	} else {
		isp->isp_loaded_fw = 0;
		isp_prt(isp, ISP_LOGDEBUG2, "skipping f/w download");
	}

	/*
	 * If we loaded firmware, verify its checksum
	 */
	if (isp->isp_loaded_fw) {
		MBSINIT(&mbs, MBOX_VERIFY_CHECKSUM, MBLOGNONE, 0);
		mbs.param[0] = MBOX_VERIFY_CHECKSUM;
		if (IS_24XX(isp)) {
			mbs.param[1] = code_org >> 16;
			mbs.param[2] = code_org;
		} else {
			mbs.param[1] = code_org;
		}
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGERR, dcrc);
			ISP_RESET0(isp);
			return;
		}
	}

	/*
	 * Now start it rolling.
	 *
	 * If we didn't actually download f/w,
	 * we still need to (re)start it.
	 */


	MBSINIT(&mbs, MBOX_EXEC_FIRMWARE, MBLOGALL, 5000000);
	if (IS_24XX(isp)) {
		mbs.param[1] = code_org >> 16;
		mbs.param[2] = code_org;
		if (isp->isp_loaded_fw) {
			mbs.param[3] = 0;
		} else {
			mbs.param[3] = 1;
		}
		if (IS_25XX(isp)) {
			mbs.ibits |= 0x10;
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
	isp_mboxcmd(isp, &mbs);
	if (IS_2322(isp) || IS_24XX(isp)) {
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			ISP_RESET0(isp);
			return;
		}
	}

	/*
	 * Give it a chance to finish starting up.
	 * Give the 24XX more time.
	 */
	if (IS_24XX(isp)) {
		ISP_DELAY(500000);
		/*
		 * Check to see if the 24XX firmware really started.
		 */
		if (mbs.param[1] == 0xdead) {
			isp_prt(isp, ISP_LOGERR, "f/w didn't *really* start");
			ISP_RESET0(isp);
			return;
		}
	} else {
		ISP_DELAY(250000);
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
	}

	/*
	 * Ask the chip for the current firmware version.
	 * This should prove that the new firmware is working.
	 */
	MBSINIT(&mbs, MBOX_ABOUT_FIRMWARE, MBLOGALL, 0);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
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
			isp->isp_fwattr = ISP_FW_ATTR_SCCLUN;
#else
			isp->isp_fwattr = 0;
#endif
		} else {
			isp->isp_fwattr = mbs.param[6];
		}
		if (IS_24XX(isp)) {
			isp->isp_fwattr |= ((uint64_t) mbs.param[15]) << 16;
			if (isp->isp_fwattr & ISP2400_FW_ATTR_EXTNDED) {
				isp->isp_fwattr |=
				    (((uint64_t) mbs.param[16]) << 32) |
				    (((uint64_t) mbs.param[17]) << 48);
			}
		}
	} else if (IS_SCSI(isp)) {
#ifndef	ISP_TARGET_MODE
		isp->isp_fwattr = ISP_FW_ATTR_TMODE;
#else
		isp->isp_fwattr = 0;
#endif
	}

	isp_prt(isp, ISP_LOGCONFIG, "Board Type %s, Chip Revision 0x%x, %s F/W Revision %d.%d.%d",
	    btype, isp->isp_revision, dodnld? "loaded" : "resident", isp->isp_fwrev[0], isp->isp_fwrev[1], isp->isp_fwrev[2]);

	fwt = isp->isp_fwattr;
	if (IS_24XX(isp)) {
		buf = FCPARAM(isp, 0)->isp_scratch;
		ISP_SNPRINTF(buf, ISP_FC_SCRLEN, "Attributes:");
		if (fwt & ISP2400_FW_ATTR_CLASS2) {
			fwt ^=ISP2400_FW_ATTR_CLASS2;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s Class2", buf);
		}
		if (fwt & ISP2400_FW_ATTR_IP) {
			fwt ^=ISP2400_FW_ATTR_IP;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s IP", buf);
		}
		if (fwt & ISP2400_FW_ATTR_MULTIID) {
			fwt ^=ISP2400_FW_ATTR_MULTIID;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s MultiID", buf);
		}
		if (fwt & ISP2400_FW_ATTR_SB2) {
			fwt ^=ISP2400_FW_ATTR_SB2;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s SB2", buf);
		}
		if (fwt & ISP2400_FW_ATTR_T10CRC) {
			fwt ^=ISP2400_FW_ATTR_T10CRC;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s T10CRC", buf);
		}
		if (fwt & ISP2400_FW_ATTR_VI) {
			fwt ^=ISP2400_FW_ATTR_VI;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s VI", buf);
		}
		if (fwt & ISP2400_FW_ATTR_MQ) {
			fwt ^=ISP2400_FW_ATTR_MQ;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s MQ", buf);
		}
		if (fwt & ISP2400_FW_ATTR_MSIX) {
			fwt ^=ISP2400_FW_ATTR_MSIX;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s MSIX", buf);
		}
		if (fwt & ISP2400_FW_ATTR_FCOE) {
			fwt ^=ISP2400_FW_ATTR_FCOE;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s FCOE", buf);
		}
		if (fwt & ISP2400_FW_ATTR_VP0) {
			fwt ^= ISP2400_FW_ATTR_VP0;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s VP0_Decoupling", buf);
		}
		if (fwt & ISP2400_FW_ATTR_EXPFW) {
			fwt ^= ISP2400_FW_ATTR_EXPFW;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s (Experimental)", buf);
		}
		if (fwt & ISP2400_FW_ATTR_HOTFW) {
			fwt ^= ISP2400_FW_ATTR_HOTFW;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s HotFW", buf);
		}
		fwt &= ~ISP2400_FW_ATTR_EXTNDED;
		if (fwt & ISP2400_FW_ATTR_EXTVP) {
			fwt ^= ISP2400_FW_ATTR_EXTVP;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s ExtVP", buf);
		}
		if (fwt & ISP2400_FW_ATTR_VN2VN) {
			fwt ^= ISP2400_FW_ATTR_VN2VN;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s VN2VN", buf);
		}
		if (fwt & ISP2400_FW_ATTR_EXMOFF) {
			fwt ^= ISP2400_FW_ATTR_EXMOFF;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s EXMOFF", buf);
		}
		if (fwt & ISP2400_FW_ATTR_NPMOFF) {
			fwt ^= ISP2400_FW_ATTR_NPMOFF;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s NPMOFF", buf);
		}
		if (fwt & ISP2400_FW_ATTR_DIFCHOP) {
			fwt ^= ISP2400_FW_ATTR_DIFCHOP;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s DIFCHOP", buf);
		}
		if (fwt & ISP2400_FW_ATTR_SRIOV) {
			fwt ^= ISP2400_FW_ATTR_SRIOV;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s SRIOV", buf);
		}
		if (fwt & ISP2400_FW_ATTR_ASICTMP) {
			fwt ^= ISP2400_FW_ATTR_ASICTMP;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s ASICTMP", buf);
		}
		if (fwt & ISP2400_FW_ATTR_ATIOMQ) {
			fwt ^= ISP2400_FW_ATTR_ATIOMQ;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s ATIOMQ", buf);
		}
		if (fwt) {
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s (unknown 0x%08x%08x)", buf,
			    (uint32_t) (fwt >> 32), (uint32_t) fwt);
		}
		isp_prt(isp, ISP_LOGCONFIG, "%s", buf);
	} else if (IS_FC(isp)) {
		buf = FCPARAM(isp, 0)->isp_scratch;
		ISP_SNPRINTF(buf, ISP_FC_SCRLEN, "Attributes:");
		if (fwt & ISP_FW_ATTR_TMODE) {
			fwt ^=ISP_FW_ATTR_TMODE;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s TargetMode", buf);
		}
		if (fwt & ISP_FW_ATTR_SCCLUN) {
			fwt ^=ISP_FW_ATTR_SCCLUN;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s SCC-Lun", buf);
		}
		if (fwt & ISP_FW_ATTR_FABRIC) {
			fwt ^=ISP_FW_ATTR_FABRIC;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s Fabric", buf);
		}
		if (fwt & ISP_FW_ATTR_CLASS2) {
			fwt ^=ISP_FW_ATTR_CLASS2;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s Class2", buf);
		}
		if (fwt & ISP_FW_ATTR_FCTAPE) {
			fwt ^=ISP_FW_ATTR_FCTAPE;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s FC-Tape", buf);
		}
		if (fwt & ISP_FW_ATTR_IP) {
			fwt ^=ISP_FW_ATTR_IP;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s IP", buf);
		}
		if (fwt & ISP_FW_ATTR_VI) {
			fwt ^=ISP_FW_ATTR_VI;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s VI", buf);
		}
		if (fwt & ISP_FW_ATTR_VI_SOLARIS) {
			fwt ^=ISP_FW_ATTR_VI_SOLARIS;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s VI_SOLARIS", buf);
		}
		if (fwt & ISP_FW_ATTR_2KLOGINS) {
			fwt ^=ISP_FW_ATTR_2KLOGINS;
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s 2K-Login", buf);
		}
		if (fwt != 0) {
			ISP_SNPRINTF(buf, ISP_FC_SCRLEN - strlen(buf), "%s (unknown 0x%08x%08x)", buf,
			    (uint32_t) (fwt >> 32), (uint32_t) fwt);
		}
		isp_prt(isp, ISP_LOGCONFIG, "%s", buf);
	}

	if (IS_24XX(isp)) {
		MBSINIT(&mbs, MBOX_GET_RESOURCE_COUNT, MBLOGALL, 0);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			ISP_RESET0(isp);
			return;
		}
		if (isp->isp_maxcmds >= mbs.param[3]) {
			isp->isp_maxcmds = mbs.param[3];
		}
	} else {
		MBSINIT(&mbs, MBOX_GET_FIRMWARE_STATUS, MBLOGALL, 0);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			ISP_RESET0(isp);
			return;
		}
		if (isp->isp_maxcmds >= mbs.param[2]) {
			isp->isp_maxcmds = mbs.param[2];
		}
	}
	isp_prt(isp, ISP_LOGCONFIG, "%d max I/O command limit set", isp->isp_maxcmds);

	/*
	 * If we don't have Multi-ID f/w loaded, we need to restrict channels to one.
	 * Only make this check for non-SCSI cards (I'm not sure firmware attributes
	 * work for them).
	 */
	if (IS_FC(isp) && isp->isp_nchan > 1) {
		if (!ISP_CAP_MULTI_ID(isp)) {
			isp_prt(isp, ISP_LOGWARN, "non-MULTIID f/w loaded, only can enable 1 of %d channels", isp->isp_nchan);
			isp->isp_nchan = 1;
		}
	}
	for (i = 0; i < isp->isp_nchan; i++) {
		isp_fw_state(isp, i);
	}
	if (isp->isp_dead) {
		isp_shutdown(isp);
		ISP_DISABLE_INTS(isp);
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
		if (ISP_CAP_SCCFW(isp)) {
			isp->isp_maxluns = 0;	/* No limit -- 2/8 bytes */
		} else {
			isp->isp_maxluns = 16;
		}
	}

	/*
	 * We get some default values established. As a side
	 * effect, NVRAM is read here (unless overriden by
	 * a configuration flag).
	 */
	if (do_load_defaults) {
		if (IS_SCSI(isp)) {
			isp_setdfltsdparm(isp);
		} else {
			for (i = 0; i < isp->isp_nchan; i++) {
				isp_setdfltfcparm(isp, i);
			}
		}
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
		if (IS_24XX(isp)) {
			isp_fibre_init_2400(isp);
		} else {
			isp_fibre_init(isp);
		}
	} else {
		isp_scsi_init(isp);
	}
	GET_NANOTIME(&isp->isp_init_time);
}

static void
isp_scsi_init(ispsoftc_t *isp)
{
	sdparam *sdp_chan0, *sdp_chan1;
	mbreg_t mbs;

	isp->isp_state = ISP_INITSTATE;

	sdp_chan0 = SDPARAM(isp, 0);
	sdp_chan1 = sdp_chan0;
	if (IS_DUALBUS(isp)) {
		sdp_chan1 = SDPARAM(isp, 1);
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
	MBSINIT(&mbs, MBOX_SET_RETRY_COUNT, MBLOGALL, 0);
	mbs.param[1] = sdp_chan0->isp_retry_count;
	mbs.param[2] = sdp_chan0->isp_retry_delay;
	mbs.param[6] = sdp_chan1->isp_retry_count;
	mbs.param[7] = sdp_chan1->isp_retry_delay;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}

	/*
	 * Set ASYNC DATA SETUP time. This is very important.
	 */
	MBSINIT(&mbs, MBOX_SET_ASYNC_DATA_SETUP_TIME, MBLOGALL, 0);
	mbs.param[1] = sdp_chan0->isp_async_data_setup;
	mbs.param[2] = sdp_chan1->isp_async_data_setup;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}

	/*
	 * Set ACTIVE Negation State.
	 */
	MBSINIT(&mbs, MBOX_SET_ACT_NEG_STATE, MBLOGNONE, 0);
	mbs.param[1] =
	    (sdp_chan0->isp_req_ack_active_neg << 4) |
	    (sdp_chan0->isp_data_line_active_neg << 5);
	mbs.param[2] =
	    (sdp_chan1->isp_req_ack_active_neg << 4) |
	    (sdp_chan1->isp_data_line_active_neg << 5);
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
	MBSINIT(&mbs, MBOX_SET_TAG_AGE_LIMIT, MBLOGALL, 0);
	mbs.param[1] = sdp_chan0->isp_tag_aging;
	mbs.param[2] = sdp_chan1->isp_tag_aging;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_prt(isp, ISP_LOGERR, "failed to set tag age limit (%d,%d)",
		    sdp_chan0->isp_tag_aging, sdp_chan1->isp_tag_aging);
		return;
	}

	/*
	 * Set selection timeout.
	 */
	MBSINIT(&mbs, MBOX_SET_SELECT_TIMEOUT, MBLOGALL, 0);
	mbs.param[1] = sdp_chan0->isp_selection_timeout;
	mbs.param[2] = sdp_chan1->isp_selection_timeout;
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
		MBSINIT(&mbs, MBOX_INIT_RES_QUEUE_A64, MBLOGALL, 0);
		mbs.param[1] = RESULT_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_result_dma);
		mbs.param[3] = DMA_WD0(isp->isp_result_dma);
		mbs.param[4] = 0;
		mbs.param[6] = DMA_WD3(isp->isp_result_dma);
		mbs.param[7] = DMA_WD2(isp->isp_result_dma);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		isp->isp_residx = isp->isp_resodx = mbs.param[5];

		MBSINIT(&mbs, MBOX_INIT_REQ_QUEUE_A64, MBLOGALL, 0);
		mbs.param[1] = RQUEST_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
		mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
		mbs.param[5] = 0;
		mbs.param[6] = DMA_WD3(isp->isp_result_dma);
		mbs.param[7] = DMA_WD2(isp->isp_result_dma);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		isp->isp_reqidx = isp->isp_reqodx = mbs.param[4];
	} else {
		MBSINIT(&mbs, MBOX_INIT_RES_QUEUE, MBLOGALL, 0);
		mbs.param[1] = RESULT_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_result_dma);
		mbs.param[3] = DMA_WD0(isp->isp_result_dma);
		mbs.param[4] = 0;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		isp->isp_residx = isp->isp_resodx = mbs.param[5];

		MBSINIT(&mbs, MBOX_INIT_REQ_QUEUE, MBLOGALL, 0);
		mbs.param[1] = RQUEST_QUEUE_LEN(isp);
		mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
		mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
		mbs.param[5] = 0;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		isp->isp_reqidx = isp->isp_reqodx = mbs.param[4];
	}

	/*
	 * Turn on LVD transitions for ULTRA2 or better and other features
	 *
	 * Now that we have 32 bit handles, don't do any fast posting
	 * any more. For Ultra2/Ultra3 cards, we can turn on 32 bit RIO
	 * operation or use fast posting. To be conservative, we'll only
	 * do this for Ultra3 cards now because the other cards are so
	 * rare for this author to find and test with.
	 */

	MBSINIT(&mbs, MBOX_SET_FW_FEATURES, MBLOGALL, 0);
	if (IS_ULTRA2(isp))
		mbs.param[1] |= FW_FEATURE_LVD_NOTIFY;
#ifdef	ISP_NO_RIO
	if (IS_ULTRA3(isp))
		mbs.param[1] |= FW_FEATURE_FAST_POST;
#else
	if (IS_ULTRA3(isp))
		mbs.param[1] |= FW_FEATURE_RIO_32BIT;
#endif
	if (mbs.param[1] != 0) {
		uint16_t sfeat = mbs.param[1];
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGINFO,
			    "Enabled FW features (0x%x)", sfeat);
		}
	}

	isp->isp_state = ISP_RUNSTATE;
}

static void
isp_scsi_channel_init(ispsoftc_t *isp, int chan)
{
	sdparam *sdp;
	mbreg_t mbs;
	int tgt;

	sdp = SDPARAM(isp, chan);

	/*
	 * Set (possibly new) Initiator ID.
	 */
	MBSINIT(&mbs, MBOX_SET_INIT_SCSI_ID, MBLOGALL, 0);
	mbs.param[1] = (chan << 7) | sdp->isp_initiator_id;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}
	isp_prt(isp, ISP_LOGINFO, "Chan %d Initiator ID is %d",
	    chan, sdp->isp_initiator_id);


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
		MBSINIT(&mbs, MBOX_SET_TARGET_PARAMS, MBLOGNONE, 0);
		mbs.param[1] = (chan << 15) | (tgt << 8);
		mbs.param[2] = sdf;
		if ((sdf & DPARM_SYNC) == 0) {
			mbs.param[3] = 0;
		} else {
			mbs.param[3] =
			    (sdp->isp_devparam[tgt].goal_offset << 8) |
			    (sdp->isp_devparam[tgt].goal_period);
		}
		isp_prt(isp, ISP_LOGDEBUG0, "Initial Settings bus%d tgt%d flags 0x%x off 0x%x per 0x%x",
		    chan, tgt, mbs.param[2], mbs.param[3] >> 8, mbs.param[3] & 0xff);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			sdf = DPARM_SAFE_DFLT;
			MBSINIT(&mbs, MBOX_SET_TARGET_PARAMS, MBLOGALL, 0);
			mbs.param[1] = (tgt << 8) | (chan << 15);
			mbs.param[2] = sdf;
			mbs.param[3] = 0;
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
			MBSINIT(&mbs, MBOX_SET_DEV_QUEUE_PARAMS, MBLOGALL, 0);
			mbs.param[1] = (chan << 15) | (tgt << 8) | lun;
			mbs.param[2] = sdp->isp_max_queue_depth;
			mbs.param[3] = sdp->isp_devparam[tgt].exc_throttle;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				break;
			}
		}
	}
	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		if (sdp->isp_devparam[tgt].dev_refresh) {
			sdp->sendmarker = 1;
			sdp->update = 1;
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

	/*
	 * We only support one channel on non-24XX cards
	 */
	fcp = FCPARAM(isp, 0);
	if (fcp->role == ISP_ROLE_NONE)
		return;

	isp->isp_state = ISP_INITSTATE;
	ISP_MEMZERO(icbp, sizeof (*icbp));
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
	if (fcp->role & ISP_ROLE_TARGET) {
		icbp->icb_fwoptions |= ICBOPT_TGT_ENABLE;
	} else {
		icbp->icb_fwoptions &= ~ICBOPT_TGT_ENABLE;
	}

	if (fcp->role & ISP_ROLE_INITIATOR) {
		icbp->icb_fwoptions &= ~ICBOPT_INI_DISABLE;
	} else {
		icbp->icb_fwoptions |= ICBOPT_INI_DISABLE;
	}

	icbp->icb_maxfrmlen = DEFAULT_FRAMESIZE(isp);
	if (icbp->icb_maxfrmlen < ICB_MIN_FRMLEN || icbp->icb_maxfrmlen > ICB_MAX_FRMLEN) {
		isp_prt(isp, ISP_LOGERR, "bad frame length (%d) from NVRAM- using %d", DEFAULT_FRAMESIZE(isp), ICB_DFLT_FRMLEN);
		icbp->icb_maxfrmlen = ICB_DFLT_FRMLEN;
	}
	icbp->icb_maxalloc = fcp->isp_maxalloc;
	if (icbp->icb_maxalloc < 1) {
		isp_prt(isp, ISP_LOGERR, "bad maximum allocation (%d)- using 16", fcp->isp_maxalloc);
		icbp->icb_maxalloc = 16;
	}
	icbp->icb_execthrottle = DEFAULT_EXEC_THROTTLE(isp);
	if (icbp->icb_execthrottle < 1) {
		isp_prt(isp, ISP_LOGERR, "bad execution throttle of %d- using %d", DEFAULT_EXEC_THROTTLE(isp), ICB_DFLT_THROTTLE);
		icbp->icb_execthrottle = ICB_DFLT_THROTTLE;
	}
	icbp->icb_retry_delay = fcp->isp_retry_delay;
	icbp->icb_retry_count = fcp->isp_retry_count;
	icbp->icb_hardaddr = fcp->isp_loopid;
	ownloopid = (isp->isp_confopts & ISP_CFG_OWNLOOPID) != 0;
	if (icbp->icb_hardaddr >= LOCAL_LOOP_LIM) {
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
	if (IS_2100(isp)) {
		/*
		 * We can't have Fast Posting any more- we now
		 * have 32 bit handles.
		 */
		icbp->icb_fwoptions &= ~ICBOPT_FAST_POST;
	} else if (IS_2200(isp) || IS_23XX(isp)) {
		icbp->icb_fwoptions |= ICBOPT_EXTENDED;

		icbp->icb_xfwoptions = fcp->isp_xfwoptions;

		if (ISP_CAP_FCTAPE(isp)) {
			if (isp->isp_confopts & ISP_CFG_NOFCTAPE)
				icbp->icb_xfwoptions &= ~ICBXOPT_FCTAPE;

			if (isp->isp_confopts & ISP_CFG_FCTAPE)
				icbp->icb_xfwoptions |= ICBXOPT_FCTAPE;

			if (icbp->icb_xfwoptions & ICBXOPT_FCTAPE) {
				icbp->icb_fwoptions &= ~ICBOPT_FULL_LOGIN;	/* per documents */
				icbp->icb_xfwoptions |= ICBXOPT_FCTAPE_CCQ|ICBXOPT_FCTAPE_CONFIRM;
				FCPARAM(isp, 0)->fctape_enabled = 1;
			} else {
				FCPARAM(isp, 0)->fctape_enabled = 0;
			}
		} else {
			icbp->icb_xfwoptions &= ~ICBXOPT_FCTAPE;
			FCPARAM(isp, 0)->fctape_enabled = 0;
		}

		/*
		 * Prefer or force Point-To-Point instead Loop?
		 */
		switch (isp->isp_confopts & ISP_CFG_PORT_PREF) {
		case ISP_CFG_NPORT:
			icbp->icb_xfwoptions &= ~ICBXOPT_TOPO_MASK;
			icbp->icb_xfwoptions |= ICBXOPT_PTP_2_LOOP;
			break;
		case ISP_CFG_NPORT_ONLY:
			icbp->icb_xfwoptions &= ~ICBXOPT_TOPO_MASK;
			icbp->icb_xfwoptions |= ICBXOPT_PTP_ONLY;
			break;
		case ISP_CFG_LPORT_ONLY:
			icbp->icb_xfwoptions &= ~ICBXOPT_TOPO_MASK;
			icbp->icb_xfwoptions |= ICBXOPT_LOOP_ONLY;
			break;
		default:
			/*
			 * Let NVRAM settings define it if they are sane
			 */
			switch (icbp->icb_xfwoptions & ICBXOPT_TOPO_MASK) {
			case ICBXOPT_PTP_2_LOOP:
			case ICBXOPT_PTP_ONLY:
			case ICBXOPT_LOOP_ONLY:
			case ICBXOPT_LOOP_2_PTP:
				break;
			default:
				icbp->icb_xfwoptions &= ~ICBXOPT_TOPO_MASK;
				icbp->icb_xfwoptions |= ICBXOPT_LOOP_2_PTP;
			}
			break;
		}
		if (IS_2200(isp)) {
			/*
			 * We can't have Fast Posting any more- we now
			 * have 32 bit handles.
			 *
			 * RIO seemed to have to much breakage.
			 *
			 * Just opt for safety.
			 */
			icbp->icb_xfwoptions &= ~ICBXOPT_RIO_16BIT;
			icbp->icb_fwoptions &= ~ICBOPT_FAST_POST;
		} else {
			/*
			 * QLogic recommends that FAST Posting be turned
			 * off for 23XX cards and instead allow the HBA
			 * to write response queue entries and interrupt
			 * after a delay (ZIO).
			 */
			icbp->icb_fwoptions &= ~ICBOPT_FAST_POST;
			if ((fcp->isp_xfwoptions & ICBXOPT_TIMER_MASK) == ICBXOPT_ZIO) {
				icbp->icb_xfwoptions |= ICBXOPT_ZIO;
				icbp->icb_idelaytimer = 10;
			}
			icbp->icb_zfwoptions = fcp->isp_zfwoptions;
			if (isp->isp_confopts & ISP_CFG_ONEGB) {
				icbp->icb_zfwoptions &= ~ICBZOPT_RATE_MASK;
				icbp->icb_zfwoptions |= ICBZOPT_RATE_ONEGB;
			} else if (isp->isp_confopts & ISP_CFG_TWOGB) {
				icbp->icb_zfwoptions &= ~ICBZOPT_RATE_MASK;
				icbp->icb_zfwoptions |= ICBZOPT_RATE_TWOGB;
			} else {
				switch (icbp->icb_zfwoptions & ICBZOPT_RATE_MASK) {
				case ICBZOPT_RATE_ONEGB:
				case ICBZOPT_RATE_TWOGB:
				case ICBZOPT_RATE_AUTO:
					break;
				default:
					icbp->icb_zfwoptions &= ~ICBZOPT_RATE_MASK;
					icbp->icb_zfwoptions |= ICBZOPT_RATE_AUTO;
					break;
				}
			}
		}
	}


	/*
	 * For 22XX > 2.1.26 && 23XX, set some options.
	 */
	if (ISP_FW_NEWER_THAN(isp, 2, 26, 0)) {
		MBSINIT(&mbs, MBOX_SET_FIRMWARE_OPTIONS, MBLOGALL, 0);
		mbs.param[1] = IFCOPT1_DISF7SWTCH|IFCOPT1_LIPASYNC|IFCOPT1_LIPF8;
		mbs.param[2] = 0;
		mbs.param[3] = 0;
		if (ISP_FW_NEWER_THAN(isp, 3, 16, 0)) {
			mbs.param[1] |= IFCOPT1_EQFQASYNC|IFCOPT1_CTIO_RETRY;
			if (fcp->role & ISP_ROLE_TARGET) {
				if (ISP_FW_NEWER_THAN(isp, 3, 25, 0)) {
					mbs.param[1] |= IFCOPT1_ENAPURE;
				}
				mbs.param[3] = IFCOPT3_NOPRLI;
			}
		}
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
	}
	icbp->icb_logintime = ICB_LOGIN_TOV;

#ifdef	ISP_TARGET_MODE
	if (ISP_FW_NEWER_THAN(isp, 3, 25, 0) && (icbp->icb_fwoptions & ICBOPT_TGT_ENABLE)) {
		icbp->icb_lunenables = 0xffff;
		icbp->icb_ccnt = DFLT_CMND_CNT;
		icbp->icb_icnt = DFLT_INOT_CNT;
		icbp->icb_lunetimeout = ICB_LUN_ENABLE_TOV;
	}
#endif
	if (fcp->isp_wwnn && fcp->isp_wwpn) {
		icbp->icb_fwoptions |= ICBOPT_BOTH_WWNS;
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_nodename, fcp->isp_wwnn);
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_portname, fcp->isp_wwpn);
		isp_prt(isp, ISP_LOGDEBUG1,
		    "Setting ICB Node 0x%08x%08x Port 0x%08x%08x",
		    ((uint32_t) (fcp->isp_wwnn >> 32)),
		    ((uint32_t) (fcp->isp_wwnn)),
		    ((uint32_t) (fcp->isp_wwpn >> 32)),
		    ((uint32_t) (fcp->isp_wwpn)));
	} else if (fcp->isp_wwpn) {
		icbp->icb_fwoptions &= ~ICBOPT_BOTH_WWNS;
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_portname, fcp->isp_wwpn);
		isp_prt(isp, ISP_LOGDEBUG1,
		    "Setting ICB Port 0x%08x%08x",
		    ((uint32_t) (fcp->isp_wwpn >> 32)),
		    ((uint32_t) (fcp->isp_wwpn)));
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

	if (FC_SCRATCH_ACQUIRE(isp, 0)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return;
	}
	isp_prt(isp, ISP_LOGDEBUG0, "isp_fibre_init: fwopt 0x%x xfwopt 0x%x zfwopt 0x%x",
	    icbp->icb_fwoptions, icbp->icb_xfwoptions, icbp->icb_zfwoptions);

	isp_put_icb(isp, icbp, (isp_icb_t *)fcp->isp_scratch);

	/*
	 * Init the firmware
	 */
	MBSINIT(&mbs, MBOX_INIT_FIRMWARE, MBLOGALL, 30000000);
	mbs.param[1] = 0;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	mbs.logval = MBLOGALL;
	isp_prt(isp, ISP_LOGDEBUG0, "INIT F/W from %p (%08x%08x)",
	    fcp->isp_scratch, (uint32_t) ((uint64_t)fcp->isp_scdma >> 32),
	    (uint32_t) fcp->isp_scdma);
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, sizeof (*icbp), 0);
	isp_mboxcmd(isp, &mbs);
	FC_SCRATCH_RELEASE(isp, 0);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		isp_print_bytes(isp, "isp_fibre_init", sizeof (*icbp), icbp);
		return;
	}
	isp->isp_reqidx = 0;
	isp->isp_reqodx = 0;
	isp->isp_residx = 0;
	isp->isp_resodx = 0;

	/*
	 * Whatever happens, we're now committed to being here.
	 */
	isp->isp_state = ISP_RUNSTATE;
}

static void
isp_fibre_init_2400(ispsoftc_t *isp)
{
	fcparam *fcp;
	isp_icb_2400_t local, *icbp = &local;
	mbreg_t mbs;
	int chan;
	int ownloopid = 0;

	/*
	 * Check to see whether all channels have *some* kind of role
	 */
	for (chan = 0; chan < isp->isp_nchan; chan++) {
		fcp = FCPARAM(isp, chan);
		if (fcp->role != ISP_ROLE_NONE) {
			break;
		}
	}
	if (chan == isp->isp_nchan) {
		isp_prt(isp, ISP_LOG_WARN1, "all %d channels with role 'none'", chan);
		return;
	}

	isp->isp_state = ISP_INITSTATE;

	/*
	 * Start with channel 0.
	 */
	fcp = FCPARAM(isp, 0);

	/*
	 * Turn on LIP F8 async event (1)
	 */
	MBSINIT(&mbs, MBOX_SET_FIRMWARE_OPTIONS, MBLOGALL, 0);
	mbs.param[1] = 1;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}

	ISP_MEMZERO(icbp, sizeof (*icbp));
	icbp->icb_fwoptions1 = fcp->isp_fwoptions;
	if (fcp->role & ISP_ROLE_TARGET) {
		icbp->icb_fwoptions1 |= ICB2400_OPT1_TGT_ENABLE;
	} else {
		icbp->icb_fwoptions1 &= ~ICB2400_OPT1_TGT_ENABLE;
	}

	if (fcp->role & ISP_ROLE_INITIATOR) {
		icbp->icb_fwoptions1 &= ~ICB2400_OPT1_INI_DISABLE;
	} else {
		icbp->icb_fwoptions1 |= ICB2400_OPT1_INI_DISABLE;
	}

	icbp->icb_version = ICB_VERSION1;
	icbp->icb_maxfrmlen = DEFAULT_FRAMESIZE(isp);
	if (icbp->icb_maxfrmlen < ICB_MIN_FRMLEN || icbp->icb_maxfrmlen > ICB_MAX_FRMLEN) {
		isp_prt(isp, ISP_LOGERR, "bad frame length (%d) from NVRAM- using %d", DEFAULT_FRAMESIZE(isp), ICB_DFLT_FRMLEN);
		icbp->icb_maxfrmlen = ICB_DFLT_FRMLEN;
	}

	icbp->icb_execthrottle = DEFAULT_EXEC_THROTTLE(isp);
	if (icbp->icb_execthrottle < 1) {
		isp_prt(isp, ISP_LOGERR, "bad execution throttle of %d- using %d", DEFAULT_EXEC_THROTTLE(isp), ICB_DFLT_THROTTLE);
		icbp->icb_execthrottle = ICB_DFLT_THROTTLE;
	}

	/*
	 * Set target exchange count. Take half if we are supporting both roles.
	 */
	if (icbp->icb_fwoptions1 & ICB2400_OPT1_TGT_ENABLE) {
		icbp->icb_xchgcnt = isp->isp_maxcmds;
		if ((icbp->icb_fwoptions1 & ICB2400_OPT1_INI_DISABLE) == 0)
			icbp->icb_xchgcnt >>= 1;
	}


	ownloopid = (isp->isp_confopts & ISP_CFG_OWNLOOPID) != 0;
	icbp->icb_hardaddr = fcp->isp_loopid;
	if (icbp->icb_hardaddr >= LOCAL_LOOP_LIM) {
		icbp->icb_hardaddr = 0;
		ownloopid = 0;
	}

	if (ownloopid)
		icbp->icb_fwoptions1 |= ICB2400_OPT1_HARD_ADDRESS;

	icbp->icb_fwoptions2 = fcp->isp_xfwoptions;
	if (isp->isp_confopts & ISP_CFG_NOFCTAPE) {
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_FCTAPE;
	}
	if (isp->isp_confopts & ISP_CFG_FCTAPE) {
		icbp->icb_fwoptions2 |= ICB2400_OPT2_FCTAPE;
	}

	for (chan = 0; chan < isp->isp_nchan; chan++) {
		if (icbp->icb_fwoptions2 & ICB2400_OPT2_FCTAPE)
			FCPARAM(isp, chan)->fctape_enabled = 1;
		else
			FCPARAM(isp, chan)->fctape_enabled = 0;
	}

	switch (isp->isp_confopts & ISP_CFG_PORT_PREF) {
	case ISP_CFG_NPORT_ONLY:
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TOPO_MASK;
		icbp->icb_fwoptions2 |= ICB2400_OPT2_PTP_ONLY;
		break;
	case ISP_CFG_LPORT_ONLY:
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TOPO_MASK;
		icbp->icb_fwoptions2 |= ICB2400_OPT2_LOOP_ONLY;
		break;
	default:
		/* ISP_CFG_PTP_2_LOOP not available in 24XX/25XX */
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
		isp_prt(isp, ISP_LOGWARN, "bad value %x in fwopt2 timer field", icbp->icb_fwoptions2 & ICB2400_OPT2_TIMER_MASK);
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TIMER_MASK;
		break;
	}

	icbp->icb_fwoptions3 = fcp->isp_zfwoptions;
	if ((icbp->icb_fwoptions3 & ICB2400_OPT3_RSPSZ_MASK) == 0) {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RSPSZ_24;
	}
	icbp->icb_fwoptions3 &= ~ICB2400_OPT3_RATE_AUTO;
	if (isp->isp_confopts & ISP_CFG_ONEGB) {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_ONEGB;
	} else if (isp->isp_confopts & ISP_CFG_TWOGB) {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_TWOGB;
	} else if (isp->isp_confopts & ISP_CFG_FOURGB) {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_FOURGB;
	} else if (IS_25XX(isp) && (isp->isp_confopts & ISP_CFG_EIGHTGB)) {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_EIGHTGB;
	} else {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_AUTO;
	}
	if (ownloopid == 0) {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_SOFTID;
	}
	icbp->icb_logintime = ICB_LOGIN_TOV;

	if (fcp->isp_wwnn && fcp->isp_wwpn) {
		icbp->icb_fwoptions1 |= ICB2400_OPT1_BOTH_WWNS;
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_portname, fcp->isp_wwpn);
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_nodename, fcp->isp_wwnn);
		isp_prt(isp, ISP_LOGDEBUG1, "Setting ICB Node 0x%08x%08x Port 0x%08x%08x", ((uint32_t) (fcp->isp_wwnn >> 32)), ((uint32_t) (fcp->isp_wwnn)),
		    ((uint32_t) (fcp->isp_wwpn >> 32)), ((uint32_t) (fcp->isp_wwpn)));
	} else if (fcp->isp_wwpn) {
		icbp->icb_fwoptions1 &= ~ICB2400_OPT1_BOTH_WWNS;
		MAKE_NODE_NAME_FROM_WWN(icbp->icb_portname, fcp->isp_wwpn);
		isp_prt(isp, ISP_LOGDEBUG1, "Setting ICB Node to be same as Port 0x%08x%08x", ((uint32_t) (fcp->isp_wwpn >> 32)), ((uint32_t) (fcp->isp_wwpn)));
	} else {
		isp_prt(isp, ISP_LOGERR, "No valid WWNs to use");
		return;
	}
	icbp->icb_retry_count = fcp->isp_retry_count;

	icbp->icb_rqstqlen = RQUEST_QUEUE_LEN(isp);
	if (icbp->icb_rqstqlen < 8) {
		isp_prt(isp, ISP_LOGERR, "bad request queue length %d", icbp->icb_rqstqlen);
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
	/* unconditionally set up the ATIO queue if we support target mode */
	icbp->icb_atioqlen = RESULT_QUEUE_LEN(isp);
	if (icbp->icb_atioqlen < 8) {
		isp_prt(isp, ISP_LOGERR, "bad ATIO queue length %d", icbp->icb_atioqlen);
		return;
	}
	icbp->icb_atioqaddr[RQRSP_ADDR0015] = DMA_WD0(isp->isp_atioq_dma);
	icbp->icb_atioqaddr[RQRSP_ADDR1631] = DMA_WD1(isp->isp_atioq_dma);
	icbp->icb_atioqaddr[RQRSP_ADDR3247] = DMA_WD2(isp->isp_atioq_dma);
	icbp->icb_atioqaddr[RQRSP_ADDR4863] = DMA_WD3(isp->isp_atioq_dma);
	isp_prt(isp, ISP_LOGDEBUG0, "isp_fibre_init_2400: atioq %04x%04x%04x%04x", DMA_WD3(isp->isp_atioq_dma), DMA_WD2(isp->isp_atioq_dma),
	    DMA_WD1(isp->isp_atioq_dma), DMA_WD0(isp->isp_atioq_dma));
#endif

	isp_prt(isp, ISP_LOGDEBUG0, "isp_fibre_init_2400: fwopt1 0x%x fwopt2 0x%x fwopt3 0x%x", icbp->icb_fwoptions1, icbp->icb_fwoptions2, icbp->icb_fwoptions3);

	isp_prt(isp, ISP_LOGDEBUG0, "isp_fibre_init_2400: rqst %04x%04x%04x%04x rsp %04x%04x%04x%04x", DMA_WD3(isp->isp_rquest_dma), DMA_WD2(isp->isp_rquest_dma),
	    DMA_WD1(isp->isp_rquest_dma), DMA_WD0(isp->isp_rquest_dma), DMA_WD3(isp->isp_result_dma), DMA_WD2(isp->isp_result_dma),
	    DMA_WD1(isp->isp_result_dma), DMA_WD0(isp->isp_result_dma));

	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "isp_fibre_init_2400", sizeof (*icbp), icbp);
	}

	if (FC_SCRATCH_ACQUIRE(isp, 0)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return;
	}
	ISP_MEMZERO(fcp->isp_scratch, ISP_FC_SCRLEN);
	isp_put_icb_2400(isp, icbp, fcp->isp_scratch);

	/*
	 * Now fill in information about any additional channels
	 */
	if (isp->isp_nchan > 1) {
		isp_icb_2400_vpinfo_t vpinfo, *vdst;
		vp_port_info_t pi, *pdst;
		size_t amt = 0;
		uint8_t *off;

		vpinfo.vp_global_options = 0;
		if (isp->isp_fwattr & ISP2400_FW_ATTR_VP0) {
			vpinfo.vp_global_options |= ICB2400_VPGOPT_VP0_DECOUPLE;
			vpinfo.vp_count = isp->isp_nchan;
			chan = 0;
		} else {
			vpinfo.vp_count = isp->isp_nchan - 1;
			chan = 1;
		}
		off = fcp->isp_scratch;
		off += ICB2400_VPINFO_OFF;
		vdst = (isp_icb_2400_vpinfo_t *) off;
		isp_put_icb_2400_vpinfo(isp, &vpinfo, vdst);
		amt = ICB2400_VPINFO_OFF + sizeof (isp_icb_2400_vpinfo_t);
		for (; chan < isp->isp_nchan; chan++) {
			fcparam *fcp2;

			ISP_MEMZERO(&pi, sizeof (pi));
			fcp2 = FCPARAM(isp, chan);
			if (fcp2->role != ISP_ROLE_NONE) {
				pi.vp_port_options = ICB2400_VPOPT_ENABLED;
				if (fcp2->role & ISP_ROLE_INITIATOR)
					pi.vp_port_options |= ICB2400_VPOPT_INI_ENABLE;
				if ((fcp2->role & ISP_ROLE_TARGET) == 0)
					pi.vp_port_options |= ICB2400_VPOPT_TGT_DISABLE;
			}
			MAKE_NODE_NAME_FROM_WWN(pi.vp_port_portname, fcp2->isp_wwpn);
			MAKE_NODE_NAME_FROM_WWN(pi.vp_port_nodename, fcp2->isp_wwnn);
			off = fcp->isp_scratch;
			if (isp->isp_fwattr & ISP2400_FW_ATTR_VP0)
				off += ICB2400_VPINFO_PORT_OFF(chan);
			else
				off += ICB2400_VPINFO_PORT_OFF(chan - 1);
			pdst = (vp_port_info_t *) off;
			isp_put_vp_port_info(isp, &pi, pdst);
			amt += ICB2400_VPOPT_WRITE_SIZE;
		}
		if (isp->isp_dblev & ISP_LOGDEBUG1) {
			isp_print_bytes(isp, "isp_fibre_init_2400",
			    amt - ICB2400_VPINFO_OFF,
			    (char *)fcp->isp_scratch + ICB2400_VPINFO_OFF);
		}
	}

	/*
	 * Init the firmware
	 */
	MBSINIT(&mbs, 0, MBLOGALL, 30000000);
	if (isp->isp_nchan > 1) {
		mbs.param[0] = MBOX_INIT_FIRMWARE_MULTI_ID;
	} else {
		mbs.param[0] = MBOX_INIT_FIRMWARE;
	}
	mbs.param[1] = 0;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	isp_prt(isp, ISP_LOGDEBUG0, "INIT F/W from %04x%04x%04x%04x", DMA_WD3(fcp->isp_scdma), DMA_WD2(fcp->isp_scdma), DMA_WD1(fcp->isp_scdma), DMA_WD0(fcp->isp_scdma));
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, sizeof (*icbp), 0);
	isp_mboxcmd(isp, &mbs);
	FC_SCRATCH_RELEASE(isp, 0);

	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}
	isp->isp_reqidx = 0;
	isp->isp_reqodx = 0;
	isp->isp_residx = 0;
	isp->isp_resodx = 0;
	isp->isp_atioodx = 0;

	/*
	 * Whatever happens, we're now committed to being here.
	 */
	isp->isp_state = ISP_RUNSTATE;
}

static void
isp_mark_portdb(ispsoftc_t *isp, int chan, int disposition)
{
	fcparam *fcp = FCPARAM(isp, chan);
	fcportdb_t *lp;
	int i;

	if (chan < 0 || chan >= isp->isp_nchan) {
		isp_prt(isp, ISP_LOGWARN, "isp_mark_portdb: bad channel %d", chan);
		return;
	}
	for (i = 0; i < MAX_FC_TARG; i++) {
		lp = &fcp->portdb[i];
		switch (lp->state) {
		case FC_PORTDB_STATE_PROBATIONAL:
		case FC_PORTDB_STATE_DEAD:
		case FC_PORTDB_STATE_CHANGED:
		case FC_PORTDB_STATE_PENDING_VALID:
		case FC_PORTDB_STATE_VALID:
			if (disposition > 0)
				lp->state = FC_PORTDB_STATE_PROBATIONAL;
			else {
				lp->state = FC_PORTDB_STATE_NIL;
				isp_async(isp, ISPASYNC_DEV_GONE, chan, lp);
			}
			break;
		case FC_PORTDB_STATE_ZOMBIE:
			break;
		case FC_PORTDB_STATE_NIL:
		case FC_PORTDB_STATE_NEW:
		default:
			ISP_MEMZERO(lp, sizeof(*lp));
			lp->state = FC_PORTDB_STATE_NIL;
			break;
		}
	}
}

/*
 * Perform an IOCB PLOGI or LOGO via EXECUTE IOCB A64 for 24XX cards
 * or via FABRIC LOGIN/FABRIC LOGOUT for other cards.
 */
static int
isp_plogx(ispsoftc_t *isp, int chan, uint16_t handle, uint32_t portid, int flags, int gs)
{
	mbreg_t mbs;
	uint8_t q[QENTRY_LEN];
	isp_plogx_t *plp;
	fcparam *fcp;
	uint8_t *scp;
	uint32_t sst, parm1;
	int rval, lev;
	const char *msg;
	char buf[64];

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

	ISP_MEMZERO(q, QENTRY_LEN);
	plp = (isp_plogx_t *) q;
	plp->plogx_header.rqs_entry_count = 1;
	plp->plogx_header.rqs_entry_type = RQSTYPE_LOGIN;
	plp->plogx_handle = 0xffffffff;
	plp->plogx_nphdl = handle;
	plp->plogx_vphdl = chan;
	plp->plogx_portlo = portid;
	plp->plogx_rspsz_porthi = (portid >> 16) & 0xff;
	plp->plogx_flags = flags;

	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "IOCB LOGX", QENTRY_LEN, plp);
	}

	if (gs == 0) {
		if (FC_SCRATCH_ACQUIRE(isp, chan)) {
			isp_prt(isp, ISP_LOGERR, sacq);
			return (-1);
		}
	}
	fcp = FCPARAM(isp, chan);
	scp = fcp->isp_scratch;
	isp_put_plogx(isp, plp, (isp_plogx_t *) scp);

	MBSINIT(&mbs, MBOX_EXEC_COMMAND_IOCB_A64, MBLOGALL, 500000);
	mbs.param[1] = QENTRY_LEN;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, QENTRY_LEN, chan);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		rval = mbs.param[0];
		goto out;
	}
	MEMORYBARRIER(isp, SYNC_SFORCPU, QENTRY_LEN, QENTRY_LEN, chan);
	scp += QENTRY_LEN;
	isp_get_plogx(isp, (isp_plogx_t *) scp, plp);
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "IOCB LOGX response", QENTRY_LEN, plp);
	}

	if (plp->plogx_status == PLOGX_STATUS_OK) {
		rval = 0;
		goto out;
	} else if (plp->plogx_status != PLOGX_STATUS_IOCBERR) {
		isp_prt(isp, ISP_LOGWARN,
		    "status 0x%x on port login IOCB channel %d",
		    plp->plogx_status, chan);
		rval = -1;
		goto out;
	}

	sst = plp->plogx_ioparm[0].lo16 | (plp->plogx_ioparm[0].hi16 << 16);
	parm1 = plp->plogx_ioparm[1].lo16 | (plp->plogx_ioparm[1].hi16 << 16);

	rval = -1;
	lev = ISP_LOGERR;
	msg = NULL;

	switch (sst) {
	case PLOGX_IOCBERR_NOLINK:
		msg = "no link";
		break;
	case PLOGX_IOCBERR_NOIOCB:
		msg = "no IOCB buffer";
		break;
	case PLOGX_IOCBERR_NOXGHG:
		msg = "no Exchange Control Block";
		break;
	case PLOGX_IOCBERR_FAILED:
		ISP_SNPRINTF(buf, sizeof (buf), "reason 0x%x (last LOGIN state 0x%x)", parm1 & 0xff, (parm1 >> 8) & 0xff);
		msg = buf;
		break;
	case PLOGX_IOCBERR_NOFABRIC:
		msg = "no fabric";
		break;
	case PLOGX_IOCBERR_NOTREADY:
		msg = "firmware not ready";
		break;
	case PLOGX_IOCBERR_NOLOGIN:
		ISP_SNPRINTF(buf, sizeof (buf), "not logged in (last state 0x%x)", parm1);
		msg = buf;
		rval = MBOX_NOT_LOGGED_IN;
		break;
	case PLOGX_IOCBERR_REJECT:
		ISP_SNPRINTF(buf, sizeof (buf), "LS_RJT = 0x%x", parm1);
		msg = buf;
		break;
	case PLOGX_IOCBERR_NOPCB:
		msg = "no PCB allocated";
		break;
	case PLOGX_IOCBERR_EINVAL:
		ISP_SNPRINTF(buf, sizeof (buf), "invalid parameter at offset 0x%x", parm1);
		msg = buf;
		break;
	case PLOGX_IOCBERR_PORTUSED:
		lev = ISP_LOG_SANCFG|ISP_LOG_WARN1;
		ISP_SNPRINTF(buf, sizeof (buf), "already logged in with N-Port handle 0x%x", parm1);
		msg = buf;
		rval = MBOX_PORT_ID_USED | (parm1 << 16);
		break;
	case PLOGX_IOCBERR_HNDLUSED:
		lev = ISP_LOG_SANCFG|ISP_LOG_WARN1;
		ISP_SNPRINTF(buf, sizeof (buf), "handle already used for PortID 0x%06x", parm1);
		msg = buf;
		rval = MBOX_LOOP_ID_USED;
		break;
	case PLOGX_IOCBERR_NOHANDLE:
		msg = "no handle allocated";
		break;
	case PLOGX_IOCBERR_NOFLOGI:
		msg = "no FLOGI_ACC";
		break;
	default:
		ISP_SNPRINTF(buf, sizeof (buf), "status %x from %x", plp->plogx_status, flags);
		msg = buf;
		break;
	}
	if (msg) {
		isp_prt(isp, ISP_LOGERR, "Chan %d PLOGX PortID 0x%06x to N-Port handle 0x%x: %s", chan, portid, handle, msg);
	}
out:
	if (gs == 0) {
		FC_SCRATCH_RELEASE(isp, chan);
	}
	return (rval);
}

static int
isp_port_login(ispsoftc_t *isp, uint16_t handle, uint32_t portid)
{
	mbreg_t mbs;

	MBSINIT(&mbs, MBOX_FABRIC_LOGIN, MBLOGNONE, 500000);
	if (ISP_CAP_2KLOGIN(isp)) {
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
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOG_WARN1, "isp_port_login: portid 0x%06x already logged in as %u", portid, mbs.param[1]);
		return (MBOX_PORT_ID_USED | (mbs.param[1] << 16));

	case MBOX_LOOP_ID_USED:
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOG_WARN1, "isp_port_login: handle 0x%x in use for port id 0x%02xXXXX", handle, mbs.param[1] & 0xff);
		return (MBOX_LOOP_ID_USED);

	case MBOX_COMMAND_COMPLETE:
		return (0);

	case MBOX_COMMAND_ERROR:
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOG_WARN1, "isp_port_login: error 0x%x in PLOGI to port 0x%06x", mbs.param[1], portid);
		return (MBOX_COMMAND_ERROR);

	case MBOX_ALL_IDS_USED:
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOG_WARN1, "isp_port_login: all IDs used for fabric login");
		return (MBOX_ALL_IDS_USED);

	default:
		isp_prt(isp, ISP_LOG_SANCFG, "isp_port_login: error 0x%x on port login of 0x%06x@0x%0x", mbs.param[0], portid, handle);
		return (mbs.param[0]);
	}
}

/*
 * Pre-24XX fabric port logout
 *
 * Note that portid is not used
 */
static int
isp_port_logout(ispsoftc_t *isp, uint16_t handle, uint32_t portid)
{
	mbreg_t mbs;

	MBSINIT(&mbs, MBOX_FABRIC_LOGOUT, MBLOGNONE, 500000);
	if (ISP_CAP_2KLOGIN(isp)) {
		mbs.param[1] = handle;
		mbs.ibits = (1 << 10);
	} else {
		mbs.param[1] = handle << 8;
	}
	isp_mboxcmd(isp, &mbs);
	return (mbs.param[0] == MBOX_COMMAND_COMPLETE? 0 : mbs.param[0]);
}

static int
isp_getpdb(ispsoftc_t *isp, int chan, uint16_t id, isp_pdb_t *pdb, int dolock)
{
	fcparam *fcp = FCPARAM(isp, chan);
	mbreg_t mbs;
	union {
		isp_pdb_21xx_t fred;
		isp_pdb_24xx_t bill;
	} un;

	MBSINIT(&mbs, MBOX_GET_PORT_DB, MBLOGALL & ~MBOX_COMMAND_PARAM_ERROR, 250000);
	if (IS_24XX(isp)) {
		mbs.ibits = (1 << 9)|(1 << 10);
		mbs.param[1] = id;
		mbs.param[9] = chan;
	} else if (ISP_CAP_2KLOGIN(isp)) {
		mbs.param[1] = id;
	} else {
		mbs.param[1] = id << 8;
	}
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	if (dolock) {
		if (FC_SCRATCH_ACQUIRE(isp, chan)) {
			isp_prt(isp, ISP_LOGERR, sacq);
			return (-1);
		}
	}
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, sizeof (un), chan);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		if (dolock) {
			FC_SCRATCH_RELEASE(isp, chan);
		}
		return (mbs.param[0]);
	}
	if (IS_24XX(isp)) {
		isp_get_pdb_24xx(isp, fcp->isp_scratch, &un.bill);
		pdb->handle = un.bill.pdb_handle;
		pdb->prli_word3 = un.bill.pdb_prli_svc3;
		pdb->portid = BITS2WORD_24XX(un.bill.pdb_portid_bits);
		ISP_MEMCPY(pdb->portname, un.bill.pdb_portname, 8);
		ISP_MEMCPY(pdb->nodename, un.bill.pdb_nodename, 8);
		isp_prt(isp, ISP_LOG_SANCFG, "Chan %d handle 0x%x Port 0x%06x flags 0x%x curstate %x", chan, id, pdb->portid, un.bill.pdb_flags, un.bill.pdb_curstate);
		if (un.bill.pdb_curstate < PDB2400_STATE_PLOGI_DONE || un.bill.pdb_curstate > PDB2400_STATE_LOGGED_IN) {
			mbs.param[0] = MBOX_NOT_LOGGED_IN;
			if (dolock) {
				FC_SCRATCH_RELEASE(isp, chan);
			}
			return (mbs.param[0]);
		}
	} else {
		isp_get_pdb_21xx(isp, fcp->isp_scratch, &un.fred);
		pdb->handle = un.fred.pdb_loopid;
		pdb->prli_word3 = un.fred.pdb_prli_svc3;
		pdb->portid = BITS2WORD(un.fred.pdb_portid_bits);
		ISP_MEMCPY(pdb->portname, un.fred.pdb_portname, 8);
		ISP_MEMCPY(pdb->nodename, un.fred.pdb_nodename, 8);
	}
	if (dolock) {
		FC_SCRATCH_RELEASE(isp, chan);
	}
	return (0);
}

static void
isp_dump_chip_portdb(ispsoftc_t *isp, int chan, int dolock)
{
	isp_pdb_t pdb;
	int lim, loopid;

	isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGINFO, "Chan %d chip port dump", chan);
	if (ISP_CAP_2KLOGIN(isp)) {
		lim = NPH_MAX_2K;
	} else {
		lim = NPH_MAX;
	}
	for (loopid = 0; loopid != lim; loopid++) {
		if (isp_getpdb(isp, chan, loopid, &pdb, dolock)) {
			continue;
		}
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGINFO, "Chan %d Loopid 0x%04x "
		    "PortID 0x%06x WWPN 0x%02x%02x%02x%02x%02x%02x%02x%02x",
		    chan, loopid, pdb.portid, pdb.portname[0], pdb.portname[1],
		    pdb.portname[2], pdb.portname[3], pdb.portname[4],
		    pdb.portname[5], pdb.portname[6], pdb.portname[7]);
	}
}

static uint64_t
isp_get_wwn(ispsoftc_t *isp, int chan, int loopid, int nodename)
{
	uint64_t wwn = INI_NONE;
	fcparam *fcp = FCPARAM(isp, chan);
	mbreg_t mbs;

	if (fcp->isp_fwstate < FW_READY ||
	    fcp->isp_loopstate < LOOP_PDB_RCVD) {
		return (wwn);
	}
	MBSINIT(&mbs, MBOX_GET_PORT_NAME, MBLOGALL & ~MBOX_COMMAND_PARAM_ERROR, 500000);
	if (ISP_CAP_2KLOGIN(isp)) {
		mbs.param[1] = loopid;
		if (nodename) {
			mbs.param[10] = 1;
		}
		mbs.param[9] = chan;
	} else {
		mbs.ibitm = 3;
		mbs.param[1] = loopid << 8;
		if (nodename) {
			mbs.param[1] |= 1;
		}
	}
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return (wwn);
	}
	if (IS_24XX(isp)) {
		wwn =
		    (((uint64_t)(mbs.param[2] >> 8))	<< 56) |
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
isp_fclink_test(ispsoftc_t *isp, int chan, int usdelay)
{
	mbreg_t mbs;
	int count, check_for_fabric, r;
	uint8_t lwfs;
	int loopid;
	fcparam *fcp;
	fcportdb_t *lp;
	isp_pdb_t pdb;

	fcp = FCPARAM(isp, chan);

	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC Link Test Entry", chan);
	ISP_MARK_PORTDB(isp, chan, 1);

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
		isp_fw_state(isp, chan);
		if (lwfs != fcp->isp_fwstate) {
			isp_prt(isp, ISP_LOGCONFIG|ISP_LOG_SANCFG, "Chan %d Firmware State <%s->%s>", chan, isp_fc_fw_statename((int)lwfs), isp_fc_fw_statename((int)fcp->isp_fwstate));
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

		isp_prt(isp, ISP_LOGDEBUG1, "usec%d: 0x%lx->0x%lx enano 0x%x%08x", count, (long) GET_NANOSEC(&hra), (long) GET_NANOSEC(&hrb), (uint32_t)(enano >> 32), (uint32_t)(enano));

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
				ISP_SLEEP(isp, 4000000);
				enano -= (uint64_t) 4000000000U;
			}
			wrk = enano;
			wrk /= 1000;
			ISP_SLEEP(isp, wrk);
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
		isp_prt(isp, ISP_LOG_SANCFG, "%s: chan %d not at FW_READY state", __func__, chan);
		return (-1);
	}

	/*
	 * Get our Loop ID and Port ID.
	 */
	MBSINIT(&mbs, MBOX_GET_LOOP_ID, MBLOGALL, 0);
	mbs.param[9] = chan;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return (-1);
	}

	if (ISP_CAP_2KLOGIN(isp)) {
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
	} else if (fcp->isp_topo == TOPO_FL_PORT || fcp->isp_topo == TOPO_F_PORT) {
		check_for_fabric = 1;
	} else {
		check_for_fabric = 0;
	}

	/*
	 * Check to make sure we got a valid loopid
	 * The 24XX seems to mess this up for multiple channels.
	 */
	if (fcp->isp_topo == TOPO_FL_PORT || fcp->isp_topo == TOPO_NL_PORT) {
		uint8_t alpa = fcp->isp_portid;

		if (alpa == 0) {
			/* "Cannot Happen" */
			isp_prt(isp, ISP_LOGWARN, "Zero AL_PA for Loop Topology?");
		} else {
			int i;
			for (i = 0; alpa_map[i]; i++) {
				if (alpa_map[i] == alpa) {
					break;
				}
			}
			if (alpa_map[i] && fcp->isp_loopid != i) {
				isp_prt(isp, ISP_LOG_SANCFG,
				    "Chan %d deriving loopid %d from AL_PA map (AL_PA 0x%x) and ignoring returned value %d (AL_PA 0x%x)",
				    chan, i, alpa_map[i], fcp->isp_loopid, alpa);
				fcp->isp_loopid = i;
			}
		}
	}


	if (IS_24XX(isp)) { /* XXX SHOULDN'T THIS BE FOR 2K F/W? XXX */
		loopid = NPH_FL_ID;
	} else {
		loopid = FL_ID;
	}
	if (check_for_fabric) {
		r = isp_getpdb(isp, chan, loopid, &pdb, 1);
		if (r && (fcp->isp_topo == TOPO_F_PORT || fcp->isp_topo == TOPO_FL_PORT)) {
			isp_prt(isp, ISP_LOGWARN, "fabric topology but cannot get info about fabric controller (0x%x)", r);
			fcp->isp_topo = TOPO_PTP_STUB;
		}
	} else {
		r = -1;
	}
	if (r == 0) {
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
		lp->prli_word3 = pdb.prli_word3;
		lp->portid = pdb.portid;
		lp->handle = pdb.handle;
		lp->new_portid = lp->portid;
		lp->new_prli_word3 = lp->prli_word3;
		if (IS_24XX(isp)) {
			if (check_for_fabric) {
				/*
				 * The mbs is still hanging out from the MBOX_GET_LOOP_ID above.
				 */
				fcp->isp_fabric_params = mbs.param[7];
			} else {
				fcp->isp_fabric_params = 0;
			}
			if (chan) {
				fcp->isp_sns_hdl = NPH_SNS_HDLBASE + chan;
				r = isp_plogx(isp, chan, fcp->isp_sns_hdl, SNS_PORT_ID, PLOGX_FLG_CMD_PLOGI | PLOGX_FLG_COND_PLOGI | PLOGX_FLG_SKIP_PRLI, 0);
				if (r) {
					isp_prt(isp, ISP_LOGWARN, "%s: Chan %d cannot log into SNS", __func__, chan);
					return (-1);
				}
			} else {
				fcp->isp_sns_hdl = NPH_SNS_ID;
			}
			r = isp_register_fc4_type_24xx(isp, chan);
		} else {
			fcp->isp_sns_hdl = SNS_ID;
			r = isp_register_fc4_type(isp, chan);
		}
		if (r) {
			isp_prt(isp, ISP_LOGWARN|ISP_LOG_SANCFG, "%s: register fc4 type failed", __func__);
			return (-1);
		}
	} else {
not_on_fabric:
		fcp->portdb[FL_ID].state = FC_PORTDB_STATE_NIL;
	}

	fcp->isp_gbspeed = 1;
	if (IS_23XX(isp) || IS_24XX(isp)) {
		MBSINIT(&mbs, MBOX_GET_SET_DATA_RATE, MBLOGALL, 3000000);
		mbs.param[1] = MBGSD_GET_RATE;
		/* mbs.param[2] undefined if we're just getting rate */
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			if (mbs.param[1] == MBGSD_EIGHTGB) {
				isp_prt(isp, ISP_LOGINFO, "Chan %d 8Gb link speed", chan);
				fcp->isp_gbspeed = 8;
			} else if (mbs.param[1] == MBGSD_FOURGB) {
				isp_prt(isp, ISP_LOGINFO, "Chan %d 4Gb link speed", chan);
				fcp->isp_gbspeed = 4;
			} else if (mbs.param[1] == MBGSD_TWOGB) {
				isp_prt(isp, ISP_LOGINFO, "Chan %d 2Gb link speed", chan);
				fcp->isp_gbspeed = 2;
			} else if (mbs.param[1] == MBGSD_ONEGB) {
				isp_prt(isp, ISP_LOGINFO, "Chan %d 1Gb link speed", chan);
				fcp->isp_gbspeed = 1;
			}
		}
	}

	/*
	 * Announce ourselves, too.
	 */
	isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGCONFIG, topology, chan, (uint32_t) (fcp->isp_wwpn >> 32), (uint32_t) fcp->isp_wwpn, fcp->isp_portid, fcp->isp_loopid, isp_fc_toponame(fcp));
	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC Link Test Complete", chan);
	return (0);
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
 * devices and refresh old ones ot make sure that they point to the correct
 * entities.
 */
static int
isp_pdb_sync(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
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
			if (isp_scan_loop(isp, chan) != 0) {
				isp_prt(isp, ISP_LOGWARN,
				    "isp_pdb_sync: isp_scan_loop failed");
				return (-1);
			}
		}
	}

	if (fcp->isp_topo == TOPO_F_PORT || fcp->isp_topo == TOPO_FL_PORT) {
		if (fcp->isp_loopstate < LOOP_FSCAN_DONE) {
			if (isp_scan_fabric(isp, chan) != 0) {
				isp_prt(isp, ISP_LOGWARN,
				    "isp_pdb_sync: isp_scan_fabric failed");
				return (-1);
			}
		}
	}

	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Synchronizing PDBs", chan);

	fcp->isp_loopstate = LOOP_SYNCING_PDB;

	for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
		lp = &fcp->portdb[dbidx];

		if (lp->state == FC_PORTDB_STATE_NIL ||
		    lp->state == FC_PORTDB_STATE_VALID) {
			continue;
		}

		switch (lp->state) {
		case FC_PORTDB_STATE_PROBATIONAL:
		case FC_PORTDB_STATE_DEAD:
			lp->state = FC_PORTDB_STATE_NIL;
			isp_async(isp, ISPASYNC_DEV_GONE, chan, lp);
			if (lp->autologin == 0) {
				(void) isp_plogx(isp, chan, lp->handle,
				    lp->portid,
				    PLOGX_FLG_CMD_LOGO |
				    PLOGX_FLG_IMPLICIT |
				    PLOGX_FLG_FREE_NPHDL, 0);
			} else {
				lp->autologin = 0;
			}
			lp->new_prli_word3 = 0;
			lp->new_portid = 0;
			/*
			 * Note that we might come out of this with our state
			 * set to FC_PORTDB_STATE_ZOMBIE.
			 */
			break;
		case FC_PORTDB_STATE_NEW:
			lp->portid = lp->new_portid;
			lp->prli_word3 = lp->new_prli_word3;
			lp->state = FC_PORTDB_STATE_VALID;
			isp_async(isp, ISPASYNC_DEV_ARRIVED, chan, lp);
			lp->new_prli_word3 = 0;
			lp->new_portid = 0;
			break;
		case FC_PORTDB_STATE_CHANGED:
			lp->state = FC_PORTDB_STATE_VALID;
			isp_async(isp, ISPASYNC_DEV_CHANGED, chan, lp);
			lp->portid = lp->new_portid;
			lp->prli_word3 = lp->new_prli_word3;
			lp->new_prli_word3 = 0;
			lp->new_portid = 0;
			break;
		case FC_PORTDB_STATE_PENDING_VALID:
			lp->portid = lp->new_portid;
			lp->prli_word3 = lp->new_prli_word3;
			lp->state = FC_PORTDB_STATE_VALID;
			isp_async(isp, ISPASYNC_DEV_STAYED, chan, lp);
			if (dbidx != FL_ID) {
				lp->new_prli_word3 = 0;
				lp->new_portid = 0;
			}
			break;
		case FC_PORTDB_STATE_ZOMBIE:
			break;
		default:
			isp_prt(isp, ISP_LOGWARN,
			    "isp_pdb_sync: state %d for idx %d",
			    lp->state, dbidx);
			isp_dump_portdb(isp, chan);
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
isp_scan_loop(ispsoftc_t *isp, int chan)
{
	fcportdb_t *lp, tmp;
	fcparam *fcp = FCPARAM(isp, chan);
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
	 * The firmware has (typically) peformed a PLOGI for us. We skip this
	 * step if we're a ISP_24XX in NP-IV mode.
	 *
	 * If we're a N-port connection, we treat this is a short loop (0..1).
	 */
	switch (fcp->isp_topo) {
	case TOPO_NL_PORT:
		lim = LOCAL_LOOP_LIM;
		break;
	case TOPO_FL_PORT:
		if (IS_24XX(isp) && isp->isp_nchan > 1) {
			isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Skipping Local Loop Scan", chan);
			fcp->isp_loopstate = LOOP_LSCAN_DONE;
			return (0);
		}
		lim = LOCAL_LOOP_LIM;
		break;
	case TOPO_N_PORT:
		lim = 2;
		break;
	default:
		isp_prt(isp, ISP_LOG_SANCFG, "Chan %d no loop topology to scan", chan);
		fcp->isp_loopstate = LOOP_LSCAN_DONE;
		return (0);
	}

	fcp->isp_loopstate = LOOP_SCANNING_LOOP;

	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC scan loop 0..%d", chan, lim-1);

	/*
	 * Run through the list and get the port database info for each one.
	 */
	for (handle = 0; handle < lim; handle++) {
		int r;
		/*
		 * Don't scan "special" ids.
		 */
		if (handle >= FL_ID && handle <= SNS_ID) {
			continue;
		}
		if (ISP_CAP_2KLOGIN(isp)) {
			if (handle >= NPH_RESERVED && handle <= NPH_IP_BCST) {
				continue;
			}
		}
		/*
		 * In older cards with older f/w GET_PORT_DATABASE has been
		 * known to hang. This trick gets around that problem.
		 */
		if (IS_2100(isp) || IS_2200(isp)) {
			uint64_t node_wwn = isp_get_wwn(isp, chan, handle, 1);
			if (fcp->isp_loopstate < LOOP_SCANNING_LOOP) {
				isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC scan loop DONE (bad)", chan);
				return (-1);
			}
			if (node_wwn == INI_NONE) {
				continue;
			}
		}

		/*
		 * Get the port database entity for this index.
		 */
		r = isp_getpdb(isp, chan, handle, &pdb, 1);
		if (r != 0) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "Chan %d FC scan loop handle %d returned %x",
			    chan, handle, r);
			if (fcp->isp_loopstate < LOOP_SCANNING_LOOP) {
				ISP_MARK_PORTDB(isp, chan, 1);
				isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC scan loop DONE (bad)", chan);
				return (-1);
			}
			continue;
		}

		if (fcp->isp_loopstate < LOOP_SCANNING_LOOP) {
			ISP_MARK_PORTDB(isp, chan, 1);
			isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC scan loop DONE (bad)", chan);
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
			    "Chan %d cannot synchronize port database", chan);
			ISP_MARK_PORTDB(isp, chan, 1);
			isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC scan loop DONE (bad)", chan);
			return (-1);
		}

		/*
		 * Save the pertinent info locally.
		 */
		MAKE_WWN_FROM_NODE_NAME(tmp.node_wwn, pdb.nodename);
		MAKE_WWN_FROM_NODE_NAME(tmp.port_wwn, pdb.portname);
		tmp.prli_word3 = pdb.prli_word3;
		tmp.portid = pdb.portid;
		tmp.handle = pdb.handle;

		/*
		 * Check to make sure it's still a valid entry. The 24XX seems
		 * to return a portid but not a WWPN/WWNN or role for devices
		 * which shift on a loop.
		 */
		if (tmp.node_wwn == 0 || tmp.port_wwn == 0 || tmp.portid == 0) {
			int a, b, c;
			isp_prt(isp, ISP_LOGWARN,
			    "Chan %d bad pdb (WWNN %016jx, WWPN %016jx, PortID %06x, W3 0x%x, H 0x%x) @ handle 0x%x",
			    chan, tmp.node_wwn, tmp.port_wwn, tmp.portid, tmp.prli_word3, tmp.handle, handle);
			a = (tmp.node_wwn == 0);
			b = (tmp.port_wwn == 0);
			c = (tmp.portid == 0);
			if (a == 0 && b == 0) {
				tmp.node_wwn =
				    isp_get_wwn(isp, chan, handle, 1);
				tmp.port_wwn =
				    isp_get_wwn(isp, chan, handle, 0);
				if (tmp.node_wwn && tmp.port_wwn) {
					isp_prt(isp, ISP_LOGWARN, "DODGED!");
					goto cont;
				}
			}
			isp_dump_portdb(isp, chan);
			continue;
		}
  cont:

		/*
		 * Now search the entire port database
		 * for the same Port WWN.
		 */
		if (isp_find_pdb_by_wwn(isp, chan, tmp.port_wwn, &lp)) {
			/*
			 * Okay- we've found a non-nil entry that matches.
			 * Check to make sure it's probational or a zombie.
			 */
			if (lp->state != FC_PORTDB_STATE_PROBATIONAL &&
			    lp->state != FC_PORTDB_STATE_ZOMBIE &&
			    lp->state != FC_PORTDB_STATE_VALID) {
				isp_prt(isp, ISP_LOGERR,
				    "Chan %d [%d] not probational/zombie (0x%x)",
				    chan, FC_PORTDB_TGT(isp, chan, lp), lp->state);
				isp_dump_portdb(isp, chan);
				ISP_MARK_PORTDB(isp, chan, 1);
				isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC scan loop DONE (bad)", chan);
				return (-1);
			}

			/*
			 * Mark the device as something the f/w logs into
			 * automatically.
			 */
			lp->autologin = 1;
			lp->node_wwn = tmp.node_wwn;

			/*
			 * Check to make see if really still the same
			 * device. If it is, we mark it pending valid.
			 */
			if (lp->portid == tmp.portid && lp->handle == tmp.handle && lp->prli_word3 == tmp.prli_word3) {
				lp->new_portid = tmp.portid;
				lp->new_prli_word3 = tmp.prli_word3;
				lp->state = FC_PORTDB_STATE_PENDING_VALID;
				isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Loop Port 0x%06x@0x%04x Pending Valid", chan, tmp.portid, tmp.handle);
				continue;
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
			isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Loop Port 0x%06x@0x%04x changed", chan, tmp.portid, tmp.handle);
			lp->state = FC_PORTDB_STATE_CHANGED;
			lp->new_portid = tmp.portid;
			lp->new_prli_word3 = tmp.prli_word3;
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
			isp_prt(isp, ISP_LOGERR,
			    "Chan %d out of portdb entries", chan);
			continue;
		}
		lp = &fcp->portdb[i];

		ISP_MEMZERO(lp, sizeof (fcportdb_t));
		lp->autologin = 1;
		lp->state = FC_PORTDB_STATE_NEW;
		lp->new_portid = tmp.portid;
		lp->new_prli_word3 = tmp.prli_word3;
		lp->handle = tmp.handle;
		lp->port_wwn = tmp.port_wwn;
		lp->node_wwn = tmp.node_wwn;
		isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Loop Port 0x%06x@0x%04x is New Entry", chan, tmp.portid, tmp.handle);
	}
	fcp->isp_loopstate = LOOP_LSCAN_DONE;
	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC scan loop DONE", chan);
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
#define	GIDLEN	((ISP_FC_SCRLEN >> 1) - 16 - SNS_GID_FT_REQ_SIZE)
#define	NGENT	((GIDLEN - 16) >> 2)

#define	IGPOFF	(2 * QENTRY_LEN)
#define	OGPOFF	(ISP_FC_SCRLEN >> 1)
#define	ZTXOFF	(ISP_FC_SCRLEN - (1 * QENTRY_LEN))
#define	CTXOFF	(ISP_FC_SCRLEN - (2 * QENTRY_LEN))
#define	XTXOFF	(ISP_FC_SCRLEN - (3 * QENTRY_LEN))

static int
isp_gid_ft_sns(ispsoftc_t *isp, int chan)
{
	union {
		sns_gid_ft_req_t _x;
		uint8_t _y[SNS_GID_FT_REQ_SIZE];
	} un;
	fcparam *fcp = FCPARAM(isp, chan);
	sns_gid_ft_req_t *rq = &un._x;
	mbreg_t mbs;

	isp_prt(isp, ISP_LOGDEBUG0, "Chan %d scanning fabric (GID_FT) via SNS", chan);

	ISP_MEMZERO(rq, SNS_GID_FT_REQ_SIZE);
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
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, SNS_GID_FT_REQ_SIZE, chan);

	MBSINIT(&mbs, MBOX_SEND_SNS, MBLOGALL, 10000000);
	mbs.param[0] = MBOX_SEND_SNS;
	mbs.param[1] = SNS_GID_FT_REQ_SIZE >> 1;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
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
isp_gid_ft_ct_passthru(ispsoftc_t *isp, int chan)
{
	mbreg_t mbs;
	fcparam *fcp = FCPARAM(isp, chan);
	union {
		isp_ct_pt_t plocal;
		ct_hdr_t clocal;
		uint8_t q[QENTRY_LEN];
	} un;
	isp_ct_pt_t *pt;
	ct_hdr_t *ct;
	uint32_t *rp;
	uint8_t *scp = fcp->isp_scratch;

	isp_prt(isp, ISP_LOGDEBUG0, "Chan %d scanning fabric (GID_FT) via CT", chan);

	if (!IS_24XX(isp)) {
		return (1);
	}

	/*
	 * Build a Passthrough IOCB in memory.
	 */
	pt = &un.plocal;
	ISP_MEMZERO(un.q, QENTRY_LEN);
	pt->ctp_header.rqs_entry_count = 1;
	pt->ctp_header.rqs_entry_type = RQSTYPE_CT_PASSTHRU;
	pt->ctp_handle = 0xffffffff;
	pt->ctp_nphdl = fcp->isp_sns_hdl;
	pt->ctp_cmd_cnt = 1;
	pt->ctp_vpidx = ISP_GET_VPIDX(isp, chan);
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
	ISP_MEMZERO(ct, sizeof (*ct));
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
	ISP_MEMZERO(&scp[ZTXOFF], QENTRY_LEN);
	MBSINIT(&mbs, MBOX_EXEC_COMMAND_IOCB_A64, MBLOGALL, 500000);
	mbs.param[1] = QENTRY_LEN;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma + CTXOFF);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma + CTXOFF);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma + CTXOFF);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma + CTXOFF);
	MEMORYBARRIER(isp, SYNC_SFORDEV, XTXOFF, 2 * QENTRY_LEN, chan);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return (-1);
	}
	MEMORYBARRIER(isp, SYNC_SFORCPU, ZTXOFF, QENTRY_LEN, chan);
	pt = &un.plocal;
	isp_get_ct_pt(isp, (isp_ct_pt_t *) &scp[ZTXOFF], pt);
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "IOCB response", QENTRY_LEN, pt);
	}

	if (pt->ctp_status && pt->ctp_status != RQCS_DATA_UNDERRUN) {
		isp_prt(isp, ISP_LOGWARN,
		    "Chan %d ISP GID FT CT Passthrough returned 0x%x",
		    chan, pt->ctp_status);
		return (-1);
	}
	MEMORYBARRIER(isp, SYNC_SFORCPU, IGPOFF, GIDLEN + 16, chan);
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "CT response", GIDLEN+16, &scp[IGPOFF]);
	}
	return (0);
}

static int
isp_scan_fabric(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	uint32_t portid;
	uint16_t handle, oldhandle, loopid;
	isp_pdb_t pdb;
	int portidx, portlim, r;
	sns_gid_ft_rsp_t *rs0, *rs1;

	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC Scan Fabric", chan);
	if (fcp->isp_fwstate != FW_READY || fcp->isp_loopstate < LOOP_LSCAN_DONE) {
		return (-1);
	}
	if (fcp->isp_loopstate > LOOP_SCANNING_FABRIC) {
		return (0);
	}
	if (fcp->isp_topo != TOPO_FL_PORT && fcp->isp_topo != TOPO_F_PORT) {
		fcp->isp_loopstate = LOOP_FSCAN_DONE;
		isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC Scan Fabric Done (no fabric)", chan);
		return (0);
	}

	fcp->isp_loopstate = LOOP_SCANNING_FABRIC;
	if (FC_SCRATCH_ACQUIRE(isp, chan)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		ISP_MARK_PORTDB(isp, chan, 1);
		return (-1);
	}
	if (fcp->isp_loopstate < LOOP_SCANNING_FABRIC) {
		FC_SCRATCH_RELEASE(isp, chan);
		ISP_MARK_PORTDB(isp, chan, 1);
		return (-1);
	}

	/*
	 * Make sure we still are logged into the fabric controller.
	 */
	if (IS_24XX(isp)) {	/* XXX SHOULDN'T THIS BE TRUE FOR 2K F/W? XXX */
		loopid = NPH_FL_ID;
	} else {
		loopid = FL_ID;
	}
	r = isp_getpdb(isp, chan, loopid, &pdb, 0);
	if (r == MBOX_NOT_LOGGED_IN) {
		isp_dump_chip_portdb(isp, chan, 0);
	}
	if (r) {
		fcp->isp_loopstate = LOOP_PDB_RCVD;
		FC_SCRATCH_RELEASE(isp, chan);
		ISP_MARK_PORTDB(isp, chan, 1);
		return (-1);
	}

	if (IS_24XX(isp)) {
		r = isp_gid_ft_ct_passthru(isp, chan);
	} else {
		r = isp_gid_ft_sns(isp, chan);
	}

	if (fcp->isp_loopstate < LOOP_SCANNING_FABRIC) {
		FC_SCRATCH_RELEASE(isp, chan);
		ISP_MARK_PORTDB(isp, chan, 1);
		return (-1);
	}

	if (r > 0) {
		fcp->isp_loopstate = LOOP_FSCAN_DONE;
		FC_SCRATCH_RELEASE(isp, chan);
		return (0);
	} else if (r < 0) {
		fcp->isp_loopstate = LOOP_PDB_RCVD;	/* try again */
		FC_SCRATCH_RELEASE(isp, chan);
		return (0);
	}

	MEMORYBARRIER(isp, SYNC_SFORCPU, IGPOFF, GIDLEN, chan);
	rs0 = (sns_gid_ft_rsp_t *) ((uint8_t *)fcp->isp_scratch+IGPOFF);
	rs1 = (sns_gid_ft_rsp_t *) ((uint8_t *)fcp->isp_scratch+OGPOFF);
	isp_get_gid_ft_response(isp, rs0, rs1, NGENT);
	if (fcp->isp_loopstate < LOOP_SCANNING_FABRIC) {
		FC_SCRATCH_RELEASE(isp, chan);
		ISP_MARK_PORTDB(isp, chan, 1);
		return (-1);
	}
	if (rs1->snscb_cthdr.ct_cmd_resp != LS_ACC) {
		int level;
		if (rs1->snscb_cthdr.ct_reason == 9 && rs1->snscb_cthdr.ct_explanation == 7) {
			level = ISP_LOG_SANCFG;
		} else {
			level = ISP_LOGWARN;
		}
		isp_prt(isp, level, "Chan %d Fabric Nameserver rejected GID_FT"
		    " (Reason=0x%x Expl=0x%x)", chan,
		    rs1->snscb_cthdr.ct_reason,
		    rs1->snscb_cthdr.ct_explanation);
		FC_SCRATCH_RELEASE(isp, chan);
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
	oldhandle = FCPARAM(isp, 0)->isp_lasthdl;

	/*
	 * Go through the list and remove duplicate port ids.
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
		    "fabric too big for scratch area: increase ISP_FC_SCRLEN");
	}
	portlim = portidx + 1;
	isp_prt(isp, ISP_LOG_SANCFG,
	    "Chan %d got %d ports back from name server", chan, portlim);

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
			isp_prt(isp, ISP_LOG_SANCFG, "Chan %d removing duplicate PortID 0x%06x entry from list", chan, portid);
		}
	}

	/*
	 * We now have a list of Port IDs for all FC4 SCSI devices
	 * that the Fabric Name server knows about.
	 *
	 * For each entry on this list go through our port database looking
	 * for probational entries- if we find one, then an old entry is
	 * maybe still this one. We get some information to find out.
	 *
	 * Otherwise, it's a new fabric device, and we log into it
	 * (unconditionally). After searching the entire database
	 * again to make sure that we never ever ever ever have more
	 * than one entry that has the same PortID or the same
	 * WWNN/WWPN duple, we enter the device into our database.
	 */

	for (portidx = 0; portidx < portlim; portidx++) {
		fcportdb_t *lp;
		uint64_t wwnn, wwpn;
		int dbidx, nr;

		portid =
		    ((rs1->snscb_ports[portidx].portid[0]) << 16) |
		    ((rs1->snscb_ports[portidx].portid[1]) << 8) |
		    ((rs1->snscb_ports[portidx].portid[2]));

		if (portid == 0) {
			isp_prt(isp, ISP_LOG_SANCFG,
			    "Chan %d skipping null PortID at idx %d",
			    chan, portidx);
			continue;
		}

		/*
		 * Skip ourselves here and on other channels. If we're
		 * multi-id, we can't check the portids in other FCPARAM
		 * arenas because the resolutions here aren't synchronized.
		 * The best way to do this is to exclude looking at portids
		 * that have the same domain and area code as our own
		 * portid.
		 */
		if (ISP_CAP_MULTI_ID(isp) && isp->isp_nchan > 1) {
			if ((portid >> 8) == (fcp->isp_portid >> 8)) {
				isp_prt(isp, ISP_LOG_SANCFG,
				    "Chan %d skip PortID 0x%06x",
				    chan, portid);
				continue;
			}
		} else if (portid == fcp->isp_portid) {
			isp_prt(isp, ISP_LOG_SANCFG,
			    "Chan %d skip ourselves on @ PortID 0x%06x",
			    chan, portid);
			continue;
		}

		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d Checking Fabric Port 0x%06x", chan, portid);

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

			r = isp_getpdb(isp, chan, lp->handle, &pdb, 0);
			if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
				FC_SCRATCH_RELEASE(isp, chan);
				ISP_MARK_PORTDB(isp, chan, 1);
				return (-1);
			}
			if (r != 0) {
				lp->new_portid = portid;
				lp->state = FC_PORTDB_STATE_DEAD;
				isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Fabric PortID 0x%06x handle 0x%x is dead (%d)", chan, portid, lp->handle, r);
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
			    (lp->node_wwn != 0 && wwnn != lp->node_wwn)) {
				isp_prt(isp, ISP_LOG_SANCFG,
				    fconf, chan, dbidx, pdb.handle, pdb.portid,
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
				if ((fcp->role & ISP_ROLE_INITIATOR) == 0 ||
				    isp_login_device(isp, chan, portid, &pdb,
				     &oldhandle)) {
					lp->new_portid = portid;
					lp->state = FC_PORTDB_STATE_DEAD;
					if (fcp->isp_loopstate !=
					    LOOP_SCANNING_FABRIC) {
						FC_SCRATCH_RELEASE(isp, chan);
						ISP_MARK_PORTDB(isp, chan, 1);
						return (-1);
					}
					continue;
				}
				if (fcp->isp_loopstate !=
				    LOOP_SCANNING_FABRIC) {
					FC_SCRATCH_RELEASE(isp, chan);
					ISP_MARK_PORTDB(isp, chan, 1);
					return (-1);
				}
				FCPARAM(isp, 0)->isp_lasthdl = oldhandle;
				MAKE_WWN_FROM_NODE_NAME(wwnn, pdb.nodename);
				MAKE_WWN_FROM_NODE_NAME(wwpn, pdb.portname);
				if (wwpn != lp->port_wwn ||
				    (lp->node_wwn != 0 && wwnn != lp->node_wwn)) {
					isp_prt(isp, ISP_LOGWARN, "changed WWN"
					    " after relogin");
					lp->new_portid = portid;
					lp->state = FC_PORTDB_STATE_DEAD;
					continue;
				}

				lp->handle = pdb.handle;
				handle_changed++;
			}

			nr = pdb.prli_word3;

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
			lp->new_prli_word3 = nr;
			if (pdb.portid != lp->portid || nr != lp->prli_word3 || handle_changed) {
				isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Fabric Port 0x%06x changed", chan, portid);
				lp->state = FC_PORTDB_STATE_CHANGED;
			} else {
				isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Fabric Port 0x%06x Now Pending Valid", chan, portid);
				lp->state = FC_PORTDB_STATE_PENDING_VALID;
			}
			continue;
		}

		if ((fcp->role & ISP_ROLE_INITIATOR) == 0)
			continue;

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
			isp_prt(isp, ISP_LOGWARN, "Chan %d PortID 0x%06x "
			    "already at %d handle %d state %d",
			    chan, portid, dbidx, lp->handle, lp->state);
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
		if (isp_login_device(isp, chan, portid, &pdb, &oldhandle)) {
			if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
				FC_SCRATCH_RELEASE(isp, chan);
				ISP_MARK_PORTDB(isp, chan, 1);
				return (-1);
			}
			continue;
		}
		if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
			FC_SCRATCH_RELEASE(isp, chan);
			ISP_MARK_PORTDB(isp, chan, 1);
			return (-1);
		}
		FCPARAM(isp, 0)->isp_lasthdl = oldhandle;

		handle = pdb.handle;
		MAKE_WWN_FROM_NODE_NAME(wwnn, pdb.nodename);
		MAKE_WWN_FROM_NODE_NAME(wwpn, pdb.portname);
		nr = pdb.prli_word3;

		/*
		 * And go through the database *one* more time to make sure
		 * that we do not make more than one entry that has the same
		 * WWNN/WWPN duple
		 */
		for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
			if (dbidx >= FL_ID && dbidx <= SNS_ID) {
				continue;
			}
			if ((fcp->portdb[dbidx].node_wwn == wwnn ||
			     fcp->portdb[dbidx].node_wwn == 0) &&
			    fcp->portdb[dbidx].port_wwn == wwpn) {
				break;
			}
		}

		if (dbidx == MAX_FC_TARG) {
			ISP_MEMZERO(lp, sizeof (fcportdb_t));
			lp->handle = handle;
			lp->node_wwn = wwnn;
			lp->port_wwn = wwpn;
			lp->new_portid = portid;
			lp->new_prli_word3 = nr;
			lp->state = FC_PORTDB_STATE_NEW;
			isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Fabric Port 0x%06x is a New Entry", chan, portid);
			continue;
		}

    		if (fcp->portdb[dbidx].state != FC_PORTDB_STATE_ZOMBIE) {
			isp_prt(isp, ISP_LOGWARN,
			    "Chan %d PortID 0x%x 0x%08x%08x/0x%08x%08x %ld "
			    "already at idx %d, state 0x%x", chan, portid,
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
		lp->node_wwn = wwnn;
		lp->new_portid = portid;
		lp->new_prli_word3 = nr;
		if (lp->portid != portid || lp->prli_word3 != nr) {
			isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Zombie Fabric Port 0x%06x Now Changed", chan, portid);
			lp->state = FC_PORTDB_STATE_CHANGED;
		} else {
			isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Zombie Fabric Port 0x%06x Now Pending Valid", chan, portid);
			lp->state = FC_PORTDB_STATE_PENDING_VALID;
		}
	}

	FC_SCRATCH_RELEASE(isp, chan);
	if (fcp->isp_loopstate != LOOP_SCANNING_FABRIC) {
		ISP_MARK_PORTDB(isp, chan, 1);
		return (-1);
	}
	fcp->isp_loopstate = LOOP_FSCAN_DONE;
	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC Scan Fabric Done", chan);
	return (0);
}

/*
 * Find an unused handle and try and use to login to a port.
 */
static int
isp_login_device(ispsoftc_t *isp, int chan, uint32_t portid, isp_pdb_t *p, uint16_t *ohp)
{
	int lim, i, r;
	uint16_t handle;

	if (ISP_CAP_2KLOGIN(isp)) {
		lim = NPH_MAX_2K;
	} else {
		lim = NPH_MAX;
	}

	handle = isp_nxt_handle(isp, chan, *ohp);
	for (i = 0; i < lim; i++) {
		/*
		 * See if we're still logged into something with
		 * this handle and that something agrees with this
		 * port id.
		 */
		r = isp_getpdb(isp, chan, handle, p, 0);
		if (r == 0 && p->portid != portid) {
			(void) isp_plogx(isp, chan, handle, portid, PLOGX_FLG_CMD_LOGO | PLOGX_FLG_IMPLICIT | PLOGX_FLG_FREE_NPHDL, 1);
		} else if (r == 0) {
			break;
		}
		if (FCPARAM(isp, chan)->isp_loopstate != LOOP_SCANNING_FABRIC) {
			return (-1);
		}
		/*
		 * Now try and log into the device
		 */
		r = isp_plogx(isp, chan, handle, portid, PLOGX_FLG_CMD_PLOGI, 1);
		if (FCPARAM(isp, chan)->isp_loopstate != LOOP_SCANNING_FABRIC) {
			return (-1);
		}
		if (r == 0) {
			*ohp = handle;
			break;
		} else if ((r & 0xffff) == MBOX_PORT_ID_USED) {
			/*
			 * If we get here, then the firmwware still thinks we're logged into this device, but with a different
			 * handle. We need to break that association. We used to try and just substitute the handle, but then
			 * failed to get any data via isp_getpdb (below).
			 */
			if (isp_plogx(isp, chan, r >> 16, portid, PLOGX_FLG_CMD_LOGO | PLOGX_FLG_IMPLICIT | PLOGX_FLG_FREE_NPHDL, 1)) {
				isp_prt(isp, ISP_LOGERR, "baw... logout of %x failed", r >> 16);
			}
			if (FCPARAM(isp, chan)->isp_loopstate != LOOP_SCANNING_FABRIC) {
				return (-1);
			}
			r = isp_plogx(isp, chan, handle, portid, PLOGX_FLG_CMD_PLOGI, 1);
			if (FCPARAM(isp, chan)->isp_loopstate != LOOP_SCANNING_FABRIC) {
				return (-1);
			}
			if (r == 0) {
				*ohp = handle;
			} else {
				i = lim;
			}
			break;
		} else if ((r & 0xffff) == MBOX_LOOP_ID_USED) {
			/*
			 * Try the next loop id.
			 */
			*ohp = handle;
			handle = isp_nxt_handle(isp, chan, handle);
		} else {
			/*
			 * Give up.
			 */
			i = lim;
			break;
		}
	}

	if (i == lim) {
		isp_prt(isp, ISP_LOGWARN, "Chan %d PLOGI 0x%06x failed", chan, portid);
		return (-1);
	}

	/*
	 * If we successfully logged into it, get the PDB for it
	 * so we can crosscheck that it is still what we think it
	 * is and that we also have the role it plays
	 */
	r = isp_getpdb(isp, chan, handle, p, 0);
	if (FCPARAM(isp, chan)->isp_loopstate != LOOP_SCANNING_FABRIC) {
		return (-1);
	}
	if (r != 0) {
		isp_prt(isp, ISP_LOGERR, "Chan %d new device 0x%06x@0x%x disappeared", chan, portid, handle);
		return (-1);
	}

	if (p->handle != handle || p->portid != portid) {
		isp_prt(isp, ISP_LOGERR, "Chan %d new device 0x%06x@0x%x changed (0x%06x@0x%0x)",
		    chan, portid, handle, p->portid, p->handle);
		return (-1);
	}
	return (0);
}

static int
isp_register_fc4_type(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	uint8_t local[SNS_RFT_ID_REQ_SIZE];
	sns_screq_t *reqp = (sns_screq_t *) local;
	mbreg_t mbs;

	ISP_MEMZERO((void *) reqp, SNS_RFT_ID_REQ_SIZE);
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
	if (FC_SCRATCH_ACQUIRE(isp, chan)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return (-1);
	}
	isp_put_sns_request(isp, reqp, (sns_screq_t *) fcp->isp_scratch);
	MBSINIT(&mbs, MBOX_SEND_SNS, MBLOGALL, 1000000);
	mbs.param[1] = SNS_RFT_ID_REQ_SIZE >> 1;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, SNS_RFT_ID_REQ_SIZE, chan);
	isp_mboxcmd(isp, &mbs);
	FC_SCRATCH_RELEASE(isp, chan);
	if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
		return (0);
	} else {
		return (-1);
	}
}

static int
isp_register_fc4_type_24xx(ispsoftc_t *isp, int chan)
{
	mbreg_t mbs;
	fcparam *fcp = FCPARAM(isp, chan);
	union {
		isp_ct_pt_t plocal;
		rft_id_t clocal;
		uint8_t q[QENTRY_LEN];
	} un;
	isp_ct_pt_t *pt;
	ct_hdr_t *ct;
	rft_id_t *rp;
	uint8_t *scp = fcp->isp_scratch;

	if (FC_SCRATCH_ACQUIRE(isp, chan)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return (-1);
	}

	/*
	 * Build a Passthrough IOCB in memory.
	 */
	ISP_MEMZERO(un.q, QENTRY_LEN);
	pt = &un.plocal;
	pt->ctp_header.rqs_entry_count = 1;
	pt->ctp_header.rqs_entry_type = RQSTYPE_CT_PASSTHRU;
	pt->ctp_handle = 0xffffffff;
	pt->ctp_nphdl = fcp->isp_sns_hdl;
	pt->ctp_cmd_cnt = 1;
	pt->ctp_vpidx = ISP_GET_VPIDX(isp, chan);
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
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "IOCB CT Request", QENTRY_LEN, pt);
	}

	/*
	 * Build the CT header and command in memory.
	 *
	 * Note that the CT header has to end up as Big Endian format in memory.
	 */
	ISP_MEMZERO(&un.clocal, sizeof (un.clocal));
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
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "CT Header", QENTRY_LEN, &scp[XTXOFF]);
	}

	ISP_MEMZERO(&scp[ZTXOFF], sizeof (ct_hdr_t));

	MBSINIT(&mbs, MBOX_EXEC_COMMAND_IOCB_A64, MBLOGALL, 1000000);
	mbs.param[1] = QENTRY_LEN;
	mbs.param[2] = DMA_WD1(fcp->isp_scdma + CTXOFF);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma + CTXOFF);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma + CTXOFF);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma + CTXOFF);
	MEMORYBARRIER(isp, SYNC_SFORDEV, XTXOFF, 2 * QENTRY_LEN, chan);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		FC_SCRATCH_RELEASE(isp, chan);
		return (-1);
	}
	MEMORYBARRIER(isp, SYNC_SFORCPU, ZTXOFF, QENTRY_LEN, chan);
	pt = &un.plocal;
	isp_get_ct_pt(isp, (isp_ct_pt_t *) &scp[ZTXOFF], pt);
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "IOCB response", QENTRY_LEN, pt);
	}
	if (pt->ctp_status) {
		FC_SCRATCH_RELEASE(isp, chan);
		isp_prt(isp, ISP_LOGWARN,
		    "Chan %d Register FC4 Type CT Passthrough returned 0x%x",
		    chan, pt->ctp_status);
		return (1);
	}

	isp_get_ct_hdr(isp, (ct_hdr_t *) &scp[IGPOFF], ct);
	FC_SCRATCH_RELEASE(isp, chan);

	if (ct->ct_cmd_resp == LS_RJT) {
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOG_WARN1, "Chan %d Register FC4 Type rejected", chan);
		return (-1);
	} else if (ct->ct_cmd_resp == LS_ACC) {
		isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Register FC4 Type accepted", chan);
		return (0);
	} else {
		isp_prt(isp, ISP_LOGWARN, "Chan %d Register FC4 Type: 0x%x", chan, ct->ct_cmd_resp);
		return (-1);
	}
}

static uint16_t
isp_nxt_handle(ispsoftc_t *isp, int chan, uint16_t handle)
{
	int i;
	if (handle == NIL_HANDLE) {
		if (FCPARAM(isp, chan)->isp_topo == TOPO_F_PORT) {
			handle = 0;
		} else {
			handle = SNS_ID+1;
		}
	} else {
		handle += 1;
		if (handle >= FL_ID && handle <= SNS_ID) {
			handle = SNS_ID+1;
		}
		if (handle >= NPH_RESERVED && handle <= NPH_IP_BCST) {
			handle = NPH_IP_BCST + 1;
		}
		if (ISP_CAP_2KLOGIN(isp)) {
			if (handle == NPH_MAX_2K) {
				handle = 0;
			}
		} else {
			if (handle == NPH_MAX) {
				handle = 0;
			}
		}
	}
	if (handle == FCPARAM(isp, chan)->isp_loopid) {
		return (isp_nxt_handle(isp, chan, handle));
	}
	for (i = 0; i < MAX_FC_TARG; i++) {
		if (FCPARAM(isp, chan)->portdb[i].state ==
		    FC_PORTDB_STATE_NIL) {
			continue;
		}
		if (FCPARAM(isp, chan)->portdb[i].handle == handle) {
			return (isp_nxt_handle(isp, chan, handle));
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
	uint32_t handle, cdblen;
	uint8_t local[QENTRY_LEN];
	ispreq_t *reqp;
	void *cdbp, *qep;
	uint16_t *tptr;
	fcportdb_t *lp;
	int target, dmaresult;

	XS_INITERR(xs);
	isp = XS_ISP(xs);

	/*
	 * Check command CDB length, etc.. We really are limited to 16 bytes
	 * for Fibre Channel, but can do up to 44 bytes in parallel SCSI,
	 * but probably only if we're running fairly new firmware (we'll
	 * let the old f/w choke on an extended command queue entry).
	 */

	if (XS_CDBLEN(xs) > (IS_FC(isp)? 16 : 44) || XS_CDBLEN(xs) == 0) {
		isp_prt(isp, ISP_LOGERR, "unsupported cdb length (%d, CDB[0]=0x%x)", XS_CDBLEN(xs), XS_CDBP(xs)[0] & 0xff);
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}

	/*
	 * Translate the target to device handle as appropriate, checking
	 * for correct device state as well.
	 */
	target = XS_TGT(xs);
	if (IS_FC(isp)) {
		fcparam *fcp = FCPARAM(isp, XS_CHANNEL(xs));

		if ((fcp->role & ISP_ROLE_INITIATOR) == 0) {
			isp_prt(isp, ISP_LOG_WARN1,
			    "%d.%d.%jx I am not an initiator",
			    XS_CHANNEL(xs), target, (uintmax_t)XS_LUN(xs));
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_COMPLETE);
		}

		if (isp->isp_state != ISP_RUNSTATE) {
			isp_prt(isp, ISP_LOGERR, "Adapter not at RUNSTATE");
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
		}

		/*
		 * Try again later.
		 */
		if (fcp->isp_fwstate != FW_READY || fcp->isp_loopstate != LOOP_READY) {
			return (CMD_RQLATER);
		}

		isp_prt(isp, ISP_LOGDEBUG2, "XS_TGT(xs)=%d", target);
		lp = &fcp->portdb[target];
		if (target < 0 || target >= MAX_FC_TARG ||
		    lp->is_target == 0) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_COMPLETE);
		}
		if (lp->state == FC_PORTDB_STATE_ZOMBIE) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "%d.%d.%jx target zombie",
			    XS_CHANNEL(xs), target, (uintmax_t)XS_LUN(xs));
			return (CMD_RQLATER);
		}
		if (lp->state != FC_PORTDB_STATE_VALID) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "%d.%d.%jx bad db port state 0x%x",
			    XS_CHANNEL(xs), target, (uintmax_t)XS_LUN(xs), lp->state);
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_COMPLETE);
		}
	} else {
		sdparam *sdp = SDPARAM(isp, XS_CHANNEL(xs));
		if ((sdp->role & ISP_ROLE_INITIATOR) == 0) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "%d.%d.%jx I am not an initiator",
			    XS_CHANNEL(xs), target, (uintmax_t)XS_LUN(xs));
			XS_SETERR(xs, HBA_SELTIMEOUT);
			return (CMD_COMPLETE);
		}

		if (isp->isp_state != ISP_RUNSTATE) {
			isp_prt(isp, ISP_LOGERR, "Adapter not at RUNSTATE");
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
		}

		if (sdp->update) {
			isp_spi_update(isp, XS_CHANNEL(xs));
		}
		lp = NULL;
	}

 start_again:

	qep = isp_getrqentry(isp);
	if (qep == NULL) {
		isp_prt(isp, ISP_LOG_WARN1, "Request Queue Overflow");
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	}
	XS_SETERR(xs, HBA_NOERROR);

	/*
	 * Now see if we need to synchronize the ISP with respect to anything.
	 * We do dual duty here (cough) for synchronizing for busses other
	 * than which we got here to send a command to.
	 */
	reqp = (ispreq_t *) local;
	ISP_MEMZERO(local, QENTRY_LEN);
	if (ISP_TST_SENDMARKER(isp, XS_CHANNEL(xs))) {
		if (IS_24XX(isp)) {
			isp_marker_24xx_t *m = (isp_marker_24xx_t *) reqp;
			m->mrk_header.rqs_entry_count = 1;
			m->mrk_header.rqs_entry_type = RQSTYPE_MARKER;
			m->mrk_modifier = SYNC_ALL;
			isp_put_marker_24xx(isp, m, qep);
		} else {
			isp_marker_t *m = (isp_marker_t *) reqp;
			m->mrk_header.rqs_entry_count = 1;
			m->mrk_header.rqs_entry_type = RQSTYPE_MARKER;
			m->mrk_target = (XS_CHANNEL(xs) << 7);	/* bus # */
			m->mrk_modifier = SYNC_ALL;
			isp_put_marker(isp, m, qep);
		}
		ISP_SYNC_REQUEST(isp);
		ISP_SET_SENDMARKER(isp, XS_CHANNEL(xs), 0);
		goto start_again;
	}

	reqp->req_header.rqs_entry_count = 1;

	/*
	 * Select and install Header Code.
	 * Note that it might be overridden before going out
	 * if we're on a 64 bit platform. The lower level
	 * code (isp_send_cmd) will select the appropriate
	 * 64 bit variant if it needs to.
	 */
	if (IS_24XX(isp)) {
		reqp->req_header.rqs_entry_type = RQSTYPE_T7RQS;
	} else if (IS_FC(isp)) {
		reqp->req_header.rqs_entry_type = RQSTYPE_T2RQS;
	} else {
		if (XS_CDBLEN(xs) > 12) {
			reqp->req_header.rqs_entry_type = RQSTYPE_CMDONLY;
		} else {
			reqp->req_header.rqs_entry_type = RQSTYPE_REQUEST;
		}
	}

	/*
	 * Set task attributes
	 */
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
		/* XS_SET_RESID(xs, 0); */

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
		sdparam *sdp = SDPARAM(isp, XS_CHANNEL(xs));
		if ((sdp->isp_devparam[target].actv_flags & DPARM_TQING) && XS_TAG_P(xs)) {
			reqp->req_flags = XS_TAG_TYPE(xs);
		}
	}

	tptr = &reqp->req_time;

	/*
	 * NB: we do not support long CDBs (yet)
	 */
	cdblen = XS_CDBLEN(xs);

	if (IS_SCSI(isp)) {
		if (cdblen > sizeof (reqp->req_cdb)) {
			isp_prt(isp, ISP_LOGERR, "Command Length %u too long for this chip", cdblen);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
		}
		reqp->req_target = target | (XS_CHANNEL(xs) << 7);
		reqp->req_lun_trn = XS_LUN(xs);
		cdbp = reqp->req_cdb;
		reqp->req_cdblen = cdblen;
	} else if (IS_24XX(isp)) {
		ispreqt7_t *t7 = (ispreqt7_t *)local;

		if (cdblen > sizeof (t7->req_cdb)) {
			isp_prt(isp, ISP_LOGERR, "Command Length %u too long for this chip", cdblen);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
		}

		t7->req_nphdl = lp->handle;
		t7->req_tidlo = lp->portid;
		t7->req_tidhi = lp->portid >> 16;
		t7->req_vpidx = ISP_GET_VPIDX(isp, XS_CHANNEL(xs));
#if __FreeBSD_version >= 1000700
		be64enc(t7->req_lun, CAM_EXTLUN_BYTE_SWIZZLE(XS_LUN(xs)));
#else
		if (XS_LUN(xs) >= 256) {
			t7->req_lun[0] = XS_LUN(xs) >> 8;
			t7->req_lun[0] |= 0x40;
		}
		t7->req_lun[1] = XS_LUN(xs);
#endif
		if (FCPARAM(isp, XS_CHANNEL(xs))->fctape_enabled && (lp->prli_word3 & PRLI_WD3_RETRY)) {
			if (FCP_NEXT_CRN(isp, &t7->req_crn, xs)) {
				isp_prt(isp, ISP_LOG_WARN1,
				    "%d.%d.%jx cannot generate next CRN",
				    XS_CHANNEL(xs), target, (uintmax_t)XS_LUN(xs));
				XS_SETERR(xs, HBA_BOTCH);
				return (CMD_EAGAIN);
			}
		}
		tptr = &t7->req_time;
		cdbp = t7->req_cdb;
	} else {
		ispreqt2_t *t2 = (ispreqt2_t *)local;

		if (cdblen > sizeof t2->req_cdb) {
			isp_prt(isp, ISP_LOGERR, "Command Length %u too long for this chip", cdblen);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
		}
		if (FCPARAM(isp, XS_CHANNEL(xs))->fctape_enabled && (lp->prli_word3 & PRLI_WD3_RETRY)) {
			if (FCP_NEXT_CRN(isp, &t2->req_crn, xs)) {
				isp_prt(isp, ISP_LOG_WARN1,
				    "%d.%d.%jx cannot generate next CRN",
				    XS_CHANNEL(xs), target, (uintmax_t)XS_LUN(xs));
				XS_SETERR(xs, HBA_BOTCH);
				return (CMD_EAGAIN);
			}
		}
		if (ISP_CAP_2KLOGIN(isp)) {
			ispreqt2e_t *t2e = (ispreqt2e_t *)local;
			t2e->req_target = lp->handle;
			t2e->req_scclun = XS_LUN(xs);
#if __FreeBSD_version < 1000700
			if (XS_LUN(xs) >= 256)
				t2e->req_scclun |= 0x4000;
#endif
			cdbp = t2e->req_cdb;
		} else if (ISP_CAP_SCCFW(isp)) {
			ispreqt2_t *t2 = (ispreqt2_t *)local;
			t2->req_target = lp->handle;
			t2->req_scclun = XS_LUN(xs);
#if __FreeBSD_version < 1000700
			if (XS_LUN(xs) >= 256)
				t2->req_scclun |= 0x4000;
#endif
			cdbp = t2->req_cdb;
		} else {
			t2->req_target = lp->handle;
			t2->req_lun_trn = XS_LUN(xs);
			cdbp = t2->req_cdb;
		}
	}
	ISP_MEMCPY(cdbp, XS_CDBP(xs), cdblen);

	*tptr = XS_TIME(xs) / 1000;
	if (*tptr == 0 && XS_TIME(xs)) {
		*tptr = 1;
	}
	if (IS_24XX(isp) && *tptr > 0x1999) {
		*tptr = 0x1999;
	}

	if (isp_allocate_xs(isp, xs, &handle)) {
		isp_prt(isp, ISP_LOG_WARN1, "out of xflist pointers");
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	}
	/* Whew. Thankfully the same for type 7 requests */
	reqp->req_handle = handle;

	/*
	 * Set up DMA and/or do any platform dependent swizzling of the request entry
	 * so that the Qlogic F/W understands what is being asked of it.
	 *
	 * The callee is responsible for adding all requests at this point.
	 */
	dmaresult = ISP_DMASETUP(isp, xs, reqp);
	if (dmaresult != CMD_QUEUED) {
		isp_destroy_handle(isp, handle);
		/*
		 * dmasetup sets actual error in packet, and
		 * return what we were given to return.
		 */
		return (dmaresult);
	}
	isp_xs_prt(isp, xs, ISP_LOGDEBUG0, "START cmd cdb[0]=0x%x datalen %ld", XS_CDBP(xs)[0], (long) XS_XFRLEN(xs));
	isp->isp_nactive++;
	return (CMD_QUEUED);
}

/*
 * isp control
 * Locks (ints blocked) assumed held.
 */

int
isp_control(ispsoftc_t *isp, ispctl_t ctl, ...)
{
	XS_T *xs;
	mbreg_t *mbr, mbs;
	int chan, tgt;
	uint32_t handle;
	va_list ap;

	switch (ctl) {
	case ISPCTL_RESET_BUS:
		/*
		 * Issue a bus reset.
		 */
		if (IS_24XX(isp)) {
			isp_prt(isp, ISP_LOGERR, "BUS RESET NOT IMPLEMENTED");
			break;
		} else if (IS_FC(isp)) {
			mbs.param[1] = 10;
			chan = 0;
		} else {
			va_start(ap, ctl);
			chan = va_arg(ap, int);
			va_end(ap);
			mbs.param[1] = SDPARAM(isp, chan)->isp_bus_reset_delay;
			if (mbs.param[1] < 2) {
				mbs.param[1] = 2;
			}
			mbs.param[2] = chan;
		}
		MBSINIT(&mbs, MBOX_BUS_RESET, MBLOGALL, 0);
		ISP_SET_SENDMARKER(isp, chan, 1);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			break;
		}
		isp_prt(isp, ISP_LOGINFO, "driver initiated bus reset of bus %d", chan);
		return (0);

	case ISPCTL_RESET_DEV:
		va_start(ap, ctl);
		chan = va_arg(ap, int);
		tgt = va_arg(ap, int);
		va_end(ap);
		if (IS_24XX(isp)) {
			uint8_t local[QENTRY_LEN];
			isp24xx_tmf_t *tmf;
			isp24xx_statusreq_t *sp;
			fcparam *fcp = FCPARAM(isp, chan);
			fcportdb_t *lp;

			if (tgt < 0 || tgt >= MAX_FC_TARG) {
				isp_prt(isp, ISP_LOGWARN, "Chan %d trying to reset bad target %d", chan, tgt);
				break;
			}
			lp = &fcp->portdb[tgt];
			if (lp->is_target == 0 ||
			    lp->state != FC_PORTDB_STATE_VALID) {
				isp_prt(isp, ISP_LOGWARN, "Chan %d abort of no longer valid target %d", chan, tgt);
				break;
			}

			tmf = (isp24xx_tmf_t *) local;
			ISP_MEMZERO(tmf, QENTRY_LEN);
			tmf->tmf_header.rqs_entry_type = RQSTYPE_TSK_MGMT;
			tmf->tmf_header.rqs_entry_count = 1;
			tmf->tmf_nphdl = lp->handle;
			tmf->tmf_delay = 2;
			tmf->tmf_timeout = 2;
			tmf->tmf_flags = ISP24XX_TMF_TARGET_RESET;
			tmf->tmf_tidlo = lp->portid;
			tmf->tmf_tidhi = lp->portid >> 16;
			tmf->tmf_vpidx = ISP_GET_VPIDX(isp, chan);
			isp_prt(isp, ISP_LOGALL, "Chan %d Reset N-Port Handle 0x%04x @ Port 0x%06x", chan, lp->handle, lp->portid);
			MBSINIT(&mbs, MBOX_EXEC_COMMAND_IOCB_A64, MBLOGALL, 5000000);
			mbs.param[1] = QENTRY_LEN;
			mbs.param[2] = DMA_WD1(fcp->isp_scdma);
			mbs.param[3] = DMA_WD0(fcp->isp_scdma);
			mbs.param[6] = DMA_WD3(fcp->isp_scdma);
			mbs.param[7] = DMA_WD2(fcp->isp_scdma);

			if (FC_SCRATCH_ACQUIRE(isp, chan)) {
				isp_prt(isp, ISP_LOGERR, sacq);
				break;
			}
			isp_put_24xx_tmf(isp, tmf, fcp->isp_scratch);
			MEMORYBARRIER(isp, SYNC_SFORDEV, 0, QENTRY_LEN, chan);
			fcp->sendmarker = 1;
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				FC_SCRATCH_RELEASE(isp, chan);
				break;
			}
			MEMORYBARRIER(isp, SYNC_SFORCPU, QENTRY_LEN, QENTRY_LEN, chan);
			sp = (isp24xx_statusreq_t *) local;
			isp_get_24xx_response(isp, &((isp24xx_statusreq_t *)fcp->isp_scratch)[1], sp);
			FC_SCRATCH_RELEASE(isp, chan);
			if (sp->req_completion_status == 0) {
				return (0);
			}
			isp_prt(isp, ISP_LOGWARN, "Chan %d reset of target %d returned 0x%x", chan, tgt, sp->req_completion_status);
			break;
		} else if (IS_FC(isp)) {
			if (ISP_CAP_2KLOGIN(isp)) {
				mbs.param[1] = tgt;
				mbs.ibits = (1 << 10);
			} else {
				mbs.param[1] = (tgt << 8);
			}
		} else {
			mbs.param[1] = (chan << 15) | (tgt << 8);
		}
		MBSINIT(&mbs, MBOX_ABORT_TARGET, MBLOGALL, 0);
		mbs.param[2] = 3;	/* 'delay', in seconds */
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			break;
		}
		isp_prt(isp, ISP_LOGINFO, "Target %d on Bus %d Reset Succeeded", tgt, chan);
		ISP_SET_SENDMARKER(isp, chan, 1);
		return (0);

	case ISPCTL_ABORT_CMD:
		va_start(ap, ctl);
		xs = va_arg(ap, XS_T *);
		va_end(ap);

		tgt = XS_TGT(xs);
		chan = XS_CHANNEL(xs);

		handle = isp_find_handle(isp, xs);
		if (handle == 0) {
			isp_prt(isp, ISP_LOGWARN, "cannot find handle for command to abort");
			break;
		}
		if (IS_24XX(isp)) {
			isp24xx_abrt_t local, *ab = &local, *ab2;
			fcparam *fcp;
			fcportdb_t *lp;

			fcp = FCPARAM(isp, chan);
			if (tgt < 0 || tgt >= MAX_FC_TARG) {
				isp_prt(isp, ISP_LOGWARN, "Chan %d trying to abort bad target %d", chan, tgt);
				break;
			}
			lp = &fcp->portdb[tgt];
			if (lp->is_target == 0 ||
			    lp->state != FC_PORTDB_STATE_VALID) {
				isp_prt(isp, ISP_LOGWARN, "Chan %d abort of no longer valid target %d", chan, tgt);
				break;
			}
			isp_prt(isp, ISP_LOGALL, "Chan %d Abort Cmd for N-Port 0x%04x @ Port 0x%06x", chan, lp->handle, lp->portid);
			ISP_MEMZERO(ab, QENTRY_LEN);
			ab->abrt_header.rqs_entry_type = RQSTYPE_ABORT_IO;
			ab->abrt_header.rqs_entry_count = 1;
			ab->abrt_handle = lp->handle;
			ab->abrt_cmd_handle = handle;
			ab->abrt_tidlo = lp->portid;
			ab->abrt_tidhi = lp->portid >> 16;
			ab->abrt_vpidx = ISP_GET_VPIDX(isp, chan);

			ISP_MEMZERO(&mbs, sizeof (mbs));
			MBSINIT(&mbs, MBOX_EXEC_COMMAND_IOCB_A64, MBLOGALL, 5000000);
			mbs.param[1] = QENTRY_LEN;
			mbs.param[2] = DMA_WD1(fcp->isp_scdma);
			mbs.param[3] = DMA_WD0(fcp->isp_scdma);
			mbs.param[6] = DMA_WD3(fcp->isp_scdma);
			mbs.param[7] = DMA_WD2(fcp->isp_scdma);

			if (FC_SCRATCH_ACQUIRE(isp, chan)) {
				isp_prt(isp, ISP_LOGERR, sacq);
				break;
			}
			isp_put_24xx_abrt(isp, ab, fcp->isp_scratch);
			ab2 = (isp24xx_abrt_t *) &((uint8_t *)fcp->isp_scratch)[QENTRY_LEN];
			ab2->abrt_nphdl = 0xdeaf;
			MEMORYBARRIER(isp, SYNC_SFORDEV, 0, 2 * QENTRY_LEN, chan);
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
				FC_SCRATCH_RELEASE(isp, chan);
				break;
			}
			MEMORYBARRIER(isp, SYNC_SFORCPU, QENTRY_LEN, QENTRY_LEN, chan);
			isp_get_24xx_abrt(isp, ab2, ab);
			FC_SCRATCH_RELEASE(isp, chan);
			if (ab->abrt_nphdl == ISP24XX_ABRT_OKAY) {
				return (0);
			}
			isp_prt(isp, ISP_LOGWARN, "Chan %d handle %d abort returned 0x%x", chan, tgt, ab->abrt_nphdl);
			break;
		} else if (IS_FC(isp)) {
			if (ISP_CAP_SCCFW(isp)) {
				if (ISP_CAP_2KLOGIN(isp)) {
					mbs.param[1] = tgt;
				} else {
					mbs.param[1] = tgt << 8;
				}
				mbs.param[6] = XS_LUN(xs);
			} else {
				mbs.param[1] = tgt << 8 | XS_LUN(xs);
			}
		} else {
			mbs.param[1] = (chan << 15) | (tgt << 8) | XS_LUN(xs);
		}
		MBSINIT(&mbs, MBOX_ABORT, MBLOGALL & ~MBOX_COMMAND_ERROR, 0);
		mbs.param[2] = handle;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			break;
		}
		return (0);

	case ISPCTL_UPDATE_PARAMS:

		va_start(ap, ctl);
		chan = va_arg(ap, int);
		va_end(ap);
		isp_spi_update(isp, chan);
		return (0);

	case ISPCTL_FCLINK_TEST:

		if (IS_FC(isp)) {
			int usdelay;
			va_start(ap, ctl);
			chan = va_arg(ap, int);
			usdelay = va_arg(ap, int);
			va_end(ap);
			if (usdelay == 0) {
				usdelay =  250000;
			}
			return (isp_fclink_test(isp, chan, usdelay));
		}
		break;

	case ISPCTL_SCAN_FABRIC:

		if (IS_FC(isp)) {
			va_start(ap, ctl);
			chan = va_arg(ap, int);
			va_end(ap);
			return (isp_scan_fabric(isp, chan));
		}
		break;

	case ISPCTL_SCAN_LOOP:

		if (IS_FC(isp)) {
			va_start(ap, ctl);
			chan = va_arg(ap, int);
			va_end(ap);
			return (isp_scan_loop(isp, chan));
		}
		break;

	case ISPCTL_PDB_SYNC:

		if (IS_FC(isp)) {
			va_start(ap, ctl);
			chan = va_arg(ap, int);
			va_end(ap);
			return (isp_pdb_sync(isp, chan));
		}
		break;

	case ISPCTL_SEND_LIP:

		if (IS_FC(isp) && !IS_24XX(isp)) {
			MBSINIT(&mbs, MBOX_INIT_LIP, MBLOGALL, 0);
			if (ISP_CAP_2KLOGIN(isp)) {
				mbs.ibits = (1 << 10);
			}
			isp_mboxcmd(isp, &mbs);
			if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
				return (0);
			}
		}
		break;

	case ISPCTL_GET_PDB:
		if (IS_FC(isp)) {
			isp_pdb_t *pdb;
			va_start(ap, ctl);
			chan = va_arg(ap, int);
			tgt = va_arg(ap, int);
			pdb = va_arg(ap, isp_pdb_t *);
			va_end(ap);
			return (isp_getpdb(isp, chan, tgt, pdb, 1));
		}
		break;

	case ISPCTL_GET_NAMES:
	{
		uint64_t *wwnn, *wwnp;
		va_start(ap, ctl);
		chan = va_arg(ap, int);
		tgt = va_arg(ap, int);
		wwnn = va_arg(ap, uint64_t *);
		wwnp = va_arg(ap, uint64_t *);
		va_end(ap);
		if (wwnn == NULL && wwnp == NULL) {
			break;
		}
		if (wwnn) {
			*wwnn = isp_get_wwn(isp, chan, tgt, 1);
			if (*wwnn == INI_NONE) {
				break;
			}
		}
		if (wwnp) {
			*wwnp = isp_get_wwn(isp, chan, tgt, 0);
			if (*wwnp == INI_NONE) {
				break;
			}
		}
		return (0);
	}
	case ISPCTL_RUN_MBOXCMD:
	{
		va_start(ap, ctl);
		mbr = va_arg(ap, mbreg_t *);
		va_end(ap);
		isp_mboxcmd(isp, mbr);
		return (0);
	}
	case ISPCTL_PLOGX:
	{
		isp_plcmd_t *p;
		int r;

		va_start(ap, ctl);
		p = va_arg(ap, isp_plcmd_t *);
		va_end(ap);

		if ((p->flags & PLOGX_FLG_CMD_MASK) != PLOGX_FLG_CMD_PLOGI || (p->handle != NIL_HANDLE)) {
			return (isp_plogx(isp, p->channel, p->handle, p->portid, p->flags, 0));
		}
		do {
			p->handle = isp_nxt_handle(isp, p->channel, p->handle);
			r = isp_plogx(isp, p->channel, p->handle, p->portid, p->flags, 0);
			if ((r & 0xffff) == MBOX_PORT_ID_USED) {
				p->handle = r >> 16;
				r = 0;
				break;
			}
		} while ((r & 0xffff) == MBOX_LOOP_ID_USED);
		return (r);
	}
	case ISPCTL_CHANGE_ROLE:
	{
		int role, r;

		va_start(ap, ctl);
		chan = va_arg(ap, int);
		role = va_arg(ap, int);
		va_end(ap);
		if (IS_FC(isp)) {
			r = isp_fc_change_role(isp, chan, role);
		} else {
			SDPARAM(isp, chan)->role = role;
			r = 0;
		}
		return (r);
	}
	default:
		isp_prt(isp, ISP_LOGERR, "Unknown Control Opcode 0x%x", ctl);
		break;

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
isp_intr(ispsoftc_t *isp, uint16_t isr, uint16_t sema, uint16_t info)
{
	XS_T *complist[MAX_REQUESTQ_COMPLETIONS], *xs;
	uint32_t iptr, optr, junk;
	int i, nlooked = 0, ndone = 0, continuations_expected = 0;
	int etype, last_etype = 0;

again:
	/*
	 * Is this a mailbox related interrupt?
	 * The mailbox semaphore will be nonzero if so.
	 */
	if (sema) {
 fmbox:
		if (info & MBOX_COMMAND_COMPLETE) {
			isp->isp_intmboxc++;
			if (isp->isp_mboxbsy) {
				int obits = isp->isp_obits;
				isp->isp_mboxtmp[0] = info;
				for (i = 1; i < ISP_NMBOX(isp); i++) {
					if ((obits & (1 << i)) == 0) {
						continue;
					}
					isp->isp_mboxtmp[i] = ISP_READ(isp, MBOX_OFF(i));
				}
				if (isp->isp_mbxwrk0) {
					if (isp_mbox_continue(isp) == 0) {
						return;
					}
				}
				MBOX_NOTIFY_COMPLETE(isp);
			} else {
				isp_prt(isp, ISP_LOGWARN, "mailbox cmd (0x%x) with no waiters", info);
			}
		} else {
			i = IS_FC(isp)? isp_parse_async_fc(isp, info) : isp_parse_async(isp, info);
			if (i < 0) {
				return;
			}
		}
		if ((IS_FC(isp) && info != ASYNC_RIOZIO_STALL) || isp->isp_state != ISP_RUNSTATE) {
			goto out;
		}
	}

	/*
	 * We can't be getting this now.
	 */
	if (isp->isp_state != ISP_RUNSTATE) {
		/*
		 * This seems to happen to 23XX and 24XX cards- don't know why.
		 */
		 if (isp->isp_mboxbsy && isp->isp_lastmbxcmd == MBOX_ABOUT_FIRMWARE) {
			goto fmbox;
		}
		isp_prt(isp, ISP_LOGINFO, "interrupt (ISR=%x SEMA=%x INFO=%x) "
		    "when not ready", isr, sema, info);
		/*
		 * Thank you very much!  *Burrrp*!
		 */
		isp->isp_residx = ISP_READ(isp, isp->isp_respinrp);
		isp->isp_resodx = isp->isp_residx;
		ISP_WRITE(isp, isp->isp_respoutrp, isp->isp_resodx);
		if (IS_24XX(isp)) {
			ISP_DISABLE_INTS(isp);
		}
		goto out;
	}

#ifdef	ISP_TARGET_MODE
	/*
	 * Check for ATIO Queue entries.
	 */
	if (IS_24XX(isp) &&
	    (isr == ISPR2HST_ATIO_UPDATE || isr == ISPR2HST_ATIO_RSPQ_UPDATE ||
	     isr == ISPR2HST_ATIO_UPDATE2)) {
		iptr = ISP_READ(isp, BIU2400_ATIO_RSPINP);
		optr = isp->isp_atioodx;

		while (optr != iptr) {
			uint8_t qe[QENTRY_LEN];
			isphdr_t *hp;
			uint32_t oop;
			void *addr;

			oop = optr;
			MEMORYBARRIER(isp, SYNC_ATIOQ, oop, QENTRY_LEN, -1);
			addr = ISP_QUEUE_ENTRY(isp->isp_atioq, oop);
			isp_get_hdr(isp, addr, (isphdr_t *)qe);
			hp = (isphdr_t *)qe;
			switch (hp->rqs_entry_type) {
			case RQSTYPE_NOTIFY:
			case RQSTYPE_ATIO:
				(void) isp_target_notify(isp, addr, &oop);
				break;
			default:
				isp_print_qentry(isp, "?ATIOQ entry?", oop, addr);
				break;
			}
			optr = ISP_NXT_QENTRY(oop, RESULT_QUEUE_LEN(isp));
		}
		if (isp->isp_atioodx != optr) {
			ISP_WRITE(isp, BIU2400_ATIO_RSPOUTP, optr);
			isp->isp_atioodx = optr;
		}
	}
#endif

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
			isp_prt(isp, ISP_LOGWARN, "Response Queue Out Pointer Unstable (%x, %x)", iptr, junk);
			goto out;
		}
	} else {
		iptr = ISP_READ(isp, isp->isp_respinrp);
	}

	optr = isp->isp_resodx;
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
			ISP_DELAY(100);
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
				info = ISP_READ(isp, OUTMAILBOX0);
				if ((sema & 0x3) && (info & 0x8000)) {
					goto again;
				}
			}
			isp->isp_intbogus++;
			isp_prt(isp, ISP_LOGDEBUG1, "bogus intr- isr %x (%x) iptr %x optr %x", isr, junk, iptr, optr);
		}
	}
	isp->isp_residx = iptr;

	while (optr != iptr) {
		uint8_t qe[QENTRY_LEN];
		ispstatusreq_t *sp = (ispstatusreq_t *) qe;
		isphdr_t *hp;
		int buddaboom, scsi_status, completion_status;
		int req_status_flags, req_state_flags;
		uint8_t *snsp, *resp;
		uint32_t rlen, slen, totslen;
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
		MEMORYBARRIER(isp, SYNC_RESULT, oop, QENTRY_LEN, -1);
		isp_get_hdr(isp, hp, &sp->req_header);
		etype = sp->req_header.rqs_entry_type;

		if (IS_24XX(isp) && etype == RQSTYPE_RESPONSE) {
			isp24xx_statusreq_t *sp2 = (isp24xx_statusreq_t *)qe;
			isp_get_24xx_response(isp, (isp24xx_statusreq_t *)hp, sp2);
			if (isp->isp_dblev & ISP_LOGDEBUG1) {
				isp_print_bytes(isp, "Response Queue Entry", QENTRY_LEN, sp2);
			}
			scsi_status = sp2->req_scsi_status;
			completion_status = sp2->req_completion_status;
			if ((scsi_status & 0xff) != 0)
				req_state_flags = RQSF_GOT_STATUS;
			else
				req_state_flags = 0;
			resid = sp2->req_resid;
		} else if (etype == RQSTYPE_RESPONSE) {
			isp_get_response(isp, (ispstatusreq_t *) hp, sp);
			if (isp->isp_dblev & ISP_LOGDEBUG1) {
				isp_print_bytes(isp, "Response Queue Entry", QENTRY_LEN, sp);
			}
			scsi_status = sp->req_scsi_status;
			completion_status = sp->req_completion_status;
			req_status_flags = sp->req_status_flags;
			req_state_flags = sp->req_state_flags;
			resid = sp->req_resid;
		} else if (etype == RQSTYPE_RIO1) {
			isp_rio1_t *rio = (isp_rio1_t *) qe;
			isp_get_rio1(isp, (isp_rio1_t *) hp, rio);
			if (isp->isp_dblev & ISP_LOGDEBUG1) {
				isp_print_bytes(isp, "Response Queue Entry", QENTRY_LEN, rio);
			}
			for (i = 0; i < rio->req_header.rqs_seqno; i++) {
				isp_fastpost_complete(isp, rio->req_handles[i]);
			}
			if (isp->isp_fpcchiwater < rio->req_header.rqs_seqno) {
				isp->isp_fpcchiwater = rio->req_header.rqs_seqno;
			}
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			last_etype = etype;
			continue;
		} else if (etype == RQSTYPE_RIO2) {
			isp_prt(isp, ISP_LOGERR, "dropping RIO2 response");
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			last_etype = etype;
			continue;
		} else if (etype == RQSTYPE_STATUS_CONT) {
			isp_get_cont_response(isp, (ispstatus_cont_t *) hp, (ispstatus_cont_t *) sp);
			if (last_etype == RQSTYPE_RESPONSE && continuations_expected && ndone > 0 && (xs = complist[ndone-1]) != NULL) {
				ispstatus_cont_t *scp = (ispstatus_cont_t *) sp;
				XS_SENSE_APPEND(xs, scp->req_sense_data, sizeof (scp->req_sense_data));
				isp_prt(isp, ISP_LOGDEBUG0|ISP_LOG_CWARN, "%d more Status Continuations expected", --continuations_expected);
			} else {
				isp_prt(isp, ISP_LOG_WARN1, "Ignored Continuation Response");
			}
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			continue;
		} else {
			/*
			 * Somebody reachable via isp_handle_other_response
			 * may have updated the response queue pointers for
			 * us, so we reload our goal index.
			 */
			int r;
			uint32_t tsto = oop;
			r = isp_handle_other_response(isp, etype, hp, &tsto);
			if (r < 0) {
				goto read_again;
			}
			/*
			 * If somebody updated the output pointer, then reset
			 * optr to be one more than the updated amount.
			 */
			while (tsto != oop) {
				optr = ISP_NXT_QENTRY(tsto, RESULT_QUEUE_LEN(isp));
			}
			if (r > 0) {
				ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
				last_etype = etype;
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
				isp_prt(isp, ISP_LOGERR, notresp, etype, oop, optr, nlooked);
				isp_print_bytes(isp, "Request Queue Entry", QENTRY_LEN, sp);
				ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
				last_etype = etype;
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
				isp_print_bytes(isp, "unexpected continuation segment", QENTRY_LEN, sp);
				last_etype = etype;
				continue;
			}
			if (sp->req_header.rqs_flags & RQSFLAG_FULL) {
				isp_prt(isp, ISP_LOG_WARN1, "internal queues full");
				/*
				 * We'll synthesize a QUEUE FULL message below.
				 */
			}
			if (sp->req_header.rqs_flags & RQSFLAG_BADHEADER) {
				isp_print_bytes(isp, "bad header flag", QENTRY_LEN, sp);
				buddaboom++;
			}
			if (sp->req_header.rqs_flags & RQSFLAG_BADPACKET) {
				isp_print_bytes(isp, "bad request packet", QENTRY_LEN, sp);
				buddaboom++;
			}
			if (sp->req_header.rqs_flags & RQSFLAG_BADCOUNT) {
				isp_print_bytes(isp, "invalid entry count", QENTRY_LEN, sp);
				buddaboom++;
			}
			if (sp->req_header.rqs_flags & RQSFLAG_BADORDER) {
				isp_print_bytes(isp, "invalid IOCB ordering", QENTRY_LEN, sp);
				last_etype = etype;
				continue;
			}
		}

		if (!ISP_VALID_HANDLE(isp, sp->req_handle)) {
			isp_prt(isp, ISP_LOGERR, "bad request handle 0x%x (iocb type 0x%x)", sp->req_handle, etype);
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			last_etype = etype;
			continue;
		}
		xs = isp_find_xs(isp, sp->req_handle);
		if (xs == NULL) {
			uint8_t ts = completion_status & 0xff;
			/*
			 * Only whine if this isn't the expected fallout of
			 * aborting the command or resetting the target.
			 */
			if (etype != RQSTYPE_RESPONSE) {
				isp_prt(isp, ISP_LOGERR, "cannot find handle 0x%x (type 0x%x)", sp->req_handle, etype);
			} else if (ts != RQCS_ABORTED && ts != RQCS_RESET_OCCURRED) {
				isp_prt(isp, ISP_LOGERR, "cannot find handle 0x%x (status 0x%x)", sp->req_handle, ts);
			}
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			last_etype = etype;
			continue;
		}
		if (req_status_flags & RQSTF_BUS_RESET) {
			isp_prt(isp, ISP_LOG_WARN1, "%d.%d.%jx bus was reset",
			    XS_CHANNEL(xs), XS_TGT(xs), (uintmax_t)XS_LUN(xs));
			XS_SETERR(xs, HBA_BUSRESET);
			ISP_SET_SENDMARKER(isp, XS_CHANNEL(xs), 1);
		}
		if (buddaboom) {
			isp_prt(isp, ISP_LOG_WARN1, "%d.%d.%jx buddaboom",
			    XS_CHANNEL(xs), XS_TGT(xs), (uintmax_t)XS_LUN(xs));
			XS_SETERR(xs, HBA_BOTCH);
		}

		resp = NULL;
		rlen = 0;
		snsp = NULL;
		totslen = slen = 0;
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
				snsp = ((isp24xx_statusreq_t *)sp)->req_rsp_sense;
				snsp += rlen;
				totslen = ((isp24xx_statusreq_t *)sp)->req_sense_len;
				slen = (sizeof (((isp24xx_statusreq_t *)sp)->req_rsp_sense)) - rlen;
				if (totslen < slen)
					slen = totslen; 
			} else {
				snsp = sp->req_sense_data;
				totslen = sp->req_sense_len;
				slen = sizeof (sp->req_sense_data);
				if (totslen < slen)
					slen = totslen;
			}
		} else if (IS_SCSI(isp) && (req_state_flags & RQSF_GOT_SENSE)) {
			snsp = sp->req_sense_data;
			totslen = sp->req_sense_len;
			slen = sizeof (sp->req_sense_data);
			if (totslen < slen)
				slen = totslen;
		}
		if (req_state_flags & RQSF_GOT_STATUS) {
			*XS_STSP(xs) = scsi_status & 0xff;
		}

		switch (etype) {
		case RQSTYPE_RESPONSE:
			if (resp && rlen >= 4 && resp[FCP_RSPNS_CODE_OFFSET] != 0) {
				const char *ptr;
				char lb[64];
				const char *rnames[10] = {
				    "Task Management function complete",
				    "FCP_DATA length different than FCP_BURST_LEN",
				    "FCP_CMND fields invalid",
				    "FCP_DATA parameter mismatch with FCP_DATA_RO",
				    "Task Management function rejected",
				    "Task Management function failed",
				    NULL,
				    NULL,
				    "Task Management function succeeded",
				    "Task Management function incorrect logical unit number",
				};
				uint8_t code = resp[FCP_RSPNS_CODE_OFFSET];
				if (code >= 10 || rnames[code] == NULL) {
					ISP_SNPRINTF(lb, sizeof(lb),
					    "Unknown FCP Response Code 0x%x",
					    code);
					ptr = lb;
				} else {
					ptr = rnames[code];
				}
				isp_xs_prt(isp, xs, ISP_LOGWARN,
				    "FCP RESPONSE, LENGTH %u: %s CDB0=0x%02x",
				    rlen, ptr, XS_CDBP(xs)[0] & 0xff);
				if (code != 0 && code != 8)
					XS_SETERR(xs, HBA_BOTCH);
			}
			if (IS_24XX(isp)) {
				isp_parse_status_24xx(isp, (isp24xx_statusreq_t *)sp, xs, &resid);
			} else {
				isp_parse_status(isp, (void *)sp, xs, &resid);
			}
			if ((XS_NOERR(xs) || XS_ERR(xs) == HBA_NOERROR) && (*XS_STSP(xs) == SCSI_BUSY)) {
				XS_SETERR(xs, HBA_TGTBSY);
			}
			if (IS_SCSI(isp)) {
				XS_SET_RESID(xs, resid);
				/*
				 * A new synchronous rate was negotiated for
				 * this target. Mark state such that we'll go
				 * look up that which has changed later.
				 */
				if (req_status_flags & RQSTF_NEGOTIATION) {
					int t = XS_TGT(xs);
					sdparam *sdp = SDPARAM(isp, XS_CHANNEL(xs));
					sdp->isp_devparam[t].dev_refresh = 1;
					sdp->update = 1;
				}
			} else {
				if (req_status_flags & RQSF_XFER_COMPLETE) {
					XS_SET_RESID(xs, 0);
				} else if (scsi_status & RQCS_RESID) {
					XS_SET_RESID(xs, resid);
				} else {
					XS_SET_RESID(xs, 0);
				}
			}
			if (snsp && slen) {
				if (totslen > slen) {
					continuations_expected += ((totslen - slen + QENTRY_LEN - 5) / (QENTRY_LEN - 4));
					if (ndone > (MAX_REQUESTQ_COMPLETIONS - continuations_expected - 1)) {
						/* we'll lose some stats, but that's a small price to pay */
						for (i = 0; i < ndone; i++) {
							if (complist[i]) {
								isp->isp_rsltccmplt++;
								isp_done(complist[i]);
							}
						}
						ndone = 0;
					}
					isp_prt(isp, ISP_LOGDEBUG0|ISP_LOG_CWARN, "Expecting %d more Status Continuations for total sense length of %u",
					    continuations_expected, totslen);
				}
				XS_SAVE_SENSE(xs, snsp, totslen, slen);
			} else if ((req_status_flags & RQSF_GOT_STATUS) && (scsi_status & 0xff) == SCSI_CHECK && IS_FC(isp)) {
				isp_prt(isp, ISP_LOGWARN, "CHECK CONDITION w/o sense data for CDB=0x%x", XS_CDBP(xs)[0] & 0xff);
				isp_print_bytes(isp, "CC with no Sense", QENTRY_LEN, qe);
			}
			isp_prt(isp, ISP_LOGDEBUG2, "asked for %ld got raw resid %ld settled for %ld", (long) XS_XFRLEN(xs), resid, (long) XS_GET_RESID(xs));
			break;
		case RQSTYPE_REQUEST:
		case RQSTYPE_A64:
		case RQSTYPE_T2RQS:
		case RQSTYPE_T3RQS:
		case RQSTYPE_T7RQS:
			if (!IS_24XX(isp) && (sp->req_header.rqs_flags & RQSFLAG_FULL)) {
				/*
				 * Force Queue Full status.
				 */
				*XS_STSP(xs) = SCSI_QFULL;
				XS_SETERR(xs, HBA_NOERROR);
			} else if (XS_NOERR(xs)) {
				isp_prt(isp, ISP_LOG_WARN1,
				    "%d.%d.%jx badness at %s:%u",
				    XS_CHANNEL(xs), XS_TGT(xs),
				    (uintmax_t)XS_LUN(xs),
				    __func__, __LINE__);
				XS_SETERR(xs, HBA_BOTCH);
			}
			XS_SET_RESID(xs, XS_XFRLEN(xs));
			break;
		default:
			isp_print_bytes(isp, "Unhandled Response Type", QENTRY_LEN, qe);
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
		isp_destroy_handle(isp, sp->req_handle);

		if (isp->isp_nactive > 0) {
		    isp->isp_nactive--;
		}
		complist[ndone++] = xs;	/* defer completion call until later */
		ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
		last_etype = etype;
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
		isp->isp_resodx = optr;
		if (isp->isp_rscchiwater < ndone)
			isp->isp_rscchiwater = ndone;
	}

out:

	if (IS_24XX(isp)) {
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_RISC_INT);
	} else {
		ISP_WRITE(isp, HCCR, HCCR_CMD_CLEAR_RISC_INT);
		ISP_WRITE(isp, BIU_SEMA, 0);
	}

	for (i = 0; i < ndone; i++) {
		xs = complist[i];
		if (xs) {
			if (((isp->isp_dblev & (ISP_LOGDEBUG1|ISP_LOGDEBUG2|ISP_LOGDEBUG3))) ||
			    ((isp->isp_dblev & (ISP_LOGDEBUG0|ISP_LOG_CWARN) && ((!XS_NOERR(xs)) || (*XS_STSP(xs) != SCSI_GOOD))))) {
				isp_prt_endcmd(isp, xs);
			}
			isp->isp_rsltccmplt++;
			isp_done(xs);
		}
	}
}

/*
 * Support routines.
 */

void
isp_prt_endcmd(ispsoftc_t *isp, XS_T *xs)
{
	char cdbstr[16 * 5 + 1];
	int i, lim;

	lim = XS_CDBLEN(xs) > 16? 16 : XS_CDBLEN(xs);
	ISP_SNPRINTF(cdbstr, sizeof (cdbstr), "0x%02x ", XS_CDBP(xs)[0]);
	for (i = 1; i < lim; i++) {
		ISP_SNPRINTF(cdbstr, sizeof (cdbstr), "%s0x%02x ", cdbstr, XS_CDBP(xs)[i]);
	}
	if (XS_SENSE_VALID(xs)) {
		isp_xs_prt(isp, xs, ISP_LOGALL, "FIN dl%d resid %ld CDB=%s SenseLength=%u/%u KEY/ASC/ASCQ=0x%02x/0x%02x/0x%02x",
		    XS_XFRLEN(xs), (long) XS_GET_RESID(xs), cdbstr, XS_CUR_SNSLEN(xs), XS_TOT_SNSLEN(xs), XS_SNSKEY(xs), XS_SNSASC(xs), XS_SNSASCQ(xs));
	} else {
		isp_xs_prt(isp, xs, ISP_LOGALL, "FIN dl%d resid %ld CDB=%s STS 0x%x XS_ERR=0x%x", XS_XFRLEN(xs), (long) XS_GET_RESID(xs), cdbstr, *XS_STSP(xs), XS_ERR(xs));
	}
}

/*
 * Parse an ASYNC mailbox complete
 *
 * Return non-zero if the event has been acknowledged.
 */
static int
isp_parse_async(ispsoftc_t *isp, uint16_t mbox)
{
	int acked = 0;
	uint32_t h1 = 0, h2 = 0;
	uint16_t chan = 0;

	/*
	 * Pick up the channel, but not if this is a ASYNC_RIO32_2,
	 * where Mailboxes 6/7 have the second handle.
	 */
	if (mbox != ASYNC_RIO32_2) {
		if (IS_DUALBUS(isp)) {
			chan = ISP_READ(isp, OUTMAILBOX6);
		}
	}
	isp_prt(isp, ISP_LOGDEBUG2, "Async Mbox 0x%x", mbox);

	switch (mbox) {
	case ASYNC_BUS_RESET:
		ISP_SET_SENDMARKER(isp, chan, 1);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, chan, mbox)) {
			acked = 1;
		}
#endif
		isp_async(isp, ISPASYNC_BUS_RESET, chan);
		break;
	case ASYNC_SYSTEM_ERROR:
		isp->isp_dead = 1;
		isp->isp_state = ISP_CRASHED;
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
		isp_async(isp, ISPASYNC_FW_CRASH);
		acked = 1;
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
		isp_prt(isp, ISP_LOGWARN, "timeout initiated SCSI bus reset of chan %d", chan);
		ISP_SET_SENDMARKER(isp, chan, 1);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, chan, mbox)) {
			acked = 1;
		}
#endif
		break;

	case ASYNC_DEVICE_RESET:
		isp_prt(isp, ISP_LOGINFO, "device reset on chan %d", chan);
		ISP_SET_SENDMARKER(isp, chan, 1);
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, chan, mbox)) {
			acked = 1;
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
		isp_prt(isp, ISP_LOGERR, "stalled SCSI Bus after DATA Overrun");
		/* XXX: Need to issue SCSI reset at this point */
		break;

	case ASYNC_KILLED_BUS:
		isp_prt(isp, ISP_LOGERR, "SCSI Bus reset after DATA Overrun");
		break;

	case ASYNC_BUS_TRANSIT:
		mbox = ISP_READ(isp, OUTMAILBOX2);
		switch (mbox & SXP_PINS_MODE_MASK) {
		case SXP_PINS_LVD_MODE:
			isp_prt(isp, ISP_LOGINFO, "Transition to LVD mode");
			SDPARAM(isp, chan)->isp_diffmode = 0;
			SDPARAM(isp, chan)->isp_ultramode = 0;
			SDPARAM(isp, chan)->isp_lvdmode = 1;
			break;
		case SXP_PINS_HVD_MODE:
			isp_prt(isp, ISP_LOGINFO,
			    "Transition to Differential mode");
			SDPARAM(isp, chan)->isp_diffmode = 1;
			SDPARAM(isp, chan)->isp_ultramode = 0;
			SDPARAM(isp, chan)->isp_lvdmode = 0;
			break;
		case SXP_PINS_SE_MODE:
			isp_prt(isp, ISP_LOGINFO,
			    "Transition to Single Ended mode");
			SDPARAM(isp, chan)->isp_diffmode = 0;
			SDPARAM(isp, chan)->isp_ultramode = 1;
			SDPARAM(isp, chan)->isp_lvdmode = 0;
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
		ISP_SET_SENDMARKER(isp, chan, 1);
		break;

	case ASYNC_CMD_CMPLT:
	case ASYNC_RIO32_1:
		if (!IS_ULTRA3(isp)) {
			isp_prt(isp, ISP_LOGERR, "unexpected fast posting completion");
			break;
		}
		/* FALLTHROUGH */
		h1 = (ISP_READ(isp, OUTMAILBOX2) << 16) | ISP_READ(isp, OUTMAILBOX1);
		break;

	case ASYNC_RIO32_2:
		h1 = (ISP_READ(isp, OUTMAILBOX2) << 16) | ISP_READ(isp, OUTMAILBOX1);
		h2 = (ISP_READ(isp, OUTMAILBOX7) << 16) | ISP_READ(isp, OUTMAILBOX6);
		break;

	case ASYNC_RIO16_5:
	case ASYNC_RIO16_4:
	case ASYNC_RIO16_3:
	case ASYNC_RIO16_2:
	case ASYNC_RIO16_1:
		isp_prt(isp, ISP_LOGERR, "unexpected 16 bit RIO handle");
		break;
	default:
		isp_prt(isp, ISP_LOGWARN, "%s: unhandled async code 0x%x", __func__, mbox);
		break;
	}

	if (h1 || h2) {
		isp_prt(isp, ISP_LOGDEBUG3, "fast post/rio completion of 0x%08x", h1);
		isp_fastpost_complete(isp, h1);
		if (h2) {
			isp_prt(isp, ISP_LOGDEBUG3, "fast post/rio completion of 0x%08x", h2);
			isp_fastpost_complete(isp, h2);
			if (isp->isp_fpcchiwater < 2) {
				isp->isp_fpcchiwater = 2;
			}
		} else {
			if (isp->isp_fpcchiwater < 1) {
				isp->isp_fpcchiwater = 1;
			}
		}
	} else {
		isp->isp_intoasync++;
	}
	return (acked);
}

#define	GET_24XX_BUS(isp, chan, msg)										\
	if (IS_24XX(isp)) {											\
		chan = ISP_READ(isp, OUTMAILBOX3) & 0xff;							\
		if (chan >= isp->isp_nchan) {									\
			isp_prt(isp, ISP_LOGERR, "bogus channel %u for %s at line %d",	chan, msg, __LINE__);	\
			break;											\
		}												\
	}


static int
isp_parse_async_fc(ispsoftc_t *isp, uint16_t mbox)
{
	int acked = 0;
	uint16_t chan;

	if (IS_DUALBUS(isp)) {
		chan = ISP_READ(isp, OUTMAILBOX6);
	} else {
		chan = 0;
	}
	isp_prt(isp, ISP_LOGDEBUG2, "Async Mbox 0x%x", mbox);

	switch (mbox) {
	case ASYNC_SYSTEM_ERROR:
		isp->isp_dead = 1;
		isp->isp_state = ISP_CRASHED;
		FCPARAM(isp, chan)->isp_loopstate = LOOP_NIL;
		FCPARAM(isp, chan)->isp_fwstate = FW_CONFIG_WAIT;
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
		isp_async(isp, ISPASYNC_FW_CRASH);
		acked = 1;
		break;

	case ASYNC_RQS_XFER_ERR:
		isp_prt(isp, ISP_LOGERR, "Request Queue Transfer Error");
		break;

	case ASYNC_RSP_XFER_ERR:
		isp_prt(isp, ISP_LOGERR, "Response Queue Transfer Error");
		break;

	case ASYNC_QWAKEUP:
#ifdef	ISP_TARGET_MODE
		if (IS_24XX(isp)) {
			isp_prt(isp, ISP_LOGERR, "ATIO Queue Transfer Error");
			break;
		}
#endif
		isp_prt(isp, ISP_LOGERR, "%s: unexpected ASYNC_QWAKEUP code", __func__);
		break;

	case ASYNC_CMD_CMPLT:
		isp_fastpost_complete(isp, (ISP_READ(isp, OUTMAILBOX2) << 16) | ISP_READ(isp, OUTMAILBOX1));
		if (isp->isp_fpcchiwater < 1) {
			isp->isp_fpcchiwater = 1;
		}
		break;

	case ASYNC_RIOZIO_STALL:
		break;

	case ASYNC_CTIO_DONE:
#ifdef	ISP_TARGET_MODE
		if (isp_target_async(isp, (ISP_READ(isp, OUTMAILBOX2) << 16) | ISP_READ(isp, OUTMAILBOX1), mbox)) {
			acked = 1;
		} else {
			isp->isp_fphccmplt++;
		}
#else
		isp_prt(isp, ISP_LOGWARN, "unexpected ASYNC CTIO done");
#endif
		break;
	case ASYNC_LIP_ERROR:
	case ASYNC_LIP_F8:
	case ASYNC_LIP_OCCURRED:
	case ASYNC_PTPMODE:
		/*
		 * These are broadcast events that have to be sent across
		 * all active channels.
		 */
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			fcparam *fcp = FCPARAM(isp, chan);
			int topo = fcp->isp_topo;

			if (fcp->role == ISP_ROLE_NONE) {
				continue;
			}

			fcp->isp_fwstate = FW_CONFIG_WAIT;
			fcp->isp_loopstate = LOOP_LIP_RCVD;
			ISP_SET_SENDMARKER(isp, chan, 1);
			ISP_MARK_PORTDB(isp, chan, 1);
			isp_async(isp, ISPASYNC_LIP, chan);
#ifdef	ISP_TARGET_MODE
			if (isp_target_async(isp, chan, mbox)) {
				acked = 1;
			}
#endif
			/*
			 * We've had problems with data corruption occuring on
			 * commands that complete (with no apparent error) after
			 * we receive a LIP. This has been observed mostly on
			 * Local Loop topologies. To be safe, let's just mark
			 * all active initiator commands as dead.
			 */
			if (topo == TOPO_NL_PORT || topo == TOPO_FL_PORT) {
				int i, j;
				for (i = j = 0; i < isp->isp_maxcmds; i++) {
					XS_T *xs;
					isp_hdl_t *hdp;

					hdp = &isp->isp_xflist[i];
					if (ISP_H2HT(hdp->handle) != ISP_HANDLE_INITIATOR) {
						continue;
					}
					xs = hdp->cmd;
					if (XS_CHANNEL(xs) != chan) {
						continue;
					}
					j++;
					isp_prt(isp, ISP_LOG_WARN1,
					    "%d.%d.%jx bus reset set at %s:%u",
					    XS_CHANNEL(xs), XS_TGT(xs),
					    (uintmax_t)XS_LUN(xs),
					    __func__, __LINE__);
					XS_SETERR(xs, HBA_BUSRESET);
				}
				if (j) {
					isp_prt(isp, ISP_LOGERR, lipd, chan, j);
				}
			}
		}
		break;

	case ASYNC_LOOP_UP:
		/*
		 * This is a broadcast event that has to be sent across
		 * all active channels.
		 */
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			fcparam *fcp = FCPARAM(isp, chan);

			if (fcp->role == ISP_ROLE_NONE) {
				continue;
			}

			ISP_SET_SENDMARKER(isp, chan, 1);

			fcp->isp_fwstate = FW_CONFIG_WAIT;
			fcp->isp_loopstate = LOOP_LIP_RCVD;
			ISP_MARK_PORTDB(isp, chan, 1);
			isp_async(isp, ISPASYNC_LOOP_UP, chan);
#ifdef	ISP_TARGET_MODE
			if (isp_target_async(isp, chan, mbox)) {
				acked = 1;
			}
#endif
		}
		break;

	case ASYNC_LOOP_DOWN:
		/*
		 * This is a broadcast event that has to be sent across
		 * all active channels.
		 */
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			fcparam *fcp = FCPARAM(isp, chan);

			if (fcp->role == ISP_ROLE_NONE) {
				continue;
			}

			ISP_SET_SENDMARKER(isp, chan, 1);
			fcp->isp_fwstate = FW_CONFIG_WAIT;
			fcp->isp_loopstate = LOOP_NIL;
			ISP_MARK_PORTDB(isp, chan, 1);
			isp_async(isp, ISPASYNC_LOOP_DOWN, chan);
#ifdef	ISP_TARGET_MODE
			if (isp_target_async(isp, chan, mbox)) {
				acked = 1;
			}
#endif
		}
		break;

	case ASYNC_LOOP_RESET:
		/*
		 * This is a broadcast event that has to be sent across
		 * all active channels.
		 */
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			fcparam *fcp = FCPARAM(isp, chan);

			if (fcp->role == ISP_ROLE_NONE) {
				continue;
			}

			ISP_SET_SENDMARKER(isp, chan, 1);
			fcp->isp_fwstate = FW_CONFIG_WAIT;
			fcp->isp_loopstate = LOOP_NIL;
			ISP_MARK_PORTDB(isp, chan, 1);
			isp_async(isp, ISPASYNC_LOOP_RESET, chan);
#ifdef	ISP_TARGET_MODE
			if (isp_target_async(isp, chan, mbox)) {
				acked = 1;
			}
#endif
		}
		break;

	case ASYNC_PDB_CHANGED:
	{
		int nphdl, nlstate, reason;
		/*
		 * We *should* get a channel out of the 24XX, but we don't seem
		 * to get more than a PDB CHANGED on channel 0, so turn it into
		 * a broadcast event.
		 */
		if (IS_24XX(isp)) {
			nphdl = ISP_READ(isp, OUTMAILBOX1);
			nlstate = ISP_READ(isp, OUTMAILBOX2);
			reason = ISP_READ(isp, OUTMAILBOX3) >> 8;
		} else {
			nphdl = NIL_HANDLE;
			nlstate = reason = 0;
		}
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			fcparam *fcp = FCPARAM(isp, chan);

			if (fcp->role == ISP_ROLE_NONE) {
				continue;
			}
			ISP_SET_SENDMARKER(isp, chan, 1);
			fcp->isp_loopstate = LOOP_PDB_RCVD;
			ISP_MARK_PORTDB(isp, chan, 1);
			isp_async(isp, ISPASYNC_CHANGE_NOTIFY, chan, ISPASYNC_CHANGE_PDB, nphdl, nlstate, reason);
		}
		break;
	}
	case ASYNC_CHANGE_NOTIFY:
	{
		int lochan, hichan;

		if (ISP_FW_NEWER_THAN(isp, 4, 0, 25) && ISP_CAP_MULTI_ID(isp)) {
			GET_24XX_BUS(isp, chan, "ASYNC_CHANGE_NOTIFY");
			lochan = chan;
			hichan = chan + 1;
		} else {
			lochan = 0;
			hichan = isp->isp_nchan;
		}
		for (chan = lochan; chan < hichan; chan++) {
			fcparam *fcp = FCPARAM(isp, chan);

			if (fcp->role == ISP_ROLE_NONE) {
				continue;
			}

			if (fcp->isp_topo == TOPO_F_PORT) {
				fcp->isp_loopstate = LOOP_LSCAN_DONE;
			} else {
				fcp->isp_loopstate = LOOP_PDB_RCVD;
			}
			ISP_MARK_PORTDB(isp, chan, 1);
			isp_async(isp, ISPASYNC_CHANGE_NOTIFY, chan, ISPASYNC_CHANGE_SNS);
		}
		break;
	}

	case ASYNC_CONNMODE:
		/*
		 * This only applies to 2100 amd 2200 cards
		 */
		if (!IS_2200(isp) && !IS_2100(isp)) {
			isp_prt(isp, ISP_LOGWARN, "bad card for ASYNC_CONNMODE event");
			break;
		}
		chan = 0;
		mbox = ISP_READ(isp, OUTMAILBOX1);
		ISP_MARK_PORTDB(isp, chan, 1);
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
			isp->isp_dead = 1;
			isp->isp_state = ISP_CRASHED;
			isp_prt(isp, ISP_LOGERR, "FATAL CONNECTION ERROR");
			isp_async(isp, ISPASYNC_FW_CRASH);
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
		isp_async(isp, ISPASYNC_CHANGE_NOTIFY, chan, ISPASYNC_CHANGE_OTHER);
		FCPARAM(isp, chan)->sendmarker = 1;
		FCPARAM(isp, chan)->isp_fwstate = FW_CONFIG_WAIT;
		FCPARAM(isp, chan)->isp_loopstate = LOOP_LIP_RCVD;
		break;

	case ASYNC_RCV_ERR:
		if (IS_24XX(isp)) {
			isp_prt(isp, ISP_LOGWARN, "Receive Error");
		} else {
			isp_prt(isp, ISP_LOGWARN, "unexpected ASYNC_RCV_ERR");
		}
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
	if (mbox != ASYNC_CTIO_DONE && mbox != ASYNC_CMD_CMPLT) {
		isp->isp_intoasync++;
	}
	return (acked);
}

/*
 * Handle other response entries. A pointer to the request queue output
 * index is here in case we want to eat several entries at once, although
 * this is not used currently.
 */

static int
isp_handle_other_response(ispsoftc_t *isp, int type, isphdr_t *hp, uint32_t *optrp)
{
	switch (type) {
	case RQSTYPE_STATUS_CONT:
		isp_prt(isp, ISP_LOG_WARN1, "Ignored Continuation Response");
		return (1);
	case RQSTYPE_MARKER:
		isp_prt(isp, ISP_LOG_WARN1, "Marker Response");
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
	case RQSTYPE_RPT_ID_ACQ:
		if (IS_24XX(isp)) {
			isp_ridacq_t rid;
			isp_get_ridacq(isp, (isp_ridacq_t *)hp, &rid);
			if (rid.ridacq_format == 0) {
			}
			return (1);
		}
		/* FALLTHROUGH */
	case RQSTYPE_REQUEST:
	default:
		ISP_DELAY(100);
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
			isp_xs_prt(isp, xs, ISP_LOG_WARN1, "Selection Timeout @ %s:%d", __func__, __LINE__);
			if (XS_NOERR(xs)) {
				XS_SETERR(xs, HBA_SELTIMEOUT);
				*rp = XS_XFRLEN(xs);
			}
			return;
		}
		isp_xs_prt(isp, xs, ISP_LOGERR, "Command Incomplete, state 0x%x", sp->req_state_flags);
		break;

	case RQCS_DMA_ERROR:
		isp_xs_prt(isp, xs, ISP_LOGERR, "DMA Error");
		*rp = XS_XFRLEN(xs);
		break;

	case RQCS_TRANSPORT_ERROR:
	{
		char buf[172];
		ISP_SNPRINTF(buf, sizeof (buf), "states=>");
		if (sp->req_state_flags & RQSF_GOT_BUS) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s GOT_BUS", buf);
		}
		if (sp->req_state_flags & RQSF_GOT_TARGET) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s GOT_TGT", buf);
		}
		if (sp->req_state_flags & RQSF_SENT_CDB) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s SENT_CDB", buf);
		}
		if (sp->req_state_flags & RQSF_XFRD_DATA) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s XFRD_DATA", buf);
		}
		if (sp->req_state_flags & RQSF_GOT_STATUS) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s GOT_STS", buf);
		}
		if (sp->req_state_flags & RQSF_GOT_SENSE) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s GOT_SNS", buf);
		}
		if (sp->req_state_flags & RQSF_XFER_COMPLETE) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s XFR_CMPLT", buf);
		}
		ISP_SNPRINTF(buf, sizeof (buf), "%s\nstatus=>", buf);
		if (sp->req_status_flags & RQSTF_DISCONNECT) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s Disconnect", buf);
		}
		if (sp->req_status_flags & RQSTF_SYNCHRONOUS) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s Sync_xfr", buf);
		}
		if (sp->req_status_flags & RQSTF_PARITY_ERROR) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s Parity", buf);
		}
		if (sp->req_status_flags & RQSTF_BUS_RESET) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s Bus_Reset", buf);
		}
		if (sp->req_status_flags & RQSTF_DEVICE_RESET) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s Device_Reset", buf);
		}
		if (sp->req_status_flags & RQSTF_ABORTED) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s Aborted", buf);
		}
		if (sp->req_status_flags & RQSTF_TIMEOUT) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s Timeout", buf);
		}
		if (sp->req_status_flags & RQSTF_NEGOTIATION) {
			ISP_SNPRINTF(buf, sizeof (buf), "%s Negotiation", buf);
		}
		isp_xs_prt(isp, xs,  ISP_LOGERR, "Transport Error: %s", buf);
		*rp = XS_XFRLEN(xs);
		break;
	}
	case RQCS_RESET_OCCURRED:
	{
		int chan;
		isp_xs_prt(isp, xs, ISP_LOGWARN, "Bus Reset destroyed command");
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			FCPARAM(isp, chan)->sendmarker = 1;
		}
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_BUSRESET);
		}
		*rp = XS_XFRLEN(xs);
		return;
	}
	case RQCS_ABORTED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "Command Aborted");
		ISP_SET_SENDMARKER(isp, XS_CHANNEL(xs), 1);
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_ABORTED);
		}
		return;

	case RQCS_TIMEOUT:
		isp_xs_prt(isp, xs, ISP_LOGWARN, "Command timed out");
		/*
	 	 * XXX: Check to see if we logged out of the device.
		 */
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_CMDTIMEOUT);
		}
		return;

	case RQCS_DATA_OVERRUN:
		XS_SET_RESID(xs, sp->req_resid);
		isp_xs_prt(isp, xs, ISP_LOGERR, "data overrun (%ld)", (long) XS_GET_RESID(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_DATAOVR);
		}
		return;

	case RQCS_COMMAND_OVERRUN:
		isp_xs_prt(isp, xs, ISP_LOGERR, "command overrun");
		break;

	case RQCS_STATUS_OVERRUN:
		isp_xs_prt(isp, xs, ISP_LOGERR, "status overrun");
		break;

	case RQCS_BAD_MESSAGE:
		isp_xs_prt(isp, xs, ISP_LOGERR, "msg not COMMAND COMPLETE after status");
		break;

	case RQCS_NO_MESSAGE_OUT:
		isp_xs_prt(isp, xs, ISP_LOGERR, "No MESSAGE OUT phase after selection");
		break;

	case RQCS_EXT_ID_FAILED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "EXTENDED IDENTIFY failed");
		break;

	case RQCS_IDE_MSG_FAILED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "INITIATOR DETECTED ERROR rejected");
		break;

	case RQCS_ABORT_MSG_FAILED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "ABORT OPERATION rejected");
		break;

	case RQCS_REJECT_MSG_FAILED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "MESSAGE REJECT rejected");
		break;

	case RQCS_NOP_MSG_FAILED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "NOP rejected");
		break;

	case RQCS_PARITY_ERROR_MSG_FAILED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "MESSAGE PARITY ERROR rejected");
		break;

	case RQCS_DEVICE_RESET_MSG_FAILED:
		isp_xs_prt(isp, xs, ISP_LOGWARN, "BUS DEVICE RESET rejected");
		break;

	case RQCS_ID_MSG_FAILED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "IDENTIFY rejected");
		break;

	case RQCS_UNEXP_BUS_FREE:
		isp_xs_prt(isp, xs, ISP_LOGERR, "Unexpected Bus Free");
		break;

	case RQCS_DATA_UNDERRUN:
	{
		if (IS_FC(isp)) {
			int ru_marked = (sp->req_scsi_status & RQCS_RU) != 0;
			if (!ru_marked || sp->req_resid > XS_XFRLEN(xs)) {
				isp_xs_prt(isp, xs, ISP_LOGWARN, bun, XS_XFRLEN(xs), sp->req_resid, (ru_marked)? "marked" : "not marked");
				if (XS_NOERR(xs)) {
					XS_SETERR(xs, HBA_BOTCH);
				}
				return;
			}
		}
		XS_SET_RESID(xs, sp->req_resid);
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_NOERROR);
		}
		return;
	}

	case RQCS_XACT_ERR1:
		isp_xs_prt(isp, xs, ISP_LOGERR, "HBA attempted queued transaction with disconnect not set");
		break;

	case RQCS_XACT_ERR2:
		isp_xs_prt(isp, xs, ISP_LOGERR,
		    "HBA attempted queued transaction to target routine %jx",
		    (uintmax_t)XS_LUN(xs));
		break;

	case RQCS_XACT_ERR3:
		isp_xs_prt(isp, xs, ISP_LOGERR, "HBA attempted queued cmd when queueing disabled");
		break;

	case RQCS_BAD_ENTRY:
		isp_prt(isp, ISP_LOGERR, "Invalid IOCB entry type detected");
		break;

	case RQCS_QUEUE_FULL:
		isp_xs_prt(isp, xs, ISP_LOG_WARN1, "internal queues full status 0x%x", *XS_STSP(xs));

		/*
		 * If QFULL or some other status byte is set, then this
		 * isn't an error, per se.
		 *
		 * Unfortunately, some QLogic f/w writers have, in
		 * some cases, ommitted to *set* status to QFULL.
		 */
#if	0
		if (*XS_STSP(xs) != SCSI_GOOD && XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_NOERROR);
			return;
		}

#endif
		*XS_STSP(xs) = SCSI_QFULL;
		XS_SETERR(xs, HBA_NOERROR);
		return;

	case RQCS_PHASE_SKIPPED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "SCSI phase skipped");
		break;

	case RQCS_ARQS_FAILED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "Auto Request Sense Failed");
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_ARQFAIL);
		}
		return;

	case RQCS_WIDE_FAILED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "Wide Negotiation Failed");
		if (IS_SCSI(isp)) {
			sdparam *sdp = SDPARAM(isp, XS_CHANNEL(xs));
			sdp->isp_devparam[XS_TGT(xs)].goal_flags &= ~DPARM_WIDE;
			sdp->isp_devparam[XS_TGT(xs)].dev_update = 1;
			sdp->update = 1;
		}
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_NOERROR);
		}
		return;

	case RQCS_SYNCXFER_FAILED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "SDTR Message Failed");
		if (IS_SCSI(isp)) {
			sdparam *sdp = SDPARAM(isp, XS_CHANNEL(xs));
			sdp += XS_CHANNEL(xs);
			sdp->isp_devparam[XS_TGT(xs)].goal_flags &= ~DPARM_SYNC;
			sdp->isp_devparam[XS_TGT(xs)].dev_update = 1;
			sdp->update = 1;
		}
		break;

	case RQCS_LVD_BUSERR:
		isp_xs_prt(isp, xs, ISP_LOGERR, "Bad LVD condition");
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

		isp_prt(isp, ISP_LOGINFO, "port %s for target %d", reason, XS_TGT(xs));

		/*
		 * If we're on a local loop, force a LIP (which is overkill)
		 * to force a re-login of this unit. If we're on fabric,
		 * then we'll have to log in again as a matter of course.
		 */
		if (FCPARAM(isp, 0)->isp_topo == TOPO_NL_PORT ||
		    FCPARAM(isp, 0)->isp_topo == TOPO_FL_PORT) {
			mbreg_t mbs;
			MBSINIT(&mbs, MBOX_INIT_LIP, MBLOGALL, 0);
			if (ISP_CAP_2KLOGIN(isp)) {
				mbs.ibits = (1 << 10);
			}
			isp_mboxcmd_qnw(isp, &mbs, 1);
		}
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
		}
		return;
	}
	case RQCS_PORT_CHANGED:
		isp_prt(isp, ISP_LOGWARN, "port changed for target %d", XS_TGT(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
		}
		return;

	case RQCS_PORT_BUSY:
		isp_prt(isp, ISP_LOGWARN, "port busy for target %d", XS_TGT(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_TGTBSY);
		}
		return;

	default:
		isp_prt(isp, ISP_LOGERR, "Unknown Completion Status 0x%x", sp->req_completion_status);
		break;
	}
	if (XS_NOERR(xs)) {
		XS_SETERR(xs, HBA_BOTCH);
	}
}

static void
isp_parse_status_24xx(ispsoftc_t *isp, isp24xx_statusreq_t *sp, XS_T *xs, long *rp)
{
	int ru_marked, sv_marked;
	int chan = XS_CHANNEL(xs);

	switch (sp->req_completion_status) {
	case RQCS_COMPLETE:
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_NOERROR);
		}
		return;

	case RQCS_DMA_ERROR:
		isp_xs_prt(isp, xs, ISP_LOGERR, "DMA error");
		break;

	case RQCS_TRANSPORT_ERROR:
		isp_xs_prt(isp, xs,  ISP_LOGERR, "Transport Error");
		break;

	case RQCS_RESET_OCCURRED:
		isp_xs_prt(isp, xs, ISP_LOGWARN, "reset destroyed command");
		FCPARAM(isp, chan)->sendmarker = 1;
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_BUSRESET);
		}
		return;

	case RQCS_ABORTED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "Command Aborted");
		FCPARAM(isp, chan)->sendmarker = 1;
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_ABORTED);
		}
		return;

	case RQCS_TIMEOUT:
		isp_xs_prt(isp, xs, ISP_LOGWARN, "Command Timed Out");
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_CMDTIMEOUT);
		}
		return;

	case RQCS_DATA_OVERRUN:
		XS_SET_RESID(xs, sp->req_resid);
		isp_xs_prt(isp, xs, ISP_LOGERR, "Data Overrun");
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_DATAOVR);
		}
		return;

	case RQCS_24XX_DRE:	/* data reassembly error */
		isp_prt(isp, ISP_LOGERR, "Chan %d data reassembly error for target %d", chan, XS_TGT(xs));
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_ABORTED);
		}
		*rp = XS_XFRLEN(xs);
		return;

	case RQCS_24XX_TABORT:	/* aborted by target */
		isp_prt(isp, ISP_LOGERR, "Chan %d target %d sent ABTS", chan, XS_TGT(xs));
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
			isp_xs_prt(isp, xs, ISP_LOGWARN, bun, XS_XFRLEN(xs), sp->req_resid, (ru_marked)? "marked" : "not marked");
			if (XS_NOERR(xs)) {
				XS_SETERR(xs, HBA_BOTCH);
			}
			return;
		}
		XS_SET_RESID(xs, sp->req_resid);
		isp_xs_prt(isp, xs, ISP_LOG_WARN1, "Data Underrun (%d) for command 0x%x", sp->req_resid, XS_CDBP(xs)[0] & 0xff);
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

		isp_prt(isp, ISP_LOGINFO, "Chan %d port %s for target %d",
		    chan, reason, XS_TGT(xs));

		/*
		 * There is no MBOX_INIT_LIP for the 24XX.
		 */
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
		}
		return;
	}
	case RQCS_PORT_CHANGED:
		isp_prt(isp, ISP_LOGWARN, "port changed for target %d chan %d", XS_TGT(xs), chan);
		if (XS_NOERR(xs)) {
			XS_SETERR(xs, HBA_SELTIMEOUT);
		}
		return;


	case RQCS_24XX_ENOMEM:	/* f/w resource unavailable */
		isp_prt(isp, ISP_LOGWARN, "f/w resource unavailable for target %d chan %d", XS_TGT(xs), chan);
		if (XS_NOERR(xs)) {
			*XS_STSP(xs) = SCSI_BUSY;
			XS_SETERR(xs, HBA_TGTBSY);
		}
		return;

	case RQCS_24XX_TMO:	/* task management overrun */
		isp_prt(isp, ISP_LOGWARN, "command for target %d overlapped task management for chan %d", XS_TGT(xs), chan);
		if (XS_NOERR(xs)) {
			*XS_STSP(xs) = SCSI_BUSY;
			XS_SETERR(xs, HBA_TGTBSY);
		}
		return;

	default:
		isp_prt(isp, ISP_LOGERR, "Unknown Completion Status 0x%x on chan %d", sp->req_completion_status, chan);
		break;
	}
	if (XS_NOERR(xs)) {
		XS_SETERR(xs, HBA_BOTCH);
	}
}

static void
isp_fastpost_complete(ispsoftc_t *isp, uint32_t fph)
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
	XS_SET_RESID(xs, 0);
	*XS_STSP(xs) = SCSI_GOOD;
	if (XS_XFRLEN(xs)) {
		ISP_DMAFREE(isp, xs, fph);
	}
	if (isp->isp_nactive) {
		isp->isp_nactive--;
	}
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
	ISP_MEMZERO(&mbs, sizeof (mbs));
	ptr = isp->isp_mbxworkp;
	switch (isp->isp_lastmbxcmd) {
	case MBOX_WRITE_RAM_WORD:
		mbs.param[1] = isp->isp_mbxwrk1++;
		mbs.param[2] = *ptr++;
		break;
	case MBOX_READ_RAM_WORD:
		*ptr++ = isp->isp_mboxtmp[2];
		mbs.param[1] = isp->isp_mbxwrk1++;
		break;
	case MBOX_WRITE_RAM_WORD_EXTENDED:
		if (IS_24XX(isp)) {
			uint32_t *lptr = (uint32_t *)ptr;
			mbs.param[2] = lptr[0];
			mbs.param[3] = lptr[0] >> 16;
			lptr++;
			ptr = (uint16_t *)lptr;
		} else {
			mbs.param[2] = *ptr++;
		}
		offset = isp->isp_mbxwrk1;
		offset |= isp->isp_mbxwrk8 << 16;
		mbs.param[1] = offset;
		mbs.param[8] = offset >> 16;
		offset++;
		isp->isp_mbxwrk1 = offset;
		isp->isp_mbxwrk8 = offset >> 16;
		break;
	case MBOX_READ_RAM_WORD_EXTENDED:
		if (IS_24XX(isp)) {
			uint32_t *lptr = (uint32_t *)ptr;
			uint32_t val = isp->isp_mboxtmp[2];
			val |= (isp->isp_mboxtmp[3]) << 16;
			*lptr++ = val;
			ptr = (uint16_t *)lptr;
		} else {
			*ptr++ = isp->isp_mboxtmp[2];
		}
		offset = isp->isp_mbxwrk1;
		offset |= isp->isp_mbxwrk8 << 16;
		mbs.param[1] = offset;
		mbs.param[8] = offset >> 16;
		offset++;
		isp->isp_mbxwrk1 = offset;
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

#define	ISP_SCSI_IBITS(op)		(mbpscsi[((op)<<1)])
#define	ISP_SCSI_OBITS(op)		(mbpscsi[((op)<<1) + 1])
#define	ISP_SCSI_OPMAP(in, out)		in, out
static const uint8_t mbpscsi[] = {
	ISP_SCSI_OPMAP(0x01, 0x01),	/* 0x00: MBOX_NO_OP */
	ISP_SCSI_OPMAP(0x1f, 0x01),	/* 0x01: MBOX_LOAD_RAM */
	ISP_SCSI_OPMAP(0x03, 0x01),	/* 0x02: MBOX_EXEC_FIRMWARE */
	ISP_SCSI_OPMAP(0x1f, 0x01),	/* 0x03: MBOX_DUMP_RAM */
	ISP_SCSI_OPMAP(0x07, 0x07),	/* 0x04: MBOX_WRITE_RAM_WORD */
	ISP_SCSI_OPMAP(0x03, 0x07),	/* 0x05: MBOX_READ_RAM_WORD */
	ISP_SCSI_OPMAP(0x3f, 0x3f),	/* 0x06: MBOX_MAILBOX_REG_TEST */
	ISP_SCSI_OPMAP(0x07, 0x07),	/* 0x07: MBOX_VERIFY_CHECKSUM	*/
	ISP_SCSI_OPMAP(0x01, 0x0f),	/* 0x08: MBOX_ABOUT_FIRMWARE */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x09: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x0a: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x0b: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x0c: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x0d: */
	ISP_SCSI_OPMAP(0x01, 0x05),	/* 0x0e: MBOX_CHECK_FIRMWARE */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x0f: */
	ISP_SCSI_OPMAP(0x1f, 0x1f),	/* 0x10: MBOX_INIT_REQ_QUEUE */
	ISP_SCSI_OPMAP(0x3f, 0x3f),	/* 0x11: MBOX_INIT_RES_QUEUE */
	ISP_SCSI_OPMAP(0x0f, 0x0f),	/* 0x12: MBOX_EXECUTE_IOCB */
	ISP_SCSI_OPMAP(0x03, 0x03),	/* 0x13: MBOX_WAKE_UP	*/
	ISP_SCSI_OPMAP(0x01, 0x3f),	/* 0x14: MBOX_STOP_FIRMWARE */
	ISP_SCSI_OPMAP(0x0f, 0x0f),	/* 0x15: MBOX_ABORT */
	ISP_SCSI_OPMAP(0x03, 0x03),	/* 0x16: MBOX_ABORT_DEVICE */
	ISP_SCSI_OPMAP(0x07, 0x07),	/* 0x17: MBOX_ABORT_TARGET */
	ISP_SCSI_OPMAP(0x07, 0x07),	/* 0x18: MBOX_BUS_RESET */
	ISP_SCSI_OPMAP(0x03, 0x07),	/* 0x19: MBOX_STOP_QUEUE */
	ISP_SCSI_OPMAP(0x03, 0x07),	/* 0x1a: MBOX_START_QUEUE */
	ISP_SCSI_OPMAP(0x03, 0x07),	/* 0x1b: MBOX_SINGLE_STEP_QUEUE */
	ISP_SCSI_OPMAP(0x03, 0x07),	/* 0x1c: MBOX_ABORT_QUEUE */
	ISP_SCSI_OPMAP(0x03, 0x4f),	/* 0x1d: MBOX_GET_DEV_QUEUE_STATUS */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x1e: */
	ISP_SCSI_OPMAP(0x01, 0x07),	/* 0x1f: MBOX_GET_FIRMWARE_STATUS */
	ISP_SCSI_OPMAP(0x01, 0x07),	/* 0x20: MBOX_GET_INIT_SCSI_ID */
	ISP_SCSI_OPMAP(0x01, 0x07),	/* 0x21: MBOX_GET_SELECT_TIMEOUT */
	ISP_SCSI_OPMAP(0x01, 0xc7),	/* 0x22: MBOX_GET_RETRY_COUNT	*/
	ISP_SCSI_OPMAP(0x01, 0x07),	/* 0x23: MBOX_GET_TAG_AGE_LIMIT */
	ISP_SCSI_OPMAP(0x01, 0x03),	/* 0x24: MBOX_GET_CLOCK_RATE */
	ISP_SCSI_OPMAP(0x01, 0x07),	/* 0x25: MBOX_GET_ACT_NEG_STATE */
	ISP_SCSI_OPMAP(0x01, 0x07),	/* 0x26: MBOX_GET_ASYNC_DATA_SETUP_TIME */
	ISP_SCSI_OPMAP(0x01, 0x07),	/* 0x27: MBOX_GET_PCI_PARAMS */
	ISP_SCSI_OPMAP(0x03, 0x4f),	/* 0x28: MBOX_GET_TARGET_PARAMS */
	ISP_SCSI_OPMAP(0x03, 0x0f),	/* 0x29: MBOX_GET_DEV_QUEUE_PARAMS */
	ISP_SCSI_OPMAP(0x01, 0x07),	/* 0x2a: MBOX_GET_RESET_DELAY_PARAMS */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x2b: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x2c: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x2d: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x2e: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x2f: */
	ISP_SCSI_OPMAP(0x03, 0x03),	/* 0x30: MBOX_SET_INIT_SCSI_ID */
	ISP_SCSI_OPMAP(0x07, 0x07),	/* 0x31: MBOX_SET_SELECT_TIMEOUT */
	ISP_SCSI_OPMAP(0xc7, 0xc7),	/* 0x32: MBOX_SET_RETRY_COUNT	*/
	ISP_SCSI_OPMAP(0x07, 0x07),	/* 0x33: MBOX_SET_TAG_AGE_LIMIT */
	ISP_SCSI_OPMAP(0x03, 0x03),	/* 0x34: MBOX_SET_CLOCK_RATE */
	ISP_SCSI_OPMAP(0x07, 0x07),	/* 0x35: MBOX_SET_ACT_NEG_STATE */
	ISP_SCSI_OPMAP(0x07, 0x07),	/* 0x36: MBOX_SET_ASYNC_DATA_SETUP_TIME */
	ISP_SCSI_OPMAP(0x07, 0x07),	/* 0x37: MBOX_SET_PCI_CONTROL_PARAMS */
	ISP_SCSI_OPMAP(0x4f, 0x4f),	/* 0x38: MBOX_SET_TARGET_PARAMS */
	ISP_SCSI_OPMAP(0x0f, 0x0f),	/* 0x39: MBOX_SET_DEV_QUEUE_PARAMS */
	ISP_SCSI_OPMAP(0x07, 0x07),	/* 0x3a: MBOX_SET_RESET_DELAY_PARAMS */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x3b: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x3c: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x3d: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x3e: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x3f: */
	ISP_SCSI_OPMAP(0x01, 0x03),	/* 0x40: MBOX_RETURN_BIOS_BLOCK_ADDR */
	ISP_SCSI_OPMAP(0x3f, 0x01),	/* 0x41: MBOX_WRITE_FOUR_RAM_WORDS */
	ISP_SCSI_OPMAP(0x03, 0x07),	/* 0x42: MBOX_EXEC_BIOS_IOCB */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x43: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x44: */
	ISP_SCSI_OPMAP(0x03, 0x03),	/* 0x45: SET SYSTEM PARAMETER */
	ISP_SCSI_OPMAP(0x01, 0x03),	/* 0x46: GET SYSTEM PARAMETER */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x47: */
	ISP_SCSI_OPMAP(0x01, 0xcf),	/* 0x48: GET SCAM CONFIGURATION */
	ISP_SCSI_OPMAP(0xcf, 0xcf),	/* 0x49: SET SCAM CONFIGURATION */
	ISP_SCSI_OPMAP(0x03, 0x03),	/* 0x4a: MBOX_SET_FIRMWARE_FEATURES */
	ISP_SCSI_OPMAP(0x01, 0x03),	/* 0x4b: MBOX_GET_FIRMWARE_FEATURES */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x4c: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x4d: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x4e: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x4f: */
	ISP_SCSI_OPMAP(0xdf, 0xdf),	/* 0x50: LOAD RAM A64 */
	ISP_SCSI_OPMAP(0xdf, 0xdf),	/* 0x51: DUMP RAM A64 */
	ISP_SCSI_OPMAP(0xdf, 0xff),	/* 0x52: INITIALIZE REQUEST QUEUE A64 */
	ISP_SCSI_OPMAP(0xef, 0xff),	/* 0x53: INITIALIZE RESPONSE QUEUE A64 */
	ISP_SCSI_OPMAP(0xcf, 0x01),	/* 0x54: EXECUCUTE COMMAND IOCB A64 */
	ISP_SCSI_OPMAP(0x07, 0x01),	/* 0x55: ENABLE TARGET MODE */
	ISP_SCSI_OPMAP(0x03, 0x0f),	/* 0x56: GET TARGET STATUS */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x57: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x58: */
	ISP_SCSI_OPMAP(0x00, 0x00),	/* 0x59: */
	ISP_SCSI_OPMAP(0x03, 0x03),	/* 0x5a: SET DATA OVERRUN RECOVERY MODE */
	ISP_SCSI_OPMAP(0x01, 0x03),	/* 0x5b: GET DATA OVERRUN RECOVERY MODE */
	ISP_SCSI_OPMAP(0x0f, 0x0f),	/* 0x5c: SET HOST DATA */
	ISP_SCSI_OPMAP(0x01, 0x01)	/* 0x5d: GET NOST DATA */
};
#define	MAX_SCSI_OPCODE	0x5d

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

#define	ISP_FC_IBITS(op)	((mbpfc[((op)<<3) + 0] << 24) | (mbpfc[((op)<<3) + 1] << 16) | (mbpfc[((op)<<3) + 2] << 8) | (mbpfc[((op)<<3) + 3]))
#define	ISP_FC_OBITS(op)	((mbpfc[((op)<<3) + 4] << 24) | (mbpfc[((op)<<3) + 5] << 16) | (mbpfc[((op)<<3) + 6] << 8) | (mbpfc[((op)<<3) + 7]))

#define	ISP_FC_OPMAP(in0, out0)							  0,   0,   0, in0,    0,    0,    0, out0
#define	ISP_FC_OPMAP_HALF(in1, in0, out1, out0)					  0,   0, in1, in0,    0,    0, out1, out0
#define	ISP_FC_OPMAP_FULL(in3, in2, in1, in0, out3, out2, out1, out0)		in3, in2, in1, in0, out3, out2, out1, out0
static const uint32_t mbpfc[] = {
	ISP_FC_OPMAP(0x01, 0x01),	/* 0x00: MBOX_NO_OP */
	ISP_FC_OPMAP(0x1f, 0x01),	/* 0x01: MBOX_LOAD_RAM */
	ISP_FC_OPMAP(0x0f, 0x01),	/* 0x02: MBOX_EXEC_FIRMWARE */
	ISP_FC_OPMAP(0xdf, 0x01),	/* 0x03: MBOX_DUMP_RAM */
	ISP_FC_OPMAP(0x07, 0x07),	/* 0x04: MBOX_WRITE_RAM_WORD */
	ISP_FC_OPMAP(0x03, 0x07),	/* 0x05: MBOX_READ_RAM_WORD */
	ISP_FC_OPMAP_FULL(0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff),	/* 0x06: MBOX_MAILBOX_REG_TEST */
	ISP_FC_OPMAP(0x07, 0x07),	/* 0x07: MBOX_VERIFY_CHECKSUM	*/
	ISP_FC_OPMAP_FULL(0x0, 0x0, 0x0, 0x01, 0x0, 0x3, 0x80, 0x7f),	/* 0x08: MBOX_ABOUT_FIRMWARE */
	ISP_FC_OPMAP(0xdf, 0x01),	/* 0x09: MBOX_LOAD_RISC_RAM_2100 */
	ISP_FC_OPMAP(0xdf, 0x01),	/* 0x0a: DUMP RAM */
	ISP_FC_OPMAP_HALF(0x1, 0xff, 0x0, 0x01),	/* 0x0b: MBOX_LOAD_RISC_RAM */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x0c: */
	ISP_FC_OPMAP_HALF(0x1, 0x0f, 0x0, 0x01),	/* 0x0d: MBOX_WRITE_RAM_WORD_EXTENDED */
	ISP_FC_OPMAP(0x01, 0x05),	/* 0x0e: MBOX_CHECK_FIRMWARE */
	ISP_FC_OPMAP_HALF(0x1, 0x03, 0x0, 0x0d),	/* 0x0f: MBOX_READ_RAM_WORD_EXTENDED */
	ISP_FC_OPMAP(0x1f, 0x11),	/* 0x10: MBOX_INIT_REQ_QUEUE */
	ISP_FC_OPMAP(0x2f, 0x21),	/* 0x11: MBOX_INIT_RES_QUEUE */
	ISP_FC_OPMAP(0x0f, 0x01),	/* 0x12: MBOX_EXECUTE_IOCB */
	ISP_FC_OPMAP(0x03, 0x03),	/* 0x13: MBOX_WAKE_UP	*/
	ISP_FC_OPMAP(0x01, 0xff),	/* 0x14: MBOX_STOP_FIRMWARE */
	ISP_FC_OPMAP(0x4f, 0x01),	/* 0x15: MBOX_ABORT */
	ISP_FC_OPMAP(0x07, 0x01),	/* 0x16: MBOX_ABORT_DEVICE */
	ISP_FC_OPMAP(0x07, 0x01),	/* 0x17: MBOX_ABORT_TARGET */
	ISP_FC_OPMAP(0x03, 0x03),	/* 0x18: MBOX_BUS_RESET */
	ISP_FC_OPMAP(0x07, 0x05),	/* 0x19: MBOX_STOP_QUEUE */
	ISP_FC_OPMAP(0x07, 0x05),	/* 0x1a: MBOX_START_QUEUE */
	ISP_FC_OPMAP(0x07, 0x05),	/* 0x1b: MBOX_SINGLE_STEP_QUEUE */
	ISP_FC_OPMAP(0x07, 0x05),	/* 0x1c: MBOX_ABORT_QUEUE */
	ISP_FC_OPMAP(0x07, 0x03),	/* 0x1d: MBOX_GET_DEV_QUEUE_STATUS */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x1e: */
	ISP_FC_OPMAP(0x01, 0x07),	/* 0x1f: MBOX_GET_FIRMWARE_STATUS */
	ISP_FC_OPMAP_HALF(0x2, 0x01, 0x0, 0xcf),	/* 0x20: MBOX_GET_LOOP_ID */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x21: */
	ISP_FC_OPMAP(0x01, 0x07),	/* 0x22: MBOX_GET_RETRY_COUNT	*/
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x23: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x24: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x25: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x26: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x27: */
	ISP_FC_OPMAP(0x01, 0x03),	/* 0x28: MBOX_GET_FIRMWARE_OPTIONS */
	ISP_FC_OPMAP(0x03, 0x07),	/* 0x29: MBOX_GET_PORT_QUEUE_PARAMS */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x2a: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x2b: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x2c: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x2d: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x2e: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x2f: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x30: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x31: */
	ISP_FC_OPMAP(0x07, 0x07),	/* 0x32: MBOX_SET_RETRY_COUNT	*/
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x33: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x34: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x35: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x36: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x37: */
	ISP_FC_OPMAP(0x0f, 0x01),	/* 0x38: MBOX_SET_FIRMWARE_OPTIONS */
	ISP_FC_OPMAP(0x0f, 0x07),	/* 0x39: MBOX_SET_PORT_QUEUE_PARAMS */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x3a: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x3b: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x3c: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x3d: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x3e: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x3f: */
	ISP_FC_OPMAP(0x03, 0x01),	/* 0x40: MBOX_LOOP_PORT_BYPASS */
	ISP_FC_OPMAP(0x03, 0x01),	/* 0x41: MBOX_LOOP_PORT_ENABLE */
	ISP_FC_OPMAP_HALF(0x0, 0x01, 0x3, 0xcf),	/* 0x42: MBOX_GET_RESOURCE_COUNT */
	ISP_FC_OPMAP(0x01, 0x01),	/* 0x43: MBOX_REQUEST_OFFLINE_MODE */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x44: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x45: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x46: */
	ISP_FC_OPMAP(0xcf, 0x03),	/* 0x47: GET PORT_DATABASE ENHANCED */
	ISP_FC_OPMAP(0xcf, 0x0f),	/* 0x48: MBOX_INIT_FIRMWARE_MULTI_ID */
	ISP_FC_OPMAP(0xcd, 0x01),	/* 0x49: MBOX_GET_VP_DATABASE */
	ISP_FC_OPMAP_HALF(0x2, 0xcd, 0x0, 0x01),	/* 0x4a: MBOX_GET_VP_DATABASE_ENTRY */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x4b: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x4c: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x4d: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x4e: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x4f: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x50: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x51: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x52: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x53: */
	ISP_FC_OPMAP(0xcf, 0x01),	/* 0x54: EXECUTE IOCB A64 */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x55: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x56: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x57: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x58: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x59: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x5a: */
	ISP_FC_OPMAP(0x03, 0x01),	/* 0x5b: MBOX_DRIVER_HEARTBEAT */
	ISP_FC_OPMAP(0xcf, 0x01),	/* 0x5c: MBOX_FW_HEARTBEAT */
	ISP_FC_OPMAP(0x07, 0x03),	/* 0x5d: MBOX_GET_SET_DATA_RATE */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x5e: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x5f: */
	ISP_FC_OPMAP(0xcf, 0x0f),	/* 0x60: MBOX_INIT_FIRMWARE */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x61: */
	ISP_FC_OPMAP(0x01, 0x01),	/* 0x62: MBOX_INIT_LIP */
	ISP_FC_OPMAP(0xcd, 0x03),	/* 0x63: MBOX_GET_FC_AL_POSITION_MAP */
	ISP_FC_OPMAP(0xcf, 0x01),	/* 0x64: MBOX_GET_PORT_DB */
	ISP_FC_OPMAP(0x07, 0x01),	/* 0x65: MBOX_CLEAR_ACA */
	ISP_FC_OPMAP(0x07, 0x01),	/* 0x66: MBOX_TARGET_RESET */
	ISP_FC_OPMAP(0x07, 0x01),	/* 0x67: MBOX_CLEAR_TASK_SET */
	ISP_FC_OPMAP(0x07, 0x01),	/* 0x68: MBOX_ABORT_TASK_SET */
	ISP_FC_OPMAP(0x01, 0x07),	/* 0x69: MBOX_GET_FW_STATE */
	ISP_FC_OPMAP_HALF(0x6, 0x03, 0x0, 0xcf),	/* 0x6a: MBOX_GET_PORT_NAME */
	ISP_FC_OPMAP(0xcf, 0x01),	/* 0x6b: MBOX_GET_LINK_STATUS */
	ISP_FC_OPMAP(0x0f, 0x01),	/* 0x6c: MBOX_INIT_LIP_RESET */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x6d: */
	ISP_FC_OPMAP(0xcf, 0x03),	/* 0x6e: MBOX_SEND_SNS */
	ISP_FC_OPMAP(0x0f, 0x07),	/* 0x6f: MBOX_FABRIC_LOGIN */
	ISP_FC_OPMAP(0x03, 0x01),	/* 0x70: MBOX_SEND_CHANGE_REQUEST */
	ISP_FC_OPMAP(0x03, 0x03),	/* 0x71: MBOX_FABRIC_LOGOUT */
	ISP_FC_OPMAP(0x0f, 0x0f),	/* 0x72: MBOX_INIT_LIP_LOGIN */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x73: */
	ISP_FC_OPMAP(0x07, 0x01),	/* 0x74: LOGIN LOOP PORT */
	ISP_FC_OPMAP(0xcf, 0x03),	/* 0x75: GET PORT/NODE NAME LIST */
	ISP_FC_OPMAP(0x4f, 0x01),	/* 0x76: SET VENDOR ID */
	ISP_FC_OPMAP(0xcd, 0x01),	/* 0x77: INITIALIZE IP MAILBOX */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x78: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x79: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x7a: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x7b: */
	ISP_FC_OPMAP(0x4f, 0x03),	/* 0x7c: Get ID List */
	ISP_FC_OPMAP(0xcf, 0x01),	/* 0x7d: SEND LFA */
	ISP_FC_OPMAP(0x0f, 0x01)	/* 0x7e: LUN RESET */
};
#define	MAX_FC_OPCODE	0x7e
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
	"LOAD RAM (2100)",
	"DUMP RAM",
	"LOAD RISC RAM",
	NULL,
	"WRITE RAM WORD EXTENDED",
	"CHECK FIRMWARE",
	"READ RAM WORD EXTENDED",
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
	"INIT FIRMWARE MULTI ID",
	"GET VP DATABASE",
	"GET VP DATABASE ENTRY",
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

	opcode = mbp->param[0];
	if (IS_FC(isp)) {
		ibits = ISP_FC_IBITS(opcode);
		obits = ISP_FC_OBITS(opcode);
	} else {
		ibits = ISP_SCSI_IBITS(opcode);
		obits = ISP_SCSI_OBITS(opcode);
	}
	ibits |= mbp->ibits;
	obits |= mbp->obits;
	for (box = 0; box < ISP_NMBOX(isp); box++) {
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
		ISP_DELAY(1000);
	}
}

static void
isp_mboxcmd(ispsoftc_t *isp, mbreg_t *mbp)
{
	const char *cname, *xname;
	char tname[16], mname[16];
	unsigned int ibits, obits, box, opcode;

	opcode = mbp->param[0];
	if (IS_FC(isp)) {
		if (opcode > MAX_FC_OPCODE) {
			mbp->param[0] = MBOX_INVALID_COMMAND;
			isp_prt(isp, ISP_LOGERR, "Unknown Command 0x%x", opcode);
			return;
		}
		cname = fc_mbcmd_names[opcode];
		ibits = ISP_FC_IBITS(opcode);
		obits = ISP_FC_OBITS(opcode);
	} else {
		if (opcode > MAX_SCSI_OPCODE) {
			mbp->param[0] = MBOX_INVALID_COMMAND;
			isp_prt(isp, ISP_LOGERR, "Unknown Command 0x%x", opcode);
			return;
		}
		cname = scsi_mbcmd_names[opcode];
		ibits = ISP_SCSI_IBITS(opcode);
		obits = ISP_SCSI_OBITS(opcode);
	}
	if (cname == NULL) {
		cname = tname;
		ISP_SNPRINTF(tname, sizeof tname, "opcode %x", opcode);
	}
	isp_prt(isp, ISP_LOGDEBUG3, "Mailbox Command '%s'", cname);

	/*
	 * Pick up any additional bits that the caller might have set.
	 */
	ibits |= mbp->ibits;
	obits |= mbp->obits;

	/*
	 * Mask any bits that the caller wants us to mask
	 */
	ibits &= mbp->ibitm;
	obits &= mbp->obitm;


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

	for (box = 0; box < ISP_NMBOX(isp); box++) {
		if (ibits & (1 << box)) {
			isp_prt(isp, ISP_LOGDEBUG3, "IN mbox %d = 0x%04x", box,
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
		isp->isp_mboxbsy = 0;
		MBOX_RELEASE(isp);
		goto out;
	}

	/*
	 * Copy back output registers.
	 */
	for (box = 0; box < ISP_NMBOX(isp); box++) {
		if (obits & (1 << box)) {
			mbp->param[box] = isp->isp_mboxtmp[box];
			isp_prt(isp, ISP_LOGDEBUG3, "OUT mbox %d = 0x%04x", box,
			    mbp->param[box]);
		}
	}

	isp->isp_mboxbsy = 0;
	MBOX_RELEASE(isp);
 out:
	if (mbp->logval == 0 || opcode == MBOX_EXEC_FIRMWARE) {
		return;
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
		ISP_SNPRINTF(mname, sizeof mname, "error 0x%x", mbp->param[0]);
		xname = mname;
		break;
	}
	if (xname) {
		isp_prt(isp, ISP_LOGALL, "Mailbox Command '%s' failed (%s)",
		    cname, xname);
	}
}

static void
isp_fw_state(ispsoftc_t *isp, int chan)
{
	if (IS_FC(isp)) {
		mbreg_t mbs;
		fcparam *fcp = FCPARAM(isp, chan);

		MBSINIT(&mbs, MBOX_GET_FW_STATE, MBLOGALL, 0);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
			fcp->isp_fwstate = mbs.param[1];
		}
	}
}

static void
isp_spi_update(ispsoftc_t *isp, int chan)
{
	int tgt;
	mbreg_t mbs;
	sdparam *sdp;

	if (IS_FC(isp)) {
		/*
		 * There are no 'per-bus' settings for Fibre Channel.
		 */
		return;
	}
	sdp = SDPARAM(isp, chan);
	sdp->update = 0;

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		uint16_t flags, period, offset;
		int get;

		if (sdp->isp_devparam[tgt].dev_enable == 0) {
			sdp->isp_devparam[tgt].dev_update = 0;
			sdp->isp_devparam[tgt].dev_refresh = 0;
			isp_prt(isp, ISP_LOGDEBUG0, "skipping target %d bus %d update", tgt, chan);
			continue;
		}
		/*
		 * If the goal is to update the status of the device,
		 * take what's in goal_flags and try and set the device
		 * toward that. Otherwise, if we're just refreshing the
		 * current device state, get the current parameters.
		 */

		MBSINIT(&mbs, 0, MBLOGALL, 0);

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
			isp_prt(isp, ISP_LOGDEBUG0, "bus %d set tgt %d flags 0x%x off 0x%x period 0x%x",
			    chan, tgt, mbs.param[2], mbs.param[3] >> 8, mbs.param[3] & 0xff);
			get = 0;
		} else {
			continue;
		}
		mbs.param[1] = (chan << 15) | (tgt << 8);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			continue;
		}
		if (get == 0) {
			sdp->sendmarker = 1;
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
			isp_async(isp, ISPASYNC_NEW_TGT_PARAMS, chan, tgt);
		}
	}

	for (tgt = 0; tgt < MAX_TARGETS; tgt++) {
		if (sdp->isp_devparam[tgt].dev_update ||
		    sdp->isp_devparam[tgt].dev_refresh) {
			sdp->update = 1;
			break;
		}
	}
}

static void
isp_setdfltsdparm(ispsoftc_t *isp)
{
	int tgt;
	sdparam *sdp, *sdp1;

	sdp = SDPARAM(isp, 0);
	sdp->role = GET_DEFAULT_ROLE(isp, 0);
	if (IS_DUALBUS(isp)) {
		sdp1 = sdp + 1;
		sdp1->role = GET_DEFAULT_ROLE(isp, 1);
	} else {
		sdp1 = NULL;
	}

	/*
	 * Establish some default parameters.
	 */
	sdp->isp_cmd_dma_burst_enable = 0;
	sdp->isp_data_dma_burst_enabl = 1;
	sdp->isp_fifo_threshold = 0;
	sdp->isp_initiator_id = DEFAULT_IID(isp, 0);
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

	}

	/*
	 * If we're a dual bus card, just copy the data over
	 */
	if (sdp1) {
		*sdp1 = *sdp;
		sdp1->isp_initiator_id = DEFAULT_IID(isp, 1);
	}

	/*
	 * If we've not been told to avoid reading NVRAM, try and read it.
	 * If we're successful reading it, we can then return because NVRAM
	 * will tell us what the desired settings are. Otherwise, we establish
	 * some reasonable 'fake' nvram and goal defaults.
	 */
	if ((isp->isp_confopts & ISP_CFG_NONVRAM) == 0) {
		mbreg_t mbs;

		if (isp_read_nvram(isp, 0) == 0) {
			if (IS_DUALBUS(isp)) {
				if (isp_read_nvram(isp, 1) == 0) {
					return;
				}
			}
		}
		MBSINIT(&mbs, MBOX_GET_ACT_NEG_STATE, MBLOGNONE, 0);
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			sdp->isp_req_ack_active_neg = 1;
			sdp->isp_data_line_active_neg = 1;
			if (sdp1) {
				sdp1->isp_req_ack_active_neg = 1;
				sdp1->isp_data_line_active_neg = 1;
			}
		} else {
			sdp->isp_req_ack_active_neg =
			    (mbs.param[1] >> 4) & 0x1;
			sdp->isp_data_line_active_neg =
			    (mbs.param[1] >> 5) & 0x1;
			if (sdp1) {
				sdp1->isp_req_ack_active_neg =
				    (mbs.param[2] >> 4) & 0x1;
				sdp1->isp_data_line_active_neg =
				    (mbs.param[2] >> 5) & 0x1;
			}
		}
	}

}

static void
isp_setdfltfcparm(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);

	/*
	 * Establish some default parameters.
	 */
	fcp->role = GET_DEFAULT_ROLE(isp, chan);
	fcp->isp_maxalloc = ICB_DFLT_ALLOC;
	fcp->isp_retry_delay = ICB_DFLT_RDELAY;
	fcp->isp_retry_count = ICB_DFLT_RCOUNT;
	fcp->isp_loopid = DEFAULT_LOOPID(isp, chan);
	fcp->isp_wwnn_nvram = DEFAULT_NODEWWN(isp, chan);
	fcp->isp_wwpn_nvram = DEFAULT_PORTWWN(isp, chan);
	fcp->isp_fwoptions = 0;
	fcp->isp_lasthdl = NIL_HANDLE;

	if (IS_24XX(isp)) {
		fcp->isp_fwoptions |= ICB2400_OPT1_FAIRNESS;
		fcp->isp_fwoptions |= ICB2400_OPT1_HARD_ADDRESS;
		if (isp->isp_confopts & ISP_CFG_FULL_DUPLEX) {
			fcp->isp_fwoptions |= ICB2400_OPT1_FULL_DUPLEX;
		}
		fcp->isp_fwoptions |= ICB2400_OPT1_BOTH_WWNS;
	} else {
		fcp->isp_fwoptions |= ICBOPT_FAIRNESS;
		fcp->isp_fwoptions |= ICBOPT_PDBCHANGE_AE;
		fcp->isp_fwoptions |= ICBOPT_HARD_ADDRESS;
		if (isp->isp_confopts & ISP_CFG_FULL_DUPLEX) {
			fcp->isp_fwoptions |= ICBOPT_FULL_DUPLEX;
		}
		/*
		 * Make sure this is turned off now until we get
		 * extended options from NVRAM
		 */
		fcp->isp_fwoptions &= ~ICBOPT_EXTENDED;
	}


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
			j = isp_read_nvram(isp, chan);
			if (j == 0) {
				break;
			}
		}
		if (j) {
			isp->isp_confopts |= ISP_CFG_NONVRAM;
		}
	}

	fcp->isp_wwnn = ACTIVE_NODEWWN(isp, chan);
	fcp->isp_wwpn = ACTIVE_PORTWWN(isp, chan);
	isp_prt(isp, ISP_LOGCONFIG, "Chan %d 0x%08x%08x/0x%08x%08x Role %s",
	    chan, (uint32_t) (fcp->isp_wwnn >> 32), (uint32_t) (fcp->isp_wwnn),
	    (uint32_t) (fcp->isp_wwpn >> 32), (uint32_t) (fcp->isp_wwpn),
	    isp_class3_roles[fcp->role]);
}

/*
 * Re-initialize the ISP and complete all orphaned commands
 * with a 'botched' notice. The reset/init routines should
 * not disturb an already active list of commands.
 */

int
isp_reinit(ispsoftc_t *isp, int do_load_defaults)
{
	int i, res = 0;

	if (isp->isp_state != ISP_RESETSTATE)
		isp_reset(isp, do_load_defaults);
	if (isp->isp_state != ISP_RESETSTATE) {
		res = EIO;
		isp_prt(isp, ISP_LOGERR, "%s: cannot reset card", __func__);
		ISP_DISABLE_INTS(isp);
		goto cleanup;
	}

	isp_init(isp);
	if (isp->isp_state > ISP_RESETSTATE &&
	    isp->isp_state != ISP_RUNSTATE) {
		res = EIO;
		isp_prt(isp, ISP_LOGERR, "%s: cannot init card", __func__);
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

 cleanup:
	isp->isp_nactive = 0;
	isp_clear_commands(isp);
	if (IS_FC(isp)) {
		for (i = 0; i < isp->isp_nchan; i++)
			ISP_MARK_PORTDB(isp, i, -1);
	}
	return (res);
}

/*
 * NVRAM Routines
 */
static int
isp_read_nvram(ispsoftc_t *isp, int bus)
{
	int i, amt, retval;
	uint8_t csum, minversion;
	union {
		uint8_t _x[ISP2400_NVRAM_SIZE];
		uint16_t _s[ISP2400_NVRAM_SIZE>>1];
	} _n;
#define	nvram_data	_n._x
#define	nvram_words	_n._s

	if (IS_24XX(isp)) {
		return (isp_read_nvram_2400(isp, nvram_data));
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
			isp_prt(isp, ISP_LOGDEBUG0, "%x %x %x", nvram_data[0], nvram_data[1], nvram_data[2]);
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
		isp_parse_nvram_12160(isp, bus, nvram_data);
	} else if (IS_1080(isp)) {
		isp_parse_nvram_1080(isp, bus, nvram_data);
	} else if (IS_1280(isp) || IS_1240(isp)) {
		isp_parse_nvram_1080(isp, bus, nvram_data);
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
isp_read_nvram_2400(ispsoftc_t *isp, uint8_t *nvram_data)
{
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
	ISP_DELAY(10);
	ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT|BIU_NVRAM_CLOCK);
	ISP_DELAY(10);

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
		ISP_DELAY(10);
		junk = ISP_READ(isp, BIU_NVRAM);	/* force PCI flush */
		ISP_WRITE(isp, BIU_NVRAM, bit | BIU_NVRAM_CLOCK);
		ISP_DELAY(10);
		junk = ISP_READ(isp, BIU_NVRAM);	/* force PCI flush */
		ISP_WRITE(isp, BIU_NVRAM, bit);
		ISP_DELAY(10);
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
		ISP_DELAY(10);
		rv = ISP_READ(isp, BIU_NVRAM);
		if (rv & BIU_NVRAM_DATAIN) {
			*rp |= 1;
		}
		ISP_DELAY(10);
		ISP_WRITE(isp, BIU_NVRAM, BIU_NVRAM_SELECT);
		ISP_DELAY(10);
		junk = ISP_READ(isp, BIU_NVRAM);	/* force PCI flush */
	}
	ISP_WRITE(isp, BIU_NVRAM, 0);
	ISP_DELAY(10);
	junk = ISP_READ(isp, BIU_NVRAM);	/* force PCI flush */
	ISP_SWIZZLE_NVRAM_WORD(isp, rp);
}

static void
isp_rd_2400_nvram(ispsoftc_t *isp, uint32_t addr, uint32_t *rp)
{
	int loops = 0;
	uint32_t base = 0x7ffe0000;
	uint32_t tmp = 0;

	if (IS_25XX(isp)) {
		base = 0x7ff00000 | 0x48000;
	}
	ISP_WRITE(isp, BIU2400_FLASH_ADDR, base | addr);
	for (loops = 0; loops < 5000; loops++) {
		ISP_DELAY(10);
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
	sdparam *sdp = SDPARAM(isp, 0);
	int tgt;

	sdp->isp_fifo_threshold =
		ISP_NVRAM_FIFO_THRESHOLD(nvram_data) |
		(ISP_NVRAM_FIFO_THRESHOLD_128(nvram_data) << 2);

	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0)
		sdp->isp_initiator_id = ISP_NVRAM_INITIATOR_ID(nvram_data);

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
	sdparam *sdp = SDPARAM(isp, bus);
	int tgt;

	sdp->isp_fifo_threshold =
	    ISP1080_NVRAM_FIFO_THRESHOLD(nvram_data);

	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0)
		sdp->isp_initiator_id = ISP1080_NVRAM_INITIATOR_ID(nvram_data, bus);

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
	sdparam *sdp = SDPARAM(isp, bus);
	int tgt;

	sdp->isp_fifo_threshold =
	    ISP12160_NVRAM_FIFO_THRESHOLD(nvram_data);

	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0)
		sdp->isp_initiator_id = ISP12160_NVRAM_INITIATOR_ID(nvram_data, bus);

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
		sdp->isp_devparam[tgt].goal_offset =
		    sdp->isp_devparam[tgt].nvrm_offset;
		sdp->isp_devparam[tgt].goal_period =
		    sdp->isp_devparam[tgt].nvrm_period;
		sdp->isp_devparam[tgt].goal_flags =
		    sdp->isp_devparam[tgt].nvrm_flags;
	}
}

static void
isp_parse_nvram_2100(ispsoftc_t *isp, uint8_t *nvram_data)
{
	fcparam *fcp = FCPARAM(isp, 0);
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
		    (uint32_t) (wwn >> 32), (uint32_t) (wwn));
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
			    (uint32_t) (wwn));
			if ((wwn >> 60) == 0) {
				wwn |= (((uint64_t) 2)<< 60);
			}
		} else {
			wwn = fcp->isp_wwpn_nvram & ~((uint64_t) 0xfff << 48);
		}
	} else {
		wwn &= ~((uint64_t) 0xfff << 48);
	}
	fcp->isp_wwnn_nvram = wwn;

	fcp->isp_maxalloc = ISP2100_NVRAM_MAXIOCBALLOCATION(nvram_data);
	if ((isp->isp_confopts & ISP_CFG_OWNFSZ) == 0) {
		DEFAULT_FRAMESIZE(isp) =
		    ISP2100_NVRAM_MAXFRAMELENGTH(nvram_data);
	}
	fcp->isp_retry_delay = ISP2100_NVRAM_RETRY_DELAY(nvram_data);
	fcp->isp_retry_count = ISP2100_NVRAM_RETRY_COUNT(nvram_data);
	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0) {
		fcp->isp_loopid = ISP2100_NVRAM_HARDLOOPID(nvram_data);
	}
	if ((isp->isp_confopts & ISP_CFG_OWNEXCTHROTTLE) == 0) {
		DEFAULT_EXEC_THROTTLE(isp) =
			ISP2100_NVRAM_EXECUTION_THROTTLE(nvram_data);
	}
	fcp->isp_fwoptions = ISP2100_NVRAM_OPTIONS(nvram_data);
	isp_prt(isp, ISP_LOGDEBUG0,
	    "NVRAM 0x%08x%08x 0x%08x%08x maxalloc %d maxframelen %d",
	    (uint32_t) (fcp->isp_wwnn_nvram >> 32),
	    (uint32_t) fcp->isp_wwnn_nvram,
	    (uint32_t) (fcp->isp_wwpn_nvram >> 32),
	    (uint32_t) fcp->isp_wwpn_nvram,
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
	isp_prt(isp, ISP_LOGDEBUG0, "xfwoptions 0x%x zfw options 0x%x",
	    ISP2100_XFW_OPTIONS(nvram_data), ISP2100_ZFW_OPTIONS(nvram_data));
}

static void
isp_parse_nvram_2400(ispsoftc_t *isp, uint8_t *nvram_data)
{
	fcparam *fcp = FCPARAM(isp, 0);
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
	fcp->isp_wwpn_nvram = wwn;

	wwn = ISP2400_NVRAM_NODE_NAME(nvram_data);
	if (wwn) {
		if ((wwn >> 60) != 2 && (wwn >> 60) != 5) {
			wwn = 0;
		}
	}
	if (wwn == 0 && (fcp->isp_wwpn_nvram >> 60) == 2) {
		wwn = fcp->isp_wwpn_nvram;
		wwn &= ~((uint64_t) 0xfff << 48);
	}
	fcp->isp_wwnn_nvram = wwn;

	if (ISP2400_NVRAM_EXCHANGE_COUNT(nvram_data)) {
		fcp->isp_maxalloc = ISP2400_NVRAM_EXCHANGE_COUNT(nvram_data);
	}
	if ((isp->isp_confopts & ISP_CFG_OWNFSZ) == 0) {
		DEFAULT_FRAMESIZE(isp) =
		    ISP2400_NVRAM_MAXFRAMELENGTH(nvram_data);
	}
	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0) {
		fcp->isp_loopid = ISP2400_NVRAM_HARDLOOPID(nvram_data);
	}
	if ((isp->isp_confopts & ISP_CFG_OWNEXCTHROTTLE) == 0) {
		DEFAULT_EXEC_THROTTLE(isp) =
			ISP2400_NVRAM_EXECUTION_THROTTLE(nvram_data);
	}
	fcp->isp_fwoptions = ISP2400_NVRAM_FIRMWARE_OPTIONS1(nvram_data);
	fcp->isp_xfwoptions = ISP2400_NVRAM_FIRMWARE_OPTIONS2(nvram_data);
	fcp->isp_zfwoptions = ISP2400_NVRAM_FIRMWARE_OPTIONS3(nvram_data);
}
