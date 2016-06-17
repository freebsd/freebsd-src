/*
 *
 * Alchemy Semi Au1000 pcmcia driver
 *
 * Copyright 2001-2003 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
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
 * 
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/config.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/tqueue.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/ss.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/bus_ops.h>
#include "cs_internal.h"

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

#include <asm/au1000.h>
#include <asm/au1000_pcmcia.h>

#ifdef PCMCIA_DEBUG
static int pc_debug;
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pete Popov, MontaVista Software <ppopov@mvista.com>");
MODULE_DESCRIPTION("Linux PCMCIA Card Services: Au1x00 Socket Controller");

#define MAP_SIZE 0x1000000

/* This structure maintains housekeeping state for each socket, such
 * as the last known values of the card detect pins, or the Card Services
 * callback value associated with the socket:
 */
static struct au1000_pcmcia_socket *pcmcia_socket;
static int socket_count;


/* Returned by the low-level PCMCIA interface: */
static struct pcmcia_low_level *pcmcia_low_level;

/* Event poll timer structure */
static struct timer_list poll_timer;


/* Prototypes for routines which are used internally: */

static int  au1000_pcmcia_driver_init(void);
static void au1000_pcmcia_driver_shutdown(void);
static void au1000_pcmcia_task_handler(void *data);
static void au1000_pcmcia_poll_event(unsigned long data);
static void au1000_pcmcia_interrupt(int irq, void *dev, struct pt_regs *regs);
static struct tq_struct au1000_pcmcia_task;

#ifdef CONFIG_PROC_FS
static int au1000_pcmcia_proc_status(char *buf, char **start, 
		off_t pos, int count, int *eof, void *data);
#endif


/* Prototypes for operations which are exported to the
 * new-and-impr^H^H^H^H^H^H^H^H^H^H in-kernel PCMCIA core:
 */

static int au1000_pcmcia_init(u32 sock);
static int au1000_pcmcia_suspend(u32 sock);
static int au1000_pcmcia_register_callback(u32 sock, 
		void (*handler)(void *, u32), void *info);
static int au1000_pcmcia_inquire_socket(u32 sock, socket_cap_t *cap);
static int au1000_pcmcia_get_status(u32 sock, u_int *value);
static int au1000_pcmcia_get_socket(u32 sock, socket_state_t *state);
static int au1000_pcmcia_set_socket(u32 sock, socket_state_t *state);
static int au1000_pcmcia_get_io_map(u32 sock, struct pccard_io_map *io);
static int au1000_pcmcia_set_io_map(u32 sock, struct pccard_io_map *io);
static int au1000_pcmcia_get_mem_map(u32 sock, struct pccard_mem_map *mem);
static int au1000_pcmcia_set_mem_map(u32 sock, struct pccard_mem_map *mem);
#ifdef CONFIG_PROC_FS
static void au1000_pcmcia_proc_setup(u32 sock, struct proc_dir_entry *base);
#endif

static struct pccard_operations au1000_pcmcia_operations = {
	au1000_pcmcia_init,
	au1000_pcmcia_suspend,
	au1000_pcmcia_register_callback,
	au1000_pcmcia_inquire_socket,
	au1000_pcmcia_get_status,
	au1000_pcmcia_get_socket,
	au1000_pcmcia_set_socket,
	au1000_pcmcia_get_io_map,
	au1000_pcmcia_set_io_map,
	au1000_pcmcia_get_mem_map,
	au1000_pcmcia_set_mem_map,
#ifdef CONFIG_PROC_FS
	au1000_pcmcia_proc_setup
#endif
};

static spinlock_t pcmcia_lock = SPIN_LOCK_UNLOCKED;

extern const unsigned long mips_io_port_base;

static int __init au1000_pcmcia_driver_init(void)
{
	servinfo_t info;
	struct pcmcia_init pcmcia_init;
	struct pcmcia_state state;
	unsigned int i;

	printk("\nAu1x00 PCMCIA (CS release %s)\n", CS_RELEASE);

#ifndef CONFIG_64BIT_PHYS_ADDR
	printk(KERN_ERR "Au1x00 PCMCIA 36 bit IO support not enabled\n");
	return -1;
#endif

	CardServices(GetCardServicesInfo, &info);

	if(info.Revision!=CS_RELEASE_CODE){
		printk(KERN_ERR "Card Services release codes do not match\n");
		return -1;
	}

#if defined(CONFIG_MIPS_PB1000) || defined(CONFIG_MIPS_PB1100) || defined(CONFIG_MIPS_PB1500)
	pcmcia_low_level=&pb1x00_pcmcia_ops;
#elif defined(CONFIG_MIPS_DB1000) || defined(CONFIG_MIPS_DB1100) || defined(CONFIG_MIPS_DB1500)
	pcmcia_low_level=&db1x00_pcmcia_ops;
#elif defined(CONFIG_MIPS_XXS1500)
	pcmcia_low_level=&xxs1500_pcmcia_ops;
#else
#error Unsupported AU1000 board.
#endif

	pcmcia_init.handler=au1000_pcmcia_interrupt;
	if((socket_count=pcmcia_low_level->init(&pcmcia_init))<0) {
		printk(KERN_ERR "Unable to initialize PCMCIA service.\n");
		return -EIO;
	}

	/* NOTE: the chip select must already be setup */

	pcmcia_socket = 
		kmalloc(sizeof(struct au1000_pcmcia_socket) * socket_count, 
				GFP_KERNEL);
	if (!pcmcia_socket) {
		printk(KERN_ERR "Card Services can't get memory \n");
		return -1;
	}
	memset(pcmcia_socket, 0,
			sizeof(struct au1000_pcmcia_socket) * socket_count);
			
	/* 
	 * Assuming max of 2 sockets, which the Au1000 supports.
	 * WARNING: the Pb1000 has two sockets, and both work, but you
	 * can't use them both at the same time due to glue logic conflicts.
	 */
	for(i=0; i < socket_count; i++) {

		if(pcmcia_low_level->socket_state(i, &state)<0){
			printk(KERN_ERR "Unable to get PCMCIA status\n");
			return -EIO;
		}
		pcmcia_socket[i].k_state=state;
		pcmcia_socket[i].cs_state.csc_mask=SS_DETECT;
		
		/*
		 * PCMCIA drivers use the inb/outb macros to access the
		 * IO registers. Since mips_io_port_base is added to the
		 * access address, we need to subtract it here.
		 */
		if (i == 0) {
			pcmcia_socket[i].virt_io = 
				(u32)ioremap((ioaddr_t)AU1X_SOCK0_IO, 0x1000) -
				mips_io_port_base;
			pcmcia_socket[i].phys_attr = 
				(ioaddr_t)AU1X_SOCK0_PHYS_ATTR;
			pcmcia_socket[i].phys_mem = 
				(ioaddr_t)AU1X_SOCK0_PHYS_MEM;
		}
#ifndef CONFIG_MIPS_XXS1500
		else  {
			pcmcia_socket[i].virt_io = 
				(u32)ioremap((ioaddr_t)AU1X_SOCK1_IO, 0x1000) -
				mips_io_port_base;
			pcmcia_socket[i].phys_attr = 
				(ioaddr_t)AU1X_SOCK1_PHYS_ATTR;
			pcmcia_socket[i].phys_mem = 
				(ioaddr_t)AU1X_SOCK1_PHYS_MEM;
		}
#endif
	}

	/* Only advertise as many sockets as we can detect: */
	if(register_ss_entry(socket_count, &au1000_pcmcia_operations)<0){
		printk(KERN_ERR "Unable to register socket service routine\n");
		return -ENXIO;
	}

	/* Start the event poll timer.  
	 * It will reschedule by itself afterwards. 
	 */
	au1000_pcmcia_poll_event(0);

	DEBUG(1, "au1000: initialization complete\n");
	return 0;

}  /* au1000_pcmcia_driver_init() */

module_init(au1000_pcmcia_driver_init);

static void __exit au1000_pcmcia_driver_shutdown(void)
{
	int i;

	del_timer_sync(&poll_timer);
	unregister_ss_entry(&au1000_pcmcia_operations);
	pcmcia_low_level->shutdown();
	flush_scheduled_tasks();
	for(i=0; i < socket_count; i++) {
		if (pcmcia_socket[i].virt_io) 
			iounmap((void *)pcmcia_socket[i].virt_io + 
					mips_io_port_base);
	}
	DEBUG(1, "au1000: shutdown complete\n");
}

module_exit(au1000_pcmcia_driver_shutdown);

static int au1000_pcmcia_init(unsigned int sock) { return 0; }

static int au1000_pcmcia_suspend(unsigned int sock)
{
	return 0;
}


static inline unsigned 
au1000_pcmcia_events(struct pcmcia_state *state, 
		struct pcmcia_state *prev_state, 
		unsigned int mask, unsigned int flags)
{
	unsigned int events=0;

	if(state->detect!=prev_state->detect){
		DEBUG(2, "%s(): card detect value %u\n", 
				__FUNCTION__, state->detect);
		events |= mask&SS_DETECT;
	}


	if(state->ready!=prev_state->ready){
		DEBUG(2, "%s(): card ready value %u\n", 
				__FUNCTION__, state->ready);
		events |= mask&((flags&SS_IOCARD)?0:SS_READY);
	}

	*prev_state=*state;
	return events;

}  /* au1000_pcmcia_events() */


/* 
 * Au1000_pcmcia_task_handler()
 * Processes socket events.
 */
static void au1000_pcmcia_task_handler(void *data) 
{
	struct pcmcia_state state;
	int i, events, irq_status;

	for(i=0; i<socket_count; i++)  {
		if((irq_status = pcmcia_low_level->socket_state(i, &state))<0)
			printk(KERN_ERR "low-level PCMCIA error\n");

		events = au1000_pcmcia_events(&state, 
				&pcmcia_socket[i].k_state, 
				pcmcia_socket[i].cs_state.csc_mask, 
				pcmcia_socket[i].cs_state.flags);
		if(pcmcia_socket[i].handler!=NULL) {
			pcmcia_socket[i].handler(pcmcia_socket[i].handler_info,
					events);
		}
	}

}  /* au1000_pcmcia_task_handler() */

static struct tq_struct au1000_pcmcia_task = {
	routine: au1000_pcmcia_task_handler
};


static void au1000_pcmcia_poll_event(unsigned long dummy)
{
	poll_timer.function = au1000_pcmcia_poll_event;
	poll_timer.expires = jiffies + AU1000_PCMCIA_POLL_PERIOD;
	add_timer(&poll_timer);
	schedule_task(&au1000_pcmcia_task);
}


/* 
 * au1000_pcmcia_interrupt()
 * The actual interrupt work is performed by au1000_pcmcia_task(), 
 * because the Card Services event handling code performs scheduling 
 * operations which cannot be executed from within an interrupt context.
 */
static void 
au1000_pcmcia_interrupt(int irq, void *dev, struct pt_regs *regs)
{
	schedule_task(&au1000_pcmcia_task);
}


static int 
au1000_pcmcia_register_callback(unsigned int sock, 
		void (*handler)(void *, unsigned int), void *info)
{
	if(handler==NULL){
		pcmcia_socket[sock].handler=NULL;
		MOD_DEC_USE_COUNT;
	} else {
		MOD_INC_USE_COUNT;
		pcmcia_socket[sock].handler=handler;
		pcmcia_socket[sock].handler_info=info;
	}
	return 0;
}


/* au1000_pcmcia_inquire_socket()
 *
 * From the sa1100 socket driver : 
 *
 * Implements the inquire_socket() operation for the in-kernel PCMCIA
 * service (formerly SS_InquireSocket in Card Services).  We set 
 * SS_CAP_STATIC_MAP, which disables the memory resource database check. 
 * (Mapped memory is set up within the socket driver itself.)
 *
 * In conjunction with the STATIC_MAP capability is a new field,
 * `io_offset', recommended by David Hinds. Rather than go through
 * the SetIOMap interface (which is not quite suited for communicating
 * window locations up from the socket driver), we just pass up 
 * an offset which is applied to client-requested base I/O addresses
 * in alloc_io_space().
 *
 * Returns: 0 on success, -1 if no pin has been configured for `sock'
 */
static int au1000_pcmcia_inquire_socket(unsigned int sock, socket_cap_t *cap)
{
	struct pcmcia_irq_info irq_info;

	if(sock > socket_count){
		printk(KERN_ERR "au1000: socket %u not configured\n", sock);
		return -1;
	}

	/* from the sa1100_generic driver: */

	/* SS_CAP_PAGE_REGS: used by setup_cis_mem() in cistpl.c to set the
	*   force_low argument to validate_mem() in rsrc_mgr.c -- since in
	*   general, the mapped * addresses of the PCMCIA memory regions
	*   will not be within 0xffff, setting force_low would be
	*   undesirable.
	*
	* SS_CAP_STATIC_MAP: don't bother with the (user-configured) memory
	*   resource database; we instead pass up physical address ranges
	*   and allow other parts of Card Services to deal with remapping.
	*
	* SS_CAP_PCCARD: we can deal with 16-bit PCMCIA & CF cards, but
	*   not 32-bit CardBus devices.
	*/
	cap->features=(SS_CAP_PAGE_REGS  | SS_CAP_STATIC_MAP | SS_CAP_PCCARD);

	irq_info.sock=sock;
	irq_info.irq=-1;

	if(pcmcia_low_level->get_irq_info(&irq_info)<0){
		printk(KERN_ERR "Error obtaining IRQ info socket %u\n", sock);
		return -1;
	}

	cap->irq_mask=0;
	cap->map_size=MAP_SIZE;
	cap->pci_irq=irq_info.irq;
	cap->io_offset=pcmcia_socket[sock].virt_io;

	return 0;

}  /* au1000_pcmcia_inquire_socket() */


static int 
au1000_pcmcia_get_status(unsigned int sock, unsigned int *status)
{
	struct pcmcia_state state;


	if((pcmcia_low_level->socket_state(sock, &state))<0){
		printk(KERN_ERR "Unable to get PCMCIA status from kernel.\n");
		return -1;
	}

	pcmcia_socket[sock].k_state = state;

	*status = state.detect?SS_DETECT:0;

	*status |= state.ready?SS_READY:0;

	*status |= pcmcia_socket[sock].cs_state.Vcc?SS_POWERON:0;

	if(pcmcia_socket[sock].cs_state.flags&SS_IOCARD)
		*status |= state.bvd1?SS_STSCHG:0;
	else {
		if(state.bvd1==0)
			*status |= SS_BATDEAD;
		else if(state.bvd2 == 0)
			*status |= SS_BATWARN;
	}

	*status|=state.vs_3v?SS_3VCARD:0;

	*status|=state.vs_Xv?SS_XVCARD:0;

	DEBUG(2, "\tstatus: %s%s%s%s%s%s%s%s\n",
	(*status&SS_DETECT)?"DETECT ":"",
	(*status&SS_READY)?"READY ":"", 
	(*status&SS_BATDEAD)?"BATDEAD ":"",
	(*status&SS_BATWARN)?"BATWARN ":"",
	(*status&SS_POWERON)?"POWERON ":"",
	(*status&SS_STSCHG)?"STSCHG ":"",
	(*status&SS_3VCARD)?"3VCARD ":"",
	(*status&SS_XVCARD)?"XVCARD ":"");

	return 0;

}  /* au1000_pcmcia_get_status() */


static int 
au1000_pcmcia_get_socket(unsigned int sock, socket_state_t *state)
{
	*state = pcmcia_socket[sock].cs_state;
	return 0;
}


static int 
au1000_pcmcia_set_socket(unsigned int sock, socket_state_t *state)
{
	struct pcmcia_configure configure;

	DEBUG(2, "\tmask:  %s%s%s%s%s%s\n\tflags: %s%s%s%s%s%s\n"
	"\tVcc %d  Vpp %d  irq %d\n",
	(state->csc_mask==0)?"<NONE>":"",
	(state->csc_mask&SS_DETECT)?"DETECT ":"",
	(state->csc_mask&SS_READY)?"READY ":"",
	(state->csc_mask&SS_BATDEAD)?"BATDEAD ":"",
	(state->csc_mask&SS_BATWARN)?"BATWARN ":"",
	(state->csc_mask&SS_STSCHG)?"STSCHG ":"",
	(state->flags==0)?"<NONE>":"",
	(state->flags&SS_PWR_AUTO)?"PWR_AUTO ":"",
	(state->flags&SS_IOCARD)?"IOCARD ":"",
	(state->flags&SS_RESET)?"RESET ":"",
	(state->flags&SS_SPKR_ENA)?"SPKR_ENA ":"",
	(state->flags&SS_OUTPUT_ENA)?"OUTPUT_ENA ":"",
	state->Vcc, state->Vpp, state->io_irq);

	configure.sock=sock;
	configure.vcc=state->Vcc;
	configure.vpp=state->Vpp;
	configure.output=(state->flags&SS_OUTPUT_ENA)?1:0;
	configure.speaker=(state->flags&SS_SPKR_ENA)?1:0;
	configure.reset=(state->flags&SS_RESET)?1:0;

	if(pcmcia_low_level->configure_socket(&configure)<0){
		printk(KERN_ERR "Unable to configure socket %u\n", sock);
		return -1;
	}

	pcmcia_socket[sock].cs_state = *state;
	return 0;

}  /* au1000_pcmcia_set_socket() */


static int 
au1000_pcmcia_get_io_map(unsigned int sock, struct pccard_io_map *map)
{
	DEBUG(1, "au1000_pcmcia_get_io_map: sock %d\n", sock);
	if(map->map>=MAX_IO_WIN){
		printk(KERN_ERR "%s(): map (%d) out of range\n", 
				__FUNCTION__, map->map);
		return -1;
	}
	*map=pcmcia_socket[sock].io_map[map->map];
	return 0;
}


int 
au1000_pcmcia_set_io_map(unsigned int sock, struct pccard_io_map *map)
{
	unsigned int speed;
	unsigned long start;

	if(map->map>=MAX_IO_WIN){
		printk(KERN_ERR "%s(): map (%d) out of range\n", 
				__FUNCTION__, map->map);
		return -1;
	}

	if(map->flags&MAP_ACTIVE){
		speed=(map->speed>0)?map->speed:AU1000_PCMCIA_IO_SPEED;
		pcmcia_socket[sock].speed_io=speed;
	}

	start=map->start;

	if(map->stop==1) {
		map->stop=PAGE_SIZE-1;
	}

	map->start=pcmcia_socket[sock].virt_io;
	map->stop=map->start+(map->stop-start);
	pcmcia_socket[sock].io_map[map->map]=*map;
	DEBUG(3, "set_io_map %d start %x stop %x\n", 
			map->map, map->start, map->stop);
	return 0;

}  /* au1000_pcmcia_set_io_map() */


static int 
au1000_pcmcia_get_mem_map(unsigned int sock, struct pccard_mem_map *map)
{

	if(map->map>=MAX_WIN) {
		printk(KERN_ERR "%s(): map (%d) out of range\n", 
				__FUNCTION__, map->map);
		return -1;
	}
	*map=pcmcia_socket[sock].mem_map[map->map];
	return 0;
}


static int 
au1000_pcmcia_set_mem_map(unsigned int sock, struct pccard_mem_map *map)
{
	unsigned int speed;
	unsigned long start;
	u_long flags;

	if(map->map>=MAX_WIN){
		printk(KERN_ERR "%s(): map (%d) out of range\n", 
				__FUNCTION__, map->map);
		return -1;
	}

	if(map->flags&MAP_ACTIVE){
		speed=(map->speed>0)?map->speed:AU1000_PCMCIA_MEM_SPEED;

		/* TBD */
		if(map->flags&MAP_ATTRIB){
			pcmcia_socket[sock].speed_attr=speed;
		} 
		else {
			pcmcia_socket[sock].speed_mem=speed;
		}
	}

	spin_lock_irqsave(&pcmcia_lock, flags);
	start=map->sys_start;

	if(map->sys_stop==0)
		map->sys_stop=MAP_SIZE-1;

	if (map->flags & MAP_ATTRIB) {
		map->sys_start = pcmcia_socket[sock].phys_attr + 
			map->card_start;
	}
	else {
		map->sys_start = pcmcia_socket[sock].phys_mem + 
			map->card_start;
	}

	map->sys_stop=map->sys_start+(map->sys_stop-start);
	pcmcia_socket[sock].mem_map[map->map]=*map;
	spin_unlock_irqrestore(&pcmcia_lock, flags);
	DEBUG(3, "set_mem_map %d start %x stop %x card_start %x\n", 
			map->map, map->sys_start, map->sys_stop, 
			map->card_start);
	return 0;

}  /* au1000_pcmcia_set_mem_map() */


#if defined(CONFIG_PROC_FS)

static void 
au1000_pcmcia_proc_setup(unsigned int sock, struct proc_dir_entry *base)
{
	struct proc_dir_entry *entry;

	if((entry=create_proc_entry("status", 0, base))==NULL){
		printk(KERN_ERR "Unable to install \"status\" procfs entry\n");
		return;
	}

	entry->read_proc=au1000_pcmcia_proc_status;
	entry->data=(void *)sock;
}


/* au1000_pcmcia_proc_status()
 * Implements the /proc/bus/pccard/??/status file.
 *
 * Returns: the number of characters added to the buffer
 */
static int 
au1000_pcmcia_proc_status(char *buf, char **start, off_t pos, 
		int count, int *eof, void *data)
{
	char *p=buf;
	unsigned int sock=(unsigned int)data;

	p+=sprintf(p, "k_flags  : %s%s%s%s%s%s%s\n", 
	     pcmcia_socket[sock].k_state.detect?"detect ":"",
	     pcmcia_socket[sock].k_state.ready?"ready ":"",
	     pcmcia_socket[sock].k_state.bvd1?"bvd1 ":"",
	     pcmcia_socket[sock].k_state.bvd2?"bvd2 ":"",
	     pcmcia_socket[sock].k_state.wrprot?"wrprot ":"",
	     pcmcia_socket[sock].k_state.vs_3v?"vs_3v ":"",
	     pcmcia_socket[sock].k_state.vs_Xv?"vs_Xv ":"");

	p+=sprintf(p, "status   : %s%s%s%s%s%s%s%s%s\n",
	     pcmcia_socket[sock].k_state.detect?"SS_DETECT ":"",
	     pcmcia_socket[sock].k_state.ready?"SS_READY ":"",
	     pcmcia_socket[sock].cs_state.Vcc?"SS_POWERON ":"",
	     pcmcia_socket[sock].cs_state.flags&SS_IOCARD?\
	     "SS_IOCARD ":"",
	     (pcmcia_socket[sock].cs_state.flags&SS_IOCARD &&
	      pcmcia_socket[sock].k_state.bvd1)?"SS_STSCHG ":"",
	     ((pcmcia_socket[sock].cs_state.flags&SS_IOCARD)==0 &&
	      (pcmcia_socket[sock].k_state.bvd1==0))?"SS_BATDEAD ":"",
	     ((pcmcia_socket[sock].cs_state.flags&SS_IOCARD)==0 &&
	      (pcmcia_socket[sock].k_state.bvd2==0))?"SS_BATWARN ":"",
	     pcmcia_socket[sock].k_state.vs_3v?"SS_3VCARD ":"",
	     pcmcia_socket[sock].k_state.vs_Xv?"SS_XVCARD ":"");

	p+=sprintf(p, "mask     : %s%s%s%s%s\n",
	     pcmcia_socket[sock].cs_state.csc_mask&SS_DETECT?\
	     "SS_DETECT ":"",
	     pcmcia_socket[sock].cs_state.csc_mask&SS_READY?\
	     "SS_READY ":"",
	     pcmcia_socket[sock].cs_state.csc_mask&SS_BATDEAD?\
	     "SS_BATDEAD ":"",
	     pcmcia_socket[sock].cs_state.csc_mask&SS_BATWARN?\
	     "SS_BATWARN ":"",
	     pcmcia_socket[sock].cs_state.csc_mask&SS_STSCHG?\
	     "SS_STSCHG ":"");

	p+=sprintf(p, "cs_flags : %s%s%s%s%s\n",
	     pcmcia_socket[sock].cs_state.flags&SS_PWR_AUTO?\
	     "SS_PWR_AUTO ":"",
	     pcmcia_socket[sock].cs_state.flags&SS_IOCARD?\
	     "SS_IOCARD ":"",
	     pcmcia_socket[sock].cs_state.flags&SS_RESET?\
	     "SS_RESET ":"",
	     pcmcia_socket[sock].cs_state.flags&SS_SPKR_ENA?\
	     "SS_SPKR_ENA ":"",
	     pcmcia_socket[sock].cs_state.flags&SS_OUTPUT_ENA?\
	     "SS_OUTPUT_ENA ":"");

	p+=sprintf(p, "Vcc      : %d\n", pcmcia_socket[sock].cs_state.Vcc);
	p+=sprintf(p, "Vpp      : %d\n", pcmcia_socket[sock].cs_state.Vpp);
	p+=sprintf(p, "irq      : %d\n", pcmcia_socket[sock].cs_state.io_irq);
	p+=sprintf(p, "I/O      : %u\n", pcmcia_socket[sock].speed_io);
	p+=sprintf(p, "attribute: %u\n", pcmcia_socket[sock].speed_attr);
	p+=sprintf(p, "common   : %u\n", pcmcia_socket[sock].speed_mem);
	return p-buf;
}


#endif  /* defined(CONFIG_PROC_FS) */
