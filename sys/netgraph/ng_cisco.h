
/*
 * ng_cisco.h
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
 * $Whistle: ng_cisco.h,v 1.6 1999/01/25 01:21:48 archie Exp $
 */

#ifndef _NETGRAPH_CISCO_H_
#define _NETGRAPH_CISCO_H_

/* Node type name and magic cookie */
#define NG_CISCO_NODE_TYPE		"cisco"
#define NGM_CISCO_COOKIE		860707227

/* Hook names */
#define NG_CISCO_HOOK_DOWNSTREAM	"downstream"
#define NG_CISCO_HOOK_INET		"inet"
#define NG_CISCO_HOOK_APPLETALK		"atalk"
#define NG_CISCO_HOOK_IPX		"ipx"
#define NG_CISCO_HOOK_DEBUG		"debug"

/* Netgraph commands */
enum {
	/* This takes two struct in_addr's: the IP address and netmask */
	NGM_CISCO_SET_IPADDR = 1,

	/* This is both received and *sent* by this node (to the "inet"
	   peer). The reply contains the same info as NGM_CISCO_SET_IPADDR. */
	NGM_CISCO_GET_IPADDR,

	/* This returns a struct ngciscostat (see below) */
	NGM_CISCO_GET_STATUS,
};

struct ngciscostat {
	u_int32_t   seq_retries;		/* # unack'd retries */
	u_int32_t   keepalive_period;	/* in seconds */
};

#endif /* _NETGRAPH_CISCO_H_ */

