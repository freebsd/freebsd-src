/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	decode.h - isdndecode header file
 *	---------------------------------
 *
 *	$Id: decode.h,v 1.6 1999/12/13 21:25:25 hm Exp $
 *
 * $FreeBSD: src/usr.sbin/i4b/isdndecode/decode.h,v 1.6 1999/12/14 21:07:36 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:49:50 1999]
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include "pcause.h"

#define I4BTRC_DEVICE		"/dev/i4btrc"	/* trace device file */
#define DECODE_FILE_NAME	"isdndecode"	/* default output filename */
#define DECODE_FILE_NAME_BAK	".last"		/* backup filename trailer */
#define BIN_FILE_NAME		"isdntracebin"	/* default binary filename */

#define BSIZE	4096	/* read buffer size	*/
#define NCOLS	80	/* screen width		*/

#define RxUDEF	0	/* analyze mode, default unit for receiver side */
#define TxUDEF	1	/* analyze mode, default unit for transmitter side */

void layer1(char *pbuf, unsigned char *buf);
int layer2(char *pbuf, unsigned char *buf, int is_te, int printit);
void layer3(char *pbuf, int n, int off, unsigned char *buf);
int q932_facility(char *pbuf, unsigned char *buf);
void sprintline(int, char *, int, int, int, const char *, ...);
void extension(int, char *, int, unsigned char, unsigned char);
	
/* EOF */
