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

#ifndef _SYS_KERNELDUMP_H
#define _SYS_KERNELDUMP_H

struct kerneldumpheader {
	char		magic[20];
#	    define KERNELDUMPMAGIC "FreeBSD Kernel Dump"
	char		architecture[12];
	uint32_t	version;
#	    define KERNELDUMPVERSION	1
	uint32_t	architectureversion;
#	    define KERNELDUMP_I386_VERSION 1
#	    define KERNELDUMP_IA64_VERSION 1
	uint64_t	dumplength;	/* excl headers */
	uint32_t	blocksize;
	uint64_t	dumptime;
	char		hostname[64];
	char		versionstring[192];
	char		panicstring[192];
	uint32_t	parity;
};

static __inline u_int32_t
kerneldump_parity(struct kerneldumpheader *kdhp)
{
	uint32_t *up, parity;
	u_int i;

	up = (uint32_t *)kdhp;
	parity = 0;
	for (i = 0; i < sizeof *kdhp; i += sizeof *up)
		parity ^= *up++;
	return (parity);
}


#endif /* _SYS_KERNELDUMP_H */
