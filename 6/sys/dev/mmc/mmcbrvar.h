/*-
 * Copyright (c) 2006 Bernd Walter.  All rights reserved.
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * "$FreeBSD$"
 */

#ifndef DEV_MMC_MMCBRVAR_H
#define DEV_MMC_MMCBRVAR_H

#include <dev/mmc/bridge.h>
#include "mmcbr_if.h"

enum mmcbr_device_ivars {
    MMCBR_IVAR_BUS_MODE,
    MMCBR_IVAR_BUS_WIDTH,
    MMCBR_IVAR_CHIP_SELECT,
    MMCBR_IVAR_CLOCK,
    MMCBR_IVAR_F_MIN,
    MMCBR_IVAR_F_MAX,
    MMCBR_IVAR_HOST_OCR,
    MMCBR_IVAR_MODE,
    MMCBR_IVAR_OCR,
    MMCBR_IVAR_POWER_MODE,
    MMCBR_IVAR_VDD,
//    MMCBR_IVAR_,
};

/*
 * Simplified accessors for pci devices
 */
#define MMCBR_ACCESSOR(var, ivar, type)					\
	__BUS_ACCESSOR(mmcbr, var, MMCBR, ivar, type)

MMCBR_ACCESSOR(bus_mode, BUS_MODE, int)
MMCBR_ACCESSOR(bus_width, BUS_WIDTH, int)
MMCBR_ACCESSOR(chip_select, CHIP_SELECT, int)
MMCBR_ACCESSOR(clock, CLOCK, int)
MMCBR_ACCESSOR(f_max, F_MAX, int)
MMCBR_ACCESSOR(f_min, F_MIN, int)
MMCBR_ACCESSOR(host_ocr, HOST_OCR, int)
MMCBR_ACCESSOR(mode, MODE, int)
MMCBR_ACCESSOR(ocr, OCR, int)
MMCBR_ACCESSOR(power_mode, POWER_MODE, int)
MMCBR_ACCESSOR(vdd, VDD, int)

static int __inline
mmcbr_update_ios(device_t dev)
{
	return (MMCBR_UPDATE_IOS(device_get_parent(dev), dev));
}

#endif /* DEV_MMC_MMCBRVAR_H */
