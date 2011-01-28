/*
 * Copyright (c) 2005-2008 Intel Corporation.  All rights reserved.
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
 * $Id: $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DAPL_PROVIDER
#undef DAPL_PROVIDER
#endif

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <process.h>
#include <complib/cl_types.h>
#include "..\..\..\..\etc\user\getopt.c"

#define getpid() ((int)GetCurrentProcessId())
#define F64x "%I64x"

#ifdef DBG
#define DAPL_PROVIDER "ibnic0v2d"
#else
#define DAPL_PROVIDER "ibnic0v2"
#endif

#define ntohll _byteswap_uint64
#define htonll _byteswap_uint64

#else // _WIN32 || _WIN64

#include <infiniband/endian.h>
#include <infiniband/byteswap.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <getopt.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>

#define DAPL_PROVIDER "ofa-v2-ib0"

#define F64x "%"PRIx64""

#if __BYTE_ORDER == __BIG_ENDIAN
#define htonll(x) (x)
#define ntohll(x) (x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define htonll(x)  bswap_64(x)
#define ntohll(x)  bswap_64(x)
#endif

#endif // _WIN32 || _WIN64

/* Debug: 1 == connect & close only, otherwise full-meal deal */
#define CONNECT_ONLY 0

#define MAX_POLLING_CNT 50000
#define MAX_RDMA_RD    4
#define MAX_PROCS      1000

/* Header files needed for DAT/uDAPL */
#include "dat2/udat.h"

/* definitions */
#define SERVER_CONN_QUAL  45248
#define DTO_TIMEOUT       (1000*1000*5)
#define CNO_TIMEOUT       (1000*1000*1)
#define DTO_FLUSH_TIMEOUT (1000*1000*2)
#define CONN_TIMEOUT      (1000*1000*100)
#define SERVER_TIMEOUT    DAT_TIMEOUT_INFINITE
#define RDMA_BUFFER_SIZE  (64)

/* Global DAT vars */
static DAT_IA_HANDLE h_ia = DAT_HANDLE_NULL;
static DAT_PZ_HANDLE h_pz = DAT_HANDLE_NULL;
static DAT_EP_HANDLE h_ep = DAT_HANDLE_NULL;
static DAT_PSP_HANDLE h_psp = DAT_HANDLE_NULL;
static DAT_CR_HANDLE h_cr = DAT_HANDLE_NULL;

static DAT_EVD_HANDLE h_async_evd = DAT_HANDLE_NULL;
static DAT_EVD_HANDLE h_dto_req_evd = DAT_HANDLE_NULL;
static DAT_EVD_HANDLE h_dto_rcv_evd = DAT_HANDLE_NULL;
static DAT_EVD_HANDLE h_cr_evd = DAT_HANDLE_NULL;
static DAT_EVD_HANDLE h_conn_evd = DAT_HANDLE_NULL;
static DAT_CNO_HANDLE h_dto_cno = DAT_HANDLE_NULL;

/* RDMA buffers */
static DAT_LMR_HANDLE h_lmr_send = DAT_HANDLE_NULL;
static DAT_LMR_HANDLE h_lmr_recv = DAT_HANDLE_NULL;
static DAT_LMR_CONTEXT lmr_context_send;
static DAT_LMR_CONTEXT lmr_context_recv;
static DAT_RMR_CONTEXT rmr_context_send;
static DAT_RMR_CONTEXT rmr_context_recv;
static DAT_VLEN registered_size_send;
static DAT_VLEN registered_size_recv;
static DAT_VADDR registered_addr_send;
static DAT_VADDR registered_addr_recv;

/* Initial msg receive buf, RMR exchange, and Rdma-write notification */
#define MSG_BUF_COUNT     3
#define MSG_IOV_COUNT     2
static DAT_RMR_TRIPLET rmr_recv_msg[MSG_BUF_COUNT];
static DAT_LMR_HANDLE h_lmr_recv_msg = DAT_HANDLE_NULL;
static DAT_LMR_CONTEXT lmr_context_recv_msg;
static DAT_RMR_CONTEXT rmr_context_recv_msg;
static DAT_VLEN registered_size_recv_msg;
static DAT_VADDR registered_addr_recv_msg;

/* message send buffer */
static DAT_RMR_TRIPLET rmr_send_msg;
static DAT_LMR_HANDLE h_lmr_send_msg = DAT_HANDLE_NULL;
static DAT_LMR_CONTEXT lmr_context_send_msg;
static DAT_RMR_CONTEXT rmr_context_send_msg;
static DAT_VLEN registered_size_send_msg;
static DAT_VADDR registered_addr_send_msg;
static DAT_EP_ATTR ep_attr;
char hostname[256] = { 0 };
char provider[64] = DAPL_PROVIDER;
char addr_str[INET_ADDRSTRLEN];

/* rdma pointers */
char *rbuf = NULL;
char *sbuf = NULL;
int status;

/* timers */
double start, stop, total_us, total_sec;

struct dt_time {
	double total;
	double open;
	double reg;
	double unreg;
	double pzc;
	double pzf;
	double evdc;
	double evdf;
	double cnoc;
	double cnof;
	double epc;
	double epf;
	double rdma_wr;
	double rdma_rd[MAX_RDMA_RD];
	double rdma_rd_total;
	double rtt;
	double close;
	double conn;
};

struct dt_time ts;

/* defaults */
static int failed = 0;
static int performance_times = 0;
static int connected = 0;
static int burst = 10;
static int server = 1;
static int verbose = 0;
static int polling = 0;
static int poll_count = 0;
static int rdma_wr_poll_count = 0;
static int conn_poll_count = 0;
static int rdma_rd_poll_count[MAX_RDMA_RD] = { 0 };
static int delay = 0;
static int buf_len = RDMA_BUFFER_SIZE;
static int use_cno = 0;
static int recv_msg_index = 0;
static int burst_msg_posted = 0;
static int burst_msg_index = 0;
static int ucm = 0;
static DAT_SOCK_ADDR6 remote;

/* forward prototypes */
const char *DT_RetToStr(DAT_RETURN ret_value);
const char *DT_EventToStr(DAT_EVENT_NUMBER event_code);
void print_usage(void);
double get_time(void);
void init_data(void);

DAT_RETURN send_msg(void *data,
		    DAT_COUNT size,
		    DAT_LMR_CONTEXT context,
		    DAT_DTO_COOKIE cookie, DAT_COMPLETION_FLAGS flags);

DAT_RETURN connect_ep(char *hostname, DAT_CONN_QUAL conn_id);
void disconnect_ep(void);
DAT_RETURN register_rdma_memory(void);
DAT_RETURN unregister_rdma_memory(void);
DAT_RETURN create_events(void);
DAT_RETURN destroy_events(void);
DAT_RETURN do_rdma_write_with_msg(void);
DAT_RETURN do_rdma_read_with_msg(void);
DAT_RETURN do_ping_pong_msg(void);

#define LOGPRINTF if (verbose) printf

void flush_evds(void)
{
	DAT_EVENT event;

	/* Flush async error queue */
	printf("%d ERR: Checking ASYNC EVD...\n", getpid());
	while (dat_evd_dequeue(h_async_evd, &event) == DAT_SUCCESS) {
		printf(" ASYNC EVD ENTRY: handle=%p reason=%d\n",
			event.event_data.asynch_error_event_data.dat_handle,
			event.event_data.asynch_error_event_data.reason);
	}
	/* Flush receive queue */
	printf("%d ERR: Checking RECEIVE EVD...\n", getpid());
	while (dat_evd_dequeue(h_dto_rcv_evd, &event) == DAT_SUCCESS) {
		printf(" RCV EVD ENTRY: op=%d stat=%d ln=%d ck="F64x"\n",
			event.event_data.dto_completion_event_data.operation,
			event.event_data.dto_completion_event_data.status,
			event.event_data.dto_completion_event_data.transfered_length,
			event.event_data.dto_completion_event_data.user_cookie.as_64);
	}
	/* Flush request queue */
	printf("%d ERR: Checking REQUEST EVD...\n", getpid());
	while (dat_evd_dequeue(h_dto_req_evd, &event) == DAT_SUCCESS) {
		printf(" REQ EVD ENTRY: op=%d stat=%d ln=%d ck="F64x"\n",
			event.event_data.dto_completion_event_data.operation,
			event.event_data.dto_completion_event_data.status,
			event.event_data.dto_completion_event_data.transfered_length,
			event.event_data.dto_completion_event_data.user_cookie.as_64);
	}
}


static inline DAT_RETURN
collect_event(DAT_EVD_HANDLE dto_evd,
	      DAT_EVENT *event,
	      DAT_TIMEOUT timeout,
	      int *counter)
{
	DAT_EVD_HANDLE	evd = DAT_HANDLE_NULL;
	DAT_COUNT	nmore;
	DAT_RETURN	ret = DAT_SUCCESS;

	if (use_cno) {
retry:
		/* CNO wait could return EVD's in any order and
		 * may drop some EVD notification's if already
		 * triggered. Once woken, simply dequeue the 
		 * Evd the caller wants to collect and return.
		 * If notification without EVD, retry.
		 */
		ret = dat_cno_wait(h_dto_cno, CNO_TIMEOUT, &evd);
		if (dat_evd_dequeue(dto_evd, event) != DAT_SUCCESS) {
			if (ret == DAT_SUCCESS)
				printf(" WARNING: CNO notification:"
				       " without EVD?\n");
			goto retry;
		}
		ret = DAT_SUCCESS; /* cno timed out, but EVD dequeued */
		
	} else if (!polling) {

		/* use wait to dequeue */
		ret = dat_evd_wait(dto_evd, timeout, 1, event, &nmore);
		if (ret != DAT_SUCCESS)
			fprintf(stderr,
				"Error waiting on h_dto_evd %p: %s\n",
				dto_evd, DT_RetToStr(ret));
		
	} else {
		while (dat_evd_dequeue(dto_evd, event) == DAT_QUEUE_EMPTY)
			if (counter)
				(*counter)++;
	}
	return (ret);
}

static void print_ia_address(struct sockaddr *sa)
{
	char str[INET6_ADDRSTRLEN] = {" ??? "};

	switch(sa->sa_family) {
	case AF_INET:
		inet_ntop(AF_INET, &((struct sockaddr_in *)sa)->sin_addr, str, INET6_ADDRSTRLEN);
		printf("%d Local Address AF_INET - %s port %d\n", getpid(), str, SERVER_CONN_QUAL);
		break;
	case AF_INET6:
		inet_ntop(AF_INET6, &((struct sockaddr_in6 *)sa)->sin6_addr, str, INET6_ADDRSTRLEN);
		printf("%d Local Address AF_INET6 - %s flowinfo(QPN)=0x%x, port(LID)=0x%x\n",
			getpid(), str, 
			ntohl(((struct sockaddr_in6 *)sa)->sin6_flowinfo),
			ntohs(((struct sockaddr_in6 *)sa)->sin6_port));
		break;
	default:
		printf("%d Local Address UNKOWN FAMILY - port %d\n", getpid(), SERVER_CONN_QUAL);
	}
}

int main(int argc, char **argv)
{
	int i, c;
	DAT_RETURN ret;
	DAT_EP_PARAM ep_param;
	DAT_IA_ATTR ia_attr;

	/* parse arguments */
	while ((c = getopt(argc, argv, "tscvpq:l:b:d:B:h:P:")) != -1) {
		switch (c) {
		case 't':
			performance_times = 1;
			fflush(stdout);
			break;
		case 's':
			server = 1;
			fflush(stdout);
			break;
		case 'c':
			use_cno = 1;
			printf("%d Creating CNO for DTO EVD's\n", getpid());
			fflush(stdout);
			break;
		case 'v':
			verbose = 1;
			printf("%d Verbose\n", getpid());
			fflush(stdout);
			break;
		case 'p':
			polling = 1;
			printf("%d Polling\n", getpid());
			fflush(stdout);
			break;
		case 'q':
			/* map UCM qpn into AF_INET6 sin6_flowinfo */
			remote.sin6_family = AF_INET6;
			remote.sin6_flowinfo = htonl(strtol(optarg,NULL,0));
			ucm = 1;
			server = 0;
			break;
		case 'l':
			/* map UCM lid into AF_INET6 sin6_port */
			remote.sin6_family = AF_INET6;
			remote.sin6_port = htons(strtol(optarg,NULL,0));
			ucm = 1;
			server = 0;
			break;
		case 'B':
			burst = atoi(optarg);
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case 'b':
			buf_len = atoi(optarg);
			break;
		case 'h':
			server = 0;
			strcpy(hostname, optarg);
			break;
		case 'P':
			strcpy(provider, optarg);
			break;
		default:
			print_usage();
			exit(-12);
		}
	}

#if defined(_WIN32) || defined(_WIN64)
	{
		WSADATA wsaData;

		i = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (i != 0) {
			printf("%s WSAStartup(2.2) failed? (0x%x)\n", argv[0],
			       i);
			fflush(stdout);
			exit(1);
		}
	}
#endif

	if (!server) {
		printf("%d Running as client - %s\n", getpid(), provider);
	} else {
		printf("%d Running as server - %s\n", getpid(), provider);
	}
	fflush(stdout);

	/* allocate send and receive buffers */
	if (((rbuf = malloc(buf_len * (burst+1))) == NULL) ||
	    ((sbuf = malloc(buf_len * (burst+1))) == NULL)) {
		perror("malloc");
		exit(1);
	}
	memset(&ts, 0, sizeof(struct dt_time));
	LOGPRINTF("%d Allocated RDMA buffers (r:%p,s:%p) len %d \n",
		  getpid(), rbuf, sbuf, buf_len);

	/* dat_ia_open, dat_pz_create */
	h_async_evd = DAT_HANDLE_NULL;
	start = get_time();
	ret = dat_ia_open(provider, 8, &h_async_evd, &h_ia);
	stop = get_time();
	ts.open += ((stop - start) * 1.0e6);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d: Error Adaptor open: %s\n",
			getpid(), DT_RetToStr(ret));
		exit(1);
	} else
		LOGPRINTF("%d Opened Interface Adaptor\n", getpid());

	ret = dat_ia_query(h_ia, 0, DAT_IA_FIELD_ALL, &ia_attr, 0, 0);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d: Error Adaptor query: %s\n",
			getpid(), DT_RetToStr(ret));
		exit(1);
	}
	print_ia_address(ia_attr.ia_address_ptr);

	/* Create Protection Zone */
	start = get_time();
	LOGPRINTF("%d Create Protection Zone\n", getpid());
	ret = dat_pz_create(h_ia, &h_pz);
	stop = get_time();
	ts.pzc += ((stop - start) * 1.0e6);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error creating Protection Zone: %s\n",
			getpid(), DT_RetToStr(ret));
		exit(1);
	} else
		LOGPRINTF("%d Created Protection Zone\n", getpid());

	/* Register memory */
	LOGPRINTF("%d Register RDMA memory\n", getpid());
	ret = register_rdma_memory();
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error registering RDMA memory: %s\n",
			getpid(), DT_RetToStr(ret));
		goto cleanup;
	} else
		LOGPRINTF("%d Register RDMA memory done\n", getpid());

	LOGPRINTF("%d Create events\n", getpid());
	ret = create_events();
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error creating events: %s\n",
			getpid(), DT_RetToStr(ret));
		goto cleanup;
	} else {
		LOGPRINTF("%d Create events done\n", getpid());
	}

	/* create EP */
	memset(&ep_attr, 0, sizeof(ep_attr));
	ep_attr.service_type = DAT_SERVICE_TYPE_RC;
	ep_attr.max_rdma_size = 0x10000;
	ep_attr.qos = 0;
	ep_attr.recv_completion_flags = 0;
	ep_attr.max_recv_dtos = MSG_BUF_COUNT + (burst * 3);
	ep_attr.max_request_dtos = MSG_BUF_COUNT + (burst * 3) + MAX_RDMA_RD;
	ep_attr.max_recv_iov = MSG_IOV_COUNT;
	ep_attr.max_request_iov = MSG_IOV_COUNT;
	ep_attr.max_rdma_read_in = MAX_RDMA_RD;
	ep_attr.max_rdma_read_out = MAX_RDMA_RD;
	ep_attr.request_completion_flags = DAT_COMPLETION_DEFAULT_FLAG;
	ep_attr.ep_transport_specific_count = 0;
	ep_attr.ep_transport_specific = NULL;
	ep_attr.ep_provider_specific_count = 0;
	ep_attr.ep_provider_specific = NULL;

	start = get_time();
	ret = dat_ep_create(h_ia, h_pz, h_dto_rcv_evd,
			    h_dto_req_evd, h_conn_evd, &ep_attr, &h_ep);
	stop = get_time();
	ts.epc += ((stop - start) * 1.0e6);
	ts.total += ts.epc;
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error dat_ep_create: %s\n",
			getpid(), DT_RetToStr(ret));
		goto cleanup;
	} else
		LOGPRINTF("%d EP created %p \n", getpid(), h_ep);

	/*
	 * register message buffers, establish connection, and
	 * exchange DMA RMR information info via messages
	 */
	ret = connect_ep(hostname, SERVER_CONN_QUAL);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error connect_ep: %s\n",
			getpid(), DT_RetToStr(ret));
		goto cleanup;
	} else
		LOGPRINTF("%d connect_ep complete\n", getpid());

	/* Query EP for local and remote address information, print */
	ret = dat_ep_query(h_ep, DAT_EP_FIELD_ALL, &ep_param);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error dat_ep_query: %s\n",
			getpid(), DT_RetToStr(ret));
		goto cleanup;
	} else
		LOGPRINTF("%d EP queried %p \n", getpid(), h_ep);
#if defined(_WIN32)
	printf("\n%d Query EP: LOCAL addr %s port %lld\n", getpid(),
	       inet_ntoa(((struct sockaddr_in *)
			  ep_param.local_ia_address_ptr)->sin_addr),
	       (ep_param.local_port_qual));
#else
	inet_ntop(AF_INET,
		  &((struct sockaddr_in *)ep_param.local_ia_address_ptr)->
		  sin_addr, addr_str, sizeof(addr_str));
	printf("\n%d Query EP: LOCAL addr %s port " F64x "\n", getpid(),
	       addr_str, (ep_param.local_port_qual));
#endif
#if defined(_WIN32)
	printf("%d Query EP: REMOTE addr %s port %lld\n", getpid(),
	       inet_ntoa(((struct sockaddr_in *)
			  ep_param.local_ia_address_ptr)->sin_addr),
	       (ep_param.remote_port_qual));
#else
	inet_ntop(AF_INET,
		  &((struct sockaddr_in *)ep_param.remote_ia_address_ptr)->
		  sin_addr, addr_str, sizeof(addr_str));
	printf("%d Query EP: REMOTE addr %s port " F64x "\n", getpid(),
	       addr_str, (ep_param.remote_port_qual));
#endif
	fflush(stdout);

#if CONNECT_ONLY
#if defined(_WIN32) || defined(_WIN64)
	Sleep(1 * 1000);
#else
	sleep(1);
#endif
	goto cleanup;
#endif

	/*********** RDMA write data *************/
	ret = do_rdma_write_with_msg();
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error do_rdma_write_with_msg: %s\n",
			getpid(), DT_RetToStr(ret));
		goto cleanup;
	} else
		LOGPRINTF("%d do_rdma_write_with_msg complete\n", getpid());

	/*********** RDMA read data *************/
	ret = do_rdma_read_with_msg();
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error do_rdma_read_with_msg: %s\n",
			getpid(), DT_RetToStr(ret));
		goto cleanup;
	} else
		LOGPRINTF("%d do_rdma_read_with_msg complete\n", getpid());

	/*********** PING PING messages ************/
	ret = do_ping_pong_msg();
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error do_ping_pong_msg: %s\n",
			getpid(), DT_RetToStr(ret));
		goto cleanup;
	} else {
		LOGPRINTF("%d do_ping_pong_msg complete\n", getpid());
		goto complete;
	}

cleanup:
	flush_evds();
	failed++;
complete:

	/* disconnect and free EP resources */
	if (h_ep != DAT_HANDLE_NULL) {
		/* unregister message buffers and tear down connection */
		LOGPRINTF("%d Disconnect and Free EP %p \n", getpid(), h_ep);
		disconnect_ep();

		/* free EP */
		LOGPRINTF("%d Free EP %p \n", getpid(), h_ep);
		start = get_time();
		ret = dat_ep_free(h_ep);
		stop = get_time();
		ts.epf += ((stop - start) * 1.0e6);
		ts.total += ts.epf;
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error freeing EP: %s\n",
				getpid(), DT_RetToStr(ret));
		} else {
			LOGPRINTF("%d Freed EP\n", getpid());
			h_ep = DAT_HANDLE_NULL;
		}
	}

	/* free EVDs */
	LOGPRINTF("%d destroy events\n", getpid());
	ret = destroy_events();
	if (ret != DAT_SUCCESS)
		fprintf(stderr, "%d Error destroy_events: %s\n",
			getpid(), DT_RetToStr(ret));
	else
		LOGPRINTF("%d destroy events done\n", getpid());

	ret = unregister_rdma_memory();
	LOGPRINTF("%d unregister_rdma_memory \n", getpid());
	if (ret != DAT_SUCCESS)
		fprintf(stderr, "%d Error unregister_rdma_memory: %s\n",
			getpid(), DT_RetToStr(ret));
	else
		LOGPRINTF("%d unregister_rdma_memory done\n", getpid());

	/* Free protection domain */
	LOGPRINTF("%d Freeing pz\n", getpid());
	start = get_time();
	ret = dat_pz_free(h_pz);
	stop = get_time();
	ts.pzf += ((stop - start) * 1.0e6);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error freeing PZ: %s\n",
			getpid(), DT_RetToStr(ret));
	} else {
		LOGPRINTF("%d Freed pz\n", getpid());
		h_pz = NULL;
	}

	/* close the device */
	LOGPRINTF("%d Closing Interface Adaptor\n", getpid());
	start = get_time();
	ret = dat_ia_close(h_ia, DAT_CLOSE_ABRUPT_FLAG);
	stop = get_time();
	ts.close += ((stop - start) * 1.0e6);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d: Error Adaptor close: %s\n",
			getpid(), DT_RetToStr(ret));
	} else
		LOGPRINTF("%d Closed Interface Adaptor\n", getpid());

	/* free rdma buffers */
	free(rbuf);
	free(sbuf);

	printf("\n%d: DAPL Test Complete. %s\n\n",
	       getpid(), failed ? "FAILED" : "PASSED");

	fflush(stderr);
	fflush(stdout);

	if (!performance_times)
		exit(0);

	printf("\n%d: DAPL Test Complete.\n\n", getpid());
	printf("%d: Message RTT: Total=%10.2lf usec, %d bursts, itime=%10.2lf"
	       " usec, pc=%d\n",
	       getpid(), ts.rtt, burst, ts.rtt / burst, poll_count);
	printf("%d: RDMA write:  Total=%10.2lf usec, %d bursts, itime=%10.2lf"
	       " usec, pc=%d\n",
	       getpid(), ts.rdma_wr, burst,
	       ts.rdma_wr / burst, rdma_wr_poll_count);
	for (i = 0; i < MAX_RDMA_RD; i++) {
		printf("%d: RDMA read:   Total=%10.2lf usec,   %d bursts, "
		       "itime=%10.2lf usec, pc=%d\n",
		       getpid(), ts.rdma_rd_total, MAX_RDMA_RD,
		       ts.rdma_rd[i], rdma_rd_poll_count[i]);
	}
	printf("%d: open:      %10.2lf usec\n", getpid(), ts.open);
	printf("%d: close:     %10.2lf usec\n", getpid(), ts.close);
	printf("%d: PZ create: %10.2lf usec\n", getpid(), ts.pzc);
	printf("%d: PZ free:   %10.2lf usec\n", getpid(), ts.pzf);
	printf("%d: LMR create:%10.2lf usec\n", getpid(), ts.reg);
	printf("%d: LMR free:  %10.2lf usec\n", getpid(), ts.unreg);
	printf("%d: EVD create:%10.2lf usec\n", getpid(), ts.evdc);
	printf("%d: EVD free:  %10.2lf usec\n", getpid(), ts.evdf);
	if (use_cno) {
		printf("%d: CNO create:  %10.2lf usec\n", getpid(), ts.cnoc);
		printf("%d: CNO free:    %10.2lf usec\n", getpid(), ts.cnof);
	}
	printf("%d: EP create: %10.2lf usec\n", getpid(), ts.epc);
	printf("%d: EP free:   %10.2lf usec\n", getpid(), ts.epf);
	if (!server)
		printf("%d: connect:   %10.2lf usec, poll_cnt=%d\n", 
		       getpid(), ts.conn, conn_poll_count);
	printf("%d: TOTAL:     %10.2lf usec\n", getpid(), ts.total);

#if defined(_WIN32) || defined(_WIN64)
	WSACleanup();
#endif
	return (0);
}

double get_time(void)
{
	struct timeval tp;

	gettimeofday(&tp, NULL);
	return ((double)tp.tv_sec + (double)tp.tv_usec * 1e-6);
}

void init_data(void)
{
	memset(rbuf, 'a', buf_len);
	memset(sbuf, 'b', buf_len);
}

DAT_RETURN
send_msg(void *data,
	 DAT_COUNT size,
	 DAT_LMR_CONTEXT context,
	 DAT_DTO_COOKIE cookie, DAT_COMPLETION_FLAGS flags)
{
	DAT_LMR_TRIPLET iov;
	DAT_EVENT event;
	DAT_RETURN ret;

	iov.lmr_context = context;
#if defined(_WIN32)
	iov.virtual_address = (DAT_VADDR) data;
#else
	iov.virtual_address = (DAT_VADDR) (unsigned long)data;
#endif
	iov.segment_length = size;

	LOGPRINTF("%d calling post_send\n", getpid());
	cookie.as_64 = 0xaaaa;
	ret = dat_ep_post_send(h_ep, 1, &iov, cookie, flags);

	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d: ERROR: dat_ep_post_send() %s\n",
			getpid(), DT_RetToStr(ret));
		return ret;
	}

	if (!(flags & DAT_COMPLETION_SUPPRESS_FLAG)) {
		
		if (collect_event(h_dto_req_evd, 
				  &event, 
				  DTO_TIMEOUT, 
				  &poll_count) != DAT_SUCCESS)
			return (DAT_ABORT);

		/* validate event number, len, cookie, and status */
		if (event.event_number != DAT_DTO_COMPLETION_EVENT) {
			fprintf(stderr, "%d: ERROR: DTO event number %s\n",
				getpid(), 
				DT_EventToStr(event.event_number));
			return (DAT_ABORT);
		}

		if ((event.event_data.dto_completion_event_data.
		     transfered_length != size)
		    || (event.event_data.dto_completion_event_data.user_cookie.
			as_64 != 0xaaaa)) {
			fprintf(stderr,
				"%d: ERROR: DTO len %d or cookie " F64x " \n",
				getpid(),
				event.event_data.dto_completion_event_data.
				transfered_length,
				event.event_data.dto_completion_event_data.
				user_cookie.as_64);
			return (DAT_ABORT);

		}
		if (event.event_data.dto_completion_event_data.status !=
		    DAT_SUCCESS) {
			fprintf(stderr, "%d: ERROR: DTO event status %s\n",
				getpid(), DT_RetToStr(ret));
			return (DAT_ABORT);
		}
	}

	return DAT_SUCCESS;
}

DAT_RETURN connect_ep(char *hostname, DAT_CONN_QUAL conn_id)
{
	DAT_IA_ADDRESS_PTR remote_addr = (DAT_IA_ADDRESS_PTR)&remote;
	DAT_RETURN ret;
	DAT_REGION_DESCRIPTION region;
	DAT_EVENT event;
	DAT_COUNT nmore;
	DAT_LMR_TRIPLET l_iov;
	DAT_RMR_TRIPLET r_iov;
	DAT_DTO_COOKIE cookie;
	int i;
	unsigned char *buf;
	DAT_CR_PARAM cr_param = { 0 };
	unsigned char pdata[48] = { 0 };

	/* Register send message buffer */
	LOGPRINTF("%d Registering send Message Buffer %p, len %d\n",
		  getpid(), &rmr_send_msg, (int)sizeof(DAT_RMR_TRIPLET));
	region.for_va = &rmr_send_msg;
	ret = dat_lmr_create(h_ia,
			     DAT_MEM_TYPE_VIRTUAL,
			     region,
			     sizeof(DAT_RMR_TRIPLET),
			     h_pz,
			     DAT_MEM_PRIV_LOCAL_WRITE_FLAG,
			     DAT_VA_TYPE_VA,
			     &h_lmr_send_msg,
			     &lmr_context_send_msg,
			     &rmr_context_send_msg,
			     &registered_size_send_msg,
			     &registered_addr_send_msg);

	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error registering send msg buffer: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else
		LOGPRINTF("%d Registered send Message Buffer %p \n",
			  getpid(), region.for_va);

	/* Register Receive buffers */
	LOGPRINTF("%d Registering Receive Message Buffer %p\n",
		  getpid(), rmr_recv_msg);
	region.for_va = rmr_recv_msg;
	ret = dat_lmr_create(h_ia,
			     DAT_MEM_TYPE_VIRTUAL,
			     region,
			     sizeof(DAT_RMR_TRIPLET) * MSG_BUF_COUNT,
			     h_pz,
			     DAT_MEM_PRIV_LOCAL_WRITE_FLAG,
			     DAT_VA_TYPE_VA,
			     &h_lmr_recv_msg,
			     &lmr_context_recv_msg,
			     &rmr_context_recv_msg,
			     &registered_size_recv_msg,
			     &registered_addr_recv_msg);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error registering recv msg buffer: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else
		LOGPRINTF("%d Registered Receive Message Buffer %p\n",
			  getpid(), region.for_va);

	for (i = 0; i < MSG_BUF_COUNT; i++) {
		cookie.as_64 = i;
		l_iov.lmr_context = lmr_context_recv_msg;
#if defined(_WIN32)
		l_iov.virtual_address = (DAT_VADDR) & rmr_recv_msg[i];
#else
		l_iov.virtual_address =
		    (DAT_VADDR) (unsigned long)&rmr_recv_msg[i];
#endif
		l_iov.segment_length = sizeof(DAT_RMR_TRIPLET);

		LOGPRINTF("%d Posting Receive Message Buffer %p\n",
			  getpid(), &rmr_recv_msg[i]);
		ret = dat_ep_post_recv(h_ep,
				       1,
				       &l_iov,
				       cookie, DAT_COMPLETION_DEFAULT_FLAG);

		if (ret != DAT_SUCCESS) {
			fprintf(stderr,
				"%d Error registering recv msg buffer: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else
			LOGPRINTF("%d Registered Receive Message Buffer %p\n",
				  getpid(), region.for_va);

	}

	/* setup receive rdma buffer to initial string to be overwritten */
	strcpy((char *)rbuf, "blah, blah, blah\n");

	/* clear event structure */
	memset(&event, 0, sizeof(DAT_EVENT));

	if (server) {		/* SERVER */

		/* create the service point for server listen */
		LOGPRINTF("%d Creating service point for listen\n", getpid());
		ret = dat_psp_create(h_ia,
				     conn_id,
				     h_cr_evd, DAT_PSP_CONSUMER_FLAG, &h_psp);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error dat_psp_create: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else
			LOGPRINTF("%d dat_psp_created for server listen\n",
				  getpid());

		printf("%d Server waiting for connect request on port " F64x
		       "\n", getpid(), conn_id);

		ret = dat_evd_wait(h_cr_evd, SERVER_TIMEOUT, 1, &event, &nmore);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error dat_evd_wait: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else
			LOGPRINTF("%d dat_evd_wait for cr_evd completed\n",
				  getpid());

		if (event.event_number != DAT_CONNECTION_REQUEST_EVENT) {
			fprintf(stderr, "%d Error unexpected cr event : %s\n",
				getpid(), 
				DT_EventToStr(event.event_number));
			return (DAT_ABORT);
		}
		if ((event.event_data.cr_arrival_event_data.conn_qual !=
		     SERVER_CONN_QUAL)
		    || (event.event_data.cr_arrival_event_data.sp_handle.
			psp_handle != h_psp)) {
			fprintf(stderr, "%d Error wrong cr event data : %s\n",
				getpid(), 
				DT_EventToStr(event.event_number));
			return (DAT_ABORT);
		}

		/* use to test rdma_cma timeout logic */
#if defined(_WIN32) || defined(_WIN64)
		if (delay)
			Sleep(delay * 1000);
#else
		if (delay)
			sleep(delay);
#endif

		/* accept connect request from client */
		h_cr = event.event_data.cr_arrival_event_data.cr_handle;
		LOGPRINTF("%d Accepting connect request from client\n",
			  getpid());

		/* private data - check and send it back */
		dat_cr_query(h_cr, DAT_CSP_FIELD_ALL, &cr_param);

		buf = (unsigned char *)cr_param.private_data;
		LOGPRINTF("%d CONN REQUEST Private Data %p[0]=%d [47]=%d\n",
			  getpid(), buf, buf[0], buf[47]);
		for (i = 0; i < 48; i++) {
			if (buf[i] != i + 1) {
				fprintf(stderr, "%d Error with CONNECT REQUEST"
					" private data: %p[%d]=%d s/be %d\n",
					getpid(), buf, i, buf[i], i + 1);
				dat_cr_reject(h_cr, 0, NULL);
				return (DAT_ABORT);
			}
			buf[i]++;	/* change for trip back */
		}

#ifdef TEST_REJECT_WITH_PRIVATE_DATA
		printf("%d REJECT request with 48 bytes of private data\n",
		       getpid());
		ret = dat_cr_reject(h_cr, 48, cr_param.private_data);
		printf("\n%d: DAPL Test Complete. %s\n\n",
		       getpid(), ret ? "FAILED" : "PASSED");
		exit(0);
#endif

		ret = dat_cr_accept(h_cr, h_ep, 48, cr_param.private_data);

		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error dat_cr_accept: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else
			LOGPRINTF("%d dat_cr_accept completed\n", getpid());
	} else {		/* CLIENT */
		struct addrinfo *target;
		int rval;

		if (ucm)
			goto no_resolution;

#if defined(_WIN32) || defined(_WIN64)
		if ((rval = getaddrinfo(hostname, "ftp", NULL, &target)) != 0) {
			printf("\n remote name resolution failed! %s\n",
			       gai_strerror(rval));
			exit(1);
		}
		rval = ((struct sockaddr_in *)target->ai_addr)->sin_addr.s_addr;
#else
		if (getaddrinfo(hostname, NULL, NULL, &target) != 0) {
			perror("\n remote name resolution failed!");
			exit(1);
		}
		rval = ((struct sockaddr_in *)target->ai_addr)->sin_addr.s_addr;
#endif
		printf("%d Server Name: %s \n", getpid(), hostname);
		printf("%d Server Net Address: %d.%d.%d.%d port " F64x "\n",
		       getpid(), (rval >> 0) & 0xff, (rval >> 8) & 0xff,
		       (rval >> 16) & 0xff, (rval >> 24) & 0xff, conn_id);

		remote_addr = (DAT_IA_ADDRESS_PTR)target->ai_addr; /* IP */
no_resolution:
		for (i = 0; i < 48; i++)	/* simple pattern in private data */
			pdata[i] = i + 1;

		LOGPRINTF("%d Connecting to server\n", getpid());
        	start = get_time();
		ret = dat_ep_connect(h_ep,
				     remote_addr,
				     conn_id,
				     CONN_TIMEOUT,
				     48,
				     (DAT_PVOID) pdata,
				     0, DAT_CONNECT_DEFAULT_FLAG);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error dat_ep_connect: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else
			LOGPRINTF("%d dat_ep_connect completed\n", getpid());

		if (!ucm)
			freeaddrinfo(target);
	}

	printf("%d Waiting for connect response\n", getpid());

	if (polling) 
		while (DAT_GET_TYPE(dat_evd_dequeue(h_conn_evd, &event)) == 
		       DAT_QUEUE_EMPTY)
			conn_poll_count++;
	else 
		ret = dat_evd_wait(h_conn_evd, DAT_TIMEOUT_INFINITE, 
				   1, &event, &nmore);

	if (!server) {
        	stop = get_time();
        	ts.conn += ((stop - start) * 1.0e6);
	}

#ifdef TEST_REJECT_WITH_PRIVATE_DATA
	if (event.event_number != DAT_CONNECTION_EVENT_PEER_REJECTED) {
		fprintf(stderr, "%d expected conn reject event : %s\n",
			getpid(), DT_EventToStr(event.event_number));
		return (DAT_ABORT);
	}
	/* get the reject private data and validate */
	buf = (unsigned char *)event.event_data.connect_event_data.private_data;
	printf("%d Received REJECT with private data %p[0]=%d [47]=%d\n",
	       getpid(), buf, buf[0], buf[47]);
	for (i = 0; i < 48; i++) {
		if (buf[i] != i + 2) {
			fprintf(stderr, "%d client: Error with REJECT event"
				" private data: %p[%d]=%d s/be %d\n",
				getpid(), buf, i, buf[i], i + 2);
			dat_ep_disconnect(h_ep, DAT_CLOSE_ABRUPT_FLAG);
			return (DAT_ABORT);
		}
	}
	printf("\n%d: DAPL Test Complete. PASSED\n\n", getpid());
	exit(0);
#endif

	if (event.event_number != DAT_CONNECTION_EVENT_ESTABLISHED) {
		fprintf(stderr, "%d Error unexpected conn event : 0x%x %s\n",
			getpid(), event.event_number,
			DT_EventToStr(event.event_number));
		return (DAT_ABORT);
	}

	/* check private data back from server  */
	if (!server) {
		buf =
		    (unsigned char *)event.event_data.connect_event_data.
		    private_data;
		LOGPRINTF("%d CONN Private Data %p[0]=%d [47]=%d\n", getpid(),
			  buf, buf[0], buf[47]);
		for (i = 0; i < 48; i++) {
			if (buf[i] != i + 2) {
				fprintf(stderr, "%d Error with CONNECT event"
					" private data: %p[%d]=%d s/be %d\n",
					getpid(), buf, i, buf[i], i + 2);
				dat_ep_disconnect(h_ep, DAT_CLOSE_ABRUPT_FLAG);
				LOGPRINTF
				    ("%d waiting for disconnect event...\n",
				     getpid());
				dat_evd_wait(h_conn_evd, DAT_TIMEOUT_INFINITE,
					     1, &event, &nmore);
				return (DAT_ABORT);
			}
		}
	}

	printf("\n%d CONNECTED!\n\n", getpid());
	connected = 1;

#if CONNECT_ONLY
	return 0;
#endif

	/*
	 *  Setup our remote memory and tell the other side about it
	 */
	rmr_send_msg.virtual_address = htonll((DAT_VADDR) (uintptr_t) rbuf);
	rmr_send_msg.segment_length = htonl(RDMA_BUFFER_SIZE);
	rmr_send_msg.rmr_context = htonl(rmr_context_recv);

	printf("%d Send RMR msg to remote: r_key_ctx=0x%x,va=%p,len=0x%x\n",
	       getpid(), rmr_context_recv, rbuf, RDMA_BUFFER_SIZE);

	ret = send_msg(&rmr_send_msg,
		       sizeof(DAT_RMR_TRIPLET),
		       lmr_context_send_msg,
		       cookie, DAT_COMPLETION_SUPPRESS_FLAG);

	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error send_msg: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else
		LOGPRINTF("%d send_msg completed\n", getpid());

	/*
	 *  Wait for remote RMR information for RDMA
	 */
	if (collect_event(h_dto_rcv_evd, 
			  &event, 
			  DTO_TIMEOUT, 
			  &poll_count) != DAT_SUCCESS)
		return (DAT_ABORT);
	
	printf("%d remote RMR data arrived!\n", getpid());

	if (event.event_number != DAT_DTO_COMPLETION_EVENT) {
		fprintf(stderr, "%d Error unexpected DTO event : %s\n",
			getpid(), DT_EventToStr(event.event_number));
		return (DAT_ABORT);
	}
	if ((event.event_data.dto_completion_event_data.transfered_length !=
	     sizeof(DAT_RMR_TRIPLET)) ||
	    (event.event_data.dto_completion_event_data.user_cookie.as_64 !=
	     recv_msg_index)) {
		fprintf(stderr,
			"ERR recv event: len=%d cookie=" F64x
			" expected %d/%d\n",
			(int)event.event_data.dto_completion_event_data.
			transfered_length,
			event.event_data.dto_completion_event_data.user_cookie.
			as_64, (int)sizeof(DAT_RMR_TRIPLET), recv_msg_index);
		return (DAT_ABORT);
	}

	/* swap received RMR msg: network order to host order */
	r_iov = rmr_recv_msg[recv_msg_index];
	rmr_recv_msg[recv_msg_index].rmr_context = ntohl(r_iov.rmr_context);
	rmr_recv_msg[recv_msg_index].virtual_address =
	    ntohll(r_iov.virtual_address);
	rmr_recv_msg[recv_msg_index].segment_length =
	    ntohl(r_iov.segment_length);

	printf("%d Received RMR from remote: "
	       "r_iov: r_key_ctx=%x,va=" F64x ",len=0x%x\n",
	       getpid(), rmr_recv_msg[recv_msg_index].rmr_context,
	       rmr_recv_msg[recv_msg_index].virtual_address,
	       rmr_recv_msg[recv_msg_index].segment_length);

	recv_msg_index++;

	return (DAT_SUCCESS);
}

void disconnect_ep(void)
{
	DAT_RETURN ret;
	DAT_EVENT event;
	DAT_COUNT nmore;

	if (connected) {

		/* 
		 * Only the client needs to call disconnect. The server _should_ be able
		 * to just wait on the EVD associated with connection events for a
		 * disconnect request and then exit.
		 */
		if (!server) {
			LOGPRINTF("%d dat_ep_disconnect\n", getpid());
			ret = dat_ep_disconnect(h_ep, DAT_CLOSE_DEFAULT);
			if (ret != DAT_SUCCESS) {
				fprintf(stderr,
					"%d Error dat_ep_disconnect: %s\n",
					getpid(), DT_RetToStr(ret));
			} else {
				LOGPRINTF("%d dat_ep_disconnect completed\n",
					  getpid());
			}
		} else {
			LOGPRINTF("%d Server waiting for disconnect...\n",
				  getpid());
		}

		ret =
		    dat_evd_wait(h_conn_evd, DAT_TIMEOUT_INFINITE, 1, &event,
				 &nmore);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error dat_evd_wait: %s\n",
				getpid(), DT_RetToStr(ret));
		} else {
			LOGPRINTF("%d dat_evd_wait for h_conn_evd completed\n",
				  getpid());
		}
	}

	/* destroy service point */
	if ((server) && (h_psp != DAT_HANDLE_NULL)) {
		ret = dat_psp_free(h_psp);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error dat_psp_free: %s\n",
				getpid(), DT_RetToStr(ret));
		} else {
			LOGPRINTF("%d dat_psp_free completed\n", getpid());
		}
	}

	/* Unregister Send message Buffer */
	if (h_lmr_send_msg != DAT_HANDLE_NULL) {
		LOGPRINTF("%d Unregister send message h_lmr %p \n", getpid(),
			  h_lmr_send_msg);
		ret = dat_lmr_free(h_lmr_send_msg);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr,
				"%d Error deregistering send msg mr: %s\n",
				getpid(), DT_RetToStr(ret));
		} else {
			LOGPRINTF("%d Unregistered send message Buffer\n",
				  getpid());
			h_lmr_send_msg = NULL;
		}
	}

	/* Unregister recv message Buffer */
	if (h_lmr_recv_msg != DAT_HANDLE_NULL) {
		LOGPRINTF("%d Unregister recv message h_lmr %p \n", getpid(),
			  h_lmr_recv_msg);
		ret = dat_lmr_free(h_lmr_recv_msg);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr,
				"%d Error deregistering recv msg mr: %s\n",
				getpid(), DT_RetToStr(ret));
		} else {
			LOGPRINTF("%d Unregistered recv message Buffer\n",
				  getpid());
			h_lmr_recv_msg = NULL;
		}
	}
	return;
}

DAT_RETURN do_rdma_write_with_msg(void)
{
	DAT_EVENT event;
	DAT_LMR_TRIPLET l_iov[MSG_IOV_COUNT];
	DAT_RMR_TRIPLET r_iov;
	DAT_DTO_COOKIE cookie;
	DAT_RETURN ret;
	int i;

	printf("\n %d RDMA WRITE DATA with SEND MSG\n\n", getpid());

	cookie.as_64 = 0x5555;

	if (recv_msg_index >= MSG_BUF_COUNT)
		return (DAT_ABORT);

	/* get RMR information from previously received message */
	r_iov = rmr_recv_msg[recv_msg_index - 1];

	if (server)
		strcpy((char *)sbuf, "server RDMA write data...");
	else
		strcpy((char *)sbuf, "client RDMA write data...");

	for (i = 0; i < MSG_IOV_COUNT; i++) {
		l_iov[i].lmr_context = lmr_context_send;
		l_iov[i].segment_length = buf_len / MSG_IOV_COUNT;
		l_iov[i].virtual_address = (DAT_VADDR) (uintptr_t)
		    (&sbuf[l_iov[i].segment_length * i]);

		LOGPRINTF("%d rdma_write iov[%d] buf=%p,len=%d\n",
			  getpid(), i, &sbuf[l_iov[i].segment_length * i],
			  l_iov[i].segment_length);
	}

	start = get_time();
	for (i = 0; i < burst; i++) {
		cookie.as_64 = 0x9999;
		ret = dat_ep_post_rdma_write(h_ep,	// ep_handle
					     MSG_IOV_COUNT,	// num_segments
					     l_iov,	// LMR
					     cookie,	// user_cookie
					     &r_iov,	// RMR
					     DAT_COMPLETION_SUPPRESS_FLAG);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr,
				"%d: ERROR: dat_ep_post_rdma_write() %s\n",
				getpid(), DT_RetToStr(ret));
			return (DAT_ABORT);
		}
		LOGPRINTF("%d rdma_write # %d completed\n", getpid(), i + 1);
	}

	/*
	 *  Send RMR information a 2nd time to indicate completion
	 *  NOTE: already swapped to network order in connect_ep
	 */
	printf("%d Sending RDMA WRITE completion message\n", getpid());

	ret = send_msg(&rmr_send_msg,
		       sizeof(DAT_RMR_TRIPLET),
		       lmr_context_send_msg,
		       cookie, DAT_COMPLETION_SUPPRESS_FLAG);

	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error send_msg: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else {
		LOGPRINTF("%d send_msg completed\n", getpid());
	}

	/* inbound recv event, send completion's suppressed */
	if (collect_event(h_dto_rcv_evd, 
			  &event, 
			  DTO_TIMEOUT, 
			  &rdma_wr_poll_count) != DAT_SUCCESS)
		return (DAT_ABORT);
	
	stop = get_time();
	ts.rdma_wr = ((stop - start) * 1.0e6);

	/* validate event number and status */
	printf("%d inbound rdma_write; send message arrived!\n", getpid());
	if (event.event_number != DAT_DTO_COMPLETION_EVENT) {
		fprintf(stderr, "%d Error unexpected DTO event : %s\n",
			getpid(), DT_EventToStr(event.event_number));
		return (DAT_ABORT);
	}

	if ((event.event_data.dto_completion_event_data.transfered_length !=
	     sizeof(DAT_RMR_TRIPLET))
	    || (event.event_data.dto_completion_event_data.user_cookie.as_64 !=
		recv_msg_index)) {
		fprintf(stderr,
			"unexpected event data for receive: len=%d cookie=" F64x
			" exp %d/%d\n",
			(int)event.event_data.dto_completion_event_data.
			transfered_length,
			event.event_data.dto_completion_event_data.user_cookie.
			as_64, (int)sizeof(DAT_RMR_TRIPLET), recv_msg_index);

		return (DAT_ABORT);
	}

	/* swap received RMR msg: network order to host order */
	r_iov = rmr_recv_msg[recv_msg_index];
	rmr_recv_msg[recv_msg_index].virtual_address =
	    ntohll(rmr_recv_msg[recv_msg_index].virtual_address);
	rmr_recv_msg[recv_msg_index].segment_length =
	    ntohl(rmr_recv_msg[recv_msg_index].segment_length);
	rmr_recv_msg[recv_msg_index].rmr_context =
	    ntohl(rmr_recv_msg[recv_msg_index].rmr_context);

	printf("%d Received RMR from remote: "
	       "r_iov: r_key_ctx=%x,va=" F64x ",len=0x%x\n",
	       getpid(), rmr_recv_msg[recv_msg_index].rmr_context,
	       rmr_recv_msg[recv_msg_index].virtual_address,
	       rmr_recv_msg[recv_msg_index].segment_length);

	LOGPRINTF("%d inbound rdma_write; send msg event SUCCESS!!\n",
		  getpid());

	printf("%d %s RDMA write buffer contains: %s\n",
	       getpid(), server ? "SERVER:" : "CLIENT:", rbuf);

	recv_msg_index++;

	return (DAT_SUCCESS);
}

DAT_RETURN do_rdma_read_with_msg(void)
{
	DAT_EVENT event;
	DAT_LMR_TRIPLET l_iov;
	DAT_RMR_TRIPLET r_iov;
	DAT_DTO_COOKIE cookie;
	DAT_RETURN ret;
	int i;

	printf("\n %d RDMA READ DATA with SEND MSG\n\n", getpid());

	if (recv_msg_index >= MSG_BUF_COUNT)
		return (DAT_ABORT);

	/* get RMR information from previously received message */
	r_iov = rmr_recv_msg[recv_msg_index - 1];

	/* setup rdma read buffer to initial string to be overwritten */
	strcpy((char *)sbuf, "blah, blah, blah\n");

	if (server)
		strcpy((char *)rbuf, "server RDMA read data...");
	else
		strcpy((char *)rbuf, "client RDMA read data...");

	l_iov.lmr_context = lmr_context_send;
	l_iov.virtual_address = (DAT_VADDR) (uintptr_t) sbuf;
	l_iov.segment_length = buf_len;

	for (i = 0; i < MAX_RDMA_RD; i++) {
		cookie.as_64 = 0x9999;
		start = get_time();
		ret = dat_ep_post_rdma_read(h_ep,	// ep_handle
					    1,	// num_segments
					    &l_iov,	// LMR
					    cookie,	// user_cookie
					    &r_iov,	// RMR
					    DAT_COMPLETION_DEFAULT_FLAG);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr,
				"%d: ERROR: dat_ep_post_rdma_read() %s\n",
				getpid(), DT_RetToStr(ret));
			return (DAT_ABORT);
		}

		/* RDMA read completion event */
		if (collect_event(h_dto_req_evd, 
				  &event, 
		 		  DTO_TIMEOUT, 
				  &rdma_rd_poll_count[i]) != DAT_SUCCESS)
			return (DAT_ABORT);

		/* validate event number, len, cookie, and status */
		if (event.event_number != DAT_DTO_COMPLETION_EVENT) {
			fprintf(stderr, "%d: ERROR: DTO event number %s\n",
				getpid(), DT_EventToStr(event.event_number));
			return (DAT_ABORT);
		}
		if ((event.event_data.dto_completion_event_data.
		     transfered_length != buf_len)
		    || (event.event_data.dto_completion_event_data.user_cookie.
			as_64 != 0x9999)) {
			fprintf(stderr,
				"%d: ERROR: DTO len %d or cookie " F64x "\n",
				getpid(),
				event.event_data.dto_completion_event_data.
				transfered_length,
				event.event_data.dto_completion_event_data.
				user_cookie.as_64);
			return (DAT_ABORT);
		}
		if (event.event_data.dto_completion_event_data.status !=
		    DAT_SUCCESS) {
			fprintf(stderr, "%d: ERROR: DTO event status %s\n",
				getpid(), DT_RetToStr(ret));
			return (DAT_ABORT);
		}
		stop = get_time();
		ts.rdma_rd[i] = ((stop - start) * 1.0e6);
		ts.rdma_rd_total += ts.rdma_rd[i];

		LOGPRINTF("%d rdma_read # %d completed\n", getpid(), i + 1);
	}

	/*
	 *  Send RMR information a 3rd time to indicate completion
	 *  NOTE: already swapped to network order in connect_ep
	 */
	printf("%d Sending RDMA read completion message\n", getpid());

	/* give remote chance to process read completes */
	if (use_cno) {
#if defined(_WIN32) || defined(_WIN64)
		Sleep(1000);
#else
		sleep(1);
#endif
	}

	ret = send_msg(&rmr_send_msg,
		       sizeof(DAT_RMR_TRIPLET),
		       lmr_context_send_msg,
		       cookie, DAT_COMPLETION_SUPPRESS_FLAG);

	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error send_msg: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else {
		LOGPRINTF("%d send_msg completed\n", getpid());
	}

	printf("%d Waiting for inbound message....\n", getpid());

	if (collect_event(h_dto_rcv_evd, 
			  &event, 
		 	  DTO_TIMEOUT, 
			  &poll_count) != DAT_SUCCESS)
		return (DAT_ABORT);

	/* validate event number and status */
	printf("%d inbound rdma_read; send message arrived!\n", getpid());
	if (event.event_number != DAT_DTO_COMPLETION_EVENT) {
		fprintf(stderr, "%d Error unexpected DTO event : %s\n",
			getpid(), DT_EventToStr(event.event_number));
		return (DAT_ABORT);
	}

	if ((event.event_data.dto_completion_event_data.transfered_length !=
	     sizeof(DAT_RMR_TRIPLET))
	    || (event.event_data.dto_completion_event_data.user_cookie.as_64 !=
		recv_msg_index)) {

		fprintf(stderr,
			"unexpected event data for receive: len=%d cookie=" F64x
			" exp %d/%d\n",
			(int)event.event_data.dto_completion_event_data.
			transfered_length,
			event.event_data.dto_completion_event_data.user_cookie.
			as_64, (int)sizeof(DAT_RMR_TRIPLET), recv_msg_index);

		return (DAT_ABORT);
	}

	/* swap received RMR msg: network order to host order */
	r_iov = rmr_recv_msg[recv_msg_index];
	rmr_recv_msg[recv_msg_index].virtual_address =
	    ntohll(rmr_recv_msg[recv_msg_index].virtual_address);
	rmr_recv_msg[recv_msg_index].segment_length =
	    ntohl(rmr_recv_msg[recv_msg_index].segment_length);
	rmr_recv_msg[recv_msg_index].rmr_context =
	    ntohl(rmr_recv_msg[recv_msg_index].rmr_context);

	printf("%d Received RMR from remote: "
	       "r_iov: r_key_ctx=%x,va=" F64x ",len=0x%x\n",
	       getpid(), rmr_recv_msg[recv_msg_index].rmr_context,
	       rmr_recv_msg[recv_msg_index].virtual_address,
	       rmr_recv_msg[recv_msg_index].segment_length);

	LOGPRINTF("%d inbound rdma_write; send msg event SUCCESS!!\n",
		  getpid());

	printf("%d %s RCV RDMA read buffer contains: %s\n",
	       getpid(), server ? "SERVER:" : "CLIENT:", sbuf);

	recv_msg_index++;

	return (DAT_SUCCESS);
}

DAT_RETURN do_ping_pong_msg()
{
	DAT_EVENT event;
	DAT_DTO_COOKIE cookie;
	DAT_LMR_TRIPLET l_iov;
	DAT_RETURN ret;
	int i;
	char *snd_buf;
	char *rcv_buf;

	printf("\n %d PING DATA with SEND MSG\n\n", getpid());

	snd_buf = sbuf;
	rcv_buf = rbuf;

	/* pre-post all buffers */
	for (i = 0; i < burst; i++) {
		burst_msg_posted++;
		cookie.as_64 = i;
		l_iov.lmr_context = lmr_context_recv;
		l_iov.virtual_address = (DAT_VADDR) (uintptr_t) rcv_buf;
		l_iov.segment_length = buf_len;

		LOGPRINTF("%d Pre-posting Receive Message Buffers %p\n",
			  getpid(), rcv_buf);

		ret = dat_ep_post_recv(h_ep,
				       1,
				       &l_iov,
				       cookie, DAT_COMPLETION_DEFAULT_FLAG);

		if (ret != DAT_SUCCESS) {
			fprintf(stderr,
				"%d Error posting recv msg buffer: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else {
			LOGPRINTF("%d Posted Receive Message Buffer %p\n",
				  getpid(), rcv_buf);
		}

		/* next buffer */
		rcv_buf += buf_len;
	}
#if defined(_WIN32) || defined(_WIN64)
	Sleep(1000);
#else
	sleep(1);
#endif

	/* Initialize recv_buf and index to beginning */
	rcv_buf = rbuf;
	burst_msg_index = 0;

	/* client ping 0x55, server pong 0xAA in first byte */
	start = get_time();
	for (i = 0; i < burst; i++) {
		/* walk the send and recv buffers */
		if (!server) {
			*snd_buf = 0x55;

			LOGPRINTF("%d %s SND buffer %p contains: 0x%x len=%d\n",
				  getpid(), server ? "SERVER:" : "CLIENT:",
				  snd_buf, *snd_buf, buf_len);

			ret = send_msg(snd_buf,
				       buf_len,
				       lmr_context_send,
				       cookie, DAT_COMPLETION_SUPPRESS_FLAG);

			if (ret != DAT_SUCCESS) {
				fprintf(stderr, "%d Error send_msg: %s\n",
					getpid(), DT_RetToStr(ret));
				return (ret);
			} else {
				LOGPRINTF("%d send_msg completed\n", getpid());
			}
		}

		/* recv message, send completions suppressed */
		if (collect_event(h_dto_rcv_evd, 
				  &event, 
				  DTO_TIMEOUT, 
				  &poll_count) != DAT_SUCCESS)
			return (DAT_ABORT);

		
		/* start timer after first message arrives on server */
		if (i == 0) {
			start = get_time();
		}
		/* validate event number and status */
		LOGPRINTF("%d inbound message; message arrived!\n", getpid());
		if (event.event_number != DAT_DTO_COMPLETION_EVENT) {
			fprintf(stderr, "%d Error unexpected DTO event : %s\n",
				getpid(), DT_EventToStr(event.event_number));
			return (DAT_ABORT);
		}
		if ((event.event_data.dto_completion_event_data.
		     transfered_length != buf_len)
		    || (event.event_data.dto_completion_event_data.user_cookie.
			as_64 != burst_msg_index)) {
			fprintf(stderr,
				"ERR: recv event: len=%d cookie=" F64x
				" exp %d/%d\n",
				(int)event.event_data.dto_completion_event_data.
				transfered_length,
				event.event_data.dto_completion_event_data.
				user_cookie.as_64, (int)buf_len,
				(int)burst_msg_index);

			return (DAT_ABORT);
		}

		LOGPRINTF("%d %s RCV buffer %p contains: 0x%x len=%d\n",
			  getpid(), server ? "SERVER:" : "CLIENT:",
			  rcv_buf, *rcv_buf, buf_len);

		burst_msg_index++;

		/* If server, change data and send it back to client */
		if (server) {
			*snd_buf = 0xaa;

			LOGPRINTF("%d %s SND buffer %p contains: 0x%x len=%d\n",
				  getpid(), server ? "SERVER:" : "CLIENT:",
				  snd_buf, *snd_buf, buf_len);

			ret = send_msg(snd_buf,
				       buf_len,
				       lmr_context_send,
				       cookie, DAT_COMPLETION_SUPPRESS_FLAG);

			if (ret != DAT_SUCCESS) {
				fprintf(stderr, "%d Error send_msg: %s\n",
					getpid(), DT_RetToStr(ret));
				return (ret);
			} else {
				LOGPRINTF("%d send_msg completed\n", getpid());
			}
		}

		/* next buffers */
		rcv_buf += buf_len;
		snd_buf += buf_len;
	}
	stop = get_time();
	ts.rtt = ((stop - start) * 1.0e6);

	return (DAT_SUCCESS);
}

/* Register RDMA Receive buffer */
DAT_RETURN register_rdma_memory(void)
{
	DAT_RETURN ret;
	DAT_REGION_DESCRIPTION region;

	region.for_va = rbuf;
	start = get_time();
	ret = dat_lmr_create(h_ia,
			     DAT_MEM_TYPE_VIRTUAL,
			     region,
			     buf_len * (burst+1),
			     h_pz,
			     DAT_MEM_PRIV_ALL_FLAG,
			     DAT_VA_TYPE_VA,
			     &h_lmr_recv,
			     &lmr_context_recv,
			     &rmr_context_recv,
			     &registered_size_recv, &registered_addr_recv);
	stop = get_time();
	ts.reg += ((stop - start) * 1.0e6);
	ts.total += ts.reg;

	if (ret != DAT_SUCCESS) {
		fprintf(stderr,
			"%d Error registering Receive RDMA buffer: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else {
		LOGPRINTF("%d Registered Receive RDMA Buffer %p\n",
			  getpid(), region.for_va);
	}

	/* Register RDMA Send buffer */
	region.for_va = sbuf;
	ret = dat_lmr_create(h_ia,
			     DAT_MEM_TYPE_VIRTUAL,
			     region,
			     buf_len * (burst + 1),
			     h_pz,
			     DAT_MEM_PRIV_ALL_FLAG,
			     DAT_VA_TYPE_VA,
			     &h_lmr_send,
			     &lmr_context_send,
			     &rmr_context_send,
			     &registered_size_send, &registered_addr_send);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error registering send RDMA buffer: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else {
		LOGPRINTF("%d Registered Send RDMA Buffer %p\n",
			  getpid(), region.for_va);
	}

	return DAT_SUCCESS;
}

/*
 * Unregister RDMA memory
 */
DAT_RETURN unregister_rdma_memory(void)
{
	DAT_RETURN ret;

	/* Unregister Recv Buffer */
	if (h_lmr_recv != DAT_HANDLE_NULL) {
		LOGPRINTF("%d Unregister h_lmr %p \n", getpid(), h_lmr_recv);
		start = get_time();
		ret = dat_lmr_free(h_lmr_recv);
		stop = get_time();
		ts.unreg += ((stop - start) * 1.0e6);
		ts.total += ts.unreg;
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error deregistering recv mr: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else {
			LOGPRINTF("%d Unregistered Recv Buffer\n", getpid());
			h_lmr_recv = NULL;
		}
	}

	/* Unregister Send Buffer */
	if (h_lmr_send != DAT_HANDLE_NULL) {
		LOGPRINTF("%d Unregister h_lmr %p \n", getpid(), h_lmr_send);
		ret = dat_lmr_free(h_lmr_send);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error deregistering send mr: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else {
			LOGPRINTF("%d Unregistered send Buffer\n", getpid());
			h_lmr_send = NULL;
		}
	}
	return DAT_SUCCESS;
}

 /*
  * Create CNO, CR, CONN, and DTO events
  */
DAT_RETURN create_events(void)
{
	DAT_RETURN ret;
	DAT_EVD_PARAM param;

	/* create CNO */
	if (use_cno) {
		start = get_time();
#if defined(_WIN32) || defined(_WIN64)
		{
			DAT_OS_WAIT_PROXY_AGENT pa = { NULL, NULL };
			ret = dat_cno_create(h_ia, pa, &h_dto_cno);
		}
#else
		ret =
		    dat_cno_create(h_ia, DAT_OS_WAIT_PROXY_AGENT_NULL,
				   &h_dto_cno);
#endif
		stop = get_time();
		ts.cnoc += ((stop - start) * 1.0e6);
		ts.total += ts.cnoc;
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error dat_cno_create: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else {
			LOGPRINTF("%d cr_evd created, %p\n", getpid(),
				  h_dto_cno);
		}
	}

	/* create cr EVD */
	start = get_time();
	ret =
	    dat_evd_create(h_ia, 10, DAT_HANDLE_NULL, DAT_EVD_CR_FLAG,
			   &h_cr_evd);
	stop = get_time();
	ts.evdc += ((stop - start) * 1.0e6);
	ts.total += ts.evdc;
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error dat_evd_create: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else {
		LOGPRINTF("%d cr_evd created %p\n", getpid(), h_cr_evd);
	}

	/* create conn EVD */
	ret = dat_evd_create(h_ia,
			     10,
			     DAT_HANDLE_NULL,
			     DAT_EVD_CONNECTION_FLAG, &h_conn_evd);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error dat_evd_create: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else {
		LOGPRINTF("%d con_evd created %p\n", getpid(), h_conn_evd);
	}

	/* create dto SND EVD, with CNO if use_cno was set */
	ret = dat_evd_create(h_ia,
			     (MSG_BUF_COUNT + MAX_RDMA_RD + burst) * 2,
			     h_dto_cno, DAT_EVD_DTO_FLAG, &h_dto_req_evd);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error dat_evd_create REQ: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else {
		LOGPRINTF("%d dto_req_evd created %p\n", getpid(),
			  h_dto_req_evd);
	}

	/* create dto RCV EVD, with CNO if use_cno was set */
	ret = dat_evd_create(h_ia,
			     MSG_BUF_COUNT + burst,
			     h_dto_cno, DAT_EVD_DTO_FLAG, &h_dto_rcv_evd);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error dat_evd_create RCV: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else {
		LOGPRINTF("%d dto_rcv_evd created %p\n", getpid(),
			  h_dto_rcv_evd);
	}

	/* query DTO req EVD and check size */
	ret = dat_evd_query(h_dto_req_evd, DAT_EVD_FIELD_EVD_QLEN, &param);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d Error dat_evd_query request evd: %s\n",
			getpid(), DT_RetToStr(ret));
		return (ret);
	} else if (param.evd_qlen < (MSG_BUF_COUNT + MAX_RDMA_RD + burst) * 2) {
		fprintf(stderr, "%d Error dat_evd qsize too small: %d < %d\n",
			getpid(), param.evd_qlen,
			(MSG_BUF_COUNT + MAX_RDMA_RD + burst) * 2);
		return (ret);
	}

	LOGPRINTF("%d dto_req_evd QLEN - requested %d and actual %d\n",
		  getpid(), (MSG_BUF_COUNT + MAX_RDMA_RD + burst) * 2,
		  param.evd_qlen);

	return DAT_SUCCESS;
}

/*
 * Destroy CR, CONN, CNO, and DTO events
 */

DAT_RETURN destroy_events(void)
{
	DAT_RETURN ret;

	/* free cr EVD */
	if (h_cr_evd != DAT_HANDLE_NULL) {
		LOGPRINTF("%d Free cr EVD %p \n", getpid(), h_cr_evd);
		ret = dat_evd_free(h_cr_evd);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error freeing cr EVD: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else {
			LOGPRINTF("%d Freed cr EVD\n", getpid());
			h_cr_evd = DAT_HANDLE_NULL;
		}
	}

	/* free conn EVD */
	if (h_conn_evd != DAT_HANDLE_NULL) {
		LOGPRINTF("%d Free conn EVD %p \n", getpid(), h_conn_evd);
		ret = dat_evd_free(h_conn_evd);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error freeing conn EVD: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else {
			LOGPRINTF("%d Freed conn EVD\n", getpid());
			h_conn_evd = DAT_HANDLE_NULL;
		}
	}

	/* free RCV dto EVD */
	if (h_dto_rcv_evd != DAT_HANDLE_NULL) {
		LOGPRINTF("%d Free RCV dto EVD %p \n", getpid(), h_dto_rcv_evd);
		start = get_time();
		ret = dat_evd_free(h_dto_rcv_evd);
		stop = get_time();
		ts.evdf += ((stop - start) * 1.0e6);
		ts.total += ts.evdf;
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error freeing dto EVD: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else {
			LOGPRINTF("%d Freed dto EVD\n", getpid());
			h_dto_rcv_evd = DAT_HANDLE_NULL;
		}
	}

	/* free REQ dto EVD */
	if (h_dto_req_evd != DAT_HANDLE_NULL) {
		LOGPRINTF("%d Free REQ dto EVD %p \n", getpid(), h_dto_req_evd);
		ret = dat_evd_free(h_dto_req_evd);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error freeing dto EVD: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else {
			LOGPRINTF("%d Freed dto EVD\n", getpid());
			h_dto_req_evd = DAT_HANDLE_NULL;
		}
	}

	/* free CNO */
	if (h_dto_cno != DAT_HANDLE_NULL) {
		LOGPRINTF("%d Free dto CNO %p \n", getpid(), h_dto_cno);
		start = get_time();
		ret = dat_cno_free(h_dto_cno);
		stop = get_time();
		ts.cnof += ((stop - start) * 1.0e6);
		ts.total += ts.cnof;
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, "%d Error freeing dto CNO: %s\n",
				getpid(), DT_RetToStr(ret));
			return (ret);
		} else {
			LOGPRINTF("%d Freed dto CNO\n", getpid());
			h_dto_cno = DAT_HANDLE_NULL;
		}
	}
	return DAT_SUCCESS;
}

/*
 * Map DAT_RETURN values to readable strings,
 * but don't assume the values are zero-based or contiguous.
 */
char errmsg[512] = { 0 };
const char *DT_RetToStr(DAT_RETURN ret_value)
{
	const char *major_msg, *minor_msg;

	dat_strerror(ret_value, &major_msg, &minor_msg);

	strcpy(errmsg, major_msg);
	strcat(errmsg, " ");
	strcat(errmsg, minor_msg);

	return errmsg;
}

/*
 * Map DAT_EVENT_CODE values to readable strings
 */
const char *DT_EventToStr(DAT_EVENT_NUMBER event_code)
{
	unsigned int i;
	static struct {
		const char *name;
		DAT_RETURN value;
	} dat_events[] = {
#   define DATxx(x) { # x, x }
		DATxx(DAT_DTO_COMPLETION_EVENT),
		    DATxx(DAT_RMR_BIND_COMPLETION_EVENT),
		    DATxx(DAT_CONNECTION_REQUEST_EVENT),
		    DATxx(DAT_CONNECTION_EVENT_ESTABLISHED),
		    DATxx(DAT_CONNECTION_EVENT_PEER_REJECTED),
		    DATxx(DAT_CONNECTION_EVENT_NON_PEER_REJECTED),
		    DATxx(DAT_CONNECTION_EVENT_ACCEPT_COMPLETION_ERROR),
		    DATxx(DAT_CONNECTION_EVENT_DISCONNECTED),
		    DATxx(DAT_CONNECTION_EVENT_BROKEN),
		    DATxx(DAT_CONNECTION_EVENT_TIMED_OUT),
		    DATxx(DAT_CONNECTION_EVENT_UNREACHABLE),
		    DATxx(DAT_ASYNC_ERROR_EVD_OVERFLOW),
		    DATxx(DAT_ASYNC_ERROR_IA_CATASTROPHIC),
		    DATxx(DAT_ASYNC_ERROR_EP_BROKEN),
		    DATxx(DAT_ASYNC_ERROR_TIMED_OUT),
		    DATxx(DAT_ASYNC_ERROR_PROVIDER_INTERNAL_ERROR),
		    DATxx(DAT_SOFTWARE_EVENT)
#   undef DATxx
	};
#   define NUM_EVENTS (sizeof(dat_events)/sizeof(dat_events[0]))

	for (i = 0; i < NUM_EVENTS; i++) {
		if (dat_events[i].value == event_code) {
			return (dat_events[i].name);
		}
	}

	return ("Invalid_DAT_EVENT_NUMBER");
}

void print_usage(void)
{
	printf("\n DAPL USAGE \n\n");
	printf("s: server\n");
	printf("t: performance times\n");
	printf("c: use cno\n");
	printf("v: verbose\n");
	printf("p: polling\n");
	printf("d: delay before accept\n");
	printf("b: buf length to allocate\n");
	printf("B: burst count, rdma and msgs \n");
	printf("h: hostname/address of server, specified on client\n");
	printf("P: provider name (default = OpenIB-cma)\n");
	printf("l: server lid (required ucm provider)\n");
	printf("q: server qpn (required ucm provider)\n");
	printf("\n");
}

