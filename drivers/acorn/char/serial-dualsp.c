/*
 *  linux/drivers/acorn/char/serial-dualsp.c
 *
 *  Copyright (C) 1996 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   30-07-1996	RMK	Created
 */
#include <linux/ioport.h>
#include <asm/ecard.h>

#define MAX_PORTS       3

struct serial_card_type {
	unsigned int	num_ports;
	unsigned int	baud_base;
	unsigned int	type;
	unsigned int	offset[MAX_PORTS];
};

static struct serial_card_type serport_type = {
	.num_ports	= 2,
	.baud_base	= 3686400 / 16,
	.type		= ECARD_RES_IOCSLOW,
	.offset 	= { 0x2000, 0x2020 },
};

static const struct ecard_id serial_cids[] = {
	{ MANU_SERPORT, 	PROD_SERPORT_DSPORT,	&serport_type	},
	{ 0xffff, 0xffff }
};

#include "serial-card.c"
