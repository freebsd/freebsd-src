/*
 * Interface to the 93C46 serial EEPROM that is used to store BIOS
 * settings for the aic7xxx based adaptec SCSI controllers.  It can
 * also be used for 93C26 and 93C06 serial EEPROMS.
 *
 * Copyright (c) 1994, 1995 Justin T. Gibbs.
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
 *    Justin T. Gibbs.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 *      $Id: 93cx6.h,v 1.2 1995/09/05 23:52:00 gibbs Exp $
 */

#include <sys/param.h>
#if !defined(__NetBSD__)
#include <sys/systm.h>
#endif

struct seeprom_descriptor {
#if defined(__FreeBSD__)
	u_long sd_iobase;
#elif defined(__NetBSD__)
	bus_chipset_tag_t sd_bc;
	bus_io_handle_t sd_ioh;
	bus_io_size_t sd_offset;
#endif
	u_int16_t sd_MS;
	u_int16_t sd_RDY;
	u_int16_t sd_CS;
	u_int16_t sd_CK;
	u_int16_t sd_DO;
	u_int16_t sd_DI;
};

/*
 * This function will read count 16-bit words from the serial EEPROM and
 * return their value in buf.  The port address of the aic7xxx serial EEPROM
 * control register is passed in as offset.  The following parameters are
 * also passed in:
 *
 *   CS  - Chip select
 *   CK  - Clock
 *   DO  - Data out
 *   DI  - Data in
 *   RDY - SEEPROM ready
 *   MS  - Memory port mode select
 *
 *  A failed read attempt returns 0, and a successful read returns 1.
 */

#if defined(__FreeBSD__)
#define	SEEPROM_INB(sd)		inb(sd->sd_iobase)
#define	SEEPROM_OUTB(sd, value)	outb(sd->sd_iobase, value)
#elif defined(__NetBSD__)
#define	SEEPROM_INB(sd) \
	bus_io_read_1(sd->sd_bc, sd->sd_ioh, sd->sd_offset)
#define	SEEPROM_OUTB(sd, value) \
	bus_io_write_1(sd->sd_bc, sd->sd_ioh, sd->sd_offset, value)
#endif

#if defined(__FreeBSD__)
int read_seeprom __P((struct seeprom_descriptor *sd,
    u_int16_t *buf, u_int start_addr, int count));
#elif defined(__NetBSD__)
int read_seeprom __P((struct seeprom_descriptor *sd,
    u_int16_t *buf, bus_io_size_t start_addr, bus_io_size_t count));
#endif
