
/*
 * ng_echo.c
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
 * Author: Julian Elisher <julian@freebsd.org>
 *
 * $FreeBSD$
 * $Whistle: ng_echo.c,v 1.13 1999/11/01 09:24:51 julian Exp $
 */

/*
 * Netgraph "echo" node
 *
 * This node simply bounces data and messages back to whence they came.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_echo.h>

/* Netgraph methods */
static ng_rcvmsg_t	nge_rcvmsg;
static ng_rcvdata_t	nge_rcvdata;
static ng_disconnect_t	nge_disconnect;

/* Netgraph type */
static struct ng_type typestruct = {
	NG_VERSION,
	NG_ECHO_NODE_TYPE,
	NULL,
	NULL,
	nge_rcvmsg,
	NULL,
	NULL,
	NULL,
	NULL,
	nge_rcvdata,
	nge_rcvdata,
	nge_disconnect,
	NULL
};
NETGRAPH_INIT(echo, &typestruct);

/*
 * Receive control message. We just bounce it back as a reply.
 */
static int
nge_rcvmsg(node_p node, struct ng_mesg *msg, const char *retaddr,
	   struct ng_mesg **rptr)
{
	if (rptr) {
		msg->header.flags |= NGF_RESP;
		*rptr = msg;
	} else {
		FREE(msg, M_NETGRAPH);
	}
	return (0);
}

/*
 * Receive data
 */
static int
nge_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	int error = 0;

	NG_SEND_DATA(error, hook, m, meta);
	return (error);
}

/*
 * Removal of the last link destroys the nodeo
 */
static int
nge_disconnect(hook_p hook)
{
	if (hook->node->numhooks == 0)
		ng_rmnode(hook->node);
	return (0);
}

