/*
 *  rtc.c, RTC(has only timer function) routines for NEC VR4100 series.
 *
 *  Copyright (C) 2003  Yoichi Yuasa <yuasa@hh.iij4u.or.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/types.h>

#include <asm/io.h>
#include <asm/time.h>
#include <asm/vr41xx/vr41xx.h>

static uint32_t rtc1_base;
static uint32_t rtc2_base;

static uint64_t previous_elapsedtime;
static unsigned int remainder_per_sec;
static unsigned int cycles_per_sec;
static unsigned int cycles_per_jiffy;
static unsigned long epoch_time;

#define CLOCK_TICK_RATE		32768	/* 32.768kHz */

#define CYCLES_PER_JIFFY	(CLOCK_TICK_RATE / HZ)
#define REMAINDER_PER_SEC	(CLOCK_TICK_RATE - (CYCLES_PER_JIFFY * HZ))
#define CYCLES_PER_100USEC	((CLOCK_TICK_RATE + (10000 / 2)) / 10000)

#define ETIMELREG_TYPE1		KSEG1ADDR(0x0b0000c0)
#define TCLKLREG_TYPE1		KSEG1ADDR(0x0b0001c0)

#define ETIMELREG_TYPE2		KSEG1ADDR(0x0f000100)
#define TCLKLREG_TYPE2		KSEG1ADDR(0x0f000120)

/* RTC 1 registers */
#define ETIMELREG		0x00
#define ETIMEMREG		0x02
#define ETIMEHREG		0x04
/* RFU */
#define ECMPLREG		0x08
#define ECMPMREG		0x0a
#define ECMPHREG		0x0c
/* RFU */
#define RTCL1LREG		0x10
#define RTCL1HREG		0x12
#define RTCL1CNTLREG		0x14
#define RTCL1CNTHREG		0x16
#define RTCL2LREG		0x18
#define RTCL2HREG		0x1a
#define RTCL2CNTLREG		0x1c
#define RTCL2CNTHREG		0x1e

/* RTC 2 registers */
#define TCLKLREG		0x00
#define TCLKHREG		0x02
#define TCLKCNTLREG		0x04
#define TCLKCNTHREG		0x06
/* RFU */
#define RTCINTREG		0x1e
 #define TCLOCK_INT		0x08
 #define RTCLONG2_INT		0x04
 #define RTCLONG1_INT		0x02
 #define ELAPSEDTIME_INT	0x01

#define read_rtc1(offset)	readw(rtc1_base + (offset))
#define write_rtc1(val, offset)	writew((val), rtc1_base + (offset))

#define read_rtc2(offset)	readw(rtc2_base + (offset))
#define write_rtc2(val, offset)	writew((val), rtc2_base + (offset))

static inline uint64_t read_elapsedtime_counter(void)
{
	uint64_t first, second;
	uint32_t first_mid, first_low;
	uint32_t second_mid, second_low;

	do {
		first_low = (uint32_t)read_rtc1(ETIMELREG);
		first_mid = (uint32_t)read_rtc1(ETIMEMREG);
		first = (uint64_t)read_rtc1(ETIMEHREG);
		second_low = (uint32_t)read_rtc1(ETIMELREG);
		second_mid = (uint32_t)read_rtc1(ETIMEMREG);
		second = (uint64_t)read_rtc1(ETIMEHREG);
	} while (first_low != second_low || first_mid != second_mid ||
	         first != second);

	return (first << 32) | (uint64_t)((first_mid << 16) | first_low);
}

static inline void write_elapsedtime_counter(uint64_t time)
{
	write_rtc1((uint16_t)time, ETIMELREG);
	write_rtc1((uint16_t)(time >> 16), ETIMEMREG);
	write_rtc1((uint16_t)(time >> 32), ETIMEHREG);
}

static inline void write_elapsedtime_compare(uint64_t time)
{
	write_rtc1((uint16_t)time, ECMPLREG);
	write_rtc1((uint16_t)(time >> 16), ECMPMREG);
	write_rtc1((uint16_t)(time >> 32), ECMPHREG);
}

void vr41xx_set_rtclong1_cycle(uint32_t cycles)
{
	write_rtc1((uint16_t)cycles, RTCL1LREG);
	write_rtc1((uint16_t)(cycles >> 16), RTCL1HREG);
}

uint32_t vr41xx_read_rtclong1_counter(void)
{
	uint32_t first_high, first_low;
	uint32_t second_high, second_low;

	do {
		first_low = (uint32_t)read_rtc1(RTCL1CNTLREG);
		first_high = (uint32_t)read_rtc1(RTCL1CNTHREG);
		second_low = (uint32_t)read_rtc1(RTCL1CNTLREG);
		second_high = (uint32_t)read_rtc1(RTCL1CNTHREG);
	} while (first_low != second_low || first_high != second_high);

	return (first_high << 16) | first_low;
}

void vr41xx_set_rtclong2_cycle(uint32_t cycles)
{
	write_rtc1((uint16_t)cycles, RTCL2LREG);
	write_rtc1((uint16_t)(cycles >> 16), RTCL2HREG);
}

uint32_t vr41xx_read_rtclong2_counter(void)
{
	uint32_t first_high, first_low;
	uint32_t second_high, second_low;

	do {
		first_low = (uint32_t)read_rtc1(RTCL2CNTLREG);
		first_high = (uint32_t)read_rtc1(RTCL2CNTHREG);
		second_low = (uint32_t)read_rtc1(RTCL2CNTLREG);
		second_high = (uint32_t)read_rtc1(RTCL2CNTHREG);
	} while (first_low != second_low || first_high != second_high);

	return (first_high << 16) | first_low;
}

void vr41xx_set_tclock_cycle(uint32_t cycles)
{
	write_rtc2((uint16_t)cycles, TCLKLREG);
	write_rtc2((uint16_t)(cycles >> 16), TCLKHREG);
}

uint32_t vr41xx_read_tclock_counter(void)
{
	uint32_t first_high, first_low;
	uint32_t second_high, second_low;

	do {
		first_low = (uint32_t)read_rtc2(TCLKCNTLREG);
		first_high = (uint32_t)read_rtc2(TCLKCNTHREG);
		second_low = (uint32_t)read_rtc2(TCLKCNTLREG);
		second_high = (uint32_t)read_rtc2(TCLKCNTHREG);
	} while (first_low != second_low || first_high != second_high);

	return (first_high << 16) | first_low;
}

static void vr41xx_timer_ack(void)
{
	uint64_t cur;

	write_rtc2(ELAPSEDTIME_INT, RTCINTREG);

	previous_elapsedtime += (uint64_t)cycles_per_jiffy;
	cycles_per_sec += cycles_per_jiffy;

	if (cycles_per_sec >= CLOCK_TICK_RATE) {
		cycles_per_sec = 0;
		remainder_per_sec = REMAINDER_PER_SEC;
	}

	cycles_per_jiffy = 0;

	do {
		cycles_per_jiffy += CYCLES_PER_JIFFY;
		if (remainder_per_sec > 0) {
			cycles_per_jiffy++;
			remainder_per_sec--;
		}

		cur = read_elapsedtime_counter();
	} while (cur >= previous_elapsedtime + (uint64_t)cycles_per_jiffy);

	write_elapsedtime_compare(previous_elapsedtime + (uint64_t)cycles_per_jiffy);
}

static void vr41xx_hpt_init(unsigned int count)
{
}

static unsigned int vr41xx_hpt_read(void)
{
	uint64_t cur;

	cur = read_elapsedtime_counter();

	return (unsigned int)cur;
}

static unsigned long vr41xx_gettimeoffset(void)
{
	uint64_t cur;
	unsigned long gap;

	cur = read_elapsedtime_counter();
	gap = (unsigned long)(cur - previous_elapsedtime);
	gap = gap / CYCLES_PER_100USEC * 100;	/* usec */

	return gap;
}

static unsigned long vr41xx_get_time(void)
{
	uint64_t counts;

	counts = read_elapsedtime_counter();
	counts >>= 15;

	return epoch_time + (unsigned long)counts;

}

static int vr41xx_set_time(unsigned long sec)
{
	if (sec < epoch_time)
		return -EINVAL;

	sec -= epoch_time;

	write_elapsedtime_counter((uint64_t)sec << 15);

	return 0;
}

void vr41xx_set_epoch_time(unsigned long time)
{
	epoch_time = time;
}

void __init vr41xx_time_init(void)
{
	switch (current_cpu_data.cputype) {
	case CPU_VR4111:
	case CPU_VR4121:
		rtc1_base = ETIMELREG_TYPE1;
		rtc2_base = TCLKLREG_TYPE1;
		break;
	case CPU_VR4122:
	case CPU_VR4131:
	case CPU_VR4133:
		rtc1_base = ETIMELREG_TYPE2;
		rtc2_base = TCLKLREG_TYPE2;
		break;
	default:
		panic("Unexpected CPU of NEC VR4100 series");
		break;
	}

	mips_timer_ack = vr41xx_timer_ack;

	mips_hpt_init = vr41xx_hpt_init;
	mips_hpt_read = vr41xx_hpt_read;
	mips_hpt_frequency = CLOCK_TICK_RATE;

	if (epoch_time == 0)
		epoch_time = mktime(1970, 1, 1, 0, 0, 0);

	rtc_get_time = vr41xx_get_time;
	rtc_set_time = vr41xx_set_time;
}

void __init vr41xx_timer_setup(struct irqaction *irq)
{
	do_gettimeoffset = vr41xx_gettimeoffset;

	remainder_per_sec = REMAINDER_PER_SEC;
	cycles_per_jiffy = CYCLES_PER_JIFFY;

	if (remainder_per_sec > 0) {
		cycles_per_jiffy++;
		remainder_per_sec--;
	}

	previous_elapsedtime = read_elapsedtime_counter();
	write_elapsedtime_compare(previous_elapsedtime + (uint64_t)cycles_per_jiffy);
	write_rtc2(ELAPSEDTIME_INT, RTCINTREG);

	setup_irq(ELAPSEDTIME_IRQ, irq);
}
