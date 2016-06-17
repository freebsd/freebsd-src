/*
 * Copyright (C) 2003 Broadcom Corporation
 *    originally contributed to SiByte, Inc as
 *    "sb1250pc.c 0.10 (Stanley Chen & James Liao)"
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * Notes / Apologies:
 *   - only ATA cards tested so far
 *   - requires hack in cs.c to avoid poking the CISCOR register
 *   - ack_intr routine might be improved to avoid error msgs.
 *   - remove and re-insert doesn't work (crash or fail to probe drive)
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/ide.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/cs.h>
#include "cs_internal.h"

#include <asm/io.h>

#include <asm/sibyte/board.h>
#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_scd.h>
#include <asm/sibyte/sb1250_int.h>
#include <asm/sibyte/sb1250_genbus.h>
#include <asm/sibyte/64bit.h>

#define PFX "sibyte-pcmcia: "

MODULE_AUTHOR("Kip Walker, Stanley Chen & James Liao");
MODULE_DESCRIPTION("SiByte PCMCIA socket driver");

#undef PCMCIA_DEBUG
#ifdef PCMCIA_DEBUG
#define DPRINTK(args...) do { printk(KERN_DEBUG args); } while (0)
#else
#define DPRINTK(n, args...) do { } while (0)
#endif

static unsigned long sb_pcmcia_base = PCMCIA_PHYS;
static unsigned long sb_pcmcia_size;
#define SIBYTE_CS_REG(pcaddr)  (IO_SPACE_BASE + sb_pcmcia_base - mips_io_port_base + pcaddr)
#define SB_PC_PORT 0xff00
extern void sibyte_set_ideops(ide_hwif_t *hwif);

/* The card status change interrupt */
static int cs_irq = K_INT_PCMCIA;

/* Memory map windows */
static struct pccard_mem_map sibyte_memmap[MAX_WIN];
static struct pccard_io_map  sibyte_iomap[MAX_IO_WIN];

/*====================================================================*/
/* Socket structures                                                  */
/*====================================================================*/


static void sb1250pc_interrupt(int irq, void *dev, struct pt_regs *regs);
static struct pccard_operations sb1250pc_operations;

typedef struct socket_handler_t {
	void	(*handler)(void *info, u_int events);
	void	*info;
} socket_handler_t;

static socket_handler_t socket_handler;

/*
 * cap features:
 *   full 32-bit addressing for 16-bit PCcard memory windows
 *   16-bit card memory and IO accesses need bus_ops
 *   only 16-bit PCcards
 *   align memory windows
 *   statically mapped memory windows
 */
static socket_cap_t sb1250pc_cap = {
	features: (SS_CAP_PAGE_REGS |
//		   SS_CAP_VIRTUAL_BUS |
		   SS_CAP_PCCARD |
		   SS_CAP_MEM_ALIGN | SS_CAP_STATIC_MAP),
	irq_mask:  0,		/* tell ide layer to take PCI irq */
	map_size:  0x4000000,	/* 64MB minimum window size (What *should* this be?)*/
	io_offset: SB_PC_PORT,	/* This is ide5 -- just a special token for ide-sibyte */
	pci_irq:   K_INT_PC_READY, /* XXXKW This serves as IREQ# for CompactFlash */
	cb_dev:    NULL,
	bus:       NULL
};

/*====================================================================*/
/* Useful macros                                                      */
/*====================================================================*/

#define READ_PHYSADDR(addr) (*(volatile u32 *)(KSEG1ADDR(addr)))
#define WRITE_PHYSADDR(addr, data) (*(volatile u32 *)(KSEG1ADDR(addr))) = (data)

#define READ_CSR32(reg)		csr_in32(IO_SPACE_BASE + (reg))
#define WRITE_CSR32(data, reg)	csr_out32(data, IO_SPACE_BASE + (reg))

#define sb1250pc_write_config(data)	WRITE_CSR32(data, A_IO_PCMCIA_CFG)
#define sb1250pc_read_config()		READ_CSR32(A_IO_PCMCIA_CFG)
#define sb1250pc_read_status()		READ_CSR32(A_IO_PCMCIA_STATUS)

#define CARDPRESENT(s) (((s) & (M_PCMCIA_STATUS_CD1 | M_PCMCIA_STATUS_CD2)) == 0)

int sb_pcmcia_ack_intr(struct hwif_s *hwif)
{
	/*
	 * XXXKW verify interrupt and return appropriate value?
	 * Simple check of the bit in A_GPIO_READ didn't DTRT
	 */

	/* Clear out the GPIO edge detector */
	WRITE_CSR32(1 << K_GPIO_PC_READY, A_GPIO_CLR_EDGE);
	return 1;
}

/*====================================================================*/
/* Interrupt handling                                                 */
/*====================================================================*/

static unsigned int pending_events;
static spinlock_t pending_event_lock = SPIN_LOCK_UNLOCKED;

static void sb1250pc_bh(void *dummy)
{
	unsigned int events;

	spin_lock_irq(&pending_event_lock);
	events = pending_events;
	pending_events = 0;
	spin_unlock_irq(&pending_event_lock);

	if (socket_handler.handler)
		socket_handler.handler(socket_handler.info, events);
}

static struct tq_struct sb1250pc_task =
{
	routine:	sb1250pc_bh
};

static void sb1250pc_interrupt(int irq, void *dev, struct pt_regs *regs)
{
	unsigned int events = 0;
	uint32_t status;

	status = sb1250pc_read_status();

	if (status & M_PCMCIA_STATUS_CDCHG) {
		events = SS_DETECT;
		if (CARDPRESENT(status)) {
			events |= SS_INSERTION;
		} else {
			events |= SS_EJECTION;
		}
	}
#if 0
	/* XXXKW ignore everything but CD? */
	if (status & M_PCMCIA_STATUS_RDYCHG) {
		if (status & M_PCMCIA_STATUS_RDY) {
			/* XXXKW if ide, ack the interrupt? */
			events |= SS_READY;
		}
	}
#endif

	if (events) {
		DPRINTK(PFX " passing %x to bh\n", events);

		/* Pass the events off to the bottom-half */
		spin_lock(&pending_event_lock);
		pending_events |= events;
		spin_unlock(&pending_event_lock);
		schedule_task(&sb1250pc_task);
	}
}

/*====================================================================*/
/* PC Card Operations                                                 */
/*====================================================================*/

static int sb1250pc_register_callback(unsigned int lsock,
				      void (*handler)(void *, unsigned int),
				      void * info)
{
	DPRINTK(PFX "sb1250pc_register_callback(%d)\n", lsock);

	socket_handler.handler = handler;
	socket_handler.info = info;
	if (handler == NULL) {
		MOD_DEC_USE_COUNT;
	} else {
		MOD_INC_USE_COUNT;
	}
	return 0;
}

/*====================================================================*/

static int sb1250pc_get_status(unsigned int lsock, u_int *value)
{
	u_int val;
	uint32_t status;
#if PCMCIA_DEBUG
	u32 config;
#endif

	status = sb1250pc_read_status();
#if PCMCIA_DEBUG
	config = sb1250pc_read_config();
#endif

	val = CARDPRESENT(status) ? SS_DETECT : 0;
	val |= (status & M_PCMCIA_CFG_RESET) ? SS_RESET : 0;
	val |= (status & (M_PCMCIA_STATUS_3VEN | M_PCMCIA_STATUS_5VEN)) ?
		SS_POWERON : 0;
	val |= (status & M_PCMCIA_STATUS_RDY) ? SS_READY : 0;
	val |= (status & M_PCMCIA_STATUS_WP) ? SS_WRPROT : 0;
	val |= ((status & M_PCMCIA_STATUS_VS2) &&
		(~status & M_PCMCIA_STATUS_VS1)) ? SS_3VCARD : 0;
	val |= (status & (M_PCMCIA_STATUS_CDCHG | M_PCMCIA_STATUS_WPCHG
			  | M_PCMCIA_STATUS_RDYCHG)) ? SS_STSCHG : 0;
	/* XXXKW SS_INSERTION on cdchange? */

	DPRINTK(PFX "GetStatus(%d) = %x", lsock, val);
#if PCMCIA_DEBUG
	DPRINTK(" [config(0x%4.4x) status(0x%4.4x)]", config, status);
#endif
	DPRINTK("\n");

	*value = val;
	return 0;
}

/*====================================================================*/

static int sb1250pc_inquire_socket(unsigned int lsock, socket_cap_t *cap)
{
	*cap = sb1250pc_cap;

	DPRINTK(PFX "InquireSocket(%d) = features 0x%4.4x, irq_mask "
	       "0x%4.4x, map_size 0x%4.4x\n", lsock, cap->features,
	       cap->irq_mask, cap->map_size);

	return 0;
}

/*====================================================================*/

// This garbage function never seems to get called...
static int sb1250pc_get_socket(unsigned int lsock, socket_state_t *state)
{
	DPRINTK(PFX "Does GetSocket(%d) ever get called???", lsock);
	return -1;
} /* sb1250pc_get_socket */

/*====================================================================*/

static uint32_t sibyte_set_power(uint32_t config, int vcc)
{
	config &= ~(M_PCMCIA_CFG_3VEN | M_PCMCIA_CFG_5VEN);
	if (vcc == 33) {
		config |= M_PCMCIA_CFG_3VEN;
	} else if (vcc == 50) {
		config |= M_PCMCIA_CFG_5VEN;
	}

	sb1250pc_write_config(config);
	return config;
}

static int sb1250pc_set_socket(unsigned int lsock, socket_state_t *state)
{
	uint32_t config;

	DPRINTK(PFX "SetSocket(%d, flags %#3.3x, Vcc %d, Vpp %d, "
	       "io_irq %d, csc_mask %#2.2x)\n", lsock, state->flags,
	       state->Vcc, state->Vpp, state->io_irq, state->csc_mask);

	config = sb1250pc_read_config();

	config = sibyte_set_power(config, state->Vcc);

	if (state->flags & SS_DEBOUNCED)
		state->flags &= ~SS_DEBOUNCED; /* SS_DEBOUNCED is oneshot */
	/* XXXKW SS_OUTPUT_ENA? */
	/* XXXKW SS_PWR_AUTO? */
	/* XXXKW SS_IOCARD? */

	if (state->csc_mask & SS_DETECT)
		config &= ~M_PCMCIA_CFG_CDMASK;
	else
		config |= M_PCMCIA_CFG_CDMASK;

	config &= ~M_PCMCIA_CFG_RESET;
	if (state->flags & SS_RESET) {
		DPRINTK(PFX "  resetting PCMCIA\n");
		config |= M_PCMCIA_CFG_RESET;
	}
	sb1250pc_write_config(config);

	DPRINTK(PFX "  new config: %x\n", sb1250pc_read_config());

	return 0;
}

/*====================================================================*/

static int sb1250pc_get_io_map(unsigned int lsock, struct pccard_io_map *io)
{
	*io = sibyte_iomap[io->map];

	DPRINTK(PFX "GetIOMap(%d, %d) = %#2.2x, %d ns, "
	       "%#4.4x-%#4.4x\n", lsock, io->map, io->flags,
	       io->speed, io->start, io->stop);
	return 0;
} /* sb1250pc_get_io_map */

static int sb1250pc_set_io_map(unsigned int lsock, struct pccard_io_map *io)
{
   	unsigned int speed;
	unsigned long start;
	u32 config;

	/* SB1250 uses direct mapping */
	DPRINTK(PFX "SetIOMap(%d, %d, %#2.2x, %d ns, "
	       "%#4.4x-%#4.4x) called\n", lsock, io->map, io->flags,
	       io->speed, io->start, io->stop);

	if (io->map >= MAX_IO_WIN) {
		DPRINTK(KERN_ERR PFX "map (%d) out of range\n", io->map);
		return -1;
	}

	if (io->flags & MAP_ACTIVE) {
		speed = (io->speed > 0) ? io->speed : 255;
	}

	config = sb1250pc_read_config();

	if (io->flags & MAP_ATTRIB) {
		DPRINTK(PFX "  Setting pcmcia_cfg_reg to 1  (Attribute Mode)\n");
		config |= M_PCMCIA_CFG_ATTRMEM;
	} else {
		DPRINTK(PFX "  Setting pcmcia_cfg_reg to 0  (Data Mode)\n");
		config &= ~M_PCMCIA_CFG_ATTRMEM;
	}
	sb1250pc_write_config(config);

	start = io->start;

	if (io->stop == 1) {
		io->stop = PAGE_SIZE - 1;
	}

	if (io->start == 0)
		io->start = sb_pcmcia_base;

	io->stop = io->start + (io->stop - start);

	sibyte_iomap[io->map] = *io;

	DPRINTK(PFX "SetIOMap(%d, %d, %#2.2x, %d ns, "
	       "%#4.4x-%#4.4x) returns\n", lsock, io->map, io->flags,
	       io->speed, io->start, io->stop);
	return 0;
} /* sb1250pc_set_io_map */

/*====================================================================*/

static int sb1250pc_get_mem_map(unsigned int lsock, struct pccard_mem_map *mem)
{
	if(mem->map >= MAX_WIN)
		return -EINVAL;
	
	*mem = sibyte_memmap[mem->map];

	DPRINTK(PFX "GetMemMap(%d, mem[%d, %#2.2x, %d ns, "
	       "%#5.5lx-%#5.5lx, %#5.5x) called\n", lsock, mem->map, mem->flags,
	       mem->speed, mem->sys_start, mem->sys_stop, mem->card_start);

	return 0;
}

static int sb1250pc_set_mem_map(unsigned int lsock, struct pccard_mem_map *mem)
{
	u32 old_config, new_config;

	if (mem->map >= MAX_WIN) {
		DPRINTK(KERN_ERR PFX "map (%d) out of range\n", mem->map);
		return -1;
	}

	if (mem->sys_start == 0)
		mem->sys_start = mem->card_start + sb_pcmcia_base;

	if (mem->sys_stop == 0)
		mem->sys_stop = mem->sys_start + sb_pcmcia_size - 1;

	old_config = sb1250pc_read_config();

	DPRINTK(PFX "  Setting mem_map %p\n", mem);
	if (mem->flags & MAP_ATTRIB) {
		DPRINTK(PFX "  Setting pcmcia_cfg_reg to 1 (Attribute Mode)\n");
		new_config = old_config | M_PCMCIA_CFG_ATTRMEM;
	} else {
		DPRINTK(PFX "  Setting pcmcia_cfg_reg to 0 (Data Mode)\n");
		new_config = old_config & ~M_PCMCIA_CFG_ATTRMEM;
	}
	if (new_config != old_config)
		sb1250pc_write_config(new_config);

	sibyte_memmap[mem->map] = *mem;

	DPRINTK(PFX "SetMemMap(%d, mem[%d, %#2.2x, %d ns], "
	       "%#5.5lx-%#5.5lx, %#5.5x)\n", lsock, mem->map, mem->flags,
	       mem->speed, mem->sys_start, mem->sys_stop, mem->card_start);

	return 0;
}

/*====================================================================*/

#ifdef CONFIG_PROC_FS
/* sb1250pc_proc_status()
 * Implements the /proc/bus/pccard/??/status file.
 *
 * Returns: the number of characters added to the buffer
 *
 * Be aware that reading status clears the "change" bits; this seems
 * unlikely to bite us by making us miss interrupts.
 */
static int sb1250pc_proc_status(char *buf, char **start, off_t pos,
				int count, int *eof, void *data)
{
	char *p = buf;
	u32 addr, temp;
	u32 status, config;
	//unsigned int sock = (unsigned int) data;

	config = sb1250pc_read_config();
	status = sb1250pc_read_status();
	p += sprintf(p, "config(0x%4.4x) status(0x%4.4x)\n", config, status);

	for (addr = sb_pcmcia_base; addr < sb_pcmcia_base + 8; addr+=4) {
		temp = READ_PHYSADDR(addr);
		p += sprintf(p, " Looking up addr 0x%x: 0x%8.8x\n", addr, temp);
	}

	return p - buf;
}

static void sb1250pc_proc_setup(unsigned int sock, struct proc_dir_entry *base)
{
	struct proc_dir_entry *entry;
	
	if ((entry = create_proc_entry("sb1250pc", 0, base)) == NULL) {
		printk(KERN_ERR PFX "Unable to install \"sb1250pc\" procfs entry\n");
		return;
	} else
		printk(KERN_INFO PFX "Setting up \"sb1250pc\" procfs entry\n");
	
	entry->read_proc = sb1250pc_proc_status;
	entry->data = (void *) sock;
}

#endif

/*====================================================================*/

static int sibyte_pcmcia_initted = 0;

static int sb1250pc_init(unsigned int s)
{
	u32 config, status;
	
	DPRINTK(PFX "Initializing SB1250 PCMCIA:\n");

	/* Read status to clear interrupt sources */
	status = sb1250pc_read_status();

	/*
	 * Before getting setting up the IRQ, set the config:
	 *   reset off, auto-power off
	 *   interrupt mask: WP, CD, Ready off
	 */
	config = sb1250pc_read_config();
	config = M_PCMCIA_CFG_CDMASK | M_PCMCIA_CFG_WPMASK | M_PCMCIA_CFG_RDYMASK;
	sb1250pc_write_config(config);
	   
	if (!sibyte_pcmcia_initted) {
		uint32_t gpio_ctrl;
		/* Set up the GPIO for PC_READY for use in ide-cs */
		gpio_ctrl = READ_CSR32(A_GPIO_INT_TYPE);
		gpio_ctrl &= ~M_GPIO_INTR_TYPEX(K_GPIO_PC_READY);
		gpio_ctrl |= V_GPIO_INTR_TYPEX(K_GPIO_PC_READY, K_GPIO_INTR_EDGE);
		WRITE_CSR32(gpio_ctrl, A_GPIO_INT_TYPE);
		WRITE_CSR32(1 << K_GPIO_PC_READY, A_GPIO_CLR_EDGE);

		/* Invert to get busy->ready transition */
		gpio_ctrl = READ_CSR32(A_GPIO_INPUT_INVERT);
		gpio_ctrl |= 1 << K_GPIO_PC_READY;
		WRITE_CSR32(gpio_ctrl, A_GPIO_INPUT_INVERT);

		/* Should not be any pending since we masked all sources */
		if (request_irq(cs_irq, sb1250pc_interrupt, 0, "pcmcia", NULL))
			return -ENODEV;
		DPRINTK(PFX "  IRQ %d registered\n", cs_irq);

		sibyte_pcmcia_initted = 1;
	}
	
	status = sb1250pc_read_status();
	DPRINTK(PFX "  config(0x%4.4x) status(0x%4.4x)\n", config, status);

	sb1250pc_set_socket(s, &dead_socket);

	return 0;
}

static int sb1250pc_suspend(unsigned int sock)
{
	free_irq(cs_irq, NULL);
	DPRINTK(KERN_INFO PFX "  IRQ %d freed\n", cs_irq);
	return sb1250pc_set_socket(sock, &dead_socket);
}

static struct pccard_operations sb1250pc_operations = {
	sb1250pc_init,
	sb1250pc_suspend,
	sb1250pc_register_callback,
	sb1250pc_inquire_socket,
	sb1250pc_get_status,
	sb1250pc_get_socket,
	sb1250pc_set_socket,
	sb1250pc_get_io_map,
	sb1250pc_set_io_map,
	sb1250pc_get_mem_map,
	sb1250pc_set_mem_map,
#ifdef CONFIG_PROC_FS
	sb1250pc_proc_setup
#endif
};

/*
 * XXXKW This is a hack.  The ide-cs stuff seems to leave us in
 * Attribute mode.  Since I know that a SELECT_DRIVE will happen as
 * the first I/O access, use this opportunity to enter data mode.
 */
static void sibyte_pcmcia_selectproc(ide_drive_t *drive)
{
	sb1250pc_write_config(sb1250pc_read_config() & ~M_PCMCIA_CFG_ATTRMEM);
}

static int sibyte_pc_prep_ide(void)
{
	int i;
	ide_hwif_t *hwif = NULL;

	/* Stake early claim on an ide_hwif */
	for (i = 0; i < MAX_HWIFS; i++) {
		if (!ide_hwifs[i].io_ports[IDE_DATA_OFFSET]) {
			hwif = &ide_hwifs[i];
			break;
		}
	}
	if (hwif == NULL) {
		printk("No space for SiByte onboard PCMCIA driver in ide_hwifs[].  Not enabled.\n");
		return 1;
	}

	/*
	 * Prime the hwif with port values, so when a card is
	 * detected, the 'io_offset' from the capabilities will lead
	 * it here
	 */
	hwif->hw.io_ports[IDE_DATA_OFFSET]    = SIBYTE_CS_REG(0);
	hwif->hw.io_ports[IDE_ERROR_OFFSET]   = SIBYTE_CS_REG(1);
	hwif->hw.io_ports[IDE_NSECTOR_OFFSET] = SIBYTE_CS_REG(2);
	hwif->hw.io_ports[IDE_SECTOR_OFFSET]  = SIBYTE_CS_REG(3);
	hwif->hw.io_ports[IDE_LCYL_OFFSET]    = SIBYTE_CS_REG(4);
	hwif->hw.io_ports[IDE_HCYL_OFFSET]    = SIBYTE_CS_REG(5);
	hwif->hw.io_ports[IDE_SELECT_OFFSET]  = SIBYTE_CS_REG(6);
	hwif->hw.io_ports[IDE_STATUS_OFFSET]  = SIBYTE_CS_REG(7);
	hwif->hw.io_ports[IDE_CONTROL_OFFSET] = SIBYTE_CS_REG(6); /* XXXKW ? */
	hwif->hw.ack_intr                     = sb_pcmcia_ack_intr;
	hwif->selectproc                      = sibyte_pcmcia_selectproc;
	hwif->hold                            = 1;
	hwif->mmio                            = 2;
	sibyte_set_ideops(&ide_hwifs[i]);

	printk("SiByte onboard PCMCIA-IDE configured as device %i\n", i);

	return 0;
}

static int __init sibyte_pcmcia_init(void)
{
	servinfo_t info;
	u32 addr, temp;
	u64 cfg;

	CardServices(GetCardServicesInfo, &info);
	if (info.Revision != CS_RELEASE_CODE) {
		printk(KERN_ERR PFX "Card Services release does not match!\n");
		return -1;
	}

        cfg = in64(IO_SPACE_BASE + A_SCD_SYSTEM_CFG);
        if (!(cfg & M_SYS_PCMCIA_ENABLE)) {
		printk(KERN_INFO PFX "chip not configured for PCMCIA\n");
		return -1;
	}

	/* Find memory base address and size */
	addr = A_IO_EXT_REG(R_IO_EXT_REG(R_IO_EXT_MULT_SIZE, PCMCIA_CS));
	temp = G_IO_MULT_SIZE(csr_in32(IO_SPACE_BASE + addr));
	printk(PFX "Looking up addr 0x%x: 0x%4.4x (IO size)\n", addr, temp);
	sb_pcmcia_size = (temp+1) << S_IO_REGSIZE;

	addr = A_IO_EXT_REG(R_IO_EXT_REG(R_IO_EXT_START_ADDR, PCMCIA_CS));
	temp = G_IO_START_ADDR(csr_in32(KSEG1|addr));
	printk(PFX "Looking up addr 0x%x: 0x%4.4x (IO Base Address)\n", addr, temp);
	if (temp << S_IO_ADDRBASE != PCMCIA_PHYS)
		panic(PFX "pcmcia base doesn't match gencs value\n");

	/* check and request memory region */
	if (check_mem_region(sb_pcmcia_base, sb_pcmcia_size)) {
		printk(KERN_ERR PFX "Can't request memory region?\n");
	} else {
		request_mem_region(sb_pcmcia_base, sb_pcmcia_size, "sibyte-pcmcia");
		printk(PFX "Memory region 0x%8.8lx of size 0x%8.8lx requested.\n",
		       sb_pcmcia_base, sb_pcmcia_size);
	}

	/* register with socket services */
	if (register_ss_entry(1, &sb1250pc_operations)) {
		printk(KERN_ERR PFX "register_ss_entry() failed\n");
		release_region(sb_pcmcia_base, sb_pcmcia_size);
		return -ENODEV;
	}

	if (!sibyte_pc_prep_ide()) {
		/* XXXKW hack for ide-cs warning squash */
		request_region(sb1250pc_cap.io_offset, 16, "ide-cs");
		request_mem_region(SIBYTE_CS_REG(0), 8, "sibyte-ide-cs");
	}

	return 0;
}

static void __exit sibyte_pcmcia_exit(void)
{
	/* XXXKW untested as module */
	unregister_ss_entry(&sb1250pc_operations);
	release_region(sb_pcmcia_base, sb_pcmcia_size);
}

module_init(sibyte_pcmcia_init);
module_exit(sibyte_pcmcia_exit);
