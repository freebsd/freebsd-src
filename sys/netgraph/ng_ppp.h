
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
#define NGM_PPP_COOKIE		940897792

/* Maximum number of supported links */
#define NG_PPP_MAX_LINKS	16

/* Pseudo-link number representing the multi-link bundle */
#define NG_PPP_BUNDLE_LINKNUM	0xffff

/* Max allowable link latency (miliseconds) and bandwidth (bytes/second/10) */
#define NG_PPP_MAX_LATENCY	1000		/* 1 second */
#define NG_PPP_MAX_BANDWIDTH	125000		/* 10 Mbits / second */

/* Hook names */
#define NG_PPP_HOOK_BYPASS	"bypass"	/* unknown protocols */
#define NG_PPP_HOOK_COMPRESS	"compress"	/* outgoing compression */
#define NG_PPP_HOOK_DECOMPRESS	"decompress"	/* incoming decompression */
#define NG_PPP_HOOK_ENCRYPT	"encrypt"	/* outgoing encryption */
#define NG_PPP_HOOK_DECRYPT	"decrypt"	/* incoming decryption */
#define NG_PPP_HOOK_VJC_IP	"vjc_ip"	/* VJC raw IP */
#define NG_PPP_HOOK_VJC_COMP	"vjc_vjcomp"	/* VJC compressed TCP */
#define NG_PPP_HOOK_VJC_UNCOMP	"vjc_vjuncomp"	/* VJC uncompressed TCP */
#define NG_PPP_HOOK_VJC_VJIP	"vjc_vjip"	/* VJC uncompressed IP */
#define NG_PPP_HOOK_INET	"inet"		/* IP packet data */
#define NG_PPP_HOOK_ATALK	"atalk"		/* AppleTalk packet data */
#define NG_PPP_HOOK_IPX		"ipx"		/* IPX packet data */

#define NG_PPP_HOOK_LINK_PREFIX	"link"		/* append decimal link number */

/* Netgraph commands */
enum {
	NGM_PPP_SET_CONFIG = 1,		/* takes struct ng_ppp_bundle_config */
	NGM_PPP_GET_CONFIG,		/* returns ng_ppp_bundle_config */
	NGM_PPP_GET_LINK_STATS,		/* takes link #, returns stats struct */
	NGM_PPP_CLR_LINK_STATS,		/* takes link #, clears link stats */
};

/* Per-link config structure */
struct ng_ppp_link_config {
	u_char		enableLink;	/* enable this link */
	u_char		enableProtoComp;/* enable protocol field compression */
	u_int16_t	mru;		/* peer MRU */
	u_int32_t	latency;	/* link latency (in milliseconds) */
	u_int32_t	bandwidth;	/* link bandwidth (in bytes/second) */
};

/* Node config structure */
struct ng_ppp_node_config {
	u_int16_t	mrru;			/* multilink peer MRRU */
	u_char		enableMultilink;	/* enable multilink */
	u_char		recvShortSeq;		/* recv multilink short seq # */
	u_char		xmitShortSeq;		/* xmit multilink short seq # */
	u_char		enableRoundRobin;	/* xmit whole packets */
	u_char		enableIP;		/* enable IP data flow */
	u_char		enableAtalk;		/* enable AppleTalk data flow */
	u_char		enableIPX;		/* enable IPX data flow */
	u_char		enableCompression;	/* enable PPP compression */
	u_char		enableDecompression;	/* enable PPP decompression */
	u_char		enableEncryption;	/* enable PPP encryption */
	u_char		enableDecryption;	/* enable PPP decryption */
	u_char		enableVJCompression;	/* enable VJ compression */
	u_char		enableVJDecompression;	/* enable VJ decompression */
	struct ng_ppp_link_config		/* per link config params */
			links[NG_PPP_MAX_LINKS];
};

/* Statistics struct for a link (or the bundle if NG_PPP_BUNDLE_LINKNUM) */
struct ng_ppp_link_stat {
	u_int32_t xmitFrames;		/* xmit frames on link */
	u_int32_t xmitOctets;		/* xmit octets on link */
	u_int32_t recvFrames;		/* recv frames on link */
	u_int32_t recvOctets;		/* recv octets on link */
	u_int32_t badProtos;		/* frames rec'd with bogus protocol */
	u_int32_t dupFragments;		/* MP frames with duplicate seq # */
};

#endif /* _NETGRAPH_PPP_H_ */
