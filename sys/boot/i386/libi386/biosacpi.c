/*-
 * Copyright (c) 2001 Michael Smith <msmith@freebsd.org>
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

#include <stand.h>
#include <machine/stdarg.h>
#include <bootstrap.h>

#include "acfreebsd.h"
#include "acconfig.h"
#define ACPI_SYSTEM_XFACE
#include "actypes.h"
#include "actbl.h"

/*
 * Detect ACPI and export information about the APCI BIOS into the
 * environment.
 */

static RSDP_DESCRIPTOR	*biosacpi_find_rsdp(void);
static RSDP_DESCRIPTOR	*biosacpi_search_rsdp(char *base, int length);

#define RSDP_CHECKSUM_LENGTH 20

void
biosacpi_detect(void)
{
    RSDP_DESCRIPTOR	*rsdp;
    char		buf[16];
    int			revision;

    /* XXX check the BIOS datestamp */

    /* locate and validate the RSDP */
    if ((rsdp = biosacpi_find_rsdp()) == NULL)
	return;

    /* export values from the RSDP */
    revision = rsdp->Revision;
    if (revision == 0)
	revision = 1;
    sprintf(buf, "%d", revision);
    setenv("hint.acpi.0.revision", buf, 1);
    sprintf(buf, "%6s", rsdp->OemId);
    buf[6] = '\0';
    setenv("hint.acpi.0.oem", buf, 1);
    sprintf(buf, "0x%08x", rsdp->RsdtPhysicalAddress);
    setenv("hint.acpi.0.rsdt", buf, 1);
    if (revision >= 2) {
	/* XXX extended checksum? */
	sprintf(buf, "0x%016llx", rsdp->XsdtPhysicalAddress);
	setenv("hint.acpi.0.xsdt", buf, 1);
	sprintf(buf, "%d", rsdp->Length);
	setenv("hint.acpi.0.xsdt_length", buf, 1);
    }
    /* XXX other tables? */

    setenv("acpi_load", "YES", 1);
}

/*
 * Find the RSDP in low memory.
 */
static RSDP_DESCRIPTOR *
biosacpi_find_rsdp(void)
{
    RSDP_DESCRIPTOR	*rsdp;

    /* search the EBDA */
    if ((rsdp = biosacpi_search_rsdp((char *)0, 0x400)) != NULL)
	return(rsdp);

    /* search the BIOS space */
    if ((rsdp = biosacpi_search_rsdp((char *)0xe0000, 0x20000)) != NULL)
	return(rsdp);

    return(NULL);
}

static RSDP_DESCRIPTOR *
biosacpi_search_rsdp(char *base, int length)
{
    RSDP_DESCRIPTOR	*rsdp;
    u_int8_t		*cp, sum;
    int			ofs, idx;

    /* search on 16-byte boundaries */
    for (ofs = 0; ofs < length; ofs += 16) {
	rsdp = (RSDP_DESCRIPTOR *)(base + ofs);

	/* compare signature, validate checksum */
	if (!strncmp(rsdp->Signature, RSDP_SIG, strlen(RSDP_SIG))) {
	    cp = (u_int8_t *)rsdp;
	    sum = 0;
	    for (idx = 0; idx < RSDP_CHECKSUM_LENGTH; idx++)
		sum += *(cp + idx);
	    if (sum != 0) {
		printf("acpi: bad RSDP checksum (%d)\n", sum);
		continue;
	    }
	    return(rsdp);
	}
    }
    return(NULL);
}
