/*
 * Copyright (c) 2006 QLogic, Inc.  All rights reserved.
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

#include <linux/string.h>
#include <linux/random.h>
#include <linux/netdevice.h>
#include <linux/list.h>

#include "vnic_util.h"
#include "vnic_data.h"
#include "vnic_config.h"
#include "vnic_ib.h"
#include "vnic_viport.h"
#include "vnic_sys.h"
#include "vnic_main.h"
#include "vnic_stats.h"

static int vnic_ib_inited;
static void vnic_add_one(struct ib_device *device);
static void vnic_remove_one(struct ib_device *device);
static int vnic_defer_completion(void *ptr);

static int vnic_ib_mc_init_qp(struct mc_data *mc_data,
		struct vnic_ib_config *config,
		struct ib_pd *pd,
		struct viport_config *viport_config);

static struct ib_client vnic_client = {
	.name = "vnic",
	.add = vnic_add_one,
	.remove = vnic_remove_one
};

struct ib_sa_client vnic_sa_client;

int vnic_ib_init(void)
{
	int ret = -1;

	IB_FUNCTION("vnic_ib_init()\n");

	/* class has to be registered before
	 * calling ib_register_client() because, that call
	 * will trigger vnic_add_port() which will register
	 * class_device for the port with the parent class
	 * as vnic_class
	 */
	ret = class_register(&vnic_class);
	if (ret) {
		printk(KERN_ERR PFX "couldn't register class"
		       " infiniband_qlgc_vnic; error %d", ret);
		goto out;
	}

	ib_sa_register_client(&vnic_sa_client);
	ret = ib_register_client(&vnic_client);
	if (ret) {
		printk(KERN_ERR PFX "couldn't register IB client;"
		       " error %d", ret);
		goto err_ib_reg;
	}

	interface_dev.dev.class = &vnic_class;
	interface_dev.dev.release = vnic_release_dev;
	dev_set_name(&interface_dev.dev, "interfaces");
	init_completion(&interface_dev.released);
	ret = device_register(&interface_dev.dev);
	if (ret) {
		printk(KERN_ERR PFX "couldn't register class interfaces;"
		       " error %d", ret);
		goto err_class_dev;
	}
	ret = device_create_file(&interface_dev.dev,
				       &dev_attr_delete_vnic);
	if (ret) {
		printk(KERN_ERR PFX "couldn't create class file"
		       " 'delete_vnic'; error %d", ret);
		goto err_class_file;
	}

	ret = device_create_file(&interface_dev.dev, &dev_attr_force_failover);
	if (ret) {
		printk(KERN_ERR PFX "couldn't create class file"
		       " 'force_failover'; error %d", ret);
		goto err_force_failover_file;
	}

	ret = device_create_file(&interface_dev.dev, &dev_attr_unfailover);
	if (ret) {
		printk(KERN_ERR PFX "couldn't create class file"
		       " 'unfailover'; error %d", ret);
		goto err_unfailover_file;
	}
	vnic_ib_inited = 1;

	return ret;
err_unfailover_file:
	device_remove_file(&interface_dev.dev, &dev_attr_force_failover);
err_force_failover_file:
	device_remove_file(&interface_dev.dev, &dev_attr_delete_vnic);
err_class_file:
	device_unregister(&interface_dev.dev);
err_class_dev:
	ib_unregister_client(&vnic_client);
err_ib_reg:
	ib_sa_unregister_client(&vnic_sa_client);
	class_unregister(&vnic_class);
out:
	return ret;
}

static struct vnic_ib_port *vnic_add_port(struct vnic_ib_device *device,
					  u8 port_num)
{
	struct vnic_ib_port *port;

	port = kzalloc(sizeof *port, GFP_KERNEL);
	if (!port)
		return NULL;

	init_completion(&port->pdev_info.released);
	port->dev = device;
	port->port_num = port_num;

	port->pdev_info.dev.class = &vnic_class;
	port->pdev_info.dev.parent = NULL;
	port->pdev_info.dev.release = vnic_release_dev;
	dev_set_name(&port->pdev_info.dev, "vnic-%s-%d",
		device->dev->name, port_num);

	if (device_register(&port->pdev_info.dev))
		goto free_port;

	if (device_create_file(&port->pdev_info.dev,
				     &dev_attr_create_primary))
		goto err_class;
	if (device_create_file(&port->pdev_info.dev,
				     &dev_attr_create_secondary))
		goto err_class;

	return port;
err_class:
	device_unregister(&port->pdev_info.dev);
free_port:
	kfree(port);

	return NULL;
}

static void vnic_add_one(struct ib_device *device)
{
	struct vnic_ib_device *vnic_dev;
	struct vnic_ib_port *port;
	int s, e, p;

	vnic_dev = kmalloc(sizeof *vnic_dev, GFP_KERNEL);
	if (!vnic_dev)
		return;

	vnic_dev->dev = device;
	INIT_LIST_HEAD(&vnic_dev->port_list);

	if (device->node_type == RDMA_NODE_IB_SWITCH) {
		s = 0;
		e = 0;

	} else {
		s = 1;
		e = device->phys_port_cnt;

	}

	for (p = s; p <= e; p++) {
		port = vnic_add_port(vnic_dev, p);
		if (port)
			list_add_tail(&port->list, &vnic_dev->port_list);
	}

	ib_set_client_data(device, &vnic_client, vnic_dev);

}

static void vnic_remove_one(struct ib_device *device)
{
	struct vnic_ib_device *vnic_dev;
	struct vnic_ib_port *port, *tmp_port;

	vnic_dev = ib_get_client_data(device, &vnic_client);
	list_for_each_entry_safe(port, tmp_port,
				 &vnic_dev->port_list, list) {

		device_remove_file(&port->pdev_info.dev, &dev_attr_create_primary);
		device_remove_file(&port->pdev_info.dev, &dev_attr_create_secondary);
		device_unregister(&port->pdev_info.dev);
		/*
		 * wait for sysfs entries to go away, so that no new vnics
		 * are created
		 */
		wait_for_completion(&port->pdev_info.released);
		kfree(port);

	}
	kfree(vnic_dev);

	/* TODO Only those vnic interfaces associated with
	 * the HCA whose remove event is called should be freed
	 * Currently all the vnic interfaces are freed
	 */

	while (!list_empty(&vnic_list)) {
		struct vnic *vnic =
		    list_entry(vnic_list.next, struct vnic, list_ptrs);
		vnic_free(vnic);
	}

	vnic_npevent_cleanup();
	viport_cleanup();

}

void vnic_ib_cleanup(void)
{
	IB_FUNCTION("vnic_ib_cleanup()\n");

	if (!vnic_ib_inited)
		return;

	device_remove_file(&interface_dev.dev, &dev_attr_unfailover);
	device_remove_file(&interface_dev.dev, &dev_attr_force_failover);
	device_remove_file(&interface_dev.dev, &dev_attr_delete_vnic);

	device_unregister(&interface_dev.dev);
	wait_for_completion(&interface_dev.released);

	ib_unregister_client(&vnic_client);
	ib_sa_unregister_client(&vnic_sa_client);
	class_unregister(&vnic_class);
}

static void vnic_path_rec_completion(int status,
				     struct ib_sa_path_rec *pathrec,
				     void *context)
{
	struct vnic_ib_path_info *p = context;
	p->status = status;
	IB_INFO("Service level for VNIC is %d\n", pathrec->sl);
	if (!status)
		p->path = *pathrec;

	complete(&p->done);
}

int vnic_ib_get_path(struct netpath *netpath, struct vnic *vnic)
{
	struct viport_config *config = netpath->viport->config;
	int ret = 0;

	init_completion(&config->path_info.done);
	IB_INFO("Using SA path rec get time out value of %d\n",
	       config->sa_path_rec_get_timeout);
	config->path_info.path_query_id =
			 ib_sa_path_rec_get(&vnic_sa_client,
					    config->ibdev,
					    config->port,
					    &config->path_info.path,
					    IB_SA_PATH_REC_SERVICE_ID	|
					    IB_SA_PATH_REC_DGID     	|
					    IB_SA_PATH_REC_SGID      	|
					    IB_SA_PATH_REC_NUMB_PATH 	|
					    IB_SA_PATH_REC_PKEY,
					    config->sa_path_rec_get_timeout,
					    GFP_KERNEL,
					    vnic_path_rec_completion,
					    &config->path_info,
					    &config->path_info.path_query);

	if (config->path_info.path_query_id < 0) {
		IB_ERROR("SA path record query failed; error %d\n",
			 config->path_info.path_query_id);
		ret = config->path_info.path_query_id;
		goto out;
	}

	wait_for_completion(&config->path_info.done);

	if (config->path_info.status < 0) {
		printk(KERN_WARNING PFX "connection not available to dgid "
		       "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
		       (int)be16_to_cpu(*(__be16 *) &config->path_info.path.
					dgid.raw[0]),
		       (int)be16_to_cpu(*(__be16 *) &config->path_info.path.
					dgid.raw[2]),
		       (int)be16_to_cpu(*(__be16 *) &config->path_info.path.
					dgid.raw[4]),
		       (int)be16_to_cpu(*(__be16 *) &config->path_info.path.
					dgid.raw[6]),
		       (int)be16_to_cpu(*(__be16 *) &config->path_info.path.
					dgid.raw[8]),
		       (int)be16_to_cpu(*(__be16 *) &config->path_info.path.
					dgid.raw[10]),
		       (int)be16_to_cpu(*(__be16 *) &config->path_info.path.
					dgid.raw[12]),
		       (int)be16_to_cpu(*(__be16 *) &config->path_info.path.
					dgid.raw[14]));

		if (config->path_info.status == -ETIMEDOUT)
			printk(KERN_INFO " path query timed out\n");
		else if (config->path_info.status == -EIO)
			printk(KERN_INFO " path query sending error\n");
		else
			printk(KERN_INFO " error %d\n",
			       config->path_info.status);

		ret = config->path_info.status;
	}
out:
	if (ret)
		netpath_timer(netpath, vnic->config->no_path_timeout);

	return ret;
}

static inline void vnic_ib_handle_completions(struct ib_wc *wc,
					      struct vnic_ib_conn *ib_conn,
					      u32 *comp_num,
					      cycles_t *comp_time)
{
	struct io *io;

	io = (struct io *)(wc->wr_id);
	vnic_ib_comp_stats(ib_conn, comp_num);
	if (wc->status) {
		IB_INFO("completion error  wc.status %d"
			 " wc.opcode %d vendor err 0x%x\n",
			  wc->status, wc->opcode, wc->vendor_err);
	} else if (io) {
		vnic_ib_io_stats(io, ib_conn, *comp_time);
		if (io->type == RECV_UD) {
			struct ud_recv_io *recv_io =
				container_of(io, struct ud_recv_io, io);
			recv_io->len = wc->byte_len;
		}
		if (io->routine)
			(*io->routine) (io);
	}
}

static void ib_qp_event(struct ib_event *event, void *context)
{
	IB_ERROR("QP event %d\n", event->event);
}

static void vnic_ib_completion(struct ib_cq *cq, void *ptr)
{
	struct vnic_ib_conn *ib_conn = ptr;
	unsigned long	 flags;
	int compl_received;
	struct ib_wc wc;
	cycles_t  comp_time;
	u32  comp_num = 0;

	/* for multicast, cm_id is NULL, so skip that test */
	if (ib_conn->cm_id &&
	    (ib_conn->state != IB_CONN_CONNECTED))
		return;

	/* Check if completion processing is taking place in thread
	 * If not then process completions in this handler,
	 * else set compl_received if not set, to indicate that
	 * there are more completions to process in thread.
	 */

	spin_lock_irqsave(&ib_conn->compl_received_lock, flags);
	compl_received = ib_conn->compl_received;
	spin_unlock_irqrestore(&ib_conn->compl_received_lock, flags);

	if (ib_conn->in_thread || compl_received) {
		if (!compl_received) {
			spin_lock_irqsave(&ib_conn->compl_received_lock, flags);
			ib_conn->compl_received = 1;
			spin_unlock_irqrestore(&ib_conn->compl_received_lock,
									flags);
		}
		wake_up(&(ib_conn->callback_wait_queue));
	} else {
		vnic_ib_note_comptime_stats(&comp_time);
		vnic_ib_callback_stats(ib_conn);
		ib_req_notify_cq(cq, IB_CQ_NEXT_COMP);
		while (ib_poll_cq(cq, 1, &wc) > 0) {
			vnic_ib_handle_completions(&wc, ib_conn, &comp_num,
								 &comp_time);
			if (ib_conn->cm_id &&
				 ib_conn->state != IB_CONN_CONNECTED)
				break;

			/* If we get more completions than the completion limit
			 * defer completion to the thread
			 */
			if ((!ib_conn->in_thread) &&
			    (comp_num >= ib_conn->ib_config->completion_limit)) {
				ib_conn->in_thread = 1;
				spin_lock_irqsave(
					&ib_conn->compl_received_lock, flags);
				ib_conn->compl_received = 1;
				spin_unlock_irqrestore(
					&ib_conn->compl_received_lock, flags);
				wake_up(&(ib_conn->callback_wait_queue));
				break;
			}

		}
		vnic_ib_maxio_stats(ib_conn, comp_num);
	}
}

static int vnic_ib_mod_qp_to_rts(struct ib_cm_id *cm_id,
			     struct vnic_ib_conn *ib_conn)
{
	int attr_mask = 0;
	int ret;
	struct ib_qp_attr *qp_attr = NULL;

	qp_attr = kmalloc(sizeof *qp_attr, GFP_KERNEL);
	if (!qp_attr)
		return -ENOMEM;

	qp_attr->qp_state = IB_QPS_RTR;

	ret = ib_cm_init_qp_attr(cm_id, qp_attr, &attr_mask);
	if (ret)
		goto out;

	ret = ib_modify_qp(ib_conn->qp, qp_attr, attr_mask);
	if (ret)
		goto out;

	IB_INFO("QP RTR\n");

	qp_attr->qp_state = IB_QPS_RTS;

	ret = ib_cm_init_qp_attr(cm_id, qp_attr, &attr_mask);
	if (ret)
		goto out;

	ret = ib_modify_qp(ib_conn->qp, qp_attr, attr_mask);
	if (ret)
		goto out;

	IB_INFO("QP RTS\n");

	ret = ib_send_cm_rtu(cm_id, NULL, 0);
	if (ret)
		goto out;
out:
	kfree(qp_attr);
	return ret;
}

int vnic_ib_cm_handler(struct ib_cm_id *cm_id, struct ib_cm_event *event)
{
	struct vnic_ib_conn *ib_conn = cm_id->context;
	struct viport *viport = ib_conn->viport;
	int err = 0;

	switch (event->event) {
	case IB_CM_REQ_ERROR:
		IB_ERROR("sending CM REQ failed\n");
		err = 1;
		viport->retry = 1;
		break;
	case IB_CM_REP_RECEIVED:
		IB_INFO("CM REP recvd\n");
		if (vnic_ib_mod_qp_to_rts(cm_id, ib_conn))
			err = 1;
		else {
			ib_conn->state = IB_CONN_CONNECTED;
			vnic_ib_connected_time_stats(ib_conn);
			IB_INFO("RTU SENT\n");
		}
		break;
	case IB_CM_REJ_RECEIVED:
		printk(KERN_ERR PFX " CM rejected control connection\n");
		if (event->param.rej_rcvd.reason ==
		    IB_CM_REJ_INVALID_SERVICE_ID)
			printk(KERN_ERR "reason: invalid service ID. "
			       "IOCGUID value specified may be incorrect\n");
		else
			printk(KERN_ERR "reason code : 0x%x\n",
			       event->param.rej_rcvd.reason);

		err = 1;
		viport->retry = 1;
		break;
	case IB_CM_MRA_RECEIVED:
		IB_INFO("CM MRA received\n");
		break;

	case IB_CM_DREP_RECEIVED:
		IB_INFO("CM DREP recvd\n");
		ib_conn->state = IB_CONN_DISCONNECTED;
		break;

	case IB_CM_TIMEWAIT_EXIT:
		IB_ERROR("CM timewait exit\n");
		err = 1;
		break;

	default:
		IB_INFO("unhandled CM event %d\n", event->event);
		break;

	}

	if (err) {
		ib_conn->state = IB_CONN_DISCONNECTED;
		viport_failure(viport);
	}

	viport_kick(viport);
	return 0;
}


int vnic_ib_cm_connect(struct vnic_ib_conn *ib_conn)
{
	struct ib_cm_req_param	*req = NULL;
	struct viport		*viport;
	int 			ret = -1;

	if (!vnic_ib_conn_initted(ib_conn)) {
		IB_ERROR("IB Connection out of state for CM connect (%d)\n",
			 ib_conn->state);
		return -EINVAL;
	}

	vnic_ib_conntime_stats(ib_conn);
	req = kzalloc(sizeof *req, GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	viport	= ib_conn->viport;

	req->primary_path	= &viport->config->path_info.path;
	req->alternate_path	= NULL;
	req->qp_num		= ib_conn->qp->qp_num;
	req->qp_type		= ib_conn->qp->qp_type;
	req->service_id 	= ib_conn->ib_config->service_id;
	req->private_data	= &ib_conn->ib_config->conn_data;
	req->private_data_len	= sizeof(struct vnic_connection_data);
	req->flow_control	= 1;

	get_random_bytes(&req->starting_psn, 4);
	req->starting_psn &= 0xffffff;

	/*
	 * Both responder_resources and initiator_depth are set to zero
	 * as we do not need RDMA read.
	 *
	 * They also must be set to zero, otherwise data connections
	 * are rejected by VEx.
	 */
	req->responder_resources 	= 0;
	req->initiator_depth		= 0;
	req->remote_cm_response_timeout = 20;
	req->local_cm_response_timeout  = 20;
	req->retry_count		= ib_conn->ib_config->retry_count;
	req->rnr_retry_count		= ib_conn->ib_config->rnr_retry_count;
	req->max_cm_retries		= 15;

	ib_conn->state = IB_CONN_CONNECTING;

	ret = ib_send_cm_req(ib_conn->cm_id, req);

	kfree(req);

	if (ret) {
		IB_ERROR("CM REQ sending failed; error %d \n", ret);
		ib_conn->state = IB_CONN_DISCONNECTED;
	}

	return ret;
}

static int vnic_ib_init_qp(struct vnic_ib_conn *ib_conn,
			   struct vnic_ib_config *config,
			   struct ib_pd	*pd,
			   struct viport_config *viport_config)
{
	struct ib_qp_init_attr	*init_attr;
	struct ib_qp_attr	*attr;
	int			ret;

	init_attr = kzalloc(sizeof *init_attr, GFP_KERNEL);
	if (!init_attr)
		return -ENOMEM;

	init_attr->event_handler	= ib_qp_event;
	init_attr->cap.max_send_wr	= config->num_sends;
	init_attr->cap.max_recv_wr	= config->num_recvs;
	init_attr->cap.max_recv_sge	= config->recv_scatter;
	init_attr->cap.max_send_sge	= config->send_gather;
	init_attr->sq_sig_type		= IB_SIGNAL_ALL_WR;
	init_attr->qp_type		= IB_QPT_RC;
	init_attr->send_cq		= ib_conn->cq;
	init_attr->recv_cq		= ib_conn->cq;

	ib_conn->qp = ib_create_qp(pd, init_attr);

	if (IS_ERR(ib_conn->qp)) {
		ret = -1;
		IB_ERROR("could not create QP\n");
		goto free_init_attr;
	}

	attr = kmalloc(sizeof *attr, GFP_KERNEL);
	if (!attr) {
		ret = -ENOMEM;
		goto destroy_qp;
	}

	ret = ib_find_pkey(viport_config->ibdev, viport_config->port,
			  be16_to_cpu(viport_config->path_info.path.pkey),
			  &attr->pkey_index);
	if (ret) {
		printk(KERN_WARNING PFX "ib_find_pkey() failed; "
		       "error %d\n", ret);
		goto freeattr;
	}

	attr->qp_state		= IB_QPS_INIT;
	attr->qp_access_flags	= IB_ACCESS_REMOTE_WRITE;
	attr->port_num		= viport_config->port;

	ret = ib_modify_qp(ib_conn->qp, attr,
			   IB_QP_STATE |
			   IB_QP_PKEY_INDEX |
			   IB_QP_ACCESS_FLAGS | IB_QP_PORT);
	if (ret) {
		printk(KERN_WARNING PFX "could not modify QP; error %d \n",
		       ret);
		goto freeattr;
	}

	kfree(attr);
	kfree(init_attr);
	return ret;

freeattr:
	kfree(attr);
destroy_qp:
	ib_destroy_qp(ib_conn->qp);
free_init_attr:
	kfree(init_attr);
	return ret;
}

int vnic_ib_conn_init(struct vnic_ib_conn *ib_conn, struct viport *viport,
		      struct ib_pd *pd, struct vnic_ib_config *config)
{
	struct viport_config	*viport_config = viport->config;
	int		ret = -1;
	unsigned int	cq_size = config->num_sends + config->num_recvs;


	if (!vnic_ib_conn_uninitted(ib_conn)) {
		IB_ERROR("IB Connection out of state for init (%d)\n",
			 ib_conn->state);
		return -EINVAL;
	}

	ib_conn->cq = ib_create_cq(viport_config->ibdev, vnic_ib_completion,
#ifdef BUILD_FOR_OFED_1_2
				   NULL, ib_conn, cq_size);
#else
				   NULL, ib_conn, cq_size, 0);
#endif
	if (IS_ERR(ib_conn->cq)) {
		IB_ERROR("could not create CQ\n");
		goto out;
	}

	IB_INFO("cq created %p %d\n", ib_conn->cq, cq_size);
	ib_req_notify_cq(ib_conn->cq, IB_CQ_NEXT_COMP);
	init_waitqueue_head(&(ib_conn->callback_wait_queue));
	init_completion(&(ib_conn->callback_thread_exit));

	spin_lock_init(&ib_conn->compl_received_lock);

	ib_conn->callback_thread = kthread_run(vnic_defer_completion, ib_conn,
						"qlgc_vnic_def_compl");
	if (IS_ERR(ib_conn->callback_thread)) {
		IB_ERROR("Could not create vnic_callback_thread;"
			" error %d\n", (int) PTR_ERR(ib_conn->callback_thread));
		ib_conn->callback_thread = NULL;
		goto destroy_cq;
	}

	ret = vnic_ib_init_qp(ib_conn, config, pd, viport_config);

	if (ret)
		goto destroy_thread;

	spin_lock_init(&ib_conn->conn_lock);
	ib_conn->state = IB_CONN_INITTED;

	return ret;

destroy_thread:
	vnic_completion_cleanup(ib_conn);
destroy_cq:
	ib_destroy_cq(ib_conn->cq);
out:
	return ret;
}

int vnic_ib_post_recv(struct vnic_ib_conn *ib_conn, struct io *io)
{
	cycles_t		post_time;
	struct ib_recv_wr	*bad_wr;
	int			ret = -1;
	unsigned long		flags;

	IB_FUNCTION("vnic_ib_post_recv()\n");

	spin_lock_irqsave(&ib_conn->conn_lock, flags);

	if (!vnic_ib_conn_initted(ib_conn) &&
	    !vnic_ib_conn_connected(ib_conn)) {
		ret = -EINVAL;
		goto out;
	}

	vnic_ib_pre_rcvpost_stats(ib_conn, io, &post_time);
	io->type = RECV;
	ret = ib_post_recv(ib_conn->qp, &io->rwr, &bad_wr);
	if (ret) {
		IB_ERROR("error in posting rcv wr; error %d\n", ret);
		ib_conn->state = IB_CONN_ERRORED;
		goto out;
	}

	vnic_ib_post_rcvpost_stats(ib_conn, post_time);
out:
	spin_unlock_irqrestore(&ib_conn->conn_lock, flags);
	return ret;

}

int vnic_ib_post_send(struct vnic_ib_conn *ib_conn, struct io *io)
{
	cycles_t		post_time;
	unsigned long		flags;
	struct ib_send_wr	*bad_wr;
	int			ret = -1;

	IB_FUNCTION("vnic_ib_post_send()\n");

	spin_lock_irqsave(&ib_conn->conn_lock, flags);
	if (!vnic_ib_conn_connected(ib_conn)) {
		IB_ERROR("IB Connection out of state for"
			 " posting sends (%d)\n", ib_conn->state);
		goto out;
	}

	vnic_ib_pre_sendpost_stats(io, &post_time);
	if (io->swr.opcode == IB_WR_RDMA_WRITE)
		io->type = RDMA;
	else
		io->type = SEND;

	ret = ib_post_send(ib_conn->qp, &io->swr, &bad_wr);
	if (ret) {
		IB_ERROR("error in posting send wr; error %d\n", ret);
		ib_conn->state = IB_CONN_ERRORED;
		goto out;
	}

	vnic_ib_post_sendpost_stats(ib_conn, io, post_time);
out:
	spin_unlock_irqrestore(&ib_conn->conn_lock, flags);
	return ret;
}

static int vnic_defer_completion(void *ptr)
{
	struct vnic_ib_conn *ib_conn = ptr;
	struct ib_wc wc;
	struct ib_cq *cq = ib_conn->cq;
	cycles_t 	 comp_time;
	u32              comp_num = 0;
	unsigned long	flags;

	while (!ib_conn->callback_thread_end) {
		wait_event_interruptible(ib_conn->callback_wait_queue,
					 ib_conn->compl_received ||
					 ib_conn->callback_thread_end);
		ib_conn->in_thread = 1;
		spin_lock_irqsave(&ib_conn->compl_received_lock, flags);
		ib_conn->compl_received = 0;
		spin_unlock_irqrestore(&ib_conn->compl_received_lock, flags);
		if (ib_conn->cm_id &&
		    ib_conn->state != IB_CONN_CONNECTED)
			goto out_thread;

		vnic_ib_note_comptime_stats(&comp_time);
		vnic_ib_callback_stats(ib_conn);
		ib_req_notify_cq(cq, IB_CQ_NEXT_COMP);
		while (ib_poll_cq(cq, 1, &wc) > 0) {
			vnic_ib_handle_completions(&wc, ib_conn, &comp_num,
								 &comp_time);
			if (ib_conn->cm_id &&
				 ib_conn->state != IB_CONN_CONNECTED)
				break;
		}
		vnic_ib_maxio_stats(ib_conn, comp_num);
out_thread:
		ib_conn->in_thread = 0;
	}
	complete_and_exit(&(ib_conn->callback_thread_exit), 0);
	return 0;
}

void vnic_completion_cleanup(struct vnic_ib_conn *ib_conn)
{
	if (ib_conn->callback_thread) {
		ib_conn->callback_thread_end = 1;
		wake_up(&(ib_conn->callback_wait_queue));
		wait_for_completion(&(ib_conn->callback_thread_exit));
		ib_conn->callback_thread = NULL;
	}
}

int vnic_ib_mc_init(struct mc_data *mc_data, struct viport *viport,
		      struct ib_pd *pd, struct vnic_ib_config *config)
{
	struct viport_config	*viport_config = viport->config;
	int		ret = -1;
	unsigned int	cq_size = config->num_recvs; /* recvs only */

	IB_FUNCTION("vnic_ib_mc_init\n");

	mc_data->ib_conn.cq = ib_create_cq(viport_config->ibdev, vnic_ib_completion,
#ifdef BUILD_FOR_OFED_1_2
				   NULL, &mc_data->ib_conn, cq_size);
#else
				   NULL, &mc_data->ib_conn, cq_size, 0);
#endif
	if (IS_ERR(mc_data->ib_conn.cq)) {
		IB_ERROR("ib_create_cq failed\n");
		goto out;
	}
	IB_INFO("mc cq created %p %d\n", mc_data->ib_conn.cq, cq_size);

	ret = ib_req_notify_cq(mc_data->ib_conn.cq, IB_CQ_NEXT_COMP);
	if (ret) {
		IB_ERROR("ib_req_notify_cq failed %x \n", ret);
		goto destroy_cq;
	}

	init_waitqueue_head(&(mc_data->ib_conn.callback_wait_queue));
	init_completion(&(mc_data->ib_conn.callback_thread_exit));

	spin_lock_init(&mc_data->ib_conn.compl_received_lock);
	mc_data->ib_conn.callback_thread = kthread_run(vnic_defer_completion,
							&mc_data->ib_conn,
							"qlgc_vnic_mc_def_compl");
	if (IS_ERR(mc_data->ib_conn.callback_thread)) {
		IB_ERROR("Could not create vnic_callback_thread for MULTICAST;"
			" error %d\n",
			(int) PTR_ERR(mc_data->ib_conn.callback_thread));
		mc_data->ib_conn.callback_thread = NULL;
		goto destroy_cq;
	}
	IB_INFO("callback_thread created\n");

	ret = vnic_ib_mc_init_qp(mc_data, config, pd, viport_config);
	if (ret)
		goto destroy_thread;

	spin_lock_init(&mc_data->ib_conn.conn_lock);
	mc_data->ib_conn.state = IB_CONN_INITTED; /* stays in this state */

	return ret;

destroy_thread:
	vnic_completion_cleanup(&mc_data->ib_conn);
destroy_cq:
	ib_destroy_cq(mc_data->ib_conn.cq);
	mc_data->ib_conn.cq = (struct ib_cq *)ERR_PTR(-EINVAL);
out:
	return ret;
}

static int vnic_ib_mc_init_qp(struct mc_data *mc_data,
			   struct vnic_ib_config *config,
			   struct ib_pd	*pd,
			   struct viport_config *viport_config)
{
	struct ib_qp_init_attr	*init_attr;
	struct ib_qp_attr	*qp_attr;
	int			ret;

	IB_FUNCTION("vnic_ib_mc_init_qp\n");

	if (!mc_data->ib_conn.cq) {
		IB_ERROR("cq is null\n");
		return -ENOMEM;
	}

	init_attr = kzalloc(sizeof *init_attr, GFP_KERNEL);
	if (!init_attr) {
		IB_ERROR("failed to alloc init_attr\n");
		return -ENOMEM;
	}

	init_attr->cap.max_recv_wr	= config->num_recvs;
	init_attr->cap.max_send_wr	= 1;
	init_attr->cap.max_recv_sge	= 2;
	init_attr->cap.max_send_sge	= 1;

	/* Completion for all work requests. */
	init_attr->sq_sig_type		= IB_SIGNAL_ALL_WR;

	init_attr->qp_type		= IB_QPT_UD;

	init_attr->send_cq		= mc_data->ib_conn.cq;
	init_attr->recv_cq		= mc_data->ib_conn.cq;

	IB_INFO("creating qp %d \n", config->num_recvs);

	mc_data->ib_conn.qp = ib_create_qp(pd, init_attr);

	if (IS_ERR(mc_data->ib_conn.qp)) {
		ret = -1;
		IB_ERROR("could not create QP\n");
		goto free_init_attr;
	}

	qp_attr = kzalloc(sizeof *qp_attr, GFP_KERNEL);
	if (!qp_attr) {
		ret = -ENOMEM;
		goto destroy_qp;
	}

	qp_attr->qp_state	= IB_QPS_INIT;
	qp_attr->port_num	= viport_config->port;
	qp_attr->qkey 		= IOC_NUMBER(be64_to_cpu(viport_config->ioc_guid));
	qp_attr->pkey_index	= 0;
	/* cannot set access flags for UD qp
	qp_attr->qp_access_flags	= IB_ACCESS_REMOTE_WRITE; */

	IB_INFO("port_num:%d qkey:%d pkey:%d\n", qp_attr->port_num,
			qp_attr->qkey, qp_attr->pkey_index);
	ret = ib_modify_qp(mc_data->ib_conn.qp, qp_attr,
			   IB_QP_STATE |
			   IB_QP_PKEY_INDEX |
			   IB_QP_QKEY |

			/* cannot set this for UD
			   IB_QP_ACCESS_FLAGS | */

			   IB_QP_PORT);
	if (ret) {
		IB_ERROR("ib_modify_qp to INIT failed %d \n", ret);
		goto free_qp_attr;
	}

	kfree(qp_attr);
	kfree(init_attr);
	return ret;

free_qp_attr:
	kfree(qp_attr);
destroy_qp:
	ib_destroy_qp(mc_data->ib_conn.qp);
	mc_data->ib_conn.qp = ERR_PTR(-EINVAL);
free_init_attr:
	kfree(init_attr);
	return ret;
}

int vnic_ib_mc_mod_qp_to_rts(struct ib_qp *qp)
{
	int ret;
	struct ib_qp_attr *qp_attr = NULL;

	IB_FUNCTION("vnic_ib_mc_mod_qp_to_rts\n");
	qp_attr = kmalloc(sizeof *qp_attr, GFP_KERNEL);
	if (!qp_attr)
		return -ENOMEM;

	memset(qp_attr, 0, sizeof *qp_attr);
	qp_attr->qp_state = IB_QPS_RTR;

	ret = ib_modify_qp(qp, qp_attr, IB_QP_STATE);
	if (ret) {
		IB_ERROR("ib_modify_qp to RTR failed %d\n", ret);
		goto out;
	}
	IB_INFO("MC QP RTR\n");

	memset(qp_attr, 0, sizeof *qp_attr);
	qp_attr->qp_state = IB_QPS_RTS;
	qp_attr->sq_psn = 0;

	ret = ib_modify_qp(qp, qp_attr, IB_QP_STATE | IB_QP_SQ_PSN);
	if (ret) {
		IB_ERROR("ib_modify_qp to RTS failed %d\n", ret);
		goto out;
	}
	IB_INFO("MC QP RTS\n");

	kfree(qp_attr);
	return 0;

out:
	kfree(qp_attr);
	return -1;
}

int vnic_ib_mc_post_recv(struct mc_data *mc_data, struct io *io)
{
	cycles_t		post_time;
	struct ib_recv_wr	*bad_wr;
	int			ret = -1;

	IB_FUNCTION("vnic_ib_mc_post_recv()\n");

	vnic_ib_pre_rcvpost_stats(&mc_data->ib_conn, io, &post_time);
	io->type = RECV_UD;
	ret = ib_post_recv(mc_data->ib_conn.qp, &io->rwr, &bad_wr);
	if (ret) {
		IB_ERROR("error in posting rcv wr; error %d\n", ret);
		goto out;
	}
	vnic_ib_post_rcvpost_stats(&mc_data->ib_conn, post_time);

out:
	return ret;
}
