
/*
 * ng_ppp.h
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
 * $Whistle: ng_ppp.h,v 1.8 1999/01/25 02:40:02 archie Exp $
 */

#ifndef _NETGRAPH_PPP_H_
#define _NETGRAPH_PPP_H_

/* Node type name and magic cookie */
#define NG_PPP_NODE_TYPE	"ppp"
#define NGM_PPP_COOKIE		860635544

/* Hook names */
#define NG_PPP_HOOK_DOWNLINK	"downlink"	/* downstream hook */
#define NG_PPP_HOOK_BYPASS	"bypass"	/* any unhooked protocol */

/* Netgraph commands */
enum {
	NGM_PPP_SET_PROTOCOMP = 1,	/* takes an integer 0 or 1 */
	NGM_PPP_GET_STATS,		/* returns struct ng_ppp_stat */
	NGM_PPP_CLR_STATS,		/* clear stats */
};

/* Statistics struct */
struct ng_ppp_stat {
	u_int32_t xmitFrames;		/* xmit frames on "downstream" */
	u_int32_t xmitOctets;		/* xmit octets on "downstream" */
	u_int32_t recvFrames;		/* recv frames on "downstream" */
	u_int32_t recvOctets;		/* recv octets on "downstream" */
	u_int32_t badProto;		/* frames with invalid protocol */
	u_int32_t unknownProto;		/* frames sent to "unhooked" */
};

/*
 * We recognize these hook names for some various PPP protocols. But we
 * always recognize the hook name "0xNNNN" for any protocol, including these.
 * So these are really just alias hook names.
 */
#define NG_PPP_HOOK_LCP		"lcp"		/* 0xc021 */
#define NG_PPP_HOOK_IPCP	"ipcp"		/* 0x8021 */
#define NG_PPP_HOOK_ATCP	"atcp"		/* 0x8029 */
#define NG_PPP_HOOK_CCP		"ccp"		/* 0x80fd */
#define NG_PPP_HOOK_ECP		"ecp"		/* 0x8053 */
#define NG_PPP_HOOK_IP		"ip"		/* 0x0021 */
#define NG_PPP_HOOK_VJCOMP	"vjcomp"	/* 0x002d */
#define NG_PPP_HOOK_VJUNCOMP	"vjuncomp"	/* 0x002f */
#define NG_PPP_HOOK_MP		"mp"		/* 0x003d */
#define NG_PPP_HOOK_COMPD	"compd"		/* 0x00fd */
#define NG_PPP_HOOK_CRYPTD	"cryptd"	/* 0x0053 */
#define NG_PPP_HOOK_PAP		"pap"		/* 0xc023 */
#define NG_PPP_HOOK_CHAP	"chap"		/* 0xc223 */
#define NG_PPP_HOOK_LQR		"lqr"		/* 0xc025 */

#endif /* _NETGRAPH_PPP_H_ */
