/*-
 * Copyright (c) 2016 Landon Fuller <landonf@FreeBSD.org>
 *
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

#include <dev/bhnd/bhnd.h>
#include <dev/bhnd/bhndreg.h>

#include <dev/bhnd/siba/sibareg.h>
#include <dev/bhnd/siba/sibavar.h>

#include "bcm_machdep.h"

int
bcm_find_core_siba(struct bhnd_chipid *chipid, bhnd_devclass_t devclass,
    int unit, struct bhnd_core_info *info, uintptr_t *addr)
{
	struct siba_core_id	scid;
	uintptr_t		cc_addr;
	uint32_t		idhigh, idlow;

	/* No other cores are required during early boot on siba(4) devices */
	if (devclass != BHND_DEVCLASS_CC || unit != 0)
		return (ENOENT);

	cc_addr = chipid->enum_addr;
	idhigh = BCM_SOC_READ_4(cc_addr, SB0_REG_ABS(SIBA_CFG0_IDHIGH));
	idlow = BCM_SOC_READ_4(cc_addr, SB0_REG_ABS(SIBA_CFG0_IDHIGH));

	scid = siba_parse_core_id(idhigh, idlow, 0, 0);

	if (info != NULL)
		*info = scid.core_info;

	if (addr != NULL)
		*addr = cc_addr;

	return (0);
}
