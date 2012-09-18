/*-
 * Copyright (c) 2012 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <vis.h>

#define	_VIS_OGLOB	0x100

char *
__vis_oglob(char *dst, int c, int flag, int nextc)
{

	if (flag & _VIS_OGLOB)
		flag = (flag & ~_VIS_OGLOB) ^ VIS_GLOB;
	return vis(dst, c, flag, nextc);
}

int
__strvis_oglob(char *dst, const char *src, int flag)
{

	if (flag & _VIS_OGLOB)
		flag = (flag & ~_VIS_OGLOB) ^ VIS_GLOB;
	return strvis(dst, src, flag);
}

int
__strvisx_oglob(char *dst, const char *src, size_t len, int flag)
{

	if (flag & _VIS_OGLOB)
		flag = (flag & ~_VIS_OGLOB) ^ VIS_GLOB;
	return strvisx(dst, src, len, flag);
}

__sym_compat(vis, __vis_oglob, FBSD_1.0);
__sym_compat(strvis, __strvis_oglob, FBSD_1.0);
__sym_compat(strvisx, __strvisx_oglob, FBSD_1.0);
