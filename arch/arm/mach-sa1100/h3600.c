/*
 * Hardware definitions for Compaq iPAQ H3xxx Handheld Computers
 *
 * Copyright 2000,1 Compaq Computer Corporation.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Jamey Hicks.
 *
 * History:
 *
 * 2001-10-??	Andrew Christian   Added support for iPAQ H3800
 *				   and abstracted EGPIO interface.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>

#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>
#include <linux/serial_core.h>
#include <asm/arch/h3600_gpio.h>

#include "generic.h"

/*
 * H3600 has extended, write-only memory-mapped GPIO's
 * H3100 has 1/2 extended, write-only GPIO and 1/2 on
 *	 regular GPIO lines.
 * H3800 has memory-mapped GPIO through ASIC1 & 2
 */

#define H3600_EGPIO	(*(volatile unsigned int *)H3600_EGPIO_VIRT)

static unsigned int h3600_egpio;

/************************* H3100 *************************/

#define H3100_DIRECT_EGPIO (GPIO_H3100_BT_ON	  \
			  | GPIO_H3100_GPIO3	  \
			  | GPIO_H3100_QMUTE	  \
			  | GPIO_H3100_LCD_3V_ON  \
			  | GPIO_H3100_AUD_ON	  \
			  | GPIO_H3100_AUD_PWR_ON \
			  | GPIO_H3100_IR_ON	  \
			  | GPIO_H3100_IR_FSEL)

static void h3100_init_egpio( void )
{
	GPDR |= H3100_DIRECT_EGPIO;
	GPCR = H3100_DIRECT_EGPIO;   /* Initially all off */

	/* Older bootldrs put GPIO2-9 in alternate mode on the
	   assumption that they are used for video */
	GAFR &= ~H3100_DIRECT_EGPIO;

	h3600_egpio = EGPIO_H3600_RS232_ON;
	H3600_EGPIO = h3600_egpio;
}

static void h3100_control_egpio( enum ipaq_egpio_type x, int setp )
{
	unsigned int egpio = 0;
	long	     gpio = 0;
	unsigned long flags;

	switch (x) {
	case IPAQ_EGPIO_LCD_ON:
		egpio |= EGPIO_H3600_LCD_ON;
		gpio  |= GPIO_H3100_LCD_3V_ON;
		break;
	case IPAQ_EGPIO_CODEC_NRESET:
		egpio |= EGPIO_H3600_CODEC_NRESET;
		break;
	case IPAQ_EGPIO_AUDIO_ON:
		gpio |= GPIO_H3100_AUD_PWR_ON
			| GPIO_H3100_AUD_ON;
		break;
	case IPAQ_EGPIO_QMUTE:
		gpio |= GPIO_H3100_QMUTE;
		break;
	case IPAQ_EGPIO_OPT_NVRAM_ON:
		egpio |= EGPIO_H3600_OPT_NVRAM_ON;
		break;
	case IPAQ_EGPIO_OPT_ON:
		egpio |= EGPIO_H3600_OPT_ON;
		break;
	case IPAQ_EGPIO_CARD_RESET:
		egpio |= EGPIO_H3600_CARD_RESET;
		break;
	case IPAQ_EGPIO_OPT_RESET:
		egpio |= EGPIO_H3600_OPT_RESET;
		break;
	case IPAQ_EGPIO_IR_ON:
		gpio |= GPIO_H3100_IR_ON;
		break;
	case IPAQ_EGPIO_IR_FSEL:
		gpio |= GPIO_H3100_IR_FSEL;
		break;
	case IPAQ_EGPIO_RS232_ON:
		egpio |= EGPIO_H3600_RS232_ON;
		break;
	case IPAQ_EGPIO_VPP_ON:
		egpio |= EGPIO_H3600_VPP_ON;
		break;
	}

	local_irq_save(flags);
	if ( setp ) {
		h3600_egpio |= egpio;
		GPSR = gpio;
	} else {
		h3600_egpio &= ~egpio;
		GPCR = gpio;
	}
	H3600_EGPIO = h3600_egpio;
	local_irq_restore(flags);

	/*
	if ( x != IPAQ_EGPIO_VPP_ON ) {
		printk(__FUNCTION__ " : type=%d (%s) gpio=0x%x (0x%x) egpio=0x%x (0x%x) setp=%d\n",
		       x, egpio_names[x], GPLR, gpio, h3600_egpio, egpio, setp );
	}
	*/
}

static unsigned long h3100_read_egpio( void )
{
	return h3600_egpio;
}

static struct ipaq_model_ops h3100_model_ops __initdata = {
	.model		= IPAQ_H3100,
	.generic_name	= "3100",
	.initialize	= h3100_init_egpio,
	.control	= h3100_control_egpio,
	.read		= h3100_read_egpio
};


/************************* H3600 *************************/

static void h3600_init_egpio( void )
{
	h3600_egpio = EGPIO_H3600_RS232_ON;
	H3600_EGPIO = h3600_egpio;
}

static void h3600_control_egpio( enum ipaq_egpio_type x, int setp )
{
	unsigned int egpio = 0;
	unsigned long flags;

	switch (x) {
	case IPAQ_EGPIO_LCD_ON:
		egpio |= EGPIO_H3600_LCD_ON |
			 EGPIO_H3600_LCD_PCI |
			 EGPIO_H3600_LCD_5V_ON |
			 EGPIO_H3600_LVDD_ON;
		break;
	case IPAQ_EGPIO_CODEC_NRESET:
		egpio |= EGPIO_H3600_CODEC_NRESET;
		break;
	case IPAQ_EGPIO_AUDIO_ON:
		egpio |= EGPIO_H3600_AUD_AMP_ON |
			EGPIO_H3600_AUD_PWR_ON;
		break;
	case IPAQ_EGPIO_QMUTE:
		egpio |= EGPIO_H3600_QMUTE;
		break;
	case IPAQ_EGPIO_OPT_NVRAM_ON:
		egpio |= EGPIO_H3600_OPT_NVRAM_ON;
		break;
	case IPAQ_EGPIO_OPT_ON:
		egpio |= EGPIO_H3600_OPT_ON;
		break;
	case IPAQ_EGPIO_CARD_RESET:
		egpio |= EGPIO_H3600_CARD_RESET;
		break;
	case IPAQ_EGPIO_OPT_RESET:
		egpio |= EGPIO_H3600_OPT_RESET;
		break;
	case IPAQ_EGPIO_IR_ON:
		egpio |= EGPIO_H3600_IR_ON;
		break;
	case IPAQ_EGPIO_IR_FSEL:
		egpio |= EGPIO_H3600_IR_FSEL;
		break;
	case IPAQ_EGPIO_RS232_ON:
		egpio |= EGPIO_H3600_RS232_ON;
		break;
	case IPAQ_EGPIO_VPP_ON:
		egpio |= EGPIO_H3600_VPP_ON;
		break;
	}

	local_irq_save(flags);
	if ( setp )
		h3600_egpio |= egpio;
	else
		h3600_egpio &= ~egpio;
	H3600_EGPIO = h3600_egpio;
	local_irq_restore(flags);
}

static unsigned long h3600_read_egpio( void )
{
	return h3600_egpio;
}

static struct ipaq_model_ops h3600_model_ops __initdata = {
	.model		= IPAQ_H3600,
	.generic_name	= "3600",
	.initialize	= h3600_init_egpio,
	.control	= h3600_control_egpio,
	.read		= h3600_read_egpio
};

/************************* H3800 *************************/

#define ASIC1_OUTPUTS	 0x7fff   /* First 15 bits are used */

static unsigned int h3800_asic1_gpio;
static unsigned int h3800_asic2_gpio;

static void h3800_init_egpio(void)
{
	/* Set up ASIC #1 */
	H3800_ASIC1_GPIO_Direction    = ASIC1_OUTPUTS;		  /* All outputs */
	H3800_ASIC1_GPIO_Mask	      = ASIC1_OUTPUTS;		  /* No interrupts */
	H3800_ASIC1_GPIO_SleepMask    = ASIC1_OUTPUTS;
	H3800_ASIC1_GPIO_SleepDir     = ASIC1_OUTPUTS;
	H3800_ASIC1_GPIO_SleepOut     = GPIO_H3800_ASIC1_EAR_ON_N;
	H3800_ASIC1_GPIO_BattFaultDir = ASIC1_OUTPUTS;
	H3800_ASIC1_GPIO_BattFaultOut = GPIO_H3800_ASIC1_EAR_ON_N;

	h3800_asic1_gpio = GPIO_H3800_ASIC1_IR_ON_N   /* TODO: Check IR level */
		| GPIO_H3800_ASIC1_RS232_ON
		| GPIO_H3800_ASIC1_EAR_ON_N;

	H3800_ASIC1_GPIO_Out = h3800_asic1_gpio;

	/* Set up ASIC #2 */
	H3800_ASIC2_GPIO_Direction = GPIO_H3800_ASIC2_PEN_IRQ
		| GPIO_H3800_ASIC2_SD_DETECT
		| GPIO_H3800_ASIC2_EAR_IN_N
		| GPIO_H3800_ASIC2_USB_DETECT_N
		| GPIO_H3800_ASIC2_SD_CON_SLT;

	h3800_asic2_gpio = GPIO_H3800_ASIC2_IN_Y1_N | GPIO_H3800_ASIC2_IN_X1_N;
	H3800_ASIC2_GPIO_Data	      = h3800_asic2_gpio;
	H3800_ASIC2_GPIO_BattFaultOut = h3800_asic2_gpio;

	/* TODO : Set sleep states & battery fault states */

	/* Clear VPP Enable */
	H3800_ASIC1_FlashWP_VPP_ON = 0;
}

static void h3800_control_egpio( enum ipaq_egpio_type x, int setp )
{
	unsigned int set_asic1_egpio = 0;
	unsigned int clear_asic1_egpio = 0;
	unsigned long flags;

	switch (x) {
	case IPAQ_EGPIO_LCD_ON:
		set_asic1_egpio |= GPIO_H3800_ASIC1_LCD_5V_ON
			| GPIO_H3800_ASIC1_LCD_ON
			| GPIO_H3800_ASIC1_LCD_PCI
			| GPIO_H3800_ASIC1_VGH_ON
			| GPIO_H3800_ASIC1_VGL_ON;
		break;
	case IPAQ_EGPIO_CODEC_NRESET:
		break;
	case IPAQ_EGPIO_AUDIO_ON:
		break;
	case IPAQ_EGPIO_QMUTE:
		break;
	case IPAQ_EGPIO_OPT_NVRAM_ON:
		break;
	case IPAQ_EGPIO_OPT_ON:
		break;
	case IPAQ_EGPIO_CARD_RESET:
		break;
	case IPAQ_EGPIO_OPT_RESET:
		break;
	case IPAQ_EGPIO_IR_ON:
		clear_asic1_egpio |= GPIO_H3800_ASIC1_IR_ON_N;	 /* TODO : This is backwards? */
		break;
	case IPAQ_EGPIO_IR_FSEL:
		break;
	case IPAQ_EGPIO_RS232_ON:
		set_asic1_egpio |= GPIO_H3800_ASIC1_RS232_ON;
		break;
	case IPAQ_EGPIO_VPP_ON:
		H3800_ASIC1_FlashWP_VPP_ON = setp;
		break;
	}

	local_irq_save(flags);
	if ( setp ) {
		h3800_asic1_gpio |= set_asic1_egpio;
		h3800_asic1_gpio &= ~clear_asic1_egpio;
	}
	else {
		h3800_asic1_gpio &= ~set_asic1_egpio;
		h3800_asic1_gpio |= clear_asic1_egpio;
	}
	H3800_ASIC1_GPIO_Out = h3800_asic1_gpio;
	local_irq_restore(flags);
}

static unsigned long h3800_read_egpio( void )
{
	return h3800_asic1_gpio | (h3800_asic2_gpio << 16);
}

static struct ipaq_model_ops h3800_model_ops __initdata = {
	.model		= IPAQ_H3800,
	.generic_name	= "3800",
	.initialize	= h3800_init_egpio,
	.control	= h3800_control_egpio,
	.read		= h3800_read_egpio
};


/*
 * Use command line argument to choose between ipaq handheld model numbers
 */

struct ipaq_model_ops ipaq_model_ops;
EXPORT_SYMBOL(ipaq_model_ops);

static int __init h3600_init_model_ops(void)
{
	if (machine_is_h3xxx()) {
		if (machine_is_h3100()) {
			ipaq_model_ops = h3100_model_ops;
		} else if (machine_is_h3600()) {
			ipaq_model_ops = h3600_model_ops;
		} else if (machine_is_h3800()) {
			ipaq_model_ops = h3800_model_ops;
		}
		init_h3600_egpio();
	}
	return 0;
}

__initcall(h3600_init_model_ops);

/*
 * low-level UART features
 */

static void h3600_uart_set_mctrl(struct uart_port *port, u_int mctrl)
{
	if (port->mapbase == _Ser3UTCR0) {
		if (mctrl & TIOCM_RTS)
			GPCR = GPIO_H3600_COM_RTS;
		else
			GPSR = GPIO_H3600_COM_RTS;
	}
}

static u_int h3600_uart_get_mctrl(struct uart_port *port)
{
	u_int ret = TIOCM_CD | TIOCM_CTS | TIOCM_DSR;

	if (port->mapbase == _Ser3UTCR0) {
		int gplr = GPLR;
		if (gplr & GPIO_H3600_COM_DCD)
			ret &= ~TIOCM_CD;
		if (gplr & GPIO_H3600_COM_CTS)
			ret &= ~TIOCM_CTS;
	}

	return ret;
}

static void h3600_dcd_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	struct uart_port *port = dev_id;
	/* Note: should only call this if something has changed */
	uart_handle_dcd_change(port, !(GPLR & GPIO_H3600_COM_DCD));
}

static void h3600_cts_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	struct uart_port *port = dev_id;
	/* Note: should only call this if something has changed */
	uart_handle_cts_change(port, !(GPLR & GPIO_H3600_COM_CTS));
}

static void h3600_uart_pm(struct uart_port *port, u_int state, u_int oldstate)
{
	if (port->mapbase == _Ser2UTCR0) {
		assign_h3600_egpio( IPAQ_EGPIO_IR_ON, !state );
	} else if (port->mapbase == _Ser3UTCR0) {
		assign_h3600_egpio( IPAQ_EGPIO_RS232_ON, !state );
	}
}

/*
 * Enable/Disable wake up events for this serial port.
 * Obviously, we only support this on the normal COM port.
 */
static int h3600_uart_set_wake(struct uart_port *port, u_int enable)
{
	int err = -EINVAL;

	if (port->mapbase == _Ser3UTCR0) {
		if (enable)
			PWER |= PWER_GPIO23 | PWER_GPIO25 ; /* DCD and CTS */
		else
			PWER &= ~(PWER_GPIO23 | PWER_GPIO25); /* DCD and CTS */
		err = 0;
	}
	return err;
}

static int h3600_uart_open(struct uart_port *port)
{
	int ret = 0;

	if (port->mapbase == _Ser2UTCR0) {
		Ser2UTCR4 = UTCR4_HSE;
		Ser2HSCR0 = 0;
		Ser2HSSR0 = HSSR0_EIF | HSSR0_TUR |
			    HSSR0_RAB | HSSR0_FRE;
	} else if (port->mapbase == _Ser3UTCR0) {
		set_GPIO_IRQ_edge(GPIO_H3600_COM_DCD|GPIO_H3600_COM_CTS,
				  GPIO_BOTH_EDGES);

		ret = request_irq(IRQ_GPIO_H3600_COM_DCD, h3600_dcd_intr,
				  0, "RS232 DCD", port);
		if (ret)
			return ret;

		ret = request_irq(IRQ_GPIO_H3600_COM_CTS, h3600_cts_intr,
				  0, "RS232 CTS", port);
		if (ret)
			free_irq(IRQ_GPIO_H3600_COM_DCD, port);
	}
	return ret;
}

static void h3600_uart_close(struct uart_port *port)
{
	if (port->mapbase == _Ser3UTCR0) {
		free_irq(IRQ_GPIO_H3600_COM_DCD, port);
		free_irq(IRQ_GPIO_H3600_COM_CTS, port);
	}
}

static struct sa1100_port_fns h3600_port_fns __initdata = {
	.set_mctrl	= h3600_uart_set_mctrl,
	.get_mctrl	= h3600_uart_get_mctrl,
	.pm		= h3600_uart_pm,
	.set_wake	= h3600_uart_set_wake,
	.open		= h3600_uart_open,
	.close		= h3600_uart_close,
};

static struct map_desc h3600_io_desc[] __initdata = {
 /* virtual	       physical    length      domain	  r  w	c  b */
  { 0xe8000000,        0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 0 	 CS#0 */
  { H3600_EGPIO_VIRT,  0x49000000, 0x01000000, DOMAIN_IO, 0, 1, 0, 0 }, /* EGPIO 0		 CS#5 */
  { H3600_BANK_2_VIRT, 0x10000000, 0x02800000, DOMAIN_IO, 0, 1, 0, 0 }, /* static memory bank 2  CS#2 */
  { H3600_BANK_4_VIRT, 0x40000000, 0x00800000, DOMAIN_IO, 0, 1, 0, 0 }, /* static memory bank 4  CS#4 */
  LAST_DESC
};

static void __init h3600_map_io(void)
{
	sa1100_map_io();
	iotable_init(h3600_io_desc);

	sa1100_register_uart_fns(&h3600_port_fns);
	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1); /* isn't this one driven elsewhere? */

	/*
	 * Default GPIO settings.  Should be set by machine
	 */
	GPCR = 0x0fffffff;
//	GPDR = 0x0401f3fc;
	GPDR = GPIO_H3600_COM_RTS  | GPIO_H3600_L3_CLOCK |
	       GPIO_H3600_L3_MODE  | GPIO_H3600_L3_DATA  |
	       GPIO_H3600_CLK_SET1 | GPIO_H3600_CLK_SET0 |
	       GPIO_LDD15 | GPIO_LDD14 | GPIO_LDD13 | GPIO_LDD12 |
	       GPIO_LDD11 | GPIO_LDD10 | GPIO_LDD9  | GPIO_LDD8;

	init_h3600_egpio();

	/*
	 * Ensure those pins are outputs and driving low.
	 */
	PPDR |= PPC_TXD4 | PPC_SCLK | PPC_SFRM;
	PPSR &= ~(PPC_TXD4 | PPC_SCLK | PPC_SFRM);

	/* Configure suspend conditions */
	PGSR = 0;
	PWER = PWER_GPIO0 | PWER_RTC;
	PCFR = PCFR_OPDE;
	PSDR = 0;
}

MACHINE_START(H3600, "Compaq iPAQ H3600")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	MAPIO(h3600_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
MACHINE_START(H3100, "Compaq iPAQ H3100")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	MAPIO(h3600_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
MACHINE_START(H3800, "Compaq iPAQ H3800")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	MAPIO(h3600_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
