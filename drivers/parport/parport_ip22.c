/*
 * Low-level parallel port routines for the SGI Indy/Indigo2 builtin port
 *
 * Copyright (c) 2002 Vincent Stehlé <vincent.stehle@free.fr>
 * (shamelessly based on other parport_xxx.c)
 * See the "COPYING" license file at the top-level.
 *
 * PI1 registers have the same bits as the PC parallel port registers, but
 * all PI1 bits are "direct"; i.e. no invertion: A 1 is TTL "high" (5 V),
 * a 0 is TTL "low" (0 V). There are also some bits on the PI1, which have
 * no equivalents on the PC. They are for non-SPP modes.
 *
 * Here follows a little summary of the signals and their meanings:
 *
 * A "/" in front of a signal means that the _bit_ is inverted: i.e. a "1"
 * bit value means a TTL "low" (0 V) signal.
 *
 * A "-" in front of a signal means that it is low active: i.e. a TTL "low"
 * (0 V) value means that the signal is active.
 *
 * Internal only signals are between ()'s.
 *
 * Control
 * -------
 *                   PC                     SGI
 *
 * 7 6 5 4 3 2 1 0
 *  \ \ \ \ \ \ \ `- /-STROBE               -STROBE
 *   \ \ \ \ \ \ `-- /-AUTO FEED            -AFD
 *    \ \ \ \ \ `---  -INIT                 -INIT
 *     \ \ \ \ `---- /-SELECT               -SLIN
 *      \ \ \ `----- (IRQ 1=enabled)
 *       \ \ `------ (DIRECTION 0=forward)
 *        \ `-------                        (SEL ?)
 *         `--------
 *
 * Status
 * ------
 *                   PC                     SGI
 *
 * 7 6 5 4 3 2 1 0
 *  \ \ \ \ \ \ \ `-                         DEVID
 *   \ \ \ \ \ \ `--                          " "
 *    \ \ \ \ \ `---                         NOINK
 *     \ \ \ \ `----  -ERROR                -ERROR
 *      \ \ \ `-----   SELECT IN             ONLINE
 *       \ \ `------   PAPER END             PE
 *        \ `-------  -ACK                  -ACK
 *         `-------- / BUSY                  BUSY
 *
 * Note that some of those information have been "guessed" with a multimeter
 * and some spare nights :)
 *
 * Reminder: all functions await PC-style values, i.e. one should convert
 *           status and control.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/parport.h>
#include <asm/sgi/ioc.h>
#include <asm/sgi/pi1.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTK printk
#else
#define DPRINTK(x...)	do { } while (0)
#endif

const unsigned char control_invert = PARPORT_CONTROL_STROBE
	| PARPORT_CONTROL_AUTOFD | PARPORT_CONTROL_SELECT;
const unsigned char status_invert = PARPORT_STATUS_BUSY;

static struct parport *this_port = NULL;

static inline void debug_dump_registers(void)
{
#ifdef DEBUG
	printk(KERN_DEBUG
		"PI1 registers:\n"
		"             data= %02x\n"
		"          control= %02x\n"
		"           status= %02x\n"
		"      dma control= %02x\n"
		" interrupt status= %02x\n"
		"   interrupt mask= %02x\n"
		"          timer 1= %02x\n"
		"          timer 2= %02x\n"
		"          timer 3= %02x\n"
		"          timer 4= %02x\n"
		,
		sgioc->pport.data,
		sgioc->pport.ctrl,
		sgioc->pport.status,
		sgioc->pport.dmactrl,
		sgioc->pport.intstat,
		sgioc->pport.intmask,
		sgioc->pport.timer1,
		sgioc->pport.timer2,
		sgioc->pport.timer3,
		sgioc->pport.timer4
	);
#endif
}

static unsigned char pi1_read_data(struct parport *p)
{
	unsigned char data = sgioc->pport.data;
	DPRINTK("pi1_read_data: %#x\n", data);
	return data;
}

static void pi1_write_data(struct parport *p, unsigned char data)
{
	DPRINTK("pi1_write_data: %#x\n", data);
	sgioc->pport.data = data;
}

static unsigned char pi1_read_control(struct parport *p)
{
	const unsigned char mask = PARPORT_CONTROL_STROBE
		| PARPORT_CONTROL_AUTOFD | PARPORT_CONTROL_INIT
		| PARPORT_CONTROL_SELECT;
	unsigned char pctrl = sgioc->pport.ctrl,
		control = ((pctrl & mask) ^ control_invert);
	DPRINTK("pi1_read_control: %#x, %#x\n", pctrl, control);
	return control;
}

static void pi1_write_control(struct parport *p, unsigned char control)
{
	const unsigned char mask = PARPORT_CONTROL_STROBE
		| PARPORT_CONTROL_AUTOFD | PARPORT_CONTROL_INIT
		| PARPORT_CONTROL_SELECT,
	/* we enforce some necessary bits */
		force = 0x30;
	unsigned char pctrl = ((control & mask) ^ control_invert) | force;
	DPRINTK("pi1_write_control: %#x, %#x\n", control, pctrl);
	sgioc->pport.ctrl = pctrl;
}

static unsigned char pi1_frob_control(struct parport *p,
	unsigned char mask, unsigned char val)
{
	unsigned char old;
	DPRINTK(KERN_DEBUG "pi1_frob_control mask %02x, value %02x\n",mask,val);
	old = pi1_read_control(p);
	pi1_write_control(p, (old & ~mask) ^ val);
	return old;
}

static unsigned char pi1_read_status(struct parport *p)
{
	const unsigned mask = PARPORT_STATUS_ERROR | PARPORT_STATUS_SELECT
		| PARPORT_STATUS_PAPEROUT | PARPORT_STATUS_ACK
		| PARPORT_STATUS_BUSY;
	unsigned char pstat = sgioc->pport.status,
		status = ((pstat & mask) ^ status_invert);
	DPRINTK("pi1_read_status: %#x, %#x\n", pstat, status);
	return status;
}

static void pi1_init_state(struct pardevice *d, struct parport_state *s)
{
	DPRINTK("pi1_init_state\n");
}

static void pi1_save_state(struct parport *p, struct parport_state *s)
{
	DPRINTK("pi1_save_state\n");
}

static void pi1_restore_state(struct parport *p, struct parport_state *s)
{
	DPRINTK("pi1_restore_state\n");
}

static void pi1_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	DPRINTK("pi1_interrupt\n");
	parport_generic_irq(irq, (struct parport *) dev_id, regs);
}

static void pi1_enable_irq(struct parport *p)
{
	DPRINTK("pi1_enable_irq\n");
}

static void pi1_disable_irq(struct parport *p)
{
	DPRINTK("pi1_disable_irq\n");
}

static void pi1_data_forward(struct parport *p)
{
	DPRINTK("pi1_data_forward\n");
	sgioc->pport.ctrl &= ~PI1_CTRL_DIR;
}

static void pi1_data_reverse(struct parport *p)
{
	DPRINTK("pi1_data_reverse\n");
	sgioc->pport.ctrl |= PI1_CTRL_DIR;
}

static void pi1_inc_use_count(void)
{
	DPRINTK("pi1_inc_use_count\n");
	MOD_INC_USE_COUNT;
}

static void pi1_dec_use_count(void)
{
	DPRINTK("pi1_dec_use_count\n");
	MOD_DEC_USE_COUNT;
}

static struct parport_operations pi1_ops = {
	pi1_write_data,
	pi1_read_data,
	pi1_write_control,
	pi1_read_control,
	pi1_frob_control,
	pi1_read_status,
	pi1_enable_irq,
	pi1_disable_irq,
	pi1_data_forward,
	pi1_data_reverse,
	pi1_init_state,
	pi1_save_state,
	pi1_restore_state,
	pi1_inc_use_count,
	pi1_dec_use_count,
	parport_ieee1284_epp_write_data,
	parport_ieee1284_epp_read_data,
	parport_ieee1284_epp_write_addr,
	parport_ieee1284_epp_read_addr,
	parport_ieee1284_ecp_write_data,
	parport_ieee1284_ecp_read_data,
	parport_ieee1284_ecp_write_addr,
	parport_ieee1284_write_compat,
	parport_ieee1284_read_nibble,
	parport_ieee1284_read_byte
};

static void init_hardware(void)
{
	sgioc->pport.intmask = 0xfc;
	sgioc->pport.dmactrl = 1;
	sgioc->pport.ctrl = 0x3f;
	sgioc->pport.timer1 = 0;
	sgioc->pport.timer2 = 0;
	sgioc->pport.timer3 = 0;
	sgioc->pport.timer4 = 0;
	sgioc->pport.data = 0;
}

static int __init pi1_init(void)
{
	struct parport *p;

	DPRINTK("pi1_init\n");
	p = parport_register_port((unsigned long) &sgioc->pport.data,
				  PARPORT_IRQ_NONE, PARPORT_DMA_NONE,
				  &pi1_ops);
	if (!p)
		return -EBUSY;

/* TODO
	err = request_irq(IRQ_AMIGA_CIAA_FLG, amiga_interrupt, 0, p->name, p);
	if (err)
		goto out_irq;
*/
	
	/* tell what we are capable of */
	p->modes = PARPORT_MODE_PCSPP;

/* TODO
	 | PARPORT_MODE_TRISTATE;*/

	/* remember for exit */
	this_port = p;

	/* put hardware into known initial state */
	init_hardware();

	printk(KERN_INFO "%s: SGI PI1\n", p->name);
	parport_proc_register(p);
	parport_announce_port(p);

	return 0;
}

static void __exit pi1_exit(void)
{
	DPRINTK("pi1_exit\n");
	if (this_port->irq != PARPORT_IRQ_NONE)
		free_irq(this_port->irq, this_port);

	parport_proc_unregister(this_port);
	parport_unregister_port(this_port);
}

MODULE_AUTHOR("Vincent Stehle <vincent.stehle@free.fr>");
MODULE_DESCRIPTION("Driver for SGI Indy/Indigo2 parallel port");
MODULE_SUPPORTED_DEVICE("SGI PI1 parallel port");
MODULE_LICENSE("GPL");

module_init(pi1_init);
module_exit(pi1_exit);
