/*-
 * Copyright (c) 2003 Jake Burkholder
 * Copyright (c) 2003 Poul-Henning Kamp
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
 *
 * Functions to encode or decode struct sun_disklabel into a bytestream
 * of correct endianess and packing.
 *
 * NB!  This file must be usable both in kernel and userland.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/sun_disklabel.h>

#define	SL_TEXT		0x0
#define	SL_TEXT_SIZEOF	0x80
#define	SL_RPM		0x1a4
#define	SL_PCYLINDERS	0x1a6
#define	SL_SPARESPERCYL	0x1a8
#define	SL_INTERLEAVE	0x1ae
#define	SL_NCYLINDERS	0x1b0
#define	SL_ACYLINDERS	0x1b2
#define	SL_NTRACKS	0x1b4
#define	SL_NSECTORS	0x1b6
#define	SL_PART		0x1bc
#define	SL_MAGIC	0x1fc
#define	SL_CKSUM	0x1fe

#define	SDKP_CYLOFFSET	0
#define	SDKP_NSECTORS	0x4
#define	SDKP_SIZEOF	0x8

/*
 * Decode the relevant fields of a sun disk label, and return zero if the
 * magic and checksum works out OK.
 */
int
sunlabel_dec(void const *pp, struct sun_disklabel *sl)
{
	const uint8_t *p;
	size_t i;
	u_int u;

	p = pp;
	for (i = 0; i < sizeof(sl->sl_text); i++)
		sl->sl_text[i] = p[SL_TEXT + i];
	sl->sl_rpm = be16dec(p + SL_RPM);
	sl->sl_pcylinders = be16dec(p + SL_PCYLINDERS);
	sl->sl_sparespercyl = be16dec(p + SL_SPARESPERCYL);
	sl->sl_interleave = be16dec(p + SL_INTERLEAVE);
	sl->sl_ncylinders = be16dec(p + SL_NCYLINDERS);
	sl->sl_acylinders = be16dec(p + SL_ACYLINDERS);
	sl->sl_ntracks = be16dec(p + SL_NTRACKS);
	sl->sl_nsectors = be16dec(p + SL_NSECTORS);
	for (i = 0; i < SUN_NPART; i++) {
		sl->sl_part[i].sdkp_cyloffset = be32dec(p + SL_PART +
		    (i * SDKP_SIZEOF) + SDKP_CYLOFFSET);
		sl->sl_part[i].sdkp_nsectors = be32dec(p + SL_PART +
		    (i * SDKP_SIZEOF) + SDKP_NSECTORS);
	}
	sl->sl_magic = be16dec(p + SL_MAGIC);
	for (i = u = 0; i < SUN_SIZE; i += 2)
		u ^= be16dec(p + i);
	if (u == 0 && sl->sl_magic == SUN_DKMAGIC)
		return (0);
	else
		return (EINVAL);
}

/*
 * Encode the relevant fields into a sun disklabel, compute new checksum.
 */
void
sunlabel_enc(void *pp, struct sun_disklabel *sl)
{
	uint8_t *p;
	size_t i;
	u_int u;

	p = pp;
	for (i = 0; i < SL_TEXT_SIZEOF; i++)
		p[SL_TEXT + i] = sl->sl_text[i];
	be16enc(p + SL_RPM, sl->sl_rpm);
	be16enc(p + SL_PCYLINDERS, sl->sl_pcylinders);
	be16enc(p + SL_SPARESPERCYL, sl->sl_sparespercyl);
	be16enc(p + SL_INTERLEAVE, sl->sl_interleave);
	be16enc(p + SL_NCYLINDERS, sl->sl_ncylinders);
	be16enc(p + SL_ACYLINDERS, sl->sl_acylinders);
	be16enc(p + SL_NTRACKS, sl->sl_ntracks);
	be16enc(p + SL_NSECTORS, sl->sl_nsectors);
	for (i = 0; i < SUN_NPART; i++) {
		be32enc(p + SL_PART + (i * SDKP_SIZEOF) + SDKP_CYLOFFSET,
		    sl->sl_part[i].sdkp_cyloffset);
		be32enc(p + SL_PART + (i * SDKP_SIZEOF) + SDKP_NSECTORS,
		    sl->sl_part[i].sdkp_nsectors);
	}
	be16enc(p + SL_MAGIC, sl->sl_magic);
	for (i = u = 0; i < SUN_SIZE; i += 2)
		u ^= be16dec(p + i);
	be16enc(p + SL_CKSUM, u);
}
