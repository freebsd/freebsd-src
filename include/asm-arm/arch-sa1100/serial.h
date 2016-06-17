/*
 * include/asm-arm/arch-sa1100/serial.h
 * (C) 1999 Nicolas Pitre <nico@cam.org>
 *
 * All this is intended to be used with a 16550-like UART on the SA1100's 
 * PCMCIA bus.  It has nothing to do with the SA1100's internal serial ports.
 * This is included by serial.c -- serial_sa1100.c makes no use of it.
 */


/*
 * This assumes you have a 1.8432 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD ( 1843200 / 16 )

/* Standard COM flags */
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

#define RS_TABLE_SIZE 4


/*
 * Rather empty table...
 * Hardwired serial ports should be defined here.
 * PCMCIA will fill it dynamically.
 */
#define STD_SERIAL_PORT_DEFNS	\
       /* UART	CLK     	PORT		IRQ	FLAGS		*/ \
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS },   \
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS },   \
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS },   \
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS }

#define EXTRA_SERIAL_PORT_DEFNS

