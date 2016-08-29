/*-
 * Copyright (c) 2012, 2016 Chelsio Communications, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/eventhandler.h>

#include "common.h"
#include "t4_regs.h"
#include "t4_regs_values.h"
#include "firmware/t4fw_interface.h"

#undef msleep
#define msleep(x) do { \
	if (cold) \
		DELAY((x) * 1000); \
	else \
		pause("t4hw", (x) * hz / 1000); \
} while (0)

/**
 *	t4_wait_op_done_val - wait until an operation is completed
 *	@adapter: the adapter performing the operation
 *	@reg: the register to check for completion
 *	@mask: a single-bit field within @reg that indicates completion
 *	@polarity: the value of the field when the operation is completed
 *	@attempts: number of check iterations
 *	@delay: delay in usecs between iterations
 *	@valp: where to store the value of the register at completion time
 *
 *	Wait until an operation is completed by checking a bit in a register
 *	up to @attempts times.  If @valp is not NULL the value of the register
 *	at the time it indicated completion is stored there.  Returns 0 if the
 *	operation completes and	-EAGAIN	otherwise.
 */
static int t4_wait_op_done_val(struct adapter *adapter, int reg, u32 mask,
			       int polarity, int attempts, int delay, u32 *valp)
{
	while (1) {
		u32 val = t4_read_reg(adapter, reg);

		if (!!(val & mask) == polarity) {
			if (valp)
				*valp = val;
			return 0;
		}
		if (--attempts == 0)
			return -EAGAIN;
		if (delay)
			udelay(delay);
	}
}

static inline int t4_wait_op_done(struct adapter *adapter, int reg, u32 mask,
				  int polarity, int attempts, int delay)
{
	return t4_wait_op_done_val(adapter, reg, mask, polarity, attempts,
				   delay, NULL);
}

/**
 *	t4_set_reg_field - set a register field to a value
 *	@adapter: the adapter to program
 *	@addr: the register address
 *	@mask: specifies the portion of the register to modify
 *	@val: the new value for the register field
 *
 *	Sets a register field specified by the supplied mask to the
 *	given value.
 */
void t4_set_reg_field(struct adapter *adapter, unsigned int addr, u32 mask,
		      u32 val)
{
	u32 v = t4_read_reg(adapter, addr) & ~mask;

	t4_write_reg(adapter, addr, v | val);
	(void) t4_read_reg(adapter, addr);      /* flush */
}

/**
 *	t4_read_indirect - read indirectly addressed registers
 *	@adap: the adapter
 *	@addr_reg: register holding the indirect address
 *	@data_reg: register holding the value of the indirect register
 *	@vals: where the read register values are stored
 *	@nregs: how many indirect registers to read
 *	@start_idx: index of first indirect register to read
 *
 *	Reads registers that are accessed indirectly through an address/data
 *	register pair.
 */
void t4_read_indirect(struct adapter *adap, unsigned int addr_reg,
			     unsigned int data_reg, u32 *vals,
			     unsigned int nregs, unsigned int start_idx)
{
	while (nregs--) {
		t4_write_reg(adap, addr_reg, start_idx);
		*vals++ = t4_read_reg(adap, data_reg);
		start_idx++;
	}
}

/**
 *	t4_write_indirect - write indirectly addressed registers
 *	@adap: the adapter
 *	@addr_reg: register holding the indirect addresses
 *	@data_reg: register holding the value for the indirect registers
 *	@vals: values to write
 *	@nregs: how many indirect registers to write
 *	@start_idx: address of first indirect register to write
 *
 *	Writes a sequential block of registers that are accessed indirectly
 *	through an address/data register pair.
 */
void t4_write_indirect(struct adapter *adap, unsigned int addr_reg,
		       unsigned int data_reg, const u32 *vals,
		       unsigned int nregs, unsigned int start_idx)
{
	while (nregs--) {
		t4_write_reg(adap, addr_reg, start_idx++);
		t4_write_reg(adap, data_reg, *vals++);
	}
}

/*
 * Read a 32-bit PCI Configuration Space register via the PCI-E backdoor
 * mechanism.  This guarantees that we get the real value even if we're
 * operating within a Virtual Machine and the Hypervisor is trapping our
 * Configuration Space accesses.
 *
 * N.B. This routine should only be used as a last resort: the firmware uses
 *      the backdoor registers on a regular basis and we can end up
 *      conflicting with it's uses!
 */
u32 t4_hw_pci_read_cfg4(adapter_t *adap, int reg)
{
	u32 req = V_FUNCTION(adap->pf) | V_REGISTER(reg);
	u32 val;

	if (chip_id(adap) <= CHELSIO_T5)
		req |= F_ENABLE;
	else
		req |= F_T6_ENABLE;

	if (is_t4(adap))
		req |= F_LOCALCFG;

	t4_write_reg(adap, A_PCIE_CFG_SPACE_REQ, req);
	val = t4_read_reg(adap, A_PCIE_CFG_SPACE_DATA);

	/*
	 * Reset F_ENABLE to 0 so reads of PCIE_CFG_SPACE_DATA won't cause a
	 * Configuration Space read.  (None of the other fields matter when
	 * F_ENABLE is 0 so a simple register write is easier than a
	 * read-modify-write via t4_set_reg_field().)
	 */
	t4_write_reg(adap, A_PCIE_CFG_SPACE_REQ, 0);

	return val;
}

/*
 * t4_report_fw_error - report firmware error
 * @adap: the adapter
 *
 * The adapter firmware can indicate error conditions to the host.
 * If the firmware has indicated an error, print out the reason for
 * the firmware error.
 */
static void t4_report_fw_error(struct adapter *adap)
{
	static const char *const reason[] = {
		"Crash",			/* PCIE_FW_EVAL_CRASH */
		"During Device Preparation",	/* PCIE_FW_EVAL_PREP */
		"During Device Configuration",	/* PCIE_FW_EVAL_CONF */
		"During Device Initialization",	/* PCIE_FW_EVAL_INIT */
		"Unexpected Event",		/* PCIE_FW_EVAL_UNEXPECTEDEVENT */
		"Insufficient Airflow",		/* PCIE_FW_EVAL_OVERHEAT */
		"Device Shutdown",		/* PCIE_FW_EVAL_DEVICESHUTDOWN */
		"Reserved",			/* reserved */
	};
	u32 pcie_fw;

	pcie_fw = t4_read_reg(adap, A_PCIE_FW);
	if (pcie_fw & F_PCIE_FW_ERR)
		CH_ERR(adap, "Firmware reports adapter error: %s\n",
			reason[G_PCIE_FW_EVAL(pcie_fw)]);
}

/*
 * Get the reply to a mailbox command and store it in @rpl in big-endian order.
 */
static void get_mbox_rpl(struct adapter *adap, __be64 *rpl, int nflit,
			 u32 mbox_addr)
{
	for ( ; nflit; nflit--, mbox_addr += 8)
		*rpl++ = cpu_to_be64(t4_read_reg64(adap, mbox_addr));
}

/*
 * Handle a FW assertion reported in a mailbox.
 */
static void fw_asrt(struct adapter *adap, struct fw_debug_cmd *asrt)
{
	CH_ALERT(adap,
		  "FW assertion at %.16s:%u, val0 %#x, val1 %#x\n",
		  asrt->u.assert.filename_0_7,
		  be32_to_cpu(asrt->u.assert.line),
		  be32_to_cpu(asrt->u.assert.x),
		  be32_to_cpu(asrt->u.assert.y));
}

#define X_CIM_PF_NOACCESS 0xeeeeeeee
/**
 *	t4_wr_mbox_meat_timeout - send a command to FW through the given mailbox
 *	@adap: the adapter
 *	@mbox: index of the mailbox to use
 *	@cmd: the command to write
 *	@size: command length in bytes
 *	@rpl: where to optionally store the reply
 *	@sleep_ok: if true we may sleep while awaiting command completion
 *	@timeout: time to wait for command to finish before timing out
 *		(negative implies @sleep_ok=false)
 *
 *	Sends the given command to FW through the selected mailbox and waits
 *	for the FW to execute the command.  If @rpl is not %NULL it is used to
 *	store the FW's reply to the command.  The command and its optional
 *	reply are of the same length.  Some FW commands like RESET and
 *	INITIALIZE can take a considerable amount of time to execute.
 *	@sleep_ok determines whether we may sleep while awaiting the response.
 *	If sleeping is allowed we use progressive backoff otherwise we spin.
 *	Note that passing in a negative @timeout is an alternate mechanism
 *	for specifying @sleep_ok=false.  This is useful when a higher level
 *	interface allows for specification of @timeout but not @sleep_ok ...
 *
 *	The return value is 0 on success or a negative errno on failure.  A
 *	failure can happen either because we are not able to execute the
 *	command or FW executes it but signals an error.  In the latter case
 *	the return value is the error code indicated by FW (negated).
 */
int t4_wr_mbox_meat_timeout(struct adapter *adap, int mbox, const void *cmd,
			    int size, void *rpl, bool sleep_ok, int timeout)
{
	/*
	 * We delay in small increments at first in an effort to maintain
	 * responsiveness for simple, fast executing commands but then back
	 * off to larger delays to a maximum retry delay.
	 */
	static const int delay[] = {
		1, 1, 3, 5, 10, 10, 20, 50, 100
	};
	u32 v;
	u64 res;
	int i, ms, delay_idx, ret;
	const __be64 *p = cmd;
	u32 data_reg = PF_REG(mbox, A_CIM_PF_MAILBOX_DATA);
	u32 ctl_reg = PF_REG(mbox, A_CIM_PF_MAILBOX_CTRL);
	u32 ctl;
	__be64 cmd_rpl[MBOX_LEN/8];
	u32 pcie_fw;

	if ((size & 15) || size > MBOX_LEN)
		return -EINVAL;

	if (adap->flags & IS_VF) {
		if (is_t6(adap))
			data_reg = FW_T6VF_MBDATA_BASE_ADDR;
		else
			data_reg = FW_T4VF_MBDATA_BASE_ADDR;
		ctl_reg = VF_CIM_REG(A_CIM_VF_EXT_MAILBOX_CTRL);
	}

	/*
	 * If we have a negative timeout, that implies that we can't sleep.
	 */
	if (timeout < 0) {
		sleep_ok = false;
		timeout = -timeout;
	}

	/*
	 * Attempt to gain access to the mailbox.
	 */
	for (i = 0; i < 4; i++) {
		ctl = t4_read_reg(adap, ctl_reg);
		v = G_MBOWNER(ctl);
		if (v != X_MBOWNER_NONE)
			break;
	}

	/*
	 * If we were unable to gain access, dequeue ourselves from the
	 * mailbox atomic access list and report the error to our caller.
	 */
	if (v != X_MBOWNER_PL) {
		t4_report_fw_error(adap);
		ret = (v == X_MBOWNER_FW) ? -EBUSY : -ETIMEDOUT;
		return ret;
	}

	/*
	 * If we gain ownership of the mailbox and there's a "valid" message
	 * in it, this is likely an asynchronous error message from the
	 * firmware.  So we'll report that and then proceed on with attempting
	 * to issue our own command ... which may well fail if the error
	 * presaged the firmware crashing ...
	 */
	if (ctl & F_MBMSGVALID) {
		CH_ERR(adap, "found VALID command in mbox %u: "
		       "%llx %llx %llx %llx %llx %llx %llx %llx\n", mbox,
		       (unsigned long long)t4_read_reg64(adap, data_reg),
		       (unsigned long long)t4_read_reg64(adap, data_reg + 8),
		       (unsigned long long)t4_read_reg64(adap, data_reg + 16),
		       (unsigned long long)t4_read_reg64(adap, data_reg + 24),
		       (unsigned long long)t4_read_reg64(adap, data_reg + 32),
		       (unsigned long long)t4_read_reg64(adap, data_reg + 40),
		       (unsigned long long)t4_read_reg64(adap, data_reg + 48),
		       (unsigned long long)t4_read_reg64(adap, data_reg + 56));
	}

	/*
	 * Copy in the new mailbox command and send it on its way ...
	 */
	for (i = 0; i < size; i += 8, p++)
		t4_write_reg64(adap, data_reg + i, be64_to_cpu(*p));

	if (adap->flags & IS_VF) {
		/*
		 * For the VFs, the Mailbox Data "registers" are
		 * actually backed by T4's "MA" interface rather than
		 * PL Registers (as is the case for the PFs).  Because
		 * these are in different coherency domains, the write
		 * to the VF's PL-register-backed Mailbox Control can
		 * race in front of the writes to the MA-backed VF
		 * Mailbox Data "registers".  So we need to do a
		 * read-back on at least one byte of the VF Mailbox
		 * Data registers before doing the write to the VF
		 * Mailbox Control register.
		 */
		t4_read_reg(adap, data_reg);
	}

	CH_DUMP_MBOX(adap, mbox, data_reg);

	t4_write_reg(adap, ctl_reg, F_MBMSGVALID | V_MBOWNER(X_MBOWNER_FW));
	t4_read_reg(adap, ctl_reg);	/* flush write */

	delay_idx = 0;
	ms = delay[0];

	/*
	 * Loop waiting for the reply; bail out if we time out or the firmware
	 * reports an error.
	 */
	pcie_fw = 0;
	for (i = 0; i < timeout; i += ms) {
		if (!(adap->flags & IS_VF)) {
			pcie_fw = t4_read_reg(adap, A_PCIE_FW);
			if (pcie_fw & F_PCIE_FW_ERR)
				break;
		}
		if (sleep_ok) {
			ms = delay[delay_idx];  /* last element may repeat */
			if (delay_idx < ARRAY_SIZE(delay) - 1)
				delay_idx++;
			msleep(ms);
		} else {
			mdelay(ms);
		}

		v = t4_read_reg(adap, ctl_reg);
		if (v == X_CIM_PF_NOACCESS)
			continue;
		if (G_MBOWNER(v) == X_MBOWNER_PL) {
			if (!(v & F_MBMSGVALID)) {
				t4_write_reg(adap, ctl_reg,
					     V_MBOWNER(X_MBOWNER_NONE));
				continue;
			}

			/*
			 * Retrieve the command reply and release the mailbox.
			 */
			get_mbox_rpl(adap, cmd_rpl, MBOX_LEN/8, data_reg);
			t4_write_reg(adap, ctl_reg, V_MBOWNER(X_MBOWNER_NONE));

			CH_DUMP_MBOX(adap, mbox, data_reg);

			res = be64_to_cpu(cmd_rpl[0]);
			if (G_FW_CMD_OP(res >> 32) == FW_DEBUG_CMD) {
				fw_asrt(adap, (struct fw_debug_cmd *)cmd_rpl);
				res = V_FW_CMD_RETVAL(EIO);
			} else if (rpl)
				memcpy(rpl, cmd_rpl, size);
			return -G_FW_CMD_RETVAL((int)res);
		}
	}

	/*
	 * We timed out waiting for a reply to our mailbox command.  Report
	 * the error and also check to see if the firmware reported any
	 * errors ...
	 */
	ret = (pcie_fw & F_PCIE_FW_ERR) ? -ENXIO : -ETIMEDOUT;
	CH_ERR(adap, "command %#x in mailbox %d timed out\n",
	       *(const u8 *)cmd, mbox);

	t4_report_fw_error(adap);
	t4_fatal_err(adap);
	return ret;
}

int t4_wr_mbox_meat(struct adapter *adap, int mbox, const void *cmd, int size,
		    void *rpl, bool sleep_ok)
{
		return t4_wr_mbox_meat_timeout(adap, mbox, cmd, size, rpl,
					       sleep_ok, FW_CMD_MAX_TIMEOUT);

}

static int t4_edc_err_read(struct adapter *adap, int idx)
{
	u32 edc_ecc_err_addr_reg;
	u32 edc_bist_status_rdata_reg;

	if (is_t4(adap)) {
		CH_WARN(adap, "%s: T4 NOT supported.\n", __func__);
		return 0;
	}
	if (idx != 0 && idx != 1) {
		CH_WARN(adap, "%s: idx %d NOT supported.\n", __func__, idx);
		return 0;
	}

	edc_ecc_err_addr_reg = EDC_T5_REG(A_EDC_H_ECC_ERR_ADDR, idx);
	edc_bist_status_rdata_reg = EDC_T5_REG(A_EDC_H_BIST_STATUS_RDATA, idx);

	CH_WARN(adap,
		"edc%d err addr 0x%x: 0x%x.\n",
		idx, edc_ecc_err_addr_reg,
		t4_read_reg(adap, edc_ecc_err_addr_reg));
	CH_WARN(adap,
	 	"bist: 0x%x, status %llx %llx %llx %llx %llx %llx %llx %llx %llx.\n",
		edc_bist_status_rdata_reg,
		(unsigned long long)t4_read_reg64(adap, edc_bist_status_rdata_reg),
		(unsigned long long)t4_read_reg64(adap, edc_bist_status_rdata_reg + 8),
		(unsigned long long)t4_read_reg64(adap, edc_bist_status_rdata_reg + 16),
		(unsigned long long)t4_read_reg64(adap, edc_bist_status_rdata_reg + 24),
		(unsigned long long)t4_read_reg64(adap, edc_bist_status_rdata_reg + 32),
		(unsigned long long)t4_read_reg64(adap, edc_bist_status_rdata_reg + 40),
		(unsigned long long)t4_read_reg64(adap, edc_bist_status_rdata_reg + 48),
		(unsigned long long)t4_read_reg64(adap, edc_bist_status_rdata_reg + 56),
		(unsigned long long)t4_read_reg64(adap, edc_bist_status_rdata_reg + 64));

	return 0;
}

/**
 *	t4_mc_read - read from MC through backdoor accesses
 *	@adap: the adapter
 *	@idx: which MC to access
 *	@addr: address of first byte requested
 *	@data: 64 bytes of data containing the requested address
 *	@ecc: where to store the corresponding 64-bit ECC word
 *
 *	Read 64 bytes of data from MC starting at a 64-byte-aligned address
 *	that covers the requested address @addr.  If @parity is not %NULL it
 *	is assigned the 64-bit ECC word for the read data.
 */
int t4_mc_read(struct adapter *adap, int idx, u32 addr, __be32 *data, u64 *ecc)
{
	int i;
	u32 mc_bist_cmd_reg, mc_bist_cmd_addr_reg, mc_bist_cmd_len_reg;
	u32 mc_bist_status_rdata_reg, mc_bist_data_pattern_reg;

	if (is_t4(adap)) {
		mc_bist_cmd_reg = A_MC_BIST_CMD;
		mc_bist_cmd_addr_reg = A_MC_BIST_CMD_ADDR;
		mc_bist_cmd_len_reg = A_MC_BIST_CMD_LEN;
		mc_bist_status_rdata_reg = A_MC_BIST_STATUS_RDATA;
		mc_bist_data_pattern_reg = A_MC_BIST_DATA_PATTERN;
	} else {
		mc_bist_cmd_reg = MC_REG(A_MC_P_BIST_CMD, idx);
		mc_bist_cmd_addr_reg = MC_REG(A_MC_P_BIST_CMD_ADDR, idx);
		mc_bist_cmd_len_reg = MC_REG(A_MC_P_BIST_CMD_LEN, idx);
		mc_bist_status_rdata_reg = MC_REG(A_MC_P_BIST_STATUS_RDATA,
						  idx);
		mc_bist_data_pattern_reg = MC_REG(A_MC_P_BIST_DATA_PATTERN,
						  idx);
	}

	if (t4_read_reg(adap, mc_bist_cmd_reg) & F_START_BIST)
		return -EBUSY;
	t4_write_reg(adap, mc_bist_cmd_addr_reg, addr & ~0x3fU);
	t4_write_reg(adap, mc_bist_cmd_len_reg, 64);
	t4_write_reg(adap, mc_bist_data_pattern_reg, 0xc);
	t4_write_reg(adap, mc_bist_cmd_reg, V_BIST_OPCODE(1) |
		     F_START_BIST | V_BIST_CMD_GAP(1));
	i = t4_wait_op_done(adap, mc_bist_cmd_reg, F_START_BIST, 0, 10, 1);
	if (i)
		return i;

#define MC_DATA(i) MC_BIST_STATUS_REG(mc_bist_status_rdata_reg, i)

	for (i = 15; i >= 0; i--)
		*data++ = ntohl(t4_read_reg(adap, MC_DATA(i)));
	if (ecc)
		*ecc = t4_read_reg64(adap, MC_DATA(16));
#undef MC_DATA
	return 0;
}

/**
 *	t4_edc_read - read from EDC through backdoor accesses
 *	@adap: the adapter
 *	@idx: which EDC to access
 *	@addr: address of first byte requested
 *	@data: 64 bytes of data containing the requested address
 *	@ecc: where to store the corresponding 64-bit ECC word
 *
 *	Read 64 bytes of data from EDC starting at a 64-byte-aligned address
 *	that covers the requested address @addr.  If @parity is not %NULL it
 *	is assigned the 64-bit ECC word for the read data.
 */
int t4_edc_read(struct adapter *adap, int idx, u32 addr, __be32 *data, u64 *ecc)
{
	int i;
	u32 edc_bist_cmd_reg, edc_bist_cmd_addr_reg, edc_bist_cmd_len_reg;
	u32 edc_bist_cmd_data_pattern, edc_bist_status_rdata_reg;

	if (is_t4(adap)) {
		edc_bist_cmd_reg = EDC_REG(A_EDC_BIST_CMD, idx);
		edc_bist_cmd_addr_reg = EDC_REG(A_EDC_BIST_CMD_ADDR, idx);
		edc_bist_cmd_len_reg = EDC_REG(A_EDC_BIST_CMD_LEN, idx);
		edc_bist_cmd_data_pattern = EDC_REG(A_EDC_BIST_DATA_PATTERN,
						    idx);
		edc_bist_status_rdata_reg = EDC_REG(A_EDC_BIST_STATUS_RDATA,
						    idx);
	} else {
/*
 * These macro are missing in t4_regs.h file.
 * Added temporarily for testing.
 */
#define EDC_STRIDE_T5 (EDC_T51_BASE_ADDR - EDC_T50_BASE_ADDR)
#define EDC_REG_T5(reg, idx) (reg + EDC_STRIDE_T5 * idx)
		edc_bist_cmd_reg = EDC_REG_T5(A_EDC_H_BIST_CMD, idx);
		edc_bist_cmd_addr_reg = EDC_REG_T5(A_EDC_H_BIST_CMD_ADDR, idx);
		edc_bist_cmd_len_reg = EDC_REG_T5(A_EDC_H_BIST_CMD_LEN, idx);
		edc_bist_cmd_data_pattern = EDC_REG_T5(A_EDC_H_BIST_DATA_PATTERN,
						    idx);
		edc_bist_status_rdata_reg = EDC_REG_T5(A_EDC_H_BIST_STATUS_RDATA,
						    idx);
#undef EDC_REG_T5
#undef EDC_STRIDE_T5
	}

	if (t4_read_reg(adap, edc_bist_cmd_reg) & F_START_BIST)
		return -EBUSY;
	t4_write_reg(adap, edc_bist_cmd_addr_reg, addr & ~0x3fU);
	t4_write_reg(adap, edc_bist_cmd_len_reg, 64);
	t4_write_reg(adap, edc_bist_cmd_data_pattern, 0xc);
	t4_write_reg(adap, edc_bist_cmd_reg,
		     V_BIST_OPCODE(1) | V_BIST_CMD_GAP(1) | F_START_BIST);
	i = t4_wait_op_done(adap, edc_bist_cmd_reg, F_START_BIST, 0, 10, 1);
	if (i)
		return i;

#define EDC_DATA(i) EDC_BIST_STATUS_REG(edc_bist_status_rdata_reg, i)

	for (i = 15; i >= 0; i--)
		*data++ = ntohl(t4_read_reg(adap, EDC_DATA(i)));
	if (ecc)
		*ecc = t4_read_reg64(adap, EDC_DATA(16));
#undef EDC_DATA
	return 0;
}

/**
 *	t4_mem_read - read EDC 0, EDC 1 or MC into buffer
 *	@adap: the adapter
 *	@mtype: memory type: MEM_EDC0, MEM_EDC1 or MEM_MC
 *	@addr: address within indicated memory type
 *	@len: amount of memory to read
 *	@buf: host memory buffer
 *
 *	Reads an [almost] arbitrary memory region in the firmware: the
 *	firmware memory address, length and host buffer must be aligned on
 *	32-bit boudaries.  The memory is returned as a raw byte sequence from
 *	the firmware's memory.  If this memory contains data structures which
 *	contain multi-byte integers, it's the callers responsibility to
 *	perform appropriate byte order conversions.
 */
int t4_mem_read(struct adapter *adap, int mtype, u32 addr, u32 len,
		__be32 *buf)
{
	u32 pos, start, end, offset;
	int ret;

	/*
	 * Argument sanity checks ...
	 */
	if ((addr & 0x3) || (len & 0x3))
		return -EINVAL;

	/*
	 * The underlaying EDC/MC read routines read 64 bytes at a time so we
	 * need to round down the start and round up the end.  We'll start
	 * copying out of the first line at (addr - start) a word at a time.
	 */
	start = rounddown2(addr, 64);
	end = roundup2(addr + len, 64);
	offset = (addr - start)/sizeof(__be32);

	for (pos = start; pos < end; pos += 64, offset = 0) {
		__be32 data[16];

		/*
		 * Read the chip's memory block and bail if there's an error.
		 */
		if ((mtype == MEM_MC) || (mtype == MEM_MC1))
			ret = t4_mc_read(adap, mtype - MEM_MC, pos, data, NULL);
		else
			ret = t4_edc_read(adap, mtype, pos, data, NULL);
		if (ret)
			return ret;

		/*
		 * Copy the data into the caller's memory buffer.
		 */
		while (offset < 16 && len > 0) {
			*buf++ = data[offset++];
			len -= sizeof(__be32);
		}
	}

	return 0;
}

/*
 * Return the specified PCI-E Configuration Space register from our Physical
 * Function.  We try first via a Firmware LDST Command (if fw_attach != 0)
 * since we prefer to let the firmware own all of these registers, but if that
 * fails we go for it directly ourselves.
 */
u32 t4_read_pcie_cfg4(struct adapter *adap, int reg, int drv_fw_attach)
{

	/*
	 * If fw_attach != 0, construct and send the Firmware LDST Command to
	 * retrieve the specified PCI-E Configuration Space register.
	 */
	if (drv_fw_attach != 0) {
		struct fw_ldst_cmd ldst_cmd;
		int ret;

		memset(&ldst_cmd, 0, sizeof(ldst_cmd));
		ldst_cmd.op_to_addrspace =
			cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
				    F_FW_CMD_REQUEST |
				    F_FW_CMD_READ |
				    V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_FUNC_PCIE));
		ldst_cmd.cycles_to_len16 = cpu_to_be32(FW_LEN16(ldst_cmd));
		ldst_cmd.u.pcie.select_naccess = V_FW_LDST_CMD_NACCESS(1);
		ldst_cmd.u.pcie.ctrl_to_fn =
			(F_FW_LDST_CMD_LC | V_FW_LDST_CMD_FN(adap->pf));
		ldst_cmd.u.pcie.r = reg;

		/*
		 * If the LDST Command succeeds, return the result, otherwise
		 * fall through to reading it directly ourselves ...
		 */
		ret = t4_wr_mbox(adap, adap->mbox, &ldst_cmd, sizeof(ldst_cmd),
				 &ldst_cmd);
		if (ret == 0)
			return be32_to_cpu(ldst_cmd.u.pcie.data[0]);

		CH_WARN(adap, "Firmware failed to return "
			"Configuration Space register %d, err = %d\n",
			reg, -ret);
	}

	/*
	 * Read the desired Configuration Space register via the PCI-E
	 * Backdoor mechanism.
	 */
	return t4_hw_pci_read_cfg4(adap, reg);
}

/**
 *	t4_get_regs_len - return the size of the chips register set
 *	@adapter: the adapter
 *
 *	Returns the size of the chip's BAR0 register space.
 */
unsigned int t4_get_regs_len(struct adapter *adapter)
{
	unsigned int chip_version = chip_id(adapter);

	switch (chip_version) {
	case CHELSIO_T4:
		if (adapter->flags & IS_VF)
			return FW_T4VF_REGMAP_SIZE;
		return T4_REGMAP_SIZE;

	case CHELSIO_T5:
	case CHELSIO_T6:
		if (adapter->flags & IS_VF)
			return FW_T4VF_REGMAP_SIZE;
		return T5_REGMAP_SIZE;
	}

	CH_ERR(adapter,
		"Unsupported chip version %d\n", chip_version);
	return 0;
}

/**
 *	t4_get_regs - read chip registers into provided buffer
 *	@adap: the adapter
 *	@buf: register buffer
 *	@buf_size: size (in bytes) of register buffer
 *
 *	If the provided register buffer isn't large enough for the chip's
 *	full register range, the register dump will be truncated to the
 *	register buffer's size.
 */
void t4_get_regs(struct adapter *adap, u8 *buf, size_t buf_size)
{
	static const unsigned int t4_reg_ranges[] = {
		0x1008, 0x1108,
		0x1180, 0x1184,
		0x1190, 0x1194,
		0x11a0, 0x11a4,
		0x11b0, 0x11b4,
		0x11fc, 0x123c,
		0x1300, 0x173c,
		0x1800, 0x18fc,
		0x3000, 0x30d8,
		0x30e0, 0x30e4,
		0x30ec, 0x5910,
		0x5920, 0x5924,
		0x5960, 0x5960,
		0x5968, 0x5968,
		0x5970, 0x5970,
		0x5978, 0x5978,
		0x5980, 0x5980,
		0x5988, 0x5988,
		0x5990, 0x5990,
		0x5998, 0x5998,
		0x59a0, 0x59d4,
		0x5a00, 0x5ae0,
		0x5ae8, 0x5ae8,
		0x5af0, 0x5af0,
		0x5af8, 0x5af8,
		0x6000, 0x6098,
		0x6100, 0x6150,
		0x6200, 0x6208,
		0x6240, 0x6248,
		0x6280, 0x62b0,
		0x62c0, 0x6338,
		0x6370, 0x638c,
		0x6400, 0x643c,
		0x6500, 0x6524,
		0x6a00, 0x6a04,
		0x6a14, 0x6a38,
		0x6a60, 0x6a70,
		0x6a78, 0x6a78,
		0x6b00, 0x6b0c,
		0x6b1c, 0x6b84,
		0x6bf0, 0x6bf8,
		0x6c00, 0x6c0c,
		0x6c1c, 0x6c84,
		0x6cf0, 0x6cf8,
		0x6d00, 0x6d0c,
		0x6d1c, 0x6d84,
		0x6df0, 0x6df8,
		0x6e00, 0x6e0c,
		0x6e1c, 0x6e84,
		0x6ef0, 0x6ef8,
		0x6f00, 0x6f0c,
		0x6f1c, 0x6f84,
		0x6ff0, 0x6ff8,
		0x7000, 0x700c,
		0x701c, 0x7084,
		0x70f0, 0x70f8,
		0x7100, 0x710c,
		0x711c, 0x7184,
		0x71f0, 0x71f8,
		0x7200, 0x720c,
		0x721c, 0x7284,
		0x72f0, 0x72f8,
		0x7300, 0x730c,
		0x731c, 0x7384,
		0x73f0, 0x73f8,
		0x7400, 0x7450,
		0x7500, 0x7530,
		0x7600, 0x760c,
		0x7614, 0x761c,
		0x7680, 0x76cc,
		0x7700, 0x7798,
		0x77c0, 0x77fc,
		0x7900, 0x79fc,
		0x7b00, 0x7b58,
		0x7b60, 0x7b84,
		0x7b8c, 0x7c38,
		0x7d00, 0x7d38,
		0x7d40, 0x7d80,
		0x7d8c, 0x7ddc,
		0x7de4, 0x7e04,
		0x7e10, 0x7e1c,
		0x7e24, 0x7e38,
		0x7e40, 0x7e44,
		0x7e4c, 0x7e78,
		0x7e80, 0x7ea4,
		0x7eac, 0x7edc,
		0x7ee8, 0x7efc,
		0x8dc0, 0x8e04,
		0x8e10, 0x8e1c,
		0x8e30, 0x8e78,
		0x8ea0, 0x8eb8,
		0x8ec0, 0x8f6c,
		0x8fc0, 0x9008,
		0x9010, 0x9058,
		0x9060, 0x9060,
		0x9068, 0x9074,
		0x90fc, 0x90fc,
		0x9400, 0x9408,
		0x9410, 0x9458,
		0x9600, 0x9600,
		0x9608, 0x9638,
		0x9640, 0x96bc,
		0x9800, 0x9808,
		0x9820, 0x983c,
		0x9850, 0x9864,
		0x9c00, 0x9c6c,
		0x9c80, 0x9cec,
		0x9d00, 0x9d6c,
		0x9d80, 0x9dec,
		0x9e00, 0x9e6c,
		0x9e80, 0x9eec,
		0x9f00, 0x9f6c,
		0x9f80, 0x9fec,
		0xd004, 0xd004,
		0xd010, 0xd03c,
		0xdfc0, 0xdfe0,
		0xe000, 0xea7c,
		0xf000, 0x11190,
		0x19040, 0x1906c,
		0x19078, 0x19080,
		0x1908c, 0x190e4,
		0x190f0, 0x190f8,
		0x19100, 0x19110,
		0x19120, 0x19124,
		0x19150, 0x19194,
		0x1919c, 0x191b0,
		0x191d0, 0x191e8,
		0x19238, 0x1924c,
		0x193f8, 0x1943c,
		0x1944c, 0x19474,
		0x19490, 0x194e0,
		0x194f0, 0x194f8,
		0x19800, 0x19c08,
		0x19c10, 0x19c90,
		0x19ca0, 0x19ce4,
		0x19cf0, 0x19d40,
		0x19d50, 0x19d94,
		0x19da0, 0x19de8,
		0x19df0, 0x19e40,
		0x19e50, 0x19e90,
		0x19ea0, 0x19f4c,
		0x1a000, 0x1a004,
		0x1a010, 0x1a06c,
		0x1a0b0, 0x1a0e4,
		0x1a0ec, 0x1a0f4,
		0x1a100, 0x1a108,
		0x1a114, 0x1a120,
		0x1a128, 0x1a130,
		0x1a138, 0x1a138,
		0x1a190, 0x1a1c4,
		0x1a1fc, 0x1a1fc,
		0x1e040, 0x1e04c,
		0x1e284, 0x1e28c,
		0x1e2c0, 0x1e2c0,
		0x1e2e0, 0x1e2e0,
		0x1e300, 0x1e384,
		0x1e3c0, 0x1e3c8,
		0x1e440, 0x1e44c,
		0x1e684, 0x1e68c,
		0x1e6c0, 0x1e6c0,
		0x1e6e0, 0x1e6e0,
		0x1e700, 0x1e784,
		0x1e7c0, 0x1e7c8,
		0x1e840, 0x1e84c,
		0x1ea84, 0x1ea8c,
		0x1eac0, 0x1eac0,
		0x1eae0, 0x1eae0,
		0x1eb00, 0x1eb84,
		0x1ebc0, 0x1ebc8,
		0x1ec40, 0x1ec4c,
		0x1ee84, 0x1ee8c,
		0x1eec0, 0x1eec0,
		0x1eee0, 0x1eee0,
		0x1ef00, 0x1ef84,
		0x1efc0, 0x1efc8,
		0x1f040, 0x1f04c,
		0x1f284, 0x1f28c,
		0x1f2c0, 0x1f2c0,
		0x1f2e0, 0x1f2e0,
		0x1f300, 0x1f384,
		0x1f3c0, 0x1f3c8,
		0x1f440, 0x1f44c,
		0x1f684, 0x1f68c,
		0x1f6c0, 0x1f6c0,
		0x1f6e0, 0x1f6e0,
		0x1f700, 0x1f784,
		0x1f7c0, 0x1f7c8,
		0x1f840, 0x1f84c,
		0x1fa84, 0x1fa8c,
		0x1fac0, 0x1fac0,
		0x1fae0, 0x1fae0,
		0x1fb00, 0x1fb84,
		0x1fbc0, 0x1fbc8,
		0x1fc40, 0x1fc4c,
		0x1fe84, 0x1fe8c,
		0x1fec0, 0x1fec0,
		0x1fee0, 0x1fee0,
		0x1ff00, 0x1ff84,
		0x1ffc0, 0x1ffc8,
		0x20000, 0x2002c,
		0x20100, 0x2013c,
		0x20190, 0x201a0,
		0x201a8, 0x201b8,
		0x201c4, 0x201c8,
		0x20200, 0x20318,
		0x20400, 0x204b4,
		0x204c0, 0x20528,
		0x20540, 0x20614,
		0x21000, 0x21040,
		0x2104c, 0x21060,
		0x210c0, 0x210ec,
		0x21200, 0x21268,
		0x21270, 0x21284,
		0x212fc, 0x21388,
		0x21400, 0x21404,
		0x21500, 0x21500,
		0x21510, 0x21518,
		0x2152c, 0x21530,
		0x2153c, 0x2153c,
		0x21550, 0x21554,
		0x21600, 0x21600,
		0x21608, 0x2161c,
		0x21624, 0x21628,
		0x21630, 0x21634,
		0x2163c, 0x2163c,
		0x21700, 0x2171c,
		0x21780, 0x2178c,
		0x21800, 0x21818,
		0x21820, 0x21828,
		0x21830, 0x21848,
		0x21850, 0x21854,
		0x21860, 0x21868,
		0x21870, 0x21870,
		0x21878, 0x21898,
		0x218a0, 0x218a8,
		0x218b0, 0x218c8,
		0x218d0, 0x218d4,
		0x218e0, 0x218e8,
		0x218f0, 0x218f0,
		0x218f8, 0x21a18,
		0x21a20, 0x21a28,
		0x21a30, 0x21a48,
		0x21a50, 0x21a54,
		0x21a60, 0x21a68,
		0x21a70, 0x21a70,
		0x21a78, 0x21a98,
		0x21aa0, 0x21aa8,
		0x21ab0, 0x21ac8,
		0x21ad0, 0x21ad4,
		0x21ae0, 0x21ae8,
		0x21af0, 0x21af0,
		0x21af8, 0x21c18,
		0x21c20, 0x21c20,
		0x21c28, 0x21c30,
		0x21c38, 0x21c38,
		0x21c80, 0x21c98,
		0x21ca0, 0x21ca8,
		0x21cb0, 0x21cc8,
		0x21cd0, 0x21cd4,
		0x21ce0, 0x21ce8,
		0x21cf0, 0x21cf0,
		0x21cf8, 0x21d7c,
		0x21e00, 0x21e04,
		0x22000, 0x2202c,
		0x22100, 0x2213c,
		0x22190, 0x221a0,
		0x221a8, 0x221b8,
		0x221c4, 0x221c8,
		0x22200, 0x22318,
		0x22400, 0x224b4,
		0x224c0, 0x22528,
		0x22540, 0x22614,
		0x23000, 0x23040,
		0x2304c, 0x23060,
		0x230c0, 0x230ec,
		0x23200, 0x23268,
		0x23270, 0x23284,
		0x232fc, 0x23388,
		0x23400, 0x23404,
		0x23500, 0x23500,
		0x23510, 0x23518,
		0x2352c, 0x23530,
		0x2353c, 0x2353c,
		0x23550, 0x23554,
		0x23600, 0x23600,
		0x23608, 0x2361c,
		0x23624, 0x23628,
		0x23630, 0x23634,
		0x2363c, 0x2363c,
		0x23700, 0x2371c,
		0x23780, 0x2378c,
		0x23800, 0x23818,
		0x23820, 0x23828,
		0x23830, 0x23848,
		0x23850, 0x23854,
		0x23860, 0x23868,
		0x23870, 0x23870,
		0x23878, 0x23898,
		0x238a0, 0x238a8,
		0x238b0, 0x238c8,
		0x238d0, 0x238d4,
		0x238e0, 0x238e8,
		0x238f0, 0x238f0,
		0x238f8, 0x23a18,
		0x23a20, 0x23a28,
		0x23a30, 0x23a48,
		0x23a50, 0x23a54,
		0x23a60, 0x23a68,
		0x23a70, 0x23a70,
		0x23a78, 0x23a98,
		0x23aa0, 0x23aa8,
		0x23ab0, 0x23ac8,
		0x23ad0, 0x23ad4,
		0x23ae0, 0x23ae8,
		0x23af0, 0x23af0,
		0x23af8, 0x23c18,
		0x23c20, 0x23c20,
		0x23c28, 0x23c30,
		0x23c38, 0x23c38,
		0x23c80, 0x23c98,
		0x23ca0, 0x23ca8,
		0x23cb0, 0x23cc8,
		0x23cd0, 0x23cd4,
		0x23ce0, 0x23ce8,
		0x23cf0, 0x23cf0,
		0x23cf8, 0x23d7c,
		0x23e00, 0x23e04,
		0x24000, 0x2402c,
		0x24100, 0x2413c,
		0x24190, 0x241a0,
		0x241a8, 0x241b8,
		0x241c4, 0x241c8,
		0x24200, 0x24318,
		0x24400, 0x244b4,
		0x244c0, 0x24528,
		0x24540, 0x24614,
		0x25000, 0x25040,
		0x2504c, 0x25060,
		0x250c0, 0x250ec,
		0x25200, 0x25268,
		0x25270, 0x25284,
		0x252fc, 0x25388,
		0x25400, 0x25404,
		0x25500, 0x25500,
		0x25510, 0x25518,
		0x2552c, 0x25530,
		0x2553c, 0x2553c,
		0x25550, 0x25554,
		0x25600, 0x25600,
		0x25608, 0x2561c,
		0x25624, 0x25628,
		0x25630, 0x25634,
		0x2563c, 0x2563c,
		0x25700, 0x2571c,
		0x25780, 0x2578c,
		0x25800, 0x25818,
		0x25820, 0x25828,
		0x25830, 0x25848,
		0x25850, 0x25854,
		0x25860, 0x25868,
		0x25870, 0x25870,
		0x25878, 0x25898,
		0x258a0, 0x258a8,
		0x258b0, 0x258c8,
		0x258d0, 0x258d4,
		0x258e0, 0x258e8,
		0x258f0, 0x258f0,
		0x258f8, 0x25a18,
		0x25a20, 0x25a28,
		0x25a30, 0x25a48,
		0x25a50, 0x25a54,
		0x25a60, 0x25a68,
		0x25a70, 0x25a70,
		0x25a78, 0x25a98,
		0x25aa0, 0x25aa8,
		0x25ab0, 0x25ac8,
		0x25ad0, 0x25ad4,
		0x25ae0, 0x25ae8,
		0x25af0, 0x25af0,
		0x25af8, 0x25c18,
		0x25c20, 0x25c20,
		0x25c28, 0x25c30,
		0x25c38, 0x25c38,
		0x25c80, 0x25c98,
		0x25ca0, 0x25ca8,
		0x25cb0, 0x25cc8,
		0x25cd0, 0x25cd4,
		0x25ce0, 0x25ce8,
		0x25cf0, 0x25cf0,
		0x25cf8, 0x25d7c,
		0x25e00, 0x25e04,
		0x26000, 0x2602c,
		0x26100, 0x2613c,
		0x26190, 0x261a0,
		0x261a8, 0x261b8,
		0x261c4, 0x261c8,
		0x26200, 0x26318,
		0x26400, 0x264b4,
		0x264c0, 0x26528,
		0x26540, 0x26614,
		0x27000, 0x27040,
		0x2704c, 0x27060,
		0x270c0, 0x270ec,
		0x27200, 0x27268,
		0x27270, 0x27284,
		0x272fc, 0x27388,
		0x27400, 0x27404,
		0x27500, 0x27500,
		0x27510, 0x27518,
		0x2752c, 0x27530,
		0x2753c, 0x2753c,
		0x27550, 0x27554,
		0x27600, 0x27600,
		0x27608, 0x2761c,
		0x27624, 0x27628,
		0x27630, 0x27634,
		0x2763c, 0x2763c,
		0x27700, 0x2771c,
		0x27780, 0x2778c,
		0x27800, 0x27818,
		0x27820, 0x27828,
		0x27830, 0x27848,
		0x27850, 0x27854,
		0x27860, 0x27868,
		0x27870, 0x27870,
		0x27878, 0x27898,
		0x278a0, 0x278a8,
		0x278b0, 0x278c8,
		0x278d0, 0x278d4,
		0x278e0, 0x278e8,
		0x278f0, 0x278f0,
		0x278f8, 0x27a18,
		0x27a20, 0x27a28,
		0x27a30, 0x27a48,
		0x27a50, 0x27a54,
		0x27a60, 0x27a68,
		0x27a70, 0x27a70,
		0x27a78, 0x27a98,
		0x27aa0, 0x27aa8,
		0x27ab0, 0x27ac8,
		0x27ad0, 0x27ad4,
		0x27ae0, 0x27ae8,
		0x27af0, 0x27af0,
		0x27af8, 0x27c18,
		0x27c20, 0x27c20,
		0x27c28, 0x27c30,
		0x27c38, 0x27c38,
		0x27c80, 0x27c98,
		0x27ca0, 0x27ca8,
		0x27cb0, 0x27cc8,
		0x27cd0, 0x27cd4,
		0x27ce0, 0x27ce8,
		0x27cf0, 0x27cf0,
		0x27cf8, 0x27d7c,
		0x27e00, 0x27e04,
	};

	static const unsigned int t4vf_reg_ranges[] = {
		VF_SGE_REG(A_SGE_VF_KDOORBELL), VF_SGE_REG(A_SGE_VF_GTS),
		VF_MPS_REG(A_MPS_VF_CTL),
		VF_MPS_REG(A_MPS_VF_STAT_RX_VF_ERR_FRAMES_H),
		VF_PL_REG(A_PL_VF_WHOAMI), VF_PL_REG(A_PL_VF_WHOAMI),
		VF_CIM_REG(A_CIM_VF_EXT_MAILBOX_CTRL),
		VF_CIM_REG(A_CIM_VF_EXT_MAILBOX_STATUS),
		FW_T4VF_MBDATA_BASE_ADDR,
		FW_T4VF_MBDATA_BASE_ADDR +
		((NUM_CIM_PF_MAILBOX_DATA_INSTANCES - 1) * 4),
	};

	static const unsigned int t5_reg_ranges[] = {
		0x1008, 0x10c0,
		0x10cc, 0x10f8,
		0x1100, 0x1100,
		0x110c, 0x1148,
		0x1180, 0x1184,
		0x1190, 0x1194,
		0x11a0, 0x11a4,
		0x11b0, 0x11b4,
		0x11fc, 0x123c,
		0x1280, 0x173c,
		0x1800, 0x18fc,
		0x3000, 0x3028,
		0x3060, 0x30b0,
		0x30b8, 0x30d8,
		0x30e0, 0x30fc,
		0x3140, 0x357c,
		0x35a8, 0x35cc,
		0x35ec, 0x35ec,
		0x3600, 0x5624,
		0x56cc, 0x56ec,
		0x56f4, 0x5720,
		0x5728, 0x575c,
		0x580c, 0x5814,
		0x5890, 0x589c,
		0x58a4, 0x58ac,
		0x58b8, 0x58bc,
		0x5940, 0x59c8,
		0x59d0, 0x59dc,
		0x59fc, 0x5a18,
		0x5a60, 0x5a70,
		0x5a80, 0x5a9c,
		0x5b94, 0x5bfc,
		0x6000, 0x6020,
		0x6028, 0x6040,
		0x6058, 0x609c,
		0x60a8, 0x614c,
		0x7700, 0x7798,
		0x77c0, 0x78fc,
		0x7b00, 0x7b58,
		0x7b60, 0x7b84,
		0x7b8c, 0x7c54,
		0x7d00, 0x7d38,
		0x7d40, 0x7d80,
		0x7d8c, 0x7ddc,
		0x7de4, 0x7e04,
		0x7e10, 0x7e1c,
		0x7e24, 0x7e38,
		0x7e40, 0x7e44,
		0x7e4c, 0x7e78,
		0x7e80, 0x7edc,
		0x7ee8, 0x7efc,
		0x8dc0, 0x8de0,
		0x8df8, 0x8e04,
		0x8e10, 0x8e84,
		0x8ea0, 0x8f84,
		0x8fc0, 0x9058,
		0x9060, 0x9060,
		0x9068, 0x90f8,
		0x9400, 0x9408,
		0x9410, 0x9470,
		0x9600, 0x9600,
		0x9608, 0x9638,
		0x9640, 0x96f4,
		0x9800, 0x9808,
		0x9820, 0x983c,
		0x9850, 0x9864,
		0x9c00, 0x9c6c,
		0x9c80, 0x9cec,
		0x9d00, 0x9d6c,
		0x9d80, 0x9dec,
		0x9e00, 0x9e6c,
		0x9e80, 0x9eec,
		0x9f00, 0x9f6c,
		0x9f80, 0xa020,
		0xd004, 0xd004,
		0xd010, 0xd03c,
		0xdfc0, 0xdfe0,
		0xe000, 0x1106c,
		0x11074, 0x11088,
		0x1109c, 0x1117c,
		0x11190, 0x11204,
		0x19040, 0x1906c,
		0x19078, 0x19080,
		0x1908c, 0x190e8,
		0x190f0, 0x190f8,
		0x19100, 0x19110,
		0x19120, 0x19124,
		0x19150, 0x19194,
		0x1919c, 0x191b0,
		0x191d0, 0x191e8,
		0x19238, 0x19290,
		0x193f8, 0x19428,
		0x19430, 0x19444,
		0x1944c, 0x1946c,
		0x19474, 0x19474,
		0x19490, 0x194cc,
		0x194f0, 0x194f8,
		0x19c00, 0x19c08,
		0x19c10, 0x19c60,
		0x19c94, 0x19ce4,
		0x19cf0, 0x19d40,
		0x19d50, 0x19d94,
		0x19da0, 0x19de8,
		0x19df0, 0x19e10,
		0x19e50, 0x19e90,
		0x19ea0, 0x19f24,
		0x19f34, 0x19f34,
		0x19f40, 0x19f50,
		0x19f90, 0x19fb4,
		0x19fc4, 0x19fe4,
		0x1a000, 0x1a004,
		0x1a010, 0x1a06c,
		0x1a0b0, 0x1a0e4,
		0x1a0ec, 0x1a0f8,
		0x1a100, 0x1a108,
		0x1a114, 0x1a120,
		0x1a128, 0x1a130,
		0x1a138, 0x1a138,
		0x1a190, 0x1a1c4,
		0x1a1fc, 0x1a1fc,
		0x1e008, 0x1e00c,
		0x1e040, 0x1e044,
		0x1e04c, 0x1e04c,
		0x1e284, 0x1e290,
		0x1e2c0, 0x1e2c0,
		0x1e2e0, 0x1e2e0,
		0x1e300, 0x1e384,
		0x1e3c0, 0x1e3c8,
		0x1e408, 0x1e40c,
		0x1e440, 0x1e444,
		0x1e44c, 0x1e44c,
		0x1e684, 0x1e690,
		0x1e6c0, 0x1e6c0,
		0x1e6e0, 0x1e6e0,
		0x1e700, 0x1e784,
		0x1e7c0, 0x1e7c8,
		0x1e808, 0x1e80c,
		0x1e840, 0x1e844,
		0x1e84c, 0x1e84c,
		0x1ea84, 0x1ea90,
		0x1eac0, 0x1eac0,
		0x1eae0, 0x1eae0,
		0x1eb00, 0x1eb84,
		0x1ebc0, 0x1ebc8,
		0x1ec08, 0x1ec0c,
		0x1ec40, 0x1ec44,
		0x1ec4c, 0x1ec4c,
		0x1ee84, 0x1ee90,
		0x1eec0, 0x1eec0,
		0x1eee0, 0x1eee0,
		0x1ef00, 0x1ef84,
		0x1efc0, 0x1efc8,
		0x1f008, 0x1f00c,
		0x1f040, 0x1f044,
		0x1f04c, 0x1f04c,
		0x1f284, 0x1f290,
		0x1f2c0, 0x1f2c0,
		0x1f2e0, 0x1f2e0,
		0x1f300, 0x1f384,
		0x1f3c0, 0x1f3c8,
		0x1f408, 0x1f40c,
		0x1f440, 0x1f444,
		0x1f44c, 0x1f44c,
		0x1f684, 0x1f690,
		0x1f6c0, 0x1f6c0,
		0x1f6e0, 0x1f6e0,
		0x1f700, 0x1f784,
		0x1f7c0, 0x1f7c8,
		0x1f808, 0x1f80c,
		0x1f840, 0x1f844,
		0x1f84c, 0x1f84c,
		0x1fa84, 0x1fa90,
		0x1fac0, 0x1fac0,
		0x1fae0, 0x1fae0,
		0x1fb00, 0x1fb84,
		0x1fbc0, 0x1fbc8,
		0x1fc08, 0x1fc0c,
		0x1fc40, 0x1fc44,
		0x1fc4c, 0x1fc4c,
		0x1fe84, 0x1fe90,
		0x1fec0, 0x1fec0,
		0x1fee0, 0x1fee0,
		0x1ff00, 0x1ff84,
		0x1ffc0, 0x1ffc8,
		0x30000, 0x30030,
		0x30038, 0x30038,
		0x30040, 0x30040,
		0x30100, 0x30144,
		0x30190, 0x301a0,
		0x301a8, 0x301b8,
		0x301c4, 0x301c8,
		0x301d0, 0x301d0,
		0x30200, 0x30318,
		0x30400, 0x304b4,
		0x304c0, 0x3052c,
		0x30540, 0x3061c,
		0x30800, 0x30828,
		0x30834, 0x30834,
		0x308c0, 0x30908,
		0x30910, 0x309ac,
		0x30a00, 0x30a14,
		0x30a1c, 0x30a2c,
		0x30a44, 0x30a50,
		0x30a74, 0x30a74,
		0x30a7c, 0x30afc,
		0x30b08, 0x30c24,
		0x30d00, 0x30d00,
		0x30d08, 0x30d14,
		0x30d1c, 0x30d20,
		0x30d3c, 0x30d3c,
		0x30d48, 0x30d50,
		0x31200, 0x3120c,
		0x31220, 0x31220,
		0x31240, 0x31240,
		0x31600, 0x3160c,
		0x31a00, 0x31a1c,
		0x31e00, 0x31e20,
		0x31e38, 0x31e3c,
		0x31e80, 0x31e80,
		0x31e88, 0x31ea8,
		0x31eb0, 0x31eb4,
		0x31ec8, 0x31ed4,
		0x31fb8, 0x32004,
		0x32200, 0x32200,
		0x32208, 0x32240,
		0x32248, 0x32280,
		0x32288, 0x322c0,
		0x322c8, 0x322fc,
		0x32600, 0x32630,
		0x32a00, 0x32abc,
		0x32b00, 0x32b10,
		0x32b20, 0x32b30,
		0x32b40, 0x32b50,
		0x32b60, 0x32b70,
		0x33000, 0x33028,
		0x33030, 0x33048,
		0x33060, 0x33068,
		0x33070, 0x3309c,
		0x330f0, 0x33128,
		0x33130, 0x33148,
		0x33160, 0x33168,
		0x33170, 0x3319c,
		0x331f0, 0x33238,
		0x33240, 0x33240,
		0x33248, 0x33250,
		0x3325c, 0x33264,
		0x33270, 0x332b8,
		0x332c0, 0x332e4,
		0x332f8, 0x33338,
		0x33340, 0x33340,
		0x33348, 0x33350,
		0x3335c, 0x33364,
		0x33370, 0x333b8,
		0x333c0, 0x333e4,
		0x333f8, 0x33428,
		0x33430, 0x33448,
		0x33460, 0x33468,
		0x33470, 0x3349c,
		0x334f0, 0x33528,
		0x33530, 0x33548,
		0x33560, 0x33568,
		0x33570, 0x3359c,
		0x335f0, 0x33638,
		0x33640, 0x33640,
		0x33648, 0x33650,
		0x3365c, 0x33664,
		0x33670, 0x336b8,
		0x336c0, 0x336e4,
		0x336f8, 0x33738,
		0x33740, 0x33740,
		0x33748, 0x33750,
		0x3375c, 0x33764,
		0x33770, 0x337b8,
		0x337c0, 0x337e4,
		0x337f8, 0x337fc,
		0x33814, 0x33814,
		0x3382c, 0x3382c,
		0x33880, 0x3388c,
		0x338e8, 0x338ec,
		0x33900, 0x33928,
		0x33930, 0x33948,
		0x33960, 0x33968,
		0x33970, 0x3399c,
		0x339f0, 0x33a38,
		0x33a40, 0x33a40,
		0x33a48, 0x33a50,
		0x33a5c, 0x33a64,
		0x33a70, 0x33ab8,
		0x33ac0, 0x33ae4,
		0x33af8, 0x33b10,
		0x33b28, 0x33b28,
		0x33b3c, 0x33b50,
		0x33bf0, 0x33c10,
		0x33c28, 0x33c28,
		0x33c3c, 0x33c50,
		0x33cf0, 0x33cfc,
		0x34000, 0x34030,
		0x34038, 0x34038,
		0x34040, 0x34040,
		0x34100, 0x34144,
		0x34190, 0x341a0,
		0x341a8, 0x341b8,
		0x341c4, 0x341c8,
		0x341d0, 0x341d0,
		0x34200, 0x34318,
		0x34400, 0x344b4,
		0x344c0, 0x3452c,
		0x34540, 0x3461c,
		0x34800, 0x34828,
		0x34834, 0x34834,
		0x348c0, 0x34908,
		0x34910, 0x349ac,
		0x34a00, 0x34a14,
		0x34a1c, 0x34a2c,
		0x34a44, 0x34a50,
		0x34a74, 0x34a74,
		0x34a7c, 0x34afc,
		0x34b08, 0x34c24,
		0x34d00, 0x34d00,
		0x34d08, 0x34d14,
		0x34d1c, 0x34d20,
		0x34d3c, 0x34d3c,
		0x34d48, 0x34d50,
		0x35200, 0x3520c,
		0x35220, 0x35220,
		0x35240, 0x35240,
		0x35600, 0x3560c,
		0x35a00, 0x35a1c,
		0x35e00, 0x35e20,
		0x35e38, 0x35e3c,
		0x35e80, 0x35e80,
		0x35e88, 0x35ea8,
		0x35eb0, 0x35eb4,
		0x35ec8, 0x35ed4,
		0x35fb8, 0x36004,
		0x36200, 0x36200,
		0x36208, 0x36240,
		0x36248, 0x36280,
		0x36288, 0x362c0,
		0x362c8, 0x362fc,
		0x36600, 0x36630,
		0x36a00, 0x36abc,
		0x36b00, 0x36b10,
		0x36b20, 0x36b30,
		0x36b40, 0x36b50,
		0x36b60, 0x36b70,
		0x37000, 0x37028,
		0x37030, 0x37048,
		0x37060, 0x37068,
		0x37070, 0x3709c,
		0x370f0, 0x37128,
		0x37130, 0x37148,
		0x37160, 0x37168,
		0x37170, 0x3719c,
		0x371f0, 0x37238,
		0x37240, 0x37240,
		0x37248, 0x37250,
		0x3725c, 0x37264,
		0x37270, 0x372b8,
		0x372c0, 0x372e4,
		0x372f8, 0x37338,
		0x37340, 0x37340,
		0x37348, 0x37350,
		0x3735c, 0x37364,
		0x37370, 0x373b8,
		0x373c0, 0x373e4,
		0x373f8, 0x37428,
		0x37430, 0x37448,
		0x37460, 0x37468,
		0x37470, 0x3749c,
		0x374f0, 0x37528,
		0x37530, 0x37548,
		0x37560, 0x37568,
		0x37570, 0x3759c,
		0x375f0, 0x37638,
		0x37640, 0x37640,
		0x37648, 0x37650,
		0x3765c, 0x37664,
		0x37670, 0x376b8,
		0x376c0, 0x376e4,
		0x376f8, 0x37738,
		0x37740, 0x37740,
		0x37748, 0x37750,
		0x3775c, 0x37764,
		0x37770, 0x377b8,
		0x377c0, 0x377e4,
		0x377f8, 0x377fc,
		0x37814, 0x37814,
		0x3782c, 0x3782c,
		0x37880, 0x3788c,
		0x378e8, 0x378ec,
		0x37900, 0x37928,
		0x37930, 0x37948,
		0x37960, 0x37968,
		0x37970, 0x3799c,
		0x379f0, 0x37a38,
		0x37a40, 0x37a40,
		0x37a48, 0x37a50,
		0x37a5c, 0x37a64,
		0x37a70, 0x37ab8,
		0x37ac0, 0x37ae4,
		0x37af8, 0x37b10,
		0x37b28, 0x37b28,
		0x37b3c, 0x37b50,
		0x37bf0, 0x37c10,
		0x37c28, 0x37c28,
		0x37c3c, 0x37c50,
		0x37cf0, 0x37cfc,
		0x38000, 0x38030,
		0x38038, 0x38038,
		0x38040, 0x38040,
		0x38100, 0x38144,
		0x38190, 0x381a0,
		0x381a8, 0x381b8,
		0x381c4, 0x381c8,
		0x381d0, 0x381d0,
		0x38200, 0x38318,
		0x38400, 0x384b4,
		0x384c0, 0x3852c,
		0x38540, 0x3861c,
		0x38800, 0x38828,
		0x38834, 0x38834,
		0x388c0, 0x38908,
		0x38910, 0x389ac,
		0x38a00, 0x38a14,
		0x38a1c, 0x38a2c,
		0x38a44, 0x38a50,
		0x38a74, 0x38a74,
		0x38a7c, 0x38afc,
		0x38b08, 0x38c24,
		0x38d00, 0x38d00,
		0x38d08, 0x38d14,
		0x38d1c, 0x38d20,
		0x38d3c, 0x38d3c,
		0x38d48, 0x38d50,
		0x39200, 0x3920c,
		0x39220, 0x39220,
		0x39240, 0x39240,
		0x39600, 0x3960c,
		0x39a00, 0x39a1c,
		0x39e00, 0x39e20,
		0x39e38, 0x39e3c,
		0x39e80, 0x39e80,
		0x39e88, 0x39ea8,
		0x39eb0, 0x39eb4,
		0x39ec8, 0x39ed4,
		0x39fb8, 0x3a004,
		0x3a200, 0x3a200,
		0x3a208, 0x3a240,
		0x3a248, 0x3a280,
		0x3a288, 0x3a2c0,
		0x3a2c8, 0x3a2fc,
		0x3a600, 0x3a630,
		0x3aa00, 0x3aabc,
		0x3ab00, 0x3ab10,
		0x3ab20, 0x3ab30,
		0x3ab40, 0x3ab50,
		0x3ab60, 0x3ab70,
		0x3b000, 0x3b028,
		0x3b030, 0x3b048,
		0x3b060, 0x3b068,
		0x3b070, 0x3b09c,
		0x3b0f0, 0x3b128,
		0x3b130, 0x3b148,
		0x3b160, 0x3b168,
		0x3b170, 0x3b19c,
		0x3b1f0, 0x3b238,
		0x3b240, 0x3b240,
		0x3b248, 0x3b250,
		0x3b25c, 0x3b264,
		0x3b270, 0x3b2b8,
		0x3b2c0, 0x3b2e4,
		0x3b2f8, 0x3b338,
		0x3b340, 0x3b340,
		0x3b348, 0x3b350,
		0x3b35c, 0x3b364,
		0x3b370, 0x3b3b8,
		0x3b3c0, 0x3b3e4,
		0x3b3f8, 0x3b428,
		0x3b430, 0x3b448,
		0x3b460, 0x3b468,
		0x3b470, 0x3b49c,
		0x3b4f0, 0x3b528,
		0x3b530, 0x3b548,
		0x3b560, 0x3b568,
		0x3b570, 0x3b59c,
		0x3b5f0, 0x3b638,
		0x3b640, 0x3b640,
		0x3b648, 0x3b650,
		0x3b65c, 0x3b664,
		0x3b670, 0x3b6b8,
		0x3b6c0, 0x3b6e4,
		0x3b6f8, 0x3b738,
		0x3b740, 0x3b740,
		0x3b748, 0x3b750,
		0x3b75c, 0x3b764,
		0x3b770, 0x3b7b8,
		0x3b7c0, 0x3b7e4,
		0x3b7f8, 0x3b7fc,
		0x3b814, 0x3b814,
		0x3b82c, 0x3b82c,
		0x3b880, 0x3b88c,
		0x3b8e8, 0x3b8ec,
		0x3b900, 0x3b928,
		0x3b930, 0x3b948,
		0x3b960, 0x3b968,
		0x3b970, 0x3b99c,
		0x3b9f0, 0x3ba38,
		0x3ba40, 0x3ba40,
		0x3ba48, 0x3ba50,
		0x3ba5c, 0x3ba64,
		0x3ba70, 0x3bab8,
		0x3bac0, 0x3bae4,
		0x3baf8, 0x3bb10,
		0x3bb28, 0x3bb28,
		0x3bb3c, 0x3bb50,
		0x3bbf0, 0x3bc10,
		0x3bc28, 0x3bc28,
		0x3bc3c, 0x3bc50,
		0x3bcf0, 0x3bcfc,
		0x3c000, 0x3c030,
		0x3c038, 0x3c038,
		0x3c040, 0x3c040,
		0x3c100, 0x3c144,
		0x3c190, 0x3c1a0,
		0x3c1a8, 0x3c1b8,
		0x3c1c4, 0x3c1c8,
		0x3c1d0, 0x3c1d0,
		0x3c200, 0x3c318,
		0x3c400, 0x3c4b4,
		0x3c4c0, 0x3c52c,
		0x3c540, 0x3c61c,
		0x3c800, 0x3c828,
		0x3c834, 0x3c834,
		0x3c8c0, 0x3c908,
		0x3c910, 0x3c9ac,
		0x3ca00, 0x3ca14,
		0x3ca1c, 0x3ca2c,
		0x3ca44, 0x3ca50,
		0x3ca74, 0x3ca74,
		0x3ca7c, 0x3cafc,
		0x3cb08, 0x3cc24,
		0x3cd00, 0x3cd00,
		0x3cd08, 0x3cd14,
		0x3cd1c, 0x3cd20,
		0x3cd3c, 0x3cd3c,
		0x3cd48, 0x3cd50,
		0x3d200, 0x3d20c,
		0x3d220, 0x3d220,
		0x3d240, 0x3d240,
		0x3d600, 0x3d60c,
		0x3da00, 0x3da1c,
		0x3de00, 0x3de20,
		0x3de38, 0x3de3c,
		0x3de80, 0x3de80,
		0x3de88, 0x3dea8,
		0x3deb0, 0x3deb4,
		0x3dec8, 0x3ded4,
		0x3dfb8, 0x3e004,
		0x3e200, 0x3e200,
		0x3e208, 0x3e240,
		0x3e248, 0x3e280,
		0x3e288, 0x3e2c0,
		0x3e2c8, 0x3e2fc,
		0x3e600, 0x3e630,
		0x3ea00, 0x3eabc,
		0x3eb00, 0x3eb10,
		0x3eb20, 0x3eb30,
		0x3eb40, 0x3eb50,
		0x3eb60, 0x3eb70,
		0x3f000, 0x3f028,
		0x3f030, 0x3f048,
		0x3f060, 0x3f068,
		0x3f070, 0x3f09c,
		0x3f0f0, 0x3f128,
		0x3f130, 0x3f148,
		0x3f160, 0x3f168,
		0x3f170, 0x3f19c,
		0x3f1f0, 0x3f238,
		0x3f240, 0x3f240,
		0x3f248, 0x3f250,
		0x3f25c, 0x3f264,
		0x3f270, 0x3f2b8,
		0x3f2c0, 0x3f2e4,
		0x3f2f8, 0x3f338,
		0x3f340, 0x3f340,
		0x3f348, 0x3f350,
		0x3f35c, 0x3f364,
		0x3f370, 0x3f3b8,
		0x3f3c0, 0x3f3e4,
		0x3f3f8, 0x3f428,
		0x3f430, 0x3f448,
		0x3f460, 0x3f468,
		0x3f470, 0x3f49c,
		0x3f4f0, 0x3f528,
		0x3f530, 0x3f548,
		0x3f560, 0x3f568,
		0x3f570, 0x3f59c,
		0x3f5f0, 0x3f638,
		0x3f640, 0x3f640,
		0x3f648, 0x3f650,
		0x3f65c, 0x3f664,
		0x3f670, 0x3f6b8,
		0x3f6c0, 0x3f6e4,
		0x3f6f8, 0x3f738,
		0x3f740, 0x3f740,
		0x3f748, 0x3f750,
		0x3f75c, 0x3f764,
		0x3f770, 0x3f7b8,
		0x3f7c0, 0x3f7e4,
		0x3f7f8, 0x3f7fc,
		0x3f814, 0x3f814,
		0x3f82c, 0x3f82c,
		0x3f880, 0x3f88c,
		0x3f8e8, 0x3f8ec,
		0x3f900, 0x3f928,
		0x3f930, 0x3f948,
		0x3f960, 0x3f968,
		0x3f970, 0x3f99c,
		0x3f9f0, 0x3fa38,
		0x3fa40, 0x3fa40,
		0x3fa48, 0x3fa50,
		0x3fa5c, 0x3fa64,
		0x3fa70, 0x3fab8,
		0x3fac0, 0x3fae4,
		0x3faf8, 0x3fb10,
		0x3fb28, 0x3fb28,
		0x3fb3c, 0x3fb50,
		0x3fbf0, 0x3fc10,
		0x3fc28, 0x3fc28,
		0x3fc3c, 0x3fc50,
		0x3fcf0, 0x3fcfc,
		0x40000, 0x4000c,
		0x40040, 0x40050,
		0x40060, 0x40068,
		0x4007c, 0x4008c,
		0x40094, 0x400b0,
		0x400c0, 0x40144,
		0x40180, 0x4018c,
		0x40200, 0x40254,
		0x40260, 0x40264,
		0x40270, 0x40288,
		0x40290, 0x40298,
		0x402ac, 0x402c8,
		0x402d0, 0x402e0,
		0x402f0, 0x402f0,
		0x40300, 0x4033c,
		0x403f8, 0x403fc,
		0x41304, 0x413c4,
		0x41400, 0x4140c,
		0x41414, 0x4141c,
		0x41480, 0x414d0,
		0x44000, 0x44054,
		0x4405c, 0x44078,
		0x440c0, 0x44174,
		0x44180, 0x441ac,
		0x441b4, 0x441b8,
		0x441c0, 0x44254,
		0x4425c, 0x44278,
		0x442c0, 0x44374,
		0x44380, 0x443ac,
		0x443b4, 0x443b8,
		0x443c0, 0x44454,
		0x4445c, 0x44478,
		0x444c0, 0x44574,
		0x44580, 0x445ac,
		0x445b4, 0x445b8,
		0x445c0, 0x44654,
		0x4465c, 0x44678,
		0x446c0, 0x44774,
		0x44780, 0x447ac,
		0x447b4, 0x447b8,
		0x447c0, 0x44854,
		0x4485c, 0x44878,
		0x448c0, 0x44974,
		0x44980, 0x449ac,
		0x449b4, 0x449b8,
		0x449c0, 0x449fc,
		0x45000, 0x45004,
		0x45010, 0x45030,
		0x45040, 0x45060,
		0x45068, 0x45068,
		0x45080, 0x45084,
		0x450a0, 0x450b0,
		0x45200, 0x45204,
		0x45210, 0x45230,
		0x45240, 0x45260,
		0x45268, 0x45268,
		0x45280, 0x45284,
		0x452a0, 0x452b0,
		0x460c0, 0x460e4,
		0x47000, 0x4703c,
		0x47044, 0x4708c,
		0x47200, 0x47250,
		0x47400, 0x47408,
		0x47414, 0x47420,
		0x47600, 0x47618,
		0x47800, 0x47814,
		0x48000, 0x4800c,
		0x48040, 0x48050,
		0x48060, 0x48068,
		0x4807c, 0x4808c,
		0x48094, 0x480b0,
		0x480c0, 0x48144,
		0x48180, 0x4818c,
		0x48200, 0x48254,
		0x48260, 0x48264,
		0x48270, 0x48288,
		0x48290, 0x48298,
		0x482ac, 0x482c8,
		0x482d0, 0x482e0,
		0x482f0, 0x482f0,
		0x48300, 0x4833c,
		0x483f8, 0x483fc,
		0x49304, 0x493c4,
		0x49400, 0x4940c,
		0x49414, 0x4941c,
		0x49480, 0x494d0,
		0x4c000, 0x4c054,
		0x4c05c, 0x4c078,
		0x4c0c0, 0x4c174,
		0x4c180, 0x4c1ac,
		0x4c1b4, 0x4c1b8,
		0x4c1c0, 0x4c254,
		0x4c25c, 0x4c278,
		0x4c2c0, 0x4c374,
		0x4c380, 0x4c3ac,
		0x4c3b4, 0x4c3b8,
		0x4c3c0, 0x4c454,
		0x4c45c, 0x4c478,
		0x4c4c0, 0x4c574,
		0x4c580, 0x4c5ac,
		0x4c5b4, 0x4c5b8,
		0x4c5c0, 0x4c654,
		0x4c65c, 0x4c678,
		0x4c6c0, 0x4c774,
		0x4c780, 0x4c7ac,
		0x4c7b4, 0x4c7b8,
		0x4c7c0, 0x4c854,
		0x4c85c, 0x4c878,
		0x4c8c0, 0x4c974,
		0x4c980, 0x4c9ac,
		0x4c9b4, 0x4c9b8,
		0x4c9c0, 0x4c9fc,
		0x4d000, 0x4d004,
		0x4d010, 0x4d030,
		0x4d040, 0x4d060,
		0x4d068, 0x4d068,
		0x4d080, 0x4d084,
		0x4d0a0, 0x4d0b0,
		0x4d200, 0x4d204,
		0x4d210, 0x4d230,
		0x4d240, 0x4d260,
		0x4d268, 0x4d268,
		0x4d280, 0x4d284,
		0x4d2a0, 0x4d2b0,
		0x4e0c0, 0x4e0e4,
		0x4f000, 0x4f03c,
		0x4f044, 0x4f08c,
		0x4f200, 0x4f250,
		0x4f400, 0x4f408,
		0x4f414, 0x4f420,
		0x4f600, 0x4f618,
		0x4f800, 0x4f814,
		0x50000, 0x50084,
		0x50090, 0x500cc,
		0x50400, 0x50400,
		0x50800, 0x50884,
		0x50890, 0x508cc,
		0x50c00, 0x50c00,
		0x51000, 0x5101c,
		0x51300, 0x51308,
	};

	static const unsigned int t5vf_reg_ranges[] = {
		VF_SGE_REG(A_SGE_VF_KDOORBELL), VF_SGE_REG(A_SGE_VF_GTS),
		VF_MPS_REG(A_MPS_VF_CTL),
		VF_MPS_REG(A_MPS_VF_STAT_RX_VF_ERR_FRAMES_H),
		VF_PL_REG(A_PL_VF_WHOAMI), VF_PL_REG(A_PL_VF_REVISION),
		VF_CIM_REG(A_CIM_VF_EXT_MAILBOX_CTRL),
		VF_CIM_REG(A_CIM_VF_EXT_MAILBOX_STATUS),
		FW_T4VF_MBDATA_BASE_ADDR,
		FW_T4VF_MBDATA_BASE_ADDR +
		((NUM_CIM_PF_MAILBOX_DATA_INSTANCES - 1) * 4),
	};

	static const unsigned int t6_reg_ranges[] = {
		0x1008, 0x101c,
		0x1024, 0x10a8,
		0x10b4, 0x10f8,
		0x1100, 0x1114,
		0x111c, 0x112c,
		0x1138, 0x113c,
		0x1144, 0x114c,
		0x1180, 0x1184,
		0x1190, 0x1194,
		0x11a0, 0x11a4,
		0x11b0, 0x11b4,
		0x11fc, 0x1274,
		0x1280, 0x133c,
		0x1800, 0x18fc,
		0x3000, 0x302c,
		0x3060, 0x30b0,
		0x30b8, 0x30d8,
		0x30e0, 0x30fc,
		0x3140, 0x357c,
		0x35a8, 0x35cc,
		0x35ec, 0x35ec,
		0x3600, 0x5624,
		0x56cc, 0x56ec,
		0x56f4, 0x5720,
		0x5728, 0x575c,
		0x580c, 0x5814,
		0x5890, 0x589c,
		0x58a4, 0x58ac,
		0x58b8, 0x58bc,
		0x5940, 0x595c,
		0x5980, 0x598c,
		0x59b0, 0x59c8,
		0x59d0, 0x59dc,
		0x59fc, 0x5a18,
		0x5a60, 0x5a6c,
		0x5a80, 0x5a8c,
		0x5a94, 0x5a9c,
		0x5b94, 0x5bfc,
		0x5c10, 0x5e48,
		0x5e50, 0x5e94,
		0x5ea0, 0x5eb0,
		0x5ec0, 0x5ec0,
		0x5ec8, 0x5ed0,
		0x5ee0, 0x5ee0,
		0x5ef0, 0x5ef0,
		0x5f00, 0x5f00,
		0x6000, 0x6020,
		0x6028, 0x6040,
		0x6058, 0x609c,
		0x60a8, 0x619c,
		0x7700, 0x7798,
		0x77c0, 0x7880,
		0x78cc, 0x78fc,
		0x7b00, 0x7b58,
		0x7b60, 0x7b84,
		0x7b8c, 0x7c54,
		0x7d00, 0x7d38,
		0x7d40, 0x7d84,
		0x7d8c, 0x7ddc,
		0x7de4, 0x7e04,
		0x7e10, 0x7e1c,
		0x7e24, 0x7e38,
		0x7e40, 0x7e44,
		0x7e4c, 0x7e78,
		0x7e80, 0x7edc,
		0x7ee8, 0x7efc,
		0x8dc0, 0x8de4,
		0x8df8, 0x8e04,
		0x8e10, 0x8e84,
		0x8ea0, 0x8f88,
		0x8fb8, 0x9058,
		0x9060, 0x9060,
		0x9068, 0x90f8,
		0x9100, 0x9124,
		0x9400, 0x9470,
		0x9600, 0x9600,
		0x9608, 0x9638,
		0x9640, 0x9704,
		0x9710, 0x971c,
		0x9800, 0x9808,
		0x9820, 0x983c,
		0x9850, 0x9864,
		0x9c00, 0x9c6c,
		0x9c80, 0x9cec,
		0x9d00, 0x9d6c,
		0x9d80, 0x9dec,
		0x9e00, 0x9e6c,
		0x9e80, 0x9eec,
		0x9f00, 0x9f6c,
		0x9f80, 0xa020,
		0xd004, 0xd03c,
		0xd100, 0xd118,
		0xd200, 0xd214,
		0xd220, 0xd234,
		0xd240, 0xd254,
		0xd260, 0xd274,
		0xd280, 0xd294,
		0xd2a0, 0xd2b4,
		0xd2c0, 0xd2d4,
		0xd2e0, 0xd2f4,
		0xd300, 0xd31c,
		0xdfc0, 0xdfe0,
		0xe000, 0xf008,
		0xf010, 0xf018,
		0xf020, 0xf028,
		0x11000, 0x11014,
		0x11048, 0x1106c,
		0x11074, 0x11088,
		0x11098, 0x11120,
		0x1112c, 0x1117c,
		0x11190, 0x112e0,
		0x11300, 0x1130c,
		0x12000, 0x1206c,
		0x19040, 0x1906c,
		0x19078, 0x19080,
		0x1908c, 0x190e8,
		0x190f0, 0x190f8,
		0x19100, 0x19110,
		0x19120, 0x19124,
		0x19150, 0x19194,
		0x1919c, 0x191b0,
		0x191d0, 0x191e8,
		0x19238, 0x19290,
		0x192a4, 0x192b0,
		0x192bc, 0x192bc,
		0x19348, 0x1934c,
		0x193f8, 0x19418,
		0x19420, 0x19428,
		0x19430, 0x19444,
		0x1944c, 0x1946c,
		0x19474, 0x19474,
		0x19490, 0x194cc,
		0x194f0, 0x194f8,
		0x19c00, 0x19c48,
		0x19c50, 0x19c80,
		0x19c94, 0x19c98,
		0x19ca0, 0x19cbc,
		0x19ce4, 0x19ce4,
		0x19cf0, 0x19cf8,
		0x19d00, 0x19d28,
		0x19d50, 0x19d78,
		0x19d94, 0x19d98,
		0x19da0, 0x19dc8,
		0x19df0, 0x19e10,
		0x19e50, 0x19e6c,
		0x19ea0, 0x19ebc,
		0x19ec4, 0x19ef4,
		0x19f04, 0x19f2c,
		0x19f34, 0x19f34,
		0x19f40, 0x19f50,
		0x19f90, 0x19fac,
		0x19fc4, 0x19fc8,
		0x19fd0, 0x19fe4,
		0x1a000, 0x1a004,
		0x1a010, 0x1a06c,
		0x1a0b0, 0x1a0e4,
		0x1a0ec, 0x1a0f8,
		0x1a100, 0x1a108,
		0x1a114, 0x1a120,
		0x1a128, 0x1a130,
		0x1a138, 0x1a138,
		0x1a190, 0x1a1c4,
		0x1a1fc, 0x1a1fc,
		0x1e008, 0x1e00c,
		0x1e040, 0x1e044,
		0x1e04c, 0x1e04c,
		0x1e284, 0x1e290,
		0x1e2c0, 0x1e2c0,
		0x1e2e0, 0x1e2e0,
		0x1e300, 0x1e384,
		0x1e3c0, 0x1e3c8,
		0x1e408, 0x1e40c,
		0x1e440, 0x1e444,
		0x1e44c, 0x1e44c,
		0x1e684, 0x1e690,
		0x1e6c0, 0x1e6c0,
		0x1e6e0, 0x1e6e0,
		0x1e700, 0x1e784,
		0x1e7c0, 0x1e7c8,
		0x1e808, 0x1e80c,
		0x1e840, 0x1e844,
		0x1e84c, 0x1e84c,
		0x1ea84, 0x1ea90,
		0x1eac0, 0x1eac0,
		0x1eae0, 0x1eae0,
		0x1eb00, 0x1eb84,
		0x1ebc0, 0x1ebc8,
		0x1ec08, 0x1ec0c,
		0x1ec40, 0x1ec44,
		0x1ec4c, 0x1ec4c,
		0x1ee84, 0x1ee90,
		0x1eec0, 0x1eec0,
		0x1eee0, 0x1eee0,
		0x1ef00, 0x1ef84,
		0x1efc0, 0x1efc8,
		0x1f008, 0x1f00c,
		0x1f040, 0x1f044,
		0x1f04c, 0x1f04c,
		0x1f284, 0x1f290,
		0x1f2c0, 0x1f2c0,
		0x1f2e0, 0x1f2e0,
		0x1f300, 0x1f384,
		0x1f3c0, 0x1f3c8,
		0x1f408, 0x1f40c,
		0x1f440, 0x1f444,
		0x1f44c, 0x1f44c,
		0x1f684, 0x1f690,
		0x1f6c0, 0x1f6c0,
		0x1f6e0, 0x1f6e0,
		0x1f700, 0x1f784,
		0x1f7c0, 0x1f7c8,
		0x1f808, 0x1f80c,
		0x1f840, 0x1f844,
		0x1f84c, 0x1f84c,
		0x1fa84, 0x1fa90,
		0x1fac0, 0x1fac0,
		0x1fae0, 0x1fae0,
		0x1fb00, 0x1fb84,
		0x1fbc0, 0x1fbc8,
		0x1fc08, 0x1fc0c,
		0x1fc40, 0x1fc44,
		0x1fc4c, 0x1fc4c,
		0x1fe84, 0x1fe90,
		0x1fec0, 0x1fec0,
		0x1fee0, 0x1fee0,
		0x1ff00, 0x1ff84,
		0x1ffc0, 0x1ffc8,
		0x30000, 0x30030,
		0x30038, 0x30038,
		0x30040, 0x30040,
		0x30048, 0x30048,
		0x30050, 0x30050,
		0x3005c, 0x30060,
		0x30068, 0x30068,
		0x30070, 0x30070,
		0x30100, 0x30168,
		0x30190, 0x301a0,
		0x301a8, 0x301b8,
		0x301c4, 0x301c8,
		0x301d0, 0x301d0,
		0x30200, 0x30320,
		0x30400, 0x304b4,
		0x304c0, 0x3052c,
		0x30540, 0x3061c,
		0x30800, 0x308a0,
		0x308c0, 0x30908,
		0x30910, 0x309b8,
		0x30a00, 0x30a04,
		0x30a0c, 0x30a14,
		0x30a1c, 0x30a2c,
		0x30a44, 0x30a50,
		0x30a74, 0x30a74,
		0x30a7c, 0x30afc,
		0x30b08, 0x30c24,
		0x30d00, 0x30d14,
		0x30d1c, 0x30d3c,
		0x30d44, 0x30d4c,
		0x30d54, 0x30d74,
		0x30d7c, 0x30d7c,
		0x30de0, 0x30de0,
		0x30e00, 0x30ed4,
		0x30f00, 0x30fa4,
		0x30fc0, 0x30fc4,
		0x31000, 0x31004,
		0x31080, 0x310fc,
		0x31208, 0x31220,
		0x3123c, 0x31254,
		0x31300, 0x31300,
		0x31308, 0x3131c,
		0x31338, 0x3133c,
		0x31380, 0x31380,
		0x31388, 0x313a8,
		0x313b4, 0x313b4,
		0x31400, 0x31420,
		0x31438, 0x3143c,
		0x31480, 0x31480,
		0x314a8, 0x314a8,
		0x314b0, 0x314b4,
		0x314c8, 0x314d4,
		0x31a40, 0x31a4c,
		0x31af0, 0x31b20,
		0x31b38, 0x31b3c,
		0x31b80, 0x31b80,
		0x31ba8, 0x31ba8,
		0x31bb0, 0x31bb4,
		0x31bc8, 0x31bd4,
		0x32140, 0x3218c,
		0x321f0, 0x321f4,
		0x32200, 0x32200,
		0x32218, 0x32218,
		0x32400, 0x32400,
		0x32408, 0x3241c,
		0x32618, 0x32620,
		0x32664, 0x32664,
		0x326a8, 0x326a8,
		0x326ec, 0x326ec,
		0x32a00, 0x32abc,
		0x32b00, 0x32b38,
		0x32b40, 0x32b58,
		0x32b60, 0x32b78,
		0x32c00, 0x32c00,
		0x32c08, 0x32c3c,
		0x32e00, 0x32e2c,
		0x32f00, 0x32f2c,
		0x33000, 0x3302c,
		0x33034, 0x33050,
		0x33058, 0x33058,
		0x33060, 0x3308c,
		0x3309c, 0x330ac,
		0x330c0, 0x330c0,
		0x330c8, 0x330d0,
		0x330d8, 0x330e0,
		0x330ec, 0x3312c,
		0x33134, 0x33150,
		0x33158, 0x33158,
		0x33160, 0x3318c,
		0x3319c, 0x331ac,
		0x331c0, 0x331c0,
		0x331c8, 0x331d0,
		0x331d8, 0x331e0,
		0x331ec, 0x33290,
		0x33298, 0x332c4,
		0x332e4, 0x33390,
		0x33398, 0x333c4,
		0x333e4, 0x3342c,
		0x33434, 0x33450,
		0x33458, 0x33458,
		0x33460, 0x3348c,
		0x3349c, 0x334ac,
		0x334c0, 0x334c0,
		0x334c8, 0x334d0,
		0x334d8, 0x334e0,
		0x334ec, 0x3352c,
		0x33534, 0x33550,
		0x33558, 0x33558,
		0x33560, 0x3358c,
		0x3359c, 0x335ac,
		0x335c0, 0x335c0,
		0x335c8, 0x335d0,
		0x335d8, 0x335e0,
		0x335ec, 0x33690,
		0x33698, 0x336c4,
		0x336e4, 0x33790,
		0x33798, 0x337c4,
		0x337e4, 0x337fc,
		0x33814, 0x33814,
		0x33854, 0x33868,
		0x33880, 0x3388c,
		0x338c0, 0x338d0,
		0x338e8, 0x338ec,
		0x33900, 0x3392c,
		0x33934, 0x33950,
		0x33958, 0x33958,
		0x33960, 0x3398c,
		0x3399c, 0x339ac,
		0x339c0, 0x339c0,
		0x339c8, 0x339d0,
		0x339d8, 0x339e0,
		0x339ec, 0x33a90,
		0x33a98, 0x33ac4,
		0x33ae4, 0x33b10,
		0x33b24, 0x33b28,
		0x33b38, 0x33b50,
		0x33bf0, 0x33c10,
		0x33c24, 0x33c28,
		0x33c38, 0x33c50,
		0x33cf0, 0x33cfc,
		0x34000, 0x34030,
		0x34038, 0x34038,
		0x34040, 0x34040,
		0x34048, 0x34048,
		0x34050, 0x34050,
		0x3405c, 0x34060,
		0x34068, 0x34068,
		0x34070, 0x34070,
		0x34100, 0x34168,
		0x34190, 0x341a0,
		0x341a8, 0x341b8,
		0x341c4, 0x341c8,
		0x341d0, 0x341d0,
		0x34200, 0x34320,
		0x34400, 0x344b4,
		0x344c0, 0x3452c,
		0x34540, 0x3461c,
		0x34800, 0x348a0,
		0x348c0, 0x34908,
		0x34910, 0x349b8,
		0x34a00, 0x34a04,
		0x34a0c, 0x34a14,
		0x34a1c, 0x34a2c,
		0x34a44, 0x34a50,
		0x34a74, 0x34a74,
		0x34a7c, 0x34afc,
		0x34b08, 0x34c24,
		0x34d00, 0x34d14,
		0x34d1c, 0x34d3c,
		0x34d44, 0x34d4c,
		0x34d54, 0x34d74,
		0x34d7c, 0x34d7c,
		0x34de0, 0x34de0,
		0x34e00, 0x34ed4,
		0x34f00, 0x34fa4,
		0x34fc0, 0x34fc4,
		0x35000, 0x35004,
		0x35080, 0x350fc,
		0x35208, 0x35220,
		0x3523c, 0x35254,
		0x35300, 0x35300,
		0x35308, 0x3531c,
		0x35338, 0x3533c,
		0x35380, 0x35380,
		0x35388, 0x353a8,
		0x353b4, 0x353b4,
		0x35400, 0x35420,
		0x35438, 0x3543c,
		0x35480, 0x35480,
		0x354a8, 0x354a8,
		0x354b0, 0x354b4,
		0x354c8, 0x354d4,
		0x35a40, 0x35a4c,
		0x35af0, 0x35b20,
		0x35b38, 0x35b3c,
		0x35b80, 0x35b80,
		0x35ba8, 0x35ba8,
		0x35bb0, 0x35bb4,
		0x35bc8, 0x35bd4,
		0x36140, 0x3618c,
		0x361f0, 0x361f4,
		0x36200, 0x36200,
		0x36218, 0x36218,
		0x36400, 0x36400,
		0x36408, 0x3641c,
		0x36618, 0x36620,
		0x36664, 0x36664,
		0x366a8, 0x366a8,
		0x366ec, 0x366ec,
		0x36a00, 0x36abc,
		0x36b00, 0x36b38,
		0x36b40, 0x36b58,
		0x36b60, 0x36b78,
		0x36c00, 0x36c00,
		0x36c08, 0x36c3c,
		0x36e00, 0x36e2c,
		0x36f00, 0x36f2c,
		0x37000, 0x3702c,
		0x37034, 0x37050,
		0x37058, 0x37058,
		0x37060, 0x3708c,
		0x3709c, 0x370ac,
		0x370c0, 0x370c0,
		0x370c8, 0x370d0,
		0x370d8, 0x370e0,
		0x370ec, 0x3712c,
		0x37134, 0x37150,
		0x37158, 0x37158,
		0x37160, 0x3718c,
		0x3719c, 0x371ac,
		0x371c0, 0x371c0,
		0x371c8, 0x371d0,
		0x371d8, 0x371e0,
		0x371ec, 0x37290,
		0x37298, 0x372c4,
		0x372e4, 0x37390,
		0x37398, 0x373c4,
		0x373e4, 0x3742c,
		0x37434, 0x37450,
		0x37458, 0x37458,
		0x37460, 0x3748c,
		0x3749c, 0x374ac,
		0x374c0, 0x374c0,
		0x374c8, 0x374d0,
		0x374d8, 0x374e0,
		0x374ec, 0x3752c,
		0x37534, 0x37550,
		0x37558, 0x37558,
		0x37560, 0x3758c,
		0x3759c, 0x375ac,
		0x375c0, 0x375c0,
		0x375c8, 0x375d0,
		0x375d8, 0x375e0,
		0x375ec, 0x37690,
		0x37698, 0x376c4,
		0x376e4, 0x37790,
		0x37798, 0x377c4,
		0x377e4, 0x377fc,
		0x37814, 0x37814,
		0x37854, 0x37868,
		0x37880, 0x3788c,
		0x378c0, 0x378d0,
		0x378e8, 0x378ec,
		0x37900, 0x3792c,
		0x37934, 0x37950,
		0x37958, 0x37958,
		0x37960, 0x3798c,
		0x3799c, 0x379ac,
		0x379c0, 0x379c0,
		0x379c8, 0x379d0,
		0x379d8, 0x379e0,
		0x379ec, 0x37a90,
		0x37a98, 0x37ac4,
		0x37ae4, 0x37b10,
		0x37b24, 0x37b28,
		0x37b38, 0x37b50,
		0x37bf0, 0x37c10,
		0x37c24, 0x37c28,
		0x37c38, 0x37c50,
		0x37cf0, 0x37cfc,
		0x40040, 0x40040,
		0x40080, 0x40084,
		0x40100, 0x40100,
		0x40140, 0x401bc,
		0x40200, 0x40214,
		0x40228, 0x40228,
		0x40240, 0x40258,
		0x40280, 0x40280,
		0x40304, 0x40304,
		0x40330, 0x4033c,
		0x41304, 0x413c8,
		0x413d0, 0x413dc,
		0x413f0, 0x413f0,
		0x41400, 0x4140c,
		0x41414, 0x4141c,
		0x41480, 0x414d0,
		0x44000, 0x4407c,
		0x440c0, 0x441ac,
		0x441b4, 0x4427c,
		0x442c0, 0x443ac,
		0x443b4, 0x4447c,
		0x444c0, 0x445ac,
		0x445b4, 0x4467c,
		0x446c0, 0x447ac,
		0x447b4, 0x4487c,
		0x448c0, 0x449ac,
		0x449b4, 0x44a7c,
		0x44ac0, 0x44bac,
		0x44bb4, 0x44c7c,
		0x44cc0, 0x44dac,
		0x44db4, 0x44e7c,
		0x44ec0, 0x44fac,
		0x44fb4, 0x4507c,
		0x450c0, 0x451ac,
		0x451b4, 0x451fc,
		0x45800, 0x45804,
		0x45810, 0x45830,
		0x45840, 0x45860,
		0x45868, 0x45868,
		0x45880, 0x45884,
		0x458a0, 0x458b0,
		0x45a00, 0x45a04,
		0x45a10, 0x45a30,
		0x45a40, 0x45a60,
		0x45a68, 0x45a68,
		0x45a80, 0x45a84,
		0x45aa0, 0x45ab0,
		0x460c0, 0x460e4,
		0x47000, 0x4703c,
		0x47044, 0x4708c,
		0x47200, 0x47250,
		0x47400, 0x47408,
		0x47414, 0x47420,
		0x47600, 0x47618,
		0x47800, 0x47814,
		0x47820, 0x4782c,
		0x50000, 0x50084,
		0x50090, 0x500cc,
		0x50300, 0x50384,
		0x50400, 0x50400,
		0x50800, 0x50884,
		0x50890, 0x508cc,
		0x50b00, 0x50b84,
		0x50c00, 0x50c00,
		0x51000, 0x51020,
		0x51028, 0x510b0,
		0x51300, 0x51324,
	};

	static const unsigned int t6vf_reg_ranges[] = {
		VF_SGE_REG(A_SGE_VF_KDOORBELL), VF_SGE_REG(A_SGE_VF_GTS),
		VF_MPS_REG(A_MPS_VF_CTL),
		VF_MPS_REG(A_MPS_VF_STAT_RX_VF_ERR_FRAMES_H),
		VF_PL_REG(A_PL_VF_WHOAMI), VF_PL_REG(A_PL_VF_REVISION),
		VF_CIM_REG(A_CIM_VF_EXT_MAILBOX_CTRL),
		VF_CIM_REG(A_CIM_VF_EXT_MAILBOX_STATUS),
		FW_T6VF_MBDATA_BASE_ADDR,
		FW_T6VF_MBDATA_BASE_ADDR +
		((NUM_CIM_PF_MAILBOX_DATA_INSTANCES - 1) * 4),
	};

	u32 *buf_end = (u32 *)(buf + buf_size);
	const unsigned int *reg_ranges;
	int reg_ranges_size, range;
	unsigned int chip_version = chip_id(adap);

	/*
	 * Select the right set of register ranges to dump depending on the
	 * adapter chip type.
	 */
	switch (chip_version) {
	case CHELSIO_T4:
		if (adap->flags & IS_VF) {
			reg_ranges = t4vf_reg_ranges;
			reg_ranges_size = ARRAY_SIZE(t4vf_reg_ranges);
		} else {
			reg_ranges = t4_reg_ranges;
			reg_ranges_size = ARRAY_SIZE(t4_reg_ranges);
		}
		break;

	case CHELSIO_T5:
		if (adap->flags & IS_VF) {
			reg_ranges = t5vf_reg_ranges;
			reg_ranges_size = ARRAY_SIZE(t5vf_reg_ranges);
		} else {
			reg_ranges = t5_reg_ranges;
			reg_ranges_size = ARRAY_SIZE(t5_reg_ranges);
		}
		break;

	case CHELSIO_T6:
		if (adap->flags & IS_VF) {
			reg_ranges = t6vf_reg_ranges;
			reg_ranges_size = ARRAY_SIZE(t6vf_reg_ranges);
		} else {
			reg_ranges = t6_reg_ranges;
			reg_ranges_size = ARRAY_SIZE(t6_reg_ranges);
		}
		break;

	default:
		CH_ERR(adap,
			"Unsupported chip version %d\n", chip_version);
		return;
	}

	/*
	 * Clear the register buffer and insert the appropriate register
	 * values selected by the above register ranges.
	 */
	memset(buf, 0, buf_size);
	for (range = 0; range < reg_ranges_size; range += 2) {
		unsigned int reg = reg_ranges[range];
		unsigned int last_reg = reg_ranges[range + 1];
		u32 *bufp = (u32 *)(buf + reg);

		/*
		 * Iterate across the register range filling in the register
		 * buffer but don't write past the end of the register buffer.
		 */
		while (reg <= last_reg && bufp < buf_end) {
			*bufp++ = t4_read_reg(adap, reg);
			reg += sizeof(u32);
		}
	}
}

/*
 * Partial EEPROM Vital Product Data structure.  Includes only the ID and
 * VPD-R sections.
 */
struct t4_vpd_hdr {
	u8  id_tag;
	u8  id_len[2];
	u8  id_data[ID_LEN];
	u8  vpdr_tag;
	u8  vpdr_len[2];
};

/*
 * EEPROM reads take a few tens of us while writes can take a bit over 5 ms.
 */
#define EEPROM_DELAY		10		/* 10us per poll spin */
#define EEPROM_MAX_POLL		5000		/* x 5000 == 50ms */

#define EEPROM_STAT_ADDR	0x7bfc
#define VPD_BASE		0x400
#define VPD_BASE_OLD		0
#define VPD_LEN			1024
#define VPD_INFO_FLD_HDR_SIZE	3
#define CHELSIO_VPD_UNIQUE_ID	0x82

/*
 * Small utility function to wait till any outstanding VPD Access is complete.
 * We have a per-adapter state variable "VPD Busy" to indicate when we have a
 * VPD Access in flight.  This allows us to handle the problem of having a
 * previous VPD Access time out and prevent an attempt to inject a new VPD
 * Request before any in-flight VPD reguest has completed.
 */
static int t4_seeprom_wait(struct adapter *adapter)
{
	unsigned int base = adapter->params.pci.vpd_cap_addr;
	int max_poll;

	/*
	 * If no VPD Access is in flight, we can just return success right
	 * away.
	 */
	if (!adapter->vpd_busy)
		return 0;

	/*
	 * Poll the VPD Capability Address/Flag register waiting for it
	 * to indicate that the operation is complete.
	 */
	max_poll = EEPROM_MAX_POLL;
	do {
		u16 val;

		udelay(EEPROM_DELAY);
		t4_os_pci_read_cfg2(adapter, base + PCI_VPD_ADDR, &val);

		/*
		 * If the operation is complete, mark the VPD as no longer
		 * busy and return success.
		 */
		if ((val & PCI_VPD_ADDR_F) == adapter->vpd_flag) {
			adapter->vpd_busy = 0;
			return 0;
		}
	} while (--max_poll);

	/*
	 * Failure!  Note that we leave the VPD Busy status set in order to
	 * avoid pushing a new VPD Access request into the VPD Capability till
	 * the current operation eventually succeeds.  It's a bug to issue a
	 * new request when an existing request is in flight and will result
	 * in corrupt hardware state.
	 */
	return -ETIMEDOUT;
}

/**
 *	t4_seeprom_read - read a serial EEPROM location
 *	@adapter: adapter to read
 *	@addr: EEPROM virtual address
 *	@data: where to store the read data
 *
 *	Read a 32-bit word from a location in serial EEPROM using the card's PCI
 *	VPD capability.  Note that this function must be called with a virtual
 *	address.
 */
int t4_seeprom_read(struct adapter *adapter, u32 addr, u32 *data)
{
	unsigned int base = adapter->params.pci.vpd_cap_addr;
	int ret;

	/*
	 * VPD Accesses must alway be 4-byte aligned!
	 */
	if (addr >= EEPROMVSIZE || (addr & 3))
		return -EINVAL;

	/*
	 * Wait for any previous operation which may still be in flight to
	 * complete.
	 */
	ret = t4_seeprom_wait(adapter);
	if (ret) {
		CH_ERR(adapter, "VPD still busy from previous operation\n");
		return ret;
	}

	/*
	 * Issue our new VPD Read request, mark the VPD as being busy and wait
	 * for our request to complete.  If it doesn't complete, note the
	 * error and return it to our caller.  Note that we do not reset the
	 * VPD Busy status!
	 */
	t4_os_pci_write_cfg2(adapter, base + PCI_VPD_ADDR, (u16)addr);
	adapter->vpd_busy = 1;
	adapter->vpd_flag = PCI_VPD_ADDR_F;
	ret = t4_seeprom_wait(adapter);
	if (ret) {
		CH_ERR(adapter, "VPD read of address %#x failed\n", addr);
		return ret;
	}

	/*
	 * Grab the returned data, swizzle it into our endianness and
	 * return success.
	 */
	t4_os_pci_read_cfg4(adapter, base + PCI_VPD_DATA, data);
	*data = le32_to_cpu(*data);
	return 0;
}

/**
 *	t4_seeprom_write - write a serial EEPROM location
 *	@adapter: adapter to write
 *	@addr: virtual EEPROM address
 *	@data: value to write
 *
 *	Write a 32-bit word to a location in serial EEPROM using the card's PCI
 *	VPD capability.  Note that this function must be called with a virtual
 *	address.
 */
int t4_seeprom_write(struct adapter *adapter, u32 addr, u32 data)
{
	unsigned int base = adapter->params.pci.vpd_cap_addr;
	int ret;
	u32 stats_reg;
	int max_poll;

	/*
	 * VPD Accesses must alway be 4-byte aligned!
	 */
	if (addr >= EEPROMVSIZE || (addr & 3))
		return -EINVAL;

	/*
	 * Wait for any previous operation which may still be in flight to
	 * complete.
	 */
	ret = t4_seeprom_wait(adapter);
	if (ret) {
		CH_ERR(adapter, "VPD still busy from previous operation\n");
		return ret;
	}

	/*
	 * Issue our new VPD Read request, mark the VPD as being busy and wait
	 * for our request to complete.  If it doesn't complete, note the
	 * error and return it to our caller.  Note that we do not reset the
	 * VPD Busy status!
	 */
	t4_os_pci_write_cfg4(adapter, base + PCI_VPD_DATA,
				 cpu_to_le32(data));
	t4_os_pci_write_cfg2(adapter, base + PCI_VPD_ADDR,
				 (u16)addr | PCI_VPD_ADDR_F);
	adapter->vpd_busy = 1;
	adapter->vpd_flag = 0;
	ret = t4_seeprom_wait(adapter);
	if (ret) {
		CH_ERR(adapter, "VPD write of address %#x failed\n", addr);
		return ret;
	}

	/*
	 * Reset PCI_VPD_DATA register after a transaction and wait for our
	 * request to complete. If it doesn't complete, return error.
	 */
	t4_os_pci_write_cfg4(adapter, base + PCI_VPD_DATA, 0);
	max_poll = EEPROM_MAX_POLL;
	do {
		udelay(EEPROM_DELAY);
		t4_seeprom_read(adapter, EEPROM_STAT_ADDR, &stats_reg);
	} while ((stats_reg & 0x1) && --max_poll);
	if (!max_poll)
		return -ETIMEDOUT;

	/* Return success! */
	return 0;
}

/**
 *	t4_eeprom_ptov - translate a physical EEPROM address to virtual
 *	@phys_addr: the physical EEPROM address
 *	@fn: the PCI function number
 *	@sz: size of function-specific area
 *
 *	Translate a physical EEPROM address to virtual.  The first 1K is
 *	accessed through virtual addresses starting at 31K, the rest is
 *	accessed through virtual addresses starting at 0.
 *
 *	The mapping is as follows:
 *	[0..1K) -> [31K..32K)
 *	[1K..1K+A) -> [ES-A..ES)
 *	[1K+A..ES) -> [0..ES-A-1K)
 *
 *	where A = @fn * @sz, and ES = EEPROM size.
 */
int t4_eeprom_ptov(unsigned int phys_addr, unsigned int fn, unsigned int sz)
{
	fn *= sz;
	if (phys_addr < 1024)
		return phys_addr + (31 << 10);
	if (phys_addr < 1024 + fn)
		return EEPROMSIZE - fn + phys_addr - 1024;
	if (phys_addr < EEPROMSIZE)
		return phys_addr - 1024 - fn;
	return -EINVAL;
}

/**
 *	t4_seeprom_wp - enable/disable EEPROM write protection
 *	@adapter: the adapter
 *	@enable: whether to enable or disable write protection
 *
 *	Enables or disables write protection on the serial EEPROM.
 */
int t4_seeprom_wp(struct adapter *adapter, int enable)
{
	return t4_seeprom_write(adapter, EEPROM_STAT_ADDR, enable ? 0xc : 0);
}

/**
 *	get_vpd_keyword_val - Locates an information field keyword in the VPD
 *	@v: Pointer to buffered vpd data structure
 *	@kw: The keyword to search for
 *
 *	Returns the value of the information field keyword or
 *	-ENOENT otherwise.
 */
static int get_vpd_keyword_val(const struct t4_vpd_hdr *v, const char *kw)
{
	int i;
	unsigned int offset , len;
	const u8 *buf = (const u8 *)v;
	const u8 *vpdr_len = &v->vpdr_len[0];
	offset = sizeof(struct t4_vpd_hdr);
	len =  (u16)vpdr_len[0] + ((u16)vpdr_len[1] << 8);

	if (len + sizeof(struct t4_vpd_hdr) > VPD_LEN) {
		return -ENOENT;
	}

	for (i = offset; i + VPD_INFO_FLD_HDR_SIZE <= offset + len;) {
		if(memcmp(buf + i , kw , 2) == 0){
			i += VPD_INFO_FLD_HDR_SIZE;
			return i;
		}

		i += VPD_INFO_FLD_HDR_SIZE + buf[i+2];
	}

	return -ENOENT;
}


/**
 *	get_vpd_params - read VPD parameters from VPD EEPROM
 *	@adapter: adapter to read
 *	@p: where to store the parameters
 *	@vpd: caller provided temporary space to read the VPD into
 *
 *	Reads card parameters stored in VPD EEPROM.
 */
static int get_vpd_params(struct adapter *adapter, struct vpd_params *p,
    u8 *vpd)
{
	int i, ret, addr;
	int ec, sn, pn, na;
	u8 csum;
	const struct t4_vpd_hdr *v;

	/*
	 * Card information normally starts at VPD_BASE but early cards had
	 * it at 0.
	 */
	ret = t4_seeprom_read(adapter, VPD_BASE, (u32 *)(vpd));
	if (ret)
		return (ret);

	/*
	 * The VPD shall have a unique identifier specified by the PCI SIG.
	 * For chelsio adapters, the identifier is 0x82. The first byte of a VPD
	 * shall be CHELSIO_VPD_UNIQUE_ID (0x82). The VPD programming software
	 * is expected to automatically put this entry at the
	 * beginning of the VPD.
	 */
	addr = *vpd == CHELSIO_VPD_UNIQUE_ID ? VPD_BASE : VPD_BASE_OLD;

	for (i = 0; i < VPD_LEN; i += 4) {
		ret = t4_seeprom_read(adapter, addr + i, (u32 *)(vpd + i));
		if (ret)
			return ret;
	}
 	v = (const struct t4_vpd_hdr *)vpd;

#define FIND_VPD_KW(var,name) do { \
	var = get_vpd_keyword_val(v , name); \
	if (var < 0) { \
		CH_ERR(adapter, "missing VPD keyword " name "\n"); \
		return -EINVAL; \
	} \
} while (0)

	FIND_VPD_KW(i, "RV");
	for (csum = 0; i >= 0; i--)
		csum += vpd[i];

	if (csum) {
		CH_ERR(adapter,
			"corrupted VPD EEPROM, actual csum %u\n", csum);
		return -EINVAL;
	}

	FIND_VPD_KW(ec, "EC");
	FIND_VPD_KW(sn, "SN");
	FIND_VPD_KW(pn, "PN");
	FIND_VPD_KW(na, "NA");
#undef FIND_VPD_KW

	memcpy(p->id, v->id_data, ID_LEN);
	strstrip(p->id);
	memcpy(p->ec, vpd + ec, EC_LEN);
	strstrip(p->ec);
	i = vpd[sn - VPD_INFO_FLD_HDR_SIZE + 2];
	memcpy(p->sn, vpd + sn, min(i, SERNUM_LEN));
	strstrip(p->sn);
	i = vpd[pn - VPD_INFO_FLD_HDR_SIZE + 2];
	memcpy(p->pn, vpd + pn, min(i, PN_LEN));
	strstrip((char *)p->pn);
	i = vpd[na - VPD_INFO_FLD_HDR_SIZE + 2];
	memcpy(p->na, vpd + na, min(i, MACADDR_LEN));
	strstrip((char *)p->na);

	return 0;
}

/* serial flash and firmware constants and flash config file constants */
enum {
	SF_ATTEMPTS = 10,	/* max retries for SF operations */

	/* flash command opcodes */
	SF_PROG_PAGE    = 2,	/* program page */
	SF_WR_DISABLE   = 4,	/* disable writes */
	SF_RD_STATUS    = 5,	/* read status register */
	SF_WR_ENABLE    = 6,	/* enable writes */
	SF_RD_DATA_FAST = 0xb,	/* read flash */
	SF_RD_ID	= 0x9f,	/* read ID */
	SF_ERASE_SECTOR = 0xd8,	/* erase sector */
};

/**
 *	sf1_read - read data from the serial flash
 *	@adapter: the adapter
 *	@byte_cnt: number of bytes to read
 *	@cont: whether another operation will be chained
 *	@lock: whether to lock SF for PL access only
 *	@valp: where to store the read data
 *
 *	Reads up to 4 bytes of data from the serial flash.  The location of
 *	the read needs to be specified prior to calling this by issuing the
 *	appropriate commands to the serial flash.
 */
static int sf1_read(struct adapter *adapter, unsigned int byte_cnt, int cont,
		    int lock, u32 *valp)
{
	int ret;

	if (!byte_cnt || byte_cnt > 4)
		return -EINVAL;
	if (t4_read_reg(adapter, A_SF_OP) & F_BUSY)
		return -EBUSY;
	t4_write_reg(adapter, A_SF_OP,
		     V_SF_LOCK(lock) | V_CONT(cont) | V_BYTECNT(byte_cnt - 1));
	ret = t4_wait_op_done(adapter, A_SF_OP, F_BUSY, 0, SF_ATTEMPTS, 5);
	if (!ret)
		*valp = t4_read_reg(adapter, A_SF_DATA);
	return ret;
}

/**
 *	sf1_write - write data to the serial flash
 *	@adapter: the adapter
 *	@byte_cnt: number of bytes to write
 *	@cont: whether another operation will be chained
 *	@lock: whether to lock SF for PL access only
 *	@val: value to write
 *
 *	Writes up to 4 bytes of data to the serial flash.  The location of
 *	the write needs to be specified prior to calling this by issuing the
 *	appropriate commands to the serial flash.
 */
static int sf1_write(struct adapter *adapter, unsigned int byte_cnt, int cont,
		     int lock, u32 val)
{
	if (!byte_cnt || byte_cnt > 4)
		return -EINVAL;
	if (t4_read_reg(adapter, A_SF_OP) & F_BUSY)
		return -EBUSY;
	t4_write_reg(adapter, A_SF_DATA, val);
	t4_write_reg(adapter, A_SF_OP, V_SF_LOCK(lock) |
		     V_CONT(cont) | V_BYTECNT(byte_cnt - 1) | V_OP(1));
	return t4_wait_op_done(adapter, A_SF_OP, F_BUSY, 0, SF_ATTEMPTS, 5);
}

/**
 *	flash_wait_op - wait for a flash operation to complete
 *	@adapter: the adapter
 *	@attempts: max number of polls of the status register
 *	@delay: delay between polls in ms
 *
 *	Wait for a flash operation to complete by polling the status register.
 */
static int flash_wait_op(struct adapter *adapter, int attempts, int delay)
{
	int ret;
	u32 status;

	while (1) {
		if ((ret = sf1_write(adapter, 1, 1, 1, SF_RD_STATUS)) != 0 ||
		    (ret = sf1_read(adapter, 1, 0, 1, &status)) != 0)
			return ret;
		if (!(status & 1))
			return 0;
		if (--attempts == 0)
			return -EAGAIN;
		if (delay)
			msleep(delay);
	}
}

/**
 *	t4_read_flash - read words from serial flash
 *	@adapter: the adapter
 *	@addr: the start address for the read
 *	@nwords: how many 32-bit words to read
 *	@data: where to store the read data
 *	@byte_oriented: whether to store data as bytes or as words
 *
 *	Read the specified number of 32-bit words from the serial flash.
 *	If @byte_oriented is set the read data is stored as a byte array
 *	(i.e., big-endian), otherwise as 32-bit words in the platform's
 *	natural endianness.
 */
int t4_read_flash(struct adapter *adapter, unsigned int addr,
		  unsigned int nwords, u32 *data, int byte_oriented)
{
	int ret;

	if (addr + nwords * sizeof(u32) > adapter->params.sf_size || (addr & 3))
		return -EINVAL;

	addr = swab32(addr) | SF_RD_DATA_FAST;

	if ((ret = sf1_write(adapter, 4, 1, 0, addr)) != 0 ||
	    (ret = sf1_read(adapter, 1, 1, 0, data)) != 0)
		return ret;

	for ( ; nwords; nwords--, data++) {
		ret = sf1_read(adapter, 4, nwords > 1, nwords == 1, data);
		if (nwords == 1)
			t4_write_reg(adapter, A_SF_OP, 0);    /* unlock SF */
		if (ret)
			return ret;
		if (byte_oriented)
			*data = (__force __u32)(cpu_to_be32(*data));
	}
	return 0;
}

/**
 *	t4_write_flash - write up to a page of data to the serial flash
 *	@adapter: the adapter
 *	@addr: the start address to write
 *	@n: length of data to write in bytes
 *	@data: the data to write
 *	@byte_oriented: whether to store data as bytes or as words
 *
 *	Writes up to a page of data (256 bytes) to the serial flash starting
 *	at the given address.  All the data must be written to the same page.
 *	If @byte_oriented is set the write data is stored as byte stream
 *	(i.e. matches what on disk), otherwise in big-endian.
 */
int t4_write_flash(struct adapter *adapter, unsigned int addr,
			  unsigned int n, const u8 *data, int byte_oriented)
{
	int ret;
	u32 buf[SF_PAGE_SIZE / 4];
	unsigned int i, c, left, val, offset = addr & 0xff;

	if (addr >= adapter->params.sf_size || offset + n > SF_PAGE_SIZE)
		return -EINVAL;

	val = swab32(addr) | SF_PROG_PAGE;

	if ((ret = sf1_write(adapter, 1, 0, 1, SF_WR_ENABLE)) != 0 ||
	    (ret = sf1_write(adapter, 4, 1, 1, val)) != 0)
		goto unlock;

	for (left = n; left; left -= c) {
		c = min(left, 4U);
		for (val = 0, i = 0; i < c; ++i)
			val = (val << 8) + *data++;

		if (!byte_oriented)
			val = cpu_to_be32(val);

		ret = sf1_write(adapter, c, c != left, 1, val);
		if (ret)
			goto unlock;
	}
	ret = flash_wait_op(adapter, 8, 1);
	if (ret)
		goto unlock;

	t4_write_reg(adapter, A_SF_OP, 0);    /* unlock SF */

	/* Read the page to verify the write succeeded */
	ret = t4_read_flash(adapter, addr & ~0xff, ARRAY_SIZE(buf), buf,
			    byte_oriented);
	if (ret)
		return ret;

	if (memcmp(data - n, (u8 *)buf + offset, n)) {
		CH_ERR(adapter,
			"failed to correctly write the flash page at %#x\n",
			addr);
		return -EIO;
	}
	return 0;

unlock:
	t4_write_reg(adapter, A_SF_OP, 0);    /* unlock SF */
	return ret;
}

/**
 *	t4_get_fw_version - read the firmware version
 *	@adapter: the adapter
 *	@vers: where to place the version
 *
 *	Reads the FW version from flash.
 */
int t4_get_fw_version(struct adapter *adapter, u32 *vers)
{
	return t4_read_flash(adapter, FLASH_FW_START +
			     offsetof(struct fw_hdr, fw_ver), 1,
			     vers, 0);
}

/**
 *	t4_get_bs_version - read the firmware bootstrap version
 *	@adapter: the adapter
 *	@vers: where to place the version
 *
 *	Reads the FW Bootstrap version from flash.
 */
int t4_get_bs_version(struct adapter *adapter, u32 *vers)
{
	return t4_read_flash(adapter, FLASH_FWBOOTSTRAP_START +
			     offsetof(struct fw_hdr, fw_ver), 1,
			     vers, 0);
}

/**
 *	t4_get_tp_version - read the TP microcode version
 *	@adapter: the adapter
 *	@vers: where to place the version
 *
 *	Reads the TP microcode version from flash.
 */
int t4_get_tp_version(struct adapter *adapter, u32 *vers)
{
	return t4_read_flash(adapter, FLASH_FW_START +
			     offsetof(struct fw_hdr, tp_microcode_ver),
			     1, vers, 0);
}

/**
 *	t4_get_exprom_version - return the Expansion ROM version (if any)
 *	@adapter: the adapter
 *	@vers: where to place the version
 *
 *	Reads the Expansion ROM header from FLASH and returns the version
 *	number (if present) through the @vers return value pointer.  We return
 *	this in the Firmware Version Format since it's convenient.  Return
 *	0 on success, -ENOENT if no Expansion ROM is present.
 */
int t4_get_exprom_version(struct adapter *adap, u32 *vers)
{
	struct exprom_header {
		unsigned char hdr_arr[16];	/* must start with 0x55aa */
		unsigned char hdr_ver[4];	/* Expansion ROM version */
	} *hdr;
	u32 exprom_header_buf[DIV_ROUND_UP(sizeof(struct exprom_header),
					   sizeof(u32))];
	int ret;

	ret = t4_read_flash(adap, FLASH_EXP_ROM_START,
			    ARRAY_SIZE(exprom_header_buf), exprom_header_buf,
			    0);
	if (ret)
		return ret;

	hdr = (struct exprom_header *)exprom_header_buf;
	if (hdr->hdr_arr[0] != 0x55 || hdr->hdr_arr[1] != 0xaa)
		return -ENOENT;

	*vers = (V_FW_HDR_FW_VER_MAJOR(hdr->hdr_ver[0]) |
		 V_FW_HDR_FW_VER_MINOR(hdr->hdr_ver[1]) |
		 V_FW_HDR_FW_VER_MICRO(hdr->hdr_ver[2]) |
		 V_FW_HDR_FW_VER_BUILD(hdr->hdr_ver[3]));
	return 0;
}

/**
 *	t4_get_scfg_version - return the Serial Configuration version
 *	@adapter: the adapter
 *	@vers: where to place the version
 *
 *	Reads the Serial Configuration Version via the Firmware interface
 *	(thus this can only be called once we're ready to issue Firmware
 *	commands).  The format of the Serial Configuration version is
 *	adapter specific.  Returns 0 on success, an error on failure.
 *
 *	Note that early versions of the Firmware didn't include the ability
 *	to retrieve the Serial Configuration version, so we zero-out the
 *	return-value parameter in that case to avoid leaving it with
 *	garbage in it.
 *
 *	Also note that the Firmware will return its cached copy of the Serial
 *	Initialization Revision ID, not the actual Revision ID as written in
 *	the Serial EEPROM.  This is only an issue if a new VPD has been written
 *	and the Firmware/Chip haven't yet gone through a RESET sequence.  So
 *	it's best to defer calling this routine till after a FW_RESET_CMD has
 *	been issued if the Host Driver will be performing a full adapter
 *	initialization.
 */
int t4_get_scfg_version(struct adapter *adapter, u32 *vers)
{
	u32 scfgrev_param;
	int ret;

	scfgrev_param = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
			 V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_SCFGREV));
	ret = t4_query_params(adapter, adapter->mbox, adapter->pf, 0,
			      1, &scfgrev_param, vers);
	if (ret)
		*vers = 0;
	return ret;
}

/**
 *	t4_get_vpd_version - return the VPD version
 *	@adapter: the adapter
 *	@vers: where to place the version
 *
 *	Reads the VPD via the Firmware interface (thus this can only be called
 *	once we're ready to issue Firmware commands).  The format of the
 *	VPD version is adapter specific.  Returns 0 on success, an error on
 *	failure.
 *
 *	Note that early versions of the Firmware didn't include the ability
 *	to retrieve the VPD version, so we zero-out the return-value parameter
 *	in that case to avoid leaving it with garbage in it.
 *
 *	Also note that the Firmware will return its cached copy of the VPD
 *	Revision ID, not the actual Revision ID as written in the Serial
 *	EEPROM.  This is only an issue if a new VPD has been written and the
 *	Firmware/Chip haven't yet gone through a RESET sequence.  So it's best
 *	to defer calling this routine till after a FW_RESET_CMD has been issued
 *	if the Host Driver will be performing a full adapter initialization.
 */
int t4_get_vpd_version(struct adapter *adapter, u32 *vers)
{
	u32 vpdrev_param;
	int ret;

	vpdrev_param = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
			V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_VPDREV));
	ret = t4_query_params(adapter, adapter->mbox, adapter->pf, 0,
			      1, &vpdrev_param, vers);
	if (ret)
		*vers = 0;
	return ret;
}

/**
 *	t4_get_version_info - extract various chip/firmware version information
 *	@adapter: the adapter
 *
 *	Reads various chip/firmware version numbers and stores them into the
 *	adapter Adapter Parameters structure.  If any of the efforts fails
 *	the first failure will be returned, but all of the version numbers
 *	will be read.
 */
int t4_get_version_info(struct adapter *adapter)
{
	int ret = 0;

	#define FIRST_RET(__getvinfo) \
	do { \
		int __ret = __getvinfo; \
		if (__ret && !ret) \
			ret = __ret; \
	} while (0)

	FIRST_RET(t4_get_fw_version(adapter, &adapter->params.fw_vers));
	FIRST_RET(t4_get_bs_version(adapter, &adapter->params.bs_vers));
	FIRST_RET(t4_get_tp_version(adapter, &adapter->params.tp_vers));
	FIRST_RET(t4_get_exprom_version(adapter, &adapter->params.er_vers));
	FIRST_RET(t4_get_scfg_version(adapter, &adapter->params.scfg_vers));
	FIRST_RET(t4_get_vpd_version(adapter, &adapter->params.vpd_vers));

	#undef FIRST_RET

	return ret;
}

/**
 *	t4_flash_erase_sectors - erase a range of flash sectors
 *	@adapter: the adapter
 *	@start: the first sector to erase
 *	@end: the last sector to erase
 *
 *	Erases the sectors in the given inclusive range.
 */
int t4_flash_erase_sectors(struct adapter *adapter, int start, int end)
{
	int ret = 0;

	if (end >= adapter->params.sf_nsec)
		return -EINVAL;

	while (start <= end) {
		if ((ret = sf1_write(adapter, 1, 0, 1, SF_WR_ENABLE)) != 0 ||
		    (ret = sf1_write(adapter, 4, 0, 1,
				     SF_ERASE_SECTOR | (start << 8))) != 0 ||
		    (ret = flash_wait_op(adapter, 14, 500)) != 0) {
			CH_ERR(adapter,
				"erase of flash sector %d failed, error %d\n",
				start, ret);
			break;
		}
		start++;
	}
	t4_write_reg(adapter, A_SF_OP, 0);    /* unlock SF */
	return ret;
}

/**
 *	t4_flash_cfg_addr - return the address of the flash configuration file
 *	@adapter: the adapter
 *
 *	Return the address within the flash where the Firmware Configuration
 *	File is stored, or an error if the device FLASH is too small to contain
 *	a Firmware Configuration File.
 */
int t4_flash_cfg_addr(struct adapter *adapter)
{
	/*
	 * If the device FLASH isn't large enough to hold a Firmware
	 * Configuration File, return an error.
	 */
	if (adapter->params.sf_size < FLASH_CFG_START + FLASH_CFG_MAX_SIZE)
		return -ENOSPC;

	return FLASH_CFG_START;
}

/*
 * Return TRUE if the specified firmware matches the adapter.  I.e. T4
 * firmware for T4 adapters, T5 firmware for T5 adapters, etc.  We go ahead
 * and emit an error message for mismatched firmware to save our caller the
 * effort ...
 */
static int t4_fw_matches_chip(struct adapter *adap,
			      const struct fw_hdr *hdr)
{
	/*
	 * The expression below will return FALSE for any unsupported adapter
	 * which will keep us "honest" in the future ...
	 */
	if ((is_t4(adap) && hdr->chip == FW_HDR_CHIP_T4) ||
	    (is_t5(adap) && hdr->chip == FW_HDR_CHIP_T5) ||
	    (is_t6(adap) && hdr->chip == FW_HDR_CHIP_T6))
		return 1;

	CH_ERR(adap,
		"FW image (%d) is not suitable for this adapter (%d)\n",
		hdr->chip, chip_id(adap));
	return 0;
}

/**
 *	t4_load_fw - download firmware
 *	@adap: the adapter
 *	@fw_data: the firmware image to write
 *	@size: image size
 *
 *	Write the supplied firmware image to the card's serial flash.
 */
int t4_load_fw(struct adapter *adap, const u8 *fw_data, unsigned int size)
{
	u32 csum;
	int ret, addr;
	unsigned int i;
	u8 first_page[SF_PAGE_SIZE];
	const u32 *p = (const u32 *)fw_data;
	const struct fw_hdr *hdr = (const struct fw_hdr *)fw_data;
	unsigned int sf_sec_size = adap->params.sf_size / adap->params.sf_nsec;
	unsigned int fw_start_sec;
	unsigned int fw_start;
	unsigned int fw_size;

	if (ntohl(hdr->magic) == FW_HDR_MAGIC_BOOTSTRAP) {
		fw_start_sec = FLASH_FWBOOTSTRAP_START_SEC;
		fw_start = FLASH_FWBOOTSTRAP_START;
		fw_size = FLASH_FWBOOTSTRAP_MAX_SIZE;
	} else {
		fw_start_sec = FLASH_FW_START_SEC;
 		fw_start = FLASH_FW_START;
		fw_size = FLASH_FW_MAX_SIZE;
	}

	if (!size) {
		CH_ERR(adap, "FW image has no data\n");
		return -EINVAL;
	}
	if (size & 511) {
		CH_ERR(adap,
			"FW image size not multiple of 512 bytes\n");
		return -EINVAL;
	}
	if ((unsigned int) be16_to_cpu(hdr->len512) * 512 != size) {
		CH_ERR(adap,
			"FW image size differs from size in FW header\n");
		return -EINVAL;
	}
	if (size > fw_size) {
		CH_ERR(adap, "FW image too large, max is %u bytes\n",
			fw_size);
		return -EFBIG;
	}
	if (!t4_fw_matches_chip(adap, hdr))
		return -EINVAL;

	for (csum = 0, i = 0; i < size / sizeof(csum); i++)
		csum += be32_to_cpu(p[i]);

	if (csum != 0xffffffff) {
		CH_ERR(adap,
			"corrupted firmware image, checksum %#x\n", csum);
		return -EINVAL;
	}

	i = DIV_ROUND_UP(size, sf_sec_size);	/* # of sectors spanned */
	ret = t4_flash_erase_sectors(adap, fw_start_sec, fw_start_sec + i - 1);
	if (ret)
		goto out;

	/*
	 * We write the correct version at the end so the driver can see a bad
	 * version if the FW write fails.  Start by writing a copy of the
	 * first page with a bad version.
	 */
	memcpy(first_page, fw_data, SF_PAGE_SIZE);
	((struct fw_hdr *)first_page)->fw_ver = cpu_to_be32(0xffffffff);
	ret = t4_write_flash(adap, fw_start, SF_PAGE_SIZE, first_page, 1);
	if (ret)
		goto out;

	addr = fw_start;
	for (size -= SF_PAGE_SIZE; size; size -= SF_PAGE_SIZE) {
		addr += SF_PAGE_SIZE;
		fw_data += SF_PAGE_SIZE;
		ret = t4_write_flash(adap, addr, SF_PAGE_SIZE, fw_data, 1);
		if (ret)
			goto out;
	}

	ret = t4_write_flash(adap,
			     fw_start + offsetof(struct fw_hdr, fw_ver),
			     sizeof(hdr->fw_ver), (const u8 *)&hdr->fw_ver, 1);
out:
	if (ret)
		CH_ERR(adap, "firmware download failed, error %d\n",
			ret);
	return ret;
}

/**
 *	t4_fwcache - firmware cache operation
 *	@adap: the adapter
 *	@op  : the operation (flush or flush and invalidate)
 */
int t4_fwcache(struct adapter *adap, enum fw_params_param_dev_fwcache op)
{
	struct fw_params_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn =
	    cpu_to_be32(V_FW_CMD_OP(FW_PARAMS_CMD) |
			    F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				V_FW_PARAMS_CMD_PFN(adap->pf) |
				V_FW_PARAMS_CMD_VFN(0));
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));
	c.param[0].mnem =
	    cpu_to_be32(V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
			    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_FWCACHE));
	c.param[0].val = (__force __be32)op;

	return t4_wr_mbox(adap, adap->mbox, &c, sizeof(c), NULL);
}

void t4_cim_read_pif_la(struct adapter *adap, u32 *pif_req, u32 *pif_rsp,
			unsigned int *pif_req_wrptr,
			unsigned int *pif_rsp_wrptr)
{
	int i, j;
	u32 cfg, val, req, rsp;

	cfg = t4_read_reg(adap, A_CIM_DEBUGCFG);
	if (cfg & F_LADBGEN)
		t4_write_reg(adap, A_CIM_DEBUGCFG, cfg ^ F_LADBGEN);

	val = t4_read_reg(adap, A_CIM_DEBUGSTS);
	req = G_POLADBGWRPTR(val);
	rsp = G_PILADBGWRPTR(val);
	if (pif_req_wrptr)
		*pif_req_wrptr = req;
	if (pif_rsp_wrptr)
		*pif_rsp_wrptr = rsp;

	for (i = 0; i < CIM_PIFLA_SIZE; i++) {
		for (j = 0; j < 6; j++) {
			t4_write_reg(adap, A_CIM_DEBUGCFG, V_POLADBGRDPTR(req) |
				     V_PILADBGRDPTR(rsp));
			*pif_req++ = t4_read_reg(adap, A_CIM_PO_LA_DEBUGDATA);
			*pif_rsp++ = t4_read_reg(adap, A_CIM_PI_LA_DEBUGDATA);
			req++;
			rsp++;
		}
		req = (req + 2) & M_POLADBGRDPTR;
		rsp = (rsp + 2) & M_PILADBGRDPTR;
	}
	t4_write_reg(adap, A_CIM_DEBUGCFG, cfg);
}

void t4_cim_read_ma_la(struct adapter *adap, u32 *ma_req, u32 *ma_rsp)
{
	u32 cfg;
	int i, j, idx;

	cfg = t4_read_reg(adap, A_CIM_DEBUGCFG);
	if (cfg & F_LADBGEN)
		t4_write_reg(adap, A_CIM_DEBUGCFG, cfg ^ F_LADBGEN);

	for (i = 0; i < CIM_MALA_SIZE; i++) {
		for (j = 0; j < 5; j++) {
			idx = 8 * i + j;
			t4_write_reg(adap, A_CIM_DEBUGCFG, V_POLADBGRDPTR(idx) |
				     V_PILADBGRDPTR(idx));
			*ma_req++ = t4_read_reg(adap, A_CIM_PO_LA_MADEBUGDATA);
			*ma_rsp++ = t4_read_reg(adap, A_CIM_PI_LA_MADEBUGDATA);
		}
	}
	t4_write_reg(adap, A_CIM_DEBUGCFG, cfg);
}

void t4_ulprx_read_la(struct adapter *adap, u32 *la_buf)
{
	unsigned int i, j;

	for (i = 0; i < 8; i++) {
		u32 *p = la_buf + i;

		t4_write_reg(adap, A_ULP_RX_LA_CTL, i);
		j = t4_read_reg(adap, A_ULP_RX_LA_WRPTR);
		t4_write_reg(adap, A_ULP_RX_LA_RDPTR, j);
		for (j = 0; j < ULPRX_LA_SIZE; j++, p += 8)
			*p = t4_read_reg(adap, A_ULP_RX_LA_RDDATA);
	}
}

#define ADVERT_MASK (FW_PORT_CAP_SPEED_100M | FW_PORT_CAP_SPEED_1G |\
		     FW_PORT_CAP_SPEED_10G | FW_PORT_CAP_SPEED_40G | \
		     FW_PORT_CAP_SPEED_100G | FW_PORT_CAP_ANEG)

/**
 *	t4_link_l1cfg - apply link configuration to MAC/PHY
 *	@phy: the PHY to setup
 *	@mac: the MAC to setup
 *	@lc: the requested link configuration
 *
 *	Set up a port's MAC and PHY according to a desired link configuration.
 *	- If the PHY can auto-negotiate first decide what to advertise, then
 *	  enable/disable auto-negotiation as desired, and reset.
 *	- If the PHY does not auto-negotiate just reset it.
 *	- If auto-negotiation is off set the MAC to the proper speed/duplex/FC,
 *	  otherwise do it later based on the outcome of auto-negotiation.
 */
int t4_link_l1cfg(struct adapter *adap, unsigned int mbox, unsigned int port,
		  struct link_config *lc)
{
	struct fw_port_cmd c;
	unsigned int fc = 0, mdi = V_FW_PORT_CAP_MDI(FW_PORT_CAP_MDI_AUTO);

	lc->link_ok = 0;
	if (lc->requested_fc & PAUSE_RX)
		fc |= FW_PORT_CAP_FC_RX;
	if (lc->requested_fc & PAUSE_TX)
		fc |= FW_PORT_CAP_FC_TX;

	memset(&c, 0, sizeof(c));
	c.op_to_portid = cpu_to_be32(V_FW_CMD_OP(FW_PORT_CMD) |
				     F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				     V_FW_PORT_CMD_PORTID(port));
	c.action_to_len16 =
		cpu_to_be32(V_FW_PORT_CMD_ACTION(FW_PORT_ACTION_L1_CFG) |
			    FW_LEN16(c));

	if (!(lc->supported & FW_PORT_CAP_ANEG)) {
		c.u.l1cfg.rcap = cpu_to_be32((lc->supported & ADVERT_MASK) |
					     fc);
		lc->fc = lc->requested_fc & (PAUSE_RX | PAUSE_TX);
	} else if (lc->autoneg == AUTONEG_DISABLE) {
		c.u.l1cfg.rcap = cpu_to_be32(lc->requested_speed | fc | mdi);
		lc->fc = lc->requested_fc & (PAUSE_RX | PAUSE_TX);
	} else
		c.u.l1cfg.rcap = cpu_to_be32(lc->advertising | fc | mdi);

	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_restart_aneg - restart autonegotiation
 *	@adap: the adapter
 *	@mbox: mbox to use for the FW command
 *	@port: the port id
 *
 *	Restarts autonegotiation for the selected port.
 */
int t4_restart_aneg(struct adapter *adap, unsigned int mbox, unsigned int port)
{
	struct fw_port_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_portid = cpu_to_be32(V_FW_CMD_OP(FW_PORT_CMD) |
				     F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				     V_FW_PORT_CMD_PORTID(port));
	c.action_to_len16 =
		cpu_to_be32(V_FW_PORT_CMD_ACTION(FW_PORT_ACTION_L1_CFG) |
			    FW_LEN16(c));
	c.u.l1cfg.rcap = cpu_to_be32(FW_PORT_CAP_ANEG);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

typedef void (*int_handler_t)(struct adapter *adap);

struct intr_info {
	unsigned int mask;	/* bits to check in interrupt status */
	const char *msg;	/* message to print or NULL */
	short stat_idx;		/* stat counter to increment or -1 */
	unsigned short fatal;	/* whether the condition reported is fatal */
	int_handler_t int_handler;	/* platform-specific int handler */
};

/**
 *	t4_handle_intr_status - table driven interrupt handler
 *	@adapter: the adapter that generated the interrupt
 *	@reg: the interrupt status register to process
 *	@acts: table of interrupt actions
 *
 *	A table driven interrupt handler that applies a set of masks to an
 *	interrupt status word and performs the corresponding actions if the
 *	interrupts described by the mask have occurred.  The actions include
 *	optionally emitting a warning or alert message.  The table is terminated
 *	by an entry specifying mask 0.  Returns the number of fatal interrupt
 *	conditions.
 */
static int t4_handle_intr_status(struct adapter *adapter, unsigned int reg,
				 const struct intr_info *acts)
{
	int fatal = 0;
	unsigned int mask = 0;
	unsigned int status = t4_read_reg(adapter, reg);

	for ( ; acts->mask; ++acts) {
		if (!(status & acts->mask))
			continue;
		if (acts->fatal) {
			fatal++;
			CH_ALERT(adapter, "%s (0x%x)\n", acts->msg,
				  status & acts->mask);
		} else if (acts->msg)
			CH_WARN_RATELIMIT(adapter, "%s (0x%x)\n", acts->msg,
				 status & acts->mask);
		if (acts->int_handler)
			acts->int_handler(adapter);
		mask |= acts->mask;
	}
	status &= mask;
	if (status)	/* clear processed interrupts */
		t4_write_reg(adapter, reg, status);
	return fatal;
}

/*
 * Interrupt handler for the PCIE module.
 */
static void pcie_intr_handler(struct adapter *adapter)
{
	static const struct intr_info sysbus_intr_info[] = {
		{ F_RNPP, "RXNP array parity error", -1, 1 },
		{ F_RPCP, "RXPC array parity error", -1, 1 },
		{ F_RCIP, "RXCIF array parity error", -1, 1 },
		{ F_RCCP, "Rx completions control array parity error", -1, 1 },
		{ F_RFTP, "RXFT array parity error", -1, 1 },
		{ 0 }
	};
	static const struct intr_info pcie_port_intr_info[] = {
		{ F_TPCP, "TXPC array parity error", -1, 1 },
		{ F_TNPP, "TXNP array parity error", -1, 1 },
		{ F_TFTP, "TXFT array parity error", -1, 1 },
		{ F_TCAP, "TXCA array parity error", -1, 1 },
		{ F_TCIP, "TXCIF array parity error", -1, 1 },
		{ F_RCAP, "RXCA array parity error", -1, 1 },
		{ F_OTDD, "outbound request TLP discarded", -1, 1 },
		{ F_RDPE, "Rx data parity error", -1, 1 },
		{ F_TDUE, "Tx uncorrectable data error", -1, 1 },
		{ 0 }
	};
	static const struct intr_info pcie_intr_info[] = {
		{ F_MSIADDRLPERR, "MSI AddrL parity error", -1, 1 },
		{ F_MSIADDRHPERR, "MSI AddrH parity error", -1, 1 },
		{ F_MSIDATAPERR, "MSI data parity error", -1, 1 },
		{ F_MSIXADDRLPERR, "MSI-X AddrL parity error", -1, 1 },
		{ F_MSIXADDRHPERR, "MSI-X AddrH parity error", -1, 1 },
		{ F_MSIXDATAPERR, "MSI-X data parity error", -1, 1 },
		{ F_MSIXDIPERR, "MSI-X DI parity error", -1, 1 },
		{ F_PIOCPLPERR, "PCI PIO completion FIFO parity error", -1, 1 },
		{ F_PIOREQPERR, "PCI PIO request FIFO parity error", -1, 1 },
		{ F_TARTAGPERR, "PCI PCI target tag FIFO parity error", -1, 1 },
		{ F_CCNTPERR, "PCI CMD channel count parity error", -1, 1 },
		{ F_CREQPERR, "PCI CMD channel request parity error", -1, 1 },
		{ F_CRSPPERR, "PCI CMD channel response parity error", -1, 1 },
		{ F_DCNTPERR, "PCI DMA channel count parity error", -1, 1 },
		{ F_DREQPERR, "PCI DMA channel request parity error", -1, 1 },
		{ F_DRSPPERR, "PCI DMA channel response parity error", -1, 1 },
		{ F_HCNTPERR, "PCI HMA channel count parity error", -1, 1 },
		{ F_HREQPERR, "PCI HMA channel request parity error", -1, 1 },
		{ F_HRSPPERR, "PCI HMA channel response parity error", -1, 1 },
		{ F_CFGSNPPERR, "PCI config snoop FIFO parity error", -1, 1 },
		{ F_FIDPERR, "PCI FID parity error", -1, 1 },
		{ F_INTXCLRPERR, "PCI INTx clear parity error", -1, 1 },
		{ F_MATAGPERR, "PCI MA tag parity error", -1, 1 },
		{ F_PIOTAGPERR, "PCI PIO tag parity error", -1, 1 },
		{ F_RXCPLPERR, "PCI Rx completion parity error", -1, 1 },
		{ F_RXWRPERR, "PCI Rx write parity error", -1, 1 },
		{ F_RPLPERR, "PCI replay buffer parity error", -1, 1 },
		{ F_PCIESINT, "PCI core secondary fault", -1, 1 },
		{ F_PCIEPINT, "PCI core primary fault", -1, 1 },
		{ F_UNXSPLCPLERR, "PCI unexpected split completion error", -1,
		  0 },
		{ 0 }
	};

	static const struct intr_info t5_pcie_intr_info[] = {
		{ F_MSTGRPPERR, "Master Response Read Queue parity error",
		  -1, 1 },
		{ F_MSTTIMEOUTPERR, "Master Timeout FIFO parity error", -1, 1 },
		{ F_MSIXSTIPERR, "MSI-X STI SRAM parity error", -1, 1 },
		{ F_MSIXADDRLPERR, "MSI-X AddrL parity error", -1, 1 },
		{ F_MSIXADDRHPERR, "MSI-X AddrH parity error", -1, 1 },
		{ F_MSIXDATAPERR, "MSI-X data parity error", -1, 1 },
		{ F_MSIXDIPERR, "MSI-X DI parity error", -1, 1 },
		{ F_PIOCPLGRPPERR, "PCI PIO completion Group FIFO parity error",
		  -1, 1 },
		{ F_PIOREQGRPPERR, "PCI PIO request Group FIFO parity error",
		  -1, 1 },
		{ F_TARTAGPERR, "PCI PCI target tag FIFO parity error", -1, 1 },
		{ F_MSTTAGQPERR, "PCI master tag queue parity error", -1, 1 },
		{ F_CREQPERR, "PCI CMD channel request parity error", -1, 1 },
		{ F_CRSPPERR, "PCI CMD channel response parity error", -1, 1 },
		{ F_DREQWRPERR, "PCI DMA channel write request parity error",
		  -1, 1 },
		{ F_DREQPERR, "PCI DMA channel request parity error", -1, 1 },
		{ F_DRSPPERR, "PCI DMA channel response parity error", -1, 1 },
		{ F_HREQWRPERR, "PCI HMA channel count parity error", -1, 1 },
		{ F_HREQPERR, "PCI HMA channel request parity error", -1, 1 },
		{ F_HRSPPERR, "PCI HMA channel response parity error", -1, 1 },
		{ F_CFGSNPPERR, "PCI config snoop FIFO parity error", -1, 1 },
		{ F_FIDPERR, "PCI FID parity error", -1, 1 },
		{ F_VFIDPERR, "PCI INTx clear parity error", -1, 1 },
		{ F_MAGRPPERR, "PCI MA group FIFO parity error", -1, 1 },
		{ F_PIOTAGPERR, "PCI PIO tag parity error", -1, 1 },
		{ F_IPRXHDRGRPPERR, "PCI IP Rx header group parity error",
		  -1, 1 },
		{ F_IPRXDATAGRPPERR, "PCI IP Rx data group parity error",
		  -1, 1 },
		{ F_RPLPERR, "PCI IP replay buffer parity error", -1, 1 },
		{ F_IPSOTPERR, "PCI IP SOT buffer parity error", -1, 1 },
		{ F_TRGT1GRPPERR, "PCI TRGT1 group FIFOs parity error", -1, 1 },
		{ F_READRSPERR, "Outbound read error", -1,
		  0 },
		{ 0 }
	};

	int fat;

	if (is_t4(adapter))
		fat = t4_handle_intr_status(adapter,
				A_PCIE_CORE_UTL_SYSTEM_BUS_AGENT_STATUS,
				sysbus_intr_info) +
			t4_handle_intr_status(adapter,
					A_PCIE_CORE_UTL_PCI_EXPRESS_PORT_STATUS,
					pcie_port_intr_info) +
			t4_handle_intr_status(adapter, A_PCIE_INT_CAUSE,
					      pcie_intr_info);
	else
		fat = t4_handle_intr_status(adapter, A_PCIE_INT_CAUSE,
					    t5_pcie_intr_info);
	if (fat)
		t4_fatal_err(adapter);
}

/*
 * TP interrupt handler.
 */
static void tp_intr_handler(struct adapter *adapter)
{
	static const struct intr_info tp_intr_info[] = {
		{ 0x3fffffff, "TP parity error", -1, 1 },
		{ F_FLMTXFLSTEMPTY, "TP out of Tx pages", -1, 1 },
		{ 0 }
	};

	if (t4_handle_intr_status(adapter, A_TP_INT_CAUSE, tp_intr_info))
		t4_fatal_err(adapter);
}

/*
 * SGE interrupt handler.
 */
static void sge_intr_handler(struct adapter *adapter)
{
	u64 v;
	u32 err;

	static const struct intr_info sge_intr_info[] = {
		{ F_ERR_CPL_EXCEED_IQE_SIZE,
		  "SGE received CPL exceeding IQE size", -1, 1 },
		{ F_ERR_INVALID_CIDX_INC,
		  "SGE GTS CIDX increment too large", -1, 0 },
		{ F_ERR_CPL_OPCODE_0, "SGE received 0-length CPL", -1, 0 },
		{ F_DBFIFO_LP_INT, NULL, -1, 0, t4_db_full },
		{ F_ERR_DATA_CPL_ON_HIGH_QID1 | F_ERR_DATA_CPL_ON_HIGH_QID0,
		  "SGE IQID > 1023 received CPL for FL", -1, 0 },
		{ F_ERR_BAD_DB_PIDX3, "SGE DBP 3 pidx increment too large", -1,
		  0 },
		{ F_ERR_BAD_DB_PIDX2, "SGE DBP 2 pidx increment too large", -1,
		  0 },
		{ F_ERR_BAD_DB_PIDX1, "SGE DBP 1 pidx increment too large", -1,
		  0 },
		{ F_ERR_BAD_DB_PIDX0, "SGE DBP 0 pidx increment too large", -1,
		  0 },
		{ F_ERR_ING_CTXT_PRIO,
		  "SGE too many priority ingress contexts", -1, 0 },
		{ F_INGRESS_SIZE_ERR, "SGE illegal ingress QID", -1, 0 },
		{ F_EGRESS_SIZE_ERR, "SGE illegal egress QID", -1, 0 },
		{ 0 }
	};

	static const struct intr_info t4t5_sge_intr_info[] = {
		{ F_ERR_DROPPED_DB, NULL, -1, 0, t4_db_dropped },
		{ F_DBFIFO_HP_INT, NULL, -1, 0, t4_db_full },
		{ F_ERR_EGR_CTXT_PRIO,
		  "SGE too many priority egress contexts", -1, 0 },
		{ 0 }
	};

	/*
 	* For now, treat below interrupts as fatal so that we disable SGE and
 	* get better debug */
	static const struct intr_info t6_sge_intr_info[] = {
		{ F_ERR_PCIE_ERROR0 | F_ERR_PCIE_ERROR1,
		  "SGE PCIe error for a DBP thread", -1, 1 },
		{ F_FATAL_WRE_LEN,
		  "SGE Actual WRE packet is less than advertized length",
		  -1, 1 },
		{ 0 }
	};

	v = (u64)t4_read_reg(adapter, A_SGE_INT_CAUSE1) |
		((u64)t4_read_reg(adapter, A_SGE_INT_CAUSE2) << 32);
	if (v) {
		CH_ALERT(adapter, "SGE parity error (%#llx)\n",
				(unsigned long long)v);
		t4_write_reg(adapter, A_SGE_INT_CAUSE1, v);
		t4_write_reg(adapter, A_SGE_INT_CAUSE2, v >> 32);
	}

	v |= t4_handle_intr_status(adapter, A_SGE_INT_CAUSE3, sge_intr_info);
	if (chip_id(adapter) <= CHELSIO_T5)
		v |= t4_handle_intr_status(adapter, A_SGE_INT_CAUSE3,
					   t4t5_sge_intr_info);
	else
		v |= t4_handle_intr_status(adapter, A_SGE_INT_CAUSE3,
					   t6_sge_intr_info);

	err = t4_read_reg(adapter, A_SGE_ERROR_STATS);
	if (err & F_ERROR_QID_VALID) {
		CH_ERR(adapter, "SGE error for queue %u\n", G_ERROR_QID(err));
		if (err & F_UNCAPTURED_ERROR)
			CH_ERR(adapter, "SGE UNCAPTURED_ERROR set (clearing)\n");
		t4_write_reg(adapter, A_SGE_ERROR_STATS, F_ERROR_QID_VALID |
			     F_UNCAPTURED_ERROR);
	}

	if (v != 0)
		t4_fatal_err(adapter);
}

#define CIM_OBQ_INTR (F_OBQULP0PARERR | F_OBQULP1PARERR | F_OBQULP2PARERR |\
		      F_OBQULP3PARERR | F_OBQSGEPARERR | F_OBQNCSIPARERR)
#define CIM_IBQ_INTR (F_IBQTP0PARERR | F_IBQTP1PARERR | F_IBQULPPARERR |\
		      F_IBQSGEHIPARERR | F_IBQSGELOPARERR | F_IBQNCSIPARERR)

/*
 * CIM interrupt handler.
 */
static void cim_intr_handler(struct adapter *adapter)
{
	static const struct intr_info cim_intr_info[] = {
		{ F_PREFDROPINT, "CIM control register prefetch drop", -1, 1 },
		{ CIM_OBQ_INTR, "CIM OBQ parity error", -1, 1 },
		{ CIM_IBQ_INTR, "CIM IBQ parity error", -1, 1 },
		{ F_MBUPPARERR, "CIM mailbox uP parity error", -1, 1 },
		{ F_MBHOSTPARERR, "CIM mailbox host parity error", -1, 1 },
		{ F_TIEQINPARERRINT, "CIM TIEQ outgoing parity error", -1, 1 },
		{ F_TIEQOUTPARERRINT, "CIM TIEQ incoming parity error", -1, 1 },
		{ 0 }
	};
	static const struct intr_info cim_upintr_info[] = {
		{ F_RSVDSPACEINT, "CIM reserved space access", -1, 1 },
		{ F_ILLTRANSINT, "CIM illegal transaction", -1, 1 },
		{ F_ILLWRINT, "CIM illegal write", -1, 1 },
		{ F_ILLRDINT, "CIM illegal read", -1, 1 },
		{ F_ILLRDBEINT, "CIM illegal read BE", -1, 1 },
		{ F_ILLWRBEINT, "CIM illegal write BE", -1, 1 },
		{ F_SGLRDBOOTINT, "CIM single read from boot space", -1, 1 },
		{ F_SGLWRBOOTINT, "CIM single write to boot space", -1, 1 },
		{ F_BLKWRBOOTINT, "CIM block write to boot space", -1, 1 },
		{ F_SGLRDFLASHINT, "CIM single read from flash space", -1, 1 },
		{ F_SGLWRFLASHINT, "CIM single write to flash space", -1, 1 },
		{ F_BLKWRFLASHINT, "CIM block write to flash space", -1, 1 },
		{ F_SGLRDEEPROMINT, "CIM single EEPROM read", -1, 1 },
		{ F_SGLWREEPROMINT, "CIM single EEPROM write", -1, 1 },
		{ F_BLKRDEEPROMINT, "CIM block EEPROM read", -1, 1 },
		{ F_BLKWREEPROMINT, "CIM block EEPROM write", -1, 1 },
		{ F_SGLRDCTLINT , "CIM single read from CTL space", -1, 1 },
		{ F_SGLWRCTLINT , "CIM single write to CTL space", -1, 1 },
		{ F_BLKRDCTLINT , "CIM block read from CTL space", -1, 1 },
		{ F_BLKWRCTLINT , "CIM block write to CTL space", -1, 1 },
		{ F_SGLRDPLINT , "CIM single read from PL space", -1, 1 },
		{ F_SGLWRPLINT , "CIM single write to PL space", -1, 1 },
		{ F_BLKRDPLINT , "CIM block read from PL space", -1, 1 },
		{ F_BLKWRPLINT , "CIM block write to PL space", -1, 1 },
		{ F_REQOVRLOOKUPINT , "CIM request FIFO overwrite", -1, 1 },
		{ F_RSPOVRLOOKUPINT , "CIM response FIFO overwrite", -1, 1 },
		{ F_TIMEOUTINT , "CIM PIF timeout", -1, 1 },
		{ F_TIMEOUTMAINT , "CIM PIF MA timeout", -1, 1 },
		{ 0 }
	};
	int fat;

	if (t4_read_reg(adapter, A_PCIE_FW) & F_PCIE_FW_ERR)
		t4_report_fw_error(adapter);

	fat = t4_handle_intr_status(adapter, A_CIM_HOST_INT_CAUSE,
				    cim_intr_info) +
	      t4_handle_intr_status(adapter, A_CIM_HOST_UPACC_INT_CAUSE,
				    cim_upintr_info);
	if (fat)
		t4_fatal_err(adapter);
}

/*
 * ULP RX interrupt handler.
 */
static void ulprx_intr_handler(struct adapter *adapter)
{
	static const struct intr_info ulprx_intr_info[] = {
		{ F_CAUSE_CTX_1, "ULPRX channel 1 context error", -1, 1 },
		{ F_CAUSE_CTX_0, "ULPRX channel 0 context error", -1, 1 },
		{ 0x7fffff, "ULPRX parity error", -1, 1 },
		{ 0 }
	};

	if (t4_handle_intr_status(adapter, A_ULP_RX_INT_CAUSE, ulprx_intr_info))
		t4_fatal_err(adapter);
}

/*
 * ULP TX interrupt handler.
 */
static void ulptx_intr_handler(struct adapter *adapter)
{
	static const struct intr_info ulptx_intr_info[] = {
		{ F_PBL_BOUND_ERR_CH3, "ULPTX channel 3 PBL out of bounds", -1,
		  0 },
		{ F_PBL_BOUND_ERR_CH2, "ULPTX channel 2 PBL out of bounds", -1,
		  0 },
		{ F_PBL_BOUND_ERR_CH1, "ULPTX channel 1 PBL out of bounds", -1,
		  0 },
		{ F_PBL_BOUND_ERR_CH0, "ULPTX channel 0 PBL out of bounds", -1,
		  0 },
		{ 0xfffffff, "ULPTX parity error", -1, 1 },
		{ 0 }
	};

	if (t4_handle_intr_status(adapter, A_ULP_TX_INT_CAUSE, ulptx_intr_info))
		t4_fatal_err(adapter);
}

/*
 * PM TX interrupt handler.
 */
static void pmtx_intr_handler(struct adapter *adapter)
{
	static const struct intr_info pmtx_intr_info[] = {
		{ F_PCMD_LEN_OVFL0, "PMTX channel 0 pcmd too large", -1, 1 },
		{ F_PCMD_LEN_OVFL1, "PMTX channel 1 pcmd too large", -1, 1 },
		{ F_PCMD_LEN_OVFL2, "PMTX channel 2 pcmd too large", -1, 1 },
		{ F_ZERO_C_CMD_ERROR, "PMTX 0-length pcmd", -1, 1 },
		{ 0xffffff0, "PMTX framing error", -1, 1 },
		{ F_OESPI_PAR_ERROR, "PMTX oespi parity error", -1, 1 },
		{ F_DB_OPTIONS_PAR_ERROR, "PMTX db_options parity error", -1,
		  1 },
		{ F_ICSPI_PAR_ERROR, "PMTX icspi parity error", -1, 1 },
		{ F_C_PCMD_PAR_ERROR, "PMTX c_pcmd parity error", -1, 1},
		{ 0 }
	};

	if (t4_handle_intr_status(adapter, A_PM_TX_INT_CAUSE, pmtx_intr_info))
		t4_fatal_err(adapter);
}

/*
 * PM RX interrupt handler.
 */
static void pmrx_intr_handler(struct adapter *adapter)
{
	static const struct intr_info pmrx_intr_info[] = {
		{ F_ZERO_E_CMD_ERROR, "PMRX 0-length pcmd", -1, 1 },
		{ 0x3ffff0, "PMRX framing error", -1, 1 },
		{ F_OCSPI_PAR_ERROR, "PMRX ocspi parity error", -1, 1 },
		{ F_DB_OPTIONS_PAR_ERROR, "PMRX db_options parity error", -1,
		  1 },
		{ F_IESPI_PAR_ERROR, "PMRX iespi parity error", -1, 1 },
		{ F_E_PCMD_PAR_ERROR, "PMRX e_pcmd parity error", -1, 1},
		{ 0 }
	};

	if (t4_handle_intr_status(adapter, A_PM_RX_INT_CAUSE, pmrx_intr_info))
		t4_fatal_err(adapter);
}

/*
 * CPL switch interrupt handler.
 */
static void cplsw_intr_handler(struct adapter *adapter)
{
	static const struct intr_info cplsw_intr_info[] = {
		{ F_CIM_OP_MAP_PERR, "CPLSW CIM op_map parity error", -1, 1 },
		{ F_CIM_OVFL_ERROR, "CPLSW CIM overflow", -1, 1 },
		{ F_TP_FRAMING_ERROR, "CPLSW TP framing error", -1, 1 },
		{ F_SGE_FRAMING_ERROR, "CPLSW SGE framing error", -1, 1 },
		{ F_CIM_FRAMING_ERROR, "CPLSW CIM framing error", -1, 1 },
		{ F_ZERO_SWITCH_ERROR, "CPLSW no-switch error", -1, 1 },
		{ 0 }
	};

	if (t4_handle_intr_status(adapter, A_CPL_INTR_CAUSE, cplsw_intr_info))
		t4_fatal_err(adapter);
}

/*
 * LE interrupt handler.
 */
static void le_intr_handler(struct adapter *adap)
{
	unsigned int chip_ver = chip_id(adap);
	static const struct intr_info le_intr_info[] = {
		{ F_LIPMISS, "LE LIP miss", -1, 0 },
		{ F_LIP0, "LE 0 LIP error", -1, 0 },
		{ F_PARITYERR, "LE parity error", -1, 1 },
		{ F_UNKNOWNCMD, "LE unknown command", -1, 1 },
		{ F_REQQPARERR, "LE request queue parity error", -1, 1 },
		{ 0 }
	};

	static const struct intr_info t6_le_intr_info[] = {
		{ F_T6_LIPMISS, "LE LIP miss", -1, 0 },
		{ F_T6_LIP0, "LE 0 LIP error", -1, 0 },
		{ F_TCAMINTPERR, "LE parity error", -1, 1 },
		{ F_T6_UNKNOWNCMD, "LE unknown command", -1, 1 },
		{ F_SSRAMINTPERR, "LE request queue parity error", -1, 1 },
		{ 0 }
	};

	if (t4_handle_intr_status(adap, A_LE_DB_INT_CAUSE,
				  (chip_ver <= CHELSIO_T5) ?
				  le_intr_info : t6_le_intr_info))
		t4_fatal_err(adap);
}

/*
 * MPS interrupt handler.
 */
static void mps_intr_handler(struct adapter *adapter)
{
	static const struct intr_info mps_rx_intr_info[] = {
		{ 0xffffff, "MPS Rx parity error", -1, 1 },
		{ 0 }
	};
	static const struct intr_info mps_tx_intr_info[] = {
		{ V_TPFIFO(M_TPFIFO), "MPS Tx TP FIFO parity error", -1, 1 },
		{ F_NCSIFIFO, "MPS Tx NC-SI FIFO parity error", -1, 1 },
		{ V_TXDATAFIFO(M_TXDATAFIFO), "MPS Tx data FIFO parity error",
		  -1, 1 },
		{ V_TXDESCFIFO(M_TXDESCFIFO), "MPS Tx desc FIFO parity error",
		  -1, 1 },
		{ F_BUBBLE, "MPS Tx underflow", -1, 1 },
		{ F_SECNTERR, "MPS Tx SOP/EOP error", -1, 1 },
		{ F_FRMERR, "MPS Tx framing error", -1, 1 },
		{ 0 }
	};
	static const struct intr_info mps_trc_intr_info[] = {
		{ V_FILTMEM(M_FILTMEM), "MPS TRC filter parity error", -1, 1 },
		{ V_PKTFIFO(M_PKTFIFO), "MPS TRC packet FIFO parity error", -1,
		  1 },
		{ F_MISCPERR, "MPS TRC misc parity error", -1, 1 },
		{ 0 }
	};
	static const struct intr_info mps_stat_sram_intr_info[] = {
		{ 0x1fffff, "MPS statistics SRAM parity error", -1, 1 },
		{ 0 }
	};
	static const struct intr_info mps_stat_tx_intr_info[] = {
		{ 0xfffff, "MPS statistics Tx FIFO parity error", -1, 1 },
		{ 0 }
	};
	static const struct intr_info mps_stat_rx_intr_info[] = {
		{ 0xffffff, "MPS statistics Rx FIFO parity error", -1, 1 },
		{ 0 }
	};
	static const struct intr_info mps_cls_intr_info[] = {
		{ F_MATCHSRAM, "MPS match SRAM parity error", -1, 1 },
		{ F_MATCHTCAM, "MPS match TCAM parity error", -1, 1 },
		{ F_HASHSRAM, "MPS hash SRAM parity error", -1, 1 },
		{ 0 }
	};

	int fat;

	fat = t4_handle_intr_status(adapter, A_MPS_RX_PERR_INT_CAUSE,
				    mps_rx_intr_info) +
	      t4_handle_intr_status(adapter, A_MPS_TX_INT_CAUSE,
				    mps_tx_intr_info) +
	      t4_handle_intr_status(adapter, A_MPS_TRC_INT_CAUSE,
				    mps_trc_intr_info) +
	      t4_handle_intr_status(adapter, A_MPS_STAT_PERR_INT_CAUSE_SRAM,
				    mps_stat_sram_intr_info) +
	      t4_handle_intr_status(adapter, A_MPS_STAT_PERR_INT_CAUSE_TX_FIFO,
				    mps_stat_tx_intr_info) +
	      t4_handle_intr_status(adapter, A_MPS_STAT_PERR_INT_CAUSE_RX_FIFO,
				    mps_stat_rx_intr_info) +
	      t4_handle_intr_status(adapter, A_MPS_CLS_INT_CAUSE,
				    mps_cls_intr_info);

	t4_write_reg(adapter, A_MPS_INT_CAUSE, 0);
	t4_read_reg(adapter, A_MPS_INT_CAUSE);	/* flush */
	if (fat)
		t4_fatal_err(adapter);
}

#define MEM_INT_MASK (F_PERR_INT_CAUSE | F_ECC_CE_INT_CAUSE | \
		      F_ECC_UE_INT_CAUSE)

/*
 * EDC/MC interrupt handler.
 */
static void mem_intr_handler(struct adapter *adapter, int idx)
{
	static const char name[4][7] = { "EDC0", "EDC1", "MC/MC0", "MC1" };

	unsigned int addr, cnt_addr, v;

	if (idx <= MEM_EDC1) {
		addr = EDC_REG(A_EDC_INT_CAUSE, idx);
		cnt_addr = EDC_REG(A_EDC_ECC_STATUS, idx);
	} else if (idx == MEM_MC) {
		if (is_t4(adapter)) {
			addr = A_MC_INT_CAUSE;
			cnt_addr = A_MC_ECC_STATUS;
		} else {
			addr = A_MC_P_INT_CAUSE;
			cnt_addr = A_MC_P_ECC_STATUS;
		}
	} else {
		addr = MC_REG(A_MC_P_INT_CAUSE, 1);
		cnt_addr = MC_REG(A_MC_P_ECC_STATUS, 1);
	}

	v = t4_read_reg(adapter, addr) & MEM_INT_MASK;
	if (v & F_PERR_INT_CAUSE)
		CH_ALERT(adapter, "%s FIFO parity error\n",
			  name[idx]);
	if (v & F_ECC_CE_INT_CAUSE) {
		u32 cnt = G_ECC_CECNT(t4_read_reg(adapter, cnt_addr));

		t4_edc_err_read(adapter, idx);

		t4_write_reg(adapter, cnt_addr, V_ECC_CECNT(M_ECC_CECNT));
		CH_WARN_RATELIMIT(adapter,
				  "%u %s correctable ECC data error%s\n",
				  cnt, name[idx], cnt > 1 ? "s" : "");
	}
	if (v & F_ECC_UE_INT_CAUSE)
		CH_ALERT(adapter,
			 "%s uncorrectable ECC data error\n", name[idx]);

	t4_write_reg(adapter, addr, v);
	if (v & (F_PERR_INT_CAUSE | F_ECC_UE_INT_CAUSE))
		t4_fatal_err(adapter);
}

/*
 * MA interrupt handler.
 */
static void ma_intr_handler(struct adapter *adapter)
{
	u32 v, status = t4_read_reg(adapter, A_MA_INT_CAUSE);

	if (status & F_MEM_PERR_INT_CAUSE) {
		CH_ALERT(adapter,
			  "MA parity error, parity status %#x\n",
			  t4_read_reg(adapter, A_MA_PARITY_ERROR_STATUS1));
		if (is_t5(adapter))
			CH_ALERT(adapter,
				  "MA parity error, parity status %#x\n",
				  t4_read_reg(adapter,
					      A_MA_PARITY_ERROR_STATUS2));
	}
	if (status & F_MEM_WRAP_INT_CAUSE) {
		v = t4_read_reg(adapter, A_MA_INT_WRAP_STATUS);
		CH_ALERT(adapter, "MA address wrap-around error by "
			  "client %u to address %#x\n",
			  G_MEM_WRAP_CLIENT_NUM(v),
			  G_MEM_WRAP_ADDRESS(v) << 4);
	}
	t4_write_reg(adapter, A_MA_INT_CAUSE, status);
	t4_fatal_err(adapter);
}

/*
 * SMB interrupt handler.
 */
static void smb_intr_handler(struct adapter *adap)
{
	static const struct intr_info smb_intr_info[] = {
		{ F_MSTTXFIFOPARINT, "SMB master Tx FIFO parity error", -1, 1 },
		{ F_MSTRXFIFOPARINT, "SMB master Rx FIFO parity error", -1, 1 },
		{ F_SLVFIFOPARINT, "SMB slave FIFO parity error", -1, 1 },
		{ 0 }
	};

	if (t4_handle_intr_status(adap, A_SMB_INT_CAUSE, smb_intr_info))
		t4_fatal_err(adap);
}

/*
 * NC-SI interrupt handler.
 */
static void ncsi_intr_handler(struct adapter *adap)
{
	static const struct intr_info ncsi_intr_info[] = {
		{ F_CIM_DM_PRTY_ERR, "NC-SI CIM parity error", -1, 1 },
		{ F_MPS_DM_PRTY_ERR, "NC-SI MPS parity error", -1, 1 },
		{ F_TXFIFO_PRTY_ERR, "NC-SI Tx FIFO parity error", -1, 1 },
		{ F_RXFIFO_PRTY_ERR, "NC-SI Rx FIFO parity error", -1, 1 },
		{ 0 }
	};

	if (t4_handle_intr_status(adap, A_NCSI_INT_CAUSE, ncsi_intr_info))
		t4_fatal_err(adap);
}

/*
 * XGMAC interrupt handler.
 */
static void xgmac_intr_handler(struct adapter *adap, int port)
{
	u32 v, int_cause_reg;

	if (is_t4(adap))
		int_cause_reg = PORT_REG(port, A_XGMAC_PORT_INT_CAUSE);
	else
		int_cause_reg = T5_PORT_REG(port, A_MAC_PORT_INT_CAUSE);

	v = t4_read_reg(adap, int_cause_reg);

	v &= (F_TXFIFO_PRTY_ERR | F_RXFIFO_PRTY_ERR);
	if (!v)
		return;

	if (v & F_TXFIFO_PRTY_ERR)
		CH_ALERT(adap, "XGMAC %d Tx FIFO parity error\n",
			  port);
	if (v & F_RXFIFO_PRTY_ERR)
		CH_ALERT(adap, "XGMAC %d Rx FIFO parity error\n",
			  port);
	t4_write_reg(adap, int_cause_reg, v);
	t4_fatal_err(adap);
}

/*
 * PL interrupt handler.
 */
static void pl_intr_handler(struct adapter *adap)
{
	static const struct intr_info pl_intr_info[] = {
		{ F_FATALPERR, "Fatal parity error", -1, 1 },
		{ F_PERRVFID, "PL VFID_MAP parity error", -1, 1 },
		{ 0 }
	};

	static const struct intr_info t5_pl_intr_info[] = {
		{ F_FATALPERR, "Fatal parity error", -1, 1 },
		{ 0 }
	};

	if (t4_handle_intr_status(adap, A_PL_PL_INT_CAUSE,
				  is_t4(adap) ?
				  pl_intr_info : t5_pl_intr_info))
		t4_fatal_err(adap);
}

#define PF_INTR_MASK (F_PFSW | F_PFCIM)

/**
 *	t4_slow_intr_handler - control path interrupt handler
 *	@adapter: the adapter
 *
 *	T4 interrupt handler for non-data global interrupt events, e.g., errors.
 *	The designation 'slow' is because it involves register reads, while
 *	data interrupts typically don't involve any MMIOs.
 */
int t4_slow_intr_handler(struct adapter *adapter)
{
	u32 cause = t4_read_reg(adapter, A_PL_INT_CAUSE);

	if (!(cause & GLBL_INTR_MASK))
		return 0;
	if (cause & F_CIM)
		cim_intr_handler(adapter);
	if (cause & F_MPS)
		mps_intr_handler(adapter);
	if (cause & F_NCSI)
		ncsi_intr_handler(adapter);
	if (cause & F_PL)
		pl_intr_handler(adapter);
	if (cause & F_SMB)
		smb_intr_handler(adapter);
	if (cause & F_MAC0)
		xgmac_intr_handler(adapter, 0);
	if (cause & F_MAC1)
		xgmac_intr_handler(adapter, 1);
	if (cause & F_MAC2)
		xgmac_intr_handler(adapter, 2);
	if (cause & F_MAC3)
		xgmac_intr_handler(adapter, 3);
	if (cause & F_PCIE)
		pcie_intr_handler(adapter);
	if (cause & F_MC0)
		mem_intr_handler(adapter, MEM_MC);
	if (is_t5(adapter) && (cause & F_MC1))
		mem_intr_handler(adapter, MEM_MC1);
	if (cause & F_EDC0)
		mem_intr_handler(adapter, MEM_EDC0);
	if (cause & F_EDC1)
		mem_intr_handler(adapter, MEM_EDC1);
	if (cause & F_LE)
		le_intr_handler(adapter);
	if (cause & F_TP)
		tp_intr_handler(adapter);
	if (cause & F_MA)
		ma_intr_handler(adapter);
	if (cause & F_PM_TX)
		pmtx_intr_handler(adapter);
	if (cause & F_PM_RX)
		pmrx_intr_handler(adapter);
	if (cause & F_ULP_RX)
		ulprx_intr_handler(adapter);
	if (cause & F_CPL_SWITCH)
		cplsw_intr_handler(adapter);
	if (cause & F_SGE)
		sge_intr_handler(adapter);
	if (cause & F_ULP_TX)
		ulptx_intr_handler(adapter);

	/* Clear the interrupts just processed for which we are the master. */
	t4_write_reg(adapter, A_PL_INT_CAUSE, cause & GLBL_INTR_MASK);
	(void)t4_read_reg(adapter, A_PL_INT_CAUSE); /* flush */
	return 1;
}

/**
 *	t4_intr_enable - enable interrupts
 *	@adapter: the adapter whose interrupts should be enabled
 *
 *	Enable PF-specific interrupts for the calling function and the top-level
 *	interrupt concentrator for global interrupts.  Interrupts are already
 *	enabled at each module,	here we just enable the roots of the interrupt
 *	hierarchies.
 *
 *	Note: this function should be called only when the driver manages
 *	non PF-specific interrupts from the various HW modules.  Only one PCI
 *	function at a time should be doing this.
 */
void t4_intr_enable(struct adapter *adapter)
{
	u32 val = 0;
	u32 whoami = t4_read_reg(adapter, A_PL_WHOAMI);
	u32 pf = (chip_id(adapter) <= CHELSIO_T5
		  ? G_SOURCEPF(whoami)
		  : G_T6_SOURCEPF(whoami));

	if (chip_id(adapter) <= CHELSIO_T5)
		val = F_ERR_DROPPED_DB | F_ERR_EGR_CTXT_PRIO | F_DBFIFO_HP_INT;
	else
		val = F_ERR_PCIE_ERROR0 | F_ERR_PCIE_ERROR1 | F_FATAL_WRE_LEN;
	t4_write_reg(adapter, A_SGE_INT_ENABLE3, F_ERR_CPL_EXCEED_IQE_SIZE |
		     F_ERR_INVALID_CIDX_INC | F_ERR_CPL_OPCODE_0 |
		     F_ERR_DATA_CPL_ON_HIGH_QID1 | F_INGRESS_SIZE_ERR |
		     F_ERR_DATA_CPL_ON_HIGH_QID0 | F_ERR_BAD_DB_PIDX3 |
		     F_ERR_BAD_DB_PIDX2 | F_ERR_BAD_DB_PIDX1 |
		     F_ERR_BAD_DB_PIDX0 | F_ERR_ING_CTXT_PRIO |
		     F_DBFIFO_LP_INT | F_EGRESS_SIZE_ERR | val);
	t4_write_reg(adapter, MYPF_REG(A_PL_PF_INT_ENABLE), PF_INTR_MASK);
	t4_set_reg_field(adapter, A_PL_INT_MAP0, 0, 1 << pf);
}

/**
 *	t4_intr_disable - disable interrupts
 *	@adapter: the adapter whose interrupts should be disabled
 *
 *	Disable interrupts.  We only disable the top-level interrupt
 *	concentrators.  The caller must be a PCI function managing global
 *	interrupts.
 */
void t4_intr_disable(struct adapter *adapter)
{
	u32 whoami = t4_read_reg(adapter, A_PL_WHOAMI);
	u32 pf = (chip_id(adapter) <= CHELSIO_T5
		  ? G_SOURCEPF(whoami)
		  : G_T6_SOURCEPF(whoami));

	t4_write_reg(adapter, MYPF_REG(A_PL_PF_INT_ENABLE), 0);
	t4_set_reg_field(adapter, A_PL_INT_MAP0, 1 << pf, 0);
}

/**
 *	t4_intr_clear - clear all interrupts
 *	@adapter: the adapter whose interrupts should be cleared
 *
 *	Clears all interrupts.  The caller must be a PCI function managing
 *	global interrupts.
 */
void t4_intr_clear(struct adapter *adapter)
{
	static const unsigned int cause_reg[] = {
		A_SGE_INT_CAUSE1, A_SGE_INT_CAUSE2, A_SGE_INT_CAUSE3,
		A_PCIE_NONFAT_ERR, A_PCIE_INT_CAUSE,
		A_MA_INT_WRAP_STATUS, A_MA_PARITY_ERROR_STATUS1, A_MA_INT_CAUSE,
		A_EDC_INT_CAUSE, EDC_REG(A_EDC_INT_CAUSE, 1),
		A_CIM_HOST_INT_CAUSE, A_CIM_HOST_UPACC_INT_CAUSE,
		MYPF_REG(A_CIM_PF_HOST_INT_CAUSE),
		A_TP_INT_CAUSE,
		A_ULP_RX_INT_CAUSE, A_ULP_TX_INT_CAUSE,
		A_PM_RX_INT_CAUSE, A_PM_TX_INT_CAUSE,
		A_MPS_RX_PERR_INT_CAUSE,
		A_CPL_INTR_CAUSE,
		MYPF_REG(A_PL_PF_INT_CAUSE),
		A_PL_PL_INT_CAUSE,
		A_LE_DB_INT_CAUSE,
	};

	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(cause_reg); ++i)
		t4_write_reg(adapter, cause_reg[i], 0xffffffff);

	t4_write_reg(adapter, is_t4(adapter) ? A_MC_INT_CAUSE :
				A_MC_P_INT_CAUSE, 0xffffffff);

	if (is_t4(adapter)) {
		t4_write_reg(adapter, A_PCIE_CORE_UTL_SYSTEM_BUS_AGENT_STATUS,
				0xffffffff);
		t4_write_reg(adapter, A_PCIE_CORE_UTL_PCI_EXPRESS_PORT_STATUS,
				0xffffffff);
	} else
		t4_write_reg(adapter, A_MA_PARITY_ERROR_STATUS2, 0xffffffff);

	t4_write_reg(adapter, A_PL_INT_CAUSE, GLBL_INTR_MASK);
	(void) t4_read_reg(adapter, A_PL_INT_CAUSE);          /* flush */
}

/**
 *	hash_mac_addr - return the hash value of a MAC address
 *	@addr: the 48-bit Ethernet MAC address
 *
 *	Hashes a MAC address according to the hash function used by HW inexact
 *	(hash) address matching.
 */
static int hash_mac_addr(const u8 *addr)
{
	u32 a = ((u32)addr[0] << 16) | ((u32)addr[1] << 8) | addr[2];
	u32 b = ((u32)addr[3] << 16) | ((u32)addr[4] << 8) | addr[5];
	a ^= b;
	a ^= (a >> 12);
	a ^= (a >> 6);
	return a & 0x3f;
}

/**
 *	t4_config_rss_range - configure a portion of the RSS mapping table
 *	@adapter: the adapter
 *	@mbox: mbox to use for the FW command
 *	@viid: virtual interface whose RSS subtable is to be written
 *	@start: start entry in the table to write
 *	@n: how many table entries to write
 *	@rspq: values for the "response queue" (Ingress Queue) lookup table
 *	@nrspq: number of values in @rspq
 *
 *	Programs the selected part of the VI's RSS mapping table with the
 *	provided values.  If @nrspq < @n the supplied values are used repeatedly
 *	until the full table range is populated.
 *
 *	The caller must ensure the values in @rspq are in the range allowed for
 *	@viid.
 */
int t4_config_rss_range(struct adapter *adapter, int mbox, unsigned int viid,
			int start, int n, const u16 *rspq, unsigned int nrspq)
{
	int ret;
	const u16 *rsp = rspq;
	const u16 *rsp_end = rspq + nrspq;
	struct fw_rss_ind_tbl_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_RSS_IND_TBL_CMD) |
				     F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				     V_FW_RSS_IND_TBL_CMD_VIID(viid));
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));

	/*
	 * Each firmware RSS command can accommodate up to 32 RSS Ingress
	 * Queue Identifiers.  These Ingress Queue IDs are packed three to
	 * a 32-bit word as 10-bit values with the upper remaining 2 bits
	 * reserved.
	 */
	while (n > 0) {
		int nq = min(n, 32);
		int nq_packed = 0;
		__be32 *qp = &cmd.iq0_to_iq2;

		/*
		 * Set up the firmware RSS command header to send the next
		 * "nq" Ingress Queue IDs to the firmware.
		 */
		cmd.niqid = cpu_to_be16(nq);
		cmd.startidx = cpu_to_be16(start);

		/*
		 * "nq" more done for the start of the next loop.
		 */
		start += nq;
		n -= nq;

		/*
		 * While there are still Ingress Queue IDs to stuff into the
		 * current firmware RSS command, retrieve them from the
		 * Ingress Queue ID array and insert them into the command.
		 */
		while (nq > 0) {
			/*
			 * Grab up to the next 3 Ingress Queue IDs (wrapping
			 * around the Ingress Queue ID array if necessary) and
			 * insert them into the firmware RSS command at the
			 * current 3-tuple position within the commad.
			 */
			u16 qbuf[3];
			u16 *qbp = qbuf;
			int nqbuf = min(3, nq);

			nq -= nqbuf;
			qbuf[0] = qbuf[1] = qbuf[2] = 0;
			while (nqbuf && nq_packed < 32) {
				nqbuf--;
				nq_packed++;
				*qbp++ = *rsp++;
				if (rsp >= rsp_end)
					rsp = rspq;
			}
			*qp++ = cpu_to_be32(V_FW_RSS_IND_TBL_CMD_IQ0(qbuf[0]) |
					    V_FW_RSS_IND_TBL_CMD_IQ1(qbuf[1]) |
					    V_FW_RSS_IND_TBL_CMD_IQ2(qbuf[2]));
		}

		/*
		 * Send this portion of the RRS table update to the firmware;
		 * bail out on any errors.
		 */
		ret = t4_wr_mbox(adapter, mbox, &cmd, sizeof(cmd), NULL);
		if (ret)
			return ret;
	}
	return 0;
}

/**
 *	t4_config_glbl_rss - configure the global RSS mode
 *	@adapter: the adapter
 *	@mbox: mbox to use for the FW command
 *	@mode: global RSS mode
 *	@flags: mode-specific flags
 *
 *	Sets the global RSS mode.
 */
int t4_config_glbl_rss(struct adapter *adapter, int mbox, unsigned int mode,
		       unsigned int flags)
{
	struct fw_rss_glb_config_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_RSS_GLB_CONFIG_CMD) |
				    F_FW_CMD_REQUEST | F_FW_CMD_WRITE);
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));
	if (mode == FW_RSS_GLB_CONFIG_CMD_MODE_MANUAL) {
		c.u.manual.mode_pkd =
			cpu_to_be32(V_FW_RSS_GLB_CONFIG_CMD_MODE(mode));
	} else if (mode == FW_RSS_GLB_CONFIG_CMD_MODE_BASICVIRTUAL) {
		c.u.basicvirtual.mode_pkd =
			cpu_to_be32(V_FW_RSS_GLB_CONFIG_CMD_MODE(mode));
		c.u.basicvirtual.synmapen_to_hashtoeplitz = cpu_to_be32(flags);
	} else
		return -EINVAL;
	return t4_wr_mbox(adapter, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_config_vi_rss - configure per VI RSS settings
 *	@adapter: the adapter
 *	@mbox: mbox to use for the FW command
 *	@viid: the VI id
 *	@flags: RSS flags
 *	@defq: id of the default RSS queue for the VI.
 *
 *	Configures VI-specific RSS properties.
 */
int t4_config_vi_rss(struct adapter *adapter, int mbox, unsigned int viid,
		     unsigned int flags, unsigned int defq)
{
	struct fw_rss_vi_config_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_RSS_VI_CONFIG_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				   V_FW_RSS_VI_CONFIG_CMD_VIID(viid));
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));
	c.u.basicvirtual.defaultq_to_udpen = cpu_to_be32(flags |
					V_FW_RSS_VI_CONFIG_CMD_DEFAULTQ(defq));
	return t4_wr_mbox(adapter, mbox, &c, sizeof(c), NULL);
}

/* Read an RSS table row */
static int rd_rss_row(struct adapter *adap, int row, u32 *val)
{
	t4_write_reg(adap, A_TP_RSS_LKP_TABLE, 0xfff00000 | row);
	return t4_wait_op_done_val(adap, A_TP_RSS_LKP_TABLE, F_LKPTBLROWVLD, 1,
				   5, 0, val);
}

/**
 *	t4_read_rss - read the contents of the RSS mapping table
 *	@adapter: the adapter
 *	@map: holds the contents of the RSS mapping table
 *
 *	Reads the contents of the RSS hash->queue mapping table.
 */
int t4_read_rss(struct adapter *adapter, u16 *map)
{
	u32 val;
	int i, ret;

	for (i = 0; i < RSS_NENTRIES / 2; ++i) {
		ret = rd_rss_row(adapter, i, &val);
		if (ret)
			return ret;
		*map++ = G_LKPTBLQUEUE0(val);
		*map++ = G_LKPTBLQUEUE1(val);
	}
	return 0;
}

/**
 *	t4_fw_tp_pio_rw - Access TP PIO through LDST
 *	@adap: the adapter
 *	@vals: where the indirect register values are stored/written
 *	@nregs: how many indirect registers to read/write
 *	@start_idx: index of first indirect register to read/write
 *	@rw: Read (1) or Write (0)
 *
 *	Access TP PIO registers through LDST
 */
void t4_fw_tp_pio_rw(struct adapter *adap, u32 *vals, unsigned int nregs,
		     unsigned int start_index, unsigned int rw)
{
	int ret, i;
	int cmd = FW_LDST_ADDRSPC_TP_PIO;
	struct fw_ldst_cmd c;

	for (i = 0 ; i < nregs; i++) {
		memset(&c, 0, sizeof(c));
		c.op_to_addrspace = cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
						F_FW_CMD_REQUEST |
						(rw ? F_FW_CMD_READ :
						     F_FW_CMD_WRITE) |
						V_FW_LDST_CMD_ADDRSPACE(cmd));
		c.cycles_to_len16 = cpu_to_be32(FW_LEN16(c));

		c.u.addrval.addr = cpu_to_be32(start_index + i);
		c.u.addrval.val  = rw ? 0 : cpu_to_be32(vals[i]);
		ret = t4_wr_mbox(adap, adap->mbox, &c, sizeof(c), &c);
		if (ret == 0) {
			if (rw)
				vals[i] = be32_to_cpu(c.u.addrval.val);
		}
	}
}

/**
 *	t4_read_rss_key - read the global RSS key
 *	@adap: the adapter
 *	@key: 10-entry array holding the 320-bit RSS key
 *
 *	Reads the global 320-bit RSS key.
 */
void t4_read_rss_key(struct adapter *adap, u32 *key)
{
	if (t4_use_ldst(adap))
		t4_fw_tp_pio_rw(adap, key, 10, A_TP_RSS_SECRET_KEY0, 1);
	else
		t4_read_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA, key, 10,
				 A_TP_RSS_SECRET_KEY0);
}

/**
 *	t4_write_rss_key - program one of the RSS keys
 *	@adap: the adapter
 *	@key: 10-entry array holding the 320-bit RSS key
 *	@idx: which RSS key to write
 *
 *	Writes one of the RSS keys with the given 320-bit value.  If @idx is
 *	0..15 the corresponding entry in the RSS key table is written,
 *	otherwise the global RSS key is written.
 */
void t4_write_rss_key(struct adapter *adap, u32 *key, int idx)
{
	u8 rss_key_addr_cnt = 16;
	u32 vrt = t4_read_reg(adap, A_TP_RSS_CONFIG_VRT);

	/*
	 * T6 and later: for KeyMode 3 (per-vf and per-vf scramble),
	 * allows access to key addresses 16-63 by using KeyWrAddrX
	 * as index[5:4](upper 2) into key table
	 */
	if ((chip_id(adap) > CHELSIO_T5) &&
	    (vrt & F_KEYEXTEND) && (G_KEYMODE(vrt) == 3))
		rss_key_addr_cnt = 32;

	if (t4_use_ldst(adap))
		t4_fw_tp_pio_rw(adap, key, 10, A_TP_RSS_SECRET_KEY0, 0);
	else
		t4_write_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA, key, 10,
				  A_TP_RSS_SECRET_KEY0);

	if (idx >= 0 && idx < rss_key_addr_cnt) {
		if (rss_key_addr_cnt > 16)
			t4_write_reg(adap, A_TP_RSS_CONFIG_VRT,
				     V_KEYWRADDRX(idx >> 4) |
				     V_T6_VFWRADDR(idx) | F_KEYWREN);
		else
			t4_write_reg(adap, A_TP_RSS_CONFIG_VRT,
				     V_KEYWRADDR(idx) | F_KEYWREN);
	}
}

/**
 *	t4_read_rss_pf_config - read PF RSS Configuration Table
 *	@adapter: the adapter
 *	@index: the entry in the PF RSS table to read
 *	@valp: where to store the returned value
 *
 *	Reads the PF RSS Configuration Table at the specified index and returns
 *	the value found there.
 */
void t4_read_rss_pf_config(struct adapter *adapter, unsigned int index,
			   u32 *valp)
{
	if (t4_use_ldst(adapter))
		t4_fw_tp_pio_rw(adapter, valp, 1,
				A_TP_RSS_PF0_CONFIG + index, 1);
	else
		t4_read_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				 valp, 1, A_TP_RSS_PF0_CONFIG + index);
}

/**
 *	t4_write_rss_pf_config - write PF RSS Configuration Table
 *	@adapter: the adapter
 *	@index: the entry in the VF RSS table to read
 *	@val: the value to store
 *
 *	Writes the PF RSS Configuration Table at the specified index with the
 *	specified value.
 */
void t4_write_rss_pf_config(struct adapter *adapter, unsigned int index,
			    u32 val)
{
	if (t4_use_ldst(adapter))
		t4_fw_tp_pio_rw(adapter, &val, 1,
				A_TP_RSS_PF0_CONFIG + index, 0);
	else
		t4_write_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				  &val, 1, A_TP_RSS_PF0_CONFIG + index);
}

/**
 *	t4_read_rss_vf_config - read VF RSS Configuration Table
 *	@adapter: the adapter
 *	@index: the entry in the VF RSS table to read
 *	@vfl: where to store the returned VFL
 *	@vfh: where to store the returned VFH
 *
 *	Reads the VF RSS Configuration Table at the specified index and returns
 *	the (VFL, VFH) values found there.
 */
void t4_read_rss_vf_config(struct adapter *adapter, unsigned int index,
			   u32 *vfl, u32 *vfh)
{
	u32 vrt, mask, data;

	if (chip_id(adapter) <= CHELSIO_T5) {
		mask = V_VFWRADDR(M_VFWRADDR);
		data = V_VFWRADDR(index);
	} else {
		 mask =  V_T6_VFWRADDR(M_T6_VFWRADDR);
		 data = V_T6_VFWRADDR(index);
	}
	/*
	 * Request that the index'th VF Table values be read into VFL/VFH.
	 */
	vrt = t4_read_reg(adapter, A_TP_RSS_CONFIG_VRT);
	vrt &= ~(F_VFRDRG | F_VFWREN | F_KEYWREN | mask);
	vrt |= data | F_VFRDEN;
	t4_write_reg(adapter, A_TP_RSS_CONFIG_VRT, vrt);

	/*
	 * Grab the VFL/VFH values ...
	 */
	if (t4_use_ldst(adapter)) {
		t4_fw_tp_pio_rw(adapter, vfl, 1, A_TP_RSS_VFL_CONFIG, 1);
		t4_fw_tp_pio_rw(adapter, vfh, 1, A_TP_RSS_VFH_CONFIG, 1);
	} else {
		t4_read_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				 vfl, 1, A_TP_RSS_VFL_CONFIG);
		t4_read_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				 vfh, 1, A_TP_RSS_VFH_CONFIG);
	}
}

/**
 *	t4_write_rss_vf_config - write VF RSS Configuration Table
 *
 *	@adapter: the adapter
 *	@index: the entry in the VF RSS table to write
 *	@vfl: the VFL to store
 *	@vfh: the VFH to store
 *
 *	Writes the VF RSS Configuration Table at the specified index with the
 *	specified (VFL, VFH) values.
 */
void t4_write_rss_vf_config(struct adapter *adapter, unsigned int index,
			    u32 vfl, u32 vfh)
{
	u32 vrt, mask, data;

	if (chip_id(adapter) <= CHELSIO_T5) {
		mask = V_VFWRADDR(M_VFWRADDR);
		data = V_VFWRADDR(index);
	} else {
		mask =  V_T6_VFWRADDR(M_T6_VFWRADDR);
		data = V_T6_VFWRADDR(index);
	}

	/*
	 * Load up VFL/VFH with the values to be written ...
	 */
	if (t4_use_ldst(adapter)) {
		t4_fw_tp_pio_rw(adapter, &vfl, 1, A_TP_RSS_VFL_CONFIG, 0);
		t4_fw_tp_pio_rw(adapter, &vfh, 1, A_TP_RSS_VFH_CONFIG, 0);
	} else {
		t4_write_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				  &vfl, 1, A_TP_RSS_VFL_CONFIG);
		t4_write_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				  &vfh, 1, A_TP_RSS_VFH_CONFIG);
	}

	/*
	 * Write the VFL/VFH into the VF Table at index'th location.
	 */
	vrt = t4_read_reg(adapter, A_TP_RSS_CONFIG_VRT);
	vrt &= ~(F_VFRDRG | F_VFWREN | F_KEYWREN | mask);
	vrt |= data | F_VFRDEN;
	t4_write_reg(adapter, A_TP_RSS_CONFIG_VRT, vrt);
}

/**
 *	t4_read_rss_pf_map - read PF RSS Map
 *	@adapter: the adapter
 *
 *	Reads the PF RSS Map register and returns its value.
 */
u32 t4_read_rss_pf_map(struct adapter *adapter)
{
	u32 pfmap;

	if (t4_use_ldst(adapter))
		t4_fw_tp_pio_rw(adapter, &pfmap, 1, A_TP_RSS_PF_MAP, 1);
	else
		t4_read_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				 &pfmap, 1, A_TP_RSS_PF_MAP);
	return pfmap;
}

/**
 *	t4_write_rss_pf_map - write PF RSS Map
 *	@adapter: the adapter
 *	@pfmap: PF RSS Map value
 *
 *	Writes the specified value to the PF RSS Map register.
 */
void t4_write_rss_pf_map(struct adapter *adapter, u32 pfmap)
{
	if (t4_use_ldst(adapter))
		t4_fw_tp_pio_rw(adapter, &pfmap, 1, A_TP_RSS_PF_MAP, 0);
	else
		t4_write_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				  &pfmap, 1, A_TP_RSS_PF_MAP);
}

/**
 *	t4_read_rss_pf_mask - read PF RSS Mask
 *	@adapter: the adapter
 *
 *	Reads the PF RSS Mask register and returns its value.
 */
u32 t4_read_rss_pf_mask(struct adapter *adapter)
{
	u32 pfmask;

	if (t4_use_ldst(adapter))
		t4_fw_tp_pio_rw(adapter, &pfmask, 1, A_TP_RSS_PF_MSK, 1);
	else
		t4_read_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				 &pfmask, 1, A_TP_RSS_PF_MSK);
	return pfmask;
}

/**
 *	t4_write_rss_pf_mask - write PF RSS Mask
 *	@adapter: the adapter
 *	@pfmask: PF RSS Mask value
 *
 *	Writes the specified value to the PF RSS Mask register.
 */
void t4_write_rss_pf_mask(struct adapter *adapter, u32 pfmask)
{
	if (t4_use_ldst(adapter))
		t4_fw_tp_pio_rw(adapter, &pfmask, 1, A_TP_RSS_PF_MSK, 0);
	else
		t4_write_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				  &pfmask, 1, A_TP_RSS_PF_MSK);
}

/**
 *	t4_tp_get_tcp_stats - read TP's TCP MIB counters
 *	@adap: the adapter
 *	@v4: holds the TCP/IP counter values
 *	@v6: holds the TCP/IPv6 counter values
 *
 *	Returns the values of TP's TCP/IP and TCP/IPv6 MIB counters.
 *	Either @v4 or @v6 may be %NULL to skip the corresponding stats.
 */
void t4_tp_get_tcp_stats(struct adapter *adap, struct tp_tcp_stats *v4,
			 struct tp_tcp_stats *v6)
{
	u32 val[A_TP_MIB_TCP_RXT_SEG_LO - A_TP_MIB_TCP_OUT_RST + 1];

#define STAT_IDX(x) ((A_TP_MIB_TCP_##x) - A_TP_MIB_TCP_OUT_RST)
#define STAT(x)     val[STAT_IDX(x)]
#define STAT64(x)   (((u64)STAT(x##_HI) << 32) | STAT(x##_LO))

	if (v4) {
		t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, val,
				 ARRAY_SIZE(val), A_TP_MIB_TCP_OUT_RST);
		v4->tcp_out_rsts = STAT(OUT_RST);
		v4->tcp_in_segs  = STAT64(IN_SEG);
		v4->tcp_out_segs = STAT64(OUT_SEG);
		v4->tcp_retrans_segs = STAT64(RXT_SEG);
	}
	if (v6) {
		t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, val,
				 ARRAY_SIZE(val), A_TP_MIB_TCP_V6OUT_RST);
		v6->tcp_out_rsts = STAT(OUT_RST);
		v6->tcp_in_segs  = STAT64(IN_SEG);
		v6->tcp_out_segs = STAT64(OUT_SEG);
		v6->tcp_retrans_segs = STAT64(RXT_SEG);
	}
#undef STAT64
#undef STAT
#undef STAT_IDX
}

/**
 *	t4_tp_get_err_stats - read TP's error MIB counters
 *	@adap: the adapter
 *	@st: holds the counter values
 *
 *	Returns the values of TP's error counters.
 */
void t4_tp_get_err_stats(struct adapter *adap, struct tp_err_stats *st)
{
	int nchan = adap->chip_params->nchan;

	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA,
			st->mac_in_errs, nchan, A_TP_MIB_MAC_IN_ERR_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA,
			st->hdr_in_errs, nchan, A_TP_MIB_HDR_IN_ERR_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA,
			st->tcp_in_errs, nchan, A_TP_MIB_TCP_IN_ERR_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA,
			st->tnl_cong_drops, nchan, A_TP_MIB_TNL_CNG_DROP_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA,
			st->ofld_chan_drops, nchan, A_TP_MIB_OFD_CHN_DROP_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA,
			st->tnl_tx_drops, nchan, A_TP_MIB_TNL_DROP_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA,
			st->ofld_vlan_drops, nchan, A_TP_MIB_OFD_VLN_DROP_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA,
			st->tcp6_in_errs, nchan, A_TP_MIB_TCP_V6IN_ERR_0);

	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA,
			 &st->ofld_no_neigh, 2, A_TP_MIB_OFD_ARP_DROP);
}

/**
 *	t4_tp_get_proxy_stats - read TP's proxy MIB counters
 *	@adap: the adapter
 *	@st: holds the counter values
 *
 *	Returns the values of TP's proxy counters.
 */
void t4_tp_get_proxy_stats(struct adapter *adap, struct tp_proxy_stats *st)
{
	int nchan = adap->chip_params->nchan;

	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, st->proxy,
			 nchan, A_TP_MIB_TNL_LPBK_0);
}

/**
 *	t4_tp_get_cpl_stats - read TP's CPL MIB counters
 *	@adap: the adapter
 *	@st: holds the counter values
 *
 *	Returns the values of TP's CPL counters.
 */
void t4_tp_get_cpl_stats(struct adapter *adap, struct tp_cpl_stats *st)
{
	int nchan = adap->chip_params->nchan;

	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, st->req,
			 nchan, A_TP_MIB_CPL_IN_REQ_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, st->rsp,
			 nchan, A_TP_MIB_CPL_OUT_RSP_0);
}

/**
 *	t4_tp_get_rdma_stats - read TP's RDMA MIB counters
 *	@adap: the adapter
 *	@st: holds the counter values
 *
 *	Returns the values of TP's RDMA counters.
 */
void t4_tp_get_rdma_stats(struct adapter *adap, struct tp_rdma_stats *st)
{
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, &st->rqe_dfr_pkt,
			 2, A_TP_MIB_RQE_DFR_PKT);
}

/**
 *	t4_get_fcoe_stats - read TP's FCoE MIB counters for a port
 *	@adap: the adapter
 *	@idx: the port index
 *	@st: holds the counter values
 *
 *	Returns the values of TP's FCoE counters for the selected port.
 */
void t4_get_fcoe_stats(struct adapter *adap, unsigned int idx,
		       struct tp_fcoe_stats *st)
{
	u32 val[2];

	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, &st->frames_ddp,
			 1, A_TP_MIB_FCOE_DDP_0 + idx);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, &st->frames_drop,
			 1, A_TP_MIB_FCOE_DROP_0 + idx);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, val,
			 2, A_TP_MIB_FCOE_BYTE_0_HI + 2 * idx);
	st->octets_ddp = ((u64)val[0] << 32) | val[1];
}

/**
 *	t4_get_usm_stats - read TP's non-TCP DDP MIB counters
 *	@adap: the adapter
 *	@st: holds the counter values
 *
 *	Returns the values of TP's counters for non-TCP directly-placed packets.
 */
void t4_get_usm_stats(struct adapter *adap, struct tp_usm_stats *st)
{
	u32 val[4];

	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, val, 4,
			 A_TP_MIB_USM_PKTS);
	st->frames = val[0];
	st->drops = val[1];
	st->octets = ((u64)val[2] << 32) | val[3];
}

/**
 *	t4_read_mtu_tbl - returns the values in the HW path MTU table
 *	@adap: the adapter
 *	@mtus: where to store the MTU values
 *	@mtu_log: where to store the MTU base-2 log (may be %NULL)
 *
 *	Reads the HW path MTU table.
 */
void t4_read_mtu_tbl(struct adapter *adap, u16 *mtus, u8 *mtu_log)
{
	u32 v;
	int i;

	for (i = 0; i < NMTUS; ++i) {
		t4_write_reg(adap, A_TP_MTU_TABLE,
			     V_MTUINDEX(0xff) | V_MTUVALUE(i));
		v = t4_read_reg(adap, A_TP_MTU_TABLE);
		mtus[i] = G_MTUVALUE(v);
		if (mtu_log)
			mtu_log[i] = G_MTUWIDTH(v);
	}
}

/**
 *	t4_read_cong_tbl - reads the congestion control table
 *	@adap: the adapter
 *	@incr: where to store the alpha values
 *
 *	Reads the additive increments programmed into the HW congestion
 *	control table.
 */
void t4_read_cong_tbl(struct adapter *adap, u16 incr[NMTUS][NCCTRL_WIN])
{
	unsigned int mtu, w;

	for (mtu = 0; mtu < NMTUS; ++mtu)
		for (w = 0; w < NCCTRL_WIN; ++w) {
			t4_write_reg(adap, A_TP_CCTRL_TABLE,
				     V_ROWINDEX(0xffff) | (mtu << 5) | w);
			incr[mtu][w] = (u16)t4_read_reg(adap,
						A_TP_CCTRL_TABLE) & 0x1fff;
		}
}

/**
 *	t4_tp_wr_bits_indirect - set/clear bits in an indirect TP register
 *	@adap: the adapter
 *	@addr: the indirect TP register address
 *	@mask: specifies the field within the register to modify
 *	@val: new value for the field
 *
 *	Sets a field of an indirect TP register to the given value.
 */
void t4_tp_wr_bits_indirect(struct adapter *adap, unsigned int addr,
			    unsigned int mask, unsigned int val)
{
	t4_write_reg(adap, A_TP_PIO_ADDR, addr);
	val |= t4_read_reg(adap, A_TP_PIO_DATA) & ~mask;
	t4_write_reg(adap, A_TP_PIO_DATA, val);
}

/**
 *	init_cong_ctrl - initialize congestion control parameters
 *	@a: the alpha values for congestion control
 *	@b: the beta values for congestion control
 *
 *	Initialize the congestion control parameters.
 */
static void init_cong_ctrl(unsigned short *a, unsigned short *b)
{
	a[0] = a[1] = a[2] = a[3] = a[4] = a[5] = a[6] = a[7] = a[8] = 1;
	a[9] = 2;
	a[10] = 3;
	a[11] = 4;
	a[12] = 5;
	a[13] = 6;
	a[14] = 7;
	a[15] = 8;
	a[16] = 9;
	a[17] = 10;
	a[18] = 14;
	a[19] = 17;
	a[20] = 21;
	a[21] = 25;
	a[22] = 30;
	a[23] = 35;
	a[24] = 45;
	a[25] = 60;
	a[26] = 80;
	a[27] = 100;
	a[28] = 200;
	a[29] = 300;
	a[30] = 400;
	a[31] = 500;

	b[0] = b[1] = b[2] = b[3] = b[4] = b[5] = b[6] = b[7] = b[8] = 0;
	b[9] = b[10] = 1;
	b[11] = b[12] = 2;
	b[13] = b[14] = b[15] = b[16] = 3;
	b[17] = b[18] = b[19] = b[20] = b[21] = 4;
	b[22] = b[23] = b[24] = b[25] = b[26] = b[27] = 5;
	b[28] = b[29] = 6;
	b[30] = b[31] = 7;
}

/* The minimum additive increment value for the congestion control table */
#define CC_MIN_INCR 2U

/**
 *	t4_load_mtus - write the MTU and congestion control HW tables
 *	@adap: the adapter
 *	@mtus: the values for the MTU table
 *	@alpha: the values for the congestion control alpha parameter
 *	@beta: the values for the congestion control beta parameter
 *
 *	Write the HW MTU table with the supplied MTUs and the high-speed
 *	congestion control table with the supplied alpha, beta, and MTUs.
 *	We write the two tables together because the additive increments
 *	depend on the MTUs.
 */
void t4_load_mtus(struct adapter *adap, const unsigned short *mtus,
		  const unsigned short *alpha, const unsigned short *beta)
{
	static const unsigned int avg_pkts[NCCTRL_WIN] = {
		2, 6, 10, 14, 20, 28, 40, 56, 80, 112, 160, 224, 320, 448, 640,
		896, 1281, 1792, 2560, 3584, 5120, 7168, 10240, 14336, 20480,
		28672, 40960, 57344, 81920, 114688, 163840, 229376
	};

	unsigned int i, w;

	for (i = 0; i < NMTUS; ++i) {
		unsigned int mtu = mtus[i];
		unsigned int log2 = fls(mtu);

		if (!(mtu & ((1 << log2) >> 2)))     /* round */
			log2--;
		t4_write_reg(adap, A_TP_MTU_TABLE, V_MTUINDEX(i) |
			     V_MTUWIDTH(log2) | V_MTUVALUE(mtu));

		for (w = 0; w < NCCTRL_WIN; ++w) {
			unsigned int inc;

			inc = max(((mtu - 40) * alpha[w]) / avg_pkts[w],
				  CC_MIN_INCR);

			t4_write_reg(adap, A_TP_CCTRL_TABLE, (i << 21) |
				     (w << 16) | (beta[w] << 13) | inc);
		}
	}
}

/**
 *	t4_set_pace_tbl - set the pace table
 *	@adap: the adapter
 *	@pace_vals: the pace values in microseconds
 *	@start: index of the first entry in the HW pace table to set
 *	@n: how many entries to set
 *
 *	Sets (a subset of the) HW pace table.
 */
int t4_set_pace_tbl(struct adapter *adap, const unsigned int *pace_vals,
		     unsigned int start, unsigned int n)
{
	unsigned int vals[NTX_SCHED], i;
	unsigned int tick_ns = dack_ticks_to_usec(adap, 1000);

	if (n > NTX_SCHED)
	    return -ERANGE;

	/* convert values from us to dack ticks, rounding to closest value */
	for (i = 0; i < n; i++, pace_vals++) {
		vals[i] = (1000 * *pace_vals + tick_ns / 2) / tick_ns;
		if (vals[i] > 0x7ff)
			return -ERANGE;
		if (*pace_vals && vals[i] == 0)
			return -ERANGE;
	}
	for (i = 0; i < n; i++, start++)
		t4_write_reg(adap, A_TP_PACE_TABLE, (start << 16) | vals[i]);
	return 0;
}

/**
 *	t4_set_sched_bps - set the bit rate for a HW traffic scheduler
 *	@adap: the adapter
 *	@kbps: target rate in Kbps
 *	@sched: the scheduler index
 *
 *	Configure a Tx HW scheduler for the target rate.
 */
int t4_set_sched_bps(struct adapter *adap, int sched, unsigned int kbps)
{
	unsigned int v, tps, cpt, bpt, delta, mindelta = ~0;
	unsigned int clk = adap->params.vpd.cclk * 1000;
	unsigned int selected_cpt = 0, selected_bpt = 0;

	if (kbps > 0) {
		kbps *= 125;     /* -> bytes */
		for (cpt = 1; cpt <= 255; cpt++) {
			tps = clk / cpt;
			bpt = (kbps + tps / 2) / tps;
			if (bpt > 0 && bpt <= 255) {
				v = bpt * tps;
				delta = v >= kbps ? v - kbps : kbps - v;
				if (delta < mindelta) {
					mindelta = delta;
					selected_cpt = cpt;
					selected_bpt = bpt;
				}
			} else if (selected_cpt)
				break;
		}
		if (!selected_cpt)
			return -EINVAL;
	}
	t4_write_reg(adap, A_TP_TM_PIO_ADDR,
		     A_TP_TX_MOD_Q1_Q0_RATE_LIMIT - sched / 2);
	v = t4_read_reg(adap, A_TP_TM_PIO_DATA);
	if (sched & 1)
		v = (v & 0xffff) | (selected_cpt << 16) | (selected_bpt << 24);
	else
		v = (v & 0xffff0000) | selected_cpt | (selected_bpt << 8);
	t4_write_reg(adap, A_TP_TM_PIO_DATA, v);
	return 0;
}

/**
 *	t4_set_sched_ipg - set the IPG for a Tx HW packet rate scheduler
 *	@adap: the adapter
 *	@sched: the scheduler index
 *	@ipg: the interpacket delay in tenths of nanoseconds
 *
 *	Set the interpacket delay for a HW packet rate scheduler.
 */
int t4_set_sched_ipg(struct adapter *adap, int sched, unsigned int ipg)
{
	unsigned int v, addr = A_TP_TX_MOD_Q1_Q0_TIMER_SEPARATOR - sched / 2;

	/* convert ipg to nearest number of core clocks */
	ipg *= core_ticks_per_usec(adap);
	ipg = (ipg + 5000) / 10000;
	if (ipg > M_TXTIMERSEPQ0)
		return -EINVAL;

	t4_write_reg(adap, A_TP_TM_PIO_ADDR, addr);
	v = t4_read_reg(adap, A_TP_TM_PIO_DATA);
	if (sched & 1)
		v = (v & V_TXTIMERSEPQ0(M_TXTIMERSEPQ0)) | V_TXTIMERSEPQ1(ipg);
	else
		v = (v & V_TXTIMERSEPQ1(M_TXTIMERSEPQ1)) | V_TXTIMERSEPQ0(ipg);
	t4_write_reg(adap, A_TP_TM_PIO_DATA, v);
	t4_read_reg(adap, A_TP_TM_PIO_DATA);
	return 0;
}

/*
 * Calculates a rate in bytes/s given the number of 256-byte units per 4K core
 * clocks.  The formula is
 *
 * bytes/s = bytes256 * 256 * ClkFreq / 4096
 *
 * which is equivalent to
 *
 * bytes/s = 62.5 * bytes256 * ClkFreq_ms
 */
static u64 chan_rate(struct adapter *adap, unsigned int bytes256)
{
	u64 v = bytes256 * adap->params.vpd.cclk;

	return v * 62 + v / 2;
}

/**
 *	t4_get_chan_txrate - get the current per channel Tx rates
 *	@adap: the adapter
 *	@nic_rate: rates for NIC traffic
 *	@ofld_rate: rates for offloaded traffic
 *
 *	Return the current Tx rates in bytes/s for NIC and offloaded traffic
 *	for each channel.
 */
void t4_get_chan_txrate(struct adapter *adap, u64 *nic_rate, u64 *ofld_rate)
{
	u32 v;

	v = t4_read_reg(adap, A_TP_TX_TRATE);
	nic_rate[0] = chan_rate(adap, G_TNLRATE0(v));
	nic_rate[1] = chan_rate(adap, G_TNLRATE1(v));
	if (adap->chip_params->nchan > 2) {
		nic_rate[2] = chan_rate(adap, G_TNLRATE2(v));
		nic_rate[3] = chan_rate(adap, G_TNLRATE3(v));
	}

	v = t4_read_reg(adap, A_TP_TX_ORATE);
	ofld_rate[0] = chan_rate(adap, G_OFDRATE0(v));
	ofld_rate[1] = chan_rate(adap, G_OFDRATE1(v));
	if (adap->chip_params->nchan > 2) {
		ofld_rate[2] = chan_rate(adap, G_OFDRATE2(v));
		ofld_rate[3] = chan_rate(adap, G_OFDRATE3(v));
	}
}

/**
 *	t4_set_trace_filter - configure one of the tracing filters
 *	@adap: the adapter
 *	@tp: the desired trace filter parameters
 *	@idx: which filter to configure
 *	@enable: whether to enable or disable the filter
 *
 *	Configures one of the tracing filters available in HW.  If @tp is %NULL
 *	it indicates that the filter is already written in the register and it
 *	just needs to be enabled or disabled.
 */
int t4_set_trace_filter(struct adapter *adap, const struct trace_params *tp,
    int idx, int enable)
{
	int i, ofst = idx * 4;
	u32 data_reg, mask_reg, cfg;
	u32 multitrc = F_TRCMULTIFILTER;
	u32 en = is_t4(adap) ? F_TFEN : F_T5_TFEN;

	if (idx < 0 || idx >= NTRACE)
		return -EINVAL;

	if (tp == NULL || !enable) {
		t4_set_reg_field(adap, A_MPS_TRC_FILTER_MATCH_CTL_A + ofst, en,
		    enable ? en : 0);
		return 0;
	}

	/*
	 * TODO - After T4 data book is updated, specify the exact
	 * section below.
	 *
	 * See T4 data book - MPS section for a complete description
	 * of the below if..else handling of A_MPS_TRC_CFG register
	 * value.
	 */
	cfg = t4_read_reg(adap, A_MPS_TRC_CFG);
	if (cfg & F_TRCMULTIFILTER) {
		/*
		 * If multiple tracers are enabled, then maximum
		 * capture size is 2.5KB (FIFO size of a single channel)
		 * minus 2 flits for CPL_TRACE_PKT header.
		 */
		if (tp->snap_len > ((10 * 1024 / 4) - (2 * 8)))
			return -EINVAL;
	} else {
		/*
		 * If multiple tracers are disabled, to avoid deadlocks
		 * maximum packet capture size of 9600 bytes is recommended.
		 * Also in this mode, only trace0 can be enabled and running.
		 */
		multitrc = 0;
		if (tp->snap_len > 9600 || idx)
			return -EINVAL;
	}

	if (tp->port > (is_t4(adap) ? 11 : 19) || tp->invert > 1 ||
	    tp->skip_len > M_TFLENGTH || tp->skip_ofst > M_TFOFFSET ||
	    tp->min_len > M_TFMINPKTSIZE)
		return -EINVAL;

	/* stop the tracer we'll be changing */
	t4_set_reg_field(adap, A_MPS_TRC_FILTER_MATCH_CTL_A + ofst, en, 0);

	idx *= (A_MPS_TRC_FILTER1_MATCH - A_MPS_TRC_FILTER0_MATCH);
	data_reg = A_MPS_TRC_FILTER0_MATCH + idx;
	mask_reg = A_MPS_TRC_FILTER0_DONT_CARE + idx;

	for (i = 0; i < TRACE_LEN / 4; i++, data_reg += 4, mask_reg += 4) {
		t4_write_reg(adap, data_reg, tp->data[i]);
		t4_write_reg(adap, mask_reg, ~tp->mask[i]);
	}
	t4_write_reg(adap, A_MPS_TRC_FILTER_MATCH_CTL_B + ofst,
		     V_TFCAPTUREMAX(tp->snap_len) |
		     V_TFMINPKTSIZE(tp->min_len));
	t4_write_reg(adap, A_MPS_TRC_FILTER_MATCH_CTL_A + ofst,
		     V_TFOFFSET(tp->skip_ofst) | V_TFLENGTH(tp->skip_len) | en |
		     (is_t4(adap) ?
		     V_TFPORT(tp->port) | V_TFINVERTMATCH(tp->invert) :
		     V_T5_TFPORT(tp->port) | V_T5_TFINVERTMATCH(tp->invert)));

	return 0;
}

/**
 *	t4_get_trace_filter - query one of the tracing filters
 *	@adap: the adapter
 *	@tp: the current trace filter parameters
 *	@idx: which trace filter to query
 *	@enabled: non-zero if the filter is enabled
 *
 *	Returns the current settings of one of the HW tracing filters.
 */
void t4_get_trace_filter(struct adapter *adap, struct trace_params *tp, int idx,
			 int *enabled)
{
	u32 ctla, ctlb;
	int i, ofst = idx * 4;
	u32 data_reg, mask_reg;

	ctla = t4_read_reg(adap, A_MPS_TRC_FILTER_MATCH_CTL_A + ofst);
	ctlb = t4_read_reg(adap, A_MPS_TRC_FILTER_MATCH_CTL_B + ofst);

	if (is_t4(adap)) {
		*enabled = !!(ctla & F_TFEN);
		tp->port =  G_TFPORT(ctla);
		tp->invert = !!(ctla & F_TFINVERTMATCH);
	} else {
		*enabled = !!(ctla & F_T5_TFEN);
		tp->port = G_T5_TFPORT(ctla);
		tp->invert = !!(ctla & F_T5_TFINVERTMATCH);
	}
	tp->snap_len = G_TFCAPTUREMAX(ctlb);
	tp->min_len = G_TFMINPKTSIZE(ctlb);
	tp->skip_ofst = G_TFOFFSET(ctla);
	tp->skip_len = G_TFLENGTH(ctla);

	ofst = (A_MPS_TRC_FILTER1_MATCH - A_MPS_TRC_FILTER0_MATCH) * idx;
	data_reg = A_MPS_TRC_FILTER0_MATCH + ofst;
	mask_reg = A_MPS_TRC_FILTER0_DONT_CARE + ofst;

	for (i = 0; i < TRACE_LEN / 4; i++, data_reg += 4, mask_reg += 4) {
		tp->mask[i] = ~t4_read_reg(adap, mask_reg);
		tp->data[i] = t4_read_reg(adap, data_reg) & tp->mask[i];
	}
}

/**
 *	t4_pmtx_get_stats - returns the HW stats from PMTX
 *	@adap: the adapter
 *	@cnt: where to store the count statistics
 *	@cycles: where to store the cycle statistics
 *
 *	Returns performance statistics from PMTX.
 */
void t4_pmtx_get_stats(struct adapter *adap, u32 cnt[], u64 cycles[])
{
	int i;
	u32 data[2];

	for (i = 0; i < adap->chip_params->pm_stats_cnt; i++) {
		t4_write_reg(adap, A_PM_TX_STAT_CONFIG, i + 1);
		cnt[i] = t4_read_reg(adap, A_PM_TX_STAT_COUNT);
		if (is_t4(adap))
			cycles[i] = t4_read_reg64(adap, A_PM_TX_STAT_LSB);
		else {
			t4_read_indirect(adap, A_PM_TX_DBG_CTRL,
					 A_PM_TX_DBG_DATA, data, 2,
					 A_PM_TX_DBG_STAT_MSB);
			cycles[i] = (((u64)data[0] << 32) | data[1]);
		}
	}
}

/**
 *	t4_pmrx_get_stats - returns the HW stats from PMRX
 *	@adap: the adapter
 *	@cnt: where to store the count statistics
 *	@cycles: where to store the cycle statistics
 *
 *	Returns performance statistics from PMRX.
 */
void t4_pmrx_get_stats(struct adapter *adap, u32 cnt[], u64 cycles[])
{
	int i;
	u32 data[2];

	for (i = 0; i < adap->chip_params->pm_stats_cnt; i++) {
		t4_write_reg(adap, A_PM_RX_STAT_CONFIG, i + 1);
		cnt[i] = t4_read_reg(adap, A_PM_RX_STAT_COUNT);
		if (is_t4(adap)) {
			cycles[i] = t4_read_reg64(adap, A_PM_RX_STAT_LSB);
		} else {
			t4_read_indirect(adap, A_PM_RX_DBG_CTRL,
					 A_PM_RX_DBG_DATA, data, 2,
					 A_PM_RX_DBG_STAT_MSB);
			cycles[i] = (((u64)data[0] << 32) | data[1]);
		}
	}
}

/**
 *	t4_get_mps_bg_map - return the buffer groups associated with a port
 *	@adap: the adapter
 *	@idx: the port index
 *
 *	Returns a bitmap indicating which MPS buffer groups are associated
 *	with the given port.  Bit i is set if buffer group i is used by the
 *	port.
 */
static unsigned int t4_get_mps_bg_map(struct adapter *adap, int idx)
{
	u32 n = G_NUMPORTS(t4_read_reg(adap, A_MPS_CMN_CTL));

	if (n == 0)
		return idx == 0 ? 0xf : 0;
	if (n == 1 && chip_id(adap) <= CHELSIO_T5)
		return idx < 2 ? (3 << (2 * idx)) : 0;
	return 1 << idx;
}

/**
 *      t4_get_port_type_description - return Port Type string description
 *      @port_type: firmware Port Type enumeration
 */
const char *t4_get_port_type_description(enum fw_port_type port_type)
{
	static const char *const port_type_description[] = {
		"Fiber_XFI",
		"Fiber_XAUI",
		"BT_SGMII",
		"BT_XFI",
		"BT_XAUI",
		"KX4",
		"CX4",
		"KX",
		"KR",
		"SFP",
		"BP_AP",
		"BP4_AP",
		"QSFP_10G",
		"QSA",
		"QSFP",
		"BP40_BA",
	};

	if (port_type < ARRAY_SIZE(port_type_description))
		return port_type_description[port_type];
	return "UNKNOWN";
}

/**
 *      t4_get_port_stats_offset - collect port stats relative to a previous
 *				   snapshot
 *      @adap: The adapter
 *      @idx: The port
 *      @stats: Current stats to fill
 *      @offset: Previous stats snapshot
 */
void t4_get_port_stats_offset(struct adapter *adap, int idx,
		struct port_stats *stats,
		struct port_stats *offset)
{
	u64 *s, *o;
	int i;

	t4_get_port_stats(adap, idx, stats);
	for (i = 0, s = (u64 *)stats, o = (u64 *)offset ;
			i < (sizeof(struct port_stats)/sizeof(u64)) ;
			i++, s++, o++)
		*s -= *o;
}

/**
 *	t4_get_port_stats - collect port statistics
 *	@adap: the adapter
 *	@idx: the port index
 *	@p: the stats structure to fill
 *
 *	Collect statistics related to the given port from HW.
 */
void t4_get_port_stats(struct adapter *adap, int idx, struct port_stats *p)
{
	u32 bgmap = t4_get_mps_bg_map(adap, idx);
	u32 stat_ctl;

#define GET_STAT(name) \
	t4_read_reg64(adap, \
	(is_t4(adap) ? PORT_REG(idx, A_MPS_PORT_STAT_##name##_L) : \
	T5_PORT_REG(idx, A_MPS_PORT_STAT_##name##_L)))
#define GET_STAT_COM(name) t4_read_reg64(adap, A_MPS_STAT_##name##_L)

	stat_ctl = t4_read_reg(adap, A_MPS_STAT_CTL);

	p->tx_pause		= GET_STAT(TX_PORT_PAUSE);
	p->tx_octets		= GET_STAT(TX_PORT_BYTES);
	p->tx_frames		= GET_STAT(TX_PORT_FRAMES);
	p->tx_bcast_frames	= GET_STAT(TX_PORT_BCAST);
	p->tx_mcast_frames	= GET_STAT(TX_PORT_MCAST);
	p->tx_ucast_frames	= GET_STAT(TX_PORT_UCAST);
	p->tx_error_frames	= GET_STAT(TX_PORT_ERROR);
	p->tx_frames_64		= GET_STAT(TX_PORT_64B);
	p->tx_frames_65_127	= GET_STAT(TX_PORT_65B_127B);
	p->tx_frames_128_255	= GET_STAT(TX_PORT_128B_255B);
	p->tx_frames_256_511	= GET_STAT(TX_PORT_256B_511B);
	p->tx_frames_512_1023	= GET_STAT(TX_PORT_512B_1023B);
	p->tx_frames_1024_1518	= GET_STAT(TX_PORT_1024B_1518B);
	p->tx_frames_1519_max	= GET_STAT(TX_PORT_1519B_MAX);
	p->tx_drop		= GET_STAT(TX_PORT_DROP);
	p->tx_ppp0		= GET_STAT(TX_PORT_PPP0);
	p->tx_ppp1		= GET_STAT(TX_PORT_PPP1);
	p->tx_ppp2		= GET_STAT(TX_PORT_PPP2);
	p->tx_ppp3		= GET_STAT(TX_PORT_PPP3);
	p->tx_ppp4		= GET_STAT(TX_PORT_PPP4);
	p->tx_ppp5		= GET_STAT(TX_PORT_PPP5);
	p->tx_ppp6		= GET_STAT(TX_PORT_PPP6);
	p->tx_ppp7		= GET_STAT(TX_PORT_PPP7);

	if (stat_ctl & F_COUNTPAUSESTATTX) {
		p->tx_frames -= p->tx_pause;
		p->tx_octets -= p->tx_pause * 64;
		p->tx_mcast_frames -= p->tx_pause;
	}

	p->rx_pause		= GET_STAT(RX_PORT_PAUSE);
	p->rx_octets		= GET_STAT(RX_PORT_BYTES);
	p->rx_frames		= GET_STAT(RX_PORT_FRAMES);
	p->rx_bcast_frames	= GET_STAT(RX_PORT_BCAST);
	p->rx_mcast_frames	= GET_STAT(RX_PORT_MCAST);
	p->rx_ucast_frames	= GET_STAT(RX_PORT_UCAST);
	p->rx_too_long		= GET_STAT(RX_PORT_MTU_ERROR);
	p->rx_jabber		= GET_STAT(RX_PORT_MTU_CRC_ERROR);
	p->rx_fcs_err		= GET_STAT(RX_PORT_CRC_ERROR);
	p->rx_len_err		= GET_STAT(RX_PORT_LEN_ERROR);
	p->rx_symbol_err	= GET_STAT(RX_PORT_SYM_ERROR);
	p->rx_runt		= GET_STAT(RX_PORT_LESS_64B);
	p->rx_frames_64		= GET_STAT(RX_PORT_64B);
	p->rx_frames_65_127	= GET_STAT(RX_PORT_65B_127B);
	p->rx_frames_128_255	= GET_STAT(RX_PORT_128B_255B);
	p->rx_frames_256_511	= GET_STAT(RX_PORT_256B_511B);
	p->rx_frames_512_1023	= GET_STAT(RX_PORT_512B_1023B);
	p->rx_frames_1024_1518	= GET_STAT(RX_PORT_1024B_1518B);
	p->rx_frames_1519_max	= GET_STAT(RX_PORT_1519B_MAX);
	p->rx_ppp0		= GET_STAT(RX_PORT_PPP0);
	p->rx_ppp1		= GET_STAT(RX_PORT_PPP1);
	p->rx_ppp2		= GET_STAT(RX_PORT_PPP2);
	p->rx_ppp3		= GET_STAT(RX_PORT_PPP3);
	p->rx_ppp4		= GET_STAT(RX_PORT_PPP4);
	p->rx_ppp5		= GET_STAT(RX_PORT_PPP5);
	p->rx_ppp6		= GET_STAT(RX_PORT_PPP6);
	p->rx_ppp7		= GET_STAT(RX_PORT_PPP7);

	if (stat_ctl & F_COUNTPAUSESTATRX) {
		p->rx_frames -= p->rx_pause;
		p->rx_octets -= p->rx_pause * 64;
		p->rx_mcast_frames -= p->rx_pause;
	}

	p->rx_ovflow0 = (bgmap & 1) ? GET_STAT_COM(RX_BG_0_MAC_DROP_FRAME) : 0;
	p->rx_ovflow1 = (bgmap & 2) ? GET_STAT_COM(RX_BG_1_MAC_DROP_FRAME) : 0;
	p->rx_ovflow2 = (bgmap & 4) ? GET_STAT_COM(RX_BG_2_MAC_DROP_FRAME) : 0;
	p->rx_ovflow3 = (bgmap & 8) ? GET_STAT_COM(RX_BG_3_MAC_DROP_FRAME) : 0;
	p->rx_trunc0 = (bgmap & 1) ? GET_STAT_COM(RX_BG_0_MAC_TRUNC_FRAME) : 0;
	p->rx_trunc1 = (bgmap & 2) ? GET_STAT_COM(RX_BG_1_MAC_TRUNC_FRAME) : 0;
	p->rx_trunc2 = (bgmap & 4) ? GET_STAT_COM(RX_BG_2_MAC_TRUNC_FRAME) : 0;
	p->rx_trunc3 = (bgmap & 8) ? GET_STAT_COM(RX_BG_3_MAC_TRUNC_FRAME) : 0;

#undef GET_STAT
#undef GET_STAT_COM
}

/**
 *	t4_get_lb_stats - collect loopback port statistics
 *	@adap: the adapter
 *	@idx: the loopback port index
 *	@p: the stats structure to fill
 *
 *	Return HW statistics for the given loopback port.
 */
void t4_get_lb_stats(struct adapter *adap, int idx, struct lb_port_stats *p)
{
	u32 bgmap = t4_get_mps_bg_map(adap, idx);

#define GET_STAT(name) \
	t4_read_reg64(adap, \
	(is_t4(adap) ? \
	PORT_REG(idx, A_MPS_PORT_STAT_LB_PORT_##name##_L) : \
	T5_PORT_REG(idx, A_MPS_PORT_STAT_LB_PORT_##name##_L)))
#define GET_STAT_COM(name) t4_read_reg64(adap, A_MPS_STAT_##name##_L)

	p->octets	= GET_STAT(BYTES);
	p->frames	= GET_STAT(FRAMES);
	p->bcast_frames	= GET_STAT(BCAST);
	p->mcast_frames	= GET_STAT(MCAST);
	p->ucast_frames	= GET_STAT(UCAST);
	p->error_frames	= GET_STAT(ERROR);

	p->frames_64		= GET_STAT(64B);
	p->frames_65_127	= GET_STAT(65B_127B);
	p->frames_128_255	= GET_STAT(128B_255B);
	p->frames_256_511	= GET_STAT(256B_511B);
	p->frames_512_1023	= GET_STAT(512B_1023B);
	p->frames_1024_1518	= GET_STAT(1024B_1518B);
	p->frames_1519_max	= GET_STAT(1519B_MAX);
	p->drop			= GET_STAT(DROP_FRAMES);

	p->ovflow0 = (bgmap & 1) ? GET_STAT_COM(RX_BG_0_LB_DROP_FRAME) : 0;
	p->ovflow1 = (bgmap & 2) ? GET_STAT_COM(RX_BG_1_LB_DROP_FRAME) : 0;
	p->ovflow2 = (bgmap & 4) ? GET_STAT_COM(RX_BG_2_LB_DROP_FRAME) : 0;
	p->ovflow3 = (bgmap & 8) ? GET_STAT_COM(RX_BG_3_LB_DROP_FRAME) : 0;
	p->trunc0 = (bgmap & 1) ? GET_STAT_COM(RX_BG_0_LB_TRUNC_FRAME) : 0;
	p->trunc1 = (bgmap & 2) ? GET_STAT_COM(RX_BG_1_LB_TRUNC_FRAME) : 0;
	p->trunc2 = (bgmap & 4) ? GET_STAT_COM(RX_BG_2_LB_TRUNC_FRAME) : 0;
	p->trunc3 = (bgmap & 8) ? GET_STAT_COM(RX_BG_3_LB_TRUNC_FRAME) : 0;

#undef GET_STAT
#undef GET_STAT_COM
}

/**
 *	t4_wol_magic_enable - enable/disable magic packet WoL
 *	@adap: the adapter
 *	@port: the physical port index
 *	@addr: MAC address expected in magic packets, %NULL to disable
 *
 *	Enables/disables magic packet wake-on-LAN for the selected port.
 */
void t4_wol_magic_enable(struct adapter *adap, unsigned int port,
			 const u8 *addr)
{
	u32 mag_id_reg_l, mag_id_reg_h, port_cfg_reg;

	if (is_t4(adap)) {
		mag_id_reg_l = PORT_REG(port, A_XGMAC_PORT_MAGIC_MACID_LO);
		mag_id_reg_h = PORT_REG(port, A_XGMAC_PORT_MAGIC_MACID_HI);
		port_cfg_reg = PORT_REG(port, A_XGMAC_PORT_CFG2);
	} else {
		mag_id_reg_l = T5_PORT_REG(port, A_MAC_PORT_MAGIC_MACID_LO);
		mag_id_reg_h = T5_PORT_REG(port, A_MAC_PORT_MAGIC_MACID_HI);
		port_cfg_reg = T5_PORT_REG(port, A_MAC_PORT_CFG2);
	}

	if (addr) {
		t4_write_reg(adap, mag_id_reg_l,
			     (addr[2] << 24) | (addr[3] << 16) |
			     (addr[4] << 8) | addr[5]);
		t4_write_reg(adap, mag_id_reg_h,
			     (addr[0] << 8) | addr[1]);
	}
	t4_set_reg_field(adap, port_cfg_reg, F_MAGICEN,
			 V_MAGICEN(addr != NULL));
}

/**
 *	t4_wol_pat_enable - enable/disable pattern-based WoL
 *	@adap: the adapter
 *	@port: the physical port index
 *	@map: bitmap of which HW pattern filters to set
 *	@mask0: byte mask for bytes 0-63 of a packet
 *	@mask1: byte mask for bytes 64-127 of a packet
 *	@crc: Ethernet CRC for selected bytes
 *	@enable: enable/disable switch
 *
 *	Sets the pattern filters indicated in @map to mask out the bytes
 *	specified in @mask0/@mask1 in received packets and compare the CRC of
 *	the resulting packet against @crc.  If @enable is %true pattern-based
 *	WoL is enabled, otherwise disabled.
 */
int t4_wol_pat_enable(struct adapter *adap, unsigned int port, unsigned int map,
		      u64 mask0, u64 mask1, unsigned int crc, bool enable)
{
	int i;
	u32 port_cfg_reg;

	if (is_t4(adap))
		port_cfg_reg = PORT_REG(port, A_XGMAC_PORT_CFG2);
	else
		port_cfg_reg = T5_PORT_REG(port, A_MAC_PORT_CFG2);

	if (!enable) {
		t4_set_reg_field(adap, port_cfg_reg, F_PATEN, 0);
		return 0;
	}
	if (map > 0xff)
		return -EINVAL;

#define EPIO_REG(name) \
	(is_t4(adap) ? PORT_REG(port, A_XGMAC_PORT_EPIO_##name) : \
	T5_PORT_REG(port, A_MAC_PORT_EPIO_##name))

	t4_write_reg(adap, EPIO_REG(DATA1), mask0 >> 32);
	t4_write_reg(adap, EPIO_REG(DATA2), mask1);
	t4_write_reg(adap, EPIO_REG(DATA3), mask1 >> 32);

	for (i = 0; i < NWOL_PAT; i++, map >>= 1) {
		if (!(map & 1))
			continue;

		/* write byte masks */
		t4_write_reg(adap, EPIO_REG(DATA0), mask0);
		t4_write_reg(adap, EPIO_REG(OP), V_ADDRESS(i) | F_EPIOWR);
		t4_read_reg(adap, EPIO_REG(OP));                /* flush */
		if (t4_read_reg(adap, EPIO_REG(OP)) & F_BUSY)
			return -ETIMEDOUT;

		/* write CRC */
		t4_write_reg(adap, EPIO_REG(DATA0), crc);
		t4_write_reg(adap, EPIO_REG(OP), V_ADDRESS(i + 32) | F_EPIOWR);
		t4_read_reg(adap, EPIO_REG(OP));                /* flush */
		if (t4_read_reg(adap, EPIO_REG(OP)) & F_BUSY)
			return -ETIMEDOUT;
	}
#undef EPIO_REG

	t4_set_reg_field(adap, port_cfg_reg, 0, F_PATEN);
	return 0;
}

/*     t4_mk_filtdelwr - create a delete filter WR
 *     @ftid: the filter ID
 *     @wr: the filter work request to populate
 *     @qid: ingress queue to receive the delete notification
 *
 *     Creates a filter work request to delete the supplied filter.  If @qid is
 *     negative the delete notification is suppressed.
 */
void t4_mk_filtdelwr(unsigned int ftid, struct fw_filter_wr *wr, int qid)
{
	memset(wr, 0, sizeof(*wr));
	wr->op_pkd = cpu_to_be32(V_FW_WR_OP(FW_FILTER_WR));
	wr->len16_pkd = cpu_to_be32(V_FW_WR_LEN16(sizeof(*wr) / 16));
	wr->tid_to_iq = cpu_to_be32(V_FW_FILTER_WR_TID(ftid) |
				    V_FW_FILTER_WR_NOREPLY(qid < 0));
	wr->del_filter_to_l2tix = cpu_to_be32(F_FW_FILTER_WR_DEL_FILTER);
	if (qid >= 0)
		wr->rx_chan_rx_rpl_iq =
				cpu_to_be16(V_FW_FILTER_WR_RX_RPL_IQ(qid));
}

#define INIT_CMD(var, cmd, rd_wr) do { \
	(var).op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_##cmd##_CMD) | \
					F_FW_CMD_REQUEST | \
					F_FW_CMD_##rd_wr); \
	(var).retval_len16 = cpu_to_be32(FW_LEN16(var)); \
} while (0)

int t4_fwaddrspace_write(struct adapter *adap, unsigned int mbox,
			  u32 addr, u32 val)
{
	u32 ldst_addrspace;
	struct fw_ldst_cmd c;

	memset(&c, 0, sizeof(c));
	ldst_addrspace = V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_FIRMWARE);
	c.op_to_addrspace = cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
					F_FW_CMD_REQUEST |
					F_FW_CMD_WRITE |
					ldst_addrspace);
	c.cycles_to_len16 = cpu_to_be32(FW_LEN16(c));
	c.u.addrval.addr = cpu_to_be32(addr);
	c.u.addrval.val = cpu_to_be32(val);

	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_mdio_rd - read a PHY register through MDIO
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@phy_addr: the PHY address
 *	@mmd: the PHY MMD to access (0 for clause 22 PHYs)
 *	@reg: the register to read
 *	@valp: where to store the value
 *
 *	Issues a FW command through the given mailbox to read a PHY register.
 */
int t4_mdio_rd(struct adapter *adap, unsigned int mbox, unsigned int phy_addr,
	       unsigned int mmd, unsigned int reg, unsigned int *valp)
{
	int ret;
	u32 ldst_addrspace;
	struct fw_ldst_cmd c;

	memset(&c, 0, sizeof(c));
	ldst_addrspace = V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_MDIO);
	c.op_to_addrspace = cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
					F_FW_CMD_REQUEST | F_FW_CMD_READ |
					ldst_addrspace);
	c.cycles_to_len16 = cpu_to_be32(FW_LEN16(c));
	c.u.mdio.paddr_mmd = cpu_to_be16(V_FW_LDST_CMD_PADDR(phy_addr) |
					 V_FW_LDST_CMD_MMD(mmd));
	c.u.mdio.raddr = cpu_to_be16(reg);

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret == 0)
		*valp = be16_to_cpu(c.u.mdio.rval);
	return ret;
}

/**
 *	t4_mdio_wr - write a PHY register through MDIO
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@phy_addr: the PHY address
 *	@mmd: the PHY MMD to access (0 for clause 22 PHYs)
 *	@reg: the register to write
 *	@valp: value to write
 *
 *	Issues a FW command through the given mailbox to write a PHY register.
 */
int t4_mdio_wr(struct adapter *adap, unsigned int mbox, unsigned int phy_addr,
	       unsigned int mmd, unsigned int reg, unsigned int val)
{
	u32 ldst_addrspace;
	struct fw_ldst_cmd c;

	memset(&c, 0, sizeof(c));
	ldst_addrspace = V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_MDIO);
	c.op_to_addrspace = cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
					F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
					ldst_addrspace);
	c.cycles_to_len16 = cpu_to_be32(FW_LEN16(c));
	c.u.mdio.paddr_mmd = cpu_to_be16(V_FW_LDST_CMD_PADDR(phy_addr) |
					 V_FW_LDST_CMD_MMD(mmd));
	c.u.mdio.raddr = cpu_to_be16(reg);
	c.u.mdio.rval = cpu_to_be16(val);

	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *
 *	t4_sge_decode_idma_state - decode the idma state
 *	@adap: the adapter
 *	@state: the state idma is stuck in
 */
void t4_sge_decode_idma_state(struct adapter *adapter, int state)
{
	static const char * const t4_decode[] = {
		"IDMA_IDLE",
		"IDMA_PUSH_MORE_CPL_FIFO",
		"IDMA_PUSH_CPL_MSG_HEADER_TO_FIFO",
		"Not used",
		"IDMA_PHYSADDR_SEND_PCIEHDR",
		"IDMA_PHYSADDR_SEND_PAYLOAD_FIRST",
		"IDMA_PHYSADDR_SEND_PAYLOAD",
		"IDMA_SEND_FIFO_TO_IMSG",
		"IDMA_FL_REQ_DATA_FL_PREP",
		"IDMA_FL_REQ_DATA_FL",
		"IDMA_FL_DROP",
		"IDMA_FL_H_REQ_HEADER_FL",
		"IDMA_FL_H_SEND_PCIEHDR",
		"IDMA_FL_H_PUSH_CPL_FIFO",
		"IDMA_FL_H_SEND_CPL",
		"IDMA_FL_H_SEND_IP_HDR_FIRST",
		"IDMA_FL_H_SEND_IP_HDR",
		"IDMA_FL_H_REQ_NEXT_HEADER_FL",
		"IDMA_FL_H_SEND_NEXT_PCIEHDR",
		"IDMA_FL_H_SEND_IP_HDR_PADDING",
		"IDMA_FL_D_SEND_PCIEHDR",
		"IDMA_FL_D_SEND_CPL_AND_IP_HDR",
		"IDMA_FL_D_REQ_NEXT_DATA_FL",
		"IDMA_FL_SEND_PCIEHDR",
		"IDMA_FL_PUSH_CPL_FIFO",
		"IDMA_FL_SEND_CPL",
		"IDMA_FL_SEND_PAYLOAD_FIRST",
		"IDMA_FL_SEND_PAYLOAD",
		"IDMA_FL_REQ_NEXT_DATA_FL",
		"IDMA_FL_SEND_NEXT_PCIEHDR",
		"IDMA_FL_SEND_PADDING",
		"IDMA_FL_SEND_COMPLETION_TO_IMSG",
		"IDMA_FL_SEND_FIFO_TO_IMSG",
		"IDMA_FL_REQ_DATAFL_DONE",
		"IDMA_FL_REQ_HEADERFL_DONE",
	};
	static const char * const t5_decode[] = {
		"IDMA_IDLE",
		"IDMA_ALMOST_IDLE",
		"IDMA_PUSH_MORE_CPL_FIFO",
		"IDMA_PUSH_CPL_MSG_HEADER_TO_FIFO",
		"IDMA_SGEFLRFLUSH_SEND_PCIEHDR",
		"IDMA_PHYSADDR_SEND_PCIEHDR",
		"IDMA_PHYSADDR_SEND_PAYLOAD_FIRST",
		"IDMA_PHYSADDR_SEND_PAYLOAD",
		"IDMA_SEND_FIFO_TO_IMSG",
		"IDMA_FL_REQ_DATA_FL",
		"IDMA_FL_DROP",
		"IDMA_FL_DROP_SEND_INC",
		"IDMA_FL_H_REQ_HEADER_FL",
		"IDMA_FL_H_SEND_PCIEHDR",
		"IDMA_FL_H_PUSH_CPL_FIFO",
		"IDMA_FL_H_SEND_CPL",
		"IDMA_FL_H_SEND_IP_HDR_FIRST",
		"IDMA_FL_H_SEND_IP_HDR",
		"IDMA_FL_H_REQ_NEXT_HEADER_FL",
		"IDMA_FL_H_SEND_NEXT_PCIEHDR",
		"IDMA_FL_H_SEND_IP_HDR_PADDING",
		"IDMA_FL_D_SEND_PCIEHDR",
		"IDMA_FL_D_SEND_CPL_AND_IP_HDR",
		"IDMA_FL_D_REQ_NEXT_DATA_FL",
		"IDMA_FL_SEND_PCIEHDR",
		"IDMA_FL_PUSH_CPL_FIFO",
		"IDMA_FL_SEND_CPL",
		"IDMA_FL_SEND_PAYLOAD_FIRST",
		"IDMA_FL_SEND_PAYLOAD",
		"IDMA_FL_REQ_NEXT_DATA_FL",
		"IDMA_FL_SEND_NEXT_PCIEHDR",
		"IDMA_FL_SEND_PADDING",
		"IDMA_FL_SEND_COMPLETION_TO_IMSG",
	};
	static const char * const t6_decode[] = {
		"IDMA_IDLE",
		"IDMA_PUSH_MORE_CPL_FIFO",
		"IDMA_PUSH_CPL_MSG_HEADER_TO_FIFO",
		"IDMA_SGEFLRFLUSH_SEND_PCIEHDR",
		"IDMA_PHYSADDR_SEND_PCIEHDR",
		"IDMA_PHYSADDR_SEND_PAYLOAD_FIRST",
		"IDMA_PHYSADDR_SEND_PAYLOAD",
		"IDMA_FL_REQ_DATA_FL",
		"IDMA_FL_DROP",
		"IDMA_FL_DROP_SEND_INC",
		"IDMA_FL_H_REQ_HEADER_FL",
		"IDMA_FL_H_SEND_PCIEHDR",
		"IDMA_FL_H_PUSH_CPL_FIFO",
		"IDMA_FL_H_SEND_CPL",
		"IDMA_FL_H_SEND_IP_HDR_FIRST",
		"IDMA_FL_H_SEND_IP_HDR",
		"IDMA_FL_H_REQ_NEXT_HEADER_FL",
		"IDMA_FL_H_SEND_NEXT_PCIEHDR",
		"IDMA_FL_H_SEND_IP_HDR_PADDING",
		"IDMA_FL_D_SEND_PCIEHDR",
		"IDMA_FL_D_SEND_CPL_AND_IP_HDR",
		"IDMA_FL_D_REQ_NEXT_DATA_FL",
		"IDMA_FL_SEND_PCIEHDR",
		"IDMA_FL_PUSH_CPL_FIFO",
		"IDMA_FL_SEND_CPL",
		"IDMA_FL_SEND_PAYLOAD_FIRST",
		"IDMA_FL_SEND_PAYLOAD",
		"IDMA_FL_REQ_NEXT_DATA_FL",
		"IDMA_FL_SEND_NEXT_PCIEHDR",
		"IDMA_FL_SEND_PADDING",
		"IDMA_FL_SEND_COMPLETION_TO_IMSG",
	};
	static const u32 sge_regs[] = {
		A_SGE_DEBUG_DATA_LOW_INDEX_2,
		A_SGE_DEBUG_DATA_LOW_INDEX_3,
		A_SGE_DEBUG_DATA_HIGH_INDEX_10,
	};
	const char * const *sge_idma_decode;
	int sge_idma_decode_nstates;
	int i;
	unsigned int chip_version = chip_id(adapter);

	/* Select the right set of decode strings to dump depending on the
	 * adapter chip type.
	 */
	switch (chip_version) {
	case CHELSIO_T4:
		sge_idma_decode = (const char * const *)t4_decode;
		sge_idma_decode_nstates = ARRAY_SIZE(t4_decode);
		break;

	case CHELSIO_T5:
		sge_idma_decode = (const char * const *)t5_decode;
		sge_idma_decode_nstates = ARRAY_SIZE(t5_decode);
		break;

	case CHELSIO_T6:
		sge_idma_decode = (const char * const *)t6_decode;
		sge_idma_decode_nstates = ARRAY_SIZE(t6_decode);
		break;

	default:
		CH_ERR(adapter,	"Unsupported chip version %d\n", chip_version);
		return;
	}

	if (state < sge_idma_decode_nstates)
		CH_WARN(adapter, "idma state %s\n", sge_idma_decode[state]);
	else
		CH_WARN(adapter, "idma state %d unknown\n", state);

	for (i = 0; i < ARRAY_SIZE(sge_regs); i++)
		CH_WARN(adapter, "SGE register %#x value %#x\n",
			sge_regs[i], t4_read_reg(adapter, sge_regs[i]));
}

/**
 *      t4_sge_ctxt_flush - flush the SGE context cache
 *      @adap: the adapter
 *      @mbox: mailbox to use for the FW command
 *
 *      Issues a FW command through the given mailbox to flush the
 *      SGE context cache.
 */
int t4_sge_ctxt_flush(struct adapter *adap, unsigned int mbox)
{
	int ret;
	u32 ldst_addrspace;
	struct fw_ldst_cmd c;

	memset(&c, 0, sizeof(c));
	ldst_addrspace = V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_SGE_EGRC);
	c.op_to_addrspace = cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
					F_FW_CMD_REQUEST | F_FW_CMD_READ |
					ldst_addrspace);
	c.cycles_to_len16 = cpu_to_be32(FW_LEN16(c));
	c.u.idctxt.msg_ctxtflush = cpu_to_be32(F_FW_LDST_CMD_CTXTFLUSH);

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	return ret;
}

/**
 *      t4_fw_hello - establish communication with FW
 *      @adap: the adapter
 *      @mbox: mailbox to use for the FW command
 *      @evt_mbox: mailbox to receive async FW events
 *      @master: specifies the caller's willingness to be the device master
 *	@state: returns the current device state (if non-NULL)
 *
 *	Issues a command to establish communication with FW.  Returns either
 *	an error (negative integer) or the mailbox of the Master PF.
 */
int t4_fw_hello(struct adapter *adap, unsigned int mbox, unsigned int evt_mbox,
		enum dev_master master, enum dev_state *state)
{
	int ret;
	struct fw_hello_cmd c;
	u32 v;
	unsigned int master_mbox;
	int retries = FW_CMD_HELLO_RETRIES;

retry:
	memset(&c, 0, sizeof(c));
	INIT_CMD(c, HELLO, WRITE);
	c.err_to_clearinit = cpu_to_be32(
		V_FW_HELLO_CMD_MASTERDIS(master == MASTER_CANT) |
		V_FW_HELLO_CMD_MASTERFORCE(master == MASTER_MUST) |
		V_FW_HELLO_CMD_MBMASTER(master == MASTER_MUST ?
					mbox : M_FW_HELLO_CMD_MBMASTER) |
		V_FW_HELLO_CMD_MBASYNCNOT(evt_mbox) |
		V_FW_HELLO_CMD_STAGE(FW_HELLO_CMD_STAGE_OS) |
		F_FW_HELLO_CMD_CLEARINIT);

	/*
	 * Issue the HELLO command to the firmware.  If it's not successful
	 * but indicates that we got a "busy" or "timeout" condition, retry
	 * the HELLO until we exhaust our retry limit.  If we do exceed our
	 * retry limit, check to see if the firmware left us any error
	 * information and report that if so ...
	 */
	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret != FW_SUCCESS) {
		if ((ret == -EBUSY || ret == -ETIMEDOUT) && retries-- > 0)
			goto retry;
		if (t4_read_reg(adap, A_PCIE_FW) & F_PCIE_FW_ERR)
			t4_report_fw_error(adap);
		return ret;
	}

	v = be32_to_cpu(c.err_to_clearinit);
	master_mbox = G_FW_HELLO_CMD_MBMASTER(v);
	if (state) {
		if (v & F_FW_HELLO_CMD_ERR)
			*state = DEV_STATE_ERR;
		else if (v & F_FW_HELLO_CMD_INIT)
			*state = DEV_STATE_INIT;
		else
			*state = DEV_STATE_UNINIT;
	}

	/*
	 * If we're not the Master PF then we need to wait around for the
	 * Master PF Driver to finish setting up the adapter.
	 *
	 * Note that we also do this wait if we're a non-Master-capable PF and
	 * there is no current Master PF; a Master PF may show up momentarily
	 * and we wouldn't want to fail pointlessly.  (This can happen when an
	 * OS loads lots of different drivers rapidly at the same time).  In
	 * this case, the Master PF returned by the firmware will be
	 * M_PCIE_FW_MASTER so the test below will work ...
	 */
	if ((v & (F_FW_HELLO_CMD_ERR|F_FW_HELLO_CMD_INIT)) == 0 &&
	    master_mbox != mbox) {
		int waiting = FW_CMD_HELLO_TIMEOUT;

		/*
		 * Wait for the firmware to either indicate an error or
		 * initialized state.  If we see either of these we bail out
		 * and report the issue to the caller.  If we exhaust the
		 * "hello timeout" and we haven't exhausted our retries, try
		 * again.  Otherwise bail with a timeout error.
		 */
		for (;;) {
			u32 pcie_fw;

			msleep(50);
			waiting -= 50;

			/*
			 * If neither Error nor Initialialized are indicated
			 * by the firmware keep waiting till we exhaust our
			 * timeout ... and then retry if we haven't exhausted
			 * our retries ...
			 */
			pcie_fw = t4_read_reg(adap, A_PCIE_FW);
			if (!(pcie_fw & (F_PCIE_FW_ERR|F_PCIE_FW_INIT))) {
				if (waiting <= 0) {
					if (retries-- > 0)
						goto retry;

					return -ETIMEDOUT;
				}
				continue;
			}

			/*
			 * We either have an Error or Initialized condition
			 * report errors preferentially.
			 */
			if (state) {
				if (pcie_fw & F_PCIE_FW_ERR)
					*state = DEV_STATE_ERR;
				else if (pcie_fw & F_PCIE_FW_INIT)
					*state = DEV_STATE_INIT;
			}

			/*
			 * If we arrived before a Master PF was selected and
			 * there's not a valid Master PF, grab its identity
			 * for our caller.
			 */
			if (master_mbox == M_PCIE_FW_MASTER &&
			    (pcie_fw & F_PCIE_FW_MASTER_VLD))
				master_mbox = G_PCIE_FW_MASTER(pcie_fw);
			break;
		}
	}

	return master_mbox;
}

/**
 *	t4_fw_bye - end communication with FW
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *
 *	Issues a command to terminate communication with FW.
 */
int t4_fw_bye(struct adapter *adap, unsigned int mbox)
{
	struct fw_bye_cmd c;

	memset(&c, 0, sizeof(c));
	INIT_CMD(c, BYE, WRITE);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_fw_reset - issue a reset to FW
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@reset: specifies the type of reset to perform
 *
 *	Issues a reset command of the specified type to FW.
 */
int t4_fw_reset(struct adapter *adap, unsigned int mbox, int reset)
{
	struct fw_reset_cmd c;

	memset(&c, 0, sizeof(c));
	INIT_CMD(c, RESET, WRITE);
	c.val = cpu_to_be32(reset);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_fw_halt - issue a reset/halt to FW and put uP into RESET
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW RESET command (if desired)
 *	@force: force uP into RESET even if FW RESET command fails
 *
 *	Issues a RESET command to firmware (if desired) with a HALT indication
 *	and then puts the microprocessor into RESET state.  The RESET command
 *	will only be issued if a legitimate mailbox is provided (mbox <=
 *	M_PCIE_FW_MASTER).
 *
 *	This is generally used in order for the host to safely manipulate the
 *	adapter without fear of conflicting with whatever the firmware might
 *	be doing.  The only way out of this state is to RESTART the firmware
 *	...
 */
int t4_fw_halt(struct adapter *adap, unsigned int mbox, int force)
{
	int ret = 0;

	/*
	 * If a legitimate mailbox is provided, issue a RESET command
	 * with a HALT indication.
	 */
	if (mbox <= M_PCIE_FW_MASTER) {
		struct fw_reset_cmd c;

		memset(&c, 0, sizeof(c));
		INIT_CMD(c, RESET, WRITE);
		c.val = cpu_to_be32(F_PIORST | F_PIORSTMODE);
		c.halt_pkd = cpu_to_be32(F_FW_RESET_CMD_HALT);
		ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
	}

	/*
	 * Normally we won't complete the operation if the firmware RESET
	 * command fails but if our caller insists we'll go ahead and put the
	 * uP into RESET.  This can be useful if the firmware is hung or even
	 * missing ...  We'll have to take the risk of putting the uP into
	 * RESET without the cooperation of firmware in that case.
	 *
	 * We also force the firmware's HALT flag to be on in case we bypassed
	 * the firmware RESET command above or we're dealing with old firmware
	 * which doesn't have the HALT capability.  This will serve as a flag
	 * for the incoming firmware to know that it's coming out of a HALT
	 * rather than a RESET ... if it's new enough to understand that ...
	 */
	if (ret == 0 || force) {
		t4_set_reg_field(adap, A_CIM_BOOT_CFG, F_UPCRST, F_UPCRST);
		t4_set_reg_field(adap, A_PCIE_FW, F_PCIE_FW_HALT,
				 F_PCIE_FW_HALT);
	}

	/*
	 * And we always return the result of the firmware RESET command
	 * even when we force the uP into RESET ...
	 */
	return ret;
}

/**
 *	t4_fw_restart - restart the firmware by taking the uP out of RESET
 *	@adap: the adapter
 *	@reset: if we want to do a RESET to restart things
 *
 *	Restart firmware previously halted by t4_fw_halt().  On successful
 *	return the previous PF Master remains as the new PF Master and there
 *	is no need to issue a new HELLO command, etc.
 *
 *	We do this in two ways:
 *
 *	 1. If we're dealing with newer firmware we'll simply want to take
 *	    the chip's microprocessor out of RESET.  This will cause the
 *	    firmware to start up from its start vector.  And then we'll loop
 *	    until the firmware indicates it's started again (PCIE_FW.HALT
 *	    reset to 0) or we timeout.
 *
 *	 2. If we're dealing with older firmware then we'll need to RESET
 *	    the chip since older firmware won't recognize the PCIE_FW.HALT
 *	    flag and automatically RESET itself on startup.
 */
int t4_fw_restart(struct adapter *adap, unsigned int mbox, int reset)
{
	if (reset) {
		/*
		 * Since we're directing the RESET instead of the firmware
		 * doing it automatically, we need to clear the PCIE_FW.HALT
		 * bit.
		 */
		t4_set_reg_field(adap, A_PCIE_FW, F_PCIE_FW_HALT, 0);

		/*
		 * If we've been given a valid mailbox, first try to get the
		 * firmware to do the RESET.  If that works, great and we can
		 * return success.  Otherwise, if we haven't been given a
		 * valid mailbox or the RESET command failed, fall back to
		 * hitting the chip with a hammer.
		 */
		if (mbox <= M_PCIE_FW_MASTER) {
			t4_set_reg_field(adap, A_CIM_BOOT_CFG, F_UPCRST, 0);
			msleep(100);
			if (t4_fw_reset(adap, mbox,
					F_PIORST | F_PIORSTMODE) == 0)
				return 0;
		}

		t4_write_reg(adap, A_PL_RST, F_PIORST | F_PIORSTMODE);
		msleep(2000);
	} else {
		int ms;

		t4_set_reg_field(adap, A_CIM_BOOT_CFG, F_UPCRST, 0);
		for (ms = 0; ms < FW_CMD_MAX_TIMEOUT; ) {
			if (!(t4_read_reg(adap, A_PCIE_FW) & F_PCIE_FW_HALT))
				return FW_SUCCESS;
			msleep(100);
			ms += 100;
		}
		return -ETIMEDOUT;
	}
	return 0;
}

/**
 *	t4_fw_upgrade - perform all of the steps necessary to upgrade FW
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW RESET command (if desired)
 *	@fw_data: the firmware image to write
 *	@size: image size
 *	@force: force upgrade even if firmware doesn't cooperate
 *
 *	Perform all of the steps necessary for upgrading an adapter's
 *	firmware image.  Normally this requires the cooperation of the
 *	existing firmware in order to halt all existing activities
 *	but if an invalid mailbox token is passed in we skip that step
 *	(though we'll still put the adapter microprocessor into RESET in
 *	that case).
 *
 *	On successful return the new firmware will have been loaded and
 *	the adapter will have been fully RESET losing all previous setup
 *	state.  On unsuccessful return the adapter may be completely hosed ...
 *	positive errno indicates that the adapter is ~probably~ intact, a
 *	negative errno indicates that things are looking bad ...
 */
int t4_fw_upgrade(struct adapter *adap, unsigned int mbox,
		  const u8 *fw_data, unsigned int size, int force)
{
	const struct fw_hdr *fw_hdr = (const struct fw_hdr *)fw_data;
	unsigned int bootstrap =
	    be32_to_cpu(fw_hdr->magic) == FW_HDR_MAGIC_BOOTSTRAP;
	int reset, ret;

	if (!t4_fw_matches_chip(adap, fw_hdr))
		return -EINVAL;

	if (!bootstrap) {
		ret = t4_fw_halt(adap, mbox, force);
		if (ret < 0 && !force)
			return ret;
	}

	ret = t4_load_fw(adap, fw_data, size);
	if (ret < 0 || bootstrap)
		return ret;

	/*
	 * Older versions of the firmware don't understand the new
	 * PCIE_FW.HALT flag and so won't know to perform a RESET when they
	 * restart.  So for newly loaded older firmware we'll have to do the
	 * RESET for it so it starts up on a clean slate.  We can tell if
	 * the newly loaded firmware will handle this right by checking
	 * its header flags to see if it advertises the capability.
	 */
	reset = ((be32_to_cpu(fw_hdr->flags) & FW_HDR_FLAGS_RESET_HALT) == 0);
	return t4_fw_restart(adap, mbox, reset);
}

/**
 *	t4_fw_initialize - ask FW to initialize the device
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *
 *	Issues a command to FW to partially initialize the device.  This
 *	performs initialization that generally doesn't depend on user input.
 */
int t4_fw_initialize(struct adapter *adap, unsigned int mbox)
{
	struct fw_initialize_cmd c;

	memset(&c, 0, sizeof(c));
	INIT_CMD(c, INITIALIZE, WRITE);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_query_params_rw - query FW or device parameters
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@pf: the PF
 *	@vf: the VF
 *	@nparams: the number of parameters
 *	@params: the parameter names
 *	@val: the parameter values
 *	@rw: Write and read flag
 *
 *	Reads the value of FW or device parameters.  Up to 7 parameters can be
 *	queried at once.
 */
int t4_query_params_rw(struct adapter *adap, unsigned int mbox, unsigned int pf,
		       unsigned int vf, unsigned int nparams, const u32 *params,
		       u32 *val, int rw)
{
	int i, ret;
	struct fw_params_cmd c;
	__be32 *p = &c.param[0].mnem;

	if (nparams > 7)
		return -EINVAL;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_PARAMS_CMD) |
				  F_FW_CMD_REQUEST | F_FW_CMD_READ |
				  V_FW_PARAMS_CMD_PFN(pf) |
				  V_FW_PARAMS_CMD_VFN(vf));
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));

	for (i = 0; i < nparams; i++) {
		*p++ = cpu_to_be32(*params++);
		if (rw)
			*p = cpu_to_be32(*(val + i));
		p++;
	}

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret == 0)
		for (i = 0, p = &c.param[0].val; i < nparams; i++, p += 2)
			*val++ = be32_to_cpu(*p);
	return ret;
}

int t4_query_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int nparams, const u32 *params,
		    u32 *val)
{
	return t4_query_params_rw(adap, mbox, pf, vf, nparams, params, val, 0);
}

/**
 *      t4_set_params_timeout - sets FW or device parameters
 *      @adap: the adapter
 *      @mbox: mailbox to use for the FW command
 *      @pf: the PF
 *      @vf: the VF
 *      @nparams: the number of parameters
 *      @params: the parameter names
 *      @val: the parameter values
 *      @timeout: the timeout time
 *
 *      Sets the value of FW or device parameters.  Up to 7 parameters can be
 *      specified at once.
 */
int t4_set_params_timeout(struct adapter *adap, unsigned int mbox,
			  unsigned int pf, unsigned int vf,
			  unsigned int nparams, const u32 *params,
			  const u32 *val, int timeout)
{
	struct fw_params_cmd c;
	__be32 *p = &c.param[0].mnem;

	if (nparams > 7)
		return -EINVAL;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_PARAMS_CMD) |
				  F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				  V_FW_PARAMS_CMD_PFN(pf) |
				  V_FW_PARAMS_CMD_VFN(vf));
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));

	while (nparams--) {
		*p++ = cpu_to_be32(*params++);
		*p++ = cpu_to_be32(*val++);
	}

	return t4_wr_mbox_timeout(adap, mbox, &c, sizeof(c), NULL, timeout);
}

/**
 *	t4_set_params - sets FW or device parameters
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@pf: the PF
 *	@vf: the VF
 *	@nparams: the number of parameters
 *	@params: the parameter names
 *	@val: the parameter values
 *
 *	Sets the value of FW or device parameters.  Up to 7 parameters can be
 *	specified at once.
 */
int t4_set_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		  unsigned int vf, unsigned int nparams, const u32 *params,
		  const u32 *val)
{
	return t4_set_params_timeout(adap, mbox, pf, vf, nparams, params, val,
				     FW_CMD_MAX_TIMEOUT);
}

/**
 *	t4_cfg_pfvf - configure PF/VF resource limits
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@pf: the PF being configured
 *	@vf: the VF being configured
 *	@txq: the max number of egress queues
 *	@txq_eth_ctrl: the max number of egress Ethernet or control queues
 *	@rxqi: the max number of interrupt-capable ingress queues
 *	@rxq: the max number of interruptless ingress queues
 *	@tc: the PCI traffic class
 *	@vi: the max number of virtual interfaces
 *	@cmask: the channel access rights mask for the PF/VF
 *	@pmask: the port access rights mask for the PF/VF
 *	@nexact: the maximum number of exact MPS filters
 *	@rcaps: read capabilities
 *	@wxcaps: write/execute capabilities
 *
 *	Configures resource limits and capabilities for a physical or virtual
 *	function.
 */
int t4_cfg_pfvf(struct adapter *adap, unsigned int mbox, unsigned int pf,
		unsigned int vf, unsigned int txq, unsigned int txq_eth_ctrl,
		unsigned int rxqi, unsigned int rxq, unsigned int tc,
		unsigned int vi, unsigned int cmask, unsigned int pmask,
		unsigned int nexact, unsigned int rcaps, unsigned int wxcaps)
{
	struct fw_pfvf_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_PFVF_CMD) | F_FW_CMD_REQUEST |
				  F_FW_CMD_WRITE | V_FW_PFVF_CMD_PFN(pf) |
				  V_FW_PFVF_CMD_VFN(vf));
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));
	c.niqflint_niq = cpu_to_be32(V_FW_PFVF_CMD_NIQFLINT(rxqi) |
				     V_FW_PFVF_CMD_NIQ(rxq));
	c.type_to_neq = cpu_to_be32(V_FW_PFVF_CMD_CMASK(cmask) |
				    V_FW_PFVF_CMD_PMASK(pmask) |
				    V_FW_PFVF_CMD_NEQ(txq));
	c.tc_to_nexactf = cpu_to_be32(V_FW_PFVF_CMD_TC(tc) |
				      V_FW_PFVF_CMD_NVI(vi) |
				      V_FW_PFVF_CMD_NEXACTF(nexact));
	c.r_caps_to_nethctrl = cpu_to_be32(V_FW_PFVF_CMD_R_CAPS(rcaps) |
				     V_FW_PFVF_CMD_WX_CAPS(wxcaps) |
				     V_FW_PFVF_CMD_NETHCTRL(txq_eth_ctrl));
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_alloc_vi_func - allocate a virtual interface
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@port: physical port associated with the VI
 *	@pf: the PF owning the VI
 *	@vf: the VF owning the VI
 *	@nmac: number of MAC addresses needed (1 to 5)
 *	@mac: the MAC addresses of the VI
 *	@rss_size: size of RSS table slice associated with this VI
 *	@portfunc: which Port Application Function MAC Address is desired
 *	@idstype: Intrusion Detection Type
 *
 *	Allocates a virtual interface for the given physical port.  If @mac is
 *	not %NULL it contains the MAC addresses of the VI as assigned by FW.
 *	If @rss_size is %NULL the VI is not assigned any RSS slice by FW.
 *	@mac should be large enough to hold @nmac Ethernet addresses, they are
 *	stored consecutively so the space needed is @nmac * 6 bytes.
 *	Returns a negative error number or the non-negative VI id.
 */
int t4_alloc_vi_func(struct adapter *adap, unsigned int mbox,
		     unsigned int port, unsigned int pf, unsigned int vf,
		     unsigned int nmac, u8 *mac, u16 *rss_size,
		     unsigned int portfunc, unsigned int idstype)
{
	int ret;
	struct fw_vi_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_VI_CMD) | F_FW_CMD_REQUEST |
				  F_FW_CMD_WRITE | F_FW_CMD_EXEC |
				  V_FW_VI_CMD_PFN(pf) | V_FW_VI_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_VI_CMD_ALLOC | FW_LEN16(c));
	c.type_to_viid = cpu_to_be16(V_FW_VI_CMD_TYPE(idstype) |
				     V_FW_VI_CMD_FUNC(portfunc));
	c.portid_pkd = V_FW_VI_CMD_PORTID(port);
	c.nmac = nmac - 1;
	if(!rss_size)
		c.norss_rsssize = F_FW_VI_CMD_NORSS;

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret)
		return ret;

	if (mac) {
		memcpy(mac, c.mac, sizeof(c.mac));
		switch (nmac) {
		case 5:
			memcpy(mac + 24, c.nmac3, sizeof(c.nmac3));
		case 4:
			memcpy(mac + 18, c.nmac2, sizeof(c.nmac2));
		case 3:
			memcpy(mac + 12, c.nmac1, sizeof(c.nmac1));
		case 2:
			memcpy(mac + 6,  c.nmac0, sizeof(c.nmac0));
		}
	}
	if (rss_size)
		*rss_size = G_FW_VI_CMD_RSSSIZE(be16_to_cpu(c.norss_rsssize));
	return G_FW_VI_CMD_VIID(be16_to_cpu(c.type_to_viid));
}

/**
 *      t4_alloc_vi - allocate an [Ethernet Function] virtual interface
 *      @adap: the adapter
 *      @mbox: mailbox to use for the FW command
 *      @port: physical port associated with the VI
 *      @pf: the PF owning the VI
 *      @vf: the VF owning the VI
 *      @nmac: number of MAC addresses needed (1 to 5)
 *      @mac: the MAC addresses of the VI
 *      @rss_size: size of RSS table slice associated with this VI
 *
 *	backwards compatible and convieniance routine to allocate a Virtual
 *	Interface with a Ethernet Port Application Function and Intrustion
 *	Detection System disabled.
 */
int t4_alloc_vi(struct adapter *adap, unsigned int mbox, unsigned int port,
		unsigned int pf, unsigned int vf, unsigned int nmac, u8 *mac,
		u16 *rss_size)
{
	return t4_alloc_vi_func(adap, mbox, port, pf, vf, nmac, mac, rss_size,
				FW_VI_FUNC_ETH, 0);
}

/**
 * 	t4_free_vi - free a virtual interface
 * 	@adap: the adapter
 * 	@mbox: mailbox to use for the FW command
 * 	@pf: the PF owning the VI
 * 	@vf: the VF owning the VI
 * 	@viid: virtual interface identifiler
 *
 * 	Free a previously allocated virtual interface.
 */
int t4_free_vi(struct adapter *adap, unsigned int mbox, unsigned int pf,
	       unsigned int vf, unsigned int viid)
{
	struct fw_vi_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_VI_CMD) |
				  F_FW_CMD_REQUEST |
				  F_FW_CMD_EXEC |
				  V_FW_VI_CMD_PFN(pf) |
				  V_FW_VI_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_VI_CMD_FREE | FW_LEN16(c));
	c.type_to_viid = cpu_to_be16(V_FW_VI_CMD_VIID(viid));

	return t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
}

/**
 *	t4_set_rxmode - set Rx properties of a virtual interface
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@viid: the VI id
 *	@mtu: the new MTU or -1
 *	@promisc: 1 to enable promiscuous mode, 0 to disable it, -1 no change
 *	@all_multi: 1 to enable all-multi mode, 0 to disable it, -1 no change
 *	@bcast: 1 to enable broadcast Rx, 0 to disable it, -1 no change
 *	@vlanex: 1 to enable HW VLAN extraction, 0 to disable it, -1 no change
 *	@sleep_ok: if true we may sleep while awaiting command completion
 *
 *	Sets Rx properties of a virtual interface.
 */
int t4_set_rxmode(struct adapter *adap, unsigned int mbox, unsigned int viid,
		  int mtu, int promisc, int all_multi, int bcast, int vlanex,
		  bool sleep_ok)
{
	struct fw_vi_rxmode_cmd c;

	/* convert to FW values */
	if (mtu < 0)
		mtu = M_FW_VI_RXMODE_CMD_MTU;
	if (promisc < 0)
		promisc = M_FW_VI_RXMODE_CMD_PROMISCEN;
	if (all_multi < 0)
		all_multi = M_FW_VI_RXMODE_CMD_ALLMULTIEN;
	if (bcast < 0)
		bcast = M_FW_VI_RXMODE_CMD_BROADCASTEN;
	if (vlanex < 0)
		vlanex = M_FW_VI_RXMODE_CMD_VLANEXEN;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_RXMODE_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				   V_FW_VI_RXMODE_CMD_VIID(viid));
	c.retval_len16 = cpu_to_be32(FW_LEN16(c));
	c.mtu_to_vlanexen =
		cpu_to_be32(V_FW_VI_RXMODE_CMD_MTU(mtu) |
			    V_FW_VI_RXMODE_CMD_PROMISCEN(promisc) |
			    V_FW_VI_RXMODE_CMD_ALLMULTIEN(all_multi) |
			    V_FW_VI_RXMODE_CMD_BROADCASTEN(bcast) |
			    V_FW_VI_RXMODE_CMD_VLANEXEN(vlanex));
	return t4_wr_mbox_meat(adap, mbox, &c, sizeof(c), NULL, sleep_ok);
}

/**
 *	t4_alloc_mac_filt - allocates exact-match filters for MAC addresses
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@viid: the VI id
 *	@free: if true any existing filters for this VI id are first removed
 *	@naddr: the number of MAC addresses to allocate filters for (up to 7)
 *	@addr: the MAC address(es)
 *	@idx: where to store the index of each allocated filter
 *	@hash: pointer to hash address filter bitmap
 *	@sleep_ok: call is allowed to sleep
 *
 *	Allocates an exact-match filter for each of the supplied addresses and
 *	sets it to the corresponding address.  If @idx is not %NULL it should
 *	have at least @naddr entries, each of which will be set to the index of
 *	the filter allocated for the corresponding MAC address.  If a filter
 *	could not be allocated for an address its index is set to 0xffff.
 *	If @hash is not %NULL addresses that fail to allocate an exact filter
 *	are hashed and update the hash filter bitmap pointed at by @hash.
 *
 *	Returns a negative error number or the number of filters allocated.
 */
int t4_alloc_mac_filt(struct adapter *adap, unsigned int mbox,
		      unsigned int viid, bool free, unsigned int naddr,
		      const u8 **addr, u16 *idx, u64 *hash, bool sleep_ok)
{
	int offset, ret = 0;
	struct fw_vi_mac_cmd c;
	unsigned int nfilters = 0;
	unsigned int max_naddr = adap->chip_params->mps_tcam_size;
	unsigned int rem = naddr;

	if (naddr > max_naddr)
		return -EINVAL;

	for (offset = 0; offset < naddr ; /**/) {
		unsigned int fw_naddr = (rem < ARRAY_SIZE(c.u.exact)
					 ? rem
					 : ARRAY_SIZE(c.u.exact));
		size_t len16 = DIV_ROUND_UP(offsetof(struct fw_vi_mac_cmd,
						     u.exact[fw_naddr]), 16);
		struct fw_vi_mac_exact *p;
		int i;

		memset(&c, 0, sizeof(c));
		c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_MAC_CMD) |
					   F_FW_CMD_REQUEST |
					   F_FW_CMD_WRITE |
					   V_FW_CMD_EXEC(free) |
					   V_FW_VI_MAC_CMD_VIID(viid));
		c.freemacs_to_len16 = cpu_to_be32(V_FW_VI_MAC_CMD_FREEMACS(free) |
						  V_FW_CMD_LEN16(len16));

		for (i = 0, p = c.u.exact; i < fw_naddr; i++, p++) {
			p->valid_to_idx =
				cpu_to_be16(F_FW_VI_MAC_CMD_VALID |
					    V_FW_VI_MAC_CMD_IDX(FW_VI_MAC_ADD_MAC));
			memcpy(p->macaddr, addr[offset+i], sizeof(p->macaddr));
		}

		/*
		 * It's okay if we run out of space in our MAC address arena.
		 * Some of the addresses we submit may get stored so we need
		 * to run through the reply to see what the results were ...
		 */
		ret = t4_wr_mbox_meat(adap, mbox, &c, sizeof(c), &c, sleep_ok);
		if (ret && ret != -FW_ENOMEM)
			break;

		for (i = 0, p = c.u.exact; i < fw_naddr; i++, p++) {
			u16 index = G_FW_VI_MAC_CMD_IDX(
						be16_to_cpu(p->valid_to_idx));

			if (idx)
				idx[offset+i] = (index >=  max_naddr
						 ? 0xffff
						 : index);
			if (index < max_naddr)
				nfilters++;
			else if (hash)
				*hash |= (1ULL << hash_mac_addr(addr[offset+i]));
		}

		free = false;
		offset += fw_naddr;
		rem -= fw_naddr;
	}

	if (ret == 0 || ret == -FW_ENOMEM)
		ret = nfilters;
	return ret;
}

/**
 *	t4_change_mac - modifies the exact-match filter for a MAC address
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@viid: the VI id
 *	@idx: index of existing filter for old value of MAC address, or -1
 *	@addr: the new MAC address value
 *	@persist: whether a new MAC allocation should be persistent
 *	@add_smt: if true also add the address to the HW SMT
 *
 *	Modifies an exact-match filter and sets it to the new MAC address if
 *	@idx >= 0, or adds the MAC address to a new filter if @idx < 0.  In the
 *	latter case the address is added persistently if @persist is %true.
 *
 *	Note that in general it is not possible to modify the value of a given
 *	filter so the generic way to modify an address filter is to free the one
 *	being used by the old address value and allocate a new filter for the
 *	new address value.
 *
 *	Returns a negative error number or the index of the filter with the new
 *	MAC value.  Note that this index may differ from @idx.
 */
int t4_change_mac(struct adapter *adap, unsigned int mbox, unsigned int viid,
		  int idx, const u8 *addr, bool persist, bool add_smt)
{
	int ret, mode;
	struct fw_vi_mac_cmd c;
	struct fw_vi_mac_exact *p = c.u.exact;
	unsigned int max_mac_addr = adap->chip_params->mps_tcam_size;

	if (idx < 0)		/* new allocation */
		idx = persist ? FW_VI_MAC_ADD_PERSIST_MAC : FW_VI_MAC_ADD_MAC;
	mode = add_smt ? FW_VI_MAC_SMT_AND_MPSTCAM : FW_VI_MAC_MPS_TCAM_ENTRY;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_MAC_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				   V_FW_VI_MAC_CMD_VIID(viid));
	c.freemacs_to_len16 = cpu_to_be32(V_FW_CMD_LEN16(1));
	p->valid_to_idx = cpu_to_be16(F_FW_VI_MAC_CMD_VALID |
				      V_FW_VI_MAC_CMD_SMAC_RESULT(mode) |
				      V_FW_VI_MAC_CMD_IDX(idx));
	memcpy(p->macaddr, addr, sizeof(p->macaddr));

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret == 0) {
		ret = G_FW_VI_MAC_CMD_IDX(be16_to_cpu(p->valid_to_idx));
		if (ret >= max_mac_addr)
			ret = -ENOMEM;
	}
	return ret;
}

/**
 *	t4_set_addr_hash - program the MAC inexact-match hash filter
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@viid: the VI id
 *	@ucast: whether the hash filter should also match unicast addresses
 *	@vec: the value to be written to the hash filter
 *	@sleep_ok: call is allowed to sleep
 *
 *	Sets the 64-bit inexact-match hash filter for a virtual interface.
 */
int t4_set_addr_hash(struct adapter *adap, unsigned int mbox, unsigned int viid,
		     bool ucast, u64 vec, bool sleep_ok)
{
	struct fw_vi_mac_cmd c;
	u32 val;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_MAC_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
				   V_FW_VI_ENABLE_CMD_VIID(viid));
	val = V_FW_VI_MAC_CMD_ENTRY_TYPE(FW_VI_MAC_TYPE_HASHVEC) |
	      V_FW_VI_MAC_CMD_HASHUNIEN(ucast) | V_FW_CMD_LEN16(1);
	c.freemacs_to_len16 = cpu_to_be32(val);
	c.u.hash.hashvec = cpu_to_be64(vec);
	return t4_wr_mbox_meat(adap, mbox, &c, sizeof(c), NULL, sleep_ok);
}

/**
 *      t4_enable_vi_params - enable/disable a virtual interface
 *      @adap: the adapter
 *      @mbox: mailbox to use for the FW command
 *      @viid: the VI id
 *      @rx_en: 1=enable Rx, 0=disable Rx
 *      @tx_en: 1=enable Tx, 0=disable Tx
 *      @dcb_en: 1=enable delivery of Data Center Bridging messages.
 *
 *      Enables/disables a virtual interface.  Note that setting DCB Enable
 *      only makes sense when enabling a Virtual Interface ...
 */
int t4_enable_vi_params(struct adapter *adap, unsigned int mbox,
			unsigned int viid, bool rx_en, bool tx_en, bool dcb_en)
{
	struct fw_vi_enable_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_ENABLE_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				   V_FW_VI_ENABLE_CMD_VIID(viid));
	c.ien_to_len16 = cpu_to_be32(V_FW_VI_ENABLE_CMD_IEN(rx_en) |
				     V_FW_VI_ENABLE_CMD_EEN(tx_en) |
				     V_FW_VI_ENABLE_CMD_DCB_INFO(dcb_en) |
				     FW_LEN16(c));
	return t4_wr_mbox_ns(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_enable_vi - enable/disable a virtual interface
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@viid: the VI id
 *	@rx_en: 1=enable Rx, 0=disable Rx
 *	@tx_en: 1=enable Tx, 0=disable Tx
 *
 *	Enables/disables a virtual interface.  Note that setting DCB Enable
 *	only makes sense when enabling a Virtual Interface ...
 */
int t4_enable_vi(struct adapter *adap, unsigned int mbox, unsigned int viid,
		 bool rx_en, bool tx_en)
{
	return t4_enable_vi_params(adap, mbox, viid, rx_en, tx_en, 0);
}

/**
 *	t4_identify_port - identify a VI's port by blinking its LED
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@viid: the VI id
 *	@nblinks: how many times to blink LED at 2.5 Hz
 *
 *	Identifies a VI's port by blinking its LED.
 */
int t4_identify_port(struct adapter *adap, unsigned int mbox, unsigned int viid,
		     unsigned int nblinks)
{
	struct fw_vi_enable_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = cpu_to_be32(V_FW_CMD_OP(FW_VI_ENABLE_CMD) |
				   F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				   V_FW_VI_ENABLE_CMD_VIID(viid));
	c.ien_to_len16 = cpu_to_be32(F_FW_VI_ENABLE_CMD_LED | FW_LEN16(c));
	c.blinkdur = cpu_to_be16(nblinks);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_iq_stop - stop an ingress queue and its FLs
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@pf: the PF owning the queues
 *	@vf: the VF owning the queues
 *	@iqtype: the ingress queue type (FW_IQ_TYPE_FL_INT_CAP, etc.)
 *	@iqid: ingress queue id
 *	@fl0id: FL0 queue id or 0xffff if no attached FL0
 *	@fl1id: FL1 queue id or 0xffff if no attached FL1
 *
 *	Stops an ingress queue and its associated FLs, if any.  This causes
 *	any current or future data/messages destined for these queues to be
 *	tossed.
 */
int t4_iq_stop(struct adapter *adap, unsigned int mbox, unsigned int pf,
	       unsigned int vf, unsigned int iqtype, unsigned int iqid,
	       unsigned int fl0id, unsigned int fl1id)
{
	struct fw_iq_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
				  F_FW_CMD_EXEC | V_FW_IQ_CMD_PFN(pf) |
				  V_FW_IQ_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_IQ_CMD_IQSTOP | FW_LEN16(c));
	c.type_to_iqandstindex = cpu_to_be32(V_FW_IQ_CMD_TYPE(iqtype));
	c.iqid = cpu_to_be16(iqid);
	c.fl0id = cpu_to_be16(fl0id);
	c.fl1id = cpu_to_be16(fl1id);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_iq_free - free an ingress queue and its FLs
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@pf: the PF owning the queues
 *	@vf: the VF owning the queues
 *	@iqtype: the ingress queue type (FW_IQ_TYPE_FL_INT_CAP, etc.)
 *	@iqid: ingress queue id
 *	@fl0id: FL0 queue id or 0xffff if no attached FL0
 *	@fl1id: FL1 queue id or 0xffff if no attached FL1
 *
 *	Frees an ingress queue and its associated FLs, if any.
 */
int t4_iq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
	       unsigned int vf, unsigned int iqtype, unsigned int iqid,
	       unsigned int fl0id, unsigned int fl1id)
{
	struct fw_iq_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
				  F_FW_CMD_EXEC | V_FW_IQ_CMD_PFN(pf) |
				  V_FW_IQ_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_IQ_CMD_FREE | FW_LEN16(c));
	c.type_to_iqandstindex = cpu_to_be32(V_FW_IQ_CMD_TYPE(iqtype));
	c.iqid = cpu_to_be16(iqid);
	c.fl0id = cpu_to_be16(fl0id);
	c.fl1id = cpu_to_be16(fl1id);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_eth_eq_free - free an Ethernet egress queue
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@pf: the PF owning the queue
 *	@vf: the VF owning the queue
 *	@eqid: egress queue id
 *
 *	Frees an Ethernet egress queue.
 */
int t4_eth_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		   unsigned int vf, unsigned int eqid)
{
	struct fw_eq_eth_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_EQ_ETH_CMD) |
				  F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				  V_FW_EQ_ETH_CMD_PFN(pf) |
				  V_FW_EQ_ETH_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_EQ_ETH_CMD_FREE | FW_LEN16(c));
	c.eqid_pkd = cpu_to_be32(V_FW_EQ_ETH_CMD_EQID(eqid));
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_ctrl_eq_free - free a control egress queue
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@pf: the PF owning the queue
 *	@vf: the VF owning the queue
 *	@eqid: egress queue id
 *
 *	Frees a control egress queue.
 */
int t4_ctrl_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int eqid)
{
	struct fw_eq_ctrl_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_EQ_CTRL_CMD) |
				  F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				  V_FW_EQ_CTRL_CMD_PFN(pf) |
				  V_FW_EQ_CTRL_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_EQ_CTRL_CMD_FREE | FW_LEN16(c));
	c.cmpliqid_eqid = cpu_to_be32(V_FW_EQ_CTRL_CMD_EQID(eqid));
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_ofld_eq_free - free an offload egress queue
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@pf: the PF owning the queue
 *	@vf: the VF owning the queue
 *	@eqid: egress queue id
 *
 *	Frees a control egress queue.
 */
int t4_ofld_eq_free(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int eqid)
{
	struct fw_eq_ofld_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_EQ_OFLD_CMD) |
				  F_FW_CMD_REQUEST | F_FW_CMD_EXEC |
				  V_FW_EQ_OFLD_CMD_PFN(pf) |
				  V_FW_EQ_OFLD_CMD_VFN(vf));
	c.alloc_to_len16 = cpu_to_be32(F_FW_EQ_OFLD_CMD_FREE | FW_LEN16(c));
	c.eqid_pkd = cpu_to_be32(V_FW_EQ_OFLD_CMD_EQID(eqid));
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_link_down_rc_str - return a string for a Link Down Reason Code
 *	@link_down_rc: Link Down Reason Code
 *
 *	Returns a string representation of the Link Down Reason Code.
 */
const char *t4_link_down_rc_str(unsigned char link_down_rc)
{
	static const char *reason[] = {
		"Link Down",
		"Remote Fault",
		"Auto-negotiation Failure",
		"Reserved3",
		"Insufficient Airflow",
		"Unable To Determine Reason",
		"No RX Signal Detected",
		"Reserved7",
	};

	if (link_down_rc >= ARRAY_SIZE(reason))
		return "Bad Reason Code";

	return reason[link_down_rc];
}

/**
 *	t4_handle_fw_rpl - process a FW reply message
 *	@adap: the adapter
 *	@rpl: start of the FW message
 *
 *	Processes a FW message, such as link state change messages.
 */
int t4_handle_fw_rpl(struct adapter *adap, const __be64 *rpl)
{
	u8 opcode = *(const u8 *)rpl;
	const struct fw_port_cmd *p = (const void *)rpl;
	unsigned int action =
			G_FW_PORT_CMD_ACTION(be32_to_cpu(p->action_to_len16));

	if (opcode == FW_PORT_CMD && action == FW_PORT_ACTION_GET_PORT_INFO) {
		/* link/module state change message */
		int speed = 0, fc = 0, i;
		int chan = G_FW_PORT_CMD_PORTID(be32_to_cpu(p->op_to_portid));
		struct port_info *pi = NULL;
		struct link_config *lc;
		u32 stat = be32_to_cpu(p->u.info.lstatus_to_modtype);
		int link_ok = (stat & F_FW_PORT_CMD_LSTATUS) != 0;
		u32 mod = G_FW_PORT_CMD_MODTYPE(stat);

		if (stat & F_FW_PORT_CMD_RXPAUSE)
			fc |= PAUSE_RX;
		if (stat & F_FW_PORT_CMD_TXPAUSE)
			fc |= PAUSE_TX;
		if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_100M))
			speed = 100;
		else if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_1G))
			speed = 1000;
		else if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_10G))
			speed = 10000;
		else if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_40G))
			speed = 40000;

		for_each_port(adap, i) {
			pi = adap2pinfo(adap, i);
			if (pi->tx_chan == chan)
				break;
		}
		lc = &pi->link_cfg;

		if (mod != pi->mod_type) {
			pi->mod_type = mod;
			t4_os_portmod_changed(adap, i);
		}
		if (link_ok != lc->link_ok || speed != lc->speed ||
		    fc != lc->fc) {                    /* something changed */
			int reason;

			if (!link_ok && lc->link_ok)
				reason = G_FW_PORT_CMD_LINKDNRC(stat);
			else
				reason = -1;

			lc->link_ok = link_ok;
			lc->speed = speed;
			lc->fc = fc;
			lc->supported = be16_to_cpu(p->u.info.pcap);
			t4_os_link_changed(adap, i, link_ok, reason);
		}
	} else {
		CH_WARN_RATELIMIT(adap, "Unknown firmware reply %d\n", opcode);
		return -EINVAL;
	}
	return 0;
}

/**
 *	get_pci_mode - determine a card's PCI mode
 *	@adapter: the adapter
 *	@p: where to store the PCI settings
 *
 *	Determines a card's PCI mode and associated parameters, such as speed
 *	and width.
 */
static void get_pci_mode(struct adapter *adapter,
				   struct pci_params *p)
{
	u16 val;
	u32 pcie_cap;

	pcie_cap = t4_os_find_pci_capability(adapter, PCI_CAP_ID_EXP);
	if (pcie_cap) {
		t4_os_pci_read_cfg2(adapter, pcie_cap + PCI_EXP_LNKSTA, &val);
		p->speed = val & PCI_EXP_LNKSTA_CLS;
		p->width = (val & PCI_EXP_LNKSTA_NLW) >> 4;
	}
}

/**
 *	init_link_config - initialize a link's SW state
 *	@lc: structure holding the link state
 *	@caps: link capabilities
 *
 *	Initializes the SW state maintained for each link, including the link's
 *	capabilities and default speed/flow-control/autonegotiation settings.
 */
static void init_link_config(struct link_config *lc, unsigned int caps)
{
	lc->supported = caps;
	lc->requested_speed = 0;
	lc->speed = 0;
	lc->requested_fc = lc->fc = PAUSE_RX | PAUSE_TX;
	if (lc->supported & FW_PORT_CAP_ANEG) {
		lc->advertising = lc->supported & ADVERT_MASK;
		lc->autoneg = AUTONEG_ENABLE;
		lc->requested_fc |= PAUSE_AUTONEG;
	} else {
		lc->advertising = 0;
		lc->autoneg = AUTONEG_DISABLE;
	}
}

struct flash_desc {
	u32 vendor_and_model_id;
	u32 size_mb;
};

int t4_get_flash_params(struct adapter *adapter)
{
	/*
	 * Table for non-Numonix supported flash parts.  Numonix parts are left
	 * to the preexisting well-tested code.  All flash parts have 64KB
	 * sectors.
	 */
	static struct flash_desc supported_flash[] = {
		{ 0x150201, 4 << 20 },       /* Spansion 4MB S25FL032P */
	};

	int ret;
	u32 info = 0;

	ret = sf1_write(adapter, 1, 1, 0, SF_RD_ID);
	if (!ret)
		ret = sf1_read(adapter, 3, 0, 1, &info);
	t4_write_reg(adapter, A_SF_OP, 0);	/* unlock SF */
	if (ret < 0)
		return ret;

	for (ret = 0; ret < ARRAY_SIZE(supported_flash); ++ret)
		if (supported_flash[ret].vendor_and_model_id == info) {
			adapter->params.sf_size = supported_flash[ret].size_mb;
			adapter->params.sf_nsec =
				adapter->params.sf_size / SF_SEC_SIZE;
			return 0;
		}

	if ((info & 0xff) != 0x20)		/* not a Numonix flash */
		return -EINVAL;
	info >>= 16;				/* log2 of size */
	if (info >= 0x14 && info < 0x18)
		adapter->params.sf_nsec = 1 << (info - 16);
	else if (info == 0x18)
		adapter->params.sf_nsec = 64;
	else
		return -EINVAL;
	adapter->params.sf_size = 1 << info;

	/*
	 * We should ~probably~ reject adapters with FLASHes which are too
	 * small but we have some legacy FPGAs with small FLASHes that we'd
	 * still like to use.  So instead we emit a scary message ...
	 */
	if (adapter->params.sf_size < FLASH_MIN_SIZE)
		CH_WARN(adapter, "WARNING!!! FLASH size %#x < %#x!!!\n",
			adapter->params.sf_size, FLASH_MIN_SIZE);

	return 0;
}

static void set_pcie_completion_timeout(struct adapter *adapter,
						  u8 range)
{
	u16 val;
	u32 pcie_cap;

	pcie_cap = t4_os_find_pci_capability(adapter, PCI_CAP_ID_EXP);
	if (pcie_cap) {
		t4_os_pci_read_cfg2(adapter, pcie_cap + PCI_EXP_DEVCTL2, &val);
		val &= 0xfff0;
		val |= range ;
		t4_os_pci_write_cfg2(adapter, pcie_cap + PCI_EXP_DEVCTL2, val);
	}
}

static const struct chip_params *get_chip_params(int chipid)
{
	static const struct chip_params chip_params[] = {
		{
			/* T4 */
			.nchan = NCHAN,
			.pm_stats_cnt = PM_NSTATS,
			.cng_ch_bits_log = 2,
			.nsched_cls = 15,
			.cim_num_obq = CIM_NUM_OBQ,
			.mps_rplc_size = 128,
			.vfcount = 128,
			.sge_fl_db = F_DBPRIO,
			.mps_tcam_size = NUM_MPS_CLS_SRAM_L_INSTANCES,
		},
		{
			/* T5 */
			.nchan = NCHAN,
			.pm_stats_cnt = PM_NSTATS,
			.cng_ch_bits_log = 2,
			.nsched_cls = 16,
			.cim_num_obq = CIM_NUM_OBQ_T5,
			.mps_rplc_size = 128,
			.vfcount = 128,
			.sge_fl_db = F_DBPRIO | F_DBTYPE,
			.mps_tcam_size = NUM_MPS_T5_CLS_SRAM_L_INSTANCES,
		},
		{
			/* T6 */
			.nchan = T6_NCHAN,
			.pm_stats_cnt = T6_PM_NSTATS,
			.cng_ch_bits_log = 3,
			.nsched_cls = 16,
			.cim_num_obq = CIM_NUM_OBQ_T5,
			.mps_rplc_size = 256,
			.vfcount = 256,
			.sge_fl_db = 0,
			.mps_tcam_size = NUM_MPS_T5_CLS_SRAM_L_INSTANCES,
		},
	};

	chipid -= CHELSIO_T4;
	if (chipid < 0 || chipid >= ARRAY_SIZE(chip_params))
		return NULL;

	return &chip_params[chipid];
}

/**
 *	t4_prep_adapter - prepare SW and HW for operation
 *	@adapter: the adapter
 *	@buf: temporary space of at least VPD_LEN size provided by the caller.
 *
 *	Initialize adapter SW state for the various HW modules, set initial
 *	values for some adapter tunables, take PHYs out of reset, and
 *	initialize the MDIO interface.
 */
int t4_prep_adapter(struct adapter *adapter, u8 *buf)
{
	int ret;
	uint16_t device_id;
	uint32_t pl_rev;

	get_pci_mode(adapter, &adapter->params.pci);

	pl_rev = t4_read_reg(adapter, A_PL_REV);
	adapter->params.chipid = G_CHIPID(pl_rev);
	adapter->params.rev = G_REV(pl_rev);
	if (adapter->params.chipid == 0) {
		/* T4 did not have chipid in PL_REV (T5 onwards do) */
		adapter->params.chipid = CHELSIO_T4;

		/* T4A1 chip is not supported */
		if (adapter->params.rev == 1) {
			CH_ALERT(adapter, "T4 rev 1 chip is not supported.\n");
			return -EINVAL;
		}
	}

	adapter->chip_params = get_chip_params(chip_id(adapter));
	if (adapter->chip_params == NULL)
		return -EINVAL;

	adapter->params.pci.vpd_cap_addr =
	    t4_os_find_pci_capability(adapter, PCI_CAP_ID_VPD);

	ret = t4_get_flash_params(adapter);
	if (ret < 0)
		return ret;

	ret = get_vpd_params(adapter, &adapter->params.vpd, buf);
	if (ret < 0)
		return ret;

	/* Cards with real ASICs have the chipid in the PCIe device id */
	t4_os_pci_read_cfg2(adapter, PCI_DEVICE_ID, &device_id);
	if (device_id >> 12 == chip_id(adapter))
		adapter->params.cim_la_size = CIMLA_SIZE;
	else {
		/* FPGA */
		adapter->params.fpga = 1;
		adapter->params.cim_la_size = 2 * CIMLA_SIZE;
	}

	init_cong_ctrl(adapter->params.a_wnd, adapter->params.b_wnd);

	/*
	 * Default port and clock for debugging in case we can't reach FW.
	 */
	adapter->params.nports = 1;
	adapter->params.portvec = 1;
	adapter->params.vpd.cclk = 50000;

	/* Set pci completion timeout value to 4 seconds. */
	set_pcie_completion_timeout(adapter, 0xd);
	return 0;
}

/**
 *	t4_shutdown_adapter - shut down adapter, host & wire
 *	@adapter: the adapter
 *
 *	Perform an emergency shutdown of the adapter and stop it from
 *	continuing any further communication on the ports or DMA to the
 *	host.  This is typically used when the adapter and/or firmware
 *	have crashed and we want to prevent any further accidental
 *	communication with the rest of the world.  This will also force
 *	the port Link Status to go down -- if register writes work --
 *	which should help our peers figure out that we're down.
 */
int t4_shutdown_adapter(struct adapter *adapter)
{
	int port;

	t4_intr_disable(adapter);
	t4_write_reg(adapter, A_DBG_GPIO_EN, 0);
	for_each_port(adapter, port) {
		u32 a_port_cfg = PORT_REG(port,
					  is_t4(adapter)
					  ? A_XGMAC_PORT_CFG
					  : A_MAC_PORT_CFG);

		t4_write_reg(adapter, a_port_cfg,
			     t4_read_reg(adapter, a_port_cfg)
			     & ~V_SIGNAL_DET(1));
	}
	t4_set_reg_field(adapter, A_SGE_CONTROL, F_GLOBALENABLE, 0);

	return 0;
}

/**
 *	t4_init_devlog_params - initialize adapter->params.devlog
 *	@adap: the adapter
 *	@fw_attach: whether we can talk to the firmware
 *
 *	Initialize various fields of the adapter's Firmware Device Log
 *	Parameters structure.
 */
int t4_init_devlog_params(struct adapter *adap, int fw_attach)
{
	struct devlog_params *dparams = &adap->params.devlog;
	u32 pf_dparams;
	unsigned int devlog_meminfo;
	struct fw_devlog_cmd devlog_cmd;
	int ret;

	/* If we're dealing with newer firmware, the Device Log Paramerters
	 * are stored in a designated register which allows us to access the
	 * Device Log even if we can't talk to the firmware.
	 */
	pf_dparams =
		t4_read_reg(adap, PCIE_FW_REG(A_PCIE_FW_PF, PCIE_FW_PF_DEVLOG));
	if (pf_dparams) {
		unsigned int nentries, nentries128;

		dparams->memtype = G_PCIE_FW_PF_DEVLOG_MEMTYPE(pf_dparams);
		dparams->start = G_PCIE_FW_PF_DEVLOG_ADDR16(pf_dparams) << 4;

		nentries128 = G_PCIE_FW_PF_DEVLOG_NENTRIES128(pf_dparams);
		nentries = (nentries128 + 1) * 128;
		dparams->size = nentries * sizeof(struct fw_devlog_e);

		return 0;
	}

	/*
	 * For any failing returns ...
	 */
	memset(dparams, 0, sizeof *dparams);

	/*
	 * If we can't talk to the firmware, there's really nothing we can do
	 * at this point.
	 */
	if (!fw_attach)
		return -ENXIO;

	/* Otherwise, ask the firmware for it's Device Log Parameters.
	 */
	memset(&devlog_cmd, 0, sizeof devlog_cmd);
	devlog_cmd.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_DEVLOG_CMD) |
					     F_FW_CMD_REQUEST | F_FW_CMD_READ);
	devlog_cmd.retval_len16 = cpu_to_be32(FW_LEN16(devlog_cmd));
	ret = t4_wr_mbox(adap, adap->mbox, &devlog_cmd, sizeof(devlog_cmd),
			 &devlog_cmd);
	if (ret)
		return ret;

	devlog_meminfo =
		be32_to_cpu(devlog_cmd.memtype_devlog_memaddr16_devlog);
	dparams->memtype = G_FW_DEVLOG_CMD_MEMTYPE_DEVLOG(devlog_meminfo);
	dparams->start = G_FW_DEVLOG_CMD_MEMADDR16_DEVLOG(devlog_meminfo) << 4;
	dparams->size = be32_to_cpu(devlog_cmd.memsize_devlog);

	return 0;
}

/**
 *	t4_init_sge_params - initialize adap->params.sge
 *	@adapter: the adapter
 *
 *	Initialize various fields of the adapter's SGE Parameters structure.
 */
int t4_init_sge_params(struct adapter *adapter)
{
	u32 r;
	struct sge_params *sp = &adapter->params.sge;
	unsigned i;

	r = t4_read_reg(adapter, A_SGE_INGRESS_RX_THRESHOLD);
	sp->counter_val[0] = G_THRESHOLD_0(r);
	sp->counter_val[1] = G_THRESHOLD_1(r);
	sp->counter_val[2] = G_THRESHOLD_2(r);
	sp->counter_val[3] = G_THRESHOLD_3(r);

	r = t4_read_reg(adapter, A_SGE_TIMER_VALUE_0_AND_1);
	sp->timer_val[0] = core_ticks_to_us(adapter, G_TIMERVALUE0(r));
	sp->timer_val[1] = core_ticks_to_us(adapter, G_TIMERVALUE1(r));
	r = t4_read_reg(adapter, A_SGE_TIMER_VALUE_2_AND_3);
	sp->timer_val[2] = core_ticks_to_us(adapter, G_TIMERVALUE2(r));
	sp->timer_val[3] = core_ticks_to_us(adapter, G_TIMERVALUE3(r));
	r = t4_read_reg(adapter, A_SGE_TIMER_VALUE_4_AND_5);
	sp->timer_val[4] = core_ticks_to_us(adapter, G_TIMERVALUE4(r));
	sp->timer_val[5] = core_ticks_to_us(adapter, G_TIMERVALUE5(r));

	r = t4_read_reg(adapter, A_SGE_CONM_CTRL);
	sp->fl_starve_threshold = G_EGRTHRESHOLD(r) * 2 + 1;
	if (is_t4(adapter))
		sp->fl_starve_threshold2 = sp->fl_starve_threshold;
	else
		sp->fl_starve_threshold2 = G_EGRTHRESHOLDPACKING(r) * 2 + 1;

	/* egress queues: log2 of # of doorbells per BAR2 page */
	r = t4_read_reg(adapter, A_SGE_EGRESS_QUEUES_PER_PAGE_PF);
	r >>= S_QUEUESPERPAGEPF0 +
	    (S_QUEUESPERPAGEPF1 - S_QUEUESPERPAGEPF0) * adapter->pf;
	sp->eq_s_qpp = r & M_QUEUESPERPAGEPF0;

	/* ingress queues: log2 of # of doorbells per BAR2 page */
	r = t4_read_reg(adapter, A_SGE_INGRESS_QUEUES_PER_PAGE_PF);
	r >>= S_QUEUESPERPAGEPF0 +
	    (S_QUEUESPERPAGEPF1 - S_QUEUESPERPAGEPF0) * adapter->pf;
	sp->iq_s_qpp = r & M_QUEUESPERPAGEPF0;

	r = t4_read_reg(adapter, A_SGE_HOST_PAGE_SIZE);
	r >>= S_HOSTPAGESIZEPF0 +
	    (S_HOSTPAGESIZEPF1 - S_HOSTPAGESIZEPF0) * adapter->pf;
	sp->page_shift = (r & M_HOSTPAGESIZEPF0) + 10;

	r = t4_read_reg(adapter, A_SGE_CONTROL);
	sp->sge_control = r;
	sp->spg_len = r & F_EGRSTATUSPAGESIZE ? 128 : 64;
	sp->fl_pktshift = G_PKTSHIFT(r);
	sp->pad_boundary = 1 << (G_INGPADBOUNDARY(r) + 5);
	if (is_t4(adapter))
		sp->pack_boundary = sp->pad_boundary;
	else {
		r = t4_read_reg(adapter, A_SGE_CONTROL2);
		if (G_INGPACKBOUNDARY(r) == 0)
			sp->pack_boundary = 16;
		else
			sp->pack_boundary = 1 << (G_INGPACKBOUNDARY(r) + 5);
	}
	for (i = 0; i < SGE_FLBUF_SIZES; i++)
		sp->sge_fl_buffer_size[i] = t4_read_reg(adapter,
		    A_SGE_FL_BUFFER_SIZE0 + (4 * i));

	return 0;
}

/*
 * Read and cache the adapter's compressed filter mode and ingress config.
 */
static void read_filter_mode_and_ingress_config(struct adapter *adap)
{
	struct tp_params *tpp = &adap->params.tp;

	if (t4_use_ldst(adap)) {
		t4_fw_tp_pio_rw(adap, &tpp->vlan_pri_map, 1,
				A_TP_VLAN_PRI_MAP, 1);
		t4_fw_tp_pio_rw(adap, &tpp->ingress_config, 1,
				A_TP_INGRESS_CONFIG, 1);
	} else {
		t4_read_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				 &tpp->vlan_pri_map, 1, A_TP_VLAN_PRI_MAP);
		t4_read_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA,
				 &tpp->ingress_config, 1, A_TP_INGRESS_CONFIG);
	}

	/*
	 * Now that we have TP_VLAN_PRI_MAP cached, we can calculate the field
	 * shift positions of several elements of the Compressed Filter Tuple
	 * for this adapter which we need frequently ...
	 */
	tpp->fcoe_shift = t4_filter_field_shift(adap, F_FCOE);
	tpp->port_shift = t4_filter_field_shift(adap, F_PORT);
	tpp->vnic_shift = t4_filter_field_shift(adap, F_VNIC_ID);
	tpp->vlan_shift = t4_filter_field_shift(adap, F_VLAN);
	tpp->tos_shift = t4_filter_field_shift(adap, F_TOS);
	tpp->protocol_shift = t4_filter_field_shift(adap, F_PROTOCOL);
	tpp->ethertype_shift = t4_filter_field_shift(adap, F_ETHERTYPE);
	tpp->macmatch_shift = t4_filter_field_shift(adap, F_MACMATCH);
	tpp->matchtype_shift = t4_filter_field_shift(adap, F_MPSHITTYPE);
	tpp->frag_shift = t4_filter_field_shift(adap, F_FRAGMENTATION);

	/*
	 * If TP_INGRESS_CONFIG.VNID == 0, then TP_VLAN_PRI_MAP.VNIC_ID
	 * represents the presence of an Outer VLAN instead of a VNIC ID.
	 */
	if ((tpp->ingress_config & F_VNIC) == 0)
		tpp->vnic_shift = -1;
}

/**
 *      t4_init_tp_params - initialize adap->params.tp
 *      @adap: the adapter
 *
 *      Initialize various fields of the adapter's TP Parameters structure.
 */
int t4_init_tp_params(struct adapter *adap)
{
	int chan;
	u32 v;
	struct tp_params *tpp = &adap->params.tp;

	v = t4_read_reg(adap, A_TP_TIMER_RESOLUTION);
	tpp->tre = G_TIMERRESOLUTION(v);
	tpp->dack_re = G_DELAYEDACKRESOLUTION(v);

	/* MODQ_REQ_MAP defaults to setting queues 0-3 to chan 0-3 */
	for (chan = 0; chan < MAX_NCHAN; chan++)
		tpp->tx_modq[chan] = chan;

	read_filter_mode_and_ingress_config(adap);

	/*
	 * For T6, cache the adapter's compressed error vector
	 * and passing outer header info for encapsulated packets.
	 */
	if (chip_id(adap) > CHELSIO_T5) {
		v = t4_read_reg(adap, A_TP_OUT_CONFIG);
		tpp->rx_pkt_encap = (v & F_CRXPKTENC) ? 1 : 0;
	}

	return 0;
}

/**
 *      t4_filter_field_shift - calculate filter field shift
 *      @adap: the adapter
 *      @filter_sel: the desired field (from TP_VLAN_PRI_MAP bits)
 *
 *      Return the shift position of a filter field within the Compressed
 *      Filter Tuple.  The filter field is specified via its selection bit
 *      within TP_VLAN_PRI_MAL (filter mode).  E.g. F_VLAN.
 */
int t4_filter_field_shift(const struct adapter *adap, int filter_sel)
{
	unsigned int filter_mode = adap->params.tp.vlan_pri_map;
	unsigned int sel;
	int field_shift;

	if ((filter_mode & filter_sel) == 0)
		return -1;

	for (sel = 1, field_shift = 0; sel < filter_sel; sel <<= 1) {
		switch (filter_mode & sel) {
		case F_FCOE:
			field_shift += W_FT_FCOE;
			break;
		case F_PORT:
			field_shift += W_FT_PORT;
			break;
		case F_VNIC_ID:
			field_shift += W_FT_VNIC_ID;
			break;
		case F_VLAN:
			field_shift += W_FT_VLAN;
			break;
		case F_TOS:
			field_shift += W_FT_TOS;
			break;
		case F_PROTOCOL:
			field_shift += W_FT_PROTOCOL;
			break;
		case F_ETHERTYPE:
			field_shift += W_FT_ETHERTYPE;
			break;
		case F_MACMATCH:
			field_shift += W_FT_MACMATCH;
			break;
		case F_MPSHITTYPE:
			field_shift += W_FT_MPSHITTYPE;
			break;
		case F_FRAGMENTATION:
			field_shift += W_FT_FRAGMENTATION;
			break;
		}
	}
	return field_shift;
}

int t4_port_init(struct adapter *adap, int mbox, int pf, int vf, int port_id)
{
	u8 addr[6];
	int ret, i, j;
	struct fw_port_cmd c;
	u16 rss_size;
	struct port_info *p = adap2pinfo(adap, port_id);
	u32 param, val;

	memset(&c, 0, sizeof(c));

	for (i = 0, j = -1; i <= p->port_id; i++) {
		do {
			j++;
		} while ((adap->params.portvec & (1 << j)) == 0);
	}

	if (!(adap->flags & IS_VF) ||
	    adap->params.vfres.r_caps & FW_CMD_CAP_PORT) {
		c.op_to_portid = htonl(V_FW_CMD_OP(FW_PORT_CMD) |
				       F_FW_CMD_REQUEST | F_FW_CMD_READ |
				       V_FW_PORT_CMD_PORTID(j));
		c.action_to_len16 = htonl(
			V_FW_PORT_CMD_ACTION(FW_PORT_ACTION_GET_PORT_INFO) |
			FW_LEN16(c));
		ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
		if (ret)
			return ret;

		ret = be32_to_cpu(c.u.info.lstatus_to_modtype);
		p->mdio_addr = (ret & F_FW_PORT_CMD_MDIOCAP) ?
			G_FW_PORT_CMD_MDIOADDR(ret) : -1;
		p->port_type = G_FW_PORT_CMD_PTYPE(ret);
		p->mod_type = G_FW_PORT_CMD_MODTYPE(ret);

		init_link_config(&p->link_cfg, be16_to_cpu(c.u.info.pcap));
	}

	ret = t4_alloc_vi(adap, mbox, j, pf, vf, 1, addr, &rss_size);
	if (ret < 0)
		return ret;

	p->vi[0].viid = ret;
	p->tx_chan = j;
	p->rx_chan_map = t4_get_mps_bg_map(adap, j);
	p->lport = j;
	p->vi[0].rss_size = rss_size;
	t4_os_set_hw_addr(adap, p->port_id, addr);

	param = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
	    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_RSSINFO) |
	    V_FW_PARAMS_PARAM_YZ(p->vi[0].viid);
	ret = t4_query_params(adap, mbox, pf, vf, 1, &param, &val);
	if (ret)
		p->vi[0].rss_base = 0xffff;
	else {
		/* MPASS((val >> 16) == rss_size); */
		p->vi[0].rss_base = val & 0xffff;
	}

	return 0;
}

/**
 *	t4_read_cimq_cfg - read CIM queue configuration
 *	@adap: the adapter
 *	@base: holds the queue base addresses in bytes
 *	@size: holds the queue sizes in bytes
 *	@thres: holds the queue full thresholds in bytes
 *
 *	Returns the current configuration of the CIM queues, starting with
 *	the IBQs, then the OBQs.
 */
void t4_read_cimq_cfg(struct adapter *adap, u16 *base, u16 *size, u16 *thres)
{
	unsigned int i, v;
	int cim_num_obq = adap->chip_params->cim_num_obq;

	for (i = 0; i < CIM_NUM_IBQ; i++) {
		t4_write_reg(adap, A_CIM_QUEUE_CONFIG_REF, F_IBQSELECT |
			     V_QUENUMSELECT(i));
		v = t4_read_reg(adap, A_CIM_QUEUE_CONFIG_CTRL);
		/* value is in 256-byte units */
		*base++ = G_CIMQBASE(v) * 256;
		*size++ = G_CIMQSIZE(v) * 256;
		*thres++ = G_QUEFULLTHRSH(v) * 8; /* 8-byte unit */
	}
	for (i = 0; i < cim_num_obq; i++) {
		t4_write_reg(adap, A_CIM_QUEUE_CONFIG_REF, F_OBQSELECT |
			     V_QUENUMSELECT(i));
		v = t4_read_reg(adap, A_CIM_QUEUE_CONFIG_CTRL);
		/* value is in 256-byte units */
		*base++ = G_CIMQBASE(v) * 256;
		*size++ = G_CIMQSIZE(v) * 256;
	}
}

/**
 *	t4_read_cim_ibq - read the contents of a CIM inbound queue
 *	@adap: the adapter
 *	@qid: the queue index
 *	@data: where to store the queue contents
 *	@n: capacity of @data in 32-bit words
 *
 *	Reads the contents of the selected CIM queue starting at address 0 up
 *	to the capacity of @data.  @n must be a multiple of 4.  Returns < 0 on
 *	error and the number of 32-bit words actually read on success.
 */
int t4_read_cim_ibq(struct adapter *adap, unsigned int qid, u32 *data, size_t n)
{
	int i, err, attempts;
	unsigned int addr;
	const unsigned int nwords = CIM_IBQ_SIZE * 4;

	if (qid > 5 || (n & 3))
		return -EINVAL;

	addr = qid * nwords;
	if (n > nwords)
		n = nwords;

	/* It might take 3-10ms before the IBQ debug read access is allowed.
	 * Wait for 1 Sec with a delay of 1 usec.
	 */
	attempts = 1000000;

	for (i = 0; i < n; i++, addr++) {
		t4_write_reg(adap, A_CIM_IBQ_DBG_CFG, V_IBQDBGADDR(addr) |
			     F_IBQDBGEN);
		err = t4_wait_op_done(adap, A_CIM_IBQ_DBG_CFG, F_IBQDBGBUSY, 0,
				      attempts, 1);
		if (err)
			return err;
		*data++ = t4_read_reg(adap, A_CIM_IBQ_DBG_DATA);
	}
	t4_write_reg(adap, A_CIM_IBQ_DBG_CFG, 0);
	return i;
}

/**
 *	t4_read_cim_obq - read the contents of a CIM outbound queue
 *	@adap: the adapter
 *	@qid: the queue index
 *	@data: where to store the queue contents
 *	@n: capacity of @data in 32-bit words
 *
 *	Reads the contents of the selected CIM queue starting at address 0 up
 *	to the capacity of @data.  @n must be a multiple of 4.  Returns < 0 on
 *	error and the number of 32-bit words actually read on success.
 */
int t4_read_cim_obq(struct adapter *adap, unsigned int qid, u32 *data, size_t n)
{
	int i, err;
	unsigned int addr, v, nwords;
	int cim_num_obq = adap->chip_params->cim_num_obq;

	if ((qid > (cim_num_obq - 1)) || (n & 3))
		return -EINVAL;

	t4_write_reg(adap, A_CIM_QUEUE_CONFIG_REF, F_OBQSELECT |
		     V_QUENUMSELECT(qid));
	v = t4_read_reg(adap, A_CIM_QUEUE_CONFIG_CTRL);

	addr = G_CIMQBASE(v) * 64;    /* muliple of 256 -> muliple of 4 */
	nwords = G_CIMQSIZE(v) * 64;  /* same */
	if (n > nwords)
		n = nwords;

	for (i = 0; i < n; i++, addr++) {
		t4_write_reg(adap, A_CIM_OBQ_DBG_CFG, V_OBQDBGADDR(addr) |
			     F_OBQDBGEN);
		err = t4_wait_op_done(adap, A_CIM_OBQ_DBG_CFG, F_OBQDBGBUSY, 0,
				      2, 1);
		if (err)
			return err;
		*data++ = t4_read_reg(adap, A_CIM_OBQ_DBG_DATA);
	}
	t4_write_reg(adap, A_CIM_OBQ_DBG_CFG, 0);
	return i;
}

enum {
	CIM_QCTL_BASE     = 0,
	CIM_CTL_BASE      = 0x2000,
	CIM_PBT_ADDR_BASE = 0x2800,
	CIM_PBT_LRF_BASE  = 0x3000,
	CIM_PBT_DATA_BASE = 0x3800
};

/**
 *	t4_cim_read - read a block from CIM internal address space
 *	@adap: the adapter
 *	@addr: the start address within the CIM address space
 *	@n: number of words to read
 *	@valp: where to store the result
 *
 *	Reads a block of 4-byte words from the CIM intenal address space.
 */
int t4_cim_read(struct adapter *adap, unsigned int addr, unsigned int n,
		unsigned int *valp)
{
	int ret = 0;

	if (t4_read_reg(adap, A_CIM_HOST_ACC_CTRL) & F_HOSTBUSY)
		return -EBUSY;

	for ( ; !ret && n--; addr += 4) {
		t4_write_reg(adap, A_CIM_HOST_ACC_CTRL, addr);
		ret = t4_wait_op_done(adap, A_CIM_HOST_ACC_CTRL, F_HOSTBUSY,
				      0, 5, 2);
		if (!ret)
			*valp++ = t4_read_reg(adap, A_CIM_HOST_ACC_DATA);
	}
	return ret;
}

/**
 *	t4_cim_write - write a block into CIM internal address space
 *	@adap: the adapter
 *	@addr: the start address within the CIM address space
 *	@n: number of words to write
 *	@valp: set of values to write
 *
 *	Writes a block of 4-byte words into the CIM intenal address space.
 */
int t4_cim_write(struct adapter *adap, unsigned int addr, unsigned int n,
		 const unsigned int *valp)
{
	int ret = 0;

	if (t4_read_reg(adap, A_CIM_HOST_ACC_CTRL) & F_HOSTBUSY)
		return -EBUSY;

	for ( ; !ret && n--; addr += 4) {
		t4_write_reg(adap, A_CIM_HOST_ACC_DATA, *valp++);
		t4_write_reg(adap, A_CIM_HOST_ACC_CTRL, addr | F_HOSTWRITE);
		ret = t4_wait_op_done(adap, A_CIM_HOST_ACC_CTRL, F_HOSTBUSY,
				      0, 5, 2);
	}
	return ret;
}

static int t4_cim_write1(struct adapter *adap, unsigned int addr,
			 unsigned int val)
{
	return t4_cim_write(adap, addr, 1, &val);
}

/**
 *	t4_cim_ctl_read - read a block from CIM control region
 *	@adap: the adapter
 *	@addr: the start address within the CIM control region
 *	@n: number of words to read
 *	@valp: where to store the result
 *
 *	Reads a block of 4-byte words from the CIM control region.
 */
int t4_cim_ctl_read(struct adapter *adap, unsigned int addr, unsigned int n,
		    unsigned int *valp)
{
	return t4_cim_read(adap, addr + CIM_CTL_BASE, n, valp);
}

/**
 *	t4_cim_read_la - read CIM LA capture buffer
 *	@adap: the adapter
 *	@la_buf: where to store the LA data
 *	@wrptr: the HW write pointer within the capture buffer
 *
 *	Reads the contents of the CIM LA buffer with the most recent entry at
 *	the end	of the returned data and with the entry at @wrptr first.
 *	We try to leave the LA in the running state we find it in.
 */
int t4_cim_read_la(struct adapter *adap, u32 *la_buf, unsigned int *wrptr)
{
	int i, ret;
	unsigned int cfg, val, idx;

	ret = t4_cim_read(adap, A_UP_UP_DBG_LA_CFG, 1, &cfg);
	if (ret)
		return ret;

	if (cfg & F_UPDBGLAEN) {	/* LA is running, freeze it */
		ret = t4_cim_write1(adap, A_UP_UP_DBG_LA_CFG, 0);
		if (ret)
			return ret;
	}

	ret = t4_cim_read(adap, A_UP_UP_DBG_LA_CFG, 1, &val);
	if (ret)
		goto restart;

	idx = G_UPDBGLAWRPTR(val);
	if (wrptr)
		*wrptr = idx;

	for (i = 0; i < adap->params.cim_la_size; i++) {
		ret = t4_cim_write1(adap, A_UP_UP_DBG_LA_CFG,
				    V_UPDBGLARDPTR(idx) | F_UPDBGLARDEN);
		if (ret)
			break;
		ret = t4_cim_read(adap, A_UP_UP_DBG_LA_CFG, 1, &val);
		if (ret)
			break;
		if (val & F_UPDBGLARDEN) {
			ret = -ETIMEDOUT;
			break;
		}
		ret = t4_cim_read(adap, A_UP_UP_DBG_LA_DATA, 1, &la_buf[i]);
		if (ret)
			break;

		/* address can't exceed 0xfff (UpDbgLaRdPtr is of 12-bits) */
		idx = (idx + 1) & M_UPDBGLARDPTR;
		/*
		 * Bits 0-3 of UpDbgLaRdPtr can be between 0000 to 1001 to
		 * identify the 32-bit portion of the full 312-bit data
		 */
		if (is_t6(adap))
			while ((idx & 0xf) > 9)
				idx = (idx + 1) % M_UPDBGLARDPTR;
	}
restart:
	if (cfg & F_UPDBGLAEN) {
		int r = t4_cim_write1(adap, A_UP_UP_DBG_LA_CFG,
				      cfg & ~F_UPDBGLARDEN);
		if (!ret)
			ret = r;
	}
	return ret;
}

/**
 *	t4_tp_read_la - read TP LA capture buffer
 *	@adap: the adapter
 *	@la_buf: where to store the LA data
 *	@wrptr: the HW write pointer within the capture buffer
 *
 *	Reads the contents of the TP LA buffer with the most recent entry at
 *	the end	of the returned data and with the entry at @wrptr first.
 *	We leave the LA in the running state we find it in.
 */
void t4_tp_read_la(struct adapter *adap, u64 *la_buf, unsigned int *wrptr)
{
	bool last_incomplete;
	unsigned int i, cfg, val, idx;

	cfg = t4_read_reg(adap, A_TP_DBG_LA_CONFIG) & 0xffff;
	if (cfg & F_DBGLAENABLE)			/* freeze LA */
		t4_write_reg(adap, A_TP_DBG_LA_CONFIG,
			     adap->params.tp.la_mask | (cfg ^ F_DBGLAENABLE));

	val = t4_read_reg(adap, A_TP_DBG_LA_CONFIG);
	idx = G_DBGLAWPTR(val);
	last_incomplete = G_DBGLAMODE(val) >= 2 && (val & F_DBGLAWHLF) == 0;
	if (last_incomplete)
		idx = (idx + 1) & M_DBGLARPTR;
	if (wrptr)
		*wrptr = idx;

	val &= 0xffff;
	val &= ~V_DBGLARPTR(M_DBGLARPTR);
	val |= adap->params.tp.la_mask;

	for (i = 0; i < TPLA_SIZE; i++) {
		t4_write_reg(adap, A_TP_DBG_LA_CONFIG, V_DBGLARPTR(idx) | val);
		la_buf[i] = t4_read_reg64(adap, A_TP_DBG_LA_DATAL);
		idx = (idx + 1) & M_DBGLARPTR;
	}

	/* Wipe out last entry if it isn't valid */
	if (last_incomplete)
		la_buf[TPLA_SIZE - 1] = ~0ULL;

	if (cfg & F_DBGLAENABLE)		/* restore running state */
		t4_write_reg(adap, A_TP_DBG_LA_CONFIG,
			     cfg | adap->params.tp.la_mask);
}

/*
 * SGE Hung Ingress DMA Warning Threshold time and Warning Repeat Rate (in
 * seconds).  If we find one of the SGE Ingress DMA State Machines in the same
 * state for more than the Warning Threshold then we'll issue a warning about
 * a potential hang.  We'll repeat the warning as the SGE Ingress DMA Channel
 * appears to be hung every Warning Repeat second till the situation clears.
 * If the situation clears, we'll note that as well.
 */
#define SGE_IDMA_WARN_THRESH 1
#define SGE_IDMA_WARN_REPEAT 300

/**
 *	t4_idma_monitor_init - initialize SGE Ingress DMA Monitor
 *	@adapter: the adapter
 *	@idma: the adapter IDMA Monitor state
 *
 *	Initialize the state of an SGE Ingress DMA Monitor.
 */
void t4_idma_monitor_init(struct adapter *adapter,
			  struct sge_idma_monitor_state *idma)
{
	/* Initialize the state variables for detecting an SGE Ingress DMA
	 * hang.  The SGE has internal counters which count up on each clock
	 * tick whenever the SGE finds its Ingress DMA State Engines in the
	 * same state they were on the previous clock tick.  The clock used is
	 * the Core Clock so we have a limit on the maximum "time" they can
	 * record; typically a very small number of seconds.  For instance,
	 * with a 600MHz Core Clock, we can only count up to a bit more than
	 * 7s.  So we'll synthesize a larger counter in order to not run the
	 * risk of having the "timers" overflow and give us the flexibility to
	 * maintain a Hung SGE State Machine of our own which operates across
	 * a longer time frame.
	 */
	idma->idma_1s_thresh = core_ticks_per_usec(adapter) * 1000000; /* 1s */
	idma->idma_stalled[0] = idma->idma_stalled[1] = 0;
}

/**
 *	t4_idma_monitor - monitor SGE Ingress DMA state
 *	@adapter: the adapter
 *	@idma: the adapter IDMA Monitor state
 *	@hz: number of ticks/second
 *	@ticks: number of ticks since the last IDMA Monitor call
 */
void t4_idma_monitor(struct adapter *adapter,
		     struct sge_idma_monitor_state *idma,
		     int hz, int ticks)
{
	int i, idma_same_state_cnt[2];

	 /* Read the SGE Debug Ingress DMA Same State Count registers.  These
	  * are counters inside the SGE which count up on each clock when the
	  * SGE finds its Ingress DMA State Engines in the same states they
	  * were in the previous clock.  The counters will peg out at
	  * 0xffffffff without wrapping around so once they pass the 1s
	  * threshold they'll stay above that till the IDMA state changes.
	  */
	t4_write_reg(adapter, A_SGE_DEBUG_INDEX, 13);
	idma_same_state_cnt[0] = t4_read_reg(adapter, A_SGE_DEBUG_DATA_HIGH);
	idma_same_state_cnt[1] = t4_read_reg(adapter, A_SGE_DEBUG_DATA_LOW);

	for (i = 0; i < 2; i++) {
		u32 debug0, debug11;

		/* If the Ingress DMA Same State Counter ("timer") is less
		 * than 1s, then we can reset our synthesized Stall Timer and
		 * continue.  If we have previously emitted warnings about a
		 * potential stalled Ingress Queue, issue a note indicating
		 * that the Ingress Queue has resumed forward progress.
		 */
		if (idma_same_state_cnt[i] < idma->idma_1s_thresh) {
			if (idma->idma_stalled[i] >= SGE_IDMA_WARN_THRESH*hz)
				CH_WARN(adapter, "SGE idma%d, queue %u, "
					"resumed after %d seconds\n",
					i, idma->idma_qid[i],
					idma->idma_stalled[i]/hz);
			idma->idma_stalled[i] = 0;
			continue;
		}

		/* Synthesize an SGE Ingress DMA Same State Timer in the Hz
		 * domain.  The first time we get here it'll be because we
		 * passed the 1s Threshold; each additional time it'll be
		 * because the RX Timer Callback is being fired on its regular
		 * schedule.
		 *
		 * If the stall is below our Potential Hung Ingress Queue
		 * Warning Threshold, continue.
		 */
		if (idma->idma_stalled[i] == 0) {
			idma->idma_stalled[i] = hz;
			idma->idma_warn[i] = 0;
		} else {
			idma->idma_stalled[i] += ticks;
			idma->idma_warn[i] -= ticks;
		}

		if (idma->idma_stalled[i] < SGE_IDMA_WARN_THRESH*hz)
			continue;

		/* We'll issue a warning every SGE_IDMA_WARN_REPEAT seconds.
		 */
		if (idma->idma_warn[i] > 0)
			continue;
		idma->idma_warn[i] = SGE_IDMA_WARN_REPEAT*hz;

		/* Read and save the SGE IDMA State and Queue ID information.
		 * We do this every time in case it changes across time ...
		 * can't be too careful ...
		 */
		t4_write_reg(adapter, A_SGE_DEBUG_INDEX, 0);
		debug0 = t4_read_reg(adapter, A_SGE_DEBUG_DATA_LOW);
		idma->idma_state[i] = (debug0 >> (i * 9)) & 0x3f;

		t4_write_reg(adapter, A_SGE_DEBUG_INDEX, 11);
		debug11 = t4_read_reg(adapter, A_SGE_DEBUG_DATA_LOW);
		idma->idma_qid[i] = (debug11 >> (i * 16)) & 0xffff;

		CH_WARN(adapter, "SGE idma%u, queue %u, potentially stuck in "
			" state %u for %d seconds (debug0=%#x, debug11=%#x)\n",
			i, idma->idma_qid[i], idma->idma_state[i],
			idma->idma_stalled[i]/hz,
			debug0, debug11);
		t4_sge_decode_idma_state(adapter, idma->idma_state[i]);
	}
}

/**
 *	t4_read_pace_tbl - read the pace table
 *	@adap: the adapter
 *	@pace_vals: holds the returned values
 *
 *	Returns the values of TP's pace table in microseconds.
 */
void t4_read_pace_tbl(struct adapter *adap, unsigned int pace_vals[NTX_SCHED])
{
	unsigned int i, v;

	for (i = 0; i < NTX_SCHED; i++) {
		t4_write_reg(adap, A_TP_PACE_TABLE, 0xffff0000 + i);
		v = t4_read_reg(adap, A_TP_PACE_TABLE);
		pace_vals[i] = dack_ticks_to_usec(adap, v);
	}
}

/**
 *	t4_get_tx_sched - get the configuration of a Tx HW traffic scheduler
 *	@adap: the adapter
 *	@sched: the scheduler index
 *	@kbps: the byte rate in Kbps
 *	@ipg: the interpacket delay in tenths of nanoseconds
 *
 *	Return the current configuration of a HW Tx scheduler.
 */
void t4_get_tx_sched(struct adapter *adap, unsigned int sched, unsigned int *kbps,
		     unsigned int *ipg)
{
	unsigned int v, addr, bpt, cpt;

	if (kbps) {
		addr = A_TP_TX_MOD_Q1_Q0_RATE_LIMIT - sched / 2;
		t4_write_reg(adap, A_TP_TM_PIO_ADDR, addr);
		v = t4_read_reg(adap, A_TP_TM_PIO_DATA);
		if (sched & 1)
			v >>= 16;
		bpt = (v >> 8) & 0xff;
		cpt = v & 0xff;
		if (!cpt)
			*kbps = 0;	/* scheduler disabled */
		else {
			v = (adap->params.vpd.cclk * 1000) / cpt; /* ticks/s */
			*kbps = (v * bpt) / 125;
		}
	}
	if (ipg) {
		addr = A_TP_TX_MOD_Q1_Q0_TIMER_SEPARATOR - sched / 2;
		t4_write_reg(adap, A_TP_TM_PIO_ADDR, addr);
		v = t4_read_reg(adap, A_TP_TM_PIO_DATA);
		if (sched & 1)
			v >>= 16;
		v &= 0xffff;
		*ipg = (10000 * v) / core_ticks_per_usec(adap);
	}
}

/**
 *	t4_load_cfg - download config file
 *	@adap: the adapter
 *	@cfg_data: the cfg text file to write
 *	@size: text file size
 *
 *	Write the supplied config text file to the card's serial flash.
 */
int t4_load_cfg(struct adapter *adap, const u8 *cfg_data, unsigned int size)
{
	int ret, i, n, cfg_addr;
	unsigned int addr;
	unsigned int flash_cfg_start_sec;
	unsigned int sf_sec_size = adap->params.sf_size / adap->params.sf_nsec;

	cfg_addr = t4_flash_cfg_addr(adap);
	if (cfg_addr < 0)
		return cfg_addr;

	addr = cfg_addr;
	flash_cfg_start_sec = addr / SF_SEC_SIZE;

	if (size > FLASH_CFG_MAX_SIZE) {
		CH_ERR(adap, "cfg file too large, max is %u bytes\n",
		       FLASH_CFG_MAX_SIZE);
		return -EFBIG;
	}

	i = DIV_ROUND_UP(FLASH_CFG_MAX_SIZE,	/* # of sectors spanned */
			 sf_sec_size);
	ret = t4_flash_erase_sectors(adap, flash_cfg_start_sec,
				     flash_cfg_start_sec + i - 1);
	/*
	 * If size == 0 then we're simply erasing the FLASH sectors associated
	 * with the on-adapter Firmware Configuration File.
	 */
	if (ret || size == 0)
		goto out;

	/* this will write to the flash up to SF_PAGE_SIZE at a time */
	for (i = 0; i< size; i+= SF_PAGE_SIZE) {
		if ( (size - i) <  SF_PAGE_SIZE)
			n = size - i;
		else
			n = SF_PAGE_SIZE;
		ret = t4_write_flash(adap, addr, n, cfg_data, 1);
		if (ret)
			goto out;

		addr += SF_PAGE_SIZE;
		cfg_data += SF_PAGE_SIZE;
	}

out:
	if (ret)
		CH_ERR(adap, "config file %s failed %d\n",
		       (size == 0 ? "clear" : "download"), ret);
	return ret;
}

/**
 *	t5_fw_init_extern_mem - initialize the external memory
 *	@adap: the adapter
 *
 *	Initializes the external memory on T5.
 */
int t5_fw_init_extern_mem(struct adapter *adap)
{
	u32 params[1], val[1];
	int ret;

	if (!is_t5(adap))
		return 0;

	val[0] = 0xff; /* Initialize all MCs */
	params[0] = (V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) |
			V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_MCINIT));
	ret = t4_set_params_timeout(adap, adap->mbox, adap->pf, 0, 1, params, val,
			FW_CMD_MAX_TIMEOUT);

	return ret;
}

/* BIOS boot headers */
typedef struct pci_expansion_rom_header {
	u8	signature[2]; /* ROM Signature. Should be 0xaa55 */
	u8	reserved[22]; /* Reserved per processor Architecture data */
	u8	pcir_offset[2]; /* Offset to PCI Data Structure */
} pci_exp_rom_header_t; /* PCI_EXPANSION_ROM_HEADER */

/* Legacy PCI Expansion ROM Header */
typedef struct legacy_pci_expansion_rom_header {
	u8	signature[2]; /* ROM Signature. Should be 0xaa55 */
	u8	size512; /* Current Image Size in units of 512 bytes */
	u8	initentry_point[4];
	u8	cksum; /* Checksum computed on the entire Image */
	u8	reserved[16]; /* Reserved */
	u8	pcir_offset[2]; /* Offset to PCI Data Struture */
} legacy_pci_exp_rom_header_t; /* LEGACY_PCI_EXPANSION_ROM_HEADER */

/* EFI PCI Expansion ROM Header */
typedef struct efi_pci_expansion_rom_header {
	u8	signature[2]; // ROM signature. The value 0xaa55
	u8	initialization_size[2]; /* Units 512. Includes this header */
	u8	efi_signature[4]; /* Signature from EFI image header. 0x0EF1 */
	u8	efi_subsystem[2]; /* Subsystem value for EFI image header */
	u8	efi_machine_type[2]; /* Machine type from EFI image header */
	u8	compression_type[2]; /* Compression type. */
		/*
		 * Compression type definition
		 * 0x0: uncompressed
		 * 0x1: Compressed
		 * 0x2-0xFFFF: Reserved
		 */
	u8	reserved[8]; /* Reserved */
	u8	efi_image_header_offset[2]; /* Offset to EFI Image */
	u8	pcir_offset[2]; /* Offset to PCI Data Structure */
} efi_pci_exp_rom_header_t; /* EFI PCI Expansion ROM Header */

/* PCI Data Structure Format */
typedef struct pcir_data_structure { /* PCI Data Structure */
	u8	signature[4]; /* Signature. The string "PCIR" */
	u8	vendor_id[2]; /* Vendor Identification */
	u8	device_id[2]; /* Device Identification */
	u8	vital_product[2]; /* Pointer to Vital Product Data */
	u8	length[2]; /* PCIR Data Structure Length */
	u8	revision; /* PCIR Data Structure Revision */
	u8	class_code[3]; /* Class Code */
	u8	image_length[2]; /* Image Length. Multiple of 512B */
	u8	code_revision[2]; /* Revision Level of Code/Data */
	u8	code_type; /* Code Type. */
		/*
		 * PCI Expansion ROM Code Types
		 * 0x00: Intel IA-32, PC-AT compatible. Legacy
		 * 0x01: Open Firmware standard for PCI. FCODE
		 * 0x02: Hewlett-Packard PA RISC. HP reserved
		 * 0x03: EFI Image. EFI
		 * 0x04-0xFF: Reserved.
		 */
	u8	indicator; /* Indicator. Identifies the last image in the ROM */
	u8	reserved[2]; /* Reserved */
} pcir_data_t; /* PCI__DATA_STRUCTURE */

/* BOOT constants */
enum {
	BOOT_FLASH_BOOT_ADDR = 0x0,/* start address of boot image in flash */
	BOOT_SIGNATURE = 0xaa55,   /* signature of BIOS boot ROM */
	BOOT_SIZE_INC = 512,       /* image size measured in 512B chunks */
	BOOT_MIN_SIZE = sizeof(pci_exp_rom_header_t), /* basic header */
	BOOT_MAX_SIZE = 1024*BOOT_SIZE_INC, /* 1 byte * length increment  */
	VENDOR_ID = 0x1425, /* Vendor ID */
	PCIR_SIGNATURE = 0x52494350 /* PCIR signature */
};

/*
 *	modify_device_id - Modifies the device ID of the Boot BIOS image
 *	@adatper: the device ID to write.
 *	@boot_data: the boot image to modify.
 *
 *	Write the supplied device ID to the boot BIOS image.
 */
static void modify_device_id(int device_id, u8 *boot_data)
{
	legacy_pci_exp_rom_header_t *header;
	pcir_data_t *pcir_header;
	u32 cur_header = 0;

	/*
	 * Loop through all chained images and change the device ID's
	 */
	while (1) {
		header = (legacy_pci_exp_rom_header_t *) &boot_data[cur_header];
		pcir_header = (pcir_data_t *) &boot_data[cur_header +
			      le16_to_cpu(*(u16*)header->pcir_offset)];

		/*
		 * Only modify the Device ID if code type is Legacy or HP.
		 * 0x00: Okay to modify
		 * 0x01: FCODE. Do not be modify
		 * 0x03: Okay to modify
		 * 0x04-0xFF: Do not modify
		 */
		if (pcir_header->code_type == 0x00) {
			u8 csum = 0;
			int i;

			/*
			 * Modify Device ID to match current adatper
			 */
			*(u16*) pcir_header->device_id = device_id;

			/*
			 * Set checksum temporarily to 0.
			 * We will recalculate it later.
			 */
			header->cksum = 0x0;

			/*
			 * Calculate and update checksum
			 */
			for (i = 0; i < (header->size512 * 512); i++)
				csum += (u8)boot_data[cur_header + i];

			/*
			 * Invert summed value to create the checksum
			 * Writing new checksum value directly to the boot data
			 */
			boot_data[cur_header + 7] = -csum;

		} else if (pcir_header->code_type == 0x03) {

			/*
			 * Modify Device ID to match current adatper
			 */
			*(u16*) pcir_header->device_id = device_id;

		}


		/*
		 * Check indicator element to identify if this is the last
		 * image in the ROM.
		 */
		if (pcir_header->indicator & 0x80)
			break;

		/*
		 * Move header pointer up to the next image in the ROM.
		 */
		cur_header += header->size512 * 512;
	}
}

/*
 *	t4_load_boot - download boot flash
 *	@adapter: the adapter
 *	@boot_data: the boot image to write
 *	@boot_addr: offset in flash to write boot_data
 *	@size: image size
 *
 *	Write the supplied boot image to the card's serial flash.
 *	The boot image has the following sections: a 28-byte header and the
 *	boot image.
 */
int t4_load_boot(struct adapter *adap, u8 *boot_data,
		 unsigned int boot_addr, unsigned int size)
{
	pci_exp_rom_header_t *header;
	int pcir_offset ;
	pcir_data_t *pcir_header;
	int ret, addr;
	uint16_t device_id;
	unsigned int i;
	unsigned int boot_sector = (boot_addr * 1024 );
	unsigned int sf_sec_size = adap->params.sf_size / adap->params.sf_nsec;

	/*
	 * Make sure the boot image does not encroach on the firmware region
	 */
	if ((boot_sector + size) >> 16 > FLASH_FW_START_SEC) {
		CH_ERR(adap, "boot image encroaching on firmware region\n");
		return -EFBIG;
	}

	/*
	 * The boot sector is comprised of the Expansion-ROM boot, iSCSI boot,
	 * and Boot configuration data sections. These 3 boot sections span
	 * sectors 0 to 7 in flash and live right before the FW image location.
	 */
	i = DIV_ROUND_UP(size ? size : FLASH_FW_START,
			sf_sec_size);
	ret = t4_flash_erase_sectors(adap, boot_sector >> 16,
				     (boot_sector >> 16) + i - 1);

	/*
	 * If size == 0 then we're simply erasing the FLASH sectors associated
	 * with the on-adapter option ROM file
	 */
	if (ret || (size == 0))
		goto out;

	/* Get boot header */
	header = (pci_exp_rom_header_t *)boot_data;
	pcir_offset = le16_to_cpu(*(u16 *)header->pcir_offset);
	/* PCIR Data Structure */
	pcir_header = (pcir_data_t *) &boot_data[pcir_offset];

	/*
	 * Perform some primitive sanity testing to avoid accidentally
	 * writing garbage over the boot sectors.  We ought to check for
	 * more but it's not worth it for now ...
	 */
	if (size < BOOT_MIN_SIZE || size > BOOT_MAX_SIZE) {
		CH_ERR(adap, "boot image too small/large\n");
		return -EFBIG;
	}

#ifndef CHELSIO_T4_DIAGS
	/*
	 * Check BOOT ROM header signature
	 */
	if (le16_to_cpu(*(u16*)header->signature) != BOOT_SIGNATURE ) {
		CH_ERR(adap, "Boot image missing signature\n");
		return -EINVAL;
	}

	/*
	 * Check PCI header signature
	 */
	if (le32_to_cpu(*(u32*)pcir_header->signature) != PCIR_SIGNATURE) {
		CH_ERR(adap, "PCI header missing signature\n");
		return -EINVAL;
	}

	/*
	 * Check Vendor ID matches Chelsio ID
	 */
	if (le16_to_cpu(*(u16*)pcir_header->vendor_id) != VENDOR_ID) {
		CH_ERR(adap, "Vendor ID missing signature\n");
		return -EINVAL;
	}
#endif

	/*
	 * Retrieve adapter's device ID
	 */
	t4_os_pci_read_cfg2(adap, PCI_DEVICE_ID, &device_id);
	/* Want to deal with PF 0 so I strip off PF 4 indicator */
	device_id = device_id & 0xf0ff;

	/*
	 * Check PCIE Device ID
	 */
	if (le16_to_cpu(*(u16*)pcir_header->device_id) != device_id) {
		/*
		 * Change the device ID in the Boot BIOS image to match
		 * the Device ID of the current adapter.
		 */
		modify_device_id(device_id, boot_data);
	}

	/*
	 * Skip over the first SF_PAGE_SIZE worth of data and write it after
	 * we finish copying the rest of the boot image. This will ensure
	 * that the BIOS boot header will only be written if the boot image
	 * was written in full.
	 */
	addr = boot_sector;
	for (size -= SF_PAGE_SIZE; size; size -= SF_PAGE_SIZE) {
		addr += SF_PAGE_SIZE;
		boot_data += SF_PAGE_SIZE;
		ret = t4_write_flash(adap, addr, SF_PAGE_SIZE, boot_data, 0);
		if (ret)
			goto out;
	}

	ret = t4_write_flash(adap, boot_sector, SF_PAGE_SIZE,
			     (const u8 *)header, 0);

out:
	if (ret)
		CH_ERR(adap, "boot image download failed, error %d\n", ret);
	return ret;
}

/*
 *	t4_flash_bootcfg_addr - return the address of the flash optionrom configuration
 *	@adapter: the adapter
 *
 *	Return the address within the flash where the OptionROM Configuration
 *	is stored, or an error if the device FLASH is too small to contain
 *	a OptionROM Configuration.
 */
static int t4_flash_bootcfg_addr(struct adapter *adapter)
{
	/*
	 * If the device FLASH isn't large enough to hold a Firmware
	 * Configuration File, return an error.
	 */
	if (adapter->params.sf_size < FLASH_BOOTCFG_START + FLASH_BOOTCFG_MAX_SIZE)
		return -ENOSPC;

	return FLASH_BOOTCFG_START;
}

int t4_load_bootcfg(struct adapter *adap,const u8 *cfg_data, unsigned int size)
{
	int ret, i, n, cfg_addr;
	unsigned int addr;
	unsigned int flash_cfg_start_sec;
	unsigned int sf_sec_size = adap->params.sf_size / adap->params.sf_nsec;

	cfg_addr = t4_flash_bootcfg_addr(adap);
	if (cfg_addr < 0)
		return cfg_addr;

	addr = cfg_addr;
	flash_cfg_start_sec = addr / SF_SEC_SIZE;

	if (size > FLASH_BOOTCFG_MAX_SIZE) {
		CH_ERR(adap, "bootcfg file too large, max is %u bytes\n",
			FLASH_BOOTCFG_MAX_SIZE);
		return -EFBIG;
	}

	i = DIV_ROUND_UP(FLASH_BOOTCFG_MAX_SIZE,/* # of sectors spanned */
			 sf_sec_size);
	ret = t4_flash_erase_sectors(adap, flash_cfg_start_sec,
					flash_cfg_start_sec + i - 1);

	/*
	 * If size == 0 then we're simply erasing the FLASH sectors associated
	 * with the on-adapter OptionROM Configuration File.
	 */
	if (ret || size == 0)
		goto out;

	/* this will write to the flash up to SF_PAGE_SIZE at a time */
	for (i = 0; i< size; i+= SF_PAGE_SIZE) {
		if ( (size - i) <  SF_PAGE_SIZE)
			n = size - i;
		else
			n = SF_PAGE_SIZE;
		ret = t4_write_flash(adap, addr, n, cfg_data, 0);
		if (ret)
			goto out;

		addr += SF_PAGE_SIZE;
		cfg_data += SF_PAGE_SIZE;
	}

out:
	if (ret)
		CH_ERR(adap, "boot config data %s failed %d\n",
				(size == 0 ? "clear" : "download"), ret);
	return ret;
}

/**
 *	t4_set_filter_mode - configure the optional components of filter tuples
 *	@adap: the adapter
 *	@mode_map: a bitmap selcting which optional filter components to enable
 *
 *	Sets the filter mode by selecting the optional components to enable
 *	in filter tuples.  Returns 0 on success and a negative error if the
 *	requested mode needs more bits than are available for optional
 *	components.
 */
int t4_set_filter_mode(struct adapter *adap, unsigned int mode_map)
{
	static u8 width[] = { 1, 3, 17, 17, 8, 8, 16, 9, 3, 1 };

	int i, nbits = 0;

	for (i = S_FCOE; i <= S_FRAGMENTATION; i++)
		if (mode_map & (1 << i))
			nbits += width[i];
	if (nbits > FILTER_OPT_LEN)
		return -EINVAL;
	if (t4_use_ldst(adap))
		t4_fw_tp_pio_rw(adap, &mode_map, 1, A_TP_VLAN_PRI_MAP, 0);
	else
		t4_write_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA, &mode_map,
				  1, A_TP_VLAN_PRI_MAP);
	read_filter_mode_and_ingress_config(adap);

	return 0;
}

/**
 *	t4_clr_port_stats - clear port statistics
 *	@adap: the adapter
 *	@idx: the port index
 *
 *	Clear HW statistics for the given port.
 */
void t4_clr_port_stats(struct adapter *adap, int idx)
{
	unsigned int i;
	u32 bgmap = t4_get_mps_bg_map(adap, idx);
	u32 port_base_addr;

	if (is_t4(adap))
		port_base_addr = PORT_BASE(idx);
	else
		port_base_addr = T5_PORT_BASE(idx);

	for (i = A_MPS_PORT_STAT_TX_PORT_BYTES_L;
			i <= A_MPS_PORT_STAT_TX_PORT_PPP7_H; i += 8)
		t4_write_reg(adap, port_base_addr + i, 0);
	for (i = A_MPS_PORT_STAT_RX_PORT_BYTES_L;
			i <= A_MPS_PORT_STAT_RX_PORT_LESS_64B_H; i += 8)
		t4_write_reg(adap, port_base_addr + i, 0);
	for (i = 0; i < 4; i++)
		if (bgmap & (1 << i)) {
			t4_write_reg(adap,
			A_MPS_STAT_RX_BG_0_MAC_DROP_FRAME_L + i * 8, 0);
			t4_write_reg(adap,
			A_MPS_STAT_RX_BG_0_MAC_TRUNC_FRAME_L + i * 8, 0);
		}
}

/**
 *	t4_i2c_rd - read I2C data from adapter
 *	@adap: the adapter
 *	@port: Port number if per-port device; <0 if not
 *	@devid: per-port device ID or absolute device ID
 *	@offset: byte offset into device I2C space
 *	@len: byte length of I2C space data
 *	@buf: buffer in which to return I2C data
 *
 *	Reads the I2C data from the indicated device and location.
 */
int t4_i2c_rd(struct adapter *adap, unsigned int mbox,
	      int port, unsigned int devid,
	      unsigned int offset, unsigned int len,
	      u8 *buf)
{
	u32 ldst_addrspace;
	struct fw_ldst_cmd ldst;
	int ret;

	if (port >= 4 ||
	    devid >= 256 ||
	    offset >= 256 ||
	    len > sizeof ldst.u.i2c.data)
		return -EINVAL;

	memset(&ldst, 0, sizeof ldst);
	ldst_addrspace = V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_I2C);
	ldst.op_to_addrspace =
		cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
			    F_FW_CMD_REQUEST |
			    F_FW_CMD_READ |
			    ldst_addrspace);
	ldst.cycles_to_len16 = cpu_to_be32(FW_LEN16(ldst));
	ldst.u.i2c.pid = (port < 0 ? 0xff : port);
	ldst.u.i2c.did = devid;
	ldst.u.i2c.boffset = offset;
	ldst.u.i2c.blen = len;
	ret = t4_wr_mbox(adap, mbox, &ldst, sizeof ldst, &ldst);
	if (!ret)
		memcpy(buf, ldst.u.i2c.data, len);
	return ret;
}

/**
 *	t4_i2c_wr - write I2C data to adapter
 *	@adap: the adapter
 *	@port: Port number if per-port device; <0 if not
 *	@devid: per-port device ID or absolute device ID
 *	@offset: byte offset into device I2C space
 *	@len: byte length of I2C space data
 *	@buf: buffer containing new I2C data
 *
 *	Write the I2C data to the indicated device and location.
 */
int t4_i2c_wr(struct adapter *adap, unsigned int mbox,
	      int port, unsigned int devid,
	      unsigned int offset, unsigned int len,
	      u8 *buf)
{
	u32 ldst_addrspace;
	struct fw_ldst_cmd ldst;

	if (port >= 4 ||
	    devid >= 256 ||
	    offset >= 256 ||
	    len > sizeof ldst.u.i2c.data)
		return -EINVAL;

	memset(&ldst, 0, sizeof ldst);
	ldst_addrspace = V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_I2C);
	ldst.op_to_addrspace =
		cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
			    F_FW_CMD_REQUEST |
			    F_FW_CMD_WRITE |
			    ldst_addrspace);
	ldst.cycles_to_len16 = cpu_to_be32(FW_LEN16(ldst));
	ldst.u.i2c.pid = (port < 0 ? 0xff : port);
	ldst.u.i2c.did = devid;
	ldst.u.i2c.boffset = offset;
	ldst.u.i2c.blen = len;
	memcpy(ldst.u.i2c.data, buf, len);
	return t4_wr_mbox(adap, mbox, &ldst, sizeof ldst, &ldst);
}

/**
 * 	t4_sge_ctxt_rd - read an SGE context through FW
 * 	@adap: the adapter
 * 	@mbox: mailbox to use for the FW command
 * 	@cid: the context id
 * 	@ctype: the context type
 * 	@data: where to store the context data
 *
 * 	Issues a FW command through the given mailbox to read an SGE context.
 */
int t4_sge_ctxt_rd(struct adapter *adap, unsigned int mbox, unsigned int cid,
		   enum ctxt_type ctype, u32 *data)
{
	int ret;
	struct fw_ldst_cmd c;

	if (ctype == CTXT_EGRESS)
		ret = FW_LDST_ADDRSPC_SGE_EGRC;
	else if (ctype == CTXT_INGRESS)
		ret = FW_LDST_ADDRSPC_SGE_INGC;
	else if (ctype == CTXT_FLM)
		ret = FW_LDST_ADDRSPC_SGE_FLMC;
	else
		ret = FW_LDST_ADDRSPC_SGE_CONMC;

	memset(&c, 0, sizeof(c));
	c.op_to_addrspace = cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
					F_FW_CMD_REQUEST | F_FW_CMD_READ |
					V_FW_LDST_CMD_ADDRSPACE(ret));
	c.cycles_to_len16 = cpu_to_be32(FW_LEN16(c));
	c.u.idctxt.physid = cpu_to_be32(cid);

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret == 0) {
		data[0] = be32_to_cpu(c.u.idctxt.ctxt_data0);
		data[1] = be32_to_cpu(c.u.idctxt.ctxt_data1);
		data[2] = be32_to_cpu(c.u.idctxt.ctxt_data2);
		data[3] = be32_to_cpu(c.u.idctxt.ctxt_data3);
		data[4] = be32_to_cpu(c.u.idctxt.ctxt_data4);
		data[5] = be32_to_cpu(c.u.idctxt.ctxt_data5);
	}
	return ret;
}

/**
 * 	t4_sge_ctxt_rd_bd - read an SGE context bypassing FW
 * 	@adap: the adapter
 * 	@cid: the context id
 * 	@ctype: the context type
 * 	@data: where to store the context data
 *
 * 	Reads an SGE context directly, bypassing FW.  This is only for
 * 	debugging when FW is unavailable.
 */
int t4_sge_ctxt_rd_bd(struct adapter *adap, unsigned int cid, enum ctxt_type ctype,
		      u32 *data)
{
	int i, ret;

	t4_write_reg(adap, A_SGE_CTXT_CMD, V_CTXTQID(cid) | V_CTXTTYPE(ctype));
	ret = t4_wait_op_done(adap, A_SGE_CTXT_CMD, F_BUSY, 0, 3, 1);
	if (!ret)
		for (i = A_SGE_CTXT_DATA0; i <= A_SGE_CTXT_DATA5; i += 4)
			*data++ = t4_read_reg(adap, i);
	return ret;
}

int t4_sched_config(struct adapter *adapter, int type, int minmaxen,
    		    int sleep_ok)
{
	struct fw_sched_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_SCHED_CMD) |
				      F_FW_CMD_REQUEST |
				      F_FW_CMD_WRITE);
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));

	cmd.u.config.sc = FW_SCHED_SC_CONFIG;
	cmd.u.config.type = type;
	cmd.u.config.minmaxen = minmaxen;

	return t4_wr_mbox_meat(adapter,adapter->mbox, &cmd, sizeof(cmd),
			       NULL, sleep_ok);
}

int t4_sched_params(struct adapter *adapter, int type, int level, int mode,
		    int rateunit, int ratemode, int channel, int cl,
		    int minrate, int maxrate, int weight, int pktsize,
		    int sleep_ok)
{
	struct fw_sched_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_SCHED_CMD) |
				      F_FW_CMD_REQUEST |
				      F_FW_CMD_WRITE);
	cmd.retval_len16 = cpu_to_be32(FW_LEN16(cmd));

	cmd.u.params.sc = FW_SCHED_SC_PARAMS;
	cmd.u.params.type = type;
	cmd.u.params.level = level;
	cmd.u.params.mode = mode;
	cmd.u.params.ch = channel;
	cmd.u.params.cl = cl;
	cmd.u.params.unit = rateunit;
	cmd.u.params.rate = ratemode;
	cmd.u.params.min = cpu_to_be32(minrate);
	cmd.u.params.max = cpu_to_be32(maxrate);
	cmd.u.params.weight = cpu_to_be16(weight);
	cmd.u.params.pktsize = cpu_to_be16(pktsize);

	return t4_wr_mbox_meat(adapter,adapter->mbox, &cmd, sizeof(cmd),
			       NULL, sleep_ok);
}

/*
 *	t4_config_watchdog - configure (enable/disable) a watchdog timer
 *	@adapter: the adapter
 * 	@mbox: mailbox to use for the FW command
 * 	@pf: the PF owning the queue
 * 	@vf: the VF owning the queue
 *	@timeout: watchdog timeout in ms
 *	@action: watchdog timer / action
 *
 *	There are separate watchdog timers for each possible watchdog
 *	action.  Configure one of the watchdog timers by setting a non-zero
 *	timeout.  Disable a watchdog timer by using a timeout of zero.
 */
int t4_config_watchdog(struct adapter *adapter, unsigned int mbox,
		       unsigned int pf, unsigned int vf,
		       unsigned int timeout, unsigned int action)
{
	struct fw_watchdog_cmd wdog;
	unsigned int ticks;

	/*
	 * The watchdog command expects a timeout in units of 10ms so we need
	 * to convert it here (via rounding) and force a minimum of one 10ms
	 * "tick" if the timeout is non-zero but the conversion results in 0
	 * ticks.
	 */
	ticks = (timeout + 5)/10;
	if (timeout && !ticks)
		ticks = 1;

	memset(&wdog, 0, sizeof wdog);
	wdog.op_to_vfn = cpu_to_be32(V_FW_CMD_OP(FW_WATCHDOG_CMD) |
				     F_FW_CMD_REQUEST |
				     F_FW_CMD_WRITE |
				     V_FW_PARAMS_CMD_PFN(pf) |
				     V_FW_PARAMS_CMD_VFN(vf));
	wdog.retval_len16 = cpu_to_be32(FW_LEN16(wdog));
	wdog.timeout = cpu_to_be32(ticks);
	wdog.action = cpu_to_be32(action);

	return t4_wr_mbox(adapter, mbox, &wdog, sizeof wdog, NULL);
}

int t4_get_devlog_level(struct adapter *adapter, unsigned int *level)
{
	struct fw_devlog_cmd devlog_cmd;
	int ret;

	memset(&devlog_cmd, 0, sizeof(devlog_cmd));
	devlog_cmd.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_DEVLOG_CMD) |
					     F_FW_CMD_REQUEST | F_FW_CMD_READ);
	devlog_cmd.retval_len16 = cpu_to_be32(FW_LEN16(devlog_cmd));
	ret = t4_wr_mbox(adapter, adapter->mbox, &devlog_cmd,
			 sizeof(devlog_cmd), &devlog_cmd);
	if (ret)
		return ret;

	*level = devlog_cmd.level;
	return 0;
}

int t4_set_devlog_level(struct adapter *adapter, unsigned int level)
{
	struct fw_devlog_cmd devlog_cmd;

	memset(&devlog_cmd, 0, sizeof(devlog_cmd));
	devlog_cmd.op_to_write = cpu_to_be32(V_FW_CMD_OP(FW_DEVLOG_CMD) |
					     F_FW_CMD_REQUEST |
					     F_FW_CMD_WRITE);
	devlog_cmd.level = level;
	devlog_cmd.retval_len16 = cpu_to_be32(FW_LEN16(devlog_cmd));
	return t4_wr_mbox(adapter, adapter->mbox, &devlog_cmd,
			  sizeof(devlog_cmd), &devlog_cmd);
}
