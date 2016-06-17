/* $Id: pcikbd.h,v 1.2 1997/12/25 21:13:14 geert Exp $
 * pcikbd.h: PCI/PC 8042 keyboard/mouse driver stuff.  Mostly snarfed
 *           from the existing driver by Martin Mares.
 *
 * Copyright (C) 1997 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *
 * Ultra/AX specific hacks are:
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef __PCIKBD_H
#define __PCIKBD_H

/*
 *	Configuration Switches
 */

#define KBD_REPORT_ERR			/* Report keyboard errors */
#define KBD_REPORT_UNKN			/* Report unknown scan codes */
#define KBD_REPORT_TIMEOUTS		/* Report keyboard timeouts */

#define KBD_INIT_TIMEOUT HZ		/* Timeout for initializing the keyboard */
#define KBC_TIMEOUT (HZ/4)		/* Timeout for sending to keyboard controller */
#define KBD_TIMEOUT (HZ/4)		/* Timeout for keyboard command acknowledge */

/*
 *	Internal variables of the driver
 */

extern unsigned char pckbd_read_mask;
extern unsigned char aux_device_present;

extern unsigned long pcikbd_iobase;
extern unsigned int pcikbd_irq;

/*
 *	Keyboard Controller Registers
 *
 * NOTE: These are offsets from pcikbd_iobase, not absolute.
 */

#define KBD_STATUS_REG		0x04
#define KBD_CNTL_REG		KBD_STATUS_REG
#define KBD_DATA_REG		0x00

/*
 *	Keyboard Controller Commands
 */

#define KBD_CCMD_READ_MODE	0x20	/* Read mode bits */
#define KBD_CCMD_WRITE_MODE	0x60	/* Write mode bits */
#define KBD_CCMD_GET_VERSION	0xA1	/* Get controller version */
#define KBD_CCMD_MOUSE_DISABLE	0xA7	/* Disable mouse interface */
#define KBD_CCMD_MOUSE_ENABLE	0xA8	/* Enable mouse interface */
#define KBD_CCMD_TEST_MOUSE	0xA9	/* Mouse interface test */
#define KBD_CCMD_SELF_TEST	0xAA	/* Controller self test */
#define KBD_CCMD_KBD_TEST	0xAB	/* Keyboard interface test */
#define KBD_CCMD_KBD_DISABLE	0xAD	/* Keyboard interface disable */
#define KBD_CCMD_KBD_ENABLE	0xAE	/* Keyboard interface enable */
#define KBD_CCMD_WRITE_MOUSE	0xD4	/* Write the following byte to the mouse */

/*
 *	Keyboard Commands
 */

#define KBD_CMD_SET_LEDS	0xED	/* Set keyboard leds */
#define KBD_CMD_SET_RATE	0xF3	/* Set typematic rate */
#define KBD_CMD_ENABLE		0xF4	/* Enable scanning */
#define KBD_CMD_DISABLE		0xF5	/* Disable scanning */
#define KBD_CMD_RESET		0xFF	/* Reset */

/*
 *	Keyboard Replies
 */

#define KBD_REPLY_POR		0xAA	/* Power on reset */
#define KBD_REPLY_ACK		0xFA	/* Command ACK */
#define KBD_REPLY_RESEND	0xFE	/* Command NACK, send the cmd again */

/*
 *	Status Register Bits
 */

#define KBD_STAT_OBF 		0x01	/* Keyboard output buffer full */
#define KBD_STAT_IBF 		0x02	/* Keyboard input buffer full */
#define KBD_STAT_SELFTEST	0x04	/* Self test successful */
#define KBD_STAT_CMD		0x08	/* Last write was a command write (0=data) */
#define KBD_STAT_UNLOCKED	0x10	/* Zero if keyboard locked */
#define KBD_STAT_MOUSE_OBF	0x20	/* Mouse output buffer full */
#define KBD_STAT_GTO 		0x40	/* General receive/xmit timeout */
#define KBD_STAT_PERR 		0x80	/* Parity error */

#define AUX_STAT_OBF (KBD_STAT_OBF | KBD_STAT_MOUSE_OBF)

/*
 *	Controller Mode Register Bits
 */

#define KBD_MODE_KBD_INT	0x01	/* Keyboard data generage IRQ1 */
#define KBD_MODE_MOUSE_INT	0x02	/* Mouse data generate IRQ12 */
#define KBD_MODE_SYS 		0x04	/* The system flag (?) */
#define KBD_MODE_NO_KEYLOCK	0x08	/* The keylock doesn't affect the keyboard if set */
#define KBD_MODE_DISABLE_KBD	0x10	/* Disable keyboard interface */
#define KBD_MODE_DISABLE_MOUSE	0x20	/* Disable mouse interface */
#define KBD_MODE_KCC 		0x40	/* Scan code conversion to PC format */
#define KBD_MODE_RFU		0x80

/*
 *	Mouse Commands
 */

#define AUX_SET_RES		0xE8	/* Set resolution */
#define AUX_SET_SCALE11		0xE6	/* Set 1:1 scaling */
#define AUX_SET_SCALE21		0xE7	/* Set 2:1 scaling */
#define AUX_GET_SCALE		0xE9	/* Get scaling factor */
#define AUX_SET_STREAM		0xEA	/* Set stream mode */
#define AUX_SET_SAMPLE		0xF3	/* Set sample rate */
#define AUX_ENABLE_DEV		0xF4	/* Enable aux device */
#define AUX_DISABLE_DEV		0xF5	/* Disable aux device */
#define AUX_RESET		0xFF	/* Reset aux device */

#endif /* __PCIKBD_H */
