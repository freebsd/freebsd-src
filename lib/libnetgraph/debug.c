
/*
 * debug.c
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
 * $Whistle: debug.c,v 1.24 1999/01/24 01:15:33 archie Exp $
 */

#include <sys/types.h>
#include <stdarg.h>
#include <netgraph/ng_message.h>
#include <netgraph/ng_socket.h>

#include "netgraph.h"
#include "internal.h"

#include <netgraph/ng_socket.h>
#include <netgraph/ng_message.h>
#include <netgraph/ng_iface.h>
#include <netgraph/ng_rfc1490.h>
#include <netgraph/ng_cisco.h>
#include <netgraph/ng_async.h>
#include <netgraph/ng_ppp.h>
#include <netgraph/ng_frame_relay.h>
#include <netgraph/ng_lmi.h>
#include <netgraph/ng_tty.h>
#include <netgraph/ng_tty.h>

/* Global debug level */
int     _gNgDebugLevel = 0;

/* Debug printing functions */
void    (*_NgLog) (const char *fmt,...) = warn;
void    (*_NgLogx) (const char *fmt,...) = warnx;

/* Internal functions */
static const	char *NgCookie(int cookie);
static const	char *NgCmd(int cookie, int cmd);
static void	NgArgs(int cookie, int cmd, int resp, void *args, int arglen);

/*
 * Set debug level, ie, verbosity, if "level" is non-negative.
 * Returns old debug level.
 */
int
NgSetDebug(int level)
{
	int old = _gNgDebugLevel;

	if (level < 0)
		level = old;
	_gNgDebugLevel = level;
	return (old);
}

/*
 * Set debug logging functions.
 */
void
NgSetErrLog(void (*log) (const char *fmt,...),
		void (*logx) (const char *fmt,...))
{
	_NgLog = log;
	_NgLogx = logx;
}

/*
 * Display a netgraph sockaddr
 */
void
_NgDebugSockaddr(struct sockaddr_ng *sg)
{
	NGLOGX("SOCKADDR: { fam=%d len=%d addr=\"%s\" }",
	       sg->sg_family, sg->sg_len, sg->sg_data);
}

/*
 * Display a negraph message
 */
void
_NgDebugMsg(struct ng_mesg * msg)
{
	NGLOGX("NG_MESG :");
	NGLOGX("  vers   %d", msg->header.version);
	NGLOGX("  arglen %d", msg->header.arglen);
	NGLOGX("  flags  %ld", msg->header.flags);
	NGLOGX("  token  %lu", (u_long) msg->header.token);
	NGLOGX("  cookie %s", NgCookie(msg->header.typecookie));
	NGLOGX("  cmd    %s", NgCmd(msg->header.typecookie, msg->header.cmd));
	NgArgs(msg->header.typecookie, msg->header.cmd,
	       (msg->header.flags & NGF_RESP), msg->data, msg->header.arglen);
}

/*
 * Return the name of the node type corresponding to the cookie
 */
static const char *
NgCookie(int cookie)
{
	static char buf[20];

	switch (cookie) {
	case NGM_GENERIC_COOKIE:
		return "generic";
	case NGM_TTY_COOKIE:
		return "tty";
	case NGM_ASYNC_COOKIE:
		return "async";
	case NGM_IFACE_COOKIE:
		return "iface";
	case NGM_FRAMERELAY_COOKIE:
		return "frame_relay";
	case NGM_LMI_COOKIE:
		return "lmi";
	case NGM_CISCO_COOKIE:
		return "cisco";
	case NGM_PPP_COOKIE:
		return "ppp";
	case NGM_RFC1490_NODE_COOKIE:
		return "rfc1490";
	case NGM_SOCKET_COOKIE:
		return "socket";
	}
	snprintf(buf, sizeof(buf), "?? (%d)", cookie);
	return buf;
}

/*
 * Return the name of the command
 */
static const char *
NgCmd(int cookie, int cmd)
{
	static char buf[20];

	switch (cookie) {
	case NGM_GENERIC_COOKIE:
		switch (cmd) {
		case NGM_SHUTDOWN:
			return "shutdown";
		case NGM_MKPEER:
			return "mkpeer";
		case NGM_CONNECT:
			return "connect";
		case NGM_NAME:
			return "name";
		case NGM_RMHOOK:
			return "rmhook";
		case NGM_NODEINFO:
			return "nodeinfo";
		case NGM_LISTHOOKS:
			return "listhooks";
		case NGM_LISTNAMES:
			return "listnames";
		case NGM_LISTNODES:
			return "listnodes";
		case NGM_TEXT_STATUS:
			return "text_status";
		}
		break;
	case NGM_TTY_COOKIE:
		switch (cmd) {
		case NGM_TTY_GET_HOTCHAR:
			return "getHotChar";
		case NGM_TTY_SET_HOTCHAR:
			return "setHotChar";
		}
		break;
	case NGM_ASYNC_COOKIE:
		switch (cmd) {
		case NGM_ASYNC_CMD_GET_STATS:
			return "getStats";
		case NGM_ASYNC_CMD_CLR_STATS:
			return "setStats";
		case NGM_ASYNC_CMD_SET_CONFIG:
			return "setConfig";
		case NGM_ASYNC_CMD_GET_CONFIG:
			return "getConfig";
		}
		break;
	case NGM_IFACE_COOKIE:
		switch (cmd) {
		case NGM_IFACE_GET_IFNAME:
			return "getIfName";
		case NGM_IFACE_GET_IFADDRS:
			return "getIfAddrs";
		}
		break;
	case NGM_LMI_COOKIE:
		switch (cmd) {
		case NGM_LMI_GET_STATUS:
			return "get-status";
		}
		break;
	}
	snprintf(buf, sizeof(buf), "?? (%d)", cmd);
	return buf;
}

/*
 * Decode message arguments
 */
static void
NgArgs(int cookie, int cmd, int resp, void *args, int arglen)
{

switch (cookie) {
case NGM_GENERIC_COOKIE:
	switch (cmd) {
	case NGM_SHUTDOWN:
		return;
	case NGM_MKPEER:
	    {
		struct ngm_mkpeer *const mkp = (struct ngm_mkpeer *) args;

		if (resp)
			return;
		NGLOGX("    type     \"%s\"", mkp->type);
		NGLOGX("    ourhook  \"%s\"", mkp->ourhook);
		NGLOGX("    peerhook \"%s\"", mkp->peerhook);
		return;
	    }
	case NGM_CONNECT:
	    {
		struct ngm_connect *const ngc = (struct ngm_connect *) args;

		if (resp)
			return;
		NGLOGX("    path     \"%s\"", ngc->path);
		NGLOGX("    ourhook  \"%s\"", ngc->ourhook);
		NGLOGX("    peerhook \"%s\"", ngc->peerhook);
		return;
	    }
	case NGM_NAME:
	    {
		struct ngm_name *const ngn = (struct ngm_name *) args;

		if (resp)
			return;
		NGLOGX("    name \"%s\"", ngn->name);
		return;
	    }
	case NGM_RMHOOK:
	    {
		struct ngm_rmhook *const ngr = (struct ngm_rmhook *) args;

		if (resp)
			return;
		NGLOGX("    hook \"%s\"", ngr->ourhook);
		return;
	    }
	case NGM_NODEINFO:
		return;
	case NGM_LISTHOOKS:
		return;
	case NGM_LISTNAMES:
	case NGM_LISTNODES:
		return;
	case NGM_TEXT_STATUS:
		if (!resp)
			return;
		NGLOGX("    status \"%s\"", (char *) args);
	    	return;
	}
	break;

case NGM_TTY_COOKIE:
	switch (cmd) {
	case NGM_TTY_GET_HOTCHAR:
		if (!resp)
			return;
		NGLOGX("    char 0x%02x", *((int *) args));
		return;
	case NGM_TTY_SET_HOTCHAR:
		NGLOGX("    char 0x%02x", *((int *) args));
		return;
	}
	break;

case NGM_ASYNC_COOKIE:
	switch (cmd) {
	case NGM_ASYNC_CMD_GET_STATS:
	    {
		struct ng_async_stat *const as = (struct ng_async_stat *) args;

		if (!resp)
			return;
		NGLOGX("    syncOctets = %lu", as->syncOctets);
		NGLOGX("    syncFrames = %lu", as->syncFrames);
		NGLOGX("    syncOverflows = %lu", as->syncOverflows);
		NGLOGX("    asyncOctets = %lu", as->asyncOctets);
		NGLOGX("    asyncFrames = %lu", as->asyncFrames);
		NGLOGX("    asyncRunts = %lu", as->asyncRunts);
		NGLOGX("    asyncOverflows = %lu", as->asyncOverflows);
		NGLOGX("    asyncBadCheckSums = %lu", as->asyncBadCheckSums);
		return;
	    }
	case NGM_ASYNC_CMD_GET_CONFIG:
	case NGM_ASYNC_CMD_SET_CONFIG:
	    {
		struct ng_async_cfg *const ac = (struct ng_async_cfg *) args;

		if (!resp ^ (cmd != NGM_ASYNC_CMD_GET_CONFIG))
			return;
		NGLOGX("    enabled   %s", ac->enabled ? "YES" : "NO");
		NGLOGX("    acfcomp   %s", ac->acfcomp ? "YES" : "NO");
		NGLOGX("    Async MRU %u", ac->amru);
		NGLOGX("    Sync MRU  %u", ac->smru);
		NGLOGX("    ACCM      0x%08x", ac->accm);
		return;
	    }
	case NGM_ASYNC_CMD_CLR_STATS:
		return;
	}
	break;

case NGM_IFACE_COOKIE:
	switch (cmd) {
	case NGM_IFACE_GET_IFNAME:
		return;
	case NGM_IFACE_GET_IFADDRS:
		return;
	}
	break;

	}
	_NgDebugBytes(args, arglen);
}

/*
 * Dump bytes in hex
 */
void
_NgDebugBytes(const u_char * ptr, int len)
{
	char    buf[100];
	int     k, count;

#define BYPERLINE	16

	for (count = 0; count < len; ptr += BYPERLINE, count += BYPERLINE) {

		/* Do hex */
		snprintf(buf, sizeof(buf), "%04x:  ", count);
		for (k = 0; k < BYPERLINE; k++, count++)
			if (count < len)
				snprintf(buf + strlen(buf),
				    sizeof(buf) - strlen(buf), "%02x ", ptr[k]);
			else
				snprintf(buf + strlen(buf),
				    sizeof(buf) - strlen(buf), "   ");
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "  ");
		count -= BYPERLINE;

		/* Do ASCII */
		for (k = 0; k < BYPERLINE; k++, count++)
			if (count < len)
				snprintf(buf + strlen(buf),
				    sizeof(buf) - strlen(buf),
				    "%c", isprint(ptr[k]) ? ptr[k] : '.');
			else
				snprintf(buf + strlen(buf),
				    sizeof(buf) - strlen(buf), "  ");
		count -= BYPERLINE;

		/* Print it */
		NGLOGX("%s", buf);
	}
}

