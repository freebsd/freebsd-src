/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 *      form: @(#)lptreg.h	1.1 (Berkeley) 12/19/90
 *	$Id: lptreg.h,v 1.2 1993/10/16 13:46:12 rgrimes Exp $
 */

/*
 * AT Parallel Port (for lineprinter)
 * Interface port and bit definitions
 * Written by William Jolitz 12/18/90
 * Copyright (C) William Jolitz 1990
 */

#define lpt_data	0	/* Data to/from printer (R/W) */

#define lpt_status	1	/* Status of printer (R) */
#define	LPS_NERR		0x08	/* printer no error */
#define	LPS_SEL			0x10	/* printer selected */
#define	LPS_OUT			0x20	/* printer out of paper */
#define	LPS_NACK		0x40	/* printer no ack of data */
#define	LPS_NBSY		0x80	/* printer no ack of data */

#define lpt_control	2	/* Control printer (R/W) */
#define	LPC_STB			0x01	/* strobe data to printer */
#define	LPC_AUTOL		0x02	/* automatic linefeed */
#define	LPC_NINIT		0x04	/* initialize printer */
#define	LPC_SEL			0x08	/* printer selected */
#define	LPC_ENA			0x10	/* printer out of paper */
