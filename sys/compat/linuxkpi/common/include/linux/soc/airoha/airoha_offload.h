/*-
 * Copyright (c) 2026 Bjoern A. Zeeb
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_LINUXKPI_LINUX_SOC_AIROHA_AIROHA_OFFLOAD_H
#define	_LINUXKPI_LINUX_SOC_AIROHA_AIROHA_OFFLOAD_H

#include <linux/kernel.h>	/* pr_debug */

enum airoha_npu_wlan_get_cmd {
	__dummy_airoha_npu_wlan_get_cmd,
};
enum airoha_npu_wlan_set_cmd {
	__dummy_airoha_npu_wlan_set_cmd,
};

struct airoha_npu {
};
struct airoha_npu_rx_dma_desc {
};
struct airoha_npu_tx_dma_desc {
};

static __inline int
airoha_npu_wlan_send_msg(void *npu, int ifindex,
    enum airoha_npu_wlan_set_cmd cmd, void *val, size_t len, gfp_t gfp)
{
	pr_debug("%s: TODO\n", __func__);
	return (-EOPNOTSUPP);
}

static __inline int
airoha_npu_wlan_get_msg(void *npu, int ifindex,
    enum airoha_npu_wlan_get_cmd cmd, void *val, size_t len, gfp_t gfp)
{
	pr_debug("%s: TODO\n", __func__);
	return (-EOPNOTSUPP);
}

static __inline void
airoha_npu_wlan_enable_irq(struct airoha_npu *npu, int q)
{
	pr_debug("%s: TODO\n", __func__);
}

#endif	/* _LINUXKPI_LINUX_SOC_AIROHA_AIROHA_OFFLOAD_H */
