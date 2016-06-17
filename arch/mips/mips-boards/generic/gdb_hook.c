/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * This is the interface to the remote debugger stub.
 *
 */
#include <linux/config.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>

#include <asm/serial.h>
#include <asm/io.h>

static struct serial_state rs_table[RS_TABLE_SIZE] = {
	SERIAL_PORT_DFNS	/* Defined in serial.h */
};

static struct async_struct kdb_port_info = {0};

int (*generic_putDebugChar)(char);
char (*generic_getDebugChar)(void);

static __inline__ unsigned int serial_in(struct async_struct *info, int offset)
{
	return inb(info->port + offset);
}

static __inline__ void serial_out(struct async_struct *info, int offset,
				int value)
{
	outb(value, info->port+offset);
}

void rs_kgdb_hook(int tty_no) {
	int t;
	struct serial_state *ser = &rs_table[tty_no];

	kdb_port_info.state = ser;
	kdb_port_info.magic = SERIAL_MAGIC;
	kdb_port_info.port = ser->port;
	kdb_port_info.flags = ser->flags;

	/*
	 * Clear all interrupts
	 */
	serial_in(&kdb_port_info, UART_LSR);
	serial_in(&kdb_port_info, UART_RX);
	serial_in(&kdb_port_info, UART_IIR);
	serial_in(&kdb_port_info, UART_MSR);

	/*
	 * Now, initialize the UART
	 */
	serial_out(&kdb_port_info, UART_LCR, UART_LCR_WLEN8);	/* reset DLAB */
	if (kdb_port_info.flags & ASYNC_FOURPORT) {
		kdb_port_info.MCR = UART_MCR_DTR | UART_MCR_RTS;
		t = UART_MCR_DTR | UART_MCR_OUT1;
	} else {
		kdb_port_info.MCR
			= UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2;
		t = UART_MCR_DTR | UART_MCR_RTS;
	}

	kdb_port_info.MCR = t;		/* no interrupts, please */
	serial_out(&kdb_port_info, UART_MCR, kdb_port_info.MCR);

	/*
	 * and set the speed of the serial port
	 * (currently hardwired to 9600 8N1
	 */

	/* baud rate is fixed to 9600 (is this sufficient?)*/
	t = kdb_port_info.state->baud_base / 9600;
	/* set DLAB */
	serial_out(&kdb_port_info, UART_LCR, UART_LCR_WLEN8 | UART_LCR_DLAB);
	serial_out(&kdb_port_info, UART_DLL, t & 0xff);/* LS of divisor */
	serial_out(&kdb_port_info, UART_DLM, t >> 8);  /* MS of divisor */
	/* reset DLAB */
	serial_out(&kdb_port_info, UART_LCR, UART_LCR_WLEN8);
}

int putDebugChar(char c)
{
	return generic_putDebugChar(c);
}

char getDebugChar(void)
{
	return generic_getDebugChar();
}

int rs_putDebugChar(char c)
{

	if (!kdb_port_info.state) { 	/* need to init device first */
		return 0;
	}

	while ((serial_in(&kdb_port_info, UART_LSR) & UART_LSR_THRE) == 0)
		;

	serial_out(&kdb_port_info, UART_TX, c);

	return 1;
}

char rs_getDebugChar(void)
{
	if (!kdb_port_info.state) { 	/* need to init device first */
		return 0;
	}

	while (!(serial_in(&kdb_port_info, UART_LSR) & 1))
		;

	return(serial_in(&kdb_port_info, UART_RX));
}


#ifdef CONFIG_MIPS_ATLAS

#include <asm/mips-boards/atlas.h>
#include <asm/mips-boards/saa9730_uart.h>

#define INB(a)     inb((unsigned long)a)
#define OUTB(x,a)  outb(x,(unsigned long)a)

/*
 * This is the interface to the remote debugger stub
 * if the Philips part is used for the debug port,
 * called from the platform setup code.
 *
 * PCI init will not have been done yet, we make a
 * universal assumption about the way the bootloader (YAMON)
 * have located and set up the chip.
 */
static t_uart_saa9730_regmap *kgdb_uart = (void *)(ATLAS_SAA9730_REG + SAA9730_UART_REGS_ADDR);

static int saa9730_kgdb_active = 0;

void saa9730_kgdb_hook(void)
{
        volatile unsigned char t;

        /*
         * Clear all interrupts
         */
	t = INB(&kgdb_uart->Lsr);
	t += INB(&kgdb_uart->Msr);
	t += INB(&kgdb_uart->Thr_Rbr);
	t += INB(&kgdb_uart->Iir_Fcr);

        /*
         * Now, initialize the UART
         */
	/* 8 data bits, one stop bit, no parity */
	OUTB(SAA9730_LCR_DATA8, &kgdb_uart->Lcr);

        /* baud rate is fixed to 9600 (is this sufficient?)*/
	OUTB(0, &kgdb_uart->BaudDivMsb); /* HACK - Assumes standard crystal */
	OUTB(23, &kgdb_uart->BaudDivLsb); /* HACK - known for MIPS Atlas */

	/* Set RTS/DTR active */
	OUTB(SAA9730_MCR_DTR | SAA9730_MCR_RTS, &kgdb_uart->Mcr);
	saa9730_kgdb_active = 1;
}

int saa9730_putDebugChar(char c)
{

        if (!saa9730_kgdb_active) {     /* need to init device first */
                return 0;
        }

        while (!(INB(&kgdb_uart->Lsr) & SAA9730_LSR_THRE))
                ;
	OUTB(c, &kgdb_uart->Thr_Rbr);

        return 1;
}

char saa9730_getDebugChar(void)
{
	char c;

        if (!saa9730_kgdb_active) {     /* need to init device first */
                return 0;
        }
        while (!(INB(&kgdb_uart->Lsr) & SAA9730_LSR_DR))
                ;

	c = INB(&kgdb_uart->Thr_Rbr);
        return(c);
}

#endif
