/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
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

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Dieter Baron.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

 /* An implementation of key value pair (KVP) functionality for FreeBSD */ 

/**
 * Code for handling all KVP related messages 
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/socket.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/un.h>
#include <sys/endian.h>

#include <net/if_arp.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/netvsc/hv_net_vsc.h>
#include "hv_kvp.h"

/* Unicode Conversions */
#include <sys/cdefs.h>
#include <sys/_null.h>

static int hv_kvp_daemon_ack = 0;
SYSCTL_INT(_dev, OID_AUTO, hv_kvp_daemon_ack, CTLFLAG_RW, &hv_kvp_daemon_ack,
		0, "First ack when daemon is ready");

static size_t convert8_to_16(uint16_t *, size_t, const char *, size_t, int *);
static size_t convert16_to_8(char *, size_t, const uint16_t *, size_t, int *);

/* Unicode declarations ends */
#define KVP_SUCCESS	0
#define kvp_hdr hdr.kvp_hdr
typedef struct hv_kvp_msg hv_kvp_bsd_msg;

static int hv_kvp_ready(void);
static int hv_kvp_transaction_active(void);
static void hv_kvp_transaction_init(uint32_t, hv_vmbus_channel *, uint64_t,
    uint8_t *);
static void hv_kvp_conn_register(void);
static void hv_kvp_process_msg(void *p);

/*
 * We maintain a global state, assuming only one transaction can be active
 * at any point in time.
 * Inited by the kvp callback routine (utils file) when a valid message is
 * received from the host;
 */
static struct {
	boolean_t kvp_ready; /* indicates if kvp module is ready or not */
	boolean_t in_progress; /* transaction status - active or not */
	uint32_t host_msg_len; /* length of host message */
	hv_vmbus_channel *channelp; /* pointer to channel */
	uint64_t host_msg_id; /* host message id */
	hv_kvp_bsd_msg  *host_kvp_msg; /* current message from the host */
	uint8_t *rcv_buf; /* rcv buffer for communicating with the host*/
} kvp_msg_state;


/* We use an alternative, more convenient representation in the generator. */

/* 
 * Data Buffer used by kernel for to/from communication with user daemon 
 */
static hv_kvp_bsd_msg  hv_user_kvp_msg; 

static boolean_t conn_ready; /* indicates if connection to daemon done */
static boolean_t register_done; /* indicates daemon registered with driver */

/* Original socket created during connection establishment */
static int sock_fd;

/* Handle to KVP device */ 
struct hv_device* kvp_hv_dev;

/*
 * Check if kvp routines are ready to receive and respond
 */
static int 
hv_kvp_ready(void)
{

	return (kvp_msg_state.kvp_ready);
}

/*
 * Check if kvp transaction is in progres
 */
static int 
hv_kvp_transaction_active(void)
{

	return (kvp_msg_state.in_progress);
}


/*
 * This routine is called whenever a message is received from the host
 */
static void 
hv_kvp_transaction_init(uint32_t rcv_len, hv_vmbus_channel *rcv_channel, 
			uint64_t request_id, uint8_t *rcv_buf)
{

	/*
	 * Store all the relevant message details in the global structure
	 */
	kvp_msg_state.in_progress = TRUE;
	kvp_msg_state.host_msg_len = rcv_len;
	kvp_msg_state.channelp = rcv_channel;
	kvp_msg_state.host_msg_id = request_id;
	kvp_msg_state.rcv_buf = rcv_buf;
	kvp_msg_state.host_kvp_msg = (hv_kvp_bsd_msg *)&rcv_buf[
	    sizeof(struct hv_vmbus_pipe_hdr) +
            sizeof(struct hv_vmbus_icmsg_hdr)];
}

static void
hv_kvp_negotiate_version(struct hv_vmbus_icmsg_hdr *icmsghdrp,
    struct hv_vmbus_icmsg_negotiate *negop, uint8_t *buf)
{
        int icframe_vercnt;
        int icmsg_vercnt;

	if (bootverbose)
		printf("hv_kvp_negotiate_version\n");

        icmsghdrp->icmsgsize = 0x10;

        negop = (struct hv_vmbus_icmsg_negotiate *) &buf[
            sizeof(struct hv_vmbus_pipe_hdr) +
            sizeof(struct hv_vmbus_icmsg_hdr)];
        icframe_vercnt = negop->icframe_vercnt;
        icmsg_vercnt = negop->icmsg_vercnt;

	/*
	 * Select the framework version number we will
	 * support.
	 */
	if ((icframe_vercnt >= 2) && (negop->icversion_data[1].major == 3)) {
		icframe_vercnt = 3;
	 	if (icmsg_vercnt >=2) {
			icmsg_vercnt = 4;
		} else {
			icmsg_vercnt = 3;
		}
	} else {
		icframe_vercnt = 1;
		icmsg_vercnt = 1; 			
	}

        /*
	 * Respond with the maximum framework and service
	 * version numbers we can support.
	 */
        negop->icframe_vercnt = 1;
        negop->icmsg_vercnt = 1;
        negop->icversion_data[0].major = icframe_vercnt;
        negop->icversion_data[0].minor = 0;
        negop->icversion_data[1].major = icmsg_vercnt;
        negop->icversion_data[1].minor = 0;
}

/*
 * Establish a UNIX socket connection with user daemon 
 */
static int
kvp_connect_user(void)
{
	int sock_error;
	struct socket_args unix_sock;
	struct sockaddr_un sock_sun;
	struct thread *thread_ptr;

	thread_ptr = curthread;

	/* Open a Unix Domain socket */
	unix_sock.domain = AF_UNIX;
	unix_sock.type = SOCK_STREAM;
	unix_sock.protocol = 0;
	sock_error = sys_socket(thread_ptr, &unix_sock);
	if (sock_error) {
                return sock_error;
	}

	/* Try to connect to user daemon using Unix socket */
	sock_fd = thread_ptr->td_retval[0];
	sock_sun.sun_family = AF_UNIX;
	strcpy(sock_sun.sun_path, BSD_SOC_PATH);
	sock_sun.sun_len = sizeof(struct sockaddr_un) - 
	    sizeof(sock_sun.sun_path) + strlen(sock_sun.sun_path) + 1;

	sock_error = kern_connect(thread_ptr, sock_fd, 
	    (struct sockaddr *) &sock_sun);
	if (sock_error) {
                kern_close(thread_ptr, sock_fd);
	}

	return (sock_error);
}


/*
 * Send kvp msg on the established unix socket connection to the user
 */
static int
kvp_send_user(void)
{
	int send_fd, send_error;
	struct uio send_uio;
	struct iovec send_iovec;
	struct thread *thread_ptr = curthread;

	if (!hv_kvp_ready()) {
		hv_kvp_conn_register();
	}

	send_fd = sock_fd;
	memset(&send_uio, 0, sizeof(struct uio));
	
	send_iovec.iov_base = (void *)&hv_user_kvp_msg;
	send_iovec.iov_len = sizeof(hv_kvp_bsd_msg);
	send_uio.uio_iov = &send_iovec;
	send_uio.uio_iovcnt = 1;
	send_uio.uio_resid = send_iovec.iov_len;
	send_uio.uio_segflg = UIO_SYSSPACE;
	send_error = kern_writev(thread_ptr, send_fd, &send_uio);

	return (send_error);
}


/*
 * Receive kvp msg on the established unix socket connection from the user
 */
static int
kvp_rcv_user(void)
{
	int rcv_fd, rcv_error=0;
	struct uio rcv_uio;
	struct iovec rcv_iovec;
	struct thread *thread_ptr = curthread;

	rcv_fd = sock_fd;

	memset(&rcv_uio, 0, sizeof(struct uio)); 	

	rcv_iovec.iov_base = (void *)&hv_user_kvp_msg;
	rcv_iovec.iov_len = sizeof(hv_kvp_bsd_msg);
	rcv_uio.uio_iov = &rcv_iovec;
	rcv_uio.uio_iovcnt = 1;
	rcv_uio.uio_resid = rcv_iovec.iov_len;
	rcv_uio.uio_segflg = UIO_SYSSPACE;
	rcv_error = kern_readv(thread_ptr, rcv_fd, &rcv_uio);

	return (rcv_error);
}

/*
 * Converts utf8 to utf16
 */
static size_t
convert8_to_16(uint16_t *dst, size_t dst_len,const char *src, size_t src_len,
   int *errp)
{
    const unsigned char *s;
    size_t spos, dpos;
    int error, flags = 1;
    uint16_t c;
#define IS_CONT(c)      (((c)&0xc0) == 0x80)

    error = 0;
    s = (const unsigned char *)src;
    spos = dpos = 0;
 
    while (spos<src_len) {
        if (s[spos] < 0x80)
            c = s[spos++];
        else if ((flags & 0x03)
                 && (spos >= src_len || !IS_CONT(s[spos+1]))
                 && s[spos]>=0xa0) {
            /* not valid UTF-8, assume ISO 8859-1 */
            c = s[spos++];
        }
        else if (s[spos] < 0xc0 || s[spos] >= 0xf5) {
            /* continuation byte without lead byte
               or lead byte for codepoint above 0x10ffff */
            error++;
            spos++;
            continue;
        }
        else if (s[spos] < 0xe0) {
            if (spos >= src_len || !IS_CONT(s[spos+1])) {
                spos++;
                error++;
                continue;
            }
            c = ((s[spos] & 0x3f) << 6) | (s[spos+1] & 0x3f);
            spos += 2;
            if (c < 0x80) {
                /* overlong encoding */
                error++;
                continue;
		}
        }
        else if (s[spos] < 0xf0) {
            if (spos >= src_len-2
                || !IS_CONT(s[spos+1]) || !IS_CONT(s[spos+2])) {
                spos++;
                error++;
                continue;
            }
            c = ((s[spos] & 0x0f) << 12) | ((s[spos+1] & 0x3f) << 6)
                | (s[spos+2] & 0x3f);
            spos += 3;
            if (c < 0x800 || (c & 0xdf00) == 0xd800 ) {
                /* overlong encoding or encoded surrogate */
                error++;
                continue;
            }
        }
        else {
            uint32_t cc;
            /* UTF-16 surrogate pair */

            if (spos >= src_len-3 || !IS_CONT(s[spos+1])
                || !IS_CONT(s[spos+2]) || !IS_CONT(s[spos+3])) {
                spos++;
                error++;

                continue;
            }
            cc = ((s[spos] & 0x03) << 18) | ((s[spos+1] & 0x3f) << 12)
| ((s[spos+2] & 0x3f) << 6) | (s[spos+3] & 0x3f);
            spos += 4;
            if (cc < 0x10000) {
                /* overlong encoding */
                error++;
                continue;
            }
            if (dst && dpos < dst_len)
                dst[dpos] = (0xd800 | ((cc-0x10000)>>10));
            dpos++;
            c = 0xdc00 | ((cc-0x10000) & 0x3ffff);
        }

        if (dst && dpos < dst_len)
            dst[dpos] = c;
        dpos++;
    }

    if (errp)
       *errp = error;

    return dpos;

#undef IS_CONT
}

/*
 * Converts utf16 to utf8
*/
static size_t
convert16_to_8(char *dst, size_t dst_len, const uint16_t *src, size_t src_len,
    int *errp)
{
    uint16_t spos, dpos;
    int error;
#define CHECK_LENGTH(l) (dpos > dst_len-(l) ? dst=NULL : NULL)
#define ADD_BYTE(b)     (dst ? dst[dpos] = (b) : 0, dpos++)

    error = 0;
    dpos = 0;
 for (spos=0; spos<src_len; spos++) {
	 if (src[spos] < 0x80) {
            CHECK_LENGTH(1);
            ADD_BYTE(src[spos]);
        }
        else if (src[spos] < 0x800) {
            CHECK_LENGTH(2);
            ADD_BYTE(0xc0 | (src[spos]>>6));
            ADD_BYTE(0x80 | (src[spos] & 0x3f));
        }
        else if ((src[spos] & 0xdc00) == 0xd800) {
            uint32_t c;
            /* first surrogate */
            if (spos == src_len - 1 || (src[spos] & 0xdc00) != 0xdc00) {
                /* no second surrogate present */
                error++;
                continue;
            }
            spos++;
            CHECK_LENGTH(4);
            c = (((src[spos]&0x3ff) << 10) | (src[spos+1]&0x3ff)) + 0x10000;
            ADD_BYTE(0xf0 | (c>>18));
            ADD_BYTE(0x80 | ((c>>12) & 0x3f));
            ADD_BYTE(0x80 | ((c>>6) & 0x3f));
            ADD_BYTE(0x80 | (c & 0x3f));
        }
        else if ((src[spos] & 0xdc00) == 0xdc00) {
            /* second surrogate without preceding first surrogate */
            error++;
        }
        else {
            CHECK_LENGTH(3);
            ADD_BYTE(0xe0 | src[spos]>>12);
            ADD_BYTE(0x80 | ((src[spos]>>6) & 0x3f));
            ADD_BYTE(0x80 | (src[spos] & 0x3f));
        }
    }

    if (errp)
 *errp = error;
  return dpos;

#undef ADD_BYTE
#undef CHECK_LENGTH
}


/*
 * Convert ip related info in umsg from utf8 to utf16 and store in hmsg
 */
static int 
ipinfo_utf8_utf16(hv_kvp_bsd_msg *umsg, struct hv_kvp_ip_msg *host_ip_msg)
{
        int err_ip, err_subnet, err_gway, err_dns, err_adap;

	size_t len=0;
        len = convert8_to_16((uint16_t *)host_ip_msg->kvp_ip_val.ip_addr,
			MAX_IP_ADDR_SIZE,
                        (char *)umsg->body.kvp_ip_val.ip_addr,
                        strlen((char *)umsg->body.kvp_ip_val.ip_addr),
			&err_ip);
        len = convert8_to_16((uint16_t *)host_ip_msg->kvp_ip_val.sub_net,
                        MAX_IP_ADDR_SIZE,
                        (char *)umsg->body.kvp_ip_val.sub_net,
                        strlen((char *)umsg->body.kvp_ip_val.sub_net),
                        &err_subnet);
        len = convert8_to_16((uint16_t *)host_ip_msg->kvp_ip_val.gate_way,
                        MAX_GATEWAY_SIZE,
                        (char *)umsg->body.kvp_ip_val.gate_way,
                        strlen((char *)umsg->body.kvp_ip_val.gate_way),
                        &err_gway);
        len = convert8_to_16((uint16_t *)host_ip_msg->kvp_ip_val.dns_addr,
                        MAX_IP_ADDR_SIZE,
                        (char *)umsg->body.kvp_ip_val.dns_addr,
                        strlen((char *)umsg->body.kvp_ip_val.dns_addr),
                        &err_dns);
        len = convert8_to_16((uint16_t *)host_ip_msg->kvp_ip_val.adapter_id,
                        MAX_IP_ADDR_SIZE,
                        (char *)umsg->body.kvp_ip_val.adapter_id,
                        strlen((char *)umsg->body.kvp_ip_val.adapter_id),
                        &err_adap);
        host_ip_msg->kvp_ip_val.dhcp_enabled =
	    umsg->body.kvp_ip_val.dhcp_enabled;
        host_ip_msg->kvp_ip_val.addr_family =
	    umsg->body.kvp_ip_val.addr_family;

        return (err_ip | err_subnet | err_gway | err_dns | err_adap);
}

/* 
 * Convert ip related info in hmsg from utf16 to utf8 and store in umsg
 */
static int 
ipinfo_utf16_utf8(struct hv_kvp_ip_msg *host_ip_msg, hv_kvp_bsd_msg *umsg)
{
	int err_ip, err_subnet, err_gway, err_dns, err_adap;
	int j;
 	struct hv_device *hv_dev;	/* GUID Data Structure */
     	hn_softc_t *sc;			/* hn softc structure  */ 
	char if_name[4];	
	unsigned char guid_instance[40];
	char* guid_data = NULL;
	char buf[39];			 
	int len=16;
	struct guid_extract {
        	char a1[2];
        	char a2[2];
        	char a3[2];
        	char a4[2];
        	char b1[2];
        	char b2[2];
        	char c1[2];
        	char c2[2];
        	char d[4];
        	char e[12];
	} *id;
	device_t *devs;
	int devcnt;

	/* IP Address */
	len = convert16_to_8((char *)umsg->body.kvp_ip_val.ip_addr, 
			     MAX_IP_ADDR_SIZE,
     		             (uint16_t *)host_ip_msg->kvp_ip_val.ip_addr,
	                     MAX_IP_ADDR_SIZE,  &err_ip);

	/* Adapter ID : GUID */
	len = convert16_to_8((char *)umsg->body.kvp_ip_val.adapter_id,
			     MAX_ADAPTER_ID_SIZE,
			     (uint16_t *)host_ip_msg->kvp_ip_val.adapter_id,
			     MAX_ADAPTER_ID_SIZE,  &err_adap);

	if (devclass_get_devices(devclass_find("hn"), &devs, &devcnt) == 0) {
		for (devcnt = devcnt - 1; devcnt >= 0; devcnt--) {
			sc = device_get_softc( devs[devcnt] );
	
			/* Trying to find GUID of Network Device */
 	    		hv_dev = sc->hn_dev_obj;

			for (j = 0; j < 16 ; j++) {
	           		sprintf(&guid_instance[j * 2], "%02x",
       		   		hv_dev->device_id.data[j]);
			}
			
			guid_data =(char *) guid_instance;
			id = (struct guid_extract *)guid_data;
			snprintf(buf, sizeof(buf) ,
			    "{%.2s%.2s%.2s%.2s-%.2s%.2s-%.2s%.2s-%.4s-%s}",
			    id->a4,id->a3,id->a2,id->a1, id->b2, id->b1,
			    id->c2, id->c1,id->d,id->e);
			guid_data = NULL;
			sprintf(if_name, "%s%d", "hn",
			    device_get_unit(devs[devcnt]));

			/*
			 * XXX Need to implement multiple network adapter
			 * handler
			 */
			if (strncmp(buf,
			    (char *)umsg->body.kvp_ip_val.adapter_id,39) == 
			    0) {
				/* Pass interface Name */
				strcpy((char *)umsg->body.kvp_ip_val.adapter_id,if_name);
				break;
			} else {
				/* XXX Implement safe exit */
			}
		}
		free( devs, M_TEMP );
	}

	/* Address Family , DHCP , SUBNET, Gateway, DNS */ 
	umsg->kvp_hdr.operation = host_ip_msg->operation;
	umsg->body.kvp_ip_val.addr_family =
	    host_ip_msg->kvp_ip_val.addr_family;
        umsg->body.kvp_ip_val.dhcp_enabled =
	    host_ip_msg->kvp_ip_val.dhcp_enabled;
	convert16_to_8((char *)umsg->body.kvp_ip_val.sub_net, MAX_IP_ADDR_SIZE,
                        (uint16_t *)host_ip_msg->kvp_ip_val.sub_net,
                        MAX_IP_ADDR_SIZE,  &err_subnet);
	convert16_to_8((char *)umsg->body.kvp_ip_val.gate_way,
		       MAX_GATEWAY_SIZE,
		       (uint16_t *)host_ip_msg->kvp_ip_val.gate_way,
		       MAX_GATEWAY_SIZE,  &err_gway);
       convert16_to_8((char *)umsg->body.kvp_ip_val.dns_addr, MAX_IP_ADDR_SIZE,
		      (uint16_t *)host_ip_msg->kvp_ip_val.dns_addr,
		      MAX_IP_ADDR_SIZE,  &err_dns);

       return (err_ip | err_subnet | err_gway | err_dns | err_adap);
}


/*
 * Prepare a user kvp msg based on host kvp msg (utf16 to utf8)
 * Ensure utf16_utf8 takes care of the additional string terminating char!!
 */
static void
host_user_kvp_msg(void)
{
	int utf_err=0;
	uint32_t value_type;
	struct hv_kvp_ip_msg *host_ip_msg = (struct hv_kvp_ip_msg *)kvp_msg_state.host_kvp_msg;
	hv_kvp_bsd_msg *hmsg = kvp_msg_state.host_kvp_msg;
	hv_kvp_bsd_msg *umsg = &hv_user_kvp_msg;
	umsg->kvp_hdr.operation = hmsg->kvp_hdr.operation;
	umsg->kvp_hdr.pool = hmsg->kvp_hdr.pool;
	
	switch (umsg->kvp_hdr.operation) {
	case HV_KVP_OP_SET_IP_INFO:
		ipinfo_utf16_utf8(host_ip_msg, umsg);
		break;
	case HV_KVP_OP_GET_IP_INFO:		
        	convert16_to_8((char *)umsg->body.kvp_ip_val.adapter_id,
			MAX_ADAPTER_ID_SIZE,
                        (uint16_t *)host_ip_msg->kvp_ip_val.adapter_id,
                        MAX_ADAPTER_ID_SIZE,  &utf_err);
        	umsg->body.kvp_ip_val.addr_family = 
		    host_ip_msg->kvp_ip_val.addr_family;
		break;
	case HV_KVP_OP_SET:
		value_type = hmsg->body.kvp_set.data.value_type;
		switch (value_type) {
		case HV_REG_SZ:
			umsg->body.kvp_set.data.value_size =
				convert16_to_8(
				(char *)umsg->body.kvp_set.data.msg_value.value,
				HV_KVP_EXCHANGE_MAX_VALUE_SIZE - 1,
				(uint16_t *)hmsg->body.kvp_set.data.msg_value.value,
				hmsg->body.kvp_set.data.value_size,
				&utf_err);
			/* utf8 encoding */
			umsg->body.kvp_set.data.value_size = 
				umsg->body.kvp_set.data.value_size/2;
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
			convert16_to_8(
				umsg->body.kvp_set.data.key,
				HV_KVP_EXCHANGE_MAX_KEY_SIZE - 1,
				(uint16_t *)hmsg->body.kvp_set.data.key,
				hmsg->body.kvp_set.data.key_size,
				&utf_err);

		/* utf8 encoding */
		umsg->body.kvp_set.data.key_size = 
			umsg->body.kvp_set.data.key_size/2;
		break;
	case HV_KVP_OP_GET:
		umsg->body.kvp_get.data.key_size =
			convert16_to_8(umsg->body.kvp_get.data.key,
			HV_KVP_EXCHANGE_MAX_KEY_SIZE - 1,
			(uint16_t *)hmsg->body.kvp_get.data.key,
			hmsg->body.kvp_get.data.key_size,
			 &utf_err);
		/* utf8 encoding */
		umsg->body.kvp_get.data.key_size = 
			umsg->body.kvp_get.data.key_size/2;
			break;
	case HV_KVP_OP_DELETE:
		umsg->body.kvp_delete.key_size =
			convert16_to_8(umsg->body.kvp_delete.key,
			HV_KVP_EXCHANGE_MAX_KEY_SIZE - 1,
			(uint16_t *)hmsg->body.kvp_delete.key,
			hmsg->body.kvp_delete.key_size,
			 &utf_err);
		/* utf8 encoding */
		umsg->body.kvp_delete.key_size = 
			umsg->body.kvp_delete.key_size/2; 
			break;
	case HV_KVP_OP_ENUMERATE:
		umsg->body.kvp_enum_data.index =
			hmsg->body.kvp_enum_data.index;
			break;
	default:
		printf("host_user_kvp_msg: Invalid operation : %d\n", 
			umsg->kvp_hdr.operation);
	}
}

/* 
 * Prepare a host kvp msg based on user kvp msg (utf8 to utf16)
 */
static int
user_host_kvp_msg(void)
{
	int hkey_len=0, hvalue_len=0, utf_err=0;
	struct hv_kvp_exchg_msg_value  *host_exchg_data;
	char *key_name, *value;
	hv_kvp_bsd_msg *umsg = &hv_user_kvp_msg;
	hv_kvp_bsd_msg *hmsg = kvp_msg_state.host_kvp_msg;
	struct hv_kvp_ip_msg *host_ip_msg = (struct hv_kvp_ip_msg *)hmsg;
 
	switch (kvp_msg_state.host_kvp_msg->kvp_hdr.operation) {
	case HV_KVP_OP_GET_IP_INFO:
		return(ipinfo_utf8_utf16(umsg, host_ip_msg));

	case HV_KVP_OP_SET_IP_INFO:
	case HV_KVP_OP_SET:
	case HV_KVP_OP_DELETE:
		return (KVP_SUCCESS);

	case HV_KVP_OP_ENUMERATE:
		host_exchg_data = &hmsg->body.kvp_enum_data.data; 
		key_name = umsg->body.kvp_enum_data.data.key;
		hkey_len = convert8_to_16((uint16_t *) host_exchg_data->key,
				((HV_KVP_EXCHANGE_MAX_KEY_SIZE / 2) - 2),
				key_name, strlen(key_name), 
				 &utf_err);
		/* utf16 encoding */
		host_exchg_data->key_size = 2*(hkey_len + 1); 
		value = umsg->body.kvp_enum_data.data.msg_value.value;	
		hvalue_len = 
		convert8_to_16( (uint16_t *)host_exchg_data->msg_value.value,
				( (HV_KVP_EXCHANGE_MAX_VALUE_SIZE / 2) - 2),
				value, strlen(value),
				 &utf_err);
		host_exchg_data->value_size = 2 * (hvalue_len + 1);
		host_exchg_data->value_type = HV_REG_SZ;

		if ((hkey_len < 0) || (hvalue_len < 0)) return(HV_KVP_E_FAIL);
		return (KVP_SUCCESS);

	case HV_KVP_OP_GET:
		host_exchg_data = &hmsg->body.kvp_get.data;
		value = umsg->body.kvp_get.data.msg_value.value;
		hvalue_len = convert8_to_16(
				(uint16_t *) host_exchg_data->msg_value.value,
				((HV_KVP_EXCHANGE_MAX_VALUE_SIZE / 2) - 2),
				value, strlen(value), 
				 &utf_err);
		/* Convert value size to uft16 */
		host_exchg_data->value_size = 2*(hvalue_len + 1); 
		/* Use values by string */
		host_exchg_data->value_type = HV_REG_SZ; 
		
		if ((hkey_len < 0) || (hvalue_len < 0)) return(HV_KVP_E_FAIL);
		return (KVP_SUCCESS);
	default:
		return (HV_KVP_E_FAIL);
	}
}


/*
 * Send the response back to the host.
 */
static void
kvp_respond_host(int error)
{
	struct hv_vmbus_icmsg_hdr *hv_icmsg_hdrp;

	if (!hv_kvp_transaction_active()) {
		/* XXX Triage why we are here */
		goto finish;
	}

	hv_icmsg_hdrp = (struct hv_vmbus_icmsg_hdr *)
	    &kvp_msg_state.rcv_buf[sizeof(struct hv_vmbus_pipe_hdr)];

	if (error) error = HV_KVP_E_FAIL;
	hv_icmsg_hdrp->status = error;
	hv_icmsg_hdrp->icflags = 
	     HV_ICMSGHDRFLAG_TRANSACTION | HV_ICMSGHDRFLAG_RESPONSE;

	error = hv_vmbus_channel_send_packet(kvp_msg_state.channelp, 
			kvp_msg_state.rcv_buf, 
			kvp_msg_state.host_msg_len, kvp_msg_state.host_msg_id,
			HV_VMBUS_PACKET_TYPE_DATA_IN_BAND, 0);

	if (error) {
		printf("kvp_respond_host: sendpacket error:%d\n", error);
	}

	/* Now ready to process another transaction */
	kvp_msg_state.in_progress = FALSE;

finish:
	return;
}

/* 
 * Initiate a connection and receive REGISTER message from the user daemon
 */
static void
hv_kvp_conn_register()
{
	int error = KVP_SUCCESS;

	if (conn_ready == FALSE) {		
		/* Wait until the user daemon is ready */
		if (!hv_kvp_daemon_ack) {
			return;
		}
		
		if (kvp_connect_user() != KVP_SUCCESS) {
			return;
		} else {
			conn_ready = TRUE;
		}
	}

	/*
	 * First message from the user should be a HV_KVP_OP_REGISTER msg
	 */
	if (register_done == FALSE) {
		error = kvp_rcv_user();	/* receive a message from user */

		if (hv_user_kvp_msg.kvp_hdr.operation == HV_KVP_OP_REGISTER) {
			register_done = TRUE;
			kvp_msg_state.kvp_ready = TRUE;
		}
	}
}


/**
 * This is the main kvp kernel process that interacts with both user daemon 
 * and the host 
 */
static void
hv_kvp_process_msg(void *p)
{
	int error = KVP_SUCCESS; 

	/* Prepare kvp_msg to be sent to user */
	host_user_kvp_msg(); 

	/* Send msg to user on Unix Socket */
	error = kvp_send_user();

	if (error != KVP_SUCCESS) {
		if (error == EPIPE) {
			conn_ready = FALSE;
			register_done = FALSE;
			kvp_msg_state.kvp_ready = FALSE;
		}
       	
		kvp_respond_host(HV_KVP_E_FAIL);
		return;
	}

	/* Rcv response from user on Unix Socket */
	hv_user_kvp_msg.hdr.error = HV_KVP_E_FAIL;
	error = kvp_rcv_user();

	if ((error == KVP_SUCCESS) && 
	    (hv_user_kvp_msg.hdr.error != HV_KVP_E_FAIL)) {
		/* Convert user kvp to host kvp and then respond */
		error = user_host_kvp_msg();

		if (error != KVP_SUCCESS) {
			kvp_respond_host(HV_KVP_E_FAIL);
		} else {
			kvp_respond_host(hv_user_kvp_msg.hdr.error);
		} 
	} else {
		if (error == EPIPE) {
			conn_ready = FALSE;
			register_done = FALSE;
			kvp_msg_state.kvp_ready = FALSE;
		}
		kvp_respond_host(HV_KVP_E_FAIL);
	}
}

/* 
 * Callback routine that gets called whenever there is a message from host
 */
void
hv_kvp_callback(void *context)
{
	uint8_t*		kvp_buf;
	hv_vmbus_channel*	channel = context;
	uint32_t		recvlen;
	uint64_t		requestid;
	int			ret = 0;
	struct hv_vmbus_icmsg_hdr *icmsghdrp;

	kvp_buf = receive_buffer[HV_KVP];

	/*
	 * Check if already one transaction is under process
	 */
	if (!hv_kvp_transaction_active()) {
		ret = hv_vmbus_channel_recv_packet(channel, kvp_buf,
		    2 * PAGE_SIZE, &recvlen, &requestid);

		if ((ret == 0) && (recvlen > 0)) {
			icmsghdrp = (struct hv_vmbus_icmsg_hdr *)
			    &kvp_buf[sizeof(struct hv_vmbus_pipe_hdr)];

			hv_kvp_transaction_init(recvlen, channel,requestid,
			    kvp_buf);

			if (icmsghdrp->icmsgtype == HV_ICMSGTYPE_NEGOTIATE) {
				hv_kvp_negotiate_version(icmsghdrp, NULL,
				    kvp_buf);
				kvp_respond_host(ret);
			} else {
				/*
				 * Queue packet for processing.
				 */
				hv_queue_work_item(
				    service_table[HV_KVP].work_queue,
				    hv_kvp_process_msg,
				    NULL);
			}
		} else {
			ret = HV_KVP_E_FAIL;
		}
	} else {
		ret = HV_KVP_E_FAIL;
	}
	
	if (ret != 0)
		kvp_respond_host(ret);
}

int 
hv_kvp_init(hv_vmbus_service *srv)
{
	int error;
	hv_work_queue *work_queue;

	error = 0;

	work_queue = hv_work_queue_create("KVP Service");
	if (work_queue == NULL) {
		error = ENOMEM;
	} else
		srv->work_queue = work_queue;

	return (error);
}
