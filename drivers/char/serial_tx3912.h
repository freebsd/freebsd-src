/*
 *  drivers/char/serial_tx3912.h
 *
 *  Copyright (C) 2001 Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Serial driver for TMPR3912/05 and PR31700 processors
 */
#include <linux/serialP.h>
#include <linux/generic_serial.h>

#undef TX3912_UART_DEBUG

#ifdef TX3912_UART_DEBUG
#define TX3912_UART_DEBUG_OPEN		0x00000001
#define TX3912_UART_DEBUG_SETTING	0x00000002
#define TX3912_UART_DEBUG_FLOW		0x00000004
#define TX3912_UART_DEBUG_MODEMSIGNALS	0x00000008
#define TX3912_UART_DEBUG_TERMIOS	0x00000010
#define TX3912_UART_DEBUG_TRANSMIT	0x00000020
#define TX3912_UART_DEBUG_RECEIVE	0x00000040
#define TX3912_UART_DEBUG_INTERRUPTS	0x00000080
#define TX3912_UART_DEBUG_PROBE		0x00000100
#define TX3912_UART_DEBUG_INIT		0x00000200
#define TX3912_UART_DEBUG_CLEANUP	0x00000400
#define TX3912_UART_DEBUG_CLOSE		0x00000800
#define TX3912_UART_DEBUG_FIRMWARE	0x00001000
#define TX3912_UART_DEBUG_MEMTEST	0x00002000
#define TX3912_UART_DEBUG_THROTTLE	0x00004000
#define TX3912_UART_DEBUG_NO_TX		0xffffffdf
#define TX3912_UART_DEBUG_ALL		0xffffffff

#define rs_dprintk(f, str...) if(TX3912_UART_DEBUG_NO_TX & f) printk(str)
#define func_enter() rs_dprintk(TX3912_UART_DEBUG_FLOW,		\
				"rs: enter " __FUNCTION__ "\n")
#define func_exit() rs_dprintk(TX3912_UART_DEBUG_FLOW,		\
				"rs: exit " __FUNCTION__ "\n")
#else
#define rs_dprintk(f, str...)
#define func_enter()
#define func_exit()
#endif

/*
 * Hardware specific serial port structure
 */
struct rs_port { 	
	struct gs_port		gs;		/* Must be first field! */
	struct wait_queue	*shutdown_wait; 
	int			stat_flags;
	struct async_icount	icount;		/* Counters for 4 input IRQs */
	int			read_status_mask;
	int			ignore_status_mask;
	int			x_char;		/* XON/XOFF character */
}; 
