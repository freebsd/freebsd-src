/*-
 * SPDX-License-Identifier: BSD-2-Clause OR GPL-2.0
 *
 * Copyright (c) 2017 Mellanox Technologies Ltd.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/cdefs.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>

#include <rdma/ib_verbs.h>

/* # of WCs to poll for with a single call to ib_poll_cq */
#define IB_POLL_BATCH			16

/* # of WCs to iterate over before yielding */
#define IB_POLL_BUDGET_WORKQUEUE	65536

#define IB_POLL_FLAGS \
	(IB_CQ_NEXT_COMP | IB_CQ_REPORT_MISSED_EVENTS)

static int __ib_process_cq(struct ib_cq *cq, int budget)
{
	int i, n, completed = 0;

	while ((n = ib_poll_cq(cq, IB_POLL_BATCH, cq->wc)) > 0) {
		for (i = 0; i < n; i++) {
			struct ib_wc *wc = &cq->wc[i];

			if (wc->wr_cqe)
				wc->wr_cqe->done(cq, wc);
			else
				WARN_ON_ONCE(wc->status == IB_WC_SUCCESS);
		}

		completed += n;

		if (n != IB_POLL_BATCH ||
		    (budget != -1 && completed >= budget))
			break;
	}

	return completed;
}

/**
 * ib_process_direct_cq - process a CQ in caller context
 * @cq:		CQ to process
 * @budget:	number of CQEs to poll for
 *
 * This function is used to process all outstanding CQ entries on a
 * %IB_POLL_DIRECT CQ.  It does not offload CQ processing to a different
 * context and does not ask for completion interrupts from the HCA.
 *
 * Note: for compatibility reasons -1 can be passed in %budget for unlimited
 * polling.  Do not use this feature in new code, it will be removed soon.
 */
int ib_process_cq_direct(struct ib_cq *cq, int budget)
{
	WARN_ON_ONCE(cq->poll_ctx != IB_POLL_DIRECT);

	return __ib_process_cq(cq, budget);
}
EXPORT_SYMBOL(ib_process_cq_direct);

static void ib_cq_completion_direct(struct ib_cq *cq, void *private)
{
	WARN_ONCE(1, "got unsolicited completion for CQ 0x%p\n", cq);
}

static void ib_cq_poll_work(struct work_struct *work)
{
	struct ib_cq *cq = container_of(work, struct ib_cq, work);
	int completed;

	completed = __ib_process_cq(cq, IB_POLL_BUDGET_WORKQUEUE);
	if (completed >= IB_POLL_BUDGET_WORKQUEUE ||
	    ib_req_notify_cq(cq, IB_POLL_FLAGS) > 0)
		queue_work(ib_comp_wq, &cq->work);
}

static void
ib_cq_completion_workqueue(struct ib_cq *cq, void *private)
{
	queue_work(ib_comp_wq, &cq->work);
}

struct ib_cq *
__ib_alloc_cq_user(struct ib_device *dev, void *private,
		   int nr_cqe, int comp_vector,
		   enum ib_poll_context poll_ctx,
		   const char *caller, struct ib_udata *udata)
{
	struct ib_cq_init_attr cq_attr = {
		.cqe = nr_cqe,
		.comp_vector = comp_vector,
	};
	struct ib_cq *cq;
	int ret = -ENOMEM;

	/*
	 * Check for invalid parameters early on to avoid
	 * extra error handling code:
	 */
	switch (poll_ctx) {
	case IB_POLL_DIRECT:
	case IB_POLL_SOFTIRQ:
	case IB_POLL_WORKQUEUE:
		break;
	default:
		return (ERR_PTR(-EINVAL));
	}

	cq = rdma_zalloc_drv_obj(dev, ib_cq);
	if (!cq)
		return ERR_PTR(-ENOMEM);

	cq->device = dev;
	cq->cq_context = private;
	cq->poll_ctx = poll_ctx;
	atomic_set(&cq->usecnt, 0);

	cq->wc = kmalloc_array(IB_POLL_BATCH, sizeof(*cq->wc), GFP_KERNEL);
	if (!cq->wc)
		goto out_free_cq;

	ret = dev->create_cq(cq, &cq_attr, NULL);
	if (ret)
		goto out_free_wc;

	switch (cq->poll_ctx) {
	case IB_POLL_DIRECT:
		cq->comp_handler = ib_cq_completion_direct;
		break;
	case IB_POLL_SOFTIRQ:
	case IB_POLL_WORKQUEUE:
		cq->comp_handler = ib_cq_completion_workqueue;
		INIT_WORK(&cq->work, ib_cq_poll_work);
		ib_req_notify_cq(cq, IB_CQ_NEXT_COMP);
		break;
	default:
		break;
	}
	return (cq);

out_free_wc:
	kfree(cq->wc);
out_free_cq:
	kfree(cq);
	return (ERR_PTR(ret));
}
EXPORT_SYMBOL(__ib_alloc_cq_user);

void
ib_free_cq_user(struct ib_cq *cq, struct ib_udata *udata)
{

	if (WARN_ON_ONCE(atomic_read(&cq->usecnt) != 0))
		return;

	switch (cq->poll_ctx) {
	case IB_POLL_DIRECT:
		break;
	case IB_POLL_SOFTIRQ:
	case IB_POLL_WORKQUEUE:
		flush_work(&cq->work);
		break;
	default:
		break;
	}

	kfree(cq->wc);
	cq->device->destroy_cq(cq, udata);
	kfree(cq);
}
EXPORT_SYMBOL(ib_free_cq_user);
