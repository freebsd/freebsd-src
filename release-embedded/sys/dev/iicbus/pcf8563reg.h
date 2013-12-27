/*	$NetBSD: pcf8563reg.h,v 1.1 2011/01/21 19:11:47 jakllsch Exp $	*/

/*-
 * Jonathan Kollasch, 2011
 *
 * This file is in the public domain.
 *
 * $FreeBSD$
 */

/*
 * NXP (Philips) PCF8563 RTC registers
 */

/* We only have clock mode registers here. */

#ifndef _PCF8563REG_H_
#define _PCF8563REG_H_

/*
 * PCF8563 RTC I2C address:
 *
 *	101 0001
 */
#define PCF8563_ADDR		0xa2

#define PCF8563_R_CS1		0x00
#define PCF8563_R_CS2		0x01
#define PCF8563_R_SECOND	0x02
#define PCF8563_R_MINUTE	0x03
#define PCF8563_R_HOUR		0x04
#define PCF8563_R_DAY		0x05
#define PCF8563_R_WEEKDAY	0x06
#define PCF8563_R_MONTH		0x07
#define PCF8563_R_YEAR		0x08
#define PCF8563_R_MINUTE_ALARM	0x09
#define PCF8563_R_HOUR_ALARM	0x0a
#define PCF8563_R_DAY_ALARM	0x0b
#define PCF8563_R_WEEKDAY_ALARM	0x0c
#define PCF8563_R_CLKOUT_CNTRL	0x0d
#define PCF8563_R_TIMER_CNTRL	0x0e
#define PCF8563_R_TIMER		0x0f

#define PCF8563_R_SECOND_VL	0x80
#define	PCF8563_R_MONTH_C	0x80

#define PCF8563_NREGS		0x10

#define PCF8563_M_SECOND	0x7f
#define PCF8563_M_MINUTE	0x7f
#define PCF8563_M_HOUR		0x3f
#define PCF8563_M_DAY		0x3f
#define PCF8563_M_WEEKDAY	0x07
#define PCF8563_M_MONTH		0x1f
#define PCF8563_M_CENTURY	0x80
#define PCF8563_M_YEAR		0xff

#endif	/* _PCF8563REG_H_ */
