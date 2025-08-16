/*-
 * Copyright (c) 2025, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ufshci_private.h"

void
ufshci_ctrlr_cmd_send_task_mgmt_request(struct ufshci_controller *ctrlr,
    ufshci_cb_fn_t cb_fn, void *cb_arg, uint8_t function, uint8_t lun,
    uint8_t task_tag, uint8_t iid)
{
	struct ufshci_request *req;
	struct ufshci_task_mgmt_request_upiu *upiu;

	req = ufshci_allocate_request_vaddr(NULL, 0, M_WAITOK, cb_fn, cb_arg);

	req->request_size = sizeof(struct ufshci_task_mgmt_request_upiu);
	req->response_size = sizeof(struct ufshci_task_mgmt_response_upiu);

	upiu = (struct ufshci_task_mgmt_request_upiu *)&req->request_upiu;
	memset(upiu, 0, req->request_size);
	upiu->header.trans_type =
	    UFSHCI_UPIU_TRANSACTION_CODE_TASK_MANAGEMENT_REQUEST;
	upiu->header.lun = lun;
	upiu->header.ext_iid_or_function = function;
	upiu->input_param1 = lun;
	upiu->input_param2 = task_tag;
	upiu->input_param3 = iid;

	ufshci_ctrlr_submit_task_mgmt_request(ctrlr, req);
}

void
ufshci_ctrlr_cmd_send_nop(struct ufshci_controller *ctrlr, ufshci_cb_fn_t cb_fn,
    void *cb_arg)
{
	struct ufshci_request *req;
	struct ufshci_nop_out_upiu *upiu;

	req = ufshci_allocate_request_vaddr(NULL, 0, M_WAITOK, cb_fn, cb_arg);

	req->request_size = sizeof(struct ufshci_nop_out_upiu);
	req->response_size = sizeof(struct ufshci_nop_in_upiu);

	upiu = (struct ufshci_nop_out_upiu *)&req->request_upiu;
	memset(upiu, 0, req->request_size);
	upiu->header.trans_type = UFSHCI_UPIU_TRANSACTION_CODE_NOP_OUT;

	ufshci_ctrlr_submit_admin_request(ctrlr, req);
}

void
ufshci_ctrlr_cmd_send_query_request(struct ufshci_controller *ctrlr,
    ufshci_cb_fn_t cb_fn, void *cb_arg, struct ufshci_query_param param)
{
	struct ufshci_request *req;
	struct ufshci_query_request_upiu *upiu;

	req = ufshci_allocate_request_vaddr(NULL, 0, M_WAITOK, cb_fn, cb_arg);

	req->request_size = sizeof(struct ufshci_query_request_upiu);
	req->response_size = sizeof(struct ufshci_query_response_upiu);

	upiu = (struct ufshci_query_request_upiu *)&req->request_upiu;
	memset(upiu, 0, req->request_size);
	upiu->header.trans_type = UFSHCI_UPIU_TRANSACTION_CODE_QUERY_REQUEST;
	upiu->header.ext_iid_or_function = param.function;
	upiu->opcode = param.opcode;
	upiu->idn = param.type;
	upiu->index = param.index;
	upiu->selector = param.selector;
	upiu->value_64 = param.value;
	upiu->length = param.desc_size;

	ufshci_ctrlr_submit_admin_request(ctrlr, req);
}
