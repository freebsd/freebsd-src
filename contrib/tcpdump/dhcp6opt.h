/* @(#) $Header: /tcpdump/master/tcpdump/dhcp6opt.h,v 1.3 2000/12/17 23:07:49 guy Exp $ (LBL) */
/*
 * Copyright (C) 1998 and 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * draft-ietf-dhc-v6exts-11
 */

#ifndef __DHCP6OPT_H_DEFINED
#define __DHCP6OPT_H_DEFINED

#define OL6_N	-1
#define OL6_16N	-2
#define OL6_Z	-3

#define OT6_NONE	0
#define OT6_V6		1
#define OT6_STR		2
#define OT6_NUM		3

struct dhcp6_opt {
	u_int code;
	int len;
	char *name;
	int type;
};

/* index to parameters */
#define DH6T_CLIENT_ADV_WAIT		1	/* milliseconds */
#define DH6T_DEFAULT_SOLICIT_HOPCOUNT	2	/* times */
#define DH6T_SERVER_MIN_ADV_DELAY	3	/* milliseconds */
#define DH6T_SERVER_MAX_ADV_DELAY	4	/* milliseconds */
#define DH6T_REQUEST_MSG_MIN_RETRANS	5	/* retransmissions */
#define DH6T_REPLY_MSG_TIMEOUT		6	/* milliseconds */
#define DH6T_REPLY_MSG_RETRANS_INTERVAL	7	/* milliseconds */
#define DH6T_RECONF_MSG_TIMEOUT		8	/* milliseconds */
#define DH6T_RECONF_MSG_MIN_RETRANS	9	/* retransmissions */
#define DH6T_RECONF_MSG_RETRANS_INTERVAL 10	/* milliseconds */
#define DH6T_RECONF_MMSG_MIN_RESP	11	/* milliseconds */
#define DH6T_RECONF_MMSG_MAX_RESP	12	/* milliseconds */
#define DH6T_MIN_SOLICIT_DELAY		13	/* milliseconds */
#define DH6T_MAX_SOLICIT_DELAY		14	/* milliseconds */
#define DH6T_XID_TIMEOUT		15	/* milliseconds */
#define DH6T_RECONF_MULTICAST_REQUEST_WAIT 16	/* milliseconds */

#if 0
extern struct dhcp6_opt *dh6o_pad;
extern struct dhcp6_opt *dh6o_end;
extern int dhcp6_param[];
extern void dhcp6opttab_init (void);
extern struct dhcp6_opt *dhcp6opttab_byname (char *);
extern struct dhcp6_opt *dhcp6opttab_bycode (u_int);
#endif

#endif /*__DHCP6OPT_H_DEFINED*/
