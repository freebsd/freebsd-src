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
 * Populate the controller's advanced aio features via BMIC cmd.
 */
int
pqisrc_QuerySenseFeatures(pqisrc_softstate_t *softs)
{
	bmic_sense_feature_aio_buffer_t *features;
	int ret;
	pqisrc_raid_req_t request;

	/* Initializing defaults for AIO support subpage */
	softs->max_aio_write_raid5_6 =
		PQISRC_MAX_AIO_RAID5_OR_6_WRITE;
	softs->max_aio_write_raid1_10_2drv =
		PQISRC_MAX_AIO_RAID1_OR_10_WRITE_2DRV;
	softs->max_aio_write_raid1_10_3drv =
		PQISRC_MAX_AIO_RAID1_OR_10_WRITE_3DRV;
	softs->max_aio_rw_xfer_crypto_nvme =
		PQISRC_MAX_AIO_RW_XFER_NVME_CRYPTO;
	softs->max_aio_rw_xfer_crypto_sas_sata =
		PQISRC_MAX_AIO_RW_XFER_SAS_SATA_CRYPTO;

#ifdef DEVICE_HINT
	softs->enable_stream_detection = softs->hint.stream_status;
#endif

	/* Implement SENSE_FEATURE BMIC to populate AIO limits */
	features = os_mem_alloc(softs, sizeof(*features));
	if (!features) {
		DBG_ERR("Failed to allocate memory for sense aio features.\n");
		goto err;
	}
	memset(features, 0, sizeof(*features));

	memset(&request, 0, sizeof(request));
	request.data_direction = SOP_DATA_DIR_TO_DEVICE;
	request.cmd.bmic_cdb.op_code = BMIC_READ;
	request.cmd.cdb[2] = IO_SENSE_FEATURES_PAGE;
	request.cmd.cdb[3] = SENSE_FEATURES_AIO_SUBPAGE;
	request.cmd.bmic_cdb.cmd = BMIC_SENSE_FEATURE;
	request.cmd.bmic_cdb.xfer_len = BE_16(sizeof(*features));
	ret = pqisrc_prepare_send_ctrlr_request(softs, &request,
		features, sizeof(*features));

	if (ret)
		goto free_err;

	/* If AIO subpage was valid, use values from that page */
	if (features->aio_subpage.header.total_length >=
		MINIMUM_AIO_SUBPAGE_LENGTH) {
		DBG_INIT("AIO support subpage valid. total_length = 0x%0x.\n",
			features->aio_subpage.header.total_length);
		softs->adv_aio_capable = true;

		/* AIO transfer limits are reported in  kbytes, so x 1024.
 		 * Values of 0 mean 'no limit'.
 		 */

		softs->max_aio_write_raid5_6 =
			(features->aio_subpage.max_aio_write_raid5_6 == 0) ?
			PQISRC_MAX_AIO_NO_LIMIT :
			features->aio_subpage.max_aio_write_raid5_6 * 1024;
		softs->max_aio_write_raid1_10_2drv =
			(features->aio_subpage.max_aio_write_raid1_10_2drv
			== 0) ?  PQISRC_MAX_AIO_NO_LIMIT :
			features->aio_subpage.max_aio_write_raid1_10_2drv
			* 1024;
		softs->max_aio_write_raid1_10_3drv =
			(features->aio_subpage.max_aio_write_raid1_10_3drv
			== 0) ?  PQISRC_MAX_AIO_NO_LIMIT :
			features->aio_subpage.max_aio_write_raid1_10_3drv
			* 1024;
		softs->max_aio_rw_xfer_crypto_nvme =
			(features->aio_subpage.max_aio_rw_xfer_crypto_nvme
			== 0) ?  PQISRC_MAX_AIO_NO_LIMIT :
			features->aio_subpage.max_aio_rw_xfer_crypto_nvme
			* 1024;
		softs->max_aio_rw_xfer_crypto_sas_sata =
			(features->aio_subpage.max_aio_rw_xfer_crypto_sas_sata
			== 0) ?  PQISRC_MAX_AIO_NO_LIMIT :
			features->aio_subpage.max_aio_rw_xfer_crypto_sas_sata
			* 1024;

		DBG_INIT("softs->max_aio_write_raid5_6: 0x%x\n",
			softs->max_aio_write_raid5_6);
		DBG_INIT("softs->max_aio_write_raid1_10_2drv: 0x%x\n",
			softs->max_aio_write_raid1_10_2drv);
		DBG_INIT("softs->max_aio_write_raid1_10_3drv: 0x%x\n",
			softs->max_aio_write_raid1_10_3drv);
		DBG_INIT("softs->max_aio_rw_xfer_crypto_nvme: 0x%x\n",
			softs->max_aio_rw_xfer_crypto_nvme);
		DBG_INIT("softs->max_aio_rw_xfer_crypto_sas_sata: 0x%x\n",
			softs->max_aio_rw_xfer_crypto_sas_sata);

	} else {
		DBG_WARN("Problem getting AIO support subpage settings. "
			"Disabling advanced AIO writes.\n");
		softs->adv_aio_capable = false;
	}


	os_mem_free(softs, features, sizeof(*features));
	return ret;
free_err:
	os_mem_free(softs, features, sizeof(*features));
err:
	return PQI_STATUS_FAILURE;
}

/*
 * Initialize target ID pool for exposed physical devices .
 */
void
pqisrc_init_bitmap(pqisrc_softstate_t *softs)
{
	memset(&softs->bit_map, SLOT_AVAILABLE, sizeof(softs->bit_map));
}

void
pqisrc_remove_target_bit(pqisrc_softstate_t *softs, int target)
{
	if((target == PQI_CTLR_INDEX) || (target == INVALID_ELEM)) {
		DBG_ERR("Invalid target ID\n");
		return;
	}
	DBG_DISC("Giving back target %d\n", target);
	softs->bit_map.bit_vector[target] = SLOT_AVAILABLE;
}

/* Use bit map to find availible targets */
int
pqisrc_find_avail_target(pqisrc_softstate_t *softs)
{

	int avail_target;
	for(avail_target = 1; avail_target < MAX_TARGET_BIT; avail_target++) {
		if(softs->bit_map.bit_vector[avail_target] == SLOT_AVAILABLE){
			softs->bit_map.bit_vector[avail_target] = SLOT_TAKEN;
			DBG_DISC("Avail_target is %d\n", avail_target);
			return avail_target;
		}
	}
	DBG_ERR("No available targets\n");
	return INVALID_ELEM;
}

/* Subroutine used to set Bus-Target-Lun for the requested device */
static inline void
pqisrc_set_btl(pqi_scsi_dev_t *device, int bus, int target, int lun)
{
	DBG_FUNC("IN\n");

	device->bus = bus;
	device->target = target;
	device->lun = lun;

	DBG_FUNC("OUT\n");
}

/* Add all exposed physical devices, logical devices, controller devices, PT RAID
*  devices and multi-lun devices */
boolean_t
pqisrc_add_softs_entry(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device,
						uint8_t *scsi3addr)
{
	/* Add physical devices with targets that need
	*  targets */
	int j;
	int tid = 0;
	unsigned char addr1[8], addr2[8];
	pqi_scsi_dev_t *temp_device;

	/* If controller device, add it to list because its lun/bus/target
	*  values are already set */
	if(pqisrc_is_hba_lunid(scsi3addr))
		goto add_device_to_dev_list;

	/* If exposed physical device give it a target then add it
	*  to the dev list */
	if(!pqisrc_is_logical_device(device)) {
		tid = pqisrc_find_avail_target(softs);
		if(INVALID_ELEM != tid){
			pqisrc_set_btl(device, PQI_PHYSICAL_DEVICE_BUS, tid, 0);
			goto add_device_to_dev_list;
		}
	}

	/* If external raid device , assign target from the target pool.
	 * If a non-zero lun device, search through the list & find the
         * device which has same target (byte 2 of LUN address).
         * Assign the same target for this new lun. */
	if (pqisrc_is_external_raid_device(device)) {
		memcpy(addr1, device->scsi3addr, 8);
		for(j = 0; j < PQI_MAX_DEVICES; j++) {
			if(softs->dev_list[j] == NULL)
				continue;
			temp_device = softs->dev_list[j];
			memcpy(addr2, temp_device->scsi3addr, 8);
			if (addr1[2] == addr2[2]) {
				pqisrc_set_btl(device, PQI_EXTERNAL_RAID_VOLUME_BUS,
					temp_device->target,device->scsi3addr[0]);
				goto add_device_to_dev_list;
			}
		}
		tid = pqisrc_find_avail_target(softs);
		if(INVALID_ELEM != tid){
			pqisrc_set_btl(device, PQI_EXTERNAL_RAID_VOLUME_BUS, tid, device->scsi3addr[0]);
			goto add_device_to_dev_list;
		}
	}

	/* If logical device, add it to list because its lun/bus/target
	*  values are already set */
	if(pqisrc_is_logical_device(device) && !pqisrc_is_external_raid_device(device))
		goto add_device_to_dev_list;

	/* This is a non-zero lun of a multi-lun device.
	*  Search through our list and find the device which
	*  has the same 8 byte LUN address, except with bytes 4 and 5.
	*  Assign the same bus and target for this new LUN.
	*  Use the logical unit number from the firmware. */
	memcpy(addr1, device->scsi3addr, 8);
	addr1[4] = 0;
	addr1[5] = 0;
	for(j = 0; j < PQI_MAX_DEVICES; j++) {
		if(softs->dev_list[j] == NULL)
			continue;
		temp_device = softs->dev_list[j];
		memcpy(addr2, temp_device->scsi3addr, 8);
		addr2[4] = 0;
		addr2[5] = 0;
		/* If addresses are the same, except for bytes 4 and 5
		*  then the passed-in device is an additional lun of a
		*  previously added multi-lun device. Use the same target
		*  id as that previous device. Otherwise, use the new
		*  target id */
		if(memcmp(addr1, addr2, 8) == 0) {
			pqisrc_set_btl(device, temp_device->bus,
				temp_device->target, temp_device->scsi3addr[4]);
			goto add_device_to_dev_list;
		}
	}
	DBG_ERR("The device is not a physical, lun or ptraid device"
		"B %d: T %d: L %d\n", device->bus, device->target,
		device->lun );
	return false;

add_device_to_dev_list:
	/* Actually add the device to the driver list
	*  softs->dev_list */
	softs->num_devs++;
	for(j = 0; j < PQI_MAX_DEVICES; j++) {
		if(softs->dev_list[j])
			continue;
		softs->dev_list[j] = device;
		break;
	}
	DBG_NOTE("Added device [%d of %d]: B %d: T %d: L %d\n",
		j, softs->num_devs, device->bus, device->target,
		device->lun);
	return true;
}

/* Return a given index for a specific bus, target, lun within the
*  softs dev_list (This function is specifically for freebsd)*/
int
pqisrc_find_btl_list_index(pqisrc_softstate_t *softs,
							int bus, int target, int lun)
{

	int index;
	pqi_scsi_dev_t *temp_device;
	for(index = 0; index < PQI_MAX_DEVICES; index++) {
		if(softs->dev_list[index] == NULL)
			continue;
		temp_device = softs->dev_list[index];
		/* Match the devices then return the location
		*  of that device for further use*/
		if(bus == softs->bus_id &&
			target == temp_device->target &&
			lun == temp_device->lun){
			DBG_DISC("Returning device list index %d\n", index);
			return index;

		}
		if ((temp_device->is_physical_device) && (target == temp_device->target)
					&& (temp_device->is_multi_lun)) {
			return index;
		}
	}
	return INVALID_ELEM;
}

/* Return a given index for a specific device within the
*  softs dev_list */
int
pqisrc_find_device_list_index(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{

	int index;
	pqi_scsi_dev_t *temp_device;
	for(index = 0; index < PQI_MAX_DEVICES; index++) {
		if(softs->dev_list[index] == NULL)
			continue;
		temp_device = softs->dev_list[index];
		/* Match the devices then return the location
		*  of that device for further use*/
		if(device->bus == temp_device->bus &&
			device->target == temp_device->target
			&& device->lun == temp_device->lun){
			DBG_DISC("Returning device list index %d\n", index);
			return index;

		}
	}
	return INVALID_ELEM;
}

/* Delete a given device from the softs dev_list*/
int
pqisrc_delete_softs_entry(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{

	int index;
	index = pqisrc_find_device_list_index(softs, device);
	if (0 <= index && index < MAX_TARGET_BIT) {
		softs->dev_list[index] = NULL;
		softs->num_devs--;
		DBG_NOTE("Removing device : B %d: T %d: L %d positioned at %d\n",
				device->bus, device->target, device->lun, softs->num_devs);
		return PQI_STATUS_SUCCESS;
	}
	if (index == INVALID_ELEM) {
		DBG_NOTE("Invalid device, either it was already removed "
				"or never added\n");
		return PQI_STATUS_FAILURE;
	}
	DBG_ERR("This is a bogus device\n");
	return PQI_STATUS_FAILURE;
}

int
pqisrc_simple_dma_alloc(pqisrc_softstate_t *softs, struct dma_mem *device_mem,
					    size_t datasize, sgt_t *sgd)
{
	int ret = PQI_STATUS_SUCCESS;

	memset(device_mem, 0, sizeof(struct dma_mem));

	/* for TUR datasize: 0 buff: NULL */
	if (datasize) {

		os_strlcpy(device_mem->tag, "device_mem", sizeof(device_mem->tag));
		device_mem->size = datasize;
		device_mem->align = PQISRC_DEFAULT_DMA_ALIGN;

		ret = os_dma_mem_alloc(softs, device_mem);

		if (ret) {
			DBG_ERR("failed to allocate dma memory for device_mem return code %d\n", ret);
			return ret;
		}

		ASSERT(device_mem->size == datasize);

		sgd->addr = device_mem->dma_addr;
		sgd->len = datasize;
		sgd->flags = SG_FLAG_LAST;

	}

	return ret;
}

/*
 * Function used to build the internal raid request and analyze the response
 */
static int
pqisrc_build_send_raid_request(pqisrc_softstate_t *softs, struct dma_mem device_mem,
							   pqisrc_raid_req_t *request, void *buff,
							   size_t datasize, uint8_t cmd, uint8_t *scsi3addr,
							   raid_path_error_info_elem_t *error_info)
{

	uint32_t tag = 0;
	int ret = PQI_STATUS_SUCCESS;

	ib_queue_t *ib_q = &softs->op_raid_ib_q[PQI_DEFAULT_IB_QUEUE];
	ob_queue_t *ob_q = &softs->op_ob_q[PQI_DEFAULT_IB_QUEUE];

	rcb_t *rcb = NULL;

	/* Build raid path request */
	request->header.iu_type = PQI_IU_TYPE_RAID_PATH_IO_REQUEST;

	request->header.iu_length = LE_16(offsetof(pqisrc_raid_req_t,
							sg_descriptors[1]) - PQI_REQUEST_HEADER_LENGTH);
	request->buffer_length = LE_32(datasize);
	memcpy(request->lun_number, scsi3addr, sizeof(request->lun_number));
	request->task_attribute = SOP_TASK_ATTRIBUTE_SIMPLE;
	request->additional_cdb_bytes_usage = PQI_ADDITIONAL_CDB_BYTES_0;

	tag = pqisrc_get_tag(&softs->taglist);
	if (INVALID_ELEM == tag) {
		DBG_ERR("Tag not available\n");
		ret = PQI_STATUS_FAILURE;
		goto err_notag;
	}

	((pqisrc_raid_req_t *)request)->request_id = tag;
	((pqisrc_raid_req_t *)request)->error_index = ((pqisrc_raid_req_t *)request)->request_id;
	((pqisrc_raid_req_t *)request)->response_queue_id = ob_q->q_id;
	rcb = &softs->rcb[tag];
	rcb->success_cmp_callback = pqisrc_process_internal_raid_response_success;
	rcb->error_cmp_callback = pqisrc_process_internal_raid_response_error;

	rcb->req_pending = true;
	rcb->tag = tag;
	/* Submit Command */
	ret = pqisrc_submit_cmnd(softs, ib_q, request);

	if (ret != PQI_STATUS_SUCCESS) {
		DBG_ERR("Unable to submit command\n");
		goto err_out;
	}

	ret = pqisrc_wait_on_condition(softs, rcb, PQISRC_CMD_TIMEOUT);
	if (ret != PQI_STATUS_SUCCESS) {
		DBG_ERR("Internal RAID request timed out: cmd : 0x%c\n", cmd);
		goto err_out;
	}

	if (datasize) {
		if (buff) {
			memcpy(buff, device_mem.virt_addr, datasize);
		}
		os_dma_mem_free(softs, &device_mem);
	}

	ret = rcb->status;
	if (ret) {
		if(error_info) {
			memcpy(error_info,
			       rcb->error_info,
			       sizeof(*error_info));

			if (error_info->data_out_result ==
			    PQI_RAID_DATA_IN_OUT_UNDERFLOW) {
				ret = PQI_STATUS_SUCCESS;
			}
			else{
				DBG_WARN("Bus=%u Target=%u, Cmd=0x%x,"
					"Ret=%d\n", BMIC_GET_LEVEL_2_BUS(scsi3addr),
					BMIC_GET_LEVEL_TWO_TARGET(scsi3addr),
					cmd, ret);
				ret = PQI_STATUS_FAILURE;
			}
		}
	} else {
		if(error_info) {
			ret = PQI_STATUS_SUCCESS;
			memset(error_info, 0, sizeof(*error_info));
		}
	}

	os_reset_rcb(rcb);
	pqisrc_put_tag(&softs->taglist, ((pqisrc_raid_req_t *)request)->request_id);
	DBG_FUNC("OUT\n");
	return ret;

err_out:
	DBG_ERR("Error!! Bus=%u Target=%u, Cmd=0x%x, Ret=%d\n",
		BMIC_GET_LEVEL_2_BUS(scsi3addr), BMIC_GET_LEVEL_TWO_TARGET(scsi3addr),
		cmd, ret);
	os_reset_rcb(rcb);
	pqisrc_put_tag(&softs->taglist, ((pqisrc_raid_req_t *)request)->request_id);
err_notag:
	if (datasize)
		os_dma_mem_free(softs, &device_mem);
	DBG_FUNC("FAILED \n");
	return ret;
}

/* Use this if you need to specify specific target or if you want error info */
int
pqisrc_prepare_send_raid(pqisrc_softstate_t *softs, pqisrc_raid_req_t *request,
    					 void *buff, size_t datasize, uint8_t *scsi3addr,
						 raid_path_error_info_elem_t *error_info)
{
	struct dma_mem device_mem;
	int ret = PQI_STATUS_SUCCESS;
	uint8_t cmd = IS_BMIC_OPCODE(request->cmd.cdb[0]) ? request->cmd.cdb[6] : request->cmd.cdb[0];

	ret = pqisrc_simple_dma_alloc(softs, &device_mem, datasize, request->sg_descriptors);
	if (PQI_STATUS_SUCCESS != ret){
		DBG_ERR("failed to allocate dma memory for device_mem return code %d\n", ret);
		return ret;
	}

	/* If we are sending out data, copy it over to dma buf */
	if (datasize && buff && request->data_direction == SOP_DATA_DIR_FROM_DEVICE)
		memcpy(device_mem.virt_addr, buff, datasize);

	ret = pqisrc_build_send_raid_request(softs, device_mem, request, buff, datasize,
		cmd, scsi3addr, error_info);

	return ret;
}

/* Use this to target controller and don't care about error info */
int
pqisrc_prepare_send_ctrlr_request(pqisrc_softstate_t *softs, pqisrc_raid_req_t *request,
    							  void *buff, size_t datasize)
{
   raid_path_error_info_elem_t error_info; /* will be thrown away */
   uint8_t *scsi3addr = RAID_CTLR_LUNID;

   return pqisrc_prepare_send_raid(softs, request, buff, datasize, scsi3addr, &error_info);
}

/* common function used to send report physical and logical luns cmds */
static int
pqisrc_report_luns(pqisrc_softstate_t *softs, uint8_t cmd,
	void *buff, size_t buf_len)
{
	int ret;
	pqisrc_raid_req_t request;

	DBG_FUNC("IN\n");

	memset(&request, 0, sizeof(request));

	request.data_direction = SOP_DATA_DIR_TO_DEVICE;

	switch (cmd) {
	case SA_REPORT_LOG:
		request.cmd.cdb[0] = SA_REPORT_LOG;
		request.cmd.cdb[1] = SA_REPORT_LOG_EXTENDED;
		break;
	case SA_REPORT_PHYS:
		request.cmd.cdb[0] = SA_REPORT_PHYS;
		request.cmd.cdb[1] = SA_REPORT_PHYS_EXTENDED;
		break;
   /* @todo: 0x56 does not exist, this is kludgy, need to pass in options */
	case PQI_LOG_EXT_QUEUE_ENABLE:
		request.cmd.cdb[0] = SA_REPORT_LOG;
		request.cmd.cdb[1] = (PQI_LOG_EXT_QUEUE_DEPTH_ENABLED | SA_REPORT_LOG_EXTENDED);
		break;
	}

	request.cmd.cdb[8] = (uint8_t)((buf_len) >> 8);
	request.cmd.cdb[9] = (uint8_t)buf_len;

	ret = pqisrc_prepare_send_ctrlr_request(softs, &request, buff, buf_len);

	DBG_FUNC("OUT\n");

	return ret;
}

/* subroutine used to get physical and logical luns of the device */
int
pqisrc_get_physical_logical_luns(pqisrc_softstate_t *softs, uint8_t cmd,
		reportlun_data_ext_t **buff, size_t *data_length)
{
	int ret;
	size_t list_len;
	size_t data_len;
	size_t new_lun_list_length;
	reportlun_data_ext_t *lun_data;
	reportlun_header_t report_lun_header;

	DBG_FUNC("IN\n");

	ret = pqisrc_report_luns(softs, cmd, &report_lun_header,
		sizeof(report_lun_header));

	if (ret) {
		DBG_ERR("failed return code: %d\n", ret);
		return ret;
	}
	list_len = BE_32(report_lun_header.list_length);

retry:
	data_len = sizeof(reportlun_header_t) + list_len;
	*data_length = data_len;

	lun_data = os_mem_alloc(softs, data_len);

	if (!lun_data) {
		DBG_ERR("failed to allocate memory for lun_data\n");
		return PQI_STATUS_FAILURE;
	}

	if (list_len == 0) {
		DBG_DISC("list_len is 0\n");
		memcpy(lun_data, &report_lun_header, sizeof(report_lun_header));
		goto out;
	}

	ret = pqisrc_report_luns(softs, cmd, lun_data, data_len);

	if (ret) {
		DBG_ERR("error\n");
		goto error;
	}

	new_lun_list_length = BE_32(lun_data->header.list_length);

	if (new_lun_list_length > list_len) {
		list_len = new_lun_list_length;
		os_mem_free(softs, (void *)lun_data, data_len);
		goto retry;
	}

out:
	*buff = lun_data;
	DBG_FUNC("OUT\n");
	return 0;

error:
	os_mem_free(softs, (void *)lun_data, data_len);
	DBG_ERR("FAILED\n");
	return ret;
}

/*
 * Function used to grab queue depth ext lun data for logical devices
 */
static int
pqisrc_get_queue_lun_list(pqisrc_softstate_t *softs, uint8_t cmd,
                reportlun_queue_depth_data_t **buff, size_t *data_length)
{
        int ret;
        size_t list_len;
        size_t data_len;
        size_t new_lun_list_length;
        reportlun_queue_depth_data_t *lun_data;
        reportlun_header_t report_lun_header;

        DBG_FUNC("IN\n");

        ret = pqisrc_report_luns(softs, cmd, &report_lun_header,
                sizeof(report_lun_header));

        if (ret) {
                DBG_ERR("failed return code: %d\n", ret);
                return ret;
        }
        list_len = BE_32(report_lun_header.list_length);
retry:
        data_len = sizeof(reportlun_header_t) + list_len;
        *data_length = data_len;
        lun_data = os_mem_alloc(softs, data_len);

	if (!lun_data) {
                DBG_ERR("failed to allocate memory for lun_data\n");
                return PQI_STATUS_FAILURE;
        }

        if (list_len == 0) {
                DBG_DISC("list_len is 0\n");
                memcpy(lun_data, &report_lun_header, sizeof(report_lun_header));
                goto out;
        }
        ret = pqisrc_report_luns(softs, cmd, lun_data, data_len);

        if (ret) {
                DBG_ERR("error\n");
                goto error;
        }
        new_lun_list_length = BE_32(lun_data->header.list_length);

        if (new_lun_list_length > list_len) {
                list_len = new_lun_list_length;
                os_mem_free(softs, (void *)lun_data, data_len);
                goto retry;
        }

out:
        *buff = lun_data;
        DBG_FUNC("OUT\n");
        return 0;

error:
        os_mem_free(softs, (void *)lun_data, data_len);
        DBG_ERR("FAILED\n");
        return ret;
}

/*
 * Function used to get physical and logical device list
 */
static int
pqisrc_get_phys_log_device_list(pqisrc_softstate_t *softs,
	reportlun_data_ext_t **physical_dev_list,
	reportlun_data_ext_t **logical_dev_list,
	reportlun_queue_depth_data_t **queue_dev_list,
	size_t *queue_data_length,
	size_t *phys_data_length,
	size_t *log_data_length)
{
	int ret = PQI_STATUS_SUCCESS;
	size_t logical_list_length;
	size_t logdev_data_length;
	size_t data_length;
	reportlun_data_ext_t *local_logdev_list;
	reportlun_data_ext_t *logdev_data;
	reportlun_header_t report_lun_header;

	DBG_FUNC("IN\n");

	ret = pqisrc_get_physical_logical_luns(softs, SA_REPORT_PHYS, physical_dev_list, phys_data_length);
	if (ret) {
		DBG_ERR("report physical LUNs failed");
		return ret;
	}

	ret = pqisrc_get_physical_logical_luns(softs, SA_REPORT_LOG, logical_dev_list, log_data_length);
	if (ret) {
		DBG_ERR("report logical LUNs failed");
		return ret;
	}

#ifdef PQI_NEED_RESCAN_TIMER_FOR_RBOD_HOTPLUG
	/* Save the report_log_dev buffer for deciding rescan requirement from OS driver*/
	if(softs->log_dev_data_length != *log_data_length) {
		if(softs->log_dev_list)
			os_mem_free(softs, softs->log_dev_list, softs->log_dev_data_length);
		softs->log_dev_list = os_mem_alloc(softs, *log_data_length);
	}
	memcpy(softs->log_dev_list, *logical_dev_list, *log_data_length);
	softs->log_dev_data_length = *log_data_length;
#endif

	ret = pqisrc_get_queue_lun_list(softs, PQI_LOG_EXT_QUEUE_ENABLE, queue_dev_list, queue_data_length);
	if (ret) {
		DBG_ERR("report logical LUNs failed");
		return ret;
	}

	logdev_data = *logical_dev_list;

	if (logdev_data) {
		logical_list_length =
			BE_32(logdev_data->header.list_length);
	} else {
		memset(&report_lun_header, 0, sizeof(report_lun_header));
		logdev_data =
			(reportlun_data_ext_t *)&report_lun_header;
		logical_list_length = 0;
	}

	logdev_data_length = sizeof(reportlun_header_t) +
		logical_list_length;

	/* Adding LOGICAL device entry for controller */
	local_logdev_list = os_mem_alloc(softs,
					    logdev_data_length + sizeof(reportlun_ext_entry_t));
	if (!local_logdev_list) {
		data_length = *log_data_length;
		os_mem_free(softs, (char *)*logical_dev_list, data_length);
		*logical_dev_list = NULL;
		return PQI_STATUS_FAILURE;
	}

	memcpy(local_logdev_list, logdev_data, logdev_data_length);
	memset((uint8_t *)local_logdev_list + logdev_data_length, 0,
		sizeof(reportlun_ext_entry_t));
	local_logdev_list->header.list_length = BE_32(logical_list_length +
							sizeof(reportlun_ext_entry_t));
	data_length = *log_data_length;
	os_mem_free(softs, (char *)*logical_dev_list, data_length);
	*log_data_length = logdev_data_length + sizeof(reportlun_ext_entry_t);
	*logical_dev_list = local_logdev_list;

	DBG_FUNC("OUT\n");

	return ret;
}

inline boolean_t
pqisrc_is_external_raid_device(pqi_scsi_dev_t *device)
{
	return device->is_external_raid_device;
}

static inline boolean_t
pqisrc_is_external_raid_addr(uint8_t *scsi3addr)
{
	return scsi3addr[2] != 0;
}

/* Function used to assign Bus-Target-Lun for the requested device */
static void
pqisrc_assign_btl(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	uint8_t *scsi3addr;
	uint32_t lunid;
	uint32_t bus;
	uint32_t target;
	uint32_t lun;
	DBG_FUNC("IN\n");

	scsi3addr = device->scsi3addr;
	lunid = GET_LE32(scsi3addr);

	if (pqisrc_is_hba_lunid(scsi3addr)) {
	/* The specified device is the controller. */
		pqisrc_set_btl(device, PQI_HBA_BUS, PQI_CTLR_INDEX, (lunid & 0x3fff));
		device->target_lun_valid = true;
		return;
	}

	/* When the specified device is a logical volume,
	*  physicals will be given targets in pqisrc update
	*  device list in pqisrc scan devices. */
	if (pqisrc_is_logical_device(device)) {
			bus = PQI_RAID_VOLUME_BUS;
			lun = (lunid & 0x3fff) + 1;
			target = 0;
			pqisrc_set_btl(device, bus, target, lun);
			device->target_lun_valid = true;
			return;
	}

	DBG_FUNC("OUT\n");
}

/* Build and send the internal INQUIRY command to particular device */
int
pqisrc_send_scsi_inquiry(pqisrc_softstate_t *softs,
	uint8_t *scsi3addr, uint16_t vpd_page, uint8_t *buff, int buf_len)
{
	int ret = PQI_STATUS_SUCCESS;
	pqisrc_raid_req_t request;
	raid_path_error_info_elem_t error_info;

	DBG_FUNC("IN\n");

	memset(&request, 0, sizeof(request));

	request.data_direction = SOP_DATA_DIR_TO_DEVICE;
	request.cmd.cdb[0] = SA_INQUIRY;
	if (vpd_page & VPD_PAGE) {
		request.cmd.cdb[1] = 0x1;
		request.cmd.cdb[2] = (uint8_t)vpd_page;
	}
	ASSERT(buf_len < 256);
	request.cmd.cdb[4] = (uint8_t)buf_len;

	if (softs->timeout_in_passthrough) {
		request.timeout_in_sec = PQISRC_INQUIRY_TIMEOUT;
	}

	pqisrc_prepare_send_raid(softs, &request, buff, buf_len, scsi3addr, &error_info);

	DBG_FUNC("OUT\n");
	return ret;
}

/* Determine logical volume status from vpd buffer.*/
static void pqisrc_get_dev_vol_status(pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *device)
{
	int ret;
	uint8_t status = SA_LV_STATUS_VPD_UNSUPPORTED;
	uint8_t vpd_size = sizeof(vpd_volume_status);
	uint8_t offline = true;
	size_t page_length;
	vpd_volume_status *vpd;

	DBG_FUNC("IN\n");

	vpd = os_mem_alloc(softs, vpd_size);
	if (vpd == NULL)
		goto out;

	/* Get the size of the VPD return buff. */
	ret = pqisrc_send_scsi_inquiry(softs, device->scsi3addr, VPD_PAGE | SA_VPD_LV_STATUS,
		(uint8_t *)vpd, vpd_size);

	if (ret) {
		DBG_WARN("Inquiry returned failed status\n");
		goto out;
	}

	if (vpd->page_code != SA_VPD_LV_STATUS) {
		DBG_WARN("Returned invalid buffer\n");
		goto out;
	}

	page_length = offsetof(vpd_volume_status, volume_status) + vpd->page_length;
	if (page_length < vpd_size)
		goto out;

	status = vpd->volume_status;
	offline = (vpd->flags & SA_LV_FLAGS_NO_HOST_IO)!=0;

out:
	device->volume_offline = offline;
	device->volume_status = status;

	os_mem_free(softs, (char *)vpd, vpd_size);

	DBG_FUNC("OUT\n");

	return;
}


/* Validate the RAID map parameters */
static int
pqisrc_raid_map_validation(pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *device, pqisrc_raid_map_t *raid_map)
{
	char *error_msg;
	uint32_t raidmap_size;
	uint32_t r5or6_blocks_per_row;
/*	unsigned phys_dev_num; */

	DBG_FUNC("IN\n");

	raidmap_size = LE_32(raid_map->structure_size);
	if (raidmap_size < offsetof(pqisrc_raid_map_t, dev_data)) {
		error_msg = "RAID map too small\n";
		goto error;
	}

#if 0
	phys_dev_num = LE_16(raid_map->layout_map_count) *
	(LE_16(raid_map->data_disks_per_row) +
	LE_16(raid_map->metadata_disks_per_row));
#endif

	if (device->raid_level == SA_RAID_1) {
		if (LE_16(raid_map->layout_map_count) != 2) {
			error_msg = "invalid RAID-1 map\n";
			goto error;
		}
	} else if (device->raid_level == SA_RAID_ADM) {
		if (LE_16(raid_map->layout_map_count) != 3) {
			error_msg = "invalid RAID-1(triple) map\n";
			goto error;
		}
	} else if ((device->raid_level == SA_RAID_5 ||
		device->raid_level == SA_RAID_6) &&
		LE_16(raid_map->layout_map_count) > 1) {
		/* RAID 50/60 */
		r5or6_blocks_per_row =
			LE_16(raid_map->strip_size) *
			LE_16(raid_map->data_disks_per_row);
		if (r5or6_blocks_per_row == 0) {
			error_msg = "invalid RAID-5 or RAID-6 map\n";
			goto error;
		}
	}

	DBG_FUNC("OUT\n");

	return 0;

error:
	DBG_NOTE("%s\n", error_msg);
	return PQI_STATUS_FAILURE;
}

/* Get device raidmap for the requested device */
static int
pqisrc_get_device_raidmap(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	int ret = PQI_STATUS_SUCCESS;
	int raidmap_alloc_size = sizeof(pqisrc_raid_map_t);
	int raidmap_reported_size;
	int structure_size;
	int ii;
	int *next_offload_to_mirror;

	pqisrc_raid_req_t request;
	pqisrc_raid_map_t *raid_map;

	DBG_FUNC("IN\n");

	for (ii = 0; ii < 2; ii++)
	{
		raid_map = os_mem_alloc(softs, raidmap_alloc_size);
		if (!raid_map)
			return PQI_STATUS_FAILURE;

		memset(&request, 0, sizeof(request));
		request.data_direction = SOP_DATA_DIR_TO_DEVICE;
		request.cmd.cdb[0] = SA_CISS_READ;
		request.cmd.cdb[1] = SA_GET_RAID_MAP;
		request.cmd.cdb[8] = (uint8_t)((raidmap_alloc_size) >> 8);
		request.cmd.cdb[9] = (uint8_t)(raidmap_alloc_size);

		ret = pqisrc_prepare_send_raid(softs, &request, raid_map, raidmap_alloc_size, device->scsi3addr, NULL);

		if (ret) {
			DBG_ERR("error in build send raid req ret=%d\n", ret);
			goto err_out;
		}

		raidmap_reported_size = LE_32(raid_map->structure_size);
		if (raidmap_reported_size <= raidmap_alloc_size)
			break;

		DBG_NOTE("Raid map is larger than 1024 entries, request once again");
		os_mem_free(softs, (char*)raid_map, raidmap_alloc_size);

		raidmap_alloc_size = raidmap_reported_size;
	}

	ret = pqisrc_raid_map_validation(softs, device, raid_map);
	if (ret) {
		DBG_NOTE("error in raid map validation ret=%d\n", ret);
		goto err_out;
	}

	structure_size = raid_map->data_disks_per_row * sizeof(*next_offload_to_mirror);
	next_offload_to_mirror = os_mem_alloc(softs, structure_size);
	if (!next_offload_to_mirror) {
		ret = PQI_STATUS_FAILURE;
		goto err_out;
	}

	device->raid_map = raid_map;
	device->offload_to_mirror = next_offload_to_mirror;
	DBG_FUNC("OUT\n");
	return 0;

err_out:
	os_mem_free(softs, (char*)raid_map, sizeof(*raid_map));
	DBG_FUNC("FAILED \n");
	return ret;
}

/* Get device ioaccel_status to validate the type of device */
static void
pqisrc_get_dev_ioaccel_status(pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *device)
{
	int ret = PQI_STATUS_SUCCESS;
	uint8_t *buff;
	uint8_t ioaccel_status;

	DBG_FUNC("IN\n");

	buff = os_mem_alloc(softs, 64);
	if (!buff)
		return;

	ret = pqisrc_send_scsi_inquiry(softs, device->scsi3addr,
					VPD_PAGE | SA_VPD_LV_IOACCEL_STATUS, buff, 64);
	if (ret) {
		DBG_ERR("error in send scsi inquiry ret=%d\n", ret);
		goto err_out;
	}

	ioaccel_status = buff[IOACCEL_STATUS_BYTE];
	device->offload_config =
		!!(ioaccel_status & OFFLOAD_CONFIGURED_BIT);

	if (device->offload_config) {
		device->offload_enabled_pending =
			!!(ioaccel_status & OFFLOAD_ENABLED_BIT);
		if (pqisrc_get_device_raidmap(softs, device))
			device->offload_enabled_pending = false;
	}

	DBG_DISC("offload_config: 0x%x offload_enabled_pending: 0x%x \n",
			device->offload_config, device->offload_enabled_pending);

err_out:
	os_mem_free(softs, (char*)buff, 64);
	DBG_FUNC("OUT\n");
}

/* Get RAID level of requested device */
static void
pqisrc_get_dev_raid_level(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	uint8_t raid_level;
	uint8_t *buff;

	DBG_FUNC("IN\n");

	raid_level = SA_RAID_UNKNOWN;

	buff = os_mem_alloc(softs, 64);
	if (buff) {
		int ret;
		ret = pqisrc_send_scsi_inquiry(softs, device->scsi3addr,
			VPD_PAGE | SA_VPD_LV_DEVICE_GEOMETRY, buff, 64);
		if (ret == 0) {
			raid_level = buff[8];
			if (raid_level > SA_RAID_MAX)
				raid_level = SA_RAID_UNKNOWN;
		}
		os_mem_free(softs, (char*)buff, 64);
	}

	device->raid_level = raid_level;
	DBG_DISC("RAID LEVEL: %x \n",  raid_level);
	DBG_FUNC("OUT\n");
}

/* Parse the inquiry response and determine the type of device */
static int
pqisrc_get_dev_data(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	int ret = PQI_STATUS_SUCCESS;
	uint8_t *inq_buff;
	int retry = 3;

	DBG_FUNC("IN\n");

	inq_buff = os_mem_alloc(softs, OBDR_TAPE_INQ_SIZE);
	if (!inq_buff)
		return PQI_STATUS_FAILURE;

	while(retry--) {
		/* Send an inquiry to the device to see what it is. */
		ret = pqisrc_send_scsi_inquiry(softs, device->scsi3addr, 0, inq_buff,
			OBDR_TAPE_INQ_SIZE);
		if (!ret)
			break;
		DBG_WARN("Retrying inquiry !!!\n");
	}
	if(retry <= 0)
		goto err_out;
	pqisrc_sanitize_inquiry_string(&inq_buff[8], 8);
	pqisrc_sanitize_inquiry_string(&inq_buff[16], 16);

	device->devtype = inq_buff[0] & 0x1f;
	memcpy(device->vendor, &inq_buff[8],
		sizeof(device->vendor));
	memcpy(device->model, &inq_buff[16],
		sizeof(device->model));
	DBG_DISC("DEV_TYPE: %x VENDOR: %.8s MODEL: %.16s\n",  device->devtype, device->vendor, device->model);

	if (pqisrc_is_logical_device(device) && device->devtype == DISK_DEVICE) {
		if (pqisrc_is_external_raid_device(device)) {
			device->raid_level = SA_RAID_UNKNOWN;
			device->volume_status = SA_LV_OK;
			device->volume_offline = false;
		}
		else {
			pqisrc_get_dev_raid_level(softs, device);
			pqisrc_get_dev_ioaccel_status(softs, device);
			pqisrc_get_dev_vol_status(softs, device);
		}
	}

	/*
	 * Check if this is a One-Button-Disaster-Recovery device
	 * by looking for "$DR-10" at offset 43 in the inquiry data.
	 */
	device->is_obdr_device = (device->devtype == ROM_DEVICE &&
		memcmp(&inq_buff[OBDR_SIG_OFFSET], OBDR_TAPE_SIG,
			OBDR_SIG_LEN) == 0);
err_out:
	os_mem_free(softs, (char*)inq_buff, OBDR_TAPE_INQ_SIZE);

	DBG_FUNC("OUT\n");
	return ret;
}

/*
 * BMIC (Basic Management And Interface Commands) command
 * to get the controller identify params
 */
static int
pqisrc_identify_ctrl(pqisrc_softstate_t *softs, bmic_ident_ctrl_t *buff)
{
	int ret = PQI_STATUS_SUCCESS;
	pqisrc_raid_req_t request;

	DBG_FUNC("IN\n");

	memset(&request, 0, sizeof(request));

	request.data_direction = SOP_DATA_DIR_TO_DEVICE;
	request.cmd.bmic_cdb.op_code = BMIC_READ;
	request.cmd.bmic_cdb.cmd = BMIC_IDENTIFY_CONTROLLER;
	request.cmd.bmic_cdb.xfer_len = BE_16(sizeof(*buff));

	ret = pqisrc_prepare_send_ctrlr_request(softs, &request, buff, sizeof(*buff));

	DBG_FUNC("OUT\n");

	return ret;
}

/* Get the adapter FW version using BMIC_IDENTIFY_CONTROLLER */
int
pqisrc_get_ctrl_fw_version(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_SUCCESS;
	bmic_ident_ctrl_t *identify_ctrl;

	DBG_FUNC("IN\n");

	identify_ctrl = os_mem_alloc(softs, sizeof(*identify_ctrl));
	if (!identify_ctrl) {
		DBG_ERR("failed to allocate memory for identify_ctrl\n");
		return PQI_STATUS_FAILURE;
	}

	memset(identify_ctrl, 0, sizeof(*identify_ctrl));

	ret = pqisrc_identify_ctrl(softs, identify_ctrl);
	if (ret)
		goto out;

	softs->fw_build_number = identify_ctrl->fw_build_number;
	memcpy(softs->fw_version, identify_ctrl->fw_version,
		sizeof(identify_ctrl->fw_version));
	softs->fw_version[sizeof(identify_ctrl->fw_version)] = '\0';
	snprintf(softs->fw_version +
		strlen(softs->fw_version),
		sizeof(softs->fw_version),
		"-%u", identify_ctrl->fw_build_number);
out:
	os_mem_free(softs, (char *)identify_ctrl, sizeof(*identify_ctrl));
	DBG_NOTE("Firmware version: %s Firmware build number: %d\n", softs->fw_version, softs->fw_build_number);
	DBG_FUNC("OUT\n");
	return ret;
}

/* BMIC command to determine scsi device identify params */
static int
pqisrc_identify_physical_disk(pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *device,
	bmic_ident_physdev_t *buff,
	int buf_len)
{
	int ret = PQI_STATUS_SUCCESS;
	uint16_t bmic_device_index;
	pqisrc_raid_req_t request;


	DBG_FUNC("IN\n");

	memset(&request, 0, sizeof(request));
	bmic_device_index = BMIC_GET_DRIVE_NUMBER(device->scsi3addr);

	request.data_direction = SOP_DATA_DIR_TO_DEVICE;
	request.cmd.bmic_cdb.op_code = BMIC_READ;
	request.cmd.bmic_cdb.cmd = BMIC_IDENTIFY_PHYSICAL_DEVICE;
	request.cmd.bmic_cdb.xfer_len = BE_16(buf_len);
	request.cmd.cdb[2] = (uint8_t)bmic_device_index;
	request.cmd.cdb[9] = (uint8_t)(bmic_device_index >> 8);

	ret = pqisrc_prepare_send_ctrlr_request(softs, &request, buff, buf_len);

	DBG_FUNC("OUT\n");
	return ret;
}

/*
 * Function used to get the scsi device information using one of BMIC
 * BMIC_IDENTIFY_PHYSICAL_DEVICE
 */
static void
pqisrc_get_physical_device_info(pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *device,
	bmic_ident_physdev_t *id_phys)
{
	int ret = PQI_STATUS_SUCCESS;

	DBG_FUNC("IN\n");
	memset(id_phys, 0, sizeof(*id_phys));

	ret= pqisrc_identify_physical_disk(softs, device,
		id_phys, sizeof(*id_phys));
	if (ret) {
		device->queue_depth = PQI_PHYSICAL_DISK_DEFAULT_MAX_QUEUE_DEPTH;
		return;
	}

	device->queue_depth =
		LE_16(id_phys->current_queue_depth_limit);
	device->device_type = id_phys->device_type;
	device->active_path_index = id_phys->active_path_number;
	device->path_map = id_phys->redundant_path_present_map;
	memcpy(&device->box,
		&id_phys->alternate_paths_phys_box_on_port,
		sizeof(device->box));
	memcpy(&device->phys_connector,
		&id_phys->alternate_paths_phys_connector,
		sizeof(device->phys_connector));
	device->bay = id_phys->phys_bay_in_box;
	if (id_phys->multi_lun_device_lun_count) {
		device->is_multi_lun = true;
	}

	DBG_DISC("BMIC DEV_TYPE: %x QUEUE DEPTH: 0x%x \n",  device->device_type, device->queue_depth);
	DBG_FUNC("OUT\n");
}


/* Function used to find the entry of the device in a list */
static device_status_t
pqisrc_scsi_find_entry(pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *device_to_find,	pqi_scsi_dev_t **same_device)
{
	pqi_scsi_dev_t *device;
	int i;
	DBG_FUNC("IN\n");
	for(i = 0; i < PQI_MAX_DEVICES; i++) {
		device = softs->dev_list[i];
		if(device == NULL)
			continue;
		if (pqisrc_scsi3addr_equal(device_to_find->scsi3addr,
			device->scsi3addr)) {
			*same_device = device;
			if (device->in_remove == true)
				return DEVICE_IN_REMOVE;
			if (pqisrc_device_equal(device_to_find, device)) {
				if (device_to_find->volume_offline)
					return DEVICE_CHANGED;
				return DEVICE_UNCHANGED;
			}
			return DEVICE_CHANGED;
		}
	}
	DBG_FUNC("OUT\n");

	return DEVICE_NOT_FOUND;
}


/* Update the newly added devices as existed device */
static void
pqisrc_exist_device_update(pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *device_exist, pqi_scsi_dev_t *new_device)
{
	DBG_FUNC("IN\n");
	device_exist->expose_device = new_device->expose_device;
	memcpy(device_exist->vendor, new_device->vendor,
		sizeof(device_exist->vendor));
	memcpy(device_exist->model, new_device->model,
		sizeof(device_exist->model));
	device_exist->is_physical_device = new_device->is_physical_device;
	device_exist->is_external_raid_device =
		new_device->is_external_raid_device;
	/* Whenever a logical device expansion happens, reprobe of
	 * all existing LDs will be triggered, which is resulting
	 * in updating the size to the os. */
	if ((softs->ld_rescan) && (pqisrc_is_logical_device(device_exist))) {
		device_exist->scsi_rescan = true;
	}

	device_exist->sas_address = new_device->sas_address;
	device_exist->raid_level = new_device->raid_level;
	device_exist->queue_depth = new_device->queue_depth;
	device_exist->ioaccel_handle = new_device->ioaccel_handle;
	device_exist->volume_status = new_device->volume_status;
	device_exist->active_path_index = new_device->active_path_index;
	device_exist->path_map = new_device->path_map;
	device_exist->bay = new_device->bay;
	memcpy(device_exist->box, new_device->box,
		sizeof(device_exist->box));
	memcpy(device_exist->phys_connector, new_device->phys_connector,
		sizeof(device_exist->phys_connector));
	device_exist->offload_config = new_device->offload_config;
	device_exist->offload_enabled_pending =
		new_device->offload_enabled_pending;
	if (device_exist->offload_to_mirror)
		os_mem_free(softs,
			(int *) device_exist->offload_to_mirror,
			sizeof(*(device_exist->offload_to_mirror)));
	device_exist->offload_to_mirror = new_device->offload_to_mirror;
	if (device_exist->raid_map)
		os_mem_free(softs,
			(char *)device_exist->raid_map,
			sizeof(*device_exist->raid_map));
	device_exist->raid_map = new_device->raid_map;
	/* To prevent these from being freed later. */
	new_device->raid_map = NULL;
	new_device->offload_to_mirror = NULL;
	DBG_FUNC("OUT\n");
}

/* Function used to add a scsi device to OS scsi subsystem */
static int
pqisrc_add_device(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	DBG_FUNC("IN\n");
	DBG_NOTE("vendor: %s model: %s bus:%d target:%d lun:%d is_physical_device:0x%x expose_device:0x%x volume_offline 0x%x volume_status 0x%x \n",
		device->vendor, device->model, device->bus, device->target, device->lun, device->is_physical_device, device->expose_device, device->volume_offline, device->volume_status);

	device->invalid = false;
	device->schedule_rescan = false;
	device->softs = softs;
	device->in_remove = false;

	if(device->expose_device) {
		pqisrc_init_device_active_io(softs, device);
		/* TBD: Call OS upper layer function to add the device entry */
		os_add_device(softs,device);
	}
	DBG_FUNC("OUT\n");
	return PQI_STATUS_SUCCESS;

}

/* Function used to remove a scsi device from OS scsi subsystem */
void
pqisrc_remove_device(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	DBG_FUNC("IN\n");
	DBG_NOTE("vendor: %s model: %s bus:%d target:%d lun:%d is_physical_device:0x%x expose_device:0x%x volume_offline 0x%x volume_status 0x%x \n",
		device->vendor, device->model, device->bus, device->target, device->lun, device->is_physical_device, device->expose_device, device->volume_offline, device->volume_status);
	device->invalid = true;
	if (device->expose_device == false) {
		/*Masked physical devices are not been exposed to storage stack.
		*Hence, free the masked device resources such as
		*device memory, Target ID,etc., here.
		*/
		DBG_NOTE("Deallocated Masked Device Resources.\n");
		/* softs->device_list[device->target][device->lun] = NULL; */
		pqisrc_free_device(softs,device);
		return;
	}
	/* Wait for device outstanding Io's */
	pqisrc_wait_for_device_commands_to_complete(softs, device);
	/* Call OS upper layer function to remove the exposed device entry */
	os_remove_device(softs,device);
	DBG_FUNC("OUT\n");
}


/*
 * When exposing new device to OS fails then adjst list according to the
 * mid scsi list
 */
static void
pqisrc_adjust_list(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	int i;
	unsigned char addr1[8], addr2[8];
	pqi_scsi_dev_t *temp_device;
	DBG_FUNC("IN\n");

	if (!device) {
		DBG_ERR("softs = %p: device is NULL !!!\n", softs);
		return;
	}

	OS_ACQUIRE_SPINLOCK(&softs->devlist_lock);
	uint8_t *scsi3addr;
	/*For external raid device, there can be multiple luns
	 *with same target. So while freeing external raid device,
	 *free target only after removing all luns with same target.*/
	if (pqisrc_is_external_raid_device(device)) {
		memcpy(addr1, device->scsi3addr, 8);
		for(i = 0; i < PQI_MAX_DEVICES; i++) {
			if(softs->dev_list[i] == NULL)
				continue;
			temp_device = softs->dev_list[i];
			memcpy(addr2, temp_device->scsi3addr, 8);
			if(memcmp(addr1, addr2, 8) == 0)  {
				continue;
			}
			if (addr1[2] == addr2[2]) {
				break;
			}
                }
		if(i == PQI_MAX_DEVICES) {
			pqisrc_remove_target_bit(softs, device->target);
		}
	}

	if(pqisrc_delete_softs_entry(softs, device) == PQI_STATUS_SUCCESS){
		scsi3addr = device->scsi3addr;
		if (!pqisrc_is_logical_device(device) && !MASKED_DEVICE(scsi3addr)){
			DBG_NOTE("About to remove target bit %d \n", device->target);
			pqisrc_remove_target_bit(softs, device->target);
		}
	}
	OS_RELEASE_SPINLOCK(&softs->devlist_lock);
	pqisrc_device_mem_free(softs, device);

	DBG_FUNC("OUT\n");
}

/* Debug routine used to display the RAID volume status of the device */
static void
pqisrc_display_volume_status(pqisrc_softstate_t *softs,	pqi_scsi_dev_t *device)
{
	char *status;

	DBG_FUNC("IN\n");
	switch (device->volume_status) {
	case SA_LV_OK:
		status = "Volume is online.";
		break;
	case SA_LV_UNDERGOING_ERASE:
		status = "Volume is undergoing background erase process.";
		break;
	case SA_LV_NOT_AVAILABLE:
		status = "Volume is waiting for transforming volume.";
		break;
	case SA_LV_UNDERGOING_RPI:
		status = "Volume is undergoing rapid parity initialization process.";
		break;
	case SA_LV_PENDING_RPI:
		status = "Volume is queued for rapid parity initialization process.";
		break;
	case SA_LV_ENCRYPTED_NO_KEY:
		status = "Volume is encrypted and cannot be accessed because key is not present.";
		break;
	case SA_LV_PLAINTEXT_IN_ENCRYPT_ONLY_CONTROLLER:
		status = "Volume is not encrypted and cannot be accessed because controller is in encryption-only mode.";
		break;
	case SA_LV_UNDERGOING_ENCRYPTION:
		status = "Volume is undergoing encryption process.";
		break;
	case SA_LV_UNDERGOING_ENCRYPTION_REKEYING:
		status = "Volume is undergoing encryption re-keying process.";
		break;
	case SA_LV_ENCRYPTED_IN_NON_ENCRYPTED_CONTROLLER:
		status = "Volume is encrypted and cannot be accessed because controller does not have encryption enabled.";
		break;
	case SA_LV_PENDING_ENCRYPTION:
		status = "Volume is pending migration to encrypted state, but process has not started.";
		break;
	case SA_LV_PENDING_ENCRYPTION_REKEYING:
		status = "Volume is encrypted and is pending encryption rekeying.";
		break;
	case SA_LV_STATUS_VPD_UNSUPPORTED:
		status = "Volume status is not available through vital product data pages.";
		break;
	case SA_LV_UNDERGOING_EXPANSION:
		status = "Volume undergoing expansion";
		break;
	case SA_LV_QUEUED_FOR_EXPANSION:
		status = "Volume queued for expansion";
		break;
	case SA_LV_EJECTED:
		status = "Volume ejected";
		break;
	case SA_LV_WRONG_PHYSICAL_DRIVE_REPLACED:
		status = "Volume has wrong physical drive replaced";
		break;
	case SA_LV_DISABLED_SCSI_ID_CONFLICT:
		status = "Volume disabled scsi id conflict";
		break;
	case SA_LV_HARDWARE_HAS_OVERHEATED:
		status = "Volume hardware has over heated";
		break;
	case SA_LV_HARDWARE_OVERHEATING:
		status = "Volume hardware over heating";
		break;
	case SA_LV_PHYSICAL_DRIVE_CONNECTION_PROBLEM:
		status = "Volume physical drive connection problem";
		break;
	default:
		status = "Volume is in an unknown state.";
		break;
	}

	DBG_NOTE("scsi BTL %d:%d:%d %s\n",
		device->bus, device->target, device->lun, status);
	DBG_FUNC("OUT\n");
}

void
pqisrc_device_mem_free(pqisrc_softstate_t *softs, pqi_scsi_dev_t *device)
{
	DBG_FUNC("IN\n");
	if (!device)
		return;
	if (device->raid_map) {
			os_mem_free(softs, (char *)device->raid_map, sizeof(pqisrc_raid_map_t));
	}
	if (device->offload_to_mirror) {
		os_mem_free(softs, (int *)device->offload_to_mirror, sizeof(*(device->offload_to_mirror)));
	}
	os_mem_free(softs, (char *)device,sizeof(*device));
	DBG_FUNC("OUT\n");

}

/* OS should call this function to free the scsi device */
void
pqisrc_free_device(pqisrc_softstate_t * softs, pqi_scsi_dev_t *device)
{
	rcb_t *rcb;
	uint8_t *scsi3addr;
	int i, index;
	pqi_scsi_dev_t *temp_device;
	unsigned char addr1[8], addr2[8];
	/* Clear the "device" field in the rcb.
	 * Response coming after device removal shouldn't access this field
	 */
	for(i = 1; i <= softs->max_outstanding_io; i++)
	{
		rcb = &softs->rcb[i];
		if(rcb->dvp == device) {
			DBG_WARN("Pending requests for the removing device\n");
			rcb->dvp = NULL;
		}
	}
	/* Find the entry in device list for the freed device softs->dev_list[i]&
	 *make it NULL before freeing the device memory
	 */
	index = pqisrc_find_device_list_index(softs, device);

	OS_ACQUIRE_SPINLOCK(&softs->devlist_lock);
	scsi3addr = device->scsi3addr;
	if (!pqisrc_is_logical_device(device) && !MASKED_DEVICE(scsi3addr)) {
		DBG_NOTE("Giving back target %i \n", device->target);
		pqisrc_remove_target_bit(softs, device->target);
	}
	/*For external raid device, there can be multiple luns
	 *with same target. So while freeing external raid device,
	 *free target only after removing all luns with same target.*/
	if (pqisrc_is_external_raid_device(device)) {
		memcpy(addr1, device->scsi3addr, 8);
		for(i = 0; i < PQI_MAX_DEVICES; i++) {
			if(softs->dev_list[i] == NULL)
				continue;
			temp_device = softs->dev_list[i];
			memcpy(addr2, temp_device->scsi3addr, 8);
			if(memcmp(addr1, addr2, 8) == 0)  {
				continue;
			}
			if (addr1[2] == addr2[2]) {
				break;
			}
		}
		if(i == PQI_MAX_DEVICES) {
			 pqisrc_remove_target_bit(softs, device->target);
		}
	}

	if (index >= 0 && index < PQI_MAX_DEVICES)
		softs->dev_list[index] = NULL;
	if (device->expose_device == true){
		pqisrc_delete_softs_entry(softs, device);
		DBG_NOTE("Removed memory for device : B %d: T %d: L %d\n",
			device->bus, device->target, device->lun);
		OS_RELEASE_SPINLOCK(&softs->devlist_lock);
		pqisrc_device_mem_free(softs, device);
	} else {
		OS_RELEASE_SPINLOCK(&softs->devlist_lock);
	}
}


/* Update the newly added devices to the device list */
static void
pqisrc_update_device_list(pqisrc_softstate_t *softs,
	pqi_scsi_dev_t *new_device_list[], int num_new_devices)
{
	int ret;
	int i;
	device_status_t dev_status;
	pqi_scsi_dev_t *device;
	pqi_scsi_dev_t *same_device;
	pqi_scsi_dev_t **added = NULL;
	pqi_scsi_dev_t **removed = NULL;
	int nadded = 0, nremoved = 0;
	uint8_t *scsi3addr;

	DBG_FUNC("IN\n");

	added = os_mem_alloc(softs, sizeof(*added) * PQI_MAX_DEVICES);
	removed = os_mem_alloc(softs, sizeof(*removed) * PQI_MAX_DEVICES);

	if (!added || !removed) {
		DBG_WARN("Out of memory \n");
		goto free_and_out;
	}

	OS_ACQUIRE_SPINLOCK(&softs->devlist_lock);

	for(i = 0; i < PQI_MAX_DEVICES; i++) {
		if(softs->dev_list[i] == NULL)
			continue;
		device = softs->dev_list[i];
		device->device_gone = true;
	}

	/* TODO:Remove later */
	DBG_IO("Device list used an array\n");
	for (i = 0; i < num_new_devices; i++) {
		device = new_device_list[i];

		dev_status = pqisrc_scsi_find_entry(softs, device,
			&same_device);

		switch (dev_status) {
		case DEVICE_UNCHANGED:
			/* New Device present in existing device list  */
			device->new_device = false;
			same_device->device_gone = false;
			pqisrc_exist_device_update(softs, same_device, device);
			break;
		case DEVICE_NOT_FOUND:
			/* Device not found in existing list */
			device->new_device = true;
			break;
		case DEVICE_CHANGED:
			/* Actual device gone need to add device to list*/
			device->new_device = true;
			break;
		case DEVICE_IN_REMOVE:
			/*Older device with same target/lun is in removal stage*/
			/*New device will be added/scanned when same target/lun
			 * device_list[] gets removed from the OS target
			 * free call*/
			device->new_device = false;
			same_device->schedule_rescan = true;
			break;
		default:
			break;
		}
	}

	/* Process all devices that have gone away. */
	for(i = 0; i < PQI_MAX_DEVICES; i++) {
		device = softs->dev_list[i];
		if(device == NULL)
			continue;
		if (device->device_gone) {
			if(device->in_remove == true)
         {
				continue;
			}
			device->in_remove = true;
			removed[nremoved] = device;
			softs->num_devs--;
			nremoved++;
		}
	}

	/* Process all new devices. */
	for (i = 0, nadded = 0; i < num_new_devices; i++) {
		device = new_device_list[i];
		if (!device->new_device)
			continue;
		if (device->volume_offline)
			continue;

		/* Find out which devices to add to the driver list
		*  in softs->dev_list */
		scsi3addr = device->scsi3addr;
		if (device->expose_device || !MASKED_DEVICE(scsi3addr)){
			if(pqisrc_add_softs_entry(softs, device, scsi3addr)){
				/* To prevent this entry from being freed later. */
				new_device_list[i] = NULL;
				added[nadded] = device;
				nadded++;
			}
		}

	}

	for(i = 0; i < PQI_MAX_DEVICES; i++) {
		device = softs->dev_list[i];
		if(device == NULL)
			continue;
		if (device->offload_enabled != device->offload_enabled_pending)
		{
			DBG_NOTE("[%d:%d:%d]Changing AIO to %d (was %d)\n",
				device->bus, device->target, device->lun,
				device->offload_enabled_pending,
				device->offload_enabled);
		}
		device->offload_enabled = device->offload_enabled_pending;
	}

	OS_RELEASE_SPINLOCK(&softs->devlist_lock);

	for(i = 0; i < nremoved; i++) {
		device = removed[i];
		if (device == NULL)
			continue;
		pqisrc_display_device_info(softs, "removed", device);
		pqisrc_remove_device(softs, device);
	}

	OS_ACQUIRE_SPINLOCK(&softs->devlist_lock);

	for(i = 0; i < PQI_MAX_DEVICES; i++) {
		if(softs->dev_list[i] == NULL)
			continue;
		device = softs->dev_list[i];
		if (device->in_remove)
			continue;
		/*
		* If firmware queue depth is corrupt or not working
		* use the PQI_LOGICAL_DISK_DEFAULT_MAX_QUEUE_DEPTH
		* which is 0. That means there is no limit to the
		* queue depth all the way up to the controller
		* queue depth
		*/
		if (pqisrc_is_logical_device(device) &&
				device->firmware_queue_depth_set == false)
			device->queue_depth = PQI_LOGICAL_DISK_DEFAULT_MAX_QUEUE_DEPTH;

		if (device->scsi_rescan) {
			os_rescan_target(softs, device);
		}
	}
	softs->ld_rescan = false;

	OS_RELEASE_SPINLOCK(&softs->devlist_lock);

	for(i = 0; i < nadded; i++) {
		device = added[i];
		if (device->expose_device) {
			ret = pqisrc_add_device(softs, device);
			if (ret) {
				DBG_WARN("scsi %d:%d:%d addition failed, device not added\n",
					device->bus, device->target, device->lun);
				pqisrc_adjust_list(softs, device);
				continue;
			}
		}

		pqisrc_display_device_info(softs, "added", device);
	}

	/* Process all volumes that are offline. */
	for (i = 0; i < num_new_devices; i++) {
		device = new_device_list[i];
		if (!device)
			continue;
		if (!device->new_device)
			continue;
		if (device->volume_offline) {
			pqisrc_display_volume_status(softs, device);
			pqisrc_display_device_info(softs, "offline", device);
		}
	}

	for (i = 0; i < PQI_MAX_DEVICES; i++) {
		device = softs->dev_list[i];
		if(device == NULL)
			continue;
		DBG_DISC("Current device %d : B%d:T%d:L%d\n",
			i, device->bus, device->target,
			device->lun);
	}

free_and_out:
	if (added)
		os_mem_free(softs, (char *)added,
			    sizeof(*added) * PQI_MAX_DEVICES);
	if (removed)
		os_mem_free(softs, (char *)removed,
			    sizeof(*removed) * PQI_MAX_DEVICES);

	DBG_FUNC("OUT\n");
}

/*
 * Let the Adapter know about driver version using one of BMIC
 * BMIC_WRITE_HOST_WELLNESS
 */
int
pqisrc_write_driver_version_to_host_wellness(pqisrc_softstate_t *softs)
{
	int rval = PQI_STATUS_SUCCESS;
	struct bmic_host_wellness_driver_version *host_wellness_driver_ver;
	size_t data_length;
	pqisrc_raid_req_t request;

	DBG_FUNC("IN\n");

	memset(&request, 0, sizeof(request));
	data_length = sizeof(*host_wellness_driver_ver);

	host_wellness_driver_ver = os_mem_alloc(softs, data_length);
	if (!host_wellness_driver_ver) {
		DBG_ERR("failed to allocate memory for host wellness driver_version\n");
		return PQI_STATUS_FAILURE;
	}

	host_wellness_driver_ver->start_tag[0] = '<';
	host_wellness_driver_ver->start_tag[1] = 'H';
	host_wellness_driver_ver->start_tag[2] = 'W';
	host_wellness_driver_ver->start_tag[3] = '>';
	host_wellness_driver_ver->driver_version_tag[0] = 'D';
	host_wellness_driver_ver->driver_version_tag[1] = 'V';
	host_wellness_driver_ver->driver_version_length = LE_16(sizeof(host_wellness_driver_ver->driver_version));
	strncpy(host_wellness_driver_ver->driver_version, softs->os_name,
        sizeof(host_wellness_driver_ver->driver_version));
    if (strlen(softs->os_name) < sizeof(host_wellness_driver_ver->driver_version) ) {
        strncpy(host_wellness_driver_ver->driver_version + strlen(softs->os_name), PQISRC_DRIVER_VERSION,
			sizeof(host_wellness_driver_ver->driver_version) -  strlen(softs->os_name));
    } else {
        DBG_DISC("OS name length(%u) is longer than buffer of driver_version\n",
            (unsigned int)strlen(softs->os_name));

    }
	host_wellness_driver_ver->driver_version[sizeof(host_wellness_driver_ver->driver_version) - 1] = '\0';
	host_wellness_driver_ver->end_tag[0] = 'Z';
	host_wellness_driver_ver->end_tag[1] = 'Z';


	request.data_direction = SOP_DATA_DIR_FROM_DEVICE;
	request.cmd.bmic_cdb.op_code = BMIC_WRITE;
	request.cmd.bmic_cdb.cmd = BMIC_WRITE_HOST_WELLNESS;
	request.cmd.bmic_cdb.xfer_len = BE_16(data_length);

	rval = pqisrc_prepare_send_ctrlr_request(softs, &request, host_wellness_driver_ver, data_length);

	os_mem_free(softs, (char *)host_wellness_driver_ver, data_length);

	DBG_FUNC("OUT");
	return rval;
}

/*
 * Write current RTC time from host to the adapter using
 * BMIC_WRITE_HOST_WELLNESS
 */
int
pqisrc_write_current_time_to_host_wellness(pqisrc_softstate_t *softs)
{
	int rval = PQI_STATUS_SUCCESS;
	struct bmic_host_wellness_time *host_wellness_time;
	size_t data_length;
	pqisrc_raid_req_t request;

	DBG_FUNC("IN\n");

	memset(&request, 0, sizeof(request));
	data_length = sizeof(*host_wellness_time);

	host_wellness_time = os_mem_alloc(softs, data_length);
	if (!host_wellness_time) {
		DBG_ERR("failed to allocate memory for host wellness time structure\n");
		return PQI_STATUS_FAILURE;
	}

	host_wellness_time->start_tag[0] = '<';
	host_wellness_time->start_tag[1] = 'H';
	host_wellness_time->start_tag[2] = 'W';
	host_wellness_time->start_tag[3] = '>';
	host_wellness_time->time_tag[0] = 'T';
	host_wellness_time->time_tag[1] = 'D';
	host_wellness_time->time_length = LE_16(offsetof(struct bmic_host_wellness_time, time_length) -
											offsetof(struct bmic_host_wellness_time, century));

	os_get_time(host_wellness_time);

	host_wellness_time->dont_write_tag[0] = 'D';
	host_wellness_time->dont_write_tag[1] = 'W';
	host_wellness_time->end_tag[0] = 'Z';
	host_wellness_time->end_tag[1] = 'Z';


	request.data_direction = SOP_DATA_DIR_FROM_DEVICE;
	request.cmd.bmic_cdb.op_code = BMIC_WRITE;
	request.cmd.bmic_cdb.cmd = BMIC_WRITE_HOST_WELLNESS;
	request.cmd.bmic_cdb.xfer_len = BE_16(data_length);

	rval = pqisrc_prepare_send_ctrlr_request(softs, &request, host_wellness_time, data_length);

	os_mem_free(softs, (char *)host_wellness_time, data_length);

	DBG_FUNC("OUT");
	return rval;
}
static void
pqisrc_get_device_vpd_info(pqisrc_softstate_t *softs,
		bmic_ident_physdev_t *bmic_phy_info,pqi_scsi_dev_t *device)
{
	DBG_FUNC("IN\n");
        memcpy(&device->wwid, &bmic_phy_info->padding[79], sizeof(device->wwid));
        DBG_FUNC("OUT\n");
}
/*
 * Function used to perform a rescan of scsi devices
 * for any config change events
 */
int
pqisrc_scan_devices(pqisrc_softstate_t *softs)
{
	boolean_t is_physical_device;
	int ret;
	int i;
	int new_dev_cnt;
	int phy_log_dev_cnt;
	size_t queue_log_data_length;
	uint8_t *scsi3addr;
	uint8_t multiplier;
	uint16_t qdepth;
	uint32_t physical_cnt;
	uint32_t logical_cnt;
	uint32_t logical_queue_cnt;
	uint32_t ndev_allocated = 0;
	size_t phys_data_length, log_data_length;
	reportlun_data_ext_t *physical_dev_list = NULL;
	reportlun_data_ext_t *logical_dev_list = NULL;
	reportlun_ext_entry_t *lun_ext_entry = NULL;
	reportlun_queue_depth_data_t *logical_queue_dev_list = NULL;
	bmic_ident_physdev_t *bmic_phy_info = NULL;
	pqi_scsi_dev_t **new_device_list = NULL;
	pqi_scsi_dev_t *device = NULL;
#ifdef PQI_NEED_RESCAN_TIMER_FOR_RBOD_HOTPLUG
	int num_ext_raid_devices = 0;
#endif

	DBG_FUNC("IN\n");

	ret = pqisrc_get_phys_log_device_list(softs, &physical_dev_list, &logical_dev_list,
					&logical_queue_dev_list, &queue_log_data_length,
					&phys_data_length, &log_data_length);

	if (ret)
		goto err_out;

	physical_cnt = BE_32(physical_dev_list->header.list_length)
		/ sizeof(physical_dev_list->lun_entries[0]);

	logical_cnt = BE_32(logical_dev_list->header.list_length)
		/ sizeof(logical_dev_list->lun_entries[0]);

	logical_queue_cnt = BE_32(logical_queue_dev_list->header.list_length)
                / sizeof(logical_queue_dev_list->lun_entries[0]);


	DBG_DISC("physical_cnt %u logical_cnt %u queue_cnt %u\n", physical_cnt, logical_cnt, logical_queue_cnt);

	if (physical_cnt) {
		bmic_phy_info = os_mem_alloc(softs, sizeof(*bmic_phy_info));
		if (bmic_phy_info == NULL) {
			ret = PQI_STATUS_FAILURE;
			DBG_ERR("failed to allocate memory for BMIC ID PHYS Device : %d\n", ret);
			goto err_out;
		}
	}
	phy_log_dev_cnt = physical_cnt + logical_cnt;
	new_device_list = os_mem_alloc(softs,
				sizeof(*new_device_list) * phy_log_dev_cnt);

	if (new_device_list == NULL) {
		ret = PQI_STATUS_FAILURE;
		DBG_ERR("failed to allocate memory for device list : %d\n", ret);
		goto err_out;
	}

	for (i = 0; i < phy_log_dev_cnt; i++) {
		new_device_list[i] = os_mem_alloc(softs,
						sizeof(*new_device_list[i]));
		if (new_device_list[i] == NULL) {
			ret = PQI_STATUS_FAILURE;
			DBG_ERR("failed to allocate memory for device list : %d\n", ret);
			ndev_allocated = i;
			goto err_out;
		}
	}

	ndev_allocated = phy_log_dev_cnt;
	new_dev_cnt = 0;
	for (i = 0; i < phy_log_dev_cnt; i++) {

		if (i < physical_cnt) {
			is_physical_device = true;
			lun_ext_entry = &physical_dev_list->lun_entries[i];
		} else {
			is_physical_device = false;
			lun_ext_entry =
				&logical_dev_list->lun_entries[i - physical_cnt];
		}

		scsi3addr = lun_ext_entry->lunid;

		/* Save the target sas adderess for external raid device */
		if(lun_ext_entry->device_type == CONTROLLER_DEVICE) {
#ifdef PQI_NEED_RESCAN_TIMER_FOR_RBOD_HOTPLUG
			num_ext_raid_devices++;
#endif
			int target = lun_ext_entry->lunid[3] & 0x3f;
			softs->target_sas_addr[target] = BE_64(lun_ext_entry->wwid);
		}

		/* Skip masked physical non-disk devices. */
		if (MASKED_DEVICE(scsi3addr) && is_physical_device
				&& (lun_ext_entry->ioaccel_handle == 0))
			continue;

		device = new_device_list[new_dev_cnt];
		memset(device, 0, sizeof(*device));
		memcpy(device->scsi3addr, scsi3addr, sizeof(device->scsi3addr));
		device->wwid = lun_ext_entry->wwid;
		device->is_physical_device = is_physical_device;
		if (!is_physical_device && logical_queue_cnt--) {
			device->is_external_raid_device =
				pqisrc_is_external_raid_addr(scsi3addr);
			/* The multiplier is the value we multiply the queue
 			 * depth value with to get the actual queue depth.
 			 * If multiplier is 1 multiply by 256 if
 			 * multiplier 0 then multiply by 16 */
			multiplier = logical_queue_dev_list->lun_entries[i - physical_cnt].multiplier;
			qdepth = logical_queue_dev_list->lun_entries[i - physical_cnt].queue_depth;
			if (multiplier) {
				device->firmware_queue_depth_set = true;
				device->queue_depth = qdepth*256;
			} else {
				device->firmware_queue_depth_set = true;
				device->queue_depth = qdepth*16;
			}
			if (device->queue_depth > softs->adapterQDepth) {
				device->firmware_queue_depth_set = true;
				device->queue_depth = softs->adapterQDepth;
			}
			if ((multiplier == 1) &&
				(qdepth >= MAX_RAW_M256_QDEPTH))
				device->firmware_queue_depth_set = false;
			if ((multiplier == 0) &&
				(qdepth >= MAX_RAW_M16_QDEPTH))
				device->firmware_queue_depth_set = false;

		}


		/* Get device type, vendor, model, device ID. */
		ret = pqisrc_get_dev_data(softs, device);
		if (ret) {
			DBG_WARN("Inquiry failed, skipping device %016llx\n",
				 (unsigned long long)BE_64(device->scsi3addr[0]));
			DBG_DISC("INQUIRY FAILED \n");
			continue;
		}
		/* Set controller queue depth to what
 		 * it was from the scsi midlayer */
		if (device->devtype == RAID_DEVICE) {
			device->firmware_queue_depth_set = true;
			device->queue_depth = softs->adapterQDepth;
		}
		pqisrc_assign_btl(softs, device);

		/*
		 * Expose all devices except for physical devices that
		 * are masked.
		 */
		if (device->is_physical_device &&
			MASKED_DEVICE(scsi3addr))
			device->expose_device = false;
		else
			device->expose_device = true;

		if (device->is_physical_device &&
		    (lun_ext_entry->device_flags &
		     REPORT_LUN_DEV_FLAG_AIO_ENABLED) &&
		     lun_ext_entry->ioaccel_handle) {
			device->aio_enabled = true;
		}
		switch (device->devtype) {
		case ROM_DEVICE:
			/*
			 * We don't *really* support actual CD-ROM devices,
			 * but we do support the HP "One Button Disaster
			 * Recovery" tape drive which temporarily pretends to
			 * be a CD-ROM drive.
			 */
			if (device->is_obdr_device)
				new_dev_cnt++;
			break;
		case DISK_DEVICE:
		case ZBC_DEVICE:
			if (device->is_physical_device) {
				device->ioaccel_handle =
					lun_ext_entry->ioaccel_handle;
				pqisrc_get_physical_device_info(softs, device,
					bmic_phy_info);
				if ( (!softs->page83id_in_rpl) && (bmic_phy_info->device_type == BMIC_DEVICE_TYPE_SATA)) {
					pqisrc_get_device_vpd_info(softs, bmic_phy_info, device);
				}
				device->sas_address = BE_64(device->wwid);
			}
			new_dev_cnt++;
			break;
		case ENCLOSURE_DEVICE:
			if (device->is_physical_device) {
				device->sas_address = BE_64(lun_ext_entry->wwid);
			}
			new_dev_cnt++;
			break;
		case TAPE_DEVICE:
		case MEDIUM_CHANGER_DEVICE:
			new_dev_cnt++;
			break;
		case RAID_DEVICE:
			/*
			 * Only present the HBA controller itself as a RAID
			 * controller.  If it's a RAID controller other than
			 * the HBA itself (an external RAID controller, MSA500
			 * or similar), don't present it.
			 */
			if (pqisrc_is_hba_lunid(scsi3addr))
				new_dev_cnt++;
			break;
		case SES_DEVICE:
		case CONTROLLER_DEVICE:
		default:
			break;
		}
	}
	DBG_DISC("new_dev_cnt %d\n", new_dev_cnt);
#ifdef PQI_NEED_RESCAN_TIMER_FOR_RBOD_HOTPLUG
	if(num_ext_raid_devices)
		os_start_rescan_timer(softs);
	else
		 os_stop_rescan_timer(softs);
#endif
	pqisrc_update_device_list(softs, new_device_list, new_dev_cnt);

err_out:
	if (new_device_list) {
		for (i = 0; i < ndev_allocated; i++) {
			if (new_device_list[i]) {
				if(new_device_list[i]->raid_map)
					os_mem_free(softs, (char *)new_device_list[i]->raid_map,
					    					sizeof(pqisrc_raid_map_t));
				os_mem_free(softs, (char*)new_device_list[i],
					    			sizeof(*new_device_list[i]));
			}
		}
		os_mem_free(softs, (char *)new_device_list,
			    		sizeof(*new_device_list) * ndev_allocated);
	}
	if(physical_dev_list)
		os_mem_free(softs, (char *)physical_dev_list, phys_data_length);
    	if(logical_dev_list)
		os_mem_free(softs, (char *)logical_dev_list, log_data_length);
	if(logical_queue_dev_list)
		os_mem_free(softs, (char*)logical_queue_dev_list,
			queue_log_data_length);
	if (bmic_phy_info)
		os_mem_free(softs, (char *)bmic_phy_info, sizeof(*bmic_phy_info));

	DBG_FUNC("OUT \n");

	return ret;
}

/*
 * Clean up memory allocated for devices.
 */
void
pqisrc_cleanup_devices(pqisrc_softstate_t *softs)
{
	int i = 0;
	pqi_scsi_dev_t *device = NULL;
	DBG_FUNC("IN\n");
	for(i = 0; i < PQI_MAX_DEVICES; i++) {
		if(softs->dev_list[i] == NULL)
			continue;
		device = softs->dev_list[i];
		pqisrc_device_mem_free(softs, device);
	}

	DBG_FUNC("OUT\n");
}
