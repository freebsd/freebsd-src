/* 
 * Contributed by HD Associates (hd@world.std.com).
 * Copyright (c) 1992, 1993 HD Associates
 *
 * Berkeley style copyright.  I've just snarfed it out of stdio.h:
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)stdio.h	5.17 (Berkeley) 6/3/91
 *	$Id: sgio.h,v 1.2 1993/10/16 17:17:35 rgrimes Exp $
 */

/*
 * SGI dsreq.h clone derived from the man page.
 * On SGI this header is in usr/include/sys.
 */

#ifndef _SYS_SGIO_H_
#define _SYS_SGIO_H_

#include <sys/types.h>
#include <sys/ioctl.h>

typedef struct dsreq {
	u_long	ds_flags;
	u_long	ds_time;

	u_long	ds_private;

	caddr_t ds_cmdbuf;
	u_char ds_cmdlen;

	caddr_t ds_databuf;
	u_long ds_datalen;

	caddr_t ds_sensebuf;
	u_char ds_senselen;

	u_char ds_ret;
	u_char ds_status;
	u_char ds_msg;
	u_char ds_cmdsent;
	u_long ds_datasent;
	u_char ds_sensesent;
} dsreq_t;

#define DS_ENTER		_IOWR('d', 1, struct dsreq)
#define DS_DISCONNECT	_IOW('d', 2, int)
#define DS_SYNC			_IOW('d', 3, int)
#define DS_TARGET		_IOW('d', 4, int)

/* Data transfer:
 */
#define DSRQ_READ   0x00000001
#define DSRQ_WRITE  0x00000002
#define DSRQ_IOV	0x00000004
#define DSRQ_BUF	0x00000008

/* devscsi options:
 */
#define DSRQ_ASYNC	0x00000010
#define DSRQ_SENSE	0x00000020
#define DSRQ_TARGET	0x00000040

/* select options:
 */
#define DSRQ_SELATN	0x00000080
#define DSRQ_DISC	0x00000100
#define DSRQ_SYNXFR	0x00000200
#define DSRQ_SELMSG	0x00000400

/* progress/continuation callbacs:
 */
#define DSRQ_CALL	0x00000800
#define DSRQ_ACKH	0x00001000
#define DSRQ_ATNH	0x00002000
#define DSRQ_ABORT	0x00004000

/* Host options (non-portable):
 */
#define DSRQ_TRACE	0x00008000
#define DSRQ_PRINT	0x00010000
#define DSRQ_CTRL1	0x00020000
#define DSRQ_CTRL2	0x00040000

/* Additional flags:
 */
#define DSRQ_MIXRDWR	0x00080000

#define DSRT_OK 0

#define DSRT_DEVSCSI 1
#define DSRT_MULT    2
#define DSRT_CANCEL  3
#define DSRT_REVCODE 4
#define DSRT_AGAIN   5
#define DSRT_UNIMPL  6

#define DSRT_HOST    7
#define DSRT_NOSEL   8
#define DSRT_SHORT   9
#define DSRT_SENSE   10
#define DSRT_NOSENSE 11
#define DSRT_TIMEOUT 12
#define DSRT_LONG    13

#define DSRT_PROTO    14
#define DSRT_EBSY     15
#define DSRT_REJECT   16
#define DSRT_PARITY   17
#define DSRT_MEMORY   18
#define DSRT_CMDO     19
#define DSRT_STAI     20

#ifdef BRAINDEAD
/* BUG: This does not belong here,
 * but I don't want to break my code; it will be moved out
 * in the near future.
 */

#define CMDBUF(DS) (DS)->ds_cmdbuf 
#endif

#endif /* _SYS_SGIO_H_ */
