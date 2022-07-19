/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include "adf_accel_devices.h"
#include "adf_fw_counters.h"
#include "adf_common_drv.h"
#include "icp_qat_fw_init_admin.h"
#include <sys/mutex.h>
#include <sys/sbuf.h>
#define ADF_FW_COUNTERS_BUF_SZ 4096

#define ADF_RAS_EVENT_STR "RAS events"
#define ADF_FW_REQ_STR "Firmware Requests"
#define ADF_FW_RESP_STR "Firmware Responses"

static void adf_fw_counters_section_del_all(struct list_head *head);
static void adf_fw_counters_del_all(struct adf_accel_dev *accel_dev);
static int
adf_fw_counters_add_key_value_param(struct adf_accel_dev *accel_dev,
				    const char *section_name,
				    const unsigned long sec_name_max_size,
				    const char *key,
				    const void *val);
static int adf_fw_counters_section_add(struct adf_accel_dev *accel_dev,
				       const char *name,
				       const unsigned long name_max_size);
int adf_get_fw_counters(struct adf_accel_dev *accel_dev);
int adf_read_fw_counters(SYSCTL_HANDLER_ARGS);

int
adf_get_fw_counters(struct adf_accel_dev *accel_dev)
{
	struct icp_qat_fw_init_admin_req req;
	struct icp_qat_fw_init_admin_resp resp;
	unsigned long ae_mask;
	int i;
	int ret = 0;
	char aeidstr[16] = { 0 };
	struct adf_hw_device_data *hw_device;

	if (!accel_dev) {
		ret = EFAULT;
		goto fail_clean;
	}
	if (!adf_dev_started(accel_dev)) {
		device_printf(GET_DEV(accel_dev), "Qat Device not started\n");
		ret = EFAULT;
		goto fail_clean;
	}

	hw_device = accel_dev->hw_device;
	if (!hw_device) {
		ret = EFAULT;
		goto fail_clean;
	}

	adf_fw_counters_del_all(accel_dev);
	explicit_bzero(&req, sizeof(struct icp_qat_fw_init_admin_req));
	req.cmd_id = ICP_QAT_FW_COUNTERS_GET;
	ae_mask = hw_device->ae_mask;
	for_each_set_bit(i, &ae_mask, GET_MAX_ACCELENGINES(accel_dev))
	{
		explicit_bzero(&resp,
			       sizeof(struct icp_qat_fw_init_admin_resp));
		if (adf_put_admin_msg_sync(accel_dev, i, &req, &resp) ||
		    resp.status) {
			resp.req_rec_count = ADF_FW_COUNTERS_NO_RESPONSE;
			resp.resp_sent_count = ADF_FW_COUNTERS_NO_RESPONSE;
			resp.ras_event_count = ADF_FW_COUNTERS_NO_RESPONSE;
		}
		explicit_bzero(aeidstr, sizeof(aeidstr));
		snprintf(aeidstr, sizeof(aeidstr), "AE %2d", i);

		if (adf_fw_counters_section_add(accel_dev,
						aeidstr,
						sizeof(aeidstr))) {
			ret = ENOMEM;
			goto fail_clean;
		}

		if (adf_fw_counters_add_key_value_param(
			accel_dev,
			aeidstr,
			sizeof(aeidstr),
			ADF_FW_REQ_STR,
			(void *)&resp.req_rec_count)) {
			adf_fw_counters_del_all(accel_dev);
			ret = ENOMEM;
			goto fail_clean;
		}

		if (adf_fw_counters_add_key_value_param(
			accel_dev,
			aeidstr,
			sizeof(aeidstr),
			ADF_FW_RESP_STR,
			(void *)&resp.resp_sent_count)) {
			adf_fw_counters_del_all(accel_dev);
			ret = ENOMEM;
			goto fail_clean;
		}

		if (hw_device->count_ras_event &&
		    hw_device->count_ras_event(accel_dev,
					       (void *)&resp.ras_event_count,
					       aeidstr)) {
			adf_fw_counters_del_all(accel_dev);
			ret = ENOMEM;
			goto fail_clean;
		}
	}

fail_clean:
	return ret;
}

int adf_read_fw_counters(SYSCTL_HANDLER_ARGS)
{
	struct adf_accel_dev *accel_dev = arg1;
	struct adf_fw_counters_section *ptr = NULL;
	struct list_head *list = NULL, *list_ptr = NULL;
	struct list_head *tmp = NULL, *tmp_val = NULL;
	int ret = 0;
	struct sbuf *sbuf = NULL;
	char *cbuf = NULL;

	if (accel_dev == NULL) {
		return EINVAL;
	}
	cbuf = malloc(ADF_FW_COUNTERS_BUF_SZ, M_QAT, M_WAITOK | M_ZERO);

	sbuf = sbuf_new(NULL, cbuf, ADF_FW_COUNTERS_BUF_SZ, SBUF_FIXEDLEN);
	if (sbuf == NULL) {
		free(cbuf, M_QAT);
		return ENOMEM;
	}
	ret = adf_get_fw_counters(accel_dev);

	if (ret) {
		sbuf_delete(sbuf);
		free(cbuf, M_QAT);
		return ret;
	}

	sbuf_printf(sbuf,
		    "\n+------------------------------------------------+\n");
	sbuf_printf(
	    sbuf,
	    "| FW Statistics for Qat Device					   |\n");
	sbuf_printf(sbuf,
		    "+------------------------------------------------+\n");

	list_for_each_prev_safe(list,
				tmp,
				&accel_dev->fw_counters_data->ae_sec_list)
	{
		ptr = list_entry(list, struct adf_fw_counters_section, list);
		sbuf_printf(sbuf, "%s\n", ptr->name);
		list_for_each_prev_safe(list_ptr, tmp_val, &ptr->param_head)
		{
			struct adf_fw_counters_val *count =
			    list_entry(list_ptr,
				       struct adf_fw_counters_val,
				       list);
			sbuf_printf(sbuf, "%s:%s\n", count->key, count->val);
		}
	}

	sbuf_finish(sbuf);
	ret = SYSCTL_OUT(req, sbuf_data(sbuf), sbuf_len(sbuf));
	sbuf_delete(sbuf);
	free(cbuf, M_QAT);
	return ret;
}

int
adf_fw_count_ras_event(struct adf_accel_dev *accel_dev,
		       u32 *ras_event,
		       char *aeidstr)
{
	unsigned long count = 0;

	if (!accel_dev || !ras_event || !aeidstr)
		return EINVAL;

	count = (*ras_event == ADF_FW_COUNTERS_NO_RESPONSE ?
		     ADF_FW_COUNTERS_NO_RESPONSE :
		     (unsigned long)*ras_event);

	return adf_fw_counters_add_key_value_param(
	    accel_dev, aeidstr, 16, ADF_RAS_EVENT_STR, (void *)&count);
}

/**
 * adf_fw_counters_add() - Create an acceleration device FW counters table.
 * @accel_dev:	Pointer to acceleration device.
 *
 * Function creates a FW counters statistics table for the given
 * acceleration device.
 * The table stores device specific values of FW Requests sent to the FW and
 * FW Responses received from the FW.
 * To be used by QAT device specific drivers.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_fw_counters_add(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_counters_data *fw_counters_data;
	struct sysctl_ctx_list *qat_sysctl_ctx;
	struct sysctl_oid *qat_sysctl_tree;
	struct sysctl_oid *rc = 0;

	fw_counters_data =
	    malloc(sizeof(*fw_counters_data), M_QAT, M_WAITOK | M_ZERO);

	INIT_LIST_HEAD(&fw_counters_data->ae_sec_list);

	init_rwsem(&fw_counters_data->lock);
	accel_dev->fw_counters_data = fw_counters_data;

	qat_sysctl_ctx =
	    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);
	qat_sysctl_tree =
	    device_get_sysctl_tree(accel_dev->accel_pci_dev.pci_dev);
	rc = SYSCTL_ADD_OID(qat_sysctl_ctx,
			    SYSCTL_CHILDREN(qat_sysctl_tree),
			    OID_AUTO,
			    "fw_counters",
			    CTLTYPE_STRING | CTLFLAG_RD,
			    accel_dev,
			    0,
			    adf_read_fw_counters,
			    "A",
			    "QAT FW counters");
	if (!rc)
		return ENOMEM;
	else
		return 0;
}

static void
adf_fw_counters_del_all(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_counters_data *fw_counters_data =
	    accel_dev->fw_counters_data;

	down_write(&fw_counters_data->lock);
	adf_fw_counters_section_del_all(&fw_counters_data->ae_sec_list);
	up_write(&fw_counters_data->lock);
}

static void
adf_fw_counters_keyval_add(struct adf_fw_counters_val *new,
			   struct adf_fw_counters_section *sec)
{
	list_add_tail(&new->list, &sec->param_head);
}

static void
adf_fw_counters_keyval_del_all(struct list_head *head)
{
	struct list_head *list_ptr = NULL, *tmp = NULL;

	list_for_each_prev_safe(list_ptr, tmp, head)
	{
		struct adf_fw_counters_val *ptr =
		    list_entry(list_ptr, struct adf_fw_counters_val, list);
		list_del(list_ptr);
		free(ptr, M_QAT);
	}
}

static void
adf_fw_counters_section_del_all(struct list_head *head)
{
	struct adf_fw_counters_section *ptr = NULL;
	struct list_head *list = NULL, *tmp = NULL;

	list_for_each_prev_safe(list, tmp, head)
	{
		ptr = list_entry(list, struct adf_fw_counters_section, list);
		adf_fw_counters_keyval_del_all(&ptr->param_head);
		list_del(list);
		free(ptr, M_QAT);
	}
}

static struct adf_fw_counters_section *
adf_fw_counters_sec_find(struct adf_accel_dev *accel_dev,
			 const char *sec_name,
			 const unsigned long sec_name_max_size)
{
	struct adf_fw_counters_data *fw_counters_data =
	    accel_dev->fw_counters_data;
	struct list_head *list = NULL;

	list_for_each(list, &fw_counters_data->ae_sec_list)
	{
		struct adf_fw_counters_section *ptr =
		    list_entry(list, struct adf_fw_counters_section, list);
		if (!strncmp(ptr->name, sec_name, sec_name_max_size))
			return ptr;
	}
	return NULL;
}

static int
adf_fw_counters_add_key_value_param(struct adf_accel_dev *accel_dev,
				    const char *section_name,
				    const unsigned long sec_name_max_size,
				    const char *key,
				    const void *val)
{
	struct adf_fw_counters_data *fw_counters_data =
	    accel_dev->fw_counters_data;
	struct adf_fw_counters_val *key_val;
	struct adf_fw_counters_section *section =
	    adf_fw_counters_sec_find(accel_dev,
				     section_name,
				     sec_name_max_size);
	long tmp = *((const long *)val);

	if (!section)
		return EFAULT;
	key_val = malloc(sizeof(*key_val), M_QAT, M_WAITOK | M_ZERO);

	INIT_LIST_HEAD(&key_val->list);

	if (tmp == ADF_FW_COUNTERS_NO_RESPONSE) {
		snprintf(key_val->val,
			 FW_COUNTERS_MAX_VAL_LEN_IN_BYTES,
			 "No Response");
	} else {
		snprintf(key_val->val,
			 FW_COUNTERS_MAX_VAL_LEN_IN_BYTES,
			 "%ld",
			 tmp);
	}

	strlcpy(key_val->key, key, sizeof(key_val->key));
	down_write(&fw_counters_data->lock);
	adf_fw_counters_keyval_add(key_val, section);
	up_write(&fw_counters_data->lock);
	return 0;
}

/**
 * adf_fw_counters_section_add() - Add AE section entry to FW counters table.
 * @accel_dev:	Pointer to acceleration device.
 * @name: Name of the section
 *
 * Function adds a section for each AE where FW Requests/Responses and their
 * values will be stored.
 * To be used by QAT device specific drivers.
 *
 * Return: 0 on success, error code otherwise.
 */
static int
adf_fw_counters_section_add(struct adf_accel_dev *accel_dev,
			    const char *name,
			    const unsigned long name_max_size)
{
	struct adf_fw_counters_data *fw_counters_data =
	    accel_dev->fw_counters_data;
	struct adf_fw_counters_section *sec =
	    adf_fw_counters_sec_find(accel_dev, name, name_max_size);

	if (sec)
		return 0;

	sec = malloc(sizeof(*sec), M_QAT, M_WAITOK | M_ZERO);

	strlcpy(sec->name, name, sizeof(sec->name));
	INIT_LIST_HEAD(&sec->param_head);

	down_write(&fw_counters_data->lock);

	list_add_tail(&sec->list, &fw_counters_data->ae_sec_list);
	up_write(&fw_counters_data->lock);
	return 0;
}

/**
 * adf_fw_counters_remove() - Clears acceleration device FW counters table.
 * @accel_dev:	Pointer to acceleration device.
 *
 * Function removes FW counters table from the given acceleration device
 * and frees all allocated memory.
 * To be used by QAT device specific drivers.
 *
 * Return: void
 */
void
adf_fw_counters_remove(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_counters_data *fw_counters_data =
	    accel_dev->fw_counters_data;

	if (!fw_counters_data)
		return;

	down_write(&fw_counters_data->lock);
	adf_fw_counters_section_del_all(&fw_counters_data->ae_sec_list);
	up_write(&fw_counters_data->lock);
	free(fw_counters_data, M_QAT);
	accel_dev->fw_counters_data = NULL;
}
