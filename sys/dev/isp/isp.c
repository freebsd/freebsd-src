/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 *  Copyright (c) 2009-2020 Alexander Motin <mav@FreeBSD.org>
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
 * Local static data
 */
static const char notresp[] = "Unknown IOCB in RESPONSE Queue (type 0x%x) @ idx %d (next %d)";
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
static int isp_handle_control(ispsoftc_t *, isphdr_t *);
static void isp_handle_rpt_id_acq(ispsoftc_t *, isphdr_t *);
static void isp_parse_status_24xx(ispsoftc_t *, isp24xx_statusreq_t *, XS_T *);
static void isp_clear_portdb(ispsoftc_t *, int);
static void isp_mark_portdb(ispsoftc_t *, int);
static int isp_plogx(ispsoftc_t *, int, uint16_t, uint32_t, int);
static int isp_getpdb(ispsoftc_t *, int, uint16_t, isp_pdb_t *);
static int isp_gethandles(ispsoftc_t *, int, uint16_t *, int *, int);
static void isp_dump_chip_portdb(ispsoftc_t *, int);
static uint64_t isp_get_wwn(ispsoftc_t *, int, int, int);
static int isp_fclink_test(ispsoftc_t *, int, int);
static int isp_pdb_sync(ispsoftc_t *, int);
static int isp_scan_loop(ispsoftc_t *, int);
static int isp_gid_pt(ispsoftc_t *, int);
static int isp_scan_fabric(ispsoftc_t *, int);
static int isp_login_device(ispsoftc_t *, int, uint32_t, isp_pdb_t *, uint16_t *);
static int isp_register_fc4_type(ispsoftc_t *, int);
static int isp_register_fc4_features_24xx(ispsoftc_t *, int);
static int isp_register_port_name_24xx(ispsoftc_t *, int);
static int isp_register_node_name_24xx(ispsoftc_t *, int);
static uint16_t isp_next_handle(ispsoftc_t *, uint16_t *);
static int isp_fw_state(ispsoftc_t *, int);
static void isp_mboxcmd(ispsoftc_t *, mbreg_t *);

static void isp_setdfltfcparm(ispsoftc_t *, int);
static int isp_read_nvram(ispsoftc_t *, int);
static int isp_read_nvram_2400(ispsoftc_t *);
static void isp_rd_2400_nvram(ispsoftc_t *, uint32_t, uint32_t *);
static void isp_parse_nvram_2400(ispsoftc_t *, uint8_t *);

static void
isp_change_fw_state(ispsoftc_t *isp, int chan, int state)
{
	fcparam *fcp = FCPARAM(isp, chan);

	if (fcp->isp_fwstate == state)
		return;
	isp_prt(isp, ISP_LOGCONFIG|ISP_LOG_SANCFG,
	    "Chan %d Firmware state <%s->%s>", chan,
	    isp_fc_fw_statename(fcp->isp_fwstate), isp_fc_fw_statename(state));
	fcp->isp_fwstate = state;
}

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
	int loaded_fw, loops, i, dodnld = 1;
	const char *btype = "????";
	static const char dcrc[] = "Downloaded RISC Code Checksum Failure";

	isp->isp_state = ISP_NILSTATE;
	ISP_DISABLE_INTS(isp);

	/*
	 * Put the board into PAUSE mode (so we can read the SXP registers
	 * or write FPM/FBM registers).
	 */
	ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_HOST_INT);
	ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_RISC_INT);
	ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_PAUSE);

	switch (isp->isp_type) {
	case ISP_HA_FC_2400:
		btype = "2422";
		break;
	case ISP_HA_FC_2500:
		btype = "2532";
		break;
	case ISP_HA_FC_2600:
		btype = "2600";
		break;
	case ISP_HA_FC_2700:
		btype = "2700";
		break;
	default:
		break;
	}

	/*
	 * Stop DMA and wait for it to stop.
	 */
	ISP_WRITE(isp, BIU2400_CSR, BIU2400_DMA_STOP|(3 << 4));
	for (loops = 0; loops < 100000; loops++) {
		ISP_DELAY(10);
		val = ISP_READ(isp, BIU2400_CSR);
		if ((val & BIU2400_DMA_ACTIVE) == 0) {
			break;
		}
	}
	if (val & BIU2400_DMA_ACTIVE)
		isp_prt(isp, ISP_LOGERR, "DMA Failed to Stop on Reset");

	/*
	 * Hold it in SOFT_RESET and STOP state for 100us.
	 */
	ISP_WRITE(isp, BIU2400_CSR, BIU2400_SOFT_RESET|BIU2400_DMA_STOP|(3 << 4));
	ISP_DELAY(100);
	for (loops = 0; loops < 10000; loops++) {
		ISP_DELAY(5);
		val = ISP_READ(isp, OUTMAILBOX0);
		if (val != 0x4)
			break;
	}
	switch (val) {
	case 0x0:
		break;
	case 0x4:
		isp_prt(isp, ISP_LOGERR, "The ROM code is busy after 50ms.");
		return;
	case 0xf:
		isp_prt(isp, ISP_LOGERR, "Board configuration error.");
		return;
	default:
		isp_prt(isp, ISP_LOGERR, "Unknown RISC Status Code 0x%x.", val);
		return;
	}

	/*
	 * Reset RISC Processor
	 */
	ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_RESET);
	ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_RELEASE);
	ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_RESET);

	/*
	 * Post-RISC Reset stuff.
	 */
	for (loops = 0; loops < 10000; loops++) {
		ISP_DELAY(5);
		val = ISP_READ(isp, OUTMAILBOX0);
		if (val != 0x4)
			break;
	}
	switch (val) {
	case 0x0:
		break;
	case 0x4:
		isp_prt(isp, ISP_LOGERR, "The ROM code is busy after 50ms.");
		return;
	case 0xf:
		isp_prt(isp, ISP_LOGERR, "Board configuration error.");
		return;
	default:
		isp_prt(isp, ISP_LOGERR, "Unknown RISC Status Code 0x%x.", val);
		return;
	}

	isp->isp_reqidx = isp->isp_reqodx = 0;
	isp->isp_resodx = 0;
	isp->isp_atioodx = 0;
	ISP_WRITE(isp, BIU2400_REQINP, 0);
	ISP_WRITE(isp, BIU2400_REQOUTP, 0);
	ISP_WRITE(isp, BIU2400_RSPINP, 0);
	ISP_WRITE(isp, BIU2400_RSPOUTP, 0);
	if (!IS_26XX(isp)) {
		ISP_WRITE(isp, BIU2400_PRI_REQINP, 0);
		ISP_WRITE(isp, BIU2400_PRI_REQOUTP, 0);
	}
	ISP_WRITE(isp, BIU2400_ATIO_RSPINP, 0);
	ISP_WRITE(isp, BIU2400_ATIO_RSPOUTP, 0);

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
		return;
	}

	/*
	 * Do some operational tests
	 */
	{
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
		MBSINIT(&mbs, MBOX_MAILBOX_REG_TEST, MBLOGALL, 0);
		for (i = 1; i < nmbox; i++) {
			mbs.param[i] = patterns[i];
		}
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			return;
		}
		for (i = 1; i < nmbox; i++) {
			if (mbs.param[i] != patterns[i]) {
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
	} else {

		/*
		 * Set up DMA for the request and response queues.
		 * We do this now so we can use the request queue
		 * for dma to load firmware from.
		 */
		if (ISP_MBOXDMASETUP(isp) != 0) {
			isp_prt(isp, ISP_LOGERR, "Cannot setup DMA");
			return;
		}
	}

	code_org = ISP_CODE_ORG_2400;
	loaded_fw = 0;
	if (dodnld) {
		const uint32_t *ptr = isp->isp_mdvec->dv_ispfw;
		uint32_t la, wi, wl;

		/*
		 * Keep loading until we run out of f/w.
		 */
		code_org = ptr[2];	/* 1st load address is our start addr */
		for (;;) {
			isp_prt(isp, ISP_LOGDEBUG0, "load 0x%x words of code at load address 0x%x", ptr[3], ptr[2]);

			wi = 0;
			la = ptr[2];
			wl = ptr[3];
			while (wi < ptr[3]) {
				uint32_t *cp;
				uint32_t nw;

				nw = min(wl, ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp)) / 4);
				cp = isp->isp_rquest;
				for (i = 0; i < nw; i++)
					ISP_IOXPUT_32(isp, ptr[wi + i], &cp[i]);
				MEMORYBARRIER(isp, SYNC_REQUEST, 0, ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp)), -1);
				MBSINIT(&mbs, MBOX_LOAD_RISC_RAM, MBLOGALL, 0);
				mbs.param[1] = la;
				mbs.param[2] = DMA_WD1(isp->isp_rquest_dma);
				mbs.param[3] = DMA_WD0(isp->isp_rquest_dma);
				mbs.param[4] = nw >> 16;
				mbs.param[5] = nw;
				mbs.param[6] = DMA_WD3(isp->isp_rquest_dma);
				mbs.param[7] = DMA_WD2(isp->isp_rquest_dma);
				mbs.param[8] = la >> 16;
				isp_prt(isp, ISP_LOGDEBUG0, "LOAD RISC RAM %u words at load address 0x%x", nw, la);
				isp_mboxcmd(isp, &mbs);
				if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
					isp_prt(isp, ISP_LOGERR, "F/W download failed");
					return;
				}
				la += nw;
				wi += nw;
				wl -= nw;
			}

			if (ptr[1] == 0) {
				break;
			}
			ptr += ptr[3];
		}
		loaded_fw = 1;
	} else if (IS_26XX(isp)) {
		isp_prt(isp, ISP_LOGDEBUG1, "loading firmware from flash");
		MBSINIT(&mbs, MBOX_LOAD_FLASH_FIRMWARE, MBLOGALL, 5000000);
		mbs.ibitm = 0x01;
		mbs.obitm = 0x07;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGERR, "Flash F/W load failed");
			return;
		}
	} else {
		isp_prt(isp, ISP_LOGDEBUG2, "skipping f/w download");
	}

	/*
	 * If we loaded firmware, verify its checksum
	 */
	if (loaded_fw) {
		MBSINIT(&mbs, MBOX_VERIFY_CHECKSUM, MBLOGNONE, 0);
		mbs.param[1] = code_org >> 16;
		mbs.param[2] = code_org;
		isp_mboxcmd(isp, &mbs);
		if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGERR, dcrc);
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
	mbs.param[1] = code_org >> 16;
	mbs.param[2] = code_org;
	if (!IS_26XX(isp))
		mbs.param[3] = loaded_fw ? 0 : 1;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE)
		return;

	/*
	 * Ask the chip for the current firmware version.
	 * This should prove that the new firmware is working.
	 */
	MBSINIT(&mbs, MBOX_ABOUT_FIRMWARE, MBLOGALL, 5000000);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return;
	}

	isp->isp_fwrev[0] = mbs.param[1];
	isp->isp_fwrev[1] = mbs.param[2];
	isp->isp_fwrev[2] = mbs.param[3];
	isp->isp_fwattr = mbs.param[6];
	isp->isp_fwattr |= ((uint64_t) mbs.param[15]) << 16;
	if (isp->isp_fwattr & ISP2400_FW_ATTR_EXTNDED) {
		isp->isp_fwattr |=
		    (((uint64_t) mbs.param[16]) << 32) |
		    (((uint64_t) mbs.param[17]) << 48);
	}

	isp_prt(isp, ISP_LOGCONFIG, "Board Type %s, Chip Revision 0x%x, %s F/W Revision %d.%d.%d",
	    btype, isp->isp_revision, dodnld? "loaded" : "resident", isp->isp_fwrev[0], isp->isp_fwrev[1], isp->isp_fwrev[2]);

	fwt = isp->isp_fwattr;
	buf = FCPARAM(isp, 0)->isp_scanscratch;
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

	/*
	 * For the maximum number of commands take free exchange control block
	 * buffer count reported by firmware, limiting it to the maximum of our
	 * hardcoded handle format (16K now) minus some management reserve.
	 */
	MBSINIT(&mbs, MBOX_GET_RESOURCE_COUNT, MBLOGALL, 0);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE)
		return;
	isp->isp_maxcmds = MIN(mbs.param[3], ISP_HANDLE_MAX - ISP_HANDLE_RESERVE);
	isp_prt(isp, ISP_LOGCONFIG, "%d max I/O command limit set", isp->isp_maxcmds);

	/*
	 * If we don't have Multi-ID f/w loaded, we need to restrict channels to one.
	 * Only make this check for non-SCSI cards (I'm not sure firmware attributes
	 * work for them).
	 */
	if (isp->isp_nchan > 1) {
		if (!ISP_CAP_MULTI_ID(isp)) {
			isp_prt(isp, ISP_LOGWARN, "non-MULTIID f/w loaded, "
			    "only can enable 1 of %d channels", isp->isp_nchan);
			isp->isp_nchan = 1;
		} else if (!ISP_CAP_VP0(isp)) {
			isp_prt(isp, ISP_LOGWARN, "We can not use MULTIID "
			    "feature properly without VP0_Decoupling");
			isp->isp_nchan = 1;
		}
	}

	/*
	 * Final DMA setup after we got isp_maxcmds.
	 */
	if (ISP_MBOXDMASETUP(isp) != 0) {
		isp_prt(isp, ISP_LOGERR, "Cannot setup DMA");
		return;
	}

	/*
	 * Setup interrupts.
	 */
	if (ISP_IRQSETUP(isp) != 0) {
		isp_prt(isp, ISP_LOGERR, "Cannot setup IRQ");
		return;
	}
	ISP_ENABLE_INTS(isp);

	for (i = 0; i < isp->isp_nchan; i++)
		isp_change_fw_state(isp, i, FW_CONFIG_WAIT);

	isp->isp_state = ISP_RESETSTATE;

	/*
	 * We get some default values established. As a side
	 * effect, NVRAM is read here (unless overriden by
	 * a configuration flag).
	 */
	if (do_load_defaults) {
		for (i = 0; i < isp->isp_nchan; i++)
			isp_setdfltfcparm(isp, i);
	}
}

/*
 * Clean firmware shutdown.
 */
static int
isp_stop(ispsoftc_t *isp)
{
	mbreg_t mbs;

	isp->isp_state = ISP_NILSTATE;
	MBSINIT(&mbs, MBOX_STOP_FIRMWARE, MBLOGALL, 500000);
	mbs.param[1] = 0;
	mbs.param[2] = 0;
	mbs.param[3] = 0;
	mbs.param[4] = 0;
	mbs.param[5] = 0;
	mbs.param[6] = 0;
	mbs.param[7] = 0;
	mbs.param[8] = 0;
	isp_mboxcmd(isp, &mbs);
	return (mbs.param[0] == MBOX_COMMAND_COMPLETE ? 0 : mbs.param[0]);
}

/*
 * Hardware shutdown.
 */
void
isp_shutdown(ispsoftc_t *isp)
{

	if (isp->isp_state >= ISP_RESETSTATE)
		isp_stop(isp);
	ISP_DISABLE_INTS(isp);
	ISP_WRITE(isp, BIU2400_ICR, 0);
	ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_PAUSE);
}

/*
 * Initialize Parameters of Hardware to a known state.
 *
 * Locks are held before coming here.
 */
void
isp_init(ispsoftc_t *isp)
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
	icbp->icb_fwoptions2 = fcp->isp_xfwoptions;
	icbp->icb_fwoptions3 = fcp->isp_zfwoptions;
	if (isp->isp_nchan > 1 && ISP_CAP_VP0(isp)) {
		icbp->icb_fwoptions1 &= ~ICB2400_OPT1_INI_DISABLE;
		icbp->icb_fwoptions1 |= ICB2400_OPT1_TGT_ENABLE;
	} else {
		if (fcp->role & ISP_ROLE_TARGET)
			icbp->icb_fwoptions1 |= ICB2400_OPT1_TGT_ENABLE;
		else
			icbp->icb_fwoptions1 &= ~ICB2400_OPT1_TGT_ENABLE;
		if (fcp->role & ISP_ROLE_INITIATOR)
			icbp->icb_fwoptions1 &= ~ICB2400_OPT1_INI_DISABLE;
		else
			icbp->icb_fwoptions1 |= ICB2400_OPT1_INI_DISABLE;
	}

	icbp->icb_version = ICB_VERSION1;
	icbp->icb_maxfrmlen = DEFAULT_FRAMESIZE(isp);
	if (icbp->icb_maxfrmlen < ICB_MIN_FRMLEN || icbp->icb_maxfrmlen > ICB_MAX_FRMLEN) {
		isp_prt(isp, ISP_LOGERR, "bad frame length (%d) from NVRAM- using %d", DEFAULT_FRAMESIZE(isp), ICB_DFLT_FRMLEN);
		icbp->icb_maxfrmlen = ICB_DFLT_FRMLEN;
	}

	if (!IS_26XX(isp))
		icbp->icb_execthrottle = 0xffff;

#ifdef	ISP_TARGET_MODE
	/*
	 * Set target exchange count. Take half if we are supporting both roles.
	 */
	if (icbp->icb_fwoptions1 & ICB2400_OPT1_TGT_ENABLE) {
		if ((icbp->icb_fwoptions1 & ICB2400_OPT1_INI_DISABLE) == 0)
			icbp->icb_xchgcnt = MIN(isp->isp_maxcmds / 2, ATPDPSIZE);
		else
			icbp->icb_xchgcnt = isp->isp_maxcmds;
	}
#endif

	ownloopid = (isp->isp_confopts & ISP_CFG_OWNLOOPID) != 0;
	icbp->icb_hardaddr = fcp->isp_loopid;
	if (icbp->icb_hardaddr >= LOCAL_LOOP_LIM) {
		icbp->icb_hardaddr = 0;
		ownloopid = 0;
	}

	if (ownloopid)
		icbp->icb_fwoptions1 |= ICB2400_OPT1_HARD_ADDRESS;

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
	case ISP_CFG_LPORT_ONLY:
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TOPO_MASK;
		icbp->icb_fwoptions2 |= ICB2400_OPT2_LOOP_ONLY;
		break;
	case ISP_CFG_NPORT_ONLY:
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TOPO_MASK;
		icbp->icb_fwoptions2 |= ICB2400_OPT2_PTP_ONLY;
		break;
	case ISP_CFG_NPORT:
		/* ISP_CFG_PTP_2_LOOP not available in 24XX/25XX */
	case ISP_CFG_LPORT:
		icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TOPO_MASK;
		icbp->icb_fwoptions2 |= ICB2400_OPT2_LOOP_2_PTP;
		break;
	default:
		/* Let NVRAM settings define it if they are sane */
		switch (icbp->icb_fwoptions2 & ICB2400_OPT2_TOPO_MASK) {
		case ICB2400_OPT2_LOOP_ONLY:
		case ICB2400_OPT2_PTP_ONLY:
		case ICB2400_OPT2_LOOP_2_PTP:
			break;
		default:
			icbp->icb_fwoptions2 &= ~ICB2400_OPT2_TOPO_MASK;
			icbp->icb_fwoptions2 |= ICB2400_OPT2_LOOP_2_PTP;
		}
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

	if (IS_26XX(isp)) {
		/* Use handshake to reduce global lock congestion. */
		icbp->icb_fwoptions2 |= ICB2400_OPT2_ENA_IHR;
		icbp->icb_fwoptions2 |= ICB2400_OPT2_ENA_IHA;
	}

	if ((icbp->icb_fwoptions3 & ICB2400_OPT3_RSPSZ_MASK) == 0) {
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RSPSZ_24;
	}
	if (isp->isp_confopts & ISP_CFG_1GB) {
		icbp->icb_fwoptions3 &= ~ICB2400_OPT3_RATE_MASK;
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_1GB;
	} else if (isp->isp_confopts & ISP_CFG_2GB) {
		icbp->icb_fwoptions3 &= ~ICB2400_OPT3_RATE_MASK;
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_2GB;
	} else if (isp->isp_confopts & ISP_CFG_4GB) {
		icbp->icb_fwoptions3 &= ~ICB2400_OPT3_RATE_MASK;
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_4GB;
	} else if (isp->isp_confopts & ISP_CFG_8GB) {
		icbp->icb_fwoptions3 &= ~ICB2400_OPT3_RATE_MASK;
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_8GB;
	} else if (isp->isp_confopts & ISP_CFG_16GB) {
		icbp->icb_fwoptions3 &= ~ICB2400_OPT3_RATE_MASK;
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_16GB;
	} else if (isp->isp_confopts & ISP_CFG_32GB) {
		icbp->icb_fwoptions3 &= ~ICB2400_OPT3_RATE_MASK;
		icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_32GB;
	} else {
		switch (icbp->icb_fwoptions3 & ICB2400_OPT3_RATE_MASK) {
		case ICB2400_OPT3_RATE_4GB:
		case ICB2400_OPT3_RATE_8GB:
		case ICB2400_OPT3_RATE_16GB:
		case ICB2400_OPT3_RATE_32GB:
		case ICB2400_OPT3_RATE_AUTO:
			break;
		case ICB2400_OPT3_RATE_2GB:
			if (isp->isp_type <= ISP_HA_FC_2500)
				break;
			/*FALLTHROUGH*/
		case ICB2400_OPT3_RATE_1GB:
			if (isp->isp_type <= ISP_HA_FC_2400)
				break;
			/*FALLTHROUGH*/
		default:
			icbp->icb_fwoptions3 &= ~ICB2400_OPT3_RATE_MASK;
			icbp->icb_fwoptions3 |= ICB2400_OPT3_RATE_AUTO;
			break;
		}
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
	icbp->icb_rspnsin = isp->isp_resodx;
	icbp->icb_rqstout = isp->isp_reqidx;
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
	icbp->icb_atio_in = isp->isp_atioodx;
	icbp->icb_atioqlen = ATIO_QUEUE_LEN(isp);
	if (icbp->icb_atioqlen < 8) {
		isp_prt(isp, ISP_LOGERR, "bad ATIO queue length %d", icbp->icb_atioqlen);
		return;
	}
	icbp->icb_atioqaddr[RQRSP_ADDR0015] = DMA_WD0(isp->isp_atioq_dma);
	icbp->icb_atioqaddr[RQRSP_ADDR1631] = DMA_WD1(isp->isp_atioq_dma);
	icbp->icb_atioqaddr[RQRSP_ADDR3247] = DMA_WD2(isp->isp_atioq_dma);
	icbp->icb_atioqaddr[RQRSP_ADDR4863] = DMA_WD3(isp->isp_atioq_dma);
	isp_prt(isp, ISP_LOGDEBUG0, "isp_init: atioq %04x%04x%04x%04x", DMA_WD3(isp->isp_atioq_dma), DMA_WD2(isp->isp_atioq_dma),
	    DMA_WD1(isp->isp_atioq_dma), DMA_WD0(isp->isp_atioq_dma));
#endif

	if (ISP_CAP_MSIX(isp) && isp->isp_nirq >= 2) {
		icbp->icb_msixresp = 1;
		if (IS_26XX(isp) && isp->isp_nirq >= 3)
			icbp->icb_msixatio = 2;
	}

	isp_prt(isp, ISP_LOGDEBUG0, "isp_init: fwopt1 0x%x fwopt2 0x%x fwopt3 0x%x", icbp->icb_fwoptions1, icbp->icb_fwoptions2, icbp->icb_fwoptions3);

	isp_prt(isp, ISP_LOGDEBUG0, "isp_init: rqst %04x%04x%04x%04x rsp %04x%04x%04x%04x", DMA_WD3(isp->isp_rquest_dma), DMA_WD2(isp->isp_rquest_dma),
	    DMA_WD1(isp->isp_rquest_dma), DMA_WD0(isp->isp_rquest_dma), DMA_WD3(isp->isp_result_dma), DMA_WD2(isp->isp_result_dma),
	    DMA_WD1(isp->isp_result_dma), DMA_WD0(isp->isp_result_dma));

	if (FC_SCRATCH_ACQUIRE(isp, 0)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return;
	}
	ISP_MEMZERO(fcp->isp_scratch, ISP_FC_SCRLEN);
	isp_put_icb_2400(isp, icbp, fcp->isp_scratch);
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "isp_init",
		    sizeof (*icbp), fcp->isp_scratch);
	}

	/*
	 * Now fill in information about any additional channels
	 */
	if (isp->isp_nchan > 1) {
		isp_icb_2400_vpinfo_t vpinfo, *vdst;
		vp_port_info_t pi, *pdst;
		size_t amt = 0;
		uint8_t *off;

		vpinfo.vp_global_options = ICB2400_VPGOPT_GEN_RIDA;
		if (ISP_CAP_VP0(isp)) {
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
				pi.vp_port_options = ICB2400_VPOPT_ENABLED |
				    ICB2400_VPOPT_ENA_SNSLOGIN;
				if (fcp2->role & ISP_ROLE_INITIATOR)
					pi.vp_port_options |= ICB2400_VPOPT_INI_ENABLE;
				if ((fcp2->role & ISP_ROLE_TARGET) == 0)
					pi.vp_port_options |= ICB2400_VPOPT_TGT_DISABLE;
				if (fcp2->isp_loopid < LOCAL_LOOP_LIM) {
					pi.vp_port_loopid = fcp2->isp_loopid;
					if (isp->isp_confopts & ISP_CFG_OWNLOOPID)
						pi.vp_port_options |= ICB2400_VPOPT_HARD_ADDRESS;
				}

			}
			MAKE_NODE_NAME_FROM_WWN(pi.vp_port_portname, fcp2->isp_wwpn);
			MAKE_NODE_NAME_FROM_WWN(pi.vp_port_nodename, fcp2->isp_wwnn);
			off = fcp->isp_scratch;
			if (ISP_CAP_VP0(isp))
				off += ICB2400_VPINFO_PORT_OFF(chan);
			else
				off += ICB2400_VPINFO_PORT_OFF(chan - 1);
			pdst = (vp_port_info_t *) off;
			isp_put_vp_port_info(isp, &pi, pdst);
			amt += ICB2400_VPOPT_WRITE_SIZE;
		}
		if (isp->isp_dblev & ISP_LOGDEBUG1) {
			isp_print_bytes(isp, "isp_init",
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

	/*
	 * Whatever happens, we're now committed to being here.
	 */
	isp->isp_state = ISP_RUNSTATE;
}

static int
isp_fc_enable_vp(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	vp_modify_t vp;
	int retval;

	/* Build a VP MODIFY command in memory */
	ISP_MEMZERO(&vp, sizeof(vp));
	vp.vp_mod_hdr.rqs_entry_type = RQSTYPE_VP_MODIFY;
	vp.vp_mod_hdr.rqs_entry_count = 1;
	vp.vp_mod_cnt = 1;
	vp.vp_mod_idx0 = chan;
	vp.vp_mod_cmd = VP_MODIFY_ENA;
	vp.vp_mod_ports[0].options = ICB2400_VPOPT_ENABLED |
	    ICB2400_VPOPT_ENA_SNSLOGIN;
	if (fcp->role & ISP_ROLE_INITIATOR)
		vp.vp_mod_ports[0].options |= ICB2400_VPOPT_INI_ENABLE;
	if ((fcp->role & ISP_ROLE_TARGET) == 0)
		vp.vp_mod_ports[0].options |= ICB2400_VPOPT_TGT_DISABLE;
	if (fcp->isp_loopid < LOCAL_LOOP_LIM) {
		vp.vp_mod_ports[0].loopid = fcp->isp_loopid;
		if (isp->isp_confopts & ISP_CFG_OWNLOOPID)
			vp.vp_mod_ports[0].options |= ICB2400_VPOPT_HARD_ADDRESS;
	}
	MAKE_NODE_NAME_FROM_WWN(vp.vp_mod_ports[0].wwpn, fcp->isp_wwpn);
	MAKE_NODE_NAME_FROM_WWN(vp.vp_mod_ports[0].wwnn, fcp->isp_wwnn);

	retval = isp_exec_entry_queue(isp, &vp, &vp, 5);
	if (retval != 0) {
		isp_prt(isp, ISP_LOGERR, "%s: VP_MODIFY of chan %d error %d",
		    __func__, chan, retval);
		return (retval);
	}

	if (vp.vp_mod_hdr.rqs_flags != 0 || vp.vp_mod_status != VP_STS_OK) {
		isp_prt(isp, ISP_LOGERR,
		    "%s: VP_MODIFY of Chan %d failed with flags %x status %d",
		    __func__, chan, vp.vp_mod_hdr.rqs_flags, vp.vp_mod_status);
		return (EIO);
	}
	return (0);
}

static int
isp_fc_disable_vp(ispsoftc_t *isp, int chan)
{
	vp_ctrl_info_t vp;
	int retval;

	/* Build a VP CTRL command in memory */
	ISP_MEMZERO(&vp, sizeof(vp));
	vp.vp_ctrl_hdr.rqs_entry_type = RQSTYPE_VP_CTRL;
	vp.vp_ctrl_hdr.rqs_entry_count = 1;
	if (ISP_CAP_VP0(isp)) {
		vp.vp_ctrl_status = 1;
	} else {
		vp.vp_ctrl_status = 0;
		chan--;	/* VP0 can not be controlled in this case. */
	}
	vp.vp_ctrl_command = VP_CTRL_CMD_DISABLE_VP_LOGO_ALL;
	vp.vp_ctrl_vp_count = 1;
	vp.vp_ctrl_idmap[chan / 16] |= (1 << chan % 16);

	retval = isp_exec_entry_queue(isp, &vp, &vp, 5);
	if (retval != 0) {
		isp_prt(isp, ISP_LOGERR, "%s: VP_CTRL of chan %d error %d",
		    __func__, chan, retval);
		return (retval);
	}

	if (vp.vp_ctrl_hdr.rqs_flags != 0 || vp.vp_ctrl_status != 0) {
		isp_prt(isp, ISP_LOGERR,
		    "%s: VP_CTRL of Chan %d failed with flags %x status %d %d",
		    __func__, chan, vp.vp_ctrl_hdr.rqs_flags,
		    vp.vp_ctrl_status, vp.vp_ctrl_index_fail);
		return (EIO);
	}
	return (0);
}

static int
isp_fc_change_role(ispsoftc_t *isp, int chan, int new_role)
{
	fcparam *fcp = FCPARAM(isp, chan);
	int i, was, res = 0;

	if (chan >= isp->isp_nchan) {
		isp_prt(isp, ISP_LOGWARN, "%s: bad channel %d", __func__, chan);
		return (ENXIO);
	}
	if (fcp->role == new_role)
		return (0);
	for (was = 0, i = 0; i < isp->isp_nchan; i++) {
		if (FCPARAM(isp, i)->role != ISP_ROLE_NONE)
			was++;
	}
	if (was == 0 || (was == 1 && fcp->role != ISP_ROLE_NONE)) {
		fcp->role = new_role;
		return (isp_reinit(isp, 0));
	}
	if (fcp->role != ISP_ROLE_NONE) {
		res = isp_fc_disable_vp(isp, chan);
		isp_clear_portdb(isp, chan);
	}
	fcp->role = new_role;
	if (fcp->role != ISP_ROLE_NONE)
		res = isp_fc_enable_vp(isp, chan);
	return (res);
}

static void
isp_clear_portdb(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	fcportdb_t *lp;
	int i;

	for (i = 0; i < MAX_FC_TARG; i++) {
		lp = &fcp->portdb[i];
		switch (lp->state) {
		case FC_PORTDB_STATE_DEAD:
		case FC_PORTDB_STATE_CHANGED:
		case FC_PORTDB_STATE_VALID:
			lp->state = FC_PORTDB_STATE_NIL;
			isp_async(isp, ISPASYNC_DEV_GONE, chan, lp);
			break;
		case FC_PORTDB_STATE_NIL:
		case FC_PORTDB_STATE_NEW:
			lp->state = FC_PORTDB_STATE_NIL;
			break;
		case FC_PORTDB_STATE_ZOMBIE:
			break;
		default:
			panic("Don't know how to clear state %d\n", lp->state);
		}
	}
}

static void
isp_mark_portdb(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	fcportdb_t *lp;
	int i;

	for (i = 0; i < MAX_FC_TARG; i++) {
		lp = &fcp->portdb[i];
		if (lp->state == FC_PORTDB_STATE_NIL)
			continue;
		if (lp->portid >= DOMAIN_CONTROLLER_BASE &&
		    lp->portid <= DOMAIN_CONTROLLER_END)
			continue;
		fcp->portdb[i].probational = 1;
	}
}

/*
 * Perform an IOCB PLOGI or LOGO via EXECUTE IOCB A64 for 24XX cards
 * or via FABRIC LOGIN/FABRIC LOGOUT for other cards.
 */
static int
isp_plogx(ispsoftc_t *isp, int chan, uint16_t handle, uint32_t portid, int flags)
{
	isp_plogx_t pl;
	uint32_t sst, parm1;
	int retval, lev;
	const char *msg;
	char buf[64];

	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d PLOGX %s PortID 0x%06x nphdl 0x%x",
	    chan, (flags & PLOGX_FLG_CMD_MASK) == PLOGX_FLG_CMD_PLOGI ?
	    "Login":"Logout", portid, handle);

	ISP_MEMZERO(&pl, sizeof(pl));
	pl.plogx_header.rqs_entry_count = 1;
	pl.plogx_header.rqs_entry_type = RQSTYPE_LOGIN;
	pl.plogx_nphdl = handle;
	pl.plogx_vphdl = chan;
	pl.plogx_portlo = portid;
	pl.plogx_rspsz_porthi = (portid >> 16) & 0xff;
	pl.plogx_flags = flags;

	retval = isp_exec_entry_queue(isp, &pl, &pl, 3 * ICB_LOGIN_TOV);
	if (retval != 0) {
		isp_prt(isp, ISP_LOGERR, "%s: PLOGX of chan %d error %d",
		    __func__, chan, retval);
		return (retval);
	}

	if (pl.plogx_status == PLOGX_STATUS_OK) {
		return (0);
	} else if (pl.plogx_status != PLOGX_STATUS_IOCBERR) {
		isp_prt(isp, ISP_LOGWARN,
		    "status 0x%x on port login IOCB channel %d",
		    pl.plogx_status, chan);
		return (-1);
	}

	sst = pl.plogx_ioparm[0].lo16 | (pl.plogx_ioparm[0].hi16 << 16);
	parm1 = pl.plogx_ioparm[1].lo16 | (pl.plogx_ioparm[1].hi16 << 16);

	retval = -1;
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
		retval = MBOX_NOT_LOGGED_IN;
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
		retval = MBOX_PORT_ID_USED | (parm1 << 16);
		break;
	case PLOGX_IOCBERR_HNDLUSED:
		lev = ISP_LOG_SANCFG|ISP_LOG_WARN1;
		ISP_SNPRINTF(buf, sizeof (buf), "handle already used for PortID 0x%06x", parm1);
		msg = buf;
		retval = MBOX_LOOP_ID_USED;
		break;
	case PLOGX_IOCBERR_NOHANDLE:
		msg = "no handle allocated";
		break;
	case PLOGX_IOCBERR_NOFLOGI:
		msg = "no FLOGI_ACC";
		break;
	default:
		ISP_SNPRINTF(buf, sizeof (buf), "status %x from %x", pl.plogx_status, flags);
		msg = buf;
		break;
	}
	if (msg) {
		isp_prt(isp, lev, "Chan %d PLOGX PortID 0x%06x to N-Port handle 0x%x: %s",
		    chan, portid, handle, msg);
	}
	return (retval);
}

static int
isp_getpdb(ispsoftc_t *isp, int chan, uint16_t id, isp_pdb_t *pdb)
{
	mbreg_t mbs;
	union {
		isp_pdb_24xx_t bill;
	} un;

	MBSINIT(&mbs, MBOX_GET_PORT_DB,
	    MBLOGALL & ~MBLOGMASK(MBOX_COMMAND_PARAM_ERROR), 250000);
	mbs.ibits = (1 << 9)|(1 << 10);
	mbs.param[1] = id;
	mbs.param[2] = DMA_WD1(isp->isp_iocb_dma);
	mbs.param[3] = DMA_WD0(isp->isp_iocb_dma);
	mbs.param[6] = DMA_WD3(isp->isp_iocb_dma);
	mbs.param[7] = DMA_WD2(isp->isp_iocb_dma);
	mbs.param[9] = chan;
	MEMORYBARRIER(isp, SYNC_IFORDEV, 0, sizeof(un), chan);

	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE)
		return (mbs.param[0] | (mbs.param[1] << 16));

	MEMORYBARRIER(isp, SYNC_IFORCPU, 0, sizeof(un), chan);
	isp_get_pdb_24xx(isp, isp->isp_iocb, &un.bill);
	pdb->handle = un.bill.pdb_handle;
	pdb->prli_word0 = un.bill.pdb_prli_svc0;
	pdb->prli_word3 = un.bill.pdb_prli_svc3;
	pdb->portid = BITS2WORD_24XX(un.bill.pdb_portid_bits);
	ISP_MEMCPY(pdb->portname, un.bill.pdb_portname, 8);
	ISP_MEMCPY(pdb->nodename, un.bill.pdb_nodename, 8);
	isp_prt(isp, ISP_LOGDEBUG0,
	    "Chan %d handle 0x%x Port 0x%06x flags 0x%x curstate %x laststate %x",
	    chan, id, pdb->portid, un.bill.pdb_flags,
	    un.bill.pdb_curstate, un.bill.pdb_laststate);

	if (un.bill.pdb_curstate < PDB2400_STATE_PLOGI_DONE || un.bill.pdb_curstate > PDB2400_STATE_LOGGED_IN) {
		mbs.param[0] = MBOX_NOT_LOGGED_IN;
		return (mbs.param[0]);
	}
	return (0);
}

static int
isp_gethandles(ispsoftc_t *isp, int chan, uint16_t *handles, int *num, int loop)
{
	fcparam *fcp = FCPARAM(isp, chan);
	mbreg_t mbs;
	isp_pnhle_24xx_t el4, *elp4;
	int i, j;
	uint32_t p;

	MBSINIT(&mbs, MBOX_GET_ID_LIST, MBLOGALL, 250000);
	mbs.param[2] = DMA_WD1(fcp->isp_scdma);
	mbs.param[3] = DMA_WD0(fcp->isp_scdma);
	mbs.param[6] = DMA_WD3(fcp->isp_scdma);
	mbs.param[7] = DMA_WD2(fcp->isp_scdma);
	mbs.param[8] = ISP_FC_SCRLEN;
	mbs.param[9] = chan;
	if (FC_SCRATCH_ACQUIRE(isp, chan)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return (-1);
	}
	MEMORYBARRIER(isp, SYNC_SFORDEV, 0, ISP_FC_SCRLEN, chan);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		FC_SCRATCH_RELEASE(isp, chan);
		return (mbs.param[0] | (mbs.param[1] << 16));
	}
	MEMORYBARRIER(isp, SYNC_SFORCPU, 0, ISP_FC_SCRLEN, chan);
	elp4 = fcp->isp_scratch;
	for (i = 0, j = 0; i < mbs.param[1] && j < *num; i++) {
		isp_get_pnhle_24xx(isp, &elp4[i], &el4);
		p = el4.pnhle_port_id_lo | (el4.pnhle_port_id_hi << 16);
		if (loop && (p >> 8) != (fcp->isp_portid >> 8))
			continue;
		handles[j++] = el4.pnhle_handle;
	}
	*num = j;
	FC_SCRATCH_RELEASE(isp, chan);
	return (0);
}

static void
isp_dump_chip_portdb(ispsoftc_t *isp, int chan)
{
	isp_pdb_t pdb;
	uint16_t nphdl;

	isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGINFO, "Chan %d chip port dump", chan);
	for (nphdl = 0; nphdl != NPH_MAX_2K; nphdl++) {
		if (isp_getpdb(isp, chan, nphdl, &pdb)) {
			continue;
		}
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGINFO, "Chan %d Handle 0x%04x "
		    "PortID 0x%06x WWPN 0x%02x%02x%02x%02x%02x%02x%02x%02x",
		    chan, nphdl, pdb.portid, pdb.portname[0], pdb.portname[1],
		    pdb.portname[2], pdb.portname[3], pdb.portname[4],
		    pdb.portname[5], pdb.portname[6], pdb.portname[7]);
	}
}

static uint64_t
isp_get_wwn(ispsoftc_t *isp, int chan, int nphdl, int nodename)
{
	uint64_t wwn = INI_NONE;
	mbreg_t mbs;

	MBSINIT(&mbs, MBOX_GET_PORT_NAME,
	    MBLOGALL & ~MBLOGMASK(MBOX_COMMAND_PARAM_ERROR), 500000);
	mbs.param[1] = nphdl;
	if (nodename)
		mbs.param[10] = 1;
	mbs.param[9] = chan;
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] != MBOX_COMMAND_COMPLETE) {
		return (wwn);
	}
	wwn = (((uint64_t)(mbs.param[2] >> 8))	<< 56) |
	      (((uint64_t)(mbs.param[2] & 0xff))<< 48) |
	      (((uint64_t)(mbs.param[3] >> 8))	<< 40) |
	      (((uint64_t)(mbs.param[3] & 0xff))<< 32) |
	      (((uint64_t)(mbs.param[6] >> 8))	<< 24) |
	      (((uint64_t)(mbs.param[6] & 0xff))<< 16) |
	      (((uint64_t)(mbs.param[7] >> 8))	<<  8) |
	      (((uint64_t)(mbs.param[7] & 0xff)));
	return (wwn);
}

/*
 * Make sure we have good FC link.
 */

static int
isp_fclink_test(ispsoftc_t *isp, int chan, int usdelay)
{
	mbreg_t mbs;
	int i, r, topo;
	fcparam *fcp;
	isp_pdb_t pdb;
	NANOTIME_T hra, hrb;

	fcp = FCPARAM(isp, chan);

	if (fcp->isp_loopstate < LOOP_HAVE_LINK)
		return (-1);
	if (fcp->isp_loopstate >= LOOP_LTEST_DONE)
		return (0);

	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC link test", chan);

	/*
	 * Wait up to N microseconds for F/W to go to a ready state.
	 */
	GET_NANOTIME(&hra);
	while (1) {
		isp_change_fw_state(isp, chan, isp_fw_state(isp, chan));
		if (fcp->isp_fwstate == FW_READY) {
			break;
		}
		if (fcp->isp_loopstate < LOOP_HAVE_LINK)
			goto abort;
		GET_NANOTIME(&hrb);
		if ((NANOTIME_SUB(&hrb, &hra) / 1000 + 1000 >= usdelay))
			break;
		ISP_SLEEP(isp, 1000);
	}
	if (fcp->isp_fwstate != FW_READY) {
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d Firmware is not ready (%s)",
		    chan, isp_fc_fw_statename(fcp->isp_fwstate));
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

	topo = (int) mbs.param[6];
	if (topo < TOPO_NL_PORT || topo > TOPO_PTP_STUB)
		topo = TOPO_PTP_STUB;
	fcp->isp_topo = topo;
	fcp->isp_portid = mbs.param[2] | (mbs.param[3] << 16);

	if (!TOPO_IS_FABRIC(fcp->isp_topo)) {
		fcp->isp_loopid = mbs.param[1] & 0xff;
	} else if (fcp->isp_topo != TOPO_F_PORT) {
		uint8_t alpa = fcp->isp_portid;

		for (i = 0; alpa_map[i]; i++) {
			if (alpa_map[i] == alpa)
				break;
		}
		if (alpa_map[i])
			fcp->isp_loopid = i;
	}

#if 0
	fcp->isp_loopstate = LOOP_HAVE_ADDR;
#endif
	fcp->isp_loopstate = LOOP_TESTING_LINK;

	if (fcp->isp_topo == TOPO_F_PORT || fcp->isp_topo == TOPO_FL_PORT) {
		r = isp_getpdb(isp, chan, NPH_FL_ID, &pdb);
		if (r != 0 || pdb.portid == 0) {
			isp_prt(isp, ISP_LOGWARN,
			    "fabric topology, but cannot get info about fabric controller (0x%x)", r);
			fcp->isp_topo = TOPO_PTP_STUB;
			goto not_on_fabric;
		}

		fcp->isp_fabric_params = mbs.param[7];
		fcp->isp_sns_hdl = NPH_SNS_ID;
		r = isp_register_fc4_type(isp, chan);
		if (fcp->isp_loopstate < LOOP_TESTING_LINK)
			goto abort;
		if (r != 0)
			goto not_on_fabric;
		r = isp_register_fc4_features_24xx(isp, chan);
		if (fcp->isp_loopstate < LOOP_TESTING_LINK)
			goto abort;
		if (r != 0)
			goto not_on_fabric;
		r = isp_register_port_name_24xx(isp, chan);
		if (fcp->isp_loopstate < LOOP_TESTING_LINK)
			goto abort;
		if (r != 0)
			goto not_on_fabric;
		isp_register_node_name_24xx(isp, chan);
		if (fcp->isp_loopstate < LOOP_TESTING_LINK)
			goto abort;
	}

not_on_fabric:
	/* Get link speed. */
	fcp->isp_gbspeed = 1;
	MBSINIT(&mbs, MBOX_GET_SET_DATA_RATE, MBLOGALL, 3000000);
	mbs.param[1] = MBGSD_GET_RATE;
	/* mbs.param[2] undefined if we're just getting rate */
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] == MBOX_COMMAND_COMPLETE) {
		if (mbs.param[1] == MBGSD_10GB)
			fcp->isp_gbspeed = 10;
		else if (mbs.param[1] == MBGSD_32GB)
			fcp->isp_gbspeed = 32;
		else if (mbs.param[1] == MBGSD_16GB)
			fcp->isp_gbspeed = 16;
		else if (mbs.param[1] == MBGSD_8GB)
			fcp->isp_gbspeed = 8;
		else if (mbs.param[1] == MBGSD_4GB)
			fcp->isp_gbspeed = 4;
		else if (mbs.param[1] == MBGSD_2GB)
			fcp->isp_gbspeed = 2;
		else if (mbs.param[1] == MBGSD_1GB)
			fcp->isp_gbspeed = 1;
	}

	if (fcp->isp_loopstate < LOOP_TESTING_LINK) {
abort:
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d FC link test aborted", chan);
		return (1);
	}
	fcp->isp_loopstate = LOOP_LTEST_DONE;
	isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGCONFIG,
	    "Chan %d WWPN %016jx WWNN %016jx",
	    chan, (uintmax_t)fcp->isp_wwpn, (uintmax_t)fcp->isp_wwnn);
	isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGCONFIG,
	    "Chan %d %dGb %s PortID 0x%06x LoopID 0x%02x",
	    chan, fcp->isp_gbspeed, isp_fc_toponame(fcp), fcp->isp_portid,
	    fcp->isp_loopid);
	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC link test done", chan);
	return (0);
}

/*
 * Complete the synchronization of our Port Database.
 *
 * At this point, we've scanned the local loop (if any) and the fabric
 * and performed fabric logins on all new devices.
 *
 * Our task here is to go through our port database removing any entities
 * that are still marked probational (issuing PLOGO for ones which we had
 * PLOGI'd into) or are dead, and notifying upper layers about new/changed
 * devices.
 */
static int
isp_pdb_sync(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	fcportdb_t *lp;
	uint16_t dbidx;

	if (fcp->isp_loopstate < LOOP_FSCAN_DONE)
		return (-1);
	if (fcp->isp_loopstate >= LOOP_READY)
		return (0);

	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC PDB sync", chan);

	fcp->isp_loopstate = LOOP_SYNCING_PDB;

	for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
		lp = &fcp->portdb[dbidx];

		if (lp->state == FC_PORTDB_STATE_NIL)
			continue;
		if (lp->probational && lp->state != FC_PORTDB_STATE_ZOMBIE)
			lp->state = FC_PORTDB_STATE_DEAD;
		switch (lp->state) {
		case FC_PORTDB_STATE_DEAD:
			lp->state = FC_PORTDB_STATE_NIL;
			isp_async(isp, ISPASYNC_DEV_GONE, chan, lp);
			if ((lp->portid & 0xffff00) != 0) {
				(void) isp_plogx(isp, chan, lp->handle,
				    lp->portid,
				    PLOGX_FLG_CMD_LOGO |
				    PLOGX_FLG_IMPLICIT |
				    PLOGX_FLG_FREE_NPHDL);
			}
			/*
			 * Note that we might come out of this with our state
			 * set to FC_PORTDB_STATE_ZOMBIE.
			 */
			break;
		case FC_PORTDB_STATE_NEW:
			lp->state = FC_PORTDB_STATE_VALID;
			isp_async(isp, ISPASYNC_DEV_ARRIVED, chan, lp);
			break;
		case FC_PORTDB_STATE_CHANGED:
			lp->state = FC_PORTDB_STATE_VALID;
			isp_async(isp, ISPASYNC_DEV_CHANGED, chan, lp);
			lp->portid = lp->new_portid;
			lp->prli_word0 = lp->new_prli_word0;
			lp->prli_word3 = lp->new_prli_word3;
			break;
		case FC_PORTDB_STATE_VALID:
			isp_async(isp, ISPASYNC_DEV_STAYED, chan, lp);
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

	if (fcp->isp_loopstate < LOOP_SYNCING_PDB) {
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d FC PDB sync aborted", chan);
		return (1);
	}

	fcp->isp_loopstate = LOOP_READY;
	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC PDB sync done", chan);
	return (0);
}

static void
isp_pdb_add_update(ispsoftc_t *isp, int chan, isp_pdb_t *pdb)
{
	fcportdb_t *lp;
	uint64_t wwnn, wwpn;

	MAKE_WWN_FROM_NODE_NAME(wwnn, pdb->nodename);
	MAKE_WWN_FROM_NODE_NAME(wwpn, pdb->portname);

	/* Search port database for the same WWPN. */
	if (isp_find_pdb_by_wwpn(isp, chan, wwpn, &lp)) {
		if (!lp->probational) {
			isp_prt(isp, ISP_LOGERR,
			    "Chan %d Port 0x%06x@0x%04x [%d] is not probational (0x%x)",
			    chan, lp->portid, lp->handle,
			    FC_PORTDB_TGT(isp, chan, lp), lp->state);
			isp_dump_portdb(isp, chan);
			return;
		}
		lp->probational = 0;
		lp->node_wwn = wwnn;

		/* Old device, nothing new. */
		if (lp->portid == pdb->portid &&
		    lp->handle == pdb->handle &&
		    lp->prli_word3 == pdb->prli_word3 &&
		    ((pdb->prli_word0 & PRLI_WD0_EST_IMAGE_PAIR) ==
		     (lp->prli_word0 & PRLI_WD0_EST_IMAGE_PAIR))) {
			if (lp->state != FC_PORTDB_STATE_NEW)
				lp->state = FC_PORTDB_STATE_VALID;
			isp_prt(isp, ISP_LOG_SANCFG,
			    "Chan %d Port 0x%06x@0x%04x is valid",
			    chan, pdb->portid, pdb->handle);
			return;
		}

		/* Something has changed. */
		lp->state = FC_PORTDB_STATE_CHANGED;
		lp->handle = pdb->handle;
		lp->new_portid = pdb->portid;
		lp->new_prli_word0 = pdb->prli_word0;
		lp->new_prli_word3 = pdb->prli_word3;
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d Port 0x%06x@0x%04x is changed",
		    chan, pdb->portid, pdb->handle);
		return;
	}

	/* It seems like a new port. Find an empty slot for it. */
	if (!isp_find_pdb_empty(isp, chan, &lp)) {
		isp_prt(isp, ISP_LOGERR, "Chan %d out of portdb entries", chan);
		return;
	}

	ISP_MEMZERO(lp, sizeof (fcportdb_t));
	lp->probational = 0;
	lp->state = FC_PORTDB_STATE_NEW;
	lp->portid = lp->new_portid = pdb->portid;
	lp->prli_word0 = lp->new_prli_word0 = pdb->prli_word0;
	lp->prli_word3 = lp->new_prli_word3 = pdb->prli_word3;
	lp->handle = pdb->handle;
	lp->port_wwn = wwpn;
	lp->node_wwn = wwnn;
	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Port 0x%06x@0x%04x is new",
	    chan, pdb->portid, pdb->handle);
}

/*
 * Scan local loop for devices.
 */
static int
isp_scan_loop(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	int idx, lim, r;
	isp_pdb_t pdb;
	uint16_t *handles;
	uint16_t handle;

	if (fcp->isp_loopstate < LOOP_LTEST_DONE)
		return (-1);
	if (fcp->isp_loopstate >= LOOP_LSCAN_DONE)
		return (0);

	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC loop scan", chan);
	fcp->isp_loopstate = LOOP_SCANNING_LOOP;
	if (TOPO_IS_FABRIC(fcp->isp_topo)) {
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d FC loop scan done (no loop)", chan);
		fcp->isp_loopstate = LOOP_LSCAN_DONE;
		return (0);
	}

	handles = (uint16_t *)fcp->isp_scanscratch;
	lim = ISP_FC_SCRLEN / 2;
	r = isp_gethandles(isp, chan, handles, &lim, 1);
	if (r != 0) {
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d Getting list of handles failed with %x", chan, r);
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d FC loop scan done (bad)", chan);
		return (-1);
	}

	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Got %d handles",
	    chan, lim);

	/*
	 * Run through the list and get the port database info for each one.
	 */
	isp_mark_portdb(isp, chan);
	for (idx = 0; idx < lim; idx++) {
		handle = handles[idx];

		/*
		 * Don't scan "special" ids.
		 */
		if (handle >= NPH_RESERVED)
			continue;

		/*
		 * Get the port database entity for this index.
		 */
		r = isp_getpdb(isp, chan, handle, &pdb);
		if (fcp->isp_loopstate < LOOP_SCANNING_LOOP) {
abort:
			isp_prt(isp, ISP_LOG_SANCFG,
			    "Chan %d FC loop scan aborted", chan);
			return (1);
		}
		if (r != 0) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "Chan %d FC Scan Loop handle %d returned %x",
			    chan, handle, r);
			continue;
		}

		isp_pdb_add_update(isp, chan, &pdb);
	}
	if (fcp->isp_loopstate < LOOP_SCANNING_LOOP)
		goto abort;
	fcp->isp_loopstate = LOOP_LSCAN_DONE;
	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC loop scan done", chan);
	return (0);
}

static int
isp_ct_passthru(ispsoftc_t *isp, int chan, uint32_t cmd_bcnt, uint32_t rsp_bcnt)
{
	fcparam *fcp = FCPARAM(isp, chan);
	isp_ct_pt_t pt;
	int retval;

	if (isp->isp_dblev & ISP_LOGDEBUG1)
		isp_print_bytes(isp, "CT request", cmd_bcnt, fcp->isp_scratch);

	/*
	 * Build a Passthrough IOCB in memory.
	 */
	ISP_MEMZERO(&pt, sizeof(pt));
	pt.ctp_header.rqs_entry_count = 1;
	pt.ctp_header.rqs_entry_type = RQSTYPE_CT_PASSTHRU;
	pt.ctp_nphdl = fcp->isp_sns_hdl;
	pt.ctp_cmd_cnt = 1;
	pt.ctp_vpidx = ISP_GET_VPIDX(isp, chan);
	pt.ctp_time = 10;
	pt.ctp_rsp_cnt = 1;
	pt.ctp_rsp_bcnt = rsp_bcnt;
	pt.ctp_cmd_bcnt = cmd_bcnt;
	pt.ctp_dataseg[0].ds_base = DMA_LO32(fcp->isp_scdma);
	pt.ctp_dataseg[0].ds_basehi = DMA_HI32(fcp->isp_scdma);
	pt.ctp_dataseg[0].ds_count = cmd_bcnt;
	pt.ctp_dataseg[1].ds_base = DMA_LO32(fcp->isp_scdma);
	pt.ctp_dataseg[1].ds_basehi = DMA_HI32(fcp->isp_scdma);
	pt.ctp_dataseg[1].ds_count = rsp_bcnt;

	retval = isp_exec_entry_queue(isp, &pt, &pt, 2 * pt.ctp_time);
	if (retval != 0) {
		isp_prt(isp, ISP_LOGERR, "%s: CTP of chan %d error %d",
		    __func__, chan, retval);
		return (retval);
	}

	if (pt.ctp_status && pt.ctp_status != RQCS_DATA_UNDERRUN) {
		isp_prt(isp, ISP_LOGWARN,
		    "Chan %d CT pass-through returned 0x%x",
		    chan, pt.ctp_status);
		return (-1);
	}

	if (isp->isp_dblev & ISP_LOGDEBUG1)
		isp_print_bytes(isp, "CT response", rsp_bcnt, fcp->isp_scratch);

	return (0);
}

/*
 * Scan the fabric for devices and add them to our port database.
 *
 * Use the GID_PT command to get list of all Nx_Port IDs SNS knows.
 * Use GFF_ID and GFT_ID to check port type (FCP) and features (target).
 *
 * We use CT Pass-through IOCB.
 */
#define	GIDLEN	ISP_FC_SCRLEN
#define	NGENT	((GIDLEN - 16) >> 2)

static int
isp_gid_pt(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	ct_hdr_t ct;
	uint8_t *scp = fcp->isp_scratch;

	isp_prt(isp, ISP_LOGDEBUG0, "Chan %d requesting GID_PT", chan);
	if (FC_SCRATCH_ACQUIRE(isp, chan)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return (-1);
	}

	/* Build the CT command and execute via pass-through. */
	ISP_MEMZERO(&ct, sizeof (ct));
	ct.ct_revision = CT_REVISION;
	ct.ct_fcs_type = CT_FC_TYPE_FC;
	ct.ct_fcs_subtype = CT_FC_SUBTYPE_NS;
	ct.ct_cmd_resp = SNS_GID_PT;
	ct.ct_bcnt_resid = (GIDLEN - 16) >> 2;
	isp_put_ct_hdr(isp, &ct, (ct_hdr_t *)scp);
	scp[sizeof(ct)] = 0x7f;		/* Port Type = Nx_Port */
	scp[sizeof(ct)+1] = 0;		/* Domain_ID = any */
	scp[sizeof(ct)+2] = 0;		/* Area_ID = any */
	scp[sizeof(ct)+3] = 0;		/* Flags = no Area_ID */

	if (isp_ct_passthru(isp, chan, sizeof(ct) + sizeof(uint32_t), GIDLEN)) {
		FC_SCRATCH_RELEASE(isp, chan);
		return (-1);
	}

	isp_get_gid_xx_response(isp, (sns_gid_xx_rsp_t *)scp,
	    (sns_gid_xx_rsp_t *)fcp->isp_scanscratch, NGENT);
	FC_SCRATCH_RELEASE(isp, chan);
	return (0);
}

static int
isp_gff_id(ispsoftc_t *isp, int chan, uint32_t portid)
{
	fcparam *fcp = FCPARAM(isp, chan);
	ct_hdr_t ct;
	uint32_t *rp;
	uint8_t *scp = fcp->isp_scratch;
	sns_gff_id_rsp_t rsp;
	int i, res = -1;

	if (!fcp->isp_use_gff_id)	/* User may block GFF_ID use. */
		return (res);

	isp_prt(isp, ISP_LOGDEBUG0, "Chan %d requesting GFF_ID", chan);
	if (FC_SCRATCH_ACQUIRE(isp, chan)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return (res);
	}

	/* Build the CT command and execute via pass-through. */
	ISP_MEMZERO(&ct, sizeof (ct));
	ct.ct_revision = CT_REVISION;
	ct.ct_fcs_type = CT_FC_TYPE_FC;
	ct.ct_fcs_subtype = CT_FC_SUBTYPE_NS;
	ct.ct_cmd_resp = SNS_GFF_ID;
	ct.ct_bcnt_resid = (SNS_GFF_ID_RESP_SIZE - sizeof(ct)) / 4;
	isp_put_ct_hdr(isp, &ct, (ct_hdr_t *)scp);
	rp = (uint32_t *) &scp[sizeof(ct)];
	ISP_IOZPUT_32(isp, portid, rp);

	if (isp_ct_passthru(isp, chan, sizeof(ct) + sizeof(uint32_t),
	    SNS_GFF_ID_RESP_SIZE)) {
		FC_SCRATCH_RELEASE(isp, chan);
		return (res);
	}

	isp_get_gff_id_response(isp, (sns_gff_id_rsp_t *)scp, &rsp);
	if (rsp.snscb_cthdr.ct_cmd_resp == LS_ACC) {
		for (i = 0; i < 32; i++) {
			if (rsp.snscb_fc4_features[i] != 0) {
				res = 0;
				break;
			}
		}
		if (((rsp.snscb_fc4_features[FC4_SCSI / 8] >>
		    ((FC4_SCSI % 8) * 4)) & 0x01) != 0)
			res = 1;
		/* Workaround for broken Brocade firmware. */
		if (((ISP_SWAP32(isp, rsp.snscb_fc4_features[FC4_SCSI / 8]) >>
		    ((FC4_SCSI % 8) * 4)) & 0x01) != 0)
			res = 1;
	}
	FC_SCRATCH_RELEASE(isp, chan);
	isp_prt(isp, ISP_LOGDEBUG0, "Chan %d GFF_ID result is %d", chan, res);
	return (res);
}

static int
isp_gft_id(ispsoftc_t *isp, int chan, uint32_t portid)
{
	fcparam *fcp = FCPARAM(isp, chan);
	ct_hdr_t ct;
	uint32_t *rp;
	uint8_t *scp = fcp->isp_scratch;
	sns_gft_id_rsp_t rsp;
	int i, res = -1;

	if (!fcp->isp_use_gft_id)	/* User may block GFT_ID use. */
		return (res);

	isp_prt(isp, ISP_LOGDEBUG0, "Chan %d requesting GFT_ID", chan);
	if (FC_SCRATCH_ACQUIRE(isp, chan)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return (res);
	}

	/* Build the CT command and execute via pass-through. */
	ISP_MEMZERO(&ct, sizeof (ct));
	ct.ct_revision = CT_REVISION;
	ct.ct_fcs_type = CT_FC_TYPE_FC;
	ct.ct_fcs_subtype = CT_FC_SUBTYPE_NS;
	ct.ct_cmd_resp = SNS_GFT_ID;
	ct.ct_bcnt_resid = (SNS_GFT_ID_RESP_SIZE - sizeof(ct)) / 4;
	isp_put_ct_hdr(isp, &ct, (ct_hdr_t *)scp);
	rp = (uint32_t *) &scp[sizeof(ct)];
	ISP_IOZPUT_32(isp, portid, rp);

	if (isp_ct_passthru(isp, chan, sizeof(ct) + sizeof(uint32_t),
	    SNS_GFT_ID_RESP_SIZE)) {
		FC_SCRATCH_RELEASE(isp, chan);
		return (res);
	}

	isp_get_gft_id_response(isp, (sns_gft_id_rsp_t *)scp, &rsp);
	if (rsp.snscb_cthdr.ct_cmd_resp == LS_ACC) {
		for (i = 0; i < 8; i++) {
			if (rsp.snscb_fc4_types[i] != 0) {
				res = 0;
				break;
			}
		}
		if (((rsp.snscb_fc4_types[FC4_SCSI / 32] >>
		    (FC4_SCSI % 32)) & 0x01) != 0)
			res = 1;
	}
	FC_SCRATCH_RELEASE(isp, chan);
	isp_prt(isp, ISP_LOGDEBUG0, "Chan %d GFT_ID result is %d", chan, res);
	return (res);
}

static int
isp_scan_fabric(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	fcportdb_t *lp;
	uint32_t portid;
	isp_pdb_t pdb;
	int portidx, portlim, r;
	sns_gid_xx_rsp_t *rs;

	if (fcp->isp_loopstate < LOOP_LSCAN_DONE)
		return (-1);
	if (fcp->isp_loopstate >= LOOP_FSCAN_DONE)
		return (0);

	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC fabric scan", chan);
	fcp->isp_loopstate = LOOP_SCANNING_FABRIC;
	if (!TOPO_IS_FABRIC(fcp->isp_topo)) {
		fcp->isp_loopstate = LOOP_FSCAN_DONE;
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d FC fabric scan done (no fabric)", chan);
		return (0);
	}

	if (fcp->isp_loopstate < LOOP_SCANNING_FABRIC) {
abort:
		FC_SCRATCH_RELEASE(isp, chan);
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d FC fabric scan aborted", chan);
		return (1);
	}

	/*
	 * Make sure we still are logged into the fabric controller.
	 */
	r = isp_getpdb(isp, chan, NPH_FL_ID, &pdb);
	if ((r & 0xffff) == MBOX_NOT_LOGGED_IN) {
		isp_dump_chip_portdb(isp, chan);
	}
	if (r) {
		fcp->isp_loopstate = LOOP_LTEST_DONE;
fail:
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d FC fabric scan done (bad)", chan);
		return (-1);
	}

	/* Get list of port IDs from SNS. */
	r = isp_gid_pt(isp, chan);
	if (fcp->isp_loopstate < LOOP_SCANNING_FABRIC)
		goto abort;
	if (r > 0) {
		fcp->isp_loopstate = LOOP_FSCAN_DONE;
		return (-1);
	} else if (r < 0) {
		fcp->isp_loopstate = LOOP_LTEST_DONE;	/* try again */
		return (-1);
	}

	rs = (sns_gid_xx_rsp_t *) fcp->isp_scanscratch;
	if (fcp->isp_loopstate < LOOP_SCANNING_FABRIC)
		goto abort;
	if (rs->snscb_cthdr.ct_cmd_resp != LS_ACC) {
		int level;
		/* FC-4 Type and Port Type not registered are not errors. */
		if (rs->snscb_cthdr.ct_reason == 9 &&
		    (rs->snscb_cthdr.ct_explanation == 0x07 ||
		     rs->snscb_cthdr.ct_explanation == 0x0a)) {
			level = ISP_LOG_SANCFG;
		} else {
			level = ISP_LOGWARN;
		}
		isp_prt(isp, level, "Chan %d Fabric Nameserver rejected GID_PT"
		    " (Reason=0x%x Expl=0x%x)", chan,
		    rs->snscb_cthdr.ct_reason,
		    rs->snscb_cthdr.ct_explanation);
		fcp->isp_loopstate = LOOP_FSCAN_DONE;
		return (-1);
	}

	/* Check our buffer was big enough to get the full list. */
	for (portidx = 0; portidx < NGENT-1; portidx++) {
		if (rs->snscb_ports[portidx].control & 0x80)
			break;
	}
	if ((rs->snscb_ports[portidx].control & 0x80) == 0) {
		isp_prt(isp, ISP_LOGWARN,
		    "fabric too big for scratch area: increase ISP_FC_SCRLEN");
	}
	portlim = portidx + 1;
	isp_prt(isp, ISP_LOG_SANCFG,
	    "Chan %d Got %d ports back from name server", chan, portlim);

	/* Go through the list and remove duplicate port ids. */
	for (portidx = 0; portidx < portlim; portidx++) {
		int npidx;

		portid =
		    ((rs->snscb_ports[portidx].portid[0]) << 16) |
		    ((rs->snscb_ports[portidx].portid[1]) << 8) |
		    ((rs->snscb_ports[portidx].portid[2]));

		for (npidx = portidx + 1; npidx < portlim; npidx++) {
			uint32_t new_portid =
			    ((rs->snscb_ports[npidx].portid[0]) << 16) |
			    ((rs->snscb_ports[npidx].portid[1]) << 8) |
			    ((rs->snscb_ports[npidx].portid[2]));
			if (new_portid == portid) {
				break;
			}
		}

		if (npidx < portlim) {
			rs->snscb_ports[npidx].portid[0] = 0;
			rs->snscb_ports[npidx].portid[1] = 0;
			rs->snscb_ports[npidx].portid[2] = 0;
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
	isp_mark_portdb(isp, chan);
	for (portidx = 0; portidx < portlim; portidx++) {
		portid = ((rs->snscb_ports[portidx].portid[0]) << 16) |
			 ((rs->snscb_ports[portidx].portid[1]) << 8) |
			 ((rs->snscb_ports[portidx].portid[2]));
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d Checking fabric port 0x%06x", chan, portid);
		if (portid == 0) {
			isp_prt(isp, ISP_LOG_SANCFG,
			    "Chan %d Port at idx %d is zero",
			    chan, portidx);
			continue;
		}
		if (portid == fcp->isp_portid) {
			isp_prt(isp, ISP_LOG_SANCFG,
			    "Chan %d Port 0x%06x is our", chan, portid);
			continue;
		}

		/* Now search the entire port database for the same portid. */
		if (isp_find_pdb_by_portid(isp, chan, portid, &lp)) {
			if (!lp->probational) {
				isp_prt(isp, ISP_LOGERR,
				    "Chan %d Port 0x%06x@0x%04x [%d] is not probational (0x%x)",
				    chan, lp->portid, lp->handle,
				    FC_PORTDB_TGT(isp, chan, lp), lp->state);
				isp_dump_portdb(isp, chan);
				goto fail;
			}

			if (lp->state == FC_PORTDB_STATE_ZOMBIE)
				goto relogin;

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
			 */
			r = isp_getpdb(isp, chan, lp->handle, &pdb);
			if (fcp->isp_loopstate < LOOP_SCANNING_FABRIC)
				goto abort;
			if (r != 0) {
				lp->state = FC_PORTDB_STATE_DEAD;
				isp_prt(isp, ISP_LOG_SANCFG,
				    "Chan %d Port 0x%06x handle 0x%x is dead (%d)",
				    chan, portid, lp->handle, r);
				goto relogin;
			}

			isp_pdb_add_update(isp, chan, &pdb);
			continue;
		}

relogin:
		if ((fcp->role & ISP_ROLE_INITIATOR) == 0) {
			isp_prt(isp, ISP_LOG_SANCFG,
			    "Chan %d Port 0x%06x is not logged in", chan, portid);
			continue;
		}

		r = isp_gff_id(isp, chan, portid);
		if (r == 0) {
			isp_prt(isp, ISP_LOG_SANCFG,
			    "Chan %d Port 0x%06x is not an FCP target", chan, portid);
			continue;
		}
		if (r < 0)
			r = isp_gft_id(isp, chan, portid);
		if (r == 0) {
			isp_prt(isp, ISP_LOG_SANCFG,
			    "Chan %d Port 0x%06x is not FCP", chan, portid);
			continue;
		}

		if (isp_login_device(isp, chan, portid, &pdb,
		    &FCPARAM(isp, 0)->isp_lasthdl)) {
			if (fcp->isp_loopstate < LOOP_SCANNING_FABRIC)
				goto abort;
			continue;
		}

		isp_pdb_add_update(isp, chan, &pdb);
	}

	if (fcp->isp_loopstate < LOOP_SCANNING_FABRIC)
		goto abort;
	fcp->isp_loopstate = LOOP_FSCAN_DONE;
	isp_prt(isp, ISP_LOG_SANCFG, "Chan %d FC fabric scan done", chan);
	return (0);
}

/*
 * Find an unused handle and try and use to login to a port.
 */
static int
isp_login_device(ispsoftc_t *isp, int chan, uint32_t portid, isp_pdb_t *p, uint16_t *ohp)
{
	int i, r;
	uint16_t handle;

	handle = isp_next_handle(isp, ohp);
	for (i = 0; i < NPH_MAX_2K; i++) {
		if (FCPARAM(isp, chan)->isp_loopstate != LOOP_SCANNING_FABRIC)
			return (-1);

		/* Check if this handle is free. */
		r = isp_getpdb(isp, chan, handle, p);
		if (r == 0) {
			if (p->portid != portid) {
				/* This handle is busy, try next one. */
				handle = isp_next_handle(isp, ohp);
				continue;
			}
			break;
		}
		if (FCPARAM(isp, chan)->isp_loopstate != LOOP_SCANNING_FABRIC)
			return (-1);

		/*
		 * Now try and log into the device
		 */
		r = isp_plogx(isp, chan, handle, portid, PLOGX_FLG_CMD_PLOGI);
		if (r == 0) {
			break;
		} else if ((r & 0xffff) == MBOX_PORT_ID_USED) {
			/*
			 * If we get here, then the firmwware still thinks we're logged into this device, but with a different
			 * handle. We need to break that association. We used to try and just substitute the handle, but then
			 * failed to get any data via isp_getpdb (below).
			 */
			if (isp_plogx(isp, chan, r >> 16, portid, PLOGX_FLG_CMD_LOGO | PLOGX_FLG_IMPLICIT | PLOGX_FLG_FREE_NPHDL)) {
				isp_prt(isp, ISP_LOGERR, "baw... logout of %x failed", r >> 16);
			}
			if (FCPARAM(isp, chan)->isp_loopstate != LOOP_SCANNING_FABRIC)
				return (-1);
			r = isp_plogx(isp, chan, handle, portid, PLOGX_FLG_CMD_PLOGI);
			if (r != 0)
				i = NPH_MAX_2K;
			break;
		} else if ((r & 0xffff) == MBOX_LOOP_ID_USED) {
			/* Try the next handle. */
			handle = isp_next_handle(isp, ohp);
		} else {
			/* Give up. */
			i = NPH_MAX_2K;
			break;
		}
	}

	if (i == NPH_MAX_2K) {
		isp_prt(isp, ISP_LOGWARN, "Chan %d PLOGI 0x%06x failed", chan, portid);
		return (-1);
	}

	/*
	 * If we successfully logged into it, get the PDB for it
	 * so we can crosscheck that it is still what we think it
	 * is and that we also have the role it plays
	 */
	r = isp_getpdb(isp, chan, handle, p);
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
	rft_id_t rp;
	ct_hdr_t *ct = &rp.rftid_hdr;
	uint8_t *scp = fcp->isp_scratch;

	if (FC_SCRATCH_ACQUIRE(isp, chan)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return (-1);
	}

	/* Build the CT command and execute via pass-through. */
	ISP_MEMZERO(&rp, sizeof(rp));
	ct->ct_revision = CT_REVISION;
	ct->ct_fcs_type = CT_FC_TYPE_FC;
	ct->ct_fcs_subtype = CT_FC_SUBTYPE_NS;
	ct->ct_cmd_resp = SNS_RFT_ID;
	ct->ct_bcnt_resid = (sizeof (rft_id_t) - sizeof (ct_hdr_t)) >> 2;
	rp.rftid_portid[0] = fcp->isp_portid >> 16;
	rp.rftid_portid[1] = fcp->isp_portid >> 8;
	rp.rftid_portid[2] = fcp->isp_portid;
	rp.rftid_fc4types[FC4_SCSI >> 5] = 1 << (FC4_SCSI & 0x1f);
	isp_put_rft_id(isp, &rp, (rft_id_t *)scp);

	if (isp_ct_passthru(isp, chan, sizeof(rft_id_t), sizeof(ct_hdr_t))) {
		FC_SCRATCH_RELEASE(isp, chan);
		return (-1);
	}

	isp_get_ct_hdr(isp, (ct_hdr_t *) scp, ct);
	FC_SCRATCH_RELEASE(isp, chan);
	if (ct->ct_cmd_resp == LS_RJT) {
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOG_WARN1, "Chan %d Register FC4 Type rejected", chan);
		return (-1);
	} else if (ct->ct_cmd_resp == LS_ACC) {
		isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Register FC4 Type accepted", chan);
	} else {
		isp_prt(isp, ISP_LOGWARN, "Chan %d Register FC4 Type: 0x%x", chan, ct->ct_cmd_resp);
		return (-1);
	}
	return (0);
}

static int
isp_register_fc4_features_24xx(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	ct_hdr_t *ct;
	rff_id_t rp;
	uint8_t *scp = fcp->isp_scratch;

	if (FC_SCRATCH_ACQUIRE(isp, chan)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return (-1);
	}

	/*
	 * Build the CT header and command in memory.
	 */
	ISP_MEMZERO(&rp, sizeof(rp));
	ct = &rp.rffid_hdr;
	ct->ct_revision = CT_REVISION;
	ct->ct_fcs_type = CT_FC_TYPE_FC;
	ct->ct_fcs_subtype = CT_FC_SUBTYPE_NS;
	ct->ct_cmd_resp = SNS_RFF_ID;
	ct->ct_bcnt_resid = (sizeof (rff_id_t) - sizeof (ct_hdr_t)) >> 2;
	rp.rffid_portid[0] = fcp->isp_portid >> 16;
	rp.rffid_portid[1] = fcp->isp_portid >> 8;
	rp.rffid_portid[2] = fcp->isp_portid;
	rp.rffid_fc4features = 0;
	if (fcp->role & ISP_ROLE_TARGET)
		rp.rffid_fc4features |= 1;
	if (fcp->role & ISP_ROLE_INITIATOR)
		rp.rffid_fc4features |= 2;
	rp.rffid_fc4type = FC4_SCSI;
	isp_put_rff_id(isp, &rp, (rff_id_t *)scp);
	if (isp->isp_dblev & ISP_LOGDEBUG1)
		isp_print_bytes(isp, "CT request", sizeof(rft_id_t), scp);

	if (isp_ct_passthru(isp, chan, sizeof(rft_id_t), sizeof(ct_hdr_t))) {
		FC_SCRATCH_RELEASE(isp, chan);
		return (-1);
	}

	isp_get_ct_hdr(isp, (ct_hdr_t *) scp, ct);
	FC_SCRATCH_RELEASE(isp, chan);
	if (ct->ct_cmd_resp == LS_RJT) {
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOG_WARN1,
		    "Chan %d Register FC4 Features rejected", chan);
		return (-1);
	} else if (ct->ct_cmd_resp == LS_ACC) {
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d Register FC4 Features accepted", chan);
	} else {
		isp_prt(isp, ISP_LOGWARN,
		    "Chan %d Register FC4 Features: 0x%x", chan, ct->ct_cmd_resp);
		return (-1);
	}
	return (0);
}

static int
isp_register_port_name_24xx(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	ct_hdr_t *ct;
	rspn_id_t rp;
	uint8_t *scp = fcp->isp_scratch;
	int len;

	if (FC_SCRATCH_ACQUIRE(isp, chan)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return (-1);
	}

	/*
	 * Build the CT header and command in memory.
	 */
	ISP_MEMZERO(&rp, sizeof(rp));
	ct = &rp.rspnid_hdr;
	ct->ct_revision = CT_REVISION;
	ct->ct_fcs_type = CT_FC_TYPE_FC;
	ct->ct_fcs_subtype = CT_FC_SUBTYPE_NS;
	ct->ct_cmd_resp = SNS_RSPN_ID;
	rp.rspnid_portid[0] = fcp->isp_portid >> 16;
	rp.rspnid_portid[1] = fcp->isp_portid >> 8;
	rp.rspnid_portid[2] = fcp->isp_portid;
	rp.rspnid_length = 0;
	len = offsetof(rspn_id_t, rspnid_name);
	mtx_lock(&prison0.pr_mtx);
	rp.rspnid_length += sprintf(&scp[len + rp.rspnid_length],
	    "%s", prison0.pr_hostname[0] ? prison0.pr_hostname : "FreeBSD");
	mtx_unlock(&prison0.pr_mtx);
	rp.rspnid_length += sprintf(&scp[len + rp.rspnid_length],
	    ":%s", device_get_nameunit(isp->isp_dev));
	if (chan != 0) {
		rp.rspnid_length += sprintf(&scp[len + rp.rspnid_length],
		    "/%d", chan);
	}
	len += rp.rspnid_length;
	ct->ct_bcnt_resid = (len - sizeof(ct_hdr_t)) >> 2;
	isp_put_rspn_id(isp, &rp, (rspn_id_t *)scp);

	if (isp_ct_passthru(isp, chan, len, sizeof(ct_hdr_t))) {
		FC_SCRATCH_RELEASE(isp, chan);
		return (-1);
	}

	isp_get_ct_hdr(isp, (ct_hdr_t *) scp, ct);
	FC_SCRATCH_RELEASE(isp, chan);
	if (ct->ct_cmd_resp == LS_RJT) {
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOG_WARN1,
		    "Chan %d Register Symbolic Port Name rejected", chan);
		return (-1);
	} else if (ct->ct_cmd_resp == LS_ACC) {
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d Register Symbolic Port Name accepted", chan);
	} else {
		isp_prt(isp, ISP_LOGWARN,
		    "Chan %d Register Symbolic Port Name: 0x%x", chan, ct->ct_cmd_resp);
		return (-1);
	}
	return (0);
}

static int
isp_register_node_name_24xx(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	ct_hdr_t *ct;
	rsnn_nn_t rp;
	uint8_t *scp = fcp->isp_scratch;
	int len;

	if (FC_SCRATCH_ACQUIRE(isp, chan)) {
		isp_prt(isp, ISP_LOGERR, sacq);
		return (-1);
	}

	/*
	 * Build the CT header and command in memory.
	 */
	ISP_MEMZERO(&rp, sizeof(rp));
	ct = &rp.rsnnnn_hdr;
	ct->ct_revision = CT_REVISION;
	ct->ct_fcs_type = CT_FC_TYPE_FC;
	ct->ct_fcs_subtype = CT_FC_SUBTYPE_NS;
	ct->ct_cmd_resp = SNS_RSNN_NN;
	MAKE_NODE_NAME_FROM_WWN(rp.rsnnnn_nodename, fcp->isp_wwnn);
	rp.rsnnnn_length = 0;
	len = offsetof(rsnn_nn_t, rsnnnn_name);
	mtx_lock(&prison0.pr_mtx);
	rp.rsnnnn_length += sprintf(&scp[len + rp.rsnnnn_length],
	    "%s", prison0.pr_hostname[0] ? prison0.pr_hostname : "FreeBSD");
	mtx_unlock(&prison0.pr_mtx);
	len += rp.rsnnnn_length;
	ct->ct_bcnt_resid = (len - sizeof(ct_hdr_t)) >> 2;
	isp_put_rsnn_nn(isp, &rp, (rsnn_nn_t *)scp);

	if (isp_ct_passthru(isp, chan, len, sizeof(ct_hdr_t))) {
		FC_SCRATCH_RELEASE(isp, chan);
		return (-1);
	}

	isp_get_ct_hdr(isp, (ct_hdr_t *) scp, ct);
	FC_SCRATCH_RELEASE(isp, chan);
	if (ct->ct_cmd_resp == LS_RJT) {
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOG_WARN1,
		    "Chan %d Register Symbolic Node Name rejected", chan);
		return (-1);
	} else if (ct->ct_cmd_resp == LS_ACC) {
		isp_prt(isp, ISP_LOG_SANCFG,
		    "Chan %d Register Symbolic Node Name accepted", chan);
	} else {
		isp_prt(isp, ISP_LOGWARN,
		    "Chan %d Register Symbolic Node Name: 0x%x", chan, ct->ct_cmd_resp);
		return (-1);
	}
	return (0);
}

static uint16_t
isp_next_handle(ispsoftc_t *isp, uint16_t *ohp)
{
	fcparam *fcp;
	int i, chan, wrap;
	uint16_t handle;

	handle = *ohp;
	wrap = 0;

next:
	if (handle == NIL_HANDLE) {
		handle = 0;
	} else {
		handle++;
		if (handle > NPH_RESERVED - 1) {
			if (++wrap >= 2) {
				isp_prt(isp, ISP_LOGERR, "Out of port handles!");
				return (NIL_HANDLE);
			}
			handle = 0;
		}
	}
	for (chan = 0; chan < isp->isp_nchan; chan++) {
		fcp = FCPARAM(isp, chan);
		if (fcp->role == ISP_ROLE_NONE)
			continue;
		for (i = 0; i < MAX_FC_TARG; i++) {
			if (fcp->portdb[i].state != FC_PORTDB_STATE_NIL &&
			    fcp->portdb[i].handle == handle)
				goto next;
		}
	}
	*ohp = handle;
	return (handle);
}

/*
 * Start a command. Locking is assumed done in the caller.
 */

int
isp_start(XS_T *xs)
{
	ispsoftc_t *isp;
	fcparam *fcp;
	uint32_t cdblen;
	ispreqt7_t local, *reqp = &local;
	void *qep;
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

	if (XS_CDBLEN(xs) > 16 || XS_CDBLEN(xs) == 0) {
		isp_prt(isp, ISP_LOGERR, "unsupported cdb length (%d, CDB[0]=0x%x)", XS_CDBLEN(xs), XS_CDBP(xs)[0] & 0xff);
		XS_SETERR(xs, HBA_REQINVAL);
		return (CMD_COMPLETE);
	}

	/*
	 * Translate the target to device handle as appropriate, checking
	 * for correct device state as well.
	 */
	target = XS_TGT(xs);
	fcp = FCPARAM(isp, XS_CHANNEL(xs));

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

	isp_prt(isp, ISP_LOGDEBUG2, "XS_TGT(xs)=%d", target);
	lp = &fcp->portdb[target];
	if (target < 0 || target >= MAX_FC_TARG ||
	    lp->is_target == 0) {
		XS_SETERR(xs, HBA_SELTIMEOUT);
		return (CMD_COMPLETE);
	}
	if (fcp->isp_loopstate != LOOP_READY) {
		isp_prt(isp, ISP_LOGDEBUG1,
		    "%d.%d.%jx loop is not ready",
		    XS_CHANNEL(xs), target, (uintmax_t)XS_LUN(xs));
		return (CMD_RQLATER);
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
	 * We do dual duty here (cough) for synchronizing for buses other
	 * than which we got here to send a command to.
	 */
	ISP_MEMZERO(reqp, QENTRY_LEN);
	if (ISP_TST_SENDMARKER(isp, XS_CHANNEL(xs))) {
		isp_marker_24xx_t *m = (isp_marker_24xx_t *) reqp;
		m->mrk_header.rqs_entry_count = 1;
		m->mrk_header.rqs_entry_type = RQSTYPE_MARKER;
		m->mrk_modifier = SYNC_ALL;
		m->mrk_vphdl = XS_CHANNEL(xs);
		isp_put_marker_24xx(isp, m, qep);
		ISP_SYNC_REQUEST(isp);
		ISP_SET_SENDMARKER(isp, XS_CHANNEL(xs), 0);
		goto start_again;
	}

	/*
	 * NB: we do not support long CDBs (yet)
	 */
	cdblen = XS_CDBLEN(xs);
	if (cdblen > sizeof (reqp->req_cdb)) {
		isp_prt(isp, ISP_LOGERR, "Command Length %u too long for this chip", cdblen);
		XS_SETERR(xs, HBA_REQINVAL);
		return (CMD_COMPLETE);
	}

	reqp->req_header.rqs_entry_type = RQSTYPE_T7RQS;
	reqp->req_header.rqs_entry_count = 1;
	reqp->req_nphdl = lp->handle;
	reqp->req_time = XS_TIME(xs);
	be64enc(reqp->req_lun, CAM_EXTLUN_BYTE_SWIZZLE(XS_LUN(xs)));
	if (XS_XFRIN(xs))
		reqp->req_alen_datadir = FCP_CMND_DATA_READ;
	else if (XS_XFROUT(xs))
		reqp->req_alen_datadir = FCP_CMND_DATA_WRITE;
	if (XS_TAG_P(xs))
		reqp->req_task_attribute = XS_TAG_TYPE(xs);
	else
		reqp->req_task_attribute = FCP_CMND_TASK_ATTR_SIMPLE;
	reqp->req_task_attribute |= (XS_PRIORITY(xs) << FCP_CMND_PRIO_SHIFT) &
	     FCP_CMND_PRIO_MASK;
	if (FCPARAM(isp, XS_CHANNEL(xs))->fctape_enabled && (lp->prli_word3 & PRLI_WD3_RETRY)) {
		if (FCP_NEXT_CRN(isp, &reqp->req_crn, xs)) {
			isp_prt(isp, ISP_LOG_WARN1,
			    "%d.%d.%jx cannot generate next CRN",
			    XS_CHANNEL(xs), target, (uintmax_t)XS_LUN(xs));
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_EAGAIN);
		}
	}
	ISP_MEMCPY(reqp->req_cdb, XS_CDBP(xs), cdblen);
	reqp->req_dl = XS_XFRLEN(xs);
	reqp->req_tidlo = lp->portid;
	reqp->req_tidhi = lp->portid >> 16;
	reqp->req_vpidx = ISP_GET_VPIDX(isp, XS_CHANNEL(xs));

	/* Whew. Thankfully the same for type 7 requests */
	reqp->req_handle = isp_allocate_handle(isp, xs, ISP_HANDLE_INITIATOR);
	if (reqp->req_handle == 0) {
		isp_prt(isp, ISP_LOG_WARN1, "out of xflist pointers");
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_EAGAIN);
	}

	/*
	 * Set up DMA and/or do any platform dependent swizzling of the request entry
	 * so that the Qlogic F/W understands what is being asked of it.
	 *
	 * The callee is responsible for adding all requests at this point.
	 */
	dmaresult = ISP_DMASETUP(isp, xs, reqp);
	if (dmaresult != 0) {
		isp_destroy_handle(isp, reqp->req_handle);
		/*
		 * dmasetup sets actual error in packet, and
		 * return what we were given to return.
		 */
		return (dmaresult);
	}
	isp_xs_prt(isp, xs, ISP_LOGDEBUG0, "START cmd cdb[0]=0x%x datalen %ld", XS_CDBP(xs)[0], (long) XS_XFRLEN(xs));
	return (0);
}

/*
 * isp control
 * Locks (ints blocked) assumed held.
 */

int
isp_control(ispsoftc_t *isp, ispctl_t ctl, ...)
{
	fcparam *fcp;
	fcportdb_t *lp;
	XS_T *xs;
	mbreg_t *mbr;
	int chan, tgt;
	uint32_t handle;
	va_list ap;
	uint8_t local[QENTRY_LEN];

	switch (ctl) {
	case ISPCTL_RESET_BUS:
		/*
		 * Issue a bus reset.
		 */
		isp_prt(isp, ISP_LOGERR, "BUS RESET NOT IMPLEMENTED");
		break;

	case ISPCTL_RESET_DEV:
	{
		isp24xx_tmf_t *tmf;
		isp24xx_statusreq_t *sp;

		va_start(ap, ctl);
		chan = va_arg(ap, int);
		tgt = va_arg(ap, int);
		va_end(ap);
		fcp = FCPARAM(isp, chan);

		if (tgt < 0 || tgt >= MAX_FC_TARG) {
			isp_prt(isp, ISP_LOGWARN, "Chan %d trying to reset bad target %d", chan, tgt);
			break;
		}
		lp = &fcp->portdb[tgt];
		if (lp->is_target == 0 || lp->state != FC_PORTDB_STATE_VALID) {
			isp_prt(isp, ISP_LOGWARN, "Chan %d abort of no longer valid target %d", chan, tgt);
			break;
		}

		tmf = (isp24xx_tmf_t *) local;
		ISP_MEMZERO(tmf, QENTRY_LEN);
		tmf->tmf_header.rqs_entry_type = RQSTYPE_TSK_MGMT;
		tmf->tmf_header.rqs_entry_count = 1;
		tmf->tmf_nphdl = lp->handle;
		tmf->tmf_delay = 2;
		tmf->tmf_timeout = 4;
		tmf->tmf_flags = ISP24XX_TMF_TARGET_RESET;
		tmf->tmf_tidlo = lp->portid;
		tmf->tmf_tidhi = lp->portid >> 16;
		tmf->tmf_vpidx = ISP_GET_VPIDX(isp, chan);
		fcp->sendmarker = 1;
		isp_prt(isp, ISP_LOGALL, "Chan %d Reset N-Port Handle 0x%04x @ Port 0x%06x", chan, lp->handle, lp->portid);

		sp = (isp24xx_statusreq_t *) local;
		if (isp_exec_entry_mbox(isp, tmf, sp, 2 * tmf->tmf_timeout))
			break;

		if (sp->req_completion_status == 0)
			return (0);
		isp_prt(isp, ISP_LOGWARN, "Chan %d reset of target %d returned 0x%x", chan, tgt, sp->req_completion_status);
		break;
	}
	case ISPCTL_ABORT_CMD:
	{
		isp24xx_abrt_t *ab = (isp24xx_abrt_t *)&local;

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

		fcp = FCPARAM(isp, chan);
		if (tgt < 0 || tgt >= MAX_FC_TARG) {
			isp_prt(isp, ISP_LOGWARN, "Chan %d trying to abort bad target %d", chan, tgt);
			break;
		}
		lp = &fcp->portdb[tgt];
		if (lp->is_target == 0 || lp->state != FC_PORTDB_STATE_VALID) {
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

		if (isp_exec_entry_mbox(isp, ab, ab, 5))
			break;

		if (ab->abrt_nphdl == ISP24XX_ABRT_OKAY)
			return (0);
		isp_prt(isp, ISP_LOGWARN, "Chan %d handle %d abort returned 0x%x", chan, tgt, ab->abrt_nphdl);
	}
	case ISPCTL_FCLINK_TEST:
	{
		int usdelay;

		va_start(ap, ctl);
		chan = va_arg(ap, int);
		usdelay = va_arg(ap, int);
		va_end(ap);
		if (usdelay == 0)
			usdelay = 250000;
		return (isp_fclink_test(isp, chan, usdelay));
	}
	case ISPCTL_SCAN_FABRIC:

		va_start(ap, ctl);
		chan = va_arg(ap, int);
		va_end(ap);
		return (isp_scan_fabric(isp, chan));

	case ISPCTL_SCAN_LOOP:

		va_start(ap, ctl);
		chan = va_arg(ap, int);
		va_end(ap);
		return (isp_scan_loop(isp, chan));

	case ISPCTL_PDB_SYNC:

		va_start(ap, ctl);
		chan = va_arg(ap, int);
		va_end(ap);
		return (isp_pdb_sync(isp, chan));

	case ISPCTL_SEND_LIP:
		break;

	case ISPCTL_GET_PDB:
	{
		isp_pdb_t *pdb;
		va_start(ap, ctl);
		chan = va_arg(ap, int);
		tgt = va_arg(ap, int);
		pdb = va_arg(ap, isp_pdb_t *);
		va_end(ap);
		return (isp_getpdb(isp, chan, tgt, pdb));
	}
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
			return (isp_plogx(isp, p->channel, p->handle, p->portid, p->flags));
		}
		do {
			isp_next_handle(isp, &p->handle);
			r = isp_plogx(isp, p->channel, p->handle, p->portid, p->flags);
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
		int role;

		va_start(ap, ctl);
		chan = va_arg(ap, int);
		role = va_arg(ap, int);
		va_end(ap);
		return (isp_fc_change_role(isp, chan, role));
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

#ifdef	ISP_TARGET_MODE
void
isp_intr_atioq(ispsoftc_t *isp)
{
	void *addr;
	uint32_t iptr, optr, oop;

	iptr = ISP_READ(isp, BIU2400_ATIO_RSPINP);
	optr = isp->isp_atioodx;
	while (optr != iptr) {
		oop = optr;
		MEMORYBARRIER(isp, SYNC_ATIOQ, oop, QENTRY_LEN, -1);
		addr = ISP_QUEUE_ENTRY(isp->isp_atioq, oop);
		switch (((isphdr_t *)addr)->rqs_entry_type) {
		case RQSTYPE_NOTIFY:
		case RQSTYPE_ATIO:
		case RQSTYPE_NOTIFY_ACK:	/* Can be set to ATIO queue.*/
		case RQSTYPE_ABTS_RCVD:		/* Can be set to ATIO queue.*/
			(void) isp_target_notify(isp, addr, &oop,
			    ATIO_QUEUE_LEN(isp));
			break;
		case RQSTYPE_RPT_ID_ACQ:	/* Can be set to ATIO queue.*/
		default:
			isp_print_qentry(isp, "?ATIOQ entry?", oop, addr);
			break;
		}
		optr = ISP_NXT_QENTRY(oop, ATIO_QUEUE_LEN(isp));
	}
	if (isp->isp_atioodx != optr) {
		ISP_WRITE(isp, BIU2400_ATIO_RSPOUTP, optr);
		isp->isp_atioodx = optr;
	}
}
#endif

void
isp_intr_mbox(ispsoftc_t *isp, uint16_t mbox0)
{
	int i, obits;

	if (!isp->isp_mboxbsy) {
		isp_prt(isp, ISP_LOGWARN, "mailbox 0x%x with no waiters", mbox0);
		return;
	}
	obits = isp->isp_obits;
	isp->isp_mboxtmp[0] = mbox0;
	for (i = 1; i < ISP_NMBOX(isp); i++) {
		if ((obits & (1 << i)) == 0)
			continue;
		isp->isp_mboxtmp[i] = ISP_READ(isp, MBOX_OFF(i));
	}
	isp->isp_mboxbsy = 0;
}

void
isp_intr_respq(ispsoftc_t *isp)
{
	XS_T *xs, *cont_xs;
	uint8_t qe[QENTRY_LEN];
	isp24xx_statusreq_t *sp = (isp24xx_statusreq_t *)qe;
	ispstatus_cont_t *scp = (ispstatus_cont_t *)qe;
	isphdr_t *hp;
	uint8_t *resp, *snsp, etype;
	uint16_t scsi_status;
	uint32_t iptr, cont = 0, cptr, optr, rlen, slen, totslen;
#ifdef	ISP_TARGET_MODE
	uint32_t sptr;
#endif

	/*
	 * We can't be getting this now.
	 */
	if (isp->isp_state != ISP_RUNSTATE) {
		isp_prt(isp, ISP_LOGINFO, "respq interrupt when not ready");
		return;
	}

	iptr = ISP_READ(isp, BIU2400_RSPINP);
	optr = isp->isp_resodx;
	while (optr != iptr) {
		cptr = optr;
#ifdef	ISP_TARGET_MODE
		sptr = optr;
#endif
		hp = (isphdr_t *) ISP_QUEUE_ENTRY(isp->isp_result, cptr);
		optr = ISP_NXT_QENTRY(optr, RESULT_QUEUE_LEN(isp));

		/*
		 * Synchronize our view of this response queue entry.
		 */
		MEMORYBARRIER(isp, SYNC_RESULT, cptr, QENTRY_LEN, -1);
		if (isp->isp_dblev & ISP_LOGDEBUG1)
			isp_print_qentry(isp, "Response Queue Entry", cptr, hp);
		isp_get_hdr(isp, hp, &sp->req_header);

		/*
		 * Log IOCBs rejected by the firmware.  We can't really do
		 * much more about them, since it just should not happen.
		 */
		if (sp->req_header.rqs_flags & RQSFLAG_BADTYPE) {
			isp_print_qentry(isp, "invalid entry type", cptr, hp);
			continue;
		}
		if (sp->req_header.rqs_flags & RQSFLAG_BADPARAM) {
			isp_print_qentry(isp, "invalid entry parameter", cptr, hp);
			continue;
		}
		if (sp->req_header.rqs_flags & RQSFLAG_BADCOUNT) {
			isp_print_qentry(isp, "invalid entry count", cptr, hp);
			continue;
		}
		if (sp->req_header.rqs_flags & RQSFLAG_BADORDER) {
			isp_print_qentry(isp, "invalid entry order", cptr, hp);
			continue;
		}

		etype = sp->req_header.rqs_entry_type;

		/* We expected Status Continuation, but got different IOCB. */
		if (cont > 0 && etype != RQSTYPE_STATUS_CONT) {
			cont = 0;
			isp_done(cont_xs);
		}

		if (isp_handle_control(isp, hp)) {
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			continue;
		}

		switch (etype) {
		case RQSTYPE_RESPONSE:
			isp_get_24xx_response(isp, (isp24xx_statusreq_t *)hp, sp);
			break;
		case RQSTYPE_MARKER:
			isp_prt(isp, ISP_LOG_WARN1, "Marker Response");
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			continue;
		case RQSTYPE_STATUS_CONT:
			isp_get_cont_response(isp, (ispstatus_cont_t *)hp, scp);
			if (cont > 0) {
				slen = min(cont, sizeof(scp->req_sense_data));
				XS_SENSE_APPEND(cont_xs, scp->req_sense_data, slen);
				cont -= slen;
				if (cont == 0) {
					isp_done(cont_xs);
				} else {
					isp_prt(isp, ISP_LOGDEBUG0|ISP_LOG_CWARN,
					    "Expecting Status Continuations for %u bytes",
					    cont);
				}
			} else {
				isp_prt(isp, ISP_LOG_WARN1, "Ignored Continuation Response");
			}
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			continue;
#ifdef	ISP_TARGET_MODE
		case RQSTYPE_NOTIFY_ACK:	/* Can be set to ATIO queue. */
		case RQSTYPE_CTIO7:
		case RQSTYPE_ABTS_RCVD:		/* Can be set to ATIO queue. */
		case RQSTYPE_ABTS_RSP:
			isp_target_notify(isp, hp, &cptr, RESULT_QUEUE_LEN(isp));
			/* More then one IOCB could be consumed. */
			while (sptr != cptr) {
				ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
				sptr = ISP_NXT_QENTRY(sptr, RESULT_QUEUE_LEN(isp));
				hp = (isphdr_t *)ISP_QUEUE_ENTRY(isp->isp_result, sptr);
			}
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			optr = ISP_NXT_QENTRY(cptr, RESULT_QUEUE_LEN(isp));
			continue;
#endif
		case RQSTYPE_RPT_ID_ACQ:	/* Can be set to ATIO queue.*/
			isp_handle_rpt_id_acq(isp, hp);
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			continue;
		default:
			/* We don't know what was this -- log and skip. */
			isp_prt(isp, ISP_LOGERR, notresp, etype, cptr, optr);
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			continue;
		}

		xs = isp_find_xs(isp, sp->req_handle);
		if (xs == NULL) {
			/*
			 * Only whine if this isn't the expected fallout of
			 * aborting the command or resetting the target.
			 */
			if (sp->req_completion_status != RQCS_ABORTED &&
			    sp->req_completion_status != RQCS_RESET_OCCURRED)
				isp_prt(isp, ISP_LOGERR, "cannot find handle 0x%x (status 0x%x)",
				    sp->req_handle, sp->req_completion_status);
			ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */
			continue;
		}

		resp = snsp = sp->req_rsp_sense;
		rlen = slen = totslen = 0;
		scsi_status = sp->req_scsi_status;
		if (scsi_status & RQCS_RV) {
			rlen = sp->req_response_len;
			snsp += rlen;
		}
		if (scsi_status & RQCS_SV) {
			totslen = sp->req_sense_len;
			slen = MIN(totslen, sizeof(sp->req_rsp_sense) - rlen);
		}
		*XS_STSP(xs) = scsi_status & 0xff;
		if (scsi_status & RQCS_RESID)
			XS_SET_RESID(xs, sp->req_fcp_residual);
		else
			XS_SET_RESID(xs, 0);

		if (rlen >= 4 && resp[FCP_RSPNS_CODE_OFFSET] != 0) {
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
			if (code >= nitems(rnames) || rnames[code] == NULL) {
				ISP_SNPRINTF(lb, sizeof(lb),
				    "Unknown FCP Response Code 0x%x", code);
				ptr = lb;
			} else {
				ptr = rnames[code];
			}
			isp_xs_prt(isp, xs, ISP_LOGWARN,
			    "FCP RESPONSE, LENGTH %u: %s CDB0=0x%02x",
			    rlen, ptr, XS_CDBP(xs)[0] & 0xff);
			if (code != FCP_RSPNS_TMF_DONE &&
			    code != FCP_RSPNS_TMF_SUCCEEDED)
				XS_SETERR(xs, HBA_BOTCH);
		}
		isp_parse_status_24xx(isp, sp, xs);
		if (slen > 0) {
			XS_SAVE_SENSE(xs, snsp, slen);
			if (totslen > slen) {
				cont = totslen - slen;
				cont_xs = xs;
				isp_prt(isp, ISP_LOGDEBUG0|ISP_LOG_CWARN,
				    "Expecting Status Continuations for %u bytes",
				    cont);
			}
		}

		ISP_DMAFREE(isp, xs);
		isp_destroy_handle(isp, sp->req_handle);
		ISP_MEMZERO(hp, QENTRY_LEN);	/* PERF */

		/* Complete command if we expect no Status Continuations. */
		if (cont == 0)
			isp_done(xs);
	}

	/* We haven't received all Status Continuations, but that is it. */
	if (cont > 0)
		isp_done(cont_xs);

	/* If we processed any IOCBs, let ISP know about it. */
	if (optr != isp->isp_resodx) {
		ISP_WRITE(isp, BIU2400_RSPOUTP, optr);
		isp->isp_resodx = optr;
	}
}


void
isp_intr_async(ispsoftc_t *isp, uint16_t mbox)
{
	fcparam *fcp;
	uint16_t chan;

	isp_prt(isp, ISP_LOGDEBUG2, "Async Mbox 0x%x", mbox);

	switch (mbox) {
	case ASYNC_SYSTEM_ERROR:
		isp->isp_state = ISP_CRASHED;
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			FCPARAM(isp, chan)->isp_loopstate = LOOP_NIL;
			isp_change_fw_state(isp, chan, FW_CONFIG_WAIT);
		}
		/*
		 * Were we waiting for a mailbox command to complete?
		 * If so, it's dead, so wake up the waiter.
		 */
		if (isp->isp_mboxbsy) {
			isp->isp_obits = 1;
			isp->isp_mboxtmp[0] = MBOX_HOST_INTERFACE_ERROR;
			isp->isp_mboxbsy = 0;
		}
		/*
		 * It's up to the handler for isp_async to reinit stuff and
		 * restart the firmware
		 */
		isp_async(isp, ISPASYNC_FW_CRASH);
		break;

	case ASYNC_RQS_XFER_ERR:
		isp_prt(isp, ISP_LOGERR, "Request Queue Transfer Error");
		break;

	case ASYNC_RSP_XFER_ERR:
		isp_prt(isp, ISP_LOGERR, "Response Queue Transfer Error");
		break;

	case ASYNC_ATIO_XFER_ERR:
		isp_prt(isp, ISP_LOGERR, "ATIO Queue Transfer Error");
		break;

	case ASYNC_LIP_OCCURRED:
	case ASYNC_LIP_NOS_OLS_RECV:
	case ASYNC_LIP_ERROR:
	case ASYNC_PTPMODE:
		/*
		 * These are broadcast events that have to be sent across
		 * all active channels.
		 */
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			fcp = FCPARAM(isp, chan);
			int topo = fcp->isp_topo;

			if (fcp->role == ISP_ROLE_NONE)
				continue;
			if (fcp->isp_loopstate > LOOP_HAVE_LINK)
				fcp->isp_loopstate = LOOP_HAVE_LINK;
			ISP_SET_SENDMARKER(isp, chan, 1);
			isp_async(isp, ISPASYNC_LIP, chan);
#ifdef	ISP_TARGET_MODE
			isp_target_async(isp, chan, mbox);
#endif
			/*
			 * We've had problems with data corruption occurring on
			 * commands that complete (with no apparent error) after
			 * we receive a LIP. This has been observed mostly on
			 * Local Loop topologies. To be safe, let's just mark
			 * all active initiator commands as dead.
			 */
			if (topo == TOPO_NL_PORT || topo == TOPO_FL_PORT) {
				int i, j;
				for (i = j = 0; i < ISP_HANDLE_NUM(isp); i++) {
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
			fcp = FCPARAM(isp, chan);
			if (fcp->role == ISP_ROLE_NONE)
				continue;
			fcp->isp_linkstate = 1;
			if (fcp->isp_loopstate < LOOP_HAVE_LINK)
				fcp->isp_loopstate = LOOP_HAVE_LINK;
			ISP_SET_SENDMARKER(isp, chan, 1);
			isp_async(isp, ISPASYNC_LOOP_UP, chan);
#ifdef	ISP_TARGET_MODE
			isp_target_async(isp, chan, mbox);
#endif
		}
		break;

	case ASYNC_LOOP_DOWN:
		/*
		 * This is a broadcast event that has to be sent across
		 * all active channels.
		 */
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			fcp = FCPARAM(isp, chan);
			if (fcp->role == ISP_ROLE_NONE)
				continue;
			ISP_SET_SENDMARKER(isp, chan, 1);
			fcp->isp_linkstate = 0;
			fcp->isp_loopstate = LOOP_NIL;
			isp_async(isp, ISPASYNC_LOOP_DOWN, chan);
#ifdef	ISP_TARGET_MODE
			isp_target_async(isp, chan, mbox);
#endif
		}
		break;

	case ASYNC_LOOP_RESET:
		/*
		 * This is a broadcast event that has to be sent across
		 * all active channels.
		 */
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			fcp = FCPARAM(isp, chan);
			if (fcp->role == ISP_ROLE_NONE)
				continue;
			ISP_SET_SENDMARKER(isp, chan, 1);
			if (fcp->isp_loopstate > LOOP_HAVE_LINK)
				fcp->isp_loopstate = LOOP_HAVE_LINK;
			isp_async(isp, ISPASYNC_LOOP_RESET, chan);
#ifdef	ISP_TARGET_MODE
			isp_target_async(isp, chan, mbox);
#endif
		}
		break;

	case ASYNC_PDB_CHANGED:
	{
		int echan, nphdl, nlstate, reason;

		nphdl = ISP_READ(isp, OUTMAILBOX1);
		nlstate = ISP_READ(isp, OUTMAILBOX2);
		reason = ISP_READ(isp, OUTMAILBOX3) >> 8;
		if (ISP_CAP_MULTI_ID(isp)) {
			chan = ISP_READ(isp, OUTMAILBOX3) & 0xff;
			if (chan == 0xff || nphdl == NIL_HANDLE) {
				chan = 0;
				echan = isp->isp_nchan - 1;
			} else if (chan >= isp->isp_nchan) {
				break;
			} else {
				echan = chan;
			}
		} else {
			chan = echan = 0;
		}
		for (; chan <= echan; chan++) {
			fcp = FCPARAM(isp, chan);
			if (fcp->role == ISP_ROLE_NONE)
				continue;
			if (fcp->isp_loopstate > LOOP_LTEST_DONE) {
				if (nphdl != NIL_HANDLE &&
				    nphdl == fcp->isp_login_hdl &&
				    reason == PDB24XX_AE_OPN_2)
					continue;
				fcp->isp_loopstate = LOOP_LTEST_DONE;
			} else if (fcp->isp_loopstate < LOOP_HAVE_LINK)
				fcp->isp_loopstate = LOOP_HAVE_LINK;
			isp_async(isp, ISPASYNC_CHANGE_NOTIFY, chan,
			    ISPASYNC_CHANGE_PDB, nphdl, nlstate, reason);
		}
		break;
	}
	case ASYNC_CHANGE_NOTIFY:
	{
		int portid;

		portid = ((ISP_READ(isp, OUTMAILBOX1) & 0xff) << 16) |
		    ISP_READ(isp, OUTMAILBOX2);
		if (ISP_CAP_MULTI_ID(isp)) {
			chan = ISP_READ(isp, OUTMAILBOX3) & 0xff;
			if (chan >= isp->isp_nchan)
				break;
		} else {
			chan = 0;
		}
		fcp = FCPARAM(isp, chan);
		if (fcp->role == ISP_ROLE_NONE)
			break;
		if (fcp->isp_loopstate > LOOP_LTEST_DONE)
			fcp->isp_loopstate = LOOP_LTEST_DONE;
		else if (fcp->isp_loopstate < LOOP_HAVE_LINK)
			fcp->isp_loopstate = LOOP_HAVE_LINK;
		isp_async(isp, ISPASYNC_CHANGE_NOTIFY, chan,
		    ISPASYNC_CHANGE_SNS, portid);
		break;
	}
	case ASYNC_ERR_LOGGING_DISABLED:
		isp_prt(isp, ISP_LOGWARN, "Error logging disabled (reason 0x%x)",
		    ISP_READ(isp, OUTMAILBOX1));
		break;
	case ASYNC_P2P_INIT_ERR:
		isp_prt(isp, ISP_LOGWARN, "P2P init error (reason 0x%x)",
		    ISP_READ(isp, OUTMAILBOX1));
		break;
	case ASYNC_RCV_ERR:
		isp_prt(isp, ISP_LOGWARN, "Receive Error");
		break;
	case ASYNC_RJT_SENT:	/* same as ASYNC_QFULL_SENT */
		isp_prt(isp, ISP_LOGTDEBUG0, "LS_RJT sent");
		break;
	case ASYNC_FW_RESTART_COMPLETE:
		isp_prt(isp, ISP_LOGDEBUG0, "FW restart complete");
		break;
	case ASYNC_TEMPERATURE_ALERT:
		isp_prt(isp, ISP_LOGERR, "Temperature alert (subcode 0x%x)",
		    ISP_READ(isp, OUTMAILBOX1));
		break;
	case ASYNC_INTER_DRIVER_COMP:
		isp_prt(isp, ISP_LOGDEBUG0, "Inter-driver communication complete");
		break;
	case ASYNC_INTER_DRIVER_NOTIFY:
		isp_prt(isp, ISP_LOGDEBUG0, "Inter-driver communication notification");
		break;
	case ASYNC_INTER_DRIVER_TIME_EXT:
		isp_prt(isp, ISP_LOGDEBUG0, "Inter-driver communication time extended");
		break;
	case ASYNC_TRANSCEIVER_INSERTION:
		isp_prt(isp, ISP_LOGDEBUG0, "Transceiver insertion (0x%x)",
		    ISP_READ(isp, OUTMAILBOX1));
		break;
	case ASYNC_TRANSCEIVER_REMOVAL:
		isp_prt(isp, ISP_LOGDEBUG0, "Transceiver removal");
		break;
	case ASYNC_NIC_FW_STATE_CHANGE:
		isp_prt(isp, ISP_LOGDEBUG0, "NIC Firmware State Change");
		break;
	case ASYNC_AUTOLOAD_FW_COMPLETE:
		isp_prt(isp, ISP_LOGDEBUG0, "Autoload FW init complete");
		break;
	case ASYNC_AUTOLOAD_FW_FAILURE:
		isp_prt(isp, ISP_LOGERR, "Autoload FW init failure");
		break;
	default:
		isp_prt(isp, ISP_LOGWARN, "Unknown Async Code 0x%x", mbox);
		break;
	}
}

/*
 * Handle completions with control handles by waking up waiting threads.
 */
static int
isp_handle_control(ispsoftc_t *isp, isphdr_t *hp)
{
	uint32_t hdl;
	void *ptr;

	switch (hp->rqs_entry_type) {
	case RQSTYPE_RESPONSE:
	case RQSTYPE_MARKER:
	case RQSTYPE_NOTIFY_ACK:
	case RQSTYPE_CTIO7:
	case RQSTYPE_TSK_MGMT:
	case RQSTYPE_CT_PASSTHRU:
	case RQSTYPE_VP_MODIFY:
	case RQSTYPE_VP_CTRL:
	case RQSTYPE_ABORT_IO:
	case RQSTYPE_MBOX:
	case RQSTYPE_LOGIN:
	case RQSTYPE_ELS_PASSTHRU:
		ISP_IOXGET_32(isp, (uint32_t *)(hp + 1), hdl);
		if (ISP_H2HT(hdl) != ISP_HANDLE_CTRL)
			break;
		ptr = isp_find_xs(isp, hdl);
		if (ptr != NULL) {
			isp_destroy_handle(isp, hdl);
			memcpy(ptr, hp, QENTRY_LEN);
			wakeup(ptr);
		}
		return (1);
	}
	return (0);
}

static void
isp_handle_rpt_id_acq(ispsoftc_t *isp, isphdr_t *hp)
{
	fcparam *fcp;
	isp_ridacq_t rid;
	int chan, c;
	uint32_t portid;

	isp_get_ridacq(isp, (isp_ridacq_t *)hp, &rid);
	portid = (uint32_t)rid.ridacq_vp_port_hi << 16 |
	    rid.ridacq_vp_port_lo;
	if (rid.ridacq_format == 0) {
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			fcp = FCPARAM(isp, chan);
			if (fcp->role == ISP_ROLE_NONE)
				continue;
			c = (chan == 0) ? 127 : (chan - 1);
			if (rid.ridacq_map[c / 16] & (1 << (c % 16)) ||
			    chan == 0) {
				fcp->isp_loopstate = LOOP_HAVE_LINK;
				isp_async(isp, ISPASYNC_CHANGE_NOTIFY,
				    chan, ISPASYNC_CHANGE_OTHER);
			} else {
				fcp->isp_loopstate = LOOP_NIL;
				isp_async(isp, ISPASYNC_LOOP_DOWN,
				    chan);
			}
		}
	} else {
		fcp = FCPARAM(isp, rid.ridacq_vp_index);
		if (rid.ridacq_vp_status == RIDACQ_STS_COMPLETE ||
		    rid.ridacq_vp_status == RIDACQ_STS_CHANGED) {
			fcp->isp_topo = (rid.ridacq_map[0] >> 9) & 0x7;
			fcp->isp_portid = portid;
			fcp->isp_loopstate = LOOP_HAVE_ADDR;
			isp_async(isp, ISPASYNC_CHANGE_NOTIFY,
			    rid.ridacq_vp_index, ISPASYNC_CHANGE_OTHER);
		} else {
			fcp->isp_loopstate = LOOP_NIL;
			isp_async(isp, ISPASYNC_LOOP_DOWN,
			    rid.ridacq_vp_index);
		}
	}
}

static void
isp_parse_status_24xx(ispsoftc_t *isp, isp24xx_statusreq_t *sp, XS_T *xs)
{
	int ru_marked, sv_marked;
	int chan = XS_CHANNEL(xs);

	switch (sp->req_completion_status) {
	case RQCS_COMPLETE:
		return;

	case RQCS_DMA_ERROR:
		isp_xs_prt(isp, xs, ISP_LOGERR, "DMA error");
		if (XS_NOERR(xs))
			XS_SETERR(xs, HBA_BOTCH);
		break;

	case RQCS_TRANSPORT_ERROR:
		isp_xs_prt(isp, xs,  ISP_LOGERR, "Transport Error");
		if (XS_NOERR(xs))
			XS_SETERR(xs, HBA_BOTCH);
		break;

	case RQCS_RESET_OCCURRED:
		isp_xs_prt(isp, xs, ISP_LOGWARN, "reset destroyed command");
		FCPARAM(isp, chan)->sendmarker = 1;
		if (XS_NOERR(xs))
			XS_SETERR(xs, HBA_BUSRESET);
		return;

	case RQCS_ABORTED:
		isp_xs_prt(isp, xs, ISP_LOGERR, "Command Aborted");
		FCPARAM(isp, chan)->sendmarker = 1;
		if (XS_NOERR(xs))
			XS_SETERR(xs, HBA_ABORTED);
		return;

	case RQCS_TIMEOUT:
		isp_xs_prt(isp, xs, ISP_LOGWARN, "Command Timed Out");
		if (XS_NOERR(xs))
			XS_SETERR(xs, HBA_CMDTIMEOUT);
		return;

	case RQCS_DATA_OVERRUN:
		XS_SET_RESID(xs, sp->req_resid);
		isp_xs_prt(isp, xs, ISP_LOGERR, "Data Overrun");
		if (XS_NOERR(xs))
			XS_SETERR(xs, HBA_DATAOVR);
		return;

	case RQCS_DRE:		/* data reassembly error */
		isp_prt(isp, ISP_LOGERR, "Chan %d data reassembly error for target %d", chan, XS_TGT(xs));
		if (XS_NOERR(xs))
			XS_SETERR(xs, HBA_BOTCH);
		return;

	case RQCS_TABORT:	/* aborted by target */
		isp_prt(isp, ISP_LOGERR, "Chan %d target %d sent ABTS", chan, XS_TGT(xs));
		if (XS_NOERR(xs))
			XS_SETERR(xs, HBA_ABORTED);
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
			if (XS_NOERR(xs))
				XS_SETERR(xs, HBA_BOTCH);
			return;
		}
		XS_SET_RESID(xs, sp->req_resid);
		isp_xs_prt(isp, xs, ISP_LOG_WARN1, "Data Underrun (%d) for command 0x%x", sp->req_resid, XS_CDBP(xs)[0] & 0xff);
		return;

	case RQCS_PORT_UNAVAILABLE:
		/*
		 * No such port on the loop. Moral equivalent of SELTIMEO
		 */
	case RQCS_PORT_LOGGED_OUT:
	{
		const char *reason;
		uint8_t sts = sp->req_completion_status & 0xff;
		fcparam *fcp = FCPARAM(isp, XS_CHANNEL(xs));
		fcportdb_t *lp;

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

		/* XXX: Should we trigger rescan or FW announce change? */

		if (XS_NOERR(xs)) {
			lp = &fcp->portdb[XS_TGT(xs)];
			if (lp->state == FC_PORTDB_STATE_ZOMBIE) {
				*XS_STSP(xs) = SCSI_BUSY;
				XS_SETERR(xs, HBA_TGTBSY);
			} else
				XS_SETERR(xs, HBA_SELTIMEOUT);
		}
		return;
	}
	case RQCS_PORT_CHANGED:
		isp_prt(isp, ISP_LOGWARN, "port changed for target %d chan %d", XS_TGT(xs), chan);
		if (XS_NOERR(xs)) {
			*XS_STSP(xs) = SCSI_BUSY;
			XS_SETERR(xs, HBA_TGTBSY);
		}
		return;

	case RQCS_ENOMEM:	/* f/w resource unavailable */
		isp_prt(isp, ISP_LOGWARN, "f/w resource unavailable for target %d chan %d", XS_TGT(xs), chan);
		if (XS_NOERR(xs)) {
			*XS_STSP(xs) = SCSI_BUSY;
			XS_SETERR(xs, HBA_TGTBSY);
		}
		return;

	case RQCS_TMO:		/* task management overrun */
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
	if (XS_NOERR(xs))
		XS_SETERR(xs, HBA_BOTCH);
}

#define	ISP_FC_IBITS(op)	((mbpfc[((op)<<3) + 0] << 24) | (mbpfc[((op)<<3) + 1] << 16) | (mbpfc[((op)<<3) + 2] << 8) | (mbpfc[((op)<<3) + 3]))
#define	ISP_FC_OBITS(op)	((mbpfc[((op)<<3) + 4] << 24) | (mbpfc[((op)<<3) + 5] << 16) | (mbpfc[((op)<<3) + 6] << 8) | (mbpfc[((op)<<3) + 7]))

#define	ISP_FC_OPMAP(in0, out0)							  0,   0,   0, in0,    0,    0,    0, out0
#define	ISP_FC_OPMAP_HALF(in1, in0, out1, out0)					  0,   0, in1, in0,    0,    0, out1, out0
#define	ISP_FC_OPMAP_FULL(in3, in2, in1, in0, out3, out2, out1, out0)		in3, in2, in1, in0, out3, out2, out1, out0
static const uint32_t mbpfc[] = {
	ISP_FC_OPMAP(0x01, 0x01),	/* 0x00: MBOX_NO_OP */
	ISP_FC_OPMAP(0x1f, 0x01),	/* 0x01: MBOX_LOAD_RAM */
	ISP_FC_OPMAP_HALF(0x07, 0xff, 0x00, 0x1f),	/* 0x02: MBOX_EXEC_FIRMWARE */
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
	ISP_FC_OPMAP_HALF(0x1, 0xff, 0x0, 0x03),	/* 0x14: MBOX_STOP_FIRMWARE */
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
	ISP_FC_OPMAP_HALF(0x2, 0x01, 0x7e, 0xcf),	/* 0x20: MBOX_GET_LOOP_ID */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x21: */
	ISP_FC_OPMAP(0x03, 0x4b),	/* 0x22: MBOX_GET_TIMEOUT_PARAMS */
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
	ISP_FC_OPMAP(0x4b, 0x4b),	/* 0x32: MBOX_SET_TIMEOUT_PARAMS */
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
	ISP_FC_OPMAP_HALF(0x0, 0x01, 0x1f, 0xcf),	/* 0x42: MBOX_GET_RESOURCE_COUNT */
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
	ISP_FC_OPMAP(0x07, 0x1f),	/* 0x5d: MBOX_GET_SET_DATA_RATE */
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
	ISP_FC_OPMAP_HALF(0x00, 0x01, 0x0f, 0x1f),	/* 0x69: MBOX_GET_FW_STATE */
	ISP_FC_OPMAP_HALF(0x6, 0x03, 0x0, 0xcf),	/* 0x6a: MBOX_GET_PORT_NAME */
	ISP_FC_OPMAP(0xcf, 0x01),	/* 0x6b: MBOX_GET_LINK_STATUS */
	ISP_FC_OPMAP(0x0f, 0x01),	/* 0x6c: MBOX_INIT_LIP_RESET */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x6d: */
	ISP_FC_OPMAP(0xcf, 0x03),	/* 0x6e: MBOX_SEND_SNS */
	ISP_FC_OPMAP(0x0f, 0x07),	/* 0x6f: MBOX_FABRIC_LOGIN */
	ISP_FC_OPMAP_HALF(0x02, 0x03, 0x00, 0x03),	/* 0x70: MBOX_SEND_CHANGE_REQUEST */
	ISP_FC_OPMAP(0x03, 0x03),	/* 0x71: MBOX_FABRIC_LOGOUT */
	ISP_FC_OPMAP(0x0f, 0x0f),	/* 0x72: MBOX_INIT_LIP_LOGIN */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x73: */
	ISP_FC_OPMAP(0x07, 0x01),	/* 0x74: LOGIN LOOP PORT */
	ISP_FC_OPMAP_HALF(0x03, 0xcf, 0x00, 0x07),	/* 0x75: GET PORT/NODE NAME LIST */
	ISP_FC_OPMAP(0x4f, 0x01),	/* 0x76: SET VENDOR ID */
	ISP_FC_OPMAP(0xcd, 0x01),	/* 0x77: INITIALIZE IP MAILBOX */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x78: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x79: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x7a: */
	ISP_FC_OPMAP(0x00, 0x00),	/* 0x7b: */
	ISP_FC_OPMAP_HALF(0x03, 0x4f, 0x00, 0x07),	/* 0x7c: Get ID List */
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
	"NO-OP",			/* 00h */
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
	"DUMP RISC RAM",
	"WRITE RAM WORD EXTENDED",
	"CHECK FIRMWARE",
	"READ RAM WORD EXTENDED",
	"INIT REQUEST QUEUE",		/* 10h */
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
	"GET LOOP ID",			/* 20h */
	NULL,
	"GET TIMEOUT PARAMS",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"GET FIRMWARE OPTIONS",
	"GET PORT QUEUE PARAMS",
	"GENERATE SYSTEM ERROR",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"WRITE SFP",			/* 30h */
	"READ SFP",
	"SET TIMEOUT PARAMS",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"SET FIRMWARE OPTIONS",
	"SET PORT QUEUE PARAMS",
	NULL,
	"SET FC LED CONF",
	NULL,
	"RESTART NIC FIRMWARE",
	"ACCESS CONTROL",
	NULL,
	"LOOP PORT BYPASS",		/* 40h */
	"LOOP PORT ENABLE",
	"GET RESOURCE COUNT",
	"REQUEST NON PARTICIPATING MODE",
	"DIAGNOSTIC ECHO TEST",
	"DIAGNOSTIC LOOPBACK",
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
	"GET FCF LIST",			/* 50h */
	"GET DCBX PARAMETERS",
	NULL,
	"HOST MEMORY COPY",
	"EXECUTE IOCB A64",
	NULL,
	NULL,
	"SEND RNID",
	NULL,
	"SET PARAMETERS",
	"GET PARAMETERS",
	"DRIVER HEARTBEAT",
	"FIRMWARE HEARTBEAT",
	"GET/SET DATA RATE",
	"SEND RNFT",
	NULL,
	"INIT FIRMWARE",		/* 60h */
	"GET INIT CONTROL BLOCK",
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
	"GET LINK STATS & PRIVATE DATA CNTS",
	"SEND SNS",
	"FABRIC LOGIN",
	"SEND CHANGE REQUEST",		/* 70h */
	"FABRIC LOGOUT",
	"INIT LIP LOGIN",
	NULL,
	"LOGIN LOOP PORT",
	"GET PORT/NODE NAME LIST",
	"SET VENDOR ID",
	"INITIALIZE IP MAILBOX",
	NULL,
	NULL,
	"GET XGMAC STATS",
	NULL,
	"GET ID LIST",
	"SEND LFA",
	"LUN RESET"
};

static void
isp_mboxcmd(ispsoftc_t *isp, mbreg_t *mbp)
{
	const char *cname, *xname, *sname;
	char tname[16], mname[16];
	unsigned int ibits, obits, box, opcode, t, to;

	opcode = mbp->param[0];
	if (opcode > MAX_FC_OPCODE) {
		mbp->param[0] = MBOX_INVALID_COMMAND;
		isp_prt(isp, ISP_LOGERR, "Unknown Command 0x%x", opcode);
		return;
	}
	cname = fc_mbcmd_names[opcode];
	ibits = ISP_FC_IBITS(opcode);
	obits = ISP_FC_OBITS(opcode);
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

	for (box = 0; box < ISP_NMBOX(isp); box++) {
		if (ibits & (1 << box)) {
			isp_prt(isp, ISP_LOGDEBUG3, "IN mbox %d = 0x%04x", box,
			    mbp->param[box]);
			ISP_WRITE(isp, MBOX_OFF(box), mbp->param[box]);
		}
		isp->isp_mboxtmp[box] = mbp->param[box] = 0;
	}

	isp->isp_obits = obits;
	isp->isp_mboxbsy = 1;

	/*
	 * Set Host Interrupt condition so that RISC will pick up mailbox regs.
	 */
	ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_SET_HOST_INT);

	/*
	 * While we haven't finished the command, spin our wheels here.
	 */
	to = (mbp->timeout == 0) ? MBCMD_DEFAULT_TIMEOUT : mbp->timeout;
	for (t = 0; t < to; t += 100) {
		if (!isp->isp_mboxbsy)
			break;
		ISP_RUN_ISR(isp);
		if (!isp->isp_mboxbsy)
			break;
		ISP_DELAY(100);
	}

	/*
	 * Did the command time out?
	 */
	if (isp->isp_mboxbsy) {
		isp->isp_mboxbsy = 0;
		isp_prt(isp, ISP_LOGWARN, "Mailbox Command (0x%x) Timeout (%uus) (%s:%d)",
		    opcode, to, mbp->func, mbp->lineno);
		mbp->param[0] = MBOX_TIMEOUT;
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

out:
	if (mbp->logval == 0 || mbp->param[0] == MBOX_COMMAND_COMPLETE)
		return;

	if ((mbp->param[0] & 0xbfe0) == 0 &&
	    (mbp->logval & MBLOGMASK(mbp->param[0])) == 0)
		return;

	xname = NULL;
	sname = "";
	switch (mbp->param[0]) {
	case MBOX_INVALID_COMMAND:
		xname = "INVALID COMMAND";
		break;
	case MBOX_HOST_INTERFACE_ERROR:
		xname = "HOST INTERFACE ERROR";
		break;
	case MBOX_TEST_FAILED:
		xname = "TEST FAILED";
		break;
	case MBOX_COMMAND_ERROR:
		xname = "COMMAND ERROR";
		ISP_SNPRINTF(mname, sizeof(mname), " subcode 0x%x",
		    mbp->param[1]);
		sname = mname;
		break;
	case MBOX_COMMAND_PARAM_ERROR:
		xname = "COMMAND PARAMETER ERROR";
		break;
	case MBOX_PORT_ID_USED:
		xname = "PORT ID ALREADY IN USE";
		break;
	case MBOX_LOOP_ID_USED:
		xname = "LOOP ID ALREADY IN USE";
		break;
	case MBOX_ALL_IDS_USED:
		xname = "ALL LOOP IDS IN USE";
		break;
	case MBOX_NOT_LOGGED_IN:
		xname = "NOT LOGGED IN";
		break;
	case MBOX_LINK_DOWN_ERROR:
		xname = "LINK DOWN ERROR";
		break;
	case MBOX_LOOPBACK_ERROR:
		xname = "LOOPBACK ERROR";
		break;
	case MBOX_CHECKSUM_ERROR:
		xname = "CHECKSUM ERROR";
		break;
	case MBOX_INVALID_PRODUCT_KEY:
		xname = "INVALID PRODUCT KEY";
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
		isp_prt(isp, ISP_LOGALL, "Mailbox Command '%s' failed (%s%s)",
		    cname, xname, sname);
	}
}

static int
isp_fw_state(ispsoftc_t *isp, int chan)
{
	mbreg_t mbs;

	MBSINIT(&mbs, MBOX_GET_FW_STATE, MBLOGALL, 0);
	isp_mboxcmd(isp, &mbs);
	if (mbs.param[0] == MBOX_COMMAND_COMPLETE)
		return (mbs.param[1]);
	return (FW_ERROR);
}

static void
isp_setdfltfcparm(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);

	/*
	 * Establish some default parameters.
	 */
	fcp->role = DEFAULT_ROLE(isp, chan);
	fcp->isp_retry_delay = ICB_DFLT_RDELAY;
	fcp->isp_retry_count = ICB_DFLT_RCOUNT;
	fcp->isp_loopid = DEFAULT_LOOPID(isp, chan);
	fcp->isp_wwnn_nvram = DEFAULT_NODEWWN(isp, chan);
	fcp->isp_wwpn_nvram = DEFAULT_PORTWWN(isp, chan);
	fcp->isp_fwoptions = 0;
	fcp->isp_xfwoptions = 0;
	fcp->isp_zfwoptions = 0;
	fcp->isp_lasthdl = NIL_HANDLE;
	fcp->isp_login_hdl = NIL_HANDLE;

	fcp->isp_fwoptions |= ICB2400_OPT1_FAIRNESS;
	fcp->isp_fwoptions |= ICB2400_OPT1_HARD_ADDRESS;
	if (isp->isp_confopts & ISP_CFG_FULL_DUPLEX)
		fcp->isp_fwoptions |= ICB2400_OPT1_FULL_DUPLEX;
	fcp->isp_fwoptions |= ICB2400_OPT1_BOTH_WWNS;
	fcp->isp_xfwoptions |= ICB2400_OPT2_LOOP_2_PTP;
	fcp->isp_zfwoptions |= ICB2400_OPT3_RATE_AUTO;

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

	if (isp->isp_state > ISP_RESETSTATE)
		isp_stop(isp);
	if (isp->isp_state != ISP_RESETSTATE)
		isp_reset(isp, do_load_defaults);
	if (isp->isp_state != ISP_RESETSTATE) {
		res = EIO;
		isp_prt(isp, ISP_LOGERR, "%s: cannot reset card", __func__);
		goto cleanup;
	}

	isp_init(isp);
	if (isp->isp_state > ISP_RESETSTATE &&
	    isp->isp_state != ISP_RUNSTATE) {
		res = EIO;
		isp_prt(isp, ISP_LOGERR, "%s: cannot init card", __func__);
		ISP_DISABLE_INTS(isp);
	}

cleanup:
	isp_clear_commands(isp);
	for (i = 0; i < isp->isp_nchan; i++)
		isp_clear_portdb(isp, i);
	return (res);
}

/*
 * NVRAM Routines
 */
static int
isp_read_nvram(ispsoftc_t *isp, int bus)
{

	return (isp_read_nvram_2400(isp));
}

static int
isp_read_nvram_2400(ispsoftc_t *isp)
{
	int retval = 0;
	uint32_t addr, csum, lwrds, *dptr;
	uint8_t nvram_data[ISP2400_NVRAM_SIZE];

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
isp_rd_2400_nvram(ispsoftc_t *isp, uint32_t addr, uint32_t *rp)
{
	int loops = 0;
	uint32_t base = 0x7ffe0000;
	uint32_t tmp = 0;

	if (IS_26XX(isp)) {
		base = 0x7fe7c000;	/* XXX: Observation, may be wrong. */
	} else if (IS_25XX(isp)) {
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
isp_parse_nvram_2400(ispsoftc_t *isp, uint8_t *nvram_data)
{
	fcparam *fcp = FCPARAM(isp, 0);
	uint64_t wwn;

	isp_prt(isp, ISP_LOGDEBUG0,
	    "NVRAM 0x%08x%08x 0x%08x%08x maxframelen %d",
	    (uint32_t) (ISP2400_NVRAM_NODE_NAME(nvram_data) >> 32),
	    (uint32_t) (ISP2400_NVRAM_NODE_NAME(nvram_data)),
	    (uint32_t) (ISP2400_NVRAM_PORT_NAME(nvram_data) >> 32),
	    (uint32_t) (ISP2400_NVRAM_PORT_NAME(nvram_data)),
	    ISP2400_NVRAM_MAXFRAMELENGTH(nvram_data));
	isp_prt(isp, ISP_LOGDEBUG0,
	    "NVRAM loopid %d fwopt1 0x%x fwopt2 0x%x fwopt3 0x%x",
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

	if ((isp->isp_confopts & ISP_CFG_OWNFSZ) == 0) {
		DEFAULT_FRAMESIZE(isp) =
		    ISP2400_NVRAM_MAXFRAMELENGTH(nvram_data);
	}
	if ((isp->isp_confopts & ISP_CFG_OWNLOOPID) == 0) {
		fcp->isp_loopid = ISP2400_NVRAM_HARDLOOPID(nvram_data);
	}
	fcp->isp_fwoptions = ISP2400_NVRAM_FIRMWARE_OPTIONS1(nvram_data);
	fcp->isp_xfwoptions = ISP2400_NVRAM_FIRMWARE_OPTIONS2(nvram_data);
	fcp->isp_zfwoptions = ISP2400_NVRAM_FIRMWARE_OPTIONS3(nvram_data);
}
