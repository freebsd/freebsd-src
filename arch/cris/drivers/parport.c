/*
 * Parallel port driver for ETRAX.
 *
 * NOTE!
 *   Since par0 shares DMA with ser2 and par 1 shares DMA with ser3
 *   this should be handled if both are enabled at the same time.
 *   THIS IS NOT HANDLED YET!
 *
 * Copyright (c) 2001, 2002, 2003 Axis Communications AB
 *
 * Author: Fredrik Hugosson
 *
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/parport.h>
#include <linux/ioport.h>
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>

#include <linux/slab.h>
#include <linux/interrupt.h>

#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/segment.h>
#include <asm/system.h>

#include <asm/svinto.h>


#undef DEBUG
#ifdef DEBUG
#define DPRINTK printk
#else
static inline int DPRINTK(void *nothing, ...) {return 0;}
#endif

/*
 * Etrax100 DMAchannels:
 * Par0 out : DMA2
 * Par0 in  : DMA3
 * Par1 out : DMA4
 * Par1 in  : DMA5
 * NOTE! par0 is shared with ser2 and par1 is shared with ser3 regarding
 *       DMA and DMA irq
 */

//#define CONFIG_PAR0_INT 1
//#define CONFIG_PAR1_INT 1

/* Define some macros to access ETRAX 100 registers */
#define SETF(var, reg, field, val) var = (var & ~IO_MASK_(reg##_, field##_)) | \
					  IO_FIELD_(reg##_, field##_, val)
#define SETS(var, reg, field, val) var = (var & ~IO_MASK_(reg##_, field##_)) | \
					  IO_STATE_(reg##_, field##_, _##val)

struct etrax100par_struct {
	/* parallell port control */
	volatile u32 *reg_ctrl_data; /* R_PARx_CTRL_DATA */
	const volatile u32 *reg_status_data; /* R_PARx_STATUS_DATA */
	volatile u32 *reg_config; /* R_PARx_CONFIG */
	volatile u32 *reg_delay; /* R_PARx_DELAY */
	
	/* DMA control */
	int odma;
	unsigned long dma_irq;  /* bitnr in R_IRQ_MASK2 for dmaX_descr */

	volatile char *oclrintradr; /* adr to R_DMA_CHx_CLR_INTR, output */
	volatile u32 *ofirstadr;   /* adr to R_DMA_CHx_FIRST, output */
	volatile char *ocmdadr;     /* adr to R_DMA_CHx_CMD, output */
	
	volatile char *iclrintradr; /* adr to R_DMA_CHx_CLR_INTR, input */
	volatile u32 *ifirstadr;   /* adr to R_DMA_CHx_FIRST, input */
	volatile char *icmdadr;     /* adr to R_DMA_CHx_CMD, input */

	/* Non DMA interrupt stuff */
	unsigned long int_irq; /* R_VECT_MASK_RD */
	const volatile u32 *irq_mask_rd; /* R_IRQ_MASKX_RD */
	volatile u32 *irq_mask_clr; /* R_IRQ_MASKX_RD */
	const volatile u32 *irq_read; /* R_IRQ_READX */
	volatile u32 *irq_mask_set; /* R_IRQ_MASKX_SET */
	unsigned long irq_mask_tx;  /* bitmask in R_IRQ_ for tx (ready) int */
	unsigned long irq_mask_rx;  /* bitmask in R_IRQ_ for rx (data) int */
	unsigned long irq_mask_ecp_cmd;  /* mask in R_IRQ_ for ecp_cmd int */
	unsigned long irq_mask_peri;  /* bitmask in R_IRQ_ for peri int */
	int portnr;
  
	/* ----- end of fields initialised in port_table[] below ----- */

	struct parport *port;
  
	/* Shadow registers */
	volatile unsigned long reg_ctrl_data_shadow; /* for R_PARx_CTRL_DATA */
	volatile unsigned long reg_config_shadow;    /* for R_PARx_CONFIG */
	volatile unsigned long reg_delay_shadow;    /* for R_PARx_DELAY */
};

/* Always have the complete structs here, even if the port is not used!
 *  (that way we can index this by the port number)
 */
static struct etrax100par_struct port_table[] = {
	{ 
		R_PAR0_CTRL_DATA,
		R_PAR0_STATUS_DATA,
		R_PAR0_CONFIG,
		R_PAR0_DELAY,
		/* DMA interrupt stuff */
		2,
		1U << 4, /* uses DMA 2 and 3 */
		R_DMA_CH2_CLR_INTR,
		R_DMA_CH2_FIRST,
		R_DMA_CH2_CMD,
		R_DMA_CH3_CLR_INTR,
		R_DMA_CH3_FIRST,
		R_DMA_CH3_CMD,
		/* Non DMA interrupt stuff */
		IO_BITNR(R_VECT_MASK_RD, par0),
		R_IRQ_MASK0_RD,
		R_IRQ_MASK0_CLR,
		R_IRQ_READ0,
		R_IRQ_MASK0_SET,
		IO_FIELD(R_IRQ_MASK0_RD, par0_ready, 1U), /* tx (ready)*/
		IO_FIELD(R_IRQ_MASK0_RD, par0_data, 1U), /* rx (data)*/
		IO_FIELD(R_IRQ_MASK0_RD, par0_ecp_cmd, 1U), /* ecp_cmd */
		IO_FIELD(R_IRQ_MASK0_RD, par0_peri, 1U), /* peri */
		0
	},
	{
		R_PAR1_CTRL_DATA,
		R_PAR1_STATUS_DATA,
		R_PAR1_CONFIG,
		R_PAR1_DELAY,
		/* DMA interrupt stuff */
		4,
		1U << 8, /* uses DMA 4 and 5 */
		
		R_DMA_CH4_CLR_INTR,
		R_DMA_CH4_FIRST,
		R_DMA_CH4_CMD,
		R_DMA_CH5_CLR_INTR,
		R_DMA_CH5_FIRST,
		R_DMA_CH5_CMD,
		/* Non DMA interrupt stuff */
		IO_BITNR(R_VECT_MASK_RD, par1),
		R_IRQ_MASK1_RD,
		R_IRQ_MASK1_CLR,
		R_IRQ_READ1,
		R_IRQ_MASK1_SET,
		IO_FIELD(R_IRQ_MASK1_RD, par1_ready, 1U), /* tx (ready)*/
		IO_FIELD(R_IRQ_MASK1_RD, par1_data, 1U), /* rx (data)*/
		IO_FIELD(R_IRQ_MASK1_RD, par1_ecp_cmd, 1U), /* ecp_cmd */
		IO_FIELD(R_IRQ_MASK1_RD, par1_peri, 1U), /* peri */
		1
	}
};


#define NR_PORTS (sizeof(port_table)/sizeof(struct etrax100par_struct))

static void
parport_etrax_write_data(struct parport *p, unsigned char value)
{
	struct etrax100par_struct *info =
		(struct etrax100par_struct *)p->private_data;

	DPRINTK("* E100 PP %d: etrax_write_data %02X\n", p->portnum, value);
	SETF(info->reg_ctrl_data_shadow, R_PAR0_CTRL_DATA, data, value);
	*info->reg_ctrl_data = info->reg_ctrl_data_shadow;
}


static unsigned char
parport_etrax_read_data(struct parport *p)
{
	unsigned char ret;
	struct etrax100par_struct *info =
		(struct etrax100par_struct *)p->private_data;

	ret = IO_EXTRACT(R_PAR0_STATUS_DATA, data, *info->reg_status_data);

	DPRINTK("* E100 PP %d: etrax_read_data %02X\n", p->portnum, ret);
	return ret;
}


static void
parport_etrax_write_control(struct parport *p, unsigned char control)
{
	struct etrax100par_struct *info =
		(struct etrax100par_struct *)p->private_data;

	DPRINTK("* E100 PP %d: etrax_write_control %02x\n", p->portnum, control);
  
	SETF(info->reg_ctrl_data_shadow, R_PAR0_CTRL_DATA, strb,
	     (control & PARPORT_CONTROL_STROBE) > 0);
	SETF(info->reg_ctrl_data_shadow, R_PAR0_CTRL_DATA, autofd,
	     (control & PARPORT_CONTROL_AUTOFD) > 0);
	SETF(info->reg_ctrl_data_shadow, R_PAR0_CTRL_DATA, init,
	     (control & PARPORT_CONTROL_INIT) == 0);
	SETF(info->reg_ctrl_data_shadow, R_PAR0_CTRL_DATA, seli,
	     (control & PARPORT_CONTROL_SELECT) > 0);

	*info->reg_ctrl_data = info->reg_ctrl_data_shadow;
}


static unsigned char
parport_etrax_read_control( struct parport *p)
{
	unsigned char ret = 0;
	struct etrax100par_struct *info =
		(struct etrax100par_struct *)p->private_data;

	if (IO_EXTRACT(R_PAR0_CTRL_DATA, strb, info->reg_ctrl_data_shadow))
		ret |= PARPORT_CONTROL_STROBE;
	if (IO_EXTRACT(R_PAR0_CTRL_DATA, autofd, info->reg_ctrl_data_shadow))
		ret |= PARPORT_CONTROL_AUTOFD;
	if (!IO_EXTRACT(R_PAR0_CTRL_DATA, init, info->reg_ctrl_data_shadow))
		ret |= PARPORT_CONTROL_INIT;
	if (IO_EXTRACT(R_PAR0_CTRL_DATA, seli, info->reg_ctrl_data_shadow))
		ret |= PARPORT_CONTROL_SELECT;

	DPRINTK("* E100 PP %d: etrax_read_control %02x\n", p->portnum, ret);
	return ret;
}


static unsigned char
parport_etrax_frob_control(struct parport *p, unsigned char mask,
                           unsigned char val)
{
	unsigned char old;

	DPRINTK("* E100 PP %d: frob_control mask %02x, value %02x\n",
		p->portnum, mask, val);
	old = parport_etrax_read_control(p);
	parport_etrax_write_control(p, (old & ~mask) ^ val);
	return old;
}


static unsigned char
parport_etrax_read_status(struct parport *p)
{
	unsigned char ret = 0;
	struct etrax100par_struct *info =
		(struct etrax100par_struct *)p->private_data;

	if (IO_EXTRACT(R_PAR0_STATUS_DATA, fault, *info->reg_status_data))
		ret |= PARPORT_STATUS_ERROR;
	if (IO_EXTRACT(R_PAR0_STATUS_DATA, sel, *info->reg_status_data))
		ret |= PARPORT_STATUS_SELECT;
	if (IO_EXTRACT(R_PAR0_STATUS_DATA, perr, *info->reg_status_data))
		ret |= PARPORT_STATUS_PAPEROUT;
	if (IO_EXTRACT(R_PAR0_STATUS_DATA, ack, *info->reg_status_data))
		ret |= PARPORT_STATUS_ACK;
	if (!IO_EXTRACT(R_PAR0_STATUS_DATA, busy, *info->reg_status_data))
		ret |= PARPORT_STATUS_BUSY;

	DPRINTK("* E100 PP %d: status register %04x\n",
		p->portnum, *info->reg_status_data);
	DPRINTK("* E100 PP %d: read_status %02x\n", p->portnum, ret);
	return ret;
}


static void
parport_etrax_enable_irq(struct parport *p)
{
	struct etrax100par_struct *info =
		(struct etrax100par_struct *)p->private_data;
	*info->irq_mask_set = info->irq_mask_tx;
	DPRINTK("* E100 PP %d: enable irq\n", p->portnum);
}


static void
parport_etrax_disable_irq(struct parport *p)
{
	struct etrax100par_struct *info =
		(struct etrax100par_struct *)p->private_data;
	*info->irq_mask_clr = info->irq_mask_tx;
	DPRINTK("* E100 PP %d: disable irq\n", p->portnum);
}


static void
parport_etrax_data_forward(struct parport *p)
{
	struct etrax100par_struct *info =
		(struct etrax100par_struct *)p->private_data;

	DPRINTK("* E100 PP %d: forward mode\n", p->portnum);
	SETS(info->reg_ctrl_data_shadow, R_PAR0_CTRL_DATA, oe, enable);
	*info->reg_ctrl_data = info->reg_ctrl_data_shadow;
}


static void
parport_etrax_data_reverse(struct parport *p)
{
	struct etrax100par_struct *info =
		(struct etrax100par_struct *)p->private_data;

	DPRINTK("* E100 PP %d: reverse mode\n", p->portnum);
	SETS(info->reg_ctrl_data_shadow, R_PAR0_CTRL_DATA, oe, disable);
	*info->reg_ctrl_data = info->reg_ctrl_data_shadow;
}


static void
parport_etrax_init_state(struct pardevice *dev, struct parport_state *s)
{
	DPRINTK("* E100 PP: parport_etrax_init_state\n");
}


static void
parport_etrax_save_state(struct parport *p, struct parport_state *s)
{
	DPRINTK("* E100 PP: parport_etrax_save_state\n");
}


static void
parport_etrax_restore_state(struct parport *p, struct parport_state *s)
{
	DPRINTK("* E100 PP: parport_etrax_restore_state\n");
}


static void
parport_etrax_inc_use_count(void)
{
	MOD_INC_USE_COUNT;
}


static void
parport_etrax_dec_use_count(void)
{
	MOD_DEC_USE_COUNT;
}


static struct
parport_operations pp_etrax_ops = {
	parport_etrax_write_data,
	parport_etrax_read_data,

	parport_etrax_write_control,
	parport_etrax_read_control,
	parport_etrax_frob_control,

	parport_etrax_read_status,

	parport_etrax_enable_irq,
	parport_etrax_disable_irq,

	parport_etrax_data_forward, 
	parport_etrax_data_reverse, 

	parport_etrax_init_state,
	parport_etrax_save_state,
	parport_etrax_restore_state,

	parport_etrax_inc_use_count,
	parport_etrax_dec_use_count,

	parport_ieee1284_epp_write_data,
	parport_ieee1284_epp_read_data,
	parport_ieee1284_epp_write_addr,
	parport_ieee1284_epp_read_addr,

	parport_ieee1284_ecp_write_data,
	parport_ieee1284_ecp_read_data,
	parport_ieee1284_ecp_write_addr,

	parport_ieee1284_write_compat,
	parport_ieee1284_read_nibble,
	parport_ieee1284_read_byte,
};

	
static void 
parport_etrax_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct etrax100par_struct *info = (struct etrax100par_struct *)
		((struct parport *)dev_id)->private_data;
	DPRINTK("* E100 PP %d: Interrupt received\n",
		((struct parport *)dev_id)->portnum);
	*info->irq_mask_clr = info->irq_mask_tx;
	parport_generic_irq(irq, (struct parport *)dev_id, regs);
}

/* ----------- Initialisation code --------------------------------- */

static void __init
parport_etrax_show_parallel_version(void)
{
	printk("ETRAX 100LX parallel port driver v1.0, (c) 2001-2003 Axis Communications AB\n");
}

#ifdef CONFIG_ETRAX_PAR0_DMA
#define PAR0_USE_DMA 1
#else
#define PAR0_USE_DMA 0
#endif

#ifdef CONFIG_ETRAX_PAR1_DMA
#define PAR1_USE_DMA 1
#else
#define PAR1_USE_DMA 0
#endif

static void __init
parport_etrax_init_registers(void)
{
	struct etrax100par_struct *info;
	int i;

	for (i = 0, info = port_table; i < 2; i++, info++) {
#ifndef CONFIG_ETRAX_PARALLEL_PORT0
		if (i == 0)
			continue;
#endif
#ifndef CONFIG_ETRAX_PARALLEL_PORT1
		if (i == 1)
			continue;
#endif
		info->reg_config_shadow = 
			IO_STATE(R_PAR0_CONFIG, iseli, inv)       |
			IO_STATE(R_PAR0_CONFIG, iautofd, inv)     |
			IO_STATE(R_PAR0_CONFIG, istrb, inv)       |
			IO_STATE(R_PAR0_CONFIG, iinit, inv)       |
			IO_STATE(R_PAR0_CONFIG, rle_in, disable)  |
			IO_STATE(R_PAR0_CONFIG, rle_out, disable) |
			IO_STATE(R_PAR0_CONFIG, enable, on)       |
			IO_STATE(R_PAR0_CONFIG, force, off)       |
			IO_STATE(R_PAR0_CONFIG, ign_ack, wait)    |
			IO_STATE(R_PAR0_CONFIG, oe_ack, wait_oe)  |
			IO_STATE(R_PAR0_CONFIG, mode, manual);

		if ((i == 0 && PAR0_USE_DMA) || (i == 1 && PAR1_USE_DMA))
			info->reg_config_shadow |=
				IO_STATE(R_PAR0_CONFIG, dma, enable);
		else
			info->reg_config_shadow |=
				IO_STATE(R_PAR0_CONFIG, dma, disable);

		*info->reg_config = info->reg_config_shadow;

		info->reg_ctrl_data_shadow = 
			IO_STATE(R_PAR0_CTRL_DATA, peri_int, nop)    |
			IO_STATE(R_PAR0_CTRL_DATA, oe, enable)       |
			IO_STATE(R_PAR0_CTRL_DATA, seli, inactive)   |
			IO_STATE(R_PAR0_CTRL_DATA, autofd, inactive) |
			IO_STATE(R_PAR0_CTRL_DATA, strb, inactive)   |
			IO_STATE(R_PAR0_CTRL_DATA, init, inactive)   |
			IO_STATE(R_PAR0_CTRL_DATA, ecp_cmd, data)    |
			IO_FIELD(R_PAR0_CTRL_DATA, data, 0);
		*info->reg_ctrl_data = info->reg_ctrl_data_shadow;

		/* Clear peri int without setting shadow */
		*info->reg_ctrl_data = info->reg_ctrl_data_shadow |
			IO_STATE(R_PAR0_CTRL_DATA, peri_int, ack);

		info->reg_delay_shadow = 
			IO_FIELD(R_PAR0_DELAY, setup, 5)  |
			IO_FIELD(R_PAR0_DELAY, strobe, 5) |
			IO_FIELD(R_PAR0_DELAY, hold, 5);
		*info->reg_delay = info->reg_delay_shadow;
	}

#ifdef CONFIG_ETRAX_PARALLEL_PORT0
#ifdef CONFIG_ETRAX_PAR0_DMA
	RESET_DMA(PAR0_TX_DMA_NBR);
	WAIT_DMA(PAR0_TX_DMA_NBR);
#ifdef CONFIG_ETRAX_SERIAL_PORT2
	printk(" Warning - DMA clash with ser2!\n");
#endif /* SERIAL_PORT2 */
#endif /* DMA */
#endif /* PORT0 */

#ifdef CONFIG_ETRAX_PARALLEL_PORT1
#ifdef CONFIG_ETRAX_PAR1_DMA
	RESET_DMA(PAR1_TX_DMA_NBR);
	WAIT_DMA(PAR1_TX_DMA_NBR);
#ifdef CONFIG_ETRAX_SERIAL_PORT3
	printk(" Warning - DMA clash with ser3!\n");
#endif /* SERIAL_PORT3 */
#endif /* DMA */
#endif /* PORT1 */
} 


int __init
parport_etrax_init(void)
{
	struct parport *p;
	int port_exists = 0;
	int i;
	struct etrax100par_struct *info;
        const char *names[] = { "parallel 0 tx+rx", "parallel 1 tx+rx" };

	parport_etrax_show_parallel_version();
	parport_etrax_init_registers();

        for (i = 0, info = port_table; i < NR_PORTS; i++, info++) {
#ifndef CONFIG_ETRAX_PARALLEL_PORT0
		if (i == 0)
			continue;
#endif
#ifndef CONFIG_ETRAX_PARALLEL_PORT1
		if (i == 1)
			continue;
#endif
                p = parport_register_port((unsigned long)0, info->int_irq,
                                          PARPORT_DMA_NONE, &pp_etrax_ops);
                if (!p)
			continue;

                info->port = p;
                p->private_data = info;
                /* Axis FIXME: Set mode flags. */
                /* p->modes = PARPORT_MODE_TRISTATE | PARPORT_MODE_SAFEININT; */

	        if(request_irq(info->int_irq, parport_etrax_interrupt,
                               SA_SHIRQ, names[i], p)) {
	        	parport_unregister_port (p);
                        continue;
                }

                printk(KERN_INFO "%s: ETRAX 100LX port %d using irq\n",
                       p->name, i);
                parport_proc_register(p);
                parport_announce_port(p);
                port_exists = 1;
        }

	return port_exists;
}

void __exit
parport_etrax_exit(void)
{
	int i;
	struct etrax100par_struct *info;

        for (i = 0, info = port_table; i < NR_PORTS; i++, info++) {
#ifndef CONFIG_ETRAX_PARALLEL_PORT0
		if (i == 0)
			continue;
#endif
#ifndef CONFIG_ETRAX_PARALLEL_PORT1
		if (i == 1)
			continue;
#endif
		if (info->int_irq != PARPORT_IRQ_NONE)
			free_irq(info->int_irq, info->port);
		parport_proc_unregister(info->port);
		parport_unregister_port(info->port);
        }
}
