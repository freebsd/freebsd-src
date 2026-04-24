/*-
 * Copyright (c) 2011-2012 Semihalf.
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

#ifndef FMAN_H_
#define FMAN_H_

#include <dev/fdt/simplebus.h>
#include <sys/vmem.h>

#define	FMAN_BMI_FIFO_UNITS	0x100
#define	FMAN_BMI_FIFO_ALIGN	0x100

#define	FM_FD_ERR_DMA		0x01000000
#define	FM_FD_ERR_FPE		0x00080000
#define	FM_FD_ERR_FSE		0x00040000
#define	FM_FD_ERR_DIS		0x00020000
#define	FM_FD_ERR_EOF		0x00008000
#define	FM_FD_ERR_NSS		0x00004000
#define	FM_FD_ERR_KSO		0x00002000
#define	FM_FD_ERR_IPP		0x00000200
#define FM_FD_ERR_PTE		0x00000080
#define	FM_FD_ERR_ISP		0x00000040
#define	FM_FD_ERR_PHE		0x00000020
#define	FM_FD_ERR_BLE		0x00000008

/**
 * FMan driver instance data.
 */
struct fman_softc {
	struct simplebus_softc sc_base;
	struct resource *mem_res;
	struct resource *irq_res;
	struct resource *err_irq_res;
	struct rman	rman;
	vmem_t	*muram_vmem;
	int mem_rid;
	int irq_rid;
	int err_irq_rid;
	void *irq_cookie;
	int qman_chan_base;
	int qman_chan_count;
	int fm_id;

	int sc_revision_major;
	int sc_revision_minor;

	uint16_t clock;
	bool timestamps;

	uint32_t iram_size;
	uint32_t dma_thresh_max_commq;
	uint32_t dma_thresh_max_buf;
	uint32_t dma_cam_num_entries;
	uint32_t max_open_dmas;

	uint32_t qmi_max_tnums;
	uint32_t qmi_def_tnums_thresh;

	uint32_t bmi_max_tasks;
	uint32_t bmi_max_fifo_size;
	uint32_t bmi_fifo_base;

	uint32_t port_cgs;
	uint32_t rx_ports;
	uint32_t total_fifo_size;

	uint32_t qman_channel_base;
	uint32_t qman_channels;
};

struct fman_port_init_params {
	int port_id;
	bool is_rx_port;
	uint8_t num_tasks;
	uint8_t extra_tasks;
	uint8_t open_dmas;
	uint8_t extra_dmas;
	uint32_t fifo_size;
	uint32_t extra_fifo_size;
	uint8_t deq_pipeline_size;
	uint16_t max_frame_length;
	uint16_t liodn;
};

/**
 * @group FMan bus interface.
 * @{
 */
struct resource *fman_alloc_resource(device_t bus, device_t child, int type,
    int rid, rman_res_t start, rman_res_t end, rman_res_t count, u_int flags);
int fman_activate_resource(device_t bus, device_t child,
    struct resource *res);
int fman_release_resource(device_t bus, device_t child, struct resource *res);
int	fman_attach(device_t dev);
int	fman_detach(device_t dev);
int	fman_suspend(device_t dev);
int	fman_resume_dev(device_t dev);
int	fman_shutdown(device_t dev);
int	fman_read_ivar(device_t dev, device_t child, int index,
	    uintptr_t *result);
int	fman_qman_channel_id(device_t, int);
void	fman_get_revision(device_t, int *, int *);
/** @} */

uint32_t	fman_get_clock(struct fman_softc *sc);
int	fman_get_bushandle(device_t dev, vm_offset_t *fm_base);
size_t	fman_get_bmi_max_fifo_size(device_t);
int	fman_reset_mac(device_t, int);
int	fman_set_port_params(device_t dev, struct fman_port_init_params *params);
int	fman_qman_channel_id(device_t, int);
int	fman_set_mac_intr_handler(device_t, int, driver_intr_t, void *);
int	fman_set_mac_err_handler(device_t, int, driver_intr_t, void *);

#endif /* FMAN_H_ */
