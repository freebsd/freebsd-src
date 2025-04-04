/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023-2024 Google LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/endian.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>

#include "gve.h"
#include "gve_adminq.h"

#define GVE_ADMINQ_SLEEP_LEN_MS 20
#define GVE_MAX_ADMINQ_EVENT_COUNTER_CHECK 10
#define GVE_ADMINQ_DEVICE_DESCRIPTOR_VERSION 1
#define GVE_REG_ADMINQ_ADDR 16
#define ADMINQ_SLOTS (ADMINQ_SIZE / sizeof(struct gve_adminq_command))

#define GVE_DEVICE_OPTION_ERROR_FMT "%s option error:\n" \
    "Expected: length=%d, feature_mask=%x.\n" \
    "Actual: length=%d, feature_mask=%x.\n"

#define GVE_DEVICE_OPTION_TOO_BIG_FMT "Length of %s option larger than expected." \
    " Possible older version of guest driver.\n"

static
void gve_parse_device_option(struct gve_priv *priv,
    struct gve_device_descriptor *device_descriptor,
    struct gve_device_option *option,
    struct gve_device_option_gqi_qpl **dev_op_gqi_qpl,
    struct gve_device_option_dqo_rda **dev_op_dqo_rda,
    struct gve_device_option_dqo_qpl **dev_op_dqo_qpl,
    struct gve_device_option_modify_ring **dev_op_modify_ring,
    struct gve_device_option_jumbo_frames **dev_op_jumbo_frames)
{
	uint32_t req_feat_mask = be32toh(option->required_features_mask);
	uint16_t option_length = be16toh(option->option_length);
	uint16_t option_id = be16toh(option->option_id);

	/*
	 * If the length or feature mask doesn't match, continue without
	 * enabling the feature.
	 */
	switch (option_id) {
	case GVE_DEV_OPT_ID_GQI_QPL:
		if (option_length < sizeof(**dev_op_gqi_qpl) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_GQI_QPL) {
			device_printf(priv->dev, GVE_DEVICE_OPTION_ERROR_FMT,
			    "GQI QPL", (int)sizeof(**dev_op_gqi_qpl),
			    GVE_DEV_OPT_REQ_FEAT_MASK_GQI_QPL,
			    option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_gqi_qpl)) {
			device_printf(priv->dev, GVE_DEVICE_OPTION_TOO_BIG_FMT,
			    "GQI QPL");
		}
		*dev_op_gqi_qpl = (void *)(option + 1);
		break;

	case GVE_DEV_OPT_ID_DQO_RDA:
		if (option_length < sizeof(**dev_op_dqo_rda) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_DQO_RDA) {
			device_printf(priv->dev, GVE_DEVICE_OPTION_ERROR_FMT,
			    "DQO RDA", (int)sizeof(**dev_op_dqo_rda),
			    GVE_DEV_OPT_REQ_FEAT_MASK_DQO_RDA,
			    option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_dqo_rda)) {
			device_printf(priv->dev, GVE_DEVICE_OPTION_TOO_BIG_FMT,
			    "DQO RDA");
		}
		*dev_op_dqo_rda = (void *)(option + 1);
		break;

	case GVE_DEV_OPT_ID_DQO_QPL:
		if (option_length < sizeof(**dev_op_dqo_qpl) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_DQO_QPL) {
			device_printf(priv->dev, GVE_DEVICE_OPTION_ERROR_FMT,
			    "DQO QPL", (int)sizeof(**dev_op_dqo_qpl),
			    GVE_DEV_OPT_REQ_FEAT_MASK_DQO_QPL,
			    option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_dqo_qpl)) {
			device_printf(priv->dev, GVE_DEVICE_OPTION_TOO_BIG_FMT,
			    "DQO QPL");
		}
		*dev_op_dqo_qpl = (void *)(option + 1);
		break;

	case GVE_DEV_OPT_ID_MODIFY_RING:
		if (option_length < (sizeof(**dev_op_modify_ring) -
		    sizeof(struct gve_ring_size_bound)) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_MODIFY_RING) {
			device_printf(priv->dev, GVE_DEVICE_OPTION_ERROR_FMT,
			    "Modify Ring", (int)sizeof(**dev_op_modify_ring),
			    GVE_DEV_OPT_REQ_FEAT_MASK_MODIFY_RING,
			    option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_modify_ring)) {
			device_printf(priv->dev, GVE_DEVICE_OPTION_TOO_BIG_FMT,
			    "Modify Ring");
		}
		*dev_op_modify_ring = (void *)(option + 1);

		/* Min ring size included; set the minimum ring size. */
		if (option_length == sizeof(**dev_op_modify_ring)) {
			priv->min_rx_desc_cnt = max(
			    be16toh((*dev_op_modify_ring)->min_ring_size.rx),
			    GVE_DEFAULT_MIN_RX_RING_SIZE);
			priv->min_tx_desc_cnt = max(
			    be16toh((*dev_op_modify_ring)->min_ring_size.tx),
			    GVE_DEFAULT_MIN_TX_RING_SIZE);
		}
		break;

	case GVE_DEV_OPT_ID_JUMBO_FRAMES:
		if (option_length < sizeof(**dev_op_jumbo_frames) ||
		    req_feat_mask != GVE_DEV_OPT_REQ_FEAT_MASK_JUMBO_FRAMES) {
			device_printf(priv->dev, GVE_DEVICE_OPTION_ERROR_FMT,
			    "Jumbo Frames", (int)sizeof(**dev_op_jumbo_frames),
			    GVE_DEV_OPT_REQ_FEAT_MASK_JUMBO_FRAMES,
			    option_length, req_feat_mask);
			break;
		}

		if (option_length > sizeof(**dev_op_jumbo_frames)) {
			device_printf(priv->dev,
			    GVE_DEVICE_OPTION_TOO_BIG_FMT, "Jumbo Frames");
		}
		*dev_op_jumbo_frames = (void *)(option + 1);
		break;

	default:
		/*
		 * If we don't recognize the option just continue
		 * without doing anything.
		 */
		device_printf(priv->dev, "Unrecognized device option 0x%hx not enabled.\n",
		    option_id);
	}
}

/* Process all device options for a given describe device call. */
static int
gve_process_device_options(struct gve_priv *priv,
    struct gve_device_descriptor *descriptor,
    struct gve_device_option_gqi_qpl **dev_op_gqi_qpl,
    struct gve_device_option_dqo_rda **dev_op_dqo_rda,
    struct gve_device_option_dqo_qpl **dev_op_dqo_qpl,
    struct gve_device_option_modify_ring **dev_op_modify_ring,
    struct gve_device_option_jumbo_frames **dev_op_jumbo_frames)
{
	char *desc_end = (char *)descriptor + be16toh(descriptor->total_length);
	const int num_options = be16toh(descriptor->num_device_options);
	struct gve_device_option *dev_opt;
	int i;

	/* The options struct directly follows the device descriptor. */
	dev_opt = (void *)(descriptor + 1);
	for (i = 0; i < num_options; i++) {
		if ((char *)(dev_opt + 1) > desc_end ||
		    (char *)(dev_opt + 1) + be16toh(dev_opt->option_length) > desc_end) {
			device_printf(priv->dev,
			    "options exceed device descriptor's total length.\n");
			return (EINVAL);
		}

		gve_parse_device_option(priv, descriptor, dev_opt,
		    dev_op_gqi_qpl,
		    dev_op_dqo_rda,
		    dev_op_dqo_qpl,
		    dev_op_modify_ring,
		    dev_op_jumbo_frames);
		dev_opt = (void *)((char *)(dev_opt + 1) + be16toh(dev_opt->option_length));
	}

	return (0);
}

static int gve_adminq_execute_cmd(struct gve_priv *priv,
    struct gve_adminq_command *cmd);

static int
gve_adminq_destroy_tx_queue(struct gve_priv *priv, uint32_t id)
{
	struct gve_adminq_command cmd = (struct gve_adminq_command){};

	cmd.opcode = htobe32(GVE_ADMINQ_DESTROY_TX_QUEUE);
	cmd.destroy_tx_queue.queue_id = htobe32(id);

	return (gve_adminq_execute_cmd(priv, &cmd));
}

static int
gve_adminq_destroy_rx_queue(struct gve_priv *priv, uint32_t id)
{
	struct gve_adminq_command cmd = (struct gve_adminq_command){};

	cmd.opcode = htobe32(GVE_ADMINQ_DESTROY_RX_QUEUE);
	cmd.destroy_rx_queue.queue_id = htobe32(id);

	return (gve_adminq_execute_cmd(priv, &cmd));
}

int
gve_adminq_destroy_rx_queues(struct gve_priv *priv, uint32_t num_queues)
{
	int err;
	int i;

	for (i = 0; i < num_queues; i++) {
		err = gve_adminq_destroy_rx_queue(priv, i);
		if (err != 0) {
			device_printf(priv->dev, "Failed to destroy rxq %d, err: %d\n",
			    i, err);
		}
	}

	if (err != 0)
		return (err);

	device_printf(priv->dev, "Destroyed %d rx queues\n", num_queues);
	return (0);
}

int
gve_adminq_destroy_tx_queues(struct gve_priv *priv, uint32_t num_queues)
{
	int err;
	int i;

	for (i = 0; i < num_queues; i++) {
		err = gve_adminq_destroy_tx_queue(priv, i);
		if (err != 0) {
			device_printf(priv->dev, "Failed to destroy txq %d, err: %d\n",
			    i, err);
		}
	}

	if (err != 0)
		return (err);

	device_printf(priv->dev, "Destroyed %d tx queues\n", num_queues);
	return (0);
}

static int
gve_adminq_create_rx_queue(struct gve_priv *priv, uint32_t queue_index)
{
	struct gve_adminq_command cmd = (struct gve_adminq_command){};
	struct gve_rx_ring *rx = &priv->rx[queue_index];
	struct gve_dma_handle *qres_dma = &rx->com.q_resources_mem;

	bus_dmamap_sync(qres_dma->tag, qres_dma->map, BUS_DMASYNC_PREREAD);

	cmd.opcode = htobe32(GVE_ADMINQ_CREATE_RX_QUEUE);
	cmd.create_rx_queue = (struct gve_adminq_create_rx_queue) {
		.queue_id = htobe32(queue_index),
		.ntfy_id = htobe32(rx->com.ntfy_id),
		.queue_resources_addr = htobe64(qres_dma->bus_addr),
		.rx_ring_size = htobe16(priv->rx_desc_cnt),
		.packet_buffer_size = htobe16(GVE_DEFAULT_RX_BUFFER_SIZE),
	};

	if (gve_is_gqi(priv)) {
		cmd.create_rx_queue.rx_desc_ring_addr =
		    htobe64(rx->desc_ring_mem.bus_addr);
		cmd.create_rx_queue.rx_data_ring_addr =
		    htobe64(rx->data_ring_mem.bus_addr);
		cmd.create_rx_queue.index =
		    htobe32(queue_index);
		cmd.create_rx_queue.queue_page_list_id =
		    htobe32((rx->com.qpl)->id);
	} else {
		cmd.create_rx_queue.queue_page_list_id =
		    htobe32(GVE_RAW_ADDRESSING_QPL_ID);
		cmd.create_rx_queue.rx_desc_ring_addr =
		    htobe64(rx->dqo.compl_ring_mem.bus_addr);
		cmd.create_rx_queue.rx_data_ring_addr =
		    htobe64(rx->desc_ring_mem.bus_addr);
		cmd.create_rx_queue.rx_buff_ring_size =
		    htobe16(priv->rx_desc_cnt);
		cmd.create_rx_queue.enable_rsc =
		    !!((if_getcapenable(priv->ifp) & IFCAP_LRO) &&
			!gve_disable_hw_lro);
	}

	return (gve_adminq_execute_cmd(priv, &cmd));
}

int
gve_adminq_create_rx_queues(struct gve_priv *priv, uint32_t num_queues)
{
	int err;
	int i;

	for (i = 0; i < num_queues; i++) {
		err = gve_adminq_create_rx_queue(priv, i);
		if (err != 0) {
			device_printf(priv->dev, "Failed to create rxq %d, err: %d\n",
			    i, err);
			goto abort;
		}
	}

	if (bootverbose)
		device_printf(priv->dev, "Created %d rx queues\n", num_queues);
	return (0);

abort:
	gve_adminq_destroy_rx_queues(priv, i);
	return (err);
}

static int
gve_adminq_create_tx_queue(struct gve_priv *priv, uint32_t queue_index)
{
	struct gve_adminq_command cmd = (struct gve_adminq_command){};
	struct gve_tx_ring *tx = &priv->tx[queue_index];
	struct gve_dma_handle *qres_dma = &tx->com.q_resources_mem;

	bus_dmamap_sync(qres_dma->tag, qres_dma->map, BUS_DMASYNC_PREREAD);

	cmd.opcode = htobe32(GVE_ADMINQ_CREATE_TX_QUEUE);
	cmd.create_tx_queue = (struct gve_adminq_create_tx_queue) {
		.queue_id = htobe32(queue_index),
		.queue_resources_addr = htobe64(qres_dma->bus_addr),
		.tx_ring_addr = htobe64(tx->desc_ring_mem.bus_addr),
		.ntfy_id = htobe32(tx->com.ntfy_id),
		.tx_ring_size = htobe16(priv->tx_desc_cnt),
	};

	if (gve_is_gqi(priv)) {
		cmd.create_tx_queue.queue_page_list_id =
		    htobe32((tx->com.qpl)->id);
	} else {
		cmd.create_tx_queue.queue_page_list_id =
		    htobe32(GVE_RAW_ADDRESSING_QPL_ID);
		cmd.create_tx_queue.tx_comp_ring_addr =
		    htobe64(tx->dqo.compl_ring_mem.bus_addr);
		cmd.create_tx_queue.tx_comp_ring_size =
		    htobe16(priv->tx_desc_cnt);
	}
	return (gve_adminq_execute_cmd(priv, &cmd));
}

int
gve_adminq_create_tx_queues(struct gve_priv *priv, uint32_t num_queues)
{
	int err;
	int i;

	for (i = 0; i < num_queues; i++) {
		err = gve_adminq_create_tx_queue(priv, i);
		if (err != 0) {
			device_printf(priv->dev, "Failed to create txq %d, err: %d\n",
			    i, err);
			goto abort;
		}
	}

	if (bootverbose)
		device_printf(priv->dev, "Created %d tx queues\n", num_queues);
	return (0);

abort:
	gve_adminq_destroy_tx_queues(priv, i);
	return (err);
}

int
gve_adminq_set_mtu(struct gve_priv *priv, uint32_t mtu) {
	struct gve_adminq_command cmd = (struct gve_adminq_command){};

	cmd.opcode = htobe32(GVE_ADMINQ_SET_DRIVER_PARAMETER);
	cmd.set_driver_param = (struct gve_adminq_set_driver_parameter) {
		.parameter_type = htobe32(GVE_SET_PARAM_MTU),
		.parameter_value = htobe64(mtu),
	};

	return (gve_adminq_execute_cmd(priv, &cmd));
}

static void
gve_enable_supported_features(struct gve_priv *priv,
    uint32_t supported_features_mask,
    const struct gve_device_option_modify_ring *dev_op_modify_ring,
    const struct gve_device_option_jumbo_frames *dev_op_jumbo_frames)
{
	if (dev_op_modify_ring &&
	    (supported_features_mask & GVE_SUP_MODIFY_RING_MASK)) {
		if (bootverbose)
			device_printf(priv->dev, "MODIFY RING device option enabled.\n");
		priv->modify_ringsize_enabled = true;
		priv->max_rx_desc_cnt = be16toh(dev_op_modify_ring->max_ring_size.rx);
		priv->max_tx_desc_cnt = be16toh(dev_op_modify_ring->max_ring_size.tx);
	}

	if (dev_op_jumbo_frames &&
	    (supported_features_mask & GVE_SUP_JUMBO_FRAMES_MASK)) {
		if (bootverbose)
			device_printf(priv->dev, "JUMBO FRAMES device option enabled: %u.\n",
			    be16toh(dev_op_jumbo_frames->max_mtu));
		priv->max_mtu = be16toh(dev_op_jumbo_frames->max_mtu);
	}
}

int
gve_adminq_describe_device(struct gve_priv *priv)
{
	struct gve_adminq_command aq_cmd = (struct gve_adminq_command){};
	struct gve_device_descriptor *desc;
	struct gve_dma_handle desc_mem;
	struct gve_device_option_gqi_qpl *dev_op_gqi_qpl = NULL;
	struct gve_device_option_dqo_rda *dev_op_dqo_rda = NULL;
	struct gve_device_option_dqo_qpl *dev_op_dqo_qpl = NULL;
	struct gve_device_option_modify_ring *dev_op_modify_ring = NULL;
	struct gve_device_option_jumbo_frames *dev_op_jumbo_frames = NULL;
	uint32_t supported_features_mask = 0;
	int rc;
	int i;

	rc = gve_dma_alloc_coherent(priv, ADMINQ_SIZE, ADMINQ_SIZE, &desc_mem);
	if (rc != 0) {
		device_printf(priv->dev, "Failed to alloc DMA mem for DescribeDevice.\n");
		return (rc);
	}

	desc = desc_mem.cpu_addr;

	aq_cmd.opcode = htobe32(GVE_ADMINQ_DESCRIBE_DEVICE);
	aq_cmd.describe_device.device_descriptor_addr = htobe64(
	    desc_mem.bus_addr);
	aq_cmd.describe_device.device_descriptor_version = htobe32(
	    GVE_ADMINQ_DEVICE_DESCRIPTOR_VERSION);
	aq_cmd.describe_device.available_length = htobe32(ADMINQ_SIZE);

	bus_dmamap_sync(desc_mem.tag, desc_mem.map, BUS_DMASYNC_PREWRITE);

	rc = gve_adminq_execute_cmd(priv, &aq_cmd);
	if (rc != 0)
		goto free_device_descriptor;

	bus_dmamap_sync(desc_mem.tag, desc_mem.map, BUS_DMASYNC_POSTREAD);

	/* Default min in case device options don't have min values */
	priv->min_rx_desc_cnt = GVE_DEFAULT_MIN_RX_RING_SIZE;
	priv->min_tx_desc_cnt = GVE_DEFAULT_MIN_TX_RING_SIZE;

	rc = gve_process_device_options(priv, desc,
	    &dev_op_gqi_qpl,
	    &dev_op_dqo_rda,
	    &dev_op_dqo_qpl,
	    &dev_op_modify_ring,
	    &dev_op_jumbo_frames);
	if (rc != 0)
		goto free_device_descriptor;

	if (dev_op_dqo_rda != NULL) {
		snprintf(gve_queue_format, sizeof(gve_queue_format),
		    "%s", "DQO RDA");
		priv->queue_format = GVE_DQO_RDA_FORMAT;
		supported_features_mask = be32toh(
		    dev_op_dqo_rda->supported_features_mask);
		if (bootverbose)
			device_printf(priv->dev,
			    "Driver is running with DQO RDA queue format.\n");
	} else if (dev_op_dqo_qpl != NULL) {
		snprintf(gve_queue_format, sizeof(gve_queue_format),
		    "%s", "DQO QPL");
		priv->queue_format = GVE_DQO_QPL_FORMAT;
		supported_features_mask = be32toh(
		    dev_op_dqo_qpl->supported_features_mask);
		if (bootverbose)
			device_printf(priv->dev,
			    "Driver is running with DQO QPL queue format.\n");
	} else if (dev_op_gqi_qpl != NULL) {
		snprintf(gve_queue_format, sizeof(gve_queue_format),
		    "%s", "GQI QPL");
		priv->queue_format = GVE_GQI_QPL_FORMAT;
		supported_features_mask = be32toh(
		    dev_op_gqi_qpl->supported_features_mask);
		if (bootverbose)
			device_printf(priv->dev,
			    "Driver is running with GQI QPL queue format.\n");
	} else {
		device_printf(priv->dev, "No compatible queue formats\n");
		rc = EINVAL;
		goto free_device_descriptor;
	}

        priv->num_event_counters = be16toh(desc->counters);
	priv->default_num_queues = be16toh(desc->default_num_queues);
	priv->tx_desc_cnt = be16toh(desc->tx_queue_entries);
	priv->rx_desc_cnt = be16toh(desc->rx_queue_entries);
	priv->rx_pages_per_qpl = be16toh(desc->rx_pages_per_qpl);
	priv->max_registered_pages = be64toh(desc->max_registered_pages);
	priv->max_mtu = be16toh(desc->mtu);
	priv->default_num_queues = be16toh(desc->default_num_queues);
	priv->supported_features =  supported_features_mask;

	/* Default max to current in case modify ring size option is disabled */
	priv->max_rx_desc_cnt = priv->rx_desc_cnt;
	priv->max_tx_desc_cnt = priv->tx_desc_cnt;

	gve_enable_supported_features(priv, supported_features_mask,
	    dev_op_modify_ring, dev_op_jumbo_frames);

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		priv->mac[i] = desc->mac[i];

free_device_descriptor:
	gve_dma_free_coherent(&desc_mem);

	return (rc);
}

int
gve_adminq_register_page_list(struct gve_priv *priv,
    struct gve_queue_page_list *qpl)
{
	struct gve_adminq_command cmd = (struct gve_adminq_command){};
	uint32_t num_entries = qpl->num_pages;
	uint32_t size = num_entries * sizeof(qpl->dmas[0].bus_addr);
	__be64 *page_list;
	struct gve_dma_handle dma;
	int err;
	int i;

	err = gve_dma_alloc_coherent(priv, size, PAGE_SIZE, &dma);
	if (err != 0)
		return (ENOMEM);

	page_list = dma.cpu_addr;

	for (i = 0; i < num_entries; i++)
		page_list[i] = htobe64(qpl->dmas[i].bus_addr);

	bus_dmamap_sync(dma.tag, dma.map, BUS_DMASYNC_PREWRITE);

	cmd.opcode = htobe32(GVE_ADMINQ_REGISTER_PAGE_LIST);
	cmd.reg_page_list = (struct gve_adminq_register_page_list) {
		.page_list_id = htobe32(qpl->id),
		.num_pages = htobe32(num_entries),
		.page_address_list_addr = htobe64(dma.bus_addr),
		.page_size = htobe64(PAGE_SIZE),
	};

	err = gve_adminq_execute_cmd(priv, &cmd);
	gve_dma_free_coherent(&dma);
	return (err);
}

int
gve_adminq_unregister_page_list(struct gve_priv *priv, uint32_t page_list_id)
{
	struct gve_adminq_command cmd = (struct gve_adminq_command){};

	cmd.opcode = htobe32(GVE_ADMINQ_UNREGISTER_PAGE_LIST);
	cmd.unreg_page_list = (struct gve_adminq_unregister_page_list) {
		.page_list_id = htobe32(page_list_id),
	};

	return (gve_adminq_execute_cmd(priv, &cmd));
}

#define GVE_NTFY_BLK_BASE_MSIX_IDX	0
int
gve_adminq_configure_device_resources(struct gve_priv *priv)
{
	struct gve_adminq_command aq_cmd = (struct gve_adminq_command){};

	bus_dmamap_sync(priv->irqs_db_mem.tag, priv->irqs_db_mem.map,
	    BUS_DMASYNC_PREREAD);
	bus_dmamap_sync(priv->counter_array_mem.tag,
	    priv->counter_array_mem.map, BUS_DMASYNC_PREREAD);

	aq_cmd.opcode = htobe32(GVE_ADMINQ_CONFIGURE_DEVICE_RESOURCES);
	aq_cmd.configure_device_resources =
	    (struct gve_adminq_configure_device_resources) {
		.counter_array = htobe64(priv->counter_array_mem.bus_addr),
		.irq_db_addr = htobe64(priv->irqs_db_mem.bus_addr),
		.num_counters = htobe32(priv->num_event_counters),
		.num_irq_dbs = htobe32(priv->num_queues),
		.irq_db_stride = htobe32(sizeof(struct gve_irq_db)),
		.ntfy_blk_msix_base_idx = htobe32(GVE_NTFY_BLK_BASE_MSIX_IDX),
		.queue_format = priv->queue_format,
	};

	return (gve_adminq_execute_cmd(priv, &aq_cmd));
}

int
gve_adminq_deconfigure_device_resources(struct gve_priv *priv)
{
	struct gve_adminq_command aq_cmd = (struct gve_adminq_command){};

	aq_cmd.opcode = htobe32(GVE_ADMINQ_DECONFIGURE_DEVICE_RESOURCES);
	return (gve_adminq_execute_cmd(priv, &aq_cmd));
}

int
gve_adminq_verify_driver_compatibility(struct gve_priv *priv,
    uint64_t driver_info_len,
    vm_paddr_t driver_info_addr)
{
	struct gve_adminq_command aq_cmd = (struct gve_adminq_command){};

	aq_cmd.opcode = htobe32(GVE_ADMINQ_VERIFY_DRIVER_COMPATIBILITY);
	aq_cmd.verify_driver_compatibility = (struct gve_adminq_verify_driver_compatibility) {
		.driver_info_len = htobe64(driver_info_len),
		.driver_info_addr = htobe64(driver_info_addr),
	};

	return (gve_adminq_execute_cmd(priv, &aq_cmd));
}

int
gve_adminq_get_ptype_map_dqo(struct gve_priv *priv,
    struct gve_ptype_lut *ptype_lut_dqo)
{
	struct gve_adminq_command aq_cmd = (struct gve_adminq_command){};
	struct gve_ptype_map *ptype_map;
	struct gve_dma_handle dma;
	int err = 0;
	int i;

	err = gve_dma_alloc_coherent(priv, sizeof(*ptype_map), PAGE_SIZE, &dma);
	if (err)
		return (err);
	ptype_map = dma.cpu_addr;

	aq_cmd.opcode = htobe32(GVE_ADMINQ_GET_PTYPE_MAP);
	aq_cmd.get_ptype_map = (struct gve_adminq_get_ptype_map) {
		.ptype_map_len = htobe64(sizeof(*ptype_map)),
		.ptype_map_addr = htobe64(dma.bus_addr),
	};

	err = gve_adminq_execute_cmd(priv, &aq_cmd);
	if (err)
		goto err;

	/* Populate ptype_lut_dqo. */
	for (i = 0; i < GVE_NUM_PTYPES; i++) {
		ptype_lut_dqo->ptypes[i].l3_type = ptype_map->ptypes[i].l3_type;
		ptype_lut_dqo->ptypes[i].l4_type = ptype_map->ptypes[i].l4_type;
	}
err:
	gve_dma_free_coherent(&dma);
	return (err);
}

int
gve_adminq_alloc(struct gve_priv *priv)
{
	int rc;

	if (gve_get_state_flag(priv, GVE_STATE_FLAG_ADMINQ_OK))
		return (0);

	if (priv->aq_mem.cpu_addr == NULL) {
		rc = gve_dma_alloc_coherent(priv, ADMINQ_SIZE, ADMINQ_SIZE,
		    &priv->aq_mem);
		if (rc != 0) {
			device_printf(priv->dev, "Failed to allocate admin queue mem\n");
			return (rc);
		}
	}

	priv->adminq = priv->aq_mem.cpu_addr;
	priv->adminq_bus_addr = priv->aq_mem.bus_addr;

	if (priv->adminq == NULL)
		return (ENOMEM);

	priv->adminq_mask = ADMINQ_SLOTS - 1;
	priv->adminq_prod_cnt = 0;
	priv->adminq_cmd_fail = 0;
	priv->adminq_timeouts = 0;
	priv->adminq_describe_device_cnt = 0;
	priv->adminq_cfg_device_resources_cnt = 0;
	priv->adminq_register_page_list_cnt = 0;
	priv->adminq_unregister_page_list_cnt = 0;
	priv->adminq_create_tx_queue_cnt = 0;
	priv->adminq_create_rx_queue_cnt = 0;
	priv->adminq_destroy_tx_queue_cnt = 0;
	priv->adminq_destroy_rx_queue_cnt = 0;
	priv->adminq_dcfg_device_resources_cnt = 0;
	priv->adminq_set_driver_parameter_cnt = 0;
	priv->adminq_get_ptype_map_cnt = 0;

	gve_reg_bar_write_4(priv, GVE_REG_ADMINQ_ADDR,
	    priv->adminq_bus_addr / ADMINQ_SIZE);

	gve_set_state_flag(priv, GVE_STATE_FLAG_ADMINQ_OK);
	return (0);
}

void
gve_release_adminq(struct gve_priv *priv)
{
	if (!gve_get_state_flag(priv, GVE_STATE_FLAG_ADMINQ_OK))
		return;

	gve_reg_bar_write_4(priv, GVE_REG_ADMINQ_ADDR, 0);
	while (gve_reg_bar_read_4(priv, GVE_REG_ADMINQ_ADDR)) {
		device_printf(priv->dev, "Waiting until admin queue is released.\n");
		pause("gve release adminq", GVE_ADMINQ_SLEEP_LEN_MS);
	}

	gve_dma_free_coherent(&priv->aq_mem);
	priv->aq_mem = (struct gve_dma_handle){};
	priv->adminq = 0;
	priv->adminq_bus_addr = 0;

	gve_clear_state_flag(priv, GVE_STATE_FLAG_ADMINQ_OK);

	if (bootverbose)
		device_printf(priv->dev, "Admin queue released\n");
}

static int
gve_adminq_parse_err(struct gve_priv *priv, uint32_t opcode, uint32_t status)
{
	if (status != GVE_ADMINQ_COMMAND_PASSED &&
	    status != GVE_ADMINQ_COMMAND_UNSET) {
		device_printf(priv->dev, "AQ command(%u): failed with status %d\n", opcode, status);
		priv->adminq_cmd_fail++;
	}
	switch (status) {
	case GVE_ADMINQ_COMMAND_PASSED:
		return (0);

	case GVE_ADMINQ_COMMAND_UNSET:
		device_printf(priv->dev,
		    "AQ command(%u): err and status both unset, this should not be possible.\n",
		    opcode);
		return (EINVAL);

	case GVE_ADMINQ_COMMAND_ERROR_ABORTED:
	case GVE_ADMINQ_COMMAND_ERROR_CANCELLED:
	case GVE_ADMINQ_COMMAND_ERROR_DATALOSS:
	case GVE_ADMINQ_COMMAND_ERROR_FAILED_PRECONDITION:
	case GVE_ADMINQ_COMMAND_ERROR_UNAVAILABLE:
		return (EAGAIN);

	case GVE_ADMINQ_COMMAND_ERROR_ALREADY_EXISTS:
	case GVE_ADMINQ_COMMAND_ERROR_INTERNAL_ERROR:
	case GVE_ADMINQ_COMMAND_ERROR_INVALID_ARGUMENT:
	case GVE_ADMINQ_COMMAND_ERROR_NOT_FOUND:
	case GVE_ADMINQ_COMMAND_ERROR_OUT_OF_RANGE:
	case GVE_ADMINQ_COMMAND_ERROR_UNKNOWN_ERROR:
		return (EINVAL);

	case GVE_ADMINQ_COMMAND_ERROR_DEADLINE_EXCEEDED:
		return (ETIMEDOUT);

	case GVE_ADMINQ_COMMAND_ERROR_PERMISSION_DENIED:
	case GVE_ADMINQ_COMMAND_ERROR_UNAUTHENTICATED:
		return (EACCES);

	case GVE_ADMINQ_COMMAND_ERROR_RESOURCE_EXHAUSTED:
		return (ENOMEM);

	case GVE_ADMINQ_COMMAND_ERROR_UNIMPLEMENTED:
		return (EOPNOTSUPP);

	default:
		device_printf(priv->dev, "AQ command(%u): unknown status code %d\n",
		    opcode, status);
		return (EINVAL);
	}
}

static void
gve_adminq_kick_cmd(struct gve_priv *priv, uint32_t prod_cnt)
{
	gve_reg_bar_write_4(priv, ADMINQ_DOORBELL, prod_cnt);

}

static bool
gve_adminq_wait_for_cmd(struct gve_priv *priv, uint32_t prod_cnt)
{
	int i;

	for (i = 0; i < GVE_MAX_ADMINQ_EVENT_COUNTER_CHECK; i++) {
		if (gve_reg_bar_read_4(priv, ADMINQ_EVENT_COUNTER) == prod_cnt)
			return (true);
		pause("gve adminq cmd", GVE_ADMINQ_SLEEP_LEN_MS);
	}

	return (false);
}

/*
 * Flushes all AQ commands currently queued and waits for them to complete.
 * If there are failures, it will return the first error.
 */
static int
gve_adminq_kick_and_wait(struct gve_priv *priv)
{
	struct gve_adminq_command *cmd;
	uint32_t status, err;
	uint32_t tail, head;
	uint32_t opcode;
	int i;

	tail = gve_reg_bar_read_4(priv, ADMINQ_EVENT_COUNTER);
	head = priv->adminq_prod_cnt;

	gve_adminq_kick_cmd(priv, head);
	if (!gve_adminq_wait_for_cmd(priv, head)) {
		device_printf(priv->dev, "AQ commands timed out, need to reset AQ\n");
		priv->adminq_timeouts++;
		return (ENOTRECOVERABLE);
	}
	bus_dmamap_sync(
	    priv->aq_mem.tag, priv->aq_mem.map, BUS_DMASYNC_POSTREAD);

	for (i = tail; i < head; i++) {
		cmd = &priv->adminq[i & priv->adminq_mask];
		status = be32toh(cmd->status);
		opcode = be32toh(cmd->opcode);
		err = gve_adminq_parse_err(priv, opcode, status);
		if (err != 0)
			return (err);
	}

	return (0);
}

/*
 * This function is not threadsafe - the caller is responsible for any
 * necessary locks.
 */
static int
gve_adminq_issue_cmd(struct gve_priv *priv, struct gve_adminq_command *cmd_orig)
{
	struct gve_adminq_command *cmd;
	uint32_t opcode;
	uint32_t tail;
	int err;

	tail = gve_reg_bar_read_4(priv, ADMINQ_EVENT_COUNTER);

	/* Check if next command will overflow the buffer. */
	if ((priv->adminq_prod_cnt - tail) > priv->adminq_mask) {
		/* Flush existing commands to make room. */
		err = gve_adminq_kick_and_wait(priv);
		if (err != 0)
			return (err);

		/* Retry. */
		tail = gve_reg_bar_read_4(priv, ADMINQ_EVENT_COUNTER);
		if ((priv->adminq_prod_cnt - tail) > priv->adminq_mask) {
			/*
			 * This should never happen. We just flushed the
			 * command queue so there should be enough space.
                         */
			return (ENOMEM);
		}
	}

	cmd = &priv->adminq[priv->adminq_prod_cnt & priv->adminq_mask];
	priv->adminq_prod_cnt++;

	memcpy(cmd, cmd_orig, sizeof(*cmd_orig));

	bus_dmamap_sync(
	    priv->aq_mem.tag, priv->aq_mem.map, BUS_DMASYNC_PREWRITE);

	opcode = be32toh(cmd->opcode);

	switch (opcode) {
	case GVE_ADMINQ_DESCRIBE_DEVICE:
		priv->adminq_describe_device_cnt++;
		break;

	case GVE_ADMINQ_CONFIGURE_DEVICE_RESOURCES:
		priv->adminq_cfg_device_resources_cnt++;
		break;

	case GVE_ADMINQ_REGISTER_PAGE_LIST:
		priv->adminq_register_page_list_cnt++;
		break;

	case GVE_ADMINQ_UNREGISTER_PAGE_LIST:
		priv->adminq_unregister_page_list_cnt++;
		break;

	case GVE_ADMINQ_CREATE_TX_QUEUE:
		priv->adminq_create_tx_queue_cnt++;
		break;

	case GVE_ADMINQ_CREATE_RX_QUEUE:
		priv->adminq_create_rx_queue_cnt++;
		break;

	case GVE_ADMINQ_DESTROY_TX_QUEUE:
		priv->adminq_destroy_tx_queue_cnt++;
		break;

	case GVE_ADMINQ_DESTROY_RX_QUEUE:
		priv->adminq_destroy_rx_queue_cnt++;
		break;

	case GVE_ADMINQ_DECONFIGURE_DEVICE_RESOURCES:
		priv->adminq_dcfg_device_resources_cnt++;
		break;

	case GVE_ADMINQ_SET_DRIVER_PARAMETER:
		priv->adminq_set_driver_parameter_cnt++;
		break;

	case GVE_ADMINQ_VERIFY_DRIVER_COMPATIBILITY:
		priv->adminq_verify_driver_compatibility_cnt++;
		break;

	case GVE_ADMINQ_GET_PTYPE_MAP:
		priv->adminq_get_ptype_map_cnt++;
		break;

	default:
		device_printf(priv->dev, "Unknown AQ command opcode %d\n", opcode);
	}

	return (0);
}

/*
 * This function is not threadsafe - the caller is responsible for any
 * necessary locks.
 * The caller is also responsible for making sure there are no commands
 * waiting to be executed.
 */
static int
gve_adminq_execute_cmd(struct gve_priv *priv, struct gve_adminq_command *cmd_orig)
{
	uint32_t tail, head;
	int err;

	tail = gve_reg_bar_read_4(priv, ADMINQ_EVENT_COUNTER);
	head = priv->adminq_prod_cnt;

	if (tail != head)
		return (EINVAL);
	err = gve_adminq_issue_cmd(priv, cmd_orig);
	if (err != 0)
		return (err);
	return (gve_adminq_kick_and_wait(priv));
}
