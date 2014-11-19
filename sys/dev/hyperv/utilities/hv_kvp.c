/*-
 * Copyright (c) 2014 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	Author:	Sainath Varanasi.
 *	Date:	4/2012
 *	Email:	bsdic@microsoft.com
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/reboot.h>
#include <sys/lock.h>
#include <sys/taskqueue.h>
#include <sys/sysctl.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/un.h>
#include <sys/endian.h>
#include <sys/_null.h>
#include <sys/signal.h>
#include <sys/syslog.h>
#include <sys/mutex.h>
#include <net/if_arp.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/netvsc/hv_net_vsc.h>

#include "unicode.h"
#include "hv_kvp.h"

/* hv_kvp defines */
#define BUFFERSIZE	sizeof(struct hv_kvp_msg)
#define KVP_SUCCESS	0
#define KVP_ERROR	1
#define kvp_hdr		hdr.kvp_hdr

/* hv_kvp debug control */
static int hv_kvp_log = 0;
SYSCTL_INT(_dev, OID_AUTO, hv_kvp_log, CTLFLAG_RW, &hv_kvp_log, 0,
	"hv_kvp log");

#define	hv_kvp_log_error(...)	do {				\
	if (hv_kvp_log > 0)				\
		log(LOG_ERR, "hv_kvp: " __VA_ARGS__);	\
} while (0)

#define	hv_kvp_log_info(...) do {				\
	if (hv_kvp_log > 1)				\
		log(LOG_INFO, "hv_kvp: " __VA_ARGS__);		\
} while (0)

/* character device prototypes */
static d_open_t		hv_kvp_dev_open;
static d_close_t	hv_kvp_dev_close;
static d_read_t		hv_kvp_dev_daemon_read;
static d_write_t	hv_kvp_dev_daemon_write;
static d_poll_t		hv_kvp_dev_daemon_poll;

/* hv_kvp prototypes */
static int	hv_kvp_req_in_progress(void);
static void	hv_kvp_transaction_init(uint32_t, hv_vmbus_channel *, uint64_t, uint8_t *);
static void	hv_kvp_send_msg_to_daemon(void);
static void	hv_kvp_process_request(void *context);

/* hv_kvp character device structure */
static struct cdevsw hv_kvp_cdevsw =
{
	.d_version	= D_VERSION,
	.d_open		= hv_kvp_dev_open,
	.d_close	= hv_kvp_dev_close,
	.d_read		= hv_kvp_dev_daemon_read,
	.d_write	= hv_kvp_dev_daemon_write,
	.d_poll		= hv_kvp_dev_daemon_poll,
	.d_name		= "hv_kvp_dev",
};
static struct cdev *hv_kvp_dev;
static struct hv_kvp_msg *hv_kvp_dev_buf;
struct proc *daemon_task;

/*
 * Global state to track and synchronize multiple
 * KVP transaction requests from the host.
 */
static struct {

	/* Pre-allocated work item for queue */
	hv_work_item		work_item;	

	/* Unless specified the pending mutex should be 
	 * used to alter the values of the following paramters:
	 * 1. req_in_progress
	 * 2. req_timed_out
	 * 3. pending_reqs.
	 */
	struct mtx		pending_mutex;	  
	
	/* To track if transaction is active or not */
	boolean_t		req_in_progress;    
	/* Tracks if daemon did not reply back in time */
	boolean_t		req_timed_out;	  
	/* Tracks if daemon is serving a request currently */
	boolean_t		daemon_busy;
	/* Count of KVP requests from Hyper-V. */
	uint64_t		pending_reqs;       
	
	
	/* Length of host message */
	uint32_t		host_msg_len;	    

	/* Pointer to channel */
	hv_vmbus_channel	*channelp;	    

	/* Host message id */
	uint64_t		host_msg_id;	   
	
	/* Current kvp message from the host */
	struct hv_kvp_msg	*host_kvp_msg;      
	
	 /* Current kvp message for daemon */
	struct hv_kvp_msg	daemon_kvp_msg;    
	
	/* Rcv buffer for communicating with the host*/
	uint8_t			*rcv_buf;	    
	
	/* Device semaphore to control communication */
	struct sema		dev_sema;	   
	
	/* Indicates if daemon registered with driver */
	boolean_t		register_done;      
	
	/* Character device status */
	boolean_t		dev_accessed;	    
} kvp_globals;

/* global vars */
MALLOC_DECLARE(M_HV_KVP_DEV_BUF);
MALLOC_DEFINE(M_HV_KVP_DEV_BUF, "hv_kvp_dev buffer", "buffer for hv_kvp_dev module");

/*
 * hv_kvp low level functions
 */

/*
 * Check if kvp transaction is in progres
 */
static int
hv_kvp_req_in_progress(void)
{

	return (kvp_globals.req_in_progress);
}


/*
 * This routine is called whenever a message is received from the host
 */
static void
hv_kvp_transaction_init(uint32_t rcv_len, hv_vmbus_channel *rcv_channel,
			uint64_t request_id, uint8_t *rcv_buf)
{
	
	/* Store all the relevant message details in the global structure */
	/* Do not need to use mutex for req_in_progress here */
	kvp_globals.req_in_progress = true;
	kvp_globals.host_msg_len = rcv_len;
	kvp_globals.channelp = rcv_channel;
	kvp_globals.host_msg_id = request_id;
	kvp_globals.rcv_buf = rcv_buf;
	kvp_globals.host_kvp_msg = (struct hv_kvp_msg *)&rcv_buf[
		sizeof(struct hv_vmbus_pipe_hdr) +
		sizeof(struct hv_vmbus_icmsg_hdr)];
}


/*
 * hv_kvp - version neogtiation function
 */
static void
hv_kvp_negotiate_version(struct hv_vmbus_icmsg_hdr *icmsghdrp,
			 struct hv_vmbus_icmsg_negotiate *negop,
			 uint8_t *buf)
{
	int icframe_vercnt;
	int icmsg_vercnt;

	icmsghdrp->icmsgsize = 0x10;

	negop = (struct hv_vmbus_icmsg_negotiate *)&buf[
		sizeof(struct hv_vmbus_pipe_hdr) +
		sizeof(struct hv_vmbus_icmsg_hdr)];
	icframe_vercnt = negop->icframe_vercnt;
	icmsg_vercnt = negop->icmsg_vercnt;

	/*
	 * Select the framework version number we will support
	 */
	if ((icframe_vercnt >= 2) && (negop->icversion_data[1].major == 3)) {
		icframe_vercnt = 3;
		if (icmsg_vercnt >= 2)
			icmsg_vercnt = 4;
		else
			icmsg_vercnt = 3;
	} else {
		icframe_vercnt = 1;
		icmsg_vercnt = 1;
	}

	negop->icframe_vercnt = 1;
	negop->icmsg_vercnt = 1;
	negop->icversion_data[0].major = icframe_vercnt;
	negop->icversion_data[0].minor = 0;
	negop->icversion_data[1].major = icmsg_vercnt;
	negop->icversion_data[1].minor = 0;
}


/*
 * Convert ip related info in umsg from utf8 to utf16 and store in hmsg
 */
static int
hv_kvp_convert_utf8_ipinfo_to_utf16(struct hv_kvp_msg *umsg, 
				    struct hv_kvp_ip_msg *host_ip_msg)
{
	int err_ip, err_subnet, err_gway, err_dns, err_adap;
	int UNUSED_FLAG = 1;
 		
	utf8_to_utf16((uint16_t *)host_ip_msg->kvp_ip_val.ip_addr,
	    MAX_IP_ADDR_SIZE,
	    (char *)umsg->body.kvp_ip_val.ip_addr,
	    strlen((char *)umsg->body.kvp_ip_val.ip_addr),
	    UNUSED_FLAG,
	    &err_ip);
	utf8_to_utf16((uint16_t *)host_ip_msg->kvp_ip_val.sub_net,
	    MAX_IP_ADDR_SIZE,
	    (char *)umsg->body.kvp_ip_val.sub_net,
	    strlen((char *)umsg->body.kvp_ip_val.sub_net),
	    UNUSED_FLAG,
	    &err_subnet);
	utf8_to_utf16((uint16_t *)host_ip_msg->kvp_ip_val.gate_way,
	    MAX_GATEWAY_SIZE,
	    (char *)umsg->body.kvp_ip_val.gate_way,
	    strlen((char *)umsg->body.kvp_ip_val.gate_way),
	    UNUSED_FLAG,
	    &err_gway);
	utf8_to_utf16((uint16_t *)host_ip_msg->kvp_ip_val.dns_addr,
	    MAX_IP_ADDR_SIZE,
	    (char *)umsg->body.kvp_ip_val.dns_addr,
	    strlen((char *)umsg->body.kvp_ip_val.dns_addr),
	    UNUSED_FLAG,
	    &err_dns);
	utf8_to_utf16((uint16_t *)host_ip_msg->kvp_ip_val.adapter_id,
	    MAX_IP_ADDR_SIZE,
	    (char *)umsg->body.kvp_ip_val.adapter_id,
	    strlen((char *)umsg->body.kvp_ip_val.adapter_id),
	    UNUSED_FLAG,
	    &err_adap);
	
	host_ip_msg->kvp_ip_val.dhcp_enabled = umsg->body.kvp_ip_val.dhcp_enabled;
	host_ip_msg->kvp_ip_val.addr_family = umsg->body.kvp_ip_val.addr_family;

	return (err_ip | err_subnet | err_gway | err_dns | err_adap);
}


/*
 * Convert ip related info in hmsg from utf16 to utf8 and store in umsg
 */
static int
hv_kvp_convert_utf16_ipinfo_to_utf8(struct hv_kvp_ip_msg *host_ip_msg,
				    struct hv_kvp_msg *umsg)
{
	int err_ip, err_subnet, err_gway, err_dns, err_adap;
	int UNUSED_FLAG = 1;
	int guid_index;
	struct hv_device *hv_dev;       /* GUID Data Structure */
	hn_softc_t *sc;                 /* hn softc structure  */
	char if_name[4];
	unsigned char guid_instance[40];
	char *guid_data = NULL;
	char buf[39];

	struct guid_extract {
		char	a1[2];
		char	a2[2];
		char	a3[2];
		char	a4[2];
		char	b1[2];
		char	b2[2];
		char	c1[2];
		char	c2[2];
		char	d[4];
		char	e[12];
	};

	struct guid_extract *id;
	device_t *devs;
	int devcnt;

	/* IP Address */
	utf16_to_utf8((char *)umsg->body.kvp_ip_val.ip_addr,
	    MAX_IP_ADDR_SIZE,
	    (uint16_t *)host_ip_msg->kvp_ip_val.ip_addr,
	    MAX_IP_ADDR_SIZE,
	    UNUSED_FLAG,
	    &err_ip);

	/* Adapter ID : GUID */
	utf16_to_utf8((char *)umsg->body.kvp_ip_val.adapter_id,
	    MAX_ADAPTER_ID_SIZE,
	    (uint16_t *)host_ip_msg->kvp_ip_val.adapter_id,
	    MAX_ADAPTER_ID_SIZE,
	    UNUSED_FLAG,
	    &err_adap);

	if (devclass_get_devices(devclass_find("hn"), &devs, &devcnt) == 0) {
		for (devcnt = devcnt - 1; devcnt >= 0; devcnt--) {
			sc = device_get_softc(devs[devcnt]);

			/* Trying to find GUID of Network Device */
			hv_dev = sc->hn_dev_obj;

			for (guid_index = 0; guid_index < 16; guid_index++) {
				sprintf(&guid_instance[guid_index * 2], "%02x",
				    hv_dev->device_id.data[guid_index]);
			}

			guid_data = (char *)guid_instance;
			id = (struct guid_extract *)guid_data;
			snprintf(buf, sizeof(buf), "{%.2s%.2s%.2s%.2s-%.2s%.2s-%.2s%.2s-%.4s-%s}",
			    id->a4, id->a3, id->a2, id->a1,
			    id->b2, id->b1, id->c2, id->c1, id->d, id->e);
			guid_data = NULL;
			sprintf(if_name, "%s%d", "hn", device_get_unit(devs[devcnt]));

			if (strncmp(buf, (char *)umsg->body.kvp_ip_val.adapter_id, 39) == 0) {
				strcpy((char *)umsg->body.kvp_ip_val.adapter_id, if_name);
				break;
			}
		}
		free(devs, M_TEMP);
	}

	/* Address Family , DHCP , SUBNET, Gateway, DNS */
	umsg->kvp_hdr.operation = host_ip_msg->operation;
	umsg->body.kvp_ip_val.addr_family = host_ip_msg->kvp_ip_val.addr_family;
	umsg->body.kvp_ip_val.dhcp_enabled = host_ip_msg->kvp_ip_val.dhcp_enabled;
	utf16_to_utf8((char *)umsg->body.kvp_ip_val.sub_net, MAX_IP_ADDR_SIZE,
	    (uint16_t *)host_ip_msg->kvp_ip_val.sub_net,
	    MAX_IP_ADDR_SIZE,
	    UNUSED_FLAG,
	    &err_subnet);
	
	utf16_to_utf8((char *)umsg->body.kvp_ip_val.gate_way, MAX_GATEWAY_SIZE,
	    (uint16_t *)host_ip_msg->kvp_ip_val.gate_way,
	    MAX_GATEWAY_SIZE,
	    UNUSED_FLAG,
	    &err_gway);

	utf16_to_utf8((char *)umsg->body.kvp_ip_val.dns_addr, MAX_IP_ADDR_SIZE,
	    (uint16_t *)host_ip_msg->kvp_ip_val.dns_addr,
	    MAX_IP_ADDR_SIZE,
	    UNUSED_FLAG,
	    &err_dns);

	return (err_ip | err_subnet | err_gway | err_dns | err_adap);
}


/*
 * Prepare a user kvp msg based on host kvp msg (utf16 to utf8)
 * Ensure utf16_utf8 takes care of the additional string terminating char!!
 */
static void
hv_kvp_convert_hostmsg_to_usermsg(void)
{
	int utf_err = 0;
	uint32_t value_type;
	struct hv_kvp_ip_msg *host_ip_msg = (struct hv_kvp_ip_msg *)
		kvp_globals.host_kvp_msg;

	struct hv_kvp_msg *hmsg = kvp_globals.host_kvp_msg;
	struct hv_kvp_msg *umsg = &kvp_globals.daemon_kvp_msg;

	memset(umsg, 0, sizeof(struct hv_kvp_msg));

	umsg->kvp_hdr.operation = hmsg->kvp_hdr.operation;
	umsg->kvp_hdr.pool = hmsg->kvp_hdr.pool;

	switch (umsg->kvp_hdr.operation) {
	case HV_KVP_OP_SET_IP_INFO:
		hv_kvp_convert_utf16_ipinfo_to_utf8(host_ip_msg, umsg);
		break;

	case HV_KVP_OP_GET_IP_INFO:
		utf16_to_utf8((char *)umsg->body.kvp_ip_val.adapter_id,
		    MAX_ADAPTER_ID_SIZE,
		    (uint16_t *)host_ip_msg->kvp_ip_val.adapter_id,
		    MAX_ADAPTER_ID_SIZE, 1, &utf_err);

		umsg->body.kvp_ip_val.addr_family =
		    host_ip_msg->kvp_ip_val.addr_family;
		break;

	case HV_KVP_OP_SET:
		value_type = hmsg->body.kvp_set.data.value_type;

		switch (value_type) {
		case HV_REG_SZ:
			umsg->body.kvp_set.data.value_size =
			    utf16_to_utf8(
				(char *)umsg->body.kvp_set.data.msg_value.value,
				HV_KVP_EXCHANGE_MAX_VALUE_SIZE - 1,
				(uint16_t *)hmsg->body.kvp_set.data.msg_value.value,
				hmsg->body.kvp_set.data.value_size,
				1, &utf_err);
			/* utf8 encoding */
			umsg->body.kvp_set.data.value_size =
			    umsg->body.kvp_set.data.value_size / 2;
			break;

		case HV_REG_U32:
			umsg->body.kvp_set.data.value_size =
			    sprintf(umsg->body.kvp_set.data.msg_value.value, "%d",
				hmsg->body.kvp_set.data.msg_value.value_u32) + 1;
			break;

		case HV_REG_U64:
			umsg->body.kvp_set.data.value_size =
			    sprintf(umsg->body.kvp_set.data.msg_value.value, "%llu",
				(unsigned long long)
				hmsg->body.kvp_set.data.msg_value.value_u64) + 1;
			break;
		}

		umsg->body.kvp_set.data.key_size =
		    utf16_to_utf8(
			umsg->body.kvp_set.data.key,
			HV_KVP_EXCHANGE_MAX_KEY_SIZE - 1,
			(uint16_t *)hmsg->body.kvp_set.data.key,
			hmsg->body.kvp_set.data.key_size,
			1, &utf_err);

		/* utf8 encoding */
		umsg->body.kvp_set.data.key_size =
		    umsg->body.kvp_set.data.key_size / 2;
		break;

	case HV_KVP_OP_GET:
		umsg->body.kvp_get.data.key_size =
		    utf16_to_utf8(umsg->body.kvp_get.data.key,
			HV_KVP_EXCHANGE_MAX_KEY_SIZE - 1,
			(uint16_t *)hmsg->body.kvp_get.data.key,
			hmsg->body.kvp_get.data.key_size,
			1, &utf_err);
		/* utf8 encoding */
		umsg->body.kvp_get.data.key_size =
		    umsg->body.kvp_get.data.key_size / 2;
		break;

	case HV_KVP_OP_DELETE:
		umsg->body.kvp_delete.key_size =
		    utf16_to_utf8(umsg->body.kvp_delete.key,
			HV_KVP_EXCHANGE_MAX_KEY_SIZE - 1,
			(uint16_t *)hmsg->body.kvp_delete.key,
			hmsg->body.kvp_delete.key_size,
			1, &utf_err);
		/* utf8 encoding */
		umsg->body.kvp_delete.key_size =
		    umsg->body.kvp_delete.key_size / 2;
		break;

	case HV_KVP_OP_ENUMERATE:
		umsg->body.kvp_enum_data.index =
		    hmsg->body.kvp_enum_data.index;
		break;

	default:
		hv_kvp_log_info("%s: daemon_kvp_msg: Invalid operation : %d\n",
		    __func__, umsg->kvp_hdr.operation);
	}
}


/*
 * Prepare a host kvp msg based on user kvp msg (utf8 to utf16)
 */
static int
hv_kvp_convert_usermsg_to_hostmsg(void)
{
	int hkey_len = 0, hvalue_len = 0, utf_err = 0;
	struct hv_kvp_exchg_msg_value *host_exchg_data;
	char *key_name, *value;

	struct hv_kvp_msg *umsg = &kvp_globals.daemon_kvp_msg;
	struct hv_kvp_msg *hmsg = kvp_globals.host_kvp_msg;
	struct hv_kvp_ip_msg *host_ip_msg = (struct hv_kvp_ip_msg *)hmsg;

	switch (hmsg->kvp_hdr.operation) {
	case HV_KVP_OP_GET_IP_INFO:
		return (hv_kvp_convert_utf8_ipinfo_to_utf16(umsg, host_ip_msg));

	case HV_KVP_OP_SET_IP_INFO:
	case HV_KVP_OP_SET:
	case HV_KVP_OP_DELETE:
		return (KVP_SUCCESS);

	case HV_KVP_OP_ENUMERATE:
		host_exchg_data = &hmsg->body.kvp_enum_data.data;
		key_name = umsg->body.kvp_enum_data.data.key;
		hkey_len = utf8_to_utf16((uint16_t *)host_exchg_data->key,
				((HV_KVP_EXCHANGE_MAX_KEY_SIZE / 2) - 2),
				key_name, strlen(key_name),
				1, &utf_err);
		/* utf16 encoding */
		host_exchg_data->key_size = 2 * (hkey_len + 1);
		value = umsg->body.kvp_enum_data.data.msg_value.value;
		hvalue_len = utf8_to_utf16(
				(uint16_t *)host_exchg_data->msg_value.value,
				((HV_KVP_EXCHANGE_MAX_VALUE_SIZE / 2) - 2),
				value, strlen(value),
				1, &utf_err);
		host_exchg_data->value_size = 2 * (hvalue_len + 1);
		host_exchg_data->value_type = HV_REG_SZ;

		if ((hkey_len < 0) || (hvalue_len < 0))
			return (HV_KVP_E_FAIL);
			
		return (KVP_SUCCESS);

	case HV_KVP_OP_GET:
		host_exchg_data = &hmsg->body.kvp_get.data;
		value = umsg->body.kvp_get.data.msg_value.value;
		hvalue_len = utf8_to_utf16(
				(uint16_t *)host_exchg_data->msg_value.value,
				((HV_KVP_EXCHANGE_MAX_VALUE_SIZE / 2) - 2),
				value, strlen(value),
				1, &utf_err);
		/* Convert value size to uft16 */
		host_exchg_data->value_size = 2 * (hvalue_len + 1);
		/* Use values by string */
		host_exchg_data->value_type = HV_REG_SZ;

		if ((hkey_len < 0) || (hvalue_len < 0)) 
			return (HV_KVP_E_FAIL);
			
		return (KVP_SUCCESS);

	default:
		return (HV_KVP_E_FAIL);
	}
}


/*
 * Send the response back to the host.
 */
static void
hv_kvp_respond_host(int error)
{
	struct hv_vmbus_icmsg_hdr *hv_icmsg_hdrp;

	hv_icmsg_hdrp = (struct hv_vmbus_icmsg_hdr *)
	    &kvp_globals.rcv_buf[sizeof(struct hv_vmbus_pipe_hdr)];

	if (error)
		error = HV_KVP_E_FAIL;

	hv_icmsg_hdrp->status = error;
	hv_icmsg_hdrp->icflags = HV_ICMSGHDRFLAG_TRANSACTION | HV_ICMSGHDRFLAG_RESPONSE;
	
	error = hv_vmbus_channel_send_packet(kvp_globals.channelp,
			kvp_globals.rcv_buf,
			kvp_globals.host_msg_len, kvp_globals.host_msg_id,
			HV_VMBUS_PACKET_TYPE_DATA_IN_BAND, 0);

	if (error)
		hv_kvp_log_info("%s: hv_kvp_respond_host: sendpacket error:%d\n",
			__func__, error);
}


/*
 * This is the main kvp kernel process that interacts with both user daemon
 * and the host
 */
static void
hv_kvp_send_msg_to_daemon(void)
{
	/* Prepare kvp_msg to be sent to user */
	hv_kvp_convert_hostmsg_to_usermsg();

	/* Send the msg to user via function deamon_read - setting sema */
	sema_post(&kvp_globals.dev_sema);
}


/*
 * Function to read the kvp request buffer from host
 * and interact with daemon
 */
static void
hv_kvp_process_request(void *context)
{
	uint8_t *kvp_buf;
	hv_vmbus_channel *channel = context;
	uint32_t recvlen = 0;
	uint64_t requestid;
	struct hv_vmbus_icmsg_hdr *icmsghdrp;
	int ret = 0;
	uint64_t pending_cnt = 1;
	
	hv_kvp_log_info("%s: entering hv_kvp_process_request\n", __func__);
	kvp_buf = receive_buffer[HV_KVP];
	ret = hv_vmbus_channel_recv_packet(channel, kvp_buf, 2 * PAGE_SIZE,
		&recvlen, &requestid);

	/*
	 * We start counting only after the daemon registers
	 * and therefore there could be requests pending in 
	 * the VMBus that are not reflected in pending_cnt.
	 * Therefore we continue reading as long as either of
	 * the below conditions is true.
	 */

	while ((pending_cnt>0) || ((ret == 0) && (recvlen > 0))) {

		if ((ret == 0) && (recvlen>0)) {
			
			icmsghdrp = (struct hv_vmbus_icmsg_hdr *)
					&kvp_buf[sizeof(struct hv_vmbus_pipe_hdr)];
	
			hv_kvp_transaction_init(recvlen, channel, requestid, kvp_buf);
			if (icmsghdrp->icmsgtype == HV_ICMSGTYPE_NEGOTIATE) {
				hv_kvp_negotiate_version(icmsghdrp, NULL, kvp_buf);
				hv_kvp_respond_host(ret);
					
				/*
				 * It is ok to not acquire the mutex before setting 
				 * req_in_progress here because negotiation is the
				 * first thing that happens and hence there is no
				 * chance of a race condition.
				 */
				
				kvp_globals.req_in_progress = false;
				hv_kvp_log_info("%s :version negotiated\n", __func__);

			} else {
				if (!kvp_globals.daemon_busy) {

					hv_kvp_log_info("%s: issuing qury to daemon\n", __func__);
					mtx_lock(&kvp_globals.pending_mutex);
					kvp_globals.req_timed_out = false;
					kvp_globals.daemon_busy = true;
					mtx_unlock(&kvp_globals.pending_mutex);

					hv_kvp_send_msg_to_daemon();
					hv_kvp_log_info("%s: waiting for daemon\n", __func__);
				}
				
				/* Wait 5 seconds for daemon to respond back */
				tsleep(&kvp_globals, 0, "kvpworkitem", 5 * hz);
				hv_kvp_log_info("%s: came out of wait\n", __func__);
			}
		}

		mtx_lock(&kvp_globals.pending_mutex);
		
		/* Notice that once req_timed_out is set to true
		 * it will remain true until the next request is
		 * sent to the daemon. The response from daemon
		 * is forwarded to host only when this flag is 
		 * false. 
		 */
		kvp_globals.req_timed_out = true;

		/*
		 * Cancel request if so need be.
		 */
		if (hv_kvp_req_in_progress()) {
			hv_kvp_log_info("%s: request was still active after wait so failing\n", __func__);
			hv_kvp_respond_host(HV_KVP_E_FAIL);
			kvp_globals.req_in_progress = false;	
		}
	
		/*
		* Decrement pending request count and
		*/
		if (kvp_globals.pending_reqs>0) {
			kvp_globals.pending_reqs = kvp_globals.pending_reqs - 1;
		}
		pending_cnt = kvp_globals.pending_reqs;
		
		mtx_unlock(&kvp_globals.pending_mutex);

		/*
		 * Try reading next buffer
		 */
		recvlen = 0;
		ret = hv_vmbus_channel_recv_packet(channel, kvp_buf, 2 * PAGE_SIZE,
			&recvlen, &requestid);
		hv_kvp_log_info("%s: read: context %p, pending_cnt %ju ret =%d, recvlen=%d\n",
			__func__, context, pending_cnt, ret, recvlen);
	} 
}


/*
 * Callback routine that gets called whenever there is a message from host
 */
void
hv_kvp_callback(void *context)
{
	uint64_t pending_cnt = 0;

	if (kvp_globals.register_done == false) {
		
		kvp_globals.channelp = context;
	} else {
		
		mtx_lock(&kvp_globals.pending_mutex);
		kvp_globals.pending_reqs = kvp_globals.pending_reqs + 1;
		pending_cnt = kvp_globals.pending_reqs;
		mtx_unlock(&kvp_globals.pending_mutex);
		if (pending_cnt == 1) {
			hv_kvp_log_info("%s: Queuing work item\n", __func__);
			hv_queue_work_item(
					service_table[HV_KVP].work_queue,
					hv_kvp_process_request,
					context
					);
		}
	}	
}


/*
 * This function is called by the hv_kvp_init -
 * creates character device hv_kvp_dev 
 * allocates memory to hv_kvp_dev_buf
 *
 */
static int
hv_kvp_dev_init(void)
{
	int error = 0;

	/* initialize semaphore */
	sema_init(&kvp_globals.dev_sema, 0, "hv_kvp device semaphore");
	/* create character device */
	error = make_dev_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK,
			&hv_kvp_dev,
			&hv_kvp_cdevsw,
			0,
			UID_ROOT,
			GID_WHEEL,
			0640,
			"hv_kvp_dev");
					   
	if (error != 0)
		return (error);

	/*
	 * Malloc with M_WAITOK flag will never fail.
	 */
	hv_kvp_dev_buf = malloc(sizeof(*hv_kvp_dev_buf), M_HV_KVP_DEV_BUF, M_WAITOK |
				M_ZERO);

	return (0);
}


/*
 * This function is called by the hv_kvp_deinit -
 * destroy character device
 */
static void
hv_kvp_dev_destroy(void)
{

        if (daemon_task != NULL) {
		PROC_LOCK(daemon_task);
        	kern_psignal(daemon_task, SIGKILL);
		PROC_UNLOCK(daemon_task);
	}
	
	destroy_dev(hv_kvp_dev);
	free(hv_kvp_dev_buf, M_HV_KVP_DEV_BUF);
	return;
}


static int
hv_kvp_dev_open(struct cdev *dev, int oflags, int devtype,
				struct thread *td)
{
	
	hv_kvp_log_info("%s: Opened device \"hv_kvp_device\" successfully.\n", __func__);
	if (kvp_globals.dev_accessed)
		return (-EBUSY);
	
	daemon_task = curproc;
	kvp_globals.dev_accessed = true;
	kvp_globals.daemon_busy = false;
	return (0);
}


static int
hv_kvp_dev_close(struct cdev *dev __unused, int fflag __unused, int devtype __unused,
				 struct thread *td __unused)
{

	hv_kvp_log_info("%s: Closing device \"hv_kvp_device\".\n", __func__);
	kvp_globals.dev_accessed = false;
	kvp_globals.register_done = false;
	return (0);
}


/*
 * hv_kvp_daemon read invokes this function
 * acts as a send to daemon
 */
static int
hv_kvp_dev_daemon_read(struct cdev *dev __unused, struct uio *uio, int ioflag __unused)
{
	size_t amt;
	int error = 0;

	/* Check hv_kvp daemon registration status*/
	if (!kvp_globals.register_done)
		return (KVP_ERROR);

	sema_wait(&kvp_globals.dev_sema);

	memcpy(hv_kvp_dev_buf, &kvp_globals.daemon_kvp_msg, sizeof(struct hv_kvp_msg));

	amt = MIN(uio->uio_resid, uio->uio_offset >= BUFFERSIZE + 1 ? 0 :
		BUFFERSIZE + 1 - uio->uio_offset);

	if ((error = uiomove(hv_kvp_dev_buf, amt, uio)) != 0)
		hv_kvp_log_info("%s: hv_kvp uiomove read failed!\n", __func__);

	return (error);
}


/*
 * hv_kvp_daemon write invokes this function
 * acts as a recieve from daemon
 */
static int
hv_kvp_dev_daemon_write(struct cdev *dev __unused, struct uio *uio, int ioflag __unused)
{
	size_t amt;
	int error = 0;

	uio->uio_offset = 0;

	amt = MIN(uio->uio_resid, BUFFERSIZE);
	error = uiomove(hv_kvp_dev_buf, amt, uio);

	if (error != 0)
		return (error);

	memcpy(&kvp_globals.daemon_kvp_msg, hv_kvp_dev_buf, sizeof(struct hv_kvp_msg));

	if (kvp_globals.register_done == false) {
		if (kvp_globals.daemon_kvp_msg.kvp_hdr.operation == HV_KVP_OP_REGISTER) {

			kvp_globals.register_done = true;
			if (kvp_globals.channelp) {
			
				hv_kvp_callback(kvp_globals.channelp);
			}
		}
		else {
			hv_kvp_log_info("%s, KVP Registration Failed\n", __func__);
			return (KVP_ERROR);
		}
	} else {

		mtx_lock(&kvp_globals.pending_mutex);

		if(!kvp_globals.req_timed_out) {

			hv_kvp_convert_usermsg_to_hostmsg();
			hv_kvp_respond_host(KVP_SUCCESS);
			wakeup(&kvp_globals);
			kvp_globals.req_in_progress = false;
		}

		kvp_globals.daemon_busy = false;
		mtx_unlock(&kvp_globals.pending_mutex);
	}

	return (error);
}


/*
 * hv_kvp_daemon poll invokes this function to check if data is available
 * for daemon to read.
 */
static int
hv_kvp_dev_daemon_poll(struct cdev *dev __unused, int events, struct thread *td  __unused)
{
	int revents = 0;

	mtx_lock(&kvp_globals.pending_mutex);
	/*
	 * We check global flag daemon_busy for the data availiability for
	 * userland to read. Deamon_busy is set to true before driver has data
	 * for daemon to read. It is set to false after daemon sends
	 * then response back to driver.
	 */
	if (kvp_globals.daemon_busy == true)
		revents = POLLIN;
	mtx_unlock(&kvp_globals.pending_mutex);

	return (revents);
}


/* 
 * hv_kvp initialization function 
 * called from hv_util service.
 *
 */
int
hv_kvp_init(hv_vmbus_service *srv)
{
	int error = 0;
	hv_work_queue *work_queue = NULL;
	
	memset(&kvp_globals, 0, sizeof(kvp_globals));

	work_queue = hv_work_queue_create("KVP Service");
	if (work_queue == NULL) {
		hv_kvp_log_info("%s: Work queue alloc failed\n", __func__);
		error = ENOMEM;
		hv_kvp_log_error("%s: ENOMEM\n", __func__);
		goto Finish;
	}
	srv->work_queue = work_queue;

	error = hv_kvp_dev_init();
	mtx_init(&kvp_globals.pending_mutex, "hv-kvp pending mutex",
		       	NULL, MTX_DEF);	
	kvp_globals.pending_reqs = 0;


Finish:
	return (error);
}


void
hv_kvp_deinit(void)
{
	hv_kvp_dev_destroy();
	mtx_destroy(&kvp_globals.pending_mutex);

	return;
}
