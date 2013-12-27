/*
 * Copyright (c) 2005-2006 Intel Corporation.  All rights reserved.
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
 *
 * $Id: cm.c 3453 2005-09-15 21:43:21Z sean.hefty $
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>
#include <infiniband/endian.h>
#include <infiniband/byteswap.h>
#include <stddef.h>

#include <infiniband/driver.h>
#include <infiniband/marshall.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_cma_abi.h>

#ifdef INCLUDE_VALGRIND
#   include <valgrind/memcheck.h>
#   ifndef VALGRIND_MAKE_MEM_DEFINED
#       warning "Valgrind requested, but VALGRIND_MAKE_MEM_DEFINED undefined"
#   endif
#endif

#ifndef VALGRIND_MAKE_MEM_DEFINED
#   define VALGRIND_MAKE_MEM_DEFINED(addr,len)
#endif

#define PFX "librdmacm: "

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#else
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#endif

static inline int ERR(int err)
{
	errno = err;
	return -1;
}

#define CMA_CREATE_MSG_CMD_RESP(msg, cmd, resp, type, size) \
do {                                        \
	struct ucma_abi_cmd_hdr *hdr;         \
                                            \
	size = sizeof(*hdr) + sizeof(*cmd); \
	msg = alloca(size);                 \
	if (!msg)                           \
		return ERR(ENOMEM);         \
	hdr = msg;                          \
	cmd = msg + sizeof(*hdr);           \
	hdr->cmd = type;                    \
	hdr->in  = sizeof(*cmd);            \
	hdr->out = sizeof(*resp);           \
	memset(cmd, 0, sizeof(*cmd));       \
	resp = alloca(sizeof(*resp));       \
	if (!resp)                          \
		return ERR(ENOMEM);         \
	cmd->response = (uintptr_t)resp;\
} while (0)

#define CMA_CREATE_MSG_CMD(msg, cmd, type, size) \
do {                                        \
	struct ucma_abi_cmd_hdr *hdr;       \
                                            \
	size = sizeof(*hdr) + sizeof(*cmd); \
	msg = alloca(size);                 \
	if (!msg)                           \
		return ERR(ENOMEM);         \
	hdr = msg;                          \
	cmd = msg + sizeof(*hdr);           \
	hdr->cmd = type;                    \
	hdr->in  = sizeof(*cmd);            \
	hdr->out = 0;                       \
	memset(cmd, 0, sizeof(*cmd));       \
} while (0)

struct cma_device {
	struct ibv_context *verbs;
	uint64_t	    guid;
	int		    port_cnt;
	uint8_t		    max_initiator_depth;
	uint8_t		    max_responder_resources;
};

struct cma_id_private {
	struct rdma_cm_id id;
	struct cma_device *cma_dev;
	int		  events_completed;
	int		  connect_error;
	pthread_cond_t	  cond;
	pthread_mutex_t	  mut;
	uint32_t	  handle;
	struct cma_multicast *mc_list;
};

struct cma_multicast {
	struct cma_multicast  *next;
	struct cma_id_private *id_priv;
	void		*context;
	int		events_completed;
	pthread_cond_t	cond;
	uint32_t	handle;
	union ibv_gid	mgid;
	uint16_t	mlid;
	struct sockaddr_storage addr;
};

struct cma_event {
	struct rdma_cm_event	event;
	uint8_t			private_data[RDMA_MAX_PRIVATE_DATA];
	struct cma_id_private	*id_priv;
	struct cma_multicast	*mc;
};

static struct cma_device *cma_dev_array;
static int cma_dev_cnt;
static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
static int abi_ver = RDMA_USER_CM_MAX_ABI_VERSION;

#define container_of(ptr, type, field) \
	((type *) ((void *)ptr - offsetof(type, field)))

static void ucma_cleanup(void)
{
	if (cma_dev_cnt) {
		while (cma_dev_cnt)
			ibv_close_device(cma_dev_array[--cma_dev_cnt].verbs);
	
		free(cma_dev_array);
		cma_dev_cnt = 0;
	}
}

static int check_abi_version(void)
{
	char value[8];

	if ((ibv_read_sysfs_file(ibv_get_sysfs_path(),
				 "class/misc/rdma_cm/abi_version",
				 value, sizeof value) < 0) &&
	    (ibv_read_sysfs_file(ibv_get_sysfs_path(),
				 "class/infiniband_ucma/abi_version",
				 value, sizeof value) < 0)) {
		/*
		 * Older version of Linux do not have class/misc.  To support
		 * backports, assume the most recent version of the ABI.  If
		 * we're wrong, we'll simply fail later when calling the ABI.
		 */
		fprintf(stderr, "librdmacm: couldn't read ABI version.\n");
		fprintf(stderr, "librdmacm: assuming: %d\n", abi_ver);
		return 0;
	}

	abi_ver = strtol(value, NULL, 10);
	if (abi_ver < RDMA_USER_CM_MIN_ABI_VERSION ||
	    abi_ver > RDMA_USER_CM_MAX_ABI_VERSION) {
		fprintf(stderr, "librdmacm: kernel ABI version %d "
				"doesn't match library version %d.\n",
				abi_ver, RDMA_USER_CM_MAX_ABI_VERSION);
		return -1;
	}
	return 0;
}

static int ucma_init(void)
{
	struct ibv_device **dev_list = NULL;
	struct cma_device *cma_dev;
	struct ibv_device_attr attr;
	int i, ret, dev_cnt;

	pthread_mutex_lock(&mut);
	if (cma_dev_cnt) {
		pthread_mutex_unlock(&mut);
		return 0;
	}

	ret = check_abi_version();
	if (ret)
		goto err1;

	dev_list = ibv_get_device_list(&dev_cnt);
	if (!dev_list) {
		printf("CMA: unable to get RDMA device list\n");
		ret = ERR(ENODEV);
		goto err1;
	}

	cma_dev_array = malloc(sizeof *cma_dev * dev_cnt);
	if (!cma_dev_array) {
		ret = ERR(ENOMEM);
		goto err2;
	}

	for (i = 0; dev_list[i];) {
		cma_dev = &cma_dev_array[i];

		cma_dev->guid = ibv_get_device_guid(dev_list[i]);
		cma_dev->verbs = ibv_open_device(dev_list[i]);
		if (!cma_dev->verbs) {
			printf("CMA: unable to open RDMA device\n");
			ret = ERR(ENODEV);
			goto err3;
		}

		i++;
		ret = ibv_query_device(cma_dev->verbs, &attr);
		if (ret) {
			printf("CMA: unable to query RDMA device\n");
			goto err3;
		}

		cma_dev->port_cnt = attr.phys_port_cnt;
		cma_dev->max_initiator_depth = (uint8_t) attr.max_qp_init_rd_atom;
		cma_dev->max_responder_resources = (uint8_t) attr.max_qp_rd_atom;
	}

	cma_dev_cnt = dev_cnt;
	pthread_mutex_unlock(&mut);
	ibv_free_device_list(dev_list);
	return 0;

err3:
	while (i--)
		ibv_close_device(cma_dev_array[i].verbs);
	free(cma_dev_array);
err2:
	ibv_free_device_list(dev_list);
err1:
	pthread_mutex_unlock(&mut);
	return ret;
}

struct ibv_context **rdma_get_devices(int *num_devices)
{
	struct ibv_context **devs = NULL;
	int i;

	if (!cma_dev_cnt && ucma_init())
		goto out;

	devs = malloc(sizeof *devs * (cma_dev_cnt + 1));
	if (!devs)
		goto out;

	for (i = 0; i < cma_dev_cnt; i++)
		devs[i] = cma_dev_array[i].verbs;
	devs[i] = NULL;
out:
	if (num_devices)
		*num_devices = devs ? cma_dev_cnt : 0;
	return devs;
}

void rdma_free_devices(struct ibv_context **list)
{
	free(list);
}

static void __attribute__((destructor)) rdma_cma_fini(void)
{
	ucma_cleanup();
}

struct rdma_event_channel *rdma_create_event_channel(void)
{
	struct rdma_event_channel *channel;

	if (!cma_dev_cnt && ucma_init())
		return NULL;

	channel = malloc(sizeof *channel);
	if (!channel)
		return NULL;

	channel->fd = open("/dev/rdma_cm", O_RDWR);
	if (channel->fd < 0) {
		printf("CMA: unable to open /dev/rdma_cm\n");
		goto err;
	}
	return channel;
err:
	free(channel);
	return NULL;
}

void rdma_destroy_event_channel(struct rdma_event_channel *channel)
{
	close(channel->fd);
	free(channel);
}

static int ucma_get_device(struct cma_id_private *id_priv, uint64_t guid)
{
	struct cma_device *cma_dev;
	int i;

	for (i = 0; i < cma_dev_cnt; i++) {
		cma_dev = &cma_dev_array[i];
		if (cma_dev->guid == guid) {
			id_priv->cma_dev = cma_dev;
			id_priv->id.verbs = cma_dev->verbs;
			return 0;
		}
	}

	return ERR(ENODEV);
}

static void ucma_free_id(struct cma_id_private *id_priv)
{
	pthread_cond_destroy(&id_priv->cond);
	pthread_mutex_destroy(&id_priv->mut);
	if (id_priv->id.route.path_rec)
		free(id_priv->id.route.path_rec);
	free(id_priv);
}

static struct cma_id_private *ucma_alloc_id(struct rdma_event_channel *channel,
					    void *context,
					    enum rdma_port_space ps)
{
	struct cma_id_private *id_priv;

	id_priv = malloc(sizeof *id_priv);
	if (!id_priv)
		return NULL;

	memset(id_priv, 0, sizeof *id_priv);
	id_priv->id.context = context;
	id_priv->id.ps = ps;
	id_priv->id.channel = channel;
	pthread_mutex_init(&id_priv->mut, NULL);
	if (pthread_cond_init(&id_priv->cond, NULL))
		goto err;

	return id_priv;

err:	ucma_free_id(id_priv);
	return NULL;
}

int rdma_create_id(struct rdma_event_channel *channel,
		   struct rdma_cm_id **id, void *context,
		   enum rdma_port_space ps)
{
	struct ucma_abi_create_id_resp *resp;
	struct ucma_abi_create_id *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size;

	ret = cma_dev_cnt ? 0 : ucma_init();
	if (ret)
		return ret;

	id_priv = ucma_alloc_id(channel, context, ps);
	if (!id_priv)
		return ERR(ENOMEM);

	CMA_CREATE_MSG_CMD_RESP(msg, cmd, resp, UCMA_CMD_CREATE_ID, size);
	cmd->uid = (uintptr_t) id_priv;
	cmd->ps = ps;

	ret = write(channel->fd, msg, size);
	if (ret != size)
		goto err;

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof *resp);

	id_priv->handle = resp->id;
	*id = &id_priv->id;
	return 0;

err:	ucma_free_id(id_priv);
	return ret;
}

static int ucma_destroy_kern_id(int fd, uint32_t handle)
{
	struct ucma_abi_destroy_id_resp *resp;
	struct ucma_abi_destroy_id *cmd;
	void *msg;
	int ret, size;
	
	CMA_CREATE_MSG_CMD_RESP(msg, cmd, resp, UCMA_CMD_DESTROY_ID, size);
	cmd->id = handle;

	ret = write(fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof *resp);

	return resp->events_reported;
}

int rdma_destroy_id(struct rdma_cm_id *id)
{
	struct cma_id_private *id_priv;
	int ret;

	id_priv = container_of(id, struct cma_id_private, id);
	ret = ucma_destroy_kern_id(id->channel->fd, id_priv->handle);
	if (ret < 0)
		return ret;

	pthread_mutex_lock(&id_priv->mut);
	while (id_priv->events_completed < ret)
		pthread_cond_wait(&id_priv->cond, &id_priv->mut);
	pthread_mutex_unlock(&id_priv->mut);

	ucma_free_id(id_priv);
	return 0;
}

static int ucma_addrlen(struct sockaddr *addr)
{
	if (!addr)
		return 0;

	switch (addr->sa_family) {
	case PF_INET:
		return sizeof(struct sockaddr_in);
	case PF_INET6:
		return sizeof(struct sockaddr_in6);
	default:
		return 0;
	}
}

static int ucma_query_route(struct rdma_cm_id *id)
{
	struct ucma_abi_query_route_resp *resp;
	struct ucma_abi_query_route *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size, i;
	
	CMA_CREATE_MSG_CMD_RESP(msg, cmd, resp, UCMA_CMD_QUERY_ROUTE, size);
	id_priv = container_of(id, struct cma_id_private, id);
	cmd->id = id_priv->handle;

	ret = write(id->channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof *resp);

	if (resp->num_paths) {
		id->route.path_rec = malloc(sizeof *id->route.path_rec *
					    resp->num_paths);
		if (!id->route.path_rec)
			return ERR(ENOMEM);

		id->route.num_paths = resp->num_paths;
		for (i = 0; i < resp->num_paths; i++)
			ibv_copy_path_rec_from_kern(&id->route.path_rec[i],
						    &resp->ib_route[i]);
	}

	memcpy(id->route.addr.addr.ibaddr.sgid.raw, resp->ib_route[0].sgid,
	       sizeof id->route.addr.addr.ibaddr.sgid);
	memcpy(id->route.addr.addr.ibaddr.dgid.raw, resp->ib_route[0].dgid,
	       sizeof id->route.addr.addr.ibaddr.dgid);
	id->route.addr.addr.ibaddr.pkey = resp->ib_route[0].pkey;
	memcpy(&id->route.addr.src_addr, &resp->src_addr,
	       sizeof resp->src_addr);
	memcpy(&id->route.addr.dst_addr, &resp->dst_addr,
	       sizeof resp->dst_addr);

	if (!id_priv->cma_dev && resp->node_guid) {
		ret = ucma_get_device(id_priv, resp->node_guid);
		if (ret)
			return ret;
		id_priv->id.port_num = resp->port_num;
	}

	return 0;
}

int rdma_bind_addr(struct rdma_cm_id *id, struct sockaddr *addr)
{
	struct ucma_abi_bind_addr *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size, addrlen;
	
	addrlen = ucma_addrlen(addr);
	if (!addrlen)
		return ERR(EINVAL);

	CMA_CREATE_MSG_CMD(msg, cmd, UCMA_CMD_BIND_ADDR, size);
	id_priv = container_of(id, struct cma_id_private, id);
	cmd->id = id_priv->handle;
	memcpy(&cmd->addr, addr, addrlen);

	ret = write(id->channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	return ucma_query_route(id);
}

int rdma_resolve_addr(struct rdma_cm_id *id, struct sockaddr *src_addr,
		      struct sockaddr *dst_addr, int timeout_ms)
{
	struct ucma_abi_resolve_addr *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size, daddrlen;
	
	daddrlen = ucma_addrlen(dst_addr);
	if (!daddrlen)
		return ERR(EINVAL);

	CMA_CREATE_MSG_CMD(msg, cmd, UCMA_CMD_RESOLVE_ADDR, size);
	id_priv = container_of(id, struct cma_id_private, id);
	cmd->id = id_priv->handle;
	if (src_addr)
		memcpy(&cmd->src_addr, src_addr, ucma_addrlen(src_addr));
	memcpy(&cmd->dst_addr, dst_addr, daddrlen);
	cmd->timeout_ms = timeout_ms;

	ret = write(id->channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	memcpy(&id->route.addr.dst_addr, dst_addr, daddrlen);
	return 0;
}

int rdma_resolve_route(struct rdma_cm_id *id, int timeout_ms)
{
	struct ucma_abi_resolve_route *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size;
	
	CMA_CREATE_MSG_CMD(msg, cmd, UCMA_CMD_RESOLVE_ROUTE, size);
	id_priv = container_of(id, struct cma_id_private, id);
	cmd->id = id_priv->handle;
	cmd->timeout_ms = timeout_ms;

	ret = write(id->channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	return 0;
}

static int ucma_is_ud_ps(enum rdma_port_space ps)
{
	return (ps == RDMA_PS_UDP || ps == RDMA_PS_IPOIB);
}

static int rdma_init_qp_attr(struct rdma_cm_id *id, struct ibv_qp_attr *qp_attr,
			     int *qp_attr_mask)
{
	struct ucma_abi_init_qp_attr *cmd;
	struct ibv_kern_qp_attr *resp;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size;
	
	CMA_CREATE_MSG_CMD_RESP(msg, cmd, resp, UCMA_CMD_INIT_QP_ATTR, size);
	id_priv = container_of(id, struct cma_id_private, id);
	cmd->id = id_priv->handle;
	cmd->qp_state = qp_attr->qp_state;

	ret = write(id->channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof *resp);

	ibv_copy_qp_attr_from_kern(qp_attr, resp);
	*qp_attr_mask = resp->qp_attr_mask;
	return 0;
}

static int ucma_modify_qp_rtr(struct rdma_cm_id *id,
			      struct rdma_conn_param *conn_param)
{
	struct ibv_qp_attr qp_attr;
	int qp_attr_mask, ret;

	if (!id->qp)
		return ERR(EINVAL);

	/* Need to update QP attributes from default values. */
	qp_attr.qp_state = IBV_QPS_INIT;
	ret = rdma_init_qp_attr(id, &qp_attr, &qp_attr_mask);
	if (ret)
		return ret;

	ret = ibv_modify_qp(id->qp, &qp_attr, qp_attr_mask);
	if (ret)
		return ret;

	qp_attr.qp_state = IBV_QPS_RTR;
	ret = rdma_init_qp_attr(id, &qp_attr, &qp_attr_mask);
	if (ret)
		return ret;

	if (conn_param)
		qp_attr.max_dest_rd_atomic = conn_param->responder_resources;
	return ibv_modify_qp(id->qp, &qp_attr, qp_attr_mask);
}

static int ucma_modify_qp_rts(struct rdma_cm_id *id)
{
	struct ibv_qp_attr qp_attr;
	int qp_attr_mask, ret;

	qp_attr.qp_state = IBV_QPS_RTS;
	ret = rdma_init_qp_attr(id, &qp_attr, &qp_attr_mask);
	if (ret)
		return ret;

	return ibv_modify_qp(id->qp, &qp_attr, qp_attr_mask);
}

static int ucma_modify_qp_sqd(struct rdma_cm_id *id)
{
	struct ibv_qp_attr qp_attr;

	if (!id->qp)
		return 0;

	qp_attr.qp_state = IBV_QPS_SQD;
	return ibv_modify_qp(id->qp, &qp_attr, IBV_QP_STATE);
}

static int ucma_modify_qp_err(struct rdma_cm_id *id)
{
	struct ibv_qp_attr qp_attr;

	if (!id->qp)
		return 0;

	qp_attr.qp_state = IBV_QPS_ERR;
	return ibv_modify_qp(id->qp, &qp_attr, IBV_QP_STATE);
}

static int ucma_find_pkey(struct cma_device *cma_dev, uint8_t port_num,
			  uint16_t pkey, uint16_t *pkey_index)
{
	int ret, i;
	uint16_t chk_pkey;

	for (i = 0, ret = 0; !ret; i++) {
		ret = ibv_query_pkey(cma_dev->verbs, port_num, i, &chk_pkey);
		if (!ret && pkey == chk_pkey) {
			*pkey_index = (uint16_t) i;
			return 0;
		}
	}
	return ERR(EINVAL);
}

static int ucma_init_conn_qp3(struct cma_id_private *id_priv, struct ibv_qp *qp)
{
	struct ibv_qp_attr qp_attr;
	int ret;

	ret = ucma_find_pkey(id_priv->cma_dev, id_priv->id.port_num,
			     id_priv->id.route.addr.addr.ibaddr.pkey,
			     &qp_attr.pkey_index);
	if (ret)
		return ret;

	qp_attr.port_num = id_priv->id.port_num;
	qp_attr.qp_state = IBV_QPS_INIT;
	qp_attr.qp_access_flags = 0;

	return ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_ACCESS_FLAGS |
					   IBV_QP_PKEY_INDEX | IBV_QP_PORT);
}

static int ucma_init_conn_qp(struct cma_id_private *id_priv, struct ibv_qp *qp)
{
	struct ibv_qp_attr qp_attr;
	int qp_attr_mask, ret;

	if (abi_ver == 3)
		return ucma_init_conn_qp3(id_priv, qp);

	qp_attr.qp_state = IBV_QPS_INIT;
	ret = rdma_init_qp_attr(&id_priv->id, &qp_attr, &qp_attr_mask);
	if (ret)
		return ret;

	return ibv_modify_qp(qp, &qp_attr, qp_attr_mask);
}

static int ucma_init_ud_qp3(struct cma_id_private *id_priv, struct ibv_qp *qp)
{
	struct ibv_qp_attr qp_attr;
	int ret;

	ret = ucma_find_pkey(id_priv->cma_dev, id_priv->id.port_num,
			     id_priv->id.route.addr.addr.ibaddr.pkey,
			     &qp_attr.pkey_index);
	if (ret)
		return ret;

	qp_attr.port_num = id_priv->id.port_num;
	qp_attr.qp_state = IBV_QPS_INIT;
	qp_attr.qkey = RDMA_UDP_QKEY;

	ret = ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_QKEY |
					  IBV_QP_PKEY_INDEX | IBV_QP_PORT);
	if (ret)
		return ret;

	qp_attr.qp_state = IBV_QPS_RTR;
	ret = ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE);
	if (ret)
		return ret;

	qp_attr.qp_state = IBV_QPS_RTS;
	qp_attr.sq_psn = 0;
	return ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_SQ_PSN);
}

static int ucma_init_ud_qp(struct cma_id_private *id_priv, struct ibv_qp *qp)
{
	struct ibv_qp_attr qp_attr;
	int qp_attr_mask, ret;

	if (abi_ver == 3)
		return ucma_init_ud_qp3(id_priv, qp);

	qp_attr.qp_state = IBV_QPS_INIT;
	ret = rdma_init_qp_attr(&id_priv->id, &qp_attr, &qp_attr_mask);
	if (ret)
		return ret;

	ret = ibv_modify_qp(qp, &qp_attr, qp_attr_mask);
	if (ret)
		return ret;

	qp_attr.qp_state = IBV_QPS_RTR;
	ret = ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE);
	if (ret)
		return ret;

	qp_attr.qp_state = IBV_QPS_RTS;
	qp_attr.sq_psn = 0;
	return ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_SQ_PSN);
}

int rdma_create_qp(struct rdma_cm_id *id, struct ibv_pd *pd,
		   struct ibv_qp_init_attr *qp_init_attr)
{
	struct cma_id_private *id_priv;
	struct ibv_qp *qp;
	int ret;

	id_priv = container_of(id, struct cma_id_private, id);
	if (id->verbs != pd->context)
		return ERR(EINVAL);

	qp = ibv_create_qp(pd, qp_init_attr);
	if (!qp)
		return ERR(ENOMEM);

	if (ucma_is_ud_ps(id->ps))
		ret = ucma_init_ud_qp(id_priv, qp);
	else
		ret = ucma_init_conn_qp(id_priv, qp);
	if (ret)
		goto err;

	id->qp = qp;
	return 0;
err:
	ibv_destroy_qp(qp);
	return ret;
}

void rdma_destroy_qp(struct rdma_cm_id *id)
{
	ibv_destroy_qp(id->qp);
}

static int ucma_valid_param(struct cma_id_private *id_priv,
			    struct rdma_conn_param *conn_param)
{
	if (id_priv->id.ps != RDMA_PS_TCP)
		return 0;

	if ((conn_param->responder_resources >
	     id_priv->cma_dev->max_responder_resources) ||
	    (conn_param->initiator_depth >
	     id_priv->cma_dev->max_initiator_depth))
		return ERR(EINVAL);

	return 0;
}

static void ucma_copy_conn_param_to_kern(struct ucma_abi_conn_param *dst,
					 struct rdma_conn_param *src,
					 uint32_t qp_num, uint8_t srq)
{
	dst->qp_num = qp_num;
	dst->srq = srq;
	dst->responder_resources = src->responder_resources;
	dst->initiator_depth = src->initiator_depth;
	dst->flow_control = src->flow_control;
	dst->retry_count = src->retry_count;
	dst->rnr_retry_count = src->rnr_retry_count;
	dst->valid = 1;

	if (src->private_data && src->private_data_len) {
		memcpy(dst->private_data, src->private_data,
		       src->private_data_len);
		dst->private_data_len = src->private_data_len;
	}
}

int rdma_connect(struct rdma_cm_id *id, struct rdma_conn_param *conn_param)
{
	struct ucma_abi_connect *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size;
	
	id_priv = container_of(id, struct cma_id_private, id);
	ret = ucma_valid_param(id_priv, conn_param);
	if (ret)
		return ret;

	CMA_CREATE_MSG_CMD(msg, cmd, UCMA_CMD_CONNECT, size);
	cmd->id = id_priv->handle;
	if (id->qp)
		ucma_copy_conn_param_to_kern(&cmd->conn_param, conn_param,
					     id->qp->qp_num,
					     (id->qp->srq != NULL));
	else
		ucma_copy_conn_param_to_kern(&cmd->conn_param, conn_param,
					     conn_param->qp_num,
					     conn_param->srq);

	ret = write(id->channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	return 0;
}

int rdma_listen(struct rdma_cm_id *id, int backlog)
{
	struct ucma_abi_listen *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size;
	
	CMA_CREATE_MSG_CMD(msg, cmd, UCMA_CMD_LISTEN, size);
	id_priv = container_of(id, struct cma_id_private, id);
	cmd->id = id_priv->handle;
	cmd->backlog = backlog;

	ret = write(id->channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	return ucma_query_route(id);
}

int rdma_accept(struct rdma_cm_id *id, struct rdma_conn_param *conn_param)
{
	struct ucma_abi_accept *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size;

	id_priv = container_of(id, struct cma_id_private, id);
	ret = ucma_valid_param(id_priv, conn_param);
	if (ret)
		return ret;

	if (!ucma_is_ud_ps(id->ps)) {
		ret = ucma_modify_qp_rtr(id, conn_param);
		if (ret)
			return ret;
	}

	CMA_CREATE_MSG_CMD(msg, cmd, UCMA_CMD_ACCEPT, size);
	cmd->id = id_priv->handle;
	cmd->uid = (uintptr_t) id_priv;
	if (id->qp)
		ucma_copy_conn_param_to_kern(&cmd->conn_param, conn_param,
					     id->qp->qp_num,
					     (id->qp->srq != NULL));
	else
		ucma_copy_conn_param_to_kern(&cmd->conn_param, conn_param,
					     conn_param->qp_num,
					     conn_param->srq);

	ret = write(id->channel->fd, msg, size);
	if (ret != size) {
		ucma_modify_qp_err(id);
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;
	}

	return 0;
}

int rdma_reject(struct rdma_cm_id *id, const void *private_data,
		uint8_t private_data_len)
{
	struct ucma_abi_reject *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size;
	
	CMA_CREATE_MSG_CMD(msg, cmd, UCMA_CMD_REJECT, size);

	id_priv = container_of(id, struct cma_id_private, id);
	cmd->id = id_priv->handle;
	if (private_data && private_data_len) {
		memcpy(cmd->private_data, private_data, private_data_len);
		cmd->private_data_len = private_data_len;
	} else
		cmd->private_data_len = 0;

	ret = write(id->channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	return 0;
}

int rdma_notify(struct rdma_cm_id *id, enum ibv_event_type event)
{
	struct ucma_abi_notify *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size;
	
	CMA_CREATE_MSG_CMD(msg, cmd, UCMA_CMD_NOTIFY, size);

	id_priv = container_of(id, struct cma_id_private, id);
	cmd->id = id_priv->handle;
	cmd->event = event;
	ret = write(id->channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	return 0;
}

int rdma_disconnect(struct rdma_cm_id *id)
{
	struct ucma_abi_disconnect *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size;

	switch (id->verbs->device->transport_type) {
	case IBV_TRANSPORT_IB:
		ret = ucma_modify_qp_err(id);
		break;
	case IBV_TRANSPORT_IWARP:
		ret = ucma_modify_qp_sqd(id);
		break;
	default:
		ret = ERR(EINVAL);
	}
	if (ret)
		return ret;

	CMA_CREATE_MSG_CMD(msg, cmd, UCMA_CMD_DISCONNECT, size);
	id_priv = container_of(id, struct cma_id_private, id);
	cmd->id = id_priv->handle;

	ret = write(id->channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	return 0;
}

int rdma_join_multicast(struct rdma_cm_id *id, struct sockaddr *addr,
			void *context)
{
	struct ucma_abi_join_mcast *cmd;
	struct ucma_abi_create_id_resp *resp;
	struct cma_id_private *id_priv;
	struct cma_multicast *mc, **pos;
	void *msg;
	int ret, size, addrlen;
	
	id_priv = container_of(id, struct cma_id_private, id);
	addrlen = ucma_addrlen(addr);
	if (!addrlen)
		return ERR(EINVAL);

	mc = malloc(sizeof *mc);
	if (!mc)
		return ERR(ENOMEM);

	memset(mc, 0, sizeof *mc);
	mc->context = context;
	mc->id_priv = id_priv;
	memcpy(&mc->addr, addr, addrlen);
	if (pthread_cond_init(&mc->cond, NULL)) {
		ret = -1;
		goto err1;
	}

	pthread_mutex_lock(&id_priv->mut);
	mc->next = id_priv->mc_list;
	id_priv->mc_list = mc;
	pthread_mutex_unlock(&id_priv->mut);

	CMA_CREATE_MSG_CMD_RESP(msg, cmd, resp, UCMA_CMD_JOIN_MCAST, size);
	cmd->id = id_priv->handle;
	memcpy(&cmd->addr, addr, addrlen);
	cmd->uid = (uintptr_t) mc;

	ret = write(id->channel->fd, msg, size);
	if (ret != size) {
		ret = (ret >= 0) ? ERR(ECONNREFUSED) : -1;
		goto err2;
	}

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof *resp);

	mc->handle = resp->id;
	return 0;
err2:
	pthread_mutex_lock(&id_priv->mut);
	for (pos = &id_priv->mc_list; *pos != mc; pos = &(*pos)->next)
		;
	*pos = mc->next;
	pthread_mutex_unlock(&id_priv->mut);
err1:
	free(mc);
	return ret;
}

int rdma_leave_multicast(struct rdma_cm_id *id, struct sockaddr *addr)
{
	struct ucma_abi_destroy_id *cmd;
	struct ucma_abi_destroy_id_resp *resp;
	struct cma_id_private *id_priv;
	struct cma_multicast *mc, **pos;
	void *msg;
	int ret, size, addrlen;
	
	addrlen = ucma_addrlen(addr);
	if (!addrlen)
		return ERR(EINVAL);

	id_priv = container_of(id, struct cma_id_private, id);
	pthread_mutex_lock(&id_priv->mut);
	for (pos = &id_priv->mc_list; *pos; pos = &(*pos)->next)
		if (!memcmp(&(*pos)->addr, addr, addrlen))
			break;

	mc = *pos;
	if (*pos)
		*pos = mc->next;
	pthread_mutex_unlock(&id_priv->mut);
	if (!mc)
		return ERR(EADDRNOTAVAIL);

	if (id->qp)
		ibv_detach_mcast(id->qp, &mc->mgid, mc->mlid);
	
	CMA_CREATE_MSG_CMD_RESP(msg, cmd, resp, UCMA_CMD_LEAVE_MCAST, size);
	cmd->id = mc->handle;

	ret = write(id->channel->fd, msg, size);
	if (ret != size) {
		ret = (ret >= 0) ? ERR(ECONNREFUSED) : -1;
		goto free;
	}

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof *resp);

	pthread_mutex_lock(&id_priv->mut);
	while (mc->events_completed < resp->events_reported)
		pthread_cond_wait(&mc->cond, &id_priv->mut);
	pthread_mutex_unlock(&id_priv->mut);

	ret = 0;
free:
	free(mc);
	return ret;
}

static void ucma_complete_event(struct cma_id_private *id_priv)
{
	pthread_mutex_lock(&id_priv->mut);
	id_priv->events_completed++;
	pthread_cond_signal(&id_priv->cond);
	pthread_mutex_unlock(&id_priv->mut);
}

static void ucma_complete_mc_event(struct cma_multicast *mc)
{
	pthread_mutex_lock(&mc->id_priv->mut);
	mc->events_completed++;
	pthread_cond_signal(&mc->cond);
	mc->id_priv->events_completed++;
	pthread_cond_signal(&mc->id_priv->cond);
	pthread_mutex_unlock(&mc->id_priv->mut);
}

int rdma_ack_cm_event(struct rdma_cm_event *event)
{
	struct cma_event *evt;

	if (!event)
		return ERR(EINVAL);

	evt = container_of(event, struct cma_event, event);

	if (evt->mc)
		ucma_complete_mc_event(evt->mc);
	else
		ucma_complete_event(evt->id_priv);
	free(evt);
	return 0;
}

static int ucma_process_conn_req(struct cma_event *evt,
				 uint32_t handle)
{
	struct cma_id_private *id_priv;
	int ret;

	id_priv = ucma_alloc_id(evt->id_priv->id.channel,
				evt->id_priv->id.context, evt->id_priv->id.ps);
	if (!id_priv) {
		ucma_destroy_kern_id(evt->id_priv->id.channel->fd, handle);
		ret = ERR(ENOMEM);
		goto err;
	}

	evt->event.listen_id = &evt->id_priv->id;
	evt->event.id = &id_priv->id;
	id_priv->handle = handle;

	ret = ucma_query_route(&id_priv->id);
	if (ret) {
		rdma_destroy_id(&id_priv->id);
		goto err;
	}

	return 0;
err:
	ucma_complete_event(evt->id_priv);
	return ret;
}

static int ucma_process_conn_resp(struct cma_id_private *id_priv)
{
	struct ucma_abi_accept *cmd;
	void *msg;
	int ret, size;

	ret = ucma_modify_qp_rtr(&id_priv->id, NULL);
	if (ret)
		goto err;

	ret = ucma_modify_qp_rts(&id_priv->id);
	if (ret)
		goto err;

	CMA_CREATE_MSG_CMD(msg, cmd, UCMA_CMD_ACCEPT, size);
	cmd->id = id_priv->handle;

	ret = write(id_priv->id.channel->fd, msg, size);
	if (ret != size) {
		ret = (ret >= 0) ? ERR(ECONNREFUSED) : -1;
		goto err;
	}

	return 0;
err:
	ucma_modify_qp_err(&id_priv->id);
	return ret;
}

static int ucma_process_establish(struct rdma_cm_id *id)
{
	int ret;

	ret = ucma_modify_qp_rts(id);
	if (ret)
		ucma_modify_qp_err(id);

	return ret;
}

static int ucma_process_join(struct cma_event *evt)
{
	evt->mc->mgid = evt->event.param.ud.ah_attr.grh.dgid;
	evt->mc->mlid = evt->event.param.ud.ah_attr.dlid;

	if (!evt->id_priv->id.qp)
		return 0;

	return ibv_attach_mcast(evt->id_priv->id.qp, &evt->mc->mgid,
				evt->mc->mlid);
}

static void ucma_copy_conn_event(struct cma_event *event,
				 struct ucma_abi_conn_param *src)
{
	struct rdma_conn_param *dst = &event->event.param.conn;

	dst->private_data_len = src->private_data_len;
	if (src->private_data_len) {
		dst->private_data = &event->private_data;
		memcpy(&event->private_data, src->private_data,
		       src->private_data_len);
	}

	dst->responder_resources = src->responder_resources;
	dst->initiator_depth = src->initiator_depth;
	dst->flow_control = src->flow_control;
	dst->retry_count = src->retry_count;
	dst->rnr_retry_count = src->rnr_retry_count;
	dst->srq = src->srq;
	dst->qp_num = src->qp_num;
}

static void ucma_copy_ud_event(struct cma_event *event,
			       struct ucma_abi_ud_param *src)
{
	struct rdma_ud_param *dst = &event->event.param.ud;

	dst->private_data_len = src->private_data_len;
	if (src->private_data_len) {
		dst->private_data = &event->private_data;
		memcpy(&event->private_data, src->private_data,
		       src->private_data_len);
	}

	ibv_copy_ah_attr_from_kern(&dst->ah_attr, &src->ah_attr);
	dst->qp_num = src->qp_num;
	dst->qkey = src->qkey;
}

int rdma_get_cm_event(struct rdma_event_channel *channel,
		      struct rdma_cm_event **event)
{
	struct ucma_abi_event_resp *resp;
	struct ucma_abi_get_event *cmd;
	struct cma_event *evt;
	void *msg;
	int ret, size;

	ret = cma_dev_cnt ? 0 : ucma_init();
	if (ret)
		return ret;

	if (!event)
		return ERR(EINVAL);

	evt = malloc(sizeof *evt);
	if (!evt)
		return ERR(ENOMEM);

retry:
	memset(evt, 0, sizeof *evt);
	CMA_CREATE_MSG_CMD_RESP(msg, cmd, resp, UCMA_CMD_GET_EVENT, size);
	ret = write(channel->fd, msg, size);
	if (ret != size) {
		free(evt);
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;
	}
	
	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof *resp);

	evt->event.event = resp->event;
	evt->id_priv = (void *) (uintptr_t) resp->uid;
	evt->event.id = &evt->id_priv->id;
	evt->event.status = resp->status;

	switch (resp->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		evt->event.status = ucma_query_route(&evt->id_priv->id);
		if (evt->event.status)
			evt->event.event = RDMA_CM_EVENT_ADDR_ERROR;
		break;
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		evt->event.status = ucma_query_route(&evt->id_priv->id);
		if (evt->event.status)
			evt->event.event = RDMA_CM_EVENT_ROUTE_ERROR;
		break;
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		evt->id_priv = (void *) (uintptr_t) resp->uid;
		if (ucma_is_ud_ps(evt->id_priv->id.ps))
			ucma_copy_ud_event(evt, &resp->param.ud);
		else
			ucma_copy_conn_event(evt, &resp->param.conn);

		ret = ucma_process_conn_req(evt, resp->id);
		if (ret)
			goto retry;
		break;
	case RDMA_CM_EVENT_CONNECT_RESPONSE:
		ucma_copy_conn_event(evt, &resp->param.conn);
		evt->event.status = ucma_process_conn_resp(evt->id_priv);
		if (!evt->event.status)
			evt->event.event = RDMA_CM_EVENT_ESTABLISHED;
		else {
			evt->event.event = RDMA_CM_EVENT_CONNECT_ERROR;
			evt->id_priv->connect_error = 1;
		}
		break;
	case RDMA_CM_EVENT_ESTABLISHED:
		if (ucma_is_ud_ps(evt->id_priv->id.ps)) {
			ucma_copy_ud_event(evt, &resp->param.ud);
			break;
		}

		ucma_copy_conn_event(evt, &resp->param.conn);
		evt->event.status = ucma_process_establish(&evt->id_priv->id);
		if (evt->event.status) {
			evt->event.event = RDMA_CM_EVENT_CONNECT_ERROR;
			evt->id_priv->connect_error = 1;
		}
		break;
	case RDMA_CM_EVENT_REJECTED:
		if (evt->id_priv->connect_error) {
			ucma_complete_event(evt->id_priv);
			goto retry;
		}
		ucma_copy_conn_event(evt, &resp->param.conn);
		ucma_modify_qp_err(evt->event.id);
		break;
	case RDMA_CM_EVENT_DISCONNECTED:
		if (evt->id_priv->connect_error) {
			ucma_complete_event(evt->id_priv);
			goto retry;
		}
		ucma_copy_conn_event(evt, &resp->param.conn);
		break;
	case RDMA_CM_EVENT_MULTICAST_JOIN:
		evt->mc = (void *) (uintptr_t) resp->uid;
		evt->id_priv = evt->mc->id_priv;
		evt->event.id = &evt->id_priv->id;
		ucma_copy_ud_event(evt, &resp->param.ud);
		evt->event.param.ud.private_data = evt->mc->context;
		evt->event.status = ucma_process_join(evt);
		if (evt->event.status)
			evt->event.event = RDMA_CM_EVENT_MULTICAST_ERROR;
		break;
	case RDMA_CM_EVENT_MULTICAST_ERROR:
		evt->mc = (void *) (uintptr_t) resp->uid;
		evt->id_priv = evt->mc->id_priv;
		evt->event.id = &evt->id_priv->id;
		evt->event.param.ud.private_data = evt->mc->context;
		break;
	default:
		evt->id_priv = (void *) (uintptr_t) resp->uid;
		evt->event.id = &evt->id_priv->id;
		evt->event.status = resp->status;
		if (ucma_is_ud_ps(evt->id_priv->id.ps))
			ucma_copy_ud_event(evt, &resp->param.ud);
		else
			ucma_copy_conn_event(evt, &resp->param.conn);
		break;
	}

	*event = &evt->event;
	return 0;
}

const char *rdma_event_str(enum rdma_cm_event_type event)
{
	switch (event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		return "RDMA_CM_EVENT_ADDR_RESOLVED";
	case RDMA_CM_EVENT_ADDR_ERROR:
		return "RDMA_CM_EVENT_ADDR_ERROR";
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		return "RDMA_CM_EVENT_ROUTE_RESOLVED";
	case RDMA_CM_EVENT_ROUTE_ERROR:
		return "RDMA_CM_EVENT_ROUTE_ERROR";
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		return "RDMA_CM_EVENT_CONNECT_REQUEST";
	case RDMA_CM_EVENT_CONNECT_RESPONSE:
		return "RDMA_CM_EVENT_CONNECT_RESPONSE";
	case RDMA_CM_EVENT_CONNECT_ERROR:
		return "RDMA_CM_EVENT_CONNECT_ERROR";
	case RDMA_CM_EVENT_UNREACHABLE:
		return "RDMA_CM_EVENT_UNREACHABLE";
	case RDMA_CM_EVENT_REJECTED:
		return "RDMA_CM_EVENT_REJECTED";
	case RDMA_CM_EVENT_ESTABLISHED:
		return "RDMA_CM_EVENT_ESTABLISHED";
	case RDMA_CM_EVENT_DISCONNECTED:
		return "RDMA_CM_EVENT_DISCONNECTED";
	case RDMA_CM_EVENT_DEVICE_REMOVAL:
		return "RDMA_CM_EVENT_DEVICE_REMOVAL";
	case RDMA_CM_EVENT_MULTICAST_JOIN:
		return "RDMA_CM_EVENT_MULTICAST_JOIN";
	case RDMA_CM_EVENT_MULTICAST_ERROR:
		return "RDMA_CM_EVENT_MULTICAST_ERROR";
	case RDMA_CM_EVENT_ADDR_CHANGE:
		return "RDMA_CM_EVENT_ADDR_CHANGE";
	case RDMA_CM_EVENT_TIMEWAIT_EXIT:
		return "RDMA_CM_EVENT_TIMEWAIT_EXIT";
	default:
		return "UNKNOWN EVENT";
	}
}

int rdma_set_option(struct rdma_cm_id *id, int level, int optname,
		    void *optval, size_t optlen)
{
	struct ucma_abi_set_option *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size;
	
	CMA_CREATE_MSG_CMD(msg, cmd, UCMA_CMD_SET_OPTION, size);
	id_priv = container_of(id, struct cma_id_private, id);
	cmd->id = id_priv->handle;
	cmd->optval = (uintptr_t) optval;
	cmd->level = level;
	cmd->optname = optname;
	cmd->optlen = optlen;

	ret = write(id->channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	return 0;
}

int rdma_migrate_id(struct rdma_cm_id *id, struct rdma_event_channel *channel)
{
	struct ucma_abi_migrate_resp *resp;
	struct ucma_abi_migrate_id *cmd;
	struct cma_id_private *id_priv;
	void *msg;
	int ret, size;

	id_priv = container_of(id, struct cma_id_private, id);
	CMA_CREATE_MSG_CMD_RESP(msg, cmd, resp, UCMA_CMD_MIGRATE_ID, size);
	cmd->id = id_priv->handle;
	cmd->fd = id->channel->fd;

	ret = write(channel->fd, msg, size);
	if (ret != size)
		return (ret >= 0) ? ERR(ECONNREFUSED) : -1;

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof *resp);

	/*
	 * Eventually if we want to support migrating channels while events are
	 * being processed on the current channel, we need to block here while
	 * there are any outstanding events on the current channel for this id
	 * to prevent the user from processing events for this id on the old
	 * channel after this call returns.
	 */
	pthread_mutex_lock(&id_priv->mut);
	id->channel = channel;
	while (id_priv->events_completed < resp->events_reported)
		pthread_cond_wait(&id_priv->cond, &id_priv->mut);
	pthread_mutex_unlock(&id_priv->mut);

	return 0;
}
