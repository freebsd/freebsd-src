/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Obtain memory configuration information from the BIOS
 */
#include <stand.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <machine/metadata.h>
#include "bootstrap.h"
#include "libi386.h"
#include "btxv86.h"

#define SMAPSIG	0x534D4150

struct smap {
	u_int64_t	base;
	u_int64_t	length;
	u_int32_t	type;
} __packed;

static struct smap smap;

static struct smap *smapbase;
static int smaplen;

void
bios_getsmap(void)
{
	int n;

	n = 0;
	smaplen = 0;
	/* Count up segments in system memory map */
	v86.ebx = 0;
	do {
		v86.ctl = V86_FLAGS;
		v86.addr = 0x15;		/* int 0x15 function 0xe820*/
		v86.eax = 0xe820;
		v86.ecx = sizeof(struct smap);
		v86.edx = SMAPSIG;
		v86.es = VTOPSEG(&smap);
		v86.edi = VTOPOFF(&smap);
		v86int();
		if ((v86.efl & 1) || (v86.eax != SMAPSIG))
			break;
		n++;
	} while (v86.ebx != 0);
	if (n == 0)
		return;
	n += 10;	/* spare room */
	smapbase = malloc(n * sizeof(*smapbase));

	/* Save system memory map */
	v86.ebx = 0;
	do {
		v86.ctl = V86_FLAGS;
		v86.addr = 0x15;		/* int 0x15 function 0xe820*/
		v86.eax = 0xe820;
		v86.ecx = sizeof(struct smap);
		v86.edx = SMAPSIG;
		v86.es = VTOPSEG(&smapbase[smaplen]);
		v86.edi = VTOPOFF(&smapbase[smaplen]);
		v86int();
		smaplen++;
		if ((v86.efl & 1) || (v86.eax != SMAPSIG))
			break;
	} while (v86.ebx != 0 && smaplen < n);
}
void
bios_addsmapdata(struct preloaded_file *kfp)
{
	int len;

	if (smapbase == 0 || smaplen == 0)
		return;
	len = smaplen * sizeof(*smapbase);
	file_addmetadata(kfp, MODINFOMD_SMAP, len, smapbase);
}
