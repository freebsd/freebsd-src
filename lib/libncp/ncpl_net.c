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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/syscall.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netipx/ipx.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "ipxsap.h"
#include <netncp/ncp_lib.h>
#include "ncp_mod.h"

static int ncp_find_server_in(struct ncp_conn_loginfo *li, int type, char *server_name);

static int
ncp_find_server_ipx(struct ncp_conn_loginfo *li, int type) {
	char server[NCP_BINDERY_NAME_LEN + 1];
	int error;
	char nearest[NCP_BINDERY_NAME_LEN + 1];
	struct nw_property prop;
	struct ipx_addr *n_addr = (struct ipx_addr *) &prop;
/*	struct ncp_conn_loginfo ltmp;*/
	int connid;

	bzero(server, sizeof(server));
	bzero(nearest, sizeof(nearest));

	strcpy(server, li->server);
	ncp_str_upper(server);

	if ((error = sap_find_nearest(type, &li->ipxaddr, nearest)) != 0) {
		return error;
	}
	/* if no server specified return info about nearest */
	if (!li->server[0]) {
		strcpy(li->server, nearest);
		return 0;
	}
/*	printf("%s\n",ipx_ntoa(li->ipxaddr.sipx_addr));*/
	if (strcmp(server, nearest) == 0) {
		return 0;
	}
	/* We have to ask the nearest server for our wanted server */
	li->opt=0;
	if ((error = ncp_connect(li, &connid)) != 0) {
		return error;
	}
	if (ncp_read_property_value(connid, type, server, 1, "NET_ADDRESS", &prop) != 0) {
		ncp_disconnect(connid);
		return EHOSTUNREACH;
	}
	if ((error = ncp_disconnect(connid)) != 0) {
		return error;
	}
	li->ipxaddr.sipx_family = AF_IPX;
	li->ipxaddr.sipx_addr.x_net = n_addr->x_net;
	li->ipxaddr.sipx_port = n_addr->x_port;
	li->ipxaddr.sipx_addr.x_host = n_addr->x_host;
	return 0;
}

static int
ncp_find_server_in(struct ncp_conn_loginfo *li, int type, char *server_name) {
	struct hostent* h;
	int l;

	h = gethostbyname(server_name);
	if (!h) {
		fprintf(stderr, "Get host address `%s': ", server_name);
		herror(NULL);
		return 1;
	}
	if (h->h_addrtype != AF_INET) {
		fprintf(stderr, "Get host address `%s': Not AF_INET\n", server_name);
		return 1;
	}
	if (h->h_length != 4) {
		fprintf(stderr, "Get host address `%s': Bad address length\n", server_name);
		return 1;
	}
	l = sizeof(struct sockaddr_in);
	bzero(&li->inaddr, l);
	li->inaddr.sin_len = l;
	li->inaddr.sin_family = h->h_addrtype;
	memcpy(&li->inaddr.sin_addr.s_addr, h->h_addr, 4);
	li->inaddr.sin_port = htons(524); /* ncp */
	return 0;
}

int 
ncp_find_server(struct ncp_conn_loginfo *li, int type, int af, char *name) {
	int error = EHOSTUNREACH;

	switch(af) {
	    case AF_IPX:
		error = ncp_find_server_ipx(li, type);
		break;
	    case AF_INET:
		if (name)
			error = ncp_find_server_in(li, type, name);
		break;
	    default:
		error = EPROTONOSUPPORT;
	}
	return error;
}

int
ncp_find_fileserver(struct ncp_conn_loginfo *li, int af, char *name) {
	return ncp_find_server(li, NCP_BINDERY_FSERVER, af, name);
}
