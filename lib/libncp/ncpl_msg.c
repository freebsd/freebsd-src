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
 *
 * $FreeBSD$
 */
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <strings.h>

#include <netncp/ncp_lib.h>
#include <netncp/ncp_nls.h>

NWCCODE
NWDisableBroadcasts(NWCONN_HANDLE connHandle) {
	DECLARE_RQ;

	ncp_init_request_s(conn, 2);
	return ncp_request(connHandle, 21, conn);
}

NWCCODE
NWEnableBroadcasts(NWCONN_HANDLE connHandle) {
	DECLARE_RQ;

	ncp_init_request_s(conn, 3);
	return ncp_request(connHandle, 21, conn);
}

NWCCODE
NWBroadcastToConsole(NWCONN_HANDLE connHandle, pnstr8 message) {
	int l, error;
	DECLARE_RQ;

	l = strlen(message);
	if (l > 60) return EMSGSIZE;
	ncp_init_request_s(conn, 9);
	ncp_add_byte(conn, l);
	ncp_add_mem_nls(conn, message, l);
	error = ncp_request(connHandle, 21, conn);
	return error;
}

NWCCODE 
NWSendBroadcastMessage(NWCONN_HANDLE  connHandle, pnstr8 message,
	    nuint16 connCount, pnuint16 connList, pnuint8 resultList)
{
	int l, i, error;
	DECLARE_RQ;

	l = strlen(message);
	if (l > 255) return EMSGSIZE;
	if (connCount > 350) return EINVAL;
		
	ncp_init_request_s(conn, 0x0A);
	ncp_add_word_lh(conn, connCount);
	for (i = 0; i < connCount; i++)
		ncp_add_dword_lh(conn, connList[i]);
	ncp_add_byte(conn, l);
	ncp_add_mem_nls(conn, message, l);
	error = ncp_request(connHandle, 0x15, conn);
	if (!error) {
		l = ncp_reply_word_lh(conn, 0);
		for (i = 0; i < l; i++)
			resultList[i] =  ncp_reply_dword_lh(conn, (i)*4 + 2);
		return 0;
	}
	if (error != 0xfb) return error;
	if (l > 58) return EMSGSIZE;
	ncp_init_request_s(conn, 0);
	ncp_add_byte(conn, connCount);
	for (i = 0; i < connCount; i++)
		ncp_add_byte(conn, connList[i]);
	ncp_add_byte(conn, l);
	ncp_add_mem_nls(conn, message, l);
	error = ncp_request(connHandle, 0x15, conn);
	if (error) return error;
	i = ncp_reply_byte(conn, 0);
	memcpy(resultList, ncp_reply_data(conn, 1), i);
	return 0;
}


NWCCODE
NWGetBroadcastMessage(NWCONN_HANDLE connHandle, pnstr8 message) {
	int i, error;
	DECLARE_RQ;

	ncp_init_request_s(conn, 0x0B);
	error = ncp_request(connHandle, 0x15, conn);
	if (error) {
		if (error != 0x89fb) return error;
		ncp_init_request_s(conn, 0x01);
		if ((error = ncp_request(connHandle, 0x15, conn)) != 0) 
			return error;
	}
	i = ncp_reply_byte(conn, 0);
	if (i == 0) return ENOENT;
	memcpy(message, ncp_reply_data(conn, 1), i);
	message[i] = 0;
	ncp_nls_str_n2u(message, message);
	return 0;
}
