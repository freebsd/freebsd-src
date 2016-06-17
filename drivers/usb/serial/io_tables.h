/*
 * IO Edgeport Driver tables
 *
 *	Copyright (C) 2001
 *	    Greg Kroah-Hartman (greg@kroah.com)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 * 
 */

#ifndef IO_TABLES_H
#define IO_TABLES_H

static struct usb_device_id edgeport_1port_id_table [] = {
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_PARALLEL_PORT) },
	{ }
};

static struct usb_device_id edgeport_2port_id_table [] = {
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_2) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_2I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_421) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_21) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_2_DIN) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_2) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_2I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_421) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_21) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_2_DIN) },
	{ }
};

static struct usb_device_id edgeport_4port_id_table [] = {
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_4) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_RAPIDPORT_4) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_4T) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_MT4X56USB) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_4I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_8_DUAL_CPU) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_4_DIN) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_COMPATIBLE) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_4) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_4T) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_4I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_8_DUAL_CPU) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_4_DIN) },
	{ }
};

static struct usb_device_id edgeport_8port_id_table [] = {
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_8) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_16_DUAL_CPU) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_EDGEPORT_8I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_8) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_16_DUAL_CPU) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_8I) },
	{ }
};

/* Devices that this driver supports */
static __devinitdata struct usb_device_id id_table_combined [] = {
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_4) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_RAPIDPORT_4) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_4T) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_MT4X56USB) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_2) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_4I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_2I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_PARALLEL_PORT) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_421) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_21) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_8_DUAL_CPU) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_8) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_2_DIN) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_4_DIN) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_16_DUAL_CPU) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_COMPATIBLE) },
	{ USB_DEVICE(USB_VENDOR_ID_ION,	ION_DEVICE_ID_EDGEPORT_8I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_2) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_2I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_421) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_21) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_2_DIN) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_4) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_4T) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_4I) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_8_DUAL_CPU) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_4_DIN) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_8) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_16_DUAL_CPU) },
	{ USB_DEVICE(USB_VENDOR_ID_ION, ION_DEVICE_ID_BB_EDGEPORT_8I) },
	{ }							/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, id_table_combined);

static struct usb_serial_device_type edgeport_1port_device = {
	owner:			THIS_MODULE,
	name:			"Edgeport 1 port adapter",
	id_table:		edgeport_1port_id_table,
	num_interrupt_in:	1,
	num_bulk_in:		1,
	num_bulk_out:		1,
	num_ports:		1,
	open:			edge_open,
	close:			edge_close,
	throttle:		edge_throttle,
	unthrottle:		edge_unthrottle,
	startup:		edge_startup,
	shutdown:		edge_shutdown,
	ioctl:			edge_ioctl,
	set_termios:		edge_set_termios,
	write:			edge_write,
	write_room:		edge_write_room,
	chars_in_buffer:	edge_chars_in_buffer,
	break_ctl:		edge_break,
};

static struct usb_serial_device_type edgeport_2port_device = {
	owner:			THIS_MODULE,
	name:			"Edgeport 2 port adapter",
	id_table:		edgeport_2port_id_table,
	num_interrupt_in:	1,
	num_bulk_in:		1,
	num_bulk_out:		1,
	num_ports:		2,
	open:			edge_open,
	close:			edge_close,
	throttle:		edge_throttle,
	unthrottle:		edge_unthrottle,
	startup:		edge_startup,
	shutdown:		edge_shutdown,
	ioctl:			edge_ioctl,
	set_termios:		edge_set_termios,
	write:			edge_write,
	write_room:		edge_write_room,
	chars_in_buffer:	edge_chars_in_buffer,
	break_ctl:		edge_break,
};

static struct usb_serial_device_type edgeport_4port_device = {
	owner:			THIS_MODULE,
	name:			"Edgeport 4 port adapter",
	id_table:		edgeport_4port_id_table,
	num_interrupt_in:	1,
	num_bulk_in:		1,
	num_bulk_out:		1,
	num_ports:		4,
	open:			edge_open,
	close:			edge_close,
	throttle:		edge_throttle,
	unthrottle:		edge_unthrottle,
	startup:		edge_startup,
	shutdown:		edge_shutdown,
	ioctl:			edge_ioctl,
	set_termios:		edge_set_termios,
	write:			edge_write,
	write_room:		edge_write_room,
	chars_in_buffer:	edge_chars_in_buffer,
	break_ctl:		edge_break,
};

static struct usb_serial_device_type edgeport_8port_device = {
	owner:			THIS_MODULE,
	name:			"Edgeport 8 port adapter",
	id_table:		edgeport_8port_id_table,
	num_interrupt_in:	1,
	num_bulk_in:		1,
	num_bulk_out:		1,
	num_ports:		8,
	open:			edge_open,
	close:			edge_close,
	throttle:		edge_throttle,
	unthrottle:		edge_unthrottle,
	startup:		edge_startup,
	shutdown:		edge_shutdown,
	ioctl:			edge_ioctl,
	set_termios:		edge_set_termios,
	write:			edge_write,
	write_room:		edge_write_room,
	chars_in_buffer:	edge_chars_in_buffer,
	break_ctl:		edge_break,
};

#endif

