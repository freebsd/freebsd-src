/*
 * Interface for the 93C46/26/06 serial eeprom parts.
 *
 * Copyright (c) 1995 Daniel M. Eischen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Daniel M. Eischen.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 *      $Id: 93cx6.c,v 1.3 1995/11/14 09:58:47 phk Exp $
 */

/*
 *   The instruction set of the 93C46/26/06 chips are as follows:
 *
 *               Start  OP
 *     Function   Bit  Code  Address    Data     Description
 *     -------------------------------------------------------------------
 *     READ        1    10   A5 - A0             Reads data stored in memory,
 *                                               starting at specified address
 *     EWEN        1    00   11XXXX              Write enable must preceed
 *                                               all programming modes
 *     ERASE       1    11   A5 - A0             Erase register A5A4A3A2A1A0
 *     WRITE       1    01   A5 - A0   D15 - D0  Writes register
 *     ERAL        1    00   10XXXX              Erase all registers
 *     WRAL        1    00   01XXXX    D15 - D0  Writes to all registers
 *     EWDS        1    00   00XXXX              Disables all programming
 *                                               instructions
 *     *Note: A value of X for address is a don't care condition.
 *
 *   The 93C46 has a four wire interface: clock, chip select, data in, and
 *   data out.  In order to perform one of the above functions, you need
 *   to enable the chip select for a clock period (typically a minimum of
 *   1 usec, with the clock high and low a minimum of 750 and 250 nsec
 *   respectively.  While the chip select remains high, you can clock in
 *   the instructions (above) starting with the start bit, followed by the
 *   OP code, Address, and Data (if needed).  For the READ instruction, the
 *   requested 16-bit register contents is read from the data out line but
 *   is preceded by an initial zero (leading 0, followed by 16-bits, MSB
 *   first).  The clock cycling from low to high initiates the next data
 *   bit to be sent from the chip.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/clock.h>
#include <i386/scsi/93cx6.h>

/*
 * Right now, we only have to read the SEEPROM.  But we make it easier to
 * add other 93Cx6 functions.
 */
static struct seeprom_cmd {
  	unsigned char len;
 	unsigned char bits[3];
} seeprom_read = {3, {1, 1, 0}};


/*
 * Wait for the SEERDY to go high; about 800 ns.
 */
#define CLOCK_PULSE(p, rdy)			\
	while ((inb(p) & rdy) == 0) {		\
		;  /* Do nothing */		\
	}

/*
 * Read the serial EEPROM and returns 1 if successful and 0 if
 * not successful.
 */
int read_seeprom (u_long   offset,
		  u_short *buf,
		  u_int	   start_addr,
		  int      count,
		  u_short  CS,   /* chip select */
		  u_short  CK,   /* clock */
		  u_short  DO,   /* data out */
		  u_short  DI,   /* data in */
		  u_short  RDY,  /* ready */
		  u_short  MS    /* mode select */)
{
	int i = 0, k = 0;
	unsigned char temp;

	/*
	 * Read the requested registers of the seeprom.  The loop
	 * will range from 0 to count-1.
	 */
	for (k = start_addr; k < count + start_addr; k = k + 1) {
		/* Send chip select for one clock cycle. */
		outb(offset, MS | CK | CS);
		CLOCK_PULSE(offset, RDY);

		/*
		 * Now we're ready to send the read command followed by the
		 * address of the 16-bit register we want to read.
		 */
		for (i = 0; i < seeprom_read.len; i = i + 1) {
			if (seeprom_read.bits[i])
				temp = MS | CS | DO;
			else
				temp = MS | CS;
			outb(offset, temp);
			CLOCK_PULSE(offset, RDY);
			temp = temp ^ CK;
			outb(offset, temp);
			CLOCK_PULSE(offset, RDY);
		}
		/* Send the 6 bit address (MSB first, LSB last). */
		for (i = 5; i >= 0; i = i - 1) {
			/* k is the address, i is the bit */
			if (k & (1 << i))
				temp = MS | CS | DO;
			else
				temp =  MS | CS;
			outb(offset, temp);
			CLOCK_PULSE(offset, RDY);
			temp = temp ^ CK;
			outb(offset, temp);
			CLOCK_PULSE(offset, RDY);
		}

		/*
		 * Now read the 16 bit register.  An initial 0 precedes the
		 * register contents which begins with bit 15 (MSB) and ends
		 * with bit 0 (LSB).  The initial 0 will be shifted off the
		 * top of our word as we let the loop run from 0 to 16.
		 */
		for (i = 0; i <= 16; i = i + 1) {
			temp = MS | CS;
			outb(offset, temp);
			CLOCK_PULSE(offset, RDY);
			temp = temp ^ CK;
			if (inb(offset) & DI)
				buf[k - start_addr] = 
					(buf[k - start_addr] << 1) | 0x1;
			else
				buf[k - start_addr] = (buf[k - start_addr]<< 1);
			outb(offset, temp);
			CLOCK_PULSE(offset, RDY);
		}

		/* Reset the chip select for the next command cycle. */
		outb(offset, MS);
		CLOCK_PULSE(offset, RDY);
		outb(offset, MS | CK);
		CLOCK_PULSE(offset, RDY);
		outb(offset, MS);
		CLOCK_PULSE(offset, RDY);
	}
#if 0
	printf ("Serial EEPROM:");
	for (k = 0; k < count; k = k + 1) {
		if (((k % 8) == 0) && (k != 0))
		{
			printf ("\n              ");
		}
		printf (" 0x%x", buf[k]);
	}
	printf ("\n");
#endif
	return (1);
}
