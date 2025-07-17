/**************************************************************************
SPDX-License-Identifier: BSD-2-Clause

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <sys/cdefs.h>
#include <common/cxgb_common.h>
#include <common/cxgb_regs.h>

enum {
	IDT75P52100 = 4,
	IDT75N43102 = 5
};

/* DBGI command mode */
enum {
	DBGI_MODE_MBUS = 0,
	DBGI_MODE_IDT52100 = 5
};

/* IDT 75P52100 commands */
#define IDT_CMD_READ   0
#define IDT_CMD_WRITE  1
#define IDT_CMD_SEARCH 2
#define IDT_CMD_LEARN  3

/* IDT LAR register address and value for 144-bit mode (low 32 bits) */
#define IDT_LAR_ADR0   	0x180006
#define IDT_LAR_MODE144	0xffff0000

/* IDT SCR and SSR addresses (low 32 bits) */
#define IDT_SCR_ADR0  0x180000
#define IDT_SSR0_ADR0 0x180002
#define IDT_SSR1_ADR0 0x180004

/* IDT GMR base address (low 32 bits) */
#define IDT_GMR_BASE_ADR0 0x180020

/* IDT data and mask array base addresses (low 32 bits) */
#define IDT_DATARY_BASE_ADR0 0
#define IDT_MSKARY_BASE_ADR0 0x80000

/* IDT 75N43102 commands */
#define IDT4_CMD_SEARCH144 3
#define IDT4_CMD_WRITE     4
#define IDT4_CMD_READ      5

/* IDT 75N43102 SCR address (low 32 bits) */
#define IDT4_SCR_ADR0  0x3

/* IDT 75N43102 GMR base addresses (low 32 bits) */
#define IDT4_GMR_BASE0 0x10
#define IDT4_GMR_BASE1 0x20
#define IDT4_GMR_BASE2 0x30

/* IDT 75N43102 data and mask array base addresses (low 32 bits) */
#define IDT4_DATARY_BASE_ADR0 0x1000000
#define IDT4_MSKARY_BASE_ADR0 0x2000000

#define MAX_WRITE_ATTEMPTS 5

#define MAX_ROUTES 2048

/*
 * Issue a command to the TCAM and wait for its completion.  The address and
 * any data required by the command must have been setup by the caller.
 */
static int mc5_cmd_write(adapter_t *adapter, u32 cmd)
{
	t3_write_reg(adapter, A_MC5_DB_DBGI_REQ_CMD, cmd);
	return t3_wait_op_done(adapter, A_MC5_DB_DBGI_RSP_STATUS,
			       F_DBGIRSPVALID, 1, MAX_WRITE_ATTEMPTS, 1);
}

static inline void dbgi_wr_data3(adapter_t *adapter, u32 v1, u32 v2, u32 v3)
{
	t3_write_reg(adapter, A_MC5_DB_DBGI_REQ_DATA0, v1);
	t3_write_reg(adapter, A_MC5_DB_DBGI_REQ_DATA1, v2);
	t3_write_reg(adapter, A_MC5_DB_DBGI_REQ_DATA2, v3);
}

static inline void dbgi_rd_rsp3(adapter_t *adapter, u32 *v1, u32 *v2, u32 *v3)
{
	*v1 = t3_read_reg(adapter, A_MC5_DB_DBGI_RSP_DATA0);
	*v2 = t3_read_reg(adapter, A_MC5_DB_DBGI_RSP_DATA1);
	*v3 = t3_read_reg(adapter, A_MC5_DB_DBGI_RSP_DATA2);
}

/*
 * Write data to the TCAM register at address (0, 0, addr_lo) using the TCAM
 * command cmd.  The data to be written must have been set up by the caller.
 * Returns -1 on failure, 0 on success.
 */
static int mc5_write(adapter_t *adapter, u32 addr_lo, u32 cmd)
{
	t3_write_reg(adapter, A_MC5_DB_DBGI_REQ_ADDR0, addr_lo);
	if (mc5_cmd_write(adapter, cmd) == 0)
		return 0;
	CH_ERR(adapter, "MC5 timeout writing to TCAM address 0x%x\n", addr_lo);
	return -1;
}

static int init_mask_data_array(struct mc5 *mc5, u32 mask_array_base,
				u32 data_array_base, u32 write_cmd,
			        int addr_shift)
{
	unsigned int i;
	adapter_t *adap = mc5->adapter;

	/*
	 * We need the size of the TCAM data and mask arrays in terms of
	 * 72-bit entries.
	 */
	unsigned int size72 = mc5->tcam_size;
	unsigned int server_base = t3_read_reg(adap, A_MC5_DB_SERVER_INDEX);

	if (mc5->mode == MC5_MODE_144_BIT) {
		size72 *= 2;      /* 1 144-bit entry is 2 72-bit entries */
		server_base *= 2;
	}

	/* Clear the data array */
	dbgi_wr_data3(adap, 0, 0, 0);
	for (i = 0; i < size72; i++)
		if (mc5_write(adap, data_array_base + (i << addr_shift),
			      write_cmd))
			return -1;

	/* Initialize the mask array. */
	for (i = 0; i < server_base; i++) {
		dbgi_wr_data3(adap, 0x3fffffff, 0xfff80000, 0xff);
		if (mc5_write(adap, mask_array_base + (i << addr_shift),
			      write_cmd))
			return -1;
		i++;
		dbgi_wr_data3(adap, 0xffffffff, 0xffffffff, 0xff);
		if (mc5_write(adap, mask_array_base + (i << addr_shift),
			      write_cmd))
			return -1;
	}

	dbgi_wr_data3(adap,
		      mc5->mode == MC5_MODE_144_BIT ? 0xfffffff9 : 0xfffffffd,
		      0xffffffff, 0xff);
	for (; i < size72; i++)
		if (mc5_write(adap, mask_array_base + (i << addr_shift),
			      write_cmd))
			return -1;

	return 0;
}

static int init_idt52100(struct mc5 *mc5)
{
	int i;
	adapter_t *adap = mc5->adapter;

	t3_write_reg(adap, A_MC5_DB_RSP_LATENCY,
		     V_RDLAT(0x15) | V_LRNLAT(0x15) | V_SRCHLAT(0x15));
	t3_write_reg(adap, A_MC5_DB_PART_ID_INDEX, 2);

	/*
	 * Use GMRs 14-15 for ELOOKUP, GMRs 12-13 for SYN lookups, and
	 * GMRs 8-9 for ACK- and AOPEN searches.
	 */
	t3_write_reg(adap, A_MC5_DB_POPEN_DATA_WR_CMD, IDT_CMD_WRITE);
	t3_write_reg(adap, A_MC5_DB_POPEN_MASK_WR_CMD, IDT_CMD_WRITE);
	t3_write_reg(adap, A_MC5_DB_AOPEN_SRCH_CMD, IDT_CMD_SEARCH);
	t3_write_reg(adap, A_MC5_DB_AOPEN_LRN_CMD, IDT_CMD_LEARN);
	t3_write_reg(adap, A_MC5_DB_SYN_SRCH_CMD, IDT_CMD_SEARCH | 0x6000);
	t3_write_reg(adap, A_MC5_DB_SYN_LRN_CMD, IDT_CMD_LEARN);
	t3_write_reg(adap, A_MC5_DB_ACK_SRCH_CMD, IDT_CMD_SEARCH);
	t3_write_reg(adap, A_MC5_DB_ACK_LRN_CMD, IDT_CMD_LEARN);
	t3_write_reg(adap, A_MC5_DB_ILOOKUP_CMD, IDT_CMD_SEARCH);
	t3_write_reg(adap, A_MC5_DB_ELOOKUP_CMD, IDT_CMD_SEARCH | 0x7000);
	t3_write_reg(adap, A_MC5_DB_DATA_WRITE_CMD, IDT_CMD_WRITE);
	t3_write_reg(adap, A_MC5_DB_DATA_READ_CMD, IDT_CMD_READ);

	/* Set DBGI command mode for IDT TCAM. */
	t3_write_reg(adap, A_MC5_DB_DBGI_CONFIG, DBGI_MODE_IDT52100);

	/* Set up LAR */
	dbgi_wr_data3(adap, IDT_LAR_MODE144, 0, 0);
	if (mc5_write(adap, IDT_LAR_ADR0, IDT_CMD_WRITE))
		goto err;

	/* Set up SSRs */
	dbgi_wr_data3(adap, 0xffffffff, 0xffffffff, 0);
	if (mc5_write(adap, IDT_SSR0_ADR0, IDT_CMD_WRITE) ||
	    mc5_write(adap, IDT_SSR1_ADR0, IDT_CMD_WRITE))
		goto err;

	/* Set up GMRs */
	for (i = 0; i < 32; ++i) {
		if (i >= 12 && i < 15)
			dbgi_wr_data3(adap, 0xfffffff9, 0xffffffff, 0xff);
		else if (i == 15)
			dbgi_wr_data3(adap, 0xfffffff9, 0xffff8007, 0xff);
		else
			dbgi_wr_data3(adap, 0xffffffff, 0xffffffff, 0xff);

		if (mc5_write(adap, IDT_GMR_BASE_ADR0 + i, IDT_CMD_WRITE))
			goto err;
	}

	/* Set up SCR */
	dbgi_wr_data3(adap, 1, 0, 0);
	if (mc5_write(adap, IDT_SCR_ADR0, IDT_CMD_WRITE))
		goto err;

	return init_mask_data_array(mc5, IDT_MSKARY_BASE_ADR0,
				    IDT_DATARY_BASE_ADR0, IDT_CMD_WRITE, 0);
 err:
	return -EIO;
}

static int init_idt43102(struct mc5 *mc5)
{
	int i;
	adapter_t *adap = mc5->adapter;

	t3_write_reg(adap, A_MC5_DB_RSP_LATENCY,
		     adap->params.rev == 0 ? V_RDLAT(0xd) | V_SRCHLAT(0x11) :
					     V_RDLAT(0xd) | V_SRCHLAT(0x12));

	/*
	 * Use GMRs 24-25 for ELOOKUP, GMRs 20-21 for SYN lookups, and no mask
	 * for ACK- and AOPEN searches.
	 */
	t3_write_reg(adap, A_MC5_DB_POPEN_DATA_WR_CMD, IDT4_CMD_WRITE);
	t3_write_reg(adap, A_MC5_DB_POPEN_MASK_WR_CMD, IDT4_CMD_WRITE);
	t3_write_reg(adap, A_MC5_DB_AOPEN_SRCH_CMD,
		     IDT4_CMD_SEARCH144 | 0x3800);
	t3_write_reg(adap, A_MC5_DB_SYN_SRCH_CMD, IDT4_CMD_SEARCH144);
	t3_write_reg(adap, A_MC5_DB_ACK_SRCH_CMD, IDT4_CMD_SEARCH144 | 0x3800);
	t3_write_reg(adap, A_MC5_DB_ILOOKUP_CMD, IDT4_CMD_SEARCH144 | 0x3800);
	t3_write_reg(adap, A_MC5_DB_ELOOKUP_CMD, IDT4_CMD_SEARCH144 | 0x800);
	t3_write_reg(adap, A_MC5_DB_DATA_WRITE_CMD, IDT4_CMD_WRITE);
	t3_write_reg(adap, A_MC5_DB_DATA_READ_CMD, IDT4_CMD_READ);

	t3_write_reg(adap, A_MC5_DB_PART_ID_INDEX, 3);

	/* Set DBGI command mode for IDT TCAM. */
	t3_write_reg(adap, A_MC5_DB_DBGI_CONFIG, DBGI_MODE_IDT52100);

	/* Set up GMRs */
	dbgi_wr_data3(adap, 0xffffffff, 0xffffffff, 0xff);
	for (i = 0; i < 7; ++i)
		if (mc5_write(adap, IDT4_GMR_BASE0 + i, IDT4_CMD_WRITE))
			goto err;

	for (i = 0; i < 4; ++i)
		if (mc5_write(adap, IDT4_GMR_BASE2 + i, IDT4_CMD_WRITE))
			goto err;

	dbgi_wr_data3(adap, 0xfffffff9, 0xffffffff, 0xff);
	if (mc5_write(adap, IDT4_GMR_BASE1, IDT4_CMD_WRITE) ||
	    mc5_write(adap, IDT4_GMR_BASE1 + 1, IDT4_CMD_WRITE) ||
	    mc5_write(adap, IDT4_GMR_BASE1 + 4, IDT4_CMD_WRITE))
		goto err;

	dbgi_wr_data3(adap, 0xfffffff9, 0xffff8007, 0xff);
	if (mc5_write(adap, IDT4_GMR_BASE1 + 5, IDT4_CMD_WRITE))
		goto err;

	/* Set up SCR */
	dbgi_wr_data3(adap, 0xf0000000, 0, 0);
	if (mc5_write(adap, IDT4_SCR_ADR0, IDT4_CMD_WRITE))
		goto err;

	return init_mask_data_array(mc5, IDT4_MSKARY_BASE_ADR0,
				    IDT4_DATARY_BASE_ADR0, IDT4_CMD_WRITE, 1);
 err:
	return -EIO;
}

/* Put MC5 in DBGI mode. */
static inline void mc5_dbgi_mode_enable(const struct mc5 *mc5)
{
	t3_set_reg_field(mc5->adapter, A_MC5_DB_CONFIG, F_PRTYEN | F_MBUSEN,
			 F_DBGIEN);
}

/* Put MC5 in M-Bus mode. */
static void mc5_dbgi_mode_disable(const struct mc5 *mc5)
{
	t3_set_reg_field(mc5->adapter, A_MC5_DB_CONFIG, F_DBGIEN,
			 V_PRTYEN(mc5->parity_enabled) | F_MBUSEN);
}

/**
 *	t3_mc5_init - initialize MC5 and the TCAM
 *	@mc5: the MC5 handle
 *	@nservers: desired number the TCP servers (listening ports)
 *	@nfilters: desired number of HW filters (classifiers)
 *	@nroutes: desired number of routes
 *
 *	Initialize MC5 and the TCAM and partition the TCAM for the requested
 *	number of servers, filters, and routes.  The number of routes is
 *	typically 0 except for specialized uses of the T3 adapters.
 */
int t3_mc5_init(struct mc5 *mc5, unsigned int nservers, unsigned int nfilters,
		unsigned int nroutes)
{
	int err;
	unsigned int tcam_size = mc5->tcam_size;
	unsigned int mode72 = mc5->mode == MC5_MODE_72_BIT;
	adapter_t *adap = mc5->adapter;

	if (!tcam_size)
		return 0;

	if (nroutes > MAX_ROUTES || nroutes + nservers + nfilters > tcam_size)
		return -EINVAL;

	if (nfilters)
		mc5->parity_enabled = 0;

	/* Reset the TCAM */
	t3_set_reg_field(adap, A_MC5_DB_CONFIG, F_TMMODE | F_COMPEN,
			 V_COMPEN(mode72) | V_TMMODE(mode72) | F_TMRST);
	if (t3_wait_op_done(adap, A_MC5_DB_CONFIG, F_TMRDY, 1, 500, 0)) {
		CH_ERR(adap, "TCAM reset timed out\n");
		return -1;
	}

	t3_write_reg(adap, A_MC5_DB_ROUTING_TABLE_INDEX, tcam_size - nroutes);
	t3_write_reg(adap, A_MC5_DB_FILTER_TABLE,
		     tcam_size - nroutes - nfilters);
	t3_write_reg(adap, A_MC5_DB_SERVER_INDEX,
		     tcam_size - nroutes - nfilters - nservers);

	/* All the TCAM addresses we access have only the low 32 bits non 0 */
	t3_write_reg(adap, A_MC5_DB_DBGI_REQ_ADDR1, 0);
	t3_write_reg(adap, A_MC5_DB_DBGI_REQ_ADDR2, 0);

	mc5_dbgi_mode_enable(mc5);

	switch (mc5->part_type) {
	case IDT75P52100:
		err = init_idt52100(mc5);
		break;
	case IDT75N43102:
		err = init_idt43102(mc5);
		break;
	default:
		CH_ERR(adap, "Unsupported TCAM type %d\n", mc5->part_type);
		err = -EINVAL;
		break;
	}

	mc5_dbgi_mode_disable(mc5);
	return err;
}

/**
 *	read_mc5_range - dump a part of the memory managed by MC5
 *	@mc5: the MC5 handle
 *	@start: the start address for the dump
 *	@n: number of 72-bit words to read
 *	@buf: result buffer
 *
 *	Read n 72-bit words from MC5 memory from the given start location.
 */
int t3_read_mc5_range(const struct mc5 *mc5, unsigned int start,
		      unsigned int n, u32 *buf)
{
	u32 read_cmd;
	int err = 0;
	adapter_t *adap = mc5->adapter;

	if (mc5->part_type == IDT75P52100)
		read_cmd = IDT_CMD_READ;
	else if (mc5->part_type == IDT75N43102)
		read_cmd = IDT4_CMD_READ;
	else
		return -EINVAL;

	mc5_dbgi_mode_enable(mc5);

	while (n--) {
		t3_write_reg(adap, A_MC5_DB_DBGI_REQ_ADDR0, start++);
		if (mc5_cmd_write(adap, read_cmd)) {
			err = -EIO;
			break;
		}
		dbgi_rd_rsp3(adap, buf + 2, buf + 1, buf);
		buf += 3;
	}

	mc5_dbgi_mode_disable(mc5);
	return err;
}

#define MC5_INT_FATAL (F_PARITYERR | F_REQQPARERR | F_DISPQPARERR)

/**
 *	t3_mc5_intr_handler - MC5 interrupt handler
 *	@mc5: the MC5 handle
 *
 *	The MC5 interrupt handler.
 */
void t3_mc5_intr_handler(struct mc5 *mc5)
{
	adapter_t *adap = mc5->adapter;
	u32 cause = t3_read_reg(adap, A_MC5_DB_INT_CAUSE);

	if ((cause & F_PARITYERR) && mc5->parity_enabled) {
		CH_ALERT(adap, "MC5 parity error\n");
		mc5->stats.parity_err++;
	}

	if (cause & F_REQQPARERR) {
		CH_ALERT(adap, "MC5 request queue parity error\n");
		mc5->stats.reqq_parity_err++;
	}

	if (cause & F_DISPQPARERR) {
		CH_ALERT(adap, "MC5 dispatch queue parity error\n");
		mc5->stats.dispq_parity_err++;
	}

	if (cause & F_ACTRGNFULL)
		mc5->stats.active_rgn_full++;
	if (cause & F_NFASRCHFAIL)
		mc5->stats.nfa_srch_err++;
	if (cause & F_UNKNOWNCMD)
		mc5->stats.unknown_cmd++;
	if (cause & F_DELACTEMPTY)
		mc5->stats.del_act_empty++;
	if (cause & MC5_INT_FATAL)
		t3_fatal_err(adap);

	t3_write_reg(adap, A_MC5_DB_INT_CAUSE, cause);
}

/**
 *	t3_mc5_prep - initialize the SW state for MC5
 *	@adapter: the adapter
 *	@mc5: the MC5 handle
 *	@mode: whether the TCAM will be in 72- or 144-bit mode
 *
 *	Initialize the SW state associated with MC5.  Among other things
 *	this determines the size of the attached TCAM.
 */
void __devinit t3_mc5_prep(adapter_t *adapter, struct mc5 *mc5, int mode)
{
#define K * 1024

	static unsigned int tcam_part_size[] = {  /* in K 72-bit entries */
		64 K, 128 K, 256 K, 32 K
	};

#undef K

	u32 cfg = t3_read_reg(adapter, A_MC5_DB_CONFIG);

	mc5->adapter = adapter;
	mc5->parity_enabled = 1;
	mc5->mode = (unsigned char) mode;
	mc5->part_type = (unsigned char) G_TMTYPE(cfg);
	if (cfg & F_TMTYPEHI)
		mc5->part_type |= 4;

	mc5->tcam_size = tcam_part_size[G_TMPARTSIZE(cfg)];
	if (mode == MC5_MODE_144_BIT)
		mc5->tcam_size /= 2;
}
