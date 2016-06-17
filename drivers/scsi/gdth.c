/************************************************************************
 * Linux driver for                                                     *  
 * ICP vortex GmbH:    GDT ISA/EISA/PCI Disk Array Controllers          *
 * Intel Corporation:  Storage RAID Controllers                         *
 *                                                                      *
 * gdth.c                                                               *
 * Copyright (C) 1995-02 ICP vortex, an Intel company,  Achim Leubner   *
 * <achim.leubner@intel.com>                                            *
 *                                                                      *
 * Additions/Fixes: Boji Tony Kannanthanam                              *
 * <boji.t.kannanthanam@intel.com>                                      *
 *                                                                      *
 * This program is free software; you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published    *
 * by the Free Software Foundation; either version 2 of the License,    *
 * or (at your option) any later version.                               *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the         *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this kernel; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            *
 *                                                                      *
 * Tested with Linux 1.2.13, ..., 2.2.20, ..., 2.4.18                   *
 *                                                                      *
 * $Log: gdth.c,v $
 * Revision 1.61  2002/10/03 09:35:22  boji
 * Fixed SCREENSERVICE intialisation in SMP cases.
 * Added checks for gdth_polling before GDTH_HA_LOCK
 *
 * Revision 1.60  2002/02/05 09:35:22  achim
 * MODULE_LICENSE only if kernel >= 2.4.11
 *
 * Revision 1.59  2002/01/30 09:46:33  achim
 * Small changes
 *
 * Revision 1.58  2002/01/29 15:30:02  achim
 * Set default value of shared_access to Y
 * New status S_CACHE_RESERV for clustering added
 *
 * Revision 1.57  2001/08/21 11:16:35  achim
 * Bugfix free_irq()
 *
 * Revision 1.56  2001/08/09 11:19:39  achim
 * Scsi_Host_Template changes
 *
 * Revision 1.55  2001/08/09 10:11:28  achim
 * Command HOST_UNFREEZE_IO before cache service init.
 *
 * Revision 1.54  2001/07/20 13:48:12  achim
 * Expand: gdth_analyse_hdrive() removed
 *
 * Revision 1.53  2001/07/17 09:52:49  achim
 * Small OEM related change
 *
 * Revision 1.52  2001/06/19 15:06:20  achim
 * New host command GDT_UNFREEZE_IO added
 *
 * Revision 1.51  2001/05/22 06:42:37  achim
 * PCI: Subdevice ID added
 *
 * Revision 1.50  2001/05/17 13:42:16  achim
 * Support for Intel Storage RAID Controllers added
 *
 * Revision 1.50  2001/05/17 12:12:34  achim
 * Support for Intel Storage RAID Controllers added
 *
 * Revision 1.49  2001/03/15 15:07:17  achim
 * New __setup interface for boot command line options added
 *
 * Revision 1.48  2001/02/06 12:36:28  achim
 * Bugfix Cluster protocol
 *
 * Revision 1.47  2001/01/10 14:42:06  achim
 * New switch shared_access added
 *
 * Revision 1.46  2001/01/09 08:11:35  achim
 * gdth_command() removed
 * meaning of Scsi_Pointer members changed
 *
 * Revision 1.45  2000/11/16 12:02:24  achim
 * Changes for kernel 2.4
 *
 * Revision 1.44  2000/10/11 08:44:10  achim
 * Clustering changes: New flag media_changed added
 *
 * Revision 1.43  2000/09/20 12:59:01  achim
 * DPMEM remap functions for all PCI controller types implemented
 * Small changes for ia64 platform
 *
 * Revision 1.42  2000/07/20 09:04:50  achim
 * Small changes for kernel 2.4
 *
 * Revision 1.41  2000/07/04 14:11:11  achim
 * gdth_analyse_hdrive() added to rescan drives after online expansion
 *
 * Revision 1.40  2000/06/27 11:24:16  achim
 * Changes Clustering, Screenservice
 *
 * Revision 1.39  2000/06/15 13:09:04  achim
 * Changes for gdth_do_cmd()
 *
 * Revision 1.38  2000/06/15 12:08:43  achim
 * Bugfix gdth_sync_event(), service SCREENSERVICE
 * Data direction for command 0xc2 changed to DOU
 *
 * Revision 1.37  2000/05/25 13:50:10  achim
 * New driver parameter virt_ctr added
 *
 * Revision 1.36  2000/05/04 08:50:46  achim
 * Event buffer now in gdth_ha_str
 *
 * Revision 1.35  2000/03/03 10:44:08  achim
 * New event_string only valid for the RP controller family
 *
 * Revision 1.34  2000/03/02 14:55:29  achim
 * New mechanism for async. event handling implemented
 *
 * Revision 1.33  2000/02/21 15:37:37  achim
 * Bugfix Alpha platform + DPMEM above 4GB
 *
 * Revision 1.32  2000/02/14 16:17:37  achim
 * Bugfix sense_buffer[] + raw devices
 *
 * Revision 1.31  2000/02/10 10:29:00  achim
 * Delete sense_buffer[0], if command OK
 *
 * Revision 1.30  1999/11/02 13:42:39  achim
 * ARRAY_DRV_LIST2 implemented
 * Now 255 log. and 100 host drives supported
 *
 * Revision 1.29  1999/10/05 13:28:47  achim
 * GDT_CLUST_RESET added
 *
 * Revision 1.28  1999/08/12 13:44:54  achim
 * MOUNTALL removed
 * Cluster drives -> removeable drives
 *
 * Revision 1.27  1999/06/22 07:22:38  achim
 * Small changes
 *
 * Revision 1.26  1999/06/10 16:09:12  achim
 * Cluster Host Drive support: Bugfixes
 *
 * Revision 1.25  1999/06/01 16:03:56  achim
 * gdth_init_pci(): Manipulate config. space to start RP controller
 *
 * Revision 1.24  1999/05/26 11:53:06  achim
 * Cluster Host Drive support added
 *
 * Revision 1.23  1999/03/26 09:12:31  achim
 * Default value for hdr_channel set to 0
 *
 * Revision 1.22  1999/03/22 16:27:16  achim
 * Bugfix: gdth_store_event() must not be locked with GDTH_LOCK_HA()
 *
 * Revision 1.21  1999/03/16 13:40:34  achim
 * Problems with reserved drives solved
 * gdth_eh_bus_reset() implemented
 *
 * Revision 1.20  1999/03/10 09:08:13  achim
 * Bugfix: Corrections in gdth_direction_tab[] made
 * Bugfix: Increase command timeout (gdth_update_timeout()) NOT in gdth_putq()
 *
 * Revision 1.19  1999/03/05 14:38:16  achim
 * Bugfix: Heads/Sectors mapping for reserved devices possibly wrong
 * -> gdth_eval_mapping() implemented, changes in gdth_bios_param()
 * INIT_RETRIES set to 100s to avoid DEINIT-Timeout for controllers
 * with BIOS disabled and memory test set to Intensive
 * Enhanced /proc support
 *
 * Revision 1.18  1999/02/24 09:54:33  achim
 * Command line parameter hdr_channel implemented
 * Bugfix for EISA controllers + Linux 2.2.x
 *
 * Revision 1.17  1998/12/17 15:58:11  achim
 * Command line parameters implemented
 * Changes for Alpha platforms
 * PCI controller scan changed
 * SMP support improved (spin_lock_irqsave(),...)
 * New async. events, new scan/reserve commands included
 *
 * Revision 1.16  1998/09/28 16:08:46  achim
 * GDT_PCIMPR: DPMEM remapping, if required
 * mdelay() added
 *
 * Revision 1.15  1998/06/03 14:54:06  achim
 * gdth_delay(), gdth_flush() implemented
 * Bugfix: gdth_release() changed
 *
 * Revision 1.14  1998/05/22 10:01:17  achim
 * mj: pcibios_strerror() removed
 * Improved SMP support (if version >= 2.1.95)
 * gdth_halt(): halt_called flag added (if version < 2.1)
 *
 * Revision 1.13  1998/04/16 09:14:57  achim
 * Reserve drives (for raw service) implemented
 * New error handling code enabled
 * Get controller name from board_info() IOCTL
 * Final round of PCI device driver patches by Martin Mares
 *
 * Revision 1.12  1998/03/03 09:32:37  achim
 * Fibre channel controller support added
 *
 * Revision 1.11  1998/01/27 16:19:14  achim
 * SA_SHIRQ added
 * add_timer()/del_timer() instead of GDTH_TIMER
 * scsi_add_timer()/scsi_del_timer() instead of SCSI_TIMER
 * New error handling included
 *
 * Revision 1.10  1997/10/31 12:29:57  achim
 * Read heads/sectors from host drive
 *
 * Revision 1.9  1997/09/04 10:07:25  achim
 * IO-mapping with virt_to_bus(), gdth_readb(), gdth_writeb(), ...
 * register_reboot_notifier() to get a notify on shutdown used
 *
 * Revision 1.8  1997/04/02 12:14:30  achim
 * Version 1.00 (see gdth.h), tested with kernel 2.0.29
 *
 * Revision 1.7  1997/03/12 13:33:37  achim
 * gdth_reset() changed, new async. events
 *
 * Revision 1.6  1997/03/04 14:01:11  achim
 * Shutdown routine gdth_halt() implemented
 *
 * Revision 1.5  1997/02/21 09:08:36  achim
 * New controller included (RP, RP1, RP2 series)
 * IOCTL interface implemented
 *
 * Revision 1.4  1996/07/05 12:48:55  achim
 * Function gdth_bios_param() implemented
 * New constant GDTH_MAXC_P_L inserted
 * GDT_WRITE_THR, GDT_EXT_INFO implemented
 * Function gdth_reset() changed
 *
 * Revision 1.3  1996/05/10 09:04:41  achim
 * Small changes for Linux 1.2.13
 *
 * Revision 1.2  1996/05/09 12:45:27  achim
 * Loadable module support implemented
 * /proc support corrections made
 *
 * Revision 1.1  1996/04/11 07:35:57  achim
 * Initial revision
 *
 ************************************************************************/
#ident "$Id: gdth.c,v 1.60 2002/02/05 09:35:22 achim Exp $" 

/* All GDT Disk Array Controllers are fully supported by this driver.
 * This includes the PCI/EISA/ISA SCSI Disk Array Controllers and the
 * PCI Fibre Channel Disk Array Controllers. See gdth.h for a complete
 * list of all controller types.
 * 
 * If you have one or more GDT3000/3020 EISA controllers with 
 * controller BIOS disabled, you have to set the IRQ values with the 
 * command line option "gdth=irq1,irq2,...", where the irq1,irq2,... are
 * the IRQ values for the EISA controllers.
 * 
 * After the optional list of IRQ values, other possible 
 * command line options are:
 * disable:Y                    disable driver
 * disable:N                    enable driver
 * reserve_mode:0               reserve no drives for the raw service
 * reserve_mode:1               reserve all not init., removable drives
 * reserve_mode:2               reserve all not init. drives
 * reserve_list:h,b,t,l,h,b,t,l,...     reserve particular drive(s) with 
 *                              h- controller no., b- channel no., 
 *                              t- target ID, l- LUN
 * reverse_scan:Y               reverse scan order for PCI controllers         
 * reverse_scan:N               scan PCI controllers like BIOS
 * max_ids:x                    x - target ID count per channel (1..MAXID)
 * rescan:Y                     rescan all channels/IDs 
 * rescan:N                     use all devices found until now
 * virt_ctr:Y                   map every channel to a virtual controller 
 * virt_ctr:N                   use multi channel support 
 * hdr_channel:x                x - number of virtual bus for host drives
 * shared_access:Y              disable driver reserve/release protocol to 
 *                              access a shared resource from several nodes, 
 *                              appropiate controller firmware required
 * shared_access:N              enable driver reserve/release protocol
 *
 * The default values are: "gdth=disable:N,reserve_mode:1,reverse_scan:N,
 *                          max_ids:127,rescan:N,virt_ctr:N,hdr_channel:0,
 *                          shared_access:Y".
 * Here is another example: "gdth=reserve_list:0,1,2,0,0,1,3,0,rescan:Y".
 * 
 * When loading the gdth driver as a module, the same options are available. 
 * You can set the IRQs with "IRQ=...". However, the syntax to specify the
 * options changes slightly. You must replace all ',' between options 
 * with ' ' and all ':' with '=' and you must use 
 * '1' in place of 'Y' and '0' in place of 'N'.
 * 
 * Default: "modprobe gdth disable=0 reserve_mode=1 reverse_scan=0
 *           max_ids=127 rescan=0 virt_ctr=0 hdr_channel=0 shared_access=0"
 * The other example: "modprobe gdth reserve_list=0,1,2,0,0,1,3,0 rescan=1".
 */

/* The meaning of the Scsi_Pointer members in this driver is as follows:
 * ptr:                         Chaining
 * this_residual:               Command priority
 * buffer:                      Unused
 * buffers_residual:            Timeout value
 * Status:                      Command status (gdth_do_cmd())
 * Message:                     Additional info (gdth_do_cmd())
 * have_data_in:                Flag for gdth_wait_completion()
 * sent_command:                Opcode special command
 * phase:                       Service/parameter/return code special command
 */

#include <linux/module.h>

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/in.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/timer.h>
#ifdef GDTH_RTC
#include <linux/mc146818rtc.h>
#endif
#if LINUX_VERSION_CODE >= 0x020100
#include <linux/reboot.h>
#else
#include <linux/bios32.h>
#endif

#include <asm/dma.h>
#include <asm/system.h>
#include <asm/io.h>
#if LINUX_VERSION_CODE >= 0x020322
#include <linux/spinlock.h>
#elif LINUX_VERSION_CODE >= 0x02015F
#include <asm/spinlock.h>
#endif

#if LINUX_VERSION_CODE >= 0x010300
#include <linux/blk.h>
#else
#include "../block/blk.h"
#endif
#include "scsi.h"
#include "hosts.h"
#include "sd.h"

#include "gdth.h"

static void gdth_delay(int milliseconds);
static void gdth_eval_mapping(ulong32 size, int *cyls, int *heads, int *secs);
#if LINUX_VERSION_CODE >= 0x010346
static void gdth_interrupt(int irq,void *dev_id,struct pt_regs *regs);
#else
static void gdth_interrupt(int irq,struct pt_regs *regs);
#endif
static int gdth_sync_event(int hanum,int service,unchar index,Scsi_Cmnd *scp);
static int gdth_async_event(int hanum);
static void gdth_log_event(gdth_evt_data *dvr, char *buffer);

static void gdth_putq(int hanum,Scsi_Cmnd *scp,unchar priority);
static void gdth_next(int hanum);
static int gdth_fill_raw_cmd(int hanum,Scsi_Cmnd *scp,unchar b);
static int gdth_special_cmd(int hanum,Scsi_Cmnd *scp);
static gdth_evt_str *gdth_store_event(gdth_ha_str *ha, ushort source,
                                      ushort idx, gdth_evt_data *evt);
static int gdth_read_event(gdth_ha_str *ha, int handle, gdth_evt_str *estr);
static void gdth_readapp_event(gdth_ha_str *ha, unchar application, 
                               gdth_evt_str *estr);
static void gdth_clear_events(void);

static void gdth_copy_internal_data(Scsi_Cmnd *scp,char *buffer,ushort count);
static int gdth_internal_cache_cmd(int hanum,Scsi_Cmnd *scp);
static int gdth_fill_cache_cmd(int hanum,Scsi_Cmnd *scp,ushort hdrive);

static int gdth_search_eisa(ushort eisa_adr);
static int gdth_search_isa(ulong32 bios_adr);
static int gdth_search_pci(gdth_pci_str *pcistr);
static void gdth_search_dev(gdth_pci_str *pcistr, ushort *cnt, 
                            ushort vendor, ushort dev);
static void gdth_sort_pci(gdth_pci_str *pcistr, int cnt);
static int gdth_init_eisa(ushort eisa_adr,gdth_ha_str *ha);
static int gdth_init_isa(ulong32 bios_adr,gdth_ha_str *ha);
static int gdth_init_pci(gdth_pci_str *pcistr,gdth_ha_str *ha);

static void gdth_enable_int(int hanum);
static int gdth_get_status(unchar *pIStatus,int irq);
static int gdth_test_busy(int hanum);
static int gdth_get_cmd_index(int hanum);
static void gdth_release_event(int hanum);
static int gdth_wait(int hanum,int index,ulong32 time);
static int gdth_internal_cmd(int hanum,unchar service,ushort opcode,ulong32 p1,
                             ulong32 p2,ulong32 p3);
static int gdth_search_drives(int hanum);
static int gdth_analyse_hdrive(int hanum, ushort hdrive);

static void *gdth_mmap(ulong paddr, ulong size);
static void gdth_munmap(void *addr);

static const char *gdth_ctr_name(int hanum);

#if LINUX_VERSION_CODE >= 0x010300
static void gdth_flush(int hanum);
#if LINUX_VERSION_CODE >= 0x020100
static int gdth_halt(struct notifier_block *nb, ulong event, void *buf);
#else
static int halt_called = FALSE;
void gdth_halt(void);
#endif
#endif

#ifdef DEBUG_GDTH
static unchar   DebugState = DEBUG_GDTH;

#ifdef __SERIAL__
#define MAX_SERBUF 160
static void ser_init(void);
static void ser_puts(char *str);
static void ser_putc(char c);
static int  ser_printk(const char *fmt, ...);
static char strbuf[MAX_SERBUF+1];
#ifdef __COM2__
#define COM_BASE 0x2f8
#else
#define COM_BASE 0x3f8
#endif
static void ser_init()
{
    unsigned port=COM_BASE;

    outb(0x80,port+3);
    outb(0,port+1);
    /* 19200 Baud, if 9600: outb(12,port) */
    outb(6, port);
    outb(3,port+3);
    outb(0,port+1);
    /*
    ser_putc('I');
    ser_putc(' ');
    */
}

static void ser_puts(char *str)
{
    char *ptr;

    ser_init();
    for (ptr=str;*ptr;++ptr)
        ser_putc(*ptr);
}

static void ser_putc(char c)
{
    unsigned port=COM_BASE;

    while ((inb(port+5) & 0x20)==0);
    outb(c,port);
    if (c==0x0a)
    {
        while ((inb(port+5) & 0x20)==0);
        outb(0x0d,port);
    }
}

static int ser_printk(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args,fmt);
    i = vsprintf(strbuf,fmt,args);
    ser_puts(strbuf);
    va_end(args);
    return i;
}

#define TRACE(a)    {if (DebugState==1) {ser_printk a;}}
#define TRACE2(a)   {if (DebugState==1 || DebugState==2) {ser_printk a;}}
#define TRACE3(a)   {if (DebugState!=0) {ser_printk a;}}

#else /* !__SERIAL__ */
#define TRACE(a)    {if (DebugState==1) {printk a;}}
#define TRACE2(a)   {if (DebugState==1 || DebugState==2) {printk a;}}
#define TRACE3(a)   {if (DebugState!=0) {printk a;}}
#endif

#else /* !DEBUG */
#define TRACE(a)
#define TRACE2(a)
#define TRACE3(a)
#endif

#ifdef GDTH_STATISTICS
static ulong32 max_rq=0, max_index=0, max_sg=0;
static ulong32 act_ints=0, act_ios=0, act_stats=0, act_rq=0;
static struct timer_list gdth_timer;
#endif

#define PTR2USHORT(a)   (ushort)(ulong)(a)
#define GDTOFFSOF(a,b)  (size_t)&(((a*)0)->b)   
#define INDEX_OK(i,t)   ((i)<sizeof(t)/sizeof((t)[0]))

#define NUMDATA(a)      ( (gdth_num_str  *)((a)->hostdata))
#define HADATA(a)       (&((gdth_ext_str *)((a)->hostdata))->haext)
#define CMDDATA(a)      (&((gdth_ext_str *)((a)->hostdata))->cmdext)

#define BUS_L2P(a,b)    ((b)>(a)->virt_bus ? (b-1):(b))

#if LINUX_VERSION_CODE < 0x010300
static void *gdth_mmap(ulong paddr, ulong size) 
{
    if (paddr >= high_memory)
        return NULL; 
    else
        return (void *)paddr;
}
static void gdth_munmap(void *addr) 
{
}
inline ulong32 virt_to_phys(volatile void *addr)
{
    return (ulong32)addr;
}
inline void *phys_to_virt(ulong32 addr)
{
    return (void *)addr;
}
#define virt_to_bus             virt_to_phys
#define bus_to_virt             phys_to_virt
#define gdth_readb(addr)        (*(volatile unchar *)(addr))
#define gdth_readw(addr)        (*(volatile ushort *)(addr))
#define gdth_readl(addr)        (*(volatile ulong32 *)(addr))
#define gdth_writeb(b,addr)     (*(volatile unchar *)(addr) = (b))
#define gdth_writew(b,addr)     (*(volatile ushort *)(addr) = (b))
#define gdth_writel(b,addr)     (*(volatile ulong32 *)(addr) = (b))
#define memset_io(a,b,c)        memset((void *)(a),(b),(c))
#define memcpy_fromio(a,b,c)    memcpy((a),(void *)(b),(c))
#define memcpy_toio(a,b,c)      memcpy((void *)(a),(b),(c))

#define PCI_SLOT(devfn)         ((devfn >> 3) & 0x1f)

#elif LINUX_VERSION_CODE < 0x020100
static int remapped = FALSE;
static void *gdth_mmap(ulong paddr, ulong size) 
{
    if ( paddr >= high_memory) {
        remapped = TRUE;
        return vremap(paddr, size);
    } else {
        return (void *)paddr; 
    }
}
static void gdth_munmap(void *addr) 
{
    if (remapped)
        vfree(addr);
    remapped = FALSE;
}
#define gdth_readb(addr)        readb((ulong)(addr))
#define gdth_readw(addr)        readw((ulong)(addr))
#define gdth_readl(addr)        (ulong32)readl((ulong)(addr))
#define gdth_writeb(b,addr)     writeb((b),(ulong)(addr))
#define gdth_writew(b,addr)     writew((b),(ulong)(addr))
#define gdth_writel(b,addr)     writel((ulong32)(b),(ulong)(addr))

#else
static void *gdth_mmap(ulong paddr, ulong size) 
{ 
    return ioremap(paddr, size); 
}
static void gdth_munmap(void *addr) 
{
    return iounmap(addr);
}
#define gdth_readb(addr)        readb((ulong)(addr))
#define gdth_readw(addr)        readw((ulong)(addr))
#define gdth_readl(addr)        (ulong32)readl((ulong)(addr))
#define gdth_writeb(b,addr)     writeb((b),(ulong)(addr))
#define gdth_writew(b,addr)     writew((b),(ulong)(addr))
#define gdth_writel(b,addr)     writel((ulong32)(b),(ulong)(addr))
#endif


static unchar   gdth_drq_tab[4] = {5,6,7,7};            /* DRQ table */
static unchar   gdth_irq_tab[6] = {0,10,11,12,14,0};    /* IRQ table */
static unchar   gdth_polling;                           /* polling if TRUE */
static unchar   gdth_from_wait  = FALSE;                /* gdth_wait() */
static int      wait_index,wait_hanum;                  /* gdth_wait() */
static int      gdth_ctr_count  = 0;                    /* controller count */
static int      gdth_ctr_vcount = 0;                    /* virt. ctr. count */
static int      gdth_ctr_released = 0;                  /* gdth_release() */
static struct Scsi_Host *gdth_ctr_tab[MAXHA];           /* controller table */
static struct Scsi_Host *gdth_ctr_vtab[MAXHA*MAXBUS];   /* virt. ctr. table */
static unchar   gdth_write_through = FALSE;             /* write through */
static gdth_evt_str ebuffer[MAX_EVENTS];                /* event buffer */
static int elastidx;
static int eoldidx;

#define DIN     1                               /* IN data direction */
#define DOU     2                               /* OUT data direction */
#define DNO     DIN                             /* no data transfer */
#define DUN     DIN                             /* unknown data direction */
static unchar gdth_direction_tab[0x100] = {
    DNO,DNO,DIN,DIN,DOU,DIN,DIN,DOU,DIN,DUN,DOU,DOU,DUN,DUN,DUN,DIN,
    DNO,DIN,DIN,DOU,DIN,DOU,DNO,DNO,DOU,DNO,DIN,DNO,DIN,DOU,DNO,DUN,
    DIN,DUN,DIN,DUN,DOU,DIN,DUN,DUN,DIN,DIN,DOU,DNO,DUN,DIN,DOU,DOU,
    DOU,DOU,DOU,DNO,DIN,DNO,DNO,DIN,DOU,DOU,DOU,DOU,DIN,DOU,DIN,DOU,
    DOU,DOU,DIN,DIN,DIN,DNO,DUN,DNO,DNO,DNO,DUN,DNO,DOU,DIN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DOU,DUN,DUN,DUN,DUN,DIN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DNO,DNO,DUN,DIN,DNO,DOU,DUN,DNO,DUN,DOU,DOU,
    DOU,DOU,DOU,DNO,DUN,DIN,DOU,DIN,DIN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DOU,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DOU,DUN,DUN,DUN,DUN,DUN,
    DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN,DUN
};

/* __initfunc, __initdata macros */
#if LINUX_VERSION_CODE >= 0x020322
#define GDTH_INITFUNC(type, func)       type __init func 
#include <linux/init.h>
#elif LINUX_VERSION_CODE >= 0x020126
#define GDTH_INITFUNC(type, func)       __initfunc(type func)
#include <linux/init.h>
#else
#define GDTH_INITFUNC(type, func)       type func
#define __initdata
#define __init
#endif

#if LINUX_VERSION_CODE >= 0x02015F
#define GDTH_INIT_LOCK_HA(ha)           spin_lock_init(&(ha)->smp_lock)
#define GDTH_LOCK_HA(ha,flags)          spin_lock_irqsave(&(ha)->smp_lock,flags)
#define GDTH_UNLOCK_HA(ha,flags)        spin_unlock_irqrestore(&(ha)->smp_lock,flags)

#define GDTH_LOCK_SCSI_DONE(flags)      spin_lock_irqsave(&io_request_lock,flags)
#define GDTH_UNLOCK_SCSI_DONE(flags)    spin_unlock_irqrestore(&io_request_lock,flags)
#define GDTH_LOCK_SCSI_DOCMD()          spin_lock_irq(&io_request_lock)
#define GDTH_UNLOCK_SCSI_DOCMD()        spin_unlock_irq(&io_request_lock)
#else
#define GDTH_INIT_LOCK_HA(ha)           do {} while (0)
#define GDTH_LOCK_HA(ha,flags)          do {save_flags(flags); cli();} while (0)
#define GDTH_UNLOCK_HA(ha,flags)        do {restore_flags(flags);} while (0)

#define GDTH_LOCK_SCSI_DONE(flags)      do {} while (0)
#define GDTH_UNLOCK_SCSI_DONE(flags)    do {} while (0)
#define GDTH_LOCK_SCSI_DOCMD()          do {} while (0)
#define GDTH_UNLOCK_SCSI_DOCMD()        do {} while (0)
#endif

/* LILO and modprobe/insmod parameters */
/* IRQ list for GDT3000/3020 EISA controllers */
static int irq[MAXHA] __initdata = 
{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
/* disable driver flag */
static int disable __initdata = 0;
/* reserve flag */
static int reserve_mode = 1;                  
/* reserve list */
static int reserve_list[MAX_RES_ARGS] = 
{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
/* scan order for PCI controllers */
static int reverse_scan = 0;
/* virtual channel for the host drives */
static int hdr_channel = 0;
/* max. IDs per channel */
static int max_ids = MAXID;
/* rescan all IDs */
static int rescan = 0;
/* map channels to virtual controllers */
static int virt_ctr = 0;
/* shared access */
static int shared_access = 1;

#ifdef MODULE
#if LINUX_VERSION_CODE >= 0x02011A
/* parameters for modprobe/insmod */
MODULE_PARM(irq, "i");
MODULE_PARM(disable, "i");
MODULE_PARM(reserve_mode, "i");
MODULE_PARM(reserve_list, "4-" __MODULE_STRING(MAX_RES_ARGS) "i");
MODULE_PARM(reverse_scan, "i");
MODULE_PARM(hdr_channel, "i");
MODULE_PARM(max_ids, "i");
MODULE_PARM(rescan, "i");
MODULE_PARM(virt_ctr, "i");
MODULE_PARM(shared_access, "i");
MODULE_AUTHOR("Achim Leubner");
#endif
#if LINUX_VERSION_CODE >= 0x02040B
MODULE_LICENSE("GPL");
#endif
#endif

/* /proc support */
#if LINUX_VERSION_CODE >= 0x010300
#include <linux/stat.h> 
#if LINUX_VERSION_CODE < 0x020322
struct proc_dir_entry proc_scsi_gdth = {
    PROC_SCSI_GDTH, 4, "gdth",
    S_IFDIR | S_IRUGO | S_IXUGO, 2
};
#endif
#include "gdth_proc.h"
#include "gdth_proc.c"
#endif

#if LINUX_VERSION_CODE >= 0x020100
/* notifier block to get a notify on system shutdown/halt/reboot */
static struct notifier_block gdth_notifier = {
    gdth_halt, NULL, 0
};
#endif


static void gdth_delay(int milliseconds)
{
    if (milliseconds == 0) {
        udelay(1);
    } else {
#if LINUX_VERSION_CODE >= 0x020168
        mdelay(milliseconds);
#else
        int i;
        for (i = 0; i < milliseconds; ++i) 
            udelay(1000);
#endif
    }
}

static void gdth_eval_mapping(ulong32 size, int *cyls, int *heads, int *secs)
{
    *cyls = size /HEADS/SECS;
    if (*cyls <= MAXCYLS) {
        *heads = HEADS;
        *secs = SECS;
    } else {                                        /* too high for 64*32 */
        *cyls = size /MEDHEADS/MEDSECS;
        if (*cyls <= MAXCYLS) {
            *heads = MEDHEADS;
            *secs = MEDSECS;
        } else {                                    /* too high for 127*63 */
            *cyls = size /BIGHEADS/BIGSECS;
            *heads = BIGHEADS;
            *secs = BIGSECS;
        }
    }
}

/* controller search and initialization functions */

GDTH_INITFUNC(static int, gdth_search_eisa(ushort eisa_adr))
{
    ulong32 id;
    
    TRACE(("gdth_search_eisa() adr. %x\n",eisa_adr));
    id = inl(eisa_adr+ID0REG);
    if (id == GDT3A_ID || id == GDT3B_ID) {     /* GDT3000A or GDT3000B */
        if ((inb(eisa_adr+EISAREG) & 8) == 0)   
            return 0;                           /* not EISA configured */
        return 1;
    }
    if (id == GDT3_ID)                          /* GDT3000 */
        return 1;

    return 0;                                   
}


GDTH_INITFUNC(static int, gdth_search_isa(ulong32 bios_adr))
{
    void *addr;
    ulong32 id;

    TRACE(("gdth_search_isa() bios adr. %x\n",bios_adr));
    if ((addr = gdth_mmap(bios_adr+BIOS_ID_OFFS, sizeof(ulong32))) != NULL) {
        id = gdth_readl(addr);
        gdth_munmap(addr);
        if (id == GDT2_ID)                          /* GDT2000 */
            return 1;
    }
    return 0;
}


GDTH_INITFUNC(static int, gdth_search_pci(gdth_pci_str *pcistr))
{
    ushort device, cnt;
    
    TRACE(("gdth_search_pci()\n"));

    cnt = 0;
    for (device = 0; device <= PCI_DEVICE_ID_VORTEX_GDT6555; ++device)
        gdth_search_dev(pcistr, &cnt, PCI_VENDOR_ID_VORTEX, device);
    for (device = PCI_DEVICE_ID_VORTEX_GDT6x17RP; 
         device <= PCI_DEVICE_ID_VORTEX_GDTMAXRP; ++device)
        gdth_search_dev(pcistr, &cnt, PCI_VENDOR_ID_VORTEX, device);
    gdth_search_dev(pcistr, &cnt, PCI_VENDOR_ID_VORTEX, 
                    PCI_DEVICE_ID_VORTEX_GDTNEWRX);
    gdth_search_dev(pcistr, &cnt, PCI_VENDOR_ID_INTEL,
                    PCI_DEVICE_ID_INTEL_SRC);
    return cnt;
}

#if LINUX_VERSION_CODE >= 0x20363
/* Vortex only makes RAID controllers.
 * We do not really want to specify all 550 ids here, so wildcard match.
 */
static struct pci_device_id gdthtable[] = {
	{PCI_VENDOR_ID_VORTEX,PCI_ANY_ID,PCI_ANY_ID, PCI_ANY_ID },
	{PCI_VENDOR_ID_INTEL,PCI_DEVICE_ID_INTEL_SRC,PCI_ANY_ID,PCI_ANY_ID }, 
	{0}
};
MODULE_DEVICE_TABLE(pci,gdthtable);
#endif

GDTH_INITFUNC(static void, gdth_search_dev(gdth_pci_str *pcistr, ushort *cnt,
                                           ushort vendor, ushort device))
{
    ulong base0, base1, base2;
#if LINUX_VERSION_CODE >= 0x2015C
    struct pci_dev *pdev;
#else
    int error;
    ushort idx;
#endif
    
    TRACE(("gdth_search_dev() cnt %d vendor %x device %x\n",
          *cnt, vendor, device));

#if LINUX_VERSION_CODE >= 0x20363
    pdev = NULL;
    while ((pdev = pci_find_device(vendor, device, pdev)) 
           != NULL) {
        if (pci_enable_device(pdev))
            continue;
        if (*cnt >= MAXHA)
            return;
        /* GDT PCI controller found, resources are already in pdev */
        pcistr[*cnt].pdev = pdev;
        pcistr[*cnt].vendor_id = vendor;
        pcistr[*cnt].device_id = device;
        pcistr[*cnt].subdevice_id = pdev->subsystem_device;
        pcistr[*cnt].bus = pdev->bus->number;
        pcistr[*cnt].device_fn = pdev->devfn;
        pcistr[*cnt].irq = pdev->irq;
        base0 = pci_resource_flags(pdev, 0);
        base1 = pci_resource_flags(pdev, 1);
        base2 = pci_resource_flags(pdev, 2);
        if (device <= PCI_DEVICE_ID_VORTEX_GDT6000B ||   /* GDT6000/B */
            device >= PCI_DEVICE_ID_VORTEX_GDT6x17RP) {  /* MPR */
            if (!(base0 & IORESOURCE_MEM)) 
                continue;
            pcistr[*cnt].dpmem = pci_resource_start(pdev, 0);
        } else {                                  /* GDT6110, GDT6120, .. */
            if (!(base0 & IORESOURCE_MEM) ||
                !(base2 & IORESOURCE_MEM) ||
                !(base1 & IORESOURCE_IO)) 
                continue;
            pcistr[*cnt].dpmem = pci_resource_start(pdev, 2);
            pcistr[*cnt].io_mm = pci_resource_start(pdev, 0);
            pcistr[*cnt].io    = pci_resource_start(pdev, 1);
        }
        TRACE2(("Controller found at %d/%d, irq %d, dpmem 0x%lx\n",
                pcistr[*cnt].bus, PCI_SLOT(pcistr[*cnt].device_fn), 
                pcistr[*cnt].irq, pcistr[*cnt].dpmem));
        (*cnt)++;
    }       
#elif LINUX_VERSION_CODE >= 0x2015C
    pdev = NULL;
    while ((pdev = pci_find_device(vendor, device, pdev)) 
           != NULL) {
        if (*cnt >= MAXHA)
            return;
        /* GDT PCI controller found, resources are already in pdev */
        pcistr[*cnt].pdev = pdev;
        pcistr[*cnt].vendor_id = vendor;
        pcistr[*cnt].device_id = device;
        pcistr[*cnt].bus = pdev->bus->number;
        pcistr[*cnt].device_fn = pdev->devfn;
        pcibios_read_config_word(pcistr[*cnt].bus, pcistr[*cnt].device_fn,
                                 PCI_SUBSYSTEM_ID, &pcistr[*cnt].subdevice_id);
        pcistr[*cnt].irq = pdev->irq;
        base0 = pdev->base_address[0];
        base1 = pdev->base_address[1];
        base2 = pdev->base_address[2];
        if (device <= PCI_DEVICE_ID_VORTEX_GDT6000B ||   /* GDT6000/B */
            device >= PCI_DEVICE_ID_VORTEX_GDT6x17RP) {  /* MPR */
            if ((base0 & PCI_BASE_ADDRESS_SPACE) != 
                PCI_BASE_ADDRESS_SPACE_MEMORY)
                continue;
            pcistr[*cnt].dpmem = base0 & PCI_BASE_ADDRESS_MEM_MASK;
        } else {                                  /* GDT6110, GDT6120, .. */
            if ((base0 & PCI_BASE_ADDRESS_SPACE) !=
                PCI_BASE_ADDRESS_SPACE_MEMORY ||
                (base2 & PCI_BASE_ADDRESS_SPACE) !=
                PCI_BASE_ADDRESS_SPACE_MEMORY ||
                (base1 & PCI_BASE_ADDRESS_SPACE) !=
                PCI_BASE_ADDRESS_SPACE_IO)
                continue;
            pcistr[*cnt].dpmem = base2 & PCI_BASE_ADDRESS_MEM_MASK;
            pcistr[*cnt].io_mm = base0 & PCI_BASE_ADDRESS_MEM_MASK;
            pcistr[*cnt].io    = base1 & PCI_BASE_ADDRESS_IO_MASK;
        }
        TRACE2(("Controller found at %d/%d, irq %d, dpmem 0x%lx\n",
                pcistr[*cnt].bus, PCI_SLOT(pcistr[*cnt].device_fn), 
                pcistr[*cnt].irq, pcistr[*cnt].dpmem));
        (*cnt)++;
    }       
#else   
    idx = 0;
    while (!pcibios_find_device(vendor, device, idx++,
                                &pcistr[*cnt].bus,&pcistr[*cnt].device_fn)) {
        if (*cnt >= MAXHA)
            return;
        /* GDT PCI ctr. found, now read resources from config space */
#if LINUX_VERSION_CODE >= 0x010300
#define GDTH_BASEP      (int *)
#else
#define GDTH_BASEP
#endif
        if ((error = pcibios_read_config_dword(pcistr[*cnt].bus,
                                               pcistr[*cnt].device_fn,
                                               PCI_BASE_ADDRESS_0,
                                               GDTH_BASEP&base0)) ||
            (error = pcibios_read_config_dword(pcistr[*cnt].bus,
                                               pcistr[*cnt].device_fn,
                                               PCI_BASE_ADDRESS_1,
                                               GDTH_BASEP&base1)) ||
            (error = pcibios_read_config_dword(pcistr[*cnt].bus,
                                               pcistr[*cnt].device_fn,
                                               PCI_BASE_ADDRESS_2,
                                               GDTH_BASEP&base2)) ||
            (error = pcibios_read_config_word(pcistr[*cnt].bus,
                                              pcistr[*cnt].device_fn,
                                              PCI_SUBSYSTEM_ID,
                                              &pcistr[*cnt].subdevice_id)) ||
            (error = pcibios_read_config_byte(pcistr[*cnt].bus,
                                              pcistr[*cnt].device_fn,
                                              PCI_INTERRUPT_LINE,
                                              &pcistr[*cnt].irq))) {
            printk("GDT-PCI: error %d reading configuration space", error);
            continue;
        }
        pcistr[*cnt].vendor_id = vendor;
        pcistr[*cnt].device_id = device;
        if (device <= PCI_DEVICE_ID_VORTEX_GDT6000B ||   /* GDT6000/B */
            device >= PCI_DEVICE_ID_VORTEX_GDT6x17RP) {  /* MPR */
            if ((base0 & PCI_BASE_ADDRESS_SPACE) !=
                PCI_BASE_ADDRESS_SPACE_MEMORY)
                continue;
            pcistr[*cnt].dpmem = base0 & PCI_BASE_ADDRESS_MEM_MASK;
        } else {                                    /* GDT6110, GDT6120, .. */
            if ((base0 & PCI_BASE_ADDRESS_SPACE) !=
                PCI_BASE_ADDRESS_SPACE_MEMORY ||
                (base2 & PCI_BASE_ADDRESS_SPACE) !=
                PCI_BASE_ADDRESS_SPACE_MEMORY ||
                (base1 & PCI_BASE_ADDRESS_SPACE) !=
                PCI_BASE_ADDRESS_SPACE_IO)
                continue;
            pcistr[*cnt].dpmem = base2 & PCI_BASE_ADDRESS_MEM_MASK;
            pcistr[*cnt].io_mm = base0 & PCI_BASE_ADDRESS_MEM_MASK;
            pcistr[*cnt].io    = base1 & PCI_BASE_ADDRESS_IO_MASK;
        }
        TRACE2(("Controller found at %d/%d, irq %d, dpmem 0x%lx\n",
                pcistr[*cnt].bus, PCI_SLOT(pcistr[*cnt].device_fn), 
                pcistr[*cnt].irq, pcistr[*cnt].dpmem));
        (*cnt)++;
    }
#endif
}   


GDTH_INITFUNC(static void, gdth_sort_pci(gdth_pci_str *pcistr, int cnt))
{    
    gdth_pci_str temp;
    int i, changed;
    
    TRACE(("gdth_sort_pci() cnt %d\n",cnt));
    if (cnt == 0)
        return;

    do {
        changed = FALSE;
        for (i = 0; i < cnt-1; ++i) {
            if (!reverse_scan) {
                if ((pcistr[i].bus > pcistr[i+1].bus) ||
                    (pcistr[i].bus == pcistr[i+1].bus &&
                     PCI_SLOT(pcistr[i].device_fn) > 
                     PCI_SLOT(pcistr[i+1].device_fn))) {
                    temp = pcistr[i];
                    pcistr[i] = pcistr[i+1];
                    pcistr[i+1] = temp;
                    changed = TRUE;
                }
            } else {
                if ((pcistr[i].bus < pcistr[i+1].bus) ||
                    (pcistr[i].bus == pcistr[i+1].bus &&
                     PCI_SLOT(pcistr[i].device_fn) < 
                     PCI_SLOT(pcistr[i+1].device_fn))) {
                    temp = pcistr[i];
                    pcistr[i] = pcistr[i+1];
                    pcistr[i+1] = temp;
                    changed = TRUE;
                }
            }
        }
    } while (changed);
}


GDTH_INITFUNC(static int, gdth_init_eisa(ushort eisa_adr,gdth_ha_str *ha))
{
    ulong32 retries,id;
    unchar prot_ver,eisacf,i,irq_found;

    TRACE(("gdth_init_eisa() adr. %x\n",eisa_adr));
    
    /* disable board interrupts, deinitialize services */
    outb(0xff,eisa_adr+EDOORREG);
    outb(0x00,eisa_adr+EDENABREG);
    outb(0x00,eisa_adr+EINTENABREG);
    
    outb(0xff,eisa_adr+LDOORREG);
    retries = INIT_RETRIES;
    gdth_delay(20);
    while (inb(eisa_adr+EDOORREG) != 0xff) {
        if (--retries == 0) {
            printk("GDT-EISA: Initialization error (DEINIT failed)\n");
            return 0;
        }
        gdth_delay(1);
        TRACE2(("wait for DEINIT: retries=%d\n",retries));
    }
    prot_ver = inb(eisa_adr+MAILBOXREG);
    outb(0xff,eisa_adr+EDOORREG);
    if (prot_ver != PROTOCOL_VERSION) {
        printk("GDT-EISA: Illegal protocol version\n");
        return 0;
    }
    ha->bmic = eisa_adr;
    ha->brd_phys = (ulong32)eisa_adr >> 12;

    outl(0,eisa_adr+MAILBOXREG);
    outl(0,eisa_adr+MAILBOXREG+4);
    outl(0,eisa_adr+MAILBOXREG+8);
    outl(0,eisa_adr+MAILBOXREG+12);

    /* detect IRQ */ 
    if ((id = inl(eisa_adr+ID0REG)) == GDT3_ID) {
        ha->oem_id = OEM_ID_ICP;
        ha->type = GDT_EISA;
        ha->stype = id;
        outl(1,eisa_adr+MAILBOXREG+8);
        outb(0xfe,eisa_adr+LDOORREG);
        retries = INIT_RETRIES;
        gdth_delay(20);
        while (inb(eisa_adr+EDOORREG) != 0xfe) {
            if (--retries == 0) {
                printk("GDT-EISA: Initialization error (get IRQ failed)\n");
                return 0;
            }
            gdth_delay(1);
        }
        ha->irq = inb(eisa_adr+MAILBOXREG);
        outb(0xff,eisa_adr+EDOORREG);
        TRACE2(("GDT3000/3020: IRQ=%d\n",ha->irq));
        /* check the result */
        if (ha->irq == 0) {
                TRACE2(("Unknown IRQ, use IRQ table from cmd line !\n"));
                for (i = 0, irq_found = FALSE; 
                     i < MAXHA && irq[i] != 0xff; ++i) {
                if (irq[i]==10 || irq[i]==11 || irq[i]==12 || irq[i]==14) {
                    irq_found = TRUE;
                    break;
                }
                }
            if (irq_found) {
                ha->irq = irq[i];
                irq[i] = 0;
                printk("GDT-EISA: Can not detect controller IRQ,\n");
                printk("Use IRQ setting from command line (IRQ = %d)\n",
                       ha->irq);
            } else {
                printk("GDT-EISA: Initialization error (unknown IRQ), Enable\n");
                printk("the controller BIOS or use command line parameters\n");
                return 0;
            }
        }
    } else {
        eisacf = inb(eisa_adr+EISAREG) & 7;
        if (eisacf > 4)                         /* level triggered */
            eisacf -= 4;
        ha->irq = gdth_irq_tab[eisacf];
        ha->oem_id = OEM_ID_ICP;
        ha->type = GDT_EISA;
        ha->stype = id;
    }
    return 1;
}

       
GDTH_INITFUNC(static int, gdth_init_isa(ulong32 bios_adr,gdth_ha_str *ha))
{
    register gdt2_dpram_str *dp2_ptr;
    int i;
    unchar irq_drq,prot_ver;
    ulong32 retries;

    TRACE(("gdth_init_isa() bios adr. %x\n",bios_adr));

    ha->brd = gdth_mmap(bios_adr, sizeof(gdt2_dpram_str));
    if (ha->brd == NULL) {
        printk("GDT-ISA: Initialization error (DPMEM remap error)\n");
        return 0;
    }
    dp2_ptr = (gdt2_dpram_str *)ha->brd;
    gdth_writeb(1, &dp2_ptr->io.memlock); /* switch off write protection */
    /* reset interface area */
    memset_io((char *)&dp2_ptr->u,0,sizeof(dp2_ptr->u));
    if (gdth_readl(&dp2_ptr->u) != 0) {
        printk("GDT-PCI: Initialization error (DPMEM write error)\n");
        gdth_munmap(ha->brd);
        return 0;
    }

    /* disable board interrupts, read DRQ and IRQ */
    gdth_writeb(0xff, &dp2_ptr->io.irqdel);
    gdth_writeb(0x00, &dp2_ptr->io.irqen);
    gdth_writeb(0x00, &dp2_ptr->u.ic.S_Status);
    gdth_writeb(0x00, &dp2_ptr->u.ic.Cmd_Index);

    irq_drq = gdth_readb(&dp2_ptr->io.rq);
    for (i=0; i<3; ++i) {
        if ((irq_drq & 1)==0)
            break;
        irq_drq >>= 1;
    }
    ha->drq = gdth_drq_tab[i];

    irq_drq = gdth_readb(&dp2_ptr->io.rq) >> 3;
    for (i=1; i<5; ++i) {
        if ((irq_drq & 1)==0)
            break;
        irq_drq >>= 1;
    }
    ha->irq = gdth_irq_tab[i];

    /* deinitialize services */
    gdth_writel(bios_adr, &dp2_ptr->u.ic.S_Info[0]);
    gdth_writeb(0xff, &dp2_ptr->u.ic.S_Cmd_Indx);
    gdth_writeb(0, &dp2_ptr->io.event);
    retries = INIT_RETRIES;
    gdth_delay(20);
    while (gdth_readb(&dp2_ptr->u.ic.S_Status) != 0xff) {
        if (--retries == 0) {
            printk("GDT-ISA: Initialization error (DEINIT failed)\n");
            gdth_munmap(ha->brd);
            return 0;
        }
        gdth_delay(1);
    }
    prot_ver = (unchar)gdth_readl(&dp2_ptr->u.ic.S_Info[0]);
    gdth_writeb(0, &dp2_ptr->u.ic.Status);
    gdth_writeb(0xff, &dp2_ptr->io.irqdel);
    if (prot_ver != PROTOCOL_VERSION) {
        printk("GDT-ISA: Illegal protocol version\n");
        gdth_munmap(ha->brd);
        return 0;
    }

    ha->oem_id = OEM_ID_ICP;
    ha->type = GDT_ISA;
    ha->ic_all_size = sizeof(dp2_ptr->u);
    ha->stype= GDT2_ID;
    ha->brd_phys = bios_adr >> 4;

    /* special request to controller BIOS */
    gdth_writel(0x00, &dp2_ptr->u.ic.S_Info[0]);
    gdth_writel(0x00, &dp2_ptr->u.ic.S_Info[1]);
    gdth_writel(0x01, &dp2_ptr->u.ic.S_Info[2]);
    gdth_writel(0x00, &dp2_ptr->u.ic.S_Info[3]);
    gdth_writeb(0xfe, &dp2_ptr->u.ic.S_Cmd_Indx);
    gdth_writeb(0, &dp2_ptr->io.event);
    retries = INIT_RETRIES;
    gdth_delay(20);
    while (gdth_readb(&dp2_ptr->u.ic.S_Status) != 0xfe) {
        if (--retries == 0) {
            printk("GDT-ISA: Initialization error\n");
            gdth_munmap(ha->brd);
            return 0;
        }
        gdth_delay(1);
    }
    gdth_writeb(0, &dp2_ptr->u.ic.Status);
    gdth_writeb(0xff, &dp2_ptr->io.irqdel);
    return 1;
}


GDTH_INITFUNC(static int, gdth_init_pci(gdth_pci_str *pcistr,gdth_ha_str *ha))
{
    register gdt6_dpram_str *dp6_ptr;
    register gdt6c_dpram_str *dp6c_ptr;
    register gdt6m_dpram_str *dp6m_ptr;
    ulong32 retries;
    unchar prot_ver;
    ushort command;
    int i, found = FALSE;
#if LINUX_VERSION_CODE < 0x2015C
    int rom_addr;
#endif

    TRACE(("gdth_init_pci()\n"));

    if (pcistr->vendor_id == PCI_VENDOR_ID_INTEL)
        ha->oem_id = OEM_ID_INTEL;
    else
        ha->oem_id = OEM_ID_ICP;
    ha->brd_phys = (pcistr->bus << 8) | (pcistr->device_fn & 0xf8);
    ha->stype = (ulong32)pcistr->device_id;
    ha->subdevice_id = pcistr->subdevice_id;
    ha->irq = pcistr->irq;
    
    if (ha->stype <= PCI_DEVICE_ID_VORTEX_GDT6000B) {  /* GDT6000/B */
        TRACE2(("init_pci() dpmem %lx irq %d\n",pcistr->dpmem,ha->irq));
        ha->brd = gdth_mmap(pcistr->dpmem, sizeof(gdt6_dpram_str));
        if (ha->brd == NULL) {
            printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
            return 0;
        }
        /* check and reset interface area */
        dp6_ptr = (gdt6_dpram_str *)ha->brd;
        gdth_writel(DPMEM_MAGIC, &dp6_ptr->u);
        if (gdth_readl(&dp6_ptr->u) != DPMEM_MAGIC) {
            printk("GDT-PCI: Cannot access DPMEM at 0x%lx (shadowed?)\n", 
                   pcistr->dpmem);
            found = FALSE;
            for (i = 0xC8000; i < 0xE8000; i += 0x4000) {
                gdth_munmap(ha->brd);
                ha->brd = gdth_mmap(i, sizeof(ushort)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                if (gdth_readw(ha->brd) != 0xffff) {
                    TRACE2(("init_pci_old() address 0x%x busy\n", i));
                    continue;
                }
                gdth_munmap(ha->brd);
#if LINUX_VERSION_CODE >= 0x2015C
                pci_write_config_dword(pcistr->pdev, 
                                       PCI_BASE_ADDRESS_0, i);
#else
                pcibios_write_config_dword(pcistr->bus, pcistr->device_fn,
                                           PCI_BASE_ADDRESS_0, i);
#endif
                ha->brd = gdth_mmap(i, sizeof(gdt6_dpram_str)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                dp6_ptr = (gdt6_dpram_str *)ha->brd;
                gdth_writel(DPMEM_MAGIC, &dp6_ptr->u);
                if (gdth_readl(&dp6_ptr->u) == DPMEM_MAGIC) {
                    printk("GDT-PCI: Use free address at 0x%x\n", i);
                    found = TRUE;
                    break;
                }
            }   
            if (!found) {
                printk("GDT-PCI: No free address found!\n");
                gdth_munmap(ha->brd);
                return 0;
            }
        }
        memset_io((char *)&dp6_ptr->u,0,sizeof(dp6_ptr->u));
        if (gdth_readl(&dp6_ptr->u) != 0) {
            printk("GDT-PCI: Initialization error (DPMEM write error)\n");
            gdth_munmap(ha->brd);
            return 0;
        }
        
        /* disable board interrupts, deinit services */
        gdth_writeb(0xff, &dp6_ptr->io.irqdel);
        gdth_writeb(0x00, &dp6_ptr->io.irqen);;
        gdth_writeb(0x00, &dp6_ptr->u.ic.S_Status);
        gdth_writeb(0x00, &dp6_ptr->u.ic.Cmd_Index);

        gdth_writel(pcistr->dpmem, &dp6_ptr->u.ic.S_Info[0]);
        gdth_writeb(0xff, &dp6_ptr->u.ic.S_Cmd_Indx);
        gdth_writeb(0, &dp6_ptr->io.event);
        retries = INIT_RETRIES;
        gdth_delay(20);
        while (gdth_readb(&dp6_ptr->u.ic.S_Status) != 0xff) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error (DEINIT failed)\n");
                gdth_munmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        prot_ver = (unchar)gdth_readl(&dp6_ptr->u.ic.S_Info[0]);
        gdth_writeb(0, &dp6_ptr->u.ic.S_Status);
        gdth_writeb(0xff, &dp6_ptr->io.irqdel);
        if (prot_ver != PROTOCOL_VERSION) {
            printk("GDT-PCI: Illegal protocol version\n");
            gdth_munmap(ha->brd);
            return 0;
        }

        ha->type = GDT_PCI;
        ha->ic_all_size = sizeof(dp6_ptr->u);
        
        /* special command to controller BIOS */
        gdth_writel(0x00, &dp6_ptr->u.ic.S_Info[0]);
        gdth_writel(0x00, &dp6_ptr->u.ic.S_Info[1]);
        gdth_writel(0x01, &dp6_ptr->u.ic.S_Info[2]);
        gdth_writel(0x00, &dp6_ptr->u.ic.S_Info[3]);
        gdth_writeb(0xfe, &dp6_ptr->u.ic.S_Cmd_Indx);
        gdth_writeb(0, &dp6_ptr->io.event);
        retries = INIT_RETRIES;
        gdth_delay(20);
        while (gdth_readb(&dp6_ptr->u.ic.S_Status) != 0xfe) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error\n");
                gdth_munmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        gdth_writeb(0, &dp6_ptr->u.ic.S_Status);
        gdth_writeb(0xff, &dp6_ptr->io.irqdel);

    } else if (ha->stype <= PCI_DEVICE_ID_VORTEX_GDT6555) { /* GDT6110, ... */
        ha->plx = (gdt6c_plx_regs *)pcistr->io;
        TRACE2(("init_pci_new() dpmem %lx irq %d\n",
            pcistr->dpmem,ha->irq));
        ha->brd = gdth_mmap(pcistr->dpmem, sizeof(gdt6c_dpram_str));
        if (ha->brd == NULL) {
            printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
            gdth_munmap(ha->brd);
            return 0;
        }
        /* check and reset interface area */
        dp6c_ptr = (gdt6c_dpram_str *)ha->brd;
        gdth_writel(DPMEM_MAGIC, &dp6c_ptr->u);
        if (gdth_readl(&dp6c_ptr->u) != DPMEM_MAGIC) {
            printk("GDT-PCI: Cannot access DPMEM at 0x%lx (shadowed?)\n", 
                   pcistr->dpmem);
            found = FALSE;
            for (i = 0xC8000; i < 0xE8000; i += 0x4000) {
                gdth_munmap(ha->brd);
                ha->brd = gdth_mmap(i, sizeof(ushort)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                if (gdth_readw(ha->brd) != 0xffff) {
                    TRACE2(("init_pci_plx() address 0x%x busy\n", i));
                    continue;
                }
                gdth_munmap(ha->brd);
#if LINUX_VERSION_CODE >= 0x2015C
                pci_write_config_dword(pcistr->pdev, 
                                       PCI_BASE_ADDRESS_2, i);
#else
                pcibios_write_config_dword(pcistr->bus, pcistr->device_fn,
                                           PCI_BASE_ADDRESS_2, i);
#endif
                ha->brd = gdth_mmap(i, sizeof(gdt6c_dpram_str)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                dp6c_ptr = (gdt6c_dpram_str *)ha->brd;
                gdth_writel(DPMEM_MAGIC, &dp6c_ptr->u);
                if (gdth_readl(&dp6c_ptr->u) == DPMEM_MAGIC) {
                    printk("GDT-PCI: Use free address at 0x%x\n", i);
                    found = TRUE;
                    break;
                }
            }   
            if (!found) {
                printk("GDT-PCI: No free address found!\n");
                gdth_munmap(ha->brd);
                return 0;
            }
        }
        memset_io((char *)&dp6c_ptr->u,0,sizeof(dp6c_ptr->u));
        if (gdth_readl(&dp6c_ptr->u) != 0) {
            printk("GDT-PCI: Initialization error (DPMEM write error)\n");
            gdth_munmap(ha->brd);
            return 0;
        }
        
        /* disable board interrupts, deinit services */
        outb(0x00,PTR2USHORT(&ha->plx->control1));
        outb(0xff,PTR2USHORT(&ha->plx->edoor_reg));
        
        gdth_writeb(0x00, &dp6c_ptr->u.ic.S_Status);
        gdth_writeb(0x00, &dp6c_ptr->u.ic.Cmd_Index);

        gdth_writel(pcistr->dpmem, &dp6c_ptr->u.ic.S_Info[0]);
        gdth_writeb(0xff, &dp6c_ptr->u.ic.S_Cmd_Indx);

        outb(1,PTR2USHORT(&ha->plx->ldoor_reg));

        retries = INIT_RETRIES;
        gdth_delay(20);
        while (gdth_readb(&dp6c_ptr->u.ic.S_Status) != 0xff) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error (DEINIT failed)\n");
                gdth_munmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        prot_ver = (unchar)gdth_readl(&dp6c_ptr->u.ic.S_Info[0]);
        gdth_writeb(0, &dp6c_ptr->u.ic.Status);
        if (prot_ver != PROTOCOL_VERSION) {
            printk("GDT-PCI: Illegal protocol version\n");
            gdth_munmap(ha->brd);
            return 0;
        }

        ha->type = GDT_PCINEW;
        ha->ic_all_size = sizeof(dp6c_ptr->u);

        /* special command to controller BIOS */
        gdth_writel(0x00, &dp6c_ptr->u.ic.S_Info[0]);
        gdth_writel(0x00, &dp6c_ptr->u.ic.S_Info[1]);
        gdth_writel(0x01, &dp6c_ptr->u.ic.S_Info[2]);
        gdth_writel(0x00, &dp6c_ptr->u.ic.S_Info[3]);
        gdth_writeb(0xfe, &dp6c_ptr->u.ic.S_Cmd_Indx);
        
        outb(1,PTR2USHORT(&ha->plx->ldoor_reg));

        retries = INIT_RETRIES;
        gdth_delay(20);
        while (gdth_readb(&dp6c_ptr->u.ic.S_Status) != 0xfe) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error\n");
                gdth_munmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        gdth_writeb(0, &dp6c_ptr->u.ic.S_Status);

    } else {                                            /* MPR */
        TRACE2(("init_pci_mpr() dpmem %lx irq %d\n",pcistr->dpmem,ha->irq));
        ha->brd = gdth_mmap(pcistr->dpmem, sizeof(gdt6m_dpram_str));
        if (ha->brd == NULL) {
            printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
            return 0;
        }

        /* manipulate config. space to enable DPMEM, start RP controller */
#if LINUX_VERSION_CODE >= 0x20363
        pci_read_config_word(pcistr->pdev, PCI_COMMAND, &command);
        command |= 6;
        pci_write_config_word(pcistr->pdev, PCI_COMMAND, command);
        if (pci_resource_start(pcistr->pdev, 8) == 1UL)
            pci_resource_start(pcistr->pdev, 8) = 0UL;
        i = 0xFEFF0001UL;
        pci_write_config_dword(pcistr->pdev, PCI_ROM_ADDRESS, i);
        gdth_delay(1);
        pci_write_config_dword(pcistr->pdev, PCI_ROM_ADDRESS,
                               pci_resource_start(pcistr->pdev, 8));
#elif LINUX_VERSION_CODE >= 0x2015C
        pci_read_config_word(pcistr->pdev, PCI_COMMAND, &command);
        command |= 6;
        pci_write_config_word(pcistr->pdev, PCI_COMMAND, command);
        if (pcistr->pdev->rom_address == 1UL)
            pcistr->pdev->rom_address = 0UL;
        i = 0xFEFF0001UL;
        pci_write_config_dword(pcistr->pdev, PCI_ROM_ADDRESS, i);
        gdth_delay(1);
        pci_write_config_dword(pcistr->pdev, PCI_ROM_ADDRESS,
                               pcistr->pdev->rom_address);
#else
        pcibios_read_config_word(pcistr->bus, pcistr->device_fn,
                                 PCI_COMMAND, &command);
        command |= 6;
        pcibios_write_config_word(pcistr->bus, pcistr->device_fn, 
                                  PCI_COMMAND, command);
        pcibios_read_config_dword(pcistr->bus, pcistr->device_fn,
                                  PCI_ROM_ADDRESS, &rom_addr);
        if (rom_addr == 1UL)
            rom_addr = 0UL;
        i = 0xFEFF0001UL;
        pcibios_write_config_dword(pcistr->bus, pcistr->device_fn,
                                   PCI_ROM_ADDRESS, i);
        gdth_delay(1);
        pcibios_write_config_dword(pcistr->bus, pcistr->device_fn,
                                   PCI_ROM_ADDRESS, rom_addr);
#endif
        
        /* check and reset interface area */
        dp6m_ptr = (gdt6m_dpram_str *)ha->brd;
        gdth_writel(DPMEM_MAGIC, &dp6m_ptr->u);
        if (gdth_readl(&dp6m_ptr->u) != DPMEM_MAGIC) {
            printk("GDT-PCI: Cannot access DPMEM at 0x%lx (shadowed?)\n", 
                   pcistr->dpmem);
            found = FALSE;
            for (i = 0xC8000; i < 0xE8000; i += 0x4000) {
                gdth_munmap(ha->brd);
                ha->brd = gdth_mmap(i, sizeof(ushort)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                if (gdth_readw(ha->brd) != 0xffff) {
                    TRACE2(("init_pci_mpr() address 0x%x busy\n", i));
                    continue;
                }
                gdth_munmap(ha->brd);
#if LINUX_VERSION_CODE >= 0x2015C
                pci_write_config_dword(pcistr->pdev, 
                                       PCI_BASE_ADDRESS_0, i);
#else
                pcibios_write_config_dword(pcistr->bus, pcistr->device_fn,
                                           PCI_BASE_ADDRESS_0, i);
#endif
                ha->brd = gdth_mmap(i, sizeof(gdt6m_dpram_str)); 
                if (ha->brd == NULL) {
                    printk("GDT-PCI: Initialization error (DPMEM remap error)\n");
                    return 0;
                }
                dp6m_ptr = (gdt6m_dpram_str *)ha->brd;
                gdth_writel(DPMEM_MAGIC, &dp6m_ptr->u);
                if (gdth_readl(&dp6m_ptr->u) == DPMEM_MAGIC) {
                    printk("GDT-PCI: Use free address at 0x%x\n", i);
                    found = TRUE;
                    break;
                }
            }   
            if (!found) {
                printk("GDT-PCI: No free address found!\n");
                gdth_munmap(ha->brd);
                return 0;
            }
        }
        memset_io((char *)&dp6m_ptr->u,0,sizeof(dp6m_ptr->u));
        
        /* disable board interrupts, deinit services */
        gdth_writeb(gdth_readb(&dp6m_ptr->i960r.edoor_en_reg) | 4,
                    &dp6m_ptr->i960r.edoor_en_reg);
        gdth_writeb(0xff, &dp6m_ptr->i960r.edoor_reg);
        gdth_writeb(0x00, &dp6m_ptr->u.ic.S_Status);
        gdth_writeb(0x00, &dp6m_ptr->u.ic.Cmd_Index);

        gdth_writel(pcistr->dpmem, &dp6m_ptr->u.ic.S_Info[0]);
        gdth_writeb(0xff, &dp6m_ptr->u.ic.S_Cmd_Indx);
        gdth_writeb(1, &dp6m_ptr->i960r.ldoor_reg);
        retries = INIT_RETRIES;
        gdth_delay(20);
        while (gdth_readb(&dp6m_ptr->u.ic.S_Status) != 0xff) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error (DEINIT failed)\n");
                gdth_munmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        prot_ver = (unchar)gdth_readl(&dp6m_ptr->u.ic.S_Info[0]);
        gdth_writeb(0, &dp6m_ptr->u.ic.S_Status);
        if (prot_ver != PROTOCOL_VERSION) {
            printk("GDT-PCI: Illegal protocol version\n");
            gdth_munmap(ha->brd);
            return 0;
        }

        ha->type = GDT_PCIMPR;
        ha->ic_all_size = sizeof(dp6m_ptr->u);
        
        /* special command to controller BIOS */
        gdth_writel(0x00, &dp6m_ptr->u.ic.S_Info[0]);
        gdth_writel(0x00, &dp6m_ptr->u.ic.S_Info[1]);
        gdth_writel(0x01, &dp6m_ptr->u.ic.S_Info[2]);
        gdth_writel(0x00, &dp6m_ptr->u.ic.S_Info[3]);
        gdth_writeb(0xfe, &dp6m_ptr->u.ic.S_Cmd_Indx);
        gdth_writeb(1, &dp6m_ptr->i960r.ldoor_reg);
        retries = INIT_RETRIES;
        gdth_delay(20);
        while (gdth_readb(&dp6m_ptr->u.ic.S_Status) != 0xfe) {
            if (--retries == 0) {
                printk("GDT-PCI: Initialization error\n");
                gdth_munmap(ha->brd);
                return 0;
            }
            gdth_delay(1);
        }
        gdth_writeb(0, &dp6m_ptr->u.ic.S_Status);
    }

    return 1;
}


/* controller protocol functions */

GDTH_INITFUNC(static void, gdth_enable_int(int hanum))
{
    gdth_ha_str *ha;
    ulong flags;
    gdt2_dpram_str *dp2_ptr;
    gdt6_dpram_str *dp6_ptr;
    gdt6m_dpram_str *dp6m_ptr;

    TRACE(("gdth_enable_int() hanum %d\n",hanum));
    ha = HADATA(gdth_ctr_tab[hanum]);
    GDTH_LOCK_HA(ha, flags);

    if (ha->type == GDT_EISA) {
        outb(0xff, ha->bmic + EDOORREG);
        outb(0xff, ha->bmic + EDENABREG);
        outb(0x01, ha->bmic + EINTENABREG);
    } else if (ha->type == GDT_ISA) {
        dp2_ptr = (gdt2_dpram_str *)ha->brd;
        gdth_writeb(1, &dp2_ptr->io.irqdel);
        gdth_writeb(0, &dp2_ptr->u.ic.Cmd_Index);
        gdth_writeb(1, &dp2_ptr->io.irqen);
    } else if (ha->type == GDT_PCI) {
        dp6_ptr = (gdt6_dpram_str *)ha->brd;
        gdth_writeb(1, &dp6_ptr->io.irqdel);
        gdth_writeb(0, &dp6_ptr->u.ic.Cmd_Index);
        gdth_writeb(1, &dp6_ptr->io.irqen);
    } else if (ha->type == GDT_PCINEW) {
        outb(0xff, PTR2USHORT(&ha->plx->edoor_reg));
        outb(0x03, PTR2USHORT(&ha->plx->control1));
    } else if (ha->type == GDT_PCIMPR) {
        dp6m_ptr = (gdt6m_dpram_str *)ha->brd;
        gdth_writeb(0xff, &dp6m_ptr->i960r.edoor_reg);
        gdth_writeb(gdth_readb(&dp6m_ptr->i960r.edoor_en_reg) & ~4,
                    &dp6m_ptr->i960r.edoor_en_reg);
    }
    GDTH_UNLOCK_HA(ha, flags);
}


static int gdth_get_status(unchar *pIStatus,int irq)
{
    register gdth_ha_str *ha;
    int i;

    TRACE(("gdth_get_status() irq %d ctr_count %d\n",
           irq,gdth_ctr_count));
    
    *pIStatus = 0;
    for (i=0; i<gdth_ctr_count; ++i) {
        ha = HADATA(gdth_ctr_tab[i]);
        if (ha->irq != (unchar)irq)             /* check IRQ */
            continue;
        if (ha->type == GDT_EISA)
            *pIStatus = inb((ushort)ha->bmic + EDOORREG);
        else if (ha->type == GDT_ISA)
            *pIStatus =
                gdth_readb(&((gdt2_dpram_str *)ha->brd)->u.ic.Cmd_Index);
        else if (ha->type == GDT_PCI)
            *pIStatus =
                gdth_readb(&((gdt6_dpram_str *)ha->brd)->u.ic.Cmd_Index);
        else if (ha->type == GDT_PCINEW) 
            *pIStatus = inb(PTR2USHORT(&ha->plx->edoor_reg));
        else if (ha->type == GDT_PCIMPR)
            *pIStatus =
                gdth_readb(&((gdt6m_dpram_str *)ha->brd)->i960r.edoor_reg);
   
        if (*pIStatus)                                  
            return i;                           /* board found */
    }
    return -1;
}
                 
    
static int gdth_test_busy(int hanum)
{
    register gdth_ha_str *ha;
    register int gdtsema0 = 0;

    TRACE(("gdth_test_busy() hanum %d\n",hanum));
    
    ha = HADATA(gdth_ctr_tab[hanum]);
    if (ha->type == GDT_EISA)
        gdtsema0 = (int)inb(ha->bmic + SEMA0REG);
    else if (ha->type == GDT_ISA)
        gdtsema0 = (int)gdth_readb(&((gdt2_dpram_str *)ha->brd)->u.ic.Sema0);
    else if (ha->type == GDT_PCI)
        gdtsema0 = (int)gdth_readb(&((gdt6_dpram_str *)ha->brd)->u.ic.Sema0);
    else if (ha->type == GDT_PCINEW) 
        gdtsema0 = (int)inb(PTR2USHORT(&ha->plx->sema0_reg));
    else if (ha->type == GDT_PCIMPR)
        gdtsema0 = 
            (int)gdth_readb(&((gdt6m_dpram_str *)ha->brd)->i960r.sema0_reg);

    return (gdtsema0 & 1);
}


static int gdth_get_cmd_index(int hanum)
{
    register gdth_ha_str *ha;
    int i;

    TRACE(("gdth_get_cmd_index() hanum %d\n",hanum));

    ha = HADATA(gdth_ctr_tab[hanum]);
    for (i=0; i<GDTH_MAXCMDS; ++i) {
        if (ha->cmd_tab[i].cmnd == UNUSED_CMND) {
            ha->cmd_tab[i].cmnd = ha->pccb->RequestBuffer;
            ha->cmd_tab[i].service = ha->pccb->Service;
            ha->pccb->CommandIndex = (ulong32)i+2;
            return (i+2);
        }
    }
    return 0;
}


static void gdth_set_sema0(int hanum)
{
    register gdth_ha_str *ha;

    TRACE(("gdth_set_sema0() hanum %d\n",hanum));

    ha = HADATA(gdth_ctr_tab[hanum]);
    if (ha->type == GDT_EISA) {
        outb(1, ha->bmic + SEMA0REG);
    } else if (ha->type == GDT_ISA) {
        gdth_writeb(1, &((gdt2_dpram_str *)ha->brd)->u.ic.Sema0);
    } else if (ha->type == GDT_PCI) {
        gdth_writeb(1, &((gdt6_dpram_str *)ha->brd)->u.ic.Sema0);
    } else if (ha->type == GDT_PCINEW) { 
        outb(1, PTR2USHORT(&ha->plx->sema0_reg));
    } else if (ha->type == GDT_PCIMPR) {
        gdth_writeb(1, &((gdt6m_dpram_str *)ha->brd)->i960r.sema0_reg);
    }
}


static void gdth_copy_command(int hanum)
{
    register gdth_ha_str *ha;
    register gdth_cmd_str *cmd_ptr;
    register gdt6m_dpram_str *dp6m_ptr;
    register gdt6c_dpram_str *dp6c_ptr;
    gdt6_dpram_str *dp6_ptr;
    gdt2_dpram_str *dp2_ptr;
    ushort cp_count,dp_offset,cmd_no;
    
    TRACE(("gdth_copy_command() hanum %d\n",hanum));

    ha = HADATA(gdth_ctr_tab[hanum]);
    cp_count = ha->cmd_len;
    dp_offset= ha->cmd_offs_dpmem;
    cmd_no   = ha->cmd_cnt;
    cmd_ptr  = ha->pccb;

    ++ha->cmd_cnt;                                                      
    if (ha->type == GDT_EISA)
        return;                                 /* no DPMEM, no copy */

    /* set cpcount dword aligned */
    if (cp_count & 3)
        cp_count += (4 - (cp_count & 3));

    ha->cmd_offs_dpmem += cp_count;
    
    /* set offset and service, copy command to DPMEM */
    if (ha->type == GDT_ISA) {
        dp2_ptr = (gdt2_dpram_str *)ha->brd;
        gdth_writew(dp_offset + DPMEM_COMMAND_OFFSET, 
                    &dp2_ptr->u.ic.comm_queue[cmd_no].offset);
        gdth_writew((ushort)cmd_ptr->Service, 
                    &dp2_ptr->u.ic.comm_queue[cmd_no].serv_id);
        memcpy_toio(&dp2_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_count);
    } else if (ha->type == GDT_PCI) {
        dp6_ptr = (gdt6_dpram_str *)ha->brd;
        gdth_writew(dp_offset + DPMEM_COMMAND_OFFSET, 
                    &dp6_ptr->u.ic.comm_queue[cmd_no].offset);
        gdth_writew((ushort)cmd_ptr->Service, 
                    &dp6_ptr->u.ic.comm_queue[cmd_no].serv_id);
        memcpy_toio(&dp6_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_count);
    } else if (ha->type == GDT_PCINEW) {
        dp6c_ptr = (gdt6c_dpram_str *)ha->brd;
        gdth_writew(dp_offset + DPMEM_COMMAND_OFFSET, 
                    &dp6c_ptr->u.ic.comm_queue[cmd_no].offset);
        gdth_writew((ushort)cmd_ptr->Service, 
                    &dp6c_ptr->u.ic.comm_queue[cmd_no].serv_id);
        memcpy_toio(&dp6c_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_count);
    } else if (ha->type == GDT_PCIMPR) {
        dp6m_ptr = (gdt6m_dpram_str *)ha->brd;
        gdth_writew(dp_offset + DPMEM_COMMAND_OFFSET, 
                    &dp6m_ptr->u.ic.comm_queue[cmd_no].offset);
        gdth_writew((ushort)cmd_ptr->Service, 
                    &dp6m_ptr->u.ic.comm_queue[cmd_no].serv_id);
        memcpy_toio(&dp6m_ptr->u.ic.gdt_dpr_cmd[dp_offset],cmd_ptr,cp_count);
    }
}


static void gdth_release_event(int hanum)
{
    register gdth_ha_str *ha;

    TRACE(("gdth_release_event() hanum %d\n",hanum));
    ha = HADATA(gdth_ctr_tab[hanum]);

#ifdef GDTH_STATISTICS
    {
        ulong32 i,j;
        for (i=0,j=0; j<GDTH_MAXCMDS; ++j) {
            if (ha->cmd_tab[j].cmnd != UNUSED_CMND)
                ++i;
        }
        if (max_index < i) {
            max_index = i;
            TRACE3(("GDT: max_index = %d\n",(ushort)i));
        }
    }
#endif

    if (ha->pccb->OpCode == GDT_INIT)
        ha->pccb->Service |= 0x80;

    if (ha->type == GDT_EISA) {
        if (ha->pccb->OpCode == GDT_INIT)               /* store DMA buffer */
            outl(virt_to_bus(ha->pccb), ha->bmic + MAILBOXREG);
        outb(ha->pccb->Service, ha->bmic + LDOORREG);
    } else if (ha->type == GDT_ISA) {
        gdth_writeb(0, &((gdt2_dpram_str *)ha->brd)->io.event);
    } else if (ha->type == GDT_PCI) {
        gdth_writeb(0, &((gdt6_dpram_str *)ha->brd)->io.event);
    } else if (ha->type == GDT_PCINEW) { 
        outb(1, PTR2USHORT(&ha->plx->ldoor_reg));
    } else if (ha->type == GDT_PCIMPR) {
        gdth_writeb(1, &((gdt6m_dpram_str *)ha->brd)->i960r.ldoor_reg);
    }
}

    
static int gdth_wait(int hanum,int index,ulong32 time)
{
    gdth_ha_str *ha;
    int answer_found = FALSE;

    TRACE(("gdth_wait() hanum %d index %d time %d\n",hanum,index,time));

    ha = HADATA(gdth_ctr_tab[hanum]);
    if (index == 0)
        return 1;                               /* no wait required */

    gdth_from_wait = TRUE;
    do {
#if LINUX_VERSION_CODE >= 0x010346
        gdth_interrupt((int)ha->irq,ha,NULL);
#else
        gdth_interrupt((int)ha->irq,NULL);
#endif
        if (wait_hanum==hanum && wait_index==index) {
            answer_found = TRUE;
            break;
        }
        gdth_delay(1);
    } while (--time);
    gdth_from_wait = FALSE;
    
    while (gdth_test_busy(hanum))
        gdth_delay(0);

    return (answer_found);
}


static int gdth_internal_cmd(int hanum,unchar service,ushort opcode,ulong32 p1,
                             ulong32 p2,ulong32 p3)
{
    register gdth_ha_str *ha;
    register gdth_cmd_str *cmd_ptr;
    int retries,index;

    TRACE2(("gdth_internal_cmd() service %d opcode %d\n",service,opcode));

    ha = HADATA(gdth_ctr_tab[hanum]);
    cmd_ptr = ha->pccb;
    memset((char*)cmd_ptr,0,sizeof(gdth_cmd_str));

    /* make command  */
    for (retries = INIT_RETRIES;;) {
        cmd_ptr->Service          = service;
        cmd_ptr->RequestBuffer    = INTERNAL_CMND;
        if (!(index=gdth_get_cmd_index(hanum))) {
            TRACE(("GDT: No free command index found\n"));
            return 0;
        }
        gdth_set_sema0(hanum);
        cmd_ptr->OpCode           = opcode;
        cmd_ptr->BoardNode        = LOCALBOARD;
        if (service == CACHESERVICE) {
            if (opcode == GDT_IOCTL) {
                cmd_ptr->u.ioctl.subfunc = p1;
                cmd_ptr->u.ioctl.channel = p2;
                cmd_ptr->u.ioctl.param_size = (ushort)p3;
                cmd_ptr->u.ioctl.p_param = virt_to_bus(ha->pscratch);
            } else {
                cmd_ptr->u.cache.DeviceNo = (ushort)p1;
                cmd_ptr->u.cache.BlockNo  = p2;
            }
        } else if (service == SCSIRAWSERVICE) {
            cmd_ptr->u.raw.direction  = p1;
            cmd_ptr->u.raw.bus        = (unchar)p2;
            cmd_ptr->u.raw.target     = (unchar)p3;
            cmd_ptr->u.raw.lun        = (unchar)(p3 >> 8);
        } else if (service == SCREENSERVICE) {
            if (opcode == GDT_REALTIME) {
                *(ulong32 *)&cmd_ptr->u.screen.su.data[0] = p1;
                *(ulong32 *)&cmd_ptr->u.screen.su.data[4] = p2;
                *(ulong32 *)&cmd_ptr->u.screen.su.data[8] = p3;
            }
        }
        ha->cmd_len          = sizeof(gdth_cmd_str);
        ha->cmd_offs_dpmem   = 0;
        ha->cmd_cnt          = 0;
        gdth_copy_command(hanum);
        gdth_release_event(hanum);
        gdth_delay(20);
        if (!gdth_wait(hanum,index,INIT_TIMEOUT)) {
            printk("GDT: Initialization error (timeout service %d)\n",service);
            return 0;
        }
        if (ha->status != S_BSY || --retries == 0)
            break;
        gdth_delay(1);   
    }   
    
    return (ha->status != S_OK ? 0:1);
}
    

/* search for devices */

GDTH_INITFUNC(static int, gdth_search_drives(int hanum))
{
    register gdth_ha_str *ha;
    ushort cdev_cnt, i;
    ulong32 bus_no, drv_cnt, drv_no, j;
    gdth_getch_str *chn;
    gdth_drlist_str *drl;
    gdth_iochan_str *ioc;
    gdth_raw_iochan_str *iocr;
    gdth_arcdl_str *alst;
    gdth_alist_str *alst2;
#ifdef GDTH_RTC
    unchar rtc[12];
    ulong flags;
#endif     
   
    TRACE(("gdth_search_drives() hanum %d\n",hanum));
    ha = HADATA(gdth_ctr_tab[hanum]);

    /* initialize controller services, at first: screen service */
    if (!gdth_internal_cmd(hanum,SCREENSERVICE,GDT_INIT,0,0,0)) {
        printk("GDT: Initialization error screen service (code %d)\n",
               ha->status);
        return 0;
    }
    TRACE2(("gdth_search_drives(): SCREENSERVICE initialized\n"));

#ifdef GDTH_RTC
    /* read realtime clock info, send to controller */
    /* 1. wait for the falling edge of update flag */
    spin_lock_irqsave(&rtc_lock, flags);
    for (j = 0; j < 1000000; ++j)
        if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
            break;
    for (j = 0; j < 1000000; ++j)
        if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
            break;
    /* 2. read info */
    do {
        for (j = 0; j < 12; ++j) 
            rtc[j] = CMOS_READ(j);
    } while (rtc[0] != CMOS_READ(0));
    spin_lock_irqrestore(&rtc_lock, flags);
    TRACE2(("gdth_search_drives(): RTC: %x/%x/%x\n",*(ulong32 *)&rtc[0],
            *(ulong32 *)&rtc[4], *(ulong32 *)&rtc[8]));
    /* 3. send to controller firmware */
    gdth_internal_cmd(hanum,SCREENSERVICE,GDT_REALTIME, *(ulong32 *)&rtc[0],
                      *(ulong32 *)&rtc[4], *(ulong32 *)&rtc[8]);
#endif  
 
    /* unfreeze all IOs */
    gdth_internal_cmd(hanum,CACHESERVICE,GDT_UNFREEZE_IO,0,0,0);
 
    /* initialize cache service */
    if (!gdth_internal_cmd(hanum,CACHESERVICE,GDT_INIT,LINUX_OS,0,0)) {
        printk("GDT: Initialization error cache service (code %d)\n",
               ha->status);
        return 0;
    }
    TRACE2(("gdth_search_drives(): CACHESERVICE initialized\n"));
    cdev_cnt = (ushort)ha->info;
    ha->fw_vers = ha->service;

    /* detect number of buses - try new IOCTL */
    iocr = (gdth_raw_iochan_str *)ha->pscratch;
    iocr->hdr.version        = 0xffffffff;
    iocr->hdr.list_entries   = MAXBUS;
    iocr->hdr.first_chan     = 0;
    iocr->hdr.last_chan      = MAXBUS-1;
    iocr->hdr.list_offset    = GDTOFFSOF(gdth_raw_iochan_str, list[0]);
    if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,IOCHAN_RAW_DESC,
                          INVALID_CHANNEL,sizeof(gdth_raw_iochan_str))) {
        TRACE2(("IOCHAN_RAW_DESC supported!\n"));
        ha->bus_cnt = iocr->hdr.chan_count;
        for (bus_no = 0; bus_no < ha->bus_cnt; ++bus_no) {
            if (iocr->list[bus_no].proc_id < MAXID)
                ha->bus_id[bus_no] = iocr->list[bus_no].proc_id;
            else
                ha->bus_id[bus_no] = 0xff;
        }
    } else {
        /* old method */
        chn = (gdth_getch_str *)ha->pscratch;
        for (bus_no = 0; bus_no < MAXBUS; ++bus_no) {
            chn->channel_no = bus_no;
            if (!gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,
                                   SCSI_CHAN_CNT | L_CTRL_PATTERN,
                                   IO_CHANNEL | INVALID_CHANNEL,
                                   sizeof(gdth_getch_str))) {
                if (bus_no == 0) {
                    printk("GDT: Error detecting channel count (0x%x)\n",
                           ha->status);
                    return 0;
                }
                break;
            }
            if (chn->siop_id < MAXID)
                ha->bus_id[bus_no] = chn->siop_id;
            else
                ha->bus_id[bus_no] = 0xff;
        }       
        ha->bus_cnt = (unchar)bus_no;
    }
    TRACE2(("gdth_search_drives() %d channels\n",ha->bus_cnt));

    /* read cache configuration */
    if (!gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,CACHE_INFO,
                           INVALID_CHANNEL,sizeof(gdth_cinfo_str))) {
        printk("GDT: Initialization error cache service (code %d)\n",
               ha->status);
        return 0;
    }
    ha->cpar = ((gdth_cinfo_str *)ha->pscratch)->cpar;
    TRACE2(("gdth_search_drives() cinfo: vs %x sta %d str %d dw %d b %d\n",
            ha->cpar.version,ha->cpar.state,ha->cpar.strategy,
            ha->cpar.write_back,ha->cpar.block_size));

    /* read board info and features */
    ha->more_proc = FALSE;
    if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,BOARD_INFO,
                          INVALID_CHANNEL,sizeof(gdth_binfo_str))) {
        memcpy(&ha->binfo, (gdth_binfo_str *)ha->pscratch,
               sizeof(gdth_binfo_str));
        if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,BOARD_FEATURES,
                              INVALID_CHANNEL,sizeof(gdth_bfeat_str))) {
            TRACE2(("BOARD_INFO/BOARD_FEATURES supported\n"));
            ha->bfeat = *(gdth_bfeat_str *)ha->pscratch;
            ha->more_proc = TRUE;
        }
    } else {
        TRACE2(("BOARD_INFO requires firmware >= 1.10/2.08\n"));
        strcpy(ha->binfo.type_string, gdth_ctr_name(hanum));
    }
    TRACE2(("Controller name: %s\n",ha->binfo.type_string));

    /* read more informations */
    if (ha->more_proc) {
        /* physical drives, channel addresses */
        ioc = (gdth_iochan_str *)ha->pscratch;
        ioc->hdr.version        = 0xffffffff;
        ioc->hdr.list_entries   = MAXBUS;
        ioc->hdr.first_chan     = 0;
        ioc->hdr.last_chan      = MAXBUS-1;
        ioc->hdr.list_offset    = GDTOFFSOF(gdth_iochan_str, list[0]);
        if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,IOCHAN_DESC,
                              INVALID_CHANNEL,sizeof(gdth_iochan_str))) {
            for (bus_no = 0; bus_no < ha->bus_cnt; ++bus_no) {
                ha->raw[bus_no].address = ioc->list[bus_no].address;
                ha->raw[bus_no].local_no = ioc->list[bus_no].local_no;
            }
        } else {
            for (bus_no = 0; bus_no < ha->bus_cnt; ++bus_no) {
                ha->raw[bus_no].address = IO_CHANNEL;
                ha->raw[bus_no].local_no = bus_no;
            }
        }
        for (bus_no = 0; bus_no < ha->bus_cnt; ++bus_no) {
            chn = (gdth_getch_str *)ha->pscratch;
            chn->channel_no = ha->raw[bus_no].local_no;
            if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,
                                  SCSI_CHAN_CNT | L_CTRL_PATTERN,
                                  ha->raw[bus_no].address | INVALID_CHANNEL,
                                  sizeof(gdth_getch_str))) {
                ha->raw[bus_no].pdev_cnt = chn->drive_cnt;
                TRACE2(("Channel %d: %d phys. drives\n",
                        bus_no,chn->drive_cnt));
            }
            if (ha->raw[bus_no].pdev_cnt > 0) {
                drl = (gdth_drlist_str *)ha->pscratch;
                drl->sc_no = ha->raw[bus_no].local_no;
                drl->sc_cnt = ha->raw[bus_no].pdev_cnt;
                if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,
                                      SCSI_DR_LIST | L_CTRL_PATTERN,
                                      ha->raw[bus_no].address | INVALID_CHANNEL,
                                      sizeof(gdth_drlist_str))) {
                    for (j = 0; j < ha->raw[bus_no].pdev_cnt; ++j) 
                        ha->raw[bus_no].id_list[j] = drl->sc_list[j];
                } else {
                    ha->raw[bus_no].pdev_cnt = 0;
                }
            }
        }

        /* logical drives */
        if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,CACHE_DRV_CNT,
                              INVALID_CHANNEL,sizeof(ulong32))) {
            drv_cnt = *(ulong32 *)ha->pscratch;
            if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,CACHE_DRV_LIST,
                                  INVALID_CHANNEL,drv_cnt * sizeof(ulong32))) {
                for (j = 0; j < drv_cnt; ++j) {
                    drv_no = ((ulong32 *)ha->pscratch)[j];
                    if (drv_no < MAX_LDRIVES) {
                        ha->hdr[drv_no].is_logdrv = TRUE;
                        TRACE2(("Drive %d is log. drive\n",drv_no));
                    }
                }
            }
            alst = (gdth_arcdl_str *)ha->pscratch;
            alst->entries_avail = MAX_LDRIVES;
            alst->first_entry = 0;
            alst->list_offset = GDTOFFSOF(gdth_arcdl_str, list[0]);
            if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,
                                  ARRAY_DRV_LIST2 | LA_CTRL_PATTERN, 
                                  INVALID_CHANNEL, sizeof(gdth_arcdl_str) +
                                  (alst->entries_avail-1) * sizeof(gdth_alist_str))) { 
                for (j = 0; j < alst->entries_init; ++j) {
                    ha->hdr[j].is_arraydrv = alst->list[j].is_arrayd;
                    ha->hdr[j].is_master = alst->list[j].is_master;
                    ha->hdr[j].is_parity = alst->list[j].is_parity;
                    ha->hdr[j].is_hotfix = alst->list[j].is_hotfix;
                    ha->hdr[j].master_no = alst->list[j].cd_handle;
                }
            } else if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_IOCTL,
                                         ARRAY_DRV_LIST | LA_CTRL_PATTERN,
                                         0, 35 * sizeof(gdth_alist_str))) {
                for (j = 0; j < 35; ++j) {
                    alst2 = &((gdth_alist_str *)ha->pscratch)[j];
                    ha->hdr[j].is_arraydrv = alst2->is_arrayd;
                    ha->hdr[j].is_master = alst2->is_master;
                    ha->hdr[j].is_parity = alst2->is_parity;
                    ha->hdr[j].is_hotfix = alst2->is_hotfix;
                    ha->hdr[j].master_no = alst2->cd_handle;
                }
            }
        }
    }       
                                  
    /* initialize raw service */
    if (!gdth_internal_cmd(hanum,SCSIRAWSERVICE,GDT_INIT,0,0,0)) {
        printk("GDT: Initialization error raw service (code %d)\n",
               ha->status);
        return 0;
    }
    TRACE2(("gdth_search_drives(): RAWSERVICE initialized\n"));

    /* set/get features raw service (scatter/gather) */
    if (gdth_internal_cmd(hanum,SCSIRAWSERVICE,GDT_SET_FEAT,SCATTER_GATHER,
                          0,0)) {
        TRACE2(("gdth_search_drives(): set features RAWSERVICE OK\n"));
        if (gdth_internal_cmd(hanum,SCSIRAWSERVICE,GDT_GET_FEAT,0,0,0)) {
            TRACE2(("gdth_search_dr(): get feat RAWSERVICE %d\n",
                    ha->info));
            ha->raw_feat = (ushort)ha->info;
        }
    } 

    /* set/get features cache service (equal to raw service) */
    if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_SET_FEAT,0,
                          SCATTER_GATHER,0)) {
        TRACE2(("gdth_search_drives(): set features CACHESERVICE OK\n"));
        if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_GET_FEAT,0,0,0)) {
            TRACE2(("gdth_search_dr(): get feat CACHESERV. %d\n",
                    ha->info));
            ha->cache_feat = (ushort)ha->info;
        }
    }

    /* reserve drives for raw service */
    if (reserve_mode != 0) {
        gdth_internal_cmd(hanum,SCSIRAWSERVICE,GDT_RESERVE_ALL,
                          reserve_mode == 1 ? 1 : 3, 0, 0);
        TRACE2(("gdth_search_drives(): RESERVE_ALL code %d\n", 
                ha->status));
    }
    for (i = 0; i < MAX_RES_ARGS; i += 4) {
        if (reserve_list[i] == hanum && reserve_list[i+1] < ha->bus_cnt && 
            reserve_list[i+2] < ha->tid_cnt && reserve_list[i+3] < MAXLUN) {
            TRACE2(("gdth_search_drives(): reserve ha %d bus %d id %d lun %d\n",
                    reserve_list[i], reserve_list[i+1],
                    reserve_list[i+2], reserve_list[i+3]));
            if (!gdth_internal_cmd(hanum,SCSIRAWSERVICE,GDT_RESERVE,0,
                                   reserve_list[i+1], reserve_list[i+2] | 
                                   (reserve_list[i+3] << 8))) {
                printk("GDT: Error raw service (RESERVE, code %d)\n",
                       ha->status);
             }
        }
    }

    /* scanning for host drives */
    for (i = 0; i < cdev_cnt; ++i) 
        gdth_analyse_hdrive(hanum,i);
    
    TRACE(("gdth_search_drives() OK\n"));
    return 1;
}

static int gdth_analyse_hdrive(int hanum,ushort hdrive)
{
    register gdth_ha_str *ha;
    int drv_cyls, drv_hds, drv_secs;

    TRACE(("gdth_analyse_hdrive() hanum %d drive %d\n",hanum,hdrive));
    if (hdrive >= MAX_HDRIVES)
        return 0;
    ha = HADATA(gdth_ctr_tab[hanum]);

    if (!gdth_internal_cmd(hanum,CACHESERVICE,GDT_INFO,hdrive,0,0)) 
        return 0;
    ha->hdr[hdrive].present = TRUE;
    ha->hdr[hdrive].size = ha->info;

    /* evaluate mapping (sectors per head, heads per cylinder) */
    ha->hdr[hdrive].size &= ~SECS32;
    if (ha->info2 == 0) {
        gdth_eval_mapping(ha->hdr[hdrive].size,&drv_cyls,&drv_hds,&drv_secs);
    } else {
        drv_hds = ha->info2 & 0xff;
        drv_secs = (ha->info2 >> 8) & 0xff;
        drv_cyls = ha->hdr[hdrive].size /drv_hds/drv_secs;
    }
    ha->hdr[hdrive].heads = (unchar)drv_hds;
    ha->hdr[hdrive].secs  = (unchar)drv_secs;
    /* round size */
    ha->hdr[hdrive].size  = drv_cyls * drv_hds * drv_secs;
    TRACE2(("gdth_search_dr() cdr. %d size %d hds %d scs %d\n",
            hdrive,ha->hdr[hdrive].size,drv_hds,drv_secs));
            
    /* get informations about device */
    if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_DEVTYPE,hdrive,0,0)) {
        TRACE2(("gdth_search_dr() cache drive %d devtype %d\n",
                hdrive,ha->info));
        ha->hdr[hdrive].devtype = (ushort)ha->info;
    }

    /* cluster info */
    if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_CLUST_INFO,hdrive,0,0)) {
        TRACE2(("gdth_search_dr() cache drive %d cluster info %d\n",
                hdrive,ha->info));
        if (!shared_access)
            ha->hdr[hdrive].cluster_type = (unchar)ha->info;
    }

    /* R/W attributes */
    if (gdth_internal_cmd(hanum,CACHESERVICE,GDT_RW_ATTRIBS,hdrive,0,0)) {
        TRACE2(("gdth_search_dr() cache drive %d r/w attrib. %d\n",
                hdrive,ha->info));
        ha->hdr[hdrive].rw_attribs = (unchar)ha->info;
    }

    return 1;
}


/* command queueing/sending functions */

static void gdth_putq(int hanum,Scsi_Cmnd *scp,unchar priority)
{
    register gdth_ha_str *ha;
    register Scsi_Cmnd *pscp;
    register Scsi_Cmnd *nscp;
    ulong flags;
    unchar b, t;

    TRACE(("gdth_putq() priority %d\n",priority));
    ha = HADATA(gdth_ctr_tab[hanum]);
    GDTH_LOCK_HA(ha, flags);

    scp->SCp.this_residual = (int)priority;
    b = virt_ctr ? NUMDATA(scp->host)->busnum : scp->channel;
    t = scp->target;
#if LINUX_VERSION_CODE >= 0x010300
    if (priority >= DEFAULT_PRI) {
        if ((b != ha->virt_bus && ha->raw[BUS_L2P(ha,b)].lock) ||
            (b == ha->virt_bus && t < MAX_HDRIVES && ha->hdr[t].lock)) {
            TRACE2(("gdth_putq(): locked IO -> update_timeout()\n"));
            scp->SCp.buffers_residual = gdth_update_timeout(hanum, scp, 0);
        }
    }
#endif

    if (ha->req_first==NULL) {
        ha->req_first = scp;                    /* queue was empty */
        scp->SCp.ptr = NULL;
    } else {                                    /* queue not empty */
        pscp = ha->req_first;
        nscp = (Scsi_Cmnd *)pscp->SCp.ptr;
        /* priority: 0-highest,..,0xff-lowest */
        while (nscp && (unchar)nscp->SCp.this_residual <= priority) {
            pscp = nscp;
            nscp = (Scsi_Cmnd *)pscp->SCp.ptr;
        }
        pscp->SCp.ptr = (char *)scp;
        scp->SCp.ptr  = (char *)nscp;
    }
    GDTH_UNLOCK_HA(ha, flags);

#ifdef GDTH_STATISTICS
    flags = 0;
    for (nscp=ha->req_first; nscp; nscp=(Scsi_Cmnd*)nscp->SCp.ptr)
        ++flags;
    if (max_rq < flags) {
        max_rq = flags;
        TRACE3(("GDT: max_rq = %d\n",(ushort)max_rq));
    }
#endif
}

static void gdth_next(int hanum)
{
    register gdth_ha_str *ha;
    register Scsi_Cmnd *pscp;
    register Scsi_Cmnd *nscp;
    unchar b, t, firsttime;
    unchar this_cmd, next_cmd;
    ulong flags = 0;
    int cmd_index;

    TRACE(("gdth_next() hanum %d\n",hanum));
    ha = HADATA(gdth_ctr_tab[hanum]);
    if (!gdth_polling) 
    	GDTH_LOCK_HA(ha, flags);

    ha->cmd_cnt = ha->cmd_offs_dpmem = 0;
    this_cmd = firsttime = TRUE;
    next_cmd = gdth_polling ? FALSE:TRUE;
    cmd_index = 0;

    for (nscp = pscp = ha->req_first; nscp; nscp = (Scsi_Cmnd *)nscp->SCp.ptr) {
        if (nscp != pscp && nscp != (Scsi_Cmnd *)pscp->SCp.ptr)
            pscp = (Scsi_Cmnd *)pscp->SCp.ptr;
        b = virt_ctr ? NUMDATA(nscp->host)->busnum : nscp->channel;
        t = nscp->target;
        if (nscp->SCp.this_residual >= DEFAULT_PRI) {
            if ((b != ha->virt_bus && ha->raw[BUS_L2P(ha,b)].lock) ||
                (b == ha->virt_bus && t < MAX_HDRIVES && ha->hdr[t].lock)) 
                continue;
        }

        if (firsttime) {
            if (gdth_test_busy(hanum)) {        /* controller busy ? */
                TRACE(("gdth_next() controller %d busy !\n",hanum));
                if (!gdth_polling) {
                    GDTH_UNLOCK_HA(ha, flags);
                    return;
                }
                while (gdth_test_busy(hanum))
                    gdth_delay(1);
            }   
            firsttime = FALSE;
        }

#if LINUX_VERSION_CODE >= 0x010300
        if (nscp->done != gdth_scsi_done || nscp->cmnd[0] != 0xff) 
#endif
        {
        if (nscp->SCp.phase == -1) {
            nscp->SCp.phase = CACHESERVICE;           /* default: cache svc. */ 
            if (nscp->cmnd[0] == TEST_UNIT_READY) {
                TRACE2(("TEST_UNIT_READY Bus %d Id %d LUN %d\n", 
                        b, t, nscp->lun));
                /* TEST_UNIT_READY -> set scan mode */
                if ((ha->scan_mode & 0x0f) == 0) {
                    if (b == 0 && t == 0 && nscp->lun == 0) {
                        ha->scan_mode |= 1;
                        TRACE2(("Scan mode: 0x%x\n", ha->scan_mode));
                    }
                } else if ((ha->scan_mode & 0x0f) == 1) {
                    if (b == 0 && ((t == 0 && nscp->lun == 1) ||
                         (t == 1 && nscp->lun == 0))) {
                        nscp->SCp.sent_command = GDT_SCAN_START;
                        nscp->SCp.phase = ((ha->scan_mode & 0x10 ? 1:0) << 8) 
                            | SCSIRAWSERVICE;
                        ha->scan_mode = 0x12;
                        TRACE2(("Scan mode: 0x%x (SCAN_START)\n", 
                                ha->scan_mode));
                    } else {
                        ha->scan_mode &= 0x10;
                        TRACE2(("Scan mode: 0x%x\n", ha->scan_mode));
                    }                   
                } else if (ha->scan_mode == 0x12) {
                    if (b == ha->bus_cnt && t == ha->tid_cnt-1) {
                        nscp->SCp.phase = SCSIRAWSERVICE;
                        nscp->SCp.sent_command = GDT_SCAN_END;
                        ha->scan_mode &= 0x10;
                        TRACE2(("Scan mode: 0x%x (SCAN_END)\n", 
                                ha->scan_mode));
                    }
                }
            }
            if (b == ha->virt_bus && nscp->cmnd[0] != INQUIRY &&
                nscp->cmnd[0] != READ_CAPACITY && nscp->cmnd[0] != MODE_SENSE &&
                (ha->hdr[t].cluster_type & CLUSTER_DRIVE)) {
                /* always GDT_CLUST_INFO! */
                nscp->SCp.sent_command = GDT_CLUST_INFO;
            }
        }
        }

        if (nscp->SCp.sent_command != -1) {
            if ((nscp->SCp.phase & 0xff) == CACHESERVICE) {
                if (!(cmd_index=gdth_fill_cache_cmd(hanum,nscp,t)))
                    this_cmd = FALSE;
                next_cmd = FALSE;
            } else if ((nscp->SCp.phase & 0xff) == SCSIRAWSERVICE) {
                if (!(cmd_index=gdth_fill_raw_cmd(hanum,nscp,BUS_L2P(ha,b))))
                    this_cmd = FALSE;
                next_cmd = FALSE;
            } else {
                memset((char*)nscp->sense_buffer,0,16);
                nscp->sense_buffer[0] = 0x70;
                nscp->sense_buffer[2] = NOT_READY;
                nscp->result = (DID_OK << 16) | (CHECK_CONDITION << 1);
                if (!nscp->SCp.have_data_in)
                    nscp->SCp.have_data_in++;
                else {
                    if (!gdth_polling) 
                        GDTH_UNLOCK_HA(ha,flags);
                    /* io_request_lock already active ! */
                    nscp->scsi_done(nscp);
                    if (!gdth_polling) 
                    	GDTH_LOCK_HA(ha,flags);
                }
            }
        } else

#if LINUX_VERSION_CODE >= 0x010300
        if (nscp->done == gdth_scsi_done && nscp->cmnd[0] == 0xff) {
            if (!(cmd_index=gdth_special_cmd(hanum,nscp)))
                this_cmd = FALSE;
            next_cmd = FALSE;
        } else
#endif
        if (b != ha->virt_bus) {
            if (ha->raw[BUS_L2P(ha,b)].io_cnt[t] >= GDTH_MAX_RAW ||
                !(cmd_index=gdth_fill_raw_cmd(hanum,nscp,BUS_L2P(ha,b)))) 
                this_cmd = FALSE;
            else 
                ha->raw[BUS_L2P(ha,b)].io_cnt[t]++;
        } else if (t >= MAX_HDRIVES || !ha->hdr[t].present || nscp->lun != 0) {
            TRACE2(("Command 0x%x to bus %d id %d lun %d -> IGNORE\n",
                    nscp->cmnd[0], b, t, nscp->lun));
            nscp->result = DID_BAD_TARGET << 16;
            if (!nscp->SCp.have_data_in)
                nscp->SCp.have_data_in++;
            else {
                if (!gdth_polling) 
                	GDTH_UNLOCK_HA(ha,flags);
                /* io_request_lock already active ! */      
                nscp->scsi_done(nscp);
                if (!gdth_polling) 
                	GDTH_LOCK_HA(ha,flags);
            }
        } else {
            switch (nscp->cmnd[0]) {
              case TEST_UNIT_READY:
              case INQUIRY:
              case REQUEST_SENSE:
              case READ_CAPACITY:
              case VERIFY:
              case START_STOP:
              case MODE_SENSE:
                TRACE(("cache cmd %x/%x/%x/%x/%x/%x\n",nscp->cmnd[0],
                       nscp->cmnd[1],nscp->cmnd[2],nscp->cmnd[3],
                       nscp->cmnd[4],nscp->cmnd[5]));
                if (ha->hdr[t].media_changed && nscp->cmnd[0] != INQUIRY) {
                    /* return UNIT_ATTENTION */
                    TRACE2(("cmd 0x%x target %d: UNIT_ATTENTION\n",
                             nscp->cmnd[0], t));
                    ha->hdr[t].media_changed = FALSE;
                    memset((char*)nscp->sense_buffer,0,16);
                    nscp->sense_buffer[0] = 0x70;
                    nscp->sense_buffer[2] = UNIT_ATTENTION;
                    nscp->result = (DID_OK << 16) | (CHECK_CONDITION << 1);
                    if (!nscp->SCp.have_data_in)
                        nscp->SCp.have_data_in++;
                    else {
                	if (!gdth_polling) 
                        	GDTH_UNLOCK_HA(ha,flags);
                        /* io_request_lock already active ! */      
                        nscp->scsi_done(nscp);
                	if (!gdth_polling) 
                        	GDTH_LOCK_HA(ha,flags);
                    }
                } else if (gdth_internal_cache_cmd(hanum,nscp)) {
                    if (!gdth_polling) 
                    	GDTH_UNLOCK_HA(ha,flags);
                    /* io_request_lock already active ! */      
                    nscp->scsi_done(nscp);
                    if (!gdth_polling) 
                    	GDTH_LOCK_HA(ha,flags);
                }
                break;

              case ALLOW_MEDIUM_REMOVAL:
                TRACE(("cache cmd %x/%x/%x/%x/%x/%x\n",nscp->cmnd[0],
                       nscp->cmnd[1],nscp->cmnd[2],nscp->cmnd[3],
                       nscp->cmnd[4],nscp->cmnd[5]));
                if ( (nscp->cmnd[4]&1) && !(ha->hdr[t].devtype&1) ) {
                    TRACE(("Prevent r. nonremov. drive->do nothing\n"));
                    nscp->result = DID_OK << 16;
                    nscp->sense_buffer[0] = 0;
                    if (!nscp->SCp.have_data_in)
                        nscp->SCp.have_data_in++;
                    else {
                    	if (!gdth_polling) 
                        	GDTH_UNLOCK_HA(ha,flags);
                        /* io_request_lock already active ! */      
                        nscp->scsi_done(nscp);
                    	if (!gdth_polling) 
                        	GDTH_LOCK_HA(ha,flags);
                    }
                } else {
                    nscp->cmnd[3] = (ha->hdr[t].devtype&1) ? 1:0;
                    TRACE(("Prevent/allow r. %d rem. drive %d\n",
                           nscp->cmnd[4],nscp->cmnd[3]));
                    if (!(cmd_index=gdth_fill_cache_cmd(hanum,nscp,t)))
                        this_cmd = FALSE;
                }
                break;
                
              case RESERVE:
              case RELEASE:
                TRACE2(("cache cmd %s\n",nscp->cmnd[0] == RESERVE ?
                        "RESERVE" : "RELEASE"));
                if (!(cmd_index=gdth_fill_cache_cmd(hanum,nscp,t)))
                    this_cmd = FALSE;
                break;
                
              case READ_6:
              case WRITE_6:
              case READ_10:
              case WRITE_10:
                if (ha->hdr[t].media_changed) {
                    /* return UNIT_ATTENTION */
                    TRACE2(("cmd 0x%x target %d: UNIT_ATTENTION\n",
                             nscp->cmnd[0], t));
                    ha->hdr[t].media_changed = FALSE;
                    memset((char*)nscp->sense_buffer,0,16);
                    nscp->sense_buffer[0] = 0x70;
                    nscp->sense_buffer[2] = UNIT_ATTENTION;
                    nscp->result = (DID_OK << 16) | (CHECK_CONDITION << 1);
                    if (!nscp->SCp.have_data_in)
                        nscp->SCp.have_data_in++;
                    else {
                    	if (!gdth_polling) 
                        	GDTH_UNLOCK_HA(ha,flags);
                        /* io_request_lock already active ! */      
                        nscp->scsi_done(nscp);
                    	if (!gdth_polling) 
                        	GDTH_LOCK_HA(ha,flags);
                    }
                } else if (!(cmd_index=gdth_fill_cache_cmd(hanum,nscp,t)))
                    this_cmd = FALSE;
                break;

              default:
                TRACE2(("cache cmd %x/%x/%x/%x/%x/%x unknown\n",nscp->cmnd[0],
                        nscp->cmnd[1],nscp->cmnd[2],nscp->cmnd[3],
                        nscp->cmnd[4],nscp->cmnd[5]));
                printk("GDT: Unknown SCSI command 0x%x to cache service !\n",
                       nscp->cmnd[0]);
                nscp->result = DID_ABORT << 16;
                if (!nscp->SCp.have_data_in)
                    nscp->SCp.have_data_in++;
                else {
                    if (!gdth_polling) 
                    	GDTH_UNLOCK_HA(ha,flags);
                    /* io_request_lock already active ! */  
                    nscp->scsi_done(nscp);
                    if (!gdth_polling) 
                    	GDTH_LOCK_HA(ha,flags);
                }
                break;
            }
        }

        if (!this_cmd)
            break;
        if (nscp == ha->req_first)
            ha->req_first = pscp = (Scsi_Cmnd *)nscp->SCp.ptr;
        else
            pscp->SCp.ptr = nscp->SCp.ptr;
        if (!next_cmd)
            break;
    }

    if (ha->cmd_cnt > 0) {
        gdth_release_event(hanum);
    }

    if (!gdth_polling) 
    	GDTH_UNLOCK_HA(ha, flags);

    if (gdth_polling && ha->cmd_cnt > 0) {
        if (!gdth_wait(hanum,cmd_index,POLL_TIMEOUT))
            printk("GDT: Controller %d: Command %d timed out !\n",
                   hanum,cmd_index);
    }
}
    
static void gdth_copy_internal_data(Scsi_Cmnd *scp,char *buffer,ushort count)
{
    ushort cpcount,i;
    ushort cpsum,cpnow;
    struct scatterlist *sl;

    cpcount = count<=(ushort)scp->bufflen ? count:(ushort)scp->bufflen;
    if (scp->use_sg) {
        sl = (struct scatterlist *)scp->request_buffer;
        for (i=0,cpsum=0; i<scp->use_sg; ++i,++sl) {
            cpnow = (ushort)sl->length;
            TRACE(("copy_internal() now %d sum %d count %d %d\n",
                          cpnow,cpsum,cpcount,(ushort)scp->bufflen));
            if (cpsum+cpnow > cpcount) 
                cpnow = cpcount - cpsum;
            cpsum += cpnow;
            memcpy((char*)sl->address,buffer,cpnow);
            if (cpsum == cpcount)
                break;
            buffer += cpnow;
        }
    } else {
        TRACE(("copy_internal() count %d\n",cpcount));
        memcpy((char*)scp->request_buffer,buffer,cpcount);
    }
}

static int gdth_internal_cache_cmd(int hanum,Scsi_Cmnd *scp)
{
    register gdth_ha_str *ha;
    unchar t;
    gdth_inq_data inq;
    gdth_rdcap_data rdc;
    gdth_sense_data sd;
    gdth_modep_data mpd;

    ha = HADATA(gdth_ctr_tab[hanum]);
    t  = scp->target;
    TRACE(("gdth_internal_cache_cmd() cmd 0x%x hdrive %d\n",
           scp->cmnd[0],t));

    switch (scp->cmnd[0]) {
      case TEST_UNIT_READY:
      case VERIFY:
      case START_STOP:
        TRACE2(("Test/Verify/Start hdrive %d\n",t));
        break;

      case INQUIRY:
        TRACE2(("Inquiry hdrive %d devtype %d\n",
                t,ha->hdr[t].devtype));
        inq.type_qual = (ha->hdr[t].devtype&4) ? TYPE_ROM:TYPE_DISK;
        /* you can here set all disks to removable, if you want to do
           a flush using the ALLOW_MEDIUM_REMOVAL command */
        inq.modif_rmb = 0x00;
        if ((ha->hdr[t].devtype & 1) ||
            (ha->hdr[t].cluster_type & CLUSTER_DRIVE))
            inq.modif_rmb = 0x80;
        inq.version   = 2;
        inq.resp_aenc = 2;
        inq.add_length= 32;
        if (ha->oem_id == OEM_ID_INTEL)
            strcpy(inq.vendor,"Intel  ");
        else 
            strcpy(inq.vendor,"ICP    ");
        sprintf(inq.product,"Host Drive  #%02d",t);
        strcpy(inq.revision,"   ");
        gdth_copy_internal_data(scp,(char*)&inq,sizeof(gdth_inq_data));
        break;

      case REQUEST_SENSE:
        TRACE2(("Request sense hdrive %d\n",t));
        sd.errorcode = 0x70;
        sd.segno     = 0x00;
        sd.key       = NO_SENSE;
        sd.info      = 0;
        sd.add_length= 0;
        gdth_copy_internal_data(scp,(char*)&sd,sizeof(gdth_sense_data));
        break;

      case MODE_SENSE:
        TRACE2(("Mode sense hdrive %d\n",t));
        memset((char*)&mpd,0,sizeof(gdth_modep_data));
        mpd.hd.data_length = sizeof(gdth_modep_data);
        mpd.hd.dev_par     = (ha->hdr[t].devtype&2) ? 0x80:0;
        mpd.hd.bd_length   = sizeof(mpd.bd);
        mpd.bd.block_length[0] = (SECTOR_SIZE & 0x00ff0000) >> 16;
        mpd.bd.block_length[1] = (SECTOR_SIZE & 0x0000ff00) >> 8;
        mpd.bd.block_length[2] = (SECTOR_SIZE & 0x000000ff);
        gdth_copy_internal_data(scp,(char*)&mpd,sizeof(gdth_modep_data));
        break;

      case READ_CAPACITY:
        TRACE2(("Read capacity hdrive %d\n",t));
        rdc.last_block_no = ntohl(ha->hdr[t].size-1);
        rdc.block_length  = ntohl(SECTOR_SIZE);
        gdth_copy_internal_data(scp,(char*)&rdc,sizeof(gdth_rdcap_data));
        break;

      default:
        TRACE2(("Internal cache cmd 0x%x unknown\n",scp->cmnd[0]));
        break;
    }

    scp->result = DID_OK << 16;
    scp->sense_buffer[0] = 0;

    if (!scp->SCp.have_data_in)
        scp->SCp.have_data_in++;
    else 
        return 1;

    return 0;
}
    
static int gdth_fill_cache_cmd(int hanum,Scsi_Cmnd *scp,ushort hdrive)
{
    register gdth_ha_str *ha;
    register gdth_cmd_str *cmdp;
    struct scatterlist *sl;
    ushort i, cnt;
    ulong32 no;
    int cmd_index, read_write;

    ha = HADATA(gdth_ctr_tab[hanum]);
    cmdp = ha->pccb;
    TRACE(("gdth_fill_cache_cmd() cmd 0x%x cmdsize %d hdrive %d\n",
                 scp->cmnd[0],scp->cmd_len,hdrive));

    if (ha->type==GDT_EISA && ha->cmd_cnt>0) 
        return 0;

    cmdp->Service = CACHESERVICE;
    cmdp->RequestBuffer = scp;
    /* search free command index */
    if (!(cmd_index=gdth_get_cmd_index(hanum))) {
        TRACE(("GDT: No free command index found\n"));
        return 0;
    }
    /* if it's the first command, set command semaphore */
    if (ha->cmd_cnt == 0)
        gdth_set_sema0(hanum);

    /* fill command */
    read_write = FALSE;
    if (scp->SCp.sent_command != -1) 
        cmdp->OpCode = scp->SCp.sent_command;   /* special cache cmd. */
    else if (scp->cmnd[0] == RESERVE) 
        cmdp->OpCode = GDT_RESERVE_DRV;
    else if (scp->cmnd[0] == RELEASE)
        cmdp->OpCode = GDT_RELEASE_DRV;
    else if (scp->cmnd[0] == ALLOW_MEDIUM_REMOVAL) {
        if (scp->cmnd[4] & 1)                   /* prevent ? */
            cmdp->OpCode = GDT_MOUNT;
        else if (scp->cmnd[3] & 1)              /* removable drive ? */
            cmdp->OpCode = GDT_UNMOUNT;
        else
            cmdp->OpCode = GDT_FLUSH;
    } else if (scp->cmnd[0] == WRITE_6 || scp->cmnd[0] == WRITE_10) {
        read_write = TRUE;
        if (gdth_write_through || ((ha->hdr[hdrive].rw_attribs & 1) && 
                                   (ha->cache_feat & GDT_WR_THROUGH)))
            cmdp->OpCode = GDT_WRITE_THR;
        else
            cmdp->OpCode = GDT_WRITE;
    } else {
        read_write = TRUE;
        cmdp->OpCode = GDT_READ;
    }
    
    cmdp->BoardNode        = LOCALBOARD;
    cmdp->u.cache.DeviceNo = hdrive;
    cmdp->u.cache.BlockNo  = 1;
    cmdp->u.cache.sg_canz  = 0;

    if (read_write) {
        if (scp->cmd_len != 6) {
            memcpy(&no, &scp->cmnd[2], sizeof(ulong32));
            cmdp->u.cache.BlockNo = ntohl(no);
            memcpy(&cnt, &scp->cmnd[7], sizeof(ushort));
            cmdp->u.cache.BlockCnt = (ulong32)ntohs(cnt);
        } else {
            memcpy(&no, &scp->cmnd[0], sizeof(ulong32));
            cmdp->u.cache.BlockNo = ntohl(no) & 0x001fffffUL;
            cmdp->u.cache.BlockCnt= scp->cmnd[4]==0 ? 0x100 : scp->cmnd[4];
        }

        if (scp->use_sg) {
            cmdp->u.cache.DestAddr= 0xffffffff;
            sl = (struct scatterlist *)scp->request_buffer;
            for (i=0; i<scp->use_sg; ++i,++sl) {
                cmdp->u.cache.sg_lst[i].sg_ptr = virt_to_bus(sl->address);
                cmdp->u.cache.sg_lst[i].sg_len = (ulong32)sl->length;
            }
            cmdp->u.cache.sg_canz = (ulong32)i;

#ifdef GDTH_STATISTICS
            if (max_sg < (ulong32)i) {
                max_sg = (ulong32)i;
                TRACE3(("GDT: max_sg = %d\n",i));
            }
#endif
            if (i<GDTH_MAXSG)
                cmdp->u.cache.sg_lst[i].sg_len = 0;
        } else {
            if (ha->cache_feat & SCATTER_GATHER) {
                cmdp->u.cache.DestAddr = 0xffffffff;
                cmdp->u.cache.sg_canz = 1;
                cmdp->u.cache.sg_lst[0].sg_ptr = 
                    virt_to_bus(scp->request_buffer);
                cmdp->u.cache.sg_lst[0].sg_len = scp->request_bufflen;
                cmdp->u.cache.sg_lst[1].sg_len = 0;
            } else {
                cmdp->u.cache.DestAddr  = virt_to_bus(scp->request_buffer);
                cmdp->u.cache.sg_canz= 0;
            }
        }
    }
    TRACE(("cache cmd: addr. %x sganz %x sgptr0 %x sglen0 %x\n",
           cmdp->u.cache.DestAddr,cmdp->u.cache.sg_canz,
           cmdp->u.cache.sg_lst[0].sg_ptr,
           cmdp->u.cache.sg_lst[0].sg_len));
    TRACE(("cache cmd: cmd %d blockno. %d, blockcnt %d\n",
           cmdp->OpCode,cmdp->u.cache.BlockNo,cmdp->u.cache.BlockCnt));

    /* evaluate command size, check space */
    ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.cache.sg_lst) +
        (ushort)cmdp->u.cache.sg_canz * sizeof(gdth_sg_str);
    if (ha->cmd_len & 3)
        ha->cmd_len += (4 - (ha->cmd_len & 3));

    if (ha->cmd_cnt > 0) {
        if ((ha->cmd_offs_dpmem + ha->cmd_len + DPMEM_COMMAND_OFFSET) >
            ha->ic_all_size) {
            TRACE2(("gdth_fill_cache() DPMEM overflow\n"));
            ha->cmd_tab[cmd_index-2].cmnd = UNUSED_CMND;
            return 0;
        }
    }

    /* copy command */
    gdth_copy_command(hanum);
    return cmd_index;
}

static int gdth_fill_raw_cmd(int hanum,Scsi_Cmnd *scp,unchar b)
{
    register gdth_ha_str *ha;
    register gdth_cmd_str *cmdp;
    struct scatterlist *sl;
    ushort i;
    int cmd_index;
    unchar t,l;

    ha = HADATA(gdth_ctr_tab[hanum]);
    t = scp->target;
    l = scp->lun;
    cmdp = ha->pccb;
    TRACE(("gdth_fill_raw_cmd() cmd 0x%x bus %d ID %d LUN %d\n",
           scp->cmnd[0],b,t,l));

    if (ha->type==GDT_EISA && ha->cmd_cnt>0) 
        return 0;

    cmdp->Service = SCSIRAWSERVICE;
    cmdp->RequestBuffer = scp;
    /* search free command index */
    if (!(cmd_index=gdth_get_cmd_index(hanum))) {
        TRACE(("GDT: No free command index found\n"));
        return 0;
    }
    /* if it's the first command, set command semaphore */
    if (ha->cmd_cnt == 0)
        gdth_set_sema0(hanum);

    /* fill command */  
    if (scp->SCp.sent_command != -1) {
        cmdp->OpCode           = scp->SCp.sent_command; /* special raw cmd. */
        cmdp->BoardNode        = LOCALBOARD;
        cmdp->u.raw.direction  = (scp->SCp.phase >> 8);
        TRACE2(("special raw cmd 0x%x param 0x%x\n", 
                cmdp->OpCode, cmdp->u.raw.direction));

        /* evaluate command size */
        ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.raw.sg_lst);
    } else {
        cmdp->OpCode           = GDT_WRITE;             /* always */
        cmdp->BoardNode        = LOCALBOARD;
        cmdp->u.raw.reserved   = 0;
        cmdp->u.raw.mdisc_time = 0;
        cmdp->u.raw.mcon_time  = 0;
        cmdp->u.raw.clen       = scp->cmd_len;
        cmdp->u.raw.target     = t;
        cmdp->u.raw.lun        = l;
        cmdp->u.raw.bus        = b;
        cmdp->u.raw.priority   = 0;
        cmdp->u.raw.link_p     = 0;
        cmdp->u.raw.sdlen      = scp->request_bufflen;
        cmdp->u.raw.sense_len  = 16;
        cmdp->u.raw.sense_data = virt_to_bus(scp->sense_buffer);
        cmdp->u.raw.direction  = 
            gdth_direction_tab[scp->cmnd[0]]==DOU ? GDTH_DATA_OUT:GDTH_DATA_IN;
        memcpy(cmdp->u.raw.cmd,scp->cmnd,12);

        if (scp->use_sg) {
            cmdp->u.raw.sdata  = 0xffffffff;
            sl = (struct scatterlist *)scp->request_buffer;
            for (i=0; i<scp->use_sg; ++i,++sl) {
                cmdp->u.raw.sg_lst[i].sg_ptr = virt_to_bus(sl->address);
                cmdp->u.raw.sg_lst[i].sg_len = (ulong32)sl->length;
            }
            cmdp->u.raw.sg_ranz = (ulong32)i;

#ifdef GDTH_STATISTICS
            if (max_sg < (ulong32)i) {
                max_sg = (ulong32)i;
                TRACE3(("GDT: max_sg = %d\n",i));
            }
#endif
            if (i<GDTH_MAXSG)
                cmdp->u.raw.sg_lst[i].sg_len = 0;
        } else {
            if (ha->raw_feat & SCATTER_GATHER) {
                cmdp->u.raw.sdata  = 0xffffffff;
                cmdp->u.raw.sg_ranz= 1;
                cmdp->u.raw.sg_lst[0].sg_ptr = virt_to_bus(scp->request_buffer);
                cmdp->u.raw.sg_lst[0].sg_len = scp->request_bufflen;
                cmdp->u.raw.sg_lst[1].sg_len = 0;
            } else {
                cmdp->u.raw.sdata  = virt_to_bus(scp->request_buffer);
                cmdp->u.raw.sg_ranz= 0;
            }
        }
        TRACE(("raw cmd: addr. %x sganz %x sgptr0 %x sglen0 %x\n",
               cmdp->u.raw.sdata,cmdp->u.raw.sg_ranz,
               cmdp->u.raw.sg_lst[0].sg_ptr,
               cmdp->u.raw.sg_lst[0].sg_len));

        /* evaluate command size */
        ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.raw.sg_lst) +
            (ushort)cmdp->u.raw.sg_ranz * sizeof(gdth_sg_str);
    }
    /* check space */
    if (ha->cmd_len & 3)
        ha->cmd_len += (4 - (ha->cmd_len & 3));

    if (ha->cmd_cnt > 0) {
        if ((ha->cmd_offs_dpmem + ha->cmd_len + DPMEM_COMMAND_OFFSET) >
            ha->ic_all_size) {
            TRACE2(("gdth_fill_raw() DPMEM overflow\n"));
            ha->cmd_tab[cmd_index-2].cmnd = UNUSED_CMND;
            return 0;
        }
    }

    /* copy command */
    gdth_copy_command(hanum);
    return cmd_index;
}

static int gdth_special_cmd(int hanum,Scsi_Cmnd *scp)
{
    register gdth_ha_str *ha;
    register gdth_cmd_str *cmdp;
    int cmd_index;

    ha  = HADATA(gdth_ctr_tab[hanum]);
    cmdp= ha->pccb;
    TRACE2(("gdth_special_cmd(): "));

    if (ha->type==GDT_EISA && ha->cmd_cnt>0) 
        return 0;

    memcpy( cmdp, scp->request_buffer, sizeof(gdth_cmd_str));
    cmdp->RequestBuffer = scp;

    /* search free command index */
    if (!(cmd_index=gdth_get_cmd_index(hanum))) {
        TRACE(("GDT: No free command index found\n"));
        return 0;
    }

    /* if it's the first command, set command semaphore */
    if (ha->cmd_cnt == 0)
       gdth_set_sema0(hanum);

    /* evaluate command size, check space */
    if (cmdp->OpCode == GDT_IOCTL) {
        TRACE2(("IOCTL\n"));
        ha->cmd_len = 
            GDTOFFSOF(gdth_cmd_str,u.ioctl.p_param) + sizeof(ulong32);
    } else if (cmdp->Service == CACHESERVICE) {
        TRACE2(("cache command %d\n",cmdp->OpCode));
        ha->cmd_len = 
            GDTOFFSOF(gdth_cmd_str,u.cache.sg_lst) + sizeof(gdth_sg_str);
    } else if (cmdp->Service == SCSIRAWSERVICE) {
        TRACE2(("raw command %d/%d\n",cmdp->OpCode,cmdp->u.raw.cmd[0]));
        ha->cmd_len = 
            GDTOFFSOF(gdth_cmd_str,u.raw.sg_lst) + sizeof(gdth_sg_str);
    }

    if (ha->cmd_len & 3)
        ha->cmd_len += (4 - (ha->cmd_len & 3));

    if (ha->cmd_cnt > 0) {
        if ((ha->cmd_offs_dpmem + ha->cmd_len + DPMEM_COMMAND_OFFSET) >
            ha->ic_all_size) {
            TRACE2(("gdth_special_cmd() DPMEM overflow\n"));
            ha->cmd_tab[cmd_index-2].cmnd = UNUSED_CMND;
            return 0;
        }
    }

    /* copy command */
    gdth_copy_command(hanum);
    return cmd_index;
}    


/* Controller event handling functions */
static gdth_evt_str *gdth_store_event(gdth_ha_str *ha, ushort source, 
                                      ushort idx, gdth_evt_data *evt)
{
    gdth_evt_str *e;
    struct timeval tv;

    /* no GDTH_LOCK_HA() ! */
    TRACE2(("gdth_store_event() source %d idx %d\n", source, idx));
    if (source == 0)                        /* no source -> no event */
        return 0;

    if (ebuffer[elastidx].event_source == source &&
        ebuffer[elastidx].event_idx == idx &&
        ((evt->size != 0 && ebuffer[elastidx].event_data.size != 0 &&
            !memcmp((char *)&ebuffer[elastidx].event_data.eu,
            (char *)&evt->eu, evt->size)) ||
        (evt->size == 0 && ebuffer[elastidx].event_data.size == 0 &&
            !strcmp((char *)&ebuffer[elastidx].event_data.event_string,
            (char *)&evt->event_string)))) { 
        e = &ebuffer[elastidx];
        do_gettimeofday(&tv);
        e->last_stamp = tv.tv_sec;
        ++e->same_count;
    } else {
        if (ebuffer[elastidx].event_source != 0) {  /* entry not free ? */
            ++elastidx;
            if (elastidx == MAX_EVENTS)
                elastidx = 0;
            if (elastidx == eoldidx) {              /* reached mark ? */
                ++eoldidx;
                if (eoldidx == MAX_EVENTS)
                    eoldidx = 0;
            }
        }
        e = &ebuffer[elastidx];
        e->event_source = source;
        e->event_idx = idx;
        do_gettimeofday(&tv);
        e->first_stamp = e->last_stamp = tv.tv_sec;
        e->same_count = 1;
        e->event_data = *evt;
        e->application = 0;
    }
    return e;
}

static int gdth_read_event(gdth_ha_str *ha, int handle, gdth_evt_str *estr)
{
    gdth_evt_str *e;
    int eindex;
    ulong flags;

    TRACE2(("gdth_read_event() handle %d\n", handle));
    GDTH_LOCK_HA(ha, flags);
    if (handle == -1)
        eindex = eoldidx;
    else
        eindex = handle;
    estr->event_source = 0;

    if (eindex >= MAX_EVENTS) {
        GDTH_UNLOCK_HA(ha, flags);
        return eindex;
    }
    e = &ebuffer[eindex];
    if (e->event_source != 0) {
        if (eindex != elastidx) {
            if (++eindex == MAX_EVENTS)
                eindex = 0;
        } else {
            eindex = -1;
        }
        memcpy(estr, e, sizeof(gdth_evt_str));
    }
    GDTH_UNLOCK_HA(ha, flags);
    return eindex;
}

static void gdth_readapp_event(gdth_ha_str *ha,
                               unchar application, gdth_evt_str *estr)
{
    gdth_evt_str *e;
    int eindex;
    ulong flags;
    unchar found = FALSE;

    TRACE2(("gdth_readapp_event() app. %d\n", application));
    GDTH_LOCK_HA(ha, flags);
    eindex = eoldidx;
    for (;;) {
        e = &ebuffer[eindex];
        if (e->event_source == 0)
            break;
        if ((e->application & application) == 0) {
            e->application |= application;
            found = TRUE;
            break;
        }
        if (eindex == elastidx)
            break;
        if (++eindex == MAX_EVENTS)
            eindex = 0;
    }
    if (found)
        memcpy(estr, e, sizeof(gdth_evt_str));
    else
        estr->event_source = 0;
    GDTH_UNLOCK_HA(ha, flags);
}

static void gdth_clear_events()
{
    TRACE(("gdth_clear_events()"));

    eoldidx = elastidx = 0;
    ebuffer[0].event_source = 0;
}


/* SCSI interface functions */

#if LINUX_VERSION_CODE >= 0x010346
static void gdth_interrupt(int irq,void *dev_id,struct pt_regs *regs)
#else
static void gdth_interrupt(int irq,struct pt_regs *regs)
#endif
{
    register gdth_ha_str *ha;
    gdt6m_dpram_str *dp6m_ptr;
    gdt6_dpram_str *dp6_ptr;
    gdt2_dpram_str *dp2_ptr;
    Scsi_Cmnd *scp;
    int hanum, rval, i;
    unchar IStatus;
    ushort Service;
    ulong flags = 0;

    TRACE(("gdth_interrupt() IRQ %d\n",irq));

    /* if polling and not from gdth_wait() -> return */
    if (gdth_polling) {
        if (!gdth_from_wait) {
            return;
        }
    }

    if (!gdth_polling)
        GDTH_LOCK_HA((gdth_ha_str *)dev_id,flags);
    wait_index = 0;

    /* search controller */
    if ((hanum = gdth_get_status(&IStatus,irq)) == -1) {
        /* spurious interrupt */
        if (!gdth_polling)
            GDTH_UNLOCK_HA((gdth_ha_str *)dev_id,flags);
        return;
    }

#ifdef GDTH_STATISTICS
    ++act_ints;
#endif
    
    ha = HADATA(gdth_ctr_tab[hanum]);
    if (ha->type == GDT_EISA) {
        if (IStatus & 0x80) {                       /* error flag */
            IStatus &= ~0x80;
            ha->status = inw(ha->bmic + MAILBOXREG+8);
            TRACE2(("gdth_interrupt() error %d/%d\n",IStatus,ha->status));
        } else                                      /* no error */
            ha->status = S_OK;
        ha->info = inl(ha->bmic + MAILBOXREG+12);
        ha->service = inw(ha->bmic + MAILBOXREG+10);
        ha->info2 = inl(ha->bmic + MAILBOXREG+4);

        outb(0xff, ha->bmic + EDOORREG);            /* acknowledge interrupt */
        outb(0x00, ha->bmic + SEMA1REG);            /* reset status semaphore */
    } else if (ha->type == GDT_ISA) {
        dp2_ptr = (gdt2_dpram_str *)ha->brd;
        if (IStatus & 0x80) {                       /* error flag */
            IStatus &= ~0x80;
            ha->status = gdth_readw(&dp2_ptr->u.ic.Status);
            TRACE2(("gdth_interrupt() error %d/%d\n",IStatus,ha->status));
        } else                                      /* no error */
            ha->status = S_OK;
        ha->info = gdth_readl(&dp2_ptr->u.ic.Info[0]);
        ha->service = gdth_readw(&dp2_ptr->u.ic.Service);
        ha->info2 = gdth_readl(&dp2_ptr->u.ic.Info[1]);

        gdth_writeb(0xff, &dp2_ptr->io.irqdel);     /* acknowledge interrupt */
        gdth_writeb(0, &dp2_ptr->u.ic.Cmd_Index);   /* reset command index */
        gdth_writeb(0, &dp2_ptr->io.Sema1);         /* reset status semaphore */
    } else if (ha->type == GDT_PCI) {
        dp6_ptr = (gdt6_dpram_str *)ha->brd;
        if (IStatus & 0x80) {                       /* error flag */
            IStatus &= ~0x80;
            ha->status = gdth_readw(&dp6_ptr->u.ic.Status);
            TRACE2(("gdth_interrupt() error %d/%d\n",IStatus,ha->status));
        } else                                      /* no error */
            ha->status = S_OK;
        ha->info = gdth_readl(&dp6_ptr->u.ic.Info[0]);
        ha->service = gdth_readw(&dp6_ptr->u.ic.Service);
        ha->info2 = gdth_readl(&dp6_ptr->u.ic.Info[1]);

        gdth_writeb(0xff, &dp6_ptr->io.irqdel);     /* acknowledge interrupt */
        gdth_writeb(0, &dp6_ptr->u.ic.Cmd_Index);   /* reset command index */
        gdth_writeb(0, &dp6_ptr->io.Sema1);         /* reset status semaphore */
    } else if (ha->type == GDT_PCINEW) {
        if (IStatus & 0x80) {                       /* error flag */
            IStatus &= ~0x80;
            ha->status = inw(PTR2USHORT(&ha->plx->status));
            TRACE2(("gdth_interrupt() error %d/%d\n",IStatus,ha->status));
        } else
            ha->status = S_OK;
        ha->info = inl(PTR2USHORT(&ha->plx->info[0]));
        ha->service = inw(PTR2USHORT(&ha->plx->service));
        ha->info2 = inl(PTR2USHORT(&ha->plx->info[1]));

        outb(0xff, PTR2USHORT(&ha->plx->edoor_reg)); 
        outb(0x00, PTR2USHORT(&ha->plx->sema1_reg)); 
    } else if (ha->type == GDT_PCIMPR) {
        dp6m_ptr = (gdt6m_dpram_str *)ha->brd;
        if (IStatus & 0x80) {                       /* error flag */
            IStatus &= ~0x80;
            ha->status = gdth_readw(&dp6m_ptr->i960r.status);
            TRACE2(("gdth_interrupt() error %d/%d\n",IStatus,ha->status));
        } else                                      /* no error */
            ha->status = S_OK;
        ha->info = gdth_readl(&dp6m_ptr->i960r.info[0]);
        ha->service = gdth_readw(&dp6m_ptr->i960r.service);
        ha->info2 = gdth_readl(&dp6m_ptr->i960r.info[1]);

        /* event string */
        if (IStatus == ASYNCINDEX) {
            if (ha->service != SCREENSERVICE &&
                (ha->fw_vers & 0xff) >= 0x1a) {
                ha->dvr.severity =   
                    gdth_readb(&((gdt6m_dpram_str *)ha->brd)->i960r.severity);
                for (i = 0; i < 256; ++i) {
                    ha->dvr.event_string[i] = gdth_readb
                        (&((gdt6m_dpram_str *)ha->brd)->i960r.evt_str[i]);
                    if (ha->dvr.event_string[i] == 0)
                        break;
                }
            }
        }
        gdth_writeb(0xff, &dp6m_ptr->i960r.edoor_reg);
        gdth_writeb(0, &dp6m_ptr->i960r.sema1_reg);
    } else {
        TRACE2(("gdth_interrupt() unknown controller type\n"));
        if (!gdth_polling)
            GDTH_UNLOCK_HA((gdth_ha_str *)dev_id,flags);
        return;
    }

    TRACE(("gdth_interrupt() index %d stat %d info %d\n",
           IStatus,ha->status,ha->info));

    if (gdth_from_wait) {
        wait_hanum = hanum;
        wait_index = (int)IStatus;
    }

    if (IStatus == ASYNCINDEX) {
        TRACE2(("gdth_interrupt() async. event\n"));
        gdth_async_event(hanum);
        if (!gdth_polling)
            GDTH_UNLOCK_HA((gdth_ha_str *)dev_id,flags);
        gdth_next(hanum);
        return;
    } 

    if (IStatus == SPEZINDEX) {
        TRACE2(("Service unknown or not initialized !\n"));
        ha->dvr.size = sizeof(ha->dvr.eu.driver);
        ha->dvr.eu.driver.ionode = hanum;
        gdth_store_event(ha, ES_DRIVER, 4, &ha->dvr);
        if (!gdth_polling)
            GDTH_UNLOCK_HA((gdth_ha_str *)dev_id,flags);
        return;
    }
    scp     = ha->cmd_tab[IStatus-2].cmnd;
    Service = ha->cmd_tab[IStatus-2].service;
    ha->cmd_tab[IStatus-2].cmnd = UNUSED_CMND;
    if (scp == UNUSED_CMND) {
        TRACE2(("gdth_interrupt() index to unused command (%d)\n",IStatus));
        ha->dvr.size = sizeof(ha->dvr.eu.driver);
        ha->dvr.eu.driver.ionode = hanum;
        ha->dvr.eu.driver.index = IStatus;
        gdth_store_event(ha, ES_DRIVER, 1, &ha->dvr);
        if (!gdth_polling)
            GDTH_UNLOCK_HA((gdth_ha_str *)dev_id,flags);
        return;
    }
    if (scp == INTERNAL_CMND) {
        TRACE(("gdth_interrupt() answer to internal command\n"));
        if (!gdth_polling)
            GDTH_UNLOCK_HA((gdth_ha_str *)dev_id,flags);
        return;
    }

    TRACE(("gdth_interrupt() sync. status\n"));
    rval = gdth_sync_event(hanum,Service,IStatus,scp);
    if (!gdth_polling)
        GDTH_UNLOCK_HA((gdth_ha_str *)dev_id,flags);
    if (rval == 2) {
        gdth_putq(hanum,scp,scp->SCp.this_residual);
    } else if (rval == 1) {
        GDTH_LOCK_SCSI_DONE(flags);
        scp->scsi_done(scp);
        GDTH_UNLOCK_SCSI_DONE(flags);
    }
    gdth_next(hanum);
}

static int gdth_sync_event(int hanum,int service,unchar index,Scsi_Cmnd *scp)
{
    register gdth_ha_str *ha;
    gdth_msg_str *msg;
    gdth_cmd_str *cmdp;
    unchar b;

    ha   = HADATA(gdth_ctr_tab[hanum]);
    cmdp = ha->pccb;
    TRACE(("gdth_sync_event() serv %d status %d\n",
           service,ha->status));

    if (service == SCREENSERVICE) {
        msg  = (gdth_msg_str *)ha->pscratch;
        ha->scratch_busy = FALSE;
        TRACE(("len: %d, answer: %d, ext: %d, alen: %d\n",
               msg->msg_len,msg->msg_answer,msg->msg_ext,msg->msg_alen));
        if (msg->msg_len)
            if (!(msg->msg_answer && msg->msg_ext)) {
                msg->msg_text[msg->msg_len] = '\0';
                printk("%s",msg->msg_text);
            }

        if (msg->msg_ext && !msg->msg_answer) {
            while (gdth_test_busy(hanum))
                gdth_delay(0);
            cmdp->Service       = SCREENSERVICE;
            cmdp->RequestBuffer = SCREEN_CMND;
            gdth_get_cmd_index(hanum);
            gdth_set_sema0(hanum);
            cmdp->OpCode        = GDT_READ;
            cmdp->BoardNode     = LOCALBOARD;
            cmdp->u.screen.reserved  = 0;
            cmdp->u.screen.su.msg.msg_handle= msg->msg_handle;
            cmdp->u.screen.su.msg.msg_addr  = virt_to_bus(msg);
            ha->scratch_busy = TRUE;
            ha->cmd_offs_dpmem = 0;
            ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.screen.su.msg.msg_addr) 
                + sizeof(ulong32);
            ha->cmd_cnt = 0;
            gdth_copy_command(hanum);
            gdth_release_event(hanum);
            return 0;
        }

        if (msg->msg_answer && msg->msg_alen) {
            /* default answers (getchar() not possible) */
            if (msg->msg_alen == 1) {
                msg->msg_alen = 0;
                msg->msg_len = 1;
                msg->msg_text[0] = 0;
            } else {
                msg->msg_alen -= 2;
                msg->msg_len = 2;
                msg->msg_text[0] = 1;
                msg->msg_text[1] = 0;
            }
            msg->msg_ext    = 0;
            msg->msg_answer = 0;
            while (gdth_test_busy(hanum))
                gdth_delay(0);
            cmdp->Service       = SCREENSERVICE;
            cmdp->RequestBuffer = SCREEN_CMND;
            gdth_get_cmd_index(hanum);
            gdth_set_sema0(hanum);
            cmdp->OpCode        = GDT_WRITE;
            cmdp->BoardNode     = LOCALBOARD;
            cmdp->u.screen.reserved  = 0;
            cmdp->u.screen.su.msg.msg_handle= msg->msg_handle;
            cmdp->u.screen.su.msg.msg_addr  = virt_to_bus(msg);
            ha->scratch_busy = TRUE;
            ha->cmd_offs_dpmem = 0;
            ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.screen.su.msg.msg_addr) 
                + sizeof(ulong32);
            ha->cmd_cnt = 0;
            gdth_copy_command(hanum);
            gdth_release_event(hanum);
            return 0;
        }
        printk("\n");

    } else {
        b = virt_ctr ? NUMDATA(scp->host)->busnum : scp->channel;
        if (scp->SCp.sent_command == -1 && b != ha->virt_bus) {
            ha->raw[BUS_L2P(ha,b)].io_cnt[scp->target]--;
        }
        /* cache or raw service */
        if (ha->status == S_OK) {
            scp->SCp.Status = S_OK;
            scp->SCp.Message = ha->info;
            if (scp->SCp.sent_command != -1) {
                TRACE2(("gdth_sync_event(): special cmd 0x%x OK\n",
                        scp->SCp.sent_command));
                /* special commands GDT_CLUST_INFO/GDT_MOUNT ? */
                if (scp->SCp.sent_command == GDT_CLUST_INFO) {
                    ha->hdr[scp->target].cluster_type = (unchar)ha->info;
                    if (!(ha->hdr[scp->target].cluster_type & 
                        CLUSTER_MOUNTED)) {
                        /* NOT MOUNTED -> MOUNT */
                        scp->SCp.sent_command = GDT_MOUNT;
                        if (ha->hdr[scp->target].cluster_type & 
                            CLUSTER_RESERVED) {
                            /* cluster drive RESERVED (on the other node) */
                            scp->SCp.phase = -2;      /* reservation conflict */
                        }
                    } else {
                        scp->SCp.sent_command = -1;
                    }
                } else {
                    if (scp->SCp.sent_command == GDT_MOUNT) {
                        ha->hdr[scp->target].cluster_type |= CLUSTER_MOUNTED;
                        ha->hdr[scp->target].media_changed = TRUE;
                    } else if (scp->SCp.sent_command == GDT_UNMOUNT) {
                        ha->hdr[scp->target].cluster_type &= ~CLUSTER_MOUNTED;
                        ha->hdr[scp->target].media_changed = TRUE;
                    } 
                    scp->SCp.sent_command = -1;
                }
                /* retry */
                scp->SCp.this_residual = HIGH_PRI;
                return 2;
            } else {
                /* RESERVE/RELEASE ? */
                if (scp->cmnd[0] == RESERVE) {
                    ha->hdr[scp->target].cluster_type |= CLUSTER_RESERVED;
                } else if (scp->cmnd[0] == RELEASE) {
                    ha->hdr[scp->target].cluster_type &= ~CLUSTER_RESERVED;
                }           
                scp->result = DID_OK << 16;
                scp->sense_buffer[0] = 0;
            }
        } else if (ha->status == S_BSY) {
            TRACE2(("Controller busy -> retry !\n"));
            scp->SCp.Status = S_BSY;
            scp->SCp.Message = ha->info;
            if (scp->SCp.sent_command == GDT_MOUNT)
                scp->SCp.sent_command = GDT_CLUST_INFO;
            /* retry */
            return 2;
        } else {
            scp->SCp.Status = ha->status;
            scp->SCp.Message = ha->info;

            if (scp->SCp.sent_command != -1) {
                TRACE2(("gdth_sync_event(): special cmd 0x%x error 0x%x\n",
                        scp->SCp.sent_command, ha->status));
                if (scp->SCp.sent_command == GDT_SCAN_START ||
                    scp->SCp.sent_command == GDT_SCAN_END) {
                    scp->SCp.sent_command = -1;
                    /* retry */
                    scp->SCp.this_residual = HIGH_PRI;
                    return 2;
                }
                memset((char*)scp->sense_buffer,0,16);
                scp->sense_buffer[0] = 0x70;
                scp->sense_buffer[2] = NOT_READY;
                scp->result = (DID_OK << 16) | (CHECK_CONDITION << 1);
            } else if (service == CACHESERVICE) {
                if (ha->status == S_CACHE_UNKNOWN &&
                    (ha->hdr[scp->target].cluster_type & 
                     CLUSTER_RESERVE_STATE) == CLUSTER_RESERVE_STATE) {
                    /* bus reset -> force GDT_CLUST_INFO */
                    ha->hdr[scp->target].cluster_type &= ~CLUSTER_RESERVED;
                }
                memset((char*)scp->sense_buffer,0,16);
                if (ha->status == (ushort)S_CACHE_RESERV) {
                    scp->result = (DID_OK << 16) | (RESERVATION_CONFLICT << 1);
                } else {
                    scp->sense_buffer[0] = 0x70;
                    scp->sense_buffer[2] = NOT_READY;
                    scp->result = (DID_OK << 16) | (CHECK_CONDITION << 1);
                }
#if LINUX_VERSION_CODE >= 0x010300
                if (scp->done != gdth_scsi_done)
#endif  
                {
                    ha->dvr.size = sizeof(ha->dvr.eu.sync);
                    ha->dvr.eu.sync.ionode  = hanum;
                    ha->dvr.eu.sync.service = service;
                    ha->dvr.eu.sync.status  = ha->status;
                    ha->dvr.eu.sync.info    = ha->info;
                    ha->dvr.eu.sync.hostdrive = scp->target;
                    if (ha->status >= 0x8000)
                        gdth_store_event(ha, ES_SYNC, 0, &ha->dvr);
                    else
                        gdth_store_event(ha, ES_SYNC, service, &ha->dvr);
                }
            } else {
                /* sense buffer filled from controller firmware (DMA) */
                if (ha->status != S_RAW_SCSI || ha->info >= 0x100) {
                    scp->result = DID_BAD_TARGET << 16;
                } else {
                    scp->result = (DID_OK << 16) | ha->info;
                }
            }
        }
        if (!scp->SCp.have_data_in)
            scp->SCp.have_data_in++;
        else 
            return 1;
    }

    return 0;
}

static char *async_cache_tab[] = {
/* 0*/  "\011\000\002\002\002\004\002\006\004"
        "GDT HA %u, service %u, async. status %u/%lu unknown",
/* 1*/  "\011\000\002\002\002\004\002\006\004"
        "GDT HA %u, service %u, async. status %u/%lu unknown",
/* 2*/  "\005\000\002\006\004"
        "GDT HA %u, Host Drive %lu not ready",
/* 3*/  "\005\000\002\006\004"
        "GDT HA %u, Host Drive %lu: REASSIGN not successful and/or data error on reassigned blocks. Drive may crash in the future and should be replaced",
/* 4*/  "\005\000\002\006\004"
        "GDT HA %u, mirror update on Host Drive %lu failed",
/* 5*/  "\005\000\002\006\004"
        "GDT HA %u, Mirror Drive %lu failed",
/* 6*/  "\005\000\002\006\004"
        "GDT HA %u, Mirror Drive %lu: REASSIGN not successful and/or data error on reassigned blocks. Drive may crash in the future and should be replaced",
/* 7*/  "\005\000\002\006\004"
        "GDT HA %u, Host Drive %lu write protected",
/* 8*/  "\005\000\002\006\004"
        "GDT HA %u, media changed in Host Drive %lu",
/* 9*/  "\005\000\002\006\004"
        "GDT HA %u, Host Drive %lu is offline",
/*10*/  "\005\000\002\006\004"
        "GDT HA %u, media change of Mirror Drive %lu",
/*11*/  "\005\000\002\006\004"
        "GDT HA %u, Mirror Drive %lu is write protected",
/*12*/  "\005\000\002\006\004"
        "GDT HA %u, general error on Host Drive %lu. Please check the devices of this drive!",
/*13*/  "\007\000\002\006\002\010\002"
        "GDT HA %u, Array Drive %u: Cache Drive %u failed",
/*14*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: FAIL state entered",
/*15*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: error",
/*16*/  "\007\000\002\006\002\010\002"
        "GDT HA %u, Array Drive %u: failed drive replaced by Cache Drive %u",
/*17*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: parity build failed",
/*18*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: drive rebuild failed",
/*19*/  "\005\000\002\010\002"
        "GDT HA %u, Test of Hot Fix %u failed",
/*20*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: drive build finished successfully",
/*21*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: drive rebuild finished successfully",
/*22*/  "\007\000\002\006\002\010\002"
        "GDT HA %u, Array Drive %u: Hot Fix %u activated",
/*23*/  "\005\000\002\006\002"
        "GDT HA %u, Host Drive %u: processing of i/o aborted due to serious drive error",
/*24*/  "\005\000\002\010\002"
        "GDT HA %u, mirror update on Cache Drive %u completed",
/*25*/  "\005\000\002\010\002"
        "GDT HA %u, mirror update on Cache Drive %lu failed",
/*26*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: drive rebuild started",
/*27*/  "\005\000\002\012\001"
        "GDT HA %u, Fault bus %u: SHELF OK detected",
/*28*/  "\005\000\002\012\001"
        "GDT HA %u, Fault bus %u: SHELF not OK detected",
/*29*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, Fault bus %u, ID %u: Auto Hot Plug started",
/*30*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, Fault bus %u, ID %u: new disk detected",
/*31*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, Fault bus %u, ID %u: old disk detected",
/*32*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, Fault bus %u, ID %u: plugging an active disk is illegal",
/*33*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, Fault bus %u, ID %u: illegal device detected",
/*34*/  "\011\000\002\012\001\013\001\006\004"
        "GDT HA %u, Fault bus %u, ID %u: insufficient disk capacity (%lu MB required)",
/*35*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, Fault bus %u, ID %u: disk write protected",
/*36*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, Fault bus %u, ID %u: disk not available",
/*37*/  "\007\000\002\012\001\006\004"
        "GDT HA %u, Fault bus %u: swap detected (%lu)",
/*38*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, Fault bus %u, ID %u: Auto Hot Plug finished successfully",
/*39*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, Fault bus %u, ID %u: Auto Hot Plug aborted due to user Hot Plug",
/*40*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, Fault bus %u, ID %u: Auto Hot Plug aborted",
/*41*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, Fault bus %u, ID %u: Auto Hot Plug for Hot Fix started",
/*42*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: drive build started",
/*43*/  "\003\000\002"
        "GDT HA %u, DRAM parity error detected",
/*44*/  "\005\000\002\006\002"
        "GDT HA %u, Mirror Drive %u: update started",
/*45*/  "\007\000\002\006\002\010\002"
        "GDT HA %u, Mirror Drive %u: Hot Fix %u activated",
/*46*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: no matching Pool Hot Fix Drive available",
/*47*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: Pool Hot Fix Drive available",
/*48*/  "\005\000\002\006\002"
        "GDT HA %u, Mirror Drive %u: no matching Pool Hot Fix Drive available",
/*49*/  "\005\000\002\006\002"
        "GDT HA %u, Mirror Drive %u: Pool Hot Fix Drive available",
/*50*/  "\007\000\002\012\001\013\001"
        "GDT HA %u, SCSI bus %u, ID %u: IGNORE_WIDE_RESIDUE message received",
/*51*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: expand started",
/*52*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: expand finished successfully",
/*53*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: expand failed",
/*54*/  "\003\000\002"
        "GDT HA %u, CPU temperature critical",
/*55*/  "\003\000\002"
        "GDT HA %u, CPU temperature OK",
/*56*/  "\005\000\002\006\004"
        "GDT HA %u, Host drive %lu created",
/*57*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: expand restarted",
/*58*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: expand stopped",
/*59*/  "\005\000\002\010\002"
        "GDT HA %u, Mirror Drive %u: drive build quited",
/*60*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: parity build quited",
/*61*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: drive rebuild quited",
/*62*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: parity verify started",
/*63*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: parity verify done",
/*64*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: parity verify failed",
/*65*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: parity error detected",
/*66*/  "\005\000\002\006\002"
        "GDT HA %u, Array Drive %u: parity verify quited",
/*67*/  "\005\000\002\006\002"
        "GDT HA %u, Host Drive %u reserved",
/*68*/  "\005\000\002\006\002"
        "GDT HA %u, Host Drive %u mounted and released",
/*69*/  "\005\000\002\006\002"
        "GDT HA %u, Host Drive %u released",
/*70*/  "\003\000\002"
        "GDT HA %u, DRAM error detected and corrected with ECC",
/*71*/  "\003\000\002"
        "GDT HA %u, Uncorrectable DRAM error detected with ECC",
/*72*/  "\011\000\002\012\001\013\001\014\001"
        "GDT HA %u, SCSI bus %u, ID %u, LUN %u: reassigning block",
/*73*/  "\005\000\002\006\002"
        "GDT HA %u, Host drive %u resetted locally",
/*74*/  "\005\000\002\006\002"
        "GDT HA %u, Host drive %u resetted remotely",
/*75*/  "\003\000\002"
        "GDT HA %u, async. status 75 unknown",
};


static int gdth_async_event(int hanum)
{
    gdth_ha_str *ha;
    gdth_msg_str *msg;
    gdth_cmd_str *cmdp;
    int cmd_index;

    ha  = HADATA(gdth_ctr_tab[hanum]);
    cmdp= ha->pccb;
    msg = (gdth_msg_str *)ha->pscratch;
    TRACE2(("gdth_async_event() ha %d serv %d\n",
            hanum,ha->service));

    if (ha->service == SCREENSERVICE) {
        if (ha->status == MSG_REQUEST) {
            while (gdth_test_busy(hanum))
                gdth_delay(0);
            cmdp->Service       = SCREENSERVICE;
            cmdp->RequestBuffer = SCREEN_CMND;
            cmd_index = gdth_get_cmd_index(hanum);
            gdth_set_sema0(hanum);
            cmdp->OpCode        = GDT_READ;
            cmdp->BoardNode     = LOCALBOARD;
            cmdp->u.screen.reserved  = 0;
            cmdp->u.screen.su.msg.msg_handle= MSG_INV_HANDLE;
            cmdp->u.screen.su.msg.msg_addr  = virt_to_bus(msg);
            ha->scratch_busy = TRUE;
            ha->cmd_offs_dpmem = 0;
            ha->cmd_len = GDTOFFSOF(gdth_cmd_str,u.screen.su.msg.msg_addr) 
                + sizeof(ulong32);
            ha->cmd_cnt = 0;
            gdth_copy_command(hanum);
            if (ha->type == GDT_EISA)
                printk("[EISA slot %d] ",(ushort)ha->brd_phys);
            else if (ha->type == GDT_ISA)
                printk("[DPMEM 0x%4X] ",(ushort)ha->brd_phys);
            else 
                printk("[PCI %d/%d] ",(ushort)(ha->brd_phys>>8),
                       (ushort)((ha->brd_phys>>3)&0x1f));
            gdth_release_event(hanum);
        }

    } else {
        if (ha->type == GDT_PCIMPR && 
            (ha->fw_vers & 0xff) >= 0x1a) {
            ha->dvr.size = 0;
            ha->dvr.eu.async.ionode = hanum;
            ha->dvr.eu.async.status  = ha->status;
            /* severity and event_string already set! */
        } else {        
            ha->dvr.size = sizeof(ha->dvr.eu.async);
            ha->dvr.eu.async.ionode   = hanum;
            ha->dvr.eu.async.service = ha->service;
            ha->dvr.eu.async.status  = ha->status;
            ha->dvr.eu.async.info    = ha->info;
            *(ulong32 *)ha->dvr.eu.async.scsi_coord  = ha->info2;
        }
        gdth_store_event( ha, ES_ASYNC, ha->service, &ha->dvr );
        gdth_log_event( &ha->dvr, NULL );
    
        /* new host drive from expand? */
        if (ha->service == CACHESERVICE && ha->status == 56) {
            TRACE2(("gdth_async_event(): new host drive %d created\n",
                    (ushort)ha->info));
            /* gdth_analyse_hdrive(hanum, (ushort)ha->info); */
        }   
    }
    return 1;
}

static void gdth_log_event(gdth_evt_data *dvr, char *buffer)
{
    gdth_stackframe stack;
    char *f = NULL;
    int i,j;

    TRACE2(("gdth_log_event()\n"));
    if (dvr->size == 0) {
        if (buffer == NULL) {
            printk("Adapter %d: %s\n",dvr->eu.async.ionode,dvr->event_string); 
        } else {
            sprintf(buffer,"Adapter %d: %s\n",
                dvr->eu.async.ionode,dvr->event_string); 
        }
    } else if (dvr->eu.async.service == CACHESERVICE && 
        INDEX_OK(dvr->eu.async.status, async_cache_tab)) {
        TRACE2(("GDT: Async. event cache service, event no.: %d\n",
                dvr->eu.async.status));
        
        f = async_cache_tab[dvr->eu.async.status];
        
        /* i: parameter to push, j: stack element to fill */
        for (j=0,i=1; i < f[0]; i+=2) {
            switch (f[i+1]) {
              case 4:
                stack.b[j++] = *(ulong32*)&dvr->eu.stream[(int)f[i]];
                break;
              case 2:
                stack.b[j++] = *(ushort*)&dvr->eu.stream[(int)f[i]];
                break;
              case 1:
                stack.b[j++] = *(unchar*)&dvr->eu.stream[(int)f[i]];
                break;
              default:
                break;
            }
        }
        
        if (buffer == NULL) {
            printk(&f[(int)f[0]],stack); 
            printk("\n");
        } else {
            sprintf(buffer,&f[(int)f[0]],stack); 
        }

    } else {
        if (buffer == NULL) {
            printk("GDT HA %u, Unknown async. event service %d event no. %d\n",
                   dvr->eu.async.ionode,dvr->eu.async.service,dvr->eu.async.status);
        } else {
            sprintf(buffer,"GDT HA %u, Unknown async. event service %d event no. %d",
                    dvr->eu.async.ionode,dvr->eu.async.service,dvr->eu.async.status);
        }
    }
}

#ifdef GDTH_STATISTICS
void gdth_timeout(ulong data)
{
    ulong32 i;
    Scsi_Cmnd *nscp;
    gdth_ha_str *ha;
    ulong flags;
    int hanum = 0;

    ha = HADATA(gdth_ctr_tab[hanum]);
    GDTH_LOCK_HA(ha, flags);

    for (act_stats=0,i=0; i<GDTH_MAXCMDS; ++i) 
        if (ha->cmd_tab[i].cmnd != UNUSED_CMND)
            ++act_stats;

    for (act_rq=0,nscp=ha->req_first; nscp; nscp=(Scsi_Cmnd*)nscp->SCp.ptr)
        ++act_rq;

    TRACE2(("gdth_to(): ints %d, ios %d, act_stats %d, act_rq %d\n",
            act_ints, act_ios, act_stats, act_rq));
    act_ints = act_ios = 0;

    gdth_timer.expires = jiffies + 30 * HZ;
    add_timer(&gdth_timer);
    GDTH_UNLOCK_HA(ha, flags);
}
#endif

GDTH_INITFUNC(void, internal_setup(char *str,int *ints))
{
    int i, argc;
    char *cur_str, *argv;

    TRACE2(("internal_setup() str %s ints[0] %d\n", 
            str ? str:"NULL", ints ? ints[0]:0));

    /* read irq[] from ints[] */
    if (ints) {
        argc = ints[0];
        if (argc > 0) {
            if (argc > MAXHA)
                argc = MAXHA;
            for (i = 0; i < argc; ++i)
                irq[i] = ints[i+1];
        }
    }

    /* analyse string */
    argv = str;
    while (argv && (cur_str = strchr(argv, ':'))) {
        int val = 0, c = *++cur_str;
        
        if (c == 'n' || c == 'N')
            val = 0;
        else if (c == 'y' || c == 'Y')
            val = 1;
        else
            val = (int)simple_strtoul(cur_str, NULL, 0);

        if (!strncmp(argv, "disable:", 8))
            disable = val;
        else if (!strncmp(argv, "reserve_mode:", 13))
            reserve_mode = val;
        else if (!strncmp(argv, "reverse_scan:", 13))
            reverse_scan = val;
        else if (!strncmp(argv, "hdr_channel:", 12))
            hdr_channel = val;
        else if (!strncmp(argv, "max_ids:", 8))
            max_ids = val;
        else if (!strncmp(argv, "rescan:", 7))
            rescan = val;
        else if (!strncmp(argv, "virt_ctr:", 9))
            virt_ctr = val;
        else if (!strncmp(argv, "shared_access:", 14))
            shared_access = val;
        else if (!strncmp(argv, "reserve_list:", 13)) {
            reserve_list[0] = val;
            for (i = 1; i < MAX_RES_ARGS; i++) {
                cur_str = strchr(cur_str, ',');
                if (!cur_str)
                    break;
                if (!isdigit((int)*++cur_str)) {
                    --cur_str;          
                    break;
                }
                reserve_list[i] = 
                    (int)simple_strtoul(cur_str, NULL, 0);
            }
            if (!cur_str)
                break;
            argv = ++cur_str;
            continue;
        }

        if ((argv = strchr(argv, ',')))
            ++argv;
    }
}

GDTH_INITFUNC(int, option_setup(char *str))
{
    int ints[MAXHA];
    char *cur = str;
    int i = 1;

    TRACE2(("option_setup() str %s\n", str ? str:"NULL")); 

    while (cur && isdigit(*cur) && i <= MAXHA) {
        ints[i++] = simple_strtoul(cur, NULL, 0);
        if ((cur = strchr(cur, ',')) != NULL) cur++;
    }

    ints[0] = i - 1;
    internal_setup(cur, ints);
    return 1;
}

GDTH_INITFUNC(int, gdth_detect(Scsi_Host_Template *shtp))
{
    struct Scsi_Host *shp;
    gdth_ha_str *ha;
    ulong32 isa_bios;
    ushort eisa_slot;
    int i,hanum,cnt,ctr;
    unchar b;
    
 
#ifdef DEBUG_GDTH
    printk("GDT: This driver contains debugging information !! Trace level = %d\n",
        DebugState);
    printk("     Destination of debugging information: ");
#ifdef __SERIAL__
#ifdef __COM2__
    printk("Serial port COM2\n");
#else
    printk("Serial port COM1\n");
#endif
#else
    printk("Console\n");
#endif
    gdth_delay(3000);
#endif

    TRACE(("gdth_detect()\n"));

    if (disable) {
        printk("GDT: Controller driver disabled from command line !\n");
        return 0;
    }

    printk("GDT: Storage RAID Controller Driver. Version: %s \n",GDTH_VERSION_STR);
    /* initializations */
    gdth_polling = TRUE; b = 0;
    gdth_clear_events();

    /* scanning for controllers, at first: ISA controller */
    for (isa_bios=0xc8000UL; isa_bios<=0xd8000UL; isa_bios+=0x8000UL) {
        if (gdth_ctr_count >= MAXHA) 
            break;
        if (gdth_search_isa(isa_bios)) {        /* controller found */
            shp = scsi_register(shtp,sizeof(gdth_ext_str));
            if(shp == NULL)
            	continue;
            ha = HADATA(shp);
            if (!gdth_init_isa(isa_bios,ha)) {
                scsi_unregister(shp);
                continue;
            }
#ifdef __ia64__
            break;
#else
            /* controller found and initialized */
            printk("Configuring GDT-ISA HA at BIOS 0x%05X IRQ %u DRQ %u\n",
                   isa_bios,ha->irq,ha->drq);

#if LINUX_VERSION_CODE >= 0x010346 
            if (request_irq(ha->irq,gdth_interrupt,SA_INTERRUPT,"gdth",ha))
#else
            if (request_irq(ha->irq,gdth_interrupt,SA_INTERRUPT,"gdth")) 
#endif
            {
                printk("GDT-ISA: Unable to allocate IRQ\n");
                scsi_unregister(shp);
                continue;
            }
            if (request_dma(ha->drq,"gdth")) {
                printk("GDT-ISA: Unable to allocate DMA channel\n");
#if LINUX_VERSION_CODE >= 0x010346 
                free_irq(ha->irq,ha);
#else
                free_irq(ha->irq);
#endif
                scsi_unregister(shp);
                continue;
            }
            set_dma_mode(ha->drq,DMA_MODE_CASCADE);
            enable_dma(ha->drq);
            shp->unchecked_isa_dma = 1;
            shp->irq = ha->irq;
            shp->dma_channel = ha->drq;
            hanum = gdth_ctr_count;         
            gdth_ctr_tab[gdth_ctr_count++] = shp;
            gdth_ctr_vtab[gdth_ctr_vcount++] = shp;

            NUMDATA(shp)->hanum = (ushort)hanum;
            NUMDATA(shp)->busnum= 0;

            ha->pccb = CMDDATA(shp);
#if LINUX_VERSION_CODE >= 0x020322
            ha->pscratch = (void *) __get_free_pages(GFP_ATOMIC | GFP_DMA, 
                                                     GDTH_SCRATCH_ORD);
#else
            ha->pscratch = scsi_init_malloc(GDTH_SCRATCH, GFP_ATOMIC | GFP_DMA);
#endif
            ha->scratch_busy = FALSE;
            ha->req_first = NULL;
            ha->tid_cnt = MAX_HDRIVES;
            if (max_ids > 0 && max_ids < ha->tid_cnt)
                ha->tid_cnt = max_ids;
            for (i=0; i<GDTH_MAXCMDS; ++i)
                ha->cmd_tab[i].cmnd = UNUSED_CMND;
            ha->scan_mode = rescan ? 0x10 : 0;

            if (ha->pscratch == NULL || !gdth_search_drives(hanum)) {
                printk("GDT-ISA: Error during device scan\n");
                --gdth_ctr_count;
                --gdth_ctr_vcount;
                if (ha->pscratch != NULL)
#if LINUX_VERSION_CODE >= 0x020322
                    free_pages((unsigned long)ha->pscratch, GDTH_SCRATCH_ORD);
#else
                    scsi_init_free((void *)ha->pscratch, GDTH_SCRATCH);
#endif
#if LINUX_VERSION_CODE >= 0x010346 
                free_irq(ha->irq,ha);
#else
                free_irq(ha->irq);
#endif
                scsi_unregister(shp);
                continue;
            }
            if (hdr_channel < 0 || hdr_channel > ha->bus_cnt)
                hdr_channel = ha->bus_cnt;
            ha->virt_bus = hdr_channel;

#if LINUX_VERSION_CODE >= 0x020000
            shp->max_id      = ha->tid_cnt;
            shp->max_lun     = MAXLUN;
            shp->max_channel = virt_ctr ? 0 : ha->bus_cnt;
            if (virt_ctr)  
#endif
            {
                virt_ctr = 1;
                /* register addit. SCSI channels as virtual controllers */
                for (b = 1; b < ha->bus_cnt + 1; ++b) {
                    shp = scsi_register(shtp,sizeof(gdth_num_str));
                    shp->unchecked_isa_dma = 1;
                    shp->irq = ha->irq;
                    shp->dma_channel = ha->drq;
                    gdth_ctr_vtab[gdth_ctr_vcount++] = shp;
                    NUMDATA(shp)->hanum = (ushort)hanum;
                    NUMDATA(shp)->busnum = b;
                }
            }  

            GDTH_INIT_LOCK_HA(ha);
            gdth_enable_int(hanum);
#endif /* !__ia64__ */
        }
    }

    /* scanning for EISA controllers */
    for (eisa_slot=0x1000; eisa_slot<=0x8000; eisa_slot+=0x1000) {
        if (gdth_ctr_count >= MAXHA) 
            break;
        if (gdth_search_eisa(eisa_slot)) {      /* controller found */
            shp = scsi_register(shtp,sizeof(gdth_ext_str));
	    if(shp == NULL)
		    continue;

            ha = HADATA(shp);
            if (!gdth_init_eisa(eisa_slot,ha)) {
                scsi_unregister(shp);
                continue;
            }
            /* controller found and initialized */
            printk("Configuring GDT-EISA HA at Slot %d IRQ %u\n",
                   eisa_slot>>12,ha->irq);

#if LINUX_VERSION_CODE >= 0x010346 
            if (request_irq(ha->irq,gdth_interrupt,SA_INTERRUPT,"gdth",ha))
#else
            if (request_irq(ha->irq,gdth_interrupt,SA_INTERRUPT,"gdth")) 
#endif
            {
                printk("GDT-EISA: Unable to allocate IRQ\n");
                scsi_unregister(shp);
                continue;
            }
            shp->unchecked_isa_dma = 0;
            shp->irq = ha->irq;
            shp->dma_channel = 0xff;
            hanum = gdth_ctr_count;
            gdth_ctr_tab[gdth_ctr_count++] = shp;
            gdth_ctr_vtab[gdth_ctr_vcount++] = shp;

            NUMDATA(shp)->hanum = (ushort)hanum;
            NUMDATA(shp)->busnum= 0;
            TRACE2(("EISA detect Bus 0: hanum %d\n",
                    NUMDATA(shp)->hanum));

            ha->pccb = CMDDATA(shp);
#if LINUX_VERSION_CODE >= 0x020322
            ha->pscratch = (void *) __get_free_pages(GFP_ATOMIC | GFP_DMA, 
                                                     GDTH_SCRATCH_ORD);
#else
            ha->pscratch = scsi_init_malloc(GDTH_SCRATCH, GFP_ATOMIC | GFP_DMA);
#endif
            ha->scratch_busy = FALSE;
            ha->req_first = NULL;
            ha->tid_cnt = MAX_HDRIVES;
            if (max_ids > 0 && max_ids < ha->tid_cnt)
                ha->tid_cnt = max_ids;
            for (i=0; i<GDTH_MAXCMDS; ++i)
                ha->cmd_tab[i].cmnd = UNUSED_CMND;
            ha->scan_mode = rescan ? 0x10 : 0;

            if (ha->pscratch == NULL || !gdth_search_drives(hanum)) {
                printk("GDT-EISA: Error during device scan\n");
                --gdth_ctr_count;
                --gdth_ctr_vcount;
                if (ha->pscratch != NULL)
#if LINUX_VERSION_CODE >= 0x020322
                    free_pages((unsigned long)ha->pscratch, GDTH_SCRATCH_ORD);
#else
                    scsi_init_free((void *)ha->pscratch, GDTH_SCRATCH);
#endif
#if LINUX_VERSION_CODE >= 0x010346 
                free_irq(ha->irq,ha);
#else
                free_irq(ha->irq);
#endif
                scsi_unregister(shp);
                continue;
            }
            if (hdr_channel < 0 || hdr_channel > ha->bus_cnt)
                hdr_channel = ha->bus_cnt;
            ha->virt_bus = hdr_channel;

#if LINUX_VERSION_CODE >= 0x020000
            shp->max_id      = ha->tid_cnt;
            shp->max_lun     = MAXLUN;
            shp->max_channel = virt_ctr ? 0 : ha->bus_cnt;
            if (virt_ctr)  
#endif
            {
                virt_ctr = 1;
                /* register addit. SCSI channels as virtual controllers */
                for (b = 1; b < ha->bus_cnt + 1; ++b) {
                    shp = scsi_register(shtp,sizeof(gdth_num_str));
                    shp->unchecked_isa_dma = 0;
                    shp->irq = ha->irq;
                    shp->dma_channel = 0xff;
                    gdth_ctr_vtab[gdth_ctr_vcount++] = shp;
                    NUMDATA(shp)->hanum = (ushort)hanum;
                    NUMDATA(shp)->busnum = b;
                }
            }  

            GDTH_INIT_LOCK_HA(ha);
            gdth_enable_int(hanum);
        }
    }

    /* scanning for PCI controllers */
#if LINUX_VERSION_CODE >= 0x2015C
    if (pci_present())
#else
    if (pcibios_present())
#endif
    {
        gdth_pci_str pcistr[MAXHA];

        cnt = gdth_search_pci(pcistr);
	printk("GDT: Found %d PCI Storage RAID Controllers\n",cnt);
        gdth_sort_pci(pcistr,cnt);
        for (ctr = 0; ctr < cnt; ++ctr) {
            if (gdth_ctr_count >= MAXHA)
                break;
            shp = scsi_register(shtp,sizeof(gdth_ext_str));
	    if(shp == NULL)
		    continue;

            ha = HADATA(shp);
            if (!gdth_init_pci(&pcistr[ctr],ha)) {
                scsi_unregister(shp);
                continue;
            }
            /* controller found and initialized */
            printk("GDT CTR%d: Configuring GDT-PCI HA at %d/%d IRQ %u\n",
                   ctr,pcistr[ctr].bus,PCI_SLOT(pcistr[ctr].device_fn),ha->irq);

#if LINUX_VERSION_CODE >= 0x010346 
            if (request_irq(ha->irq, gdth_interrupt,
                            SA_INTERRUPT|SA_SHIRQ, "gdth", ha))
#else
            if (request_irq(ha->irq, gdth_interrupt,
                            SA_INTERRUPT|SA_SHIRQ, "gdth")) 
#endif
            {
                printk("GDT-PCI: Unable to allocate IRQ\n");
                scsi_unregister(shp);
                continue;
            }
            shp->unchecked_isa_dma = 0;
            shp->irq = ha->irq;
            shp->dma_channel = 0xff;
            hanum = gdth_ctr_count;
            gdth_ctr_tab[gdth_ctr_count++] = shp;
            gdth_ctr_vtab[gdth_ctr_vcount++] = shp;

            NUMDATA(shp)->hanum = (ushort)hanum;
            NUMDATA(shp)->busnum= 0;

            ha->pccb = CMDDATA(shp);
#if LINUX_VERSION_CODE >= 0x020322
            ha->pscratch = (void *) __get_free_pages(GFP_ATOMIC | GFP_DMA, 
                                                     GDTH_SCRATCH_ORD);
#else
            ha->pscratch = scsi_init_malloc(GDTH_SCRATCH, GFP_ATOMIC | GFP_DMA);
#endif
            ha->scratch_busy = FALSE;
            ha->req_first = NULL;
            ha->tid_cnt = pcistr[ctr].device_id >= 0x200 ? MAXID : MAX_HDRIVES;
            if (max_ids > 0 && max_ids < ha->tid_cnt)
                ha->tid_cnt = max_ids;
            for (i=0; i<GDTH_MAXCMDS; ++i)
                ha->cmd_tab[i].cmnd = UNUSED_CMND;
            ha->scan_mode = rescan ? 0x10 : 0;

            if (ha->pscratch == NULL || !gdth_search_drives(hanum)) {
                printk("GDT-PCI: Error during device scan\n");
                --gdth_ctr_count;
                --gdth_ctr_vcount;
                if (ha->pscratch != NULL)
#if LINUX_VERSION_CODE >= 0x020322
                    free_pages((unsigned long)ha->pscratch, GDTH_SCRATCH_ORD);
#else
                    scsi_init_free((void *)ha->pscratch, GDTH_SCRATCH);
#endif
#if LINUX_VERSION_CODE >= 0x010346 
                free_irq(ha->irq,ha);
#else
                free_irq(ha->irq);
#endif
                scsi_unregister(shp);
                continue;
            }
            if (hdr_channel < 0 || hdr_channel > ha->bus_cnt)
                hdr_channel = ha->bus_cnt;
            ha->virt_bus = hdr_channel;

#if LINUX_VERSION_CODE >= 0x020000
            shp->max_id      = ha->tid_cnt;
            shp->max_lun     = MAXLUN;
            shp->max_channel = virt_ctr ? 0 : ha->bus_cnt;
            if (virt_ctr)  
#endif
            {
                virt_ctr = 1;
                /* register addit. SCSI channels as virtual controllers */
                for (b = 1; b < ha->bus_cnt + 1; ++b) {
                    shp = scsi_register(shtp,sizeof(gdth_num_str));
                    shp->unchecked_isa_dma = 0;
                    shp->irq = ha->irq;
                    shp->dma_channel = 0xff;
                    gdth_ctr_vtab[gdth_ctr_vcount++] = shp;
                    NUMDATA(shp)->hanum = (ushort)hanum;
                    NUMDATA(shp)->busnum = b;
                }
            }  

            GDTH_INIT_LOCK_HA(ha);
            gdth_enable_int(hanum);
        }
    }

    TRACE2(("gdth_detect() %d controller detected\n",gdth_ctr_count));
    if (gdth_ctr_count > 0) {
#ifdef GDTH_STATISTICS
        TRACE2(("gdth_detect(): Initializing timer !\n"));
        init_timer(&gdth_timer);
        gdth_timer.expires = jiffies + HZ;
        gdth_timer.data = 0L;
        gdth_timer.function = gdth_timeout;
        add_timer(&gdth_timer);
#endif
#if LINUX_VERSION_CODE >= 0x020100
        register_reboot_notifier(&gdth_notifier);
#endif
    }
    gdth_polling = FALSE;
    return gdth_ctr_vcount;
}


int gdth_release(struct Scsi_Host *shp)
{
    int hanum;
    gdth_ha_str *ha;

    TRACE2(("gdth_release()\n"));
    if (NUMDATA(shp)->busnum == 0) {
        hanum = NUMDATA(shp)->hanum;
        ha    = HADATA(gdth_ctr_tab[hanum]);
#if LINUX_VERSION_CODE >= 0x010300
        gdth_flush(hanum);
#endif

        if (shp->irq) {
#if LINUX_VERSION_CODE >= 0x010346
            free_irq(shp->irq,ha);
#else
            free_irq(shp->irq);
#endif
        }
#ifndef __ia64__
        if (shp->dma_channel != 0xff) {
            free_dma(shp->dma_channel);
        }
#endif
#if LINUX_VERSION_CODE >= 0x020322
        free_pages((unsigned long)ha->pscratch, GDTH_SCRATCH_ORD);
#else
        scsi_init_free((void *)ha->pscratch, GDTH_SCRATCH);
#endif
        gdth_ctr_released++;
        TRACE2(("gdth_release(): HA %d of %d\n", 
                gdth_ctr_released, gdth_ctr_count));

        if (gdth_ctr_released == gdth_ctr_count) {
#ifdef GDTH_STATISTICS
            del_timer(&gdth_timer);
#endif
#if LINUX_VERSION_CODE >= 0x020100
            unregister_reboot_notifier(&gdth_notifier);
#endif
        }
    }

    scsi_unregister(shp);
    return 0;
}
            

static const char *gdth_ctr_name(int hanum)
{
    gdth_ha_str *ha;

    TRACE2(("gdth_ctr_name()\n"));

    ha    = HADATA(gdth_ctr_tab[hanum]);

    if (ha->type == GDT_EISA) {
        switch (ha->stype) {
          case GDT3_ID:
            return("GDT3000/3020");
          case GDT3A_ID:
            return("GDT3000A/3020A/3050A");
          case GDT3B_ID:
            return("GDT3000B/3010A");
        }
    } else if (ha->type == GDT_ISA) {
        return("GDT2000/2020");
    } else if (ha->type == GDT_PCI) {
        switch (ha->stype) {
          case PCI_DEVICE_ID_VORTEX_GDT60x0:
            return("GDT6000/6020/6050");
          case PCI_DEVICE_ID_VORTEX_GDT6000B:
            return("GDT6000B/6010");
        }
    } 
    /* new controllers (GDT_PCINEW, GDT_PCIMPR, ..) use board_info IOCTL! */

    return("");
}

const char *gdth_info(struct Scsi_Host *shp)
{
    int hanum;
    gdth_ha_str *ha;

    TRACE2(("gdth_info()\n"));
    hanum = NUMDATA(shp)->hanum;
    ha    = HADATA(gdth_ctr_tab[hanum]);

    return ((const char *)ha->binfo.type_string);
}

/* old error handling */
int gdth_abort(Scsi_Cmnd *scp)
{
    TRACE2(("gdth_abort() reason %d\n",scp->abort_reason));
    return SCSI_ABORT_SNOOZE;
}

#if LINUX_VERSION_CODE >= 0x010346
int gdth_reset(Scsi_Cmnd *scp, unsigned int reset_flags)
#else
int gdth_reset(Scsi_Cmnd *scp)
#endif
{
    TRACE2(("gdth_reset()\n"));
    return SCSI_RESET_PUNT;
}

#if LINUX_VERSION_CODE >= 0x02015F
/* new error handling */
int gdth_eh_abort(Scsi_Cmnd *scp)
{
    TRACE2(("gdth_eh_abort()\n"));
    return FAILED;
}

int gdth_eh_device_reset(Scsi_Cmnd *scp)
{
    TRACE2(("gdth_eh_device_reset()\n"));
    return FAILED;
}

int gdth_eh_bus_reset(Scsi_Cmnd *scp)
{
    int i, hanum;
    gdth_ha_str *ha;
    ulong flags;
    Scsi_Cmnd *cmnd;
    unchar b;

    TRACE2(("gdth_eh_bus_reset()\n"));
    hanum = NUMDATA(scp->host)->hanum;
    b = virt_ctr ? NUMDATA(scp->host)->busnum : scp->channel;
    ha    = HADATA(gdth_ctr_tab[hanum]);

    /* clear command tab */
    GDTH_LOCK_HA(ha, flags);
    for (i = 0; i < GDTH_MAXCMDS; ++i) {
        cmnd = ha->cmd_tab[i].cmnd;
        if (!SPECIAL_SCP(cmnd) && cmnd->channel == b)
            ha->cmd_tab[i].cmnd = UNUSED_CMND;
    }
    GDTH_UNLOCK_HA(ha, flags);

    if (b == ha->virt_bus) {
        /* host drives */
        for (i = 0; i < MAX_HDRIVES; ++i) {
            if (ha->hdr[i].present) {
                GDTH_LOCK_HA(ha, flags);
                gdth_polling = TRUE;
                while (gdth_test_busy(hanum))
                    gdth_delay(0);
                if (gdth_internal_cmd(hanum, CACHESERVICE, 
                                      GDT_CLUST_RESET, i, 0, 0))
                    ha->hdr[i].cluster_type &= ~CLUSTER_RESERVED;
                gdth_polling = FALSE;
                GDTH_UNLOCK_HA(ha, flags);
            }
        }
    } else {
        /* raw devices */
        GDTH_LOCK_HA(ha, flags);
        for (i = 0; i < MAXID; ++i)
            ha->raw[BUS_L2P(ha,b)].io_cnt[i] = 0;
        gdth_polling = TRUE;
        while (gdth_test_busy(hanum))
            gdth_delay(0);
        gdth_internal_cmd(hanum, SCSIRAWSERVICE, GDT_RESET_BUS,
                          BUS_L2P(ha,b), 0, 0);
        gdth_polling = FALSE;
        GDTH_UNLOCK_HA(ha, flags);
    }
    return SUCCESS;
}

int gdth_eh_host_reset(Scsi_Cmnd *scp)
{
    TRACE2(("gdth_eh_host_reset()\n"));
    return FAILED;
}
#endif

#if LINUX_VERSION_CODE >= 0x010300
int gdth_bios_param(Disk *disk,kdev_t dev,int *ip)
#else
int gdth_bios_param(Disk *disk,int dev,int *ip)
#endif
{
    unchar b, t;
    int hanum;
    gdth_ha_str *ha;

    hanum = NUMDATA(disk->device->host)->hanum;
    b = virt_ctr ? NUMDATA(disk->device->host)->busnum : disk->device->channel;
    t = disk->device->id;
    TRACE2(("gdth_bios_param() ha %d bus %d target %d\n", hanum, b, t)); 
    ha = HADATA(gdth_ctr_tab[hanum]);

    if (b != ha->virt_bus || ha->hdr[t].heads == 0) {
        /* raw device or host drive without mapping information */
        TRACE2(("Evaluate mapping\n"));
        gdth_eval_mapping(disk->capacity,&ip[2],&ip[0],&ip[1]);
    } else {
        ip[0] = ha->hdr[t].heads;
        ip[1] = ha->hdr[t].secs;
        ip[2] = disk->capacity / ip[0] / ip[1];
    }

    TRACE2(("gdth_bios_param(): %d heads, %d secs, %d cyls\n",
            ip[0],ip[1],ip[2]));
    return 0;
}


int gdth_queuecommand(Scsi_Cmnd *scp,void (*done)(Scsi_Cmnd *))
{
    int hanum;
    int priority;

    TRACE(("gdth_queuecommand() cmd 0x%x id %d lun %d\n",
           scp->cmnd[0],scp->target,scp->lun));
    
    scp->scsi_done = (void *)done;
    scp->SCp.have_data_in = 1;
    scp->SCp.phase = -1;
    scp->SCp.sent_command = -1;
    hanum = NUMDATA(scp->host)->hanum;
#ifdef GDTH_STATISTICS
    ++act_ios;
#endif

    priority = DEFAULT_PRI;
#if LINUX_VERSION_CODE >= 0x010300
    if (scp->done == gdth_scsi_done)
        priority = scp->SCp.this_residual;
#endif
    gdth_update_timeout(hanum, scp, scp->timeout_per_command * 6);
    gdth_putq( hanum, scp, priority );
    gdth_next( hanum );
    return 0;
}

#if LINUX_VERSION_CODE >= 0x010300
/* flush routine */
static void gdth_flush(int hanum)
{
    int             i;
    gdth_ha_str     *ha;
    gdth_cmd_str    gdtcmd;
#if LINUX_VERSION_CODE >= 0x020322
    Scsi_Cmnd       *scp;
    Scsi_Device     *sdev;
#else
    Scsi_Cmnd       scp;
    Scsi_Device     sdev;
#endif
    char            cmnd[MAX_COMMAND_SIZE];   
    memset(cmnd, 0xff, MAX_COMMAND_SIZE);

    TRACE2(("gdth_flush() hanum %d\n",hanum));
    ha = HADATA(gdth_ctr_tab[hanum]);

#if LINUX_VERSION_CODE >= 0x020322
    sdev = scsi_get_host_dev(gdth_ctr_tab[hanum]);
    scp  = scsi_allocate_device(sdev, 1, FALSE);
    scp->cmd_len = 12;
    scp->use_sg = 0;
#else
    memset(&sdev,0,sizeof(Scsi_Device));
    memset(&scp, 0,sizeof(Scsi_Cmnd));
    sdev.host = scp.host = gdth_ctr_tab[hanum];
    sdev.id = scp.target = sdev.host->this_id;
    scp.device = &sdev;
#endif

    for (i = 0; i < MAX_HDRIVES; ++i) {
        if (ha->hdr[i].present) {
            gdtcmd.BoardNode = LOCALBOARD;
            gdtcmd.Service = CACHESERVICE;
            gdtcmd.OpCode = GDT_FLUSH;
            gdtcmd.u.cache.DeviceNo = i;
            gdtcmd.u.cache.BlockNo = 1;
            gdtcmd.u.cache.sg_canz = 0;
            TRACE2(("gdth_flush(): flush ha %d drive %d\n", hanum, i));
#if LINUX_VERSION_CODE >= 0x020322
            gdth_do_cmd(scp, &gdtcmd, cmnd, 30);
#else
            gdth_do_cmd(&scp, &gdtcmd, cmnd, 30);
#endif
        }
    }
#if LINUX_VERSION_CODE >= 0x020322
    scsi_release_command(scp);
    scsi_free_host_dev(sdev);
#endif
}

/* shutdown routine */
#if LINUX_VERSION_CODE >= 0x020100
static int gdth_halt(struct notifier_block *nb, ulong event, void *buf)
#else
void gdth_halt(void)
#endif
{
    int             hanum;
#ifndef __alpha__
    gdth_cmd_str    gdtcmd;
#if LINUX_VERSION_CODE >= 0x020322
    Scsi_Cmnd       *scp;
    Scsi_Device     *sdev;
#else
    Scsi_Cmnd       scp;
    Scsi_Device     sdev;
#endif
    char            cmnd[MAX_COMMAND_SIZE];   
#endif

#if LINUX_VERSION_CODE >= 0x020100
    TRACE2(("gdth_halt() event %d\n",(int)event));
    if (event != SYS_RESTART && event != SYS_HALT && event != SYS_POWER_OFF)
        return NOTIFY_DONE;
#else
    TRACE2(("gdth_halt()\n"));
    if (halt_called) {
        TRACE2(("already called\n"));
        return;
    }
    halt_called = TRUE;
#endif
    printk("GDT: Flushing all host drives .. ");
    for (hanum = 0; hanum < gdth_ctr_count; ++hanum) {
        gdth_flush(hanum);

#ifndef __alpha__
        /* controller reset */
        memset(cmnd, 0xff, MAX_COMMAND_SIZE);
#if LINUX_VERSION_CODE >= 0x020322
        sdev = scsi_get_host_dev(gdth_ctr_tab[hanum]);
        scp  = scsi_allocate_device(sdev, 1, FALSE);
        scp->cmd_len = 12;
        scp->use_sg = 0;
#else
        memset(&sdev,0,sizeof(Scsi_Device));
        memset(&scp, 0,sizeof(Scsi_Cmnd));
        sdev.host = scp.host = gdth_ctr_tab[hanum];
        sdev.id = scp.target = sdev.host->this_id;
        scp.device = &sdev;
#endif

        gdtcmd.BoardNode = LOCALBOARD;
        gdtcmd.Service = CACHESERVICE;
        gdtcmd.OpCode = GDT_RESET;
        TRACE2(("gdth_halt(): reset controller %d\n", hanum));
#if LINUX_VERSION_CODE >= 0x020322
        gdth_do_cmd(scp, &gdtcmd, cmnd, 10);
        scsi_release_command(scp);
        scsi_free_host_dev(sdev);
#else
        gdth_do_cmd(&scp, &gdtcmd, cmnd, 10);
#endif
#endif
    }
    printk("Done.\n");

#ifdef GDTH_STATISTICS
    del_timer(&gdth_timer);
#endif
#if LINUX_VERSION_CODE >= 0x020100
    unregister_reboot_notifier(&gdth_notifier);
    return NOTIFY_OK;
#endif
}
#endif


#if LINUX_VERSION_CODE < 0x020400 && !defined(MODULE)

GDTH_INITFUNC(void, gdth_setup(char *str,int *ints)) 
{    
    TRACE2(("gdth_setup() str %s ints[0] %d\n", 
            str ? str:"NULL", ints ? ints[0]:0));
    internal_setup(str, ints);
}

#else

static Scsi_Host_Template driver_template = GDTH;
#include "scsi_module.c"
#ifndef MODULE
__setup("gdth=", option_setup);
#endif

#endif
