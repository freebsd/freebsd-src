/*
 * Copyright (C) 1994 by HOSOKAWA, Tatsumi <hosokawa@jp.FreeBSD.org>
 *
 * This software may be used, modified, copied, distributed, and sold,
 * in both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its
 * use.
 *
 * Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 *
 *	$Id: apm_setup.h,v 1.4 1995/05/30 07:58:08 rgrimes Exp $
 */

extern u_long	apm_version;
extern u_long	apm_cs_entry;
extern u_short	apm_cs32_base;
extern u_short	apm_cs16_base;
extern u_short	apm_ds_base;
extern u_short	apm_cs_limit;
extern u_short	apm_ds_limit;
extern u_short	apm_flags;
extern u_short	kernelbase;
