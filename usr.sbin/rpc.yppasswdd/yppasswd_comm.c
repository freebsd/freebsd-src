/*
 * Copyright (c) 1995, 1996
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: yppasswd_comm.c,v 1.1.1.1 1996/02/12 15:09:01 wpaul Exp $
 */

/*
 * This file contains a UNIX domain socket communications package
 * that lets a client process send pseudo-RPCs to rpc.yppasswdd
 * without using IP. This 'local-only' communications channel is
 * only used when the superuser runs passwd(1) or chpass(1) on
 * the NIS master server. The idea is that we want to grant the
 * superuser permission to perfom certain special operations, but
 * we need an iron-clad way to tell when we're receiving a request
 * from the superuser and when we aren't. To connect to a UNIX
 * domain socket, one needs to be able to access a file in the
 * filesystem. The socket created by rpc.yppasswdd is owned by
 * root and has all its permission bits cleared, so the only
 * user who can sucessfully connect() to it is root.
 *
 * It is the server's responsibility to initialize the listening
 * socket with the makeservsock() function and to add the socket to
 * the set of file descriptors monitored by the svc_run() loop.
 * Once this is done, calls made through the UNIX domain socket
 * can be handled almost exactly like a normal RPC. We even use
 * the XDR functions for serializing data between the client and
 * server to simplify the passing of complex data structures.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/fcntl.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include "yppasswd_comm.h"
#include "yppasswd_private.h"
#include "ypxfr_extern.h"

#ifndef lint
static const char rcsid[] = "$Id: yppasswd_comm.c,v 1.1.1.1 1996/02/12 15:09:01 wpaul Exp $";
#endif

char *sockname = "/var/run/ypsock";
FILE *serv_fp;
FILE *clnt_fp;
int serv_sock;
int clnt_sock;

/*
 * serialize_data() and serialize_resp() are what really do most of
 * the work. These functions (ab)use xdrstdio_create() as the interface
 * to the XDR library. The RPC library uses xdrrec_create() and friends
 * for TCP based connections. I suppose we could use that here, but
 * the interface is a bit too complicated to justify using in an
 * applicatuion such as this. With xdrstdio_create(), the only catch
 * is that we need to provide a buffered file stream rather than
 * a simple socket descriptor, but we can easily turn the latter into
 * the former using fdopen(2).
 *
 * Doing things this way buys us the ability to change the form of
 * the data being exchanged without having to modify any of the
 * routines in this package.
 */
static int serialize_data(data, fp, op)
	struct master_yppasswd *data;
	FILE *fp;
	int op;
{
	XDR xdrs;

	xdrstdio_create(&xdrs, fp, op);

	if (!xdr_master_yppasswd(&xdrs, data)) {
		xdr_destroy(&xdrs);
		return(1);
	}
	return(0);
}

static int serialize_resp(resp, fp, op)
	int *resp;
	FILE *fp;
	int op;
{
	XDR xdrs;

	xdrstdio_create(&xdrs, fp, op);

	if (!xdr_int(&xdrs, resp)) {
		xdr_destroy(&xdrs);
		return(1);
	}
	return(0);
}

/*
 * Build the server's listening socket. The descriptor generated
 * here will be monitored for new connections by the svc_run() loop.
 */
int makeservsock()
{
	static int ypsock;
	struct sockaddr_un us;
	int len;

	unlink(sockname);

	if ((ypsock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "failed to create UNIX domain socket");

	bzero((char *)&us, sizeof(us));
	us.sun_family = AF_UNIX;
	strcpy((char *)&us.sun_path, sockname);
	len = strlen(us.sun_path) + sizeof(us.sun_family) + 1;
	
	if (bind(ypsock, (struct sockaddr *)&us, len) == -1)
		err(1,"failed to bind UNIX domain socket");

	listen (ypsock, 1);

	return(ypsock);
}

/*
 * Create a socket for a client and try to connect() it to the
 * server.
 */
static int makeclntsock()
{
	static int ypsock;
	struct sockaddr_un us;
	int len;

	if ((ypsock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		warn("failed to create UNIX domain socket");
		return(-1);
	}

	bzero((char *)&us, sizeof(us));
	us.sun_family = AF_UNIX;
	strcpy((char *)&us.sun_path, sockname);
	len = strlen(us.sun_path) + sizeof(us.sun_family) + 1;

	if (connect(ypsock, (struct sockaddr *)&us, len) == -1) {
		warn("failed to connect to server");
		return(-1);
	}

	return(ypsock);
}

/*
 * This function is used by the server to accept a new connection
 * from a client and read its request data into a master_yppasswd
 * stucture.
 */
struct master_yppasswd *getdat(sock)
	int sock;
{
	int len;
	struct sockaddr_un us;
	static struct master_yppasswd pw;
	struct timeval tv;
	fd_set fds;

	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	tv.tv_sec =  CONNECTION_TIMEOUT;
	tv.tv_usec = 0;

	switch(select(FD_SETSIZE, &fds, NULL, NULL, &tv)) {
	case 0:
		yp_error("select timed out");
		return(NULL);
		break;
	case -1:
		yp_error("select() failed: %s", strerror(errno));
		return(NULL);
		break;
	default:
		break;
	}

	len = sizeof(us);
	if ((serv_sock = accept(sock, (struct sockaddr *)&us, &len)) == -1) {
		yp_error("accept failed: %s", strerror(errno));
		return(NULL);
	}

	if ((serv_fp = fdopen(serv_sock, "r+")) == NULL) {
		yp_error("fdopen failed: %s",strerror(errno));
		return(NULL);
	}

	if (serialize_data(&pw, serv_fp, XDR_DECODE)) {
		yp_error("failed to receive data");
		return(NULL);
	}

	return(&pw);
}

/*
 * Client uses this to read back a response code (a single
 * integer) from the server. Note that we don't need to implement
 * any special XDR function for this since an int is a base data
 * type which the XDR library can handle directly.
 */
int *getresp()
{
	static int resp;

	if (serialize_resp(&resp, clnt_fp, XDR_DECODE)) {
		warn("failed to receive response");
		return(NULL);
	}

	fclose(clnt_fp);
	close(clnt_sock);
	return(&resp);
}

/*
 * Create a connection to the server and send a reqest
 * to be processed.
 */
int senddat(pw)
	struct master_yppasswd *pw;
{

	if ((clnt_sock = makeclntsock()) == -1) {
		warn("failed to create socket");
		return(1);
	}

	if ((clnt_fp = fdopen(clnt_sock, "r+")) == NULL) {
		warn("fdopen failed");
		return(1);
	}

	if (serialize_data(pw, clnt_fp, XDR_ENCODE)) {
		warn("failed to send data");
		return(1);
	}

	return(0);
}

/*
 * This sends a response code back to the client.
 */
int sendresp(resp)
	int resp;
{

	if (serialize_resp(&resp, serv_fp, XDR_ENCODE)) {
		yp_error("failed to send response");
		return(-1);
	}

	fclose(serv_fp);
	close(serv_sock);
	return(0);
}
