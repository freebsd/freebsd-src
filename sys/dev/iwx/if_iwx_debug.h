/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Adrian Chadd <adrian@FreeBSD.org>
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Tom Jones <thj@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#ifndef	__IF_IWX_DEBUG_H__
#define	__IF_IWX_DEBUG_H__

#ifdef	IWX_DEBUG
enum {
	IWX_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	IWX_DEBUG_RECV		= 0x00000002,	/* basic recv operation */
	IWX_DEBUG_STATE		= 0x00000004,	/* 802.11 state transitions */
	IWX_DEBUG_TXPOW		= 0x00000008,	/* tx power processing */
	IWX_DEBUG_RESET		= 0x00000010,	/* reset processing */
	IWX_DEBUG_OPS		= 0x00000020,	/* iwx_ops processing */
	IWX_DEBUG_BEACON 	= 0x00000040,	/* beacon handling */
	IWX_DEBUG_WATCHDOG 	= 0x00000080,	/* watchdog timeout */
	IWX_DEBUG_INTR		= 0x00000100,	/* ISR */
	IWX_DEBUG_CALIBRATE	= 0x00000200,	/* periodic calibration */
	IWX_DEBUG_NODE		= 0x00000400,	/* node management */
	IWX_DEBUG_LED		= 0x00000800,	/* led management */
	IWX_DEBUG_CMD		= 0x00001000,	/* cmd submission */
	IWX_DEBUG_TXRATE	= 0x00002000,	/* TX rate debugging */
	IWX_DEBUG_PWRSAVE	= 0x00004000,	/* Power save operations */
	IWX_DEBUG_SCAN		= 0x00008000,	/* Scan related operations */
	IWX_DEBUG_STATS		= 0x00010000,	/* Statistics updates */
	IWX_DEBUG_FIRMWARE_TLV	= 0x00020000,	/* Firmware TLV parsing */
	IWX_DEBUG_TRANS		= 0x00040000,	/* Transport layer (eg PCIe) */
	IWX_DEBUG_EEPROM	= 0x00080000,	/* EEPROM/channel information */
	IWX_DEBUG_TEMP		= 0x00100000,	/* Thermal Sensor handling */
	IWX_DEBUG_FW		= 0x00200000,	/* Firmware management */
	IWX_DEBUG_LAR		= 0x00400000,	/* Location Aware Regulatory */
	IWX_DEBUG_TE		= 0x00800000,	/* Time Event handling */
						/* 0x0n000000 are available */
	IWX_DEBUG_NI		= 0x10000000,	/* Not Implemented  */
	IWX_DEBUG_REGISTER	= 0x20000000,	/* print chipset register */
	IWX_DEBUG_TRACE		= 0x40000000,	/* Print begin and start driver function */
	IWX_DEBUG_FATAL		= 0x80000000,	/* fatal errors */
	IWX_DEBUG_ANY		= 0xffffffff
};

#define IWX_DPRINTF(sc, m, fmt, ...) do {			\
	if (sc->sc_debug & (m))				\
		device_printf(sc->sc_dev, fmt, ##__VA_ARGS__);	\
} while (0)
#else
#define IWX_DPRINTF(sc, m, fmt, ...) do { (void) sc; } while (0)
#endif

void print_opcode(const char *, int, uint32_t);
void print_ratenflags(const char *, int , uint32_t , int );

#endif	/* __IF_IWX_DEBUG_H__ */
