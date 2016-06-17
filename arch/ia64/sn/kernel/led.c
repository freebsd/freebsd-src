/*
 *
 *
 * Copyright (c) 2003 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * Further, this software is distributed without any warranty that it is 
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#include <linux/timer.h>
#include <asm/sn/leds.h>
#include <asm/sn/pda.h>

struct timer_list sn_led_timer;

static void 
sn_led_timeout (unsigned long arg)
{
	set_led_bits(pda.hb_state ^= LED_CPU_HEARTBEAT, LED_CPU_HEARTBEAT);
	sn_led_timer.expires = jiffies + HZ/2;
	add_timer(&sn_led_timer);
}

void __init
sn_led_timer_init (void)
{
	init_timer(&sn_led_timer);
	sn_led_timer.expires = jiffies + HZ/2;
	sn_led_timer.function = &sn_led_timeout;
	add_timer(&sn_led_timer);
}
