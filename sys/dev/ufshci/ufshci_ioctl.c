/*-
 * Copyright (c) 2026, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include "ufshci_private.h"
#include "ufshci_ioctl.h"
#include "ufshci_reg.h"

static d_ioctl_t ufshci_ioctl;

static struct cdevsw ufshci_cdevsw = {
	.d_version = D_VERSION,
	.d_ioctl = ufshci_ioctl,
	.d_name = "ufshci",
};

/*
 * Work out how much of the UPIU the controller has to read and how much
 * it may write back.
 */
static int
ufshci_passthrough_upiu_sizes(const struct ufshci_pt_command *pt,
    size_t *req_size, size_t *resp_size, bool *is_admin)
{
	const struct ufshci_upiu_header *header = &pt->req_upiu.header;
	size_t ehs_bytes = header->ehs_length * UFSHCI_EHS_UNIT_SIZE;
	size_t max_data_len;

	switch (header->trans_code) {
	case UFSHCI_UPIU_TRANSACTION_CODE_QUERY_REQUEST:
		/* Only a command UPIU has room for an EHS. */
		if (header->ehs_length != 0)
			return (EINVAL);
		*req_size = sizeof(struct ufshci_query_request_upiu);
		*resp_size = sizeof(struct ufshci_query_response_upiu);
		*is_admin = true;
		max_data_len = UFSHCI_QUERY_DATA_SEGMENT_SIZE;
		break;
	case UFSHCI_UPIU_TRANSACTION_CODE_NOP_OUT:
		if (header->ehs_length != 0)
			return (EINVAL);
		*req_size = sizeof(struct ufshci_nop_out_upiu);
		*resp_size = sizeof(struct ufshci_nop_in_upiu);
		*is_admin = true;
		max_data_len = 0;
		break;
	case UFSHCI_UPIU_TRANSACTION_CODE_COMMAND:
		*req_size = sizeof(struct ufshci_cmd_command_upiu) + ehs_bytes;
		/* The response carries an EHS too, for advanced RPMB. */
		*resp_size = sizeof(struct ufshci_cmd_response_upiu) +
		    ehs_bytes;
		*is_admin = false;
		/* A command UPIU moves its payload through the PRDT. */
		max_data_len = 0;
		break;
	default:
		return (EINVAL);
	}

	/*
	 * The descriptor has no request length, so the controller takes it
	 * from the UPIU header. A header that declares more than the UPIU
	 * has room for would read past the command descriptor.
	 */
	if (be16toh(header->data_segment_length) > max_data_len)
		return (EINVAL);

	return (0);
}

static int
ufshci_passthrough_cmd(struct ufshci_controller *ctrlr,
    struct ufshci_pt_command *pt)
{
	struct ufshci_completion_poll_status status;
	struct ufshci_request *req;
	size_t req_size, resp_size;
	void *buf = NULL;
	bool is_admin;
	int error;

	/*
	 * An aborted request is completed by hand and may leave cpl
	 * unfilled, so stack bytes would go back to userland.
	 */
	memset(&status, 0, sizeof(status));

	if (pt->timeout_ms != 0)
		return (EINVAL);

	/*
	 * The response copy below is shorter than this field, so without
	 * clearing it the tail would hold the caller's own input.
	 */
	memset(&pt->resp_upiu, 0, sizeof(pt->resp_upiu));

	/*
	 * The ABI cap is fixed. What a controller can actually map is not,
	 * so check both.
	 */
	if (pt->len > UFSHCI_PT_MAX_XFER || pt->len > ctrlr->max_xfer_size)
		return (EINVAL);
	if (pt->len != 0 && pt->buf == NULL)
		return (EINVAL);

	/* A buffer with no direction would build a PRDT nothing reads. */
	if (pt->len != 0 && (pt->flags &
	    (UFSHCI_PT_FLAG_DATA_IN | UFSHCI_PT_FLAG_DATA_OUT)) == 0)
		return (EINVAL);

	if ((pt->flags & (UFSHCI_PT_FLAG_DATA_IN | UFSHCI_PT_FLAG_DATA_OUT)) ==
	    (UFSHCI_PT_FLAG_DATA_IN | UFSHCI_PT_FLAG_DATA_OUT))
		return (EINVAL);

	error = ufshci_passthrough_upiu_sizes(pt, &req_size, &resp_size,
	    &is_admin);
	if (error)
		return (error);
	if (req_size > sizeof(struct ufshci_upiu))
		return (EINVAL);
	/*
	 * The completion union is smaller than a UPIU. An EHS must not
	 * push the response past it.
	 */
	if (resp_size > sizeof(status.cpl.response_upiu))
		return (EINVAL);
	/*
	 * Without EHSLUTRDS the controller cannot carry the EHS, so the
	 * device would answer a partial request.
	 */
	if (pt->req_upiu.header.ehs_length != 0 &&
	    UFSHCIV(UFSHCI_CAP_REG_EHSLUTRDS, ctrlr->cap) == 0)
		return (EOPNOTSUPP);

	if (pt->len != 0) {
		buf = malloc(pt->len, M_UFSHCI, M_WAITOK | M_ZERO);
		if (pt->flags & UFSHCI_PT_FLAG_DATA_OUT) {
			error = copyin(pt->buf, buf, pt->len);
			if (error)
				goto out;
		}
	}

	req = ufshci_allocate_request_vaddr(buf, pt->len, M_WAITOK,
	    ufshci_completion_poll_cb, &status);

	memcpy(&req->request_upiu, &pt->req_upiu, sizeof(req->request_upiu));
	req->request_size = req_size;
	req->response_size = resp_size;
	req->is_admin = is_admin;

	if (pt->flags & UFSHCI_PT_FLAG_DATA_OUT)
		req->data_direction = UFSHCI_DATA_DIRECTION_FROM_SYS_TO_TGT;
	else if (pt->flags & UFSHCI_PT_FLAG_DATA_IN)
		req->data_direction = UFSHCI_DATA_DIRECTION_FROM_TGT_TO_SYS;
	else
		req->data_direction = UFSHCI_DATA_DIRECTION_NO_DATA_TRANSFER;

	error = ufshci_ctrlr_submit_transfer_request(ctrlr, req);
	if (error) {
		ufshci_free_request(req);
		goto out;
	}

	ufshci_completion_poll(&status);

	memcpy(&pt->resp_upiu, &status.cpl.response_upiu,
	    min(sizeof(pt->resp_upiu), sizeof(status.cpl.response_upiu)));
	pt->xfer_len = pt->len;
	/* The completion does not carry the overall command status yet. */
	pt->ocs = 0;

	/*
	 * A refused command still answered, so let the caller read the
	 * response. A failure that left the response zeroed had no answer.
	 */
	if (status.error && pt->resp_upiu.header.response == 0) {
		error = EIO;
		goto out;
	}

	if (pt->len != 0 && (pt->flags & UFSHCI_PT_FLAG_DATA_IN))
		error = copyout(buf, pt->buf, pt->len);

out:
	free(buf, M_UFSHCI);
	return (error);
}

static int
ufshci_passthrough_uic_cmd(struct ufshci_controller *ctrlr,
    struct ufshci_pt_uic_command *pt)
{
	uint32_t return_value = 0;
	int error;

	if (pt->timeout_ms != 0)
		return (EINVAL);

	/*
	 * Only the four attribute commands. The rest can drop the link or
	 * power the device off.
	 */
	switch (pt->cmd.opcode) {
	case UFSHCI_DME_GET:
	case UFSHCI_DME_SET:
	case UFSHCI_DME_PEER_GET:
	case UFSHCI_DME_PEER_SET:
		break;
	default:
		return (EINVAL);
	}

	/*
	 * On a path that never reads the register, a value the caller left
	 * here would read as an answer.
	 */
	pt->cmd.argument2 &= ~(UFSHCI_UICCMDARG2_REG_ERROR_CODE_MASK <<
	    UFSHCI_UICCMDARG2_REG_ERROR_CODE_SHIFT);

	error = ufshci_uic_send_cmd(ctrlr, &pt->cmd, &return_value);

	pt->result = UFSHCIV(UFSHCI_UICCMDARG2_REG_ERROR_CODE,
	    pt->cmd.argument2);
	if (pt->result != 0) {
		/*
		 * A refusal is a result, not a transport failure. There is no
		 * attribute value to go with it, and the caller's own input
		 * would read like one.
		 */
		pt->cmd.argument3 = 0;
		return (0);
	}
	if (error)
		return (error);

	pt->cmd.argument3 = return_value;

	return (0);
}

static int
ufshci_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int flag,
    struct thread *td)
{
	struct ufshci_controller *ctrlr = cdev->si_drv1;

	switch (cmd) {
	case UFSHCI_PASSTHROUGH_CMD:
		return (ufshci_passthrough_cmd(ctrlr,
		    (struct ufshci_pt_command *)arg));
	case UFSHCI_PASSTHROUGH_UIC:
		return (ufshci_passthrough_uic_cmd(ctrlr,
		    (struct ufshci_pt_uic_command *)arg));
	default:
		return (ENOTTY);
	}
}

int
ufshci_ioctl_construct(struct ufshci_controller *ctrlr, device_t dev)
{
	struct make_dev_args md_args;
	int error;

	/*
	 * Set si_drv1 as the node is created. A separate assignment after
	 * make_dev leaves a window where an open that races the attach finds
	 * it unset.
	 */
	make_dev_args_init(&md_args);
	md_args.mda_devsw = &ufshci_cdevsw;
	md_args.mda_uid = UID_ROOT;
	md_args.mda_gid = GID_WHEEL;
	md_args.mda_mode = 0600;
	md_args.mda_unit = device_get_unit(dev);
	md_args.mda_si_drv1 = ctrlr;

	error = make_dev_s(&md_args, &ctrlr->cdev, "ufshci%d",
	    device_get_unit(dev));
	if (error != 0)
		return (error);

	return (0);
}

void
ufshci_ioctl_destruct(struct ufshci_controller *ctrlr)
{
	if (ctrlr->cdev != NULL) {
		destroy_dev(ctrlr->cdev);
		ctrlr->cdev = NULL;
	}
}
