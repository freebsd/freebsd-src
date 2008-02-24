/*-
 * Copyright (c) 2005, 2006 Jung-uk Kim <jkim@FreeBSD.org>
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
__FBSDID("$FreeBSD: src/sys/boot/i386/libi386/smbios.c,v 1.7 2007/05/21 18:48:18 jkim Exp $");

#include <stand.h>
#include <bootstrap.h>

#include "btxv86.h"
#include "libi386.h"

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

static uint8_t	smbios_enabled_sockets = 0;
static uint8_t	smbios_populated_sockets = 0;

static uint8_t	*smbios_parse_table(const uint8_t *dmi);
static void	smbios_setenv(const char *name, const uint8_t *dmi,
		    const int offset);
static uint8_t	smbios_checksum(const caddr_t addr, const uint8_t len);
static uint8_t	*smbios_sigsearch(const caddr_t addr, const uint32_t len);

#ifdef SMBIOS_SERIAL_NUMBERS
static void	smbios_setuuid(const char *name, const uint8_t *dmi,
		    const int offset);
#endif

void
smbios_detect(void)
{
	uint8_t		*smbios, *dmi, *addr;
	uint16_t	i, length, count;
	uint32_t	paddr;
	char		buf[4];

	/* locate and validate the SMBIOS */
	smbios = smbios_sigsearch(PTOV(SMBIOS_START), SMBIOS_LENGTH);
	if (smbios == NULL)
		return;

	length = *(uint16_t *)(smbios + 0x16);	/* Structure Table Length */
	paddr = *(uint32_t *)(smbios + 0x18);	/* Structure Table Address */
	count = *(uint16_t *)(smbios + 0x1c);	/* No of SMBIOS Structures */

	for (dmi = addr = PTOV(paddr), i = 0;
	     dmi - addr < length && i < count; i++)
		dmi = smbios_parse_table(dmi);
	sprintf(buf, "%d", smbios_enabled_sockets);
	setenv("smbios.socket.enabled", buf, 1);
	sprintf(buf, "%d", smbios_populated_sockets);
	setenv("smbios.socket.populated", buf, 1);
}

static uint8_t *
smbios_parse_table(const uint8_t *dmi)
{
	uint8_t		*dp;

	switch(dmi[0]) {
	case 0:		/* Type 0: BIOS */
		smbios_setenv("smbios.bios.vendor", dmi, 0x04);
		smbios_setenv("smbios.bios.version", dmi, 0x05);
		smbios_setenv("smbios.bios.reldate", dmi, 0x08);
		break;

	case 1:		/* Type 1: System */
		smbios_setenv("smbios.system.maker", dmi, 0x04);
		smbios_setenv("smbios.system.product", dmi, 0x05);
		smbios_setenv("smbios.system.version", dmi, 0x06);
#ifdef SMBIOS_SERIAL_NUMBERS
		smbios_setenv("smbios.system.serial", dmi, 0x07);
		smbios_setuuid("smbios.system.uuid", dmi, 0x08);
#endif
		break;

	case 2:		/* Type 2: Base Board (or Module) */
		smbios_setenv("smbios.planar.maker", dmi, 0x04);
		smbios_setenv("smbios.planar.product", dmi, 0x05);
		smbios_setenv("smbios.planar.version", dmi, 0x06);
#ifdef SMBIOS_SERIAL_NUMBERS
		smbios_setenv("smbios.planar.serial", dmi, 0x07);
#endif
		break;

	case 3:		/* Type 3: System Enclosure or Chassis */
		smbios_setenv("smbios.chassis.maker", dmi, 0x04);
		smbios_setenv("smbios.chassis.version", dmi, 0x06);
#ifdef SMBIOS_SERIAL_NUMBERS
		smbios_setenv("smbios.chassis.serial", dmi, 0x07);
		smbios_setenv("smbios.chassis.tag", dmi, 0x08);
#endif
		break;

	case 4:		/* Type 4: Processor Information */
		/*
		 * Offset 18h: Processor Status
		 *
		 * Bit 7	Reserved, must be 0
		 * Bit 6	CPU Socket Populated
		 *		1 - CPU Socket Populated
		 *		0 - CPU Socket Unpopulated
		 * Bit 5:3	Reserved, must be zero
		 * Bit 2:0	CPU Status
		 *		0h - Unknown
		 *		1h - CPU Enabled
		 *		2h - CPU Disabled by User via BIOS Setup
		 *		3h - CPU Disabled by BIOS (POST Error)
		 *		4h - CPU is Idle, waiting to be enabled
		 *		5-6h - Reserved
		 *		7h - Other
		 */
		if ((dmi[0x18] & 0x07) == 1)
			smbios_enabled_sockets++;
		if (dmi[0x18] & 0x40)
			smbios_populated_sockets++;
		break;

	default: /* skip other types */
		break;
	}
	
	/* find structure terminator */
	dp = __DECONST(uint8_t *, dmi + dmi[1]);
	while (dp[0] != 0 || dp[1] != 0)
		dp++;

	return(dp + 2);
}

static void
smbios_setenv(const char *name, const uint8_t *dmi, const int offset)
{
	char		*cp = __DECONST(char *, dmi + dmi[1]);
	int		i;

	/* skip undefined string */
	if (dmi[offset] == 0)
		return;

	for (i = 0; i < dmi[offset] - 1; i++)
		cp += strlen(cp) + 1;
	setenv(name, cp, 1);
}

static uint8_t
smbios_checksum(const caddr_t addr, const uint8_t len)
{
	const uint8_t	*cp = addr;
	uint8_t		sum;
	int		i;

	for (sum = 0, i = 0; i < len; i++)
		sum += cp[i];

	return(sum);
}

static uint8_t *
smbios_sigsearch(const caddr_t addr, const uint32_t len)
{
	caddr_t		cp;

	/* search on 16-byte boundaries */
	for (cp = addr; cp < addr + len; cp += SMBIOS_STEP) {
		/* compare signature, validate checksum */
		if (!strncmp(cp, SMBIOS_SIG, 4)) {
			if (smbios_checksum(cp, *(uint8_t *)(cp + 0x05)))
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

#ifdef SMBIOS_SERIAL_NUMBERS
static void
smbios_setuuid(const char *name, const uint8_t *dmi, const int offset)
{
	const uint8_t	*idp = dmi + offset;
	int		i, f = 0, z = 0;
	char		uuid[37];

	for (i = 0; i < 16; i++) {
		if (idp[i] == 0xff)
			f++;
		else if (idp[i] == 0x00)
			z++;
		else
			break;
	}
	if (f != 16 && z != 16) {
		sprintf(uuid, "%02x%02x%02x%02x-"
		    "%02x%02x-%02x%02x-%02x%02x-"
		    "%02x%02x%02x%02x%02x%02x",
		    idp[0], idp[1], idp[2], idp[3],
		    idp[4], idp[5], idp[6], idp[7], idp[8], idp[9],
		    idp[10], idp[11], idp[12], idp[13], idp[14], idp[15]);
		setenv(name, uuid, 1);
	}
}
#endif
