/*
 * Copyright (c) 1999, Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netipx/ipx.h>
#include <errno.h>
#include <unistd.h>
#include "ipxsap.h"

/*
 * TODO: These should go to ipx headers
 */
#define ipx_set_net(x,y) ((x).x_net.s_net[0] = (y).x_net.s_net[0]); \
			 ((x).x_net.s_net[1]=(y).x_net.s_net[1])
#define ipx_set_nullnet(x) ((x).x_net.s_net[0]=0); ((x).x_net.s_net[1]=0)
#define ipx_set_nullhost(x) ((x).x_host.s_host[0] = 0); \
	((x).x_host.s_host[1] = 0); ((x).x_host.s_host[2] = 0)
#define ipx_set_wildnet(x)	((x).x_net.s_net[0] = 0xFFFF); \
				((x).x_net.s_net[1]=0xFFFF)
#define ipx_set_wildhost(x) ((x).x_host.s_host[0] = 0xFFFF); \
	((x).x_host.s_host[1] = 0xFFFF); ((x).x_host.s_host[2] = 0xFFFF);


static struct sap_packet* sap_packet_alloc(int entries);
static int sap_size(int entries, u_short operation);
int (*sap_sendto_func)(void*,int,struct sockaddr_ipx*,int sock)=NULL;

static int
sap_sendto(void* buffer, int size, struct sockaddr_ipx* daddr, int sock)
{ 
	if (sap_sendto_func)
		return sap_sendto_func(buffer,size,daddr,sock);
	return sendto(sock, (char*)buffer, size, 0,
	    (struct sockaddr*)daddr, sizeof(*daddr));
}

static struct sap_packet* 
sap_packet_alloc(int entries)
{
	if (entries > IPX_SAP_MAX_ENTRIES)
		return NULL;
	return 
	    (struct sap_packet*)malloc(sap_size(entries, IPX_SAP_GENERAL_RESPONSE));
}

static int 
sap_size(int entries, u_short operation)
{
	if (entries <= 0)
		return 0;
	switch (operation) {
	    case IPX_SAP_GENERAL_QUERY:
    		return entries == 1 ? IPX_SAP_REQUEST_LEN : 0;
	    case IPX_SAP_GENERAL_RESPONSE:
        	if (entries > IPX_SAP_MAX_ENTRIES)
			return 0;
    		return sizeof(struct sap_packet) + (entries - 1) * sizeof(struct sap_entry);
	    case IPX_SAP_NEAREST_QUERY:
                return entries == 1 ? IPX_SAP_REQUEST_LEN : 0;
	    case IPX_SAP_NEAREST_RESPONSE:
        	return entries == 1 ? sizeof(struct sap_packet) : 0;
	    default:
        	return 0;	
	}
}

void 
sap_copyname(char *dest, const char *src)
{
	bzero(dest, IPX_SAP_SERVER_NAME_LEN);
	strncpy(dest, src, IPX_SAP_SERVER_NAME_LEN - 1);
}

int
sap_rq_init(struct sap_rq* rq, int sock)
{
	rq->buffer = sap_packet_alloc(IPX_SAP_MAX_ENTRIES);
	if (rq->buffer == NULL)
		return 0;
	rq->entries = 0;
	rq->buffer->operation = htons(IPX_SAP_GENERAL_QUERY);
	rq->dest_addr.sipx_family = AF_IPX;
	rq->dest_addr.sipx_len = sizeof(struct sockaddr_ipx);
	rq->sock = sock;
	return 1;
}

int
sap_rq_flush(struct sap_rq* rq)
{
	int result;

	if (rq->entries == 0)
		return 0;
	result = sap_sendto(rq->buffer, 
		sap_size(rq->entries, ntohs(rq->buffer->operation)), 
		&rq->dest_addr, rq->sock);
	rq->entries = 0;
	return result;
}

void
sap_rq_general_query(struct sap_rq* rq, u_short ser_type)
{
	struct sap_entry* sep;

	sap_rq_flush(rq);
	rq->buffer->operation = htons(IPX_SAP_GENERAL_QUERY);
	sep = rq->buffer->sap_entries + rq->entries++;
	sep->server_type = htons(ser_type);
}

void
sap_rq_gns_request(struct sap_rq* rq, u_short ser_type)
{
	struct sap_entry* sep;

	sap_rq_flush(rq);
	rq->buffer->operation = htons(IPX_SAP_NEAREST_QUERY);
	sep = rq->buffer->sap_entries + rq->entries++;
	sep->server_type = htons(ser_type);
}

void
sap_rq_general_response(struct sap_rq* rq,u_short type,char *name,struct sockaddr_ipx* addr, u_short hops,int down_allow)
{
	struct sap_entry* sep;

	if (hops >= IPX_SAP_SERVER_DOWN && !down_allow) return;
	if (rq->entries >= IPX_SAP_MAX_ENTRIES)
		sap_rq_flush(rq);
	if (rq->buffer->operation != htons(IPX_SAP_GENERAL_RESPONSE)){
		sap_rq_flush(rq);
		rq->buffer->operation = htons(IPX_SAP_GENERAL_RESPONSE);
	}
	sep = rq->buffer->sap_entries + rq->entries;
	sep->server_type = htons(type);
	sap_copyname(sep->server_name, name);
	memcpy(&sep->ipx, &addr->sipx_addr, sizeof(struct ipx_addr));
	sep->hops = htons(hops);
	rq->entries++;
}

void
sap_rq_gns_response(struct sap_rq* rq,u_short type,char *name,struct sockaddr_ipx* addr,u_short hops)
{
	struct sap_entry* sep;

	if (hops >= IPX_SAP_SERVER_DOWN) return;
	sap_rq_flush(rq);
	rq->buffer->operation = htons(IPX_SAP_NEAREST_RESPONSE);
	sep = rq->buffer->sap_entries + rq->entries;
	sep->server_type = htons(type);
	sap_copyname(sep->server_name, name);
	memcpy(&sep->ipx, &addr->sipx_addr, sizeof(struct ipx_addr));
	sep->hops = htons(hops);
	rq->entries++;
}

void
sap_rq_set_destination(struct sap_rq* rq,struct ipx_addr *dest)
{
	sap_rq_flush(rq);
	memcpy(&rq->dest_addr.sipx_addr,dest,sizeof(struct ipx_addr));
}

int
sap_getsock(int *rsock) {
	struct sockaddr_ipx sap_addr;
	int opt, sock, slen;

	sock = socket(AF_IPX, SOCK_DGRAM, 0);
	if (sock < 0)
		return (errno);
	slen = sizeof(sap_addr);
	bzero(&sap_addr, slen);
	sap_addr.sipx_family = AF_IPX;
	sap_addr.sipx_len = slen;
	if (bind(sock, (struct sockaddr*)&sap_addr, slen) == -1) {
		close(sock);
		return(errno);
	}
	opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) != 0){
		close(sock);
		return(errno);
	}
	*rsock = sock;
	return(0);
}

static int
sap_recv(int sock,void *buf,int len,int flags, int timeout){
	fd_set rd, wr, ex;
	struct timeval tv;
	int result;

	FD_ZERO(&rd);
	FD_ZERO(&wr);
	FD_ZERO(&ex);
	FD_SET(sock, &rd);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	if ((result = select(sock + 1, &rd, &wr, &ex, &tv)) == -1) {
		return result;
	}
	if (FD_ISSET(sock, &rd)) {
		result = recv(sock, buf, len, flags);
	} else {
		errno = ETIMEDOUT;
		result = -1;
	}
	return result;
}

int
sap_find_nearest(int server_type, struct sockaddr_ipx *daddr, char *server_name)
{
	struct ipx_addr addr;
	char data[1024];
	int sock, error, packets, len;
	struct sap_packet *reply = (struct sap_packet*)&data;
	struct sap_rq sap_rq;
	
	error = sap_getsock(&sock);
	if (error)
		return error;
	bzero(&addr, sizeof(addr));
	/* BAD: we should enum all ifs (and nets ?) */
	if (ipx_iffind(NULL, &addr) != 0) {
		return (EPROTONOSUPPORT);
	}
	ipx_set_wildhost(addr);		
	addr.x_port = htons(IPXPORT_SAP);

	if (!sap_rq_init(&sap_rq, sock)) {
		close(sock);
		return(ENOMEM);
	}
	sap_rq_set_destination(&sap_rq, &addr);
	sap_rq_gns_request(&sap_rq, server_type);
	sap_rq_flush(&sap_rq);
	packets = 5;
	do {
		len = sap_recv(sock, data, sizeof(data), 0, 1);
		if (len >= 66 && 
		    ntohs(reply->operation) == IPX_SAP_NEAREST_RESPONSE)
			break;
		if (len < 0)
			packets--;
	} while (packets > 0);

	if (packets == 0) {
		close(sock);
		return ENETDOWN;
	}

	daddr->sipx_addr = reply->sap_entries[0].ipx;
	daddr->sipx_family = AF_IPX;
	daddr->sipx_len = sizeof(struct sockaddr_ipx);
	sap_copyname(server_name, reply->sap_entries[0].server_name);
	errno = 0;
	close(sock);
	return 0;
}
