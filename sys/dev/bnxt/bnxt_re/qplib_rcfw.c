/*
 * Copyright (c) 2015-2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: RDMA Controller HW interface
 */

#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/hardirq.h>
#include <linux/device.h>

#include "hsi_struct_def.h"
#include "qplib_tlv.h"
#include "qplib_res.h"
#include "qplib_sp.h"
#include "qplib_rcfw.h"
#include "bnxt_re.h"

static void bnxt_qplib_service_creq(unsigned long data);

int __check_cmdq_stall(struct bnxt_qplib_rcfw *rcfw,
			      u32 *cur_prod, u32 *cur_cons)
{
	struct bnxt_qplib_cmdq_ctx *cmdq;

	cmdq = &rcfw->cmdq;

	if (*cur_prod == cmdq->hwq.prod &&
	    *cur_cons == cmdq->hwq.cons)
		/* No activity on CMDQ or CREQ. FW down */
		return -ETIMEDOUT;

	*cur_prod = cmdq->hwq.prod;
	*cur_cons = cmdq->hwq.cons;
	return 0;
}

static int bnxt_qplib_map_rc(u8 opcode)
{
	switch (opcode) {
	case CMDQ_BASE_OPCODE_DESTROY_QP:
	case CMDQ_BASE_OPCODE_DESTROY_SRQ:
	case CMDQ_BASE_OPCODE_DESTROY_CQ:
	case CMDQ_BASE_OPCODE_DEALLOCATE_KEY:
	case CMDQ_BASE_OPCODE_DEREGISTER_MR:
	case CMDQ_BASE_OPCODE_DELETE_GID:
	case CMDQ_BASE_OPCODE_DESTROY_QP1:
	case CMDQ_BASE_OPCODE_DESTROY_AH:
	case CMDQ_BASE_OPCODE_DEINITIALIZE_FW:
	case CMDQ_BASE_OPCODE_MODIFY_ROCE_CC:
	case CMDQ_BASE_OPCODE_SET_LINK_AGGR_MODE:
		return 0;
	default:
		return -ETIMEDOUT;
	}
}

/**
 * bnxt_re_is_fw_stalled   -	Check firmware health
 * @rcfw      -   rcfw channel instance of rdev
 * @cookie    -   cookie to track the command
 *
 * If firmware has not responded any rcfw command within
 * rcfw->max_timeout, consider firmware as stalled.
 *
 * Returns:
 * 0 if firmware is responding
 * -ENODEV if firmware is not responding
 */
static int bnxt_re_is_fw_stalled(struct bnxt_qplib_rcfw *rcfw, u16 cookie)
{
	struct bnxt_qplib_cmdq_ctx *cmdq;
	struct bnxt_qplib_crsqe *crsqe;

	crsqe = &rcfw->crsqe_tbl[cookie];
	cmdq = &rcfw->cmdq;

	if (time_after(jiffies, cmdq->last_seen +
		      (rcfw->max_timeout * HZ))) {
		dev_warn_ratelimited(&rcfw->pdev->dev,
				     "%s: FW STALL Detected. cmdq[%#x]=%#x waited (%ld > %d) msec active %d\n",
				     __func__, cookie, crsqe->opcode,
				     (long)jiffies_to_msecs(jiffies - cmdq->last_seen),
				     rcfw->max_timeout * 1000,
				     crsqe->is_in_used);
		return -ENODEV;
	}

	return 0;
}
/**
 * __wait_for_resp   -	Don't hold the cpu context and wait for response
 * @rcfw      -   rcfw channel instance of rdev
 * @cookie    -   cookie to track the command
 *
 * Wait for command completion in sleepable context.
 *
 * Returns:
 * 0 if command is completed by firmware.
 * Non zero error code for rest of the case.
 */
static int __wait_for_resp(struct bnxt_qplib_rcfw *rcfw, u16 cookie)
{
	struct bnxt_qplib_cmdq_ctx *cmdq;
	struct bnxt_qplib_crsqe *crsqe;
	unsigned long issue_time;
	int ret;

	cmdq = &rcfw->cmdq;
	issue_time = jiffies;
	crsqe = &rcfw->crsqe_tbl[cookie];

	do {
		if (RCFW_NO_FW_ACCESS(rcfw))
			return bnxt_qplib_map_rc(crsqe->opcode);
		if (test_bit(FIRMWARE_STALL_DETECTED, &cmdq->flags))
			return -ETIMEDOUT;

		/* Non zero means command completed */
		ret = wait_event_timeout(cmdq->waitq,
					 !crsqe->is_in_used ||
					 RCFW_NO_FW_ACCESS(rcfw),
					 msecs_to_jiffies(rcfw->max_timeout * 1000));

		if (!crsqe->is_in_used)
			return 0;
		/*
		 * Take care if interrupt miss or other cases like DBR drop
		 */
		bnxt_qplib_service_creq((unsigned long)rcfw);
		dev_warn_ratelimited(&rcfw->pdev->dev,
			"Non-Blocking QPLIB: cmdq[%#x]=%#x waited (%lu) msec bit %d\n",
			cookie, crsqe->opcode,
			(long)jiffies_to_msecs(jiffies - issue_time),
			crsqe->is_in_used);

		if (!crsqe->is_in_used)
			return 0;

		ret = bnxt_re_is_fw_stalled(rcfw, cookie);
		if (ret)
			return ret;

	} while (true);
};

/**
 * __block_for_resp   -	hold the cpu context and wait for response
 * @rcfw      -   rcfw channel instance of rdev
 * @cookie    -   cookie to track the command
 *
 * This function will hold the cpu (non-sleepable context) and
 * wait for command completion. Maximum holding interval is 8 second.
 *
 * Returns:
 * -ETIMEOUT if command is not completed in specific time interval.
 * 0 if command is completed by firmware.
 */
static int __block_for_resp(struct bnxt_qplib_rcfw *rcfw, u16 cookie)
{
	struct bnxt_qplib_cmdq_ctx *cmdq = &rcfw->cmdq;
	struct bnxt_qplib_crsqe *crsqe;
	unsigned long issue_time = 0;

	issue_time = jiffies;
	crsqe = &rcfw->crsqe_tbl[cookie];

	do {
		if (RCFW_NO_FW_ACCESS(rcfw))
			return bnxt_qplib_map_rc(crsqe->opcode);
		if (test_bit(FIRMWARE_STALL_DETECTED, &cmdq->flags))
			return -ETIMEDOUT;

		udelay(1);

		/* Below call is must since there can be a deadlock
		 * if interrupt is mapped to the same cpu
		 */
		bnxt_qplib_service_creq((unsigned long)rcfw);
		if (!crsqe->is_in_used)
			return 0;

	} while (time_before(jiffies, issue_time + (8 * HZ)));

	dev_warn_ratelimited(&rcfw->pdev->dev,
		"Blocking QPLIB: cmdq[%#x]=%#x taken (%lu) msec",
		cookie, crsqe->opcode,
		(long)jiffies_to_msecs(jiffies - issue_time));

	return -ETIMEDOUT;
};

/*  __send_message_no_waiter -	get cookie and post the message.
 * @rcfw      -   rcfw channel instance of rdev
 * @msg      -    qplib message internal
 *
 * This function will just post and don't bother about completion.
 * Current design of this function is -
 * user must hold the completion queue hwq->lock.
 * user must have used existing completion and free the resources.
 * this function will not check queue full condition.
 * this function will explicitly set is_waiter_alive=false.
 * current use case is - send destroy_ah if create_ah is return
 * after waiter of create_ah is lost. It can be extended for other
 * use case as well.
 *
 * Returns: Nothing
 *
 */
static  void __send_message_no_waiter(struct bnxt_qplib_rcfw *rcfw,
			  struct bnxt_qplib_cmdqmsg *msg)
{
	struct bnxt_qplib_cmdq_ctx *cmdq = &rcfw->cmdq;
	struct bnxt_qplib_hwq *cmdq_hwq = &cmdq->hwq;
	struct bnxt_qplib_crsqe *crsqe;
	struct bnxt_qplib_cmdqe *cmdqe;
	u32 sw_prod, cmdq_prod, bsize;
	u16 cookie;
	u8 *preq;

	cookie = cmdq->seq_num & RCFW_MAX_COOKIE_VALUE;
	__set_cmdq_base_cookie(msg->req, msg->req_sz, cpu_to_le16(cookie));
	crsqe = &rcfw->crsqe_tbl[cookie];

	/* Set cmd_size in terms of 16B slots in req. */
	bsize = bnxt_qplib_set_cmd_slots(msg->req);
	/* GET_CMD_SIZE would return number of slots in either case of tlv
	 * and non-tlv commands after call to bnxt_qplib_set_cmd_slots()
	 */
	crsqe->send_timestamp = jiffies;
	crsqe->is_internal_cmd = true;
	crsqe->is_waiter_alive = false;
	crsqe->is_in_used = true;
	crsqe->req_size = __get_cmdq_base_cmd_size(msg->req, msg->req_sz);

	preq = (u8 *)msg->req;
	do {
		/* Locate the next cmdq slot */
		sw_prod = HWQ_CMP(cmdq_hwq->prod, cmdq_hwq);
		cmdqe = bnxt_qplib_get_qe(cmdq_hwq, sw_prod, NULL);
		/* Copy a segment of the req cmd to the cmdq */
		memset(cmdqe, 0, sizeof(*cmdqe));
		memcpy(cmdqe, preq, min_t(u32, bsize, sizeof(*cmdqe)));
		preq += min_t(u32, bsize, sizeof(*cmdqe));
		bsize -= min_t(u32, bsize, sizeof(*cmdqe));
		cmdq_hwq->prod++;
	} while (bsize > 0);
	cmdq->seq_num++;

	cmdq_prod = cmdq_hwq->prod & 0xFFFF;
	atomic_inc(&rcfw->timeout_send);
	/* ring CMDQ DB */
	wmb();
	writel(cmdq_prod, cmdq->cmdq_mbox.prod);
	writel(RCFW_CMDQ_TRIG_VAL, cmdq->cmdq_mbox.db);
}

static int __send_message(struct bnxt_qplib_rcfw *rcfw,
			  struct bnxt_qplib_cmdqmsg *msg)
{
	u32 bsize, free_slots, required_slots;
	struct bnxt_qplib_cmdq_ctx *cmdq;
	struct bnxt_qplib_crsqe *crsqe;
	struct bnxt_qplib_cmdqe *cmdqe;
	struct bnxt_qplib_hwq *cmdq_hwq;
	u32 sw_prod, cmdq_prod;
	struct pci_dev *pdev;
	unsigned long flags;
	u16 cookie;
	u8 opcode;
	u8 *preq;

	cmdq = &rcfw->cmdq;
	cmdq_hwq = &cmdq->hwq;
	pdev = rcfw->pdev;
	opcode = __get_cmdq_base_opcode(msg->req, msg->req_sz);

	/* Cmdq are in 16-byte units, each request can consume 1 or more
	   cmdqe */
	spin_lock_irqsave(&cmdq_hwq->lock, flags);
	required_slots = bnxt_qplib_get_cmd_slots(msg->req);
	free_slots = HWQ_FREE_SLOTS(cmdq_hwq);
	cookie = cmdq->seq_num & RCFW_MAX_COOKIE_VALUE;
	crsqe = &rcfw->crsqe_tbl[cookie];

	if (required_slots >= free_slots) {
		dev_warn_ratelimited(&pdev->dev,
				"QPLIB: RCFW: CMDQ is full req/free %d/%d!\n",
				required_slots, free_slots);
		rcfw->cmdq_full_dbg++;
		spin_unlock_irqrestore(&cmdq_hwq->lock, flags);
		return -EAGAIN;
	}

	if (crsqe->is_in_used)
		panic("QPLIB: Cookie was not requested %d\n",
				cookie);

	if (msg->block)
		cookie |= RCFW_CMD_IS_BLOCKING;
	__set_cmdq_base_cookie(msg->req, msg->req_sz, cpu_to_le16(cookie));

	/* Set cmd_size in terms of 16B slots in req. */
	bsize = bnxt_qplib_set_cmd_slots(msg->req);
	/* GET_CMD_SIZE would return number of slots in either case of tlv
	 * and non-tlv commands after call to bnxt_qplib_set_cmd_slots()
	 */
	crsqe->send_timestamp = jiffies;
	crsqe->free_slots = free_slots;
	crsqe->resp = (struct creq_qp_event *)msg->resp;
	crsqe->resp->cookie = cpu_to_le16(cookie);
	crsqe->is_internal_cmd = false;
	crsqe->is_waiter_alive = true;
	crsqe->is_in_used = true;
	crsqe->opcode = opcode;
	crsqe->requested_qp_state = msg->qp_state;

	crsqe->req_size = __get_cmdq_base_cmd_size(msg->req, msg->req_sz);
	if (__get_cmdq_base_resp_size(msg->req, msg->req_sz) && msg->sb) {
		struct bnxt_qplib_rcfw_sbuf *sbuf = msg->sb;

		__set_cmdq_base_resp_addr(msg->req, msg->req_sz,
					  cpu_to_le64(sbuf->dma_addr));
		__set_cmdq_base_resp_size(msg->req, msg->req_sz,
					  ALIGN(sbuf->size, BNXT_QPLIB_CMDQE_UNITS) /
					   BNXT_QPLIB_CMDQE_UNITS);
	}

	preq = (u8 *)msg->req;
	do {
		/* Locate the next cmdq slot */
		sw_prod = HWQ_CMP(cmdq_hwq->prod, cmdq_hwq);
		cmdqe = bnxt_qplib_get_qe(cmdq_hwq, sw_prod, NULL);
		/* Copy a segment of the req cmd to the cmdq */
		memset(cmdqe, 0, sizeof(*cmdqe));
		memcpy(cmdqe, preq, min_t(u32, bsize, sizeof(*cmdqe)));
		preq += min_t(u32, bsize, sizeof(*cmdqe));
		bsize -= min_t(u32, bsize, sizeof(*cmdqe));
		cmdq_hwq->prod++;
	} while (bsize > 0);
	cmdq->seq_num++;

	cmdq_prod = cmdq_hwq->prod & 0xFFFF;
	if (test_bit(FIRMWARE_FIRST_FLAG, &cmdq->flags)) {
		/* The very first doorbell write
		 * is required to set this flag
		 * which prompts the FW to reset
		 * its internal pointers
		 */
		cmdq_prod |= BIT(FIRMWARE_FIRST_FLAG);
		clear_bit(FIRMWARE_FIRST_FLAG, &cmdq->flags);
	}
	/* ring CMDQ DB */
	wmb();
	writel(cmdq_prod, cmdq->cmdq_mbox.prod);
	writel(RCFW_CMDQ_TRIG_VAL, cmdq->cmdq_mbox.db);

	dev_dbg(&pdev->dev, "QPLIB: RCFW sent request with 0x%x 0x%x 0x%x\n",
			cmdq_prod, cmdq_hwq->prod, crsqe->req_size);
	dev_dbg(&pdev->dev,
		"QPLIB: opcode 0x%x with cookie 0x%x at cmdq/crsq 0x%p/0x%p\n",
		opcode,
		__get_cmdq_base_cookie(msg->req, msg->req_sz),
		cmdqe, crsqe);
	spin_unlock_irqrestore(&cmdq_hwq->lock, flags);
	/* Return the CREQ response pointer */
	return 0;
}

/**
 * __poll_for_resp   -	self poll completion for rcfw command
 * @rcfw      -   rcfw channel instance of rdev
 * @cookie    -   cookie to track the command
 *
 * It works same as __wait_for_resp except this function will
 * do self polling in sort interval since interrupt is disabled.
 * This function can not be called from non-sleepable context.
 *
 * Returns:
 * -ETIMEOUT if command is not completed in specific time interval.
 * 0 if command is completed by firmware.
 */
static int __poll_for_resp(struct bnxt_qplib_rcfw *rcfw, u16 cookie)
{
	struct bnxt_qplib_cmdq_ctx *cmdq = &rcfw->cmdq;
	struct bnxt_qplib_crsqe *crsqe;
	unsigned long issue_time;
	int ret;

	issue_time = jiffies;
	crsqe = &rcfw->crsqe_tbl[cookie];

	do {
		if (RCFW_NO_FW_ACCESS(rcfw))
			return bnxt_qplib_map_rc(crsqe->opcode);
		if (test_bit(FIRMWARE_STALL_DETECTED, &cmdq->flags))
			return -ETIMEDOUT;

		usleep_range(1000, 1001);

		bnxt_qplib_service_creq((unsigned long)rcfw);
		if (!crsqe->is_in_used)
			return 0;

		if (jiffies_to_msecs(jiffies - issue_time) >
		    (rcfw->max_timeout * 1000)) {
			dev_warn_ratelimited(&rcfw->pdev->dev,
				"Self Polling QPLIB: cmdq[%#x]=%#x taken (%lu) msec",
				cookie, crsqe->opcode,
				(long)jiffies_to_msecs(jiffies - issue_time));
			ret = bnxt_re_is_fw_stalled(rcfw, cookie);
			if (ret)
				return ret;
		}
	} while (true);

};

static int __send_message_basic_sanity(struct bnxt_qplib_rcfw *rcfw,
			  struct bnxt_qplib_cmdqmsg *msg, u8 opcode)
{
	struct bnxt_qplib_cmdq_ctx *cmdq;

	cmdq = &rcfw->cmdq;

	/* Prevent posting if f/w is not in a state to process */
	if (RCFW_NO_FW_ACCESS(rcfw))
		return -ENXIO;

	if (test_bit(FIRMWARE_STALL_DETECTED, &cmdq->flags))
		return -ETIMEDOUT;

	if (test_bit(FIRMWARE_INITIALIZED_FLAG, &cmdq->flags) &&
	    opcode == CMDQ_BASE_OPCODE_INITIALIZE_FW) {
		dev_err(&rcfw->pdev->dev, "QPLIB: RCFW already initialized!\n");
		return -EINVAL;
	}

	if (!test_bit(FIRMWARE_INITIALIZED_FLAG, &cmdq->flags) &&
	    (opcode != CMDQ_BASE_OPCODE_QUERY_FUNC &&
	     opcode != CMDQ_BASE_OPCODE_INITIALIZE_FW &&
	     opcode != CMDQ_BASE_OPCODE_QUERY_VERSION)) {
		dev_err(&rcfw->pdev->dev,
			"QPLIB: RCFW not initialized, reject opcode 0x%x\n",
			opcode);
		return -ENOTSUPP;
	}

	return 0;
}

/* This function will just post and do not bother about completion */
static  void __destroy_timedout_ah(struct bnxt_qplib_rcfw *rcfw,
			  struct creq_create_ah_resp *create_ah_resp)
{
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_destroy_ah req = {};

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DESTROY_AH,
				 sizeof(req));
	req.ah_cid = create_ah_resp->xid;
	msg.req = (struct cmdq_base *)&req;
	msg.req_sz = sizeof(req);
	__send_message_no_waiter(rcfw, &msg);
	dev_warn_ratelimited(&rcfw->pdev->dev,
		"From %s: ah_cid = %d timeout_send %d\n", __func__,
		req.ah_cid,
		atomic_read(&rcfw->timeout_send));
}

/**
 * __bnxt_qplib_rcfw_send_message   -	qplib interface to send
 * and complete rcfw command.
 * @rcfw      -   rcfw channel instance of rdev
 * @msg      -    qplib message internal
 *
 * This function does not account shadow queue depth. It will send
 * all the command unconditionally as long as send queue is not full.
 *
 * Returns:
 * 0 if command completed by firmware.
 * Non zero if the command is not completed by firmware.
 */
static int __bnxt_qplib_rcfw_send_message(struct bnxt_qplib_rcfw *rcfw,
				   struct bnxt_qplib_cmdqmsg *msg)
{
	struct bnxt_qplib_crsqe *crsqe;
	struct creq_qp_event *event;
	unsigned long flags;
	u16 cookie;
	int rc = 0;
	u8 opcode;

	opcode = __get_cmdq_base_opcode(msg->req, msg->req_sz);

	rc = __send_message_basic_sanity(rcfw, msg, opcode);
	if (rc)
		return rc == -ENXIO ? bnxt_qplib_map_rc(opcode) : rc;

	rc = __send_message(rcfw, msg);
	if (rc)
		return rc;

	cookie = le16_to_cpu(__get_cmdq_base_cookie(msg->req,
				msg->req_sz)) & RCFW_MAX_COOKIE_VALUE;


	if (msg->block)
		rc = __block_for_resp(rcfw, cookie);
	else if (atomic_read(&rcfw->rcfw_intr_enabled))
		rc = __wait_for_resp(rcfw, cookie);
	else
		rc = __poll_for_resp(rcfw, cookie);

	if (rc) {
		/* First check if it is FW stall.
		 * Use hwq.lock to avoid race with actual completion.
		 */
		spin_lock_irqsave(&rcfw->cmdq.hwq.lock, flags);
		crsqe = &rcfw->crsqe_tbl[cookie];
		crsqe->is_waiter_alive = false;
		if (rc == -ENODEV)
			set_bit(FIRMWARE_STALL_DETECTED, &rcfw->cmdq.flags);
		spin_unlock_irqrestore(&rcfw->cmdq.hwq.lock, flags);

		return -ETIMEDOUT;
	}

	event = (struct creq_qp_event *)msg->resp;
	if (event->status) {
		/* failed with status */
		dev_err(&rcfw->pdev->dev, "QPLIB: cmdq[%#x]=%#x (%s) status %d\n",
			cookie, opcode, GET_OPCODE_TYPE(opcode), event->status);
		rc = -EFAULT;
		/*
		 * Workaround to avoid errors in the stack during bond
		 * creation and deletion.
		 * Disable error returned for  ADD_GID/DEL_GID
		 */
		if (opcode == CMDQ_BASE_OPCODE_ADD_GID ||
		    opcode == CMDQ_BASE_OPCODE_DELETE_GID)
			rc = 0;
	}

	dev_dbg(&pdev->dev, "QPLIB: %s:%d - op 0x%x (%s), cookie 0x%x -- Return: e->status 0x%x, rc = 0x%x\n",
		__func__, __LINE__, opcode, GET_OPCODE_TYPE(opcode), cookie, event->status, rc);
	return rc;
}

/**
 * bnxt_qplib_rcfw_send_message   -	qplib interface to send
 * and complete rcfw command.
 * @rcfw      -   rcfw channel instance of rdev
 * @msg      -    qplib message internal
 *
 * Driver interact with Firmware through rcfw channel/slow path in two ways.
 * a. Blocking rcfw command send. In this path, driver cannot hold
 * the context for longer period since it is holding cpu until
 * command is not completed.
 * b. Non-blocking rcfw command send. In this path, driver can hold the
 * context for longer period. There may be many pending command waiting
 * for completion because of non-blocking nature.
 *
 * Driver will use shadow queue depth. Current queue depth of 8K
 * (due to size of rcfw message it can be actual ~4K rcfw outstanding)
 * is not optimal for rcfw command processing in firmware.
 * RCFW_CMD_NON_BLOCKING_SHADOW_QD is defined as 64.
 * Restrict at max 64 Non-Blocking rcfw commands.
 * Do not allow more than 64 non-blocking command to the Firmware.
 * Allow all blocking commands until there is no queue full.
 *
 * Returns:
 * 0 if command completed by firmware.
 * Non zero if the command is not completed by firmware.
 */
int bnxt_qplib_rcfw_send_message(struct bnxt_qplib_rcfw *rcfw,
				 struct bnxt_qplib_cmdqmsg *msg)
{
	int ret;

	if (!msg->block) {
		down(&rcfw->rcfw_inflight);
		ret = __bnxt_qplib_rcfw_send_message(rcfw, msg);
		up(&rcfw->rcfw_inflight);
	} else {
		ret = __bnxt_qplib_rcfw_send_message(rcfw, msg);
	}

	return ret;
}

static void bnxt_re_add_perf_stats(struct bnxt_qplib_rcfw *rcfw,
		struct bnxt_qplib_crsqe *crsqe)
{
	u32 latency_msec, dest_stats_id;
	u64 *dest_stats_ptr = NULL;

	latency_msec = jiffies_to_msecs(rcfw->cmdq.last_seen -
				crsqe->send_timestamp);
	if (latency_msec/1000 < RCFW_MAX_LATENCY_SEC_SLAB_INDEX)
		rcfw->rcfw_lat_slab_sec[latency_msec/1000]++;

	if (!rcfw->sp_perf_stats_enabled)
		return;

	if (latency_msec < RCFW_MAX_LATENCY_MSEC_SLAB_INDEX)
		rcfw->rcfw_lat_slab_msec[latency_msec]++;

	switch (crsqe->opcode) {
	case CMDQ_BASE_OPCODE_CREATE_QP:
		dest_stats_id = rcfw->qp_create_stats_id++;
		dest_stats_id = dest_stats_id % RCFW_MAX_STAT_INDEX;
		dest_stats_ptr = &rcfw->qp_create_stats[dest_stats_id];
		break;
	case CMDQ_BASE_OPCODE_DESTROY_QP:
		dest_stats_id = rcfw->qp_destroy_stats_id++;
		dest_stats_id = dest_stats_id % RCFW_MAX_STAT_INDEX;
		dest_stats_ptr = &rcfw->qp_destroy_stats[dest_stats_id];
		break;
	case CMDQ_BASE_OPCODE_REGISTER_MR:
		dest_stats_id = rcfw->mr_create_stats_id++;
		dest_stats_id = dest_stats_id % RCFW_MAX_STAT_INDEX;
		dest_stats_ptr = &rcfw->mr_create_stats[dest_stats_id];
		break;
	case CMDQ_BASE_OPCODE_DEREGISTER_MR:
	case CMDQ_BASE_OPCODE_DEALLOCATE_KEY:
		dest_stats_id = rcfw->mr_destroy_stats_id++;
		dest_stats_id = dest_stats_id % RCFW_MAX_STAT_INDEX;
		dest_stats_ptr = &rcfw->mr_destroy_stats[dest_stats_id];
		break;
	case CMDQ_BASE_OPCODE_MODIFY_QP:
		if (crsqe->requested_qp_state != IB_QPS_ERR)
			break;
		dest_stats_id = rcfw->qp_modify_stats_id++;
		dest_stats_id = dest_stats_id % RCFW_MAX_STAT_INDEX;
		dest_stats_ptr = &rcfw->qp_modify_stats[dest_stats_id];
		break;
	default:
		break;
	}
	if (dest_stats_ptr)
		*dest_stats_ptr = max_t(unsigned long,
				(rcfw->cmdq.last_seen - crsqe->send_timestamp), 1);

}

/* Completions */
static int bnxt_qplib_process_qp_event(struct bnxt_qplib_rcfw *rcfw,
				       struct creq_qp_event *event,
				       u32 *num_wait)
{
	struct bnxt_qplib_hwq *cmdq_hwq = &rcfw->cmdq.hwq;
	struct creq_cq_error_notification *cqerr;
	struct creq_qp_error_notification *qperr;
	struct bnxt_qplib_crsqe *crsqe;
	struct bnxt_qplib_reftbl *tbl;
	struct bnxt_qplib_qp *qp;
	struct bnxt_qplib_cq *cq;
	u16 cookie, blocked = 0;
	struct pci_dev *pdev;
	bool is_waiter_alive;
	unsigned long flags;
	u32 wait_cmds = 0;
	u32 xid, qp_idx;
	u32 req_size;
	int rc = 0;

	pdev = rcfw->pdev;
	switch (event->event) {
	case CREQ_QP_EVENT_EVENT_QP_ERROR_NOTIFICATION:
		tbl = &rcfw->res->reftbl.qpref;
		qperr = (struct creq_qp_error_notification *)event;
		xid = le32_to_cpu(qperr->xid);
		qp_idx = map_qp_id_to_tbl_indx(xid, tbl);
		spin_lock(&tbl->lock);
		qp = tbl->rec[qp_idx].handle;
		if (!qp) {
			spin_unlock(&tbl->lock);
			break;
		}
		bnxt_qplib_mark_qp_error(qp);
		rc = rcfw->creq.aeq_handler(rcfw, event, qp);
		spin_unlock(&tbl->lock);
		/*
		 * Keeping these prints as debug to avoid flooding of log
		 * messages during modify QP to error state by applications
		 */
		dev_dbg(&pdev->dev, "QPLIB: QP Error encountered!\n");
		dev_dbg(&pdev->dev,
			"QPLIB: qpid 0x%x, req_err=0x%x, resp_err=0x%x\n",
			xid, qperr->req_err_state_reason,
			qperr->res_err_state_reason);
		break;
	case CREQ_QP_EVENT_EVENT_CQ_ERROR_NOTIFICATION:
		tbl = &rcfw->res->reftbl.cqref;
		cqerr = (struct creq_cq_error_notification *)event;
		xid = le32_to_cpu(cqerr->xid);
		spin_lock(&tbl->lock);
		cq = tbl->rec[GET_TBL_INDEX(xid, tbl)].handle;
		if (!cq) {
			spin_unlock(&tbl->lock);
			break;
		}
		rc = rcfw->creq.aeq_handler(rcfw, event, cq);
		spin_unlock(&tbl->lock);
		dev_dbg(&pdev->dev, "QPLIB: CQ error encountered!\n");
		break;
	default:
		/*
		 * Command Response
		 * cmdq hwq lock needs to be acquired to synchronize
		 * the command send and completion reaping. This function
		 * is always called with creq hwq lock held. So there is no
		 * chance of deadlock here as the locking is in correct sequence.
		 * Using  the nested variant of spin_lock to annotate
		 */
		spin_lock_irqsave_nested(&cmdq_hwq->lock, flags,
					 SINGLE_DEPTH_NESTING);
		cookie = le16_to_cpu(event->cookie);
		blocked = cookie & RCFW_CMD_IS_BLOCKING;
		cookie &= RCFW_MAX_COOKIE_VALUE;

		crsqe = &rcfw->crsqe_tbl[cookie];

		bnxt_re_add_perf_stats(rcfw, crsqe);

		if (WARN_ONCE(test_bit(FIRMWARE_STALL_DETECTED,
				       &rcfw->cmdq.flags),
		    "QPLIB: Unreponsive rcfw channel detected.!!")) {
			dev_info(&pdev->dev, "rcfw timedout: cookie = %#x,"
				" latency_msec = %ld free_slots = %d\n", cookie,
				(long)jiffies_to_msecs(rcfw->cmdq.last_seen -
						 crsqe->send_timestamp),
				crsqe->free_slots);
			spin_unlock_irqrestore(&cmdq_hwq->lock, flags);
			return rc;
		}

		if (crsqe->is_internal_cmd && !event->status)
			atomic_dec(&rcfw->timeout_send);

		if (crsqe->is_waiter_alive) {
			if (crsqe->resp)
				memcpy(crsqe->resp, event, sizeof(*event));
			if (!blocked)
				wait_cmds++;
		}

		req_size = crsqe->req_size;
		is_waiter_alive = crsqe->is_waiter_alive;

		crsqe->req_size = 0;
		if (!crsqe->is_waiter_alive)
			crsqe->resp = NULL;
		crsqe->is_in_used = false;
		/* Consumer is updated so that __send_message_no_waiter
		 * can never see queue full.
		 * It is safe since we are still holding cmdq_hwq->lock.
		 */
		cmdq_hwq->cons += req_size;

		/* This is a case to handle below scenario -
		 * Create AH is completed successfully by firmware,
		 * but completion took more time and driver already lost
		 * the context of create_ah from caller.
		 * We have already return failure for create_ah verbs,
		 * so let's destroy the same address vector since it is
		 * no more used in stack. We don't care about completion
		 * in __send_message_no_waiter.
		 * If destroy_ah is failued by firmware, there will be AH
		 * resource leak and relatively not critical +  unlikely
		 * scenario. Current design is not to handle such case.
		 */
		if (!is_waiter_alive && !event->status &&
		    event->event == CREQ_QP_EVENT_EVENT_CREATE_AH)
			__destroy_timedout_ah(rcfw,
					      (struct creq_create_ah_resp *)
					      event);

		spin_unlock_irqrestore(&cmdq_hwq->lock, flags);
	}
	*num_wait += wait_cmds;
	return rc;
}

/* SP - CREQ Completion handlers */
static void bnxt_qplib_service_creq(unsigned long data)
{
	struct bnxt_qplib_rcfw *rcfw = (struct bnxt_qplib_rcfw *)data;
	struct bnxt_qplib_creq_ctx *creq = &rcfw->creq;
	struct bnxt_qplib_res *res;
	u32 type, budget = CREQ_ENTRY_POLL_BUDGET;
	struct bnxt_qplib_hwq *creq_hwq = &creq->hwq;
	struct creq_base *creqe;
	struct pci_dev *pdev;
	unsigned long flags;
	u32 num_wakeup = 0;
	int rc;

	pdev = rcfw->pdev;
	res = rcfw->res;
	/* Service the CREQ until empty */
	spin_lock_irqsave(&creq_hwq->lock, flags);
	while (budget > 0) {
		if (RCFW_NO_FW_ACCESS(rcfw)) {
			spin_unlock_irqrestore(&creq_hwq->lock, flags);
			return;
		}
		creqe = bnxt_qplib_get_qe(creq_hwq, creq_hwq->cons, NULL);
		if (!CREQ_CMP_VALID(creqe, creq->creq_db.dbinfo.flags))
			break;
		/* The valid test of the entry must be done first before
		 * reading any further.
		 */
		dma_rmb();
		type = creqe->type & CREQ_BASE_TYPE_MASK;
		rcfw->cmdq.last_seen = jiffies;

		switch (type) {
		case CREQ_BASE_TYPE_QP_EVENT:
			bnxt_qplib_process_qp_event
				(rcfw,(struct creq_qp_event *)creqe,
				 &num_wakeup);
			creq->stats.creq_qp_event_processed++;
			break;
		case CREQ_BASE_TYPE_FUNC_EVENT:
			rc = rcfw->creq.aeq_handler(rcfw, creqe, NULL);
			if (rc)
				dev_warn(&pdev->dev,
					 "QPLIB: async event type = 0x%x not handled",
					 type);
			creq->stats.creq_func_event_processed++;
			break;
		default:
			if (type != HWRM_ASYNC_EVENT_CMPL_TYPE_HWRM_ASYNC_EVENT) {
				dev_warn(&pdev->dev,
					 "QPLIB: op_event = 0x%x not handled\n",
					 type);
			}
			break;
		}
		budget--;
		bnxt_qplib_hwq_incr_cons(creq_hwq->max_elements, &creq_hwq->cons,
					 1, &creq->creq_db.dbinfo.flags);
	}
	if (budget == CREQ_ENTRY_POLL_BUDGET &&
	    !CREQ_CMP_VALID(creqe, creq->creq_db.dbinfo.flags)) {
		/* No completions received during this poll. Enable interrupt now */
		bnxt_qplib_ring_nq_db(&creq->creq_db.dbinfo, res->cctx, true);
		creq->stats.creq_arm_count++;
		dev_dbg(&pdev->dev, "QPLIB: Num of Func (0x%llx) \n",
			creq->stats.creq_func_event_processed);
		dev_dbg(&pdev->dev, "QPLIB: QP (0x%llx) events processed\n",
			creq->stats.creq_qp_event_processed);
		dev_dbg(&pdev->dev, "QPLIB: Armed:%#llx resched:%#llx \n",
			creq->stats.creq_arm_count,
			creq->stats.creq_tasklet_schedule_count);
	} else if (creq->requested) {
		/*
		 * Currently there is no bottom half implementation to process
		 * completions, all completions are processed in interrupt context
		 * only. So enable interrupts.
		 */
		bnxt_qplib_ring_nq_db(&creq->creq_db.dbinfo, res->cctx, true);
		creq->stats.creq_tasklet_schedule_count++;
	}
	spin_unlock_irqrestore(&creq_hwq->lock, flags);
	if (num_wakeup)
		wake_up_all(&rcfw->cmdq.waitq);
}

static irqreturn_t bnxt_qplib_creq_irq(int irq, void *dev_instance)
{
	struct bnxt_qplib_rcfw *rcfw = dev_instance;

	bnxt_qplib_service_creq((unsigned long)rcfw);
	return IRQ_HANDLED;
}

/* RCFW */
int bnxt_qplib_deinit_rcfw(struct bnxt_qplib_rcfw *rcfw)
{
	struct creq_deinitialize_fw_resp resp = {};
	struct cmdq_deinitialize_fw req = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	int rc;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DEINITIALIZE_FW,
				 sizeof(req));
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL,
				sizeof(req), sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		return rc;
	clear_bit(FIRMWARE_INITIALIZED_FLAG, &rcfw->cmdq.flags);
	return 0;
}

int bnxt_qplib_init_rcfw(struct bnxt_qplib_rcfw *rcfw, int is_virtfn)
{
	struct creq_initialize_fw_resp resp = {};
	struct cmdq_initialize_fw req = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct bnxt_qplib_chip_ctx *cctx;
	struct bnxt_qplib_ctx *hctx;
	struct bnxt_qplib_res *res;
	struct bnxt_qplib_hwq *hwq;
	int rc;

	res = rcfw->res;
	cctx = res->cctx;
	hctx = res->hctx;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_INITIALIZE_FW,
				 sizeof(req));
	/* Supply (log-base-2-of-host-page-size - base-page-shift)
	 * to bono to adjust the doorbell page sizes.
	 */
	req.log2_dbr_pg_size = cpu_to_le16(PAGE_SHIFT -
					   RCFW_DBR_BASE_PAGE_SHIFT);
	/*
	 * VFs need not setup the HW context area, PF
	 * shall setup this area for VF. Skipping the
	 * HW programming
	 */
	if (is_virtfn || _is_chip_gen_p5_p7(cctx))
		goto skip_ctx_setup;

	hwq = &hctx->qp_ctx.hwq;
	req.qpc_page_dir = cpu_to_le64(_get_base_addr(hwq));
	req.number_of_qp = cpu_to_le32(hwq->max_elements);
	req.qpc_pg_size_qpc_lvl = (_get_pte_pg_size(hwq) <<
				   CMDQ_INITIALIZE_FW_QPC_PG_SIZE_SFT) |
				   (u8)hwq->level;

	hwq = &hctx->mrw_ctx.hwq;
	req.mrw_page_dir = cpu_to_le64(_get_base_addr(hwq));
	req.number_of_mrw = cpu_to_le32(hwq->max_elements);
	req.mrw_pg_size_mrw_lvl = (_get_pte_pg_size(hwq) <<
				   CMDQ_INITIALIZE_FW_MRW_PG_SIZE_SFT) |
				   (u8)hwq->level;

	hwq = &hctx->srq_ctx.hwq;
	req.srq_page_dir = cpu_to_le64(_get_base_addr(hwq));
	req.number_of_srq = cpu_to_le32(hwq->max_elements);
	req.srq_pg_size_srq_lvl = (_get_pte_pg_size(hwq) <<
				   CMDQ_INITIALIZE_FW_SRQ_PG_SIZE_SFT) |
				   (u8)hwq->level;

	hwq = &hctx->cq_ctx.hwq;
	req.cq_page_dir = cpu_to_le64(_get_base_addr(hwq));
	req.number_of_cq = cpu_to_le32(hwq->max_elements);
	req.cq_pg_size_cq_lvl = (_get_pte_pg_size(hwq) <<
				 CMDQ_INITIALIZE_FW_CQ_PG_SIZE_SFT) |
				 (u8)hwq->level;

	hwq = &hctx->tim_ctx.hwq;
	req.tim_page_dir = cpu_to_le64(_get_base_addr(hwq));
	req.tim_pg_size_tim_lvl = (_get_pte_pg_size(hwq) <<
				   CMDQ_INITIALIZE_FW_TIM_PG_SIZE_SFT) |
				   (u8)hwq->level;
	hwq = &hctx->tqm_ctx.pde;
	req.tqm_page_dir = cpu_to_le64(_get_base_addr(hwq));
	req.tqm_pg_size_tqm_lvl = (_get_pte_pg_size(hwq) <<
				   CMDQ_INITIALIZE_FW_TQM_PG_SIZE_SFT) |
				   (u8)hwq->level;
skip_ctx_setup:
	if (BNXT_RE_HW_RETX(res->dattr->dev_cap_flags))
		req.flags |= CMDQ_INITIALIZE_FW_FLAGS_HW_REQUESTER_RETX_SUPPORTED;
	req.stat_ctx_id = cpu_to_le32(hctx->stats.fw_id);
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL,
				sizeof(req), sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		return rc;
	set_bit(FIRMWARE_INITIALIZED_FLAG, &rcfw->cmdq.flags);

	return 0;
}

void bnxt_qplib_free_rcfw_channel(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;

	vfree(rcfw->rcfw_lat_slab_msec);
	rcfw->rcfw_lat_slab_msec = NULL;
	vfree(rcfw->qp_create_stats);
	rcfw->qp_create_stats = NULL;
	vfree(rcfw->qp_destroy_stats);
	rcfw->qp_destroy_stats = NULL;
	vfree(rcfw->mr_create_stats);
	rcfw->mr_create_stats = NULL;
	vfree(rcfw->mr_destroy_stats);
	rcfw->mr_destroy_stats = NULL;
	vfree(rcfw->qp_modify_stats);
	rcfw->qp_modify_stats = NULL;
	rcfw->sp_perf_stats_enabled = false;

	kfree(rcfw->crsqe_tbl);
	rcfw->crsqe_tbl = NULL;

	bnxt_qplib_free_hwq(res, &rcfw->cmdq.hwq);
	bnxt_qplib_free_hwq(res, &rcfw->creq.hwq);
	rcfw->pdev = NULL;
}

int bnxt_qplib_alloc_rcfw_channel(struct bnxt_qplib_res *res)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct bnxt_qplib_sg_info sginfo = {};
	struct bnxt_qplib_cmdq_ctx *cmdq;
	struct bnxt_qplib_creq_ctx *creq;

	rcfw->pdev = res->pdev;
	rcfw->res = res;
	cmdq = &rcfw->cmdq;
	creq = &rcfw->creq;

	sginfo.pgsize = PAGE_SIZE;
	sginfo.pgshft = PAGE_SHIFT;

	hwq_attr.sginfo = &sginfo;
	hwq_attr.res = rcfw->res;
	hwq_attr.depth = BNXT_QPLIB_CREQE_MAX_CNT;
	hwq_attr.stride = BNXT_QPLIB_CREQE_UNITS;
	hwq_attr.type = _get_hwq_type(res);

	if (bnxt_qplib_alloc_init_hwq(&creq->hwq, &hwq_attr)) {
		dev_err(&rcfw->pdev->dev,
			"QPLIB: HW channel CREQ allocation failed\n");
		return -ENOMEM;
	}

	sginfo.pgsize = BNXT_QPLIB_CMDQE_PAGE_SIZE;
	hwq_attr.depth = BNXT_QPLIB_CMDQE_MAX_CNT & 0x7FFFFFFF;
	hwq_attr.stride = BNXT_QPLIB_CMDQE_UNITS;
	hwq_attr.type = HWQ_TYPE_CTX;
	if (bnxt_qplib_alloc_init_hwq(&cmdq->hwq, &hwq_attr)) {
		dev_err(&rcfw->pdev->dev,
			"QPLIB: HW channel CMDQ allocation failed\n");
		goto fail_free_creq_hwq;
	}

	rcfw->crsqe_tbl = kcalloc(cmdq->hwq.max_elements,
			sizeof(*rcfw->crsqe_tbl), GFP_KERNEL);
	if (!rcfw->crsqe_tbl) {
		dev_err(&rcfw->pdev->dev,
			"QPLIB: HW channel CRSQ allocation failed\n");
		goto fail_free_cmdq_hwq;
	}

	rcfw->max_timeout = res->cctx->hwrm_cmd_max_timeout;

	rcfw->sp_perf_stats_enabled = false;
	rcfw->rcfw_lat_slab_msec = vzalloc(sizeof(u32) *
					   RCFW_MAX_LATENCY_MSEC_SLAB_INDEX);
	rcfw->qp_create_stats = vzalloc(sizeof(u64) * RCFW_MAX_STAT_INDEX);
	rcfw->qp_destroy_stats = vzalloc(sizeof(u64) * RCFW_MAX_STAT_INDEX);
	rcfw->mr_create_stats = vzalloc(sizeof(u64) * RCFW_MAX_STAT_INDEX);
	rcfw->mr_destroy_stats = vzalloc(sizeof(u64) * RCFW_MAX_STAT_INDEX);
	rcfw->qp_modify_stats = vzalloc(sizeof(u64) * RCFW_MAX_STAT_INDEX);

	if (rcfw->rcfw_lat_slab_msec &&
	    rcfw->qp_create_stats &&
	    rcfw->qp_destroy_stats &&
	    rcfw->mr_create_stats &&
	    rcfw->mr_destroy_stats &&
	    rcfw->qp_modify_stats)
		rcfw->sp_perf_stats_enabled = true;

	return 0;
fail_free_cmdq_hwq:
	bnxt_qplib_free_hwq(res, &rcfw->cmdq.hwq);
fail_free_creq_hwq:
	bnxt_qplib_free_hwq(res, &rcfw->creq.hwq);
	return -ENOMEM;
}

void bnxt_qplib_rcfw_stop_irq(struct bnxt_qplib_rcfw *rcfw, bool kill)
{
	struct bnxt_qplib_creq_ctx *creq;
	struct bnxt_qplib_res *res;

	creq = &rcfw->creq;
	res = rcfw->res;

	if (!creq->requested)
		return;

	creq->requested = false;
	/* Mask h/w interrupts */
	bnxt_qplib_ring_nq_db(&creq->creq_db.dbinfo, res->cctx, false);
	/* Sync with last running IRQ-handler */
	synchronize_irq(creq->msix_vec);
	free_irq(creq->msix_vec, rcfw);
	kfree(creq->irq_name);
	creq->irq_name = NULL;
	/* rcfw_intr_enabled should not be greater than 1. Debug
	 * print to check if that is the case
	 */
	if (atomic_read(&rcfw->rcfw_intr_enabled) > 1) {
		dev_err(&rcfw->pdev->dev,
			"%s: rcfw->rcfw_intr_enabled = 0x%x\n", __func__,
			atomic_read(&rcfw->rcfw_intr_enabled));
	}
	atomic_set(&rcfw->rcfw_intr_enabled, 0);
	rcfw->num_irq_stopped++;
}

void bnxt_qplib_disable_rcfw_channel(struct bnxt_qplib_rcfw *rcfw)
{
	struct bnxt_qplib_creq_ctx *creq;
	struct bnxt_qplib_cmdq_ctx *cmdq;

	creq = &rcfw->creq;
	cmdq = &rcfw->cmdq;
	/* Make sure the HW channel is stopped! */
	bnxt_qplib_rcfw_stop_irq(rcfw, true);

	creq->creq_db.reg.bar_reg = NULL;
	creq->creq_db.db = NULL;

	if (cmdq->cmdq_mbox.reg.bar_reg) {
		iounmap(cmdq->cmdq_mbox.reg.bar_reg);
		cmdq->cmdq_mbox.reg.bar_reg = NULL;
		cmdq->cmdq_mbox.prod = NULL;
		cmdq->cmdq_mbox.db = NULL;
	}

	creq->aeq_handler = NULL;
	creq->msix_vec = 0;
}

int bnxt_qplib_rcfw_start_irq(struct bnxt_qplib_rcfw *rcfw, int msix_vector,
			      bool need_init)
{
	struct bnxt_qplib_creq_ctx *creq;
	struct bnxt_qplib_res *res;
	int rc;

	creq = &rcfw->creq;
	res = rcfw->res;

	if (creq->requested)
		return -EFAULT;

	creq->msix_vec = msix_vector;

	creq->irq_name = kasprintf(GFP_KERNEL, "bnxt_re-creq@pci:%s\n",
				   pci_name(res->pdev));
	if (!creq->irq_name)
		return -ENOMEM;

	rc = request_irq(creq->msix_vec, bnxt_qplib_creq_irq, 0,
			 creq->irq_name, rcfw);
	if (rc) {
		kfree(creq->irq_name);
		creq->irq_name = NULL;
		return rc;
	}
	creq->requested = true;

	bnxt_qplib_ring_nq_db(&creq->creq_db.dbinfo, res->cctx, true);

	rcfw->num_irq_started++;
	/* Debug print to check rcfw interrupt enable/disable is invoked
	 * out of sequence
	 */
	if (atomic_read(&rcfw->rcfw_intr_enabled) > 0) {
		dev_err(&rcfw->pdev->dev,
			"%s: rcfw->rcfw_intr_enabled = 0x%x\n", __func__,
			atomic_read(&rcfw->rcfw_intr_enabled));
	}
	atomic_inc(&rcfw->rcfw_intr_enabled);
	return 0;
}

static int bnxt_qplib_map_cmdq_mbox(struct bnxt_qplib_rcfw *rcfw)
{
	struct bnxt_qplib_cmdq_mbox *mbox;
	resource_size_t bar_reg;
	struct pci_dev *pdev;

	pdev = rcfw->pdev;
	mbox = &rcfw->cmdq.cmdq_mbox;

	mbox->reg.bar_id = RCFW_COMM_PCI_BAR_REGION;
	mbox->reg.len = RCFW_COMM_SIZE;
	mbox->reg.bar_base = pci_resource_start(pdev, mbox->reg.bar_id);
	if (!mbox->reg.bar_base) {
		dev_err(&pdev->dev,
			"QPLIB: CMDQ BAR region %d resc start is 0!\n",
			mbox->reg.bar_id);
		return -ENOMEM;
	}

	bar_reg = mbox->reg.bar_base + RCFW_COMM_BASE_OFFSET;
	mbox->reg.len = RCFW_COMM_SIZE;
	mbox->reg.bar_reg = ioremap(bar_reg, mbox->reg.len);
	if (!mbox->reg.bar_reg) {
		dev_err(&pdev->dev,
			"QPLIB: CMDQ BAR region %d mapping failed\n",
			mbox->reg.bar_id);
		return -ENOMEM;
	}

	mbox->prod = (void  __iomem *)((char *)mbox->reg.bar_reg +
					RCFW_PF_VF_COMM_PROD_OFFSET);
	mbox->db = (void __iomem *)((char *)mbox->reg.bar_reg +
				     RCFW_COMM_TRIG_OFFSET);
	return 0;
}

static int bnxt_qplib_map_creq_db(struct bnxt_qplib_rcfw *rcfw, u32 reg_offt)
{
	struct bnxt_qplib_creq_db *creq_db;
	struct bnxt_qplib_reg_desc *dbreg;
	struct bnxt_qplib_res *res;

	res = rcfw->res;
	creq_db = &rcfw->creq.creq_db;
	dbreg = &res->dpi_tbl.ucreg;

	creq_db->reg.bar_id = dbreg->bar_id;
	creq_db->reg.bar_base = dbreg->bar_base;
	creq_db->reg.bar_reg = dbreg->bar_reg + reg_offt;
	creq_db->reg.len = _is_chip_gen_p5_p7(res->cctx) ? sizeof(u64) :
							sizeof(u32);

	creq_db->dbinfo.db = creq_db->reg.bar_reg;
	creq_db->dbinfo.hwq = &rcfw->creq.hwq;
	creq_db->dbinfo.xid = rcfw->creq.ring_id;
	creq_db->dbinfo.seed = rcfw->creq.ring_id;
	creq_db->dbinfo.flags = 0;
	spin_lock_init(&creq_db->dbinfo.lock);
	creq_db->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
	creq_db->dbinfo.res = rcfw->res;

	return 0;
}

static void bnxt_qplib_start_rcfw(struct bnxt_qplib_rcfw *rcfw)
{
	struct bnxt_qplib_cmdq_ctx *cmdq;
	struct bnxt_qplib_creq_ctx *creq;
	struct bnxt_qplib_cmdq_mbox *mbox;
	struct cmdq_init init = {0};

	cmdq = &rcfw->cmdq;
	creq = &rcfw->creq;
	mbox = &cmdq->cmdq_mbox;

	init.cmdq_pbl = cpu_to_le64(cmdq->hwq.pbl[PBL_LVL_0].pg_map_arr[0]);
	init.cmdq_size_cmdq_lvl = cpu_to_le16(
			((BNXT_QPLIB_CMDQE_MAX_CNT << CMDQ_INIT_CMDQ_SIZE_SFT) &
			 CMDQ_INIT_CMDQ_SIZE_MASK) |
			((cmdq->hwq.level << CMDQ_INIT_CMDQ_LVL_SFT) &
			 CMDQ_INIT_CMDQ_LVL_MASK));
	init.creq_ring_id = cpu_to_le16(creq->ring_id);
	/* Write to the Bono mailbox register */
	__iowrite32_copy(mbox->reg.bar_reg, &init, sizeof(init) / 4);
}

int bnxt_qplib_enable_rcfw_channel(struct bnxt_qplib_rcfw *rcfw,
				   int msix_vector,
				   int cp_bar_reg_off,
				   aeq_handler_t aeq_handler)
{
	struct bnxt_qplib_cmdq_ctx *cmdq;
	struct bnxt_qplib_creq_ctx *creq;
	int rc;

	cmdq = &rcfw->cmdq;
	creq = &rcfw->creq;

	/* Clear to defaults */
	cmdq->seq_num = 0;
	set_bit(FIRMWARE_FIRST_FLAG, &cmdq->flags);
	init_waitqueue_head(&cmdq->waitq);

	creq->stats.creq_qp_event_processed = 0;
	creq->stats.creq_func_event_processed = 0;
	creq->aeq_handler = aeq_handler;

	rc = bnxt_qplib_map_cmdq_mbox(rcfw);
	if (rc)
		return rc;

	rc = bnxt_qplib_map_creq_db(rcfw, cp_bar_reg_off);
	if (rc)
		return rc;

	rc = bnxt_qplib_rcfw_start_irq(rcfw, msix_vector, true);
	if (rc) {
		dev_err(&rcfw->pdev->dev,
			"QPLIB: Failed to request IRQ for CREQ rc = 0x%x\n", rc);
		bnxt_qplib_disable_rcfw_channel(rcfw);
		return rc;
	}

	rcfw->curr_shadow_qd = min_not_zero(cmdq_shadow_qd,
					    (unsigned int)RCFW_CMD_NON_BLOCKING_SHADOW_QD);
	sema_init(&rcfw->rcfw_inflight, rcfw->curr_shadow_qd);
	dev_dbg(&rcfw->pdev->dev,
		"Perf Debug: shadow qd %d\n", rcfw->curr_shadow_qd);
	bnxt_qplib_start_rcfw(rcfw);

	return 0;
}
