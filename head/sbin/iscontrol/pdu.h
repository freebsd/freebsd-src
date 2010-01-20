/*-
 * Copyright (c) 2005 Daniel Braniss <danny@cs.huji.ac.il>
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
/*
 | $Id: pdu.h,v 2.1 2006/11/12 08:06:51 danny Exp $
 */

/*
 | keep in BIG endian order (network byte order).
 */

typedef struct login_req {
     char	cmd;	// 0x03

     u_char	NSG:2;
     u_char	CSG:2;
     u_char	_:2;
     u_char	C:1;
     u_char	T:1;

     char	v_max;
     char	v_min;

     int	len;	// remapped via standard bhs
     char	isid[6];
     short	tsih;
     int	itt;	// Initiator Task Tag;

     int	CID:16;
     int	rsv:16;

     int	cmdSN;
     int	expStatSN;
     int	unused[4];
} login_req_t;

typedef struct login_rsp {
     char	cmd;	// 0x23
     u_char	NSG:2;
     u_char	CSG:2;
     u_char	_1:2;
     u_char	C:1;
     u_char	T:1;

     char	v_max;
     char	v_act;

     int	len;	// remapped via standard bhs
     char	isid[6];
     short	tsih;
     int	itt;	// Initiator Task Tag;
     int	_2;
     rsp_sn_t	sn;
     int	status:16;
     int	_3:16;
     int	_4[2];
} login_rsp_t;

typedef struct text_req {
     char	cmd;	// 0x04

     u_char	_1:6;
     u_char	C:1;	// Continuation 
     u_char	F:1;	// Final
     char	_2[2];

     int	len;
     int	itt;		// Initiator Task Tag
     int	LUN[2];
     int	ttt;		// Target Transfer Tag
     int	cmdSN;
     int	expStatSN;
     int	unused[4];
} text_req_t;

/*
 | Responses
 */
typedef struct logout_req {
     char	cmd;	// 0x06
     char	reason;	// 0 - close session
     			// 1 - close connection
     			// 2 - remove the connection for recovery
     char	_2[2];

     int	len;
     int	_r[2];
     int	itt;	// Initiator Task Tag;

     u_int	CID:16;
     u_int	rsv:16;

     int	cmdSN;
     int	expStatSN;
     int	unused[4];
} logout_req_t;

typedef struct logout_rsp {
     char	cmd;	// 0x26
     char	cbits;
     char	_1[2];
     int	len;
     int	_2[2];
     int	itt;
     int	_3;
     rsp_sn_t	sn;
     short	time2wait;
     short	time2retain;
     int	_4;
} logout_rsp_t;
