/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * RTC routines for Atlas style attached Dallas chip.
 *
 */
#include <linux/mc146818rtc.h>
#include <asm/mips-boards/atlas.h>


static unsigned char atlas_rtc_read_data(unsigned long addr)
{
	volatile unsigned int *rtc_adr_reg = (void *)ATLAS_RTC_ADR_REG;
	volatile unsigned int *rtc_dat_reg = (void *)ATLAS_RTC_DAT_REG;

	*rtc_adr_reg = addr;

	return *rtc_dat_reg;
}

static void atlas_rtc_write_data(unsigned char data, unsigned long addr)
{
	volatile unsigned int *rtc_adr_reg = (void *)ATLAS_RTC_ADR_REG;
	volatile unsigned int *rtc_dat_reg = (void *)ATLAS_RTC_DAT_REG;

	*rtc_adr_reg = addr;
	*rtc_dat_reg = data;
}

static int atlas_rtc_bcd_mode(void)
{
	return 0;
}

struct rtc_ops atlas_rtc_ops = {
	&atlas_rtc_read_data,
	&atlas_rtc_write_data,
	&atlas_rtc_bcd_mode
};
