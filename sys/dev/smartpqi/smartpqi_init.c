/*-
 * Copyright 2016-2023 Microchip Technology, Inc. and/or its subsidiaries.
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


#include "smartpqi_includes.h"

/*
 * Request the adapter to get PQI capabilities supported.
 */
static int
pqisrc_report_pqi_capability(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;

	DBG_FUNC("IN\n");

	gen_adm_req_iu_t	admin_req;
	gen_adm_resp_iu_t 	admin_resp;
	dma_mem_t		pqi_cap_dma_buf;
	pqi_dev_cap_t 		*capability = NULL;
	pqi_iu_layer_desc_t	*iu_layer_desc = NULL;

	/* Allocate Non DMA memory */
	capability = os_mem_alloc(softs, sizeof(*capability));
	if (!capability) {
		DBG_ERR("Failed to allocate memory for capability\n");
		goto err_out;
	}

	memset(&admin_req, 0, sizeof(admin_req));
	memset(&admin_resp, 0, sizeof(admin_resp));

	memset(&pqi_cap_dma_buf, 0, sizeof(struct dma_mem));
	os_strlcpy(pqi_cap_dma_buf.tag, "pqi_cap_buf", sizeof(pqi_cap_dma_buf.tag));
	pqi_cap_dma_buf.size = REPORT_PQI_DEV_CAP_DATA_BUF_SIZE;
	pqi_cap_dma_buf.align = PQISRC_DEFAULT_DMA_ALIGN;

	ret = os_dma_mem_alloc(softs, &pqi_cap_dma_buf);
	if (ret) {
		DBG_ERR("Failed to allocate capability DMA buffer : %d\n", ret);
		goto err_dma_alloc;
	}

	admin_req.fn_code = PQI_FUNCTION_REPORT_DEV_CAP;
	admin_req.req_type.general_func.buf_size = pqi_cap_dma_buf.size;
	admin_req.req_type.general_func.sg_desc.length = pqi_cap_dma_buf.size;
	admin_req.req_type.general_func.sg_desc.addr = pqi_cap_dma_buf.dma_addr;
	admin_req.req_type.general_func.sg_desc.type =	SGL_DESCRIPTOR_CODE_DATA_BLOCK;

	ret = pqisrc_submit_admin_req(softs, &admin_req, &admin_resp);
	if( PQI_STATUS_SUCCESS == ret) {
                memcpy(capability,
			pqi_cap_dma_buf.virt_addr,
			pqi_cap_dma_buf.size);
	} else {
		DBG_ERR("Failed to send admin req report pqi device capability\n");
		goto err_admin_req;

	}

	softs->pqi_dev_cap.max_iqs = capability->max_iqs;
	softs->pqi_dev_cap.max_iq_elements = capability->max_iq_elements;
	softs->pqi_dev_cap.max_iq_elem_len = capability->max_iq_elem_len;
	softs->pqi_dev_cap.min_iq_elem_len = capability->min_iq_elem_len;
	softs->pqi_dev_cap.max_oqs = capability->max_oqs;
	softs->pqi_dev_cap.max_oq_elements = capability->max_oq_elements;
	softs->pqi_dev_cap.max_oq_elem_len = capability->max_oq_elem_len;
	softs->pqi_dev_cap.intr_coales_time_granularity = capability->intr_coales_time_granularity;

	iu_layer_desc = &capability->iu_layer_desc[PQI_PROTOCOL_SOP];
	softs->max_ib_iu_length_per_fw = iu_layer_desc->max_ib_iu_len;
	softs->ib_spanning_supported = iu_layer_desc->ib_spanning_supported;
	softs->ob_spanning_supported = iu_layer_desc->ob_spanning_supported;

	DBG_INIT("softs->pqi_dev_cap.max_iqs: %d\n", softs->pqi_dev_cap.max_iqs);
	DBG_INIT("softs->pqi_dev_cap.max_iq_elements: %d\n", softs->pqi_dev_cap.max_iq_elements);
	DBG_INIT("softs->pqi_dev_cap.max_iq_elem_len: %d\n", softs->pqi_dev_cap.max_iq_elem_len);
	DBG_INIT("softs->pqi_dev_cap.min_iq_elem_len: %d\n", softs->pqi_dev_cap.min_iq_elem_len);
	DBG_INIT("softs->pqi_dev_cap.max_oqs: %d\n", softs->pqi_dev_cap.max_oqs);
	DBG_INIT("softs->pqi_dev_cap.max_oq_elements: %d\n", softs->pqi_dev_cap.max_oq_elements);
	DBG_INIT("softs->pqi_dev_cap.max_oq_elem_len: %d\n", softs->pqi_dev_cap.max_oq_elem_len);
	DBG_INIT("softs->pqi_dev_cap.intr_coales_time_granularity: %d\n", softs->pqi_dev_cap.intr_coales_time_granularity);
	DBG_INIT("softs->max_ib_iu_length_per_fw: %d\n", softs->max_ib_iu_length_per_fw);
	DBG_INIT("softs->ib_spanning_supported: %d\n", softs->ib_spanning_supported);
	DBG_INIT("softs->ob_spanning_supported: %d\n", softs->ob_spanning_supported);

	/* Not expecting these to change, could cause problems if they do */
	ASSERT(softs->pqi_dev_cap.max_iq_elem_len == PQISRC_OP_MAX_ELEM_SIZE);
	ASSERT(softs->pqi_dev_cap.min_iq_elem_len == PQISRC_OP_MIN_ELEM_SIZE);
	ASSERT(softs->max_ib_iu_length_per_fw == PQISRC_MAX_SPANNING_IU_LENGTH);
	ASSERT(softs->ib_spanning_supported == true);


	os_mem_free(softs, (void *)capability,
		    REPORT_PQI_DEV_CAP_DATA_BUF_SIZE);
	os_dma_mem_free(softs, &pqi_cap_dma_buf);

	DBG_FUNC("OUT\n");
	return ret;

err_admin_req:
	os_dma_mem_free(softs, &pqi_cap_dma_buf);
err_dma_alloc:
	if (capability)
		os_mem_free(softs, (void *)capability,
			    REPORT_PQI_DEV_CAP_DATA_BUF_SIZE);
err_out:
	DBG_FUNC("failed OUT\n");
	return PQI_STATUS_FAILURE;
}

/*
 * Function used to deallocate the used rcb.
 */
void
pqisrc_free_rcb(pqisrc_softstate_t *softs, int req_count)
{

	uint32_t num_req;
	size_t size;
	int i;

	DBG_FUNC("IN\n");
	num_req = softs->max_outstanding_io + 1;
	size = num_req * sizeof(rcb_t);
	for (i = 1; i < req_count; i++)
		os_dma_mem_free(softs, &softs->sg_dma_desc[i]);
	os_mem_free(softs, (void *)softs->rcb, size);
	softs->rcb = NULL;
	DBG_FUNC("OUT\n");
}


/*
 * Allocate memory for rcb and SG descriptors.
 * TODO : Sg list should be created separately
 */
static int
pqisrc_allocate_rcb(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;
	int i = 0;
	uint32_t num_req = 0;
	uint32_t sg_buf_size = 0;
	uint64_t alloc_size = 0;
	rcb_t *rcb = NULL;
	rcb_t *prcb = NULL;
	DBG_FUNC("IN\n");

	/* Set maximum outstanding requests */
	/* The valid tag values are from 1, 2, ..., softs->max_outstanding_io
	 * The rcb will be accessed by using the tag as index
     * As 0 tag index is not used, we need to allocate one extra.
	 */
	softs->max_outstanding_io = softs->pqi_cap.max_outstanding_io;
	num_req = softs->max_outstanding_io + 1;
	DBG_INIT("Max Outstanding IO reset to %u\n", num_req);

	alloc_size = num_req * sizeof(rcb_t);

	/* Allocate Non DMA memory */
	rcb = os_mem_alloc(softs, alloc_size);
	if (!rcb) {
		DBG_ERR("Failed to allocate memory for rcb\n");
		ret = PQI_STATUS_FAILURE;
		goto err_out;
	}
	softs->rcb = rcb;

	/* Allocate sg dma memory for sg chain  */
	sg_buf_size = softs->pqi_cap.max_sg_elem *
			sizeof(sgt_t);

	prcb = &softs->rcb[1];
	/* Initialize rcb */
	for(i=1; i < num_req; i++) {
		/* TODO:Here tag is local variable */
		char tag[15];
		sprintf(tag, "sg_dma_buf%d", i);
		os_strlcpy(softs->sg_dma_desc[i].tag, tag, sizeof(softs->sg_dma_desc[i].tag));
		softs->sg_dma_desc[i].size = sg_buf_size;
		softs->sg_dma_desc[i].align = PQISRC_DEFAULT_DMA_ALIGN;

		ret = os_dma_mem_alloc(softs, &softs->sg_dma_desc[i]);
		if (ret) {
			DBG_ERR("Failed to Allocate sg desc %d\n", ret);
			ret = PQI_STATUS_FAILURE;
			goto error;
		}
		prcb->sg_chain_virt = (sgt_t *)(softs->sg_dma_desc[i].virt_addr);
		prcb->sg_chain_dma = (dma_addr_t)(softs->sg_dma_desc[i].dma_addr);
		prcb ++;
	}

	DBG_FUNC("OUT\n");
	return ret;
error:
	pqisrc_free_rcb(softs, i);
err_out:
	DBG_FUNC("failed OUT\n");
	return ret;
}

/*
 * Function used to decide the operational queue configuration params
 * - no of ibq/obq, shared/non-shared interrupt resource, IU spanning support
 */
void
pqisrc_decide_opq_config(pqisrc_softstate_t *softs)
{
	uint16_t total_iq_elements;

	DBG_FUNC("IN\n");

	DBG_INIT("softs->intr_count : %d  softs->num_cpus_online : %d",
		softs->intr_count, softs->num_cpus_online);

	/* TODO : Get the number of IB and OB queues from OS layer */

	if (softs->intr_count == 1 || softs->num_cpus_online == 1) {
		/* Share the event and Operational queue. */
		softs->num_op_obq = 1;
		softs->share_opq_and_eventq = true;
	}
	else {
		/* Note :  One OBQ (OBQ0) reserved for event queue */
		softs->num_op_obq = MIN(softs->num_cpus_online,
					softs->intr_count) - 1;
		softs->share_opq_and_eventq = false;
	}
	/* If the available interrupt count is more than one,
	we don’t need to share the interrupt for IO and event queue */
	if (softs->intr_count > 1)
		softs->share_opq_and_eventq = false;

	DBG_INIT("softs->num_op_obq : %u\n",softs->num_op_obq);

	/* TODO : Reset the interrupt count based on number of queues*/

	softs->num_op_raid_ibq = softs->num_op_obq;
	softs->num_op_aio_ibq = softs->num_op_raid_ibq;
	softs->max_ibq_elem_size =  softs->pqi_dev_cap.max_iq_elem_len * 16;
	softs->max_obq_elem_size = softs->pqi_dev_cap.max_oq_elem_len * 16;
	if (softs->max_ib_iu_length_per_fw == 256 &&
	    softs->ob_spanning_supported) {
		/* older f/w that doesn't actually support spanning. */
		softs->max_ib_iu_length = softs->max_ibq_elem_size;
	} else {
		/* max. inbound IU length is an multiple of our inbound element size. */
		softs->max_ib_iu_length = PQISRC_ROUND_DOWN(softs->max_ib_iu_length_per_fw,
			softs->max_ibq_elem_size);
	}

	/* If Max. Outstanding IO came with Max. Spanning element count then,
		needed elements per IO are multiplication of
		Max.Outstanding IO and  Max.Spanning element */
	total_iq_elements = (softs->max_outstanding_io *
		(softs->max_ib_iu_length / softs->max_ibq_elem_size));

	softs->num_elem_per_op_ibq = total_iq_elements / softs->num_op_raid_ibq;
	softs->num_elem_per_op_ibq = MIN(softs->num_elem_per_op_ibq,
		softs->pqi_dev_cap.max_iq_elements);

	softs->num_elem_per_op_obq = softs->max_outstanding_io / softs->num_op_obq;
	softs->num_elem_per_op_obq = MIN(softs->num_elem_per_op_obq,
		softs->pqi_dev_cap.max_oq_elements);

	/* spanning elements should be 9 (1152/128) */
	softs->max_spanning_elems = softs->max_ib_iu_length/softs->max_ibq_elem_size;
	ASSERT(softs->max_spanning_elems == PQISRC_MAX_SPANNING_ELEMS);

	/* max SGs should be 8 (128/16) */
	softs->max_sg_per_single_iu_element = softs->max_ibq_elem_size / sizeof(sgt_t);
	ASSERT(softs->max_sg_per_single_iu_element == MAX_EMBEDDED_SG_IN_IU);

	/* max SGs for spanning cmd should be 68*/
	softs->max_sg_per_spanning_cmd = (softs->max_spanning_elems - 1) * softs->max_sg_per_single_iu_element;
	softs->max_sg_per_spanning_cmd += MAX_EMBEDDED_SG_IN_FIRST_IU_DEFAULT;

	DBG_INIT("softs->max_ib_iu_length: %d\n", softs->max_ib_iu_length);       /* 1152 per FW advertisement */
	DBG_INIT("softs->num_elem_per_op_ibq: %u\n", softs->num_elem_per_op_ibq); /* 32 for xcal */
	DBG_INIT("softs->num_elem_per_op_obq: %u\n", softs->num_elem_per_op_obq); /* 256 for xcal */
	DBG_INIT("softs->max_spanning_elems: %d\n", softs->max_spanning_elems);   /* 9 */
	DBG_INIT("softs->max_sg_per_spanning_cmd: %u\n", softs->max_sg_per_spanning_cmd); /* 68 until we add AIO writes */

	DBG_FUNC("OUT\n");
}

/*
 * Configure the operational queue parameters.
 */
int
pqisrc_configure_op_queues(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;

	/* Get the PQI capability,
		REPORT PQI DEVICE CAPABILITY request */
	ret = pqisrc_report_pqi_capability(softs);
	if (ret) {
		DBG_ERR("Failed to send report pqi dev capability request : %d\n",
				ret);
		goto err_out;
	}

	/* Reserve required no of slots for internal requests */
	softs->max_io_for_scsi_ml = softs->max_outstanding_io - PQI_RESERVED_IO_SLOTS_CNT;

	/* Decide the Op queue configuration */
	pqisrc_decide_opq_config(softs);

	DBG_FUNC("OUT\n");
	return ret;

err_out:
	DBG_FUNC("OUT failed\n");
	return ret;
}

/*
 * Validate the PQI mode of adapter.
 */
int
pqisrc_check_pqimode(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_FAILURE;
	int tmo = 0;
	uint64_t signature = 0;

	DBG_FUNC("IN\n");

	/* Check the PQI device signature */
	tmo = PQISRC_PQIMODE_READY_TIMEOUT;
	do {
		signature = LE_64(PCI_MEM_GET64(softs, &softs->pqi_reg->signature, PQI_SIGNATURE));

		if (memcmp(&signature, PQISRC_PQI_DEVICE_SIGNATURE,
				sizeof(uint64_t)) == 0) {
			ret = PQI_STATUS_SUCCESS;
			break;
		}
		OS_SLEEP(PQISRC_MODE_READY_POLL_INTERVAL);
	} while (tmo--);

	PRINT_PQI_SIGNATURE(signature);

	if (tmo <= 0) {
		DBG_ERR("PQI Signature is invalid\n");
		ret = PQI_STATUS_TIMEOUT;
		goto err_out;
	}

	tmo = PQISRC_PQIMODE_READY_TIMEOUT;
	/* Check function and status code for the device */
	COND_WAIT((PCI_MEM_GET64(softs, &softs->pqi_reg->admin_q_config,
		PQI_ADMINQ_CONFIG) == PQI_ADMIN_QUEUE_CONF_FUNC_STATUS_IDLE), tmo);
	if (!tmo) {
		DBG_ERR("PQI device is not in IDLE state\n");
		ret = PQI_STATUS_TIMEOUT;
		goto err_out;
	}


	tmo = PQISRC_PQIMODE_READY_TIMEOUT;
	/* Check the PQI device status register */
	COND_WAIT(LE_32(PCI_MEM_GET32(softs, &softs->pqi_reg->pqi_dev_status, PQI_DEV_STATUS)) &
				PQI_DEV_STATE_AT_INIT, tmo);
	if (!tmo) {
		DBG_ERR("PQI Registers are not ready\n");
		ret = PQI_STATUS_TIMEOUT;
		goto err_out;
	}

	DBG_FUNC("OUT\n");
	return ret;
err_out:
	DBG_FUNC("OUT failed\n");
	return ret;
}

/* Wait for PQI reset completion for the adapter*/
int
pqisrc_wait_for_pqi_reset_completion(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;
	pqi_reset_reg_t reset_reg;
	int pqi_reset_timeout = 0;
	uint64_t val = 0;
	uint32_t max_timeout = 0;

	val = PCI_MEM_GET64(softs, &softs->pqi_reg->pqi_dev_adminq_cap, PQI_ADMINQ_CAP);

	max_timeout = (val & 0xFFFF00000000) >> 32;

	DBG_INIT("max_timeout for PQI reset completion in 100 msec units = %u\n", max_timeout);

	while(1) {
		if (pqi_reset_timeout++ == max_timeout) {
			return PQI_STATUS_TIMEOUT;
		}
		OS_SLEEP(PQI_RESET_POLL_INTERVAL);/* 100 msec */
		reset_reg.all_bits = PCI_MEM_GET32(softs,
			&softs->pqi_reg->dev_reset, PQI_DEV_RESET);
		if (reset_reg.bits.reset_action == PQI_RESET_ACTION_COMPLETED)
			break;
	}

	return ret;
}

/*
 * Function used to perform PQI hard reset.
 */
int
pqi_reset(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;
	uint32_t val = 0;
	pqi_reset_reg_t pqi_reset_reg;

	DBG_FUNC("IN\n");

	if (true == softs->ctrl_in_pqi_mode) {

		if (softs->pqi_reset_quiesce_allowed) {
			val = PCI_MEM_GET32(softs, &softs->ioa_reg->host_to_ioa_db,
					LEGACY_SIS_IDBR);
			val |= SIS_PQI_RESET_QUIESCE;
			PCI_MEM_PUT32(softs, &softs->ioa_reg->host_to_ioa_db,
					LEGACY_SIS_IDBR, LE_32(val));
			OS_SLEEP(1000);     /* 1 ms delay for PCI W/R ordering issue */
			ret = pqisrc_sis_wait_for_db_bit_to_clear(softs, SIS_PQI_RESET_QUIESCE);
			if (ret) {
				DBG_ERR("failed with error %d during quiesce\n", ret);
				return ret;
			}
		}

		pqi_reset_reg.all_bits = 0;
		pqi_reset_reg.bits.reset_type = PQI_RESET_TYPE_HARD_RESET;
		pqi_reset_reg.bits.reset_action = PQI_RESET_ACTION_RESET;

		PCI_MEM_PUT32(softs, &softs->pqi_reg->dev_reset, PQI_DEV_RESET,
			LE_32(pqi_reset_reg.all_bits));
		OS_SLEEP(1000);     /* 1 ms delay for PCI W/R ordering issue */

		ret = pqisrc_wait_for_pqi_reset_completion(softs);
		if (ret) {
			DBG_ERR("PQI reset timed out: ret = %d!\n", ret);
			return ret;
		}
	}
	softs->ctrl_in_pqi_mode = false;
	DBG_FUNC("OUT\n");
	return ret;
}

/*
 * Initialize the adapter with supported PQI configuration.
 */
int
pqisrc_pqi_init(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;

	DBG_FUNC("IN\n");

	/* Check the PQI signature */
	ret = pqisrc_check_pqimode(softs);
	if(ret) {
		DBG_ERR("failed to switch to pqi\n");
                goto err_out;
	}

	PQI_SAVE_CTRL_MODE(softs, CTRL_PQI_MODE);
	softs->ctrl_in_pqi_mode = true;

	/* Get the No. of Online CPUs,NUMA/Processor config from OS */
	ret = os_get_processor_config(softs);
	if (ret) {
		DBG_ERR("Failed to get processor config from OS %d\n",
			ret);
		goto err_out;
	}

	softs->intr_type = INTR_TYPE_NONE;

	/* Get the interrupt count, type, priority available from OS */
	ret = os_get_intr_config(softs);
	if (ret) {
		DBG_ERR("Failed to get interrupt config from OS %d\n",
			ret);
		goto err_out;
	}

	/*Enable/Set Legacy INTx Interrupt mask clear pqi register,
	 *if allocated interrupt is legacy type.
	 */
	if (INTR_TYPE_FIXED == softs->intr_type) {
		pqisrc_configure_legacy_intx(softs, true);
		sis_enable_intx(softs);
	}

	/* Create Admin Queue pair*/
	ret = pqisrc_create_admin_queue(softs);
	if(ret) {
                DBG_ERR("Failed to configure admin queue\n");
                goto err_admin_queue;
    	}

	/* For creating event and IO operational queues we have to submit
	   admin IU requests.So Allocate resources for submitting IUs */

	/* Allocate the request container block (rcb) */
	ret = pqisrc_allocate_rcb(softs);
	if (ret == PQI_STATUS_FAILURE) {
                DBG_ERR("Failed to allocate rcb \n");
                goto err_rcb;
    	}

	/* Allocate & initialize request id queue */
	ret = pqisrc_init_taglist(softs,&softs->taglist,
				softs->max_outstanding_io);
	if (ret) {
		DBG_ERR("Failed to allocate memory for request id q : %d\n",
			ret);
		goto err_taglist;
	}

	ret = pqisrc_configure_op_queues(softs);
	if (ret) {
			DBG_ERR("Failed to configure op queue\n");
			goto err_config_opq;
	}

	/* Create Operational queues */
	ret = pqisrc_create_op_queues(softs);
	if(ret) {
		DBG_ERR("Failed to create op queue\n");
		goto err_create_opq;
	}

	softs->ctrl_online = true;

	DBG_FUNC("OUT\n");
	return ret;

err_create_opq:
err_config_opq:
	pqisrc_destroy_taglist(softs,&softs->taglist);
err_taglist:
	pqisrc_free_rcb(softs, softs->max_outstanding_io + 1);
err_rcb:
	pqisrc_destroy_admin_queue(softs);
err_admin_queue:
	os_free_intr_config(softs);
err_out:
	DBG_FUNC("OUT failed\n");
	return PQI_STATUS_FAILURE;
}

/* */
int
pqisrc_force_sis(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;

	if (SIS_IS_KERNEL_PANIC(softs)) {
		DBG_ERR("Controller FW is not running\n");
		return PQI_STATUS_FAILURE;
	}

	if (PQI_GET_CTRL_MODE(softs) == CTRL_SIS_MODE) {
		return ret;
	}

	if (SIS_IS_KERNEL_UP(softs)) {
		PQI_SAVE_CTRL_MODE(softs, CTRL_SIS_MODE);
		return ret;
	}
	/* Disable interrupts ? */
	sis_disable_interrupt(softs);

	/* reset pqi, this will delete queues */
	ret = pqi_reset(softs);
	if (ret) {
		return ret;
	}
	/* Re enable SIS */
	ret = pqisrc_reenable_sis(softs);
	if (ret) {
		return ret;
	}

	PQI_SAVE_CTRL_MODE(softs, CTRL_SIS_MODE);

	return ret;
}

/* 5 mins timeout for quiesce */
#define PQI_QUIESCE_TIMEOUT	300000

int
pqisrc_wait_for_cmnd_complete(pqisrc_softstate_t *softs)
{

	int count = 0;
	int ret = PQI_STATUS_SUCCESS;

	DBG_NOTE("softs->taglist.num_elem : %u",softs->taglist.num_elem);

	if (softs->taglist.num_elem == softs->max_outstanding_io)
		return ret;
	else {
		DBG_WARN("%u commands pending\n",
		softs->max_outstanding_io - softs->taglist.num_elem);

		while(1) {

			/* Since heartbeat timer stopped ,check for firmware status*/
			if (SIS_IS_KERNEL_PANIC(softs)) {
				DBG_ERR("Controller FW is not running\n");
				return PQI_STATUS_FAILURE;
			}

			if (softs->taglist.num_elem != softs->max_outstanding_io) {
				/* Sleep for 1 msec */
				OS_SLEEP(1000);
				count++;
				if(count % 1000 == 0) {
					DBG_WARN("Waited for %d seconds", count/1000);
				}
				if (count >= PQI_QUIESCE_TIMEOUT) {
					return PQI_STATUS_FAILURE;
				}
				continue;
			}
			break;
		}
	}
	return ret;
}

void
pqisrc_complete_internal_cmds(pqisrc_softstate_t *softs)
{

	int tag = 0;
	rcb_t *rcb;

	for (tag = 1; tag <= softs->max_outstanding_io; tag++) {
		rcb = &softs->rcb[tag];
		if(rcb->req_pending && is_internal_req(rcb)) {
			rcb->status = PQI_STATUS_TIMEOUT;
			rcb->req_pending = false;
		}
	}
}


/*
 * Uninitialize the resources used during PQI initialization.
 */
void
pqisrc_pqi_uninit(pqisrc_softstate_t *softs)
{
	int ret;

	DBG_FUNC("IN\n");

	/* Wait for any rescan to finish */
	pqisrc_wait_for_rescan_complete(softs);

	/* Wait for commands to complete */
	ret = pqisrc_wait_for_cmnd_complete(softs);

	/* disable and free the interrupt resources */
	os_destroy_intr(softs);

	/* Complete all pending commands. */
	if(ret != PQI_STATUS_SUCCESS) {
		pqisrc_complete_internal_cmds(softs);
		os_complete_outstanding_cmds_nodevice(softs);
	}

	if(softs->devlist_lockcreated==true){
		os_uninit_spinlock(&softs->devlist_lock);
		softs->devlist_lockcreated = false;
	}

	/* Free all queues */
	pqisrc_destroy_op_ib_queues(softs);
	pqisrc_destroy_op_ob_queues(softs);
	pqisrc_destroy_event_queue(softs);

	/* Free  rcb */
	pqisrc_free_rcb(softs, softs->max_outstanding_io + 1);

	/* Free request id lists */
	pqisrc_destroy_taglist(softs,&softs->taglist);

	/* Free Admin Queue */
	pqisrc_destroy_admin_queue(softs);

	/* Switch back to SIS mode */
	if (pqisrc_force_sis(softs)) {
		DBG_ERR("Failed to switch back the adapter to SIS mode!\n");
	}

	DBG_FUNC("OUT\n");
}


/*
 * Function to do any sanity checks for OS macros
 */
void
sanity_check_os_behavior(pqisrc_softstate_t *softs)
{
#ifdef OS_ATOMIC64_INC
	OS_ATOMIC64_T atomic_test_var = 0;
	OS_ATOMIC64_T atomic_ret = 0;

	atomic_ret = OS_ATOMIC64_INC(&atomic_test_var);
	ASSERT(atomic_ret == 1);

	atomic_ret = OS_ATOMIC64_INC(&atomic_test_var);
	ASSERT(atomic_ret == 2);

	atomic_ret = OS_ATOMIC64_DEC(&atomic_test_var);
	ASSERT(atomic_ret == 1);
#else
	DBG_INIT("OS needs to define/implement atomic macros\n");
#endif
}

/*
 * Function to initialize the adapter settings.
 */
int
pqisrc_init(pqisrc_softstate_t *softs)
{
	int ret = 0;
	uint32_t	ctrl_type;

	DBG_FUNC("IN\n");

	sanity_check_os_behavior(softs);

	check_struct_sizes();

	/*Get verbose flags, defined in OS code XX_debug.h or so*/
#ifdef DISABLE_ERR_RESP_VERBOSE
	softs->err_resp_verbose = false;
#else
	softs->err_resp_verbose = true;
#endif

	/* prevent attachment of revA hardware. */
	ctrl_type = PQI_GET_CTRL_TYPE(softs);
	if (ctrl_type == PQI_CTRL_PRODUCT_ID_GEN2_REV_A) {
		DBG_ERR("adapter at B.D.F=%u.%u.%u: unsupported RevA card.\n",
			softs->bus_id, softs->device_id, softs->func_id);
		ret = PQI_STATUS_FAILURE;
		goto err_out;
	}

	/* Increment the global adapter ID and tie it to this BDF */
#ifdef OS_ATOMIC64_INC
	static OS_ATOMIC64_T g_adapter_cnt = 0;
	softs->adapter_num = (uint8_t)OS_ATOMIC64_INC(&g_adapter_cnt);
#else
	static uint64_t g_adapter_cnt = 0;
   softs->adapter_num = (uint8_t)++g_adapter_cnt;
#endif
	DBG_NOTE("Initializing adapter %u\n", (uint32_t)softs->adapter_num);

	ret = os_create_semaphore("scan_lock", 1, &softs->scan_lock);
	if(ret != PQI_STATUS_SUCCESS){
		DBG_ERR(" Failed to initialize scan lock\n");
		goto err_out;
	}

	/* Init the Sync interface */
	ret = pqisrc_sis_init(softs);
	if (ret) {
		DBG_ERR("SIS Init failed with error %d\n", ret);
		goto err_sis;
	}


	/* Init the PQI interface */
	ret = pqisrc_pqi_init(softs);
	if (ret) {
		DBG_ERR("PQI Init failed with error %d\n", ret);
		goto err_pqi;
	}

	/* Setup interrupt */
	ret = os_setup_intr(softs);
	if (ret) {
		DBG_ERR("Interrupt setup failed with error %d\n", ret);
		goto err_intr;
	}

	/* Report event configuration */
	ret = pqisrc_report_event_config(softs);
	if(ret){
				DBG_ERR(" Failed to configure Report events\n");
		goto err_event;
	}

	/* Set event configuration*/
	ret = pqisrc_set_event_config(softs);
	if(ret){
				DBG_ERR(" Failed to configure Set events\n");
				goto err_event;
	}

	/* Check for For PQI spanning */
	ret = pqisrc_get_ctrl_fw_version(softs);
	if(ret){
				DBG_ERR(" Failed to get ctrl fw version\n");
	goto err_fw_version;
	}

	/* update driver version in to FW */
	ret = pqisrc_write_driver_version_to_host_wellness(softs);
	if (ret) {
		DBG_ERR(" Failed to update driver version in to FW");
		goto err_host_wellness;
	}

	/* Setup sense features */
	ret = pqisrc_QuerySenseFeatures(softs);
	if (ret) {
		DBG_ERR("Failed to get sense features\n");
		goto err_sense;
	}

	os_strlcpy(softs->devlist_lock_name, "devlist_lock", LOCKNAME_SIZE);
	ret = os_init_spinlock(softs, &softs->devlist_lock, softs->devlist_lock_name);
	if(ret){
		DBG_ERR(" Failed to initialize devlist_lock\n");
		softs->devlist_lockcreated=false;
		goto err_lock;
	}
	softs->devlist_lockcreated = true;

	/* Get the PQI configuration table to read heart-beat counter*/
	ret = pqisrc_process_config_table(softs);
	if (ret) {
		DBG_ERR("Failed to process PQI configuration table %d\n", ret);
		goto err_config_tab;
	}

	softs->prev_heartbeat_count = CTRLR_HEARTBEAT_CNT(softs) - OS_FW_HEARTBEAT_TIMER_INTERVAL;

	memset(softs->dev_list, 0, sizeof(*softs->dev_list));
	pqisrc_init_bitmap(softs);

	DBG_FUNC("OUT\n");
	return ret;

err_config_tab:
	if(softs->devlist_lockcreated==true){
		os_uninit_spinlock(&softs->devlist_lock);
		softs->devlist_lockcreated = false;
	}
err_lock:
err_fw_version:
err_event:
err_host_wellness:
err_intr:
err_sense:
	pqisrc_pqi_uninit(softs);
err_pqi:
	pqisrc_sis_uninit(softs);
err_sis:
	os_destroy_semaphore(&softs->scan_lock);
err_out:
	DBG_FUNC("OUT failed\n");
	return ret;
}

/*
 * Write all data in the adapter's battery-backed cache to
 * storage.
 */
int
pqisrc_flush_cache( pqisrc_softstate_t *softs,
			enum pqisrc_flush_cache_event_type event_type)
{
	int rval = PQI_STATUS_SUCCESS;
	pqisrc_raid_req_t request;
	pqisrc_bmic_flush_cache_t *flush_buff = NULL;

	DBG_FUNC("IN\n");

	if (pqisrc_ctrl_offline(softs))
		return PQI_STATUS_FAILURE;

	flush_buff = os_mem_alloc(softs, sizeof(pqisrc_bmic_flush_cache_t));
	if (!flush_buff) {
		DBG_ERR("Failed to allocate memory for flush cache params\n");
		rval = PQI_STATUS_FAILURE;
		return rval;
	}

	flush_buff->halt_event = event_type;

	memset(&request, 0, sizeof(request));

	request.data_direction = SOP_DATA_DIR_FROM_DEVICE;
	request.cmd.bmic_cdb.op_code = BMIC_WRITE;
	request.cmd.bmic_cdb.cmd = BMIC_CACHE_FLUSH;
	request.cmd.bmic_cdb.xfer_len = BE_16(sizeof(*flush_buff));

	rval = pqisrc_prepare_send_ctrlr_request(softs, &request, flush_buff, sizeof(*flush_buff));

	if (rval) {
		DBG_ERR("error in build send raid req ret=%d\n", rval);
	}

	os_mem_free(softs, (void *)flush_buff, sizeof(pqisrc_bmic_flush_cache_t));

	DBG_FUNC("OUT\n");

	return rval;
}

/*
 * Uninitialize the adapter.
 */
void
pqisrc_uninit(pqisrc_softstate_t *softs)
{
	DBG_FUNC("IN\n");

	pqisrc_pqi_uninit(softs);

	pqisrc_sis_uninit(softs);

	os_destroy_semaphore(&softs->scan_lock);

	pqisrc_cleanup_devices(softs);

	DBG_FUNC("OUT\n");
}
