/*
 * Copyright (c) 1995 John Hay.  All rights reserved.
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
 *	This product includes software developed by John Hay.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY John Hay AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL John Hay OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/IPXrouted/sap.h,v 1.7 1999/08/28 01:15:03 peter Exp $
 */
#ifndef _SAP_H_
#define _SAP_H_

#define SAP_REQ			1
#define SAP_RESP		2
#define SAP_REQ_NEAR		3
#define SAP_RESP_NEAR		4

#define SAPCMD_MAX		5
#ifdef SAPCMDS
char *sapcmds[SAPCMD_MAX] =
	{ "#0", "REQUEST", "RESPONSE", "REQ NEAREST", "RESP NEAREST"};
#endif

#define MAXSAPENTRIES		7
#define SAP_WILDCARD		0xFFFF
#define SERVNAMELEN		48
typedef struct sap_info {
	u_short ServType;
	char    ServName[SERVNAMELEN];
	struct ipx_addr ipx;
	u_short hops;
	}sap_info;  

typedef struct sap_packet {
	u_short sap_cmd;
	sap_info sap[0]; /* Variable length. */
	}sap_packet;

typedef struct sap_entry {
	struct sap_entry *forw;
	struct sap_entry *back;
	struct sap_entry *clone;
	struct interface *ifp;
	struct sap_info   sap;
	struct sockaddr   source;
	int hash;
	int state;
	int timer;
	}sap_entry;

#define SAPHASHSIZ		256		/* Should be a power of 2 */
#define SAPHASHMASK		(SAPHASHSIZ-1)
typedef struct sap_hash {
	struct sap_entry *forw;
	struct sap_entry *back;
	}sap_hash;

extern sap_hash sap_head[SAPHASHSIZ];

extern struct   sap_packet *sap_msg;

void sapinit(void);
void sap_input(struct sockaddr *from, int size);
void sapsndmsg(struct sockaddr *dst, int flags, struct interface *ifp,
		int changesonly);
void sap_supply_toall(int changesonly);
void sap_supply(struct sockaddr *dst, 
                int flags, 
		struct interface *ifp, 
		int ServType,
		int changesonly);

struct sap_entry *sap_lookup(u_short ServType, char *ServName);
struct sap_entry *sap_nearestserver(ushort ServType, struct interface *ifp);
void sap_add(struct sap_info *si, struct sockaddr *from);
void sap_change(struct sap_entry *sap, 
                struct sap_info *si,
		struct sockaddr *from);
void sap_add_clone(struct sap_entry *sap, 
                   struct sap_info *clone, 
		   struct sockaddr *from);
void sap_delete(struct sap_entry *sap);

#endif /*_SAP_H_*/

