/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include <machine/vmm.h>
#include <vmmapi.h>

#include "inout.h"
#include "rtc.h"

#define	IO_RTC	0x70

#define RTC_SEC		0x00	/* seconds */
#define	RTC_SEC_ALARM	0x01
#define	RTC_MIN		0x02
#define	RTC_MIN_ALARM	0x03
#define	RTC_HRS		0x04
#define	RTC_HRS_ALARM	0x05
#define	RTC_WDAY	0x06
#define	RTC_DAY		0x07
#define	RTC_MONTH	0x08
#define	RTC_YEAR	0x09
#define	RTC_CENTURY	0x32	/* current century */

#define RTC_STATUSA	0xA
#define  RTCSA_TUP	 0x80	/* time update, don't look now */

#define	RTC_STATUSB	0xB
#define	 RTCSB_DST	 0x01
#define	 RTCSB_24HR	 0x02
#define	 RTCSB_BIN	 0x04	/* 0 = BCD, 1 = Binary */
#define	 RTCSB_PINTR	 0x40	/* 1 = enable periodic clock interrupt */
#define	 RTCSB_HALT      0x80	/* stop clock updates */

#define RTC_INTR	0x0c	/* status register C (R) interrupt source */

#define RTC_STATUSD	0x0d	/* status register D (R) Lost Power */
#define  RTCSD_PWR	 0x80	/* clock power OK */

#define	RTC_NVRAM_START	0x0e
#define	RTC_NVRAM_END	0x7f
#define RTC_NVRAM_SZ	(128 - RTC_NVRAM_START)
#define	nvoff(x)	((x) - RTC_NVRAM_START)

#define	RTC_DIAG	0x0e
#define RTC_RSTCODE	0x0f
#define	RTC_EQUIPMENT	0x14
#define	RTC_LMEM_LSB	0x34
#define	RTC_LMEM_MSB	0x35
#define	RTC_HMEM_LSB	0x5b
#define	RTC_HMEM_SB	0x5c
#define	RTC_HMEM_MSB	0x5d

#define m_64KB		(64*1024)
#define	m_16MB		(16*1024*1024)
#define	m_4GB		(4ULL*1024*1024*1024)

static int addr;

static uint8_t rtc_nvram[RTC_NVRAM_SZ];

/* XXX initialize these to default values as they would be from BIOS */
static uint8_t status_a, status_b;

static struct {
	uint8_t  hours;
	uint8_t  mins;
	uint8_t  secs;
} rtc_alarm;

static u_char const bin2bcd_data[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99
};
#define	bin2bcd(bin)	(bin2bcd_data[bin])

#define	rtcout(val)	((status_b & RTCSB_BIN) ? (val) : bin2bcd((val)))

static void
timevalfix(struct timeval *t1)
{

	if (t1->tv_usec < 0) {
		t1->tv_sec--;
		t1->tv_usec += 1000000;
	}
	if (t1->tv_usec >= 1000000) {
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
}

static void
timevalsub(struct timeval *t1, const struct timeval *t2)
{

	t1->tv_sec -= t2->tv_sec;
	t1->tv_usec -= t2->tv_usec;
	timevalfix(t1);
}

static int
rtc_addr_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		 uint32_t *eax, void *arg)
{
	if (bytes != 1)
		return (-1);

	if (in) {
		/* straight read of this register will return 0xFF */
		*eax = 0xff;
		return (0);
	}

	switch (*eax & 0x7f) {
	case RTC_SEC:
	case RTC_SEC_ALARM:
	case RTC_MIN:
	case RTC_MIN_ALARM:
	case RTC_HRS:
	case RTC_HRS_ALARM:
	case RTC_WDAY:
	case RTC_DAY:
	case RTC_MONTH:
	case RTC_YEAR:
	case RTC_STATUSA:
	case RTC_STATUSB:
	case RTC_INTR:
	case RTC_STATUSD:
	case RTC_NVRAM_START ... RTC_NVRAM_END:
		break;
	default:
		return (-1);
	}

	addr = *eax & 0x7f;
	return (0);
}

static int
rtc_data_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		 uint32_t *eax, void *arg)
{
	int hour;
	time_t t;
	struct timeval cur, delta;

	static struct timeval last;
	static struct tm tm;

	if (bytes != 1)
		return (-1);

	gettimeofday(&cur, NULL);

	/*
	 * Increment the cached time only once per second so we can guarantee
	 * that the guest has at least one second to read the hour:min:sec
	 * separately and still get a coherent view of the time.
	 */
	delta = cur;
	timevalsub(&delta, &last);
	if (delta.tv_sec >= 1 && (status_b & RTCSB_HALT) == 0) {
		t = cur.tv_sec;
		localtime_r(&t, &tm);
		last = cur;
	}

	if (in) {
		switch (addr) {
		case RTC_SEC_ALARM:
			*eax = rtc_alarm.secs;
			break;
		case RTC_MIN_ALARM:
			*eax = rtc_alarm.mins;
			break;
		case RTC_HRS_ALARM:
			*eax = rtc_alarm.hours;
			break;
		case RTC_SEC:
			*eax = rtcout(tm.tm_sec);
			return (0);
		case RTC_MIN:
			*eax = rtcout(tm.tm_min);
			return (0);
		case RTC_HRS:
			if (status_b & RTCSB_24HR)
				hour = tm.tm_hour;
			else
				hour = (tm.tm_hour % 12) + 1;
			
			*eax = rtcout(hour);

			/*
			 * If we are representing time in the 12-hour format
			 * then set the MSB to indicate PM.
			 */
			if ((status_b & RTCSB_24HR) == 0 && tm.tm_hour >= 12)
				*eax |= 0x80;

			return (0);
		case RTC_WDAY:
			*eax = rtcout(tm.tm_wday + 1);
			return (0);
		case RTC_DAY:
			*eax = rtcout(tm.tm_mday);
			return (0);
		case RTC_MONTH:
			*eax = rtcout(tm.tm_mon + 1);
			return (0);
		case RTC_YEAR:
			*eax = rtcout(tm.tm_year % 100);
			return (0);
		case RTC_STATUSA:
			*eax = status_a;
			return (0);
		case RTC_STATUSB:
			*eax = status_b;
			return (0);
		case RTC_INTR:
			*eax = 0;
			return (0);
		case RTC_STATUSD:
			*eax = RTCSD_PWR;
			return (0);
		case RTC_NVRAM_START ... RTC_NVRAM_END:
			*eax = rtc_nvram[addr - RTC_NVRAM_START];
			return (0);
		default:
			return (-1);
		}
	}

	switch (addr) {
	case RTC_STATUSA:
		status_a = *eax & ~RTCSA_TUP;
		break;
	case RTC_STATUSB:
		/* XXX not implemented yet XXX */
		if (*eax & RTCSB_PINTR)
			return (-1);
		status_b = *eax;
		break;
	case RTC_STATUSD:
		/* ignore write */
		break;
	case RTC_SEC_ALARM:
		rtc_alarm.secs = *eax;
		break;
	case RTC_MIN_ALARM:
		rtc_alarm.mins = *eax;
		break;
	case RTC_HRS_ALARM:
		rtc_alarm.hours = *eax;
		break;
	case RTC_SEC:
	case RTC_MIN:
	case RTC_HRS:
	case RTC_WDAY:
	case RTC_DAY:
	case RTC_MONTH:
	case RTC_YEAR:
		/*
		 * Ignore writes to the time of day registers
		 */
		break;
	case RTC_NVRAM_START ... RTC_NVRAM_END:
		rtc_nvram[addr - RTC_NVRAM_START] = *eax;
		break;
	default:
		return (-1);
	}
	return (0);
}

void
rtc_init(struct vmctx *ctx)
{	
	struct timeval cur;
	struct tm tm;
	size_t himem;
	size_t lomem;
	int err;

	err = gettimeofday(&cur, NULL);
	assert(err == 0);
	(void) localtime_r(&cur.tv_sec, &tm);

	memset(rtc_nvram, 0, sizeof(rtc_nvram));

	rtc_nvram[nvoff(RTC_CENTURY)] = bin2bcd((tm.tm_year + 1900) / 100);

	/* XXX init diag/reset code/equipment/checksum ? */

	/*
	 * Report guest memory size in nvram cells as required by UEFI.
	 * Little-endian encoding.
	 * 0x34/0x35 - 64KB chunks above 16MB, below 4GB
	 * 0x5b/0x5c/0x5d - 64KB chunks above 4GB
	 */
	err = vm_get_memory_seg(ctx, 0, &lomem, NULL);
	assert(err == 0);

	lomem = (lomem - m_16MB) / m_64KB;
	rtc_nvram[nvoff(RTC_LMEM_LSB)] = lomem;
	rtc_nvram[nvoff(RTC_LMEM_MSB)] = lomem >> 8;

	if (vm_get_memory_seg(ctx, m_4GB, &himem, NULL) == 0) {	  
		himem /= m_64KB;
		rtc_nvram[nvoff(RTC_HMEM_LSB)] = himem;
		rtc_nvram[nvoff(RTC_HMEM_SB)]  = himem >> 8;
		rtc_nvram[nvoff(RTC_HMEM_MSB)] = himem >> 16;
	}
}

INOUT_PORT(rtc, IO_RTC, IOPORT_F_INOUT, rtc_addr_handler);
INOUT_PORT(rtc, IO_RTC + 1, IOPORT_F_INOUT, rtc_data_handler);
