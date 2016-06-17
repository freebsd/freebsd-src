/* -*- linux-c -*- */
/*
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 */
				/* This file is linked in, but I am
				   not sure there is ever any
				   reason directly to read the
				   serial eprom on the multichannel
				   server host card. */

/* We handle PCI devices */
#include <linux/pci.h>      

/* We need to use ioremap */ 
#include <asm/io.h>

#include <linux/delay.h>

#include "8253xmcs.h"
#include "8253xctl.h"

/* read a byte out of the serial eeprom/nvram by means of
 * 16 short commands */

static unsigned int amcc_nvram_breadw(unsigned char *bridge_space, 
				      unsigned short address,
				      unsigned char *value)
{
	unsigned int count;
	unsigned rhr;
	
	for(count = 0; count < 20000; ++count)
	{
		rhr = readl(bridge_space + AMCC_RCR);
		if((rhr & AMCC_NVRBUSY) == 0)
		{
			break;
		}
		udelay(1);
	}
	if(count >= 20000)
	{
		return FALSE;
	}
	rhr = AMCC_NVRWRLA | ((address & 0x00FF) << 16);
	writel(rhr, bridge_space + AMCC_RCR);
	rhr = AMCC_NVRWRHA | ((address & 0xFF00) << 8);
	writel(rhr, bridge_space + AMCC_RCR);
	writel(AMCC_NVRRDDB, bridge_space + AMCC_RCR);
	for(count = 0; count < 20000; ++count)
	{
		rhr = readl(bridge_space + AMCC_RCR);
		if((rhr & AMCC_NVRBUSY) == 0)
		{
			break;
		}
		udelay(1);
	}
	if(count >= 20000)
	{
		return FALSE;
	}
	if(rhr & AMCC_NVRACCFAIL)
	{
		return FALSE;
	}
	*value = (unsigned char) (rhr >> 16);
	return TRUE;
}

/* read the whole serial eeprom from the host card */

unsigned int amcc_read_nvram(unsigned char* buffer, unsigned length, unsigned char *bridge_space)
{
	unsigned int count;
	length <<= 1;			/* covert words to bytes */
	
	for(count = 0; count < length; ++count)
	{
		if(amcc_nvram_breadw(bridge_space, count, &buffer[count]) == FALSE)
		{
			return FALSE;
		}
	}
	return TRUE;
}
