/*
* cycx_x25.h	Cyclom X.25 firmware API definitions.
*
* Author:	Arnaldo Carvalho de Melo <acme@conectiva.com.br>
*
* Copyright:	(c) 1998-2000 Arnaldo Carvalho de Melo
*
* Based on sdla_x25.h by Gene Kozin <74604.152@compuserve.com>
*
*		This program is free software; you can redistribute it and/or
*		modify it under the terms of the GNU General Public License
*		as published by the Free Software Foundation; either version
*		2 of the License, or (at your option) any later version.
* ============================================================================
* 2000/04/02	acme		dprintk and cycx_debug
* 1999/01/03	acme		judicious use of data types
* 1999/01/02	acme		#define X25_ACK_N3	0x4411
* 1998/12/28	acme		cleanup: lot'o'things removed
*					 commands listed,
*					 TX25Cmd & TX25Config structs
*					 typedef'ed
*/
#ifndef	_CYCX_X25_H
#define	_CYCX_X25_H

#ifndef PACKED
#define PACKED __attribute__((packed))
#endif 

/* X.25 shared memory layout. */
#define	X25_MBOX_OFFS	0x300	/* general mailbox block */
#define	X25_RXMBOX_OFFS	0x340	/* receive mailbox */

/* Debug */
#define dprintk(level, format, a...) if (cycx_debug >= level) printk(format, ##a)

extern unsigned int cycx_debug;

/* Data Structures */
/* X.25 Command Block. */
typedef struct X25Cmd
{
	u16 command PACKED;
	u16 link    PACKED; /* values: 0 or 1 */
	u16 len     PACKED; /* values: 0 thru 0x205 (517) */
	u32 buf     PACKED;
} TX25Cmd;

/* Defines for the 'command' field. */
#define X25_CONNECT_REQUEST             0x4401
#define X25_CONNECT_RESPONSE            0x4402
#define X25_DISCONNECT_REQUEST          0x4403
#define X25_DISCONNECT_RESPONSE         0x4404
#define X25_DATA_REQUEST                0x4405
#define X25_ACK_TO_VC			0x4406
#define X25_INTERRUPT_RESPONSE          0x4407
#define X25_CONFIG                      0x4408
#define X25_CONNECT_INDICATION          0x4409
#define X25_CONNECT_CONFIRM             0x440A
#define X25_DISCONNECT_INDICATION       0x440B
#define X25_DISCONNECT_CONFIRM          0x440C
#define X25_DATA_INDICATION             0x440E
#define X25_INTERRUPT_INDICATION        0x440F
#define X25_ACK_FROM_VC			0x4410
#define X25_ACK_N3			0x4411
#define X25_CONNECT_COLLISION           0x4413
#define X25_N3WIN                       0x4414
#define X25_LINE_ON                     0x4415
#define X25_LINE_OFF                    0x4416
#define X25_RESET_REQUEST               0x4417
#define X25_LOG                         0x4500
#define X25_STATISTIC                   0x4600
#define X25_TRACE                       0x4700
#define X25_N2TRACEXC                   0x4702
#define X25_N3TRACEXC                   0x4703

typedef struct X25Config {
	u8  link	PACKED; /* link number */
	u8  speed	PACKED; /* line speed */
	u8  clock	PACKED; /* internal/external */
	u8  n2		PACKED; /* # of level 2 retransm.(values: 1 thru FF) */
	u8  n2win	PACKED; /* level 2 window (values: 1 thru 7) */
	u8  n3win	PACKED; /* level 3 window (values: 1 thru 7) */
	u8  nvc		PACKED; /* # of logical channels (values: 1 thru 64) */
	u8  pktlen	PACKED; /* level 3 packet lenght - log base 2 of size */
	u8  locaddr	PACKED; /* my address */
	u8  remaddr	PACKED; /* remote address */
	u16 t1		PACKED;	/* time, in seconds */
	u16 t2		PACKED;	/* time, in seconds */
	u8  t21		PACKED;	/* time, in seconds */
	u8  npvc	PACKED;	/* # of permanent virt. circuits (1 thru nvc) */
	u8  t23		PACKED;	/* time, in seconds */
	u8  flags	PACKED;	/* see dosx25.doc, in portuguese, for details */
} TX25Config;

typedef struct X25Stats {
	u16 rx_crc_errors	PACKED;
	u16 rx_over_errors	PACKED;
	u16 n2_tx_frames 	PACKED;
	u16 n2_rx_frames 	PACKED;
	u16 tx_timeouts 	PACKED;
	u16 rx_timeouts 	PACKED;
	u16 n3_tx_packets 	PACKED;
	u16 n3_rx_packets 	PACKED;
	u16 tx_aborts	 	PACKED;
	u16 rx_aborts	 	PACKED;
} TX25Stats;
#endif	/* _CYCX_X25_H */
