/*-
 * Copyright (C) 2006 M. Warner Losh. All rights reserved.
 * Copyright (C) 2012 Ian Lepore. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _ARM_AT91_GPIO_H
#define _ARM_AT91_GPIO_H

#ifndef _KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

/* Userland GPIO API for Atmel AT91 series SOC.
 *
 * Open /dev/pioN (where N is 0 for PIOA, 1 for PIOB, etc), and use ioctl(2)
 * calls to configure the pin(s) as needed.
 *
 * The userland interrupt support allows you to use read(2) and/or select(2) to
 * get notified of interrupts on PIO pins for which you enabled interrupt
 * notifications.  Each time an interrupt occurs on a given pin, that pin number
 * is written into a buffer as a uint8_t.  Thus, reading from /dev/pioN delivers
 * info on which interrupt(s) have occurred since the last read.  You can also
 * use select() to block until an interrupt occurs (you still need to read() to
 * consume the interrupt number bytes from the buffer.)
 */

struct at91_gpio_info
{
	uint32_t	output_status;	/* Current state of output pins */
	uint32_t	input_status;	/* 1->out 0->in bitmask */
	uint32_t	highz_status;	/* 1->highz 0->driven bitmask */
	uint32_t	pullup_status;	/* 1->floating 0->pullup engaged */
	uint32_t	glitch_status;	/* 0-> no glitch filter 1->gf */
	uint32_t	enabled_status;	/* 1->used for pio 0->other */
	uint32_t	periph_status;	/* 0->A periph 1->B periph */
	uint32_t	intr_status;	/* 1-> ISR enabled, 0->disabled */
	uint32_t	extra_status[8];/* Extra status info, device depend */
};

struct at91_gpio_cfg
{
	uint32_t	cfgmask;	/* which things change */
#define	AT91_GPIO_CFG_INPUT 	0x01	/* configure input/output pins */
#define	AT91_GPIO_CFG_HI_Z  	0x02	/* HiZ */
#define	AT91_GPIO_CFG_PULLUP	0x04	/* Enable/disable pullup resistors */
#define	AT91_GPIO_CFG_GLITCH	0x08	/* Glitch filtering */
#define	AT91_GPIO_CFG_GPIO  	0x10	/* Use pin for PIO or peripheral */
#define	AT91_GPIO_CFG_PERIPH	0x20	/* Select which peripheral to use */
#define	AT91_GPIO_CFG_INTR  	0x40	/* Select pin for interrupts */
	uint32_t	iomask;		/* Mask of bits to change */
	uint32_t	input;		/* or output */
	uint32_t	hi_z;		/* Disable output */
	uint32_t	pullup;		/* Enable pullup resistor */
	uint32_t	glitch;		/* Glitch filtering */
	uint32_t	gpio;		/* Enabled for PIO (1) or periph (0) */
	uint32_t	periph;		/* Select periph A (0) or periph B (1) */
	uint32_t	intr;		/* Enable interrupt (1), or not (0) */
};

struct at91_gpio_bang
{
	uint32_t	clockpin;	/* clock pin MASK */
	uint32_t	datapin; 	/* Data pin MASK */
	uint32_t	bits;		/* bits to clock out (all 32) */
};

struct at91_gpio_bang_many
{
	uint32_t	clockpin;	/* clock pin MASK */
	uint32_t	datapin;	/* Data pin MASK */
	void		*bits;		/* bits to clock out */
	uint32_t	numbits;	/* Number of bits to clock out */
};

#define	AT91_GPIO_SET		_IOW('g', 0, uint32_t)			/* Turn bits on */
#define	AT91_GPIO_CLR		_IOW('g', 1, uint32_t)			/* Turn bits off */
#define	AT91_GPIO_READ		_IOR('g', 2, uint32_t)			/* Read input bit state */
#define	AT91_GPIO_INFO		_IOR('g', 3, struct at91_gpio_info)	/* State of pio cfg */
#define	AT91_GPIO_CFG		_IOW('g', 4, struct at91_gpio_cfg)	/* Configure pio */
#define	AT91_GPIO_BANG		_IOW('g', 5, struct at91_gpio_bang)	/* bit bang 32 bits */
#define	AT91_GPIO_BANG_MANY	_IOW('g', 6, struct at91_gpio_bang_many)/* bit bang >32 bits */

#endif /* _ARM_AT91_GPIO_H */

