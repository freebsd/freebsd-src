/*
 * linux/include/asm-arm/arch-sa1100/pleb.h
 *
 * Created 2000/12/08 by Daniel Potts <danielp@cse.unsw.edu.au>
 *
 * This file contains the hardware specific definitions for the
 * PLEB board. http://www.cse.unsw.edu.au/~pleb
 */

#ifndef _INCLUDE_PLEB_H_
#define _INCLUDE_PLEB_H_

#define PLEB_ETH0_P		(0x20000300)	/* Ethernet 0 in PCMCIA0 IO */
#define PLEB_ETH0_V		(0xf6000300)

#define GPIO_ETH0_IRQ		GPIO_GPIO (21)
#define GPIO_ETH0_EN		GPIO_GPIO (26)

#define IRQ_GPIO_ETH0_IRQ	IRQ_GPIO21

#endif
