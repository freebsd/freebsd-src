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

#ifndef __MLX5_CORE_H__
#define __MLX5_CORE_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#include <dev/mlxfw/mlxfw.h>

#define DRIVER_NAME "mlx5_core"
#ifndef DRIVER_VERSION
#define DRIVER_VERSION "3.5.2"
#endif
#define DRIVER_RELDATE "September 2019"

extern int mlx5_core_debug_mask;

#define mlx5_core_dbg(dev, format, ...)					\
	pr_debug("%s:%s:%d:(pid %d): " format,				\
		 (dev)->priv.name, __func__, __LINE__, curthread->td_proc->p_pid,	\
		 ##__VA_ARGS__)

#define mlx5_core_dbg_mask(dev, mask, format, ...)			\
do {									\
	if ((mask) & mlx5_core_debug_mask)				\
		mlx5_core_dbg(dev, format, ##__VA_ARGS__);		\
} while (0)

#define	mlx5_core_err(_dev, format, ...)					\
	device_printf((_dev)->pdev->dev.bsddev, "ERR: ""%s:%d:(pid %d): " format, \
		__func__, __LINE__, curthread->td_proc->p_pid, \
		##__VA_ARGS__)

#define	mlx5_core_warn(_dev, format, ...)				\
	device_printf((_dev)->pdev->dev.bsddev, "WARN: ""%s:%d:(pid %d): " format, \
		__func__, __LINE__, curthread->td_proc->p_pid, \
		##__VA_ARGS__)

#define	mlx5_core_info(_dev, format, ...)					\
	device_printf((_dev)->pdev->dev.bsddev, "INFO: ""%s:%d:(pid %d): " format, \
		__func__, __LINE__, curthread->td_proc->p_pid, \
		##__VA_ARGS__)

enum {
	MLX5_CMD_DATA, /* print command payload only */
	MLX5_CMD_TIME, /* print command execution time */
};

enum mlx5_semaphore_space_address {
	MLX5_SEMAPHORE_SW_RESET		= 0x20,
};

struct mlx5_core_dev;

enum mlx5_pddr_page_select {
	MLX5_PDDR_OPERATIONAL_INFO_PAGE            = 0x0,
	MLX5_PDDR_TROUBLESHOOTING_INFO_PAGE        = 0x1,
	MLX5_PDDR_MODULE_INFO_PAGE                 = 0x3,
};

enum mlx5_pddr_monitor_opcodes {
	MLX5_LINK_NO_ISSUE_OBSERVED                = 0x0,
	MLX5_LINK_PORT_CLOSED                      = 0x1,
	MLX5_LINK_AN_FAILURE                       = 0x2,
	MLX5_LINK_TRAINING_FAILURE                 = 0x5,
	MLX5_LINK_LOGICAL_MISMATCH                 = 0x9,
	MLX5_LINK_REMOTE_FAULT_INDICATION          = 0xe,
	MLX5_LINK_BAD_SIGNAL_INTEGRITY             = 0xf,
	MLX5_LINK_CABLE_COMPLIANCE_CODE_MISMATCH   = 0x10,
	MLX5_LINK_INTERNAL_ERR                     = 0x17,
	MLX5_LINK_INFO_NOT_AVAIL                   = 0x3ff,
	MLX5_LINK_CABLE_UNPLUGGED                  = 0x400,
	MLX5_LINK_LONG_RANGE_FOR_NON_MLX_CABLE     = 0x401,
	MLX5_LINK_BUS_STUCK                        = 0x402,
	MLX5_LINK_UNSUPP_EEPROM                    = 0x403,
	MLX5_LINK_PART_NUM_LIST                    = 0x404,
	MLX5_LINK_UNSUPP_CABLE                     = 0x405,
	MLX5_LINK_MODULE_TEMP_SHUTDOWN             = 0x406,
	MLX5_LINK_SHORTED_CABLE                    = 0x407,
	MLX5_LINK_POWER_BUDGET_EXCEEDED            = 0x408,
	MLX5_LINK_MNG_FORCED_DOWN                  = 0x409,
};

int mlx5_query_hca_caps(struct mlx5_core_dev *dev);
int mlx5_query_board_id(struct mlx5_core_dev *dev);
int mlx5_query_qcam_reg(struct mlx5_core_dev *mdev, u32 *qcam,
			u8 feature_group, u8 access_reg_group);
int mlx5_query_pcam_reg(struct mlx5_core_dev *dev, u32 *pcam,
			u8 feature_group, u8 access_reg_group);
int mlx5_query_mcam_reg(struct mlx5_core_dev *dev, u32 *mcap,
			u8 feature_group, u8 access_reg_group);
int mlx5_query_mfrl_reg(struct mlx5_core_dev *mdev, u8 *reset_level);
int mlx5_set_mfrl_reg(struct mlx5_core_dev *mdev, u8 reset_level);
int mlx5_cmd_init_hca(struct mlx5_core_dev *dev);
int mlx5_cmd_teardown_hca(struct mlx5_core_dev *dev);
int mlx5_cmd_force_teardown_hca(struct mlx5_core_dev *dev);
int mlx5_cmd_fast_teardown_hca(struct mlx5_core_dev *dev);
void mlx5_core_event(struct mlx5_core_dev *dev, enum mlx5_dev_event event,
		     unsigned long param);
void mlx5_enter_error_state(struct mlx5_core_dev *dev, bool force);
void mlx5_disable_device(struct mlx5_core_dev *dev);
void mlx5_recover_device(struct mlx5_core_dev *dev);
int mlx5_query_pddr_troubleshooting_info(struct mlx5_core_dev *mdev,
					 u16 *monitor_opcode,
					 u8 *status_message, size_t sm_len);

int mlx5_register_device(struct mlx5_core_dev *dev);
void mlx5_unregister_device(struct mlx5_core_dev *dev);

int mlx5_firmware_flash(struct mlx5_core_dev *dev, const struct firmware *fw);

void mlx5e_init(void);
void mlx5e_cleanup(void);

int mlx5_ctl_init(void);
void mlx5_ctl_fini(void);
void mlx5_fwdump_prep(struct mlx5_core_dev *mdev);
int mlx5_fwdump(struct mlx5_core_dev *mdev);
void mlx5_fwdump_clean(struct mlx5_core_dev *mdev);

struct mlx5_crspace_regmap {
	uint32_t addr;
	unsigned cnt;
};

extern struct pci_driver mlx5_core_driver;

SYSCTL_DECL(_hw_mlx5);

enum {
	MLX5_NIC_IFC_FULL		= 0,
	MLX5_NIC_IFC_DISABLED		= 1,
	MLX5_NIC_IFC_NO_DRAM_NIC	= 2,
	MLX5_NIC_IFC_INVALID		= 3,
	MLX5_NIC_IFC_SW_RESET		= 7,
};

u8 mlx5_get_nic_state(struct mlx5_core_dev *dev);
void mlx5_set_nic_state(struct mlx5_core_dev *dev, u8 state);

#endif /* __MLX5_CORE_H__ */
