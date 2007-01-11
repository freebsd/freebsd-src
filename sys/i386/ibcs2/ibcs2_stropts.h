/*-
 * ibcs2_stropts.h
 * Copyright (c) 1995 Scott Bartram
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 *
 * $FreeBSD: src/sys/i386/ibcs2/ibcs2_stropts.h,v 1.2 2005/01/06 23:22:04 imp Exp $
 */

#ifndef _IBCS2_STROPTS_H
#define _IBCS2_STROPTS_H

#define IBCS2_STR		('S'<<8)
#define IBCS2_I_NREAD		(IBCS2_STR|01)
#define IBCS2_I_PUSH		(IBCS2_STR|02)
#define IBCS2_I_POP		(IBCS2_STR|03)
#define IBCS2_I_LOOK		(IBCS2_STR|04)
#define IBCS2_I_FLUSH		(IBCS2_STR|05)
#define IBCS2_I_SRDOPT		(IBCS2_STR|06)
#define IBCS2_I_GRDOPT		(IBCS2_STR|07)
#define IBCS2_I_STR		(IBCS2_STR|010)
#define IBCS2_I_SETSIG		(IBCS2_STR|011)
#define IBCS2_I_GETSIG		(IBCS2_STR|012)
#define IBCS2_I_FIND		(IBCS2_STR|013)
#define IBCS2_I_LINK		(IBCS2_STR|014)
#define IBCS2_I_UNLINK		(IBCS2_STR|015)
#define IBCS2_I_PEEK		(IBCS2_STR|017)
#define IBCS2_I_FDINSERT	(IBCS2_STR|020)
#define IBCS2_I_SENDFD		(IBCS2_STR|021)
#define IBCS2_I_RECVFD		(IBCS2_STR|022)

#endif /* _IBCS2_STROPTS_H */
