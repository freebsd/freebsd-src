/*
 *   Copyright (c) 1996, 2000 Hellmuth Michaelis. All rights reserved.
 *
 *   Copyright (c) 1996 Gary Jennejohn.  All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	trace.h - header file for isdn trace
 *	------------------------------------
 *
 *	$Id: trace.h,v 1.12 2000/02/14 16:25:22 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Feb 14 14:43:40 2000]
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
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
#include <sys/stat.h>
#include <sys/param.h>

#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>

#include "pcause_1tr6.h"	/* obsolete german national ISDN */
#include "pcause_q850.h"

#define I4BTRC_DEVICE		"/dev/i4btrc"	/* trace device file */
#define TRACE_FILE_NAME		"isdntrace"	/* default output filename */
#define BIN_FILE_NAME		"isdntracebin"	/* default binary filename */

#define BSIZE	4096	/* read buffer size	*/
#define NCOLS	80	/* screen width		*/

#define RxUDEF	0	/* analyze mode, default unit for receiver side */
#define TxUDEF	1	/* analyze mode, default unit for transmitter side */

int decode_lapd(char *pbuf, int n, unsigned char *buf, int is_te, int raw, int printit);
void decode_q931(char *pbuf, int n, int off, unsigned char *buf, int raw);
void decode_unknownl3(char *pbuf, int n, int off, unsigned char *buf, int raw);
void decode_1tr6(char *pbuf, int n, int off, unsigned char *buf, int raw);
char *print_error(int prot, unsigned char code);
int q931_facility(char *pbuf, unsigned char *buf);
int p_q931cause(char *pbuf, unsigned char *buf);
int p_q931address(char *pbuf, unsigned char *buf);
int p_q931bc(char *pbuf, unsigned char *buf);
int p_q931high_compat(char *pbuf, unsigned char *buf);
int q932_facility(char *pbuf, unsigned char *buf);
int p_q931user_user(char *pbuf, unsigned char *buf);
int p_q931notification(char *pbuf, unsigned char *buf);
int p_q931redir(char *pbuf, unsigned char *buf);

/* EOF */
