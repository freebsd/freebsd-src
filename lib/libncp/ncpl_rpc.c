/*
 * Copyright (c) 1999, Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * NetWare RPCs
 *
 * $FreeBSD: src/lib/libncp/ncpl_rpc.c,v 1.1 1999/10/12 11:56:41 bp Exp $
 */
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include <strings.h>
#include <netncp/ncp_lib.h>

struct ncp_rpc_rq {
	nuint16		len;	/* HL */
	nuint8		subfn;
	nuint32		reserved[4];
	nuint8		flags[4];
} __attribute__ ((packed));

struct ncp_rpc_rp {
	nuint32		rpccode;
	nuint32		reserved[4];
	nuint32		rpcval;
} __attribute__ ((packed));

static NWCCODE
ncp_rpc(NWCONN_HANDLE cH, int rpcfn, 
	const nuint8* rpcarg, char* arg1, char *arg2, 
	nuint32* rpcval) {
	NWCCODE error;
	NW_FRAGMENT rq[4], rp;
	struct ncp_rpc_rq rqh;
	struct ncp_rpc_rp rph;

	rqh.subfn = rpcfn;
	if (rpcarg) 
		bcopy(rpcarg, rqh.reserved, 4 * 4 + 4); 
	else
		bzero(rqh.reserved, 4 * 4 + 4);
	rq[0].fragAddress = (char*)&rqh;
	rq[0].fragSize = sizeof(rqh);
	rq[1].fragAddress = arg1;
	rq[1].fragSize = strlen(arg1) + 1;
	rq[2].fragAddress = arg2;
	rq[2].fragSize = arg2 ? (strlen(arg2) + 1) : 0;
	rqh.len = htons(rq[2].fragSize + rq[1].fragSize + sizeof(rqh) - 2);
	rp.fragAddress = (char*)&rph;
	rp.fragSize = sizeof(rph);
	error = NWRequest(cH, 131, 3, rq, 1, &rp);
	if (error) return error;
	if (rp.fragSize < 4) return EBADRPC;
	error = rph.rpccode;
	if (error) return error;
	if (rpcval) {
		if (rp.fragSize < 24)
			return EBADRPC;
		*rpcval = rph.rpcval;
	}
	return 0;
}

NWCCODE
NWSMLoadNLM(NWCONN_HANDLE cH, pnstr8 cmd) {
	return ncp_rpc(cH, 1, NULL, cmd, NULL, NULL);
}

NWCCODE
NWSMUnloadNLM(NWCONN_HANDLE cH, pnstr8 cmd) {
	return ncp_rpc(cH, 2, NULL, cmd, NULL, NULL);
}

NWCCODE
NWSMMountVolume(NWCONN_HANDLE cH, pnstr8 volName, nuint32* volnum) {
	return ncp_rpc(cH, 3, NULL, volName, NULL, volnum);
}

NWCCODE
NWSMDismountVolumeByName(NWCONN_HANDLE cH, pnstr8 vol) {
	return ncp_rpc(cH, 4, NULL, vol, NULL, NULL);
}

struct ncp_set_hdr {
	nuint32	typeFlag;	/* 0 - str, 1 - value */
	nuint32	value;
	nuint32	pad[20 - 4 - 4];
} __attribute__ ((packed));

NWCCODE
NWSMSetDynamicCmdIntValue(NWCONN_HANDLE cH, pnstr8 setCommandName, nuint32 cmdValue) {
	struct ncp_set_hdr rq;

	memset(&rq, 0, sizeof(rq));
	rq.typeFlag = 1;
	rq.value = cmdValue;
	return ncp_rpc(cH, 6, (char*)&rq, setCommandName, NULL, NULL);
}

NWCCODE
NWSMSetDynamicCmdStrValue(NWCONN_HANDLE cH, pnstr8 setCommandName,
		pnstr8 cmdValue) {
	return ncp_rpc(cH, 6, NULL, setCommandName, cmdValue, NULL);
}

NWCCODE
NWSMExecuteNCFFile(NWCONN_HANDLE cH, pnstr8 NCFFileName) {
	return ncp_rpc(cH, 7, NULL, NCFFileName, NULL, NULL);
}
