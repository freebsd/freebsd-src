
/*
 * ng_pptpgre.h
 *
 * Copyright (c) 1999 Whistle Communications, Inc.
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
 * $FreeBSD: src/sys/netgraph/ng_pptpgre.h,v 1.1 1999/12/08 18:55:39 archie Exp $
 * $Whistle: ng_pptpgre.h,v 1.3 1999/12/08 00:11:36 archie Exp $
 */

#ifndef _NETGRAPH_PPTPGRE_H_
#define _NETGRAPH_PPTPGRE_H_

/* Node type name and magic cookie */
#define NG_PPTPGRE_NODE_TYPE	"pptpgre"
#define NGM_PPTPGRE_COOKIE	942783546

/* Hook names */
#define NG_PPTPGRE_HOOK_UPPER	"upper"		/* to upper layers */
#define NG_PPTPGRE_HOOK_LOWER	"lower"		/* to lower layers */

/* Configuration for a session */
struct ng_pptpgre_conf {
	u_char		enabled;	/* enables traffic flow */
	u_char		enableDelayedAck;/* enables delayed acks */
	u_int16_t	cid;		/* my call id */
	u_int16_t	peerCid;	/* peer call id */
	u_int16_t	recvWin;	/* peer recv window size */
	u_int16_t	peerPpd;	/* peer packet processing delay
					   (in units of 1/10 of a second) */
};

/* Keep this in sync with the above structure definition */
#define NG_PPTPGRE_CONF_TYPE_INFO	{			\
	{							\
	  { "enabled",		&ng_parse_int8_type	},	\
	  { "enableDelayedAck",	&ng_parse_int8_type	},	\
	  { "cid",		&ng_parse_int16_type	},	\
	  { "peerCid",		&ng_parse_int16_type	},	\
	  { "recvWin",		&ng_parse_int16_type	},	\
	  { "peerPpd",		&ng_parse_int16_type	},	\
	  { NULL },						\
	}							\
}

/* Netgraph commands */
enum {
	NGM_PPTPGRE_SET_CONFIG = 1,	/* supply a struct ng_pptpgre_conf */
	NGM_PPTPGRE_GET_CONFIG,		/* returns a struct ng_pptpgre_conf */
};

#endif /* _NETGRAPH_PPTPGRE_H_ */
