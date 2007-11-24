/*	$NetBSD: ibcs2_unistd.h,v 1.2 1994/10/26 02:53:11 cgd Exp $	*/
/* $FreeBSD$ */

/*-
 * Copyright (c) 1994 Scott Bartram
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
 *      This product includes software developed by Scott Bartram.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_IBCS2_UNISTD_H
#define	_IBCS2_UNISTD_H

#define IBCS2_R_OK		4
#define IBCS2_W_OK		2
#define IBCS2_X_OK		1
#define IBCS2_F_OK		0

#define IBCS2_F_ULOCK		0
#define IBCS2_F_LOCK		1
#define IBCS2_F_TLOCK		2
#define IBCS2_F_TEST		3

#define IBCS2_SEEK_SET		0
#define IBCS2_SEEK_CUR		1
#define IBCS2_SEEK_END		2

#define IBCS2_SC_ARG_MAX		0
#define IBCS2_SC_CHILD_MAX		1
#define IBCS2_SC_CLK_TCK		2
#define IBCS2_SC_NGROUPS_MAX		3
#define IBCS2_SC_OPEN_MAX		4
#define IBCS2_SC_JOB_CONTROL		5
#define IBCS2_SC_SAVED_IDS		6
#define IBCS2_SC_VERSION		7
#define IBCS2_SC_PASS_MAX		8
#define IBCS2_SC_XOPEN_VERSION		9

#define IBCS2_PC_LINK_MAX		0
#define IBCS2_PC_MAX_CANON		1
#define IBCS2_PC_MAX_INPUT		2
#define IBCS2_PC_NAME_MAX		3
#define IBCS2_PC_PATH_MAX		4
#define IBCS2_PC_PIPE_BUF		5
#define IBCS2_PC_CHOWN_RESTRICTED	6
#define IBCS2_PC_NO_TRUNC		7
#define IBCS2_PC_VDISABLE		8

#define IBCS2_STDIN_FILENO		0
#define IBCS2_STDOUT_FILENO		1
#define IBCS2_STDERR_FILENO		2

#endif /* _IBCS2_UNISTD_H */
