/*
 *  linux/arch/arm/drivers/char/serial-atomwide.c
 *
 *  Copyright (C) 1996 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   02-05-1996	RMK	Created
 *   07-05-1996	RMK	Altered for greater number of cards.
 *   30-07-1996	RMK	Now uses generic card code.
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

static struct serial_card_type atomwide_type = {
	.num_ports	= 3,
	.baud_base	= 7372800 / 16,
	.type		= ECARD_RES_IOCSLOW,
	.offset 	= { 0x2800, 0x2400, 0x2000 },
};

static const struct ecard_id serial_cids[] = {
	{ MANU_ATOMWIDE,	PROD_ATOMWIDE_3PSERIAL, &atomwide_type	},
	{ 0xffff, 0xffff }
};

#include "serial-card.c"
