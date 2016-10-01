/*-
 * Copyright (c) 2016 Emmanuel Vadot <manu@freeebsd.org>
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
 * $FreeBSD$
 */

#ifndef _AXP209REG_H_
#define	_AXP209REG_H_

/* Power State Register */
#define	AXP209_PSR		0x00
#define	AXP209_PSR_ACIN		0x80
#define	AXP209_PSR_ACIN_SHIFT	7
#define	AXP209_PSR_VBUS		0x20
#define	AXP209_PSR_VBUS_SHIFT	5

/* Shutdown and battery control */
#define	AXP209_SHUTBAT		0x32
#define	AXP209_SHUTBAT_SHUTDOWN	0x80

/* Voltage/Current Monitor */
#define	AXP209_ACIN_VOLTAGE		0x56
#define	AXP209_ACIN_CURRENT		0x58
#define	AXP209_VBUS_VOLTAGE		0x5A
#define	AXP209_VBUS_CURRENT		0x5C
#define	AXP209_BAT_VOLTAGE		0x78
#define	AXP209_BAT_CHARGE_CURRENT	0x7A
#define	AXP209_BAT_DISCHARGE_CURRENT	0x7C

#define	AXP209_VOLT_STEP	1700
#define	AXP209_BATVOLT_STEP	1100
#define	AXP209_ACCURRENT_STEP	625
#define	AXP209_VBUSCURRENT_STEP	375
#define	AXP209_BATCURRENT_STEP	500

/* Temperature monitor */
#define	AXP209_TEMPMON		0x5e
#define	AXP209_TEMPMON_MIN	1447	/* -144.7C */

/* Sensors conversion macros */
#define	AXP209_SENSOR_H(a)	((a) << 4)
#define	AXP209_SENSOR_L(a)	((a) & 0xf)
#define	AXP209_SENSOR_BAT_H(a)	((a) << 5)
#define	AXP209_SENSOR_BAT_L(a)	((a) & 0x1f)

#define	AXP209_0C_TO_K		2732

/* ADC Sensors */
#define	AXP209_ADC_ENABLE1	0x82
#define	AXP209_ADC_ENABLE2	0x83

#define	AXP209_ADC1_BATVOLT	(1 << 7)
#define	AXP209_ADC1_BATCURRENT	(1 << 6)
#define	AXP209_ADC1_ACVOLT	(1 << 5)
#define	AXP209_ADC1_ACCURRENT	(1 << 4)
#define	AXP209_ADC1_VBUSVOLT	(1 << 3)
#define	AXP209_ADC1_VBUSCURRENT	(1 << 2)

/* Interrupt related registers */
#define	AXP209_IRQ1_ENABLE	0x40
#define	AXP209_IRQ1_STATUS	0x48
#define	 AXP209_IRQ1_AC_OVERVOLT	(1 << 7)
#define	 AXP209_IRQ1_AC_CONN		(1 << 6)
#define	 AXP209_IRQ1_AC_DISCONN		(1 << 5)
#define	 AXP209_IRQ1_VBUS_OVERVOLT	(1 << 4)
#define	 AXP209_IRQ1_VBUS_CONN		(1 << 3)
#define	 AXP209_IRQ1_VBUS_DISCONN	(1 << 2)
#define	 AXP209_IRQ1_VBUS_LOW		(1 << 1)

#define	AXP209_IRQ2_ENABLE	0x41
#define	AXP209_IRQ2_STATUS	0x49
#define	 AXP209_IRQ2_BATT_CONN			(1 << 7)
#define	 AXP209_IRQ2_BATT_DISCONN		(1 << 6)
#define	 AXP209_IRQ2_BATT_CHARGE_ACCT_ON	(1 << 5)
#define	 AXP209_IRQ2_BATT_CHARGE_ACCT_OFF	(1 << 4)
#define	 AXP209_IRQ2_BATT_CHARGING		(1 << 3)
#define	 AXP209_IRQ2_BATT_CHARGED		(1 << 2)
#define	 AXP209_IRQ2_BATT_TEMP_OVER		(1 << 1)
#define	 AXP209_IRQ2_BATT_TEMP_LOW		(1 << 0)

#define	AXP209_IRQ3_ENABLE	0x42
#define	AXP209_IRQ3_STATUS	0x4A
#define	 AXP209_IRQ3_TEMP_OVER		(1 << 7)
#define	 AXP209_IRQ3_CHARGE_CURRENT_LOW	(1 << 6)
#define	 AXP209_IRQ3_DCDC2_LOW		(1 << 4)
#define	 AXP209_IRQ3_DCDC3_LOW		(1 << 3)
#define	 AXP209_IRQ3_LDO3_LOW		(1 << 2)
#define	 AXP209_IRQ3_PEK_SHORT		(1 << 1)
#define	 AXP209_IRQ3_PEK_LONG		(1 << 0)

#define	AXP209_IRQ4_ENABLE	0x43
#define	AXP209_IRQ4_STATUS	0x4B
#define	 AXP209_IRQ4_NOE_START		(1 << 7)
#define	 AXP209_IRQ4_NOE_SHUT		(1 << 6)
#define	 AXP209_IRQ4_VBUS_VALID		(1 << 5)
#define	 AXP209_IRQ4_VBUS_INVALID	(1 << 4)
#define	 AXP209_IRQ4_VBUS_SESSION	(1 << 3)
#define	 AXP209_IRQ4_VBUS_SESSION_END	(1 << 2)
#define	 AXP209_IRQ4_APS_LOW_1		(1 << 1)
#define	 AXP209_IRQ4_APS_LOW_2		(1 << 0)

#define	AXP209_IRQ5_ENABLE	0x44
#define	AXP209_IRQ5_STATUS	0x4C
#define	 AXP209_IRQ5_TIMER_EXPIRE	(1 << 7)
#define	 AXP209_IRQ5_PEK_RISE_EDGE	(1 << 6)
#define	 AXP209_IRQ5_PEK_FALL_EDGE	(1 << 5)
#define	 AXP209_IRQ5_GPIO3	(1 << 3)
#define	 AXP209_IRQ5_GPIO2	(1 << 2)
#define	 AXP209_IRQ5_GPIO1	(1 << 1)
#define	 AXP209_IRQ5_GPIO0	(1 << 0)

#define	AXP209_IRQ_ACK		0xff

/* GPIOs registers */
#define	AXP209_GPIO_FUNC_MASK		0x7

#define	AXP209_GPIO_FUNC_DRVLO		0x0
#define	AXP209_GPIO_FUNC_DRVHI		0x1
#define	AXP209_GPIO_FUNC_INPUT		0x2

#define	AXP209_GPIO0_CTRL	0x90
#define	AXP209_GPIO1_CTRL	0x92
#define	AXP209_GPIO2_CTRL	0x93
#define	AXP209_GPIO_STATUS	0x94

#define	AXP209_GPIO_DATA(x)	(1 << (x + 4))

enum axp209_sensor {
	AXP209_TEMP,
	AXP209_ACVOLT,
	AXP209_ACCURRENT,
	AXP209_VBUSVOLT,
	AXP209_VBUSCURRENT,
	AXP209_BATVOLT,
	AXP209_BATCHARGECURRENT,
	AXP209_BATDISCHARGECURRENT,
};

#endif /* _AXP209REG_H_ */
