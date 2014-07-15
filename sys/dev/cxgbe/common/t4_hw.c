/*-
 * Copyright (c) 2012 Chelsio Communications, Inc.
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
int t4_wait_op_done_val(struct adapter *adapter, int reg, u32 mask,
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
		      unsigned int data_reg, u32 *vals, unsigned int nregs,
		      unsigned int start_idx)
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
 */
u32 t4_hw_pci_read_cfg4(adapter_t *adap, int reg)
{
	t4_write_reg(adap, A_PCIE_CFG_SPACE_REQ,
		     F_ENABLE | F_LOCALCFG | V_FUNCTION(adap->pf) |
		     V_REGISTER(reg));
	return t4_read_reg(adap, A_PCIE_CFG_SPACE_DATA);
}

/*
 *	t4_report_fw_error - report firmware error
 *	@adap: the adapter
 *
 *	The adapter firmware can indicate error conditions to the host.
 *	This routine prints out the reason for the firmware error (as
 *	reported by the firmware).
 */
static void t4_report_fw_error(struct adapter *adap)
{
	static const char *reason[] = {
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
static void fw_asrt(struct adapter *adap, u32 mbox_addr)
{
	struct fw_debug_cmd asrt;

	get_mbox_rpl(adap, (__be64 *)&asrt, sizeof(asrt) / 8, mbox_addr);
	CH_ALERT(adap, "FW assertion at %.16s:%u, val0 %#x, val1 %#x\n",
		 asrt.u.assert.filename_0_7, ntohl(asrt.u.assert.line),
		 ntohl(asrt.u.assert.x), ntohl(asrt.u.assert.y));
}

#define X_CIM_PF_NOACCESS 0xeeeeeeee
/**
 *	t4_wr_mbox_meat - send a command to FW through the given mailbox
 *	@adap: the adapter
 *	@mbox: index of the mailbox to use
 *	@cmd: the command to write
 *	@size: command length in bytes
 *	@rpl: where to optionally store the reply
 *	@sleep_ok: if true we may sleep while awaiting command completion
 *
 *	Sends the given command to FW through the selected mailbox and waits
 *	for the FW to execute the command.  If @rpl is not %NULL it is used to
 *	store the FW's reply to the command.  The command and its optional
 *	reply are of the same length.  Some FW commands like RESET and
 *	INITIALIZE can take a considerable amount of time to execute.
 *	@sleep_ok determines whether we may sleep while awaiting the response.
 *	If sleeping is allowed we use progressive backoff otherwise we spin.
 *
 *	The return value is 0 on success or a negative errno on failure.  A
 *	failure can happen either because we are not able to execute the
 *	command or FW executes it but signals an error.  In the latter case
 *	the return value is the error code indicated by FW (negated).
 */
int t4_wr_mbox_meat(struct adapter *adap, int mbox, const void *cmd, int size,
		    void *rpl, bool sleep_ok)
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
	int i, ms, delay_idx;
	const __be64 *p = cmd;
	u32 data_reg = PF_REG(mbox, A_CIM_PF_MAILBOX_DATA);
	u32 ctl_reg = PF_REG(mbox, A_CIM_PF_MAILBOX_CTRL);

	if ((size & 15) || size > MBOX_LEN)
		return -EINVAL;

	v = G_MBOWNER(t4_read_reg(adap, ctl_reg));
	for (i = 0; v == X_MBOWNER_NONE && i < 3; i++)
		v = G_MBOWNER(t4_read_reg(adap, ctl_reg));

	if (v != X_MBOWNER_PL)
		return v ? -EBUSY : -ETIMEDOUT;

	for (i = 0; i < size; i += 8, p++)
		t4_write_reg64(adap, data_reg + i, be64_to_cpu(*p));

	t4_write_reg(adap, ctl_reg, F_MBMSGVALID | V_MBOWNER(X_MBOWNER_FW));
	t4_read_reg(adap, ctl_reg);          /* flush write */

	delay_idx = 0;
	ms = delay[0];

	for (i = 0; i < FW_CMD_MAX_TIMEOUT; i += ms) {
		if (sleep_ok) {
			ms = delay[delay_idx];  /* last element may repeat */
			if (delay_idx < ARRAY_SIZE(delay) - 1)
				delay_idx++;
			msleep(ms);
		} else
			mdelay(ms);

		v = t4_read_reg(adap, ctl_reg);
		if (v == X_CIM_PF_NOACCESS)
			continue;
		if (G_MBOWNER(v) == X_MBOWNER_PL) {
			if (!(v & F_MBMSGVALID)) {
				t4_write_reg(adap, ctl_reg,
					     V_MBOWNER(X_MBOWNER_NONE));
				continue;
			}

			res = t4_read_reg64(adap, data_reg);
			if (G_FW_CMD_OP(res >> 32) == FW_DEBUG_CMD) {
				fw_asrt(adap, data_reg);
				res = V_FW_CMD_RETVAL(EIO);
			} else if (rpl)
				get_mbox_rpl(adap, rpl, size / 8, data_reg);
			t4_write_reg(adap, ctl_reg, V_MBOWNER(X_MBOWNER_NONE));
			return -G_FW_CMD_RETVAL((int)res);
		}
	}

	/*
	 * We timed out waiting for a reply to our mailbox command.  Report
	 * the error and also check to see if the firmware reported any
	 * errors ...
	 */
	CH_ERR(adap, "command %#x in mailbox %d timed out\n",
	       *(const u8 *)cmd, mbox);
	if (t4_read_reg(adap, A_PCIE_FW) & F_PCIE_FW_ERR)
		t4_report_fw_error(adap);
	return -ETIMEDOUT;
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
	start = addr & ~(64-1);
	end = (addr + len + 64-1) & ~(64-1);
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
 * Partial EEPROM Vital Product Data structure.  Includes only the ID and
 * VPD-R header.
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
#define EEPROM_MAX_RD_POLL 40
#define EEPROM_MAX_WR_POLL 6
#define EEPROM_STAT_ADDR   0x7bfc
#define VPD_BASE           0x400
#define VPD_BASE_OLD       0
#define VPD_LEN            1024
#define VPD_INFO_FLD_HDR_SIZE	3
#define CHELSIO_VPD_UNIQUE_ID 0x82

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
	u16 val;
	int attempts = EEPROM_MAX_RD_POLL;
	unsigned int base = adapter->params.pci.vpd_cap_addr;

	if (addr >= EEPROMVSIZE || (addr & 3))
		return -EINVAL;

	t4_os_pci_write_cfg2(adapter, base + PCI_VPD_ADDR, (u16)addr);
	do {
		udelay(10);
		t4_os_pci_read_cfg2(adapter, base + PCI_VPD_ADDR, &val);
	} while (!(val & PCI_VPD_ADDR_F) && --attempts);

	if (!(val & PCI_VPD_ADDR_F)) {
		CH_ERR(adapter, "reading EEPROM address 0x%x failed\n", addr);
		return -EIO;
	}
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
	u16 val;
	int attempts = EEPROM_MAX_WR_POLL;
	unsigned int base = adapter->params.pci.vpd_cap_addr;

	if (addr >= EEPROMVSIZE || (addr & 3))
		return -EINVAL;

	t4_os_pci_write_cfg4(adapter, base + PCI_VPD_DATA,
				 cpu_to_le32(data));
	t4_os_pci_write_cfg2(adapter, base + PCI_VPD_ADDR,
				 (u16)addr | PCI_VPD_ADDR_F);
	do {
		msleep(1);
		t4_os_pci_read_cfg2(adapter, base + PCI_VPD_ADDR, &val);
	} while ((val & PCI_VPD_ADDR_F) && --attempts);

	if (val & PCI_VPD_ADDR_F) {
		CH_ERR(adapter, "write to EEPROM address 0x%x failed\n", addr);
		return -EIO;
	}
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
	 const u8 *buf = &v->id_tag;
	 const u8 *vpdr_len = &v->vpdr_tag; 
	 offset = sizeof(struct t4_vpd_hdr);
	 len =  (u16)vpdr_len[1] + ((u16)vpdr_len[2] << 8);
	 
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
 *
 *	Reads card parameters stored in VPD EEPROM.
 */
static int get_vpd_params(struct adapter *adapter, struct vpd_params *p)
{
	int i, ret, addr;
	int ec, sn, pn, na;
	u8 vpd[VPD_LEN], csum;
	const struct t4_vpd_hdr *v;

	/*
	 * Card information normally starts at VPD_BASE but early cards had
	 * it at 0.
	 */
	ret = t4_seeprom_read(adapter, VPD_BASE, (u32 *)(vpd));
	addr = *vpd == CHELSIO_VPD_UNIQUE_ID ? VPD_BASE : VPD_BASE_OLD;

	for (i = 0; i < sizeof(vpd); i += 4) {
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
		CH_ERR(adapter, "corrupted VPD EEPROM, actual csum %u\n", csum);
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
	SF_ATTEMPTS = 10,             /* max retries for SF operations */

	/* flash command opcodes */
	SF_PROG_PAGE    = 2,          /* program page */
	SF_WR_DISABLE   = 4,          /* disable writes */
	SF_RD_STATUS    = 5,          /* read status register */
	SF_WR_ENABLE    = 6,          /* enable writes */
	SF_RD_DATA_FAST = 0xb,        /* read flash */
	SF_RD_ID        = 0x9f,       /* read ID */
	SF_ERASE_SECTOR = 0xd8,       /* erase sector */
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
 *	natural endianess.
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
			*data = htonl(*data);
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
static int t4_write_flash(struct adapter *adapter, unsigned int addr,
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
			val = htonl(val);

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
		CH_ERR(adapter, "failed to correctly write the flash page "
		       "at %#x\n", addr);
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
	return t4_read_flash(adapter,
			     FLASH_FW_START + offsetof(struct fw_hdr, fw_ver), 1,
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
	return t4_read_flash(adapter, FLASH_FW_START + offsetof(struct fw_hdr,
							      tp_microcode_ver),
			     1, vers, 0);
}

/**
 *	t4_check_fw_version - check if the FW is compatible with this driver
 *	@adapter: the adapter
 *
 *	Checks if an adapter's FW is compatible with the driver.  Returns 0
 *	if there's exact match, a negative error if the version could not be
 *	read or there's a major version mismatch, and a positive value if the
 *	expected major version is found but there's a minor version mismatch.
 */
int t4_check_fw_version(struct adapter *adapter)
{
	int ret, major, minor, micro;
	int exp_major, exp_minor, exp_micro;

	ret = t4_get_fw_version(adapter, &adapter->params.fw_vers);
	if (!ret)
		ret = t4_get_tp_version(adapter, &adapter->params.tp_vers);
	if (ret)
		return ret;

	major = G_FW_HDR_FW_VER_MAJOR(adapter->params.fw_vers);
	minor = G_FW_HDR_FW_VER_MINOR(adapter->params.fw_vers);
	micro = G_FW_HDR_FW_VER_MICRO(adapter->params.fw_vers);

	switch (chip_id(adapter)) {
	case CHELSIO_T4:
		exp_major = T4FW_VERSION_MAJOR;
		exp_minor = T4FW_VERSION_MINOR;
		exp_micro = T4FW_VERSION_MICRO;
		break;
	case CHELSIO_T5:
		exp_major = T5FW_VERSION_MAJOR;
		exp_minor = T5FW_VERSION_MINOR;
		exp_micro = T5FW_VERSION_MICRO;
		break;
	default:
		CH_ERR(adapter, "Unsupported chip type, %x\n",
		    chip_id(adapter));
		return -EINVAL;
	}

	if (major != exp_major) {            /* major mismatch - fail */
		CH_ERR(adapter, "card FW has major version %u, driver wants "
		       "%u\n", major, exp_major);
		return -EINVAL;
	}

	if (minor == exp_minor && micro == exp_micro)
		return 0;                                   /* perfect match */

	/* Minor/micro version mismatch.  Report it but often it's OK. */
	return 1;
}

/**
 *	t4_flash_erase_sectors - erase a range of flash sectors
 *	@adapter: the adapter
 *	@start: the first sector to erase
 *	@end: the last sector to erase
 *
 *	Erases the sectors in the given inclusive range.
 */
static int t4_flash_erase_sectors(struct adapter *adapter, int start, int end)
{
	int ret = 0;

	while (start <= end) {
		if ((ret = sf1_write(adapter, 1, 0, 1, SF_WR_ENABLE)) != 0 ||
		    (ret = sf1_write(adapter, 4, 0, 1,
				     SF_ERASE_SECTOR | (start << 8))) != 0 ||
		    (ret = flash_wait_op(adapter, 14, 500)) != 0) {
			CH_ERR(adapter, "erase of flash sector %d failed, "
			       "error %d\n", start, ret);
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
		CH_ERR(adap, "FW image size not multiple of 512 bytes\n");
		return -EINVAL;
	}
	if (ntohs(hdr->len512) * 512 != size) {
		CH_ERR(adap, "FW image size differs from size in FW header\n");
		return -EINVAL;
	}
	if (size > fw_size) {
		CH_ERR(adap, "FW image too large, max is %u bytes\n", fw_size);
		return -EFBIG;
	}
	if ((is_t4(adap) && hdr->chip != FW_HDR_CHIP_T4) ||
	    (is_t5(adap) && hdr->chip != FW_HDR_CHIP_T5)) {
		CH_ERR(adap,
		    "FW image (%d) is not suitable for this adapter (%d)\n",
		    hdr->chip, chip_id(adap));
		return -EINVAL;
	}

	for (csum = 0, i = 0; i < size / sizeof(csum); i++)
		csum += ntohl(p[i]);

	if (csum != 0xffffffff) {
		CH_ERR(adap, "corrupted firmware image, checksum %#x\n",
		       csum);
		return -EINVAL;
	}

	i = DIV_ROUND_UP(size, sf_sec_size);        /* # of sectors spanned */
	ret = t4_flash_erase_sectors(adap, fw_start_sec, fw_start_sec + i - 1);
	if (ret)
		goto out;

	/*
	 * We write the correct version at the end so the driver can see a bad
	 * version if the FW write fails.  Start by writing a copy of the
	 * first page with a bad version.
	 */
	memcpy(first_page, fw_data, SF_PAGE_SIZE);
	((struct fw_hdr *)first_page)->fw_ver = htonl(0xffffffff);
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
		CH_ERR(adap, "firmware download failed, error %d\n", ret);
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
	unsigned int boot_sector = boot_addr * 1024;
	unsigned int sf_sec_size = adap->params.sf_size / adap->params.sf_nsec;

	/*
	 * Make sure the boot image does not encroach on the firmware region
	 */
	if ((boot_sector + size) >> 16 > FLASH_FW_START_SEC) {
		CH_ERR(adap, "boot image encroaching on firmware region\n");
		return -EFBIG;
	}

	/*
	 * Number of sectors spanned
	 */
	i = DIV_ROUND_UP(size ? size : FLASH_BOOTCFG_MAX_SIZE,
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

	/*
	 * Retrieve adapter's device ID
	 */
	t4_os_pci_read_cfg2(adap, PCI_DEVICE_ID, &device_id);
	/* Want to deal with PF 0 so I strip off PF 4 indicator */
	device_id = (device_id & 0xff) | 0x4000;

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

	ret = t4_write_flash(adap, boot_sector, SF_PAGE_SIZE, boot_data, 0);

out:
	if (ret)
		CH_ERR(adap, "boot image download failed, error %d\n", ret);
	return ret;
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
	int cim_num_obq = is_t4(adap) ? CIM_NUM_OBQ : CIM_NUM_OBQ_T5;

	for (i = 0; i < CIM_NUM_IBQ; i++) {
		t4_write_reg(adap, A_CIM_QUEUE_CONFIG_REF, F_IBQSELECT |
			     V_QUENUMSELECT(i));
		v = t4_read_reg(adap, A_CIM_QUEUE_CONFIG_CTRL);
		*base++ = G_CIMQBASE(v) * 256; /* value is in 256-byte units */
		*size++ = G_CIMQSIZE(v) * 256; /* value is in 256-byte units */
		*thres++ = G_QUEFULLTHRSH(v) * 8;   /* 8-byte unit */
	}
	for (i = 0; i < cim_num_obq; i++) {
		t4_write_reg(adap, A_CIM_QUEUE_CONFIG_REF, F_OBQSELECT |
			     V_QUENUMSELECT(i));
		v = t4_read_reg(adap, A_CIM_QUEUE_CONFIG_CTRL);
		*base++ = G_CIMQBASE(v) * 256; /* value is in 256-byte units */
		*size++ = G_CIMQSIZE(v) * 256; /* value is in 256-byte units */
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
	int i, err;
	unsigned int addr;
	const unsigned int nwords = CIM_IBQ_SIZE * 4;

	if (qid > 5 || (n & 3))
		return -EINVAL;

	addr = qid * nwords;
	if (n > nwords)
		n = nwords;

	for (i = 0; i < n; i++, addr++) {
		t4_write_reg(adap, A_CIM_IBQ_DBG_CFG, V_IBQDBGADDR(addr) |
			     F_IBQDBGEN);
		/*
		 * It might take 3-10ms before the IBQ debug read access is
		 * allowed.  Wait for 1 Sec with a delay of 1 usec.
		 */
		err = t4_wait_op_done(adap, A_CIM_IBQ_DBG_CFG, F_IBQDBGBUSY, 0,
				      1000000, 1);
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
	int cim_num_obq = is_t4(adap) ? CIM_NUM_OBQ : CIM_NUM_OBQ_T5;

	if (qid >= cim_num_obq || (n & 3))
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

static int t4_cim_write1(struct adapter *adap, unsigned int addr, unsigned int val)
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

	if (cfg & F_UPDBGLAEN) {                /* LA is running, freeze it */
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
		idx = (idx + 1) & M_UPDBGLARDPTR;
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
	if (cfg & F_DBGLAENABLE)                    /* freeze LA */
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

	if (cfg & F_DBGLAENABLE)                    /* restore running state */
		t4_write_reg(adap, A_TP_DBG_LA_CONFIG,
			     cfg | adap->params.tp.la_mask);
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
 *	t4_link_start - apply link configuration to MAC/PHY
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
int t4_link_start(struct adapter *adap, unsigned int mbox, unsigned int port,
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
	c.op_to_portid = htonl(V_FW_CMD_OP(FW_PORT_CMD) | F_FW_CMD_REQUEST |
			       F_FW_CMD_EXEC | V_FW_PORT_CMD_PORTID(port));
	c.action_to_len16 = htonl(V_FW_PORT_CMD_ACTION(FW_PORT_ACTION_L1_CFG) |
				  FW_LEN16(c));

	if (!(lc->supported & FW_PORT_CAP_ANEG)) {
		c.u.l1cfg.rcap = htonl((lc->supported & ADVERT_MASK) | fc);
		lc->fc = lc->requested_fc & (PAUSE_RX | PAUSE_TX);
	} else if (lc->autoneg == AUTONEG_DISABLE) {
		c.u.l1cfg.rcap = htonl(lc->requested_speed | fc | mdi);
		lc->fc = lc->requested_fc & (PAUSE_RX | PAUSE_TX);
	} else
		c.u.l1cfg.rcap = htonl(lc->advertising | fc | mdi);

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
	c.op_to_portid = htonl(V_FW_CMD_OP(FW_PORT_CMD) | F_FW_CMD_REQUEST |
			       F_FW_CMD_EXEC | V_FW_PORT_CMD_PORTID(port));
	c.action_to_len16 = htonl(V_FW_PORT_CMD_ACTION(FW_PORT_ACTION_L1_CFG) |
				  FW_LEN16(c));
	c.u.l1cfg.rcap = htonl(FW_PORT_CAP_ANEG);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

struct intr_info {
	unsigned int mask;       /* bits to check in interrupt status */
	const char *msg;         /* message to print or NULL */
	short stat_idx;          /* stat counter to increment or -1 */
	unsigned short fatal;    /* whether the condition reported is fatal */
};

/**
 *	t4_handle_intr_status - table driven interrupt handler
 *	@adapter: the adapter that generated the interrupt
 *	@reg: the interrupt status register to process
 *	@acts: table of interrupt actions
 *
 *	A table driven interrupt handler that applies a set of masks to an
 *	interrupt status word and performs the corresponding actions if the
 *	interrupts described by the mask have occured.  The actions include
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
			CH_ALERT(adapter, "%s (0x%x)\n",
				 acts->msg, status & acts->mask);
		} else if (acts->msg)
			CH_WARN_RATELIMIT(adapter, "%s (0x%x)\n",
					  acts->msg, status & acts->mask);
		mask |= acts->mask;
	}
	status &= mask;
	if (status)                           /* clear processed interrupts */
		t4_write_reg(adapter, reg, status);
	return fatal;
}

/*
 * Interrupt handler for the PCIE module.
 */
static void pcie_intr_handler(struct adapter *adapter)
{
	static struct intr_info sysbus_intr_info[] = {
		{ F_RNPP, "RXNP array parity error", -1, 1 },
		{ F_RPCP, "RXPC array parity error", -1, 1 },
		{ F_RCIP, "RXCIF array parity error", -1, 1 },
		{ F_RCCP, "Rx completions control array parity error", -1, 1 },
		{ F_RFTP, "RXFT array parity error", -1, 1 },
		{ 0 }
	};
	static struct intr_info pcie_port_intr_info[] = {
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
	static struct intr_info pcie_intr_info[] = {
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

	static struct intr_info t5_pcie_intr_info[] = {
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

	fat = t4_handle_intr_status(adapter,
				    A_PCIE_CORE_UTL_SYSTEM_BUS_AGENT_STATUS,
				    sysbus_intr_info) +
	      t4_handle_intr_status(adapter,
				    A_PCIE_CORE_UTL_PCI_EXPRESS_PORT_STATUS,
				    pcie_port_intr_info) +
	      t4_handle_intr_status(adapter, A_PCIE_INT_CAUSE,
				    is_t4(adapter) ?
				    pcie_intr_info : t5_pcie_intr_info);
	if (fat)
		t4_fatal_err(adapter);
}

/*
 * TP interrupt handler.
 */
static void tp_intr_handler(struct adapter *adapter)
{
	static struct intr_info tp_intr_info[] = {
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

	static struct intr_info sge_intr_info[] = {
		{ F_ERR_CPL_EXCEED_IQE_SIZE,
		  "SGE received CPL exceeding IQE size", -1, 1 },
		{ F_ERR_INVALID_CIDX_INC,
		  "SGE GTS CIDX increment too large", -1, 0 },
		{ F_ERR_CPL_OPCODE_0, "SGE received 0-length CPL", -1, 0 },
		{ F_ERR_DROPPED_DB, "SGE doorbell dropped", -1, 0 },
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
		{ F_ERR_EGR_CTXT_PRIO,
		  "SGE too many priority egress contexts", -1, 0 },
		{ F_INGRESS_SIZE_ERR, "SGE illegal ingress QID", -1, 0 },
		{ F_EGRESS_SIZE_ERR, "SGE illegal egress QID", -1, 0 },
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
	static struct intr_info cim_intr_info[] = {
		{ F_PREFDROPINT, "CIM control register prefetch drop", -1, 1 },
		{ CIM_OBQ_INTR, "CIM OBQ parity error", -1, 1 },
		{ CIM_IBQ_INTR, "CIM IBQ parity error", -1, 1 },
		{ F_MBUPPARERR, "CIM mailbox uP parity error", -1, 1 },
		{ F_MBHOSTPARERR, "CIM mailbox host parity error", -1, 1 },
		{ F_TIEQINPARERRINT, "CIM TIEQ outgoing parity error", -1, 1 },
		{ F_TIEQOUTPARERRINT, "CIM TIEQ incoming parity error", -1, 1 },
		{ 0 }
	};
	static struct intr_info cim_upintr_info[] = {
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
	static struct intr_info ulprx_intr_info[] = {
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
	static struct intr_info ulptx_intr_info[] = {
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
	static struct intr_info pmtx_intr_info[] = {
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
	static struct intr_info pmrx_intr_info[] = {
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
	static struct intr_info cplsw_intr_info[] = {
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
	static struct intr_info le_intr_info[] = {
		{ F_LIPMISS, "LE LIP miss", -1, 0 },
		{ F_LIP0, "LE 0 LIP error", -1, 0 },
		{ F_PARITYERR, "LE parity error", -1, 1 },
		{ F_UNKNOWNCMD, "LE unknown command", -1, 1 },
		{ F_REQQPARERR, "LE request queue parity error", -1, 1 },
		{ 0 }
	};

	if (t4_handle_intr_status(adap, A_LE_DB_INT_CAUSE, le_intr_info))
		t4_fatal_err(adap);
}

/*
 * MPS interrupt handler.
 */
static void mps_intr_handler(struct adapter *adapter)
{
	static struct intr_info mps_rx_intr_info[] = {
		{ 0xffffff, "MPS Rx parity error", -1, 1 },
		{ 0 }
	};
	static struct intr_info mps_tx_intr_info[] = {
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
	static struct intr_info mps_trc_intr_info[] = {
		{ V_FILTMEM(M_FILTMEM), "MPS TRC filter parity error", -1, 1 },
		{ V_PKTFIFO(M_PKTFIFO), "MPS TRC packet FIFO parity error", -1,
		  1 },
		{ F_MISCPERR, "MPS TRC misc parity error", -1, 1 },
		{ 0 }
	};
	static struct intr_info mps_stat_sram_intr_info[] = {
		{ 0x1fffff, "MPS statistics SRAM parity error", -1, 1 },
		{ 0 }
	};
	static struct intr_info mps_stat_tx_intr_info[] = {
		{ 0xfffff, "MPS statistics Tx FIFO parity error", -1, 1 },
		{ 0 }
	};
	static struct intr_info mps_stat_rx_intr_info[] = {
		{ 0xffffff, "MPS statistics Rx FIFO parity error", -1, 1 },
		{ 0 }
	};
	static struct intr_info mps_cls_intr_info[] = {
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
	t4_read_reg(adapter, A_MPS_INT_CAUSE);                    /* flush */
	if (fat)
		t4_fatal_err(adapter);
}

#define MEM_INT_MASK (F_PERR_INT_CAUSE | F_ECC_CE_INT_CAUSE | F_ECC_UE_INT_CAUSE)

/*
 * EDC/MC interrupt handler.
 */
static void mem_intr_handler(struct adapter *adapter, int idx)
{
	static const char name[3][5] = { "EDC0", "EDC1", "MC" };

	unsigned int addr, cnt_addr, v;

	if (idx <= MEM_EDC1) {
		addr = EDC_REG(A_EDC_INT_CAUSE, idx);
		cnt_addr = EDC_REG(A_EDC_ECC_STATUS, idx);
	} else {
		if (is_t4(adapter)) {
			addr = A_MC_INT_CAUSE;
			cnt_addr = A_MC_ECC_STATUS;
		} else {
			addr = A_MC_P_INT_CAUSE;
			cnt_addr = A_MC_P_ECC_STATUS;
		}
	}

	v = t4_read_reg(adapter, addr) & MEM_INT_MASK;
	if (v & F_PERR_INT_CAUSE)
		CH_ALERT(adapter, "%s FIFO parity error\n", name[idx]);
	if (v & F_ECC_CE_INT_CAUSE) {
		u32 cnt = G_ECC_CECNT(t4_read_reg(adapter, cnt_addr));

		t4_write_reg(adapter, cnt_addr, V_ECC_CECNT(M_ECC_CECNT));
		CH_WARN_RATELIMIT(adapter,
				  "%u %s correctable ECC data error%s\n",
				  cnt, name[idx], cnt > 1 ? "s" : "");
	}
	if (v & F_ECC_UE_INT_CAUSE)
		CH_ALERT(adapter, "%s uncorrectable ECC data error\n",
			 name[idx]);

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

	if (status & F_MEM_PERR_INT_CAUSE)
		CH_ALERT(adapter, "MA parity error, parity status %#x\n",
			 t4_read_reg(adapter, A_MA_PARITY_ERROR_STATUS));
	if (status & F_MEM_WRAP_INT_CAUSE) {
		v = t4_read_reg(adapter, A_MA_INT_WRAP_STATUS);
		CH_ALERT(adapter, "MA address wrap-around error by client %u to"
			 " address %#x\n", G_MEM_WRAP_CLIENT_NUM(v),
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
	static struct intr_info smb_intr_info[] = {
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
	static struct intr_info ncsi_intr_info[] = {
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
		CH_ALERT(adap, "XGMAC %d Tx FIFO parity error\n", port);
	if (v & F_RXFIFO_PRTY_ERR)
		CH_ALERT(adap, "XGMAC %d Rx FIFO parity error\n", port);
	t4_write_reg(adap, int_cause_reg, v);
	t4_fatal_err(adap);
}

/*
 * PL interrupt handler.
 */
static void pl_intr_handler(struct adapter *adap)
{
	static struct intr_info pl_intr_info[] = {
		{ F_FATALPERR, "Fatal parity error", -1, 1 },
		{ F_PERRVFID, "PL VFID_MAP parity error", -1, 1 },
		{ 0 }
	};

	static struct intr_info t5_pl_intr_info[] = {
		{ F_PL_BUSPERR, "PL bus parity error", -1, 1 },
		{ F_FATALPERR, "Fatal parity error", -1, 1 },
		{ 0 }
	};

	if (t4_handle_intr_status(adap, A_PL_PL_INT_CAUSE,
	    is_t4(adap) ?  pl_intr_info : t5_pl_intr_info))
		t4_fatal_err(adap);
}

#define PF_INTR_MASK (F_PFSW | F_PFCIM)
#define GLBL_INTR_MASK (F_CIM | F_MPS | F_PL | F_PCIE | F_MC | F_EDC0 | \
		F_EDC1 | F_LE | F_TP | F_MA | F_PM_TX | F_PM_RX | F_ULP_RX | \
		F_CPL_SWITCH | F_SGE | F_ULP_TX)

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
	if (cause & F_XGMAC0)
		xgmac_intr_handler(adapter, 0);
	if (cause & F_XGMAC1)
		xgmac_intr_handler(adapter, 1);
	if (cause & F_XGMAC_KR0)
		xgmac_intr_handler(adapter, 2);
	if (cause & F_XGMAC_KR1)
		xgmac_intr_handler(adapter, 3);
	if (cause & F_PCIE)
		pcie_intr_handler(adapter);
	if (cause & F_MC)
		mem_intr_handler(adapter, MEM_MC);
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
	(void) t4_read_reg(adapter, A_PL_INT_CAUSE); /* flush */
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
	u32 pf = G_SOURCEPF(t4_read_reg(adapter, A_PL_WHOAMI));

	t4_write_reg(adapter, A_SGE_INT_ENABLE3, F_ERR_CPL_EXCEED_IQE_SIZE |
		     F_ERR_INVALID_CIDX_INC | F_ERR_CPL_OPCODE_0 |
		     F_ERR_DROPPED_DB | F_ERR_DATA_CPL_ON_HIGH_QID1 |
		     F_ERR_DATA_CPL_ON_HIGH_QID0 | F_ERR_BAD_DB_PIDX3 |
		     F_ERR_BAD_DB_PIDX2 | F_ERR_BAD_DB_PIDX1 |
		     F_ERR_BAD_DB_PIDX0 | F_ERR_ING_CTXT_PRIO |
		     F_ERR_EGR_CTXT_PRIO | F_INGRESS_SIZE_ERR |
		     F_EGRESS_SIZE_ERR);
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
	u32 pf = G_SOURCEPF(t4_read_reg(adapter, A_PL_WHOAMI));

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
		A_PCIE_CORE_UTL_SYSTEM_BUS_AGENT_STATUS,
		A_PCIE_CORE_UTL_PCI_EXPRESS_PORT_STATUS,
		A_PCIE_NONFAT_ERR, A_PCIE_INT_CAUSE,
		A_MA_INT_WRAP_STATUS, A_MA_PARITY_ERROR_STATUS, A_MA_INT_CAUSE,
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
	cmd.op_to_viid = htonl(V_FW_CMD_OP(FW_RSS_IND_TBL_CMD) |
			       F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
			       V_FW_RSS_IND_TBL_CMD_VIID(viid));
	cmd.retval_len16 = htonl(FW_LEN16(cmd));


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
		cmd.niqid = htons(nq);
		cmd.startidx = htons(start);

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
	c.op_to_write = htonl(V_FW_CMD_OP(FW_RSS_GLB_CONFIG_CMD) |
			      F_FW_CMD_REQUEST | F_FW_CMD_WRITE);
	c.retval_len16 = htonl(FW_LEN16(c));
	if (mode == FW_RSS_GLB_CONFIG_CMD_MODE_MANUAL) {
		c.u.manual.mode_pkd = htonl(V_FW_RSS_GLB_CONFIG_CMD_MODE(mode));
	} else if (mode == FW_RSS_GLB_CONFIG_CMD_MODE_BASICVIRTUAL) {
		c.u.basicvirtual.mode_pkd =
			htonl(V_FW_RSS_GLB_CONFIG_CMD_MODE(mode));
		c.u.basicvirtual.synmapen_to_hashtoeplitz = htonl(flags);
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
	c.op_to_viid = htonl(V_FW_CMD_OP(FW_RSS_VI_CONFIG_CMD) |
			     F_FW_CMD_REQUEST | F_FW_CMD_WRITE |
			     V_FW_RSS_VI_CONFIG_CMD_VIID(viid));
	c.retval_len16 = htonl(FW_LEN16(c));
	c.u.basicvirtual.defaultq_to_udpen = htonl(flags |
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
 *	t4_read_rss_key - read the global RSS key
 *	@adap: the adapter
 *	@key: 10-entry array holding the 320-bit RSS key
 *
 *	Reads the global 320-bit RSS key.
 */
void t4_read_rss_key(struct adapter *adap, u32 *key)
{
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
void t4_write_rss_key(struct adapter *adap, const u32 *key, int idx)
{
	t4_write_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA, key, 10,
			  A_TP_RSS_SECRET_KEY0);
	if (idx >= 0 && idx < 16)
		t4_write_reg(adap, A_TP_RSS_CONFIG_VRT,
			     V_KEYWRADDR(idx) | F_KEYWREN);
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
void t4_read_rss_pf_config(struct adapter *adapter, unsigned int index, u32 *valp)
{
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
void t4_write_rss_pf_config(struct adapter *adapter, unsigned int index, u32 val)
{
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
	u32 vrt;

	/*
	 * Request that the index'th VF Table values be read into VFL/VFH.
	 */
	vrt = t4_read_reg(adapter, A_TP_RSS_CONFIG_VRT);
	vrt &= ~(F_VFRDRG | V_VFWRADDR(M_VFWRADDR) | F_VFWREN | F_KEYWREN);
	vrt |= V_VFWRADDR(index) | F_VFRDEN;
	t4_write_reg(adapter, A_TP_RSS_CONFIG_VRT, vrt);

	/*
	 * Grab the VFL/VFH values ...
	 */
	t4_read_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
			 vfl, 1, A_TP_RSS_VFL_CONFIG);
	t4_read_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
			 vfh, 1, A_TP_RSS_VFH_CONFIG);
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
	u32 vrt;

	/*
	 * Load up VFL/VFH with the values to be written ...
	 */
	t4_write_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
			  &vfl, 1, A_TP_RSS_VFL_CONFIG);
	t4_write_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
			  &vfh, 1, A_TP_RSS_VFH_CONFIG);

	/*
	 * Write the VFL/VFH into the VF Table at index'th location.
	 */
	vrt = t4_read_reg(adapter, A_TP_RSS_CONFIG_VRT);
	vrt &= ~(F_VFRDRG | F_VFRDEN | V_VFWRADDR(M_VFWRADDR) | F_KEYWREN);
	vrt |= V_VFWRADDR(index) | F_VFWREN;
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
	t4_write_indirect(adapter, A_TP_PIO_ADDR, A_TP_PIO_DATA,
			  &pfmask, 1, A_TP_RSS_PF_MSK);
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
	t4_write_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA, &mode_map, 1,
			  A_TP_VLAN_PRI_MAP);
	return 0;
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
		v4->tcpOutRsts = STAT(OUT_RST);
		v4->tcpInSegs  = STAT64(IN_SEG);
		v4->tcpOutSegs = STAT64(OUT_SEG);
		v4->tcpRetransSegs = STAT64(RXT_SEG);
	}
	if (v6) {
		t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, val,
				 ARRAY_SIZE(val), A_TP_MIB_TCP_V6OUT_RST);
		v6->tcpOutRsts = STAT(OUT_RST);
		v6->tcpInSegs  = STAT64(IN_SEG);
		v6->tcpOutSegs = STAT64(OUT_SEG);
		v6->tcpRetransSegs = STAT64(RXT_SEG);
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
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, st->macInErrs,
			 12, A_TP_MIB_MAC_IN_ERR_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, st->tnlCongDrops,
			 8, A_TP_MIB_TNL_CNG_DROP_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, st->tnlTxDrops,
			 4, A_TP_MIB_TNL_DROP_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, st->ofldVlanDrops,
			 4, A_TP_MIB_OFD_VLN_DROP_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, st->tcp6InErrs,
			 4, A_TP_MIB_TCP_V6IN_ERR_0);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, &st->ofldNoNeigh,
			 2, A_TP_MIB_OFD_ARP_DROP);
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
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, st->proxy,
			 4, A_TP_MIB_TNL_LPBK_0);
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
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, st->req,
			 8, A_TP_MIB_CPL_IN_REQ_0);
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
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, &st->rqe_dfr_mod,
			 2, A_TP_MIB_RQE_DFR_MOD);
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

	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, &st->framesDDP,
			 1, A_TP_MIB_FCOE_DDP_0 + idx);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, &st->framesDrop,
			 1, A_TP_MIB_FCOE_DROP_0 + idx);
	t4_read_indirect(adap, A_TP_MIB_INDEX, A_TP_MIB_DATA, val,
			 2, A_TP_MIB_FCOE_BYTE_0_HI + 2 * idx);
	st->octetsDDP = ((u64)val[0] << 32) | val[1];
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
static void __devinit init_cong_ctrl(unsigned short *a, unsigned short *b)
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
			*kbps = 0;        /* scheduler disabled */
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
	nic_rate[2] = chan_rate(adap, G_TNLRATE2(v));
	nic_rate[3] = chan_rate(adap, G_TNLRATE3(v));

	v = t4_read_reg(adap, A_TP_TX_ORATE);
	ofld_rate[0] = chan_rate(adap, G_OFDRATE0(v));
	ofld_rate[1] = chan_rate(adap, G_OFDRATE1(v));
	ofld_rate[2] = chan_rate(adap, G_OFDRATE2(v));
	ofld_rate[3] = chan_rate(adap, G_OFDRATE3(v));
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

	for (i = 0; i < PM_NSTATS; i++) {
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

	for (i = 0; i < PM_NSTATS; i++) {
		t4_write_reg(adap, A_PM_RX_STAT_CONFIG, i + 1);
		cnt[i] = t4_read_reg(adap, A_PM_RX_STAT_COUNT);
		if (is_t4(adap))
			cycles[i] = t4_read_reg64(adap, A_PM_RX_STAT_LSB);
		else {
			t4_read_indirect(adap, A_PM_RX_DBG_CTRL,
					 A_PM_RX_DBG_DATA, data, 2,
					 A_PM_RX_DBG_STAT_MSB);
			cycles[i] = (((u64)data[0] << 32) | data[1]);
		}
	}
}

/**
 *	get_mps_bg_map - return the buffer groups associated with a port
 *	@adap: the adapter
 *	@idx: the port index
 *
 *	Returns a bitmap indicating which MPS buffer groups are associated
 *	with the given port.  Bit i is set if buffer group i is used by the
 *	port.
 */
static unsigned int get_mps_bg_map(struct adapter *adap, int idx)
{
	u32 n = G_NUMPORTS(t4_read_reg(adap, A_MPS_CMN_CTL));

	if (n == 0)
		return idx == 0 ? 0xf : 0;
	if (n == 1)
		return idx < 2 ? (3 << (2 * idx)) : 0;
	return 1 << idx;
}

/**
 *      t4_get_port_stats_offset - collect port stats relative to a previous
 *                                 snapshot
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
	u32 bgmap = get_mps_bg_map(adap, idx);

#define GET_STAT(name) \
	t4_read_reg64(adap, \
	(is_t4(adap) ? PORT_REG(idx, A_MPS_PORT_STAT_##name##_L) : \
	T5_PORT_REG(idx, A_MPS_PORT_STAT_##name##_L)))
#define GET_STAT_COM(name) t4_read_reg64(adap, A_MPS_STAT_##name##_L)

	p->tx_pause            = GET_STAT(TX_PORT_PAUSE);
	p->tx_octets           = GET_STAT(TX_PORT_BYTES);
	p->tx_frames           = GET_STAT(TX_PORT_FRAMES);
	p->tx_bcast_frames     = GET_STAT(TX_PORT_BCAST);
	p->tx_mcast_frames     = GET_STAT(TX_PORT_MCAST);
	p->tx_ucast_frames     = GET_STAT(TX_PORT_UCAST);
	p->tx_error_frames     = GET_STAT(TX_PORT_ERROR);
	p->tx_frames_64        = GET_STAT(TX_PORT_64B);
	p->tx_frames_65_127    = GET_STAT(TX_PORT_65B_127B);
	p->tx_frames_128_255   = GET_STAT(TX_PORT_128B_255B);
	p->tx_frames_256_511   = GET_STAT(TX_PORT_256B_511B);
	p->tx_frames_512_1023  = GET_STAT(TX_PORT_512B_1023B);
	p->tx_frames_1024_1518 = GET_STAT(TX_PORT_1024B_1518B);
	p->tx_frames_1519_max  = GET_STAT(TX_PORT_1519B_MAX);
	p->tx_drop             = GET_STAT(TX_PORT_DROP);
	p->tx_ppp0             = GET_STAT(TX_PORT_PPP0);
	p->tx_ppp1             = GET_STAT(TX_PORT_PPP1);
	p->tx_ppp2             = GET_STAT(TX_PORT_PPP2);
	p->tx_ppp3             = GET_STAT(TX_PORT_PPP3);
	p->tx_ppp4             = GET_STAT(TX_PORT_PPP4);
	p->tx_ppp5             = GET_STAT(TX_PORT_PPP5);
	p->tx_ppp6             = GET_STAT(TX_PORT_PPP6);
	p->tx_ppp7             = GET_STAT(TX_PORT_PPP7);

	p->rx_pause            = GET_STAT(RX_PORT_PAUSE);
	p->rx_octets           = GET_STAT(RX_PORT_BYTES);
	p->rx_frames           = GET_STAT(RX_PORT_FRAMES);
	p->rx_bcast_frames     = GET_STAT(RX_PORT_BCAST);
	p->rx_mcast_frames     = GET_STAT(RX_PORT_MCAST);
	p->rx_ucast_frames     = GET_STAT(RX_PORT_UCAST);
	p->rx_too_long         = GET_STAT(RX_PORT_MTU_ERROR);
	p->rx_jabber           = GET_STAT(RX_PORT_MTU_CRC_ERROR);
	p->rx_fcs_err          = GET_STAT(RX_PORT_CRC_ERROR);
	p->rx_len_err          = GET_STAT(RX_PORT_LEN_ERROR);
	p->rx_symbol_err       = GET_STAT(RX_PORT_SYM_ERROR);
	p->rx_runt             = GET_STAT(RX_PORT_LESS_64B);
	p->rx_frames_64        = GET_STAT(RX_PORT_64B);
	p->rx_frames_65_127    = GET_STAT(RX_PORT_65B_127B);
	p->rx_frames_128_255   = GET_STAT(RX_PORT_128B_255B);
	p->rx_frames_256_511   = GET_STAT(RX_PORT_256B_511B);
	p->rx_frames_512_1023  = GET_STAT(RX_PORT_512B_1023B);
	p->rx_frames_1024_1518 = GET_STAT(RX_PORT_1024B_1518B);
	p->rx_frames_1519_max  = GET_STAT(RX_PORT_1519B_MAX);
	p->rx_ppp0             = GET_STAT(RX_PORT_PPP0);
	p->rx_ppp1             = GET_STAT(RX_PORT_PPP1);
	p->rx_ppp2             = GET_STAT(RX_PORT_PPP2);
	p->rx_ppp3             = GET_STAT(RX_PORT_PPP3);
	p->rx_ppp4             = GET_STAT(RX_PORT_PPP4);
	p->rx_ppp5             = GET_STAT(RX_PORT_PPP5);
	p->rx_ppp6             = GET_STAT(RX_PORT_PPP6);
	p->rx_ppp7             = GET_STAT(RX_PORT_PPP7);

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
 *	t4_clr_port_stats - clear port statistics
 *	@adap: the adapter
 *	@idx: the port index
 *
 *	Clear HW statistics for the given port.
 */
void t4_clr_port_stats(struct adapter *adap, int idx)
{
	unsigned int i;
	u32 bgmap = get_mps_bg_map(adap, idx);
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
 *	t4_get_lb_stats - collect loopback port statistics
 *	@adap: the adapter
 *	@idx: the loopback port index
 *	@p: the stats structure to fill
 *
 *	Return HW statistics for the given loopback port.
 */
void t4_get_lb_stats(struct adapter *adap, int idx, struct lb_port_stats *p)
{
	u32 bgmap = get_mps_bg_map(adap, idx);

#define GET_STAT(name) \
	t4_read_reg64(adap, \
	(is_t4(adap) ? \
	PORT_REG(idx, A_MPS_PORT_STAT_LB_PORT_##name##_L) : \
	T5_PORT_REG(idx, A_MPS_PORT_STAT_LB_PORT_##name##_L)))
#define GET_STAT_COM(name) t4_read_reg64(adap, A_MPS_STAT_##name##_L)

	p->octets           = GET_STAT(BYTES);
	p->frames           = GET_STAT(FRAMES);
	p->bcast_frames     = GET_STAT(BCAST);
	p->mcast_frames     = GET_STAT(MCAST);
	p->ucast_frames     = GET_STAT(UCAST);
	p->error_frames     = GET_STAT(ERROR);

	p->frames_64        = GET_STAT(64B);
	p->frames_65_127    = GET_STAT(65B_127B);
	p->frames_128_255   = GET_STAT(128B_255B);
	p->frames_256_511   = GET_STAT(256B_511B);
	p->frames_512_1023  = GET_STAT(512B_1023B);
	p->frames_1024_1518 = GET_STAT(1024B_1518B);
	p->frames_1519_max  = GET_STAT(1519B_MAX);
	p->drop             = GET_STAT(DROP_FRAMES);

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

/**
 *	t4_mk_filtdelwr - create a delete filter WR
 *	@ftid: the filter ID
 *	@wr: the filter work request to populate
 *	@qid: ingress queue to receive the delete notification
 *
 *	Creates a filter work request to delete the supplied filter.  If @qid is
 *	negative the delete notification is suppressed.
 */
void t4_mk_filtdelwr(unsigned int ftid, struct fw_filter_wr *wr, int qid)
{
	memset(wr, 0, sizeof(*wr));
	wr->op_pkd = htonl(V_FW_WR_OP(FW_FILTER_WR));
	wr->len16_pkd = htonl(V_FW_WR_LEN16(sizeof(*wr) / 16));
	wr->tid_to_iq = htonl(V_FW_FILTER_WR_TID(ftid) |
			      V_FW_FILTER_WR_NOREPLY(qid < 0));
	wr->del_filter_to_l2tix = htonl(F_FW_FILTER_WR_DEL_FILTER);
	if (qid >= 0)
		wr->rx_chan_rx_rpl_iq = htons(V_FW_FILTER_WR_RX_RPL_IQ(qid));
}

#define INIT_CMD(var, cmd, rd_wr) do { \
	(var).op_to_write = htonl(V_FW_CMD_OP(FW_##cmd##_CMD) | \
				  F_FW_CMD_REQUEST | F_FW_CMD_##rd_wr); \
	(var).retval_len16 = htonl(FW_LEN16(var)); \
} while (0)

int t4_fwaddrspace_write(struct adapter *adap, unsigned int mbox, u32 addr, u32 val)
{
	struct fw_ldst_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_addrspace = htonl(V_FW_CMD_OP(FW_LDST_CMD) | F_FW_CMD_REQUEST |
		F_FW_CMD_WRITE | V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_FIRMWARE));
	c.cycles_to_len16 = htonl(FW_LEN16(c));
	c.u.addrval.addr = htonl(addr);
	c.u.addrval.val = htonl(val);

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
	struct fw_ldst_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_addrspace = htonl(V_FW_CMD_OP(FW_LDST_CMD) | F_FW_CMD_REQUEST |
		F_FW_CMD_READ | V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_MDIO));
	c.cycles_to_len16 = htonl(FW_LEN16(c));
	c.u.mdio.paddr_mmd = htons(V_FW_LDST_CMD_PADDR(phy_addr) |
				   V_FW_LDST_CMD_MMD(mmd));
	c.u.mdio.raddr = htons(reg);

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret == 0)
		*valp = ntohs(c.u.mdio.rval);
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
	struct fw_ldst_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_addrspace = htonl(V_FW_CMD_OP(FW_LDST_CMD) | F_FW_CMD_REQUEST |
		F_FW_CMD_WRITE | V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_MDIO));
	c.cycles_to_len16 = htonl(FW_LEN16(c));
	c.u.mdio.paddr_mmd = htons(V_FW_LDST_CMD_PADDR(phy_addr) |
				   V_FW_LDST_CMD_MMD(mmd));
	c.u.mdio.raddr = htons(reg);
	c.u.mdio.rval = htons(val);

	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
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
	struct fw_ldst_cmd ldst;
	int ret;

	if (port >= 4 ||
	    devid >= 256 ||
	    offset >= 256 ||
	    len > sizeof ldst.u.i2c.data)
		return -EINVAL;

	memset(&ldst, 0, sizeof ldst);
	ldst.op_to_addrspace =
		cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
			    F_FW_CMD_REQUEST |
			    F_FW_CMD_READ |
			    V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_I2C));
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
	struct fw_ldst_cmd ldst;

	if (port >= 4 ||
	    devid >= 256 ||
	    offset >= 256 ||
	    len > sizeof ldst.u.i2c.data)
		return -EINVAL;

	memset(&ldst, 0, sizeof ldst);
	ldst.op_to_addrspace =
		cpu_to_be32(V_FW_CMD_OP(FW_LDST_CMD) |
			    F_FW_CMD_REQUEST |
			    F_FW_CMD_WRITE |
			    V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_I2C));
	ldst.cycles_to_len16 = cpu_to_be32(FW_LEN16(ldst));
	ldst.u.i2c.pid = (port < 0 ? 0xff : port);
	ldst.u.i2c.did = devid;
	ldst.u.i2c.boffset = offset;
	ldst.u.i2c.blen = len;
	memcpy(ldst.u.i2c.data, buf, len);
	return t4_wr_mbox(adap, mbox, &ldst, sizeof ldst, &ldst);
}

/**
 *	t4_sge_ctxt_flush - flush the SGE context cache
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *
 *	Issues a FW command through the given mailbox to flush the
 *	SGE context cache.
 */
int t4_sge_ctxt_flush(struct adapter *adap, unsigned int mbox)
{
	int ret;
	struct fw_ldst_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_addrspace = htonl(V_FW_CMD_OP(FW_LDST_CMD) | F_FW_CMD_REQUEST |
			F_FW_CMD_READ |
			V_FW_LDST_CMD_ADDRSPACE(FW_LDST_ADDRSPC_SGE_EGRC));
	c.cycles_to_len16 = htonl(FW_LEN16(c));
	c.u.idctxt.msg_ctxtflush = htonl(F_FW_LDST_CMD_CTXTFLUSH);

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	return ret;
}

/**
 *	t4_sge_ctxt_rd - read an SGE context through FW
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@cid: the context id
 *	@ctype: the context type
 *	@data: where to store the context data
 *
 *	Issues a FW command through the given mailbox to read an SGE context.
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
	c.op_to_addrspace = htonl(V_FW_CMD_OP(FW_LDST_CMD) | F_FW_CMD_REQUEST |
				  F_FW_CMD_READ | V_FW_LDST_CMD_ADDRSPACE(ret));
	c.cycles_to_len16 = htonl(FW_LEN16(c));
	c.u.idctxt.physid = htonl(cid);

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret == 0) {
		data[0] = ntohl(c.u.idctxt.ctxt_data0);
		data[1] = ntohl(c.u.idctxt.ctxt_data1);
		data[2] = ntohl(c.u.idctxt.ctxt_data2);
		data[3] = ntohl(c.u.idctxt.ctxt_data3);
		data[4] = ntohl(c.u.idctxt.ctxt_data4);
		data[5] = ntohl(c.u.idctxt.ctxt_data5);
	}
	return ret;
}

/**
 *	t4_sge_ctxt_rd_bd - read an SGE context bypassing FW
 *	@adap: the adapter
 *	@cid: the context id
 *	@ctype: the context type
 *	@data: where to store the context data
 *
 *	Reads an SGE context directly, bypassing FW.  This is only for
 *	debugging when FW is unavailable.
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

/**
 *	t4_fw_hello - establish communication with FW
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@evt_mbox: mailbox to receive async FW events
 *	@master: specifies the caller's willingness to be the device master
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
	c.err_to_clearinit = htonl(
		V_FW_HELLO_CMD_MASTERDIS(master == MASTER_CANT) |
		V_FW_HELLO_CMD_MASTERFORCE(master == MASTER_MUST) |
		V_FW_HELLO_CMD_MBMASTER(master == MASTER_MUST ? mbox :
			M_FW_HELLO_CMD_MBMASTER) |
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

	v = ntohl(c.err_to_clearinit);
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
	c.val = htonl(reset);
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
		c.val = htonl(F_PIORST | F_PIORSTMODE);
		c.halt_pkd = htonl(F_FW_RESET_CMD_HALT);
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
		t4_set_reg_field(adap, A_PCIE_FW, F_PCIE_FW_HALT, F_PCIE_FW_HALT);
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
	unsigned int bootstrap = ntohl(fw_hdr->magic) == FW_HDR_MAGIC_BOOTSTRAP;
	int reset, ret;

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
	reset = ((ntohl(fw_hdr->flags) & FW_HDR_FLAGS_RESET_HALT) == 0);
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
 *	t4_query_params - query FW or device parameters
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@pf: the PF
 *	@vf: the VF
 *	@nparams: the number of parameters
 *	@params: the parameter names
 *	@val: the parameter values
 *
 *	Reads the value of FW or device parameters.  Up to 7 parameters can be
 *	queried at once.
 */
int t4_query_params(struct adapter *adap, unsigned int mbox, unsigned int pf,
		    unsigned int vf, unsigned int nparams, const u32 *params,
		    u32 *val)
{
	int i, ret;
	struct fw_params_cmd c;
	__be32 *p = &c.param[0].mnem;

	if (nparams > 7)
		return -EINVAL;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_PARAMS_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_READ | V_FW_PARAMS_CMD_PFN(pf) |
			    V_FW_PARAMS_CMD_VFN(vf));
	c.retval_len16 = htonl(FW_LEN16(c));

	for (i = 0; i < nparams; i++, p += 2, params++)
		*p = htonl(*params);

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret == 0)
		for (i = 0, p = &c.param[0].val; i < nparams; i++, p += 2)
			*val++ = ntohl(*p);
	return ret;
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
	struct fw_params_cmd c;
	__be32 *p = &c.param[0].mnem;

	if (nparams > 7)
		return -EINVAL;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_PARAMS_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_WRITE | V_FW_PARAMS_CMD_PFN(pf) |
			    V_FW_PARAMS_CMD_VFN(vf));
	c.retval_len16 = htonl(FW_LEN16(c));

	while (nparams--) {
		*p++ = htonl(*params);
		params++;
		*p++ = htonl(*val);
		val++;
	}

	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
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
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_PFVF_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_WRITE | V_FW_PFVF_CMD_PFN(pf) |
			    V_FW_PFVF_CMD_VFN(vf));
	c.retval_len16 = htonl(FW_LEN16(c));
	c.niqflint_niq = htonl(V_FW_PFVF_CMD_NIQFLINT(rxqi) |
			       V_FW_PFVF_CMD_NIQ(rxq));
	c.type_to_neq = htonl(V_FW_PFVF_CMD_CMASK(cmask) |
			      V_FW_PFVF_CMD_PMASK(pmask) |
			      V_FW_PFVF_CMD_NEQ(txq));
	c.tc_to_nexactf = htonl(V_FW_PFVF_CMD_TC(tc) | V_FW_PFVF_CMD_NVI(vi) |
				V_FW_PFVF_CMD_NEXACTF(nexact));
	c.r_caps_to_nethctrl = htonl(V_FW_PFVF_CMD_R_CAPS(rcaps) |
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
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_VI_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_WRITE | F_FW_CMD_EXEC |
			    V_FW_VI_CMD_PFN(pf) | V_FW_VI_CMD_VFN(vf));
	c.alloc_to_len16 = htonl(F_FW_VI_CMD_ALLOC | FW_LEN16(c));
	c.type_to_viid = htons(V_FW_VI_CMD_TYPE(idstype) |
			       V_FW_VI_CMD_FUNC(portfunc));
	c.portid_pkd = V_FW_VI_CMD_PORTID(port);
	c.nmac = nmac - 1;

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
		*rss_size = G_FW_VI_CMD_RSSSIZE(ntohs(c.norss_rsssize));
	return G_FW_VI_CMD_VIID(htons(c.type_to_viid));
}

/**
 *	t4_alloc_vi - allocate an [Ethernet Function] virtual interface
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@port: physical port associated with the VI
 *	@pf: the PF owning the VI
 *	@vf: the VF owning the VI
 *	@nmac: number of MAC addresses needed (1 to 5)
 *	@mac: the MAC addresses of the VI
 *	@rss_size: size of RSS table slice associated with this VI
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
 *	t4_free_vi - free a virtual interface
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@pf: the PF owning the VI
 *	@vf: the VF owning the VI
 *	@viid: virtual interface identifiler
 *
 *	Free a previously allocated virtual interface.
 */
int t4_free_vi(struct adapter *adap, unsigned int mbox, unsigned int pf,
	       unsigned int vf, unsigned int viid)
{
	struct fw_vi_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_VI_CMD) |
			    F_FW_CMD_REQUEST |
			    F_FW_CMD_EXEC |
			    V_FW_VI_CMD_PFN(pf) |
			    V_FW_VI_CMD_VFN(vf));
	c.alloc_to_len16 = htonl(F_FW_VI_CMD_FREE | FW_LEN16(c));
	c.type_to_viid = htons(V_FW_VI_CMD_VIID(viid));

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
 *	@vlanex: 1 to enable HVLAN extraction, 0 to disable it, -1 no change
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
	c.op_to_viid = htonl(V_FW_CMD_OP(FW_VI_RXMODE_CMD) | F_FW_CMD_REQUEST |
			     F_FW_CMD_WRITE | V_FW_VI_RXMODE_CMD_VIID(viid));
	c.retval_len16 = htonl(FW_LEN16(c));
	c.mtu_to_vlanexen = htonl(V_FW_VI_RXMODE_CMD_MTU(mtu) |
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
	unsigned int max_naddr = is_t4(adap) ?
				       NUM_MPS_CLS_SRAM_L_INSTANCES :
				       NUM_MPS_T5_CLS_SRAM_L_INSTANCES;
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
		c.op_to_viid = htonl(V_FW_CMD_OP(FW_VI_MAC_CMD) |
				     F_FW_CMD_REQUEST |
				     F_FW_CMD_WRITE |
				     V_FW_CMD_EXEC(free) |
				     V_FW_VI_MAC_CMD_VIID(viid));
		c.freemacs_to_len16 = htonl(V_FW_VI_MAC_CMD_FREEMACS(free) |
					    V_FW_CMD_LEN16(len16));

		for (i = 0, p = c.u.exact; i < fw_naddr; i++, p++) {
			p->valid_to_idx = htons(
				F_FW_VI_MAC_CMD_VALID |
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
			u16 index = G_FW_VI_MAC_CMD_IDX(ntohs(p->valid_to_idx));

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
	unsigned int max_mac_addr = is_t4(adap) ?
				    NUM_MPS_CLS_SRAM_L_INSTANCES :
				    NUM_MPS_T5_CLS_SRAM_L_INSTANCES;

	if (idx < 0)                             /* new allocation */
		idx = persist ? FW_VI_MAC_ADD_PERSIST_MAC : FW_VI_MAC_ADD_MAC;
	mode = add_smt ? FW_VI_MAC_SMT_AND_MPSTCAM : FW_VI_MAC_MPS_TCAM_ENTRY;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = htonl(V_FW_CMD_OP(FW_VI_MAC_CMD) | F_FW_CMD_REQUEST |
			     F_FW_CMD_WRITE | V_FW_VI_MAC_CMD_VIID(viid));
	c.freemacs_to_len16 = htonl(V_FW_CMD_LEN16(1));
	p->valid_to_idx = htons(F_FW_VI_MAC_CMD_VALID |
				V_FW_VI_MAC_CMD_SMAC_RESULT(mode) |
				V_FW_VI_MAC_CMD_IDX(idx));
	memcpy(p->macaddr, addr, sizeof(p->macaddr));

	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret == 0) {
		ret = G_FW_VI_MAC_CMD_IDX(ntohs(p->valid_to_idx));
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

	memset(&c, 0, sizeof(c));
	c.op_to_viid = htonl(V_FW_CMD_OP(FW_VI_MAC_CMD) | F_FW_CMD_REQUEST |
			     F_FW_CMD_WRITE | V_FW_VI_ENABLE_CMD_VIID(viid));
	c.freemacs_to_len16 = htonl(F_FW_VI_MAC_CMD_HASHVECEN |
				    V_FW_VI_MAC_CMD_HASHUNIEN(ucast) |
				    V_FW_CMD_LEN16(1));
	c.u.hash.hashvec = cpu_to_be64(vec);
	return t4_wr_mbox_meat(adap, mbox, &c, sizeof(c), NULL, sleep_ok);
}

/**
 *	t4_enable_vi - enable/disable a virtual interface
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@viid: the VI id
 *	@rx_en: 1=enable Rx, 0=disable Rx
 *	@tx_en: 1=enable Tx, 0=disable Tx
 *
 *	Enables/disables a virtual interface.
 */
int t4_enable_vi(struct adapter *adap, unsigned int mbox, unsigned int viid,
		 bool rx_en, bool tx_en)
{
	struct fw_vi_enable_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = htonl(V_FW_CMD_OP(FW_VI_ENABLE_CMD) | F_FW_CMD_REQUEST |
			     F_FW_CMD_EXEC | V_FW_VI_ENABLE_CMD_VIID(viid));
	c.ien_to_len16 = htonl(V_FW_VI_ENABLE_CMD_IEN(rx_en) |
			       V_FW_VI_ENABLE_CMD_EEN(tx_en) | FW_LEN16(c));
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

int t4_enable_vi_ns(struct adapter *adap, unsigned int mbox, unsigned int viid,
		 bool rx_en, bool tx_en)
{
	struct fw_vi_enable_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_viid = htonl(V_FW_CMD_OP(FW_VI_ENABLE_CMD) | F_FW_CMD_REQUEST |
			     F_FW_CMD_EXEC | V_FW_VI_ENABLE_CMD_VIID(viid));
	c.ien_to_len16 = htonl(V_FW_VI_ENABLE_CMD_IEN(rx_en) |
			       V_FW_VI_ENABLE_CMD_EEN(tx_en) | FW_LEN16(c));
	return t4_wr_mbox_ns(adap, mbox, &c, sizeof(c), NULL);
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
	c.op_to_viid = htonl(V_FW_CMD_OP(FW_VI_ENABLE_CMD) | F_FW_CMD_REQUEST |
			     F_FW_CMD_EXEC | V_FW_VI_ENABLE_CMD_VIID(viid));
	c.ien_to_len16 = htonl(F_FW_VI_ENABLE_CMD_LED | FW_LEN16(c));
	c.blinkdur = htons(nblinks);
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
}

/**
 *	t4_iq_start_stop - enable/disable an ingress queue and its FLs
 *	@adap: the adapter
 *	@mbox: mailbox to use for the FW command
 *	@start: %true to enable the queues, %false to disable them
 *	@pf: the PF owning the queues
 *	@vf: the VF owning the queues
 *	@iqid: ingress queue id
 *	@fl0id: FL0 queue id or 0xffff if no attached FL0
 *	@fl1id: FL1 queue id or 0xffff if no attached FL1
 *
 *	Starts or stops an ingress queue and its associated FLs, if any.
 */
int t4_iq_start_stop(struct adapter *adap, unsigned int mbox, bool start,
		     unsigned int pf, unsigned int vf, unsigned int iqid,
		     unsigned int fl0id, unsigned int fl1id)
{
	struct fw_iq_cmd c;

	memset(&c, 0, sizeof(c));
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_EXEC | V_FW_IQ_CMD_PFN(pf) |
			    V_FW_IQ_CMD_VFN(vf));
	c.alloc_to_len16 = htonl(V_FW_IQ_CMD_IQSTART(start) |
				 V_FW_IQ_CMD_IQSTOP(!start) | FW_LEN16(c));
	c.iqid = htons(iqid);
	c.fl0id = htons(fl0id);
	c.fl1id = htons(fl1id);
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
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_EXEC | V_FW_IQ_CMD_PFN(pf) |
			    V_FW_IQ_CMD_VFN(vf));
	c.alloc_to_len16 = htonl(F_FW_IQ_CMD_FREE | FW_LEN16(c));
	c.type_to_iqandstindex = htonl(V_FW_IQ_CMD_TYPE(iqtype));
	c.iqid = htons(iqid);
	c.fl0id = htons(fl0id);
	c.fl1id = htons(fl1id);
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
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_EQ_ETH_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_EXEC | V_FW_EQ_ETH_CMD_PFN(pf) |
			    V_FW_EQ_ETH_CMD_VFN(vf));
	c.alloc_to_len16 = htonl(F_FW_EQ_ETH_CMD_FREE | FW_LEN16(c));
	c.eqid_pkd = htonl(V_FW_EQ_ETH_CMD_EQID(eqid));
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
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_EQ_CTRL_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_EXEC | V_FW_EQ_CTRL_CMD_PFN(pf) |
			    V_FW_EQ_CTRL_CMD_VFN(vf));
	c.alloc_to_len16 = htonl(F_FW_EQ_CTRL_CMD_FREE | FW_LEN16(c));
	c.cmpliqid_eqid = htonl(V_FW_EQ_CTRL_CMD_EQID(eqid));
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
	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_EQ_OFLD_CMD) | F_FW_CMD_REQUEST |
			    F_FW_CMD_EXEC | V_FW_EQ_OFLD_CMD_PFN(pf) |
			    V_FW_EQ_OFLD_CMD_VFN(vf));
	c.alloc_to_len16 = htonl(F_FW_EQ_OFLD_CMD_FREE | FW_LEN16(c));
	c.eqid_pkd = htonl(V_FW_EQ_OFLD_CMD_EQID(eqid));
	return t4_wr_mbox(adap, mbox, &c, sizeof(c), NULL);
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
	unsigned int action = G_FW_PORT_CMD_ACTION(ntohl(p->action_to_len16));

	if (opcode == FW_PORT_CMD && action == FW_PORT_ACTION_GET_PORT_INFO) {
		/* link/module state change message */
		int speed = 0, fc = 0, i;
		int chan = G_FW_PORT_CMD_PORTID(ntohl(p->op_to_portid));
		struct port_info *pi = NULL;
		struct link_config *lc;
		u32 stat = ntohl(p->u.info.lstatus_to_modtype);
		int link_ok = (stat & F_FW_PORT_CMD_LSTATUS) != 0;
		u32 mod = G_FW_PORT_CMD_MODTYPE(stat);

		if (stat & F_FW_PORT_CMD_RXPAUSE)
			fc |= PAUSE_RX;
		if (stat & F_FW_PORT_CMD_TXPAUSE)
			fc |= PAUSE_TX;
		if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_100M))
			speed = SPEED_100;
		else if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_1G))
			speed = SPEED_1000;
		else if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_10G))
			speed = SPEED_10000;
		else if (stat & V_FW_PORT_CMD_LSPEED(FW_PORT_CAP_SPEED_40G))
			speed = SPEED_40000;

		for_each_port(adap, i) {
			pi = adap2pinfo(adap, i);
			if (pi->tx_chan == chan)
				break;
		}
		lc = &pi->link_cfg;

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
			lc->supported = ntohs(p->u.info.pcap);
			t4_os_link_changed(adap, i, link_ok, reason);
		}
		if (mod != pi->mod_type) {
			pi->mod_type = mod;
			t4_os_portmod_changed(adap, i);
		}
	} else {
		CH_WARN_RATELIMIT(adap,
		    "Unknown firmware reply 0x%x (0x%x)\n", opcode, action);
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
static void __devinit get_pci_mode(struct adapter *adapter,
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
static void __devinit init_link_config(struct link_config *lc,
				       unsigned int caps)
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

static int __devinit get_flash_params(struct adapter *adapter)
{
	int ret;
	u32 info = 0;

	ret = sf1_write(adapter, 1, 1, 0, SF_RD_ID);
	if (!ret)
		ret = sf1_read(adapter, 3, 0, 1, &info);
	t4_write_reg(adapter, A_SF_OP, 0);               /* unlock SF */
	if (ret < 0)
		return ret;

	if ((info & 0xff) != 0x20)             /* not a Numonix flash */
		return -EINVAL;
	info >>= 16;                           /* log2 of size */
	if (info >= 0x14 && info < 0x18)
		adapter->params.sf_nsec = 1 << (info - 16);
	else if (info == 0x18)
		adapter->params.sf_nsec = 64;
	else
		return -EINVAL;
	adapter->params.sf_size = 1 << info;
	return 0;
}

static void __devinit set_pcie_completion_timeout(struct adapter *adapter,
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

/**
 *	t4_prep_adapter - prepare SW and HW for operation
 *	@adapter: the adapter
 *	@reset: if true perform a HW reset
 *
 *	Initialize adapter SW state for the various HW modules, set initial
 *	values for some adapter tunables, take PHYs out of reset, and
 *	initialize the MDIO interface.
 */
int __devinit t4_prep_adapter(struct adapter *adapter)
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
	adapter->params.pci.vpd_cap_addr =
	    t4_os_find_pci_capability(adapter, PCI_CAP_ID_VPD);

	ret = get_flash_params(adapter);
	if (ret < 0)
		return ret;

	ret = get_vpd_params(adapter, &adapter->params.vpd);
	if (ret < 0)
		return ret;

	/* Cards with real ASICs have the chipid in the PCIe device id */
	t4_os_pci_read_cfg2(adapter, PCI_DEVICE_ID, &device_id);
	if (device_id >> 12 == adapter->params.chipid)
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
 *	t4_init_tp_params - initialize adap->params.tp
 *	@adap: the adapter
 *
 *	Initialize various fields of the adapter's TP Parameters structure.
 */
int __devinit t4_init_tp_params(struct adapter *adap)
{
	int chan;
	u32 v;

	v = t4_read_reg(adap, A_TP_TIMER_RESOLUTION);
	adap->params.tp.tre = G_TIMERRESOLUTION(v);
	adap->params.tp.dack_re = G_DELAYEDACKRESOLUTION(v);

	/* MODQ_REQ_MAP defaults to setting queues 0-3 to chan 0-3 */
	for (chan = 0; chan < NCHAN; chan++)
		adap->params.tp.tx_modq[chan] = chan;

	/*
	 * Cache the adapter's Compressed Filter Mode and global Incress
	 * Configuration.
	 */
        t4_read_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA,
                         &adap->params.tp.vlan_pri_map, 1,
                         A_TP_VLAN_PRI_MAP);
	t4_read_indirect(adap, A_TP_PIO_ADDR, A_TP_PIO_DATA,
			 &adap->params.tp.ingress_config, 1,
			 A_TP_INGRESS_CONFIG);

	/*
	 * Now that we have TP_VLAN_PRI_MAP cached, we can calculate the field
	 * shift positions of several elements of the Compressed Filter Tuple
	 * for this adapter which we need frequently ...
	 */
	adap->params.tp.vlan_shift = t4_filter_field_shift(adap, F_VLAN);
	adap->params.tp.vnic_shift = t4_filter_field_shift(adap, F_VNIC_ID);
	adap->params.tp.port_shift = t4_filter_field_shift(adap, F_PORT);
	adap->params.tp.protocol_shift = t4_filter_field_shift(adap, F_PROTOCOL);

	/*
	 * If TP_INGRESS_CONFIG.VNID == 0, then TP_VLAN_PRI_MAP.VNIC_ID
	 * represents the presense of an Outer VLAN instead of a VNIC ID.
	 */
	if ((adap->params.tp.ingress_config & F_VNIC) == 0)
		adap->params.tp.vnic_shift = -1;

	return 0;
}

/**
 *	t4_filter_field_shift - calculate filter field shift
 *	@adap: the adapter
 *	@filter_sel: the desired field (from TP_VLAN_PRI_MAP bits)
 *
 *	Return the shift position of a filter field within the Compressed
 *	Filter Tuple.  The filter field is specified via its selection bit
 *	within TP_VLAN_PRI_MAL (filter mode).  E.g. F_VLAN.
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
		case F_FCOE:          field_shift += W_FT_FCOE;          break;
		case F_PORT:          field_shift += W_FT_PORT;          break;
		case F_VNIC_ID:       field_shift += W_FT_VNIC_ID;       break;
		case F_VLAN:          field_shift += W_FT_VLAN;          break;
		case F_TOS:           field_shift += W_FT_TOS;           break;
		case F_PROTOCOL:      field_shift += W_FT_PROTOCOL;      break;
		case F_ETHERTYPE:     field_shift += W_FT_ETHERTYPE;     break;
		case F_MACMATCH:      field_shift += W_FT_MACMATCH;      break;
		case F_MPSHITTYPE:    field_shift += W_FT_MPSHITTYPE;    break;
		case F_FRAGMENTATION: field_shift += W_FT_FRAGMENTATION; break;
	    }
	}
	return field_shift;
}

int __devinit t4_port_init(struct port_info *p, int mbox, int pf, int vf)
{
	u8 addr[6];
	int ret, i, j;
	struct fw_port_cmd c;
	u16 rss_size;
	adapter_t *adap = p->adapter;

	memset(&c, 0, sizeof(c));

	for (i = 0, j = -1; i <= p->port_id; i++) {
		do {
			j++;
		} while ((adap->params.portvec & (1 << j)) == 0);
	}

	c.op_to_portid = htonl(V_FW_CMD_OP(FW_PORT_CMD) |
			       F_FW_CMD_REQUEST | F_FW_CMD_READ |
			       V_FW_PORT_CMD_PORTID(j));
	c.action_to_len16 = htonl(
		V_FW_PORT_CMD_ACTION(FW_PORT_ACTION_GET_PORT_INFO) |
		FW_LEN16(c));
	ret = t4_wr_mbox(adap, mbox, &c, sizeof(c), &c);
	if (ret)
		return ret;

	ret = t4_alloc_vi(adap, mbox, j, pf, vf, 1, addr, &rss_size);
	if (ret < 0)
		return ret;

	p->viid = ret;
	p->tx_chan = j;
	p->rx_chan_map = get_mps_bg_map(adap, j);
	p->lport = j;
	p->rss_size = rss_size;
	t4_os_set_hw_addr(adap, p->port_id, addr);

	ret = ntohl(c.u.info.lstatus_to_modtype);
	p->mdio_addr = (ret & F_FW_PORT_CMD_MDIOCAP) ?
		G_FW_PORT_CMD_MDIOADDR(ret) : -1;
	p->port_type = G_FW_PORT_CMD_PTYPE(ret);
	p->mod_type = G_FW_PORT_CMD_MODTYPE(ret);

	init_link_config(&p->link_cfg, ntohs(c.u.info.pcap));

	return 0;
}

int t4_sched_config(struct adapter *adapter, int type, int minmaxen)
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
			       NULL, 1);
}

int t4_sched_params(struct adapter *adapter, int type, int level, int mode,
		    int rateunit, int ratemode, int channel, int cl,
		    int minrate, int maxrate, int weight, int pktsize)
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
			       NULL, 1);
}
