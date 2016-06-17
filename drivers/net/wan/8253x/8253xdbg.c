/* -*- linux-c -*- */
/* 
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 **/

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/stddef.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <asm/pgtable.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include "8253xctl.h"
#include "Reg9050.h"
#if 0 				/* only during debugging */
#undef DEBUGPRINT
#define DEBUGPRINT(arg) printk arg
#endif

void dump_ati_adapter_registers(unsigned int *addr, int len)
{
	int index;
	int flag = 1;
	
	for(index = 0; index < (len/(sizeof(unsigned int*))); ++index)
	{
		if(flag)
		{
			DEBUGPRINT((KERN_ALERT "bridge: %4.4x:%8.8x", (4*index), *addr++));
		}
		else
		{
			DEBUGPRINT(("%8.8x", *addr++));
		}
		if(((index + 1) % 8) == 0)
		{
			DEBUGPRINT(("\n"));
			flag = 1;
		}
		else
		{
			DEBUGPRINT((" "));
			flag = 0;
		}
	}
	if(flag == 0)
	{
		DEBUGPRINT(("\n"));
	}
}
