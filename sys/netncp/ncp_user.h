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
#ifndef _NETNCP_NCP_USER_H_
#define _NETNCP_NCP_USER_H_

/* 
 * "ncp" interface to kernel, this can be done via syscalls but may eat
 * a lot of them, so we select internal code, define req's and replays
 * as necessary. Structure for call is simple:
 * byte=NCP_CONN
 * byte=NCP_CONN_SUBFN
 * ....=data
 */
#define	NCP_CONN		0xF5	/* change if that will occupied */
#define	NCP_CONN_READ		0x01	/* read from file handle */
#define	NCP_CONN_WRITE		0x02	/* write to file handle */
#define	NCP_CONN_SETFLAGS	0x03	/* word mask, word flags */
#define	NCP_CONN_LOGIN		0x04	/* bind login on handle */
#define	NCP_CONN_GETINFO	0x05	/* get information about connection */
#define	NCP_CONN_GETUSER	0x06	/* get user name for connection */
#define	NCP_CONN_CONN2REF	0x07	/* convert handle to reference */
#define	NCP_CONN_CONNCLOSE	0x08	/* release connection handle */
#define	NCP_CONN_FRAG		0x09	/* ncp fragmented request */
#define	NCP_CONN_DUP		0x0A	/* get an additional handle */
#define	NCP_CONN_GETDATA	0x0B	/* retrieve NCP_CD_* vals */
#define	NCP_CONN_SETDATA	0x0C	/* store NCP_CD_* vals */

/*
 * Internal connection data can be set by owner or superuser and retrieved
 * only by superuser
 */
#define	NCP_CD_NDSLOGINKEY	0x01
#define	NCP_CD_NDSPRIVATEKEY	0x02
#define NCP_CD_NDSUFLAGS	0x03

/* user side structures to issue fragmented ncp calls */
typedef struct {
	char	*fragAddress;
	u_int32_t fragSize;
} NW_FRAGMENT;


struct ncp_rw {
	ncp_fh	nrw_fh;
	char 	*nrw_base;
	off_t	nrw_offset;
	int	nrw_cnt;
};

struct ncp_conn_login {
	char	*username;
	int	objtype;
	char	*password;
};

struct ncp_conn_frag {
	int		cc;		/* completion code */
	int		cs;		/* connection state */
	int		fn;
	int		rqfcnt;
	NW_FRAGMENT	*rqf;
	int		rpfcnt;
	NW_FRAGMENT	*rpf;
};

#endif
