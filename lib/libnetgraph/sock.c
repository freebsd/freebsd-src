
/*
 * sock.c
 *
 * Copyright (c) 1996-1999 Whistle Communications, Inc.
 * All rights reserved.
 * 
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Whistle Communications;
 * provided, however, that:
 * 1. Any and all reproductions of the source or object code must include the
 *    copyright notice above and the following disclaimer of warranties; and
 * 2. No rights are granted, in any manner or form, to use Whistle
 *    Communications, Inc. trademarks, including the mark "WHISTLE
 *    COMMUNICATIONS" on advertising, endorsements, or otherwise except as
 *    such appears in the above copyright notice or in the software.
 * 
 * THIS SOFTWARE IS BEING PROVIDED BY WHISTLE COMMUNICATIONS "AS IS", AND
 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, WHISTLE COMMUNICATIONS MAKES NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE,
 * INCLUDING WITHOUT LIMITATION, ANY AND ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
 * WHISTLE COMMUNICATIONS DOES NOT WARRANT, GUARANTEE, OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE USE OF THIS
 * SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY OR OTHERWISE.
 * IN NO EVENT SHALL WHISTLE COMMUNICATIONS BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF WHISTLE COMMUNICATIONS IS ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * Author: Archie Cobbs <archie@whistle.com>
 *
 * $FreeBSD$
 * $Whistle: sock.c,v 1.12 1999/01/20 00:57:23 archie Exp $
 */

#include <sys/types.h>
#include <stdarg.h>
#include <netgraph/ng_message.h>
#include <netgraph/ng_socket.h>

#include "netgraph.h"
#include "internal.h"

/* The socket node type KLD */
#define NG_SOCKET_KLD	"ng_socket.ko"

/*
 * Create a socket type node and give it the supplied name.
 * Return data and control sockets corresponding to the node.
 * Returns -1 if error and sets errno.
 */
int
NgMkSockNode(const char *name, int *csp, int *dsp)
{
	char namebuf[NG_NODELEN + 1];
	int cs = -1;		/* control socket */
	int ds = -1;		/* data socket */
	int errnosv;

	/* Empty name means no name */
	if (name && *name == 0)
		name = NULL;

	/* Create control socket; this also creates the netgraph node.
	   If we get a EPROTONOSUPPORT then the socket node type is
	   not loaded, so load it and try again. */
	if ((cs = socket(AF_NETGRAPH, SOCK_DGRAM, NG_CONTROL)) < 0) {
		if (errno == EPROTONOSUPPORT) {
			if (kldload(NG_SOCKET_KLD) < 0) {
				errnosv = errno;
				if (_gNgDebugLevel >= 1)
					NGLOG("can't load %s", NG_SOCKET_KLD);
				goto errout;
			}
			cs = socket(AF_NETGRAPH, SOCK_DGRAM, NG_CONTROL);
			if (cs >= 0)
				goto gotNode;
		}
		errnosv = errno;
		if (_gNgDebugLevel >= 1)
			NGLOG("socket");
		goto errout;
	}

gotNode:
	/* Assign the node the desired name, if any */
	if (name != NULL) {
		u_char sbuf[NG_NODELEN + 3];
		struct sockaddr_ng *const sg = (struct sockaddr_ng *) sbuf;

		/* Assign name */
		snprintf(sg->sg_data, NG_NODELEN + 1, "%s", name);
		sg->sg_family = AF_NETGRAPH;
		sg->sg_len = strlen(sg->sg_data) + 3;
		if (bind(cs, (struct sockaddr *) sg, sg->sg_len) < 0) {
			errnosv = errno;
			if (_gNgDebugLevel >= 1)
				NGLOG("bind(%s)", sg->sg_data);
			goto errout;
		}

		/* Save node name */
		snprintf(namebuf, sizeof(namebuf), "%s", name);
	} else if (dsp != NULL) {
		u_char rbuf[sizeof(struct ng_mesg) + sizeof(struct nodeinfo)];
		struct ng_mesg *const resp = (struct ng_mesg *) rbuf;
		struct nodeinfo *const ni = (struct nodeinfo *) resp->data;

		/* Find out the node ID */
		if (NgSendMsg(cs, ".", NGM_GENERIC_COOKIE,
		    NGM_NODEINFO, NULL, 0) < 0) {
			errnosv = errno;
			if (_gNgDebugLevel >= 1)
				NGLOG("send nodeinfo");
			goto errout;
		}
		if (NgRecvMsg(cs, resp, sizeof(rbuf), NULL) < 0) {
			errnosv = errno;
			if (_gNgDebugLevel >= 1)
				NGLOG("recv nodeinfo");
			goto errout;
		}

		/* Save node "name" */
		snprintf(namebuf, sizeof(namebuf), "[%lx]", (u_long) ni->id);
	}

	/* Create data socket if desired */
	if (dsp != NULL) {
		u_char sbuf[NG_NODELEN + 4];
		struct sockaddr_ng *const sg = (struct sockaddr_ng *) sbuf;

		/* Create data socket, initially just "floating" */
		if ((ds = socket(AF_NETGRAPH, SOCK_DGRAM, NG_DATA)) < 0) {
			errnosv = errno;
			if (_gNgDebugLevel >= 1)
				NGLOG("socket");
			goto errout;
		}

		/* Associate the data socket with the node */
		snprintf(sg->sg_data, NG_NODELEN + 2, "%s:", namebuf);
		sg->sg_family = AF_NETGRAPH;
		sg->sg_len = strlen(sg->sg_data) + 3;
		if (connect(ds, (struct sockaddr *) sg, sg->sg_len) < 0) {
			errnosv = errno;
			if (_gNgDebugLevel >= 1)
				NGLOG("connect(%s)", sg->sg_data);
			goto errout;
		}
	}

	/* Return the socket(s) */
	if (csp)
		*csp = cs;
	else
		close(cs);
	if (dsp)
		*dsp = ds;
	return (0);

errout:
	/* Failed */
	if (cs >= 0)
		close(cs);
	if (ds >= 0)
		close(ds);
	errno = errnosv;
	return (-1);
}

/*
 * Assign a globally unique name to a node
 * Returns -1 if error and sets errno.
 */
int
NgNameNode(int cs, const char *path, const char *fmt, ...)
{
	struct ngm_name ngn;
	va_list args;

	/* Build message arg */
	va_start(args, fmt);
	vsnprintf(ngn.name, sizeof(ngn.name), fmt, args);
	va_end(args);

	/* Send message */
	if (NgSendMsg(cs, path,
	    NGM_GENERIC_COOKIE, NGM_NAME, &ngn, sizeof(ngn)) < 0) {
		if (_gNgDebugLevel >= 1)
			NGLOGX("%s: failed", __FUNCTION__);
		return (-1);
	}

	/* Done */
	return (0);
}

/*
 * Read a packet from a data socket
 * Returns -1 if error and sets errno.
 */
int
NgRecvData(int ds, u_char * buf, size_t len, char *hook)
{
	u_char frombuf[NG_HOOKLEN + sizeof(struct sockaddr_ng)];
	struct sockaddr_ng *const from = (struct sockaddr_ng *) frombuf;
	int fromlen = sizeof(frombuf);
	int rtn, errnosv;

	/* Read packet */
	rtn = recvfrom(ds, buf, len, 0, (struct sockaddr *) from, &fromlen);
	if (rtn < 0) {
		errnosv = errno;
		if (_gNgDebugLevel >= 1)
			NGLOG("recvfrom");
		errno = errnosv;
		return (-1);
	}

	/* Copy hook name */
	if (hook != NULL)
		snprintf(hook, NG_HOOKLEN + 1, "%s", from->sg_data);

	/* Debugging */
	if (_gNgDebugLevel >= 2) {
		NGLOGX("READ %s from hook \"%s\" (%d bytes)",
		       rtn ? "PACKET" : "EOF", from->sg_data, rtn);
		if (_gNgDebugLevel >= 3)
			_NgDebugBytes(buf, rtn);
	}

	/* Done */
	return (rtn);
}

/*
 * Write a packet to a data socket. The packet will be sent
 * out the corresponding node on the specified hook.
 * Returns -1 if error and sets errno.
 */
int
NgSendData(int ds, const char *hook, const u_char * buf, size_t len)
{
	u_char sgbuf[NG_HOOKLEN + sizeof(struct sockaddr_ng)];
	struct sockaddr_ng *const sg = (struct sockaddr_ng *) sgbuf;
	int errnosv;

	/* Set up destination hook */
	sg->sg_family = AF_NETGRAPH;
	snprintf(sg->sg_data, NG_HOOKLEN + 1, "%s", hook);
	sg->sg_len = strlen(sg->sg_data) + 3;

	/* Debugging */
	if (_gNgDebugLevel >= 2) {
		NGLOGX("WRITE PACKET to hook \"%s\" (%d bytes)", hook, len);
		_NgDebugSockaddr(sg);
		if (_gNgDebugLevel >= 3)
			_NgDebugBytes(buf, len);
	}

	/* Send packet */
	if (sendto(ds, buf, len, 0, (struct sockaddr *) sg, sg->sg_len) < 0) {
		errnosv = errno;
		if (_gNgDebugLevel >= 1)
			NGLOG("sendto(%s)", sg->sg_data);
		errno = errnosv;
		return (-1);
	}

	/* Done */
	return (0);
}

