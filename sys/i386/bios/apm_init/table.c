/*
 * APM (Advanced Power Management) BIOS Device Driver
 *
 * Copyright (c) 1994-1995 by HOSOKAWA, Tatsumi <hosokawa@jp.FreeBSD.org>
 *
 * This software may be used, modified, copied, and distributed, in
 * both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its
 * use.
 *
 * Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 *
 *	$FreeBSD$
 */

#include <apm_bios.h>

struct pseudo_desc {
	unsigned short	limit;
	unsigned long	base __attribute__ ((packed));
};

struct pseudo_desc Idtr_prot = { 0, 0 }; /* filled on run time */
struct pseudo_desc Idtr_real = { 0x400 - 1, 0x0 };
