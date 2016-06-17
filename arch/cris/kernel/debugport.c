/* Serialport functions for debugging
 *
 * Copyright (c) 2000 Axis Communications AB
 *
 * Authors:  Bjorn Wesen
 *
 * Exports:
 *    console_print_etrax(char *buf)
 *    int getDebugChar()
 *    putDebugChar(int)
 *    enableDebugIRQ()
 *    init_etrax_debug()
 *
 * $Log: debugport.c,v $
 * Revision 1.9  2003/02/17 07:10:34  starvik
 * Last merge was incomplete
 *
 * Revision 1.8  2003/02/17 06:59:00  starvik
 * Merged printk corruption fix
 *
 * Revision 1.7.4.1  2003/01/27 10:21:57  starvik
 * Solved the problem with corrupted debug output
 *  * Wait until DMA, FIFO and pipe is empty before and after transmissions
 *  * Buffer data until a FIFO flush can be triggered.
 *
 * Revision 1.7  2002/04/23 15:35:50  bjornw
 * Cleaned up sercons struct and removed the waitkey ptr (2.4.19-pre)
 *
 * Revision 1.6  2001/04/17 13:58:39  orjanf
 * * Renamed CONFIG_KGDB to CONFIG_ETRAX_KGDB.
 *
 * Revision 1.5  2001/03/26 14:22:05  bjornw
 * Namechange of some config options
 *
 * Revision 1.4  2000/10/06 12:37:26  bjornw
 * Use physical addresses when talking to DMA
 *
 *
 */

#include <linux/config.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/svinto.h>
#include <asm/io.h>             /* Get SIMCOUT. */

/* Which serial-port is our debug port ? */

#if defined(CONFIG_ETRAX_DEBUG_PORT0) || defined(CONFIG_ETRAX_DEBUG_PORT_NULL)
#define DEBUG_PORT_IDX 0
#define DEBUG_OCMD R_DMA_CH6_CMD
#define DEBUG_FIRST R_DMA_CH6_FIRST
#define DEBUG_OCLRINT R_DMA_CH6_CLR_INTR
#define DEBUG_STATUS R_DMA_CH6_STATUS
#define DEBUG_READ R_SERIAL0_READ
#define DEBUG_WRITE R_SERIAL0_TR_DATA
#define DEBUG_TR_CTRL R_SERIAL0_TR_CTRL
#define DEBUG_REC_CTRL R_SERIAL0_REC_CTRL
#define DEBUG_IRQ IO_STATE(R_IRQ_MASK1_SET, ser0_data, set)
#define DEBUG_DMA_IRQ_CLR IO_STATE(R_IRQ_MASK2_CLR, dma6_descr, clr)
#endif

#ifdef CONFIG_ETRAX_DEBUG_PORT1
#define DEBUG_PORT_IDX 1
#define DEBUG_OCMD R_DMA_CH8_CMD
#define DEBUG_FIRST R_DMA_CH8_FIRST
#define DEBUG_OCLRINT R_DMA_CH8_CLR_INTR
#define DEBUG_STATUS R_DMA_CH8_STATUS
#define DEBUG_READ R_SERIAL1_READ
#define DEBUG_WRITE R_SERIAL1_TR_DATA
#define DEBUG_TR_CTRL R_SERIAL1_TR_CTRL
#define DEBUG_REC_CTRL R_SERIAL1_REC_CTRL
#define DEBUG_IRQ IO_STATE(R_IRQ_MASK1_SET, ser1_data, set)
#define DEBUG_DMA_IRQ_CLR IO_STATE(R_IRQ_MASK2_CLR, dma8_descr, clr)
#endif

#ifdef CONFIG_ETRAX_DEBUG_PORT2
#define DEBUG_PORT_IDX 2
#define DEBUG_OCMD R_DMA_CH2_CMD
#define DEBUG_FIRST R_DMA_CH2_FIRST
#define DEBUG_OCLRINT R_DMA_CH2_CLR_INTR
#define DEBUG_STATUS R_DMA_CH2_STATUS
#define DEBUG_READ R_SERIAL2_READ
#define DEBUG_WRITE R_SERIAL2_TR_DATA
#define DEBUG_TR_CTRL R_SERIAL2_TR_CTRL
#define DEBUG_REC_CTRL R_SERIAL2_REC_CTRL
#define DEBUG_IRQ IO_STATE(R_IRQ_MASK1_SET, ser2_data, set)
#define DEBUG_DMA_IRQ_CLR IO_STATE(R_IRQ_MASK2_CLR, dma2_descr, clr)
#endif

#ifdef CONFIG_ETRAX_DEBUG_PORT3
#define DEBUG_PORT_IDX 3
#define DEBUG_OCMD R_DMA_CH4_CMD
#define DEBUG_FIRST R_DMA_CH4_FIRST
#define DEBUG_OCLRINT R_DMA_CH4_CLR_INTR
#define DEBUG_STATUS R_DMA_CH4_STATUS
#define DEBUG_READ R_SERIAL3_READ
#define DEBUG_WRITE R_SERIAL3_TR_DATA
#define DEBUG_TR_CTRL R_SERIAL3_TR_CTRL
#define DEBUG_REC_CTRL R_SERIAL3_REC_CTRL
#define DEBUG_IRQ IO_STATE(R_IRQ_MASK1_SET, ser3_data, set)
#define DEBUG_DMA_IRQ_CLR IO_STATE(R_IRQ_MASK2_CLR, dma4_descr, clr)
#endif

#define MIN_SIZE 32 /* Size that triggers the FIFO to flush characters to interface */

/* Write a string of count length to the console (debug port) using DMA, polled
 * for completion. Interrupts are disabled during the whole process. Some
 * caution needs to be taken to not interfere with ttyS business on this port.
 */

static void 
console_write(struct console *co, const char *buf, unsigned int len)
{
	static struct etrax_dma_descr descr;
	static struct etrax_dma_descr descr2;
	static char tmp_buf[MIN_SIZE];
	static int tmp_size = 0;

	unsigned long flags; 
	
#ifdef CONFIG_ETRAX_DEBUG_PORT_NULL
        /* no debug printout at all */
        return;
#endif

#ifdef CONFIG_SVINTO_SIM
	/* no use to simulate the serial debug output */
	SIMCOUT(buf,len);
	return;
#endif
	
	save_flags(flags);
	cli();

#ifdef CONFIG_ETRAX_KGDB
	/* kgdb needs to output debug info using the gdb protocol */
	putDebugString(buf, len);
	restore_flags(flags);
	return;
#endif
	/* To make this work together with the real serial port driver
	 * we have to make sure that everything is flushed when we leave
	 * here. The following steps are made to assure this:
	 * 1. Wait until DMA stops, FIFO is empty and serial port pipeline empty.
	 * 2. Write at least half the FIFO to trigger flush to serial port.
	 * 3. Wait until DMA stops, FIFO is empty and serial port pipeline empty.
         */

	/* Do we have enough characters to make the DMA/FIFO happy? */
	if (tmp_size + len < MIN_SIZE)
	{
		int size = min((int)(MIN_SIZE - tmp_size),(int)len);
		memcpy(&tmp_buf[tmp_size], buf, size);
		tmp_size += size;
		len -= size;
        
		/* Pad with space if complete line */
		if (tmp_buf[tmp_size-1] == '\n')
		{
			memset(&tmp_buf[tmp_size-1], ' ', MIN_SIZE - tmp_size);
			tmp_buf[MIN_SIZE - 1] = '\n';
			tmp_size = MIN_SIZE;
			len = 0;
		}
		else
		{
                  /* Wait for more characters */
			restore_flags(flags);
			return;
		}
	}

	/* make sure the transmitter is enabled. 
	 * NOTE: this overrides any setting done in ttySx, to 8N1, no auto-CTS.
	 * in the future, move the tr/rec_ctrl shadows from etrax100ser.c to
	 * shadows.c and use it here as well...
	 */

	*DEBUG_TR_CTRL = 0x40;
	while(*DEBUG_OCMD & 7); /* Until DMA is not running */
	while(*DEBUG_STATUS & 0x7f); /* wait until output FIFO is empty as well */
	udelay(200); /* Wait for last two characters to leave the serial transmitter */

	if (tmp_size)
	{
		descr.ctrl = len ?  0 : d_eop | d_wait | d_eol;
		descr.sw_len = tmp_size;
		descr.buf = virt_to_phys(tmp_buf);
		descr.next = virt_to_phys(&descr2);
		descr2.ctrl = d_eop | d_wait | d_eol;
		descr2.sw_len = len;
		descr2.buf = virt_to_phys((char*)buf);
	}
	else
	{
		descr.ctrl = d_eop | d_wait | d_eol;
		descr.sw_len = len;
		descr.buf = virt_to_phys((char*)buf);
	}

	*DEBUG_FIRST = virt_to_phys(&descr); /* write to R_DMAx_FIRST */
	*DEBUG_OCMD = 1;       /* dma command start -> R_DMAx_CMD */

	/* wait until the output dma channel is ready again */
	while(*DEBUG_OCMD & 7);
	while(*DEBUG_STATUS & 0x7f);
	udelay(200);

	tmp_size = 0;
	restore_flags(flags);
}

/* legacy function */

void
console_print_etrax(const char *buf)
{
	console_write(NULL, buf, strlen(buf));
}

/* Use polling to get a single character FROM the debug port */

int
getDebugChar(void)
{
	unsigned long readval;
	
	do {
		readval = *DEBUG_READ;
	} while(!(readval & IO_MASK(R_SERIAL0_READ, data_avail)));

	return (readval & IO_MASK(R_SERIAL0_READ, data_in));
}

/* Use polling to put a single character to the debug port */

void
putDebugChar(int val)
{
	while(!(*DEBUG_READ & IO_MASK(R_SERIAL0_READ, tr_ready))) ;
;
	*DEBUG_WRITE = val;
}

/* Enable irq for receiving chars on the debug port, used by kgdb */

void
enableDebugIRQ(void)
{
	*R_IRQ_MASK1_SET = DEBUG_IRQ;
	/* use R_VECT_MASK directly, since we really bypass Linux normal
	 * IRQ handling in kgdb anyway, we don't need to use enable_irq
	 */
	*R_VECT_MASK_SET = IO_STATE(R_VECT_MASK_SET, serial, set);

	*DEBUG_REC_CTRL = IO_STATE(R_SERIAL0_REC_CTRL, rec_enable, enable);
}

static kdev_t 
console_device(struct console *c)
{
         return MKDEV(TTY_MAJOR, 64 + c->index);
}

static int __init 
console_setup(struct console *co, char *options)
{
        return 0;
}

static struct console sercons = {
        name : "ttyS",
        write: console_write,
        read : NULL,
        device : console_device,
	unblank : NULL,
	setup : console_setup,
	flags : CON_PRINTBUFFER,
	index : DEBUG_PORT_IDX,
	cflag : 0,
	next : NULL
};

/*
 *      Register console (for printk's etc)
 */

void __init 
init_etrax_debug(void)
{
	register_console(&sercons);
}
