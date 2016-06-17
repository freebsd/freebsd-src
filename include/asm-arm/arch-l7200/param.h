/*
 * linux/include/asm-arm/arch-l7200/param.h
 *
 * Copyright (C) 2000 Rob Scott (rscott@mtrob.fdns.net)
 *                    Steve Hill (sjhill@cotw.com)
 *
 * This file contains the hardware definitions for the
 * LinkUp Systems L7200 SOC development board.
 *
 * Changelog:
 *   04-21-2000 RS      Created L7200 version
 *   04-25-2000 SJH     Cleaned up file
 *   05-03-2000 SJH     Change comments and rate
 */
#ifndef __ASM_ARCH_PARAM_H
#define __ASM_ARCH_PARAM_H

/*
 * See 'time.h' for how the RTC HZ rate is set
 */
#define HZ 128

/*
 * Define hz_to_std, since we have a non 100Hz define
 * (see include/asm-arm/param.h)
 */

#if defined(__KERNEL__)
#define hz_to_std(a) ((a * HZ)/100)
#endif

#endif
