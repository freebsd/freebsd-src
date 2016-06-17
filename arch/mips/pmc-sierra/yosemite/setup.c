/*
 *  arch/mips/pmc-sierra/yosemite/setup.c
 *
 *  Copyright (C) 2003 PMC-Sierra Inc.
 *  Author: Manish Lachwani (lachwani@pmc-sierra.com)
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mc146818rtc.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/timex.h>
#include <linux/vmalloc.h>
#include <asm/time.h>
#include <asm/bootinfo.h>
#include <asm/page.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/reboot.h>
#include <linux/bootmem.h>
#include <linux/blk.h>

#include "setup.h"

unsigned long cpu_clock;
unsigned long yosemite_base;

void __init bus_error_init(void) 
{ 
	/* Do nothing */ 
}

unsigned long m48t37y_get_time(void)
{
	unsigned char	*rtc_base = YOSEMITE_RTC_BASE;
	unsigned int	year, month, day, hour, min, sec;

	/* Stop the update to the time */
	rtc_base[0x7ff8] = 0x40;

	year = CONV_BCD_TO_BIN(rtc_base[0x7fff]);
	year += CONV_BCD_TO_BIN(rtc_base[0x7fff1]) * 100;

	month = CONV_BCD_TO_BIN(rtc_base[0x7ffe]);
	day = CONV_BCD_TO_BIN(rtc_base[0x7ffd]);
	hour = CONV_BCD_TO_BIN(rtc_base[0x7ffb]);
	min = CONV_BCD_TO_BIN(rtc_base[0x7ffa]);
	sec = CONV_BCD_TO_BIN(rtc_base[0x7ff9]);

	/* Start the update to the time again */
	rtc_base[0x7ff8] = 0x00;

	return mktime(year, month, day, hour, min, sec);
}

int m48t37y_set_time(unsigned long sec)
{
	unsigned char   *rtc_base = YOSEMITE_RTC_BASE;
        unsigned int    year, month, day, hour, min, sec;

        struct rtc_time tm;

        /* convert to a more useful format -- note months count from 0 */
        to_tm(sec, &tm);
        tm.tm_mon += 1;

        /* enable writing */
        rtc_base[0x7ff8] = 0x80;

        /* year */
        rtc_base[0x7fff] = CONV_BIN_TO_BCD(tm.tm_year % 100);
        rtc_base[0x7ff1] = CONV_BIN_TO_BCD(tm.tm_year / 100);

        /* month */
        rtc_base[0x7ffe] = CONV_BIN_TO_BCD(tm.tm_mon);

        /* day */
        rtc_base[0x7ffd] = CONV_BIN_TO_BCD(tm.tm_mday);

        /* hour/min/sec */
        rtc_base[0x7ffb] = CONV_BIN_TO_BCD(tm.tm_hour);
        rtc_base[0x7ffa] = CONV_BIN_TO_BCD(tm.tm_min);
        rtc_base[0x7ff9] = CONV_BIN_TO_BCD(tm.tm_sec);

        /* day of week -- not really used, but let's keep it up-to-date */
        rtc_base[0x7ffc] = CONV_BIN_TO_BCD(tm.tm_wday + 1);

        /* disable writing */
        rtc_base[0x7ff8] = 0x00;

        return 0;
}

void yosemite_timer_setup(struct irqaction *irq)
{
	setup_irq(6, irq);
}

void yosemite_time_init(void)
{
	mips_counter_frequency = cpu_clock / 2;
	board_timer_setup = yosemite_timer_setup;

	rtc_get_time = m48t37y_get_time;
	rtc_set_time = m48t37y_set_time;
}

void __init pmc_yosemite_setup(void)
{
	unsigned long	val = 0;

	printk("PMC-Sierra Yosemite Board Setup  \n");
	board_time_init = yosemite_time_init;

	/* Add memory regions */
	add_memory_region(0x00000000, 0x10000000, BOOT_MEM_RAM);
	add_memory_region(0x10000000, 0x10000000, BOOT_MEM_RAM);

	/* Setup the HT controller */
	val = *(volatile u_int32_t *)(HYPERTRANSPORT_CONFIG_REG);
	val |= HYPERTRANSPORT_ENABLE;
        *(volatile u_int32_t *)(HYPERTRANSPORT_CONFIG_REG) = val;

        /* Set the BAR. Shifted mode */
        *(volatile u_int32_t *)(HYPERTRANSPORT_BAR0_REG) = HYPERTRANSPORT_BAR0_ADDR;
        *(volatile u_int32_t *)(HYPERTRANSPORT_SIZE0_REG) = HYPERTRANSPORT_SIZE0;
}
	

