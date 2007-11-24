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
 */
#ifndef _IPXSAP_H_
#define _IPXSAP_H_

#define IPX_SAP_GENERAL_QUERY		1
#define IPX_SAP_GENERAL_RESPONSE	2
#define IPX_SAP_NEAREST_QUERY		3
#define IPX_SAP_NEAREST_RESPONSE	4


#define IPX_SAP_MAX_ENTRIES		7
#define IPX_SAP_SERVER_DOWN		16
#define IPX_SAP_SERVER_NAME_LEN		48
#define IPX_SAP_REQUEST_LEN		4

/* Values for server_type */
#define IPX_SAP_FILE_SERVER		4

struct sap_query {
	u_short		query_type;	/* net order */
	u_short		server_type;	/* net order */
};

struct sap_entry {
	u_short		server_type;
	u_char		server_name[IPX_SAP_SERVER_NAME_LEN];
	struct ipx_addr	ipx;
	u_short		hops;
};

struct sap_packet {
	u_short		operation;
	struct sap_entry sap_entries[1];
};

struct sap_rq {
	struct sockaddr_ipx dest_addr;
	int		sock;
	int		entries;
	struct sap_packet* buffer;
};
/*
#define	sap_name_equal(n1,n2)	(strncmp(n1,n2,IPX_SAP_SERVER_NAME_LEN) == 0);
#define	sap_type_equal(t1,t2)	(t1==IPX_SAP_GENERAL_RQ || t2==IPX_SAP_GENERAL_RQ || t1==t2);
*/
void sap_copy_name(char *dest,char *src); 
int  sap_getsock(int *rsock);


int  sap_rq_init(struct sap_rq* out,int sock);
int  sap_rq_flush(struct sap_rq* out);
void sap_rq_general(struct sap_rq* out,u_short ser_type);
void sap_rq_gns_request(struct sap_rq* out,u_short ser_type);
void sap_rq_response(struct sap_rq* out,u_short type,char *name,struct sockaddr_ipx* addr,u_short hops,int down_allow);
void sap_rq_gns_response(struct sap_rq* out,u_short type,char * name,struct sockaddr_ipx* addr,u_short hops);
void sap_rq_set_destination(struct sap_rq* out,struct ipx_addr *dest);

int  sap_find_nearest(int server_type, struct sockaddr_ipx *result,char *server_name);

extern int (*sap_sendto_func)(void* buffer,int size,struct sockaddr_ipx* daddr,int sock);
int  ipx_iffind(char *ifname, struct ipx_addr *addr);

#endif /* !_IPXSAP_H_ */
