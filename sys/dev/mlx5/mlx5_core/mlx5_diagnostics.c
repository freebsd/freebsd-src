/*-
 * Copyright (c) 2013-2017, Mellanox Technologies, Ltd.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <dev/mlx5/driver.h>
#include <dev/mlx5/port.h>
#include <dev/mlx5/diagnostics.h>
#include <dev/mlx5/mlx5_core/mlx5_core.h>
#include <net/sff8472.h>

const struct mlx5_core_diagnostics_entry
	mlx5_core_pci_diagnostics_table[
		MLX5_CORE_PCI_DIAGNOSTICS_NUM] = {
	MLX5_CORE_PCI_DIAGNOSTICS(MLX5_CORE_DIAGNOSTICS_ENTRY)
};

const struct mlx5_core_diagnostics_entry
	mlx5_core_general_diagnostics_table[
		MLX5_CORE_GENERAL_DIAGNOSTICS_NUM] = {
	MLX5_CORE_GENERAL_DIAGNOSTICS(MLX5_CORE_DIAGNOSTICS_ENTRY)
};

static int mlx5_core_get_index_of_diag_counter(
	const struct mlx5_core_diagnostics_entry *entry,
	int size, u16 counter_id)
{
	int x;

	/* check for invalid counter ID */
	if (counter_id == 0)
		return -1;

	/* lookup counter ID in table */
	for (x = 0; x != size; x++) {
		if (entry[x].counter_id == counter_id)
			return x;
	}
	return -1;
}

static void mlx5_core_put_diag_counter(
	const struct mlx5_core_diagnostics_entry *entry,
	u64 *array, int size, u16 counter_id, u64 value)
{
	int x;

	/* check for invalid counter ID */
	if (counter_id == 0)
		return;

	/* lookup counter ID in table */
	for (x = 0; x != size; x++) {
		if (entry[x].counter_id == counter_id) {
			array[x] = value;
			break;
		}
	}
}

int mlx5_core_set_diagnostics_full(struct mlx5_core_dev *dev,
				   u8 enable_pci, u8 enable_general)
{
	void *diag_params_ctx;
	void *in;
	int numcounters;
	int inlen;
	int err;
	int x;
	int y;

	if (MLX5_CAP_GEN(dev, debug) == 0)
		return 0;

	numcounters = MLX5_CAP_GEN(dev, num_of_diagnostic_counters);
	if (numcounters == 0)
		return 0;

	inlen = MLX5_ST_SZ_BYTES(set_diagnostic_params_in) +
	    MLX5_ST_SZ_BYTES(diagnostic_counter) * numcounters;
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return -ENOMEM;

	diag_params_ctx = MLX5_ADDR_OF(set_diagnostic_params_in, in,
				       diagnostic_params_ctx);

	MLX5_SET(diagnostic_params_context, diag_params_ctx,
		 enable, enable_pci || enable_general);
	MLX5_SET(diagnostic_params_context, diag_params_ctx,
		 single, 1);
	MLX5_SET(diagnostic_params_context, diag_params_ctx,
		 on_demand, 1);

	/* collect the counters we want to enable */
	for (x = y = 0; x != numcounters; x++) {
		u16 counter_id =
			MLX5_CAP_DEBUG(dev, diagnostic_counter[x].counter_id);
		int index = -1;

		if (index < 0 && enable_pci != 0) {
			/* check if counter ID exists in local table */
			index = mlx5_core_get_index_of_diag_counter(
			    mlx5_core_pci_diagnostics_table,
			    MLX5_CORE_PCI_DIAGNOSTICS_NUM,
			    counter_id);
		}
		if (index < 0 && enable_general != 0) {
			/* check if counter ID exists in local table */
			index = mlx5_core_get_index_of_diag_counter(
			    mlx5_core_general_diagnostics_table,
			    MLX5_CORE_GENERAL_DIAGNOSTICS_NUM,
			    counter_id);
		}
		if (index < 0)
			continue;

		MLX5_SET(diagnostic_params_context,
			 diag_params_ctx,
			 counter_id[y].counter_id,
			 counter_id);
		y++;
	}

	/* recompute input length */
	inlen = MLX5_ST_SZ_BYTES(set_diagnostic_params_in) +
	    MLX5_ST_SZ_BYTES(diagnostic_counter) * y;

	/* set number of counters */
	MLX5_SET(diagnostic_params_context, diag_params_ctx,
		 num_of_counters, y);

	/* execute firmware command */
	err = mlx5_set_diagnostic_params(dev, in, inlen);

	kvfree(in);

	return err;
}

int mlx5_core_get_diagnostics_full(struct mlx5_core_dev *dev,
				   union mlx5_core_pci_diagnostics *pdiag,
				   union mlx5_core_general_diagnostics *pgen)
{
	void *out;
	void *in;
	int numcounters;
	int outlen;
	int inlen;
	int err;
	int x;

	if (MLX5_CAP_GEN(dev, debug) == 0)
		return 0;

	numcounters = MLX5_CAP_GEN(dev, num_of_diagnostic_counters);
	if (numcounters == 0)
		return 0;

	outlen = MLX5_ST_SZ_BYTES(query_diagnostic_counters_out) +
	    MLX5_ST_SZ_BYTES(diagnostic_counter) * numcounters;

	out = mlx5_vzalloc(outlen);
	if (out == NULL)
		return -ENOMEM;

	err = mlx5_query_diagnostic_counters(dev, 1, 0, out, outlen);
	if (err == 0) {
		for (x = 0; x != numcounters; x++) {
			u16 counter_id = MLX5_GET(
			    query_diagnostic_counters_out,
			    out, diag_counter[x].counter_id);
			u64 counter_value = MLX5_GET64(
			    query_diagnostic_counters_out,
			    out, diag_counter[x].counter_value_h);

			if (pdiag != NULL) {
				mlx5_core_put_diag_counter(
				    mlx5_core_pci_diagnostics_table,
				    pdiag->array,
				    MLX5_CORE_PCI_DIAGNOSTICS_NUM,
				    counter_id, counter_value);
			}
			if (pgen != NULL) {
				mlx5_core_put_diag_counter(
				    mlx5_core_general_diagnostics_table,
				    pgen->array,
				    MLX5_CORE_GENERAL_DIAGNOSTICS_NUM,
				    counter_id, counter_value);
			}
		}
	}
	kvfree(out);

	if (pdiag != NULL) {
		inlen = MLX5_ST_SZ_BYTES(mpcnt_reg);
		outlen = MLX5_ST_SZ_BYTES(mpcnt_reg);

		in = mlx5_vzalloc(inlen);
		if (in == NULL)
			return -ENOMEM;

		out = mlx5_vzalloc(outlen);
		if (out == NULL) {
			kvfree(in);
			return -ENOMEM;
		}
		MLX5_SET(mpcnt_reg, in, grp,
			 MLX5_PCIE_PERFORMANCE_COUNTERS_GROUP);

		err = mlx5_core_access_reg(dev, in, inlen, out, outlen,
					   MLX5_REG_MPCNT, 0, 0);
		if (err == 0) {
			void *pcounters = MLX5_ADDR_OF(mpcnt_reg, out,
			    counter_set.pcie_perf_counters);

			pdiag->counter.rx_pci_errors =
			    MLX5_GET(pcie_perf_counters,
				     pcounters, rx_errors);
			pdiag->counter.tx_pci_errors =
			    MLX5_GET(pcie_perf_counters,
				     pcounters, tx_errors);
		}
		MLX5_SET(mpcnt_reg, in, grp,
			 MLX5_PCIE_TIMERS_AND_STATES_COUNTERS_GROUP);

		err = mlx5_core_access_reg(dev, in, inlen, out, outlen,
		    MLX5_REG_MPCNT, 0, 0);
		if (err == 0) {
			void *pcounters = MLX5_ADDR_OF(mpcnt_reg, out,
			    counter_set.pcie_timers_states);

			pdiag->counter.tx_pci_non_fatal_errors =
			    MLX5_GET(pcie_timers_states,
				     pcounters, non_fatal_err_msg_sent);
			pdiag->counter.tx_pci_fatal_errors =
			    MLX5_GET(pcie_timers_states,
				     pcounters, fatal_err_msg_sent);
		}
		kvfree(in);
		kvfree(out);
	}
	return 0;
}

int mlx5_core_supports_diagnostics(struct mlx5_core_dev *dev, u16 counter_id)
{
	int numcounters;
	int x;

	if (MLX5_CAP_GEN(dev, debug) == 0)
		return 0;

	/* check for any counter */
	if (counter_id == 0)
		return 1;

	numcounters = MLX5_CAP_GEN(dev, num_of_diagnostic_counters);

	/* check if counter ID exists in debug capability */
	for (x = 0; x != numcounters; x++) {
		if (MLX5_CAP_DEBUG(dev, diagnostic_counter[x].counter_id) ==
		    counter_id)
			return 1;
	}
	return 0;			/* not supported counter */
}

/*
 * Read the first three bytes of the eeprom in order to get the needed info
 * for the whole reading.
 * Byte 0 - Identifier byte
 * Byte 1 - Revision byte
 * Byte 2 - Status byte
 */
int
mlx5_get_eeprom_info(struct mlx5_core_dev *dev, struct mlx5_eeprom *eeprom)
{
	u32 data = 0;
	int size_read = 0;
	int ret;

	ret = mlx5_query_module_num(dev, &eeprom->module_num);
	if (ret) {
		mlx5_core_err(dev, "Failed query module error=%d\n", ret);
		return (-ret);
	}

	/* Read the first three bytes to get Identifier, Revision and Status */
	ret = mlx5_query_eeprom(dev, eeprom->i2c_addr, eeprom->page_num,
	    eeprom->device_addr, MLX5_EEPROM_INFO_BYTES, eeprom->module_num, &data,
	    &size_read);
	if (ret) {
		mlx5_core_err(dev,
		    "Failed query EEPROM module error=0x%x\n", ret);
		return (-ret);
	}

	switch (data & MLX5_EEPROM_IDENTIFIER_BYTE_MASK) {
	case SFF_8024_ID_QSFP:
		eeprom->type = MLX5_ETH_MODULE_SFF_8436;
		eeprom->len = MLX5_ETH_MODULE_SFF_8436_LEN;
		break;
	case SFF_8024_ID_QSFPPLUS:
	case SFF_8024_ID_QSFP28:
		if ((data & MLX5_EEPROM_IDENTIFIER_BYTE_MASK) == SFF_8024_ID_QSFP28 ||
		    ((data & MLX5_EEPROM_REVISION_ID_BYTE_MASK) >> 8) >= 0x3) {
			eeprom->type = MLX5_ETH_MODULE_SFF_8636;
			eeprom->len = MLX5_ETH_MODULE_SFF_8636_LEN;
		} else {
			eeprom->type = MLX5_ETH_MODULE_SFF_8436;
			eeprom->len = MLX5_ETH_MODULE_SFF_8436_LEN;
		}
		if ((data & MLX5_EEPROM_PAGE_3_VALID_BIT_MASK) == 0)
			eeprom->page_valid = 1;
		break;
	case SFF_8024_ID_SFP:
		eeprom->type = MLX5_ETH_MODULE_SFF_8472;
		eeprom->len = MLX5_ETH_MODULE_SFF_8472_LEN;
		break;
	default:
		mlx5_core_err(dev, "Not recognized cable type = 0x%x(%s)\n",
		    data & MLX5_EEPROM_IDENTIFIER_BYTE_MASK,
		    sff_8024_id[data & MLX5_EEPROM_IDENTIFIER_BYTE_MASK]);
		return (EINVAL);
	}
	return (0);
}

/* Read both low and high pages of the eeprom */
int
mlx5_get_eeprom(struct mlx5_core_dev *dev, struct mlx5_eeprom *ee)
{
	int size_read = 0;
	int ret;

	if (ee->len == 0)
		return (EINVAL);

	/* Read low page of the eeprom */
	while (ee->device_addr < ee->len) {
		ret = mlx5_query_eeprom(dev, ee->i2c_addr, ee->page_num, ee->device_addr,
		    ee->len - ee->device_addr, ee->module_num,
		    ee->data + (ee->device_addr / 4), &size_read);
		if (ret) {
			mlx5_core_err(dev,
			    "Failed reading EEPROM, error = 0x%02x\n", ret);
			return (-ret);
		}
		ee->device_addr += size_read;
	}

	/* Read high page of the eeprom */
	if (ee->page_valid == 1) {
		ee->device_addr = MLX5_EEPROM_HIGH_PAGE_OFFSET;
		ee->page_num = MLX5_EEPROM_HIGH_PAGE;
		size_read = 0;
		while (ee->device_addr < MLX5_EEPROM_PAGE_LENGTH) {
			ret = mlx5_query_eeprom(dev, ee->i2c_addr, ee->page_num,
			    ee->device_addr, MLX5_EEPROM_PAGE_LENGTH - ee->device_addr,
			    ee->module_num, ee->data + (ee->len / 4) +
			    ((ee->device_addr - MLX5_EEPROM_HIGH_PAGE_OFFSET) / 4),
			    &size_read);
			if (ret) {
				mlx5_core_err(dev,
				    "Failed reading EEPROM, error = 0x%02x\n",
				    ret);
				return (-ret);
			}
			ee->device_addr += size_read;
		}
	}
	return (0);
}

/*
 * Read cable EEPROM module information by first inspecting the first
 * three bytes to get the initial information for a whole reading.
 * Information will be printed to dmesg.
 */
int
mlx5_read_eeprom(struct mlx5_core_dev *dev, struct mlx5_eeprom *eeprom)
{
	int error;

	eeprom->i2c_addr = MLX5_I2C_ADDR_LOW;
	eeprom->device_addr = 0;
	eeprom->page_num = MLX5_EEPROM_LOW_PAGE;
	eeprom->page_valid = 0;

	/* Read three first bytes to get important info */
	error = mlx5_get_eeprom_info(dev, eeprom);
	if (error) {
		mlx5_core_err(dev,
		    "Failed reading EEPROM initial information\n");
		return (error);
	}
	/*
	 * Allocate needed length buffer and additional space for
	 * page 0x03
	 */
	eeprom->data = malloc(eeprom->len + MLX5_EEPROM_PAGE_LENGTH,
	    M_MLX5_EEPROM, M_WAITOK | M_ZERO);

	/* Read the whole eeprom information */
	error = mlx5_get_eeprom(dev, eeprom);
	if (error) {
		mlx5_core_err(dev, "Failed reading EEPROM\n");
		error = 0;
		/*
		 * Continue printing partial information in case of
		 * an error
		 */
	}
	free(eeprom->data, M_MLX5_EEPROM);

	return (error);
}


