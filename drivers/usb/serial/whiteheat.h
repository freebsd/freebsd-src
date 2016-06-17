/*
 * USB ConnectTech WhiteHEAT driver
 *
 *      Copyright (C) 1999, 2000
 *          Greg Kroah-Hartman (greg@kroah.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 * See Documentation/usb/usb-serial.txt for more information on using this driver
 *
 */

#ifndef __LINUX_USB_SERIAL_WHITEHEAT_H
#define __LINUX_USB_SERIAL_WHITEHEAT_H


#define FALSE				0
#define TRUE				1

/* WhiteHEAT commands */
#define WHITEHEAT_OPEN			1	/* open the port */
#define WHITEHEAT_CLOSE			2	/* close the port */
#define WHITEHEAT_SETUP_PORT		3	/* change port settings */
#define WHITEHEAT_SET_RTS		4	/* turn RTS on or off */
#define WHITEHEAT_SET_DTR		5	/* turn DTR on or off */
#define WHITEHEAT_SET_BREAK		6	/* turn BREAK on or off */
#define WHITEHEAT_DUMP			7	/* dump memory */
#define WHITEHEAT_STATUS		8	/* get status */
#define WHITEHEAT_PURGE			9	/* clear the UART fifos */
#define WHITEHEAT_GET_DTR_RTS		10	/* get the state of DTR and RTS for a port */
#define WHITEHEAT_GET_HW_INFO		11	/* get EEPROM info and hardware ID */
#define WHITEHEAT_REPORT_TX_DONE	12	/* get the next TX done */
#define WHITEHEAT_EVENT			13	/* unsolicited status events */
#define WHITEHEAT_ECHO			14	/* send data to the indicated IN endpoint */
#define WHITEHEAT_DO_TEST		15	/* perform the specified test */
#define WHITEHEAT_CMD_COMPLETE		16	/* reply for certain commands */
#define WHITEHEAT_CMD_FAILURE		17	/* reply for failed commands */

/* Data for the WHITEHEAT_SETUP_PORT command */
#define WHITEHEAT_CTS_FLOW		0x08
#define WHITEHEAT_RTS_FLOW		0x80
#define WHITEHEAT_DSR_FLOW		0x10
#define WHITEHEAT_DTR_FLOW		0x02
struct whiteheat_port_settings {
	__u8	port;		/* port number (1 to N) */
	__u32	baud;		/* any value allowed, default 9600, arrives little endian, range is 7 - 460800 */
	__u8	bits;		/* 5, 6, 7, or 8, default 8 */
	__u8	stop;		/* 1 or 2, default 1 (2 = 1.5 if bits = 5) */
	__u8	parity;		/* 'n, e, o, 0, or 1' (ascii), default 'n'
				 *	n = none	e = even	o = odd
				 *	0 = force 0	1 = force 1	*/
	__u8	sflow;		/* 'n, r, t, or b' (ascii), default 'n'
				 *	n = none
				 *	r = receive (XOFF/XON transmitted when receiver fills / empties)
				 *	t = transmit (XOFF/XON received will stop/start TX)
				 *	b = both 	*/
	__u8	xoff;		/* XOFF byte value, default 0x13 */
	__u8	xon;		/* XON byte value, default 0x11 */
	__u8	hflow;		/* bits indicate mode as follows:
				 *	CTS (0x08) (CTS off/on will control/cause TX off/on)
				 *	DSR (0x10) (DSR off/on will control/cause TX off/on)
				 *	RTS (0x80) (RTS off/on when receiver fills/empties)
				 *	DTR (0x02) (DTR off/on when receiver fills/empties) */
	__u8	lloop;		/* local loopback 0 or 1, default 0 */
} __attribute__ ((packed));

/* data for WHITEHEAT_SET_RTS, WHITEHEAT_SET_DTR, and WHITEHEAT_SET_BREAK commands */
struct whiteheat_rdb_set {
	__u8	port;		/* port number (1 to N) */
	__u8	state;		/* 0 = off, non-zero = on */
};

/* data for:
	WHITEHEAT_OPEN
	WHITEHEAT_CLOSE
	WHITEHEAT_STATUS
	WHITEHEAT_GET_DTR_RTS
	WHITEHEAT_REPORT_TX_DONE */
struct whiteheat_min_set {
	__u8	port;		/* port number (1 to N) */
};

/* data for WHITEHEAT_PURGE command */
#define WHITEHEAT_PURGE_INPUT		0x01
#define WHITEHEAT_PURGE_OUTPUT		0x02
struct whiteheat_purge_set {
	__u8	port;		/* port number (1 to N) */
	__u8	what;		/* bit pattern of what to purge */
};

/* data for WHITEHEAT_DUMP command */
struct whiteheat_dump_info {
	__u8	mem_type;	/* memory type: 'd' = data, 'i' = idata, 'b' = bdata, 'x' = xdata */
	__u16	addr;		/* memory address to dump, address range depends on the above mem_type:
				 *	'd' = 0 to ff (80 to FF is SFR's)
				 *	'i' = 80 to ff
				 *	'b' = 20 to 2f (bits returned as bytes)
				 *	'x' = 0000 to ffff (also code space)	*/
	__u16	length;		/* number of bytes to dump, max 64 */
};

/* data for WHITEHEAT_ECHO command */
struct whiteheat_echo_set {
	__u8	port;		/* port number (1 to N) */
	__u8	length;		/* length of message to echo */
	__u8	echo_data[61];	/* data to echo */
};

/* data returned from WHITEHEAT_STATUS command */
#define WHITEHEAT_OVERRUN_ERROR		0x02
#define WHITEHEAT_PARITY_ERROR		0x04
#define WHITEHEAT_FRAMING_ERROR		0x08
#define WHITEHEAT_BREAK_ERROR		0x10

#define WHITEHEAT_OHFLOW		0x01	/* TX is stopped by CTS (waiting for CTS to go ON) */
#define WHITEHEAT_IHFLOW		0x02	/* remote TX is stopped by RTS */
#define WHITEHEAT_OSFLOW		0x04	/* TX is stopped by XOFF received (waiting for XON to occur) */
#define WHITEHEAT_ISFLOW		0x08	/* remote TX is stopped by XOFF transmitted */
#define WHITEHEAT_TX_DONE		0x80	/* TX has completed */

#define WHITEHEAT_MODEM_EVENT		0x01
#define WHITEHEAT_ERROR_EVENT		0x02
#define WHITEHEAT_FLOW_EVENT		0x04
#define WHITEHEAT_CONNECT_EVENT		0x08
struct whiteheat_status_info {
	__u8	port;		/* port number (1 to N) */
	__u8	event;		/* indicates which of the following bytes are the current event */
	__u8	modem;		/* modem signal status (copy of UART MSR register) */
	__u8	error;		/* PFO and RX break (copy of UART LSR register) */
	__u8	flow;		/* flow control state */
	__u8	connect;	/* connect state, non-zero value indicates connected */
};

/* data returned from WHITEHEAT_EVENT command */
struct whiteheat_event {
	__u8	port;		/* port number (1 to N) */
	__u8	event;		/* indicates which of the following bytes are the current event */
	__u8	info;		/* either modem, error, flow, or connect information */
};

/* data retured by the WHITEHEAT_GET_HW_INFO command */
struct whiteheat_hw_info {
	__u8	hw_id;		/* hardware id number, WhiteHEAT = 0 */
	__u8	sw_major_rev;	/* major version number */
	__u8	sw_minor_rev;	/* minor version number */
	struct whiteheat_hw_eeprom_info {
		__u8	b0;			/* B0 */
		__u8	vendor_id_low;		/* vendor id (low byte) */
		__u8	vendor_id_high;		/* vendor id (high byte) */
		__u8	product_id_low;		/* product id (low byte) */
		__u8	product_id_high;	/* product id (high byte) */
		__u8	device_id_low;		/* device id (low byte) */
		__u8	device_id_high;		/* device id (high byte) */
		__u8	not_used_1;
		__u8	serial_number_0;	/* serial number (low byte) */
		__u8	serial_number_1;	/* serial number */
		__u8	serial_number_2;	/* serial number */
		__u8	serial_number_3;	/* serial number (high byte) */
		__u8	not_used_2;
		__u8	not_used_3;
		__u8	checksum_low;		/* checksum (low byte) */
		__u8	checksum_high;		/* checksum (high byte */
	} hw_eeprom_info;	/* EEPROM contents */
};

#endif

