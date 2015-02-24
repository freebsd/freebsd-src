/*-
 * Copyright (c) 2006,2007
 *	Damien Bergamini <damien.bergamini@free.fr>
 *	Benjamin Close <Benjamin.Close@clearchain.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */

#ifndef __IF_WPI_DEBUG_H__
#define __IF_WPI_DEBUG_H__

#ifdef WPI_DEBUG
enum {
	WPI_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	WPI_DEBUG_RECV		= 0x00000002,	/* basic recv operation */
	WPI_DEBUG_STATE		= 0x00000004,	/* 802.11 state transitions */
	WPI_DEBUG_HW		= 0x00000008,	/* Stage 1 (eeprom) debugging */
	WPI_DEBUG_RESET		= 0x00000010,	/* reset processing */
	WPI_DEBUG_FIRMWARE	= 0x00000020,	/* firmware(9) loading debug */
	WPI_DEBUG_BEACON	= 0x00000040,	/* beacon handling */
	WPI_DEBUG_WATCHDOG	= 0x00000080,	/* watchdog timeout */
	WPI_DEBUG_INTR		= 0x00000100,	/* ISR */
	WPI_DEBUG_SCAN		= 0x00000200,	/* Scan related operations */
	WPI_DEBUG_NOTIFY	= 0x00000400,	/* State 2 Notif intr debug */
	WPI_DEBUG_TEMP		= 0x00000800,	/* TXPower/Temp Calibration */
	WPI_DEBUG_CMD		= 0x00001000,	/* cmd submission */
	WPI_DEBUG_TRACE		= 0x00002000,	/* Print begin and start driver function */
	WPI_DEBUG_PWRSAVE	= 0x00004000,	/* Power save operations */
	WPI_DEBUG_EEPROM	= 0x00008000,	/* EEPROM info */
	WPI_DEBUG_KEY		= 0x00010000,	/* node key management */
	WPI_DEBUG_EDCA		= 0x00020000,	/* WME info */
	WPI_DEBUG_ANY		= 0xffffffff
};

#define DPRINTF(sc, m, ...) do {		\
	if (sc->sc_debug & (m))			\
		printf(__VA_ARGS__);		\
} while (0)

#define TRACE_STR_BEGIN		"->%s: begin\n"
#define TRACE_STR_DOING		"->Doing %s\n"
#define TRACE_STR_END		"->%s: end\n"
#define TRACE_STR_END_ERR	"->%s: end in error\n"

static const char *wpi_cmd_str(int cmd)
{
	switch (cmd) {
	/* Notifications */
	case WPI_UC_READY:		return "UC_READY";
	case WPI_RX_DONE:		return "RX_DONE";
	case WPI_START_SCAN:		return "START_SCAN";
	case WPI_SCAN_RESULTS:		return "SCAN_RESULTS";
	case WPI_STOP_SCAN:		return "STOP_SCAN";
	case WPI_BEACON_SENT:		return "BEACON_SENT";
	case WPI_RX_STATISTICS:		return "RX_STATS";
	case WPI_BEACON_STATISTICS:	return "BEACON_STATS";
	case WPI_STATE_CHANGED:		return "STATE_CHANGED";
	case WPI_BEACON_MISSED:		return "BEACON_MISSED";

	/* Command notifications */
	case WPI_CMD_RXON:		return "WPI_CMD_RXON";
	case WPI_CMD_RXON_ASSOC:	return "WPI_CMD_RXON_ASSOC";
	case WPI_CMD_EDCA_PARAMS:	return "WPI_CMD_EDCA_PARAMS";
	case WPI_CMD_TIMING:		return "WPI_CMD_TIMING";
	case WPI_CMD_ADD_NODE:		return "WPI_CMD_ADD_NODE";
	case WPI_CMD_DEL_NODE:		return "WPI_CMD_DEL_NODE";
	case WPI_CMD_TX_DATA:		return "WPI_CMD_TX_DATA";
	case WPI_CMD_MRR_SETUP:		return "WPI_CMD_MRR_SETUP";
	case WPI_CMD_SET_LED:		return "WPI_CMD_SET_LED";
	case WPI_CMD_SET_POWER_MODE:	return "WPI_CMD_SET_POWER_MODE";
	case WPI_CMD_SCAN:		return "WPI_CMD_SCAN";
	case WPI_CMD_SET_BEACON:	return "WPI_CMD_SET_BEACON";
	case WPI_CMD_TXPOWER:		return "WPI_CMD_TXPOWER";
	case WPI_CMD_BT_COEX:		return "WPI_CMD_BT_COEX";

	default:
		KASSERT(1, ("Unknown Command: %d\n", cmd));
		return "UNKNOWN CMD";
	}
}

#else
#define DPRINTF(sc, m, ...)	do { (void) sc; } while (0)
#endif

#endif	/* __IF_WPI_DEBUG_H__ */
