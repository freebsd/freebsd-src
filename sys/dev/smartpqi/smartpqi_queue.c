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
 * Submit an admin IU to the adapter.
 * TODO : Admin command implemented using polling,
 * Add interrupt support, if required
 */
int
pqisrc_submit_admin_req(pqisrc_softstate_t *softs,
			gen_adm_req_iu_t *req, gen_adm_resp_iu_t *resp)
{
	int ret = PQI_STATUS_SUCCESS;
	ob_queue_t *ob_q = &softs->admin_ob_queue;
	ib_queue_t *ib_q = &softs->admin_ib_queue;
	int tmo = PQISRC_ADMIN_CMD_RESP_TIMEOUT;

	DBG_FUNC("IN\n");

	req->header.iu_type =
		PQI_IU_TYPE_GENERAL_ADMIN_REQUEST;
	req->header.comp_feature = 0x00;
	req->header.iu_length = PQI_STANDARD_IU_LENGTH;
	req->res1 = 0;
	req->work = 0;

	/* Get the tag */
	req->req_id = pqisrc_get_tag(&softs->taglist);
	if (INVALID_ELEM == req->req_id) {
		DBG_ERR("Tag not available0x%x\n",(uint16_t)req->req_id);
		ret = PQI_STATUS_FAILURE;
		goto err_out;
	}
	softs->rcb[req->req_id].tag = req->req_id;

	/* Submit the command to the admin ib queue */
	ret = pqisrc_submit_cmnd(softs, ib_q, req);
	if (ret != PQI_STATUS_SUCCESS) {
		DBG_ERR("Unable to submit command\n");
		goto err_cmd;
	}

	/* Wait for completion */
	COND_WAIT((*(ob_q->pi_virt_addr) != ob_q->ci_local), tmo);
	if (tmo <= 0) {
		DBG_ERR("Admin cmd timeout\n");
		DBG_ERR("tmo : %d\n",tmo);	\
		/* TODO : PQI device status and error register and report */
		ret = PQI_STATUS_TIMEOUT;
		goto err_cmd;
	}

	/* Copy the response */
	memcpy(resp, ob_q->array_virt_addr + (ob_q->ci_local * ob_q->elem_size),
					sizeof(gen_adm_resp_iu_t));

	/* Update CI */
	ob_q->ci_local = (ob_q->ci_local + 1 ) %  ob_q->num_elem;
	PCI_MEM_PUT32(softs, ob_q->ci_register_abs,
        ob_q->ci_register_offset, LE_32(ob_q->ci_local));

	/* Validate the response data */
	ASSERT(req->fn_code == resp->fn_code);
	ASSERT(resp->header.iu_type == PQI_IU_TYPE_GENERAL_ADMIN_RESPONSE);
	ret = resp->status;
	if (ret)
		goto err_cmd;

	os_reset_rcb(&softs->rcb[req->req_id]);
	pqisrc_put_tag(&softs->taglist,req->req_id);
	DBG_FUNC("OUT\n");
	return ret;
err_cmd:
	os_reset_rcb(&softs->rcb[req->req_id]);
	pqisrc_put_tag(&softs->taglist,req->req_id);
err_out:
	DBG_FUNC("failed OUT : %d\n", ret);
	return ret;
}

/*
 * Get the administration queue config parameters.
 */
void
pqisrc_get_admin_queue_config(pqisrc_softstate_t *softs)
{
	uint64_t val = 0;


	val = LE_64(PCI_MEM_GET64(softs, &softs->pqi_reg->pqi_dev_adminq_cap, PQI_ADMINQ_CAP));

	/* pqi_cap = (struct pqi_dev_adminq_cap *)&val;*/
	softs->admin_ib_queue.num_elem  =  val & 0xFF;
	softs->admin_ob_queue.num_elem  = (val & 0xFF00) >> 8;
	/* Note : size in unit of 16 byte s*/
	softs->admin_ib_queue.elem_size = ((val & 0xFF0000) >> 16) * 16;
	softs->admin_ob_queue.elem_size = ((val & 0xFF000000) >> 24) * 16;

	DBG_INIT(" admin ib: num_elem=%u elem_size=%u\n",
			softs->admin_ib_queue.num_elem, softs->admin_ib_queue.elem_size);
	DBG_INIT(" admin ob: num_elem=%u elem_size=%u\n",
			softs->admin_ob_queue.num_elem, softs->admin_ob_queue.elem_size);
}

/*
 * Decide the no of elements in admin ib and ob queues.
 */
void
pqisrc_decide_admin_queue_config(pqisrc_softstate_t *softs)
{
	/* Determine  num elements in Admin IBQ  */
	softs->admin_ib_queue.num_elem = MIN(softs->admin_ib_queue.num_elem,
					PQISRC_MAX_ADMIN_IB_QUEUE_ELEM_NUM);

	/* Determine  num elements in Admin OBQ  */
	softs->admin_ob_queue.num_elem = MIN(softs->admin_ob_queue.num_elem,
					PQISRC_MAX_ADMIN_OB_QUEUE_ELEM_NUM);
}

/*
 * Allocate DMA memory for inbound queue and initialize.
 */
int
pqisrc_allocate_and_init_inbound_q(pqisrc_softstate_t *softs, ib_queue_t *ib_q, char *tag)
{
	struct dma_mem *dma_mem = &ib_q->alloc_dma;
	uint32_t ib_array_size = 0;
	uint32_t alloc_size = 0;
	char *virt_addr = NULL;
	dma_addr_t dma_addr = 0;
	int ret = PQI_STATUS_SUCCESS;

	ib_array_size = ib_q->num_elem * ib_q->elem_size;
	ASSERT(ib_array_size > 0);

	alloc_size = ib_array_size + PQI_CI_PI_ALIGN + PQI_ADDR_ALIGN; /* for IB CI and OB PI */

	/* Allocate memory for the Q */
	memset(dma_mem, 0, sizeof(*dma_mem));
	os_strlcpy(dma_mem->tag, tag, sizeof(dma_mem->tag));
	dma_mem->size = alloc_size;
	dma_mem->align = PQI_ADDR_ALIGN;
	ret = os_dma_mem_alloc(softs, &ib_q->alloc_dma);
	if (ret) {
		DBG_ERR("Failed to Allocate Q tag=%s ret=%d\n", dma_mem->tag, ret);
		goto err_out;
	}

	DBG_INIT("alloc tag=%s size=0x%x align=0x%x virt_addr=%p dma_addr=%p\n",
		dma_mem->tag, dma_mem->size, dma_mem->align, dma_mem->virt_addr, (void*)dma_mem->dma_addr);

	/* Setup the address */
	virt_addr = dma_mem->virt_addr;
	dma_addr = dma_mem->dma_addr;
	ASSERT(!((uint64_t)virt_addr & PQI_ADDR_ALIGN_MASK));
	ASSERT(!(dma_addr & PQI_ADDR_ALIGN_MASK));

	/* IB */
	ASSERT(!(dma_addr &  PQI_ADDR_ALIGN_MASK));
	ib_q->array_virt_addr = virt_addr;
	ib_q->array_dma_addr = dma_addr;
	ib_q->pi_local = 0;

	/* update addr for the next user */
	virt_addr += ib_array_size;
	dma_addr += ib_array_size;

	/* IB CI */
	ASSERT(!(dma_addr & PQI_CI_PI_ALIGN_MASK));
	ib_q->ci_virt_addr = (uint32_t*)virt_addr;
	ib_q->ci_dma_addr = dma_addr;

	/* update addr for the next user */
	virt_addr += PQI_CI_PI_ALIGN;

	DBG_INIT("ib_q: virt_addr=%p, ci_dma_addr=%p elem=%u size=%u\n",
		ib_q->array_virt_addr, (void*)ib_q->ci_dma_addr, ib_q->num_elem, ib_array_size);

	/* Verify we aren't out of bounds from allocation */
	ASSERT(virt_addr <= ((char*)dma_mem->virt_addr + alloc_size));

	DBG_FUNC("OUT\n");
	return ret;

err_out:
	DBG_FUNC("failed OUT\n");
	return PQI_STATUS_FAILURE;
}


/*
 * Allocate DMA memory for outbound queue and initialize.
 */
int
pqisrc_allocate_and_init_outbound_q(pqisrc_softstate_t *softs, ob_queue_t *ob_q,
	char *tag)
{
	struct dma_mem *dma_mem = &ob_q->alloc_dma;
	uint32_t ob_array_size = 0;
	uint32_t alloc_size = 0;
	char *virt_addr = NULL;
	dma_addr_t dma_addr = 0;
	int ret = PQI_STATUS_SUCCESS;

	ob_array_size = ob_q->num_elem * ob_q->elem_size;
	ASSERT(ob_array_size > 0);

	alloc_size = ob_array_size + PQI_CI_PI_ALIGN + PQI_ADDR_ALIGN; /* for OB PI */

	/* Allocate memory for the Q */
	memset(dma_mem, 0, sizeof(*dma_mem));
	os_strlcpy(dma_mem->tag, tag, sizeof(dma_mem->tag));
	dma_mem->size = alloc_size;
	dma_mem->align = PQI_ADDR_ALIGN;
	ret = os_dma_mem_alloc(softs, &ob_q->alloc_dma);
	if (ret) {
		DBG_ERR("Failed to Allocate Q tag=%s ret=%d\n", dma_mem->tag, ret);
		goto err_out;
	}

	DBG_INIT("alloc tag=%s size=0x%x align=0x%x virt_addr=%p dma_addr=%p\n",
		dma_mem->tag, dma_mem->size, dma_mem->align, dma_mem->virt_addr, (void*)dma_mem->dma_addr);

	/* Setup the address */
	virt_addr = dma_mem->virt_addr;
	dma_addr = dma_mem->dma_addr;
	ASSERT(!((uint64_t)virt_addr & PQI_ADDR_ALIGN_MASK));
	ASSERT(!(dma_addr & PQI_ADDR_ALIGN_MASK));

	ob_q->array_virt_addr = virt_addr;
	ob_q->array_dma_addr = dma_addr;
	ob_q->ci_local = 0;

	/* update addr for the next user */
	virt_addr += ob_array_size;
	dma_addr += ob_array_size;

	/* OB PI */
	ASSERT(!(dma_addr & PQI_CI_PI_ALIGN_MASK));
	ob_q->pi_virt_addr = (uint32_t*)virt_addr;
	ob_q->pi_dma_addr = dma_addr;

	/* update addr to show the end next user */
	virt_addr += PQI_CI_PI_ALIGN;

	DBG_INIT("ob_q: virt_addr=%p, pi_dma_addr=%p elem=%u size=%u\n",
		ob_q->array_virt_addr, (void*)ob_q->pi_dma_addr, ob_q->num_elem, ob_array_size);

	/* Verify we aren't out of bounds from allocation */
	ASSERT(virt_addr <= ((char*)dma_mem->virt_addr + alloc_size));

	DBG_FUNC("OUT\n");
	return ret;

err_out:
	DBG_FUNC("failed OUT\n");
	return PQI_STATUS_FAILURE;
}

/*
 * Allocate DMA memory for admin queue and initialize.
 */
int pqisrc_allocate_and_init_adminq(pqisrc_softstate_t *softs)
{
	int ret;
	ib_queue_t *admin_ib_q = &softs->admin_ib_queue;
	ob_queue_t *admin_ob_q = &softs->admin_ob_queue;

	ret = pqisrc_allocate_and_init_inbound_q(softs, admin_ib_q, "admin_queue");
	if (!ret) {
		admin_ib_q->q_id = PQI_ADMIN_IB_QUEUE_ID;
		ret = pqisrc_allocate_and_init_outbound_q(softs, admin_ob_q, "admin_queue");
		if(!ret)
			admin_ob_q->q_id = PQI_ADMIN_OB_QUEUE_ID;
		else {
			if(softs->admin_ib_queue.lockcreated==true) {
				OS_UNINIT_PQILOCK(&softs->admin_ib_queue.lock);
				softs->admin_ib_queue.lockcreated = false;
			}
			if (softs->admin_ib_queue.alloc_dma.virt_addr)
				os_dma_mem_free(softs, &softs->admin_ib_queue.alloc_dma);
		}
	}
	else
		DBG_ERR("Failed to create Admin Queue pair\n");

	return ret;
}

/*
 * Subroutine used to create (or) delete the admin queue requested.
 */
int
pqisrc_create_delete_adminq(pqisrc_softstate_t *softs, uint32_t cmd)
{
	int tmo = 0;
	int ret = PQI_STATUS_SUCCESS;

	/* Create Admin Q pair writing to Admin Q config function reg */

	PCI_MEM_PUT64(softs, &softs->pqi_reg->admin_q_config, PQI_ADMINQ_CONFIG, LE_64(cmd));

	if (cmd == PQI_ADMIN_QUEUE_CONF_FUNC_CREATE_Q_PAIR)
		tmo = PQISRC_ADMIN_QUEUE_CREATE_TIMEOUT;
	else
		tmo = PQISRC_ADMIN_QUEUE_DELETE_TIMEOUT;

	/* Wait for completion */
	COND_WAIT((PCI_MEM_GET64(softs, &softs->pqi_reg->admin_q_config, PQI_ADMINQ_CONFIG) ==
				PQI_ADMIN_QUEUE_CONF_FUNC_STATUS_IDLE), tmo);
	if (tmo <= 0) {
		DBG_ERR("Unable to create/delete admin queue pair\n");
		/* TODO : PQI device status and error register and report */
		ret = PQI_STATUS_TIMEOUT;
	}

	return ret;
}

/*
 * Debug admin queue configuration params.
 */
void
pqisrc_print_adminq_config(pqisrc_softstate_t *softs)
{
	DBG_INFO(" softs->admin_ib_queue.array_dma_addr : %p\n",
		(void*)softs->admin_ib_queue.array_dma_addr);
	DBG_INFO(" softs->admin_ib_queue.array_virt_addr : %p\n",
		(void*)softs->admin_ib_queue.array_virt_addr);
	DBG_INFO(" softs->admin_ib_queue.num_elem : %u\n",
		softs->admin_ib_queue.num_elem);
	DBG_INFO(" softs->admin_ib_queue.elem_size : %u\n",
		softs->admin_ib_queue.elem_size);
	DBG_INFO(" softs->admin_ob_queue.array_dma_addr : %p\n",
		(void*)softs->admin_ob_queue.array_dma_addr);
	DBG_INFO(" softs->admin_ob_queue.array_virt_addr : %p\n",
		(void*)softs->admin_ob_queue.array_virt_addr);
	DBG_INFO(" softs->admin_ob_queue.num_elem : %u\n",
		softs->admin_ob_queue.num_elem);
	DBG_INFO(" softs->admin_ob_queue.elem_size : %u\n",
		softs->admin_ob_queue.elem_size);
	DBG_INFO(" softs->admin_ib_queue.pi_register_abs : %p\n",
		(void*)softs->admin_ib_queue.pi_register_abs);
	DBG_INFO(" softs->admin_ob_queue.ci_register_abs : %p\n",
		(void*)softs->admin_ob_queue.ci_register_abs);
}

/*
 * Function used to create an admin queue.
 */
int
pqisrc_create_admin_queue(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;
/*	struct pqi_dev_adminq_cap *pqi_cap; */
	uint32_t admin_q_param = 0;

	DBG_FUNC("IN\n");

	/* Get admin queue details  - pqi2-r00a - table 24 */
	pqisrc_get_admin_queue_config(softs);

	/* Decide admin Q config */
	pqisrc_decide_admin_queue_config(softs);

	/* Allocate and init Admin Q pair */
	ret = pqisrc_allocate_and_init_adminq(softs);
	if (ret) {
		DBG_ERR("Failed to Allocate Admin Q ret : %d\n", ret);
		goto err_out;
	}

	/* Write IB Q element array address */
	PCI_MEM_PUT64(softs, &softs->pqi_reg->admin_ibq_elem_array_addr,
        PQI_ADMIN_IBQ_ELEM_ARRAY_ADDR, LE_64(softs->admin_ib_queue.array_dma_addr));

	/* Write OB Q element array address */
	PCI_MEM_PUT64(softs, &softs->pqi_reg->admin_obq_elem_array_addr,
        PQI_ADMIN_OBQ_ELEM_ARRAY_ADDR, LE_64(softs->admin_ob_queue.array_dma_addr));

	/* Write IB Q CI address */
	PCI_MEM_PUT64(softs, &softs->pqi_reg->admin_ibq_ci_addr,
        PQI_ADMIN_IBQ_CI_ADDR, LE_64(softs->admin_ib_queue.ci_dma_addr));

	/* Write OB Q PI address */
	PCI_MEM_PUT64(softs, &softs->pqi_reg->admin_obq_pi_addr,
        PQI_ADMIN_OBQ_PI_ADDR, LE_64(softs->admin_ob_queue.pi_dma_addr));


	/* Write Admin Q params pqi-r200a table 36 */

	admin_q_param = softs->admin_ib_queue.num_elem |
			 (softs->admin_ob_queue.num_elem  << 8)|
		        PQI_ADMIN_QUEUE_MSIX_DISABLE;

	PCI_MEM_PUT32(softs, &softs->pqi_reg->admin_q_param,
        PQI_ADMINQ_PARAM, LE_32(admin_q_param));

	/* Submit cmd to create Admin Q pair */
	ret = pqisrc_create_delete_adminq(softs,
			PQI_ADMIN_QUEUE_CONF_FUNC_CREATE_Q_PAIR);
	if (ret) {
		DBG_ERR("Failed to Allocate Admin Q ret : %d\n", ret);
		goto err_q_create;
	}

	/* Admin queue created, get ci,pi offset */
    softs->admin_ib_queue.pi_register_offset =(PQISRC_PQI_REG_OFFSET +
        PCI_MEM_GET64(softs, &softs->pqi_reg->admin_ibq_pi_offset, PQI_ADMIN_IBQ_PI_OFFSET));

    softs->admin_ib_queue.pi_register_abs =(uint32_t *)(softs->pci_mem_base_vaddr +
        softs->admin_ib_queue.pi_register_offset);

    softs->admin_ob_queue.ci_register_offset = (PQISRC_PQI_REG_OFFSET +
        PCI_MEM_GET64(softs, &softs->pqi_reg->admin_obq_ci_offset, PQI_ADMIN_OBQ_CI_OFFSET));

    softs->admin_ob_queue.ci_register_abs = (uint32_t *)(softs->pci_mem_base_vaddr +
        softs->admin_ob_queue.ci_register_offset);

    os_strlcpy(softs->admin_ib_queue.lockname, "admin_ibqlock", LOCKNAME_SIZE);

    ret =OS_INIT_PQILOCK(softs, &softs->admin_ib_queue.lock,
            softs->admin_ib_queue.lockname);
    if(ret){
        DBG_ERR("Admin spinlock initialization failed\n");
        softs->admin_ib_queue.lockcreated = false;
        goto err_lock;
	}
    softs->admin_ib_queue.lockcreated = true;

	/* Print admin q config details */
	pqisrc_print_adminq_config(softs);

	DBG_FUNC("OUT\n");
	return ret;

err_lock:
#if 0
	pqisrc_create_delete_adminq(softs, PQI_ADMIN_QUEUE_CONF_FUNC_DEL_Q_PAIR);
#endif
err_q_create:
	 pqisrc_destroy_admin_queue(softs);
err_out:
	DBG_FUNC("failed OUT\n");
	return ret;
}

/*
 * Subroutine used to delete an operational queue.
 */
int
pqisrc_delete_op_queue(pqisrc_softstate_t *softs,
		uint32_t q_id, boolean_t ibq)
{
	int ret = PQI_STATUS_SUCCESS;
	/* Firmware doesn't support this now */

#if 0
	gen_adm_req_iu_t admin_req;
	gen_adm_resp_iu_t admin_resp;


	memset(&admin_req, 0, sizeof(admin_req));
	memset(&admin_resp, 0, sizeof(admin_resp));

	DBG_FUNC("IN\n");

	admin_req.req_type.create_op_iq.qid = q_id;

	if (ibq)
		admin_req.fn_code = PQI_FUNCTION_DELETE_OPERATIONAL_IQ;
	else
		admin_req.fn_code = PQI_FUNCTION_DELETE_OPERATIONAL_OQ;


	ret = pqisrc_submit_admin_req(softs, &admin_req, &admin_resp);

	DBG_FUNC("OUT\n");
#endif
	return ret;
}

/*
 * Function used to destroy the event queue.
 */
void
pqisrc_destroy_event_queue(pqisrc_softstate_t *softs)
{
	DBG_FUNC("IN\n");

	if (softs->event_q.created == true) {
		int ret = PQI_STATUS_SUCCESS;
		ret = pqisrc_delete_op_queue(softs, softs->event_q.q_id, false);
		if (ret) {
			DBG_ERR("Failed to Delete Event Q %u\n", softs->event_q.q_id);
		}
		softs->event_q.created = false;
	}

	/* Free the memory */
	if (softs->event_q.alloc_dma.virt_addr)
		os_dma_mem_free(softs, &softs->event_q.alloc_dma);

	DBG_FUNC("OUT\n");
}

/*
 * Function used to destroy operational ib queues.
 */
void
pqisrc_destroy_op_ib_queues(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;
	ib_queue_t *op_ib_q = NULL;
	uint32_t total_op_ibq = softs->num_op_raid_ibq;
	int i;

	DBG_FUNC("IN\n");

	for (i = 0; i < total_op_ibq; i++) {
                int repeat = 0;
		/* RAID first */
		op_ib_q = &softs->op_raid_ib_q[i];
release_queue:
		if (op_ib_q->created == true) {
			ret = pqisrc_delete_op_queue(softs, op_ib_q->q_id,
							true);
			if (ret) {
				DBG_ERR("Failed to Delete IB Q %u\n",
					op_ib_q->q_id);
			}
			op_ib_q->created = false;
		}

		if (op_ib_q->lockcreated == true) {
			OS_UNINIT_PQILOCK(&op_ib_q->lock);
			op_ib_q->lockcreated = false;
		}

		/* Free the memory */
		if (op_ib_q->alloc_dma.virt_addr)
			os_dma_mem_free(softs, &op_ib_q->alloc_dma);

		if (repeat < 1) {
			repeat++;
			op_ib_q = &softs->op_aio_ib_q[i];
			goto release_queue;
		}
	}

	DBG_FUNC("OUT\n");
}

/*
 * Function used to destroy operational ob queues.
 */
void
pqisrc_destroy_op_ob_queues(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;
	int i;
	ob_queue_t *op_ob_q = NULL;

	DBG_FUNC("IN\n");

	for (i = 0; i <  softs->num_op_obq; i++) {
		op_ob_q = &softs->op_ob_q[i];

		if (op_ob_q->created == true) {
			ret = pqisrc_delete_op_queue(softs, op_ob_q->q_id, false);
			if (ret) {
				DBG_ERR("Failed to Delete OB Q %u\n",op_ob_q->q_id);
			}
			op_ob_q->created = false;
		}

		/* Free the memory */
		if (op_ob_q->alloc_dma.virt_addr)
			os_dma_mem_free(softs, &op_ob_q->alloc_dma);
	}

	/* Free the memory */
	DBG_FUNC("OUT\n");
}

/*
 * Function used to destroy an admin queue.
 */
int
pqisrc_destroy_admin_queue(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;

	DBG_FUNC("IN\n");

	if(softs->admin_ib_queue.lockcreated==true) {
		OS_UNINIT_PQILOCK(&softs->admin_ib_queue.lock);
		softs->admin_ib_queue.lockcreated = false;
	}

#if 0
	ret = pqisrc_create_delete_adminq(softs,
				PQI_ADMIN_QUEUE_CONF_FUNC_DEL_Q_PAIR);
#endif

	if (softs->admin_ib_queue.alloc_dma.virt_addr)
		os_dma_mem_free(softs, &softs->admin_ib_queue.alloc_dma);

	if (softs->admin_ob_queue.alloc_dma.virt_addr)
		os_dma_mem_free(softs, &softs->admin_ob_queue.alloc_dma);

	DBG_FUNC("OUT\n");
	return ret;
}

/*
 * Function used to change operational ib queue properties.
 */
int
pqisrc_change_op_ibq_queue_prop(pqisrc_softstate_t *softs,
			ib_queue_t *op_ib_q, uint32_t prop)
{
	int ret = PQI_STATUS_SUCCESS;
	gen_adm_req_iu_t admin_req;
	gen_adm_resp_iu_t admin_resp;

	memset(&admin_req, 0, sizeof(admin_req));
	memset(&admin_resp, 0, sizeof(admin_resp));

	DBG_FUNC("IN\n");

	admin_req.fn_code = PQI_FUNCTION_CHANGE_OPERATIONAL_IQ_PROP;
	admin_req.req_type.change_op_iq_prop.qid = op_ib_q->q_id;
	admin_req.req_type.change_op_iq_prop.vend_specific = prop;

	ret = pqisrc_submit_admin_req(softs, &admin_req, &admin_resp);

	DBG_FUNC("OUT\n");
	return ret;
}

/*
 * Function used to create an operational ob queue.
 */
int
pqisrc_create_op_obq(pqisrc_softstate_t *softs,
			ob_queue_t *op_ob_q)
{
	int ret = PQI_STATUS_SUCCESS;
	gen_adm_req_iu_t admin_req;
	gen_adm_resp_iu_t admin_resp;

	DBG_FUNC("IN\n");

	memset(&admin_req, 0, sizeof(admin_req));
	memset(&admin_resp, 0, sizeof(admin_resp));

	admin_req.fn_code = PQI_FUNCTION_CREATE_OPERATIONAL_OQ;
	admin_req.req_type.create_op_oq.qid = op_ob_q->q_id;
	admin_req.req_type.create_op_oq.intr_msg_num =  op_ob_q->intr_msg_num;
	admin_req.req_type.create_op_oq.elem_arr_addr = op_ob_q->array_dma_addr;
	admin_req.req_type.create_op_oq.ob_pi_addr = op_ob_q->pi_dma_addr;
	admin_req.req_type.create_op_oq.num_elem =  op_ob_q->num_elem;
	admin_req.req_type.create_op_oq.elem_len = op_ob_q->elem_size / 16;

	DBG_INFO("admin_req.req_type.create_op_oq.qid : %x\n",admin_req.req_type.create_op_oq.qid);
	DBG_INFO("admin_req.req_type.create_op_oq.intr_msg_num  : %x\n", admin_req.req_type.create_op_oq.intr_msg_num );

	ret = pqisrc_submit_admin_req(softs, &admin_req, &admin_resp);
	if( PQI_STATUS_SUCCESS == ret) {
		op_ob_q->ci_register_offset = (PQISRC_PQI_REG_OFFSET +
				admin_resp.resp_type.create_op_oq.ci_offset);
		op_ob_q->ci_register_abs = (uint32_t *)(softs->pci_mem_base_vaddr +
				op_ob_q->ci_register_offset);
    	} else {
		int i = 0;
		DBG_WARN("Error Status Descriptors\n");
		for(i = 0; i < 4;i++)
			DBG_WARN(" %x ",admin_resp.resp_type.create_op_oq.status_desc[i]);
	}

	DBG_FUNC("OUT ret : %d\n", ret);

	return ret;
}

/*
 * Function used to create an operational ib queue.
 */
int
pqisrc_create_op_ibq(pqisrc_softstate_t *softs,
			ib_queue_t *op_ib_q)
{
	int ret = PQI_STATUS_SUCCESS;
	gen_adm_req_iu_t admin_req;
	gen_adm_resp_iu_t admin_resp;

	DBG_FUNC("IN\n");

	memset(&admin_req, 0, sizeof(admin_req));
	memset(&admin_resp, 0, sizeof(admin_resp));

	admin_req.fn_code = PQI_FUNCTION_CREATE_OPERATIONAL_IQ;
	admin_req.req_type.create_op_iq.qid = op_ib_q->q_id;
	admin_req.req_type.create_op_iq.elem_arr_addr = op_ib_q->array_dma_addr;
	admin_req.req_type.create_op_iq.iq_ci_addr = op_ib_q->ci_dma_addr;
	admin_req.req_type.create_op_iq.num_elem =  op_ib_q->num_elem;
	admin_req.req_type.create_op_iq.elem_len = op_ib_q->elem_size / 16;

	ret = pqisrc_submit_admin_req(softs, &admin_req, &admin_resp);

	if( PQI_STATUS_SUCCESS == ret) {
		op_ib_q->pi_register_offset =(PQISRC_PQI_REG_OFFSET +
				admin_resp.resp_type.create_op_iq.pi_offset);

		op_ib_q->pi_register_abs =(uint32_t *)(softs->pci_mem_base_vaddr +
				op_ib_q->pi_register_offset);
	} else {
		int i = 0;
		DBG_WARN("Error Status Decsriptors\n");
		for(i = 0; i < 4;i++)
			DBG_WARN(" %x ",admin_resp.resp_type.create_op_iq.status_desc[i]);
	}

	DBG_FUNC("OUT ret : %d\n", ret);
	return ret;
}

/*
 * subroutine used to create an operational ib queue for AIO.
 */
int
pqisrc_create_op_aio_ibq(pqisrc_softstate_t *softs,
			ib_queue_t *op_aio_ib_q)
{
	int ret = PQI_STATUS_SUCCESS;

	DBG_FUNC("IN\n");

	ret = pqisrc_create_op_ibq(softs,op_aio_ib_q);
	if ( PQI_STATUS_SUCCESS == ret)
		ret = pqisrc_change_op_ibq_queue_prop(softs,
					op_aio_ib_q, PQI_CHANGE_OP_IQ_PROP_ASSIGN_AIO);

	DBG_FUNC("OUT ret : %d\n", ret);
	return ret;
}

/*
 * subroutine used to create an operational ib queue for RAID.
 */
int
pqisrc_create_op_raid_ibq(pqisrc_softstate_t *softs,
			ib_queue_t *op_raid_ib_q)
{
	int ret = PQI_STATUS_SUCCESS;

	DBG_FUNC("IN\n");

	ret = pqisrc_create_op_ibq(softs,op_raid_ib_q);

	DBG_FUNC("OUT\n");
	return ret;
}

/*
 * Allocate and create an event queue to process supported events.
 */
int
pqisrc_alloc_and_create_event_queue(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;
	uint32_t num_elem;
	ob_queue_t *event_q = &softs->event_q;

	DBG_FUNC("IN\n");

	/*
	 * Calculate memory requirements.
	 * If event queue is shared for IO response, number of
	 * elements in event queue depends on num elements in OP OB Q
	 * also. Since event queue element size (32) is more than IO
	 * response size , event queue element size need not be checked
	 * for queue size calculation.
	 */
#ifdef SHARE_EVENT_QUEUE_FOR_IO
	num_elem = MIN(softs->num_elem_per_op_obq, PQISRC_MAX_EVENT_QUEUE_ELEM_NUM);
#else
	num_elem = PQISRC_MAX_EVENT_QUEUE_ELEM_NUM;
#endif

	event_q->num_elem = num_elem;
	event_q->elem_size = PQISRC_EVENT_Q_ELEM_SIZE_BYTES;

	ret = pqisrc_allocate_and_init_outbound_q(softs, event_q, "event_queue");

	if (ret) {
		DBG_ERR("Failed to Allocate EventQ\n");
		goto err_out;
	}
	event_q->q_id = PQI_OP_EVENT_QUEUE_ID;
	event_q->intr_msg_num = 0; /* vector zero for event */

	ret = pqisrc_create_op_obq(softs,event_q);
	if (ret) {
		DBG_ERR("Failed to Create EventQ %u\n",event_q->q_id);
		goto err_out_create;
	}
	event_q->created  = true;

	DBG_FUNC("OUT\n");
	return ret;

err_out_create:
	pqisrc_destroy_event_queue(softs);
err_out:
	DBG_FUNC("OUT failed %d\n", ret);
	return PQI_STATUS_FAILURE;
}

/*
 * Allocate DMA memory and create operational ib queues.
 */
int
pqisrc_alloc_and_create_ib_queues(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;
	ib_queue_t *op_ib_q = NULL;
	uint32_t ibq_id = PQI_MIN_OP_IB_QUEUE_ID;
	uint32_t total_op_ibq = softs->num_op_raid_ibq + softs->num_op_aio_ibq;
	int i = 0;
	char *string = NULL;

	DBG_FUNC("IN\n");

	ASSERT(softs->num_op_raid_ibq == softs->num_op_aio_ibq);

	for (i = 0; i < total_op_ibq; i++) {

		/* OP RAID IB Q */
		if (i % 2 == 0)
		{
			op_ib_q = &softs->op_raid_ib_q[i/2];
			string = "raid";
		}
		else
		{
			op_ib_q = &softs->op_aio_ib_q[i/2];
			string = "aio";
		}

		/* Allocate memory for IB queues */
		op_ib_q->num_elem = softs->num_elem_per_op_ibq;
		op_ib_q->elem_size = softs->max_ibq_elem_size;

		ret = pqisrc_allocate_and_init_inbound_q(softs, op_ib_q, "op_ib_queue");
		if (ret) {
			DBG_ERR("Failed to Allocate Operational IBQ memory ret : %d\n",
							ret);
			goto err_out;
		}
		op_ib_q->q_id = ibq_id++;

		snprintf(op_ib_q->lockname, LOCKNAME_SIZE, "%s_ibqlock_%d", string, i);
		ret = OS_INIT_PQILOCK(softs, &op_ib_q->lock, op_ib_q->lockname);
		if(ret){
			/* TODO: error handling */
			DBG_ERR("%s %d init failed\n", string, i);
			op_ib_q->lockcreated = false;
			goto err_lock;
		}
		op_ib_q->lockcreated = true;

		if (i % 2 == 0)
			ret = pqisrc_create_op_raid_ibq(softs, op_ib_q);
		else
			ret = pqisrc_create_op_aio_ibq(softs, op_ib_q);
		if (ret) {
			DBG_ERR("Failed to Create OP IBQ type=%s id=%u\n",
					string, op_ib_q->q_id);
			goto err_out_create;
		}
		op_ib_q->created  = true;
	}

	DBG_FUNC("OUT\n");
	return ret;

err_lock:
err_out_create:
err_out:
	pqisrc_destroy_op_ib_queues(softs);
	DBG_FUNC("OUT failed %d\n", ret);
	return PQI_STATUS_FAILURE;
}

/*
 * Allocate DMA memory and create operational ob queues.
 */
int
pqisrc_alloc_and_create_ob_queues(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;
	uint32_t obq_id = PQI_MIN_OP_OB_QUEUE_ID;
	ob_queue_t *op_ob_q = NULL;
	int i = 0;

	DBG_FUNC("IN\n");

	/*
	 * OB Q element array should be 64 byte aligned.
	 * So the number of elements in OB Q should be multiple
	 * of 4, so that OB Queue element size (16) * num elements
	 * will be multiple of 64.
	 */

	ALIGN_BOUNDARY(softs->num_elem_per_op_obq, 4);

	DBG_INIT("softs->num_op_obq %u max_obq_elem_size=%u\n",softs->num_op_obq, softs->max_obq_elem_size);

	for (i = 0; i <  softs->num_op_obq; i++) {
		op_ob_q = &softs->op_ob_q[i];

		/* Allocate memory for OB queues */
		op_ob_q->num_elem = softs->num_elem_per_op_obq;
		op_ob_q->elem_size = PQISRC_OP_OBQ_ELEM_SIZE_BYTES;
		ret = pqisrc_allocate_and_init_outbound_q(softs, op_ob_q, "op_ob_queue");
		if (ret) {
			DBG_ERR("Failed to Allocate Operational OBQ memory ret : %d\n",
							ret);
			goto err_out;
		}
		op_ob_q->q_id = obq_id++;
		if(softs->share_opq_and_eventq == true)
			op_ob_q->intr_msg_num = i;
		else
			op_ob_q->intr_msg_num = i + 1; /* msg num zero for event */

		ret = pqisrc_create_op_obq(softs, op_ob_q);
		if (ret) {
			DBG_ERR("Failed to Create OP OBQ %u\n",op_ob_q->q_id);
			goto err_out_create;
		}
		op_ob_q->created  = true;
	}

	DBG_FUNC("OUT\n");
	return ret;

err_out_create:
err_out:
	pqisrc_destroy_op_ob_queues(softs);
	DBG_FUNC("OUT failed %d\n", ret);
	return PQI_STATUS_FAILURE;
}

/*
 * Function used to create operational queues for the adapter.
 */
int
pqisrc_create_op_queues(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;

	DBG_FUNC("IN\n");

	/* Create Operational IB queues */
	ret = pqisrc_alloc_and_create_ib_queues(softs);
	if (ret)
		goto err_out;
	/* Create Operational OB queues */
	ret = pqisrc_alloc_and_create_ob_queues(softs);
	if (ret)
		goto err_out_obq;

	/* Create Event queue */
	ret = pqisrc_alloc_and_create_event_queue(softs);
	if (ret)
		goto err_out_eventq;

	DBG_FUNC("OUT\n");
	return ret;
err_out_eventq:
	pqisrc_destroy_op_ob_queues(softs);
err_out_obq:
	pqisrc_destroy_op_ib_queues(softs);
err_out:
	DBG_FUNC("OUT failed %d\n", ret);
	return PQI_STATUS_FAILURE;
}
