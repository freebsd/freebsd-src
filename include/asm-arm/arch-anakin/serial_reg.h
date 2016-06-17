/*
 *  linux/include/asm-arm/arch-anakin/serial_reg.h
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   09-Apr-2001 TTC	Created
 */

#ifndef ASM_ARCH_SERIAL_REG_H
#define ASM_ARCH_SERIAL_REG_H

/*
 * Serial registers (other than tx/rx)
 */

/*
 * [UARTx + 0x10]
 */
#define RXRELEASE		(1 << 0)
#define TXEMPTY			(1 << 1)
#define CTS			(1 << 2)
#define PRESCALER		(31 << 3)
#define SETBAUD(baud)		((230400 / (baud) - 1) << 3)
#define GETBAUD(prescaler)	(230400 / (((prescaler) >> 3) + 1))


/*
 * [UARTx + 0x18]
 */
#define IRQENABLE		(1 << 0)
#define SENDREQUEST		(1 << 1)
#define RTS			(1 << 2)
#define DTR			(1 << 3)
#define DCD			(1 << 4)
#define BLOCKRX			(1 << 5)
#define PARITY			(3 << 6)
#define SETPARITY(parity)	((parity) << 6)
#define GETPARITY(parity)	((parity) >> 6)
#define NONEPARITY              (0)
#define ODDPARITY               (1)
#define EVENPARITY              (2)

/*
 * [UARTx + 0x1c]
 */
#define TX			(1 << 0)
#define RX			(1 << 1)
#define OVERRUN			(1 << 2)

/*
 * [UARTx + 0x20]
 */
#define SETBREAK		(1 << 0)

/*
 * Software interrupt register
 */
#define TXENABLE		(1 << 0)

#endif 
