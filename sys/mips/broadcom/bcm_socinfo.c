/*-
 * Copyright (c) 2016 Michael Zhilin <mizhka@gmail.com>
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

#include <sys/param.h>

#include <dev/bhnd/cores/chipc/chipcreg.h>

#include "bcm_machdep.h"
#include "bcm_socinfo.h"

/* found on https://wireless.wiki.kernel.org/en/users/drivers/b43/soc */
struct bcm_socinfo bcm_socinfos[] = {
		{0x00005300, 600, 25000000, 1}, /* BCM4706 to check */
		{0x0022B83A, 300, 20000000, 1}, /* BCM4716B0 ASUS RT-N12  */
		{0x00914716, 354, 20000000, 1}, /* BCM4717A1 to check  */
		{0x00A14716, 480, 20000000, 1}, /* BCM4718A1 ASUS RT-N16 */
		{0x00435356, 300, 25000000, 1}, /* BCM5356A1 (RT-N10, WNR1000v3) */
		{0x00825357, 500, 20000000, 1}, /* BCM5358UB0 ASUS RT-N53A1 */
		{0x00845357, 300, 20000000, 1}, /* BCM5357B0 to check */
		{0x00945357, 500, 20000000, 1}, /* BCM5358 */
		{0x00A45357, 500, 20000000, 1}, /* BCM47186B0 Tenda N60  */
		{0x0085D144, 300, 20000000, 1}, /* BCM5356C0 */
		{0x00B5D144, 300, 20000000, 1}, /* BCM5357C0 */
		{0x00015365, 200, 0, 1},	/* BCM5365 */
		{0,0,0}
};

/* Most popular BCM SoC info */
struct bcm_socinfo BCM_DEFAULT_SOCINFO = {0x0, 300, 20000000, 0};

struct bcm_socinfo*
bcm_get_socinfo_by_socid(uint32_t key)
{
	struct bcm_socinfo* start;

	if(!key)
		return (NULL);

	for(start = bcm_socinfos; start->id > 0; start++)
		if(start->id == key)
			return (start);

	return (NULL);
}

struct bcm_socinfo*
bcm_get_socinfo(void)
{
	uint32_t		socid;
	struct bcm_socinfo	*socinfo;

	/*
	 * We need Chip ID + Revision + Package
	 * --------------------------------------------------------------
         * | 	Mask		| Usage					|
         * --------------------------------------------------------------
	 * |	0x0000FFFF	| Chip ID				|
	 * |	0x000F0000	| Chip Revision				|
	 * |	0x00F00000	| Package Options			|
	 * |	0x0F000000	| Number of Cores (ChipCommon Rev. >= 4)|
	 * |	0xF0000000	| Chip Type				|
	 * --------------------------------------------------------------
	 */

	socid = BCM_CHIPC_READ_4(CHIPC_ID) & 0x00FFFFFF;
	socinfo = bcm_get_socinfo_by_socid(socid);
	return (socinfo != NULL) ? socinfo : &BCM_DEFAULT_SOCINFO;
}
