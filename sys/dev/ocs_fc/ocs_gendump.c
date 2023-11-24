/*
 * Copyright (c) 2021 Broadcom. All rights reserved.
 * The term "Broadcom" refers to Broadcom Limited and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "ocs.h"
#include "ocs_gendump.h"

/* Reset all the functions associated with a bus/dev */
static int
ocs_gen_dump_reset(uint8_t bus, uint8_t dev)
{
	uint32_t index = 0;
	ocs_t *ocs;
	int rc = 0;

	while ((ocs = ocs_get_instance(index++)) != NULL) {
		uint8_t ocs_bus, ocs_dev, ocs_func;
		ocs_domain_t *domain;

		ocs_get_bus_dev_func(ocs, &ocs_bus, &ocs_dev, &ocs_func);

		if (!(ocs_bus == bus && ocs_dev == dev))
			continue;

		if (ocs_hw_reset(&ocs->hw, OCS_HW_RESET_FUNCTION)) {
			ocs_log_test(ocs, "failed to reset port\n");
			rc = -1;
			continue;
		}

		ocs_log_debug(ocs, "successfully reset port\n");
		while ((domain = ocs_list_get_head(&ocs->domain_list)) != NULL) {
			ocs_log_debug(ocs, "free domain %p\n", domain);
			ocs_domain_force_free(domain);
		}
		/* now initialize hw so user can read the dump in */
		if (ocs_hw_init(&ocs->hw)) {
			ocs_log_err(ocs, "failed to initialize hw\n");
			rc = -1;
		} else {
			ocs_log_debug(ocs, "successfully initialized hw\n");
		}
	}
	return rc;
}

int
ocs_gen_dump(ocs_t *ocs)
{
	uint32_t reset_required;
	uint32_t dump_ready;
	uint32_t ms_waited;
	uint8_t bus, dev, func;
	int rc = 0;
	int index = 0, port_index = 0;
	ocs_t *nxt_ocs;
	uint8_t nxt_bus, nxt_dev, nxt_func;
	uint8_t prev_port_state[OCS_MAX_HBA_PORTS] = {0,};
	ocs_xport_stats_t link_status;

	ocs_get_bus_dev_func(ocs, &bus, &dev, &func);

	/* Drop link on all ports belongs to this HBA*/
	while ((nxt_ocs = ocs_get_instance(index++)) != NULL) {
		ocs_get_bus_dev_func(nxt_ocs, &nxt_bus, &nxt_dev, &nxt_func);

		if (!(bus == nxt_bus && dev == nxt_dev))
			continue;

		if ((port_index >= OCS_MAX_HBA_PORTS))
			continue;

		/* Check current link status and save for future use */
		if (ocs_xport_status(nxt_ocs->xport, OCS_XPORT_PORT_STATUS,
		   &link_status) == 0) {
			if (link_status.value == OCS_XPORT_PORT_ONLINE) {
				prev_port_state[port_index] = 1;
				ocs_xport_control(nxt_ocs->xport,
						  OCS_XPORT_PORT_OFFLINE);
			} else {
				prev_port_state[port_index] = 0;
			}
		}
		port_index++;
	}

	/* Wait until all ports have quiesced */
	for (index = 0; (nxt_ocs = ocs_get_instance(index++)) != NULL; ) {
		ms_waited = 0;
		for (;;) {
			ocs_xport_stats_t status;

			ocs_xport_status(nxt_ocs->xport, OCS_XPORT_IS_QUIESCED,
					 &status);
			if (status.value) {
				ocs_log_debug(nxt_ocs, "port quiesced\n");
				break;
			}

			ocs_msleep(10);
			ms_waited += 10;
			if (ms_waited > 60000) {
				ocs_log_test(nxt_ocs,
				    "timed out waiting for port to quiesce\n");
				break;
			}
		}
	}

	/* Initiate dump */
	if (ocs_hw_raise_ue(&ocs->hw, 1) == OCS_HW_RTN_SUCCESS) {

		/* Wait for dump to complete */
		ocs_log_debug(ocs, "Dump requested, wait for completion.\n");

		dump_ready = 0;
		ms_waited = 0;
		while ((!dump_ready) && (ms_waited < 30000)) {
			ocs_hw_get(&ocs->hw, OCS_HW_DUMP_READY, &dump_ready);
			ocs_udelay(10000);
			ms_waited += 10;
		}

		if (!dump_ready) {
			ocs_log_test(ocs, "Failed to see dump after 30 secs\n");
			rc = -1;
		} else {
			ocs_log_debug(ocs, "sucessfully generated dump\n");
		}

		/* now reset port */
		ocs_hw_get(&ocs->hw, OCS_HW_RESET_REQUIRED, &reset_required);
		ocs_log_debug(ocs, "reset required=%d\n", reset_required);
		if (reset_required) {
			if (ocs_gen_dump_reset(bus, dev) == 0) {
				ocs_log_debug(ocs, "all devices reset\n");
			} else {
				ocs_log_test(ocs, "all devices NOT reset\n");
			}
		}
	} else {
		ocs_log_test(ocs, "dump request to hw failed\n");
		rc = -1;
	}

	index = port_index = 0;
	nxt_ocs = NULL;
	/* Bring links on each HBA port to previous state*/
	while ((nxt_ocs = ocs_get_instance(index++)) != NULL) {
		ocs_get_bus_dev_func(nxt_ocs, &nxt_bus, &nxt_dev, &nxt_func);
		if (port_index > OCS_MAX_HBA_PORTS) {
			ocs_log_err(NULL, "port index(%d) out of boundary\n",
				    port_index);
			rc = -1;
			break;
		}
		if ((bus == nxt_bus) && (dev == nxt_dev) &&
		    prev_port_state[port_index++]) {
			ocs_xport_control(nxt_ocs->xport, OCS_XPORT_PORT_ONLINE);
		}
	}

	return rc;
}

int
ocs_fdb_dump(ocs_t *ocs)
{
	uint32_t dump_ready;
	uint32_t ms_waited;
	int rc = 0;

#define FDB 2

	/* Initiate dump */
	if (ocs_hw_raise_ue(&ocs->hw, FDB) == OCS_HW_RTN_SUCCESS) {

		/* Wait for dump to complete */
		ocs_log_debug(ocs, "Dump requested, wait for completion.\n");

		dump_ready = 0;
		ms_waited = 0;
		while ((!(dump_ready == FDB)) && (ms_waited < 10000)) {
			ocs_hw_get(&ocs->hw, OCS_HW_DUMP_READY, &dump_ready);
			ocs_udelay(10000);
			ms_waited += 10;
		}

		if (!dump_ready) {
			ocs_log_err(ocs, "Failed to see dump after 10 secs\n");
			return -1;
		}

		ocs_log_debug(ocs, "sucessfully generated dump\n");

	} else {
		ocs_log_err(ocs, "dump request to hw failed\n");
		rc = -1;
	}

	return rc;
}

/**
 * @brief Create a Lancer dump into a memory buffer
 * @par Description
 * This function creates a DMA buffer to hold a Lancer dump,
 * sets the dump location to point to that buffer, then calls
 * ocs_gen_dump to cause a dump to be transferred to the buffer.
 * After the dump is complete it copies the dump to the provided
 * user space buffer.
 *
 * @param ocs Pointer to ocs structure
 * @param buf User space buffer in which to store the dump
 * @param buflen Length of the user buffer in bytes
 *
 * @return Returns 0 on success, non-zero on error.
 */
int
ocs_dump_to_host(ocs_t *ocs, void *buf, uint32_t buflen)
{
	int rc;
	uint32_t i, num_buffers;
	ocs_dma_t *dump_buffers;
	uint32_t rem_bytes, offset;

	if (buflen == 0) {
		ocs_log_test(ocs, "zero buffer length is invalid\n");
		return -1;
	}

	num_buffers = ((buflen + OCS_MAX_DMA_ALLOC - 1) / OCS_MAX_DMA_ALLOC);

	dump_buffers = ocs_malloc(ocs, sizeof(ocs_dma_t) * num_buffers,
				  OCS_M_ZERO | OCS_M_NOWAIT);
	if (dump_buffers == NULL) {
		ocs_log_err(ocs, "Failed to dump buffers\n");
		return -1;
	}

	/* Allocate a DMA buffers to hold the dump */
	rem_bytes = buflen;
	for (i = 0; i < num_buffers; i++) {
		uint32_t num_bytes = MIN(rem_bytes, OCS_MAX_DMA_ALLOC);

		rc = ocs_dma_alloc(ocs, &dump_buffers[i], num_bytes,
				   OCS_MIN_DMA_ALIGNMENT);
		if (rc) {
			ocs_log_err(ocs, "Failed to allocate dump buffer\n");

			/* Free any previously allocated buffers */
			goto free_and_return;
		}
		rem_bytes -= num_bytes;
	}

	rc = ocs_hw_set_dump_location(&ocs->hw, num_buffers, dump_buffers, 0);
	if (rc) {
		ocs_log_test(ocs, "ocs_hw_set_dump_location failed\n");
		goto free_and_return;
	}

	/* Generate the dump */
	rc = ocs_gen_dump(ocs);
	if (rc) {
		ocs_log_test(ocs, "ocs_gen_dump failed\n");
		goto free_and_return;
	}

	/* Copy the dump from the DMA buffer into the user buffer */
	offset = 0;
	for (i = 0; i < num_buffers; i++) {
		if (ocs_copy_to_user((uint8_t*)buf + offset,
		    dump_buffers[i].virt, dump_buffers[i].size)) {
			ocs_log_test(ocs, "ocs_copy_to_user failed\n");
			rc = -1;
		}
		offset += dump_buffers[i].size;
	}

free_and_return:
	/* Free the DMA buffer and return */
	for (i = 0; i < num_buffers; i++) {
		ocs_dma_free(ocs, &dump_buffers[i]);
	}
	ocs_free(ocs, dump_buffers, sizeof(ocs_dma_t) * num_buffers);
	return rc;
}

int
ocs_function_speciic_dump(ocs_t *ocs, void *buf, uint32_t buflen)
{
	int rc;
	uint32_t i, num_buffers;
	ocs_dma_t *dump_buffers;
	uint32_t rem_bytes, offset;

	if (buflen == 0) {
		ocs_log_err(ocs, "zero buffer length is invalid\n");
		return -1;
	}

	num_buffers = ((buflen + OCS_MAX_DMA_ALLOC - 1) / OCS_MAX_DMA_ALLOC);

	dump_buffers = ocs_malloc(ocs, sizeof(ocs_dma_t) * num_buffers,
				  OCS_M_ZERO | OCS_M_NOWAIT);
	if (dump_buffers == NULL) {
		ocs_log_err(ocs, "Failed to allocate dump buffers\n");
		return -1;
	}

	/* Allocate a DMA buffers to hold the dump */
	rem_bytes = buflen;
	for (i = 0; i < num_buffers; i++) {
		uint32_t num_bytes = MIN(rem_bytes, OCS_MAX_DMA_ALLOC);
		rc = ocs_dma_alloc(ocs, &dump_buffers[i], num_bytes,
				   OCS_MIN_DMA_ALIGNMENT);
		if (rc) {
			ocs_log_err(ocs, "Failed to allocate dma buffer\n");

			/* Free any previously allocated buffers */
			goto free_and_return;
		}
		rem_bytes -= num_bytes;
	}

	/* register buffers for function spcific dump */
	rc = ocs_hw_set_dump_location(&ocs->hw, num_buffers, dump_buffers, 1);
	if (rc) {
		ocs_log_err(ocs, "ocs_hw_set_dump_location failed\n");
		goto free_and_return;
	}

	/* Invoke dump by setting fdd=1 and ip=1 in sliport_control register */
	rc = ocs_fdb_dump(ocs);
	if (rc) {
		ocs_log_err(ocs, "ocs_gen_dump failed\n");
		goto free_and_return;
	}

	/* Copy the dump from the DMA buffer into the user buffer */
	offset = 0;
	for (i = 0; i < num_buffers; i++) {
		if (ocs_copy_to_user((uint8_t*)buf + offset,
		    dump_buffers[i].virt, dump_buffers[i].size)) {
			ocs_log_err(ocs, "ocs_copy_to_user failed\n");
			rc = -1;
		}
		offset += dump_buffers[i].size;
	}

free_and_return:
	/* Free the DMA buffer and return */
	for (i = 0; i < num_buffers; i++) {
		ocs_dma_free(ocs, &dump_buffers[i]);
	}
	ocs_free(ocs, dump_buffers, sizeof(ocs_dma_t) * num_buffers);
	return rc;

}
