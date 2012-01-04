/*-
 * Copyright (c) 1998 Robert Nordier
 * Copyright (c) 2010 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <btxv86.h>

#include "rbx.h"
#include "util.h"
#include "drv.h"
#ifdef USE_XREAD
#include "xreadorg.h"
#endif

#ifdef GPT
uint64_t
drvsize(struct dsk *dskp)
{
	unsigned char params[0x42];
	uint64_t sectors;

	*(uint32_t *)params = sizeof(params);

	v86.ctl = V86_FLAGS;
	v86.addr = 0x13;
	v86.eax = 0x4800;
	v86.edx = dskp->drive;
	v86.ds = VTOPSEG(params);
	v86.esi = VTOPOFF(params);
	v86int();
	if (V86_CY(v86.efl)) {
		printf("error %u\n", v86.eax >> 8 & 0xff);
		return (0);
	}
	memcpy(&sectors, params + 0x10, sizeof(sectors));
	return (sectors);
}
#endif	/* GPT */

#ifndef USE_XREAD
static struct {
	uint16_t	len;
	uint16_t	count;
	uint16_t	off;
	uint16_t	seg;
	uint64_t	lba;
} packet;
#endif

int
drvread(struct dsk *dskp, void *buf, daddr_t lba, unsigned nblk)
{
	static unsigned c = 0x2d5c7c2f;

	if (!OPT_CHECK(RBX_QUIET))
		printf("%c\b", c = c << 8 | c >> 24);
#ifndef USE_XREAD
	packet.len = 0x10;
	packet.count = nblk;
	packet.off = VTOPOFF(buf);
	packet.seg = VTOPSEG(buf);
	packet.lba = lba;
	v86.ctl = V86_FLAGS;
	v86.addr = 0x13;
	v86.eax = 0x4200;
	v86.edx = dskp->drive;
	v86.ds = VTOPSEG(&packet);
	v86.esi = VTOPOFF(&packet);
#else	/* USE_XREAD */
	v86.ctl = V86_ADDR | V86_CALLF | V86_FLAGS;
	v86.addr = XREADORG;		/* call to xread in boot1 */
	v86.es = VTOPSEG(buf);
	v86.eax = lba;
	v86.ebx = VTOPOFF(buf);
	v86.ecx = lba >> 32;
	v86.edx = nblk << 8 | dskp->drive;
#endif	/* USE_XREAD */
	v86int();
	if (V86_CY(v86.efl)) {
		printf("%s: error %u lba %u\n",
		    BOOTPROG, v86.eax >> 8 & 0xff, lba);
		return (-1);
	}
	return (0);
}

#ifdef GPT
int
drvwrite(struct dsk *dskp, void *buf, daddr_t lba, unsigned nblk)
{

	packet.len = 0x10;
	packet.count = nblk;
	packet.off = VTOPOFF(buf);
	packet.seg = VTOPSEG(buf);
	packet.lba = lba;
	v86.ctl = V86_FLAGS;
	v86.addr = 0x13;
	v86.eax = 0x4300;
	v86.edx = dskp->drive;
	v86.ds = VTOPSEG(&packet);
	v86.esi = VTOPOFF(&packet);
	v86int();
	if (V86_CY(v86.efl)) {
		printf("error %u lba %u\n", v86.eax >> 8 & 0xff, lba);
		return (-1);
	}
	return (0);
}
#endif	/* GPT */
