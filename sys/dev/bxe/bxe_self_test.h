/*-
 * Copyright (c) 2007-2011 Broadcom Corporation. All rights reserved.
 *
 *    Gary Zambrano <zambrano@broadcom.com>
 *    David Christensen <davidch@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Broadcom Corporation nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*$FreeBSD$*/

#ifndef _BXE_SELF_TEST_H
#define	_BXE_SELF_TEST_H

static int idle_chk_errors;
static int idle_chk_warnings;

#ifdef BXE_DEBUG
extern uint32_t
bxe_reg_read32	(struct bxe_softc *, bus_size_t);
extern void
bxe_reg_write32	(struct bxe_softc *, bus_size_t, uint32_t);
#endif

#define	IDLE_CHK_E1			0x1
#define	IDLE_CHK_E1H			0x2

#define	IDLE_CHK_ERROR			1
#define	IDLE_CHK_ERROR_NO_TRAFFIC	2
#define	IDLE_CHK_WARNING		3

#define	CHIP_MASK_CHK(chip_mask)					\
	(((((chip_mask) & IDLE_CHK_E1) && is_e1) ||			\
	(((chip_mask) & IDLE_CHK_E1H) && is_e1h)) ? 1 : 0)

#define	CONDITION_CHK(condition, severity, fail_msg, arg_list...) do {	\
	if (condition) {						\
		switch (severity) {					\
		case IDLE_CHK_ERROR:					\
			BXE_PRINTF("ERROR " fail_msg, ##arg_list);	\
			idle_chk_errors++;				\
			break;						\
		case IDLE_CHK_ERROR_NO_TRAFFIC:				\
			BXE_PRINTF("WARNING " fail_msg, ##arg_list);	\
			break;						\
		case IDLE_CHK_WARNING:					\
			BXE_PRINTF("INFO " fail_msg, ##arg_list);	\
			idle_chk_warnings++;				\
			break;						\
		}							\
	}								\
} while (0)

/* Read one reg and check the condition. */
#define	IDLE_CHK_1(chip_mask, offset, condition, severity, fail_msg) do { \
	if (CHIP_MASK_CHK(chip_mask)) {					\
		val = REG_RD(sc, offset);				\
		CONDITION_CHK(condition, severity,			\
		    fail_msg ". Value is 0x%x\n", val);			\
	}								\
} while (0)

/* Loop to read one reg and check the condition. */
#define	IDLE_CHK_2(chip_mask, offset, loop, inc, condition, severity,	\
	fail_msg) do {							\
	if (CHIP_MASK_CHK(chip_mask))					\
		for (int i = 0; i < (loop); i++) {			\
			val = REG_RD(sc, offset + i * (inc));		\
			CONDITION_CHK(condition, severity,		\
			    fail_msg ". Value is 0x%x\n", i, val);	\
		}							\
} while (0)

/* Read two regs and check the condition. */
#define	IDLE_CHK_3(chip_mask, offset1, offset2, condition, severity, 	\
	fail_msg) do {							\
	if (CHIP_MASK_CHK(chip_mask)) {					\
		val1 = REG_RD(sc, offset1);				\
		val2 = REG_RD(sc, offset2);				\
		CONDITION_CHK(condition, severity, fail_msg		\
		    ". Values are 0x%x 0x%x\n", val1, val2);		\
	}								\
} while (0)

/* Loop to read two regs and check the condition. */
#define	IDLE_CHK_4(chip_mask, offset1, offset2, loop, inc, condition,	\
	severity, fail_msg) do {					\
	if (CHIP_MASK_CHK(chip_mask))					\
		for (int i = 0; i < (loop); i++) { 			\
			val1 = REG_RD(sc, offset1 + i * (inc));		\
			val2 = (REG_RD(sc, offset2 + i * (inc)) >> 1);	\
			CONDITION_CHK(condition, severity, fail_msg	\
			" - LCID %d CID_CAM 0x%x Value is 0x%x\n",	\
			i, val2, val1);					\
		} 							\
} while (0);

/* Read one reg and check according to another reg. */
#define	IDLE_CHK_5(chip_mask,offset,offset1,offset2,condition,severity,	\
	fail_msg) do {							\
	if (CHIP_MASK_CHK(chip_mask))					\
		if (!REG_RD(sc, offset))				\
			IDLE_CHK_3(chip_mask, offset1, offset2,		\
			    condition, severity, fail_msg);		\
} while (0);

/* Read wide-bus reg and check sub-fields. */
#define	IDLE_CHK_6(chip_mask, offset, loop, inc, severity)		\
	bxe_idle_chk6(sc, chip_mask, offset, loop, inc, severity)

/*
 * Idle check 6.
 *
 * Returns:
 *   None.
 */
static void
bxe_idle_chk6(struct bxe_softc *sc, uint32_t chip_mask, uint32_t offset,
    int loop, int inc, int severity)
{
	uint32_t val1, val2;
	uint32_t rd_ptr, wr_ptr, rd_bank, wr_bank;
	int i, is_e1, is_e1h;

	is_e1 = CHIP_IS_E1(sc);
	is_e1h = CHIP_IS_E1H(sc);

	if (!CHIP_MASK_CHK(chip_mask))
		return;

	for (i = 0; i < loop; i++) {
		val1 = REG_RD(sc, offset + i*inc);
		val2 = REG_RD(sc, offset + i*inc + 4);
		rd_ptr = ((val1 & 0x3FFFFFC0) >> 6);
		wr_ptr = ((((val1 & 0xC0000000) >> 30) & 0x3) |
		    ((val2 & 0x3FFFFF) << 2));
		CONDITION_CHK((rd_ptr != wr_ptr), severity,
		    "QM: PTRTBL entry %d - rd_ptr is not"
		    " equal to wr_ptr. Values are 0x%x 0x%x\n",
		    i, rd_ptr, wr_ptr);
		rd_bank = ((val1 & 0x30) >> 4);
		wr_bank = (val1 & 0x03);
		CONDITION_CHK((rd_bank != wr_bank), severity,
		    "QM: PTRTBL entry %d - rd_bank is not"
		    " equal to wr_bank. Values are 0x%x 0x%x\n",
		    i, rd_bank, wr_bank);
	}
}

/* Loop to read wide-bus reg and check according to another reg. */
#define	IDLE_CHK_7(chip_mask, offset, offset1, offset2, loop, inc,	\
	condition, severity, fail_msg) do {				\
	if (CHIP_MASK_CHK(chip_mask))					\
		for (int i = 0; i < (loop); i++) {			\
			if (REG_RD(sc, offset + i * 4) == 1) {		\
				val1 = REG_RD(sc, offset1 + i * (inc));	\
				val1 = REG_RD(sc, offset1 + i * (inc) +	4); \
				val1 = ((REG_RD(sc, offset1 + i * (inc)	+ 8) & \
				    0x00000078) >> 3);			\
				val2 = (REG_RD(sc, offset2 + i * 4) >> 1); \
				CONDITION_CHK(condition, severity,	\
				    fail_msg " - LCID %d CID_CAM 0x%x "	\
				    "Value is 0x%x\n", i, val2, val1);	\
			}						\
		}							\
} while (0);

/*
 * Idle check.
 *
 * Performs a series of register reads and compares the returned values to
 * expected values, looking for obvious errors.  The comparisons used here
 * (IDLE_CHK_*) are machine generated and should not be modifed.
 *
 * Returns:
 *   The number of errors encountered.
 */
static int
bxe_idle_chk(struct bxe_softc *sc)
{
	struct ifnet *ifp;
	uint32_t val, val1, val2;
	int is_e1, is_e1h;

	idle_chk_errors = 0;
	idle_chk_warnings = 0;
	ifp = sc->bxe_ifp;
	is_e1 = CHIP_IS_E1(sc);
	is_e1h = CHIP_IS_E1H(sc);

	/* Don't run this code if the inteface hasn't been initialized. */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		goto bxe_idle_chk_exit;

	BXE_PRINTF(
	    "------------------------------"
	    " Idle Check "
	    "------------------------------\n");

	/* ToDo: Don't run this code is driver is NOT running. */

	/* Perform the idle checks. */
	IDLE_CHK_1(0x3, 0x2114, ((val & 0x0ff010) != 0), IDLE_CHK_ERROR,
	    "PCIE: ucorr_err_status is not 0");
	IDLE_CHK_1(0x3, 0x2114, ((val & 0x100000) != 0), IDLE_CHK_WARNING,
	    "PCIE: ucorr_err_status - Unsupported request error");
	IDLE_CHK_1(0x3, 0x2120,
	    (((val & 0x31c1) != 0x2000) && ((val & 0x31c1) != 0)),
	    IDLE_CHK_WARNING, "PCIE: corr_err_status is not 0x2000");
	IDLE_CHK_1(0x3, 0x2814, ((val & ~0x40100) != 0), IDLE_CHK_ERROR,
	    "PCIE: attentions register is not 0x40100");
	IDLE_CHK_1(0x2, 0x281c, ((val & ~0x40040100) != 0), IDLE_CHK_ERROR,
	    "PCIE: attentions register is not 0x40040100");
	IDLE_CHK_1(0x2, 0x2820, ((val & ~0x40040100) != 0), IDLE_CHK_ERROR,
	    "PCIE: attentions register is not 0x40040100");
	IDLE_CHK_1(0x1, PXP2_REG_PGL_EXP_ROM2, (val != 0xffffffff),
	    IDLE_CHK_WARNING,
	    "PXP2: There are outstanding read requests. Not all"
	    " completions have arrived for read requests on tags that"
	    " are marked with 0");
	IDLE_CHK_2(0x3, 0x212c, 4, 4, ((val != 0) && (idle_chk_errors > 0)),
	    IDLE_CHK_WARNING, "PCIE: error packet header %d is not 0");

	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ0_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ0 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ1_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ1 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ2_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ2 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ3_ENTRY_CNT, (val > 2),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ3 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ4_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ4 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ5_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ5 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ6_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ6 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ7_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ7 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ8_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ8 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ9_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ9 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ10_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ10 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ11_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ11 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ12_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ12 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ13_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ13 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ14_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ14 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ15_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ15 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ16_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ16 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ17_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ17 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ18_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ18 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ19_ENTRY_CNT, (val != 0),
	    IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ19 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ20_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ20 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ21_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ21 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ22_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ22 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ23_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ23 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ24_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ24 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ25_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ25 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ26_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ26 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ27_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ27 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ28_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ28 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ29_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ29 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ30_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ30 is not empty");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_VQ31_ENTRY_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PXP2: VQ31 is not empty");

	IDLE_CHK_1(0x3, PXP2_REG_RQ_UFIFO_NUM_OF_ENTRY, (val != 0),
	   IDLE_CHK_ERROR, "PXP2: rq_ufifo_num_of_entry is not 0");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_RBC_DONE, (val != 1), IDLE_CHK_ERROR,
	   "PXP2: rq_rbc_done is not 1");
	IDLE_CHK_1(0x3, PXP2_REG_RQ_CFG_DONE, (val != 1), IDLE_CHK_ERROR,
	   "PXP2: rq_cfg_done is not 1");
	IDLE_CHK_1(0x3, PXP2_REG_PSWRQ_BW_CREDIT, (val != 0x1b),
	   IDLE_CHK_ERROR,
	   "PXP2: rq_read_credit and rq_write_credit are not 3");
	IDLE_CHK_1(0x3, PXP2_REG_RD_START_INIT, (val != 1), IDLE_CHK_ERROR,
	   "PXP2: rd_start_init is not 1");
	IDLE_CHK_1(0x3, PXP2_REG_RD_INIT_DONE, (val != 1), IDLE_CHK_ERROR,
	   "PXP2: rd_init_done is not 1");

	IDLE_CHK_3(0x3, PXP2_REG_RD_SR_CNT, PXP2_REG_RD_SR_NUM_CFG,
	   (val1 != (val2-1)), IDLE_CHK_WARNING,
	   "PXP2: rd_sr_cnt is not equal to rd_sr_num_cfg");
	IDLE_CHK_3(0x3, PXP2_REG_RD_BLK_CNT, PXP2_REG_RD_BLK_NUM_CFG,
	   (val1 != val2), IDLE_CHK_WARNING,
	   "PXP2: rd_blk_cnt is not equal to rd_blk_num_cfg");

	IDLE_CHK_3(0x3, PXP2_REG_RD_SR_CNT, PXP2_REG_RD_SR_NUM_CFG,
	   (val1 < (val2-3)), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PXP2: There are more than two unused SRs");
	IDLE_CHK_3(0x3, PXP2_REG_RD_BLK_CNT, PXP2_REG_RD_BLK_NUM_CFG,
	   (val1 < (val2-2)), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PXP2: There are more than two unused blocks");

	IDLE_CHK_1(0x3, PXP2_REG_RD_PORT_IS_IDLE_0, (val != 1),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PXP2: P0 All delivery ports are not idle");
	IDLE_CHK_1(0x3, PXP2_REG_RD_PORT_IS_IDLE_1, (val != 1),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PXP2: P1 All delivery ports are not idle");

	IDLE_CHK_2(0x3, PXP2_REG_RD_ALMOST_FULL_0, 11, 4, (val != 0),
	   IDLE_CHK_ERROR, "PXP2: rd_almost_full_%d is not 0");

	IDLE_CHK_1(0x3, PXP2_REG_RD_DISABLE_INPUTS, (val != 0),
	   IDLE_CHK_ERROR, "PXP2: PSWRD inputs are disabled");
	IDLE_CHK_1(0x3, PXP2_REG_HST_HEADER_FIFO_STATUS, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PXP2: HST header FIFO status is not 0");
	IDLE_CHK_1(0x3, PXP2_REG_HST_DATA_FIFO_STATUS, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PXP2: HST data FIFO status is not 0");
	IDLE_CHK_1(0x3, PXP2_REG_PGL_WRITE_BLOCKED, (val != 0),
	   IDLE_CHK_ERROR, "PXP2: pgl_write_blocked is not 0");
	IDLE_CHK_1(0x3, PXP2_REG_PGL_READ_BLOCKED, (val != 0), IDLE_CHK_ERROR,
	   "PXP2: pgl_read_blocked is not 0");
	IDLE_CHK_1(0x3, PXP2_REG_PGL_TXW_CDTS, (((val >> 17) & 1) != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PXP2: There is data which is ready");

	IDLE_CHK_1(0x3, PXP_REG_HST_ARB_IS_IDLE, (val != 1), IDLE_CHK_WARNING,
	   "PXP: HST arbiter is not idle");
	IDLE_CHK_1(0x3, PXP_REG_HST_CLIENTS_WAITING_TO_ARB, (val != 0),
	   IDLE_CHK_WARNING,
	   "PXP: HST one of the clients is waiting for delivery");
	IDLE_CHK_1(0x2, PXP_REG_HST_DISCARD_INTERNAL_WRITES_STATUS,
	   (val != 0), IDLE_CHK_WARNING,
	   "PXP: HST Close the gates: Discarding internal writes");
	IDLE_CHK_1(0x2, PXP_REG_HST_DISCARD_DOORBELLS_STATUS, (val != 0),
	   IDLE_CHK_WARNING,
	   "PXP: HST Close the gates: Discarding doorbells");

	IDLE_CHK_1(0x3, DMAE_REG_GO_C0, (val != 0), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "DMAE: command 0 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C1, (val != 0), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "DMAE: command 1 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C2, (val != 0), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "DMAE: command 2 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C3, (val != 0), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "DMAE: command 3 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C4, (val != 0), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "DMAE: command 4 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C5, (val != 0), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "DMAE: command 5 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C6, (val != 0), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "DMAE: command 6 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C7, (val != 0), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "DMAE: command 7 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C8, (val != 0), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "DMAE: command 8 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C9, (val != 0), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "DMAE: command 9 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C10, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "DMAE: command 10 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C11, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "DMAE: command 11 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C12, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "DMAE: command 12 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C13, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "DMAE: command 13 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C14, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "DMAE: command 14 go is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_GO_C15, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "DMAE: command 15 go is not 0");

	IDLE_CHK_1(0x3, CFC_REG_ERROR_VECTOR, (val != 0), IDLE_CHK_ERROR,
	   "CFC: error vector is not 0");
	IDLE_CHK_1(0x3, CFC_REG_NUM_LCIDS_ARRIVING, (val != 0),
	   IDLE_CHK_ERROR, "CFC: number of arriving LCIDs is not 0");
	IDLE_CHK_1(0x3, CFC_REG_NUM_LCIDS_ALLOC, (val != 0), IDLE_CHK_ERROR,
	   "CFC: number of alloc LCIDs is not 0");
	IDLE_CHK_1(0x3, CFC_REG_NUM_LCIDS_LEAVING, (val != 0), IDLE_CHK_ERROR,
	   "CFC: number of leaving LCIDs is not 0");

	IDLE_CHK_4(0x3, CFC_REG_ACTIVITY_COUNTER, CFC_REG_CID_CAM,
	   (CFC_REG_ACTIVITY_COUNTER_SIZE >> 2), 4, (val1 > 1),
	   IDLE_CHK_ERROR, "CFC: AC > 1");
	IDLE_CHK_7(0x3, CFC_REG_ACTIVITY_COUNTER, CFC_REG_INFO_RAM,
	   CFC_REG_CID_CAM, (CFC_REG_INFO_RAM_SIZE >> 4), 16,
	   (val1 == 3), IDLE_CHK_WARNING,
	   "CFC: AC is 1, connType is 3");
	IDLE_CHK_7(0x3, CFC_REG_ACTIVITY_COUNTER, CFC_REG_INFO_RAM,
	   CFC_REG_CID_CAM, (CFC_REG_INFO_RAM_SIZE >> 4), 16,
	   ((val1 != 0) && (val1 != 3)), IDLE_CHK_ERROR,
	   "CFC: AC is 1, connType is not 0 nor 3");

	IDLE_CHK_2(0x3, QM_REG_QTASKCTR_0, 64, 4, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "QM: Q_%d, queue is not empty");

	IDLE_CHK_3(0x3, QM_REG_VOQCREDIT_0, QM_REG_VOQINITCREDIT_0,
	   (val1 != val2), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "QM: VOQ_0, VOQ credit is not equal to initial credit");
	IDLE_CHK_3(0x3, QM_REG_VOQCREDIT_1, QM_REG_VOQINITCREDIT_1,
	   (val1 != val2), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "QM: VOQ_1, VOQ credit is not equal to initial credit");
	IDLE_CHK_3(0x3, QM_REG_VOQCREDIT_4, QM_REG_VOQINITCREDIT_4,
	   (val1 != val2), IDLE_CHK_ERROR,
	   "QM: VOQ_4, VOQ credit is not equal to initial credit");

	IDLE_CHK_3(0x3, QM_REG_PORT0BYTECRD, QM_REG_BYTECRDINITVAL,
	   (val1 != val2), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "QM: P0 Byte credit is not equal to initial credit");
	IDLE_CHK_3(0x3, QM_REG_PORT1BYTECRD, QM_REG_BYTECRDINITVAL,
	   (val1 != val2), IDLE_CHK_ERROR_NO_TRAFFIC,
	   "QM: P1 Byte credit is not equal to initial credit");

	IDLE_CHK_1(0x3, CCM_REG_CAM_OCCUP, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "CCM: XX protection CAM is not empty");
	IDLE_CHK_1(0x3, TCM_REG_CAM_OCCUP, (val != 0), IDLE_CHK_ERROR,
	   "TCM: XX protection CAM is not empty");
	IDLE_CHK_1(0x3, UCM_REG_CAM_OCCUP, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "UCM: XX protection CAM is not empty");
	IDLE_CHK_1(0x3, XCM_REG_CAM_OCCUP, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "XCM: XX protection CAM is not empty");

	IDLE_CHK_1(0x3, BRB1_REG_NUM_OF_FULL_BLOCKS, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "BRB1: BRB is not empty");

	IDLE_CHK_1(0x3, CSEM_REG_SLEEP_THREADS_VALID, (val != 0),
	   IDLE_CHK_ERROR, "CSEM: There are sleeping threads");
	IDLE_CHK_1(0x3, TSEM_REG_SLEEP_THREADS_VALID, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "TSEM: There are sleeping threads");
	IDLE_CHK_1(0x3, USEM_REG_SLEEP_THREADS_VALID, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "USEM: There are sleeping threads");
	IDLE_CHK_1(0x3, XSEM_REG_SLEEP_THREADS_VALID, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "XSEM: There are sleeping threads");

	IDLE_CHK_1(0x3, CSEM_REG_SLOW_EXT_STORE_EMPTY, (val != 1),
	   IDLE_CHK_ERROR, "CSEM: External store FIFO is not empty");
	IDLE_CHK_1(0x3, TSEM_REG_SLOW_EXT_STORE_EMPTY, (val != 1),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "TSEM: External store FIFO is not empty");
	IDLE_CHK_1(0x3, USEM_REG_SLOW_EXT_STORE_EMPTY, (val != 1),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "USEM: External store FIFO is not empty");
	IDLE_CHK_1(0x3, XSEM_REG_SLOW_EXT_STORE_EMPTY, (val != 1),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "XSEM: External store FIFO is not empty");

	IDLE_CHK_1(0x3, CSDM_REG_SYNC_PARSER_EMPTY, (val != 1),
	   IDLE_CHK_ERROR, "CSDM: Parser serial FIFO is not empty");
	IDLE_CHK_1(0x3, TSDM_REG_SYNC_PARSER_EMPTY, (val != 1),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "TSDM: Parser serial FIFO is not empty");
	IDLE_CHK_1(0x3, USDM_REG_SYNC_PARSER_EMPTY, (val != 1),
	   IDLE_CHK_ERROR, "USDM: Parser serial FIFO is not empty");
	IDLE_CHK_1(0x3, XSDM_REG_SYNC_PARSER_EMPTY, (val != 1),
	   IDLE_CHK_ERROR, "XSDM: Parser serial FIFO is not empty");

	IDLE_CHK_1(0x3, CSDM_REG_SYNC_SYNC_EMPTY, (val != 1), IDLE_CHK_ERROR,
	   "CSDM: Parser SYNC serial FIFO is not empty");
	IDLE_CHK_1(0x3, TSDM_REG_SYNC_SYNC_EMPTY, (val != 1), IDLE_CHK_ERROR,
	   "TSDM: Parser SYNC serial FIFO is not empty");
	IDLE_CHK_1(0x3, USDM_REG_SYNC_SYNC_EMPTY, (val != 1), IDLE_CHK_ERROR,
	   "USDM: Parser SYNC serial FIFO is not empty");
	IDLE_CHK_1(0x3, XSDM_REG_SYNC_SYNC_EMPTY, (val != 1), IDLE_CHK_ERROR,
	   "XSDM: Parser SYNC serial FIFO is not empty");

	IDLE_CHK_1(0x3, CSDM_REG_RSP_PXP_CTRL_RDATA_EMPTY, (val != 1),
	   IDLE_CHK_ERROR,
	   "CSDM: pxp_ctrl rd_data fifo is not empty in sdm_dma_rsp block");
	IDLE_CHK_1(0x3, TSDM_REG_RSP_PXP_CTRL_RDATA_EMPTY, (val != 1),
	   IDLE_CHK_ERROR,
	   "TSDM: pxp_ctrl rd_data fifo is not empty in sdm_dma_rsp block");
	IDLE_CHK_1(0x3, USDM_REG_RSP_PXP_CTRL_RDATA_EMPTY, (val != 1),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "USDM: pxp_ctrl rd_data fifo is not empty in sdm_dma_rsp block");
	IDLE_CHK_1(0x3, XSDM_REG_RSP_PXP_CTRL_RDATA_EMPTY, (val != 1),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "XSDM: pxp_ctrl rd_data fifo is not empty in sdm_dma_rsp block");

	IDLE_CHK_1(0x3, DORQ_REG_DQ_FILL_LVLF, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "DQ: DORQ queue is not empty");

	IDLE_CHK_1(0x3, CFC_REG_CFC_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "CFC: interrupt status is not 0");
	IDLE_CHK_1(0x3, CDU_REG_CDU_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "CDU: interrupt status is not 0");
	IDLE_CHK_1(0x3, CCM_REG_CCM_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "CCM: interrupt status is not 0");
	IDLE_CHK_1(0x3, TCM_REG_TCM_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "TCM: interrupt status is not 0");
	IDLE_CHK_1(0x3, UCM_REG_UCM_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "UCM: interrupt status is not 0");
	IDLE_CHK_1(0x3, XCM_REG_XCM_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "XCM: interrupt status is not 0");
	IDLE_CHK_1(0x3, PBF_REG_PBF_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "PBF: interrupt status is not 0");
	IDLE_CHK_1(0x3, TM_REG_TM_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "TIMERS: interrupt status is not 0");
	IDLE_CHK_1(0x3, DORQ_REG_DORQ_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "DQ: interrupt status is not 0");
	IDLE_CHK_1(0x3, SRC_REG_SRC_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "SRCH: interrupt status is not 0");
	IDLE_CHK_1(0x3, PRS_REG_PRS_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "PRS: interrupt status is not 0");
	IDLE_CHK_1(0x3, BRB1_REG_BRB1_INT_STS, ((val & ~0xfc00) != 0),
	   IDLE_CHK_ERROR, "BRB1: interrupt status is not 0");
	IDLE_CHK_1(0x3, GRCBASE_XPB + PB_REG_PB_INT_STS, (val != 0),
	   IDLE_CHK_ERROR, "XPB: interrupt status is not 0");
	IDLE_CHK_1(0x3, GRCBASE_UPB + PB_REG_PB_INT_STS, (val != 0),
	   IDLE_CHK_ERROR, "UPB: interrupt status is not 0");
	IDLE_CHK_1(0x3, PXP2_REG_PXP2_INT_STS_0, (val != 0), IDLE_CHK_WARNING,
	   "PXP2: interrupt status 0 is not 0");
	IDLE_CHK_1(0x2, PXP2_REG_PXP2_INT_STS_1, (val != 0), IDLE_CHK_WARNING,
	   "PXP2: interrupt status 1 is not 0");
	IDLE_CHK_1(0x3, QM_REG_QM_INT_STS, (val != 0), IDLE_CHK_ERROR,
	   "QM: interrupt status is not 0");
	IDLE_CHK_1(0x3, PXP_REG_PXP_INT_STS_0, (val != 0), IDLE_CHK_ERROR,
	   "PXP: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, PXP_REG_PXP_INT_STS_1, (val != 0), IDLE_CHK_ERROR,
	   "PXP: interrupt status 1 is not 0");

	IDLE_CHK_1(0x3, DORQ_REG_RSPA_CRD_CNT, (val != 2),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "DQ: Credit to XCM is not full");
	IDLE_CHK_1(0x3, DORQ_REG_RSPB_CRD_CNT, (val != 2), IDLE_CHK_ERROR,
	   "DQ: Credit to UCM is not full");

	IDLE_CHK_1(0x3, QM_REG_VOQCRDERRREG, (val != 0), IDLE_CHK_ERROR,
	   "QM: Credit error register is not 0 (byte or credit"
	   " overflow/underflow)");
	IDLE_CHK_1(0x3, DORQ_REG_DQ_FULL_ST, (val != 0), IDLE_CHK_ERROR,
	   "DQ: DORQ queue is full");

	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_1_FUNC_0,
	   ((val & ~0xcffc) != 0), IDLE_CHK_WARNING,
	   "AEU: P0 AFTER_INVERT_1 is not 0");
	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_2_FUNC_0, (val != 0),
	   IDLE_CHK_ERROR, "AEU: P0 AFTER_INVERT_2 is not 0");
	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_3_FUNC_0,
	   ((val & ~0xc21b0000) != 0), IDLE_CHK_ERROR,
	   "AEU: P0 AFTER_INVERT_3 is not 0");
	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_4_FUNC_0,
	   ((val & ~0x801fffff) != 0), IDLE_CHK_ERROR,
	   "AEU: P0 AFTER_INVERT_4 is not 0");

	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_1_FUNC_1,
	   ((val & ~0xcffc) != 0), IDLE_CHK_WARNING,
	   "AEU: P1 AFTER_INVERT_1 is not 0");
	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_2_FUNC_1, (val != 0),
	   IDLE_CHK_ERROR, "AEU: P1 AFTER_INVERT_2 is not 0");
	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_3_FUNC_1,
	   ((val & ~0xc21b0000) != 0), IDLE_CHK_ERROR,
	   "AEU: P1 AFTER_INVERT_3 is not 0");
	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_4_FUNC_1,
	   ((val & ~0x801fffff) != 0), IDLE_CHK_ERROR,
	   "AEU: P1 AFTER_INVERT_4 is not 0");

	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_1_MCP,
	   ((val & ~0xcffc) != 0), IDLE_CHK_WARNING,
	   "AEU: MCP AFTER_INVERT_1 is not 0");
	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_2_MCP, (val != 0),
	   IDLE_CHK_ERROR, "AEU: MCP AFTER_INVERT_2 is not 0");
	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_3_MCP,
	   ((val & ~0xc21b0000) != 0), IDLE_CHK_ERROR,
	   "AEU: MCP AFTER_INVERT_3 is not 0");
	IDLE_CHK_1(0x3, MISC_REG_AEU_AFTER_INVERT_4_MCP,
	   ((val & ~0x801fffff) != 0), IDLE_CHK_ERROR,
	   "AEU: MCP AFTER_INVERT_4 is not 0");

	IDLE_CHK_5(0x3, PBF_REG_DISABLE_NEW_TASK_PROC_P0, PBF_REG_P0_CREDIT,
	   PBF_REG_P0_INIT_CRD, (val1 != val2),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PBF: P0 credit is not equal to init_crd");
	IDLE_CHK_5(0x3, PBF_REG_DISABLE_NEW_TASK_PROC_P1, PBF_REG_P1_CREDIT,
	   PBF_REG_P1_INIT_CRD, (val1 != val2),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PBF: P1 credit is not equal to init_crd");
	IDLE_CHK_3(0x3, PBF_REG_P4_CREDIT, PBF_REG_P4_INIT_CRD,
	   (val1 != val2), IDLE_CHK_ERROR,
	   "PBF: P4 credit is not equal to init_crd");

	IDLE_CHK_1(0x3, PBF_REG_P0_TASK_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PBF: P0 task_cnt is not 0");
	IDLE_CHK_1(0x3, PBF_REG_P1_TASK_CNT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "PBF: P1 task_cnt is not 0");
	IDLE_CHK_1(0x3, PBF_REG_P4_TASK_CNT, (val != 0), IDLE_CHK_ERROR,
	   "PBF: P4 task_cnt is not 0");

	IDLE_CHK_1(0x3, XCM_REG_CFC_INIT_CRD, (val != 1), IDLE_CHK_ERROR,
	   "XCM: CFC_INIT_CRD is not 1");
	IDLE_CHK_1(0x3, UCM_REG_CFC_INIT_CRD, (val != 1), IDLE_CHK_ERROR,
	   "UCM: CFC_INIT_CRD is not 1");
	IDLE_CHK_1(0x3, TCM_REG_CFC_INIT_CRD, (val != 1), IDLE_CHK_ERROR,
	   "TCM: CFC_INIT_CRD is not 1");
	IDLE_CHK_1(0x3, CCM_REG_CFC_INIT_CRD, (val != 1),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "CCM: CFC_INIT_CRD is not 1");

	IDLE_CHK_1(0x3, XCM_REG_XQM_INIT_CRD, (val != 32), IDLE_CHK_ERROR,
	   "XCM: XQM_INIT_CRD is not 32");
	IDLE_CHK_1(0x3, UCM_REG_UQM_INIT_CRD, (val != 32), IDLE_CHK_ERROR,
	   "UCM: UQM_INIT_CRD is not 32");
	IDLE_CHK_1(0x3, TCM_REG_TQM_INIT_CRD, (val != 32), IDLE_CHK_ERROR,
	   "TCM: TQM_INIT_CRD is not 32");
	IDLE_CHK_1(0x3, CCM_REG_CQM_INIT_CRD, (val != 32),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "CCM: CQM_INIT_CRD is not 32");

	IDLE_CHK_1(0x3, XCM_REG_TM_INIT_CRD, (val != 4), IDLE_CHK_ERROR,
	   "XCM: TM_INIT_CRD is not 4");
	IDLE_CHK_1(0x3, UCM_REG_TM_INIT_CRD, (val != 4), IDLE_CHK_ERROR,
	   "UCM: TM_INIT_CRD is not 4");

	IDLE_CHK_1(0x3, XCM_REG_FIC0_INIT_CRD, (val != 64),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "XCM: FIC0_INIT_CRD is not 64");
	IDLE_CHK_1(0x3, UCM_REG_FIC0_INIT_CRD, (val != 64),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "UCM: FIC0_INIT_CRD is not 64");
	IDLE_CHK_1(0x3, TCM_REG_FIC0_INIT_CRD, (val != 64),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "TCM: FIC0_INIT_CRD is not 64");
	IDLE_CHK_1(0x3, CCM_REG_FIC0_INIT_CRD, (val != 64),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "CCM: FIC0_INIT_CRD is not 64");

	IDLE_CHK_1(0x3, XCM_REG_FIC1_INIT_CRD, (val != 64), IDLE_CHK_ERROR,
	   "XCM: FIC1_INIT_CRD is not 64");
	IDLE_CHK_1(0x3, UCM_REG_FIC1_INIT_CRD, (val != 64), IDLE_CHK_ERROR,
	   "UCM: FIC1_INIT_CRD is not 64");
	IDLE_CHK_1(0x3, TCM_REG_FIC1_INIT_CRD, (val != 64), IDLE_CHK_ERROR,
	   "TCM: FIC1_INIT_CRD is not 64");
	IDLE_CHK_1(0x3, CCM_REG_FIC1_INIT_CRD, (val != 64), IDLE_CHK_ERROR,
	   "CCM: FIC1_INIT_CRD is not 64");

	IDLE_CHK_1(0x1, XCM_REG_XX_FREE, (val != 31), IDLE_CHK_ERROR,
	   "XCM: XX_FREE is not 31");
	IDLE_CHK_1(0x2, XCM_REG_XX_FREE, (val != 32), IDLE_CHK_ERROR,
	   "XCM: XX_FREE is not 32");
	IDLE_CHK_1(0x3, UCM_REG_XX_FREE, (val != 27),
	   IDLE_CHK_ERROR_NO_TRAFFIC, "UCM: XX_FREE is not 27");
	IDLE_CHK_1(0x3, TCM_REG_XX_FREE, (val != 32), IDLE_CHK_ERROR,
	   "TCM: XX_FREE is not 32");
	IDLE_CHK_1(0x3, CCM_REG_XX_FREE, (val != 24), IDLE_CHK_ERROR,
	   "CCM: XX_FREE is not 24");

	IDLE_CHK_1(0x3, XSEM_REG_FAST_MEMORY + 0x18000, (val !=  0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "XSEM: FOC0 credit less than initial credit");
	IDLE_CHK_1(0x3, XSEM_REG_FAST_MEMORY + 0x18040, (val !=  24),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "XSEM: FOC1 credit less than initial credit");
	IDLE_CHK_1(0x3, XSEM_REG_FAST_MEMORY + 0x18080, (val !=  12),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "XSEM: FOC2 credit less than initial credit");
	IDLE_CHK_1(0x3, XSEM_REG_FAST_MEMORY + 0x180C0, (val !=  102),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "XSEM: FOC3 credit less than initial credit");

	IDLE_CHK_1(0x3, USEM_REG_FAST_MEMORY + 0x18000, (val != 26),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "USEM: FOC0 credit less than initial credit");
	IDLE_CHK_1(0x3, USEM_REG_FAST_MEMORY + 0x18040, (val != 78),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "USEM: FOC1 credit less than initial credit");
	IDLE_CHK_1(0x3, USEM_REG_FAST_MEMORY + 0x18080, (val != 16),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "USEM: FOC2 credit less than initial credit");
	IDLE_CHK_1(0x3, USEM_REG_FAST_MEMORY + 0x180C0, (val != 32),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "USEM: FOC3 credit less than initial credit");

	IDLE_CHK_1(0x3, TSEM_REG_FAST_MEMORY + 0x18000, (val != 52),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "TSEM: FOC0 credit less than initial credit");
	IDLE_CHK_1(0x3, TSEM_REG_FAST_MEMORY + 0x18040, (val != 24),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "TSEM: FOC1 credit less than initial credit");
	IDLE_CHK_1(0x3, TSEM_REG_FAST_MEMORY + 0x18080, (val != 12),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "TSEM: FOC2 credit less than initial credit");
	IDLE_CHK_1(0x3, TSEM_REG_FAST_MEMORY + 0x180C0, (val != 32),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "TSEM: FOC3 credit less than initial credit");

	IDLE_CHK_1(0x3, CSEM_REG_FAST_MEMORY + 0x18000, (val != 16),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "CSEM: FOC0 credit less than initial credit");
	IDLE_CHK_1(0x3, CSEM_REG_FAST_MEMORY + 0x18040, (val != 18),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "CSEM: FOC1 credit less than initial credit");
	IDLE_CHK_1(0x3, CSEM_REG_FAST_MEMORY + 0x18080, (val != 48),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "CSEM: FOC2 credit less than initial credit");
	IDLE_CHK_1(0x3, CSEM_REG_FAST_MEMORY + 0x180C0, (val != 14),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "CSEM: FOC3 credit less than initial credit");

	IDLE_CHK_1(0x3, PRS_REG_TSDM_CURRENT_CREDIT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PRS: TSDM current credit is not 0");
	IDLE_CHK_1(0x3, PRS_REG_TCM_CURRENT_CREDIT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PRS: TCM current credit is not 0");
	IDLE_CHK_1(0x3, PRS_REG_CFC_LD_CURRENT_CREDIT, (val != 0),
	   IDLE_CHK_ERROR, "PRS: CFC_LD current credit is not 0");
	IDLE_CHK_1(0x3, PRS_REG_CFC_SEARCH_CURRENT_CREDIT, (val != 0),
	   IDLE_CHK_ERROR, "PRS: CFC_SEARCH current credit is not 0");
	IDLE_CHK_1(0x3, PRS_REG_SRC_CURRENT_CREDIT, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PRS: SRCH current credit is not 0");

	IDLE_CHK_1(0x3, PRS_REG_PENDING_BRB_PRS_RQ, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PRS: PENDING_BRB_PRS_RQ is not 0");
	IDLE_CHK_2(0x3, PRS_REG_PENDING_BRB_CAC0_RQ, 5, 4, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PRS: PENDING_BRB_CAC%d_RQ is not 0");

	IDLE_CHK_1(0x3, PRS_REG_SERIAL_NUM_STATUS_LSB, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PRS: SERIAL_NUM_STATUS_LSB is not 0");
	IDLE_CHK_1(0x3, PRS_REG_SERIAL_NUM_STATUS_MSB, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "PRS: SERIAL_NUM_STATUS_MSB is not 0");

	IDLE_CHK_1(0x3, CDU_REG_ERROR_DATA, (val != 0), IDLE_CHK_ERROR,
	   "CDU: ERROR_DATA is not 0");

	IDLE_CHK_1(0x3, CCM_REG_STORM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "CCM: STORM declared message length unequal to actual");
	IDLE_CHK_1(0x3, CCM_REG_CSDM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "CCM: CSDM declared message length unequal to actual");
	IDLE_CHK_1(0x3, CCM_REG_TSEM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "CCM: TSEM declared message length unequal to actual");
	IDLE_CHK_1(0x3, CCM_REG_XSEM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "CCM: XSEM declared message length unequal to actual");
	IDLE_CHK_1(0x3, CCM_REG_USEM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "CCM: USEM declared message length unequal to actual");
	IDLE_CHK_1(0x3, CCM_REG_PBF_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "CCM: PBF declared message length unequal to actual");

	IDLE_CHK_1(0x3, TCM_REG_STORM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "TCM: STORM declared message length unequal to actual");
	IDLE_CHK_1(0x3, TCM_REG_TSDM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "TCM: TSDM declared message length unequal to actual");
	IDLE_CHK_1(0x3, TCM_REG_PRS_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "TCM: PRS declared message length unequal to actual");
	IDLE_CHK_1(0x3, TCM_REG_PBF_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "TCM: PBF declared message length unequal to actual");
	IDLE_CHK_1(0x3, TCM_REG_USEM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "TCM: USEM declared message length unequal to actual");
	IDLE_CHK_1(0x3, TCM_REG_CSEM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "TCM: CSEM declared message length unequal to actual");

	IDLE_CHK_1(0x3, UCM_REG_STORM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "UCM: STORM declared message length unequal to actual");
	IDLE_CHK_1(0x3, UCM_REG_USDM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "UCM: USDM declared message length unequal to actual");
	IDLE_CHK_1(0x3, UCM_REG_TSEM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "UCM: TSEM declared message length unequal to actual");
	IDLE_CHK_1(0x3, UCM_REG_CSEM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "UCM: CSEM declared message length unequal to actual");
	IDLE_CHK_1(0x3, UCM_REG_XSEM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "UCM: XSEM declared message length unequal to actual");
	IDLE_CHK_1(0x3, UCM_REG_DORQ_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "UCM: DORQ declared message length unequal to actual");

	IDLE_CHK_1(0x3, XCM_REG_STORM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "XCM: STORM declared message length unequal to actual");
	IDLE_CHK_1(0x3, XCM_REG_XSDM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "XCM: XSDM declared message length unequal to actual");
	IDLE_CHK_1(0x3, XCM_REG_TSEM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "XCM: TSEM declared message length unequal to actual");
	IDLE_CHK_1(0x3, XCM_REG_CSEM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "XCM: CSEM declared message length unequal to actual");
	IDLE_CHK_1(0x3, XCM_REG_USEM_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "XCM: USEM declared message length unequal to actual");
	IDLE_CHK_1(0x3, XCM_REG_DORQ_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "XCM: DORQ declared message length unequal to actual");
	IDLE_CHK_1(0x3, XCM_REG_PBF_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "XCM: PBF declared message length unequal to actual");
	IDLE_CHK_1(0x3, XCM_REG_NIG0_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "XCM: NIG0 declared message length unequal to actual");
	IDLE_CHK_1(0x3, XCM_REG_NIG1_LENGTH_MIS, (val != 0), IDLE_CHK_ERROR,
	   "XCM: NIG1 declared message length unequal to actual");

	IDLE_CHK_1(0x3, QM_REG_XQM_WRC_FIFOLVL, (val != 0), IDLE_CHK_ERROR,
	   "QM: XQM wrc_fifolvl is not 0");
	IDLE_CHK_1(0x3, QM_REG_UQM_WRC_FIFOLVL, (val != 0), IDLE_CHK_ERROR,
	   "QM: UQM wrc_fifolvl is not 0");
	IDLE_CHK_1(0x3, QM_REG_TQM_WRC_FIFOLVL, (val != 0), IDLE_CHK_ERROR,
	   "QM: TQM wrc_fifolvl is not 0");
	IDLE_CHK_1(0x3, QM_REG_CQM_WRC_FIFOLVL, (val != 0), IDLE_CHK_ERROR,
	   "QM: CQM wrc_fifolvl is not 0");
	IDLE_CHK_1(0x3, QM_REG_QSTATUS_LOW, (val != 0), IDLE_CHK_ERROR,
	   "QM: QSTATUS_LOW is not 0");
	IDLE_CHK_1(0x3, QM_REG_QSTATUS_HIGH, (val != 0), IDLE_CHK_ERROR,
	   "QM: QSTATUS_HIGH is not 0");
	IDLE_CHK_1(0x3, QM_REG_PAUSESTATE0, (val != 0), IDLE_CHK_ERROR,
	   "QM: PAUSESTATE0 is not 0");
	IDLE_CHK_1(0x3, QM_REG_PAUSESTATE1, (val != 0), IDLE_CHK_ERROR,
	   "QM: PAUSESTATE1 is not 0");
	IDLE_CHK_1(0x3, QM_REG_OVFQNUM, (val != 0), IDLE_CHK_ERROR,
	   "QM: OVFQNUM is not 0");
	IDLE_CHK_1(0x3, QM_REG_OVFERROR, (val != 0), IDLE_CHK_ERROR,
	   "QM: OVFERROR is not 0");

	IDLE_CHK_6(0x3, QM_REG_PTRTBL, 64, 8, IDLE_CHK_ERROR_NO_TRAFFIC);

	IDLE_CHK_1(0x3, BRB1_REG_BRB1_PRTY_STS, ((val & ~0x8) != 0),
	   IDLE_CHK_WARNING, "BRB1: parity status is not 0");
	IDLE_CHK_1(0x3, CDU_REG_CDU_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "CDU: parity status is not 0");
	IDLE_CHK_1(0x3, CFC_REG_CFC_PRTY_STS, ((val & ~0x2) != 0),
	   IDLE_CHK_WARNING, "CFC: parity status is not 0");
	IDLE_CHK_1(0x3, CSDM_REG_CSDM_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "CSDM: parity status is not 0");
	IDLE_CHK_1(0x3, DBG_REG_DBG_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "DBG: parity status is not 0");
	IDLE_CHK_1(0x3, DMAE_REG_DMAE_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "DMAE: parity status is not 0");
	IDLE_CHK_1(0x3, DORQ_REG_DORQ_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "DQ: parity status is not 0");
	IDLE_CHK_1(0x1, TCM_REG_TCM_PRTY_STS, ((val & ~0x3ffc0) != 0),
	   IDLE_CHK_WARNING, "TCM: parity status is not 0");
	IDLE_CHK_1(0x2, TCM_REG_TCM_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "TCM: parity status is not 0");
	IDLE_CHK_1(0x1, CCM_REG_CCM_PRTY_STS, ((val & ~0x3ffc0) != 0),
	   IDLE_CHK_WARNING, "CCM: parity status is not 0");
	IDLE_CHK_1(0x2, CCM_REG_CCM_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "CCM: parity status is not 0");
	IDLE_CHK_1(0x1, UCM_REG_UCM_PRTY_STS, ((val & ~0x3ffc0) != 0),
	   IDLE_CHK_WARNING, "UCM: parity status is not 0");
	IDLE_CHK_1(0x2, UCM_REG_UCM_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "UCM: parity status is not 0");
	IDLE_CHK_1(0x1, XCM_REG_XCM_PRTY_STS, ((val & ~0x3ffc0) != 0),
	   IDLE_CHK_WARNING, "XCM: parity status is not 0");
	IDLE_CHK_1(0x2, XCM_REG_XCM_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "XCM: parity status is not 0");
	IDLE_CHK_1(0x1, HC_REG_HC_PRTY_STS, ((val & ~0x1) != 0),
	   IDLE_CHK_WARNING, "HC: parity status is not 0");
	IDLE_CHK_1(0x1, MISC_REG_MISC_PRTY_STS, ((val & ~0x1) != 0),
	   IDLE_CHK_WARNING, "MISC: parity status is not 0");
	IDLE_CHK_1(0x3, PRS_REG_PRS_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "PRS: parity status is not 0");
	IDLE_CHK_1(0x3, PXP_REG_PXP_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "PXP: parity status is not 0");
	IDLE_CHK_1(0x3, QM_REG_QM_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "QM: parity status is not 0");
	IDLE_CHK_1(0x1, SRC_REG_SRC_PRTY_STS, ((val & ~0x4) != 0),
	   IDLE_CHK_WARNING, "SRCH: parity status is not 0");
	IDLE_CHK_1(0x3, TSDM_REG_TSDM_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "TSDM: parity status is not 0");
	IDLE_CHK_1(0x3, USDM_REG_USDM_PRTY_STS, ((val & ~0x20) != 0),
	   IDLE_CHK_WARNING, "USDM: parity status is not 0");
	IDLE_CHK_1(0x3, XSDM_REG_XSDM_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "XSDM: parity status is not 0");
	IDLE_CHK_1(0x3, GRCBASE_XPB + PB_REG_PB_PRTY_STS, (val != 0),
	   IDLE_CHK_WARNING, "XPB: parity status is not 0");
	IDLE_CHK_1(0x3, GRCBASE_UPB + PB_REG_PB_PRTY_STS, (val != 0),
	   IDLE_CHK_WARNING, "UPB: parity status is not 0");

	IDLE_CHK_1(0x3, CSEM_REG_CSEM_PRTY_STS_0, (val != 0),
	   IDLE_CHK_WARNING, "CSEM: parity status 0 is not 0");
	IDLE_CHK_1(0x1, PXP2_REG_PXP2_PRTY_STS_0, ((val & ~0xfff40020) != 0),
	   IDLE_CHK_WARNING, "PXP2: parity status 0 is not 0");
	IDLE_CHK_1(0x2, PXP2_REG_PXP2_PRTY_STS_0, ((val & ~0x20) != 0),
	   IDLE_CHK_WARNING, "PXP2: parity status 0 is not 0");
	IDLE_CHK_1(0x3, TSEM_REG_TSEM_PRTY_STS_0, (val != 0),
	   IDLE_CHK_WARNING, "TSEM: parity status 0 is not 0");
	IDLE_CHK_1(0x3, USEM_REG_USEM_PRTY_STS_0, (val != 0),
	   IDLE_CHK_WARNING, "USEM: parity status 0 is not 0");
	IDLE_CHK_1(0x3, XSEM_REG_XSEM_PRTY_STS_0, (val != 0),
	   IDLE_CHK_WARNING, "XSEM: parity status 0 is not 0");

	IDLE_CHK_1(0x3, CSEM_REG_CSEM_PRTY_STS_1, (val != 0),
	   IDLE_CHK_WARNING, "CSEM: parity status 1 is not 0");
	IDLE_CHK_1(0x1, PXP2_REG_PXP2_PRTY_STS_1, ((val & ~0x20) != 0),
	   IDLE_CHK_WARNING, "PXP2: parity status 1 is not 0");
	IDLE_CHK_1(0x2, PXP2_REG_PXP2_PRTY_STS_1, (val != 0),
	   IDLE_CHK_WARNING, "PXP2: parity status 1 is not 0");
	IDLE_CHK_1(0x3, TSEM_REG_TSEM_PRTY_STS_1, (val != 0),
	   IDLE_CHK_WARNING, "TSEM: parity status 1 is not 0");
	IDLE_CHK_1(0x3, USEM_REG_USEM_PRTY_STS_1, (val != 0),
	   IDLE_CHK_WARNING, "USEM: parity status 1 is not 0");
	IDLE_CHK_1(0x3, XSEM_REG_XSEM_PRTY_STS_1, (val != 0),
	   IDLE_CHK_WARNING, "XSEM: parity status 1 is not 0");

	IDLE_CHK_2(0x2, QM_REG_QTASKCTR_EXT_A_0, 64, 4, (val != 0),
	   IDLE_CHK_ERROR_NO_TRAFFIC,
	   "QM: Q_EXT_A_%d, queue is not empty");
	IDLE_CHK_1(0x2, QM_REG_QSTATUS_LOW_EXT_A, (val != 0), IDLE_CHK_ERROR,
	   "QM: QSTATUS_LOW_EXT_A is not 0");
	IDLE_CHK_1(0x2, QM_REG_QSTATUS_HIGH_EXT_A, (val != 0), IDLE_CHK_ERROR,
	   "QM: QSTATUS_HIGH_EXT_A is not 0");
	IDLE_CHK_1(0x2, QM_REG_PAUSESTATE2, (val != 0), IDLE_CHK_ERROR,
	   "QM: PAUSESTATE2 is not 0");
	IDLE_CHK_1(0x2, QM_REG_PAUSESTATE3, (val != 0), IDLE_CHK_ERROR,
	   "QM: PAUSESTATE3 is not 0");
	IDLE_CHK_1(0x2, QM_REG_PAUSESTATE4, (val != 0), IDLE_CHK_ERROR,
	   "QM: PAUSESTATE4 is not 0");
	IDLE_CHK_1(0x2, QM_REG_PAUSESTATE5, (val != 0), IDLE_CHK_ERROR,
	   "QM: PAUSESTATE5 is not 0");
	IDLE_CHK_1(0x2, QM_REG_PAUSESTATE6, (val != 0), IDLE_CHK_ERROR,
	   "QM: PAUSESTATE6 is not 0");
	IDLE_CHK_1(0x2, QM_REG_PAUSESTATE7, (val != 0), IDLE_CHK_ERROR,
	   "QM: PAUSESTATE7 is not 0");
	IDLE_CHK_6(0x2, QM_REG_PTRTBL_EXT_A, 64, 8,
	   IDLE_CHK_ERROR_NO_TRAFFIC);

	IDLE_CHK_1(0x2, MISC_REG_AEU_SYS_KILL_OCCURRED, (val != 0),
	   IDLE_CHK_ERROR, "MISC: system kill occurd;");
	IDLE_CHK_1(0x2, MISC_REG_AEU_SYS_KILL_STATUS_0, (val != 0),
	   IDLE_CHK_ERROR,
	   "MISC: system kill occurd; status_0 register");
	IDLE_CHK_1(0x2, MISC_REG_AEU_SYS_KILL_STATUS_1, (val != 0),
	   IDLE_CHK_ERROR,
	   "MISC: system kill occurd; status_1 register");
	IDLE_CHK_1(0x2, MISC_REG_AEU_SYS_KILL_STATUS_2, (val != 0),
	   IDLE_CHK_ERROR,
	   "MISC: system kill occurd; status_2 register");
	IDLE_CHK_1(0x2, MISC_REG_AEU_SYS_KILL_STATUS_3, (val != 0),
	   IDLE_CHK_ERROR,
	   "MISC: system kill occurd; status_3 register");
	IDLE_CHK_1(0x2, MISC_REG_PCIE_HOT_RESET, (val != 0), IDLE_CHK_WARNING,
	   "MISC: pcie_rst_b was asserted without perst assertion");

	IDLE_CHK_1(0x3, NIG_REG_NIG_INT_STS_0, ((val & ~0x300) != 0),
	   IDLE_CHK_ERROR, "NIG: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, NIG_REG_NIG_INT_STS_0, (val == 0x300),
	   IDLE_CHK_WARNING,
	   "NIG: Access to BMAC while not active. If tested on FPGA,"
	   " ignore this warning.");
	IDLE_CHK_1(0x3, NIG_REG_NIG_INT_STS_1, (val != 0), IDLE_CHK_ERROR,
	   "NIG: interrupt status 1 is not 0");
	IDLE_CHK_1(0x2, NIG_REG_NIG_PRTY_STS, ((val & ~0xffc00000) != 0),
	   IDLE_CHK_WARNING, "NIG: parity status is not 0");

	IDLE_CHK_1(0x3, TSEM_REG_TSEM_INT_STS_0, ((val & ~0x10000000) != 0),
	   IDLE_CHK_ERROR, "TSEM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, TSEM_REG_TSEM_INT_STS_0, (val == 0x10000000),
	   IDLE_CHK_WARNING, "TSEM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, TSEM_REG_TSEM_INT_STS_1, (val != 0), IDLE_CHK_ERROR,
	   "TSEM: interrupt status 1 is not 0");

	IDLE_CHK_1(0x3, CSEM_REG_CSEM_INT_STS_0, ((val & ~0x10000000) != 0),
	   IDLE_CHK_ERROR, "CSEM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, CSEM_REG_CSEM_INT_STS_0, (val == 0x10000000),
	   IDLE_CHK_WARNING, "CSEM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, CSEM_REG_CSEM_INT_STS_1, (val != 0), IDLE_CHK_ERROR,
	   "CSEM: interrupt status 1 is not 0");

	IDLE_CHK_1(0x3, USEM_REG_USEM_INT_STS_0, ((val & ~0x10000000) != 0),
	   IDLE_CHK_ERROR, "USEM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, USEM_REG_USEM_INT_STS_0, (val == 0x10000000),
	   IDLE_CHK_WARNING, "USEM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, USEM_REG_USEM_INT_STS_1, (val != 0), IDLE_CHK_ERROR,
	   "USEM: interrupt status 1 is not 0");

	IDLE_CHK_1(0x3, XSEM_REG_XSEM_INT_STS_0, ((val & ~0x10000000) != 0),
	   IDLE_CHK_ERROR, "XSEM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, XSEM_REG_XSEM_INT_STS_0, (val == 0x10000000),
	   IDLE_CHK_WARNING, "XSEM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, XSEM_REG_XSEM_INT_STS_1, (val != 0), IDLE_CHK_ERROR,
	   "XSEM: interrupt status 1 is not 0");

	IDLE_CHK_1(0x3, TSDM_REG_TSDM_INT_STS_0, (val != 0), IDLE_CHK_ERROR,
	   "TSDM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, TSDM_REG_TSDM_INT_STS_1, (val != 0), IDLE_CHK_ERROR,
	   "TSDM: interrupt status 1 is not 0");

	IDLE_CHK_1(0x3, CSDM_REG_CSDM_INT_STS_0, (val != 0), IDLE_CHK_ERROR,
	   "CSDM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, CSDM_REG_CSDM_INT_STS_1, (val != 0), IDLE_CHK_ERROR,
	   "CSDM: interrupt status 1 is not 0");

	IDLE_CHK_1(0x3, USDM_REG_USDM_INT_STS_0, (val != 0), IDLE_CHK_ERROR,
	   "USDM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, USDM_REG_USDM_INT_STS_1, (val != 0), IDLE_CHK_ERROR,
	   "USDM: interrupt status 1 is not 0");

	IDLE_CHK_1(0x3, XSDM_REG_XSDM_INT_STS_0, (val != 0), IDLE_CHK_ERROR,
	   "XSDM: interrupt status 0 is not 0");
	IDLE_CHK_1(0x3, XSDM_REG_XSDM_INT_STS_1, (val != 0), IDLE_CHK_ERROR,
	   "XSDM: interrupt status 1 is not 0");

	IDLE_CHK_1(0x2, HC_REG_HC_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "HC: parity status is not 0");
	IDLE_CHK_1(0x2, MISC_REG_MISC_PRTY_STS, (val != 0), IDLE_CHK_WARNING,
	   "MISC: parity status is not 0");
	IDLE_CHK_1(0x2, SRC_REG_SRC_PRTY_STS, (val  != 0), IDLE_CHK_WARNING,
	   "SRCH: parity status is not 0");

	if (idle_chk_errors == 0) {
		BXE_PRINTF("%s(): Completed successfuly with %d warning(s).\n",
		    __FUNCTION__, idle_chk_warnings);
	} else {
		BXE_PRINTF("%s(): Failed with %d error(s) and %d warning(s)!\n",
		    __FUNCTION__, idle_chk_errors, idle_chk_warnings);
	}

	BXE_PRINTF(
	    "----------------------------"
	    "----------------"
	    "----------------------------\n");

bxe_idle_chk_exit:
	return (idle_chk_errors);
}

#endif /* _BXE_SELF_TEST_H */
