/*
 * Copyright (c) 2009 Intel Corporation.  All rights reserved.
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
#define F64d "%I64d"

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
#define F64d "%"PRId64""


#if __BYTE_ORDER == __BIG_ENDIAN
#define htonll(x) (x)
#define ntohll(x) (x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define htonll(x)  bswap_64(x)
#define ntohll(x)  bswap_64(x)
#endif

#endif // _WIN32 || _WIN64

#define MAX_POLLING_CNT 50000

/* Header files needed for DAT/uDAPL */
#include "dat2/udat.h"
#include "dat2/dat_ib_extensions.h"

/* definitions */
#define SERVER_CONN_QUAL  45248
#define CONN_TIMEOUT      (1000*1000*100)
#define CR_TIMEOUT	  DAT_TIMEOUT_INFINITE

/* Global DAT vars */
static DAT_IA_HANDLE h_ia = DAT_HANDLE_NULL;
static DAT_PZ_HANDLE h_pz = DAT_HANDLE_NULL;
static DAT_EP_HANDLE *h_ep;
static DAT_PSP_HANDLE *h_psp;
static DAT_CR_HANDLE h_cr = DAT_HANDLE_NULL;

static DAT_EVD_HANDLE h_async_evd = DAT_HANDLE_NULL;
static DAT_EVD_HANDLE h_dto_req_evd = DAT_HANDLE_NULL;
static DAT_EVD_HANDLE h_dto_rcv_evd = DAT_HANDLE_NULL;
static DAT_EVD_HANDLE h_cr_evd = DAT_HANDLE_NULL;
static DAT_EVD_HANDLE h_conn_evd = DAT_HANDLE_NULL;

static DAT_EP_ATTR ep_attr;
char hostname[256] = { 0 };
char provider[64] = DAPL_PROVIDER;
char addr_str[INET_ADDRSTRLEN];

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
	double rtt;
	double close;
	double conn;
};

struct dt_time ts;

/* defaults */
static int connected = 0;
static int multi_listens = 0;
static int ud_test = 0;
static int server = 1;
static int waiting = 0;
static int verbose = 0;
static int cr_poll_count = 0;
static int conn_poll_count = 0;
static int delay = 0;
static int connections = 1000;
static int burst = 100;
static int port_id = SERVER_CONN_QUAL;
static int ucm = 0;
static DAT_SOCK_ADDR6 remote;

/* forward prototypes */
const char *DT_RetToString(DAT_RETURN ret_value);
const char *DT_EventToSTr(DAT_EVENT_NUMBER event_code);
void print_usage(void);
double get_time(void);
DAT_RETURN conn_client(void);
DAT_RETURN conn_server(void);
DAT_RETURN disconnect_eps(void);
DAT_RETURN create_events(void);
DAT_RETURN destroy_events(void);

#define LOGPRINTF if (verbose) printf

void flush_evds(void)
{
	DAT_EVENT event;

	/* Flush async error queue */
	printf(" ERR: Checking ASYNC EVD...\n");
	while (dat_evd_dequeue(h_async_evd, &event) == DAT_SUCCESS) {
		printf(" ASYNC EVD ENTRY: handle=%p reason=%d\n",
			event.event_data.asynch_error_event_data.dat_handle,
			event.event_data.asynch_error_event_data.reason);
	}
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
	int i, c, len;
	DAT_RETURN ret;
	DAT_IA_ATTR ia_attr;
	
	/* parse arguments */
	while ((c = getopt(argc, argv, "smwvub:c:d:h:P:p:q:l:")) != -1) {
		switch (c) {
		case 's':
			server = 1;
			break;
		case 'm':
			multi_listens = 1;
			break;
		case 'w':
			waiting = 1;
			break;
		case 'u':
			ud_test = 1;
			break;
		case 'c':
			connections = atoi(optarg);
			break;
		case 'p':
			port_id = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			fflush(stdout);
			break;
		case 'd':
			delay = atoi(optarg);
			break;
		case 'b':
			burst = atoi(optarg);
			break;
		case 'h':
			server = 0;
			strcpy(hostname, optarg);
			break;
		case 'P':
			strcpy(provider, optarg);
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
		printf(" Running client on %s with %d %s connections\n", 
			provider, connections, ud_test ? "UD" : "RC");
	} else {
		printf(" Running server on %s with %d %s connections\n", 
			provider, connections, ud_test ? "UD" : "RC");
	}
	fflush(stdout);
	
	if (burst > connections)
		burst = connections;

	
	/* allocate EP handles for all connections */
	h_ep = (DAT_EP_HANDLE*)malloc(connections * sizeof(DAT_EP_HANDLE));
	if (h_ep == NULL) {	
		perror("malloc ep");
		exit(1);
	}
	memset(h_ep, 0, (burst * sizeof(DAT_PSP_HANDLE)));
	
	/* allocate PSP handles, check for multi-listens */
	if (multi_listens)
		len = burst * sizeof(DAT_PSP_HANDLE);
	else
		len = sizeof(DAT_PSP_HANDLE);

	h_psp = (DAT_PSP_HANDLE*)malloc(len);
	if (h_psp == NULL) {	
		perror("malloc psp");
		exit(1);
	}
	memset(h_psp, 0, len);
	memset(&ts, 0, sizeof(struct dt_time));

	/* dat_ia_open, dat_pz_create */
	h_async_evd = DAT_HANDLE_NULL;
	start = get_time();
	ret = dat_ia_open(provider, 8, &h_async_evd, &h_ia);
	stop = get_time();
	ts.open += ((stop - start) * 1.0e6);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error Adaptor open: %s\n",
			DT_RetToString(ret));
		exit(1);
	} else
		LOGPRINTF(" Opened Interface Adaptor\n");

	/* query for UCM addressing */
	ret = dat_ia_query(h_ia, 0, DAT_IA_FIELD_ALL, &ia_attr, 0, 0);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, "%d: Error Adaptor query: %s\n",
			getpid(), DT_RetToString(ret));
		exit(1);
	}
	print_ia_address(ia_attr.ia_address_ptr);

	/* Create Protection Zone */
	start = get_time();
	LOGPRINTF(" Create Protection Zone\n");
	ret = dat_pz_create(h_ia, &h_pz);
	stop = get_time();
	ts.pzc += ((stop - start) * 1.0e6);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error creating Protection Zone: %s\n",
			DT_RetToString(ret));
		exit(1);
	} else
		LOGPRINTF(" Created Protection Zone\n");

	LOGPRINTF(" Create events\n");
	ret = create_events();
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error creating events: %s\n",
			DT_RetToString(ret));
		goto cleanup;
	} else {
		LOGPRINTF(" Create events done\n");
	}

	/* create EP */
	memset(&ep_attr, 0, sizeof(ep_attr));
	if (ud_test) {
		ep_attr.service_type = DAT_IB_SERVICE_TYPE_UD;
		ep_attr.max_message_size = 2048;
	} else {
		ep_attr.service_type = DAT_SERVICE_TYPE_RC;
		ep_attr.max_rdma_size = 0x10000;
		ep_attr.max_rdma_read_in = 4;
		ep_attr.max_rdma_read_out = 4;
	}
	ep_attr.max_recv_dtos = 1;
	ep_attr.max_request_dtos = 1;
	ep_attr.max_recv_iov = 1;
	ep_attr.max_request_iov = 1;
	ep_attr.request_completion_flags = DAT_COMPLETION_DEFAULT_FLAG;
	
	start = get_time();
	for (i = 0; i < connections; i++) {
		ret = dat_ep_create(h_ia, h_pz, h_dto_rcv_evd,
				    h_dto_req_evd, h_conn_evd, 
				    &ep_attr, &h_ep[i]);
	}
	stop = get_time();
	ts.epc += ((stop - start) * 1.0e6);
	ts.total += ts.epc;
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error dat_ep_create: %s\n",
			DT_RetToString(ret));
		goto cleanup;
	} else
		LOGPRINTF(" EP created %p \n", h_ep[i]);

	/* create the service point for server listen */
	if (server) {
		LOGPRINTF(" Creating server service point(s)\n");
		for (i = 0; i < burst; i++) {
			ret = dat_psp_create(h_ia,
					     port_id+i,
					     h_cr_evd, 
					     DAT_PSP_CONSUMER_FLAG, 
					     &h_psp[i]);

			if (ret != DAT_SUCCESS) {
				fprintf(stderr, " ERR psp_create: %s\n",
					DT_RetToString(ret));
				goto cleanup;
			} else
				LOGPRINTF(" psp_created for listen\n");

			printf(" Server ready on port %d\n", 
				port_id+i);

			if (!multi_listens)
				break;
		}
	}
	
	/* Connect all */
	if (server)
		ret = conn_server();
	else	
		ret = conn_client();
	
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error %s: %s\n",
			 server ? "server()" : "client()",
			DT_RetToString(ret));
		goto cleanup;
	} else
		LOGPRINTF(" connect_ep complete\n");

	connected = 1;
	goto complete;

cleanup:
	flush_evds();
	goto bail;
complete:

	/* disconnect and free EP resources */
	if (h_ep[0]) {
		/* unregister message buffers and tear down connection */
		LOGPRINTF(" Disconnect EPs\n");
		ret = disconnect_eps();
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, " Error disconnect_eps: %s\n",
				DT_RetToString(ret));
			goto bail;
		} else {
			LOGPRINTF(" disconnect_eps complete\n");
		}
	}

	/* destroy server service point(s) */
	if ((server) && (h_psp[0] != DAT_HANDLE_NULL)) {
		for (i = 0; i < burst; i++) {
			ret = dat_psp_free(h_psp[i]);
			if (ret != DAT_SUCCESS) {
				fprintf(stderr, " Error dat_psp_free: %s\n",
					DT_RetToString(ret));
				goto bail;
			} else {
				LOGPRINTF(" psp_free[%d] complete\n",i);
			}
			if (!multi_listens)
				break;
		}
	}

	/* free EVDs */
	LOGPRINTF(" destroy events\n");
	ret = destroy_events();
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error destroy_events: %s\n",
			DT_RetToString(ret));
		goto bail;
	} else
		LOGPRINTF(" destroy events done\n");


	/* Free protection domain */
	LOGPRINTF(" Freeing pz\n");
	start = get_time();
	ret = dat_pz_free(h_pz);
	stop = get_time();
	ts.pzf += ((stop - start) * 1.0e6);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error freeing PZ: %s\n",
			DT_RetToString(ret));
		goto bail;
	} else {
		LOGPRINTF(" Freed pz\n");
		h_pz = NULL;
	}

	/* close the device */
	LOGPRINTF(" Closing Interface Adaptor\n");
	start = get_time();
	ret = dat_ia_close(h_ia, DAT_CLOSE_ABRUPT_FLAG);
	stop = get_time();
	ts.close += ((stop - start) * 1.0e6);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error Adaptor close: %s\n",
			DT_RetToString(ret));
		goto bail;
	} else
		LOGPRINTF(" Closed Interface Adaptor\n");

	printf(" DAPL Connection Test Complete.\n");
	printf(" open:      %10.2lf usec\n", ts.open);
	printf(" close:     %10.2lf usec\n", ts.close);
	printf(" PZ create: %10.2lf usec\n", ts.pzc);
	printf(" PZ free:   %10.2lf usec\n", ts.pzf);
	printf(" LMR create:%10.2lf usec\n", ts.reg);
	printf(" LMR free:  %10.2lf usec\n", ts.unreg);
	printf(" EVD create:%10.2lf usec\n", ts.evdc);
	printf(" EVD free:  %10.2lf usec\n", ts.evdf);
	printf(" EP create: %10.2lf usec avg\n", ts.epc/connections);
	printf(" EP free:   %10.2lf usec avg\n", ts.epf/connections);
	if (!server) {
		printf(" Connections: %8.2lf usec, CPS %7.2lf "
			"Total %4.2lf secs, poll_cnt=%u, Num=%d\n", 
		       (double)(ts.conn/connections), 
		       (double)(1/(ts.conn/1000000/connections)), 
		       (double)(ts.conn/1000000), 
		       conn_poll_count, connections);
	}
	printf(" TOTAL:     %4.2lf sec\n",  ts.total/1000000);
	fflush(stderr);	fflush(stdout);
bail:
	free(h_ep);
	free(h_psp);

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

DAT_RETURN conn_server()
{
	DAT_RETURN ret;
	DAT_EVENT event;
	DAT_COUNT nmore;
	int i,bi;
	unsigned char *buf;
	DAT_CR_ARRIVAL_EVENT_DATA *cr_event =
		&event.event_data.cr_arrival_event_data;
	DAT_CR_PARAM cr_param = { 0 };
	
	printf(" Accepting...\n");
	for (i = 0; i < connections; i++) {
					
		/* poll for CR's */
		if (!waiting) {
			cr_poll_count = 0;
			while (DAT_GET_TYPE(dat_evd_dequeue(h_cr_evd, &event)) 
					== DAT_QUEUE_EMPTY)
				cr_poll_count++;
		} else {
			ret = dat_evd_wait(h_cr_evd, CR_TIMEOUT, 
					   1, &event, &nmore);
			if (ret != DAT_SUCCESS) {
				fprintf(stderr,
					" ERR: CR dat_evd_wait() %s\n",
					 DT_RetToString(ret));
				return ret;
			}
		}
	
		if ((event.event_number != DAT_CONNECTION_REQUEST_EVENT) &&
		    (ud_test && event.event_number != 
		     DAT_IB_UD_CONNECTION_REQUEST_EVENT)) {
				fprintf(stderr, " Error unexpected CR event : %s\n",
					DT_EventToSTr(event.event_number));
				return (DAT_ABORT);
		}

		
		/* use to test rdma_cma timeout logic */
#if defined(_WIN32) || defined(_WIN64)
		if (delay) {
			printf(" Accept delayed by %d seconds...\n", delay);
			Sleep(delay * 1000);
		}
#else
		if (delay) {
			printf(" Accept delayed by %d seconds...\n", delay);
			sleep(delay);
		}
#endif
		/* accept connect request from client */
		h_cr = cr_event->cr_handle;
		LOGPRINTF(" Accepting connect request from client\n");

		/* private data - check and send it back */
		dat_cr_query(h_cr, DAT_CSP_FIELD_ALL, &cr_param);

		buf = (unsigned char *)cr_param.private_data;
		LOGPRINTF(" CONN REQUEST Private Data %p[0]=%d [47]=%d\n",
			   buf, buf[0], buf[47]);
		
		for (bi = 0; bi < 48; bi++) {
			if (buf[bi] != bi + 1) {
				fprintf(stderr, " ERR on CONNECT REQUEST"
					" private data: %p[%d]=%d s/be %d\n",
					 buf, bi, buf[bi], bi + 1);
				dat_cr_reject(h_cr, 0, NULL);
				return (DAT_ABORT);
			}
			buf[bi]++;	/* change for trip back */
		}

#ifdef TEST_REJECT_WITH_PRIVATE_DATA
		printf(" REJECT request with 48 bytes of private data\n");
		ret = dat_cr_reject(h_cr, 48, cr_param.private_data);
		printf("\n DAPL Test Complete. %s\n\n",
		        ret ? "FAILED" : "PASSED");
		exit(0);
#endif
		ret = dat_cr_accept(h_cr, h_ep[i], 48, 
				    cr_param.private_data);

		if (ret != DAT_SUCCESS) {
			fprintf(stderr, " ERR dat_cr_accept: %s\n",
				DT_RetToString(ret));
			return (ret);
		} else
			LOGPRINTF(" accept[%d] complete\n", i);
	
		event.event_number = 0;
	}

	/* process the RTU, ESTABLISHMENT event */
	printf(" Completing...\n");
        for (i=0;i<connections;i++) {
		
		/* process completions */
		if (!waiting) {
			conn_poll_count = 0;
			while (DAT_GET_TYPE(dat_evd_dequeue(h_conn_evd, 
					    &event)) == DAT_QUEUE_EMPTY)
				conn_poll_count++;
		} else {
			ret = dat_evd_wait(h_conn_evd, CONN_TIMEOUT, 
					   1, &event, &nmore);
			if (ret != DAT_SUCCESS) {
				fprintf(stderr,
					" ERR: CONN evd_wait() %s\n",
					 DT_RetToString(ret));
				return ret;
			}
		}
		if ((event.event_number != DAT_CONNECTION_EVENT_ESTABLISHED) &&
		    (ud_test && event.event_number != 
		     DAT_IB_UD_CONNECTION_EVENT_ESTABLISHED)) {

			fprintf(stderr, " Error unexpected CR EST "
				"event : 0x%x %s\n",
				 event.event_number,
				DT_EventToSTr(event.event_number));
			return (DAT_ABORT);
		}
		event.event_number = 0;
		LOGPRINTF(" CONN_EST[%d] complete\n", i);
	}

	printf("\n ALL %d CONNECTED on Server!\n\n", connections);
	return DAT_SUCCESS;
}

	
DAT_RETURN conn_client() 
{
	DAT_IA_ADDRESS_PTR raddr = (DAT_IA_ADDRESS_PTR)&remote;
	DAT_RETURN ret;
	DAT_EVENT event;
	DAT_COUNT nmore;
	DAT_CONN_QUAL conn_id;
	DAT_CONNECTION_EVENT_DATA *conn_event =
		&event.event_data.connect_event_data;
	int i,ii,bi;
	unsigned char *buf;
	unsigned char pdata[48] = { 0 };
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
	printf(" Connecting to Server: %s \n",  hostname);
	printf(" Address: %d.%d.%d.%d port %d\n",
		(rval >> 0) & 0xff, (rval >> 8) & 0xff,
		(rval >> 16) & 0xff, (rval >> 24) & 0xff, 
		port_id);

	raddr = (DAT_IA_ADDRESS_PTR)target->ai_addr;
	
no_resolution:

	for (i = 0; i < 48; i++) /* simple pattern in private data */
		pdata[i] = i + 1;

       	printf(" Connecting...\n");
	start = get_time();
	for (i = 0; i < connections; i += burst) {
		for (ii = 0; ii < burst; ii++) { /* conn_reqs */
			if (multi_listens)
				conn_id = port_id + ii;
			else
				conn_id = port_id;

			ret = dat_ep_connect(h_ep[i+ii], raddr, 
					     conn_id, CONN_TIMEOUT,
					     48, (DAT_PVOID) pdata, 0, 
					     DAT_CONNECT_DEFAULT_FLAG);
			if (ret != DAT_SUCCESS) {
				fprintf(stderr, " ERR dat_ep_connect: %s\n",
					DT_RetToString(ret));
				return (ret);
			} else
				LOGPRINTF(" dat_ep_connect[%d] complete\n", 
					  i+ii);

		} 
		for (ii = 0; ii < burst; ii++) { /* conn_events */
			if (!waiting) {
				conn_poll_count = 0;
				while (DAT_GET_TYPE(dat_evd_dequeue(
					h_conn_evd, &event)) == 
					DAT_QUEUE_EMPTY)
					conn_poll_count++;
			} else {
				ret = dat_evd_wait(h_conn_evd, CONN_TIMEOUT, 
						   1, &event, &nmore);

				if (ret != DAT_SUCCESS) {
					fprintf(stderr,
						" ERR: CONN evd_wait() %s\n",
						DT_RetToString(ret));
					return ret;
				}
			}

#ifdef TEST_REJECT_WITH_PRIVATE_DATA
			if (event.event_number != 
				DAT_CONNECTION_EVENT_PEER_REJECTED) {
				fprintf(stderr, " expected conn reject "
					"event : %s\n",
					DT_EventToSTr(event.event_number));
				return (DAT_ABORT);
			}

			/* get the reject private data and validate */
			buf = (unsigned char *)conn_event->private_data;
			printf(" Recv REJ with pdata %p[0]=%d [47]=%d\n",
			 buf, buf[0], buf[47]);
			for (bi = 0; bi < 48; bi++) {
				if (buf[bi] != idx + 2) {
					fprintf(stderr, " client: Error"
						" with REJECT event private"
						" data: %p[%d]=%d s/be %d\n",
						 buf, bi, 
						buf[bi], bi + 2);
					dat_ep_disconnect(h_ep[i+ii], 0);
					return (DAT_ABORT);
				}
			}
			printf("\n Rej Test Done. PASSED\n\n");
			exit(0);
#endif
			if ((event.event_number != 
			    DAT_CONNECTION_EVENT_ESTABLISHED) &&
			    (ud_test && event.event_number != 
			    DAT_IB_UD_CONNECTION_EVENT_ESTABLISHED)) {
				fprintf(stderr, " Error unexpected conn "
					"event : 0x%x %s\n",
					 event.event_number,
					DT_EventToSTr(event.event_number));
				return (DAT_ABORT);
			}

			/* check private data back from server  */
			buf = (unsigned char *)conn_event->private_data;

			LOGPRINTF(" CONN[%d] Private Data "
				  "%p[0]=%d [47]=%d\n",
				   i+ii, buf, buf[0], buf[47]);

			for (bi = 0; bi < 48; bi++) {
				if (buf[bi] != bi + 2) {
					DAT_COUNT nmore;
					fprintf(stderr, " ERR CONN event"
						" pdata: %p[%d]=%d s/be %d\n",
						 buf, bi, buf[bi], 
						bi + 2);
					dat_ep_disconnect(h_ep[i+ii], 
						DAT_CLOSE_ABRUPT_FLAG);
					LOGPRINTF(" waiting for disc...\n");
					dat_evd_wait(h_conn_evd, 
						     DAT_TIMEOUT_INFINITE,
						     1, &event, &nmore); 
					return (DAT_ABORT);
				}
			}
			event.event_number = 0;
		} 
	}

      	stop = get_time();
       	ts.conn += ((stop - start) * 1.0e6);

	if (!ucm)
		freeaddrinfo(target);

	printf("\n ALL %d CONNECTED on Client!\n\n",  connections);

	return (DAT_SUCCESS);
}

/* validate disconnected EP's and free them */
DAT_RETURN disconnect_eps(void)
{
	DAT_RETURN ret;
	DAT_EVENT event;
	DAT_COUNT nmore;
	int i,ii;
	DAT_CONNECTION_EVENT_DATA *conn_event =
		&event.event_data.connect_event_data;

	if (!connected)
		return DAT_SUCCESS;

	/* UD, no connection to disconnect, just free EP's */
	if (ud_test) {
		for (i = 0; i < connections; i++) {
			ret = dat_ep_free(h_ep[i]);
			if (ret != DAT_SUCCESS) {
				fprintf(stderr, 
					" ERR free EP[%d] %p: %s\n",
					i, h_ep[i], DT_RetToString(ret));
			} else {
				LOGPRINTF(" Freed EP[%d] %p\n", 
					  i, h_ep[i]);
				h_ep[i] = DAT_HANDLE_NULL;
			}
		}
		stop = get_time();
		ts.epf += ((stop - start) * 1.0e6);
		ts.total += ts.epf;
		return DAT_SUCCESS;
	}
	
	/* 
	 * Only the client needs to call disconnect. The server _should_ be able
	 * to just wait on the EVD associated with connection events for a
	 * disconnect request and then exit.
	 */
	if (!server) {
		start = get_time();
		for (i = 0; i < connections; i++) {
			LOGPRINTF(" dat_ep_disconnect\n");
			ret = dat_ep_disconnect(h_ep[i], 
						DAT_CLOSE_DEFAULT);
			if (ret != DAT_SUCCESS) {
				fprintf(stderr,
					" Error disconnect: %s\n",
						DT_RetToString(ret));
				return ret;
			} else {
				LOGPRINTF(" disconnect completed\n");
			}
		}
	} else {
		LOGPRINTF(" Server waiting for disconnect...\n");
	}

	LOGPRINTF(" Wait for Disc event, free EPs as completed\n");
	start = get_time();
	for (i = 0; i < connections; i++) {
		event.event_number = 0;
		conn_event->ep_handle = NULL;
		ret = dat_evd_wait(h_conn_evd, DAT_TIMEOUT_INFINITE, 
				   1, &event, &nmore);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, " Error dat_evd_wait: %s\n",
				DT_RetToString(ret));
			return ret;
		} else {
			LOGPRINTF(" disc event[%d] complete,"
				  " check for valid EP...\n", i);
		}

		/* check for valid EP in creation list */
		for (ii = 0; ii < connections; ii++) {
			if (h_ep[ii] == conn_event->ep_handle) {
				LOGPRINTF(" valid EP[%d] %p !\n", 
					  ii, h_ep[ii]);
				ret = dat_ep_free(h_ep[ii]);
				if (ret != DAT_SUCCESS) {
					fprintf(stderr, 
						" ERR free EP[%d] %p: %s\n",
						i, h_ep[ii], 
						DT_RetToString(ret));
				} else {
					LOGPRINTF(" Freed EP[%d] %p\n", 
						  i, h_ep[ii]);
					h_ep[ii] = DAT_HANDLE_NULL;
				}
				break;
			} else {
				continue;
			}
		}
		if (ii == connections) {
			LOGPRINTF(" %s: invalid EP[%d] %p via DISC event!\n", 
				  server ? "Server" : "Client", 
				  i, conn_event->ep_handle);
			return DAT_INVALID_HANDLE;
		}
	}
	/* free EPs */
	stop = get_time();
	ts.epf += ((stop - start) * 1.0e6);
	ts.total += ts.epf;
	return DAT_SUCCESS;
}


 /*
  * Create CR, CONN, and DTO events
  */
DAT_RETURN create_events(void)
{
	DAT_RETURN ret;

	/* create cr EVD */
	start = get_time();
	ret = dat_evd_create(h_ia, connections, DAT_HANDLE_NULL, 
			     DAT_EVD_CR_FLAG, &h_cr_evd);
	stop = get_time();
	ts.evdc += ((stop - start) * 1.0e6);
	ts.total += ts.evdc;
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error dat_evd_create: %s\n",
			 DT_RetToString(ret));
		return (ret);
	} else {
		LOGPRINTF(" cr_evd created %p\n",  h_cr_evd);
	}

	/* create conn EVD */
	ret = dat_evd_create(h_ia,
			     connections*2,
			     DAT_HANDLE_NULL,
			     DAT_EVD_CONNECTION_FLAG, &h_conn_evd);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error dat_evd_create: %s\n",
			 DT_RetToString(ret));
		return (ret);
	} else {
		LOGPRINTF(" con_evd created %p\n",  h_conn_evd);
	}

	/* create dto SND EVD */
	ret = dat_evd_create(h_ia, 1, NULL, 
			     DAT_EVD_DTO_FLAG, &h_dto_req_evd);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error dat_evd_create REQ: %s\n",
			 DT_RetToString(ret));
		return (ret);
	} else {
		LOGPRINTF(" dto_req_evd created %p\n", 
			  h_dto_req_evd);
	}

	/* create dto RCV EVD */
	ret = dat_evd_create(h_ia, 1, NULL,
			     DAT_EVD_DTO_FLAG, &h_dto_rcv_evd);
	if (ret != DAT_SUCCESS) {
		fprintf(stderr, " Error dat_evd_create RCV: %s\n",
			 DT_RetToString(ret));
		return (ret);
	} else {
		LOGPRINTF(" dto_rcv_evd created %p\n", 
			  h_dto_rcv_evd);
	}
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
		LOGPRINTF(" Free cr EVD %p \n",  h_cr_evd);
		ret = dat_evd_free(h_cr_evd);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, " Error freeing cr EVD: %s\n",
				 DT_RetToString(ret));
			return (ret);
		} else {
			LOGPRINTF(" Freed cr EVD\n");
			h_cr_evd = DAT_HANDLE_NULL;
		}
	}

	/* free conn EVD */
	if (h_conn_evd != DAT_HANDLE_NULL) {
		LOGPRINTF(" Free conn EVD %p\n",  h_conn_evd);
		ret = dat_evd_free(h_conn_evd);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, " Error freeing conn EVD: %s\n",
				 DT_RetToString(ret));
			return (ret);
		} else {
			LOGPRINTF(" Freed conn EVD\n");
			h_conn_evd = DAT_HANDLE_NULL;
		}
	}

	/* free RCV dto EVD */
	if (h_dto_rcv_evd != DAT_HANDLE_NULL) {
		LOGPRINTF(" Free RCV dto EVD %p\n",  h_dto_rcv_evd);
		start = get_time();
		ret = dat_evd_free(h_dto_rcv_evd);
		stop = get_time();
		ts.evdf += ((stop - start) * 1.0e6);
		ts.total += ts.evdf;
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, " Error freeing dto EVD: %s\n",
				 DT_RetToString(ret));
			return (ret);
		} else {
			LOGPRINTF(" Freed dto EVD\n");
			h_dto_rcv_evd = DAT_HANDLE_NULL;
		}
	}

	/* free REQ dto EVD */
	if (h_dto_req_evd != DAT_HANDLE_NULL) {
		LOGPRINTF(" Free REQ dto EVD %p\n",  h_dto_req_evd);
		ret = dat_evd_free(h_dto_req_evd);
		if (ret != DAT_SUCCESS) {
			fprintf(stderr, " Error freeing dto EVD: %s\n",
				 DT_RetToString(ret));
			return (ret);
		} else {
			LOGPRINTF(" Freed dto EVD\n");
			h_dto_req_evd = DAT_HANDLE_NULL;
		}
	}

	return DAT_SUCCESS;
}

/*
 * Map DAT_RETURN values to readable strings,
 * but don't assume the values are zero-based or contiguous.
 */
char errmsg[512] = { 0 };
const char *DT_RetToString(DAT_RETURN ret_value)
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
const char *DT_EventToSTr(DAT_EVENT_NUMBER event_code)
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
	printf("c: connections (default = 1000)\n");
	printf("v: verbose\n");
	printf("w: wait on event (default, polling)\n");
	printf("d: delay before accept\n");
	printf("h: hostname/address of server, specified on client\n");
	printf("P: provider name (default = OpenIB-v2-ib0)\n");
	printf("\n");
}

