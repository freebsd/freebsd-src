/*
 * Regular lowlevel cardbus driver ("yenta")
 *
 * (C) Copyright 1999, 2000 Linus Torvalds
 *
 * Changelog:
 * Aug 2002: Manfred Spraul <manfred@colorfullife.com>
 * 	Dynamically adjust the size of the bridge resource
 * 	
 */
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/cs.h>

#include <asm/io.h>

#include "yenta.h"
#include "i82365.h"

#if 0
#define DEBUG(x,args...)	printk(KERN_DEBUG __FUNCTION__ ": " x,##args)
#else
#define DEBUG(x,args...)
#endif

/* Don't ask.. */
#define to_cycles(ns)	((ns)/120)
#define to_ns(cycles)	((cycles)*120)

/*
 * Generate easy-to-use ways of reading a cardbus sockets
 * regular memory space ("cb_xxx"), configuration space
 * ("config_xxx") and compatibility space ("exca_xxxx")
 */
static inline u32 cb_readl(pci_socket_t *socket, unsigned reg)
{
	u32 val = readl(socket->base + reg);
	DEBUG("%p %04x %08x\n", socket, reg, val);
	return val;
}

static inline void cb_writel(pci_socket_t *socket, unsigned reg, u32 val)
{
	DEBUG("%p %04x %08x\n", socket, reg, val);
	writel(val, socket->base + reg);
}

static inline u8 config_readb(pci_socket_t *socket, unsigned offset)
{
	u8 val;
	pci_read_config_byte(socket->dev, offset, &val);
	DEBUG("%p %04x %02x\n", socket, offset, val);
	return val;
}

static inline u16 config_readw(pci_socket_t *socket, unsigned offset)
{
	u16 val;
	pci_read_config_word(socket->dev, offset, &val);
	DEBUG("%p %04x %04x\n", socket, offset, val);
	return val;
}

static inline u32 config_readl(pci_socket_t *socket, unsigned offset)
{
	u32 val;
	pci_read_config_dword(socket->dev, offset, &val);
	DEBUG("%p %04x %08x\n", socket, offset, val);
	return val;
}

static inline void config_writeb(pci_socket_t *socket, unsigned offset, u8 val)
{
	DEBUG("%p %04x %02x\n", socket, offset, val);
	pci_write_config_byte(socket->dev, offset, val);
}

static inline void config_writew(pci_socket_t *socket, unsigned offset, u16 val)
{
	DEBUG("%p %04x %04x\n", socket, offset, val);
	pci_write_config_word(socket->dev, offset, val);
}

static inline void config_writel(pci_socket_t *socket, unsigned offset, u32 val)
{
	DEBUG("%p %04x %08x\n", socket, offset, val);
	pci_write_config_dword(socket->dev, offset, val);
}

static inline u8 exca_readb(pci_socket_t *socket, unsigned reg)
{
	u8 val = readb(socket->base + 0x800 + reg);
	DEBUG("%p %04x %02x\n", socket, reg, val);
	return val;
}

static inline u8 exca_readw(pci_socket_t *socket, unsigned reg)
{
	u16 val;
	val = readb(socket->base + 0x800 + reg);
	val |= readb(socket->base + 0x800 + reg + 1) << 8;
	DEBUG("%p %04x %04x\n", socket, reg, val);
	return val;
}

static inline void exca_writeb(pci_socket_t *socket, unsigned reg, u8 val)
{
	DEBUG("%p %04x %02x\n", socket, reg, val);
	writeb(val, socket->base + 0x800 + reg);
}

static void exca_writew(pci_socket_t *socket, unsigned reg, u16 val)
{
	DEBUG("%p %04x %04x\n", socket, reg, val);
	writeb(val, socket->base + 0x800 + reg);
	writeb(val >> 8, socket->base + 0x800 + reg + 1);
}

/*
 * Ugh, mixed-mode cardbus and 16-bit pccard state: things depend
 * on what kind of card is inserted..
 */
static int yenta_get_status(pci_socket_t *socket, unsigned int *value)
{
	unsigned int val;
	u32 state = cb_readl(socket, CB_SOCKET_STATE);

	val  = (state & CB_3VCARD) ? SS_3VCARD : 0;
	val |= (state & CB_XVCARD) ? SS_XVCARD : 0;
	val |= (state & (CB_CDETECT1 | CB_CDETECT2 | CB_5VCARD | CB_3VCARD
			 | CB_XVCARD | CB_YVCARD)) ? 0 : SS_PENDING;

	if (state & CB_CBCARD) {
		val |= SS_CARDBUS;	
		val |= (state & CB_CARDSTS) ? SS_STSCHG : 0;
		val |= (state & (CB_CDETECT1 | CB_CDETECT2)) ? 0 : SS_DETECT;
		val |= (state & CB_PWRCYCLE) ? SS_POWERON | SS_READY : 0;
	} else {
		u8 status = exca_readb(socket, I365_STATUS);
		val |= ((status & I365_CS_DETECT) == I365_CS_DETECT) ? SS_DETECT : 0;
		if (exca_readb(socket, I365_INTCTL) & I365_PC_IOCARD) {
			val |= (status & I365_CS_STSCHG) ? 0 : SS_STSCHG;
		} else {
			val |= (status & I365_CS_BVD1) ? 0 : SS_BATDEAD;
			val |= (status & I365_CS_BVD2) ? 0 : SS_BATWARN;
		}
		val |= (status & I365_CS_WRPROT) ? SS_WRPROT : 0;
		val |= (status & I365_CS_READY) ? SS_READY : 0;
		val |= (status & I365_CS_POWERON) ? SS_POWERON : 0;
	}

	*value = val;
	return 0;
}

static int yenta_Vcc_power(u32 control)
{
	switch (control & CB_SC_VCC_MASK) {
	case CB_SC_VCC_5V: return 50;
	case CB_SC_VCC_3V: return 33;
	default: return 0;
	}
}

static int yenta_Vpp_power(u32 control)
{
	switch (control & CB_SC_VPP_MASK) {
	case CB_SC_VPP_12V: return 120;
	case CB_SC_VPP_5V: return 50;
	case CB_SC_VPP_3V: return 33;
	default: return 0;
	}
}

static int yenta_get_socket(pci_socket_t *socket, socket_state_t *state)
{
	u8 reg;
	u32 control;

	control = cb_readl(socket, CB_SOCKET_CONTROL);

	state->Vcc = yenta_Vcc_power(control);
	state->Vpp = yenta_Vpp_power(control);
	state->io_irq = socket->io_irq;

	if (cb_readl(socket, CB_SOCKET_STATE) & CB_CBCARD) {
		u16 bridge = config_readw(socket, CB_BRIDGE_CONTROL);
		if (bridge & CB_BRIDGE_CRST)
			state->flags |= SS_RESET;
		return 0;
	}

	/* 16-bit card state.. */
	reg = exca_readb(socket, I365_POWER);
	state->flags = (reg & I365_PWR_AUTO) ? SS_PWR_AUTO : 0;
	state->flags |= (reg & I365_PWR_OUT) ? SS_OUTPUT_ENA : 0;

	reg = exca_readb(socket, I365_INTCTL);
	state->flags |= (reg & I365_PC_RESET) ? 0 : SS_RESET;
	state->flags |= (reg & I365_PC_IOCARD) ? SS_IOCARD : 0;

	reg = exca_readb(socket, I365_CSCINT);
	state->csc_mask = (reg & I365_CSC_DETECT) ? SS_DETECT : 0;
	if (state->flags & SS_IOCARD) {
		state->csc_mask |= (reg & I365_CSC_STSCHG) ? SS_STSCHG : 0;
	} else {
		state->csc_mask |= (reg & I365_CSC_BVD1) ? SS_BATDEAD : 0;
		state->csc_mask |= (reg & I365_CSC_BVD2) ? SS_BATWARN : 0;
		state->csc_mask |= (reg & I365_CSC_READY) ? SS_READY : 0;
	}

	return 0;
}

static void yenta_set_power(pci_socket_t *socket, socket_state_t *state)
{
	u32 reg = 0;	/* CB_SC_STPCLK? */
	switch (state->Vcc) {
	case 33: reg = CB_SC_VCC_3V; break;
	case 50: reg = CB_SC_VCC_5V; break;
	default: reg = 0; break;
	}
	switch (state->Vpp) {
	case 33:  reg |= CB_SC_VPP_3V; break;
	case 50:  reg |= CB_SC_VPP_5V; break;
	case 120: reg |= CB_SC_VPP_12V; break;
	}
	if (reg != cb_readl(socket, CB_SOCKET_CONTROL))
		cb_writel(socket, CB_SOCKET_CONTROL, reg);
}

static int yenta_set_socket(pci_socket_t *socket, socket_state_t *state)
{
	u16 bridge;

	if (state->flags & SS_DEBOUNCED) {
		/* The insertion debounce period has ended.  Clear any pending insertion events */
		socket->events &= ~SS_DETECT;
		state->flags &= ~SS_DEBOUNCED;		/* SS_DEBOUNCED is oneshot */
	}
	yenta_set_power(socket, state);
	socket->io_irq = state->io_irq;
	bridge = config_readw(socket, CB_BRIDGE_CONTROL) & ~(CB_BRIDGE_CRST | CB_BRIDGE_INTR);
	if (cb_readl(socket, CB_SOCKET_STATE) & CB_CBCARD) {
		u8 intr;
		bridge |= (state->flags & SS_RESET) ? CB_BRIDGE_CRST : 0;

		/* ISA interrupt control? */
		intr = exca_readb(socket, I365_INTCTL);
		intr = (intr & ~0xf);
		if (!socket->cb_irq) {
			intr |= state->io_irq;
			bridge |= CB_BRIDGE_INTR;
		}
		exca_writeb(socket, I365_INTCTL, intr);
	}  else {
		u8 reg;

		reg = exca_readb(socket, I365_INTCTL) & (I365_RING_ENA | I365_INTR_ENA);
		reg |= (state->flags & SS_RESET) ? 0 : I365_PC_RESET;
		reg |= (state->flags & SS_IOCARD) ? I365_PC_IOCARD : 0;
		if (state->io_irq != socket->cb_irq) {
			reg |= state->io_irq;
			bridge |= CB_BRIDGE_INTR;
		}
		exca_writeb(socket, I365_INTCTL, reg);

		reg = exca_readb(socket, I365_POWER) & (I365_VCC_MASK|I365_VPP1_MASK);
		reg |= I365_PWR_NORESET;
		if (state->flags & SS_PWR_AUTO) reg |= I365_PWR_AUTO;
		if (state->flags & SS_OUTPUT_ENA) reg |= I365_PWR_OUT;
		if (exca_readb(socket, I365_POWER) != reg)
			exca_writeb(socket, I365_POWER, reg);

		/* CSC interrupt: no ISA irq for CSC */
		reg = I365_CSC_DETECT;
		if (state->flags & SS_IOCARD) {
			if (state->csc_mask & SS_STSCHG) reg |= I365_CSC_STSCHG;
		} else {
			if (state->csc_mask & SS_BATDEAD) reg |= I365_CSC_BVD1;
			if (state->csc_mask & SS_BATWARN) reg |= I365_CSC_BVD2;
			if (state->csc_mask & SS_READY) reg |= I365_CSC_READY;
		}
		exca_writeb(socket, I365_CSCINT, reg);
		exca_readb(socket, I365_CSC);
	
		if(socket->zoom_video)
			socket->zoom_video(socket, state->flags & SS_ZVCARD);
	}
	config_writew(socket, CB_BRIDGE_CONTROL, bridge);
	/* Socket event mask: get card insert/remove events.. */
	cb_writel(socket, CB_SOCKET_EVENT, -1);
	cb_writel(socket, CB_SOCKET_MASK, CB_CDMASK);
	return 0;
}

static int yenta_get_io_map(pci_socket_t *socket, struct pccard_io_map *io)
{
	int map;
	unsigned char ioctl, addr;

	map = io->map;
	if (map > 1)
		return -EINVAL;

	io->start = exca_readw(socket, I365_IO(map)+I365_W_START);
	io->stop = exca_readw(socket, I365_IO(map)+I365_W_STOP);

	ioctl = exca_readb(socket, I365_IOCTL);
	addr = exca_readb(socket, I365_ADDRWIN);
	io->speed = to_ns(ioctl & I365_IOCTL_WAIT(map)) ? 1 : 0;
	io->flags  = (addr & I365_ENA_IO(map)) ? MAP_ACTIVE : 0;
	io->flags |= (ioctl & I365_IOCTL_0WS(map)) ? MAP_0WS : 0;
	io->flags |= (ioctl & I365_IOCTL_16BIT(map)) ? MAP_16BIT : 0;
	io->flags |= (ioctl & I365_IOCTL_IOCS16(map)) ? MAP_AUTOSZ : 0;

	return 0;
}

static int yenta_set_io_map(pci_socket_t *socket, struct pccard_io_map *io)
{
	int map;
	unsigned char ioctl, addr, enable;

	map = io->map;

	if (map > 1)
		return -EINVAL;

	enable = I365_ENA_IO(map);
	addr = exca_readb(socket, I365_ADDRWIN);

	/* Disable the window before changing it.. */
	if (addr & enable) {
		addr &= ~enable;
		exca_writeb(socket, I365_ADDRWIN, addr);
	}

	exca_writew(socket, I365_IO(map)+I365_W_START, io->start);
	exca_writew(socket, I365_IO(map)+I365_W_STOP, io->stop);

	ioctl = exca_readb(socket, I365_IOCTL) & ~I365_IOCTL_MASK(map);
	if (io->flags & MAP_0WS) ioctl |= I365_IOCTL_0WS(map);
	if (io->flags & MAP_16BIT) ioctl |= I365_IOCTL_16BIT(map);
	if (io->flags & MAP_AUTOSZ) ioctl |= I365_IOCTL_IOCS16(map);
	exca_writeb(socket, I365_IOCTL, ioctl);

	if (io->flags & MAP_ACTIVE)
		exca_writeb(socket, I365_ADDRWIN, addr | enable);
	return 0;
}

static int yenta_get_mem_map(pci_socket_t *socket, struct pccard_mem_map *mem)
{
	int map;
	unsigned char addr;
	unsigned int start, stop, page, offset;

	map = mem->map;
	if (map > 4)
		return -EINVAL;

	addr = exca_readb(socket, I365_ADDRWIN);
	mem->flags = (addr & I365_ENA_MEM(map)) ? MAP_ACTIVE : 0;

	start = exca_readw(socket, I365_MEM(map) + I365_W_START);
	mem->flags |= (start & I365_MEM_16BIT) ? MAP_16BIT : 0;
	mem->flags |= (start & I365_MEM_0WS) ? MAP_0WS : 0;
	start = (start & 0x0fff) << 12;

	stop = exca_readw(socket, I365_MEM(map) + I365_W_STOP);
	mem->speed = to_ns(stop >> 14);
	stop = ((stop & 0x0fff) << 12) + 0x0fff;

	offset = exca_readw(socket, I365_MEM(map) + I365_W_OFF);
	mem->flags |= (offset & I365_MEM_WRPROT) ? MAP_WRPROT : 0;
	mem->flags |= (offset & I365_MEM_REG) ? MAP_ATTRIB : 0;
	offset = ((offset & 0x3fff) << 12) + start;
	mem->card_start = offset & 0x3ffffff;

	page = exca_readb(socket, CB_MEM_PAGE(map)) << 24;
	mem->sys_start = start + page;
	mem->sys_stop = start + page;

	return 0;
}

static int yenta_set_mem_map(pci_socket_t *socket, struct pccard_mem_map *mem)
{
	int map;
	unsigned char addr, enable;
	unsigned int start, stop, card_start;
	unsigned short word;

	map = mem->map;
	start = mem->sys_start;
	stop = mem->sys_stop;
	card_start = mem->card_start;

	if (map > 4 || start > stop || ((start ^ stop) >> 24) ||
	    (card_start >> 26) || mem->speed > 1000)
		return -EINVAL;

	enable = I365_ENA_MEM(map);
	addr = exca_readb(socket, I365_ADDRWIN);
	if (addr & enable) {
		addr &= ~enable;
		exca_writeb(socket, I365_ADDRWIN, addr);
	}

	exca_writeb(socket, CB_MEM_PAGE(map), start >> 24);

	word = (start >> 12) & 0x0fff;
	if (mem->flags & MAP_16BIT)
		word |= I365_MEM_16BIT;
	if (mem->flags & MAP_0WS)
		word |= I365_MEM_0WS;
	exca_writew(socket, I365_MEM(map) + I365_W_START, word);

	word = (stop >> 12) & 0x0fff;
	switch (to_cycles(mem->speed)) {
		case 0: break;
		case 1:  word |= I365_MEM_WS0; break;
		case 2:  word |= I365_MEM_WS1; break;
		default: word |= I365_MEM_WS1 | I365_MEM_WS0; break;
	}
	exca_writew(socket, I365_MEM(map) + I365_W_STOP, word);

	word = ((card_start - start) >> 12) & 0x3fff;
	if (mem->flags & MAP_WRPROT)
		word |= I365_MEM_WRPROT;
	if (mem->flags & MAP_ATTRIB)
		word |= I365_MEM_REG;
	exca_writew(socket, I365_MEM(map) + I365_W_OFF, word);

	if (mem->flags & MAP_ACTIVE)
		exca_writeb(socket, I365_ADDRWIN, addr | enable);
	return 0;
}

static void yenta_proc_setup(pci_socket_t *socket, struct proc_dir_entry *base)
{
	/* Not done yet */
}

static unsigned int yenta_events(pci_socket_t *socket)
{
	u8 csc;
	u32 cb_event;
	unsigned int events;

	/* Clear interrupt status for the event */
	cb_event = cb_readl(socket, CB_SOCKET_EVENT);
	cb_writel(socket, CB_SOCKET_EVENT, cb_event);

	csc = exca_readb(socket, I365_CSC);

	events = (cb_event & (CB_CD1EVENT | CB_CD2EVENT)) ? SS_DETECT : 0 ;
	events |= (csc & I365_CSC_DETECT) ? SS_DETECT : 0;
	if (exca_readb(socket, I365_INTCTL) & I365_PC_IOCARD) {
		events |= (csc & I365_CSC_STSCHG) ? SS_STSCHG : 0;
	} else {
		events |= (csc & I365_CSC_BVD1) ? SS_BATDEAD : 0;
		events |= (csc & I365_CSC_BVD2) ? SS_BATWARN : 0;
		events |= (csc & I365_CSC_READY) ? SS_READY : 0;
	}
	return events;
}


static void yenta_bh(void *data)
{
	pci_socket_t *socket = data;
	unsigned int events;

	spin_lock_irq(&socket->event_lock);
	events = socket->events;
	socket->events = 0;
	spin_unlock_irq(&socket->event_lock);
	if (socket->handler)
		socket->handler(socket->info, events);
}

static void yenta_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int events;
	pci_socket_t *socket = (pci_socket_t *) dev_id;

	events = yenta_events(socket);
	if (events) {
		spin_lock(&socket->event_lock);
		socket->events |= events;
		spin_unlock(&socket->event_lock);
		schedule_task(&socket->tq_task);
	}
}

static void yenta_interrupt_wrapper(unsigned long data)
{
	pci_socket_t *socket = (pci_socket_t *) data;

	yenta_interrupt(0, (void *)socket, NULL);
	socket->poll_timer.expires = jiffies + HZ;
	add_timer(&socket->poll_timer);
}

/*
 * Only probe "regular" interrupts, don't
 * touch dangerous spots like the mouse irq,
 * because there are mice that apparently
 * get really confused if they get fondled
 * too intimately.
 *
 * Default to 11, 10, 9, 7, 6, 5, 4, 3.
 */
static u32 isa_interrupts = 0x0ef8;

static unsigned int yenta_probe_irq(pci_socket_t *socket, u32 isa_irq_mask)
{
	int i;
	unsigned long val;
	u16 bridge_ctrl;
	u32 mask;

	/* Set up ISA irq routing to probe the ISA irqs.. */
	bridge_ctrl = config_readw(socket, CB_BRIDGE_CONTROL);
	if (!(bridge_ctrl & CB_BRIDGE_INTR)) {
		bridge_ctrl |= CB_BRIDGE_INTR;
		config_writew(socket, CB_BRIDGE_CONTROL, bridge_ctrl);
	}

	/*
	 * Probe for usable interrupts using the force
	 * register to generate bogus card status events.
	 */
	cb_writel(socket, CB_SOCKET_EVENT, -1);
	cb_writel(socket, CB_SOCKET_MASK, CB_CSTSMASK);
	exca_writeb(socket, I365_CSCINT, 0);
	val = probe_irq_on() & isa_irq_mask;
	for (i = 1; i < 16; i++) {
		if (!((val >> i) & 1))
			continue;
		exca_writeb(socket, I365_CSCINT, I365_CSC_STSCHG | (i << 4));
		cb_writel(socket, CB_SOCKET_FORCE, CB_FCARDSTS);
		udelay(100);
		cb_writel(socket, CB_SOCKET_EVENT, -1);
	}
	cb_writel(socket, CB_SOCKET_MASK, 0);
	exca_writeb(socket, I365_CSCINT, 0);
	
	mask = probe_irq_mask(val) & 0xffff;

	bridge_ctrl &= ~CB_BRIDGE_INTR;
	config_writew(socket, CB_BRIDGE_CONTROL, bridge_ctrl);

	return mask;
}

/*
 * Set static data that doesn't need re-initializing..
 */
static void yenta_get_socket_capabilities(pci_socket_t *socket, u32 isa_irq_mask)
{
	socket->cap.features |= SS_CAP_PAGE_REGS | SS_CAP_PCCARD | SS_CAP_CARDBUS;
	socket->cap.map_size = 0x1000;
	socket->cap.pci_irq = socket->cb_irq;
	socket->cap.irq_mask = yenta_probe_irq(socket, isa_irq_mask);
	socket->cap.cb_dev = socket->dev;
	socket->cap.bus = NULL;

	printk(KERN_INFO "Yenta ISA IRQ mask 0x%04x, PCI irq %d\n",
	       socket->cap.irq_mask, socket->cb_irq);
}

extern void cardbus_register(pci_socket_t *socket);

/*
 * 'Bottom half' for the yenta_open routine. Allocate the interrupt line
 *  and register the socket with the upper layers.
 */
static void yenta_open_bh(void * data)
{
	pci_socket_t * socket = (pci_socket_t *) data;

	/* It's OK to overwrite this now */
	socket->tq_task.routine = yenta_bh;

	if (!socket->cb_irq || request_irq(socket->cb_irq, yenta_interrupt, SA_SHIRQ, socket->dev->name, socket)) {
		/* No IRQ or request_irq failed. Poll */
		socket->cb_irq = 0; /* But zero is a valid IRQ number. */
		socket->poll_timer.function = yenta_interrupt_wrapper;
		socket->poll_timer.data = (unsigned long)socket;
		socket->poll_timer.expires = jiffies + HZ;
		add_timer(&socket->poll_timer);
	}

	/* Figure out what the dang thing can do for the PCMCIA layer... */
	yenta_get_socket_capabilities(socket, isa_interrupts);
	printk(KERN_INFO "Socket status: %08x\n",
	       cb_readl(socket, CB_SOCKET_STATE));

	/* Register it with the pcmcia layer.. */
	cardbus_register(socket);

	MOD_DEC_USE_COUNT;
}

static void yenta_clear_maps(pci_socket_t *socket)
{
	int i;
	pccard_io_map io = { 0, 0, 0, 0, 1 };
	pccard_mem_map mem = { 0, 0, 0, 0, 0, 0 };

	mem.sys_stop = 0x0fff;
	yenta_set_socket(socket, &dead_socket);
	for (i = 0; i < 2; i++) {
		io.map = i;
		yenta_set_io_map(socket, &io);
	}
	for (i = 0; i < 5; i++) {
		mem.map = i;
		yenta_set_mem_map(socket, &mem);
	}
}

/*
 * Initialize the standard cardbus registers
 */
static void yenta_config_init(pci_socket_t *socket)
{
	u32 state;
	u16 bridge;
	struct pci_dev *dev = socket->dev;

	pci_set_power_state(socket->dev, 0);

	config_writel(socket, CB_LEGACY_MODE_BASE, 0);
	config_writel(socket, PCI_BASE_ADDRESS_0, dev->resource[0].start);
	config_writew(socket, PCI_COMMAND,
			PCI_COMMAND_IO |
			PCI_COMMAND_MEMORY |
			PCI_COMMAND_MASTER |
			PCI_COMMAND_WAIT);

	/* MAGIC NUMBERS! Fixme */
	config_writeb(socket, PCI_CACHE_LINE_SIZE, L1_CACHE_BYTES / 4);
	config_writeb(socket, PCI_LATENCY_TIMER, 168);
	config_writel(socket, PCI_PRIMARY_BUS,
		(176 << 24) |			   /* sec. latency timer */
		(dev->subordinate->subordinate << 16) | /* subordinate bus */
		(dev->subordinate->secondary << 8) |  /* secondary bus */
		dev->subordinate->primary);		   /* primary bus */

	/*
	 * Set up the bridging state:
	 *  - enable write posting.
	 *  - memory window 0 prefetchable, window 1 non-prefetchable
	 *  - PCI interrupts enabled if a PCI interrupt exists..
	 */
	bridge = config_readw(socket, CB_BRIDGE_CONTROL);
	bridge &= ~(CB_BRIDGE_CRST | CB_BRIDGE_PREFETCH1 | CB_BRIDGE_INTR | CB_BRIDGE_ISAEN | CB_BRIDGE_VGAEN);
	bridge |= CB_BRIDGE_PREFETCH0 | CB_BRIDGE_POSTEN;
	if (!socket->cb_irq)
		bridge |= CB_BRIDGE_INTR;
	config_writew(socket, CB_BRIDGE_CONTROL, bridge);

	exca_writeb(socket, I365_GBLCTL, 0x00);
	exca_writeb(socket, I365_GENCTL, 0x00);

	/* Redo card voltage interrogation */
	state = cb_readl(socket, CB_SOCKET_STATE);
	if (!(state & (CB_CDETECT1 | CB_CDETECT2 | CB_5VCARD |
			CB_3VCARD | CB_XVCARD | CB_YVCARD)))
		
	cb_writel(socket, CB_SOCKET_FORCE, CB_CVSTEST);
}

/* Called at resume and initialization events */
static int yenta_init(pci_socket_t *socket)
{
	yenta_config_init(socket);
	yenta_clear_maps(socket);

	/* Re-enable interrupts */
	cb_writel(socket, CB_SOCKET_MASK, CB_CDMASK);
	return 0;
}

static int yenta_suspend(pci_socket_t *socket)
{
	yenta_set_socket(socket, &dead_socket);

	/* Disable interrupts */
	cb_writel(socket, CB_SOCKET_MASK, 0x0);

	/*
	 * This does not work currently. The controller
	 * loses too much information during D3 to come up
	 * cleanly. We should probably fix yenta_init()
	 * to update all the critical registers, notably
	 * the IO and MEM bridging region data.. That is
	 * something that pci_set_power_state() should
	 * probably know about bridges anyway.
	 *
	pci_set_power_state(socket->dev, 3);
	 */

	return 0;
}

/*
 * Use an adaptive allocation for the memory resource,
 * sometimes the size behind pci bridges is limited:
 * 1/8 of the size of the io window of the parent.
 * max 4 MB, min 16 kB.
 */
#define BRIDGE_SIZE_MAX	4*1024*1024
#define BRIDGE_SIZE_MIN	16*1024

static void yenta_allocate_res(pci_socket_t *socket, int nr, unsigned type)
{
	struct pci_bus *bus;
	struct resource *root, *res;
	u32 start, end;
	u32 align, size, min, max;
	unsigned offset;
	unsigned mask;

	/* The granularity of the memory limit is 4kB, on IO it's 4 bytes */
	mask = ~0xfff;
	if (type & IORESOURCE_IO)
		mask = ~3;

	offset = 0x1c + 8*nr;
	bus = socket->dev->subordinate;
	res = socket->dev->resource + PCI_BRIDGE_RESOURCES + nr;
	res->name = bus->name;
	res->flags = type;
	res->start = 0;
	res->end = 0;
	root = pci_find_parent_resource(socket->dev, res);

	if (!root)
		return;

	start = config_readl(socket, offset) & mask;
	end = config_readl(socket, offset+4) | ~mask;
	if (start && end > start) {
		res->start = start;
		res->end = end;
		if (request_resource(root, res) == 0)
			return;
		printk(KERN_INFO "yenta %s: Preassigned resource %d busy, reconfiguring...\n",
				socket->dev->slot_name, nr);
		res->start = res->end = 0;
	}

	if (type & IORESOURCE_IO) {
		align = 1024;
		size = 256;
		min = 0x4000;
		max = 0xffff;
	} else {
		unsigned long avail = root->end - root->start;
		int i;
		align = size = BRIDGE_SIZE_MAX;
		if (size > avail/8) {
			size=(avail+1)/8;
			/* round size down to next power of 2 */
			i = 0;
			while ((size /= 2) != 0)
				i++;
			size = 1 << i;
		}
		if (size < BRIDGE_SIZE_MIN)
			size = BRIDGE_SIZE_MIN;
		align = size;
		min = PCIBIOS_MIN_MEM; max = ~0U;
	}
		
	if (allocate_resource(root, res, size, min, max, align, NULL, NULL) < 0) {
		printk(KERN_INFO "yenta %s: no resource of type %x available, trying to continue...\n",
				socket->dev->slot_name, type);
		res->start = res->end = 0;
		return;
	}

	config_writel(socket, offset, res->start);
	config_writel(socket, offset+4, res->end);
}

/*
 * Allocate the bridge mappings for the device..
 */
static void yenta_allocate_resources(pci_socket_t *socket)
{
	yenta_allocate_res(socket, 0, IORESOURCE_MEM|IORESOURCE_PREFETCH);
	yenta_allocate_res(socket, 1, IORESOURCE_MEM);
	yenta_allocate_res(socket, 2, IORESOURCE_IO);
	yenta_allocate_res(socket, 3, IORESOURCE_IO);	/* PCI isn't clever enough to use this one yet */
}

/*
 * Free the bridge mappings for the device..
 */
static void yenta_free_resources(pci_socket_t *socket)
{
	int i;
	for (i=0;i<4;i++) {
		struct resource *res;
		res = socket->dev->resource + PCI_BRIDGE_RESOURCES + i;
		if (res->start != 0 && res->end != 0)
			release_resource(res);
		res->start = res->end = 0;
	}
}
/*
 * Close it down - release our resources and go home..
 */
static void yenta_close(pci_socket_t *sock)
{
	/* Disable all events so we don't die in an IRQ storm */
	cb_writel(sock, CB_SOCKET_MASK, 0x0);
	exca_writeb(sock, I365_CSCINT, 0);

	if (sock->cb_irq)
		free_irq(sock->cb_irq, sock);
	else
		del_timer_sync(&sock->poll_timer);

	if (sock->base)
		iounmap(sock->base);
	yenta_free_resources(sock);
}

#include "ti113x.h"
#include "ricoh.h"

/*
 * Different cardbus controllers have slightly different
 * initialization sequences etc details. List them here..
 */
#define PD(x,y) PCI_VENDOR_ID_##x, PCI_DEVICE_ID_##x##_##y
static struct cardbus_override_struct {
	unsigned short vendor;
	unsigned short device;
	struct pci_socket_ops *op;
} cardbus_override[] = {
	{ PD(TI,1130),	&ti113x_ops },
	{ PD(TI,1031),	&ti_ops },
	{ PD(TI,1131),	&ti113x_ops },
	{ PD(TI,1250),	&ti1250_ops },
	{ PD(TI,1220),	&ti_ops },
	{ PD(TI,1221),	&ti_ops },
	{ PD(TI,1210),	&ti_ops },
	{ PD(TI,1450),	&ti_ops },
	{ PD(TI,1225),	&ti_ops },
	{ PD(TI,1251A),	&ti_ops },
	{ PD(TI,1211),	&ti_ops },
	{ PD(TI,1251B),	&ti_ops },
	{ PD(TI,1410),	&ti_ops },
	{ PD(TI,1420),	&ti_ops },
	{ PD(TI,4410),	&ti_ops },
	{ PD(TI,4451),	&ti_ops },
	{ PD(TI,1510),  &ti_ops },
	{ PD(TI,1520),  &ti_ops },

	{ PD(ENE,1211),  &ti_ops },
	{ PD(ENE,1225),  &ti_ops },
	{ PD(ENE,1410),  &ti_ops },
	{ PD(ENE,1420),  &ti_ops },

	{ PD(RICOH,RL5C465), &ricoh_ops },
	{ PD(RICOH,RL5C466), &ricoh_ops },
	{ PD(RICOH,RL5C475), &ricoh_ops },
	{ PD(RICOH,RL5C476), &ricoh_ops },
	{ PD(RICOH,RL5C478), &ricoh_ops }
};

#define NR_OVERRIDES (sizeof(cardbus_override)/sizeof(struct cardbus_override_struct))

/*
 * Initialize a cardbus controller. Make sure we have a usable
 * interrupt, and that we can map the cardbus area. Fill in the
 * socket information structure..
 */
static int yenta_open(pci_socket_t *socket)
{
	int i;
	struct pci_dev *dev = socket->dev;

	/*
	 * Do some basic sanity checking..
	 */
	if (pci_enable_device(dev))
		return -1;
	if (!pci_resource_start(dev, 0)) {
		printk(KERN_ERR "No cardbus resource!\n");
		return -1;
	}

	/*
	 * Ok, start setup.. Map the cardbus registers,
	 * and request the IRQ.
	 */
	socket->base = ioremap(pci_resource_start(dev, 0), 0x1000);
	if (!socket->base)
		return -1;

	yenta_config_init(socket);

	/* Disable all events */
	cb_writel(socket, CB_SOCKET_MASK, 0x0);

	/* Set up the bridge regions.. */
	yenta_allocate_resources(socket);

	socket->cb_irq = dev->irq;

	/* Do we have special options for the device? */
	for (i = 0; i < NR_OVERRIDES; i++) {
		struct cardbus_override_struct *d = cardbus_override+i;
		if (dev->vendor == d->vendor && dev->device == d->device) {
			socket->op = d->op;
			if (d->op->open) {
				int retval = d->op->open(socket);
				if (retval < 0)
					return retval;
			}
		}
	}

	/* Get the PCMCIA kernel thread to complete the
	   initialisation later. We can't do this here,
	   because, er, because Linus says so :)
	*/
	socket->tq_task.routine = yenta_open_bh;
	socket->tq_task.data = socket;

	MOD_INC_USE_COUNT;
	schedule_task(&socket->tq_task);

	return 0;
}

/*
 * Standard plain cardbus - no frills, no extensions
 */
struct pci_socket_ops yenta_operations = {
	yenta_open,
	yenta_close,
	yenta_init,
	yenta_suspend,
	yenta_get_status,
	yenta_get_socket,
	yenta_set_socket,
	yenta_get_io_map,
	yenta_set_io_map,
	yenta_get_mem_map,
	yenta_set_mem_map,
	yenta_proc_setup
};
EXPORT_SYMBOL(yenta_operations);
MODULE_LICENSE("GPL");
