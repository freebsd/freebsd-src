/*-
 * Copyright (c) 2005 Jung-uk Kim <jkim@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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

#include <stand.h>
#include <bootstrap.h>

#include "btxv86.h"

/*
 * Detect SMBIOS and export information about the SMBIOS into the
 * environment.
 *
 * System Management BIOS Reference Specification, v2.4 Final
 * http://www.dmtf.org/standards/published_documents/DSP0134.pdf
 */

/*
 * Spec. 2.1.1 SMBIOS Structure Table Entry Point
 *
 * 'The SMBIOS Entry Point structure, described below, can be located by
 * application software by searching for the anchor-string on paragraph
 * (16-byte) boundaries within the physical memory address range
 * 000F0000h to 000FFFFFh.'
 */
#define	SMBIOS_START		0xf0000
#define	SMBIOS_LENGTH		0x10000
#define	SMBIOS_STEP		0x10
#define	SMBIOS_SIG		"_SM_"
#define	SMBIOS_DMI_SIG		"_DMI_"

static u_int8_t	*smbios_parse_table(const u_int8_t *dmi);
static void	smbios_setenv(const char *env, const u_int8_t *dmi,
		    const int offset);
static u_int8_t	smbios_checksum(const u_int8_t *addr, const u_int8_t len);
static u_int8_t	*smbios_sigsearch(const caddr_t addr, const u_int32_t len);

void
smbios_detect(void)
{
	u_int8_t	*smbios, *dmi, *addr;
	u_int16_t	i, length, count;
	u_int32_t	paddr;

	/* locate and validate the SMBIOS */
	smbios = smbios_sigsearch(PTOV(SMBIOS_START), SMBIOS_LENGTH);
	if (smbios == NULL)
		return;

	/* export values from the SMBIOS */
	setenv("hint.smbios.0.enabled", "YES", 1);

	length = *(u_int16_t *)(smbios + 0x16);	/* Structure Table Length */
	paddr = *(u_int32_t *)(smbios + 0x18);	/* Structure Table Address */
	count = *(u_int16_t *)(smbios + 0x1c);	/* No of SMBIOS Structures */

	for (dmi = addr = PTOV(paddr), i = 0;
	     dmi - addr < length && i < count; i++)
		dmi = smbios_parse_table(dmi);
}

static u_int8_t *
smbios_parse_table(const u_int8_t *dmi)
{
	u_int8_t	*dp;

	switch(dmi[0]) {
	case 0:		/* Type 0: BIOS */
		smbios_setenv("hint.smbios.0.bios.vendor", dmi, 0x04);
		smbios_setenv("hint.smbios.0.bios.version", dmi, 0x05);
		smbios_setenv("hint.smbios.0.bios.reldate", dmi, 0x08);
		break;

	case 1:		/* Type 1: System */
		smbios_setenv("hint.smbios.0.system.maker", dmi, 0x04);
		smbios_setenv("hint.smbios.0.system.product", dmi, 0x05);
		smbios_setenv("hint.smbios.0.system.version", dmi, 0x06);
		break;

	case 2:		/* Type 2: Base Board (or Module) */
		smbios_setenv("hint.smbios.0.planar.maker", dmi, 0x04);
		smbios_setenv("hint.smbios.0.planar.product", dmi, 0x05);
		smbios_setenv("hint.smbios.0.planar.version", dmi, 0x06);
		break;

	case 3:		/* Type 3: System Enclosure or Chassis */
		smbios_setenv("hint.smbios.0.chassis.maker", dmi, 0x04);
		smbios_setenv("hint.smbios.0.chassis.version", dmi, 0x06);
		break;

	default: /* skip other types */
		break;
	}
	
	/* find structure terminator */
	dp = (u_int8_t *)(dmi + dmi[1]);
	while (dp[0] != 0 || dp[1] != 0)
		dp++;

	return(dp + 2);
}

static void
smbios_setenv(const char *str, const u_int8_t *dmi, const int offset)
{
	char		*cp;
	int		i;

	/* skip undefined string */
	if (dmi[offset] == 0)
		return;

	for (cp = (char *)(dmi + dmi[1]), i = 0; i < dmi[offset] - 1; i++)
		cp += strlen(cp) + 1;
	setenv(str, cp, 1);
}

static u_int8_t
smbios_checksum(const u_int8_t *addr, const u_int8_t len)
{
	u_int8_t	sum;
	int		i;

	for (sum = 0, i = 0; i < len; i++)
		sum += addr[i];

	return(sum);
}

static u_int8_t *
smbios_sigsearch(const caddr_t addr, const u_int32_t len)
{
	caddr_t		cp;

	/* search on 16-byte boundaries */
	for (cp = addr; cp - addr < len; cp += SMBIOS_STEP) {
		/* compare signature, validate checksum */
		if (!strncmp(cp, SMBIOS_SIG, 4)) {
			if (smbios_checksum(cp, *(cp + 0x05)))
				continue;
			if (strncmp(cp + 0x10, SMBIOS_DMI_SIG, 5))
				continue;
			if (smbios_checksum(cp + 0x10, 0x0f))
				continue;

			return(cp);
		}
	}

	return(NULL);
}
