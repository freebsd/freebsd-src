/**********************************************************************
 * iph5526.c: IP/SCSI driver for the Interphase 5526 PCI Fibre Channel
 *			  Card.
 * Copyright (C) 1999 Vineet M Abraham <vmabraham@hotmail.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2, or 
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *********************************************************************/
/**********************************************************************
Log:
Vineet M Abraham
02.12.99 Support multiple cards.
03.15.99 Added Fabric support.
04.04.99 Added N_Port support.
04.15.99 Added SCSI support.
06.18.99 Added ABTS Protocol.
06.24.99 Fixed data corruption when multiple XFER_RDYs are received.
07.07.99 Can be loaded as part of the Kernel. Changed semaphores. Added
         more checks before invalidating SEST entries.
07.08.99 Added Broadcast IP stuff and fixed an unicast timeout bug.
***********************************************************************/
/* TODO:
	R_T_TOV set to 15msec in Loop topology. Need to be 100 msec.
    SMP testing.
	Fix ADISC Tx before completing FLOGI. 
*/	

static const char *version =
    "iph5526.c:v1.0 07.08.99 Vineet Abraham (vmabraham@hotmail.com)\n";

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <asm/system.h>
#include <asm/io.h>

#include <linux/netdevice.h>
#include <linux/fcdevice.h> /* had the declarations for init_fcdev among others + includes if_fcdevice.h */

#include <linux/blk.h>
#include "../../scsi/sd.h"
#include "../../scsi/scsi.h"
#include "../../scsi/hosts.h"
#include "../../fc4/fcp.h"

/* driver specific header files */
#include "tach.h"
#include "tach_structs.h"
#include "iph5526_ip.h"
#include "iph5526_scsi.h"
#include "iph5526_novram.c"

#define RUN_AT(x) (jiffies + (x))

#define DEBUG_5526_0 0
#define DEBUG_5526_1 0
#define DEBUG_5526_2 0

#if DEBUG_5526_0
#define DPRINTK(format, a...) {printk("%s: ", fi->name); \
							   printk(format, ##a); \
							   printk("\n");}
#define ENTER(x)	{printk("%s: ", fi->name); \
					 printk("iph5526.c : entering %s()\n", x);}
#define LEAVE(x)	{printk("%s: ", fi->name); \
					 printk("iph5526.c : leaving %s()\n",x);}

#else
#define DPRINTK(format, a...) {}
#define ENTER(x)	{}
#define LEAVE(x)	{}
#endif

#if DEBUG_5526_1
#define DPRINTK1(format, a...) {printk("%s: ", fi->name); \
							   printk(format, ##a); \
							   printk("\n");}
#else
#define DPRINTK1(format, a...) {}
#endif

#if DEBUG_5526_2
#define DPRINTK2(format, a...) {printk("%s: ", fi->name); \
							   printk(format, ##a); \
							   printk("\n");}
#else
#define DPRINTK2(format, a...) {}
#endif

#define T_MSG(format, a...) {printk("%s: ", fi->name); \
							 printk(format, ##a);\
							 printk("\n");}

#define ALIGNED_SFS_ADDR(addr) ((((unsigned long)(addr) + (SFS_BUFFER_SIZE - 1)) & ~(SFS_BUFFER_SIZE - 1)) - (unsigned long)(addr))
#define ALIGNED_ADDR(addr, len) ((((unsigned long)(addr) + (len - 1)) & ~(len - 1)) - (unsigned long)(addr))


static struct pci_device_id iph5526_pci_tbl[] __initdata = {
	{ PCI_VENDOR_ID_INTERPHASE, PCI_DEVICE_ID_INTERPHASE_5526, PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_INTERPHASE, PCI_DEVICE_ID_INTERPHASE_55x6, PCI_ANY_ID, PCI_ANY_ID, },
	{ }			/* Terminating entry */
};
MODULE_DEVICE_TABLE(pci, iph5526_pci_tbl);

MODULE_LICENSE("GPL");

#define MAX_FC_CARDS 2
static struct fc_info *fc[MAX_FC_CARDS+1];
static unsigned int pci_irq_line;
static struct {
	unsigned short vendor_id;
	unsigned short device_id;
	char *name;
}
clone_list[] __initdata  = {
	{PCI_VENDOR_ID_INTERPHASE, PCI_DEVICE_ID_INTERPHASE_5526, "Interphase Fibre Channel HBA"},
	{PCI_VENDOR_ID_INTERPHASE, PCI_DEVICE_ID_INTERPHASE_55x6, "Interphase Fibre Channel HBA"},
	{0,}
};

static void tachyon_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void tachyon_interrupt_handler(int irq, void* dev_id, struct pt_regs* regs);

static int initialize_register_pointers(struct fc_info *fi);
void clean_up_memory(struct fc_info *fi);

static int tachyon_init(struct fc_info *fi);
static int build_queues(struct fc_info *fi);
static void build_tachyon_header(struct fc_info *fi, u_int my_id, u_int r_ctl, u_int d_id, u_int type, u_char seq_id, u_char df_ctl, u_short ox_id, u_short rx_id, char *data);
static int get_free_header(struct fc_info *fi);
static void build_EDB(struct fc_info *fi, char *data, u_short flags, u_short len);
static int get_free_EDB(struct fc_info *fi);
static void build_ODB(struct fc_info *fi, u_char seq_id, u_int d_id, u_int len, u_int cntl, u_short mtu, u_short ox_id, u_short rx_id, int NW_header, int int_required, u_int frame_class);
static void write_to_tachyon_registers(struct fc_info *fi);
static void reset_latch(struct fc_info *fi);
static void reset_tachyon(struct fc_info *fi, u_int value);
static void take_tachyon_offline(struct fc_info *fi);
static void read_novram(struct fc_info *fi);
static void reset_ichip(struct fc_info *fi);
static void update_OCQ_indx(struct fc_info *fi);
static void update_IMQ_indx(struct fc_info *fi, int count);
static void update_SFSBQ_indx(struct fc_info *fi);
static void update_MFSBQ_indx(struct fc_info *fi, int count);
static void update_tachyon_header_indx(struct fc_info *fi);
static void update_EDB_indx(struct fc_info *fi);
static void handle_FM_interrupt(struct fc_info *fi);
static void handle_MFS_interrupt(struct fc_info *fi);
static void handle_OOO_interrupt(struct fc_info *fi);
static void handle_SFS_interrupt(struct fc_info *fi);
static void handle_OCI_interrupt(struct fc_info *fi);
static void handle_SFS_BUF_WARN_interrupt(struct fc_info *fi);
static void handle_MFS_BUF_WARN_interrupt(struct fc_info *fi);
static void handle_IMQ_BUF_WARN_interrupt(struct fc_info *fi);
static void handle_Unknown_Frame_interrupt(struct fc_info *fi);
static void handle_Busied_Frame_interrupt(struct fc_info *fi);
static void handle_Bad_SCSI_Frame_interrupt(struct fc_info *fi);
static void handle_Inbound_SCSI_Status_interrupt(struct fc_info *fi);
static void handle_Inbound_SCSI_Command_interrupt(struct fc_info *fi);
static void completion_message_handler(struct fc_info *fi, u_int imq_int_type);
static void fill_login_frame(struct fc_info *fi, u_int logi);

static int tx_exchange(struct fc_info *fi, char *data, u_int len, u_int r_ctl, u_int type, u_int d_id, u_int mtu, int int_required, u_short ox_id, u_int frame_class);
static int tx_sequence(struct fc_info *fi, char *data, u_int len, u_int mtu, u_int d_id, u_short ox_id, u_short rx_id, u_char seq_id, int NW_flag, int int_required, u_int frame_class);
static int validate_login(struct fc_info *fi, u_int *base_ptr);
static void add_to_address_cache(struct fc_info *fi, u_int *base_ptr);
static void remove_from_address_cache(struct fc_info *fi, u_int *data, u_int cmnd_code);
static int node_logged_in_prev(struct fc_info *fi, u_int *buff_addr);
static int sid_logged_in(struct fc_info *fi, u_int s_id);
static struct fc_node_info *look_up_cache(struct fc_info *fi, char *data);
static int display_cache(struct fc_info *fi);

static void tx_logi(struct fc_info *fi, u_int logi, u_int d_id);
static void tx_logi_acc(struct fc_info *fi, u_int logi, u_int d_id, u_short received_ox_id);
static void tx_prli(struct fc_info *fi, u_int command_code, u_int d_id, u_short received_ox_id);
static void tx_logo(struct fc_info *fi, u_int d_id, u_short received_ox_id);
static void tx_adisc(struct fc_info *fi, u_int cmnd_code, u_int d_id, u_short received_ox_id);
static void tx_ls_rjt(struct fc_info *fi, u_int d_id, u_short received_ox_id, u_short reason_code, u_short expln_code);
static u_int plogi_ok(struct fc_info *fi, u_int *buff_addr, int size);
static void tx_acc(struct fc_info *fi, u_int d_id, u_short received_ox_id);
static void tx_name_server_req(struct fc_info *fi, u_int req);
static void rscn_handler(struct fc_info *fi, u_int node_id);
static void tx_scr(struct fc_info *fi);
static void scr_timer(unsigned long data);
static void explore_fabric(struct fc_info *fi, u_int *buff_addr);
static void perform_adisc(struct fc_info *fi);
static void local_port_discovery(struct fc_info *fi);
static void add_to_ox_id_list(struct fc_info *fi, u_int transaction_id, u_int cmnd_code);
static u_int remove_from_ox_id_list(struct fc_info *fi, u_short received_ox_id);
static void add_display_cache_timer(struct fc_info *fi);

/* Timers... */
static void nos_ols_timer(unsigned long data);
static void loop_timer(unsigned long data);
static void fabric_explore_timer(unsigned long data);
static void port_discovery_timer(unsigned long data);
static void display_cache_timer(unsigned long data);

/* SCSI Stuff */
static int add_to_sest(struct fc_info *fi, Scsi_Cmnd *Cmnd, struct fc_node_info *ni);
static struct fc_node_info *resolve_target(struct fc_info *fi, u_char target);
static void update_FCP_CMND_indx(struct fc_info *fi);
static int get_free_SDB(struct fc_info *fi);
static void update_SDB_indx(struct fc_info *fi);
static void mark_scsi_sid(struct fc_info *fi, u_int *buff_addr, u_char action);
static void invalidate_SEST_entry(struct fc_info *fi, u_short received_ox_id);
static int abort_exchange(struct fc_info *fi, u_short ox_id);
static void flush_tachyon_cache(struct fc_info *fi, u_short ox_id);
static int get_scsi_oxid(struct fc_info *fi);
static void update_scsi_oxid(struct fc_info *fi);

static Scsi_Host_Template driver_template = IPH5526_SCSI_FC;

static void iph5526_timeout(struct net_device *dev);

static int iph5526_probe_pci(struct net_device *dev);

int __init iph5526_probe(struct net_device *dev)
{
	if (pci_present() && (iph5526_probe_pci(dev) == 0))
		return 0;
    return -ENODEV;
}

static int __init iph5526_probe_pci(struct net_device *dev)
{
#ifdef MODULE
	struct fc_info *fi = (struct fc_info *)dev->priv;
#else
	struct fc_info *fi;
	static int count;
 
	if(fc[count] != NULL) {
		if (dev == NULL) {
			dev = init_fcdev(NULL, 0);
			if (dev == NULL)
				return -ENOMEM;
		}
		fi = fc[count];
#endif
		fi->dev = dev;
		dev->base_addr = fi->base_addr;
		dev->irq = fi->irq;
		if (dev->priv == NULL) 
			dev->priv = fi; 
		fcdev_init(dev);
		/* Assign ur MAC address.
		 */
		dev->dev_addr[0] = (fi->g.my_port_name_high & 0x0000FF00) >> 8;
		dev->dev_addr[1] = fi->g.my_port_name_high;
		dev->dev_addr[2] = (fi->g.my_port_name_low & 0xFF000000) >> 24;
		dev->dev_addr[3] = (fi->g.my_port_name_low & 0x00FF0000) >> 16;
		dev->dev_addr[4] = (fi->g.my_port_name_low & 0x0000FF00) >> 8;
		dev->dev_addr[5] = fi->g.my_port_name_low;
#ifndef MODULE
		count++;
	}
	else
		return -ENODEV;
#endif
	display_cache(fi);
	return 0;
}

static int __init fcdev_init(struct net_device *dev)
{
	dev->open = iph5526_open;
	dev->stop = iph5526_close;
	dev->hard_start_xmit = iph5526_send_packet;
	dev->get_stats = iph5526_get_stats;
	dev->set_multicast_list = NULL;
	dev->change_mtu = iph5526_change_mtu; 
	dev->tx_timeout = iph5526_timeout;
	dev->watchdog_timeo = 5*HZ;
#ifndef MODULE
	fc_setup(dev);
#endif
	return 0;
}

/* initialize tachyon and take it OnLine */
static int tachyon_init(struct fc_info *fi)
{
	ENTER("tachyon_init");
	if (build_queues(fi) == 0) {
		T_MSG("build_queues() failed");
		return 0;
	}

	/* Retrieve your port/node name.
	 */
	read_novram(fi);

	reset_ichip(fi);

	reset_tachyon(fi, SOFTWARE_RESET);

	LEAVE("tachyon_init");
	return 1;
}

/* Build the 4 Qs - IMQ, OCQ, MFSBQ, SFSBQ */
/* Lots of dma_pages needed as Tachyon DMAs almost everything into 
 * host memory.
 */
static int build_queues(struct fc_info *fi)
{
int i,j;
u_char *addr;
	ENTER("build_queues");
	/* Initializing Queue Variables.
	 */
	fi->q.ptr_host_ocq_cons_indx = NULL;
	fi->q.ptr_host_hpcq_cons_indx = NULL;
	fi->q.ptr_host_imq_prod_indx = NULL;

	fi->q.ptr_ocq_base = NULL;
	fi->q.ocq_len = 0;
	fi->q.ocq_end = 0;
	fi->q.ocq_prod_indx = 0;

	fi->q.ptr_imq_base = NULL;
	fi->q.imq_len = 0;
	fi->q.imq_end = 0;
	fi->q.imq_cons_indx = 0;
	fi->q.imq_prod_indx = 0;

	fi->q.ptr_mfsbq_base = NULL;
	fi->q.mfsbq_len = 0;
	fi->q.mfsbq_end = 0;
	fi->q.mfsbq_prod_indx = 0;
	fi->q.mfsbq_cons_indx = 0;
	fi->q.mfsbuff_len = 0;
	fi->q.mfsbuff_end = 0;
	fi->g.mfs_buffer_count = 0;

	fi->q.ptr_sfsbq_base = NULL;
	fi->q.sfsbq_len = 0;
	fi->q.sfsbq_end = 0;
	fi->q.sfsbq_prod_indx = 0;
	fi->q.sfsbq_cons_indx = 0;
	fi->q.sfsbuff_len = 0;
	fi->q.sfsbuff_end = 0;

	fi->q.sdb_indx = 0;
	fi->q.fcp_cmnd_indx = 0;

	fi->q.ptr_edb_base = NULL;
	fi->q.edb_buffer_indx = 0;
	fi->q.ptr_tachyon_header_base = NULL;
	fi->q.tachyon_header_indx = 0;
	fi->node_info_list = NULL;
	fi->ox_id_list = NULL;
	fi->g.loop_up = FALSE;
	fi->g.ptp_up = FALSE;
	fi->g.link_up = FALSE;
	fi->g.fabric_present = FALSE;
	fi->g.n_port_try = FALSE;
	fi->g.dont_init = FALSE;
	fi->g.nport_timer_set = FALSE;
	fi->g.lport_timer_set = FALSE;
	fi->g.no_of_targets = 0;
	fi->g.sem = 0;
	fi->g.perform_adisc = FALSE;
	fi->g.e_i = 0;

	/* build OCQ */
	if ( (fi->q.ptr_ocq_base = (u_int *)__get_free_pages(GFP_KERNEL, 0)) == 0) {
		T_MSG("failed to get OCQ page");
		return 0;
	}
	/* set up the OCQ structures */
	for (i = 0; i < OCQ_LENGTH; i++)
		fi->q.ptr_odb[i] = fi->q.ptr_ocq_base + NO_OF_ENTRIES*i;

	/* build IMQ */
	if ( (fi->q.ptr_imq_base = (u_int *)__get_free_pages(GFP_KERNEL, 0)) == 0) {
		T_MSG("failed to get IMQ page");
		return 0;
	}
	for (i = 0; i < IMQ_LENGTH; i++)
		fi->q.ptr_imqe[i] = fi->q.ptr_imq_base + NO_OF_ENTRIES*i;

	/* build MFSBQ */
	if ( (fi->q.ptr_mfsbq_base = (u_int *)__get_free_pages(GFP_KERNEL, 0)) == 0) {
		T_MSG("failed to get MFSBQ page");
		return 0;
	}
	memset((char *)fi->q.ptr_mfsbq_base, 0, MFSBQ_LENGTH * 32);
	/* Allocate one huge chunk of memory... helps while reassembling
	 * frames.
	 */
	if ( (addr = (u_char *)__get_free_pages(GFP_KERNEL, 5) ) == 0) {
		T_MSG("failed to get MFSBQ page");
		return 0;
	}
	/* fill in addresses of empty buffers */
	for (i = 0; i < MFSBQ_LENGTH; i++) {
		for (j = 0; j < NO_OF_ENTRIES; j++) {
				*(fi->q.ptr_mfsbq_base + i*NO_OF_ENTRIES + j) = htonl(virt_to_bus(addr));
				addr += MFS_BUFFER_SIZE;
		}
	}

	/* The number of entries in each MFS buffer is 8. There are 8
	 * MFS buffers. That leaves us with 4096-256 bytes. We use them
	 * as temporary space for ELS frames. This is done to make sure that
	 * the addresses are aligned.
	 */
	fi->g.els_buffer[0] = fi->q.ptr_mfsbq_base + MFSBQ_LENGTH*NO_OF_ENTRIES;
	for (i = 1; i < MAX_PENDING_FRAMES; i++)
		fi->g.els_buffer[i] = fi->g.els_buffer[i-1] + 64;
	
	/* build SFSBQ */
	if ( (fi->q.ptr_sfsbq_base = (u_int *)__get_free_pages(GFP_KERNEL, 0)) == 0) {
		T_MSG("failed to get SFSBQ page");
		return 0;
	}
	memset((char *)fi->q.ptr_sfsbq_base, 0, SFSBQ_LENGTH * 32);
	/* fill in addresses of empty buffers */
	for (i = 0; i < SFSBQ_LENGTH; i++)
		for (j = 0; j < NO_OF_ENTRIES; j++){
			addr = kmalloc(SFS_BUFFER_SIZE*2, GFP_KERNEL);
			if (addr == NULL){ 
				T_MSG("ptr_sfs_buffer : memory not allocated");
				return 0;
			}
			else {
			int offset = ALIGNED_SFS_ADDR(addr);
				memset((char *)addr, 0, SFS_BUFFER_SIZE);
				fi->q.ptr_sfs_buffers[i*NO_OF_ENTRIES +j] = (u_int *)addr;
				addr += offset;
				*(fi->q.ptr_sfsbq_base + i*NO_OF_ENTRIES + j) = htonl(virt_to_bus(addr));
			}
		}

	/* The number of entries in each SFS buffer is 8. There are 8
	 * MFS buffers. That leaves us with 4096-256 bytes. We use them
	 * as temporary space for ARP frames. This is done inorder to 
	 * support HW_Types of 0x1 and 0x6. 
	 */
	fi->g.arp_buffer = (char *)fi->q.ptr_sfsbq_base + SFSBQ_LENGTH*NO_OF_ENTRIES*4;
	
	/* build EDB */
	if ((fi->q.ptr_edb_base = (u_int *)__get_free_pages(GFP_KERNEL, 5) ) == 0) {
		T_MSG("failed to get EDB page");
		return 0;
	}
	for (i = 0; i < EDB_LEN; i++)
		fi->q.ptr_edb[i] = fi->q.ptr_edb_base + 2*i;

	/* build SEST */

	/* OX_IDs range from 0x0 - 0x4FFF.
	 */
	if ((fi->q.ptr_sest_base = (u_int *)__get_free_pages(GFP_KERNEL, 5)) == 0) {
		T_MSG("failed to get SEST page");
		return 0;
	}
	for (i = 0; i < SEST_LENGTH; i++)
		fi->q.ptr_sest[i] = fi->q.ptr_sest_base + NO_OF_ENTRIES*i;
	
	if ((fi->q.ptr_sdb_base = (u_int *)__get_free_pages(GFP_KERNEL, 5)) == 0) {
		T_MSG("failed to get SDB page");
		return 0;
	}
	for (i = 0 ; i < NO_OF_SDB_ENTRIES; i++)
		fi->q.ptr_sdb_slot[i] = fi->q.ptr_sdb_base + (SDB_SIZE/4)*i;

	if ((fi->q.ptr_fcp_cmnd_base = (u_int *)__get_free_pages(GFP_KERNEL, 0)) == 0) {
		T_MSG("failed to get FCP_CMND page");
		return 0;
	}
	for (i = 0; i < NO_OF_FCP_CMNDS; i++)
		fi->q.ptr_fcp_cmnd[i] = fi->q.ptr_fcp_cmnd_base + NO_OF_ENTRIES*i;

	/* Allocate space for Tachyon Header as well... 
	 */
	if ((fi->q.ptr_tachyon_header_base = (u_int *)__get_free_pages(GFP_KERNEL, 0) ) == 0) {
		T_MSG("failed to get tachyon_header page");
		return 0;
	}
	for (i = 0; i < NO_OF_TACH_HEADERS; i++) 
		fi->q.ptr_tachyon_header[i] = fi->q.ptr_tachyon_header_base + 16*i;
	
	/* Allocate memory for indices.
	 * Indices should be aligned on 32 byte boundries. 
	 */
	fi->q.host_ocq_cons_indx = kmalloc(2*32, GFP_KERNEL);
	if (fi->q.host_ocq_cons_indx == NULL){ 
		T_MSG("fi->q.host_ocq_cons_indx : memory not allocated");
		return 0;
	}
	fi->q.ptr_host_ocq_cons_indx = fi->q.host_ocq_cons_indx; 
	if ((u_long)(fi->q.host_ocq_cons_indx) % 32)
		fi->q.host_ocq_cons_indx++;
	
	fi->q.host_hpcq_cons_indx = kmalloc(2*32, GFP_KERNEL);
	if (fi->q.host_hpcq_cons_indx == NULL){ 
		T_MSG("fi->q.host_hpcq_cons_indx : memory not allocated");
		return 0;
	}
	fi->q.ptr_host_hpcq_cons_indx= fi->q.host_hpcq_cons_indx;
	if ((u_long)(fi->q.host_hpcq_cons_indx) % 32)
		fi->q.host_hpcq_cons_indx++;

	fi->q.host_imq_prod_indx = kmalloc(2*32, GFP_KERNEL);
	if (fi->q.host_imq_prod_indx == NULL){ 
		T_MSG("fi->q.host_imq_prod_indx : memory not allocated");
		return 0;
	}
	fi->q.ptr_host_imq_prod_indx = fi->q.host_imq_prod_indx;
	if ((u_long)(fi->q.host_imq_prod_indx) % 32)
		fi->q.host_imq_prod_indx++;

	LEAVE("build_queues");
	return 1;
}


static void write_to_tachyon_registers(struct fc_info *fi)
{
u_int bus_addr, bus_indx_addr, i;

	ENTER("write_to_tachyon_registers");

	/* Clear Queues each time Tachyon is reset */
	memset((char *)fi->q.ptr_ocq_base, 0, OCQ_LENGTH * 32);
	memset((char *)fi->q.ptr_imq_base, 0, IMQ_LENGTH * 32);
	memset((char *)fi->q.ptr_edb_base, 0, EDB_LEN * 8);
	memset((char *)fi->q.ptr_sest_base, 0, SEST_LENGTH * 32);
	memset((char *)fi->q.ptr_sdb_base, 0, NO_OF_SDB_ENTRIES * SDB_SIZE);
	memset((char *)fi->q.ptr_tachyon_header_base, 0xFF, NO_OF_TACH_HEADERS * TACH_HEADER_SIZE);
	for (i = 0; i < SEST_LENGTH; i++)
		fi->q.free_scsi_oxid[i] = OXID_AVAILABLE;
	for (i = 0; i < NO_OF_SDB_ENTRIES; i++)
		fi->q.sdb_slot_status[i] = SDB_FREE;

	take_tachyon_offline(fi);
	writel(readl(fi->t_r.ptr_tach_config_reg) | SCSI_ENABLE | WRITE_STREAM_SIZE | READ_STREAM_SIZE | PARITY_EVEN | OOO_REASSEMBLY_DISABLE, fi->t_r.ptr_tach_config_reg);

	/* Write OCQ registers */
	fi->q.ocq_prod_indx = 0;
	*(fi->q.host_ocq_cons_indx) = 0;
	
	/* The Tachyon needs to be passed the "real" address */
	bus_addr = virt_to_bus(fi->q.ptr_ocq_base);
	writel(bus_addr, fi->t_r.ptr_ocq_base_reg);
	writel(OCQ_LENGTH - 1, fi->t_r. ptr_ocq_len_reg);
	bus_indx_addr = virt_to_bus(fi->q.host_ocq_cons_indx);
	writel(bus_indx_addr, fi->t_r.ptr_ocq_cons_indx_reg);

	/* Write IMQ registers */
	fi->q.imq_cons_indx = 0;
	*(fi->q.host_imq_prod_indx) = 0;
	bus_addr = virt_to_bus(fi->q.ptr_imq_base);
	writel(bus_addr, fi->t_r.ptr_imq_base_reg);
	writel(IMQ_LENGTH - 1, fi->t_r.ptr_imq_len_reg);
	bus_indx_addr = virt_to_bus(fi->q.host_imq_prod_indx);
	writel(bus_indx_addr, fi->t_r.ptr_imq_prod_indx_reg);
	
	/* Write MFSBQ registers */
	fi->q.mfsbq_prod_indx = MFSBQ_LENGTH - 1;
	fi->q.mfsbuff_end = MFS_BUFFER_SIZE - 1;
	fi->q.mfsbq_cons_indx = 0;
	bus_addr = virt_to_bus(fi->q.ptr_mfsbq_base);
	writel(bus_addr, fi->t_r.ptr_mfsbq_base_reg);
	writel(MFSBQ_LENGTH - 1, fi->t_r.ptr_mfsbq_len_reg);
	writel(fi->q.mfsbuff_end, fi->t_r.ptr_mfsbuff_len_reg);
	/* Do this last as tachyon will prefetch the 
	 * first entry as soon as we write to it.
	 */
	writel(fi->q.mfsbq_prod_indx, fi->t_r.ptr_mfsbq_prod_reg);

	/* Write SFSBQ registers */
	fi->q.sfsbq_prod_indx = SFSBQ_LENGTH - 1;
	fi->q.sfsbuff_end = SFS_BUFFER_SIZE - 1;
	fi->q.sfsbq_cons_indx = 0;
	bus_addr = virt_to_bus(fi->q.ptr_sfsbq_base);
	writel(bus_addr, fi->t_r.ptr_sfsbq_base_reg);
	writel(SFSBQ_LENGTH - 1, fi->t_r.ptr_sfsbq_len_reg);
	writel(fi->q.sfsbuff_end, fi->t_r.ptr_sfsbuff_len_reg);
	/* Do this last as tachyon will prefetch the first 
	 * entry as soon as we write to it. 
	 */
	writel(fi->q.sfsbq_prod_indx, fi->t_r.ptr_sfsbq_prod_reg);

	/* Write SEST registers */
	bus_addr = virt_to_bus(fi->q.ptr_sest_base);
	writel(bus_addr, fi->t_r.ptr_sest_base_reg);
	writel(SEST_LENGTH - 1, fi->t_r.ptr_sest_len_reg);
	/* the last 2 bits _should_ be 1 */
	writel(SEST_BUFFER_SIZE - 1, fi->t_r.ptr_scsibuff_len_reg);

	/* write AL_TIME & E_D_TOV into the registers */
	writel(TOV_VALUES, fi->t_r.ptr_fm_tov_reg);
	/* Tell Tachyon to pick a Soft Assigned AL_PA */
	writel(LOOP_INIT_SOFT_ADDRESS, fi->t_r.ptr_fm_config_reg);

	/* Read the WWN from EEPROM . But, for now we assign it here. */
	writel(WORLD_WIDE_NAME_LOW, fi->t_r.ptr_fm_wwn_low_reg);
	writel(WORLD_WIDE_NAME_HIGH, fi->t_r.ptr_fm_wwn_hi_reg);

	DPRINTK1("TACHYON initializing as L_Port...\n");
	writel(INITIALIZE, fi->t_r.ptr_fm_control_reg);
			
	LEAVE("write_to_tachyon_registers");
}


static void tachyon_interrupt(int irq, void* dev_id, struct pt_regs* regs)
{
struct Scsi_Host *host = dev_id;
struct iph5526_hostdata *hostdata = (struct iph5526_hostdata *)host->hostdata;
struct fc_info *fi = hostdata->fi; 
u_long flags;
	spin_lock_irqsave(&fi->fc_lock, flags);
	tachyon_interrupt_handler(irq, dev_id, regs);
	spin_unlock_irqrestore(&fi->fc_lock, flags);
}

static void tachyon_interrupt_handler(int irq, void* dev_id, struct pt_regs* regs)
{
struct Scsi_Host *host = dev_id;
struct iph5526_hostdata *hostdata = (struct iph5526_hostdata *)host->hostdata;
struct fc_info *fi = hostdata->fi; 
u_int *ptr_imq_entry;
u_int imq_int_type, current_IMQ_index = 0, prev_IMQ_index;
int index, no_of_entries = 0;

	DPRINTK("\n");
	ENTER("tachyon_interrupt");
	if (fi->q.host_imq_prod_indx != NULL) {
		current_IMQ_index =  ntohl(*(fi->q.host_imq_prod_indx));
	}
	else {
		/* _Should not_ happen */
		T_MSG("IMQ_indx NULL. DISABLING INTERRUPTS!!!\n");
		writel(0x0, fi->i_r.ptr_ichip_hw_control_reg);
	}

	if (current_IMQ_index > fi->q.imq_cons_indx)
		no_of_entries = current_IMQ_index - fi->q.imq_cons_indx;
	else
	if (current_IMQ_index < fi->q.imq_cons_indx)
		no_of_entries = IMQ_LENGTH - (fi->q.imq_cons_indx - current_IMQ_index);

	if (no_of_entries == 0) {
	u_int ichip_status;
		ichip_status = readl(fi->i_r.ptr_ichip_hw_status_reg);
		if (ichip_status & 0x20) {
			/* Should _never_ happen. Might require a hard reset */
			T_MSG("Too bad... PCI Bus Error. Resetting (i)chip"); 
			reset_ichip(fi);
			T_MSG("DISABLING INTERRUPTS!!!\n");
			writel(0x0, fi->i_r.ptr_ichip_hw_control_reg);
		}
	}

	prev_IMQ_index = current_IMQ_index;
	for (index = 0; index < no_of_entries; index++) {
		ptr_imq_entry = fi->q.ptr_imqe[fi->q.imq_cons_indx];
		imq_int_type = ntohl(*ptr_imq_entry);

		completion_message_handler(fi, imq_int_type);
		if ((fi->g.link_up == FALSE) && ((imq_int_type == MFS_BUF_WARN) || (imq_int_type == SFS_BUF_WARN) || (imq_int_type == IMQ_BUF_WARN))) 
			break;
		update_IMQ_indx(fi, 1);
	
		/* Check for more entries */
		current_IMQ_index =  ntohl(*(fi->q.host_imq_prod_indx));
		if (current_IMQ_index != prev_IMQ_index) {
			no_of_entries++;
			prev_IMQ_index = current_IMQ_index;
		}
	} /*end of for loop*/		
	return;
	LEAVE("tachyon_interrupt");
}


static void handle_SFS_BUF_WARN_interrupt(struct fc_info *fi)
{
int i;
	ENTER("handle_SFS_BUF_WARN_interrupt");
	if (fi->g.link_up == FALSE) {
		reset_tachyon(fi, SOFTWARE_RESET);
		return;
	}
	/* Free up all but one entry in the Q. 
	 */
	for (i = 0; i < ((SFSBQ_LENGTH - 1) * NO_OF_ENTRIES); i++) {
		handle_SFS_interrupt(fi);
		update_IMQ_indx(fi, 1);
	}
	LEAVE("handle_SFS_BUF_WARN_interrupt");
}

/* Untested_Code_Begin */ 
static void handle_MFS_BUF_WARN_interrupt(struct fc_info *fi)
{
int i;
	ENTER("handle_MFS_BUF_WARN_interrupt");
	if (fi->g.link_up == FALSE) {
		reset_tachyon(fi, SOFTWARE_RESET);
		return;
	}
	/* FIXME: freeing up 8 entries. 
	 */
	for (i = 0; i < NO_OF_ENTRIES; i++) {
		handle_MFS_interrupt(fi);
		update_IMQ_indx(fi, 1);
	}
	LEAVE("handle_MFS_BUF_WARN_interrupt");
}
/*Untested_Code_End */

static void handle_IMQ_BUF_WARN_interrupt(struct fc_info *fi)
{
u_int *ptr_imq_entry;
u_int imq_int_type, current_IMQ_index = 0, temp_imq_cons_indx;
int index, no_of_entries = 0;

	ENTER("handle_IMQ_BUF_WARN_interrupt");
	if (fi->g.link_up == FALSE) {
		reset_tachyon(fi, SOFTWARE_RESET);
		return;
	}
	current_IMQ_index =  ntohl(*(fi->q.host_imq_prod_indx));

	if (current_IMQ_index > fi->q.imq_cons_indx)
 		no_of_entries = current_IMQ_index - fi->q.imq_cons_indx;
	else
		if (current_IMQ_index < fi->q.imq_cons_indx)
			no_of_entries = IMQ_LENGTH - (fi->q.imq_cons_indx - current_IMQ_index);
	/* We dont want to look at the same IMQ entry again. 
	 */
	temp_imq_cons_indx = fi->q.imq_cons_indx + 1;
	if (no_of_entries != 0)
		no_of_entries -= 1;
	for (index = 0; index < no_of_entries; index++) {
		ptr_imq_entry = fi->q.ptr_imqe[temp_imq_cons_indx];
		imq_int_type = ntohl(*ptr_imq_entry);
		if (imq_int_type != IMQ_BUF_WARN)
			completion_message_handler(fi, imq_int_type);
		temp_imq_cons_indx++;
		if (temp_imq_cons_indx == IMQ_LENGTH)
			temp_imq_cons_indx = 0;
	} /*end of for loop*/	
	if (no_of_entries != 0)
		update_IMQ_indx(fi, no_of_entries);
	LEAVE("handle_IMQ_BUF_WARN_interrupt");
}

static void completion_message_handler(struct fc_info *fi, u_int imq_int_type)
{
	switch(imq_int_type) {
		case OUTBOUND_COMPLETION:
			DPRINTK("OUTBOUND_COMPLETION message received");
			break;
		case OUTBOUND_COMPLETION_I:
			DPRINTK("OUTBOUND_COMPLETION_I message received");
			handle_OCI_interrupt(fi);
			break;
		case OUT_HI_PRI_COMPLETION:
			DPRINTK("OUT_HI_PRI_COMPLETION message received");
			break;
		case OUT_HI_PRI_COMPLETION_I:
			DPRINTK("OUT_HI_PRI_COMPLETION_I message received");
			break;
		case INBOUND_MFS_COMPLETION:
			DPRINTK("INBOUND_MFS_COMPLETION message received");
			handle_MFS_interrupt(fi);
			break;
		case INBOUND_OOO_COMPLETION:
			DPRINTK("INBOUND_OOO_COMPLETION message received");
			handle_OOO_interrupt(fi);
			break;
		case INBOUND_SFS_COMPLETION:
			DPRINTK("INBOUND_SFS_COMPLETION message received");
			handle_SFS_interrupt(fi);
			break;
		case INBOUND_UNKNOWN_FRAME_I:
			DPRINTK("INBOUND_UNKNOWN_FRAME message received");
			handle_Unknown_Frame_interrupt(fi);
			break;
		case INBOUND_BUSIED_FRAME:
			DPRINTK("INBOUND_BUSIED_FRAME message received");
			handle_Busied_Frame_interrupt(fi);
			break;
		case FRAME_MGR_INTERRUPT:
			DPRINTK("FRAME_MGR_INTERRUPT message received");
			handle_FM_interrupt(fi);
			break;
		case READ_STATUS:
			DPRINTK("READ_STATUS message received");
			break;
		case SFS_BUF_WARN:
			DPRINTK("SFS_BUF_WARN message received");
			handle_SFS_BUF_WARN_interrupt(fi);
			break;
		case MFS_BUF_WARN:
			DPRINTK("MFS_BUF_WARN message received");
			handle_MFS_BUF_WARN_interrupt(fi);
			break;
		case IMQ_BUF_WARN:
			DPRINTK("IMQ_BUF_WARN message received");
			handle_IMQ_BUF_WARN_interrupt(fi);
			break;
		case INBOUND_C1_TIMEOUT:
			DPRINTK("INBOUND_C1_TIMEOUT message received");
			break;
		case BAD_SCSI_FRAME:
			DPRINTK("BAD_SCSI_FRAME message received");
			handle_Bad_SCSI_Frame_interrupt(fi);
			break;
		case INB_SCSI_STATUS_COMPLETION:
			DPRINTK("INB_SCSI_STATUS_COMPL message received");
			handle_Inbound_SCSI_Status_interrupt(fi);
			break;
		case INBOUND_SCSI_COMMAND:
			DPRINTK("INBOUND_SCSI_COMMAND message received");
			handle_Inbound_SCSI_Command_interrupt(fi);
			break;
		case INBOUND_SCSI_DATA_COMPLETION:
			DPRINTK("INBOUND_SCSI_DATA message received");
			/* Only for targets */
			break;
		default:		
			T_MSG("DEFAULT message received, type = %x", imq_int_type);
			return;
	}
	reset_latch(fi);
}

static void handle_OCI_interrupt(struct fc_info *fi)
{
u_int *ptr_imq_entry;
u_long transaction_id = 0;
unsigned short status, seq_count, transmitted_ox_id;
struct Scsi_Host *host = fi->host;
struct iph5526_hostdata *hostdata = (struct iph5526_hostdata *)host->hostdata;
Scsi_Cmnd *Cmnd;
u_int tag;

	ENTER("handle_OCI_interrupt");
	ptr_imq_entry = fi->q.ptr_imqe[fi->q.imq_cons_indx];
	transaction_id = ntohl(*(ptr_imq_entry + 1));
	status = ntohl(*(ptr_imq_entry + 2)) >> 16;
	seq_count = ntohl(*(ptr_imq_entry + 3));
	DPRINTK("transaction_id= %x", (u_int)transaction_id);
	tag = transaction_id & 0xFFFF0000;
	transmitted_ox_id = transaction_id;

	/* The INT could be either due to TIME_OUT | BAD_ALPA. 
	 * But we check only for TimeOuts. Bad AL_PA will 
	 * caught by FM_interrupt handler. 
	 */

	if ((status == OCM_TIMEOUT_OR_BAD_ALPA) && (!fi->g.port_discovery) && (!fi->g.perform_adisc)){
		DPRINTK("Frame TimeOut on OX_ID = %x", (u_int)transaction_id);

		/* Is it a SCSI frame that is timing out ? Not a very good check... 
		 */
		if ((transmitted_ox_id <= MAX_SCSI_OXID) && ((tag == FC_SCSI_BAD_TARGET) || (tag < 0x00FF0000))) {
			/* If it is a Bad AL_PA, we report it as BAD_TARGET.
			 * Else, we allow the command to time-out. A Link
			 * re-initialization could be taking place.
			 */
			if (tag == FC_SCSI_BAD_TARGET) {
				Cmnd = hostdata->cmnd_handler[transmitted_ox_id & MAX_SCSI_XID];
				hostdata->cmnd_handler[transmitted_ox_id & MAX_SCSI_XID] = NULL;
				if (Cmnd != NULL) {
					Cmnd->result = DID_BAD_TARGET << 16;
					(*Cmnd->scsi_done) (Cmnd);
				}
				else
					T_MSG("NULL Command out of handler!");
			} /* if Bad Target */
			else {
			u_char missing_target = tag >> 16;
			struct fc_node_info *q = fi->node_info_list;
				/* A Node that we thought was logged in has gone
				 * away. We are the optimistic kind and we keep
				 * hoping that our dear little Target will come back
				 * to us. For now we log him out.
				 */
				DPRINTK2("Missing Target = %d", missing_target);
				while (q != NULL) {
					if (q->target_id == missing_target) {
						T_MSG("Target %d Logged out", q->target_id);
						q->login = LOGIN_ATTEMPTED;
						if (fi->num_nodes > 0)
							fi->num_nodes--;
						tx_logi(fi, ELS_PLOGI, q->d_id);
						break;
					}
					else
						q = q->next;
				}
			}
		} /* End of SCSI frame timing out. */
		else {
			if (seq_count > 1) {
				/* An IP frame was transmitted to a Bad AL_PA. Free up
			 	 * the skb used.
			 	 */
				dev_kfree_skb_irq((struct sk_buff *)(bus_to_virt(transaction_id)));
				netif_wake_queue(fi->dev);
			}
		} /* End of IP frame timing out. */
	} /* End of frame timing out. */
	else {
		/* Frame was transmitted successfully. Check if it was an ELS
		 * frame or an IP frame or a Bad_Target_Notification frame (in
		 * case of a ptp_link). Ugly!
		 */
		if ((status == 0) && (seq_count == 0)) {
		u_int tag = transaction_id & 0xFFFF0000;
		/* Continue with port discovery after an ELS is successfully 
		 * transmitted. (status == 0). 
		 */
			DPRINTK("tag = %x", tag);
			switch(tag) {
				case ELS_FLOGI:
					/* Letz use the Name Server instead */
					fi->g.explore_fabric = TRUE;
					fi->g.port_discovery = FALSE;
					fi->g.alpa_list_index = MAX_NODES;
					add_to_ox_id_list(fi, transaction_id, tag);
					break;
				case ELS_PLOGI:
					if (fi->g.fabric_present && (fi->g.name_server == FALSE))
						add_to_ox_id_list(fi,transaction_id,ELS_NS_PLOGI);
					else
						add_to_ox_id_list(fi, transaction_id, tag);
					break;
				case FC_SCSI_BAD_TARGET:
					Cmnd = hostdata->cmnd_handler[transmitted_ox_id & MAX_SCSI_XID];
					hostdata->cmnd_handler[transmitted_ox_id & MAX_SCSI_XID] = NULL;
					if (Cmnd != NULL) {
						Cmnd->result = DID_BAD_TARGET << 16;
						(*Cmnd->scsi_done) (Cmnd);
					}
					else
						T_MSG("NULL Command out of handler!");
					break;
				default:
					add_to_ox_id_list(fi, transaction_id, tag);
			}
		
			if (fi->g.alpa_list_index >= MAX_NODES) {
				if (fi->g.port_discovery == TRUE) {
					fi->g.port_discovery = FALSE;
					add_display_cache_timer(fi);
				}
				fi->g.alpa_list_index = MAX_NODES;
			}
			if (fi->g.port_discovery == TRUE) 
				local_port_discovery(fi);
		}
		else {
			/* An IP frame has been successfully transmitted.
			 * Free the skb that was used for this IP frame.
			 */
			if ((status == 0) && (seq_count > 1)) {
				dev_kfree_skb_irq((struct sk_buff *)(bus_to_virt(transaction_id)));
				netif_wake_queue(fi->dev);
			}
		}
	}
	LEAVE("handle_OCI_interrupt");
}

/* Right now we discard OOO frames */
static void handle_OOO_interrupt(struct fc_info *fi)
{
u_int *ptr_imq_entry;
int queue_indx, offset, payload_size;
int no_of_buffers = 1; /* header is in a separate buffer */
	ptr_imq_entry = fi->q.ptr_imqe[fi->q.imq_cons_indx];
	offset = ntohl(*(ptr_imq_entry + 1)) & 0x00000007;
	queue_indx = ntohl(*(ptr_imq_entry + 1)) & 0xFFFF0000;
	queue_indx = queue_indx >> 16;
	payload_size = ntohl(*(ptr_imq_entry + 2)) - TACHYON_HEADER_LEN;
	/* Calculate total number of buffers */
	no_of_buffers += payload_size / MFS_BUFFER_SIZE;
	if (payload_size % MFS_BUFFER_SIZE)
		no_of_buffers++;

	/* provide Tachyon will another set of buffers */
	fi->g.mfs_buffer_count += no_of_buffers;
	if (fi->g.mfs_buffer_count >= NO_OF_ENTRIES) {
	int count = fi->g.mfs_buffer_count / NO_OF_ENTRIES;
		fi->g.mfs_buffer_count -= NO_OF_ENTRIES * count;
		update_MFSBQ_indx(fi, count);
	}
}

static void handle_MFS_interrupt(struct fc_info *fi)
{
u_int *ptr_imq_entry, *buff_addr;
u_int type_of_frame, s_id;
int queue_indx, offset, payload_size, starting_indx, starting_offset;
u_short received_ox_id;
int no_of_buffers = 1; /* header is in a separate buffer */
struct sk_buff *skb;
int wrap_around = FALSE, no_of_wrap_buffs = NO_OF_ENTRIES - 1;
	ENTER("handle_MFS_interrupt");
	ptr_imq_entry = fi->q.ptr_imqe[fi->q.imq_cons_indx];
	offset = ntohl(*(ptr_imq_entry + 1)) & 0x00000007;
	queue_indx = ntohl(*(ptr_imq_entry + 1)) & 0xFFFF0000;
	queue_indx = queue_indx >> 16;
	DPRINTK("queue_indx = %d, offset  = %d\n", queue_indx, offset);
	payload_size = ntohl(*(ptr_imq_entry + 2)) - TACHYON_HEADER_LEN;
	DPRINTK("payload_size = %d", payload_size);
	/* Calculate total number of buffers */
	no_of_buffers += payload_size / MFS_BUFFER_SIZE;
	if (payload_size % MFS_BUFFER_SIZE)
		no_of_buffers++;
	DPRINTK("no_of_buffers = %d", no_of_buffers);

	if ((no_of_buffers - 1) <= offset) {
		starting_offset = offset - (no_of_buffers - 1);
		starting_indx = queue_indx;
	}
	else {
	int temp = no_of_buffers - (offset + 1);
	int no_of_queues = temp / NO_OF_ENTRIES;
		starting_offset = temp % NO_OF_ENTRIES;
		if (starting_offset != 0) {
			no_of_wrap_buffs = starting_offset - 1; //exclude header
			starting_offset = NO_OF_ENTRIES - starting_offset;
			no_of_queues++;
		}
		starting_indx = queue_indx - no_of_queues;
		if (starting_indx < 0) {
			no_of_wrap_buffs -= (starting_indx + 1) * NO_OF_ENTRIES; 
			starting_indx = MFSBQ_LENGTH + starting_indx;
			wrap_around = TRUE;
		}
	}
	
	DPRINTK("starting_indx = %d, starting offset = %d no_of_wrap_buffs = %d\n", starting_indx, starting_offset, no_of_wrap_buffs);
	/* Get Tachyon Header from first buffer */
	buff_addr = bus_to_virt(ntohl(*(fi->q.ptr_mfsbq_base + starting_indx*NO_OF_ENTRIES + starting_offset)));
	

	/* extract Type of Frame */
	type_of_frame = (u_int)ntohl(*(buff_addr + 4)) & 0xFF000000;
	s_id = (u_int)ntohl(*(buff_addr + 3)) & 0x00FFFFFF;
	received_ox_id = ntohl(*(buff_addr + 6)) >> 16;
	buff_addr += MFS_BUFFER_SIZE/4;
	DPRINTK("type_of_frame = %x, s_id = %x, ox_id = %x", type_of_frame, s_id, received_ox_id);

 	switch(type_of_frame) {
	  case TYPE_LLC_SNAP:
		skb = dev_alloc_skb(payload_size);
		if (skb == NULL) {
			printk(KERN_NOTICE "%s: In handle_MFS_interrupt() Memory squeeze, dropping packet.\n", fi->name);
			fi->fc_stats.rx_dropped++;
			fi->g.mfs_buffer_count += no_of_buffers;
			if (fi->g.mfs_buffer_count >= NO_OF_ENTRIES) {
				int count = fi->g.mfs_buffer_count / NO_OF_ENTRIES;
				fi->g.mfs_buffer_count -= NO_OF_ENTRIES * count;
				update_MFSBQ_indx(fi, count);
			}
			return;
		}
		if (wrap_around) {
		int wrap_size = no_of_wrap_buffs * MFS_BUFFER_SIZE;
		int tail_size = payload_size - wrap_size;
			DPRINTK("wrap_size = %d, tail_size = %d\n", wrap_size, tail_size);
			if (no_of_wrap_buffs) 
				memcpy(skb_put(skb, wrap_size), buff_addr, wrap_size);
			buff_addr = bus_to_virt(ntohl(*(fi->q.ptr_mfsbq_base)));
			memcpy(skb_put(skb, tail_size), buff_addr, tail_size);
		}
		else
			memcpy(skb_put(skb, payload_size), buff_addr, payload_size);
		rx_net_mfs_packet(fi, skb);
	  	break;
	default:
		T_MSG("Unknown Frame Type received. Type = %x", type_of_frame);
	}

	/* provide Tachyon will another set of buffers */
	fi->g.mfs_buffer_count += no_of_buffers;
	if (fi->g.mfs_buffer_count >= NO_OF_ENTRIES) {
	int count = fi->g.mfs_buffer_count / NO_OF_ENTRIES;
		fi->g.mfs_buffer_count -= NO_OF_ENTRIES * count;
		update_MFSBQ_indx(fi, count);
	}
	LEAVE("handle_MFS_interrupt");
}

static void handle_Unknown_Frame_interrupt(struct fc_info *fi)
{
u_int *ptr_imq_entry;
int queue_indx, offset;
	ENTER("handle_Unknown_Frame_interrupt");
	ptr_imq_entry = fi->q.ptr_imqe[fi->q.imq_cons_indx];
	offset = ntohl(*(ptr_imq_entry + 1)) & 0x00000007;
	queue_indx = ntohl(*(ptr_imq_entry + 1)) & 0xFFFF0000;
	queue_indx = queue_indx >> 16;
	/* We discard the "unknown" frame */
	/* provide Tachyon will another set of buffers */
	if (offset == (NO_OF_ENTRIES - 1))
		update_SFSBQ_indx(fi);
	LEAVE("handle_Unknown_Frame_interrupt");
}

static void handle_Busied_Frame_interrupt(struct fc_info *fi)
{
u_int *ptr_imq_entry;
int queue_indx, offset;
	ENTER("handle_Busied_Frame_interrupt");
	ptr_imq_entry = fi->q.ptr_imqe[fi->q.imq_cons_indx];
	offset = ntohl(*(ptr_imq_entry + 1)) & 0x00000007;
	queue_indx = ntohl(*(ptr_imq_entry + 1)) & 0xFFFF0000;
	queue_indx = queue_indx >> 16;
	/* We discard the "busied" frame */
	/* provide Tachyon will another set of buffers */
	if (offset == (NO_OF_ENTRIES - 1))
		update_SFSBQ_indx(fi);
	LEAVE("handle_Busied_Frame_interrupt");
}

static void handle_Bad_SCSI_Frame_interrupt(struct fc_info *fi)
{
u_int *ptr_imq_entry, *buff_addr, *tach_header, *ptr_edb;
u_int s_id, rctl, frame_class, burst_len, transfered_len, len = 0;
int queue_indx, offset, payload_size, i;
u_short ox_id, rx_id, x_id, mtu = 512;
u_char target_id = 0xFF;

	ENTER("handle_Bad_SCSI_Frame_interrupt");
	ptr_imq_entry = fi->q.ptr_imqe[fi->q.imq_cons_indx];
	offset = ntohl(*(ptr_imq_entry + 1)) & 0x00000007;
	queue_indx = ntohl(*(ptr_imq_entry + 1)) & 0xFFFF0000;
	queue_indx = queue_indx >> 16;
	payload_size = ntohl(*(ptr_imq_entry + 2));

	buff_addr = bus_to_virt(ntohl(*(fi->q.ptr_sfsbq_base + queue_indx*NO_OF_ENTRIES + offset)));

	rctl = ntohl(*(buff_addr + 2)) & 0xFF000000;
	s_id = ntohl(*(buff_addr + 3)) & 0x00FFFFFF;
	ox_id = ntohl(*(buff_addr + 6)) >> 16;
	rx_id = ntohl(*(buff_addr + 6));
	x_id = ox_id & MAX_SCSI_XID;

	/* Any frame that comes in with OX_ID that matches an OX_ID 
	 * that has been allocated for SCSI, will be called a Bad
	 * SCSI frame if the Exchange is not valid any more.
	 *
	 * We will also get a Bad SCSI frame interrupt if we receive
	 * a XFER_RDY with offset != 0. Tachyon washes its hands off
	 * this Exchange. We have to take care of ourselves. Grrr...
	 */
	if (rctl == DATA_DESCRIPTOR) {
	struct fc_node_info *q = fi->node_info_list;
		while (q != NULL) {
			if (q->d_id == s_id) {
				target_id = q->target_id;
				mtu = q->mtu;
				break;
			}
			else
				q = q->next;
		}
		frame_class = target_id;
		transfered_len = ntohl(*(buff_addr + 8));
		burst_len = ntohl(*(buff_addr + 9));

		build_ODB(fi, fi->g.seq_id, s_id, burst_len, 0, mtu, ox_id, rx_id, 0, 0, frame_class << 16);
		/* Update the SEQ_ID and Relative Offset in the 
		 * Tachyon Header Structure.
		 */
		tach_header = bus_to_virt(ntohl(*(fi->q.ptr_sest[x_id] + 5)));
		*(tach_header + 5) = htonl(fi->g.seq_id << 24);
		*(tach_header + 7) = htonl(transfered_len);
		fi->g.odb.hdr_addr = *(fi->q.ptr_sest[x_id] + 5);

		/* Invalidate the EDBs used 
		 */
		ptr_edb = bus_to_virt(ntohl(*(fi->q.ptr_sest[x_id] + 7)));

		for (i = 0; i < EDB_LEN; i++)
			if (fi->q.ptr_edb[i] == ptr_edb)
				break;
		ptr_edb--;	
		
		if (i < EDB_LEN) {
		int j;
			do {
				ptr_edb += 2;
				len += (htonl(*ptr_edb) & 0xFFFF);
				j = i;
				fi->q.free_edb_list[i++] = EDB_FREE;
				if (i == EDB_LEN) {
					i = 0;
					ptr_edb = fi->q.ptr_edb_base - 1;
				}
			} while (len < transfered_len);
			if (len > transfered_len) {
				ptr_edb--;
				fi->q.free_edb_list[j] = EDB_BUSY;
			}
			else
				ptr_edb++;
		}
		else {
			T_MSG("EDB not found while freeing");
			if (offset == (NO_OF_ENTRIES - 1))
				update_SFSBQ_indx(fi);
			return;
		}

		/* Update the EDB pointer in the ODB.
		 */
		fi->g.odb.edb_addr = htonl(virt_to_bus(ptr_edb));
		memcpy(fi->q.ptr_odb[fi->q.ocq_prod_indx], &(fi->g.odb), sizeof(ODB));
		/* Update the EDB pointer in the SEST entry. We might need
		 * this if get another XFER_RDY for the same Exchange.
		 */
		*(fi->q.ptr_sest[x_id] + 7) = htonl(virt_to_bus(ptr_edb));

		update_OCQ_indx(fi);
		if (fi->g.seq_id == MAX_SEQ_ID)
			fi->g.seq_id = 0;
		else
			fi->g.seq_id++;
	}
	else 
	/* Could be a BA_ACC or a BA_RJT.
	 */
	if (rctl == RCTL_BASIC_ACC) {
	u_int bls_type = remove_from_ox_id_list(fi, ox_id);
		DPRINTK1("BA_ACC received from S_ID 0x%x with OX_ID = %x in response to %x", s_id, ox_id, bls_type);
		if (bls_type == RCTL_BASIC_ABTS) {
		u_int STE_bit;
			/* Invalidate resources for that Exchange.
			 */
			STE_bit = ntohl(*fi->q.ptr_sest[x_id]);
			if (STE_bit & SEST_V) {
				*(fi->q.ptr_sest[x_id]) &= htonl(SEST_INV);
				invalidate_SEST_entry(fi, ox_id);
			}
		}
	}
	else
	if (rctl == RCTL_BASIC_RJT) {
	u_int bls_type = remove_from_ox_id_list(fi, ox_id);
		DPRINTK1("BA_RJT received from S_ID 0x%x with OX_ID = %x in response to %x", s_id, ox_id, bls_type);
		if (bls_type == RCTL_BASIC_ABTS) {
		u_int STE_bit;
			/* Invalidate resources for that Exchange.
			 */
			STE_bit = ntohl(*fi->q.ptr_sest[x_id]);
			if (STE_bit & SEST_V) {
				*(fi->q.ptr_sest[x_id]) &= htonl(SEST_INV);
				invalidate_SEST_entry(fi, ox_id);
			}
		}
	}
	else
		DPRINTK1("Frame with R_CTL = %x received from S_ID 0x%x with OX_ID %x", rctl, s_id, ox_id);

	/* Else, discard the "Bad" SCSI frame.
	 */

	/* provide Tachyon will another set of buffers 
	 */
	if (offset == (NO_OF_ENTRIES - 1))
		update_SFSBQ_indx(fi);
	LEAVE("handle_Bad_SCSI_Frame_interrupt");
}

static void handle_Inbound_SCSI_Status_interrupt(struct fc_info *fi)
{
struct Scsi_Host *host = fi->host;
struct iph5526_hostdata *hostdata = (struct iph5526_hostdata *)host->hostdata;
u_int *ptr_imq_entry, *buff_addr, *ptr_rsp_info, *ptr_sense_info = NULL;
int queue_indx, offset, payload_size;
u_short received_ox_id, x_id;
Scsi_Cmnd *Cmnd;
u_int fcp_status, fcp_rsp_info_len = 0, fcp_sense_info_len = 0, s_id;
	ENTER("handle_SCSI_status_interrupt");

	ptr_imq_entry = fi->q.ptr_imqe[fi->q.imq_cons_indx];
	offset = ntohl(*(ptr_imq_entry + 1)) & 0x00000007;
	queue_indx = ntohl(*(ptr_imq_entry + 1)) & 0xFFFF0000;
	queue_indx = queue_indx >> 16;
	buff_addr = bus_to_virt(ntohl(*(fi->q.ptr_sfsbq_base + queue_indx*NO_OF_ENTRIES + offset)));
	payload_size = ntohl(*(ptr_imq_entry + 2));
	received_ox_id = ntohl(*(buff_addr + 6)) >> 16;

	buff_addr = bus_to_virt(ntohl(*(fi->q.ptr_sfsbq_base + queue_indx*NO_OF_ENTRIES + offset)));

	fcp_status = ntohl(*(buff_addr + 10));
	ptr_rsp_info = buff_addr + 14;
	if (fcp_status & FCP_STATUS_RSP_LEN)
		fcp_rsp_info_len = ntohl(*(buff_addr + 13));
		
	if (fcp_status & FCP_STATUS_SENSE_LEN) {
		ptr_sense_info = ptr_rsp_info + fcp_rsp_info_len / 4;
		fcp_sense_info_len = ntohl(*(buff_addr + 12));
		DPRINTK("sense_info = %x", (u_int)ntohl(*ptr_sense_info));
	}
	DPRINTK("fcp_status = %x, fcp_rsp_len = %x", fcp_status, fcp_rsp_info_len);
	x_id = received_ox_id & MAX_SCSI_XID;
	Cmnd = hostdata->cmnd_handler[x_id];
	hostdata->cmnd_handler[x_id] = NULL;
	if (Cmnd != NULL) {
		memset(Cmnd->sense_buffer, 0, sizeof(Cmnd->sense_buffer));
		/* Check if there is a Sense field */
		if (fcp_status & FCP_STATUS_SENSE_LEN) {
		int size = sizeof(Cmnd->sense_buffer);
			if (fcp_sense_info_len < size)
				size = fcp_sense_info_len;
			memcpy(Cmnd->sense_buffer, (char *)ptr_sense_info, size);
		}
		Cmnd->result = fcp_status & FCP_STATUS_MASK;
		(*Cmnd->scsi_done) (Cmnd);
	}
	else
		T_MSG("NULL Command out of handler!");

	invalidate_SEST_entry(fi, received_ox_id);
	s_id = ntohl(*(buff_addr + 3)) & 0x00FFFFFF;
	fi->q.free_scsi_oxid[x_id] = OXID_AVAILABLE;

	/* provide Tachyon will another set of buffers */
	if (offset == (NO_OF_ENTRIES - 1))
		update_SFSBQ_indx(fi);
	LEAVE("handle_SCSI_status_interrupt");
}

static void invalidate_SEST_entry(struct fc_info *fi, u_short received_ox_id)
{
u_short x_id = received_ox_id & MAX_SCSI_XID;
	/* Invalidate SEST entry if it is an OutBound SEST Entry 
	 */
	if (!(received_ox_id & SCSI_READ_BIT)) {
	u_int *ptr_tach_header, *ptr_edb;
	u_short temp_ox_id = NOT_SCSI_XID;
	int i;
		*(fi->q.ptr_sest[x_id]) &= htonl(SEST_INV);

		/* Invalidate the Tachyon Header structure 
		 */
		ptr_tach_header = bus_to_virt(ntohl(*(fi->q.ptr_sest[x_id] + 5)));
		for (i = 0; i < NO_OF_TACH_HEADERS; i++) 
			if(fi->q.ptr_tachyon_header[i] == ptr_tach_header)
				break;
		if (i < NO_OF_TACH_HEADERS) 
			memset(ptr_tach_header, 0xFF, 32);
		else
			T_MSG("Tachyon Header not found while freeing in invalidate_SEST_entry()");

		/* Invalidate the EDB used 
		 */
		ptr_edb = bus_to_virt(ntohl(*(fi->q.ptr_sest[x_id] + 7)));
		for (i = 0; i < EDB_LEN; i++)
			if (fi->q.ptr_edb[i] == ptr_edb)
				break;
		ptr_edb--;	
		if (i < EDB_LEN) {
			do {
				ptr_edb += 2;
				fi->q.free_edb_list[i++] = EDB_FREE;
				if (i == EDB_LEN) {
					i = 0;
					ptr_edb = fi->q.ptr_edb_base - 1;
				}
			} while ((htonl(*ptr_edb) & 0x80000000) != 0x80000000);
		}
		else
			T_MSG("EDB not found while freeing in invalidate_SEST_entry()");
		
		/* Search for its other header structure and destroy it! 
		 */
		if ((ptr_tach_header + 16) < (fi->q.ptr_tachyon_header_base + (MY_PAGE_SIZE/4)))
			ptr_tach_header += 16;
		else
			ptr_tach_header = fi->q.ptr_tachyon_header_base;
		while (temp_ox_id != x_id) {
			temp_ox_id = ntohl(*(ptr_tach_header + 6)) >> 16;
			if (temp_ox_id == x_id) {
				/* Paranoid checking...
				 */
				for (i = 0; i < NO_OF_TACH_HEADERS; i++) 
					if(fi->q.ptr_tachyon_header[i] == ptr_tach_header)
						break;
				if (i < NO_OF_TACH_HEADERS)
					memset(ptr_tach_header, 0xFF, 32);
				else
					T_MSG("Tachyon Header not found while freeing in invalidate_SEST_entry()");
				break;
			}
			else {
				if ((ptr_tach_header + 16) < (fi->q.ptr_tachyon_header_base + (MY_PAGE_SIZE/4)))
					ptr_tach_header += 16;
				else
					ptr_tach_header = fi->q.ptr_tachyon_header_base;
			}
		}
	}
	else {
	u_short sdb_table_indx;
		/* An Inbound Command has completed or needs to be Aborted. 
	 	 * Clear up the SDB buffers.
		 */
		sdb_table_indx = *(fi->q.ptr_sest[x_id] + 5);
		fi->q.sdb_slot_status[sdb_table_indx] = SDB_FREE;
	}
}

static void handle_Inbound_SCSI_Command_interrupt(struct fc_info *fi)
{
u_int *ptr_imq_entry;
int queue_indx, offset;
	ENTER("handle_Inbound_SCSI_Command_interrupt");
	ptr_imq_entry = fi->q.ptr_imqe[fi->q.imq_cons_indx];
	offset = ntohl(*(ptr_imq_entry + 1)) & 0x00000007;
	queue_indx = ntohl(*(ptr_imq_entry + 1)) & 0xFFFF0000;
	queue_indx = queue_indx >> 16;
	/* We discard the SCSI frame as we shouldn't be receiving
	 * a SCSI Command in the first place 
	 */
	/* provide Tachyon will another set of buffers */
	if (offset == (NO_OF_ENTRIES - 1))
		update_SFSBQ_indx(fi);
	LEAVE("handle_Inbound_SCSI_Command_interrupt");
}

static void handle_SFS_interrupt(struct fc_info *fi)
{
u_int *ptr_imq_entry, *buff_addr;
u_int class_of_frame, type_of_frame, s_id, els_type = 0, rctl;
int queue_indx, offset, payload_size, login_state;
u_short received_ox_id, fs_cmnd_code;
	ENTER("handle_SFS_interrupt");
	ptr_imq_entry = fi->q.ptr_imqe[fi->q.imq_cons_indx];
	offset = ntohl(*(ptr_imq_entry + 1)) & 0x00000007;
	queue_indx = ntohl(*(ptr_imq_entry + 1)) & 0xFFFF0000;
	queue_indx = queue_indx >> 16;
	DPRINTK("queue_indx = %d, offset  = %d\n", queue_indx, offset);
	payload_size = ntohl(*(ptr_imq_entry + 2));
	DPRINTK("payload_size = %d", payload_size);

	buff_addr = bus_to_virt(ntohl(*(fi->q.ptr_sfsbq_base + queue_indx*NO_OF_ENTRIES + offset)));

	/* extract Type of Frame */
	type_of_frame = ntohl(*(buff_addr + 4)) & 0xFF000000;
	s_id = ntohl(*(buff_addr + 3)) & 0x00FFFFFF;
	received_ox_id = ntohl(*(buff_addr + 6)) >> 16;
	switch(type_of_frame) {
		case TYPE_BLS:
			rctl = ntohl(*(buff_addr + 2)) & 0xFF000000;
			switch(rctl) {
				case RCTL_BASIC_ABTS:
					/* As an Initiator, we should never be receiving 
					 * this.
		 			 */
					DPRINTK1("ABTS received from S_ID 0x%x with OX_ID = %x", s_id, received_ox_id);
					break;
			}
			break;
		case TYPE_ELS:
			class_of_frame = ntohl(*(buff_addr + 8));
			login_state = sid_logged_in(fi, s_id);
			switch(class_of_frame & 0xFF000000) {
				case ELS_PLOGI:
					if (s_id != fi->g.my_id) {
						u_int ret_code;
						DPRINTK1("PLOGI received from D_ID 0x%x with 0X_ID = %x", s_id, received_ox_id);
						if ((ret_code = plogi_ok(fi, buff_addr, payload_size)) == 0){
							tx_logi_acc(fi, ELS_ACC, s_id, received_ox_id);
							add_to_address_cache(fi, buff_addr);
						}
						else {
							u_short cmnd_code = ret_code >> 16;
							u_short expln_code =  ret_code;
							tx_ls_rjt(fi, s_id, received_ox_id, cmnd_code, expln_code);
						}
					}
					break;
				case ELS_ACC:
					els_type = remove_from_ox_id_list(fi, received_ox_id);
					DPRINTK1("ELS_ACC received from D_ID 0x%x in response to ELS %x", s_id, els_type);
					switch(els_type) {
						case ELS_PLOGI:
							add_to_address_cache(fi, buff_addr);
							tx_prli(fi, ELS_PRLI, s_id, OX_ID_FIRST_SEQUENCE);
							break;
						case ELS_FLOGI:
							add_to_address_cache(fi, buff_addr);
							fi->g.my_id = ntohl(*(buff_addr + 2)) & 0x00FFFFFF;
							fi->g.fabric_present = TRUE;
							fi->g.my_ddaa = fi->g.my_id & 0xFFFF00;
							/* Login to the Name Server 
							 */
							tx_logi(fi, ELS_PLOGI, DIRECTORY_SERVER); 
							break;
						case ELS_NS_PLOGI:
							fi->g.name_server = TRUE;
							add_to_address_cache(fi, buff_addr);
							tx_name_server_req(fi, FCS_RFC_4);
							tx_scr(fi);
							/* Some devices have a delay before 
							 * registering with the Name Server 
							 */
							udelay(500); 
							tx_name_server_req(fi, FCS_GP_ID4);
							break;
						case ELS_PRLI:
							mark_scsi_sid(fi, buff_addr, ADD_ENTRY);
							break;
						case ELS_ADISC:
							if (!(validate_login(fi, buff_addr)))
								tx_logo(fi, s_id, OX_ID_FIRST_SEQUENCE);
							break;
					}
					break;
				case ELS_PDISC:
					DPRINTK1("ELS_PDISC received from D_ID 0x%x", s_id);
					tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_ADISC:
					DPRINTK1("ELS_ADISC received from D_ID 0x%x", s_id);
					if (node_logged_in_prev(fi, buff_addr))
						tx_adisc(fi, ELS_ACC, s_id, received_ox_id);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_PRLI:
					DPRINTK1("ELS_PRLI received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN)) {
						tx_prli(fi, ELS_ACC, s_id, received_ox_id);
						mark_scsi_sid(fi, buff_addr, ADD_ENTRY);
					}
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_PRLO:
					DPRINTK1("ELS_PRLO received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_OUT) || (login_state == NODE_NOT_PRESENT))
						tx_logo(fi, s_id, received_ox_id);
					else
					if (login_state == NODE_LOGGED_IN)

						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
					if (login_state == NODE_PROCESS_LOGGED_IN) {
						tx_prli(fi, ELS_ACC, s_id, received_ox_id);
						mark_scsi_sid(fi, buff_addr, DELETE_ENTRY);
					}
					break;
				case ELS_LS_RJT:
					els_type = remove_from_ox_id_list(fi, received_ox_id);
					DPRINTK1("ELS_LS_RJT received from D_ID 0x%x in response to %x", s_id, els_type);
					/* We should be chking the reason code.
					 */
					switch (els_type) {
						case ELS_ADISC:
							tx_logi(fi, ELS_PLOGI, s_id);
							break;
					}		
					break;
				case ELS_LOGO:
					els_type = remove_from_ox_id_list(fi, received_ox_id);
					DPRINTK1("ELS_LOGO received from D_ID 0x%x in response to %x", s_id, els_type);
					remove_from_address_cache(fi, buff_addr, ELS_LOGO);
					tx_acc(fi, s_id, received_ox_id);
					if (els_type == ELS_ADISC)
						tx_logi(fi, ELS_PLOGI, s_id);
					break;
				case ELS_RSCN:
					DPRINTK1("ELS_RSCN received from D_ID 0x%x", s_id);
					tx_acc(fi, s_id, received_ox_id);
					remove_from_address_cache(fi, buff_addr, ELS_RSCN);
					break;
				case ELS_FARP_REQ:
					/* We do not support FARP.
					   So, silently discard it */
					DPRINTK1("ELS_FARP_REQ received from D_ID 0x%x", s_id);
					break;
				case ELS_ABTX:
					DPRINTK1("ELS_ABTX received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_FLOGI:
					DPRINTK1("ELS_FLOGI received from D_ID 0x%x", s_id);
					if (fi->g.ptp_up == TRUE) {
						/* The node could have come up as an N_Port
						 * in a Loop! So,try initializing as an NL_port
						 */
						take_tachyon_offline(fi);
						/* write AL_TIME & E_D_TOV into the registers */
						writel(TOV_VALUES, fi->t_r.ptr_fm_tov_reg);
						writel(LOOP_INIT_SOFT_ADDRESS, fi->t_r.ptr_fm_config_reg);
						DPRINTK1("FLOGI received, TACHYON initializing as L_Port...\n");
						writel(INITIALIZE, fi->t_r.ptr_fm_control_reg);
					}
					else {
						if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
							tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
						else
							tx_logo(fi, s_id, received_ox_id);
					}
					break;
				case ELS_ADVC:
					DPRINTK1("ELS_ADVC received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_ECHO:
					DPRINTK1("ELS_ECHO received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_ESTC:
					DPRINTK1("ELS_ESTC received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_ESTS:
					DPRINTK1("ELS_ESTS received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_RCS:
					DPRINTK1("ELS_RCS received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_RES:
					DPRINTK1("ELS_RES received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_RLS:
					DPRINTK1("ELS_RLS received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_RRQ:
					DPRINTK1("ELS_RRQ received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_RSS:
					DPRINTK1("ELS_RSS received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_RTV:
					DPRINTK1("ELS_RTV received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_RSI:
					DPRINTK1("ELS_RSI received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_TEST:
					/* No reply sequence */
					DPRINTK1("ELS_TEST received from D_ID 0x%x", s_id);
					break;
				case ELS_RNC:
					DPRINTK1("ELS_RNC received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_RVCS:
					DPRINTK1("ELS_RVCS received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_TPLS:
					DPRINTK1("ELS_TPLS received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_GAID:
					DPRINTK1("ELS_GAID received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_FACT:
					DPRINTK1("ELS_FACT received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_FAN:
					/* Hmmm... You don't support FAN ??? */
					DPRINTK1("ELS_FAN received from D_ID 0x%x", s_id);
					tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					break;
				case ELS_FDACT:
					DPRINTK1("ELS_FDACT received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_NACT:
					DPRINTK1("ELS_NACT received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_NDACT:
					DPRINTK1("ELS_NDACT received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_QoSR:
					DPRINTK1("ELS_QoSR received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				case ELS_FDISC:
					DPRINTK1("ELS_FDISC received from D_ID 0x%x", s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
				default:
					DPRINTK1("ELS Frame %x received from D_ID 0x%x", class_of_frame, s_id);
					if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN))
						tx_ls_rjt(fi, s_id, received_ox_id, CMND_NOT_SUPP, NO_EXPLN);
					else
						tx_logo(fi, s_id, received_ox_id);
					break;
			}
			break;
		case TYPE_FC_SERVICES:
			fs_cmnd_code = (ntohl(*(buff_addr + 10)) & 0xFFFF0000) >>16;
			switch(fs_cmnd_code) {
				case FCS_ACC:
					els_type = remove_from_ox_id_list(fi, received_ox_id);
					DPRINTK1("FCS_ACC received from D_ID 0x%x in response to %x", s_id, els_type);
					if (els_type == FCS_GP_ID4) 
						explore_fabric(fi, buff_addr);
					break;
				case FCS_REJECT:
					DPRINTK1("FCS_REJECT received from D_ID 0x%x in response to %x", s_id, els_type);
					break;
			}
			break;
		case TYPE_LLC_SNAP:
			rx_net_packet(fi, (u_char *)buff_addr, payload_size);
			break;
		default:
			T_MSG("Frame Type %x received from %x", type_of_frame, s_id);
	}

	/* provide Tachyon will another set of buffers */
	if (offset == (NO_OF_ENTRIES - 1))
		update_SFSBQ_indx(fi);
	LEAVE("handle_SFS_interrupt");
}

static void handle_FM_interrupt(struct fc_info *fi)
{
u_int fm_status;
u_int tachyon_status;

	ENTER("handle_FM_interrupt");
	fm_status = readl(fi->t_r.ptr_fm_status_reg);
	tachyon_status = readl(fi->t_r.ptr_tach_status_reg);
	DPRINTK("FM_status = %x, Tachyon_status = %x", fm_status, tachyon_status);
	if (fm_status & LINK_DOWN) {
		T_MSG("Fibre Channel Link DOWN");
		fm_status = readl(fi->t_r.ptr_fm_status_reg);
		
		del_timer(&fi->explore_timer);
		del_timer(&fi->nport_timer);
		del_timer(&fi->lport_timer);
		del_timer(&fi->display_cache_timer);
		fi->g.link_up = FALSE;
		if (fi->g.ptp_up == TRUE)
			fi->g.n_port_try = FALSE;
		fi->g.ptp_up = FALSE;
		fi->g.port_discovery = FALSE;
		fi->g.explore_fabric = FALSE;
		fi->g.perform_adisc = FALSE;

		/* Logout will all nodes */
		if (fi->node_info_list) {
			struct fc_node_info *temp_list = fi->node_info_list;
				while(temp_list) {
					temp_list->login = LOGIN_ATTEMPTED;
					temp_list = temp_list->next;
				}
				fi->num_nodes = 0;
		}

		if ((fi->g.n_port_try == FALSE) && (fi->g.dont_init == FALSE)){
			take_tachyon_offline(fi);
			/* write AL_TIME & E_D_TOV into the registers */
			writel(TOV_VALUES, fi->t_r.ptr_fm_tov_reg);
			
			if ((fi->g.fabric_present == TRUE) && (fi->g.loop_up == TRUE)) {
			u_int al_pa = fi->g.my_id & 0xFF;
				writel((al_pa << 24) | LOOP_INIT_FABRIC_ADDRESS | LOOP_INIT_PREVIOUS_ADDRESS, fi->t_r.ptr_fm_config_reg);
			}
			else 
			if (fi->g.loop_up == TRUE) {
			u_int al_pa = fi->g.my_id & 0xFF;
				writel((al_pa << 24) | LOOP_INIT_PREVIOUS_ADDRESS, fi->t_r.ptr_fm_config_reg);
			}
			else 
				writel(LOOP_INIT_SOFT_ADDRESS, fi->t_r.ptr_fm_config_reg);
			fi->g.loop_up = FALSE;
			DPRINTK1("In LDWN TACHYON initializing as L_Port...\n");
			writel(INITIALIZE, fi->t_r.ptr_fm_control_reg);
		}
	}

    if (fm_status & NON_PARTICIPATING) {
	  	T_MSG("Did not acquire an AL_PA. I am not participating");
    }
	else
	if ((fm_status & LINK_UP) && ((fm_status & LINK_DOWN) == 0)) {
	  T_MSG("Fibre Channel Link UP");
	  if ((fm_status & NON_PARTICIPATING) != TRUE) {
		fi->g.link_up = TRUE;
		if (tachyon_status & OSM_FROZEN) {
			reset_tachyon(fi, ERROR_RELEASE);
			reset_tachyon(fi, OCQ_RESET);
		}
		init_timer(&fi->explore_timer);
		init_timer(&fi->nport_timer);
		init_timer(&fi->lport_timer);
		init_timer(&fi->display_cache_timer);
		if ((fm_status & OLD_PORT) == 0) {
			fi->g.loop_up = TRUE;
			fi->g.ptp_up = FALSE;
			fi->g.my_id = readl(fi->t_r.ptr_fm_config_reg) >> 24;
			DPRINTK1("My AL_PA = %x", fi->g.my_id);
			fi->g.port_discovery = TRUE;
			fi->g.explore_fabric = FALSE;
		}
		else
		if (((fm_status & 0xF0) == OLD_PORT) && ((fm_status & 0x0F) == PORT_STATE_ACTIVE)) {
			fi->g.loop_up = FALSE;
			fi->g.my_id = 0x0;
			/* In a point-to-point configuration, we expect to be
			 * connected to an F_Port. This driver does not yet support
			 * a configuration where it is connected to another N_Port
			 * directly.
			 */
			fi->g.explore_fabric = TRUE;
			fi->g.port_discovery = FALSE;
			if (fi->g.n_port_try == FALSE) {
				take_tachyon_offline(fi);
				/* write R_T_TOV & E_D_TOV into the registers */
				writel(PTP_TOV_VALUES, fi->t_r.ptr_fm_tov_reg);
				writel(BB_CREDIT | NPORT, fi->t_r.ptr_fm_config_reg);
				fi->g.n_port_try = TRUE;
				DPRINTK1("In LUP TACHYON initializing as N_Port...\n");
				writel(INITIALIZE, fi->t_r.ptr_fm_control_reg);
			}
			else {
				fi->g.ptp_up = TRUE;
				tx_logi(fi, ELS_FLOGI, F_PORT); 
			}
		}
		fi->g.my_ddaa = 0x0;
		fi->g.fabric_present = FALSE; 
		/* We havn't sent out any Name Server Reqs */
		fi->g.name_server = FALSE;
		fi->g.alpa_list_index = 0;
		fi->g.ox_id = NOT_SCSI_XID;
		fi->g.my_mtu = TACH_FRAME_SIZE;
		
		/* Implicitly LOGO with all logged-in nodes. 
		 */
		if (fi->node_info_list) {
		struct fc_node_info *temp_list = fi->node_info_list;
			while(temp_list) {
				temp_list->login = LOGIN_ATTEMPTED;
				temp_list = temp_list->next;
			}
			fi->num_nodes = 0;
			fi->g.perform_adisc = TRUE;
			//fi->g.perform_adisc = FALSE;
			fi->g.port_discovery = FALSE;
			tx_logi(fi, ELS_FLOGI, F_PORT); 
		}
		else { 
			/* If Link coming up for the _first_ time or no nodes
			 * were logged in before...
			 */
			fi->g.scsi_oxid = 0;
			fi->g.seq_id = 0x00;
			fi->g.perform_adisc = FALSE;
		}

		/* reset OX_ID table */
		while (fi->ox_id_list) {
		struct ox_id_els_map *temp = fi->ox_id_list;
			fi->ox_id_list = fi->ox_id_list->next;
			kfree(temp);
		}
		fi->ox_id_list = NULL;
	  } /* End of if partipating */
	}

	if (fm_status & ELASTIC_STORE_ERROR) {
		/* Too much junk on the Link 
		 */
		/* Trying to clear it up by Txing PLOGI to urself */
		if (fi->g.link_up == TRUE)
			tx_logi(fi, ELS_PLOGI, fi->g.my_id); 
	}

	if (fm_status & LOOP_UP) {
		if (tachyon_status & OSM_FROZEN) {
			reset_tachyon(fi, ERROR_RELEASE);
			reset_tachyon(fi, OCQ_RESET);
		}
	}
	
	if (fm_status & NOS_OLS_RECEIVED){
		if (fi->g.nport_timer_set == FALSE) {
			DPRINTK("NOS/OLS Received");
			DPRINTK("FM_status = %x", fm_status);
			fi->nport_timer.function = nos_ols_timer;
			fi->nport_timer.data = (unsigned long)fi;
			fi->nport_timer.expires = RUN_AT((3*HZ)/100); /* 30 msec */
			init_timer(&fi->nport_timer);
			add_timer(&fi->nport_timer);
			fi->g.nport_timer_set = TRUE;
		}
	}

	if (((fm_status & 0xF0) == OLD_PORT) && (((fm_status & 0x0F) == PORT_STATE_LF1) || ((fm_status & 0x0F) == PORT_STATE_LF2))) {
		DPRINTK1("Link Fail-I in OLD-PORT.");
		take_tachyon_offline(fi);
		reset_tachyon(fi, SOFTWARE_RESET);
	}

	if (fm_status & LOOP_STATE_TIMEOUT){
		if ((fm_status & 0xF0) == ARBITRATING) 
			DPRINTK1("ED_TOV timesout.In ARBITRATING state...");
		if ((fm_status & 0xF0) == ARB_WON)
			DPRINTK1("ED_TOV timesout.In ARBITRATION WON state...");
		if ((fm_status & 0xF0) == OPEN)
			DPRINTK1("ED_TOV timesout.In OPEN state...");
		if ((fm_status & 0xF0) == OPENED)
			DPRINTK1("ED_TOV timesout.In OPENED state...");
		if ((fm_status & 0xF0) == TX_CLS)
			DPRINTK1("ED_TOV timesout.In XMITTED CLOSE state...");
		if ((fm_status & 0xF0) == RX_CLS)
			DPRINTK1("ED_TOV timesout.In RECEIVED CLOSE state...");
		if ((fm_status & 0xF0) == INITIALIZING)
			DPRINTK1("ED_TOV timesout.In INITIALIZING state...");
		DPRINTK1("Initializing Loop...");
		writel(INITIALIZE, fi->t_r.ptr_fm_control_reg);
	}
	
	if ((fm_status & BAD_ALPA) && (fi->g.loop_up == TRUE)) {
	u_char bad_alpa = (readl(fi->t_r.ptr_fm_rx_al_pa_reg) & 0xFF00) >> 8;
		if (tachyon_status & OSM_FROZEN) {
			reset_tachyon(fi, ERROR_RELEASE);
			reset_tachyon(fi, OCQ_RESET);
		}
		/* Fix for B34 */
		tx_logi(fi, ELS_PLOGI, fi->g.my_id); 

		if (!fi->g.port_discovery && !fi->g.perform_adisc) {
			if (bad_alpa != 0xFE)
				DPRINTK("Bad AL_PA = %x", bad_alpa);
		}
		else {
			if ((fi->g.perform_adisc == TRUE) && (bad_alpa == 0x00)) {
				DPRINTK1("Performing ADISC...");
				fi->g.fabric_present = FALSE;
				perform_adisc(fi);
			}
		}
	}
	
	if (fm_status & LIPF_RECEIVED){
		DPRINTK("LIP(F8) Received");
	}

	if (fm_status & LINK_FAILURE) {
		if (fm_status & LOSS_OF_SIGNAL)
			DPRINTK1("Detected Loss of Signal.");
		if (fm_status & OUT_OF_SYNC)
			DPRINTK1("Detected Loss of Synchronization.");
	}

	if (fm_status & TRANSMIT_PARITY_ERROR) {
		/* Bad! Should not happen. Solution-> Hard Reset.
		 */
		T_MSG("Parity Error. Perform Hard Reset!");
	}

	if (fi->g.alpa_list_index >= MAX_NODES){
		if (fi->g.port_discovery == TRUE) {
			fi->g.port_discovery = FALSE;
			add_display_cache_timer(fi);
		}
		fi->g.alpa_list_index = MAX_NODES;
	}

	if (fi->g.port_discovery == TRUE) 
		local_port_discovery(fi);

	LEAVE("handle_FM_interrupt");
	return;
}

static void local_port_discovery(struct fc_info *fi)
{
	if (fi->g.loop_up == TRUE) {
		/* If this is not here, some of the Bad AL_PAs are missed. 
		 */
		udelay(20); 
		if ((fi->g.alpa_list_index == 0) && (fi->g.fabric_present == FALSE)){
			tx_logi(fi, ELS_FLOGI, F_PORT); 
		}
		else {
		int login_state = sid_logged_in(fi, fi->g.my_ddaa | alpa_list[fi->g.alpa_list_index]);
			while ((fi->g.alpa_list_index == 0) || ((fi->g.alpa_list_index < MAX_NODES) && ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN) || (alpa_list[fi->g.alpa_list_index] == (fi->g.my_id & 0xFF)))))
				fi->g.alpa_list_index++;
			if (fi->g.alpa_list_index < MAX_NODES)
				tx_logi(fi, ELS_PLOGI, alpa_list[fi->g.alpa_list_index]); 
		}
		fi->g.alpa_list_index++;
		if (fi->g.alpa_list_index >= MAX_NODES){
			if (fi->g.port_discovery == TRUE) {
				fi->g.port_discovery = FALSE;
				add_display_cache_timer(fi);
			}
			fi->g.alpa_list_index = MAX_NODES;
		}
	}
}

static void nos_ols_timer(unsigned long data)
{
struct fc_info *fi = (struct fc_info*)data;
u_int fm_status;
	fm_status = readl(fi->t_r.ptr_fm_status_reg);
	DPRINTK1("FM_status in timer= %x", fm_status);
	fi->g.nport_timer_set = FALSE;
	del_timer(&fi->nport_timer);
	if ((fi->g.ptp_up == TRUE) || (fi->g.loop_up == TRUE))
		return;
	if (((fm_status & 0xF0) == OLD_PORT) && (((fm_status & 0x0F) == PORT_STATE_ACTIVE) || ((fm_status & 0x0F) == PORT_STATE_OFFLINE))) {
		DPRINTK1("In OLD-PORT after E_D_TOV.");
		take_tachyon_offline(fi);
		/* write R_T_TOV & E_D_TOV into the registers */
		writel(PTP_TOV_VALUES, fi->t_r.ptr_fm_tov_reg);
		writel(BB_CREDIT | NPORT, fi->t_r.ptr_fm_config_reg);
		fi->g.n_port_try = TRUE;
		DPRINTK1("In timer, TACHYON initializing as N_Port...\n");
		writel(INITIALIZE, fi->t_r.ptr_fm_control_reg);
	}
	else
	if ((fi->g.lport_timer_set == FALSE) && ((fm_status & 0xF0) == LOOP_FAIL)) {
		DPRINTK1("Loop Fail after E_D_TOV.");
		fi->lport_timer.function = loop_timer;
		fi->lport_timer.data = (unsigned long)fi;
		fi->lport_timer.expires = RUN_AT((8*HZ)/100); 
		init_timer(&fi->lport_timer);
		add_timer(&fi->lport_timer);
		fi->g.lport_timer_set = TRUE;
		take_tachyon_offline(fi);
		reset_tachyon(fi, SOFTWARE_RESET);
	}
	else
	if (((fm_status & 0xF0) == OLD_PORT) && (((fm_status & 0x0F) == PORT_STATE_LF1) || ((fm_status & 0x0F) == PORT_STATE_LF2))) {
		DPRINTK1("Link Fail-II in OLD-PORT.");
		take_tachyon_offline(fi);
		reset_tachyon(fi, SOFTWARE_RESET);
	}
}

static void loop_timer(unsigned long data)
{
struct fc_info *fi = (struct fc_info*)data;
	fi->g.lport_timer_set = FALSE;
	del_timer(&fi->lport_timer);
	if ((fi->g.ptp_up == TRUE) || (fi->g.loop_up == TRUE))
		return;
}

static void add_display_cache_timer(struct fc_info *fi)
{
	fi->display_cache_timer.function = display_cache_timer;
	fi->display_cache_timer.data = (unsigned long)fi;
	fi->display_cache_timer.expires = RUN_AT(fi->num_nodes * HZ); 
	init_timer(&fi->display_cache_timer);
	add_timer(&fi->display_cache_timer);
}

static void display_cache_timer(unsigned long data)
{
struct fc_info *fi = (struct fc_info*)data;
	del_timer(&fi->display_cache_timer);
	display_cache(fi);
	return;
}

static void reset_tachyon(struct fc_info *fi, u_int value)
{
u_int tachyon_status, reset_done = OCQ_RESET_STATUS | SCSI_FREEZE_STATUS;
int not_done = 1, i = 0;
	writel(value, fi->t_r.ptr_tach_control_reg);
	if (value == OCQ_RESET) 
		fi->q.ocq_prod_indx = 0;
	tachyon_status = readl(fi->t_r.ptr_tach_status_reg);

	/* Software resets are immediately done, whereas other aren't. It 
	about 30 clocks to do the reset */
	if (value != SOFTWARE_RESET) {
		while(not_done) {
			if (i++ > 100000) {
				T_MSG("Reset was unsuccessful! Tachyon Status = %x", tachyon_status);
				break;
			}
			tachyon_status = readl(fi->t_r.ptr_tach_status_reg);
			if ((tachyon_status & reset_done) == 0)
				not_done = 0;
		}
	}
	else {
		write_to_tachyon_registers(fi);
	}
}

static void take_tachyon_offline(struct fc_info *fi)
{
u_int fm_status = readl(fi->t_r.ptr_fm_status_reg);

	/* The first two conditions will never be true. The Manual and
	 * the errata say this. But the current implementation is
	 * decently stable.
	 */	 
	//if ((fm_status & 0xF0) == LOOP_FAIL) {
	if (fm_status == LOOP_FAIL) {
		// workaround as in P. 89 
		writel(HOST_CONTROL, fi->t_r.ptr_fm_control_reg);
		if (fi->g.loop_up == TRUE)
			writel(SOFTWARE_RESET, fi->t_r.ptr_tach_control_reg);
		else {
			writel(OFFLINE, fi->t_r.ptr_fm_control_reg);
			writel(EXIT_HOST_CONTROL, fi->t_r.ptr_fm_control_reg);
		}
	}
	else
	//if ((fm_status & LOOP_UP) == LOOP_UP) {
	if (fm_status == LOOP_UP) {
		writel(SOFTWARE_RESET, fi->t_r.ptr_tach_control_reg);
	}
	else
		writel(OFFLINE, fi->t_r.ptr_fm_control_reg);
}


static void read_novram(struct fc_info *fi)
{
int off = 0;
	fi->n_r.ptr_novram_hw_control_reg = fi->i_r.ptr_ichip_hw_control_reg; 
	fi->n_r.ptr_novram_hw_status_reg = fi->i_r.ptr_ichip_hw_status_reg; 
	iph5526_nr_do_init(fi);
	if (fi->clone_id == PCI_VENDOR_ID_INTERPHASE)
		off = 32;
	
	fi->g.my_node_name_high = (fi->n_r.data[off] << 16) | fi->n_r.data[off+1];
	fi->g.my_node_name_low = (fi->n_r.data[off+2] << 16) | fi->n_r.data[off+3];
	fi->g.my_port_name_high = (fi->n_r.data[off+4] << 16) | fi->n_r.data[off+5];
	fi->g.my_port_name_low = (fi->n_r.data[off+6] << 16) | fi->n_r.data[off+7];
	DPRINTK("node_name = %x %x", fi->g.my_node_name_high, fi->g.my_node_name_low);
	DPRINTK("port_name = %x %x", fi->g.my_port_name_high, fi->g.my_port_name_low);
}

static void reset_ichip(struct fc_info *fi)
{
	/* (i)chip reset */
	writel(ICHIP_HCR_RESET, fi->i_r.ptr_ichip_hw_control_reg);
	/*wait for chip to get reset */
	mdelay(10);
	/*de-assert reset */
	writel(ICHIP_HCR_DERESET, fi->i_r.ptr_ichip_hw_control_reg);
	
	/* enable INT lines on the (i)chip */
	writel(ICHIP_HCR_ENABLE_INTA , fi->i_r.ptr_ichip_hw_control_reg);
	/* enable byte swap */
	writel(ICHIP_HAMR_BYTE_SWAP_ADDR_TR, fi->i_r.ptr_ichip_hw_addr_mask_reg);
}

static void tx_logi(struct fc_info *fi, u_int logi, u_int d_id)
{
int int_required = 1;
u_short ox_id = OX_ID_FIRST_SEQUENCE;
u_int r_ctl = RCTL_ELS_UCTL;
u_int type  = TYPE_ELS | SEQUENCE_INITIATIVE | FIRST_SEQUENCE;
u_int my_mtu = fi->g.my_mtu;
	ENTER("tx_logi");
	/* We dont want interrupted for our own logi. 
	 * It screws up the port discovery process. 
	 */
	if (d_id == fi->g.my_id)
		int_required = 0;
	fill_login_frame(fi, logi);	
	fi->g.type_of_frame = FC_ELS;
	memcpy(fi->g.els_buffer[fi->g.e_i], &fi->g.login, sizeof(LOGIN));
	tx_exchange(fi, (char *)(fi->g.els_buffer[fi->g.e_i]),sizeof(LOGIN), r_ctl, type, d_id, my_mtu, int_required, ox_id, logi);
	fi->g.e_i++;
	if (fi->g.e_i == MAX_PENDING_FRAMES)
		fi->g.e_i = 0;
	LEAVE("tx_logi");
	return;
}

static void tx_logi_acc(struct fc_info *fi, u_int logi, u_int d_id, u_short received_ox_id)
{
int int_required = 0;
u_int r_ctl = RCTL_ELS_SCTL;
u_int type  = TYPE_ELS | EXCHANGE_RESPONDER | LAST_SEQUENCE;
u_int my_mtu = fi->g.my_mtu;
	ENTER("tx_logi_acc");
	fill_login_frame(fi, logi);	
	fi->g.type_of_frame = FC_ELS;
	memcpy(fi->g.els_buffer[fi->g.e_i], &fi->g.login, sizeof(LOGIN));
	tx_exchange(fi, (char *)(fi->g.els_buffer[fi->g.e_i]),sizeof(LOGIN), r_ctl, type, d_id, my_mtu, int_required, received_ox_id, logi);
	fi->g.e_i++;
	if (fi->g.e_i == MAX_PENDING_FRAMES)
		fi->g.e_i = 0;
	LEAVE("tx_logi_acc");
	return;
}

static void tx_prli(struct fc_info *fi, u_int command_code, u_int d_id, u_short received_ox_id)
{
int int_required = 1;
u_int r_ctl = RCTL_ELS_UCTL;
u_int type  = TYPE_ELS | SEQUENCE_INITIATIVE | FIRST_SEQUENCE;
u_int my_mtu = fi->g.my_mtu;
	ENTER("tx_prli");
	if (command_code == ELS_PRLI)
		fi->g.prli.cmnd_code = htons((ELS_PRLI | PAGE_LEN) >> 16);
	else {
		fi->g.prli.cmnd_code = htons((ELS_ACC | PAGE_LEN) >> 16);
		int_required = 0;
		type  = TYPE_ELS | EXCHANGE_RESPONDER | LAST_SEQUENCE;
		r_ctl = RCTL_ELS_SCTL;
	}
	fi->g.prli.payload_length = htons(PRLI_LEN);
	fi->g.prli.type_code = htons(FCP_TYPE_CODE);
	fi->g.prli.est_image_pair = htons(IMAGE_PAIR);
	fi->g.prli.responder_pa = 0;
	fi->g.prli.originator_pa = 0;
	fi->g.prli.service_params = htonl(INITIATOR_FUNC | READ_XFER_RDY_DISABLED);
	fi->g.type_of_frame = FC_ELS;
	memcpy(fi->g.els_buffer[fi->g.e_i], &fi->g.prli, sizeof(PRLI));
	tx_exchange(fi, (char *)(fi->g.els_buffer[fi->g.e_i]), sizeof(PRLI), r_ctl, type, d_id, my_mtu, int_required, received_ox_id, command_code);
	fi->g.e_i++;
	if (fi->g.e_i == MAX_PENDING_FRAMES)
		fi->g.e_i = 0;
	LEAVE("tx_prli");
	return;
}

static void tx_logo(struct fc_info *fi, u_int d_id, u_short received_ox_id)
{
int int_required = 1;
u_int r_ctl = RCTL_ELS_UCTL;
u_int type  = TYPE_ELS | EXCHANGE_RESPONDER | SEQUENCE_RESPONDER | FIRST_SEQUENCE | END_SEQUENCE | SEQUENCE_INITIATIVE;
int size = sizeof(LOGO);
char fc_id[3];
u_int my_mtu = fi->g.my_mtu;
	ENTER("tx_logo");
	fi->g.logo.logo_cmnd = htonl(ELS_LOGO);
	fi->g.logo.reserved = 0;
	memcpy(fc_id, &(fi->g.my_id), 3);
	fi->g.logo.n_port_id_0 = fc_id[0];
	fi->g.logo.n_port_id_1 = fc_id[1];
	fi->g.logo.n_port_id_2 = fc_id[2];
	fi->g.logo.port_name_up = htonl(N_PORT_NAME_HIGH);
	fi->g.logo.port_name_low = htonl(N_PORT_NAME_LOW);
	fi->g.type_of_frame = FC_ELS;
	memcpy(fi->g.els_buffer[fi->g.e_i], &fi->g.logo, sizeof(LOGO));
	tx_exchange(fi, (char *)(fi->g.els_buffer[fi->g.e_i]),size, r_ctl, type, d_id, my_mtu, int_required, received_ox_id, ELS_LOGO);
	fi->g.e_i++;
	if (fi->g.e_i == MAX_PENDING_FRAMES)
		fi->g.e_i = 0;
	LEAVE("tx_logo");
}

static void tx_adisc(struct fc_info *fi, u_int cmnd_code, u_int d_id, u_short received_ox_id)
{
int int_required = 0;
u_int r_ctl = RCTL_ELS_SCTL;
u_int type  = TYPE_ELS | EXCHANGE_RESPONDER | SEQUENCE_RESPONDER | FIRST_SEQUENCE | END_SEQUENCE;
int size = sizeof(ADISC);
u_int my_mtu = fi->g.my_mtu;
	fi->g.adisc.ls_cmnd_code = htonl(cmnd_code);
	fi->g.adisc.hard_address = htonl(0);
	fi->g.adisc.port_name_high = htonl(N_PORT_NAME_HIGH);	
	fi->g.adisc.port_name_low = htonl(N_PORT_NAME_LOW);	
	fi->g.adisc.node_name_high = htonl(NODE_NAME_HIGH);	
	fi->g.adisc.node_name_low = htonl(NODE_NAME_LOW);	
	fi->g.adisc.n_port_id = htonl(fi->g.my_id);
	if (cmnd_code == ELS_ADISC) {
		int_required = 1;
		r_ctl = RCTL_ELS_UCTL;
		type  = TYPE_ELS | SEQUENCE_INITIATIVE | FIRST_SEQUENCE;
	}
	fi->g.type_of_frame = FC_ELS;
	memcpy(fi->g.els_buffer[fi->g.e_i], &fi->g.adisc, size);
	tx_exchange(fi, (char *)(fi->g.els_buffer[fi->g.e_i]),size, r_ctl, type, d_id, my_mtu, int_required, received_ox_id, cmnd_code);
	fi->g.e_i++;
	if (fi->g.e_i == MAX_PENDING_FRAMES)
		fi->g.e_i = 0;
}

static void tx_ls_rjt(struct fc_info *fi, u_int d_id, u_short received_ox_id, u_short reason_code, u_short expln_code)
{
int int_required = 0;
u_int r_ctl = RCTL_ELS_SCTL;
u_int type  = TYPE_ELS | EXCHANGE_RESPONDER | LAST_SEQUENCE;
int size = sizeof(LS_RJT);
u_int my_mtu = fi->g.my_mtu;
	ENTER("tx_ls_rjt");
	fi->g.ls_rjt.cmnd_code = htonl(ELS_LS_RJT);
	fi->g.ls_rjt.reason_code = htonl((reason_code << 16) | expln_code); 
	fi->g.type_of_frame = FC_ELS;
	memcpy(fi->g.els_buffer[fi->g.e_i], &fi->g.ls_rjt, size);
	tx_exchange(fi, (char *)(fi->g.els_buffer[fi->g.e_i]),size, r_ctl, type, d_id, my_mtu, int_required, received_ox_id, ELS_LS_RJT);
	fi->g.e_i++;
	if (fi->g.e_i == MAX_PENDING_FRAMES)
		fi->g.e_i = 0;
	LEAVE("tx_ls_rjt");
}

static void tx_abts(struct fc_info *fi, u_int d_id, u_short ox_id)
{
int int_required = 1;
u_int r_ctl = RCTL_BASIC_ABTS;
u_int type  = TYPE_BLS | SEQUENCE_INITIATIVE | FIRST_SEQUENCE;
int size = 0;
u_int my_mtu = fi->g.my_mtu;
	ENTER("tx_abts");
	fi->g.type_of_frame = FC_BLS;
	tx_exchange(fi, NULL, size, r_ctl, type, d_id, my_mtu, int_required, ox_id, RCTL_BASIC_ABTS);
	LEAVE("tx_abts");
}

static u_int plogi_ok(struct fc_info *fi, u_int *buff_addr, int size)
{
int ret_code = 0;
u_short mtu = ntohl(*(buff_addr + 10)) & 0x00000FFF;
u_short class3 = ntohl(*(buff_addr + 25)) >> 16;
u_short class3_conc_seq = ntohl(*(buff_addr + 27)) >> 16;
u_short open_seq = ntohl(*(buff_addr + 28)) >> 16;
	DPRINTK1("mtu = %x class3 = %x conc_seq = %x open_seq = %x", mtu, class3, class3_conc_seq, open_seq);	
	size -= TACHYON_HEADER_LEN;
	if (!(class3 & 0x8000)) {
		DPRINTK1("Received PLOGI with class3 = %x", class3);
		ret_code = (LOGICAL_ERR << 16) | NO_EXPLN;
		return ret_code;
	}
	if (mtu < 256) {
		DPRINTK1("Received PLOGI with MTU set to %x", mtu);
		ret_code = (LOGICAL_ERR << 16) | RECV_FIELD_SIZE;
		return ret_code;
	}
	if (size != PLOGI_LEN) {	
		DPRINTK1("Received PLOGI of size %x", size);
		ret_code = (LOGICAL_ERR << 16) | INV_PAYLOAD_LEN;
		return ret_code;
	}
	if (class3_conc_seq == 0) {	
		DPRINTK1("Received PLOGI with conc_seq == 0");
		ret_code = (LOGICAL_ERR << 16) | CONC_SEQ;
		return ret_code;
	}
	if (open_seq == 0) {	
		DPRINTK1("Received PLOGI with open_seq == 0");
		ret_code = (LOGICAL_ERR << 16) | NO_EXPLN;
		return ret_code;
	}

	/* Could potentially check for more fields, but might end up
	   not talking to most of the devices. ;-) */
	/* Things that could get checked are:
	   common_features = 0x8800
	   total_concurrent_seq = at least 1
	*/
	return ret_code;
}

static void tx_acc(struct fc_info *fi, u_int d_id, u_short received_ox_id)
{
int int_required = 0;
u_int r_ctl = RCTL_ELS_SCTL;
u_int type  = TYPE_ELS | EXCHANGE_RESPONDER | LAST_SEQUENCE;
int size = sizeof(ACC);
u_int my_mtu = fi->g.my_mtu;
	ENTER("tx_acc");
	fi->g.acc.cmnd_code = htonl(ELS_ACC);
	fi->g.type_of_frame = FC_ELS;
	memcpy(fi->g.els_buffer[fi->g.e_i], &fi->g.acc, size);
	tx_exchange(fi, (char *)(fi->g.els_buffer[fi->g.e_i]),size, r_ctl, type, d_id, my_mtu, int_required, received_ox_id, ELS_ACC);
	fi->g.e_i++;
	if (fi->g.e_i == MAX_PENDING_FRAMES)
		fi->g.e_i = 0;
	LEAVE("tx_acc");
}


static void tx_name_server_req(struct fc_info *fi, u_int req)
{
int int_required = 1, i, size = 0;
u_short ox_id = OX_ID_FIRST_SEQUENCE;
u_int type  = TYPE_FC_SERVICES | SEQUENCE_INITIATIVE | FIRST_SEQUENCE;
u_int r_ctl = FC4_DEVICE_DATA | UNSOLICITED_CONTROL;
u_int my_mtu = fi->g.my_mtu, d_id = DIRECTORY_SERVER;
CT_HDR ct_hdr;
	ENTER("tx_name_server_req");
	/* Fill up CT_Header */
	ct_hdr.rev_in_id = htonl(FC_CT_REV);
	ct_hdr.fs_type = DIRECTORY_SERVER_APP;
	ct_hdr.fs_subtype = NAME_SERVICE;
	ct_hdr.options = 0;
	ct_hdr.resv1 = 0;
	ct_hdr.cmnd_resp_code = htons(req >> 16);
	ct_hdr.max_res_size = 0;
	ct_hdr.resv2 = 0;
	ct_hdr.reason_code = 0;
	ct_hdr.expln_code = 0;
	ct_hdr.vendor_unique = 0;
	
	fi->g.type_of_frame = FC_ELS;
	switch(req) {
		case FCS_RFC_4:
			memcpy(&(fi->g.rfc_4.ct_hdr), &ct_hdr, sizeof(CT_HDR));
			fi->g.rfc_4.s_id = htonl(fi->g.my_id);
			for (i = 0; i < 32; i++)
				fi->g.rfc_4.bit_map[i] = 0;
			/* We support IP & SCSI */
			fi->g.rfc_4.bit_map[2] = 0x01;
			fi->g.rfc_4.bit_map[3] = 0x20;
			size = sizeof(RFC_4);
			memcpy(fi->g.els_buffer[fi->g.e_i], &fi->g.rfc_4, size);
			tx_exchange(fi, (char *)(fi->g.els_buffer[fi->g.e_i]),size, r_ctl, type, d_id, my_mtu, int_required, ox_id, req);
			break;
		case FCS_GP_ID4:
			memcpy(&(fi->g.gp_id4.ct_hdr), &ct_hdr, sizeof(CT_HDR));
			fi->g.gp_id4.port_type = htonl(PORT_TYPE_NX_PORTS);
			size = sizeof(GP_ID4);
			memcpy(fi->g.els_buffer[fi->g.e_i], &fi->g.gp_id4, size);
			tx_exchange(fi, (char *)(fi->g.els_buffer[fi->g.e_i]),size, r_ctl, type, d_id, my_mtu, int_required, ox_id, req);
			break;
	}
	fi->g.e_i++;
	if (fi->g.e_i == MAX_PENDING_FRAMES)
		fi->g.e_i = 0;
	LEAVE("tx_name_server_req");
}

static void tx_scr(struct fc_info *fi)
{
int int_required = 1, size = sizeof(SCR);
u_short ox_id = OX_ID_FIRST_SEQUENCE;
u_int type  = TYPE_ELS | SEQUENCE_INITIATIVE | FIRST_SEQUENCE;
u_int r_ctl = RCTL_ELS_UCTL;
u_int my_mtu = fi->g.my_mtu, d_id = FABRIC_CONTROLLER;
	ENTER("tx_scr");
	fi->g.scr.cmnd_code = htonl(ELS_SCR);
	fi->g.scr.reg_function = htonl(FULL_REGISTRATION);
	fi->g.type_of_frame = FC_ELS;
	memcpy(fi->g.els_buffer[fi->g.e_i], &fi->g.scr, size);
	tx_exchange(fi, (char *)(fi->g.els_buffer[fi->g.e_i]),size, r_ctl, type, d_id, my_mtu, int_required, ox_id, ELS_SCR);
	fi->g.e_i++;
	if (fi->g.e_i == MAX_PENDING_FRAMES)
		fi->g.e_i = 0;
	LEAVE("tx_scr");
}

static void perform_adisc(struct fc_info *fi)
{
int count = 0;
	/* Will be set to TRUE when timer expires in a PLDA environment. 
	 */
	fi->g.port_discovery = FALSE;

	if (fi->node_info_list) {
		struct fc_node_info *temp_list = fi->node_info_list;
		while(temp_list) {
			/* Tx ADISC to all non-fabric based 
	 	 	 * entities.
	 	 	 */
			if ((temp_list->d_id & 0xFF0000) != 0xFF0000)
				tx_adisc(fi, ELS_ADISC, temp_list->d_id, OX_ID_FIRST_SEQUENCE);
			temp_list = temp_list->next;
			udelay(20);
			count++;
		}
	}
	/* Perform Port Discovery after timer expires.
	 * We are giving time for the ADISCed nodes to respond
	 * so that we dont have to perform PLOGI to those whose
	 * login are _still_ valid.
	 */
	fi->explore_timer.function = port_discovery_timer;
	fi->explore_timer.data = (unsigned long)fi;
	fi->explore_timer.expires = RUN_AT((count*3*HZ)/100); 
	init_timer(&fi->explore_timer);
	add_timer(&fi->explore_timer);
}

static void explore_fabric(struct fc_info *fi, u_int *buff_addr)
{
u_int *addr = buff_addr + 12; /* index into payload */
u_char control_code;
u_int d_id;
int count = 0;
	ENTER("explore_fabric");
	DPRINTK1("entering explore_fabric");

	/*fi->g.perform_adisc = TRUE;
	fi->g.explore_fabric = TRUE;
	perform_adisc(fi);*/
	
	do {
		d_id = ntohl(*addr) & 0x00FFFFFF;
		if (d_id != fi->g.my_id) {
			if (sid_logged_in(fi, d_id) == NODE_NOT_PRESENT)
				tx_logi(fi, ELS_PLOGI, d_id); 
			else
			if (sid_logged_in(fi, d_id) == NODE_LOGGED_OUT)
				tx_adisc(fi, ELS_ADISC, d_id, OX_ID_FIRST_SEQUENCE); 
			count++;
		}
		control_code = (ntohl(*addr) & 0xFF000000) >> 24;
		addr++;
		DPRINTK1("cc = %x, d_id = %x", control_code, d_id);
	} while (control_code != 0x80);
	
	fi->explore_timer.function = fabric_explore_timer;
	fi->explore_timer.data = (unsigned long)fi;
	/* We give 30 msec for each device to respond and then send out
	 * our SCSI enquiries. 
	 */
	fi->explore_timer.expires = RUN_AT((count*3*HZ)/100); 
	init_timer(&fi->explore_timer);
	add_timer(&fi->explore_timer);

	DPRINTK1("leaving explore_fabric");
	LEAVE("explore_fabric");
}

static void fabric_explore_timer(unsigned long data)
{
struct fc_info *fi = (struct fc_info*)data;
	del_timer(&fi->explore_timer);

	if ((fi->g.loop_up == TRUE) && (fi->g.ptp_up == FALSE)) {
		/* Initiate Local Port Discovery on the Local Loop.
		 */
		fi->g.port_discovery = TRUE;
		fi->g.alpa_list_index = 1;
		local_port_discovery(fi);
	}
	fi->g.explore_fabric = FALSE;
	return;
}

static void port_discovery_timer(unsigned long data)
{
struct fc_info *fi = (struct fc_info*)data;
	del_timer(&fi->explore_timer);
	
	if ((fi->g.loop_up == TRUE) && (fi->g.explore_fabric != TRUE)) {
		fi->g.port_discovery = TRUE;
		fi->g.alpa_list_index = 1;
		local_port_discovery(fi);
	}
	fi->g.perform_adisc = FALSE;
	return;
}

static void add_to_ox_id_list(struct fc_info *fi, u_int transaction_id, u_int cmnd_code)
{
struct ox_id_els_map *p, *q = fi->ox_id_list, *r = NULL;
int size = sizeof(struct ox_id_els_map);
	while (q != NULL) {
		r = q;
		q = q->next;
	}
	p = (struct ox_id_els_map *)kmalloc(size, GFP_ATOMIC);
	if (p == NULL) {
		T_MSG("kmalloc failed in add_to_ox_id_list()");
		return;
	}
	p->ox_id = transaction_id;
	p->els = cmnd_code;
	p->next = NULL;
	if (fi->ox_id_list == NULL)
		fi->ox_id_list = p;
	else
		r->next = p;
	return;
}

static u_int remove_from_ox_id_list(struct fc_info *fi, u_short received_ox_id)
{
struct ox_id_els_map *p = fi->ox_id_list, *q = fi->ox_id_list;
u_int els_type;
	while (q != NULL) {
		if (q->ox_id == received_ox_id) {

			if (q == fi->ox_id_list) 
				fi->ox_id_list = fi->ox_id_list->next;
			else
				if (q->next == NULL) 
					p->next = NULL;
			else 
					p->next = q->next;

			els_type = q->els;
			kfree(q);
			return els_type;
		}
		p = q;
		q = q->next;
	}
	if (q == NULL)
		DPRINTK2("Could not find ox_id %x in ox_id_els_map", received_ox_id);
	return 0;
}

static void build_tachyon_header(struct fc_info *fi, u_int my_id, u_int r_ctl, u_int d_id, u_int type, u_char seq_id, u_char df_ctl, u_short ox_id, u_short rx_id, char *data)
{
u_char alpa = d_id & 0x0000FF;
u_int dest_ddaa = d_id &0xFFFF00;

	ENTER("build_tachyon_header");
	DPRINTK("d_id = %x, my_ddaa = %x", d_id, fi->g.my_ddaa);
	/* Does it have to go to/thru a Fabric? */
	if ((dest_ddaa != 0) && ((d_id == F_PORT) || (fi->g.fabric_present && (dest_ddaa != fi->g.my_ddaa))))
		alpa = 0x00;
	fi->g.tach_header.resv = 0x00000000;
	fi->g.tach_header.sof_and_eof = SOFI3 | EOFN;
	fi->g.tach_header.dest_alpa = alpa;
	/* Set LCr properly to have enuff credit */
	if (alpa == REPLICATE)
		fi->g.tach_header.lcr_and_time_stamp = htons(0xC00);/* LCr=3 */
	else
		fi->g.tach_header.lcr_and_time_stamp = 0;
	fi->g.tach_header.r_ctl_and_d_id = htonl(r_ctl | d_id);
	fi->g.tach_header.vc_id_and_s_id = htonl(my_id);
	fi->g.tach_header.type_and_f_cntl = htonl(type);
	fi->g.tach_header.seq_id = seq_id;
	fi->g.tach_header.df_cntl = df_ctl;
	fi->g.tach_header.seq_cnt = 0;
	fi->g.tach_header.ox_id = htons(ox_id);
	fi->g.tach_header.rx_id = htons(rx_id);
	fi->g.tach_header.ro = 0;
	if (data) {
		/* We use the Seq_Count to keep track of IP frames in the
		 * OCI_interrupt handler. Initial Seq_Count of IP frames is 1.
		 */
		if (fi->g.type_of_frame == FC_BROADCAST)
			fi->g.tach_header.seq_cnt = htons(0x1);
		else
			fi->g.tach_header.seq_cnt = htons(0x2);
		fi->g.tach_header.nw_header.d_naa = htons(0x1000);
		fi->g.tach_header.nw_header.s_naa = htons(0x1000);
		memcpy(&(fi->g.tach_header.nw_header.dest_high), data, 2);
		memcpy(&(fi->g.tach_header.nw_header.dest_low), data + 2, 4);
		memcpy(&(fi->g.tach_header.nw_header.source_high), data + 6, 2);
		memcpy(&(fi->g.tach_header.nw_header.source_low), data + 8, 4);
	}
	LEAVE("build_tachyon_header");
}

static void build_EDB(struct fc_info *fi, char *data, u_short flags, u_short len)
{
	fi->g.edb.buf_addr = ntohl((u_int)virt_to_bus(data));
	fi->g.edb.ehf = ntohs(flags);
	if (len % 4)
		len += (4 - (len % 4));
	fi->g.edb.buf_len = ntohs(len);
}

static void build_ODB(struct fc_info *fi, u_char seq_id, u_int d_id, u_int len, u_int cntl, u_short mtu, u_short ox_id, u_short rx_id, int NW_header, int int_required, u_int frame_class)
{
	fi->g.odb.seq_d_id = htonl(seq_id << 24 | d_id);
	fi->g.odb.tot_len = len;
	if (NW_header)
		fi->g.odb.tot_len += NW_HEADER_LEN;
	if (fi->g.odb.tot_len % 4)
		fi->g.odb.tot_len += (4 - (fi->g.odb.tot_len % 4));
	fi->g.odb.tot_len = htonl(fi->g.odb.tot_len);
	switch(int_required) {
		case NO_COMP_AND_INT:
			fi->g.odb.cntl = htons(ODB_CLASS_3 | ODB_EE_CREDIT | ODB_NO_INT | ODB_NO_COMP | cntl);
			break;
		case INT_AND_COMP_REQ:
			fi->g.odb.cntl = htons(ODB_CLASS_3 | ODB_EE_CREDIT | cntl);
			break;
		case NO_INT_COMP_REQ:
			fi->g.odb.cntl = htons(ODB_CLASS_3 | ODB_EE_CREDIT | ODB_NO_INT | cntl);
			break;
	}
	fi->g.odb.rx_id = htons(rx_id);
	fi->g.odb.cs_enable = 0;
	fi->g.odb.cs_seed = htons(1);

	fi->g.odb.hdr_addr = htonl(virt_to_bus(fi->q.ptr_tachyon_header[fi->q.tachyon_header_indx]));
	fi->g.odb.frame_len = htons(mtu);

	if (NW_header) {
		/* The pointer to the sk_buff is in here. Freed up when the
		 * OCI_interrupt is received.
		 */
		fi->g.odb.trans_id = htonl(frame_class);
		fi->g.odb.hdr_len = TACHYON_HEADER_LEN + NW_HEADER_LEN;
	}
	else {
		/* helps in tracking transmitted OX_IDs */
		fi->g.odb.trans_id = htonl((frame_class & 0xFFFF0000) | ox_id);
		fi->g.odb.hdr_len = TACHYON_HEADER_LEN;
	}
	fi->g.odb.hdr_len = htons(fi->g.odb.hdr_len);
		
	fi->g.odb.edb_addr = htonl(virt_to_bus(fi->q.ptr_edb[fi->q.edb_buffer_indx]));
}

static void fill_login_frame(struct fc_info *fi, u_int logi)
{
int i;
	fi->g.login.ls_cmnd_code= htonl(logi);
	fi->g.login.fc_ph_version = htons(PH_VERSION);
	if (fi->g.loop_up)
		fi->g.login.buff_to_buff_credit = htons(LOOP_BB_CREDIT);
	else
	if (fi->g.ptp_up)
		fi->g.login.buff_to_buff_credit = htons(PT2PT_BB_CREDIT);
	if ((logi != ELS_FLOGI) || (logi == ELS_ACC))
		fi->g.login.common_features = htons(PLOGI_C_F);
	else
	if (logi == ELS_FLOGI)
		fi->g.login.common_features = htons(FLOGI_C_F);
	fi->g.login.recv_data_field_size = htons(TACH_FRAME_SIZE);
	fi->g.login.n_port_total_conc_seq = htons(CONCURRENT_SEQUENCES);
	fi->g.login.rel_off_by_info_cat = htons(RO_INFO_CATEGORY);
	fi->g.login.ED_TOV = htonl(E_D_TOV);
	fi->g.login.n_port_name_high = htonl(N_PORT_NAME_HIGH);
	fi->g.login.n_port_name_low = htonl(N_PORT_NAME_LOW);
	fi->g.login.node_name_high = htonl(NODE_NAME_HIGH);
	fi->g.login.node_name_low = htonl(NODE_NAME_LOW);
	
	/* Fill Class 1 parameters */
	fi->g.login.c_of_s[0].service_options = htons(0);
	fi->g.login.c_of_s[0].initiator_ctl = htons(0);
	fi->g.login.c_of_s[0].recipient_ctl = htons(0);
	fi->g.login.c_of_s[0].recv_data_field_size = htons(0);
	fi->g.login.c_of_s[0].concurrent_sequences = htons(0);
	fi->g.login.c_of_s[0].n_port_end_to_end_credit = htons(0);
	fi->g.login.c_of_s[0].open_seq_per_exchange = htons(0);
	fi->g.login.c_of_s[0].resv = htons(0);

	/* Fill Class 2 parameters */
	fi->g.login.c_of_s[1].service_options = htons(0);
	fi->g.login.c_of_s[1].initiator_ctl = htons(0);
	fi->g.login.c_of_s[1].recipient_ctl = htons(0);
	fi->g.login.c_of_s[1].recv_data_field_size = htons(0);
	fi->g.login.c_of_s[1].concurrent_sequences = htons(0);
	fi->g.login.c_of_s[1].n_port_end_to_end_credit = htons(0);
	fi->g.login.c_of_s[1].open_seq_per_exchange = htons(0);
	fi->g.login.c_of_s[1].resv = htons(0);

	/* Fill Class 3 parameters */
	if (logi == ELS_FLOGI)
		fi->g.login.c_of_s[2].service_options  = htons(SERVICE_VALID | SEQUENCE_DELIVERY);
	else
		fi->g.login.c_of_s[2].service_options  = htons(SERVICE_VALID);
	fi->g.login.c_of_s[2].initiator_ctl = htons(0);
	fi->g.login.c_of_s[2].recipient_ctl = htons(0);
	fi->g.login.c_of_s[2].recv_data_field_size = htons(TACH_FRAME_SIZE);
	fi->g.login.c_of_s[2].concurrent_sequences = htons(CLASS3_CONCURRENT_SEQUENCE);
	fi->g.login.c_of_s[2].n_port_end_to_end_credit = htons(0);
	fi->g.login.c_of_s[2].open_seq_per_exchange = htons(CLASS3_OPEN_SEQUENCE);
	fi->g.login.c_of_s[2].resv = htons(0);
	
	for(i = 0; i < 4; i++) {
		fi->g.login.resv[i] = 0;
		fi->g.login.vendor_version_level[i] = 0;
	}
}


/* clear the Interrupt Latch on the (i)chip, so that you can receive 
 * Interrupts from Tachyon in future 
 */
static void reset_latch(struct fc_info *fi)
{
	writel(readl(fi->i_r.ptr_ichip_hw_status_reg) | ICHIP_HSR_INT_LATCH, fi->i_r.ptr_ichip_hw_status_reg);
}

static void update_OCQ_indx(struct fc_info *fi)
{
	fi->q.ocq_prod_indx++;
	if (fi->q.ocq_prod_indx == OCQ_LENGTH)
		fi->q.ocq_prod_indx = 0;
	writel(fi->q.ocq_prod_indx, fi->t_r.ptr_ocq_prod_indx_reg);
}

static void update_IMQ_indx(struct fc_info *fi, int count)
{
	fi->q.imq_cons_indx += count;
	if (fi->q.imq_cons_indx >= IMQ_LENGTH)
		fi->q.imq_cons_indx -= IMQ_LENGTH;
	writel(fi->q.imq_cons_indx, fi->t_r.ptr_imq_cons_indx_reg);
}

static void update_SFSBQ_indx(struct fc_info *fi)
{
	fi->q.sfsbq_prod_indx++;
	if (fi->q.sfsbq_prod_indx == SFSBQ_LENGTH)
		fi->q.sfsbq_prod_indx = 0;
	writel(fi->q.sfsbq_prod_indx, fi->t_r.ptr_sfsbq_prod_reg);
}

static void update_MFSBQ_indx(struct fc_info *fi, int count)
{
	fi->q.mfsbq_prod_indx += count;
	if (fi->q.mfsbq_prod_indx >= MFSBQ_LENGTH)
		fi->q.mfsbq_prod_indx -= MFSBQ_LENGTH;
	writel(fi->q.mfsbq_prod_indx, fi->t_r.ptr_mfsbq_prod_reg);
}


static void update_tachyon_header_indx(struct fc_info *fi)
{
	fi->q.tachyon_header_indx++;
	if (fi->q.tachyon_header_indx == NO_OF_TACH_HEADERS)
		fi->q.tachyon_header_indx = 0;
}

static void update_EDB_indx(struct fc_info *fi)
{
	fi->q.edb_buffer_indx++;
	if (fi->q.edb_buffer_indx == EDB_LEN)
		fi->q.edb_buffer_indx = 0;
}

static int iph5526_open(struct net_device *dev)
{
	netif_start_queue(dev);
	MOD_INC_USE_COUNT;
	return 0;
}

static int iph5526_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	MOD_DEC_USE_COUNT;
	return 0;
}

static void iph5526_timeout(struct net_device *dev)
{
	struct fc_info *fi = (struct fc_info*)dev->priv;
	printk(KERN_WARNING "%s: timed out on send.\n", dev->name);
	fi->fc_stats.rx_dropped++;
	dev->trans_start = jiffies;
	netif_wake_queue(dev);
}

static int iph5526_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct fc_info *fi = (struct fc_info*)dev->priv;
	int status = 0;
	short type = 0;
	u_long flags;
	struct fcllc *fcllc;
	
	ENTER("iph5526_send_packet");
	
	netif_stop_queue(dev);
	/* Strip off the pseudo header.
	 */
	skb->data = skb->data + 2*FC_ALEN; 
	skb->len = skb->len - 2*FC_ALEN;
	fcllc = (struct fcllc *)skb->data;
	type = ntohs(fcllc->ethertype);

	spin_lock_irqsave(&fi->fc_lock, flags);
	switch(type) {
		case ETH_P_IP:
			status = tx_ip_packet(skb, skb->len, fi);
			break;
		case ETH_P_ARP:
			status = tx_arp_packet(skb->data, skb->len, fi);
			break;
		default:
			T_MSG("WARNING!!! Received Unknown Packet Type... Discarding...");
			fi->fc_stats.rx_dropped++;
			break;
	}
	spin_unlock_irqrestore(&fi->fc_lock, flags);

	if (status) {
		fi->fc_stats.tx_bytes += skb->len;
		fi->fc_stats.tx_packets++;
	}
	else
		fi->fc_stats.rx_dropped++;
	dev->trans_start = jiffies;
	/* We free up the IP buffers in the OCI_interrupt handler.
	 * status == 0 implies that the frame was not transmitted. So the
	 * skb is freed here.
	 */
	if ((type == ETH_P_ARP) || (status == 0))
		dev_kfree_skb(skb);
	netif_wake_queue(dev);
	LEAVE("iph5526_send_packet");
	return 0;
}

static int iph5526_change_mtu(struct net_device *dev, int mtu)
{
	return 0;
}

static int tx_ip_packet(struct sk_buff *skb, unsigned long len, struct fc_info *fi)
{
u_int d_id;
int int_required = 1;
u_int r_ctl = FC4_DEVICE_DATA | UNSOLICITED_DATA;
u_int type = TYPE_LLC_SNAP;
u_short ox_id = OX_ID_FIRST_SEQUENCE;
u_int mtu;
struct fc_node_info *q;

	ENTER("tx_ip_packet");
	q = look_up_cache(fi, skb->data - 2*FC_ALEN);
	if (q != NULL) {
		d_id = q->d_id;
		DPRINTK("Look-Up Cache Succeeded for d_id = %x", d_id);
		mtu = q->mtu;
		if (q->login == LOGIN_COMPLETED){
			fi->g.type_of_frame = FC_IP;
			return tx_exchange(fi, skb->data, len, r_ctl, type, d_id, mtu, int_required, ox_id, virt_to_bus(skb));
		}
		
		if (q->d_id == BROADCAST) {
		struct fc_node_info *p = fi->node_info_list;
		int return_value = FALSE;
			fi->g.type_of_frame = FC_BROADCAST;
			/* Do unicast to local nodes.
			 */
			int_required = 0;
			while(p != NULL) {
				d_id = p->d_id;
				if ((d_id & 0xFFFF00) == fi->g.my_ddaa)
					return_value |= tx_exchange(fi, skb->data, len, r_ctl, type, d_id, fi->g.my_mtu, int_required, ox_id, TYPE_LLC_SNAP);
				p = p->next;
			}
			kfree(q);
			return return_value;
		}
		
		if (q->login != LOGIN_COMPLETED) {	
			DPRINTK1("Node not logged in... Txing PLOGI to %x", d_id);
			/* FIXME: we are dumping the frame here */
			tx_logi(fi, ELS_PLOGI, d_id); 
		}
	}
	DPRINTK2("Look-Up Cache Failed");
	LEAVE("tx_ip_packet");
	return 0;
}

static int tx_arp_packet(char *data, unsigned long len, struct fc_info *fi)
{
u_int opcode = data[ARP_OPCODE_0]; 
u_int d_id;
int int_required = 0, return_value = FALSE;
u_int r_ctl = FC4_DEVICE_DATA | UNSOLICITED_DATA;
u_int type = TYPE_LLC_SNAP;
u_short ox_id = OX_ID_FIRST_SEQUENCE;
u_int my_mtu = fi->g.my_mtu;
	ENTER("tx_arp_packet");

	opcode = opcode << 8 | data[ARP_OPCODE_1];
	fi->g.type_of_frame = FC_IP;

	if (opcode == ARPOP_REQUEST) {
	struct fc_node_info *q = fi->node_info_list;
		d_id = BROADCAST;
		return_value |= tx_exchange(fi, data, len, r_ctl, type, d_id, my_mtu, int_required, ox_id, TYPE_LLC_SNAP);
		/* Some devices support HW_TYPE 0x01 */
		memcpy(fi->g.arp_buffer, data - 2*FC_ALEN, len + 2*FC_ALEN);
		fi->g.arp_buffer[9 + 2*FC_ALEN] = 0x01;
		return_value |= tx_exchange(fi, (char *)(fi->g.arp_buffer + 2*FC_ALEN), len, r_ctl, type, d_id, my_mtu, int_required, ox_id, TYPE_LLC_SNAP);

		/* Do unicast to local nodes.
		 */
		while(q != NULL) {
			fi->g.type_of_frame = FC_BROADCAST;
			d_id = q->d_id;
			if ((d_id & 0xFFFF00) == fi->g.my_ddaa) {
				return_value |= tx_exchange(fi, data, len, r_ctl, type, d_id, my_mtu, int_required, ox_id, TYPE_LLC_SNAP);
				// Some devices support HW_TYPE 0x01
				memcpy(fi->g.arp_buffer, data - 2*FC_ALEN, len + 2*FC_ALEN);
				fi->g.arp_buffer[9 + 2*FC_ALEN] = 0x01;
				return_value |= tx_exchange(fi, (char *)(fi->g.arp_buffer + 2*FC_ALEN), len, r_ctl, type, d_id, my_mtu, int_required, ox_id, TYPE_LLC_SNAP);
			}
			q = q->next;
		}
		return return_value;
	}
	else
	if (opcode == ARPOP_REPLY) {
	struct fc_node_info *q; u_int mtu;
		DPRINTK("We are sending out an ARP reply");
		q = look_up_cache(fi, data - 2*FC_ALEN);
		if (q != NULL) {
			d_id = q->d_id;
			DPRINTK("Look-Up Cache Succeeded for d_id = %x", d_id);
			mtu = q->mtu;
			if (q->login == LOGIN_COMPLETED){
				tx_exchange(fi, data, len, r_ctl, type, d_id, mtu, int_required, ox_id, TYPE_LLC_SNAP);
				/* Some devices support HW_TYPE 0x01 */
				memcpy(fi->g.arp_buffer, data - 2*FC_ALEN, len + 2*FC_ALEN);
				fi->g.arp_buffer[9 + 2*FC_ALEN] = 0x01;
				return tx_exchange(fi, (char *)(fi->g.arp_buffer + 2*FC_ALEN), len, r_ctl, type, d_id, my_mtu, int_required, ox_id, TYPE_LLC_SNAP);
			}
			else {
				DPRINTK1("Node not logged in... Txing PLOGI to %x", d_id);
				tx_logi(fi, ELS_PLOGI, d_id); /* FIXME: we are dumping the frame here */
			}
		}
		DPRINTK2("Look-Up Cache Failed");
	}
	else {
		T_MSG("Warning!!! Invalid Opcode in ARP Packet!");
	}
	LEAVE("tx_arp_packet");
	return 0;
}


static void rx_net_packet(struct fc_info *fi, u_char *buff_addr, int payload_size)
{
struct net_device *dev = fi->dev;
struct sk_buff *skb;
u_int skb_size = 0;
struct fch_hdr fch;
	ENTER("rx_net_packet");
	skb_size = payload_size - TACHYON_HEADER_LEN;
	DPRINTK("skb_size = %d", skb_size);
	fi->fc_stats.rx_bytes += skb_size - 2;
	skb = dev_alloc_skb(skb_size);
	if (skb == NULL) {
		printk(KERN_NOTICE "%s: In rx_net_packet() Memory squeeze, dropping packet.\n", dev->name);
		fi->fc_stats.rx_dropped++;
		return;
	}
	/* Skip over the Tachyon Frame Header.
	 */
	buff_addr += TACHYON_HEADER_LEN; 

	memcpy(fch.daddr, buff_addr + 2, FC_ALEN);
	memcpy(fch.saddr, buff_addr + 10, FC_ALEN);
	buff_addr += 2;
	memcpy(buff_addr, fch.daddr, FC_ALEN);
	memcpy(buff_addr + 6, fch.saddr, FC_ALEN);
	skb_reserve(skb, 2);
	memcpy(skb_put(skb, skb_size - 2), buff_addr, skb_size - 2);
	skb->dev = dev;
	skb->protocol = fc_type_trans(skb, dev);
	DPRINTK("protocol = %x", skb->protocol);
	
	/* Hmmm... to accept HW Type 0x01 as well... 
	 */
	if (skb->protocol == ntohs(ETH_P_ARP))
		skb->data[1] = 0x06;
	netif_rx(skb);
	dev->last_rx = jiffies;
	fi->fc_stats.rx_packets++;
	LEAVE("rx_net_packet");
}


static void rx_net_mfs_packet(struct fc_info *fi, struct sk_buff *skb)
{
struct net_device *dev = fi->dev;
struct fch_hdr fch;
	ENTER("rx_net_mfs_packet");
	/* Construct your Hard Header */
	memcpy(fch.daddr, skb->data + 2, FC_ALEN);
	memcpy(fch.saddr, skb->data + 10, FC_ALEN);
	skb_pull(skb, 2);
	memcpy(skb->data, fch.daddr, FC_ALEN);
	memcpy(skb->data + 6, fch.saddr, FC_ALEN);
	skb->dev = dev;
	skb->protocol = fc_type_trans(skb, dev);
	DPRINTK("protocol = %x", skb->protocol);
	netif_rx(skb);
	dev->last_rx = jiffies;
	LEAVE("rx_net_mfs_packet");
}

static int tx_exchange(struct fc_info *fi, char *data, u_int len, u_int r_ctl, u_int type, u_int d_id, u_int mtu, int int_required, u_short tx_ox_id, u_int frame_class)
{
u_char df_ctl; 
int NW_flag = 0, h_size, return_value;
u_short rx_id = RX_ID_FIRST_SEQUENCE;
u_int tachyon_status;
u_int my_id = fi->g.my_id;
	ENTER("tx_exchange");

	tachyon_status = readl(fi->t_r.ptr_tach_status_reg);
	DPRINTK("Tachyon Status = %x len = %d MTU = %d", tachyon_status, len, mtu);
	if (tachyon_status & OSM_FROZEN) {
		reset_tachyon(fi, ERROR_RELEASE);
		reset_tachyon(fi, OCQ_RESET);
		DPRINTK("Tachyon Status = %x len = %d MTU = %d", tachyon_status, len, mtu);
	}
	if (tx_ox_id == OX_ID_FIRST_SEQUENCE) {
		switch(fi->g.type_of_frame) {
			case FC_SCSI_READ:
				tx_ox_id = fi->g.scsi_oxid | SCSI_READ_BIT;
				break;
			case FC_SCSI_WRITE:
				tx_ox_id = fi->g.scsi_oxid;
				break;
			default:
				tx_ox_id = fi->g.ox_id;
				break;
		}
	}
	else {
		switch(fi->g.type_of_frame) {
			case FC_SCSI_READ:
				rx_id = fi->g.scsi_oxid | SCSI_READ_BIT;
				break;
			case FC_SCSI_WRITE:
				rx_id = fi->g.scsi_oxid;
				break;
			case FC_BLS:
				rx_id = RX_ID_FIRST_SEQUENCE;
				break;
			default:
				rx_id = fi->g.ox_id;
				break;
		}
	}

	if (type == TYPE_LLC_SNAP) {
		df_ctl = 0x20;
		NW_flag = 1;
		/* Multi Frame Sequence ? If yes, set RO bit */
		if (len > mtu)
			type |= RELATIVE_OFF_PRESENT;
		build_tachyon_header(fi, my_id, r_ctl, d_id, type, fi->g.seq_id, df_ctl, tx_ox_id, rx_id, data - 2*FC_ALEN);
	}
	else {
		df_ctl = 0;
		/* Multi Frame Sequence ? If yes, set RO bit */
		if (len > mtu)
			type |= RELATIVE_OFF_PRESENT;
		build_tachyon_header(fi, my_id, r_ctl, d_id, type, fi->g.seq_id, df_ctl, tx_ox_id, rx_id, NULL);
	}

	/* Get free Tachyon Headers and EDBs */
	if (get_free_header(fi) || get_free_EDB(fi))
		return 0;

	if ((type & 0xFF000000) == TYPE_LLC_SNAP) {
		h_size =  TACHYON_HEADER_LEN + NW_HEADER_LEN;
		memcpy(fi->q.ptr_tachyon_header[fi->q.tachyon_header_indx], &(fi->g.tach_header), h_size);
	}
	else 
		memcpy(fi->q.ptr_tachyon_header[fi->q.tachyon_header_indx], &(fi->g.tach_header), TACHYON_HEADER_LEN);

	return_value = tx_sequence(fi, data, len, mtu, d_id, tx_ox_id, rx_id, fi->g.seq_id, NW_flag, int_required, frame_class);
	
	switch(fi->g.type_of_frame) {
		case FC_SCSI_READ:
		case FC_SCSI_WRITE:
			update_scsi_oxid(fi);
			break;
		case FC_BLS:
			break;
		default:
			fi->g.ox_id++;
			if (fi->g.ox_id == 0xFFFF)
				fi->g.ox_id = NOT_SCSI_XID;
			break;
	}

	if (fi->g.seq_id == MAX_SEQ_ID)
		fi->g.seq_id = 0;
	else
		fi->g.seq_id++;
	LEAVE("tx_exchange");
	return return_value;
}

static int tx_sequence(struct fc_info *fi, char *data, u_int len, u_int mtu, u_int d_id, u_short ox_id, u_short rx_id, u_char seq_id, int NW_flag, int int_required, u_int frame_class)
{
u_int cntl = 0;
int return_value;
	ENTER("tx_sequence");
	build_EDB(fi, data, EDB_END, len);
	memcpy(fi->q.ptr_edb[fi->q.edb_buffer_indx], &(fi->g.edb), sizeof(EDB));
	build_ODB(fi, seq_id, d_id, len, cntl, mtu, ox_id, rx_id, NW_flag, int_required, frame_class);
	memcpy(fi->q.ptr_odb[fi->q.ocq_prod_indx], &(fi->g.odb), sizeof(ODB));
	if (fi->g.link_up != TRUE) {
		DPRINTK2("Fibre Channel Link not up. Dropping Exchange!");
		return_value = FALSE;
	}
	else {
		/* To be on the safe side, a check should be included
		 * at this point to check if we are overrunning 
		 * Tachyon.
		 */
		update_OCQ_indx(fi);
		return_value = TRUE;
	}
	update_EDB_indx(fi);
	update_tachyon_header_indx(fi);
	LEAVE("tx_sequence");
	return return_value;
}

static int get_free_header(struct fc_info *fi)
{
u_short temp_ox_id;
u_int *tach_header, initial_indx = fi->q.tachyon_header_indx;
	/* Check if the header is in use.
	 * We could have an outstanding command.
	 * We should find a free slot as we can queue a
	 * maximum of 32 SCSI commands only. 
	 */
	tach_header = fi->q.ptr_tachyon_header[fi->q.tachyon_header_indx];
	temp_ox_id = ntohl(*(tach_header + 6)) >> 16;
	/* We care about the SCSI writes only. Those are the wicked ones
	 * that need an additional set of buffers.
	 */
	while(temp_ox_id <= MAX_SCSI_XID) {
		update_tachyon_header_indx(fi);
		if (fi->q.tachyon_header_indx == initial_indx) {
			/* Should never happen.
			 */
			T_MSG("No free Tachyon headers available");
			reset_tachyon(fi, SOFTWARE_RESET);
			return 1;
		}
		tach_header = fi->q.ptr_tachyon_header[fi->q.tachyon_header_indx];
		temp_ox_id = ntohl(*(tach_header + 6)) >> 16;
	}
	return 0;
}

static int get_free_EDB(struct fc_info *fi)
{
unsigned int initial_indx = fi->q.edb_buffer_indx;
	/* Check if the EDB is in use.
	 * We could have an outstanding SCSI Write command.
	 * We should find a free slot as we can queue a
	 * maximum of 32 SCSI commands only. 
	 */
	while (fi->q.free_edb_list[fi->q.edb_buffer_indx] != EDB_FREE) {
		update_EDB_indx(fi);
		if (fi->q.edb_buffer_indx == initial_indx) {
			T_MSG("No free EDB buffers avaliable")
			reset_tachyon(fi, SOFTWARE_RESET);
			return 1;
		}
	}
	return 0;
}		

static int validate_login(struct fc_info *fi, u_int *base_ptr)
{
struct fc_node_info *q = fi->node_info_list;
char n_port_name[PORT_NAME_LEN];
char node_name[NODE_NAME_LEN];
u_int s_id;
	ENTER("validate_login");
	/*index to Port Name in the payload. We need the 8 byte Port Name */
	memcpy(n_port_name, base_ptr + 10, PORT_NAME_LEN);
	memcpy(node_name, base_ptr + 12, NODE_NAME_LEN);
	s_id = ntohl(*(base_ptr + 3)) & 0x00FFFFFF;
	
	/* check if Fibre Channel IDs have changed */
	while(q != NULL) {	
		if (memcmp(n_port_name, q->hw_addr, PORT_NAME_LEN) == 0) {
			if ((s_id != q->d_id) || (memcmp(node_name, q->node_name, NODE_NAME_LEN) != 0)) {
				DPRINTK1("Fibre Channel ID of Node has changed. Txing LOGO.");
				return 0;
			}
			q->login = LOGIN_COMPLETED;
#if DEBUG_5526_2
			display_cache(fi);
#endif
			return 1;
		}
		q = q->next;
	}
	DPRINTK1("Port Name does not match. Txing LOGO.");
	return 0;
	LEAVE("validate_login");
}

static void add_to_address_cache(struct fc_info *fi, u_int *base_ptr)
{
int size = sizeof(struct fc_node_info);
struct fc_node_info *p, *q = fi->node_info_list, *r = NULL;
char n_port_name[PORT_NAME_LEN];
u_int s_id;
	ENTER("add_to_address_cache");
	/*index to Port Name in the payload. We need the 8 byte Port Name */
	memcpy(n_port_name, base_ptr + 13, PORT_NAME_LEN);
	s_id = ntohl(*(base_ptr + 3)) & 0x00FFFFFF;
	
	/* check if info already exists */
	while(q != NULL) {	
		if (memcmp(n_port_name, q->hw_addr, PORT_NAME_LEN) == 0) {
			if (s_id != q->d_id) {
				memcpy(&(q->c_of_s[0]), base_ptr + 17, 3 * sizeof(CLASS_OF_SERVICE));
				q->mtu = ntohl(*(base_ptr + 10)) & 0x00000FFF;
				q->d_id = s_id;
				memcpy(q->node_name, base_ptr + 15, NODE_NAME_LEN);
			}
			q->login = LOGIN_COMPLETED;
			q->scsi = FALSE;
			fi->num_nodes++;
#if DEBUG_5526_2
			display_cache(fi);
#endif
			return;
		}
		r = q;
		q = q->next;
	}
	p = (struct fc_node_info *)kmalloc(size, GFP_ATOMIC);
	if (p == NULL) {
		T_MSG("kmalloc failed in add_to_address_cache()");
		return;
	}
	memcpy(&(p->c_of_s[0]), base_ptr + 17, 3 * sizeof(CLASS_OF_SERVICE));
	p->mtu = ntohl(*(base_ptr + 10)) & 0x00000FFF;
	p->d_id = s_id;
	memcpy(p->hw_addr, base_ptr + 13, PORT_NAME_LEN);
	memcpy(p->node_name, base_ptr + 15, NODE_NAME_LEN);
	p->login = LOGIN_COMPLETED;
	p->scsi = FALSE;
	p->target_id = 0xFF;
	p->next = NULL;
	if (fi->node_info_list == NULL)
		fi->node_info_list = p;
	else
		r->next = p;
	fi->num_nodes++;
#if DEBUG_5526_2
	display_cache(fi);
#endif
	LEAVE("add_to_address_cache");
	return;
}

static void remove_from_address_cache(struct fc_info *fi, u_int *base_ptr, u_int cmnd_code)
{
struct fc_node_info *q = fi->node_info_list;
u_int s_id;
	ENTER("remove_from_address_cache");
	s_id = ntohl(*(base_ptr + 3)) & 0x00FFFFFF;
	switch(cmnd_code) {
		case ELS_LOGO:
			/* check if info exists */
			while (q != NULL) {
				if (s_id == q->d_id) {
					if (q->login == LOGIN_COMPLETED)
						q->login = LOGIN_ATTEMPTED;
					if (fi->num_nodes > 0)
						fi->num_nodes--;
#if DEBUG_5526_2
					display_cache(fi);
#endif
					return;
				}
				q = q->next;
			}
			DPRINTK1("ELS_LOGO received from node 0x%x which is not logged-in", s_id);
			break;
		case ELS_RSCN:
		{
		int payload_len = ntohl(*(base_ptr + 8)) & 0xFF;
		int no_of_pages, i;
		u_char address_format;
		u_short received_ox_id = ntohl(*(base_ptr + 6)) >> 16;
		u_int node_id, mask, *page_ptr = base_ptr + 9;
			if ((payload_len < 4) || (payload_len > 256)) {
				DPRINTK1("RSCN with invalid payload length received");
				tx_ls_rjt(fi, s_id, received_ox_id, LOGICAL_ERR, RECV_FIELD_SIZE);
				return;
			}
			/* Page_size includes the Command Code */
			no_of_pages = (payload_len / 4) - 1;
			for (i = 0; i < no_of_pages; i++) {
				address_format = ntohl(*page_ptr) >> 24; 
				node_id = ntohl(*page_ptr) & 0x00FFFFFF;
				switch(address_format) {
					case PORT_ADDRESS_FORMAT:
						rscn_handler(fi, node_id);
						break;
					case AREA_ADDRESS_FORMAT:
					case DOMAIN_ADDRESS_FORMAT:
						if (address_format == AREA_ADDRESS_FORMAT)
							mask = 0xFFFF00;
						else
							mask = 0xFF0000;
						while(q != NULL) {
							if ((q->d_id & mask) == (node_id & mask)) 
								rscn_handler(fi, q->d_id);
							q = q->next;
						}
						/* There might be some new nodes to be 
						 * discovered. But, some of the earlier 
						 * requests as a result of the RSCN might be 
						 * in progress. We dont want to duplicate that 
						 * effort. So letz call SCR after a lag.
						 */
						fi->explore_timer.function = scr_timer;
						fi->explore_timer.data = (unsigned long)fi;
						fi->explore_timer.expires = RUN_AT((no_of_pages*3*HZ)/100); 
						init_timer(&fi->explore_timer);
						add_timer(&fi->explore_timer);
						break;
					default:
						T_MSG("RSCN with invalid address format received");
						tx_ls_rjt(fi, s_id, received_ox_id, LOGICAL_ERR, NO_EXPLN);
				}
				page_ptr += 1;
			} /* end of for loop */
		} /* end of case RSCN: */	
		break;
	}
#if DEBUG_5526_2
	display_cache(fi);
#endif
	LEAVE("remove_from_address_cache");
}

static void rscn_handler(struct fc_info *fi, u_int node_id)
{
struct fc_node_info *q = fi->node_info_list;
int login_state = sid_logged_in(fi, node_id);
	if ((login_state == NODE_LOGGED_IN) || (login_state == NODE_PROCESS_LOGGED_IN)) {
		while(q != NULL) {
			if (q->d_id == node_id) {
				q->login = LOGIN_ATTEMPTED;
				if (fi->num_nodes > 0)
					fi->num_nodes--;
				break;
			}
			else
				q = q->next;
		}
	}
	else
	if (login_state == NODE_LOGGED_OUT)
		tx_adisc(fi, ELS_ADISC, node_id, OX_ID_FIRST_SEQUENCE); 
	else
	if (login_state == NODE_LOGGED_OUT)
		tx_logi(fi, ELS_PLOGI, node_id);
}

static void scr_timer(unsigned long data)
{
struct fc_info *fi = (struct fc_info *)data;
	del_timer(&fi->explore_timer);
	tx_name_server_req(fi, FCS_GP_ID4);
}

static int sid_logged_in(struct fc_info *fi, u_int s_id)
{
struct fc_node_info *temp = fi->node_info_list;
	while(temp != NULL)
		if ((temp->d_id == s_id) && (temp->login == LOGIN_COMPLETED)) {
			if (temp->scsi != FALSE)
				return NODE_PROCESS_LOGGED_IN;
			else
				return NODE_LOGGED_IN;
		}
		else
		if ((temp->d_id == s_id) && (temp->login != LOGIN_COMPLETED))
			return NODE_LOGGED_OUT;
		else
			temp = temp->next;
	return NODE_NOT_PRESENT;
}

static void mark_scsi_sid(struct fc_info *fi, u_int *buff_addr, u_char action)
{
struct fc_node_info *temp = fi->node_info_list;
u_int s_id;
u_int service_params;
	s_id = ntohl(*(buff_addr + 3)) & 0x00FFFFFF;
	service_params = ntohl(*(buff_addr + 12)) & 0x000000F0;
	while(temp != NULL)
		if ((temp->d_id == s_id) && (temp->login == LOGIN_COMPLETED)) {
			if (action == DELETE_ENTRY) {
				temp->scsi = FALSE;
#if DEBUG_5526_2
				display_cache(fi);
#endif
				return;
			}
			/* Check if it is a SCSI Target */
			if (!(service_params & TARGET_FUNC)) {
				temp->scsi = INITIATOR;	
#if DEBUG_5526_2
				display_cache(fi);
#endif
				return;
			}
			temp->scsi = TARGET;
			/* This helps to maintain the target_id no matter what your
			 *  Fibre Channel ID is.
			 */
			if (temp->target_id == 0xFF) {
				if (fi->g.no_of_targets <= MAX_SCSI_TARGETS)
					temp->target_id = fi->g.no_of_targets++;
				else
					T_MSG("MAX TARGETS reached!");
			}
			else
				DPRINTK1("Target_id %d already present", temp->target_id);
#if DEBUG_5526_2
			display_cache(fi);
#endif
			return;
		}
		else
			temp = temp->next;
	return;
}

static int node_logged_in_prev(struct fc_info *fi, u_int *buff_addr)
{
struct fc_node_info *temp;
u_char *data = (u_char *)buff_addr;
u_int s_id;
char node_name[NODE_NAME_LEN];
	s_id = ntohl(*(buff_addr + 3)) & 0x00FFFFFF;
	memcpy(node_name, buff_addr + 12, NODE_NAME_LEN);
	/* point to port_name in the ADISC payload */
	data += 10 * 4;
	/* point to last 6 bytes of port_name */
	data += 2;
	temp = look_up_cache(fi, data);
	if (temp != NULL) {
		if ((temp->d_id == s_id) && (memcmp(node_name, temp->node_name, NODE_NAME_LEN) == 0)) {
			temp->login = LOGIN_COMPLETED;
#if DEBUG_5526_2
			display_cache(fi);
#endif
			return TRUE;
		}
	}
	return FALSE;
}

static struct fc_node_info *look_up_cache(struct fc_info *fi, char *data)
{
struct fc_node_info *temp_list = fi->node_info_list, *q;
u_char n_port_name[FC_ALEN], temp_addr[FC_ALEN];
	ENTER("look_up_cache");
	memcpy(n_port_name, data, FC_ALEN);
	while(temp_list) {
		if (memcmp(n_port_name, &(temp_list->hw_addr[2]), FC_ALEN) == 0)
			return temp_list;
		else
			temp_list = temp_list->next;
	}
	
	/* Broadcast IP ?
	 */
	temp_addr[0] = temp_addr[1] = temp_addr[2] = 0xFF;
	temp_addr[3] = temp_addr[4] = temp_addr[5] = 0xFF;
	if (memcmp(n_port_name, temp_addr, FC_ALEN) == 0) {
		q = (struct fc_node_info *)kmalloc(sizeof(struct fc_node_info), GFP_ATOMIC);
		if (q == NULL) {
			T_MSG("kmalloc failed in look_up_cache()");
			return NULL;
		}
		q->d_id = BROADCAST;
		return q;
	}
	LEAVE("look_up_cache");
	return NULL;
}

static int display_cache(struct fc_info *fi)
{
struct fc_node_info *q = fi->node_info_list;
#if DEBUG_5526_2
struct ox_id_els_map *temp_ox_id_list = fi->ox_id_list;
#endif
int count = 0, j;
	printk("\nFibre Channel Node Information for %s\n", fi->name);
	printk("My FC_ID = %x, My WWN = %x %x, ", fi->g.my_id, fi->g.my_node_name_high, fi->g.my_node_name_low);
	if (fi->g.ptp_up == TRUE)
		printk("Port_Type = N_Port\n");
	if (fi->g.loop_up == TRUE)
		printk("Port_Type = L_Port\n");
	while(q != NULL) {
		printk("WWN = ");
		for (j = 0; j < PORT_NAME_LEN; j++)
			printk("%x ", q->hw_addr[j]); 
		printk("FC_ID = %x, ", q->d_id);
		printk("Login = ");
		if (q->login == LOGIN_COMPLETED)
			printk("ON ");
		else
			printk("OFF ");
		if (q->scsi == TARGET)
			printk("Target_ID = %d ", q->target_id);
		printk("\n");
		q = q->next;
		count++;
	}

#if DEBUG_5526_2
	printk("OX_ID -> ELS Map\n");
	while(temp_ox_id_list) {
			printk("ox_id = %x, ELS = %x\n", temp_ox_id_list->ox_id, temp_ox_id_list->els);
			temp_ox_id_list = temp_ox_id_list->next;
	}
#endif

	return 0;
}

static struct net_device_stats * iph5526_get_stats(struct net_device *dev)
{	
struct fc_info *fi = (struct fc_info*)dev->priv; 
	return (struct net_device_stats *) &fi->fc_stats;
}


/* SCSI stuff starts here */

int iph5526_detect(Scsi_Host_Template *tmpt)
{
struct Scsi_Host *host = NULL;
struct iph5526_hostdata *hostdata;
struct fc_info *fi = NULL;
int no_of_hosts = 0, timeout, i, j, count = 0;
u_int pci_maddr = 0;
struct pci_dev *pdev = NULL;

	tmpt->proc_name = "iph5526";
	if (pci_present() == 0) {
		printk("iph5526: PCI not present\n");
		return 0;
	}

	for (i = 0; i <= MAX_FC_CARDS; i++) 
		fc[i] = NULL;

	for (i = 0; clone_list[i].vendor_id != 0; i++)
	while ((pdev = pci_find_device(clone_list[i].vendor_id, clone_list[i].device_id, pdev))) {
		unsigned short pci_command;
		if (pci_enable_device(pdev))
			continue;
		if (count < MAX_FC_CARDS) {
			fc[count] = kmalloc(sizeof(struct fc_info), GFP_ATOMIC);
			if (fc[count] == NULL) {
				printk("iph5526.c: Unable to register card # %d\n", count + 1);
				return no_of_hosts;
			}
			memset(fc[count], 0, sizeof(struct fc_info));
		}
		else {
			printk("iph5526.c: Maximum Number of cards reached.\n");
			return no_of_hosts;
		}
			
		fi = fc[count];
		sprintf(fi->name, "fc%d", count);

		host = scsi_register(tmpt, sizeof(struct iph5526_hostdata));
		if(host==NULL)
			return no_of_hosts;
			
		hostdata = (struct iph5526_hostdata *)host->hostdata;
		memset(hostdata, 0 , sizeof(struct iph5526_hostdata));
		for (j = 0; j < MAX_SCSI_TARGETS; j++)
			hostdata->tag_ages[j] = jiffies;
		hostdata->fi = fi;
		fi->host = host;
		//host->max_id = MAX_SCSI_TARGETS;
		host->max_id = 5;
		host->hostt->use_new_eh_code = 1;
		host->this_id = tmpt->this_id;

		pci_maddr = pci_resource_start(pdev, 0);
		if (pci_resource_flags(pdev, 0) & IORESOURCE_IO) {
			printk("iph5526.c : Cannot find proper PCI device base address.\n");
			scsi_unregister(host);
			kfree(fc[count]);
			fc[count] = NULL;
			continue;
		}
		
		DPRINTK("pci_maddr = %x", pci_maddr);
		pci_read_config_word(pdev, PCI_COMMAND, &pci_command);
			
		pci_irq_line = pdev->irq;
		printk("iph5526.c: PCI BIOS reports %s at i/o %#x, irq %d.\n", clone_list[i].name, pci_maddr, pci_irq_line);
		fi->g.mem_base = ioremap(pci_maddr & PAGE_MASK, 1024);
		
		/* We use Memory Mapped IO. The initial space contains the
		 * PCI Configuration registers followed by the (i) chip
		 * registers followed by the Tachyon registers.
		 */
		/* Thatz where (i)chip maps Tachyon Address Space.
		 */
		fi->g.tachyon_base = (u_long)fi->g.mem_base + TACHYON_OFFSET + ( pci_maddr & ~PAGE_MASK );
		DPRINTK("fi->g.tachyon_base = %x", (u_int)fi->g.tachyon_base);
		if (fi->g.mem_base == NULL) {
			printk("iph5526.c : ioremap failed!!!\n");
			scsi_unregister(host);
			kfree(fc[count]);
			fc[count] = NULL;
			continue;
		}	
		DPRINTK("IRQ1 = %d\n", pci_irq_line);
		printk(version);
		fi->base_addr = (long) pdev;

		if (pci_irq_line) {
		int irqval = 0;
			/* Found it, get IRQ.
			 */
			irqval = request_irq(pci_irq_line, &tachyon_interrupt, pci_irq_line ? SA_SHIRQ : 0, fi->name, host);
			if (irqval) {
				printk("iph5526.c : Unable to get IRQ %d (irqval = %d).\n", pci_irq_line, irqval);
				scsi_unregister(host);
				kfree(fc[count]);
				fc[count] = NULL;
				continue;
			}
			host->irq = fi->irq = pci_irq_line;
			pci_irq_line = 0;
			fi->clone_id = clone_list[i].vendor_id;
		}

		if (!initialize_register_pointers(fi) || !tachyon_init(fi)) {
			printk("iph5526.c: TACHYON initialization failed for card # %d!!!\n", count + 1);
			free_irq(host->irq, host);
			scsi_unregister(host);
			if (fi) 
				clean_up_memory(fi);
			kfree(fc[count]);
			fc[count] = NULL;
			break;
		}
		DPRINTK1("Fibre Channel card initialized");
		/* Wait for the Link to come up and the login process 
		 * to complete. 
		 */
		for(timeout = jiffies + 10*HZ; time_before(jiffies, timeout) && ((fi->g.link_up == FALSE) || (fi->g.port_discovery == TRUE) || (fi->g.explore_fabric == TRUE) || (fi->g.perform_adisc == TRUE));)
		{
			cpu_relax();
			barrier();
		}
		
		count++;
		no_of_hosts++;
	}
	DPRINTK1("no_of_hosts = %d",no_of_hosts);
	
	/* This is to make sure that the ACC to the PRLI comes in 
	 * for the last ALPA. 
	 */
	mdelay(1000); /* Ugly! Let the Gods forgive me */

	DPRINTK1("leaving iph5526_detect\n");
	return no_of_hosts;
}


int iph5526_biosparam(Disk * disk, kdev_t n, int ip[])
{
int size = disk->capacity;
	ip[0] = 64;
	ip[1] = 32;
	ip[2] = size >> 11;
	if (ip[2] > 1024) {
		ip[0] = 255;
		ip[1] = 63;
		ip[2] = size / (ip[0] * ip[1]);
	}
	return 0;
}

int iph5526_queuecommand(Scsi_Cmnd *Cmnd, void (*done) (Scsi_Cmnd *))
{
int int_required = 0;
u_int r_ctl = FC4_DEVICE_DATA | UNSOLICITED_COMMAND;
u_int type = TYPE_FCP | SEQUENCE_INITIATIVE;
u_int frame_class = Cmnd->target;
u_short ox_id = OX_ID_FIRST_SEQUENCE;
struct Scsi_Host *host = Cmnd->host;
struct iph5526_hostdata *hostdata = (struct iph5526_hostdata*)host->hostdata;
struct fc_info *fi = hostdata->fi;
struct fc_node_info *q;
u_long flags;
	ENTER("iph5526_queuecommand");

	spin_lock_irqsave(&fi->fc_lock, flags);
	Cmnd->scsi_done = done;

	if (Cmnd->device->tagged_supported) {
		switch(Cmnd->tag) {
			case SIMPLE_QUEUE_TAG:
				hostdata->cmnd.fcp_cntl = FCP_CNTL_QTYPE_SIMPLE;
				break;
			case HEAD_OF_QUEUE_TAG:
				hostdata->cmnd.fcp_cntl = FCP_CNTL_QTYPE_HEAD_OF_Q;
				break;
			case  ORDERED_QUEUE_TAG:
				hostdata->cmnd.fcp_cntl = FCP_CNTL_QTYPE_ORDERED;
				break;
			default:
				if ((jiffies - hostdata->tag_ages[Cmnd->target]) > (5 * HZ)) {
					hostdata->cmnd.fcp_cntl = FCP_CNTL_QTYPE_ORDERED;
					hostdata->tag_ages[Cmnd->target] = jiffies;
				}
				else
					hostdata->cmnd.fcp_cntl = FCP_CNTL_QTYPE_SIMPLE;
				break;
		}
	}
	/*else
		hostdata->cmnd.fcp_cntl = FCP_CNTL_QTYPE_UNTAGGED;
	*/

	hostdata->cmnd.fcp_addr[3] = 0;
	hostdata->cmnd.fcp_addr[2] = 0;
	hostdata->cmnd.fcp_addr[1] = 0;
	hostdata->cmnd.fcp_addr[0] = htons(Cmnd->lun);

	memcpy(&hostdata->cmnd.fcp_cdb, Cmnd->cmnd, Cmnd->cmd_len);
	hostdata->cmnd.fcp_data_len = htonl(Cmnd->request_bufflen);

	/* Get an used OX_ID. We could have pending commands.
	 */
	if (get_scsi_oxid(fi)) {
		spin_unlock_irqrestore(&fi->fc_lock, flags);
		return 1;
	}
	fi->q.free_scsi_oxid[fi->g.scsi_oxid] = OXID_INUSE;	

	/* Maintain a handler so that we can associate the done() function
	 * on completion of the SCSI command. 
	 */
	hostdata->cmnd_handler[fi->g.scsi_oxid] = Cmnd;

	switch(Cmnd->cmnd[0]) {
		case WRITE_6:
		case WRITE_10:
		case WRITE_12:
			fi->g.type_of_frame = FC_SCSI_WRITE;
			hostdata->cmnd.fcp_cntl = htonl(FCP_CNTL_WRITE | hostdata->cmnd.fcp_cntl);
			break;
		default:
			fi->g.type_of_frame = FC_SCSI_READ;
			hostdata->cmnd.fcp_cntl = htonl(FCP_CNTL_READ | hostdata->cmnd.fcp_cntl);
	}
	
	memcpy(fi->q.ptr_fcp_cmnd[fi->q.fcp_cmnd_indx], &(hostdata->cmnd), sizeof(fcp_cmd));	
	
	q = resolve_target(fi, Cmnd->target);

	if (q == NULL) {
	u_int bad_id = fi->g.my_ddaa | 0xFE;
		/* We transmit to an non-existant AL_PA so that the "done" 
		 * function can be called while receiving the interrupt 
		 * due to a Timeout for a bad AL_PA. In a PTP configuration,
		 * the int_required field is set, since there is no notion
		 * of AL_PAs. This approach sucks, but works alright!
		 */
		if (fi->g.ptp_up == TRUE)
			int_required = 1;
		tx_exchange(fi, (char *)(&(hostdata->cmnd)), sizeof(fcp_cmd), r_ctl, type, bad_id, fi->g.my_mtu, int_required, ox_id, FC_SCSI_BAD_TARGET);
		spin_unlock_irqrestore(&fi->fc_lock, flags);
		DPRINTK1("Target ID %x not present", Cmnd->target);
		return 0;
	}
	if (q->login == LOGIN_COMPLETED) {
		if (add_to_sest(fi, Cmnd, q)) {
			DPRINTK1("add_to_sest() failed.");
			spin_unlock_irqrestore(&fi->fc_lock, flags);
			return 0;
		}
		tx_exchange(fi, (char *)(fi->q.ptr_fcp_cmnd[fi->q.fcp_cmnd_indx]), sizeof(fcp_cmd), r_ctl, type, q->d_id, q->mtu, int_required, ox_id, frame_class << 16);
		update_FCP_CMND_indx(fi);
	}
	spin_unlock_irqrestore(&fi->fc_lock, flags);
	/* If q != NULL, then we have a SCSI Target. 
	 * If q->login != LOGIN_COMPLETED, then that device could be 
	 * offline temporarily. So we let the command to time-out. 
	 */
	LEAVE("iph5526_queuecommand");
	return 0;
}

int iph5526_abort(Scsi_Cmnd *Cmnd)
{
struct Scsi_Host *host = Cmnd->host;
struct iph5526_hostdata *hostdata = (struct iph5526_hostdata *)host->hostdata;
struct fc_info *fi = hostdata->fi;
struct fc_node_info *q;
u_int r_ctl = FC4_DEVICE_DATA | UNSOLICITED_COMMAND;
u_int type = TYPE_FCP | SEQUENCE_INITIATIVE;
u_short ox_id = OX_ID_FIRST_SEQUENCE;
int int_required = 1, i, abort_status = FALSE;
u_long flags;
	
	ENTER("iph5526_abort");
	
	spin_lock_irqsave(&fi->fc_lock, flags);
	
	q = resolve_target(fi, Cmnd->target);
	if (q == NULL) {
	u_int bad_id = fi->g.my_ddaa | 0xFE;
		/* This should not happen as we should always be able to
		 * resolve a target id. But, jus in case...
		 * We transmit to an non-existant AL_PA so that the done 
		 * function can be called while receiving the interrupt 
		 * for a bad AL_PA. 
		 */
		DPRINTK1("Unresolved Target ID!");
		tx_exchange(fi, (char *)(&(hostdata->cmnd)), sizeof(fcp_cmd), r_ctl, type, bad_id, fi->g.my_mtu, int_required, ox_id, FC_SCSI_BAD_TARGET);
		DPRINTK1("Target ID %x not present", Cmnd->target);
		spin_unlock_irqrestore(&fi->fc_lock, flags);
		return FAILED;
	}

	/* If q != NULL, then we have a SCSI Target. If 
	 * q->login != LOGIN_COMPLETED, then that device could 
	 * be offline temporarily. So we let the command to time-out. 
	 */

	/* Get the OX_ID for the Command to be aborted.
	 */
	for (i = 0; i <= MAX_SCSI_XID; i++) {
		if (hostdata->cmnd_handler[i] == Cmnd) {
			hostdata->cmnd_handler[i] = NULL;
			ox_id = i;
			break;
		}
	}
	if (i > MAX_SCSI_XID) {
		T_MSG("Command could not be resolved to OX_ID");
		spin_unlock_irqrestore(&fi->fc_lock, flags);
		return FAILED;
	}

	switch(Cmnd->cmnd[0]) {
		case WRITE_6:
		case WRITE_10:
		case WRITE_12:
			break;
		default:
			ox_id |= SCSI_READ_BIT;
	}
	abort_status = abort_exchange(fi, ox_id);
	
	if ((q->login == LOGIN_COMPLETED) && (abort_status == TRUE)) {
		/* Then, transmit an ABTS to the target. The rest 
		 * is done when the BA_ACC is received for the ABTS.
 	 	 */
		tx_abts(fi, q->d_id, ox_id);
	}
	else {
	u_int STE_bit;
	u_short x_id;
		/* Invalidate resources for that Exchange.
		 */
		x_id = ox_id & MAX_SCSI_XID;
		STE_bit = ntohl(*fi->q.ptr_sest[x_id]);
		if (STE_bit & SEST_V) {
			*(fi->q.ptr_sest[x_id]) &= htonl(SEST_INV);
			invalidate_SEST_entry(fi, ox_id);
		}
	}

	LEAVE("iph5526_abort");
	spin_unlock_irqrestore(&fi->fc_lock, flags);
	return SUCCESS;
}

static int abort_exchange(struct fc_info *fi, u_short ox_id)
{
u_short x_id;
volatile u_int flush_SEST, STE_bit;
	x_id = ox_id & MAX_SCSI_XID;
	DPRINTK1("Aborting Exchange %x", ox_id);

	STE_bit = ntohl(*fi->q.ptr_sest[x_id]);
	/* Is the Exchange still active?.
	 */
	if (STE_bit & SEST_V) {
		if (ox_id & SCSI_READ_BIT) {
			/* If the Exchange to be aborted is Inbound, 
			 * Flush the SEST Entry from Tachyon's Cache.
			 */
			*(fi->q.ptr_sest[x_id]) &= htonl(SEST_INV);
			flush_tachyon_cache(fi, ox_id);
			flush_SEST = readl(fi->t_r.ptr_tach_flush_oxid_reg);
			while ((flush_SEST & 0x80000000) != 0) 
				flush_SEST = readl(fi->t_r.ptr_tach_flush_oxid_reg);
			STE_bit = ntohl(*fi->q.ptr_sest[x_id]);
			while ((STE_bit & 0x80000000) != 0)
				STE_bit = ntohl(*fi->q.ptr_sest[x_id]);
			flush_SEST = readl(fi->t_r.ptr_tach_flush_oxid_reg);
			invalidate_SEST_entry(fi, ox_id);
		}
		else {
		int i;
		u_int *ptr_edb;
			/* For In-Order Reassembly, the following is done:
			 * First, write zero as the buffer length in the EDB. 
		 	 */
			ptr_edb = bus_to_virt(ntohl(*(fi->q.ptr_sest[x_id] + 7)));
			for (i = 0; i < EDB_LEN; i++)
				if (fi->q.ptr_edb[i] == ptr_edb)
					break;
			if (i < EDB_LEN) 
				*ptr_edb = *ptr_edb & 0x0000FFFF;
			else
				T_MSG("EDB not found while clearing in abort_exchange()");
		}
		DPRINTK1("Exchange %x invalidated", ox_id);
		return TRUE;
	}
	else {
		DPRINTK1("SEST Entry for exchange %x not valid", ox_id);
		return FALSE;
	}	
}

static void flush_tachyon_cache(struct fc_info *fi, u_short ox_id)
{
volatile u_int tachyon_status;
	if (fi->g.loop_up == TRUE) {
		writel(HOST_CONTROL, fi->t_r.ptr_fm_control_reg);
		/* Make sure that the Inbound FIFO is empty.
		 */
		do {
			tachyon_status = readl(fi->t_r.ptr_tach_status_reg);
			udelay(200);
		}while ((tachyon_status & RECEIVE_FIFO_EMPTY) == 0);
		/* Ok. Go ahead and flushhhhhhhhh!
		 */
		writel(0x80000000 | ox_id, fi->t_r.ptr_tach_flush_oxid_reg);
		writel(EXIT_HOST_CONTROL, fi->t_r.ptr_fm_control_reg);
		return;
	}
	if (fi->g.ptp_up == TRUE) {
		take_tachyon_offline(fi);
		/* Make sure that the Inbound FIFO is empty.
		 */
		do {
			tachyon_status = readl(fi->t_r.ptr_tach_status_reg);
			udelay(200);
		}while ((tachyon_status & RECEIVE_FIFO_EMPTY) == 0);
		writel(0x80000000 | ox_id, fi->t_r.ptr_tach_flush_oxid_reg);
		/* Write the Initialize command to the FM Control reg.
		 */
		fi->g.n_port_try = TRUE;
		DPRINTK1("In abort_exchange, TACHYON initializing as N_Port...\n");
		writel(INITIALIZE, fi->t_r.ptr_fm_control_reg);
	}
}

static struct fc_node_info *resolve_target(struct fc_info *fi, u_char target)
{
struct fc_node_info *temp = fi->node_info_list;
	while(temp != NULL)
		if (temp->target_id == target) {
			if ((temp->scsi == TARGET) && (temp->login == LOGIN_COMPLETED))
				return temp;
			else {
				if (temp->login != LOGIN_COMPLETED) {
					/* The Target is not currently logged in.
					 * It could be a Target on the Local Loop or
					 * on a Remote Loop connected through a switch.
					 * In either case, we will know whenever the Target
					 * comes On-Line again. We let the command to 
					 * time-out so that it gets retried.
					 */
					T_MSG("Target %d not logged in.", temp->target_id);
					tx_logi(fi, ELS_PLOGI, temp->d_id);
					return temp;
				}
				else {
					if (temp->scsi != TARGET) {
						/* For some reason, we did not get a response to
						 * PRLI. Letz try it again...
						 */
						DPRINTK1("Node not PRLIied. Txing PRLI...");
						tx_prli(fi, ELS_PRLI, temp->d_id, OX_ID_FIRST_SEQUENCE);
					}
				}
				return temp;
			}
		}
		else
			temp = temp->next;
	return NULL;
}

static int add_to_sest(struct fc_info *fi, Scsi_Cmnd *Cmnd, struct fc_node_info *ni)
{
/* we have at least 1 buffer, the terminator */
int no_of_sdb_buffers = 1, i; 
int no_of_edb_buffers = 0; 
u_int *req_buffer = (u_int *)Cmnd->request_buffer;
u_int *ptr_sdb = NULL;
struct scatterlist *sl1, *sl2 = NULL;
int no_of_sg = 0;

	switch(fi->g.type_of_frame) {
		case FC_SCSI_READ:
			fi->g.inb_sest_entry.flags_and_byte_offset = htonl(INB_SEST_VED);
			fi->g.inb_sest_entry.byte_count = 0;
			fi->g.inb_sest_entry.no_of_recvd_frames = 0;
			fi->g.inb_sest_entry.no_of_expected_frames = 0;
			fi->g.inb_sest_entry.last_fctl = 0;

			if (Cmnd->use_sg) {
				no_of_sg = Cmnd->use_sg;
				sl1 = sl2 = (struct scatterlist *)Cmnd->request_buffer;
				for (i = 0; i < no_of_sg; i++) {
					no_of_sdb_buffers += sl1->length / SEST_BUFFER_SIZE;
					if (sl1->length % SEST_BUFFER_SIZE)
						no_of_sdb_buffers++;
					sl1++;
				}
			}
			else {
				no_of_sdb_buffers += Cmnd->request_bufflen / SEST_BUFFER_SIZE;
				if (Cmnd->request_bufflen % SEST_BUFFER_SIZE)
					no_of_sdb_buffers++;
			} /* if !use_sg */

			/* We are working with the premise that at the max we would
			 * get a scatter-gather buffer containing 63 buffers
			 * of size 1024 bytes each. Is it a _bad_ assumption?
			 */
			if (no_of_sdb_buffers > 512) {
				T_MSG("Number of SDB buffers needed = %d", no_of_sdb_buffers);
				T_MSG("Disable Scatter-Gather!!!");
				return 1;
			}
				

			/* Store it in the sdb_table so that we can retrieve that
			 * free up the memory when the Read Command completes.
			 */
			if (get_free_SDB(fi))
				return 1;
			ptr_sdb = fi->q.ptr_sdb_slot[fi->q.sdb_indx];
			fi->q.sdb_slot_status[fi->q.sdb_indx] = SDB_BUSY;
			fi->g.inb_sest_entry.sdb_address = htonl(virt_to_bus(ptr_sdb));

			if (Cmnd->use_sg) {
			int count = 0, j;
				for(i = 0; i < no_of_sg; i++) {
				char *addr_ptr = sl2->address;
					count = sl2->length / SEST_BUFFER_SIZE;
					if (sl2->length % SEST_BUFFER_SIZE)
						count++;
					for (j = 0; j < count; j++) {
						*(ptr_sdb) = htonl(virt_to_bus(addr_ptr));
						addr_ptr += SEST_BUFFER_SIZE;
						ptr_sdb++;
					}
					count = 0;
					sl2++;
				}
			}
			else {
				for (i = 0; i < no_of_sdb_buffers - 1; i++) {
					*(ptr_sdb) = htonl(virt_to_bus(req_buffer));
					req_buffer += SEST_BUFFER_SIZE/4;
					ptr_sdb++;
				}
			}
			*(ptr_sdb) = htonl(0x1); /* Terminator */
			
			/* The scratch pad is used to hold the index into the SDB.
			 */
			fi->g.inb_sest_entry.scratch_pad = fi->q.sdb_indx;
			fi->g.inb_sest_entry.expected_ro = 0;
			fi->g.inb_sest_entry.buffer_index = 0;
			fi->g.inb_sest_entry.buffer_offset = 0;
			memcpy(fi->q.ptr_sest[fi->g.scsi_oxid], &fi->g.inb_sest_entry, sizeof(INB_SEST_ENTRY));
			break;
		case FC_SCSI_WRITE:
			fi->g.outb_sest_entry.flags_and_did = htonl(OUTB_SEST_VED | ni->d_id);
			fi->g.outb_sest_entry.max_frame_len = htons(ni->mtu << 4);
			fi->g.outb_sest_entry.cntl = htons(ODB_CLASS_3 | ODB_EE_CREDIT | ODB_NO_INT | ODB_NO_COMP);
			fi->g.outb_sest_entry.total_seq_length = INV_SEQ_LEN;
			fi->g.outb_sest_entry.link = htons(OUTB_SEST_LINK);
			fi->g.outb_sest_entry.transaction_id = htonl(fi->g.scsi_oxid);
			fi->g.outb_sest_entry.seq_id = fi->g.seq_id;
			fi->g.outb_sest_entry.reserved = 0x0;
			fi->g.outb_sest_entry.header_length = htons(TACHYON_HEADER_LEN);
		
			{
			u_char df_ctl = 0;
			u_short rx_id = RX_ID_FIRST_SEQUENCE;
			u_int r_ctl = FC4_DEVICE_DATA | SOLICITED_DATA;
			u_int type = TYPE_FCP | SEQUENCE_INITIATIVE;
				/* Multi Frame Sequence ? If yes, set RO bit. 
				 */
				if (Cmnd->request_bufflen > ni->mtu)
					type |= RELATIVE_OFF_PRESENT;
				build_tachyon_header(fi, fi->g.my_id, r_ctl, ni->d_id, type, fi->g.seq_id, df_ctl, fi->g.scsi_oxid, rx_id, NULL);
				if (get_free_header(fi) || get_free_EDB(fi))
					return 1;
				memcpy(fi->q.ptr_tachyon_header[fi->q.tachyon_header_indx], &(fi->g.tach_header), TACHYON_HEADER_LEN);
				fi->g.outb_sest_entry.header_address = htonl(virt_to_bus(fi->q.ptr_tachyon_header[fi->q.tachyon_header_indx]));
				update_tachyon_header_indx(fi);
			}

			if (Cmnd->use_sg) {
				no_of_sg = Cmnd->use_sg;
				sl1 = sl2 = (struct scatterlist *)Cmnd->request_buffer;
				for (i = 0; i < no_of_sg; i++) {
					no_of_edb_buffers += sl1->length / SEST_BUFFER_SIZE;
					if (sl1->length % SEST_BUFFER_SIZE)
						no_of_edb_buffers++;
					sl1++;
				}
			}
			else {
				no_of_edb_buffers += Cmnd->request_bufflen / SEST_BUFFER_SIZE;
				if (Cmnd->request_bufflen % SEST_BUFFER_SIZE)
					no_of_edb_buffers++;
			} /* if !use_sg */


			/* We need "no_of_edb_buffers" _contiguous_ EDBs 
			 * that are FREE. Check for that first.
			 */
			for (i = 0; i < no_of_edb_buffers; i++) {
			int j;
				if ((fi->q.edb_buffer_indx + no_of_edb_buffers) >= EDB_LEN)
					fi->q.edb_buffer_indx = 0;
				if (fi->q.free_edb_list[fi->q.edb_buffer_indx + i] != EDB_FREE) {
					for (j = 0; j < i; j++)
						update_EDB_indx(fi);
					if (get_free_EDB(fi))
						return 1;
					i = 0;
				}
			}

			/* We got enuff FREE EDBs.
			 */
			if (Cmnd->use_sg) {
				fi->g.outb_sest_entry.edb_address = htonl(virt_to_bus(fi->q.ptr_edb[fi->q.edb_buffer_indx]));
				sl1 = (struct scatterlist *)Cmnd->request_buffer;
				for(i = 0; i < no_of_sg; i++) {
				int count = 0, j;
					count = sl1->length / SEST_BUFFER_SIZE;
					for (j = 0; j < count; j++) {
						build_EDB(fi, (char *)sl1->address, 0, SEST_BUFFER_SIZE);
						memcpy(fi->q.ptr_edb[fi->q.edb_buffer_indx], &(fi->g.edb), sizeof(EDB));
						/* Mark this EDB as being in use */
						fi->q.free_edb_list[fi->q.edb_buffer_indx] = EDB_BUSY;
						/* We have already made sure that we have enuff
				 	 	 * free EDBs that are contiguous. So this is 
						 * safe.
				 	 	 */
						update_EDB_indx(fi);
						sl1->address += SEST_BUFFER_SIZE;
					}
					/* Just in case itz not a multiple of 
					 * SEST_BUFFER_SIZE bytes.
					 */
					if (sl1->length % SEST_BUFFER_SIZE) {
						build_EDB(fi, (char *)sl1->address, 0, sl1->length % SEST_BUFFER_SIZE);
						memcpy(fi->q.ptr_edb[fi->q.edb_buffer_indx], &(fi->g.edb), sizeof(EDB));
						fi->q.free_edb_list[fi->q.edb_buffer_indx] = EDB_BUSY;
						update_EDB_indx(fi);
					}
					sl1++;
				}
				/* The last EDB is special. It needs the "end bit" to
				 * be set.
				 */
				*(fi->q.ptr_edb[fi->q.edb_buffer_indx - 1] + 1) = *(fi->q.ptr_edb[fi->q.edb_buffer_indx - 1] + 1) | ntohs(EDB_END);
			}
			else {
			int count = 0, j;
				fi->g.outb_sest_entry.edb_address = htonl(virt_to_bus(fi->q.ptr_edb[fi->q.edb_buffer_indx]));
				count = Cmnd->request_bufflen / SEST_BUFFER_SIZE;
				for (j = 0; j < count; j++) {
					build_EDB(fi, (char *)req_buffer, 0, SEST_BUFFER_SIZE);
					memcpy(fi->q.ptr_edb[fi->q.edb_buffer_indx], &(fi->g.edb), sizeof(EDB));
					/* Mark this EDB as being in use */
					fi->q.free_edb_list[fi->q.edb_buffer_indx] = EDB_BUSY;
					/* We have already made sure that we have enuff
			 	 	 * free EDBs that are contiguous. So this is 
					 * safe.
			 	 	 */
					update_EDB_indx(fi);
					req_buffer += SEST_BUFFER_SIZE;
				}
				/* Just in case itz not a multiple of 
				 * SEST_BUFFER_SIZE bytes.
				 */
				if (Cmnd->request_bufflen % SEST_BUFFER_SIZE) {
					build_EDB(fi, (char *)req_buffer, EDB_END, Cmnd->request_bufflen % SEST_BUFFER_SIZE);
					memcpy(fi->q.ptr_edb[fi->q.edb_buffer_indx], &(fi->g.edb), sizeof(EDB));
					fi->q.free_edb_list[fi->q.edb_buffer_indx] = EDB_BUSY;
					update_EDB_indx(fi);
				}
				else {
					/* Mark the last EDB as the "end edb".
					 */
					*(fi->q.ptr_edb[fi->q.edb_buffer_indx - 1] + 1) = *(fi->q.ptr_edb[fi->q.edb_buffer_indx - 1] + 1) | htons(EDB_END);
				}
			}

			/* Finally we have something to send!.
			 */
			memcpy(fi->q.ptr_sest[fi->g.scsi_oxid], &fi->g.outb_sest_entry, sizeof(OUTB_SEST_ENTRY));
			break;
		}		
	return 0;
}

static void update_FCP_CMND_indx(struct fc_info *fi)
{
	fi->q.fcp_cmnd_indx++;
	if (fi->q.fcp_cmnd_indx == NO_OF_FCP_CMNDS)
		fi->q.fcp_cmnd_indx = 0;
}

static int get_scsi_oxid(struct fc_info *fi)
{
u_short initial_oxid = fi->g.scsi_oxid;
	/* Check if the OX_ID is in use.
	 * We could have an outstanding SCSI command.
	 */
	while (fi->q.free_scsi_oxid[fi->g.scsi_oxid] != OXID_AVAILABLE) {
		update_scsi_oxid(fi);
		if (fi->g.scsi_oxid == initial_oxid) {
			T_MSG("No free OX_IDs avaliable")
			reset_tachyon(fi, SOFTWARE_RESET);
			return 1;
		}
	}
	return 0;
}		

static void update_scsi_oxid(struct fc_info *fi)
{
	fi->g.scsi_oxid++;
	if (fi->g.scsi_oxid == (MAX_SCSI_XID + 1))
		fi->g.scsi_oxid = 0;
}

static int get_free_SDB(struct fc_info *fi)
{
unsigned int initial_indx = fi->q.sdb_indx;
	/* Check if the SDB is in use.
	 * We could have an outstanding SCSI Read command.
	 * We should find a free slot as we can queue a
	 * maximum of 32 SCSI commands only. 
	 */
	while (fi->q.sdb_slot_status[fi->q.sdb_indx] != SDB_FREE) {
		update_SDB_indx(fi);
		if (fi->q.sdb_indx == initial_indx) {
			T_MSG("No free SDB buffers avaliable")
			reset_tachyon(fi, SOFTWARE_RESET);
			return 1;
		}
	}
	return 0;
}		

static void update_SDB_indx(struct fc_info *fi)
{
	fi->q.sdb_indx++;
	if (fi->q.sdb_indx == NO_OF_SDB_ENTRIES)
		fi->q.sdb_indx = 0;
}

int iph5526_release(struct Scsi_Host *host)
{
struct iph5526_hostdata *hostdata = (struct iph5526_hostdata*)host->hostdata;
struct fc_info *fi = hostdata->fi;
	free_irq(host->irq, host);
	iounmap(fi->g.mem_base);
	return 0;
}

const char *iph5526_info(struct Scsi_Host *host)
{
static char buf[80];
	sprintf(buf, "Interphase 5526 Fibre Channel PCI SCSI Adapter using IRQ %d\n", host->irq);
	return buf;
}

#ifdef MODULE

#define NAMELEN		8	/* # of chars for storing dev->name */

static struct net_device *dev_fc[MAX_FC_CARDS];

static int io;
static int irq;
static int bad;	/* 0xbad = bad sig or no reset ack */
static int scsi_registered;


int init_module(void)
{
int i = 0;

	driver_template.module = &__this_module;
	scsi_register_module(MODULE_SCSI_HA, &driver_template);
	if (driver_template.present)
		scsi_registered = TRUE; 
	else {
		printk("iph5526: SCSI registeration failed!!!\n");
		scsi_registered = FALSE;
		scsi_unregister_module(MODULE_SCSI_HA, &driver_template);
	}

	while(fc[i] != NULL) {
		dev_fc[i] = NULL;
		dev_fc[i] = init_fcdev(dev_fc[i], 0);	
		if (dev_fc[i] == NULL) {
			printk("iph5526.c: init_fcdev failed for card #%d\n", i+1);
			break;
		}
		dev_fc[i]->irq = irq;
		dev_fc[i]->mem_end = bad;
		dev_fc[i]->base_addr = io;
		dev_fc[i]->init = iph5526_probe;
		dev_fc[i]->priv = fc[i];
		fc[i]->dev = dev_fc[i];
		if (register_fcdev(dev_fc[i]) != 0) {
			kfree(dev_fc[i]);
			dev_fc[i] = NULL;
			if (i == 0) {
				printk("iph5526.c: IP registeration failed!!!\n");
				return -ENODEV;
			}
		}
		i++;
	}
	if (i == 0)
		return -ENODEV;
	
	return 0;
}

void cleanup_module(void)
{
int i = 0;
	while(fc[i] != NULL) {
	struct net_device *dev = fc[i]->dev;
	void *priv = dev->priv;
		fc[i]->g.dont_init = TRUE;
		take_tachyon_offline(fc[i]);
		unregister_fcdev(dev);
		clean_up_memory(fc[i]);
		if (dev->priv)
			kfree(priv);
		kfree(dev);
		dev = NULL;
		i++;
	}
	if (scsi_registered == TRUE)
		scsi_unregister_module(MODULE_SCSI_HA, &driver_template); 
}
#endif /* MODULE */

void clean_up_memory(struct fc_info *fi)
{
int i,j;
	ENTER("clean_up_memory");
	if (fi->q.ptr_mfsbq_base)
		free_pages((u_long)bus_to_virt(ntohl(*(fi->q.ptr_mfsbq_base))), 5);
	DPRINTK("after kfree2");
	for (i = 0; i < SFSBQ_LENGTH; i++)
		for (j = 0; j < NO_OF_ENTRIES; j++)
			if (fi->q.ptr_sfs_buffers[i*NO_OF_ENTRIES + j])
				kfree(fi->q.ptr_sfs_buffers[i*NO_OF_ENTRIES + j]);
	DPRINTK("after kfree1");
	if (fi->q.ptr_ocq_base)
		free_page((u_long)fi->q.ptr_ocq_base);
	if (fi->q.ptr_imq_base)
		free_page((u_long)fi->q.ptr_imq_base);
	if (fi->q.ptr_mfsbq_base)
		free_page((u_long)fi->q.ptr_mfsbq_base);
	if (fi->q.ptr_sfsbq_base)
		free_page((u_long)fi->q.ptr_sfsbq_base);
	if (fi->q.ptr_edb_base)
		free_pages((u_long)fi->q.ptr_edb_base, 5);
	if (fi->q.ptr_sest_base)
		free_pages((u_long)fi->q.ptr_sest_base, 5);
	if (fi->q.ptr_tachyon_header_base)
		free_page((u_long)fi->q.ptr_tachyon_header_base);
	if (fi->q.ptr_sdb_base)
		free_pages((u_long)fi->q.ptr_sdb_base, 5);
	if (fi->q.ptr_fcp_cmnd_base)
		free_page((u_long)fi->q.ptr_fcp_cmnd_base);
	DPRINTK("after free_pages"); 
	if (fi->q.ptr_host_ocq_cons_indx)
		kfree(fi->q.ptr_host_ocq_cons_indx);
	if (fi->q.ptr_host_hpcq_cons_indx)
		kfree(fi->q.ptr_host_hpcq_cons_indx);
	if (fi->q.ptr_host_imq_prod_indx)
		kfree(fi->q.ptr_host_imq_prod_indx);
	DPRINTK("after kfree3");
	while (fi->node_info_list) {
	struct fc_node_info *temp_list = fi->node_info_list;
		fi->node_info_list = fi->node_info_list->next;
		kfree(temp_list);
	}
	while (fi->ox_id_list) {
	struct ox_id_els_map *temp = fi->ox_id_list;
		fi->ox_id_list = fi->ox_id_list->next;
		kfree(temp);
	}
	LEAVE("clean_up_memory");
}

static int initialize_register_pointers(struct fc_info *fi)
{
ENTER("initialize_register_pointers");
if(fi->g.tachyon_base == 0)
	return -ENOMEM; 
	
fi->i_r.ptr_ichip_hw_control_reg	= ICHIP_HW_CONTROL_REG_OFF + fi->g.tachyon_base;
fi->i_r.ptr_ichip_hw_status_reg = ICHIP_HW_STATUS_REG_OFF + fi->g.tachyon_base;
fi->i_r.ptr_ichip_hw_addr_mask_reg = ICHIP_HW_ADDR_MASK_REG_OFF + fi->g.tachyon_base;
fi->t_r.ptr_ocq_base_reg = OCQ_BASE_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_ocq_len_reg = OCQ_LENGTH_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_ocq_prod_indx_reg = OCQ_PRODUCER_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_ocq_cons_indx_reg = OCQ_CONSUMER_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_imq_base_reg = IMQ_BASE_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_imq_len_reg = IMQ_LENGTH_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_imq_cons_indx_reg = IMQ_CONSUMER_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_imq_prod_indx_reg = IMQ_PRODUCER_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_mfsbq_base_reg = MFSBQ_BASE_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_mfsbq_len_reg = MFSBQ_LENGTH_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_mfsbq_prod_reg = MFSBQ_PRODUCER_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_mfsbq_cons_reg = MFSBQ_CONSUMER_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_mfsbuff_len_reg = MFS_LENGTH_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_sfsbq_base_reg = SFSBQ_BASE_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_sfsbq_len_reg = SFSBQ_LENGTH_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_sfsbq_prod_reg = SFSBQ_PRODUCER_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_sfsbq_cons_reg = SFSBQ_CONSUMER_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_sfsbuff_len_reg = SFS_LENGTH_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_sest_base_reg = SEST_BASE_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_sest_len_reg = SEST_LENGTH_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_scsibuff_len_reg = SCSI_LENGTH_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_tach_config_reg = TACHYON_CONFIG_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_tach_control_reg = TACHYON_CONTROL_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_tach_status_reg = TACHYON_STATUS_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_tach_flush_oxid_reg = TACHYON_FLUSH_SEST_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_fm_config_reg = FMGR_CONFIG_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_fm_control_reg = FMGR_CONTROL_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_fm_status_reg = FMGR_STATUS_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_fm_tov_reg = FMGR_TIMER_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_fm_wwn_hi_reg = FMGR_WWN_HI_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_fm_wwn_low_reg = FMGR_WWN_LO_REGISTER_OFFSET + fi->g.tachyon_base;
fi->t_r.ptr_fm_rx_al_pa_reg = FMGR_RCVD_ALPA_REGISTER_OFFSET + fi->g.tachyon_base;

LEAVE("initialize_register_pointers");
return 1;
}



/*
 * Local variables:
 *  compile-command: "gcc -DKERNEL -Wall -O6 -fomit-frame-pointer -I/usr/src/linux/net/tcp -c iph5526.c"
 *  version-control: t
 *  kept-new-versions: 5
 * End:
 */
