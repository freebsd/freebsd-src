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

/* Change this if need to debug why AIO is not being used */
#define DBG_AIO DBG_IO

#define SG_FLAG_LAST	0x40000000
#define SG_FLAG_CHAIN	0x80000000

/* Local Prototypes */
static void pqisrc_increment_io_counters(pqisrc_softstate_t *softs, rcb_t *rcb);
static int fill_lba_for_scsi_rw(pqisrc_softstate_t *softs, uint8_t *cdb, aio_req_locator_t *l);


/* Subroutine to find out embedded sgl count in IU */
static inline uint32_t
pqisrc_embedded_sgl_count(uint32_t elem_alloted, uint8_t iu_type)
{
	uint32_t embedded_sgl_count = MAX_EMBEDDED_SG_IN_FIRST_IU_DEFAULT;

	DBG_FUNC("IN\n");

	if (iu_type == PQI_IU_TYPE_RAID5_WRITE_BYPASS_REQUEST ||
		iu_type == PQI_IU_TYPE_RAID6_WRITE_BYPASS_REQUEST)
		embedded_sgl_count = MAX_EMBEDDED_SG_IN_FIRST_IU_RAID56_AIO;

	/**
	calculate embedded sgl count using num_elem_alloted for IO
	**/
	if(elem_alloted - 1)
		embedded_sgl_count += ((elem_alloted - 1) * MAX_EMBEDDED_SG_IN_IU);
	/* DBG_IO("embedded_sgl_count :%d\n", embedded_sgl_count); */

	DBG_FUNC("OUT\n");

	return embedded_sgl_count;

}

/* Subroutine to find out contiguous free elem in IU */
static inline uint32_t
pqisrc_contiguous_free_elem(uint32_t pi, uint32_t ci, uint32_t elem_in_q)
{
	uint32_t contiguous_free_elem = 0;

	DBG_FUNC("IN\n");

	if(pi >= ci) {
		contiguous_free_elem = (elem_in_q - pi);
		if(ci == 0)
			contiguous_free_elem -= 1;
	} else {
		contiguous_free_elem = (ci - pi - 1);
	}

	DBG_FUNC("OUT\n");

	return contiguous_free_elem;
}

/* Subroutine to find out num of elements need for the request */
static uint32_t
pqisrc_num_elem_needed(pqisrc_softstate_t *softs, uint32_t SG_Count,
                pqi_scsi_dev_t *devp, boolean_t is_write, IO_PATH_T io_path)
{
	uint32_t num_sg;
	uint32_t num_elem_required = 1;
	uint32_t sg_in_first_iu = MAX_EMBEDDED_SG_IN_FIRST_IU_DEFAULT;

	DBG_FUNC("IN\n");
	DBG_IO("SGL_Count :%u\n",SG_Count);

	if ((devp->raid_level == SA_RAID_5 || devp->raid_level == SA_RAID_6)
		&& is_write && (io_path == AIO_PATH))
		sg_in_first_iu = MAX_EMBEDDED_SG_IN_FIRST_IU_RAID56_AIO;
	/********
	If SG_Count greater than max sg per IU i.e 4 or 68
	(4 is with out spanning or 68 is with spanning) chaining is required.
	OR, If SG_Count <= MAX_EMBEDDED_SG_IN_FIRST_IU_* then,
	on these two cases one element is enough.
	********/
	if(SG_Count > softs->max_sg_per_spanning_cmd ||
		SG_Count <= sg_in_first_iu)
		return num_elem_required;
	/*
	SGL Count Other Than First IU
	 */
	num_sg = SG_Count - sg_in_first_iu;
	num_elem_required += PQISRC_DIV_ROUND_UP(num_sg, MAX_EMBEDDED_SG_IN_IU);
	DBG_FUNC("OUT\n");
	return num_elem_required;
}

/* Subroutine to build SG list for the IU submission*/
static boolean_t
pqisrc_build_sgl(sgt_t *sg_array, rcb_t *rcb, iu_header_t *iu_hdr,
			uint32_t num_elem_alloted)
{
	uint32_t i;
	uint32_t num_sg = OS_GET_IO_SG_COUNT(rcb);
	sgt_t *sgt = sg_array;
	sgt_t *sg_chain = NULL;
	boolean_t partial = false;

	DBG_FUNC("IN\n");

	/* DBG_IO("SGL_Count :%d",num_sg); */
	if (0 == num_sg) {
		goto out;
	}

	if (num_sg <= pqisrc_embedded_sgl_count(num_elem_alloted,
		iu_hdr->iu_type)) {

		for (i = 0; i < num_sg; i++, sgt++) {
			sgt->addr= OS_GET_IO_SG_ADDR(rcb,i);
			sgt->len= OS_GET_IO_SG_LEN(rcb,i);
			sgt->flags= 0;
		}

		sg_array[num_sg - 1].flags = SG_FLAG_LAST;
	} else {
	/**
	SGL Chaining
	**/
		sg_chain = rcb->sg_chain_virt;
		sgt->addr = rcb->sg_chain_dma;
		sgt->len = num_sg * sizeof(sgt_t);
		sgt->flags = SG_FLAG_CHAIN;

		sgt = sg_chain;
		for (i = 0; i < num_sg; i++, sgt++) {
			sgt->addr = OS_GET_IO_SG_ADDR(rcb,i);
			sgt->len = OS_GET_IO_SG_LEN(rcb,i);
			sgt->flags = 0;
		}

		sg_chain[num_sg - 1].flags = SG_FLAG_LAST;
		num_sg = 1;
		partial = true;

	}
out:
	iu_hdr->iu_length = num_sg * sizeof(sgt_t);
	DBG_FUNC("OUT\n");
	return partial;

}

#if 0
static inline void
pqisrc_show_raid_req(pqisrc_softstate_t *softs, pqisrc_raid_req_t *raid_req)
{
	DBG_IO("%30s: 0x%x\n", "raid_req->header.iu_type",
		raid_req->header.iu_type);
	DBG_IO("%30s: 0x%d\n", "raid_req->response_queue_id",
		raid_req->response_queue_id);
	DBG_IO("%30s: 0x%x\n", "raid_req->request_id",
		raid_req->request_id);
	DBG_IO("%30s: 0x%x\n", "raid_req->buffer_length",
		raid_req->buffer_length);
	DBG_IO("%30s: 0x%x\n", "raid_req->task_attribute",
		raid_req->task_attribute);
	DBG_IO("%30s: 0x%llx\n", "raid_req->lun_number",
		*((long long unsigned int*)raid_req->lun_number));
	DBG_IO("%30s: 0x%x\n", "raid_req->error_index",
		raid_req->error_index);
	DBG_IO("%30s: 0x%p\n", "raid_req->sg_descriptors[0].addr",
		(void *)raid_req->sg_descriptors[0].addr);
	DBG_IO("%30s: 0x%x\n", "raid_req->sg_descriptors[0].len",
		raid_req->sg_descriptors[0].len);
	DBG_IO("%30s: 0x%x\n", "raid_req->sg_descriptors[0].flags",
		raid_req->sg_descriptors[0].flags);
}
#endif

/*Subroutine used to Build the RAID request */
static void
pqisrc_build_raid_io(pqisrc_softstate_t *softs, rcb_t *rcb,
 	pqisrc_raid_req_t *raid_req, uint32_t num_elem_alloted)
{
	DBG_FUNC("IN\n");

	raid_req->header.iu_type = PQI_IU_TYPE_RAID_PATH_IO_REQUEST;
	raid_req->header.comp_feature = 0;
	raid_req->response_queue_id = OS_GET_IO_RESP_QID(softs, rcb);
	raid_req->work_area[0] = 0;
	raid_req->work_area[1] = 0;
	raid_req->request_id = rcb->tag;
	raid_req->nexus_id = 0;
	raid_req->buffer_length = GET_SCSI_BUFFLEN(rcb);
	memcpy(raid_req->lun_number, rcb->dvp->scsi3addr,
		sizeof(raid_req->lun_number));
	raid_req->protocol_spec = 0;
	raid_req->data_direction = rcb->data_dir;
	raid_req->reserved1 = 0;
	raid_req->fence = 0;
	raid_req->error_index = raid_req->request_id;
	raid_req->reserved2 = 0;
	raid_req->task_attribute = OS_GET_TASK_ATTR(rcb);
	raid_req->command_priority = 0;
	raid_req->reserved3 = 0;
	raid_req->reserved4 = 0;
	raid_req->reserved5 = 0;
	raid_req->ml_device_lun_number = (uint8_t)rcb->cm_ccb->ccb_h.target_lun;

	/* As cdb and additional_cdb_bytes are contiguous,
	   update them in a single statement */
	memcpy(raid_req->cmd.cdb, rcb->cdbp, rcb->cmdlen);
#if 0
	DBG_IO("CDB :");
	for(i = 0; i < rcb->cmdlen ; i++)
		DBG_IO(" 0x%x \n ",raid_req->cdb[i]);
#endif

	switch (rcb->cmdlen) {
		case 6:
		case 10:
		case 12:
		case 16:
			raid_req->additional_cdb_bytes_usage =
				PQI_ADDITIONAL_CDB_BYTES_0;
			break;
		case 20:
			raid_req->additional_cdb_bytes_usage =
				PQI_ADDITIONAL_CDB_BYTES_4;
			break;
		case 24:
			raid_req->additional_cdb_bytes_usage =
				PQI_ADDITIONAL_CDB_BYTES_8;
			break;
		case 28:
			raid_req->additional_cdb_bytes_usage =
				PQI_ADDITIONAL_CDB_BYTES_12;
			break;
		case 32:
		default: /* todo:review again */
			raid_req->additional_cdb_bytes_usage =
				PQI_ADDITIONAL_CDB_BYTES_16;
			break;
	}

	/* Frame SGL Descriptor */
	raid_req->partial = pqisrc_build_sgl(&raid_req->sg_descriptors[0], rcb,
		&raid_req->header, num_elem_alloted);

	raid_req->header.iu_length +=
			offsetof(pqisrc_raid_req_t, sg_descriptors) - sizeof(iu_header_t);

#if 0
	pqisrc_show_raid_req(softs, raid_req);
#endif
	rcb->success_cmp_callback = pqisrc_process_io_response_success;
	rcb->error_cmp_callback = pqisrc_process_raid_response_error;
	rcb->resp_qid = raid_req->response_queue_id;

	DBG_FUNC("OUT\n");

}

/* We will need to expand this to handle different types of
 * aio request structures.
 */
#if 0
static inline void
pqisrc_show_aio_req(pqisrc_softstate_t *softs, pqi_aio_req_t *aio_req)
{
	DBG_IO("%30s: 0x%x\n", "aio_req->header.iu_type",
		aio_req->header.iu_type);
	DBG_IO("%30s: 0x%x\n", "aio_req->resp_qid",
		aio_req->response_queue_id);
	DBG_IO("%30s: 0x%x\n", "aio_req->req_id",
		aio_req->req_id);
	DBG_IO("%30s: 0x%x\n", "aio_req->nexus",
		aio_req->nexus);
	DBG_IO("%30s: 0x%x\n", "aio_req->buf_len",
		aio_req->buf_len);
	DBG_IO("%30s: 0x%x\n", "aio_req->cmd_flags.data_dir",
		aio_req->cmd_flags.data_dir);
	DBG_IO("%30s: 0x%x\n", "aio_req->attr_prio.task_attr",
		aio_req->attr_prio.task_attr);
	DBG_IO("%30s: 0x%x\n", "aio_req->err_idx",
		aio_req->err_idx);
	DBG_IO("%30s: 0x%x\n", "aio_req->num_sg",
		aio_req->num_sg);
	DBG_IO("%30s: 0x%p\n", "aio_req->sg_desc[0].addr",
		(void *)aio_req->sg_desc[0].addr);
	DBG_IO("%30s: 0x%x\n", "aio_req->sg_desc[0].len",
		aio_req->sg_desc[0].len);
	DBG_IO("%30s: 0x%x\n", "aio_req->sg_desc[0].flags",
		aio_req->sg_desc[0].flags);
}
#endif

void
int_to_scsilun(uint64_t lun, uint8_t *scsi_lun)
{
   int i;

	memset(scsi_lun, 0, sizeof(lun));
        for (i = 0; i < sizeof(lun); i += 2) {
                scsi_lun[i] = (lun >> 8) & 0xFF;
                scsi_lun[i+1] = lun & 0xFF;
                lun = lun >> 16;
        }
}


/*Subroutine used to populate AIO IUs. */
void
pqisrc_build_aio_common(pqisrc_softstate_t *softs, pqi_aio_req_t *aio_req,
                        rcb_t *rcb, uint32_t num_elem_alloted)
{
	DBG_FUNC("IN\n");
	aio_req->header.iu_type = PQI_IU_TYPE_AIO_PATH_IO_REQUEST;
	aio_req->header.comp_feature = 0;
	aio_req->response_queue_id = OS_GET_IO_RESP_QID(softs, rcb);
	aio_req->work_area[0] = 0;
	aio_req->work_area[1] = 0;
	aio_req->req_id = rcb->tag;
	aio_req->res1[0] = 0;
	aio_req->res1[1] = 0;
	aio_req->nexus = rcb->ioaccel_handle;
	aio_req->buf_len = GET_SCSI_BUFFLEN(rcb);
	aio_req->cmd_flags.data_dir = rcb->data_dir;
	aio_req->cmd_flags.mem_type = 0;
	aio_req->cmd_flags.fence = 0;
	aio_req->cmd_flags.res2 = 0;
	aio_req->attr_prio.task_attr = OS_GET_TASK_ATTR(rcb);
	aio_req->attr_prio.cmd_prio = 0;
	aio_req->attr_prio.res3 = 0;
	aio_req->err_idx = aio_req->req_id;
	aio_req->cdb_len = rcb->cmdlen;

	if (rcb->cmdlen > sizeof(aio_req->cdb))
		rcb->cmdlen = sizeof(aio_req->cdb);
	memcpy(aio_req->cdb, rcb->cdbp, rcb->cmdlen);
	memset(aio_req->res4, 0, sizeof(aio_req->res4));

	uint64_t lun = rcb->cm_ccb->ccb_h.target_lun;
	if (lun && (rcb->dvp->is_multi_lun)) {
		int_to_scsilun(lun, aio_req->lun);
	}
	else {
		memset(aio_req->lun, 0, sizeof(aio_req->lun));
	}

	/* handle encryption fields */
	if (rcb->encrypt_enable == true) {
		aio_req->cmd_flags.encrypt_enable = true;
		aio_req->encrypt_key_index =
			LE_16(rcb->enc_info.data_enc_key_index);
		aio_req->encrypt_twk_low =
			LE_32(rcb->enc_info.encrypt_tweak_lower);
		aio_req->encrypt_twk_high =
			LE_32(rcb->enc_info.encrypt_tweak_upper);
	} else {
		aio_req->cmd_flags.encrypt_enable = 0;
		aio_req->encrypt_key_index = 0;
		aio_req->encrypt_twk_high = 0;
		aio_req->encrypt_twk_low = 0;
	}
	/* Frame SGL Descriptor */
	aio_req->cmd_flags.partial = pqisrc_build_sgl(&aio_req->sg_desc[0], rcb,
		&aio_req->header, num_elem_alloted);

	aio_req->num_sg = aio_req->header.iu_length / sizeof(sgt_t);

	/* DBG_INFO("aio_req->num_sg :%d\n", aio_req->num_sg); */

	aio_req->header.iu_length += offsetof(pqi_aio_req_t, sg_desc) -
		sizeof(iu_header_t);
	/* set completion and error handlers. */
	rcb->success_cmp_callback = pqisrc_process_io_response_success;
	rcb->error_cmp_callback = pqisrc_process_aio_response_error;
	rcb->resp_qid = aio_req->response_queue_id;
	DBG_FUNC("OUT\n");

}
/*Subroutine used to show standard AIO IU fields */
void
pqisrc_show_aio_common(pqisrc_softstate_t *softs, rcb_t *rcb,
                       pqi_aio_req_t *aio_req)
{
#ifdef DEBUG_AIO
	DBG_INFO("AIO IU Content, tag# 0x%08x", rcb->tag);
	DBG_INFO("%15s: 0x%x\n", "iu_type",	aio_req->header.iu_type);
	DBG_INFO("%15s: 0x%x\n", "comp_feat",	aio_req->header.comp_feature);
	DBG_INFO("%15s: 0x%x\n", "length",	aio_req->header.iu_length);
	DBG_INFO("%15s: 0x%x\n", "resp_qid",	aio_req->response_queue_id);
	DBG_INFO("%15s: 0x%x\n", "req_id",	aio_req->req_id);
	DBG_INFO("%15s: 0x%x\n", "nexus",	aio_req->nexus);
	DBG_INFO("%15s: 0x%x\n", "buf_len",	aio_req->buf_len);
	DBG_INFO("%15s:\n", "cmd_flags");
	DBG_INFO("%15s: 0x%x\n", "data_dir",	aio_req->cmd_flags.data_dir);
	DBG_INFO("%15s: 0x%x\n", "partial",	aio_req->cmd_flags.partial);
	DBG_INFO("%15s: 0x%x\n", "mem_type",	aio_req->cmd_flags.mem_type);
	DBG_INFO("%15s: 0x%x\n", "fence",	aio_req->cmd_flags.fence);
	DBG_INFO("%15s: 0x%x\n", "encryption",
		aio_req->cmd_flags.encrypt_enable);
	DBG_INFO("%15s:\n", "attr_prio");
	DBG_INFO("%15s: 0x%x\n", "task_attr",	aio_req->attr_prio.task_attr);
	DBG_INFO("%15s: 0x%x\n", "cmd_prio",	aio_req->attr_prio.cmd_prio);
	DBG_INFO("%15s: 0x%x\n", "dek_index",	aio_req->encrypt_key_index);
	DBG_INFO("%15s: 0x%x\n", "tweak_lower",	aio_req->encrypt_twk_low);
	DBG_INFO("%15s: 0x%x\n", "tweak_upper",	aio_req->encrypt_twk_high);
	pqisrc_show_cdb(softs, "AIOC", rcb, aio_req->cdb);
	DBG_INFO("%15s: 0x%x\n", "err_idx",	aio_req->err_idx);
	DBG_INFO("%15s: 0x%x\n", "num_sg",	aio_req->num_sg);
	DBG_INFO("%15s: 0x%x\n", "cdb_len",	aio_req->cdb_len);
#if 0
	DBG_INFO("%15s: 0x%x\n", "lun",		aio_req->lun);
	DBG_INFO("%15s: 0x%p\n", "sg_desc[0].addr",
		(void *)aio_req->sg_desc[0].addr);
	DBG_INFO("%15s: 0x%x\n", "sg_desc[0].len",
		aio_req->sg_desc[0].len);
	DBG_INFO("%15s: 0x%x\n", "sg_desc[0].flags",
		aio_req->sg_desc[0].flags);
#endif
#endif /* DEBUG_AIO */
}

/*Subroutine used to populate AIO RAID 1 write bypass IU. */
void
pqisrc_build_aio_R1_write(pqisrc_softstate_t *softs,
	pqi_aio_raid1_write_req_t *aio_req, rcb_t *rcb,
	uint32_t num_elem_alloted)
{
	DBG_FUNC("IN\n");
	if (!rcb->dvp) {
		DBG_WARN("%s: DEBUG: dev ptr is null", __func__);
		return;
	}
	if (!rcb->dvp->raid_map) {
		DBG_WARN("%s: DEBUG: raid_map is null", __func__);
		return;
	}

	aio_req->header.iu_type = PQI_IU_TYPE_RAID1_WRITE_BYPASS_REQUEST;
	aio_req->header.comp_feature = 0;
	aio_req->response_queue_id = OS_GET_IO_RESP_QID(softs, rcb);
	aio_req->work_area[0] = 0;
	aio_req->work_area[1] = 0;
	aio_req->req_id = rcb->tag;
	aio_req->volume_id =  (LE_32(rcb->dvp->scsi3addr[0]) & 0x3FFF);
	aio_req->nexus_1 = rcb->it_nexus[0];
	aio_req->nexus_2 = rcb->it_nexus[1];
	aio_req->nexus_3 = rcb->it_nexus[2];
	aio_req->buf_len = GET_SCSI_BUFFLEN(rcb);
	aio_req->cmd_flags.data_dir = rcb->data_dir;
	aio_req->cmd_flags.mem_type = 0;
	aio_req->cmd_flags.fence = 0;
	aio_req->cmd_flags.res2 = 0;
	aio_req->attr_prio.task_attr = OS_GET_TASK_ATTR(rcb);
	aio_req->attr_prio.cmd_prio = 0;
	aio_req->attr_prio.res3 = 0;
	if(rcb->cmdlen > sizeof(aio_req->cdb))
		rcb->cmdlen = sizeof(aio_req->cdb);
	memcpy(aio_req->cdb, rcb->cdbp, rcb->cmdlen);
	aio_req->err_idx = aio_req->req_id;
	aio_req->cdb_len = rcb->cmdlen;
	aio_req->num_drives = LE_16(rcb->dvp->raid_map->layout_map_count);

	/* handle encryption fields */
	if (rcb->encrypt_enable == true) {
		aio_req->cmd_flags.encrypt_enable = true;
		aio_req->encrypt_key_index =
			LE_16(rcb->enc_info.data_enc_key_index);
		aio_req->encrypt_twk_low =
			LE_32(rcb->enc_info.encrypt_tweak_lower);
		aio_req->encrypt_twk_high =
			LE_32(rcb->enc_info.encrypt_tweak_upper);
	} else {
		aio_req->cmd_flags.encrypt_enable = 0;
		aio_req->encrypt_key_index = 0;
		aio_req->encrypt_twk_high = 0;
		aio_req->encrypt_twk_low = 0;
	}
	/* Frame SGL Descriptor */
	aio_req->cmd_flags.partial = pqisrc_build_sgl(&aio_req->sg_desc[0], rcb,
		&aio_req->header, num_elem_alloted);

	aio_req->num_sg = aio_req->header.iu_length / sizeof(sgt_t);

	/* DBG_INFO("aio_req->num_sg :%d\n", aio_req->num_sg); */

	aio_req->header.iu_length += offsetof(pqi_aio_raid1_write_req_t, sg_desc) -
		sizeof(iu_header_t);

	/* set completion and error handlers. */
	rcb->success_cmp_callback = pqisrc_process_io_response_success;
	rcb->error_cmp_callback = pqisrc_process_aio_response_error;
	rcb->resp_qid = aio_req->response_queue_id;
	DBG_FUNC("OUT\n");

}

/*Subroutine used to show AIO RAID1 Write bypass IU fields */
void
pqisrc_show_aio_R1_write(pqisrc_softstate_t *softs, rcb_t *rcb,
	pqi_aio_raid1_write_req_t *aio_req)
{

#ifdef DEBUG_AIO
	DBG_INFO("AIO RAID1 Write IU Content, tag# 0x%08x", rcb->tag);
	DBG_INFO("%15s: 0x%x\n", "iu_type",	aio_req->header.iu_type);
	DBG_INFO("%15s: 0x%x\n", "comp_feat",	aio_req->header.comp_feature);
	DBG_INFO("%15s: 0x%x\n", "length",	aio_req->header.iu_length);
	DBG_INFO("%15s: 0x%x\n", "resp_qid",	aio_req->response_queue_id);
	DBG_INFO("%15s: 0x%x\n", "req_id",	aio_req->req_id);
	DBG_INFO("%15s: 0x%x\n", "volume_id",	aio_req->volume_id);
	DBG_INFO("%15s: 0x%x\n", "nexus_1",	aio_req->nexus_1);
	DBG_INFO("%15s: 0x%x\n", "nexus_2",	aio_req->nexus_2);
	DBG_INFO("%15s: 0x%x\n", "nexus_3",	aio_req->nexus_3);
	DBG_INFO("%15s: 0x%x\n", "buf_len",	aio_req->buf_len);
	DBG_INFO("%15s:\n", "cmd_flags");
	DBG_INFO("%15s: 0x%x\n", "data_dir",	aio_req->cmd_flags.data_dir);
	DBG_INFO("%15s: 0x%x\n", "partial",	aio_req->cmd_flags.partial);
	DBG_INFO("%15s: 0x%x\n", "mem_type",	aio_req->cmd_flags.mem_type);
	DBG_INFO("%15s: 0x%x\n", "fence",	aio_req->cmd_flags.fence);
	DBG_INFO("%15s: 0x%x\n", "encryption",
		aio_req->cmd_flags.encrypt_enable);
	DBG_INFO("%15s:\n", "attr_prio");
	DBG_INFO("%15s: 0x%x\n", "task_attr",	aio_req->attr_prio.task_attr);
	DBG_INFO("%15s: 0x%x\n", "cmd_prio",	aio_req->attr_prio.cmd_prio);
	DBG_INFO("%15s: 0x%x\n", "dek_index",	aio_req->encrypt_key_index);
	pqisrc_show_cdb(softs, "AIOR1W", rcb, aio_req->cdb);
	DBG_INFO("%15s: 0x%x\n", "err_idx",	aio_req->err_idx);
	DBG_INFO("%15s: 0x%x\n", "num_sg",	aio_req->num_sg);
	DBG_INFO("%15s: 0x%x\n", "cdb_len",	aio_req->cdb_len);
	DBG_INFO("%15s: 0x%x\n", "num_drives",	aio_req->num_drives);
	DBG_INFO("%15s: 0x%x\n", "tweak_lower",	aio_req->encrypt_twk_low);
	DBG_INFO("%15s: 0x%x\n", "tweak_upper",	aio_req->encrypt_twk_high);
#if 0
	DBG_INFO("%15s: 0x%p\n", "sg_desc[0].addr",
		(void *)aio_req->sg_desc[0].addr);
	DBG_INFO("%15s: 0x%x\n", "sg_desc[0].len",
		aio_req->sg_desc[0].len);
	DBG_INFO("%15s: 0x%x\n", "sg_desc[0].flags",
		aio_req->sg_desc[0].flags);
#endif
#endif /* DEBUG_AIO */
}

/*Subroutine used to populate AIO Raid5 or 6 write bypass IU */
void
pqisrc_build_aio_R5or6_write(pqisrc_softstate_t *softs,
	pqi_aio_raid5or6_write_req_t *aio_req, rcb_t *rcb,
	uint32_t num_elem_alloted)
{
	DBG_FUNC("IN\n");
	uint32_t index;
	unsigned num_data_disks;
	unsigned num_metadata_disks;
	unsigned total_disks;
	num_data_disks = LE_16(rcb->dvp->raid_map->data_disks_per_row);
	num_metadata_disks = LE_16(rcb->dvp->raid_map->metadata_disks_per_row);
	total_disks = num_data_disks + num_metadata_disks;

	index = PQISRC_DIV_ROUND_UP(rcb->raid_map_index + 1, total_disks);
	index *= total_disks;
	index -= num_metadata_disks;

	switch (rcb->dvp->raid_level) {
	case SA_RAID_5:
		aio_req->header.iu_type =
		PQI_IU_TYPE_RAID5_WRITE_BYPASS_REQUEST;
		break;
	case SA_RAID_6:
		aio_req->header.iu_type =
		PQI_IU_TYPE_RAID6_WRITE_BYPASS_REQUEST;
		break;
	default:
		DBG_ERR("WRONG RAID TYPE FOR FUNCTION\n");
	}
	aio_req->header.comp_feature = 0;
	aio_req->response_queue_id = OS_GET_IO_RESP_QID(softs, rcb);
	aio_req->work_area[0] = 0;
	aio_req->work_area[1] = 0;
	aio_req->req_id = rcb->tag;
	aio_req->volume_id =  (LE_32(rcb->dvp->scsi3addr[0]) & 0x3FFF);
	aio_req->data_it_nexus = rcb->dvp->raid_map->dev_data[rcb->raid_map_index].ioaccel_handle;
	aio_req->p_parity_it_nexus =
		rcb->dvp->raid_map->dev_data[index].ioaccel_handle;
	if (aio_req->header.iu_type ==
		PQI_IU_TYPE_RAID6_WRITE_BYPASS_REQUEST) {
			aio_req->q_parity_it_nexus =
				rcb->dvp->raid_map->dev_data[index + 1].ioaccel_handle;
	}
	aio_req->xor_multiplier =
		rcb->dvp->raid_map->dev_data[rcb->raid_map_index].xor_mult[1];
	aio_req->row = rcb->row_num;
	/*aio_req->reserved = rcb->row_num * rcb->blocks_per_row +
		rcb->dvp->raid_map->disk_starting_blk;*/
	aio_req->buf_len = GET_SCSI_BUFFLEN(rcb);
	aio_req->cmd_flags.data_dir = rcb->data_dir;
	aio_req->cmd_flags.mem_type = 0;
	aio_req->cmd_flags.fence = 0;
	aio_req->cmd_flags.res2 = 0;
	aio_req->attr_prio.task_attr = OS_GET_TASK_ATTR(rcb);
	aio_req->attr_prio.cmd_prio = 0;
	aio_req->attr_prio.res3 = 0;
	if (rcb->cmdlen > sizeof(aio_req->cdb))
		rcb->cmdlen = sizeof(aio_req->cdb);
	memcpy(aio_req->cdb, rcb->cdbp, rcb->cmdlen);
	aio_req->err_idx = aio_req->req_id;
	aio_req->cdb_len = rcb->cmdlen;
#if 0
	/* Stubbed out for later */
	aio_req->header.iu_type = iu_type;
	aio_req->data_it_nexus = ;
	aio_req->p_parity_it_nexus = ;
	aio_req->q_parity_it_nexus = ;
	aio_req->row = ;
	aio_req->stripe_lba = ;
#endif
	/* handle encryption fields */
	if (rcb->encrypt_enable == true) {
		aio_req->cmd_flags.encrypt_enable = true;
		aio_req->encrypt_key_index =
			LE_16(rcb->enc_info.data_enc_key_index);
		aio_req->encrypt_twk_low =
			LE_32(rcb->enc_info.encrypt_tweak_lower);
		aio_req->encrypt_twk_high =
			LE_32(rcb->enc_info.encrypt_tweak_upper);
	} else {
		aio_req->cmd_flags.encrypt_enable = 0;
		aio_req->encrypt_key_index = 0;
		aio_req->encrypt_twk_high = 0;
		aio_req->encrypt_twk_low = 0;
	}
	/* Frame SGL Descriptor */
	aio_req->cmd_flags.partial = pqisrc_build_sgl(&aio_req->sg_desc[0], rcb,
		&aio_req->header, num_elem_alloted);

	aio_req->num_sg = aio_req->header.iu_length / sizeof(sgt_t);

	/* DBG_INFO("aio_req->num_sg :%d\n", aio_req->num_sg); */

	aio_req->header.iu_length += offsetof(pqi_aio_raid5or6_write_req_t, sg_desc) -
		sizeof(iu_header_t);
	/* set completion and error handlers. */
	rcb->success_cmp_callback = pqisrc_process_io_response_success;
	rcb->error_cmp_callback = pqisrc_process_aio_response_error;
	rcb->resp_qid = aio_req->response_queue_id;
	DBG_FUNC("OUT\n");

}

/*Subroutine used to show AIO RAID5/6 Write bypass IU fields */
void
pqisrc_show_aio_R5or6_write(pqisrc_softstate_t *softs, rcb_t *rcb,
	pqi_aio_raid5or6_write_req_t *aio_req)
{
#ifdef DEBUG_AIO
	DBG_INFO("AIO RAID5or6 Write IU Content, tag# 0x%08x\n", rcb->tag);
	DBG_INFO("%15s: 0x%x\n", "iu_type",	aio_req->header.iu_type);
	DBG_INFO("%15s: 0x%x\n", "comp_feat",	aio_req->header.comp_feature);
	DBG_INFO("%15s: 0x%x\n", "length",	aio_req->header.iu_length);
	DBG_INFO("%15s: 0x%x\n", "resp_qid",	aio_req->response_queue_id);
	DBG_INFO("%15s: 0x%x\n", "req_id",	aio_req->req_id);
	DBG_INFO("%15s: 0x%x\n", "volume_id",	aio_req->volume_id);
	DBG_INFO("%15s: 0x%x\n", "data_it_nexus",
		aio_req->data_it_nexus);
	DBG_INFO("%15s: 0x%x\n", "p_parity_it_nexus",
		aio_req->p_parity_it_nexus);
	DBG_INFO("%15s: 0x%x\n", "q_parity_it_nexus",
		aio_req->q_parity_it_nexus);
	DBG_INFO("%15s: 0x%x\n", "buf_len",	aio_req->buf_len);
	DBG_INFO("%15s:\n", "cmd_flags");
	DBG_INFO("%15s: 0x%x\n", "data_dir",	aio_req->cmd_flags.data_dir);
	DBG_INFO("%15s: 0x%x\n", "partial",	aio_req->cmd_flags.partial);
	DBG_INFO("%15s: 0x%x\n", "mem_type",	aio_req->cmd_flags.mem_type);
	DBG_INFO("%15s: 0x%x\n", "fence",	aio_req->cmd_flags.fence);
	DBG_INFO("%15s: 0x%x\n", "encryption",
		aio_req->cmd_flags.encrypt_enable);
	DBG_INFO("%15s:\n", "attr_prio");
	DBG_INFO("%15s: 0x%x\n", "task_attr",	aio_req->attr_prio.task_attr);
	DBG_INFO("%15s: 0x%x\n", "cmd_prio",	aio_req->attr_prio.cmd_prio);
	DBG_INFO("%15s: 0x%x\n", "dek_index",	aio_req->encrypt_key_index);
	pqisrc_show_cdb(softs, "AIOR56W", rcb, aio_req->cdb);
	DBG_INFO("%15s: 0x%x\n", "err_idx",	aio_req->err_idx);
	DBG_INFO("%15s: 0x%x\n", "num_sg",	aio_req->num_sg);
	DBG_INFO("%15s: 0x%x\n", "cdb_len",	aio_req->cdb_len);
	DBG_INFO("%15s: 0x%x\n", "tweak_lower",	aio_req->encrypt_twk_low);
	DBG_INFO("%15s: 0x%x\n", "tweak_upper",	aio_req->encrypt_twk_high);
	DBG_INFO("%15s: 0x%lx\n", "row",	aio_req->row);
#if 0
	DBG_INFO("%15s: 0x%lx\n", "stripe_lba",	aio_req->stripe_lba);
	DBG_INFO("%15s: 0x%p\n", "sg_desc[0].addr",
		(void *)aio_req->sg_desc[0].addr);
	DBG_INFO("%15s: 0x%x\n", "sg_desc[0].len",
		aio_req->sg_desc[0].len);
	DBG_INFO("%15s: 0x%x\n", "sg_desc[0].flags",
		aio_req->sg_desc[0].flags);
#endif
#endif /* DEBUG_AIO */
}

/* Is the cdb a read command? */
boolean_t
pqisrc_cdb_is_read(uint8_t *cdb)
{
	if (cdb[0] == SCMD_READ_6 || cdb[0] == SCMD_READ_10 ||
		cdb[0] == SCMD_READ_12 || cdb[0] == SCMD_READ_16)
		return true;
	return false;
}

/* Is the cdb a write command? */
boolean_t
pqisrc_cdb_is_write(uint8_t *cdb)
{
	if (cdb == NULL)
		return false;

	if (cdb[0] == SCMD_WRITE_6 || cdb[0] == SCMD_WRITE_10 ||
		cdb[0] == SCMD_WRITE_12 || cdb[0] == SCMD_WRITE_16)
		return true;
	return false;
}

/*Subroutine used to show the AIO request */
void
pqisrc_show_aio_io(pqisrc_softstate_t *softs, rcb_t *rcb,
	pqi_aio_req_t *aio_req, uint32_t num_elem_alloted)
{
	boolean_t is_write;
	DBG_FUNC("IN\n");

	is_write = pqisrc_cdb_is_write(rcb->cdbp);

	if (!is_write) {
		pqisrc_show_aio_common(softs, rcb, aio_req);
		goto out;
	}

	switch (rcb->dvp->raid_level) {
	case SA_RAID_0:
		pqisrc_show_aio_common(softs, rcb, aio_req);
		break;
	case SA_RAID_1:
	case SA_RAID_ADM:
		pqisrc_show_aio_R1_write(softs, rcb,
			(pqi_aio_raid1_write_req_t *)aio_req);
		break;
	case SA_RAID_5:
	case SA_RAID_6:
		pqisrc_show_aio_R5or6_write(softs, rcb,
			(pqi_aio_raid5or6_write_req_t *)aio_req);
		break;
	}

out:
	DBG_FUNC("OUT\n");

}


void
pqisrc_build_aio_io(pqisrc_softstate_t *softs, rcb_t *rcb,
	pqi_aio_req_t *aio_req, uint32_t num_elem_alloted)
{
	boolean_t is_write;
	DBG_FUNC("IN\n");

	is_write = pqisrc_cdb_is_write(rcb->cdbp);

	if (is_write) {
		switch (rcb->dvp->raid_level) {
		case SA_RAID_0:
			pqisrc_build_aio_common(softs, aio_req,
				rcb, num_elem_alloted);
			break;
		case SA_RAID_1:
		case SA_RAID_ADM:
			pqisrc_build_aio_R1_write(softs,
				(pqi_aio_raid1_write_req_t *)aio_req,
				rcb, num_elem_alloted);

			break;
		case SA_RAID_5:
		case SA_RAID_6:
			pqisrc_build_aio_R5or6_write(softs,
				(pqi_aio_raid5or6_write_req_t *)aio_req,
				rcb, num_elem_alloted);
			break;
		}
	} else {
		pqisrc_build_aio_common(softs, aio_req, rcb, num_elem_alloted);
	}

	pqisrc_show_aio_io(softs, rcb, aio_req, num_elem_alloted);

	DBG_FUNC("OUT\n");
}

/*
 *	Return true from this function to prevent AIO from handling this request.
 *	True is returned if the request is determined to be part of a stream, or
 *	if the controller does not handle AIO at the appropriate RAID level.
 */
static boolean_t
pqisrc_is_parity_write_stream(pqisrc_softstate_t *softs, rcb_t *rcb)
{
	os_ticks_t oldest_ticks;
	uint8_t lru_index;
	int i;
	int rc;
	pqi_scsi_dev_t *device;
	struct pqi_stream_data *pqi_stream_data;
	aio_req_locator_t loc;

	DBG_FUNC("IN\n");

	rc = fill_lba_for_scsi_rw(softs, rcb->cdbp , &loc);
	if (rc != PQI_STATUS_SUCCESS) {
		return false;
	}

	/* check writes only */
	if (!pqisrc_cdb_is_write(rcb->cdbp)) {
	    return false;
	}

	if (!softs->enable_stream_detection) {
		return false;
	}

	device = rcb->dvp;
	if (!device) {
		return false;
	}

	/*
	 * check for R5/R6 streams.
	 */
	if (device->raid_level != SA_RAID_5 && device->raid_level != SA_RAID_6) {
		return false;
	}

	/*
	 * If controller does not support AIO R{5,6} writes, need to send
	 * requests down non-aio path.
	 */
	if ((device->raid_level == SA_RAID_5 && !softs->aio_raid5_write_bypass) ||
		(device->raid_level == SA_RAID_6 && !softs->aio_raid6_write_bypass)) {
		return true;
	}

	lru_index = 0;
	oldest_ticks = INT_MAX;
	for (i = 0; i < NUM_STREAMS_PER_LUN; i++) {
		pqi_stream_data = &device->stream_data[i];
		/*
		 * check for adjacent request or request is within
		 * the previous request.
		 */
		if ((pqi_stream_data->next_lba &&
			loc.block.first >= pqi_stream_data->next_lba) &&
			loc.block.first <= pqi_stream_data->next_lba +
				loc.block.cnt) {
			pqi_stream_data->next_lba = loc.block.first +
				loc.block.cnt;
			pqi_stream_data->last_accessed = TICKS;
			return true;
		}

		/* unused entry */
		if (pqi_stream_data->last_accessed == 0) {
			lru_index = i;
			break;
		}

		/* Find entry with oldest last accessed time */
		if (pqi_stream_data->last_accessed <= oldest_ticks) {
			oldest_ticks = pqi_stream_data->last_accessed;
			lru_index = i;
		}
	}

	/*
	 * Set LRU entry
	 */
	pqi_stream_data = &device->stream_data[lru_index];
	pqi_stream_data->last_accessed = TICKS;
	pqi_stream_data->next_lba = loc.block.first + loc.block.cnt;

	DBG_FUNC("OUT\n");

	return false;
}

/**
 Determine if a request is eligible for AIO.  Build/map
 the request if using AIO path to a RAID volume.

 return the path that should be used for this request
*/
static IO_PATH_T
determine_io_path_build_bypass(pqisrc_softstate_t *softs,rcb_t *rcb)
{
	IO_PATH_T io_path = AIO_PATH;
	pqi_scsi_dev_t *devp = rcb->dvp;
	int ret = PQI_STATUS_FAILURE;

	/* Default to using the host CDB directly (will be used if targeting RAID
		path or HBA mode */
	rcb->cdbp = OS_GET_CDBP(rcb);

	if(!rcb->aio_retry) {

		/**  IO for Physical Drive, Send in AIO PATH **/
		if(IS_AIO_PATH(devp)) {
			rcb->ioaccel_handle = devp->ioaccel_handle;
			return io_path;
		}

		/** IO for RAID Volume, ByPass IO, Send in AIO PATH unless part of stream **/
		if (devp->offload_enabled && !pqisrc_is_parity_write_stream(softs, rcb)) {
			ret = pqisrc_build_scsi_cmd_raidbypass(softs, devp, rcb);
		}

		if (PQI_STATUS_FAILURE == ret) {
			io_path = RAID_PATH;
		} else {
			ASSERT(rcb->cdbp == rcb->bypass_cdb);
		}
	} else {
		/* Retrying failed AIO IO */
		io_path = RAID_PATH;
	}

	return io_path;
}

uint8_t
pqisrc_get_aio_data_direction(rcb_t *rcb)
{
        switch (rcb->cm_ccb->ccb_h.flags & CAM_DIR_MASK) {
        case CAM_DIR_IN:  	return SOP_DATA_DIR_FROM_DEVICE;
        case CAM_DIR_OUT:   	return SOP_DATA_DIR_TO_DEVICE;
        case CAM_DIR_NONE:  	return SOP_DATA_DIR_NONE;
        default:		return SOP_DATA_DIR_UNKNOWN;
        }
}

uint8_t
pqisrc_get_raid_data_direction(rcb_t *rcb)
{
        switch (rcb->cm_ccb->ccb_h.flags & CAM_DIR_MASK) {
        case CAM_DIR_IN:  	return SOP_DATA_DIR_TO_DEVICE;
        case CAM_DIR_OUT:   	return SOP_DATA_DIR_FROM_DEVICE;
        case CAM_DIR_NONE:  	return SOP_DATA_DIR_NONE;
        default:		return SOP_DATA_DIR_UNKNOWN;
        }
}

/* Function used to build and send RAID/AIO */
int
pqisrc_build_send_io(pqisrc_softstate_t *softs,rcb_t *rcb)
{
	ib_queue_t *ib_q_array = softs->op_aio_ib_q;
	ib_queue_t *ib_q = NULL;
	char *ib_iu = NULL;
	IO_PATH_T io_path;
	uint32_t TraverseCount = 0;
	int first_qindex = OS_GET_IO_REQ_QINDEX(softs, rcb);
	int qindex = first_qindex;
	uint32_t num_op_ib_q = softs->num_op_aio_ibq;
	uint32_t num_elem_needed;
	uint32_t num_elem_alloted = 0;
	pqi_scsi_dev_t *devp = rcb->dvp;
	boolean_t is_write;

	DBG_FUNC("IN\n");

	/* Note: this will determine if the request is eligble for AIO */
	io_path = determine_io_path_build_bypass(softs, rcb);

	if (io_path == RAID_PATH)
	{
		/* Update direction for RAID path */
		rcb->data_dir = pqisrc_get_raid_data_direction(rcb);
		num_op_ib_q = softs->num_op_raid_ibq;
		ib_q_array = softs->op_raid_ib_q;
	}
	else {
		rcb->data_dir = pqisrc_get_aio_data_direction(rcb);
		if (rcb->data_dir == SOP_DATA_DIR_UNKNOWN) {
			DBG_ERR("Unknown Direction\n");
		}
	}

	is_write = pqisrc_cdb_is_write(rcb->cdbp);
	/* coverity[unchecked_value] */
	num_elem_needed = pqisrc_num_elem_needed(softs,
		OS_GET_IO_SG_COUNT(rcb), devp, is_write, io_path);
	DBG_IO("num_elem_needed :%u",num_elem_needed);

	do {
		uint32_t num_elem_available;
		ib_q = (ib_q_array + qindex);
		PQI_LOCK(&ib_q->lock);
		num_elem_available = pqisrc_contiguous_free_elem(ib_q->pi_local,
					*(ib_q->ci_virt_addr), ib_q->num_elem);

		DBG_IO("num_elem_avialable :%u\n",num_elem_available);
		if(num_elem_available >= num_elem_needed) {
			num_elem_alloted = num_elem_needed;
			break;
		}
		DBG_IO("Current queue is busy! Hop to next queue\n");

		PQI_UNLOCK(&ib_q->lock);
		qindex = (qindex + 1) % num_op_ib_q;
		if(qindex == first_qindex) {
			if (num_elem_needed == 1)
				break;
			TraverseCount += 1;
			num_elem_needed = 1;
		}
	}while(TraverseCount < 2);

	DBG_IO("num_elem_alloted :%u",num_elem_alloted);
	if (num_elem_alloted == 0) {
		DBG_WARN("OUT: IB Queues were full\n");
		return PQI_STATUS_QFULL;
	}

	pqisrc_increment_device_active_io(softs,devp);

	/* Get IB Queue Slot address to build IU */
	ib_iu = ib_q->array_virt_addr + (ib_q->pi_local * ib_q->elem_size);

	if(io_path == AIO_PATH) {
		/* Fill in the AIO IU per request and raid type */
		pqisrc_build_aio_io(softs, rcb, (pqi_aio_req_t *)ib_iu,
			num_elem_alloted);
	} else {
		/** Build RAID structure **/
		pqisrc_build_raid_io(softs, rcb, (pqisrc_raid_req_t *)ib_iu,
			num_elem_alloted);
	}

	rcb->req_pending = true;
	rcb->req_q = ib_q;
	rcb->path = io_path;

	pqisrc_increment_io_counters(softs, rcb);

	/* Update the local PI */
	ib_q->pi_local = (ib_q->pi_local + num_elem_alloted) % ib_q->num_elem;

	DBG_IO("ib_q->pi_local : %x\n", ib_q->pi_local);
	DBG_IO("*ib_q->ci_virt_addr: %x\n",*(ib_q->ci_virt_addr));

	/* Inform the fw about the new IU */
	PCI_MEM_PUT32(softs, ib_q->pi_register_abs, ib_q->pi_register_offset, ib_q->pi_local);

	PQI_UNLOCK(&ib_q->lock);
	DBG_FUNC("OUT\n");
	return PQI_STATUS_SUCCESS;
}

/* Subroutine used to set encryption info as part of RAID bypass IO*/
static inline void
pqisrc_set_enc_info(struct pqi_enc_info *enc_info,
		struct raid_map *raid_map, uint64_t first_block)
{
	uint32_t volume_blk_size;

	/*
	 * Set the encryption tweak values based on logical block address.
	 * If the block size is 512, the tweak value is equal to the LBA.
	 * For other block sizes, tweak value is (LBA * block size) / 512.
	 */
	volume_blk_size = GET_LE32((uint8_t *)&raid_map->volume_blk_size);
	if (volume_blk_size != 512)
		first_block = (first_block * volume_blk_size) / 512;

	enc_info->data_enc_key_index =
		GET_LE16((uint8_t *)&raid_map->data_encryption_key_index);
	enc_info->encrypt_tweak_upper = ((uint32_t)(((first_block) >> 16) >> 16));
	enc_info->encrypt_tweak_lower = ((uint32_t)(first_block));
}


/*
 * Attempt to perform offload RAID mapping for a logical volume I/O.
 */

#define HPSA_RAID_0		0
#define HPSA_RAID_4		1
#define HPSA_RAID_1		2	/* also used for RAID 10 */
#define HPSA_RAID_5		3	/* also used for RAID 50 */
#define HPSA_RAID_51		4
#define HPSA_RAID_6		5	/* also used for RAID 60 */
#define HPSA_RAID_ADM		6	/* also used for RAID 1+0 ADM */
#define HPSA_RAID_MAX		HPSA_RAID_ADM
#define HPSA_RAID_UNKNOWN	0xff

/* Subroutine used to parse the scsi opcode and build the CDB for RAID bypass*/
static int
fill_lba_for_scsi_rw(pqisrc_softstate_t *softs, uint8_t *cdb, aio_req_locator_t *l)
{

	if (!l) {
		DBG_INFO("No locator ptr: AIO ineligible");
		return PQI_STATUS_FAILURE;
	}

	if (cdb == NULL)
		return PQI_STATUS_FAILURE;

	switch (cdb[0]) {
	case SCMD_WRITE_6:
		l->is_write = true;
		/* coverity[fallthrough] */
	case SCMD_READ_6:
		l->block.first = (uint64_t)(((cdb[1] & 0x1F) << 16) |
				(cdb[2] << 8) | cdb[3]);
		l->block.cnt = (uint32_t)cdb[4];
		if (l->block.cnt == 0)
				l->block.cnt = 256; /*blkcnt 0 means 256 */
		break;
	case SCMD_WRITE_10:
		l->is_write = true;
		/* coverity[fallthrough] */
	case SCMD_READ_10:
		l->block.first = (uint64_t)GET_BE32(&cdb[2]);
		l->block.cnt = (uint32_t)GET_BE16(&cdb[7]);
		break;
	case SCMD_WRITE_12:
		l->is_write = true;
		/* coverity[fallthrough] */
	case SCMD_READ_12:
		l->block.first = (uint64_t)GET_BE32(&cdb[2]);
		l->block.cnt = GET_BE32(&cdb[6]);
		break;
	case SCMD_WRITE_16:
		l->is_write = true;
		/* coverity[fallthrough] */
	case SCMD_READ_16:
		l->block.first = GET_BE64(&cdb[2]);
		l->block.cnt = GET_BE32(&cdb[10]);
		break;
	default:
		/* Process via normal I/O path. */
		DBG_AIO("NOT read or write 6/10/12/16: AIO ineligible");
		return PQI_STATUS_FAILURE;
	}
	return PQI_STATUS_SUCCESS;
}


/* determine whether writes to certain types of RAID are supported. */
static boolean_t
pqisrc_is_supported_write(pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *device)
{

	DBG_FUNC("IN\n");

	/* Raid0 was always supported */
	if (device->raid_level == SA_RAID_0)
		return true;

	/* module params for individual adv. aio write features may be on,
	 * which affects ALL controllers, but some controllers
	 * do not support adv. aio write.
	 */
	if (!softs->adv_aio_capable)
		return false;

	/* if the raid write bypass feature is turned on,
	 * then the write is supported.
	 */
	switch (device->raid_level) {
	case SA_RAID_1:
	case SA_RAID_ADM:
		if (softs->aio_raid1_write_bypass)
			return true;
		break;
	case SA_RAID_5:
		if (softs->aio_raid5_write_bypass)
			return true;
		break;
	case SA_RAID_6:
		if (softs->aio_raid6_write_bypass)
			return true;
	}

	/* otherwise, it must be an unsupported write. */
	DBG_IO("AIO ineligible: write not supported for raid type\n");
	DBG_FUNC("OUT\n");
	return false;

}

/* check for zero-byte transfers, invalid blocks, and wraparound */
static inline boolean_t
pqisrc_is_invalid_block(pqisrc_softstate_t *softs, aio_req_locator_t *l)
{
	DBG_FUNC("IN\n");

	if (l->block.cnt == 0) {
		DBG_AIO("AIO ineligible: blk_cnt=0\n");
		DBG_FUNC("OUT\n");
		return true;
	}

	if (l->block.last < l->block.first ||
		l->block.last >=
			GET_LE64((uint8_t *)&l->raid_map->volume_blk_cnt)) {
		DBG_AIO("AIO ineligible: last block < first\n");
		DBG_FUNC("OUT\n");
		return true;
	}

	DBG_FUNC("OUT\n");
	return false;
}

/* Compute various attributes of request's location */
static inline boolean_t
pqisrc_calc_disk_params(pqisrc_softstate_t *softs, aio_req_locator_t *l,  rcb_t *rcb)
{
	DBG_FUNC("IN\n");

	/* grab #disks, strip size, and layout map count from raid map */
	l->row.data_disks =
		GET_LE16((uint8_t *)&l->raid_map->data_disks_per_row);
	l->strip_sz =
		GET_LE16((uint8_t *)(&l->raid_map->strip_size));
	l->map.layout_map_count =
		GET_LE16((uint8_t *)(&l->raid_map->layout_map_count));

	/* Calculate stripe information for the request. */
	l->row.blks_per_row =  l->row.data_disks * l->strip_sz;
	if (!l->row.blks_per_row || !l->strip_sz) {
		DBG_AIO("AIO ineligible\n");
		DBG_FUNC("OUT\n");
		return false;
	}
	/* use __udivdi3 ? */
	rcb->blocks_per_row = l->row.blks_per_row;
	l->row.first = l->block.first / l->row.blks_per_row;
	rcb->row_num = l->row.first;
	l->row.last = l->block.last / l->row.blks_per_row;
	l->row.offset_first = (uint32_t)(l->block.first -
		(l->row.first * l->row.blks_per_row));
	l->row.offset_last = (uint32_t)(l->block.last -
		(l->row.last * l->row.blks_per_row));
	l->col.first = l->row.offset_first / l->strip_sz;
	l->col.last = l->row.offset_last / l->strip_sz;

	DBG_FUNC("OUT\n");
	return true;
}

/* Not AIO-eligible if it isnt' a single row/column. */
static inline boolean_t
pqisrc_is_single_row_column(pqisrc_softstate_t *softs, aio_req_locator_t *l)
{
	boolean_t ret = true;
	DBG_FUNC("IN\n");

	if (l->row.first != l->row.last || l->col.first != l->col.last) {
		DBG_AIO("AIO ineligible\n");
		ret = false;
	}
	DBG_FUNC("OUT\n");
	return ret;
}

/* figure out disks/row, row, and map index. */
static inline boolean_t
pqisrc_set_map_row_and_idx(pqisrc_softstate_t *softs, aio_req_locator_t *l, rcb_t *rcb)
{
	if (!l->row.data_disks) {
		DBG_INFO("AIO ineligible: no data disks?\n");
		return false;
	}

	l->row.total_disks = l->row.data_disks +
		LE_16(l->raid_map->metadata_disks_per_row);

	l->map.row = ((uint32_t)(l->row.first >>
		l->raid_map->parity_rotation_shift)) %
		GET_LE16((uint8_t *)(&l->raid_map->row_cnt));

	l->map.idx = (l->map.row * l->row.total_disks) + l->col.first;
	rcb->raid_map_index = l->map.idx;
	rcb->raid_map_row = l->map.row;

	return true;
}

/* set the mirror for a raid 1/10/ADM */
static inline void
pqisrc_set_read_mirror(pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *device, aio_req_locator_t *l)
{
	/* Avoid direct use of device->offload_to_mirror within this
	 * function since multiple threads might simultaneously
	 * increment it beyond the range of device->layout_map_count -1.
	 */

	int mirror = device->offload_to_mirror[l->map.idx];
	int next_mirror = mirror + 1;

	if (next_mirror >= l->map.layout_map_count)
		next_mirror = 0;

	device->offload_to_mirror[l->map.idx] = next_mirror;
	l->map.idx += mirror * l->row.data_disks;
}

/* collect ioaccel handles for mirrors of given location. */
static inline boolean_t
pqisrc_set_write_mirrors(
	pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *device,
	aio_req_locator_t *l,
	rcb_t *rcb)
{
	uint32_t mirror = 0;
	uint32_t index;

	if (l->map.layout_map_count > PQISRC_MAX_SUPPORTED_MIRRORS)
		return false;

	do {
		index = l->map.idx + (l->row.data_disks * mirror);
		rcb->it_nexus[mirror] =
			l->raid_map->dev_data[index].ioaccel_handle;
		mirror++;
	} while (mirror != l->map.layout_map_count);

	return true;
}

/* Make sure first and last block are in the same R5/R6 RAID group. */
static inline boolean_t
pqisrc_is_r5or6_single_group(pqisrc_softstate_t *softs, aio_req_locator_t *l)
{
	boolean_t ret = true;

	DBG_FUNC("IN\n");
	l->r5or6.row.blks_per_row = l->strip_sz * l->row.data_disks;
	l->stripesz = l->r5or6.row.blks_per_row * l->map.layout_map_count;
	l->group.first = (l->block.first % l->stripesz) /
				l->r5or6.row.blks_per_row;
	l->group.last = (l->block.last % l->stripesz) /
				l->r5or6.row.blks_per_row;

	if (l->group.first != l->group.last) {
		DBG_AIO("AIO ineligible");
		ret = false;
	}

	DBG_FUNC("OUT\n");
	ASSERT(ret == true);
	return ret;
}
/* Make sure R5 or R6 request doesn't span rows. */
static inline boolean_t
pqisrc_is_r5or6_single_row(pqisrc_softstate_t *softs, aio_req_locator_t *l)
{
	boolean_t ret = true;

	DBG_FUNC("IN\n");

	/* figure row nums containing first & last block */
	l->row.first = l->r5or6.row.first =
		l->block.first / l->stripesz;
	l->r5or6.row.last = l->block.last / l->stripesz;

	if (l->r5or6.row.first != l->r5or6.row.last) {
		DBG_AIO("AIO ineligible");
		ret = false;
	}

	DBG_FUNC("OUT\n");
	ASSERT(ret == true);
	return ret;
}

/* Make sure R5 or R6 request doesn't span columns. */
static inline boolean_t
pqisrc_is_r5or6_single_column(pqisrc_softstate_t *softs, aio_req_locator_t *l)
{
	boolean_t ret = true;

	/* Find the columns of the first and last block */
	l->row.offset_first = l->r5or6.row.offset_first =
		(uint32_t)((l->block.first % l->stripesz) %
		l->r5or6.row.blks_per_row);
	l->r5or6.row.offset_last =
		(uint32_t)((l->block.last % l->stripesz) %
		l->r5or6.row.blks_per_row);

	l->col.first = l->r5or6.row.offset_first / l->strip_sz;
	l->r5or6.col.first = l->col.first;
	l->r5or6.col.last = l->r5or6.row.offset_last / l->strip_sz;

	if (l->r5or6.col.first != l->r5or6.col.last) {
		DBG_AIO("AIO ineligible");
		ret = false;
	}

	ASSERT(ret == true);
	return ret;
}


/* Set the map row and index for a R5 or R6 AIO request */
static inline void
pqisrc_set_r5or6_row_and_index(aio_req_locator_t *l,
	rcb_t *rcb)
{
	l->map.row = ((uint32_t)
		(l->row.first >> l->raid_map->parity_rotation_shift)) %
		GET_LE16((uint8_t *)(&l->raid_map->row_cnt));

	l->map.idx = (l->group.first *
		(GET_LE16((uint8_t *)(&l->raid_map->row_cnt))
		* l->row.total_disks))
		+ (l->map.row * l->row.total_disks)
		+ l->col.first;

	rcb->raid_map_index = l->map.idx;
	rcb->raid_map_row = l->map.row;
}

/* calculate physical disk block for aio request */
static inline boolean_t
pqisrc_calc_aio_block(aio_req_locator_t *l)
{
	boolean_t ret = true;

	l->block.disk_block =
		GET_LE64((uint8_t *) (&l->raid_map->disk_starting_blk))
		+ (l->row.first * l->strip_sz)
		+ ((uint64_t)(l->row.offset_first) - (uint64_t)(l->col.first) * l->strip_sz);

	/* any values we should be checking here? if not convert to void */
	return ret;
}

/* Handle differing logical/physical block sizes. */
static inline uint32_t
pqisrc_handle_blk_size_diffs(aio_req_locator_t *l)
{
	uint32_t disk_blk_cnt;
	disk_blk_cnt = l->block.cnt;

	if (l->raid_map->phys_blk_shift) {
		l->block.disk_block <<= l->raid_map->phys_blk_shift;
		disk_blk_cnt <<= l->raid_map->phys_blk_shift;
	}
	return disk_blk_cnt;
}

/* Make sure AIO request doesn't exceed the max that AIO device can
 * handle based on dev type, Raid level, and encryption status.
 * TODO: make limits dynamic when this becomes possible.
 */
static boolean_t
pqisrc_aio_req_too_big(pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *device, rcb_t *rcb,
	aio_req_locator_t *l, uint32_t disk_blk_cnt)
{
	boolean_t ret = false;
	uint32_t dev_max;
	uint32_t size = disk_blk_cnt * device->raid_map->volume_blk_size;
	dev_max = size;

	/* filter for nvme crypto */
	if (device->is_nvme && rcb->encrypt_enable) {
		if (softs->max_aio_rw_xfer_crypto_nvme != 0) {
			dev_max = MIN(dev_max,softs->max_aio_rw_xfer_crypto_nvme);
		}
	}

	/* filter for RAID 5/6/50/60 */
	if (!device->is_physical_device &&
		(device->raid_level == SA_RAID_5 ||
		device->raid_level == SA_RAID_51 ||
		device->raid_level == SA_RAID_6)) {
		if (softs->max_aio_write_raid5_6 != 0) {
			dev_max = MIN(dev_max,softs->max_aio_write_raid5_6);
		}
	}

	/* filter for RAID ADM */
	if (!device->is_physical_device &&
		(device->raid_level == SA_RAID_ADM) &&
		(softs->max_aio_write_raid1_10_3drv != 0)) {
			dev_max = MIN(dev_max,
				softs->max_aio_write_raid1_10_3drv);
	}

	/* filter for RAID 1/10 */
	if (!device->is_physical_device &&
		(device->raid_level == SA_RAID_1) &&
		(softs->max_aio_write_raid1_10_2drv != 0)) {
			dev_max = MIN(dev_max,
				softs->max_aio_write_raid1_10_2drv);
	}


	if (size > dev_max) {
		DBG_AIO("AIO ineligible: size=%u, max=%u", size, dev_max);
		ret = true;
	}

	return ret;
}


#ifdef DEBUG_RAID_MAP
static inline void
pqisrc_aio_show_raid_map(pqisrc_softstate_t *softs, struct raid_map *m)
{
	int i;

	if (!m) {
		DBG_WARN("No RAID MAP!\n");
		return;
	}
	DBG_INFO("======= Raid Map ================\n");
	DBG_INFO("%-25s: 0x%x\n", "StructureSize", m->structure_size);
	DBG_INFO("%-25s: 0x%x\n", "LogicalBlockSize", m->volume_blk_size);
	DBG_INFO("%-25s: 0x%lx\n", "LogicalBlockCount", m->volume_blk_cnt);
	DBG_INFO("%-25s: 0x%x\n", "PhysicalBlockShift", m->phys_blk_shift);
	DBG_INFO("%-25s: 0x%x\n", "ParityRotationShift",
				m->parity_rotation_shift);
	DBG_INFO("%-25s: 0x%x\n", "StripSize", m->strip_size);
	DBG_INFO("%-25s: 0x%lx\n", "DiskStartingBlock", m->disk_starting_blk);
	DBG_INFO("%-25s: 0x%lx\n", "DiskBlockCount", m->disk_blk_cnt);
	DBG_INFO("%-25s: 0x%x\n", "DataDisksPerRow", m->data_disks_per_row);
	DBG_INFO("%-25s: 0x%x\n", "MetdataDisksPerRow",
				m->metadata_disks_per_row);
	DBG_INFO("%-25s: 0x%x\n", "RowCount", m->row_cnt);
	DBG_INFO("%-25s: 0x%x\n", "LayoutMapCnt", m->layout_map_count);
	DBG_INFO("%-25s: 0x%x\n", "fEncryption", m->flags);
	DBG_INFO("%-25s: 0x%x\n", "DEK", m->data_encryption_key_index);
	for (i = 0; i < RAID_MAP_MAX_ENTRIES; i++) {
		if (m->dev_data[i].ioaccel_handle == 0)
			break;
		DBG_INFO("%-25s: %d: 0x%04x\n", "ioaccel_handle, disk",
			i, m->dev_data[i].ioaccel_handle);
	}
}
#endif /* DEBUG_RAID_MAP */

static inline void
pqisrc_aio_show_locator_info(pqisrc_softstate_t *softs,
	aio_req_locator_t *l, uint32_t disk_blk_cnt, rcb_t *rcb)
{
#ifdef DEBUG_AIO_LOCATOR
	pqisrc_aio_show_raid_map(softs, l->raid_map);

	DBG_INFO("======= AIO Locator Content, tag#0x%08x =====\n", rcb->tag);
	DBG_INFO("%-25s: 0x%lx\n", "block.first", l->block.first);
	DBG_INFO("%-25s: 0x%lx\n", "block.last", l->block.last);
	DBG_INFO("%-25s: 0x%x\n", "block.cnt", l->block.cnt);
	DBG_INFO("%-25s: 0x%lx\n", "block.disk_block", l->block.disk_block);
	DBG_INFO("%-25s: 0x%x\n", "row.blks_per_row", l->row.blks_per_row);
	DBG_INFO("%-25s: 0x%lx\n", "row.first", l->row.first);
	DBG_INFO("%-25s: 0x%lx\n", "row.last", l->row.last);
	DBG_INFO("%-25s: 0x%x\n", "row.offset_first", l->row.offset_first);
	DBG_INFO("%-25s: 0x%x\n", "row.offset_last", l->row.offset_last);
	DBG_INFO("%-25s: 0x%x\n", "row.data_disks", l->row.data_disks);
	DBG_INFO("%-25s: 0x%x\n", "row.total_disks", l->row.total_disks);
	DBG_INFO("%-25s: 0x%x\n", "col.first", l->col.first);
	DBG_INFO("%-25s: 0x%x\n", "col.last", l->col.last);

	if (l->raid_level == SA_RAID_5 || l->raid_level == SA_RAID_6) {
		DBG_INFO("%-25s: 0x%x\n", "r5or6.row.blks_per_row",
				l->r5or6.row.blks_per_row);
		DBG_INFO("%-25s: 0x%lx\n", "r5or6.row.first", l->r5or6.row.first);
		DBG_INFO("%-25s: 0x%lx\n", "r5or6.row.last", l->r5or6.row.last);
		DBG_INFO("%-25s: 0x%x\n", "r5or6.row.offset_first",
					l->r5or6.row.offset_first);
		DBG_INFO("%-25s: 0x%x\n", "r5or6.row.offset_last",
					l->r5or6.row.offset_last);
		DBG_INFO("%-25s: 0x%x\n", "r5or6.row.data_disks",
					l->r5or6.row.data_disks);
		DBG_INFO("%-25s: 0x%x\n", "r5or6.row.total_disks",
					l->r5or6.row.total_disks);
		DBG_INFO("%-25s: 0x%x\n", "r5or6.col.first", l->r5or6.col.first);
		DBG_INFO("%-25s: 0x%x\n", "r5or6.col.last", l->r5or6.col.last);
	}
	DBG_INFO("%-25s: 0x%x\n", "map.row", l->map.row);
	DBG_INFO("%-25s: 0x%x\n", "map.idx", l->map.idx);
	DBG_INFO("%-25s: 0x%x\n", "map.layout_map_count",
				l->map.layout_map_count);
	DBG_INFO("%-25s: 0x%x\n", "group.first", l->group.first);
	DBG_INFO("%-25s: 0x%x\n", "group.last", l->group.last);
	DBG_INFO("%-25s: 0x%x\n", "group.cur", l->group.cur);
	DBG_INFO("%-25s: %d\n", "is_write", l->is_write);
	DBG_INFO("%-25s: 0x%x\n", "stripesz", l->stripesz);
	DBG_INFO("%-25s: 0x%x\n", "strip_sz", l->strip_sz);
	DBG_INFO("%-25s: %d\n", "offload_to_mirror", l->offload_to_mirror);
	DBG_INFO("%-25s: %d\n", "raid_level", l->raid_level);

#endif /* DEBUG_AIO_LOCATOR */
}

/* build the aio cdb */
static void
pqisrc_aio_build_cdb(aio_req_locator_t *l,
		uint32_t disk_blk_cnt, rcb_t *rcb, uint8_t *cdb)
{
	uint8_t cdb_length;

	if (l->block.disk_block > 0xffffffff) {
		cdb[0] = l->is_write ? SCMD_WRITE_16 : SCMD_READ_16;
		cdb[1] = 0;
		PUT_BE64(l->block.disk_block, &cdb[2]);
		PUT_BE32(disk_blk_cnt, &cdb[10]);
		cdb[15] = 0;
		cdb_length = 16;
	} else {
		cdb[0] = l->is_write ? SCMD_WRITE_10 : SCMD_READ_10;
		cdb[1] = 0;
		PUT_BE32(l->block.disk_block, &cdb[2]);
		cdb[6] = 0;
		PUT_BE16(disk_blk_cnt, &cdb[7]);
		cdb[9] = 0;
		cdb_length = 10;
	}

	rcb->cmdlen = cdb_length;

}

/* print any arbitrary buffer of length total_len */
void
pqisrc_print_buffer(pqisrc_softstate_t *softs, char *msg, void *user_buf,
		uint32_t total_len, uint32_t flags)
{
#define LINE_BUF_LEN 60
#define INDEX_PER_LINE 16
	uint32_t buf_consumed = 0;
	int ii;
	char line_buf[LINE_BUF_LEN];
	int line_len; /* written length per line */
	uint8_t this_char;

	if (user_buf == NULL)
		return;

	memset(line_buf, 0, LINE_BUF_LEN);

	/* Print index columns */
	if (flags & PRINT_FLAG_HDR_COLUMN)
	{
		for (ii = 0, line_len = 0; ii < MIN(total_len, 16); ii++)
		{
			line_len += snprintf(line_buf + line_len, (LINE_BUF_LEN - line_len), "%02d ", ii);
			if ((line_len + 4) >= LINE_BUF_LEN)
				break;
		}
		DBG_INFO("%15.15s:[ %s ]\n", "header", line_buf);
	}

	/* Print index columns */
	while(buf_consumed < total_len)
	{
		memset(line_buf, 0, LINE_BUF_LEN);

		for (ii = 0, line_len = 0; ii < INDEX_PER_LINE; ii++)
		{
			this_char = *((char*)(user_buf) + buf_consumed);
			line_len += snprintf(line_buf + line_len, (LINE_BUF_LEN - line_len), "%02x ", this_char);

			buf_consumed++;
			if (buf_consumed >= total_len || (line_len + 4) >= LINE_BUF_LEN)
				break;
		}
		DBG_INFO("%15.15s:[ %s ]\n", msg, line_buf);
	}
}

/* print CDB with column header */
void
pqisrc_show_cdb(pqisrc_softstate_t *softs, char *msg, rcb_t *rcb, uint8_t *cdb)
{
	/* Print the CDB contents */
	pqisrc_print_buffer(softs, msg, cdb, rcb->cmdlen, PRINT_FLAG_HDR_COLUMN);
}

void
pqisrc_show_rcb_details(pqisrc_softstate_t *softs, rcb_t *rcb, char *msg, void *err_info)
{
   pqi_scsi_dev_t *devp;

	if (rcb == NULL || rcb->dvp == NULL)
	{
		DBG_ERR("Invalid rcb or dev ptr! rcb=%p\n", rcb);
		return;
	}

	devp = rcb->dvp;

	/* print the host and mapped CDB */
	DBG_INFO("\n");
	DBG_INFO("----- Start Dump: %s -----\n", msg);
	pqisrc_print_buffer(softs, "host cdb", OS_GET_CDBP(rcb), rcb->cmdlen, PRINT_FLAG_HDR_COLUMN);
	if (OS_GET_CDBP(rcb) != rcb->cdbp)
		pqisrc_print_buffer(softs, "aio mapped cdb", rcb->cdbp, rcb->cmdlen, 0);

	DBG_INFO("tag=0x%x dir=%u host_timeout=%ums\n", rcb->tag,
		rcb->data_dir, (uint32_t)rcb->host_timeout_ms);

	DBG_INFO("BTL: %d:%d:%d addr=0x%x\n", devp->bus, devp->target,
		devp->lun, GET_LE32(devp->scsi3addr));

	if (rcb->path == AIO_PATH)
	{
		DBG_INFO("handle=0x%x\n", rcb->ioaccel_handle);
		DBG_INFO("row=%u blk/row=%u index=%u map_row=%u\n",
			rcb->row_num, rcb->blocks_per_row, rcb->raid_map_index, rcb->raid_map_row);

		if (err_info)
			pqisrc_show_aio_error_info(softs, rcb, err_info);
	}

	else /* RAID path */
	{
		if (err_info)
			pqisrc_show_raid_error_info(softs, rcb, err_info);
	}


	DBG_INFO("-----  Done -----\n\n");
}


/*
 * Function used to build and send RAID bypass request to the adapter
 */
int
pqisrc_build_scsi_cmd_raidbypass(pqisrc_softstate_t *softs,
			pqi_scsi_dev_t *device, rcb_t *rcb)
{
	uint32_t disk_blk_cnt;
	struct aio_req_locator loc;
	struct aio_req_locator *l = &loc;
	int rc;
	memset(l, 0, sizeof(*l));

	DBG_FUNC("IN\n");

	if (device == NULL) {
		DBG_INFO("device is NULL\n");
		return PQI_STATUS_FAILURE;
	}
	if (device->raid_map == NULL) {
		DBG_INFO("tag=0x%x BTL: %d:%d:%d Raid map is NULL\n",
			rcb->tag, device->bus, device->target, device->lun);
		return PQI_STATUS_FAILURE;
	}

	/* Check for eligible op, get LBA and block count. */
	rc =  fill_lba_for_scsi_rw(softs, OS_GET_CDBP(rcb), l);
	if (rc == PQI_STATUS_FAILURE)
		return PQI_STATUS_FAILURE;

	if (l->is_write && !pqisrc_is_supported_write(softs, device))
		return PQI_STATUS_FAILURE;

	l->raid_map = device->raid_map;
	l->block.last = l->block.first + l->block.cnt - 1;
	l->raid_level = device->raid_level;

	if (pqisrc_is_invalid_block(softs, l))
		return PQI_STATUS_FAILURE;

	if (!pqisrc_calc_disk_params(softs, l, rcb))
		return PQI_STATUS_FAILURE;

	if (!pqisrc_is_single_row_column(softs, l))
		return PQI_STATUS_FAILURE;

	if (!pqisrc_set_map_row_and_idx(softs, l, rcb))
		return PQI_STATUS_FAILURE;

	/* Proceeding with driver mapping. */


	switch (device->raid_level) {
	case SA_RAID_1:
	case SA_RAID_ADM:
		if (l->is_write) {
			if (!pqisrc_set_write_mirrors(softs, device, l, rcb))
				return PQI_STATUS_FAILURE;
		} else
			pqisrc_set_read_mirror(softs, device, l);
		break;
	case SA_RAID_5:
	case SA_RAID_6:
		if (l->map.layout_map_count > 1 || l->is_write) {

			if (!pqisrc_is_r5or6_single_group(softs, l))
				return PQI_STATUS_FAILURE;

			if (!pqisrc_is_r5or6_single_row(softs, l))
				return PQI_STATUS_FAILURE;

			if (!pqisrc_is_r5or6_single_column(softs, l))
				return PQI_STATUS_FAILURE;

			pqisrc_set_r5or6_row_and_index(l, rcb);
		}
		break;
	}

	if (l->map.idx >= RAID_MAP_MAX_ENTRIES) {
		DBG_INFO("AIO ineligible: index exceeds max map entries");
		return PQI_STATUS_FAILURE;
	}

	rcb->ioaccel_handle =
		l->raid_map->dev_data[l->map.idx].ioaccel_handle;

	if (!pqisrc_calc_aio_block(l))
		return PQI_STATUS_FAILURE;

	disk_blk_cnt = pqisrc_handle_blk_size_diffs(l);


	/* Set encryption flag if needed. */
	rcb->encrypt_enable = false;
	if (GET_LE16((uint8_t *)(&l->raid_map->flags)) &
		RAID_MAP_ENCRYPTION_ENABLED) {
		pqisrc_set_enc_info(&rcb->enc_info, l->raid_map,
			l->block.first);
		rcb->encrypt_enable = true;
	}

	if (pqisrc_aio_req_too_big(softs, device, rcb, l, disk_blk_cnt))
		return PQI_STATUS_FAILURE;

	/* set the cdb ptr to the local bypass cdb */
	rcb->cdbp = &rcb->bypass_cdb[0];

	/* Build the new CDB for the physical disk I/O. */
	pqisrc_aio_build_cdb(l, disk_blk_cnt, rcb, rcb->cdbp);

	pqisrc_aio_show_locator_info(softs, l, disk_blk_cnt, rcb);

	DBG_FUNC("OUT\n");

	return PQI_STATUS_SUCCESS;
}

/* Function used to submit an AIO TMF to the adapter
 * DEVICE_RESET is not supported.
 */

static int
pqisrc_send_aio_tmf(pqisrc_softstate_t *softs, pqi_scsi_dev_t *devp,
                    rcb_t *rcb, rcb_t *rcb_to_manage, int tmf_type)
{
	int rval = PQI_STATUS_SUCCESS;
	pqi_aio_tmf_req_t tmf_req;
	ib_queue_t *op_ib_q = NULL;
	boolean_t is_write;

	memset(&tmf_req, 0, sizeof(pqi_aio_tmf_req_t));

	DBG_FUNC("IN\n");

	tmf_req.header.iu_type = PQI_REQUEST_IU_AIO_TASK_MANAGEMENT;
	tmf_req.header.iu_length = sizeof(tmf_req) - sizeof(iu_header_t);
	tmf_req.req_id = rcb->tag;
	tmf_req.error_idx = rcb->tag;
	tmf_req.nexus = devp->ioaccel_handle;
	/* memcpy(tmf_req.lun, devp->scsi3addr, sizeof(tmf_req.lun)); */
	tmf_req.tmf = tmf_type;
	tmf_req.resp_qid = OS_GET_TMF_RESP_QID(softs, rcb);
	op_ib_q = &softs->op_aio_ib_q[0];
	is_write = pqisrc_cdb_is_write(rcb->cdbp);

	uint64_t lun = rcb->cm_ccb->ccb_h.target_lun;
	if (lun && (rcb->dvp->is_multi_lun)) {
		int_to_scsilun(lun, tmf_req.lun);
	}
	else {
		memset(tmf_req.lun, 0, sizeof(tmf_req.lun));
	}

	if (tmf_type == SOP_TASK_MANAGEMENT_FUNCTION_ABORT_TASK) {
		tmf_req.req_id_to_manage = rcb_to_manage->tag;
		tmf_req.nexus = rcb_to_manage->ioaccel_handle;
	}

	if (devp->raid_level == SA_RAID_1 ||
	    devp->raid_level == SA_RAID_5 ||
	    devp->raid_level == SA_RAID_6) {
		if (tmf_type == SOP_TASK_MANAGEMENT_FUNCTION_ABORT_TASK && is_write)
			tmf_req.header.iu_type = PQI_REQUEST_IU_AIO_BYPASS_TASK_MGMT;
	}

	DBG_WARN("aio tmf: iu_type=0x%x req_id_to_manage=0x%x\n",
		tmf_req.header.iu_type, tmf_req.req_id_to_manage);
	DBG_WARN("aio tmf: req_id=0x%x nexus=0x%x tmf=0x%x QID=%u\n",
		tmf_req.req_id, tmf_req.nexus, tmf_req.tmf, op_ib_q->q_id);

	rcb->path = AIO_PATH;
	rcb->req_pending = true;
	/* Timedout tmf response goes here */
	rcb->error_cmp_callback = pqisrc_process_aio_response_error;

	rval = pqisrc_submit_cmnd(softs, op_ib_q, &tmf_req);
	if (rval != PQI_STATUS_SUCCESS) {
		DBG_ERR("Unable to submit command rval=%d\n", rval);
		return rval;
	}

	rval = pqisrc_wait_on_condition(softs, rcb, PQISRC_TMF_TIMEOUT);
	if (rval != PQI_STATUS_SUCCESS){
		DBG_ERR("Task Management tmf_type : %d timeout\n", tmf_type);
		rcb->status = rval;
	}

	if (rcb->status  != PQI_STATUS_SUCCESS) {
		DBG_ERR_BTL(devp, "Task Management failed tmf_type:%d "
				"stat:0x%x\n", tmf_type, rcb->status);
		rval = PQI_STATUS_FAILURE;
	}

	DBG_FUNC("OUT\n");
	return rval;
}

/* Function used to submit a Raid TMF to the adapter */
static int
pqisrc_send_raid_tmf(pqisrc_softstate_t *softs, pqi_scsi_dev_t *devp,
                    rcb_t *rcb, rcb_t *rcb_to_manage, int tmf_type)
{
	int rval = PQI_STATUS_SUCCESS;
	pqi_raid_tmf_req_t tmf_req;
	ib_queue_t *op_ib_q = NULL;

	memset(&tmf_req, 0, sizeof(pqi_raid_tmf_req_t));

	DBG_FUNC("IN\n");

	tmf_req.header.iu_type = PQI_REQUEST_IU_RAID_TASK_MANAGEMENT;
	tmf_req.header.iu_length = sizeof(tmf_req) - sizeof(iu_header_t);
	tmf_req.req_id = rcb->tag;

	memcpy(tmf_req.lun, devp->scsi3addr, sizeof(tmf_req.lun));
	tmf_req.ml_device_lun_number = (uint8_t)rcb->cm_ccb->ccb_h.target_lun;

	tmf_req.tmf = tmf_type;
	tmf_req.resp_qid = OS_GET_TMF_RESP_QID(softs, rcb);

	/* Decide the queue where the tmf request should be submitted */
	if (tmf_type == SOP_TASK_MANAGEMENT_FUNCTION_ABORT_TASK) {
		tmf_req.obq_id_to_manage = rcb_to_manage->resp_qid;
		tmf_req.req_id_to_manage = rcb_to_manage->tag;
	}

	if (softs->timeout_in_tmf &&
			tmf_type == SOP_TASK_MANAGEMENT_LUN_RESET) {
		/* OS_TMF_TIMEOUT_SEC - 1 to accomodate driver processing */
		tmf_req.timeout_in_sec = OS_TMF_TIMEOUT_SEC - 1;
		/* if OS tmf timeout is 0, set minimum value for timeout */
		if (!tmf_req.timeout_in_sec)
			tmf_req.timeout_in_sec = 1;
	}

	op_ib_q = &softs->op_raid_ib_q[0];

	DBG_WARN("raid tmf: iu_type=0x%x req_id_to_manage=%d\n",
		tmf_req.header.iu_type, tmf_req.req_id_to_manage);

	rcb->path = RAID_PATH;
	rcb->req_pending = true;
	/* Timedout tmf response goes here */
	rcb->error_cmp_callback = pqisrc_process_raid_response_error;

	rval = pqisrc_submit_cmnd(softs, op_ib_q, &tmf_req);
	if (rval != PQI_STATUS_SUCCESS) {
		DBG_ERR("Unable to submit command rval=%d\n", rval);
		return rval;
	}

	rval = pqisrc_wait_on_condition(softs, rcb, PQISRC_TMF_TIMEOUT);
	if (rval != PQI_STATUS_SUCCESS) {
		DBG_ERR("Task Management tmf_type : %d timeout\n", tmf_type);
		rcb->status = rval;
	}

	if (rcb->status  != PQI_STATUS_SUCCESS) {
		DBG_NOTE("Task Management failed tmf_type:%d "
				"stat:0x%x\n", tmf_type, rcb->status);
		rval = PQI_STATUS_FAILURE;
	}

	DBG_FUNC("OUT\n");
	return rval;
}

void
dump_tmf_details(pqisrc_softstate_t *softs, rcb_t *rcb, char *msg)
{
	uint32_t qid = rcb->req_q ? rcb->req_q->q_id : -1;

	DBG_INFO("%s: pending=%d path=%d tag=0x%x=%u qid=%u timeout=%ums\n",
		msg, rcb->req_pending, rcb->path, rcb->tag,
		rcb->tag, qid, (uint32_t)rcb->host_timeout_ms);
}

int
pqisrc_send_tmf(pqisrc_softstate_t *softs, pqi_scsi_dev_t *devp,
                    rcb_t *rcb, rcb_t *rcb_to_manage, int tmf_type)
{
	int ret = PQI_STATUS_SUCCESS;

	DBG_FUNC("IN\n");

	DBG_WARN("sending TMF. io outstanding=%u\n",
		softs->max_outstanding_io - softs->taglist.num_elem);

	rcb->is_abort_cmd_from_host = true;
	rcb->softs = softs;

	/* No target rcb for general purpose TMFs like LUN RESET */
	if (tmf_type == SOP_TASK_MANAGEMENT_FUNCTION_ABORT_TASK)
	{
		rcb_to_manage->host_wants_to_abort_this = true;
		dump_tmf_details(softs, rcb_to_manage, "rcb_to_manage");
	}


	dump_tmf_details(softs, rcb, "rcb");

	if(!devp->is_physical_device) {
		if (tmf_type == SOP_TASK_MANAGEMENT_FUNCTION_ABORT_TASK) {
			if(rcb_to_manage->path == AIO_PATH) {
				if(devp->offload_enabled)
					ret = pqisrc_send_aio_tmf(softs, devp, rcb, rcb_to_manage, tmf_type);
			}
			else {
				DBG_INFO("TASK ABORT not supported in raid\n");
				ret = PQI_STATUS_FAILURE;
			}
		}
		else {
			ret = pqisrc_send_raid_tmf(softs, devp, rcb, rcb_to_manage, tmf_type);
		}
	} else {
		if (tmf_type == SOP_TASK_MANAGEMENT_FUNCTION_ABORT_TASK)
			ret = pqisrc_send_aio_tmf(softs, devp, rcb, rcb_to_manage, tmf_type);
		else
			ret = pqisrc_send_raid_tmf(softs, devp, rcb, rcb_to_manage, tmf_type);
	}

	DBG_FUNC("OUT\n");

	return ret;
}

/* return index into the global (softs) counters based on raid level */
static counter_types_t
get_counter_index(rcb_t *rcb)
{
	if (IS_AIO_PATH(rcb->dvp))
		return HBA_COUNTER;

	switch (rcb->dvp->raid_level) {
		case SA_RAID_0:	return RAID0_COUNTER;
		case SA_RAID_1:
		case SA_RAID_ADM:	return RAID1_COUNTER;
		case SA_RAID_5:	return RAID5_COUNTER;
		case SA_RAID_6:	return RAID6_COUNTER;
		case SA_RAID_UNKNOWN:
		default:
		{
			static boolean_t asserted = false;
			if (!asserted)
			{
				asserted = true;
				ASSERT(rcb->path == RAID_PATH);
				ASSERT(0);
			}
			return UNKNOWN_COUNTER;
		}
	}
}

/* return the counter type as ASCII-string */
static char *
counter_type_to_raid_ascii(counter_types_t type)
{
	switch (type)
	{
		case UNKNOWN_COUNTER: return "Unknown";
		case HBA_COUNTER:		return "HbaPath";
		case RAID0_COUNTER:	return "Raid0";
		case RAID1_COUNTER:	return "Raid1";
		case RAID5_COUNTER:	return "Raid5";
		case RAID6_COUNTER:	return "Raid6";
		default:					return "Unsupported";
	}
}

/* return the path as ASCII-string */
char *
io_path_to_ascii(IO_PATH_T path)
{
	switch (path)
	{
		case AIO_PATH:		return "Aio";
		case RAID_PATH:	return "Raid";
		default:				return "Unknown";
	}
}

/* return the io type as ASCII-string */
static char *
io_type_to_ascii(io_type_t io_type)
{
	switch (io_type)
	{
		case UNKNOWN_IO_TYPE:	return "Unknown";
		case READ_IO_TYPE:		return "Read";
		case WRITE_IO_TYPE:		return "Write";
		case NON_RW_IO_TYPE:		return "NonRW";
		default:						return "Unsupported";
	}
}


/* return the io type based on cdb */
io_type_t
get_io_type_from_cdb(uint8_t *cdb)
{
	if (cdb == NULL)
		return UNKNOWN_IO_TYPE;

	else if (pqisrc_cdb_is_read(cdb))
		return READ_IO_TYPE;

	else if (pqisrc_cdb_is_write(cdb))
		return WRITE_IO_TYPE;

	return NON_RW_IO_TYPE;
}

/* increment this counter based on path and read/write */
OS_ATOMIC64_T
increment_this_counter(io_counters_t *pcounter, IO_PATH_T path, io_type_t io_type)
{
	OS_ATOMIC64_T ret_val;

	if (path == AIO_PATH)
	{
		if (io_type == READ_IO_TYPE)
			ret_val = OS_ATOMIC64_INC(&pcounter->aio_read_cnt);
		else if (io_type == WRITE_IO_TYPE)
			ret_val = OS_ATOMIC64_INC(&pcounter->aio_write_cnt);
		else
			ret_val = OS_ATOMIC64_INC(&pcounter->aio_non_read_write);
	}
	else
	{
		if (io_type == READ_IO_TYPE)
			ret_val = OS_ATOMIC64_INC(&pcounter->raid_read_cnt);
		else if (io_type == WRITE_IO_TYPE)
			ret_val = OS_ATOMIC64_INC(&pcounter->raid_write_cnt);
		else
			ret_val = OS_ATOMIC64_INC(&pcounter->raid_non_read_write);
	}

	return ret_val;
}

/* increment appropriate counter(s) anytime we post a new request */
static void
pqisrc_increment_io_counters(pqisrc_softstate_t *softs, rcb_t *rcb)
{
	io_type_t io_type = get_io_type_from_cdb(rcb->cdbp);
	counter_types_t type_index = get_counter_index(rcb);
	io_counters_t *pcounter = &softs->counters[type_index];
	OS_ATOMIC64_T ret_val;

	ret_val = increment_this_counter(pcounter, rcb->path, io_type);

#if 1 /* leave this enabled while we gain confidence for each io path */
	if (ret_val == 1)
	{
		char *raid_type = counter_type_to_raid_ascii(type_index);
		char *path = io_path_to_ascii(rcb->path);
		char *io_ascii = io_type_to_ascii(io_type);

		DBG_INFO("Got first path/type hit. "
			"Path=%s RaidType=%s IoType=%s\n",
			path, raid_type, io_ascii);
	}
#endif

	/* @todo future: may want to make a per-dev counter */
}

/* public routine to print a particular counter with header msg */
void
print_this_counter(pqisrc_softstate_t *softs, io_counters_t *pcounter, char *msg)
{
	io_counters_t counter;
	uint32_t percent_reads;
	uint32_t percent_aio;

	if (!softs->log_io_counters)
		return;

	/* Use a cached copy so percentages are based on the data that is printed */
	memcpy(&counter, pcounter, sizeof(counter));

	DBG_NOTE("Counter: %s (ptr=%p)\n", msg, pcounter);

	percent_reads = CALC_PERCENT_VS(counter.aio_read_cnt + counter.raid_read_cnt,
											counter.aio_write_cnt + counter.raid_write_cnt);

	percent_aio = CALC_PERCENT_VS(counter.aio_read_cnt + counter.aio_write_cnt,
											counter.raid_read_cnt + counter.raid_write_cnt);

	DBG_NOTE("   R/W Percentages: Reads=%3u%% AIO=%3u%%\n", percent_reads, percent_aio);

	/* Print the Read counts */
	percent_aio = CALC_PERCENT_VS(counter.aio_read_cnt, counter.raid_read_cnt);
	DBG_NOTE("   Reads : AIO=%8u(%3u%%) RAID=%8u\n",
		(uint32_t)counter.aio_read_cnt, percent_aio, (uint32_t)counter.raid_read_cnt);

	/* Print the Write counts */
	percent_aio = CALC_PERCENT_VS(counter.aio_write_cnt, counter.raid_write_cnt);
	DBG_NOTE("   Writes: AIO=%8u(%3u%%) RAID=%8u\n",
		(uint32_t)counter.aio_write_cnt, percent_aio, (uint32_t)counter.raid_write_cnt);

	/* Print the Non-Rw counts */
	percent_aio = CALC_PERCENT_VS(counter.aio_non_read_write, counter.raid_non_read_write);
	DBG_NOTE("   Non-RW: AIO=%8u(%3u%%) RAID=%8u\n",
		(uint32_t)counter.aio_non_read_write, percent_aio, (uint32_t)counter.raid_non_read_write);
}

/* return true if buffer is all zeroes */
boolean_t
is_buffer_zero(void *buffer, uint32_t size)
{
	char *buf = buffer;
	DWORD ii;

	if (buffer == NULL || size == 0)
		return false;

	for (ii = 0; ii < size; ii++)
	{
		if (buf[ii] != 0x00)
			return false;
	}
	return true;
}

/* public routine to print a all global counter types */
void
print_all_counters(pqisrc_softstate_t *softs, uint32_t flags)
{
	int ii;
	io_counters_t *pcounter;
	char *raid_type;

	for (ii = 0; ii < MAX_IO_COUNTER; ii++)
	{
		pcounter = &softs->counters[ii];
		raid_type = counter_type_to_raid_ascii(ii);

		if ((flags & COUNTER_FLAG_ONLY_NON_ZERO) &&
			is_buffer_zero(pcounter, sizeof(*pcounter)))
		{
			continue;
		}

		print_this_counter(softs, pcounter, raid_type);
	}

	if (flags & COUNTER_FLAG_CLEAR_COUNTS)
	{
		DBG_NOTE("Clearing all counters\n");
		memset(softs->counters, 0, sizeof(softs->counters));
	}
}
