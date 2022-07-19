/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "adf_dev_err.h"

struct reg_info {
	size_t offs;
	char *name;
};

static struct reg_info adf_err_regs[] = {
	{ ADF_ERRSOU0, "ERRSOU0" },
	{ ADF_ERRSOU1, "ERRSOU1" },
	{ ADF_ERRSOU3, "ERRSOU3" },
	{ ADF_ERRSOU4, "ERRSOU4" },
	{ ADF_ERRSOU5, "ERRSOU5" },
	{ ADF_RICPPINTSTS, "RICPPINTSTS" },
	{ ADF_RIERRPUSHID, "RIERRPUSHID" },
	{ ADF_RIERRPULLID, "RIERRPULLID" },
	{ ADF_CPP_CFC_ERR_STATUS, "CPP_CFC_ERR_STATUS" },
	{ ADF_CPP_CFC_ERR_PPID, "CPP_CFC_ERR_PPID" },
	{ ADF_TICPPINTSTS, "TICPPINTSTS" },
	{ ADF_TIERRPUSHID, "TIERRPUSHID" },
	{ ADF_TIERRPULLID, "TIERRPULLID" },
	{ ADF_SECRAMUERR, "SECRAMUERR" },
	{ ADF_SECRAMUERRAD, "SECRAMUERRAD" },
	{ ADF_CPPMEMTGTERR, "CPPMEMTGTERR" },
	{ ADF_ERRPPID, "ERRPPID" },
};

static u32
adf_get_intstatsssm(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_INTSTATSSM(dev));
}

static u32
adf_get_pperr(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_PPERR(dev));
}

static u32
adf_get_pperrid(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_PPERRID(dev));
}

static u32
adf_get_uerrssmsh(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMSH(dev));
}

static u32
adf_get_uerrssmshad(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMSHAD(dev));
}

static u32
adf_get_uerrssmmmp0(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMP(dev, 0));
}

static u32
adf_get_uerrssmmmp1(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMP(dev, 1));
}

static u32
adf_get_uerrssmmmp2(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMP(dev, 2));
}

static u32
adf_get_uerrssmmmp3(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMP(dev, 3));
}

static u32
adf_get_uerrssmmmp4(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMP(dev, 4));
}

static u32
adf_get_uerrssmmmpad0(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMPAD(dev, 0));
}

static u32
adf_get_uerrssmmmpad1(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMPAD(dev, 1));
}

static u32
adf_get_uerrssmmmpad2(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMPAD(dev, 2));
}

static u32
adf_get_uerrssmmmpad3(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMPAD(dev, 3));
}

static u32
adf_get_uerrssmmmpad4(struct resource *pmisc_bar_addr, size_t dev)
{
	return ADF_CSR_RD(pmisc_bar_addr, ADF_UERRSSMMMPAD(dev, 4));
}

struct reg_array_info {
	u32 (*read)(struct resource *pmisc_bar_addr, size_t dev);
	char *name;
};

static struct reg_array_info adf_accel_err_regs[] = {
	{ adf_get_intstatsssm, "INTSTATSSM" },
	{ adf_get_pperr, "PPERR" },
	{ adf_get_pperrid, "PPERRID" },
	{ adf_get_uerrssmsh, "UERRSSMSH" },
	{ adf_get_uerrssmshad, "UERRSSMSHAD" },
	{ adf_get_uerrssmmmp0, "UERRSSMMMP0" },
	{ adf_get_uerrssmmmp1, "UERRSSMMMP1" },
	{ adf_get_uerrssmmmp2, "UERRSSMMMP2" },
	{ adf_get_uerrssmmmp3, "UERRSSMMMP3" },
	{ adf_get_uerrssmmmp4, "UERRSSMMMP4" },
	{ adf_get_uerrssmmmpad0, "UERRSSMMMPAD0" },
	{ adf_get_uerrssmmmpad1, "UERRSSMMMPAD1" },
	{ adf_get_uerrssmmmpad2, "UERRSSMMMPAD2" },
	{ adf_get_uerrssmmmpad3, "UERRSSMMMPAD3" },
	{ adf_get_uerrssmmmpad4, "UERRSSMMMPAD4" },
};

static char adf_printf_buf[128] = { 0 };
static size_t adf_printf_len;

static void
adf_print_flush(struct adf_accel_dev *accel_dev)
{
	if (adf_printf_len > 0) {
		device_printf(GET_DEV(accel_dev), "%.128s\n", adf_printf_buf);
		adf_printf_len = 0;
	}
}

static void
adf_print_reg(struct adf_accel_dev *accel_dev,
	      const char *name,
	      size_t idx,
	      u32 val)
{
	adf_printf_len += snprintf(&adf_printf_buf[adf_printf_len],
				   sizeof(adf_printf_buf) - adf_printf_len,
				   "%s[%zu],%.8x,",
				   name,
				   idx,
				   val);

	if (adf_printf_len >= 80)
		adf_print_flush(accel_dev);
}

void
adf_print_err_registers(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_bar *misc_bar =
	    &GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	struct resource *csr = misc_bar->virt_addr;
	size_t i;
	unsigned int mask;
	u32 val;

	for (i = 0; i < ARRAY_SIZE(adf_err_regs); ++i) {
		val = ADF_CSR_RD(csr, adf_err_regs[i].offs);

		adf_print_reg(accel_dev, adf_err_regs[i].name, 0, val);
	}

	for (i = 0; i < ARRAY_SIZE(adf_accel_err_regs); ++i) {
		size_t accel;

		for (accel = 0, mask = hw_data->accel_mask; mask;
		     accel++, mask >>= 1) {
			if (!(mask & 1))
				continue;
			val = adf_accel_err_regs[i].read(csr, accel);

			adf_print_reg(accel_dev,
				      adf_accel_err_regs[i].name,
				      accel,
				      val);
		}
	}

	adf_print_flush(accel_dev);
}

static void
adf_log_slice_hang(struct adf_accel_dev *accel_dev,
		   u8 accel_num,
		   char *unit_name,
		   u8 unit_number)
{
	device_printf(GET_DEV(accel_dev),
		      "CPM #%x Slice Hang Detected unit: %s%d.\n",
		      accel_num,
		      unit_name,
		      unit_number);
}

bool
adf_handle_slice_hang(struct adf_accel_dev *accel_dev,
		      u8 accel_num,
		      struct resource *csr,
		      u32 slice_hang_offset)
{
	u32 slice_hang = ADF_CSR_RD(csr, slice_hang_offset);

	if (!slice_hang)
		return false;

	if (slice_hang & ADF_SLICE_HANG_AUTH0_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Auth", 0);
	if (slice_hang & ADF_SLICE_HANG_AUTH1_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Auth", 1);
	if (slice_hang & ADF_SLICE_HANG_AUTH2_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Auth", 2);
	if (slice_hang & ADF_SLICE_HANG_CPHR0_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Cipher", 0);
	if (slice_hang & ADF_SLICE_HANG_CPHR1_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Cipher", 1);
	if (slice_hang & ADF_SLICE_HANG_CPHR2_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Cipher", 2);
	if (slice_hang & ADF_SLICE_HANG_CMP0_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Comp", 0);
	if (slice_hang & ADF_SLICE_HANG_CMP1_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Comp", 1);
	if (slice_hang & ADF_SLICE_HANG_XLT0_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Xlator", 0);
	if (slice_hang & ADF_SLICE_HANG_XLT1_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "Xlator", 1);
	if (slice_hang & ADF_SLICE_HANG_MMP0_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "MMP", 0);
	if (slice_hang & ADF_SLICE_HANG_MMP1_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "MMP", 1);
	if (slice_hang & ADF_SLICE_HANG_MMP2_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "MMP", 2);
	if (slice_hang & ADF_SLICE_HANG_MMP3_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "MMP", 3);
	if (slice_hang & ADF_SLICE_HANG_MMP4_MASK)
		adf_log_slice_hang(accel_dev, accel_num, "MMP", 4);

	/* Clear the associated interrupt */
	ADF_CSR_WR(csr, slice_hang_offset, slice_hang);

	return true;
}

/**
 * adf_check_slice_hang() - Check slice hang status
 *
 * Return: true if a slice hange interrupt is serviced..
 */
bool
adf_check_slice_hang(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_bar *misc_bar =
	    &GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	struct resource *csr = misc_bar->virt_addr;
	u32 errsou3 = ADF_CSR_RD(csr, ADF_ERRSOU3);
	u32 errsou5 = ADF_CSR_RD(csr, ADF_ERRSOU5);
	u32 offset;
	u32 accel_num;
	bool handled = false;
	u32 errsou[] = { errsou3, errsou3, errsou5, errsou5, errsou5 };
	u32 mask[] = { ADF_EMSK3_CPM0_MASK,
		       ADF_EMSK3_CPM1_MASK,
		       ADF_EMSK5_CPM2_MASK,
		       ADF_EMSK5_CPM3_MASK,
		       ADF_EMSK5_CPM4_MASK };
	unsigned int accel_mask;

	for (accel_num = 0, accel_mask = hw_data->accel_mask; accel_mask;
	     accel_num++, accel_mask >>= 1) {
		if (!(accel_mask & 1))
			continue;
		if (accel_num >= ARRAY_SIZE(errsou)) {
			device_printf(GET_DEV(accel_dev),
				      "Invalid accel_num %d.\n",
				      accel_num);
			break;
		}

		if (errsou[accel_num] & mask[accel_num]) {
			if (ADF_CSR_RD(csr, ADF_INTSTATSSM(accel_num)) &
			    ADF_INTSTATSSM_SHANGERR) {
				offset = ADF_SLICEHANGSTATUS(accel_num);
				handled |= adf_handle_slice_hang(accel_dev,
								 accel_num,
								 csr,
								 offset);
			}
		}
	}

	return handled;
}
