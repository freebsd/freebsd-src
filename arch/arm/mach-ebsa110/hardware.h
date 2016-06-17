/*
 *  linux/arch/arm/mach-ebsa110/hardware.h
 *
 *  Copyright (C) 2001 Russell King
 *
 *  Local hardware definitions.
 */
#ifndef HARDWARE_H
#define HARDWARE_H

#define IRQ_MASK		0xfe000000	/* read */
#define IRQ_MSET		0xfe000000	/* write */
#define IRQ_STAT		0xff000000	/* read */
#define IRQ_MCLR		0xff000000	/* write */

#endif
