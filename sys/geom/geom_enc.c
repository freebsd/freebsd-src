/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * This file contains routines to pack and unpack integerfields of varying
 * width and endianess with.  Ideally the C language would have this built
 * in but I guess Dennis and Brian forgot that back in the early 70ies.
 *
 * The function names are systematic: g_{enc|dec}_{be|le}%d
 *	enc -> encode
 *	dec -> decode
 *	be -> big endian
 *	le -> little endian
 *	%d -> width in bytes
 *
 * Please keep the functions sorted:
 *	decode before encode
 *	big endian before little endian
 *	small width before larger width
 *	(this happens to be alphabetically)
 */

#include <sys/param.h>
#ifdef _KERNEL
#include <sys/malloc.h>
#include <sys/systm.h>
#endif
#include <geom/geom.h>

uint16_t
g_dec_be2(const u_char *p)
{

	return((p[0] << 8) | p[1]);
}

uint32_t
g_dec_be4(const u_char *p)
{

	return((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

uint16_t
g_dec_le2(const u_char *p)
{

	return((p[1] << 8) | p[0]);
}

uint32_t
g_dec_le4(const u_char *p)
{

	return((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
}

uint64_t
g_dec_le8(const u_char *p)
{

	return(g_dec_le4(p) | ((uint64_t)(g_dec_le4(p + 4)) << 32));
}

void
g_enc_le2(u_char *p, uint16_t u)
{

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
}

void
g_enc_le4(u_char *p, uint32_t u)
{

	p[0] = u & 0xff;
	p[1] = (u >> 8) & 0xff;
	p[2] = (u >> 16) & 0xff;
	p[3] = (u >> 24) & 0xff;
}

void
g_enc_le8(u_char *p, uint64_t u)
{

	g_enc_le4(p, u);
	g_enc_le4(p + 4, u >> 32);
}
