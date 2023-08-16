/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2023 Bjoern A. Zeeb
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

#ifndef	_LINUXKPI_LINUX_MHI_H
#define	_LINUXKPI_LINUX_MHI_H

#include <linux/types.h>

/* Modem Host Interface (MHI) */

/* XXX FIXME */
#define	MHI_DB_BRST_DISABLE	0
#define	MHI_ER_CTRL		0

enum mhi_callback {
	MHI_CB_SYS_ERROR,
	MHI_CB_BW_REQ,
	MHI_CB_EE_MISSION_MODE,
	MHI_CB_EE_RDDM,
	MHI_CB_FATAL_ERROR,
	MHI_CB_IDLE,
	MHI_CB_LPM_ENTER,
	MHI_CB_LPM_EXIT,
	MHI_CB_PENDING_DATA,
};

struct mhi_channel_config {
	const char		*name;
	int	auto_queue, dir, doorbell, doorbell_mode_switch, ee_mask, event_ring, lpm_notify, num, num_elements, offload_channel, pollcfg;
};

struct mhi_event_config {
	int	client_managed, data_type, hardware_event, irq, irq_moderation_ms, mode, num_elements, offload_channel, priority;
};

struct mhi_device {
};

struct mhi_controller_config {
	const struct mhi_channel_config	*ch_cfg;
	struct mhi_event_config		*event_cfg;

	int	buf_len, max_channels, num_channels, num_events, use_bounce_buf;

	uint32_t			timeout_ms;
};

struct mhi_controller {
	struct device			*cntrl_dev;
	struct mhi_device		*mhi_dev;
	void				*regs;
	int				*irq;
	const char			*fw_image;

	bool				fbc_download;
	size_t				rddm_size;
	size_t				sbl_size;
	size_t				seg_len;
	size_t				reg_len;
	int				nr_irqs;
	unsigned long			irq_flags;
	uint32_t			timeout_ms;

	dma_addr_t			iova_start;
	dma_addr_t			iova_stop;

	int				(*runtime_get)(struct mhi_controller *);
	void				(*runtime_put)(struct mhi_controller *);
	void				(*status_cb)(struct mhi_controller *, enum mhi_callback);
	int				(*read_reg)(struct mhi_controller *, void __iomem *, uint32_t *);
	void				(*write_reg)(struct mhi_controller *, void __iomem *, uint32_t);
};

/* -------------------------------------------------------------------------- */

struct mhi_controller *linuxkpi_mhi_alloc_controller(void);
void linuxkpi_mhi_free_controller(struct mhi_controller *);
int linuxkpi_mhi_register_controller(struct mhi_controller *,
    const struct mhi_controller_config *);
void linuxkpi_mhi_unregister_controller(struct mhi_controller *);

/* -------------------------------------------------------------------------- */

static inline struct mhi_controller *
mhi_alloc_controller(void)
{

	/* Keep allocations internal to our implementation. */
	return (linuxkpi_mhi_alloc_controller());
}

static inline void
mhi_free_controller(struct mhi_controller *mhi_ctrl)
{

	linuxkpi_mhi_free_controller(mhi_ctrl);
}

static inline int
mhi_register_controller(struct mhi_controller *mhi_ctrl,
    const struct mhi_controller_config *cfg)
{

	return (linuxkpi_mhi_register_controller(mhi_ctrl, cfg));
}

static inline void
mhi_unregister_controller(struct mhi_controller *mhi_ctrl)
{

	linuxkpi_mhi_unregister_controller(mhi_ctrl);
}

/* -------------------------------------------------------------------------- */

static __inline int
mhi_device_get_sync(struct mhi_device *mhi_dev)
{
	/* XXX TODO */
	return (-1);
}

static __inline void
mhi_device_put(struct mhi_device *mhi_dev)
{
	/* XXX TODO */
}

/* -------------------------------------------------------------------------- */

static __inline int
mhi_prepare_for_power_up(struct mhi_controller *mhi_ctrl)
{
	/* XXX TODO */
	return (0);
}

static __inline int
mhi_sync_power_up(struct mhi_controller *mhi_ctrl)
{
	/* XXX TODO */
	return (0);
}

static __inline int
mhi_async_power_up(struct mhi_controller *mhi_ctrl)
{
	/* XXX TODO */
	return (0);
}

static __inline void
mhi_power_down(struct mhi_controller *mhi_ctrl, bool x)
{
	/* XXX TODO */
}

static __inline void
mhi_unprepare_after_power_down(struct mhi_controller *mhi_ctrl)
{
	/* XXX TODO */
}

/* -------------------------------------------------------------------------- */

static __inline int
mhi_pm_suspend(struct mhi_controller *mhi_ctrl)
{
	/* XXX TODO */
	return (0);
}

static __inline int
mhi_pm_resume(struct mhi_controller *mhi_ctrl)
{
	/* XXX TODO */
	return (0);
}

static __inline int
mhi_pm_resume_force(struct mhi_controller *mhi_ctrl)
{
	/* XXX TODO */
	return (0);
}

/* -------------------------------------------------------------------------- */

static __inline int
mhi_force_rddm_mode(struct mhi_controller *mhi_ctrl)
{
	/* XXX TODO */
	return (0);
}

#endif	/* _LINUXKPI_LINUX_MHI_H */
