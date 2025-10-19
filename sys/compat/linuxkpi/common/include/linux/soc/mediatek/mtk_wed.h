/*-
 * Copyright (c) 2022-2025 Bjoern A. Zeeb
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_LINUXKPI_LINUX_SOC_MEDIATEK_MTK_WED_H
#define	_LINUXKPI_LINUX_SOC_MEDIATEK_MTK_WED_H

#include <linux/kernel.h>	/* pr_debug */

struct mtk_wed_device {
};

#define	WED_WO_STA_REC	0x6

#define	mtk_wed_device_start(_dev, _mask)		do { pr_debug("%s: TODO\n", __func__); } while(0)
#define	mtk_wed_device_detach(_dev)			do { pr_debug("%s: TODO\n", __func__); } while(0)
#define	mtk_wed_device_irq_get(_dev, _mask)		0
#define	mtk_wed_device_irq_set_mask(_dev, _mask)	do { pr_debug("%s: TODO\n", __func__); } while(0)
#define	mtk_wed_device_update_msg(_dev, _id, _msg, _len)	({ pr_debug("%s: TODO\n", __func__); -ENODEV; })
#define	mtk_wed_device_dma_reset(_dev)			do { pr_debug("%s: TODO\n", __func__); } while (0)
#define	mtk_wed_device_ppe_check(_dev, _skb, _reason, _entry) \
    do { pr_debug("%s: TODO\n", __func__); } while (0)
#define	mtk_wed_device_stop(_dev)			do { pr_debug("%s: TODO\n", __func__); } while(0)
#define	mtk_wed_device_start_hw_rro(_dev, _mask, _b)	do { pr_debug("%s: TODO\n", __func__); } while(0)
#define	mtk_wed_device_setup_tc(_dev, _ndev, _type, _tdata)	({ pr_debug("%s: TODO\n", __func__); -EOPNOTSUPP; })

static inline bool
mtk_wed_device_active(struct mtk_wed_device *dev __unused)
{

	pr_debug("%s: TODO\n", __func__);
	return (false);
}

static inline bool
mtk_wed_get_rx_capa(struct mtk_wed_device *dev __unused)
{

	pr_debug("%s: TODO\n", __func__);
	return (false);
}

#endif	/* _LINUXKPI_LINUX_SOC_MEDIATEK_MTK_WED_H */
