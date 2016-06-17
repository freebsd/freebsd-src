/*
 *   procfs handler for Linux I2O subsystem
 *
 *   (c) Copyright 1999 Deepak Saxena
 *   
 *   Originally written by Deepak Saxena(deepak@plexity.net)
 *
 *   This program is free software. You can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version
 *   2 of the License, or (at your option) any later version.
 *
 *   This is an initial test release. The code is based on the design
 *   of the ide procfs system (drivers/block/ide-proc.c). Some code
 *   taken from i2o-core module by Alan Cox.
 *
 *   DISCLAIMER: This code is still under development/test and may cause
 *   your system to behave unpredictably.  Use at your own discretion.
 *
 *   LAN entries by Juha Sievänen (Juha.Sievanen@cs.Helsinki.FI),
 *		    Auvo Häkkinen (Auvo.Hakkinen@cs.Helsinki.FI)
 *   University of Helsinki, Department of Computer Science
 *
 *   Some cleanup (c) 2002 Red Hat <alan@redhat.com>
 *   Working to make I2O 64bit safe and following the PCI API
 *
 *   TODO List
 *	- Clean up code to use official structure definitions 
 */

// FIXME!
#define FMT_U64_HEX "0x%08x%08x"
#define U64_VAL(pu64) *((u32*)(pu64)+1), *((u32*)(pu64))

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/i2o.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/spinlock.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/byteorder.h>

#include "i2o_lan.h"

/*
 * Structure used to define /proc entries
 */
typedef struct _i2o_proc_entry_t
{
	char *name;			/* entry name */
	mode_t mode;			/* mode */
	read_proc_t *read_proc;		/* read func */
	write_proc_t *write_proc;	/* write func */
} i2o_proc_entry;

// #define DRIVERDEBUG

static int i2o_proc_read_lct(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_hrt(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_status(char *, char **, off_t, int, int *, void *);

static int i2o_proc_read_hw(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_ddm_table(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_driver_store(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_drivers_stored(char *, char **, off_t, int, int *, void *);

static int i2o_proc_read_groups(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_phys_device(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_claimed(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_users(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_priv_msgs(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_authorized_users(char *, char **, off_t, int, int *, void *);

static int i2o_proc_read_dev_name(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_dev_identity(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_ddm_identity(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_uinfo(char *, char **, off_t, int, int *, void *);
static int i2o_proc_read_sgl_limits(char *, char **, off_t, int, int *, void *);

static int i2o_proc_read_sensors(char *, char **, off_t, int, int *, void *);

static int print_serial_number(char *, int, u8 *, int);

static int i2o_proc_create_entries(void *, i2o_proc_entry *,
				   struct proc_dir_entry *);
static void i2o_proc_remove_entries(i2o_proc_entry *, struct proc_dir_entry *);
static int i2o_proc_add_controller(struct i2o_controller *, 
				   struct proc_dir_entry * );
static void i2o_proc_remove_controller(struct i2o_controller *, 
				       struct proc_dir_entry * );
static void i2o_proc_add_device(struct i2o_device *, struct proc_dir_entry *);
static void i2o_proc_remove_device(struct i2o_device *);
static int create_i2o_procfs(void);
static int destroy_i2o_procfs(void);
static void i2o_proc_new_dev(struct i2o_controller *, struct i2o_device *);
static void i2o_proc_dev_del(struct i2o_controller *, struct i2o_device *);

static int i2o_proc_read_lan_dev_info(char *, char **, off_t, int, int *,
				      void *);
static int i2o_proc_read_lan_mac_addr(char *, char **, off_t, int, int *,
				      void *);
static int i2o_proc_read_lan_mcast_addr(char *, char **, off_t, int, int *,
					void *);
static int i2o_proc_read_lan_batch_control(char *, char **, off_t, int, int *,
					   void *);
static int i2o_proc_read_lan_operation(char *, char **, off_t, int, int *,
				       void *);
static int i2o_proc_read_lan_media_operation(char *, char **, off_t, int,
					     int *, void *);
static int i2o_proc_read_lan_alt_addr(char *, char **, off_t, int, int *,
				      void *);
static int i2o_proc_read_lan_tx_info(char *, char **, off_t, int, int *,
				     void *);
static int i2o_proc_read_lan_rx_info(char *, char **, off_t, int, int *,
				     void *);
static int i2o_proc_read_lan_hist_stats(char *, char **, off_t, int, int *,
					void *);
static int i2o_proc_read_lan_eth_stats(char *, char **, off_t, int,
				       int *, void *);
static int i2o_proc_read_lan_tr_stats(char *, char **, off_t, int, int *,
				      void *);
static int i2o_proc_read_lan_fddi_stats(char *, char **, off_t, int, int *,
					void *);

static struct proc_dir_entry *i2o_proc_dir_root;

/*
 * I2O OSM descriptor
 */
static struct i2o_handler i2o_proc_handler =
{
	NULL,
	i2o_proc_new_dev,
	i2o_proc_dev_del,
	NULL,
	"I2O procfs Layer",
	0,
	0xffffffff	// All classes
};

/*
 * IOP specific entries...write field just in case someone 
 * ever wants one.
 */
static i2o_proc_entry generic_iop_entries[] = 
{
	{"hrt", S_IFREG|S_IRUGO, i2o_proc_read_hrt, NULL},
	{"lct", S_IFREG|S_IRUGO, i2o_proc_read_lct, NULL},
	{"status", S_IFREG|S_IRUGO, i2o_proc_read_status, NULL},
	{"hw", S_IFREG|S_IRUGO, i2o_proc_read_hw, NULL},
	{"ddm_table", S_IFREG|S_IRUGO, i2o_proc_read_ddm_table, NULL},
	{"driver_store", S_IFREG|S_IRUGO, i2o_proc_read_driver_store, NULL},
	{"drivers_stored", S_IFREG|S_IRUGO, i2o_proc_read_drivers_stored, NULL},
	{NULL, 0, NULL, NULL}
};

/*
 * Device specific entries
 */
static i2o_proc_entry generic_dev_entries[] = 
{
	{"groups", S_IFREG|S_IRUGO, i2o_proc_read_groups, NULL},
	{"phys_dev", S_IFREG|S_IRUGO, i2o_proc_read_phys_device, NULL},
	{"claimed", S_IFREG|S_IRUGO, i2o_proc_read_claimed, NULL},
	{"users", S_IFREG|S_IRUGO, i2o_proc_read_users, NULL},
	{"priv_msgs", S_IFREG|S_IRUGO, i2o_proc_read_priv_msgs, NULL},
	{"authorized_users", S_IFREG|S_IRUGO, i2o_proc_read_authorized_users, NULL},
	{"dev_identity", S_IFREG|S_IRUGO, i2o_proc_read_dev_identity, NULL},
	{"ddm_identity", S_IFREG|S_IRUGO, i2o_proc_read_ddm_identity, NULL},
	{"user_info", S_IFREG|S_IRUGO, i2o_proc_read_uinfo, NULL},
	{"sgl_limits", S_IFREG|S_IRUGO, i2o_proc_read_sgl_limits, NULL},
	{"sensors", S_IFREG|S_IRUGO, i2o_proc_read_sensors, NULL},
	{NULL, 0, NULL, NULL}
};

/*
 *  Storage unit specific entries (SCSI Periph, BS) with device names
 */
static i2o_proc_entry rbs_dev_entries[] = 
{
	{"dev_name", S_IFREG|S_IRUGO, i2o_proc_read_dev_name, NULL},
	{NULL, 0, NULL, NULL}
};

#define SCSI_TABLE_SIZE	13
static char *scsi_devices[] = 
{
	"Direct-Access Read/Write",
	"Sequential-Access Storage",
	"Printer",
	"Processor",
	"WORM Device",
	"CD-ROM Device",
	"Scanner Device",
	"Optical Memory Device",
	"Medium Changer Device",
	"Communications Device",
	"Graphics Art Pre-Press Device",
	"Graphics Art Pre-Press Device",
	"Array Controller Device"
};

/* private */

/*
 * Generic LAN specific entries
 * 
 * Should groups with r/w entries have their own subdirectory?
 *
 */
static i2o_proc_entry lan_entries[] = 
{
	{"lan_dev_info", S_IFREG|S_IRUGO, i2o_proc_read_lan_dev_info, NULL},
	{"lan_mac_addr", S_IFREG|S_IRUGO, i2o_proc_read_lan_mac_addr, NULL},
	{"lan_mcast_addr", S_IFREG|S_IRUGO|S_IWUSR,
	 i2o_proc_read_lan_mcast_addr, NULL},
	{"lan_batch_ctrl", S_IFREG|S_IRUGO|S_IWUSR,
	 i2o_proc_read_lan_batch_control, NULL},
	{"lan_operation", S_IFREG|S_IRUGO, i2o_proc_read_lan_operation, NULL},
	{"lan_media_operation", S_IFREG|S_IRUGO,
	 i2o_proc_read_lan_media_operation, NULL},
	{"lan_alt_addr", S_IFREG|S_IRUGO, i2o_proc_read_lan_alt_addr, NULL},
	{"lan_tx_info", S_IFREG|S_IRUGO, i2o_proc_read_lan_tx_info, NULL},
	{"lan_rx_info", S_IFREG|S_IRUGO, i2o_proc_read_lan_rx_info, NULL},

	{"lan_hist_stats", S_IFREG|S_IRUGO, i2o_proc_read_lan_hist_stats, NULL},
	{NULL, 0, NULL, NULL}
};

/*
 * Port specific LAN entries
 * 
 */
static i2o_proc_entry lan_eth_entries[] = 
{
	{"lan_eth_stats", S_IFREG|S_IRUGO, i2o_proc_read_lan_eth_stats, NULL},
	{NULL, 0, NULL, NULL}
};

static i2o_proc_entry lan_tr_entries[] = 
{
	{"lan_tr_stats", S_IFREG|S_IRUGO, i2o_proc_read_lan_tr_stats, NULL},
	{NULL, 0, NULL, NULL}
};

static i2o_proc_entry lan_fddi_entries[] = 
{
	{"lan_fddi_stats", S_IFREG|S_IRUGO, i2o_proc_read_lan_fddi_stats, NULL},
	{NULL, 0, NULL, NULL}
};


static char *chtostr(u8 *chars, int n)
{
	char tmp[256];
	tmp[0] = 0;
        return strncat(tmp, (char *)chars, n);
}

static int i2o_report_query_status(char *buf, int block_status, char *group)
{
	switch (block_status)
	{
	case -ETIMEDOUT:
		return sprintf(buf, "Timeout reading group %s.\n",group);
	case -ENOMEM:
		return sprintf(buf, "No free memory to read the table.\n");
	case -I2O_PARAMS_STATUS_INVALID_GROUP_ID:
		return sprintf(buf, "Group %s not supported.\n", group);
	default:
		return sprintf(buf, "Error reading group %s. BlockStatus 0x%02X\n",
			       group, -block_status);
	}
}

static char* bus_strings[] = 
{ 
	"Local Bus", 
	"ISA", 
	"EISA", 
	"MCA", 
	"PCI",
	"PCMCIA", 
	"NUBUS", 
	"CARDBUS"
};

static spinlock_t i2o_proc_lock = SPIN_LOCK_UNLOCKED;

int i2o_proc_read_hrt(char *buf, char **start, off_t offset, int count, 
		      int *eof, void *data)
{
	struct i2o_controller *c = (struct i2o_controller *)data;
	i2o_hrt *hrt = (i2o_hrt *)c->hrt;
	u32 bus;
	int len, i;

	spin_lock(&i2o_proc_lock);

	len = 0;

	if(hrt->hrt_version)
	{
		len += sprintf(buf+len, 
			       "HRT table for controller is too new a version.\n");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	if((hrt->num_entries * hrt->entry_len + 8) > 2048) {
		printk(KERN_WARNING "i2o_proc: HRT does not fit into buffer\n");
		len += sprintf(buf+len,
			       "HRT table too big to fit in buffer.\n");
		spin_unlock(&i2o_proc_lock);
		return len;
	}
	
	len += sprintf(buf+len, "HRT has %d entries of %d bytes each.\n",
		       hrt->num_entries, hrt->entry_len << 2);

	for(i = 0; i < hrt->num_entries && len < count; i++)
	{
		len += sprintf(buf+len, "Entry %d:\n", i);
		len += sprintf(buf+len, "   Adapter ID: %0#10x\n", 
					hrt->hrt_entry[i].adapter_id);
		len += sprintf(buf+len, "   Controlling tid: %0#6x\n",
					hrt->hrt_entry[i].parent_tid);

		if(hrt->hrt_entry[i].bus_type != 0x80)
		{
			bus = hrt->hrt_entry[i].bus_type;
			len += sprintf(buf+len, "   %s Information\n", bus_strings[bus]);

			switch(bus)
			{
				case I2O_BUS_LOCAL:
					len += sprintf(buf+len, "     IOBase: %0#6x,",
								hrt->hrt_entry[i].bus.local_bus.LbBaseIOPort);
					len += sprintf(buf+len, " MemoryBase: %0#10x\n",
								hrt->hrt_entry[i].bus.local_bus.LbBaseMemoryAddress);
					break;

				case I2O_BUS_ISA:
					len += sprintf(buf+len, "     IOBase: %0#6x,",
								hrt->hrt_entry[i].bus.isa_bus.IsaBaseIOPort);
					len += sprintf(buf+len, " MemoryBase: %0#10x,",
								hrt->hrt_entry[i].bus.isa_bus.IsaBaseMemoryAddress);
					len += sprintf(buf+len, " CSN: %0#4x,",
								hrt->hrt_entry[i].bus.isa_bus.CSN);
					break;

				case I2O_BUS_EISA:
					len += sprintf(buf+len, "     IOBase: %0#6x,",
								hrt->hrt_entry[i].bus.eisa_bus.EisaBaseIOPort);
					len += sprintf(buf+len, " MemoryBase: %0#10x,",
								hrt->hrt_entry[i].bus.eisa_bus.EisaBaseMemoryAddress);
					len += sprintf(buf+len, " Slot: %0#4x,",
								hrt->hrt_entry[i].bus.eisa_bus.EisaSlotNumber);
					break;
			 
				case I2O_BUS_MCA:
					len += sprintf(buf+len, "     IOBase: %0#6x,",
								hrt->hrt_entry[i].bus.mca_bus.McaBaseIOPort);
					len += sprintf(buf+len, " MemoryBase: %0#10x,",
								hrt->hrt_entry[i].bus.mca_bus.McaBaseMemoryAddress);
					len += sprintf(buf+len, " Slot: %0#4x,",
								hrt->hrt_entry[i].bus.mca_bus.McaSlotNumber);
					break;

				case I2O_BUS_PCI:
					len += sprintf(buf+len, "     Bus: %0#4x",
								hrt->hrt_entry[i].bus.pci_bus.PciBusNumber);
					len += sprintf(buf+len, " Dev: %0#4x",
								hrt->hrt_entry[i].bus.pci_bus.PciDeviceNumber);
					len += sprintf(buf+len, " Func: %0#4x",
								hrt->hrt_entry[i].bus.pci_bus.PciFunctionNumber);
					len += sprintf(buf+len, " Vendor: %0#6x",
								hrt->hrt_entry[i].bus.pci_bus.PciVendorID);
					len += sprintf(buf+len, " Device: %0#6x\n",
								hrt->hrt_entry[i].bus.pci_bus.PciDeviceID);
					break;

				default:
					len += sprintf(buf+len, "      Unsupported Bus Type\n");
			}
		}
		else
			len += sprintf(buf+len, "   Unknown Bus Type\n");
	}

	spin_unlock(&i2o_proc_lock);
	
	return len;
}

int i2o_proc_read_lct(char *buf, char **start, off_t offset, int len,
	int *eof, void *data)
{
	struct i2o_controller *c = (struct i2o_controller*)data;
	i2o_lct *lct = (i2o_lct *)c->lct;
	int entries;
	int i;

#define BUS_TABLE_SIZE 3
	static char *bus_ports[] =
	{
		"Generic Bus",
		"SCSI Bus",
		"Fibre Channel Bus"
	};

	spin_lock(&i2o_proc_lock);
	len = 0;

	entries = (lct->table_size - 3)/9;

	len += sprintf(buf, "LCT contains %d %s\n", entries,
						entries == 1 ? "entry" : "entries");
	if(lct->boot_tid)	
		len += sprintf(buf+len, "Boot Device @ ID %d\n", lct->boot_tid);

	len += 
		sprintf(buf+len, "Current Change Indicator: %#10x\n", lct->change_ind);

	for(i = 0; i < entries; i++)
	{
		len += sprintf(buf+len, "Entry %d\n", i);
		len += sprintf(buf+len, "  Class, SubClass  : %s", i2o_get_class_name(lct->lct_entry[i].class_id));
	
		/*
		 *	Classes which we'll print subclass info for
		 */
		switch(lct->lct_entry[i].class_id & 0xFFF)
		{
			case I2O_CLASS_RANDOM_BLOCK_STORAGE:
				switch(lct->lct_entry[i].sub_class)
				{
					case 0x00:
						len += sprintf(buf+len, ", Direct-Access Read/Write");
						break;

					case 0x04:
						len += sprintf(buf+len, ", WORM Drive");
						break;
	
					case 0x05:
						len += sprintf(buf+len, ", CD-ROM Drive");
						break;

					case 0x07:
						len += sprintf(buf+len, ", Optical Memory Device");
						break;

					default:
						len += sprintf(buf+len, ", Unknown (0x%02x)",
							       lct->lct_entry[i].sub_class);
						break;
				}
				break;

			case I2O_CLASS_LAN:
				switch(lct->lct_entry[i].sub_class & 0xFF)
				{
					case 0x30:
						len += sprintf(buf+len, ", Ethernet");
						break;

					case 0x40:
						len += sprintf(buf+len, ", 100base VG");
						break;

					case 0x50:
						len += sprintf(buf+len, ", IEEE 802.5/Token-Ring");
						break;

					case 0x60:
						len += sprintf(buf+len, ", ANSI X3T9.5 FDDI");
						break;
		
					case 0x70:
						len += sprintf(buf+len, ", Fibre Channel");
						break;

					default:
						len += sprintf(buf+len, ", Unknown Sub-Class (0x%02x)",
							       lct->lct_entry[i].sub_class & 0xFF);
						break;
				}
				break;

			case I2O_CLASS_SCSI_PERIPHERAL:
				if(lct->lct_entry[i].sub_class < SCSI_TABLE_SIZE)
					len += sprintf(buf+len, ", %s", 
								scsi_devices[lct->lct_entry[i].sub_class]);
				else
					len += sprintf(buf+len, ", Unknown Device Type");
				break;

			case I2O_CLASS_BUS_ADAPTER_PORT:
				if(lct->lct_entry[i].sub_class < BUS_TABLE_SIZE)
					len += sprintf(buf+len, ", %s", 
								bus_ports[lct->lct_entry[i].sub_class]);
				else
					len += sprintf(buf+len, ", Unknown Bus Type");
				break;
		}
		len += sprintf(buf+len, "\n");
		
		len += sprintf(buf+len, "  Local TID        : 0x%03x\n", lct->lct_entry[i].tid);
		len += sprintf(buf+len, "  User TID         : 0x%03x\n", lct->lct_entry[i].user_tid);
		len += sprintf(buf+len, "  Parent TID       : 0x%03x\n", 
					lct->lct_entry[i].parent_tid);
		len += sprintf(buf+len, "  Identity Tag     : 0x%x%x%x%x%x%x%x%x\n",
					lct->lct_entry[i].identity_tag[0],
					lct->lct_entry[i].identity_tag[1],
					lct->lct_entry[i].identity_tag[2],
					lct->lct_entry[i].identity_tag[3],
					lct->lct_entry[i].identity_tag[4],
					lct->lct_entry[i].identity_tag[5],
					lct->lct_entry[i].identity_tag[6],
					lct->lct_entry[i].identity_tag[7]);
		len += sprintf(buf+len, "  Change Indicator : %0#10x\n", 
				lct->lct_entry[i].change_ind);
		len += sprintf(buf+len, "  Event Capab Mask : %0#10x\n", 
				lct->lct_entry[i].device_flags);
	}

	spin_unlock(&i2o_proc_lock);
	return len;
}

int i2o_proc_read_status(char *buf, char **start, off_t offset, int len, 
			 int *eof, void *data)
{
	struct i2o_controller *c = (struct i2o_controller*)data;
	char prodstr[25];
	int version;
	
	spin_lock(&i2o_proc_lock);
	len = 0;

	i2o_status_get(c); // reread the status block

	len += sprintf(buf+len,"Organization ID        : %0#6x\n", 
				c->status_block->org_id);

	version = c->status_block->i2o_version;
	
/* FIXME for Spec 2.0
	if (version == 0x02) {
		len += sprintf(buf+len,"Lowest I2O version supported: ");
		switch(workspace[2]) {
			case 0x00:
				len += sprintf(buf+len,"1.0\n");
				break;
			case 0x01:
				len += sprintf(buf+len,"1.5\n");
				break;
			case 0x02:
				len += sprintf(buf+len,"2.0\n");
				break;
		}

		len += sprintf(buf+len, "Highest I2O version supported: ");
		switch(workspace[3]) {
			case 0x00:
				len += sprintf(buf+len,"1.0\n");
				break;
			case 0x01:
				len += sprintf(buf+len,"1.5\n");
				break;
			case 0x02:
				len += sprintf(buf+len,"2.0\n");
				break;
		}
	}
*/
	len += sprintf(buf+len,"IOP ID                 : %0#5x\n", 
				c->status_block->iop_id);
	len += sprintf(buf+len,"Host Unit ID           : %0#6x\n",
				c->status_block->host_unit_id);
	len += sprintf(buf+len,"Segment Number         : %0#5x\n",
				c->status_block->segment_number);

	len += sprintf(buf+len, "I2O version            : ");
	switch (version) {
		case 0x00:
			len += sprintf(buf+len,"1.0\n");
			break;
		case 0x01:
			len += sprintf(buf+len,"1.5\n");
			break;
		case 0x02:
			len += sprintf(buf+len,"2.0\n");
			break;
		default:
			len += sprintf(buf+len,"Unknown version\n");
	}

	len += sprintf(buf+len, "IOP State              : ");
	switch (c->status_block->iop_state) {
		case 0x01:
			len += sprintf(buf+len,"INIT\n");
			break;

		case 0x02:
			len += sprintf(buf+len,"RESET\n");
			break;

		case 0x04:
			len += sprintf(buf+len,"HOLD\n");
			break;

		case 0x05:
			len += sprintf(buf+len,"READY\n");
			break;

		case 0x08:
			len += sprintf(buf+len,"OPERATIONAL\n");
			break;

		case 0x10:
			len += sprintf(buf+len,"FAILED\n");
			break;

		case 0x11:
			len += sprintf(buf+len,"FAULTED\n");
			break;

		default:
			len += sprintf(buf+len,"Unknown\n");
			break;
	}

	len += sprintf(buf+len,"Messenger Type         : ");
	switch (c->status_block->msg_type) { 
		case 0x00:
			len += sprintf(buf+len,"Memory mapped\n");
			break;
		case 0x01:
			len += sprintf(buf+len,"Memory mapped only\n");
			break;
		case 0x02:
			len += sprintf(buf+len,"Remote only\n");
			break;
		case 0x03:
			len += sprintf(buf+len,"Memory mapped and remote\n");
			break;
		default:
			len += sprintf(buf+len,"Unknown\n");
	}

	len += sprintf(buf+len,"Inbound Frame Size     : %d bytes\n", 
				c->status_block->inbound_frame_size<<2);
	len += sprintf(buf+len,"Max Inbound Frames     : %d\n", 
				c->status_block->max_inbound_frames);
	len += sprintf(buf+len,"Current Inbound Frames : %d\n", 
				c->status_block->cur_inbound_frames);
	len += sprintf(buf+len,"Max Outbound Frames    : %d\n", 
				c->status_block->max_outbound_frames);

	/* Spec doesn't say if NULL terminated or not... */
	memcpy(prodstr, c->status_block->product_id, 24);
	prodstr[24] = '\0';
	len += sprintf(buf+len,"Product ID             : %s\n", prodstr);
	len += sprintf(buf+len,"Expected LCT Size      : %d bytes\n", 
				c->status_block->expected_lct_size);

	len += sprintf(buf+len,"IOP Capabilities\n");
	len += sprintf(buf+len,"    Context Field Size Support : ");
	switch (c->status_block->iop_capabilities & 0x0000003) {
		case 0:
			len += sprintf(buf+len,"Supports only 32-bit context fields\n");
			break;
		case 1:
			len += sprintf(buf+len,"Supports only 64-bit context fields\n");
			break;
		case 2:
			len += sprintf(buf+len,"Supports 32-bit and 64-bit context fields, "
						"but not concurrently\n");
			break;
		case 3:
			len += sprintf(buf+len,"Supports 32-bit and 64-bit context fields "
						"concurrently\n");
			break;
		default:
			len += sprintf(buf+len,"0x%08x\n",c->status_block->iop_capabilities);
	}
	len += sprintf(buf+len,"    Current Context Field Size : ");
	switch (c->status_block->iop_capabilities & 0x0000000C) {
		case 0:
			len += sprintf(buf+len,"not configured\n");
			break;
		case 4:
			len += sprintf(buf+len,"Supports only 32-bit context fields\n");
			break;
		case 8:
			len += sprintf(buf+len,"Supports only 64-bit context fields\n");
			break;
		case 12:
			len += sprintf(buf+len,"Supports both 32-bit or 64-bit context fields "
						"concurrently\n");
			break;
		default:
			len += sprintf(buf+len,"\n");
	}
	len += sprintf(buf+len,"    Inbound Peer Support       : %s\n",
			(c->status_block->iop_capabilities & 0x00000010) ? "Supported" : "Not supported");
	len += sprintf(buf+len,"    Outbound Peer Support      : %s\n",
			(c->status_block->iop_capabilities & 0x00000020) ? "Supported" : "Not supported");
	len += sprintf(buf+len,"    Peer to Peer Support       : %s\n",
			(c->status_block->iop_capabilities & 0x00000040) ? "Supported" : "Not supported");

	len += sprintf(buf+len, "Desired private memory size   : %d kB\n", 
				c->status_block->desired_mem_size>>10);
	len += sprintf(buf+len, "Allocated private memory size : %d kB\n", 
				c->status_block->current_mem_size>>10);
	len += sprintf(buf+len, "Private memory base address   : %0#10x\n", 
				c->status_block->current_mem_base);
	len += sprintf(buf+len, "Desired private I/O size      : %d kB\n", 
				c->status_block->desired_io_size>>10);
	len += sprintf(buf+len, "Allocated private I/O size    : %d kB\n", 
				c->status_block->current_io_size>>10);
	len += sprintf(buf+len, "Private I/O base address      : %0#10x\n", 
				c->status_block->current_io_base);

	spin_unlock(&i2o_proc_lock);

	return len;
}

int i2o_proc_read_hw(char *buf, char **start, off_t offset, int len, 
		     int *eof, void *data)
{
	struct i2o_controller *c = (struct i2o_controller*)data;
	static u32 work32[5];
	static u8 *work8 = (u8*)work32;
	static u16 *work16 = (u16*)work32;
	int token;
	u32 hwcap;

	static char *cpu_table[] =
	{
		"Intel 80960 series",
		"AMD2900 series",
		"Motorola 68000 series",
		"ARM series",
		"MIPS series",
		"Sparc series",
		"PowerPC series",
		"Intel x86 series"
	};

	spin_lock(&i2o_proc_lock);

	len = 0;

	token = i2o_query_scalar(c, ADAPTER_TID, 0x0000, -1, &work32, sizeof(work32));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0000 IOP Hardware");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf+len, "I2O Vendor ID    : %0#6x\n", work16[0]);
	len += sprintf(buf+len, "Product ID       : %0#6x\n", work16[1]);
	len += sprintf(buf+len, "CPU              : ");
	if(work8[16] > 8)
		len += sprintf(buf+len, "Unknown\n");
	else
		len += sprintf(buf+len, "%s\n", cpu_table[work8[16]]);
	/* Anyone using ProcessorVersion? */
	
	len += sprintf(buf+len, "RAM              : %dkB\n", work32[1]>>10);
	len += sprintf(buf+len, "Non-Volatile Mem : %dkB\n", work32[2]>>10);

	hwcap = work32[3];
	len += sprintf(buf+len, "Capabilities : 0x%08x\n", hwcap);
	len += sprintf(buf+len, "   [%s] Self booting\n",
			(hwcap&0x00000001) ? "+" : "-");
	len += sprintf(buf+len, "   [%s] Upgradable IRTOS\n",
			(hwcap&0x00000002) ? "+" : "-");
	len += sprintf(buf+len, "   [%s] Supports downloading DDMs\n",
			(hwcap&0x00000004) ? "+" : "-");
	len += sprintf(buf+len, "   [%s] Supports installing DDMs\n",
			(hwcap&0x00000008) ? "+" : "-");
	len += sprintf(buf+len, "   [%s] Battery-backed RAM\n",
			(hwcap&0x00000010) ? "+" : "-");

	spin_unlock(&i2o_proc_lock);

	return len;
}


/* Executive group 0003h - Executing DDM List (table) */
int i2o_proc_read_ddm_table(char *buf, char **start, off_t offset, int len, 
			    int *eof, void *data)
{
	struct i2o_controller *c = (struct i2o_controller*)data;
	int token;
	int i;

	typedef struct _i2o_exec_execute_ddm_table {
		u16 ddm_tid;
		u8  module_type;
		u8  reserved;
		u16 i2o_vendor_id;
		u16 module_id;
		u8  module_name_version[28];
		u32 data_size;
		u32 code_size;
	} i2o_exec_execute_ddm_table;

	struct
	{
		u16 result_count;
		u16 pad;
		u16 block_size;
		u8  block_status;
		u8  error_info_size;
		u16 row_count;
		u16 more_flag;
		i2o_exec_execute_ddm_table ddm_table[MAX_I2O_MODULES];
	} result;

	i2o_exec_execute_ddm_table ddm_table;

	spin_lock(&i2o_proc_lock);
	len = 0;

	token = i2o_query_table(I2O_PARAMS_TABLE_GET,
				c, ADAPTER_TID, 
				0x0003, -1,
				NULL, 0,
				&result, sizeof(result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0003 Executing DDM List");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf+len, "Tid   Module_type     Vendor Mod_id  Module_name             Vrs  Data_size Code_size\n");
	ddm_table=result.ddm_table[0];

	for(i=0; i < result.row_count; ddm_table=result.ddm_table[++i])
	{
		len += sprintf(buf+len, "0x%03x ", ddm_table.ddm_tid & 0xFFF);

		switch(ddm_table.module_type)
		{
		case 0x01:
			len += sprintf(buf+len, "Downloaded DDM  ");
			break;			
		case 0x22:
			len += sprintf(buf+len, "Embedded DDM    ");
			break;
		default:
			len += sprintf(buf+len, "                ");
		}

		len += sprintf(buf+len, "%-#7x", ddm_table.i2o_vendor_id);
		len += sprintf(buf+len, "%-#8x", ddm_table.module_id);
		len += sprintf(buf+len, "%-29s", chtostr(ddm_table.module_name_version, 28));
		len += sprintf(buf+len, "%9d  ", ddm_table.data_size);
		len += sprintf(buf+len, "%8d", ddm_table.code_size);

		len += sprintf(buf+len, "\n");
	}

	spin_unlock(&i2o_proc_lock);

	return len;
}


/* Executive group 0004h - Driver Store (scalar) */
int i2o_proc_read_driver_store(char *buf, char **start, off_t offset, int len, 
		     int *eof, void *data)
{
	struct i2o_controller *c = (struct i2o_controller*)data;
	u32 work32[8];
	int token;

	spin_lock(&i2o_proc_lock);

	len = 0;

	token = i2o_query_scalar(c, ADAPTER_TID, 0x0004, -1, &work32, sizeof(work32));
	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0004 Driver Store");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf+len, "Module limit  : %d\n"
				"Module count  : %d\n"
				"Current space : %d kB\n"
				"Free space    : %d kB\n", 
			work32[0], work32[1], work32[2]>>10, work32[3]>>10);

	spin_unlock(&i2o_proc_lock);

	return len;
}


/* Executive group 0005h - Driver Store Table (table) */
int i2o_proc_read_drivers_stored(char *buf, char **start, off_t offset,
				 int len, int *eof, void *data)
{
	typedef struct _i2o_driver_store {
		u16 stored_ddm_index;
		u8  module_type;
		u8  reserved;
		u16 i2o_vendor_id;
		u16 module_id;
		u8  module_name_version[28];
		u8  date[8];
		u32 module_size;
		u32 mpb_size;
		u32 module_flags;
	} i2o_driver_store_table;

	struct i2o_controller *c = (struct i2o_controller*)data;
	int token;
	int i;

	typedef struct
	{
		u16 result_count;
		u16 pad;
		u16 block_size;
		u8  block_status;
		u8  error_info_size;
		u16 row_count;
		u16 more_flag;
		i2o_driver_store_table dst[MAX_I2O_MODULES];
	} i2o_driver_result_table;
	
	i2o_driver_result_table *result;
	i2o_driver_store_table *dst;


	len = 0;
	
	result = kmalloc(sizeof(i2o_driver_result_table), GFP_KERNEL);
	if(result == NULL)
		return -ENOMEM;

	spin_lock(&i2o_proc_lock);

	token = i2o_query_table(I2O_PARAMS_TABLE_GET,
				c, ADAPTER_TID, 0x0005, -1, NULL, 0, 
				result, sizeof(*result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0005 DRIVER STORE TABLE");
		spin_unlock(&i2o_proc_lock);
		kfree(result);
		return len;
	}

	len += sprintf(buf+len, "#  Module_type     Vendor Mod_id  Module_name             Vrs"  
				"Date     Mod_size Par_size Flags\n");
	for(i=0, dst=&result->dst[0]; i < result->row_count; dst=&result->dst[++i])
	{
		len += sprintf(buf+len, "%-3d", dst->stored_ddm_index);
		switch(dst->module_type)
		{
		case 0x01:
			len += sprintf(buf+len, "Downloaded DDM  ");
			break;			
		case 0x22:
			len += sprintf(buf+len, "Embedded DDM    ");
			break;
		default:
			len += sprintf(buf+len, "                ");
		}

#if 0
		if(c->i2oversion == 0x02)
			len += sprintf(buf+len, "%-d", dst->module_state);
#endif

		len += sprintf(buf+len, "%-#7x", dst->i2o_vendor_id);
		len += sprintf(buf+len, "%-#8x", dst->module_id);
		len += sprintf(buf+len, "%-29s", chtostr(dst->module_name_version,28));
		len += sprintf(buf+len, "%-9s", chtostr(dst->date,8));
		len += sprintf(buf+len, "%8d ", dst->module_size);
		len += sprintf(buf+len, "%8d ", dst->mpb_size);
		len += sprintf(buf+len, "0x%04x", dst->module_flags);
#if 0
		if(c->i2oversion == 0x02)
			len += sprintf(buf+len, "%d",
				       dst->notification_level);
#endif
		len += sprintf(buf+len, "\n");
	}

	spin_unlock(&i2o_proc_lock);
	kfree(result);
	return len;
}


/* Generic group F000h - Params Descriptor (table) */
int i2o_proc_read_groups(char *buf, char **start, off_t offset, int len, 
			 int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;
	int i;
	u8 properties;

	typedef struct _i2o_group_info
	{
		u16 group_number;
		u16 field_count;
		u16 row_count;
		u8  properties;
		u8  reserved;
	} i2o_group_info;

	struct
	{
		u16 result_count;
		u16 pad;
		u16 block_size;
		u8  block_status;
		u8  error_info_size;
		u16 row_count;
		u16 more_flag;
		i2o_group_info group[256];
	} result;

	spin_lock(&i2o_proc_lock);

	len = 0;

	token = i2o_query_table(I2O_PARAMS_TABLE_GET,
				d->controller, d->lct_data.tid, 0xF000, -1, NULL, 0,
				&result, sizeof(result));

	if (token < 0) {
		len = i2o_report_query_status(buf+len, token, "0xF000 Params Descriptor");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf+len, "#  Group   FieldCount RowCount Type   Add Del Clear\n");

	for (i=0; i < result.row_count; i++)
	{
		len += sprintf(buf+len, "%-3d", i);
		len += sprintf(buf+len, "0x%04X ", result.group[i].group_number);
		len += sprintf(buf+len, "%10d ", result.group[i].field_count);
		len += sprintf(buf+len, "%8d ",  result.group[i].row_count);

		properties = result.group[i].properties;
		if (properties & 0x1)	len += sprintf(buf+len, "Table  ");
				else	len += sprintf(buf+len, "Scalar ");
		if (properties & 0x2)	len += sprintf(buf+len, " + ");
				else	len += sprintf(buf+len, " - ");
		if (properties & 0x4)	len += sprintf(buf+len, "  + ");
				else	len += sprintf(buf+len, "  - ");
		if (properties & 0x8)	len += sprintf(buf+len, "  + ");
				else	len += sprintf(buf+len, "  - ");

		len += sprintf(buf+len, "\n");
	}

	if (result.more_flag)
		len += sprintf(buf+len, "There is more...\n");

	spin_unlock(&i2o_proc_lock);

	return len;
}


/* Generic group F001h - Physical Device Table (table) */
int i2o_proc_read_phys_device(char *buf, char **start, off_t offset, int len,
			      int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;
	int i;

	struct
	{
		u16 result_count;
		u16 pad;
		u16 block_size;
		u8  block_status;
		u8  error_info_size;
		u16 row_count;
		u16 more_flag;
		u32 adapter_id[64];
	} result;

	spin_lock(&i2o_proc_lock);
	len = 0;

	token = i2o_query_table(I2O_PARAMS_TABLE_GET,
				d->controller, d->lct_data.tid,
				0xF001, -1, NULL, 0,
				&result, sizeof(result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0xF001 Physical Device Table");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	if (result.row_count)
		len += sprintf(buf+len, "#  AdapterId\n");

	for (i=0; i < result.row_count; i++)
	{
		len += sprintf(buf+len, "%-2d", i);
		len += sprintf(buf+len, "%#7x\n", result.adapter_id[i]);
	}

	if (result.more_flag)
		len += sprintf(buf+len, "There is more...\n");

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* Generic group F002h - Claimed Table (table) */
int i2o_proc_read_claimed(char *buf, char **start, off_t offset, int len,
			  int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;
	int i;

	struct {
		u16 result_count;
		u16 pad;
		u16 block_size;
		u8  block_status;
		u8  error_info_size;
		u16 row_count;
		u16 more_flag;
		u16 claimed_tid[64];
	} result;

	spin_lock(&i2o_proc_lock);
	len = 0;

	token = i2o_query_table(I2O_PARAMS_TABLE_GET,
				d->controller, d->lct_data.tid,
				0xF002, -1, NULL, 0,
				&result, sizeof(result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0xF002 Claimed Table");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	if (result.row_count)
		len += sprintf(buf+len, "#  ClaimedTid\n");

	for (i=0; i < result.row_count; i++)
	{
		len += sprintf(buf+len, "%-2d", i);
		len += sprintf(buf+len, "%#7x\n", result.claimed_tid[i]);
	}

	if (result.more_flag)
		len += sprintf(buf+len, "There is more...\n");

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* Generic group F003h - User Table (table) */
int i2o_proc_read_users(char *buf, char **start, off_t offset, int len,
			int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;
	int i;

	typedef struct _i2o_user_table
	{
		u16 instance;
		u16 user_tid;
		u8 claim_type;
		u8  reserved1;
		u16  reserved2;
	} i2o_user_table;

	struct
	{
		u16 result_count;
		u16 pad;
		u16 block_size;
		u8  block_status;
		u8  error_info_size;
		u16 row_count;
		u16 more_flag;
		i2o_user_table user[64];
	} result;

	spin_lock(&i2o_proc_lock);
	len = 0;

	token = i2o_query_table(I2O_PARAMS_TABLE_GET,
				d->controller, d->lct_data.tid,
				0xF003, -1, NULL, 0,
				&result, sizeof(result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0xF003 User Table");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf+len, "#  Instance UserTid ClaimType\n");

	for(i=0; i < result.row_count; i++)
	{
		len += sprintf(buf+len, "%-3d", i);
		len += sprintf(buf+len, "%#8x ", result.user[i].instance);
		len += sprintf(buf+len, "%#7x ", result.user[i].user_tid);
		len += sprintf(buf+len, "%#9x\n", result.user[i].claim_type);
	}

	if (result.more_flag)
		len += sprintf(buf+len, "There is more...\n");

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* Generic group F005h - Private message extensions (table) (optional) */
int i2o_proc_read_priv_msgs(char *buf, char **start, off_t offset, int len, 
			    int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;
	int i;

	typedef struct _i2o_private
	{
		u16 ext_instance;
		u16 organization_id;
		u16 x_function_code;
	} i2o_private;

	struct
	{
		u16 result_count;
		u16 pad;
		u16 block_size;
		u8  block_status;
		u8  error_info_size;
		u16 row_count;
		u16 more_flag;
		i2o_private extension[64];
	} result;

	spin_lock(&i2o_proc_lock);

	len = 0;

	token = i2o_query_table(I2O_PARAMS_TABLE_GET,
				d->controller, d->lct_data.tid,
				0xF000, -1,
				NULL, 0,
				&result, sizeof(result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0xF005 Private Message Extensions (optional)");
		spin_unlock(&i2o_proc_lock);
		return len;
	}
	
	len += sprintf(buf+len, "Instance#  OrgId  FunctionCode\n");

	for(i=0; i < result.row_count; i++)
	{
		len += sprintf(buf+len, "%0#9x ", result.extension[i].ext_instance);
		len += sprintf(buf+len, "%0#6x ", result.extension[i].organization_id);
		len += sprintf(buf+len, "%0#6x",  result.extension[i].x_function_code);

		len += sprintf(buf+len, "\n");
	}

	if(result.more_flag)
		len += sprintf(buf+len, "There is more...\n");

	spin_unlock(&i2o_proc_lock);

	return len;
}


/* Generic group F006h - Authorized User Table (table) */
int i2o_proc_read_authorized_users(char *buf, char **start, off_t offset, int len,
				   int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;
	int i;

	struct
	{
		u16 result_count;
		u16 pad;
		u16 block_size;
		u8  block_status;
		u8  error_info_size;
		u16 row_count;
		u16 more_flag;
		u32 alternate_tid[64];
	} result;

	spin_lock(&i2o_proc_lock);
	len = 0;

	token = i2o_query_table(I2O_PARAMS_TABLE_GET,
				d->controller, d->lct_data.tid,
				0xF006, -1,
				NULL, 0,
				&result, sizeof(result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0xF006 Autohorized User Table");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	if (result.row_count)
		len += sprintf(buf+len, "#  AlternateTid\n");

	for(i=0; i < result.row_count; i++)
	{
		len += sprintf(buf+len, "%-2d", i);
		len += sprintf(buf+len, "%#7x ", result.alternate_tid[i]);
	}

	if (result.more_flag)
		len += sprintf(buf+len, "There is more...\n");

	spin_unlock(&i2o_proc_lock);
	return len;
}


/* Generic group F100h - Device Identity (scalar) */
int i2o_proc_read_dev_identity(char *buf, char **start, off_t offset, int len, 
			       int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	static u32 work32[128];		// allow for "stuff" + up to 256 byte (max) serial number
					// == (allow) 512d bytes (max)
	static u16 *work16 = (u16*)work32;
	int token;

	spin_lock(&i2o_proc_lock);
	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				0xF100,	-1,
				&work32, sizeof(work32));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token ,"0xF100 Device Identity");
		spin_unlock(&i2o_proc_lock);
		return len;
	}
	
	len += sprintf(buf,     "Device Class  : %s\n", i2o_get_class_name(work16[0]));
	len += sprintf(buf+len, "Owner TID     : %0#5x\n", work16[2]);
	len += sprintf(buf+len, "Parent TID    : %0#5x\n", work16[3]);
	len += sprintf(buf+len, "Vendor info   : %s\n", chtostr((u8 *)(work32+2), 16));
	len += sprintf(buf+len, "Product info  : %s\n", chtostr((u8 *)(work32+6), 16));
	len += sprintf(buf+len, "Description   : %s\n", chtostr((u8 *)(work32+10), 16));
	len += sprintf(buf+len, "Product rev.  : %s\n", chtostr((u8 *)(work32+14), 8));

	len += sprintf(buf+len, "Serial number : ");
	len = print_serial_number(buf, len,
			(u8*)(work32+16),
						/* allow for SNLen plus
						 * possible trailing '\0'
						 */
			sizeof(work32)-(16*sizeof(u32))-2
				);
	len +=  sprintf(buf+len, "\n");

	spin_unlock(&i2o_proc_lock);

	return len;
}


int i2o_proc_read_dev_name(char *buf, char **start, off_t offset, int len,
	int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;

	if ( d->dev_name[0] == '\0' )
		return 0;

	len = sprintf(buf, "%s\n", d->dev_name);

	return len;
}


/* Generic group F101h - DDM Identity (scalar) */
int i2o_proc_read_ddm_identity(char *buf, char **start, off_t offset, int len, 
			      int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;

	struct
	{
		u16 ddm_tid;
		u8 module_name[24];
		u8 module_rev[8];
		u8 sn_format;
		u8 serial_number[12];
		u8 pad[256]; // allow up to 256 byte (max) serial number
	} result;	

	spin_lock(&i2o_proc_lock);
	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid, 
				0xF101,	-1,
				&result, sizeof(result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0xF101 DDM Identity");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf,     "Registering DDM TID : 0x%03x\n", result.ddm_tid);
	len += sprintf(buf+len, "Module name         : %s\n", chtostr(result.module_name, 24));
	len += sprintf(buf+len, "Module revision     : %s\n", chtostr(result.module_rev, 8));

	len += sprintf(buf+len, "Serial number       : ");
	len = print_serial_number(buf, len, result.serial_number, sizeof(result)-36);
				/* allow for SNLen plus possible trailing '\0' */

	len += sprintf(buf+len, "\n");

	spin_unlock(&i2o_proc_lock);

	return len;
}

/* Generic group F102h - User Information (scalar) */
int i2o_proc_read_uinfo(char *buf, char **start, off_t offset, int len, 
			int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;

 	struct
	{
		u8 device_name[64];
		u8 service_name[64];
		u8 physical_location[64];
		u8 instance_number[4];
	} result;

	spin_lock(&i2o_proc_lock);
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				0xF102,	-1,
				&result, sizeof(result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0xF102 User Information");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf,     "Device name     : %s\n", chtostr(result.device_name, 64));
	len += sprintf(buf+len, "Service name    : %s\n", chtostr(result.service_name, 64));
	len += sprintf(buf+len, "Physical name   : %s\n", chtostr(result.physical_location, 64));
	len += sprintf(buf+len, "Instance number : %s\n", chtostr(result.instance_number, 4));

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* Generic group F103h - SGL Operating Limits (scalar) */
int i2o_proc_read_sgl_limits(char *buf, char **start, off_t offset, int len, 
			     int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	static u32 work32[12];
	static u16 *work16 = (u16 *)work32;
	static u8 *work8 = (u8 *)work32;
	int token;

	spin_lock(&i2o_proc_lock);
	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid, 
				 0xF103, -1,
				 &work32, sizeof(work32));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0xF103 SGL Operating Limits");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf,     "SGL chain size        : %d\n", work32[0]);
	len += sprintf(buf+len, "Max SGL chain size    : %d\n", work32[1]);
	len += sprintf(buf+len, "SGL chain size target : %d\n", work32[2]);
	len += sprintf(buf+len, "SGL frag count        : %d\n", work16[6]);
	len += sprintf(buf+len, "Max SGL frag count    : %d\n", work16[7]);
	len += sprintf(buf+len, "SGL frag count target : %d\n", work16[8]);

	if (d->i2oversion == 0x02)
	{
		len += sprintf(buf+len, "SGL data alignment    : %d\n", work16[8]);
		len += sprintf(buf+len, "SGL addr limit        : %d\n", work8[20]);
		len += sprintf(buf+len, "SGL addr sizes supported : ");
		if (work8[21] & 0x01)
			len += sprintf(buf+len, "32 bit ");
		if (work8[21] & 0x02)
			len += sprintf(buf+len, "64 bit ");
		if (work8[21] & 0x04)
			len += sprintf(buf+len, "96 bit ");
		if (work8[21] & 0x08)
			len += sprintf(buf+len, "128 bit ");
		len += sprintf(buf+len, "\n");
	}

	spin_unlock(&i2o_proc_lock);

	return len;
}

/* Generic group F200h - Sensors (scalar) */
int i2o_proc_read_sensors(char *buf, char **start, off_t offset, int len,
			  int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;

	struct
	{
		u16 sensor_instance;
		u8  component;
		u16 component_instance;
		u8  sensor_class;
		u8  sensor_type;
		u8  scaling_exponent;
		u32 actual_reading;
		u32 minimum_reading;
		u32 low2lowcat_treshold;
		u32 lowcat2low_treshold;
		u32 lowwarn2low_treshold;
		u32 low2lowwarn_treshold;
		u32 norm2lowwarn_treshold;
		u32 lowwarn2norm_treshold;
		u32 nominal_reading;
		u32 hiwarn2norm_treshold;
		u32 norm2hiwarn_treshold;
		u32 high2hiwarn_treshold;
		u32 hiwarn2high_treshold;
		u32 hicat2high_treshold;
		u32 hi2hicat_treshold;
		u32 maximum_reading;
		u8  sensor_state;
		u16 event_enable;
	} result;
	
	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0xF200, -1,
				 &result, sizeof(result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0xF200 Sensors (optional)");
		spin_unlock(&i2o_proc_lock);
		return len;
	}
	
	len += sprintf(buf+len, "Sensor instance       : %d\n", result.sensor_instance);

	len += sprintf(buf+len, "Component             : %d = ", result.component);
	switch (result.component)
	{
	case 0:	len += sprintf(buf+len, "Other");		
		break;
	case 1: len += sprintf(buf+len, "Planar logic Board");
		break;
	case 2: len += sprintf(buf+len, "CPU");
		break;
	case 3: len += sprintf(buf+len, "Chassis");
		break;
	case 4: len += sprintf(buf+len, "Power Supply");
		break;
	case 5: len += sprintf(buf+len, "Storage");
		break;
	case 6: len += sprintf(buf+len, "External");
		break;
	}		
	len += sprintf(buf+len,"\n");

	len += sprintf(buf+len, "Component instance    : %d\n", result.component_instance);
	len += sprintf(buf+len, "Sensor class          : %s\n",
				result.sensor_class ? "Analog" : "Digital");
	
	len += sprintf(buf+len, "Sensor type           : %d = ",result.sensor_type);
	switch (result.sensor_type)
	{
	case 0:	len += sprintf(buf+len, "Other\n");
		break;
	case 1: len += sprintf(buf+len, "Thermal\n");
		break;
	case 2: len += sprintf(buf+len, "DC voltage (DC volts)\n");
		break;
	case 3: len += sprintf(buf+len, "AC voltage (AC volts)\n");
		break;
	case 4: len += sprintf(buf+len, "DC current (DC amps)\n");
		break;
	case 5: len += sprintf(buf+len, "AC current (AC volts)\n");
		break;
	case 6: len += sprintf(buf+len, "Door open\n");
		break;
	case 7: len += sprintf(buf+len, "Fan operational\n");
		break;
 	}			

	len += sprintf(buf+len, "Scaling exponent      : %d\n", result.scaling_exponent);
	len += sprintf(buf+len, "Actual reading        : %d\n", result.actual_reading);
	len += sprintf(buf+len, "Minimum reading       : %d\n", result.minimum_reading);
	len += sprintf(buf+len, "Low2LowCat treshold   : %d\n", result.low2lowcat_treshold);
	len += sprintf(buf+len, "LowCat2Low treshold   : %d\n", result.lowcat2low_treshold);
	len += sprintf(buf+len, "LowWarn2Low treshold  : %d\n", result.lowwarn2low_treshold);
	len += sprintf(buf+len, "Low2LowWarn treshold  : %d\n", result.low2lowwarn_treshold);
	len += sprintf(buf+len, "Norm2LowWarn treshold : %d\n", result.norm2lowwarn_treshold);
	len += sprintf(buf+len, "LowWarn2Norm treshold : %d\n", result.lowwarn2norm_treshold);
	len += sprintf(buf+len, "Nominal reading       : %d\n", result.nominal_reading);
	len += sprintf(buf+len, "HiWarn2Norm treshold  : %d\n", result.hiwarn2norm_treshold);
	len += sprintf(buf+len, "Norm2HiWarn treshold  : %d\n", result.norm2hiwarn_treshold);
	len += sprintf(buf+len, "High2HiWarn treshold  : %d\n", result.high2hiwarn_treshold);
	len += sprintf(buf+len, "HiWarn2High treshold  : %d\n", result.hiwarn2high_treshold);
	len += sprintf(buf+len, "HiCat2High treshold   : %d\n", result.hicat2high_treshold);
	len += sprintf(buf+len, "High2HiCat treshold   : %d\n", result.hi2hicat_treshold);
	len += sprintf(buf+len, "Maximum reading       : %d\n", result.maximum_reading);

	len += sprintf(buf+len, "Sensor state          : %d = ", result.sensor_state);
	switch (result.sensor_state)
	{
	case 0:	 len += sprintf(buf+len, "Normal\n");
		 break;
	case 1:  len += sprintf(buf+len, "Abnormal\n");
		 break;
	case 2:  len += sprintf(buf+len, "Unknown\n");
		 break;
	case 3:  len += sprintf(buf+len, "Low Catastrophic (LoCat)\n");
		 break;
	case 4:  len += sprintf(buf+len, "Low (Low)\n");
		 break;
	case 5:  len += sprintf(buf+len, "Low Warning (LoWarn)\n");
		 break;
	case 6:  len += sprintf(buf+len, "High Warning (HiWarn)\n");
		 break;
	case 7:  len += sprintf(buf+len, "High (High)\n");
		 break;
	case 8:  len += sprintf(buf+len, "High Catastrophic (HiCat)\n");
		 break;
	}			

	len += sprintf(buf+len, "Event_enable : 0x%02X\n", result.event_enable);
	len += sprintf(buf+len, "    [%s] Operational state change. \n",
			(result.event_enable & 0x01) ? "+" : "-" );
	len += sprintf(buf+len, "    [%s] Low catastrophic. \n",
			(result.event_enable & 0x02) ? "+" : "-" );
	len += sprintf(buf+len, "    [%s] Low reading. \n",
			(result.event_enable & 0x04) ? "+" : "-" );
	len += sprintf(buf+len, "    [%s] Low warning. \n",
			(result.event_enable & 0x08) ? "+" : "-" );
	len += sprintf(buf+len, "    [%s] Change back to normal from out of range state. \n",
			(result.event_enable & 0x10) ? "+" : "-" );
	len += sprintf(buf+len, "    [%s] High warning. \n",
			(result.event_enable & 0x20) ? "+" : "-" );
	len += sprintf(buf+len, "    [%s] High reading. \n",
			(result.event_enable & 0x40) ? "+" : "-" );
	len += sprintf(buf+len, "    [%s] High catastrophic. \n",
			(result.event_enable & 0x80) ? "+" : "-" );

	spin_unlock(&i2o_proc_lock);
	return len;
}


static int print_serial_number(char *buff, int pos, u8 *serialno, int max_len)
{
	int i;

	/* 19990419 -sralston
	 *	The I2O v1.5 (and v2.0 so far) "official specification"
	 *	got serial numbers WRONG!
	 *	Apparently, and despite what Section 3.4.4 says and
	 *	Figure 3-35 shows (pg 3-39 in the pdf doc),
	 *	the convention / consensus seems to be:
	 *	  + First byte is SNFormat
	 *	  + Second byte is SNLen (but only if SNFormat==7 (?))
	 *	  + (v2.0) SCSI+BS may use IEEE Registered (64 or 128 bit) format
	 */
	switch(serialno[0])
	{
		case I2O_SNFORMAT_BINARY:		/* Binary */
			pos += sprintf(buff+pos, "0x");
			for(i = 0; i < serialno[1]; i++)
			{
				pos += sprintf(buff+pos, "%02X", serialno[2+i]);
			}
			break;
	
		case I2O_SNFORMAT_ASCII:		/* ASCII */
			if ( serialno[1] < ' ' )	/* printable or SNLen? */
			{
				/* sanity */
				max_len = (max_len < serialno[1]) ? max_len : serialno[1];
				serialno[1+max_len] = '\0';

				/* just print it */
				pos += sprintf(buff+pos, "%s", &serialno[2]);
			}
			else
			{
				/* print chars for specified length */
				for(i = 0; i < serialno[1]; i++)
				{
					pos += sprintf(buff+pos, "%c", serialno[2+i]);
				}
			}
			break;

		case I2O_SNFORMAT_UNICODE:		/* UNICODE */
			pos += sprintf(buff+pos, "UNICODE Format.  Can't Display\n");
			break;

		case I2O_SNFORMAT_LAN48_MAC:		/* LAN-48 MAC Address */
			pos += sprintf(buff+pos, 
						"LAN-48 MAC address @ %02X:%02X:%02X:%02X:%02X:%02X",
						serialno[2], serialno[3],
						serialno[4], serialno[5],
						serialno[6], serialno[7]);
			break;

		case I2O_SNFORMAT_WAN:			/* WAN MAC Address */
			/* FIXME: Figure out what a WAN access address looks like?? */
			pos += sprintf(buff+pos, "WAN Access Address");
			break;

/* plus new in v2.0 */
		case I2O_SNFORMAT_LAN64_MAC:		/* LAN-64 MAC Address */
			/* FIXME: Figure out what a LAN-64 address really looks like?? */
			pos += sprintf(buff+pos, 
						"LAN-64 MAC address @ [?:%02X:%02X:?] %02X:%02X:%02X:%02X:%02X:%02X",
						serialno[8], serialno[9],
						serialno[2], serialno[3],
						serialno[4], serialno[5],
						serialno[6], serialno[7]);
			break;


		case I2O_SNFORMAT_DDM:			/* I2O DDM */
			pos += sprintf(buff+pos, 
						"DDM: Tid=%03Xh, Rsvd=%04Xh, OrgId=%04Xh",
						*(u16*)&serialno[2],
						*(u16*)&serialno[4],
						*(u16*)&serialno[6]);
			break;

		case I2O_SNFORMAT_IEEE_REG64:		/* IEEE Registered (64-bit) */
		case I2O_SNFORMAT_IEEE_REG128:		/* IEEE Registered (128-bit) */
			/* FIXME: Figure if this is even close?? */
			pos += sprintf(buff+pos, 
						"IEEE NodeName(hi,lo)=(%08Xh:%08Xh), PortName(hi,lo)=(%08Xh:%08Xh)\n",
						*(u32*)&serialno[2],
						*(u32*)&serialno[6],
						*(u32*)&serialno[10],
						*(u32*)&serialno[14]);
			break;


		case I2O_SNFORMAT_UNKNOWN:		/* Unknown 0    */
		case I2O_SNFORMAT_UNKNOWN2:		/* Unknown 0xff */
		default:
			pos += sprintf(buff+pos, "Unknown data format (0x%02x)",
				       serialno[0]);
			break;
	}

	return pos;
}

const char * i2o_get_connector_type(int conn)
{
	int idx = 16;
	static char *i2o_connector_type[] = {
		"OTHER",
		"UNKNOWN",
		"AUI",
		"UTP",
		"BNC",
		"RJ45",
		"STP DB9",
		"FIBER MIC",
		"APPLE AUI",
		"MII",
		"DB9",
		"HSSDC",
		"DUPLEX SC FIBER",
		"DUPLEX ST FIBER",
		"TNC/BNC",
		"HW DEFAULT"
	};

	switch(conn)
	{
	case 0x00000000:
		idx = 0;
		break;
	case 0x00000001:
		idx = 1;
		break;
	case 0x00000002:
		idx = 2;
		break;
	case 0x00000003:
		idx = 3;
		break;
	case 0x00000004:
		idx = 4;
		break;
	case 0x00000005:
		idx = 5;
		break;
	case 0x00000006:
		idx = 6;
		break;
	case 0x00000007:
		idx = 7;
		break;
	case 0x00000008:
		idx = 8;
		break;
	case 0x00000009:
		idx = 9;
		break;
	case 0x0000000A:
		idx = 10;
		break;
	case 0x0000000B:
		idx = 11;
		break;
	case 0x0000000C:
		idx = 12;
		break;
	case 0x0000000D:
		idx = 13;
		break;
	case 0x0000000E:
		idx = 14;
		break;
	case 0xFFFFFFFF:
		idx = 15;
		break;
	}

	return i2o_connector_type[idx];
}


const char * i2o_get_connection_type(int conn)
{
	int idx = 0;
	static char *i2o_connection_type[] = {
		"Unknown",
		"AUI",
		"10BASE5",
		"FIORL",
		"10BASE2",
		"10BROAD36",
		"10BASE-T",
		"10BASE-FP",
		"10BASE-FB",
		"10BASE-FL",
		"100BASE-TX",
		"100BASE-FX",
		"100BASE-T4",
		"1000BASE-SX",
		"1000BASE-LX",
		"1000BASE-CX",
		"1000BASE-T",
		"100VG-ETHERNET",
		"100VG-TOKEN RING",
		"4MBIT TOKEN RING",
		"16 Mb Token Ring",
		"125 MBAUD FDDI",
		"Point-to-point",
		"Arbitrated loop",
		"Public loop",
		"Fabric",
		"Emulation",
		"Other",
		"HW default"
	};

	switch(conn)
	{
	case I2O_LAN_UNKNOWN:
		idx = 0;
		break;
	case I2O_LAN_AUI:
		idx = 1;
		break;
	case I2O_LAN_10BASE5:
		idx = 2;
		break;
	case I2O_LAN_FIORL:
		idx = 3;
		break;
	case I2O_LAN_10BASE2:
		idx = 4;
		break;
	case I2O_LAN_10BROAD36:
		idx = 5;
		break;
	case I2O_LAN_10BASE_T:
		idx = 6;
		break;
	case I2O_LAN_10BASE_FP:
		idx = 7;
		break;
	case I2O_LAN_10BASE_FB:
		idx = 8;
		break;
	case I2O_LAN_10BASE_FL:
		idx = 9;
		break;
	case I2O_LAN_100BASE_TX:
		idx = 10;
		break;
	case I2O_LAN_100BASE_FX:
		idx = 11;
		break;
	case I2O_LAN_100BASE_T4:
		idx = 12;
		break;
	case I2O_LAN_1000BASE_SX:
		idx = 13;
		break;
	case I2O_LAN_1000BASE_LX:
		idx = 14;
		break;
	case I2O_LAN_1000BASE_CX:
		idx = 15;
		break;
	case I2O_LAN_1000BASE_T:
		idx = 16;
		break;
	case I2O_LAN_100VG_ETHERNET:
		idx = 17;
		break;
	case I2O_LAN_100VG_TR:
		idx = 18;
		break;
	case I2O_LAN_4MBIT:
		idx = 19;
		break;
	case I2O_LAN_16MBIT:
		idx = 20;
		break;
	case I2O_LAN_125MBAUD:
		idx = 21;
		break;
	case I2O_LAN_POINT_POINT:
		idx = 22;
		break;
	case I2O_LAN_ARB_LOOP:
		idx = 23;
		break;
	case I2O_LAN_PUBLIC_LOOP:
		idx = 24;
		break;
	case I2O_LAN_FABRIC:
		idx = 25;
		break;
	case I2O_LAN_EMULATION:
		idx = 26;
		break;
	case I2O_LAN_OTHER:
		idx = 27;
		break;
	case I2O_LAN_DEFAULT:
		idx = 28;
		break;
	}

	return i2o_connection_type[idx];
}


/* LAN group 0000h - Device info (scalar) */
int i2o_proc_read_lan_dev_info(char *buf, char **start, off_t offset, int len, 
			       int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	static u32 work32[56];
	static u8 *work8 = (u8*)work32;
	static u16 *work16 = (u16*)work32;
	static u64 *work64 = (u64*)work32;
	int token;

	spin_lock(&i2o_proc_lock);
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0000, -1, &work32, 56*4);
	if (token < 0) {
		len += i2o_report_query_status(buf+len, token, "0x0000 LAN Device Info");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf, "LAN Type            : ");
	switch (work16[0])
	{
	case 0x0030:
		len += sprintf(buf+len, "Ethernet, ");
		break;
	case 0x0040:
		len += sprintf(buf+len, "100Base VG, ");
		break;
	case 0x0050:
		len += sprintf(buf+len, "Token Ring, ");
		break;
	case 0x0060:
		len += sprintf(buf+len, "FDDI, ");
		break;
	case 0x0070:
		len += sprintf(buf+len, "Fibre Channel, ");
		break;
	default:
		len += sprintf(buf+len, "Unknown type (0x%04x), ", work16[0]);
		break;
	}

	if (work16[1]&0x00000001)
		len += sprintf(buf+len, "emulated LAN, ");
	else
		len += sprintf(buf+len, "physical LAN port, ");

	if (work16[1]&0x00000002)
		len += sprintf(buf+len, "full duplex\n");
	else
		len += sprintf(buf+len, "simplex\n");

	len += sprintf(buf+len, "Address format      : ");
	switch(work8[4]) {
	case 0x00:
		len += sprintf(buf+len, "IEEE 48bit\n");
		break;
	case 0x01:
		len += sprintf(buf+len, "FC IEEE\n");
		break;
	default:
		len += sprintf(buf+len, "Unknown (0x%02x)\n", work8[4]);
		break;
	}

	len += sprintf(buf+len, "State               : ");
	switch(work8[5])
	{
	case 0x00:
		len += sprintf(buf+len, "Unknown\n");
		break;
	case 0x01:
		len += sprintf(buf+len, "Unclaimed\n");
		break;
	case 0x02:
		len += sprintf(buf+len, "Operational\n");
		break;
	case 0x03:
		len += sprintf(buf+len, "Suspended\n");
		break;
	case 0x04:
		len += sprintf(buf+len, "Resetting\n");
		break;
	case 0x05:
		len += sprintf(buf+len, "ERROR: ");
		if(work16[3]&0x0001)
			len += sprintf(buf+len, "TxCU inoperative ");
		if(work16[3]&0x0002)
			len += sprintf(buf+len, "RxCU inoperative ");
		if(work16[3]&0x0004)
			len += sprintf(buf+len, "Local mem alloc ");
		len += sprintf(buf+len, "\n");
		break;
	case 0x06:
		len += sprintf(buf+len, "Operational no Rx\n");
		break;
	case 0x07:
		len += sprintf(buf+len, "Suspended no Rx\n");
		break;
	default:
		len += sprintf(buf+len, "Unspecified\n");
		break;
	}

	len += sprintf(buf+len, "Min packet size     : %d\n", work32[2]);
	len += sprintf(buf+len, "Max packet size     : %d\n", work32[3]);
	len += sprintf(buf+len, "HW address          : "
		       "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
		       work8[16],work8[17],work8[18],work8[19],
		       work8[20],work8[21],work8[22],work8[23]);

	len += sprintf(buf+len, "Max Tx wire speed   : %d bps\n", (int)work64[3]);
	len += sprintf(buf+len, "Max Rx wire speed   : %d bps\n", (int)work64[4]);

	len += sprintf(buf+len, "Min SDU packet size : 0x%08x\n", work32[10]);
	len += sprintf(buf+len, "Max SDU packet size : 0x%08x\n", work32[11]);

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* LAN group 0001h - MAC address table (scalar) */
int i2o_proc_read_lan_mac_addr(char *buf, char **start, off_t offset, int len, 
			       int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	static u32 work32[48];
	static u8 *work8 = (u8*)work32;
	int token;

	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0001, -1, &work32, 48*4);
	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0001 LAN MAC Address");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf,     "Active address          : "
		       "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
		       work8[0],work8[1],work8[2],work8[3],
		       work8[4],work8[5],work8[6],work8[7]);
	len += sprintf(buf+len, "Current address         : "
		       "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
		       work8[8],work8[9],work8[10],work8[11],
		       work8[12],work8[13],work8[14],work8[15]);
	len += sprintf(buf+len, "Functional address mask : "
		       "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
		       work8[16],work8[17],work8[18],work8[19],
		       work8[20],work8[21],work8[22],work8[23]);

	len += sprintf(buf+len,"HW/DDM capabilities : 0x%08x\n", work32[7]);
	len += sprintf(buf+len,"    [%s] Unicast packets supported\n",
		       (work32[7]&0x00000001)?"+":"-");
	len += sprintf(buf+len,"    [%s] Promiscuous mode supported\n",
		       (work32[7]&0x00000002)?"+":"-");
	len += sprintf(buf+len,"    [%s] Promiscuous multicast mode supported\n",
		       (work32[7]&0x00000004)?"+":"-");
	len += sprintf(buf+len,"    [%s] Broadcast reception disabling supported\n",
		       (work32[7]&0x00000100)?"+":"-");
	len += sprintf(buf+len,"    [%s] Multicast reception disabling supported\n",
		       (work32[7]&0x00000200)?"+":"-");
	len += sprintf(buf+len,"    [%s] Functional address disabling supported\n",
		       (work32[7]&0x00000400)?"+":"-");
	len += sprintf(buf+len,"    [%s] MAC reporting supported\n",
		       (work32[7]&0x00000800)?"+":"-");

	len += sprintf(buf+len,"Filter mask : 0x%08x\n", work32[6]);
	len += sprintf(buf+len,"    [%s] Unicast packets disable\n",
		(work32[6]&0x00000001)?"+":"-");
	len += sprintf(buf+len,"    [%s] Promiscuous mode enable\n",
		(work32[6]&0x00000002)?"+":"-");
	len += sprintf(buf+len,"    [%s] Promiscuous multicast mode enable\n",
		(work32[6]&0x00000004)?"+":"-");	
	len += sprintf(buf+len,"    [%s] Broadcast packets disable\n",
		(work32[6]&0x00000100)?"+":"-");
	len += sprintf(buf+len,"    [%s] Multicast packets disable\n",
		(work32[6]&0x00000200)?"+":"-");
	len += sprintf(buf+len,"    [%s] Functional address disable\n",
		       (work32[6]&0x00000400)?"+":"-");
		       
	if (work32[7]&0x00000800) {
		len += sprintf(buf+len, "    MAC reporting mode : ");
		if (work32[6]&0x00000800)
			len += sprintf(buf+len, "Pass only priority MAC packets to user\n");
		else if (work32[6]&0x00001000)
			len += sprintf(buf+len, "Pass all MAC packets to user\n");
		else if (work32[6]&0x00001800)
			len += sprintf(buf+len, "Pass all MAC packets (promiscuous) to user\n");
		else
			len += sprintf(buf+len, "Do not pass MAC packets to user\n");
	}
	len += sprintf(buf+len, "Number of multicast addresses : %d\n", work32[8]);
	len += sprintf(buf+len, "Perfect filtering for max %d multicast addresses\n",
		       work32[9]);
	len += sprintf(buf+len, "Imperfect filtering for max %d multicast addresses\n",
		       work32[10]);

	spin_unlock(&i2o_proc_lock);

	return len;
}

/* LAN group 0002h - Multicast MAC address table (table) */
int i2o_proc_read_lan_mcast_addr(char *buf, char **start, off_t offset,
				 int len, int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;
	int i;
	u8 mc_addr[8];

	struct
	{
		u16 result_count;
		u16 pad;
		u16 block_size;
		u8  block_status;
		u8  error_info_size;
		u16 row_count;
		u16 more_flag;
		u8  mc_addr[256][8];
	} result;	

	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_table(I2O_PARAMS_TABLE_GET,
				d->controller, d->lct_data.tid, 0x0002, -1, 
				NULL, 0, &result, sizeof(result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x002 LAN Multicast MAC Address");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	for (i = 0; i < result.row_count; i++)
	{
		memcpy(mc_addr, result.mc_addr[i], 8);

		len += sprintf(buf+len, "MC MAC address[%d]: "
			       "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
			       i, mc_addr[0], mc_addr[1], mc_addr[2],
			       mc_addr[3], mc_addr[4], mc_addr[5],
			       mc_addr[6], mc_addr[7]);
	}

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* LAN group 0003h - Batch Control (scalar) */
int i2o_proc_read_lan_batch_control(char *buf, char **start, off_t offset,
				    int len, int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	static u32 work32[9];
	int token;

	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0003, -1, &work32, 9*4);
	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0003 LAN Batch Control");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf, "Batch mode ");
	if (work32[0]&0x00000001)
		len += sprintf(buf+len, "disabled");
	else
		len += sprintf(buf+len, "enabled");
	if (work32[0]&0x00000002)
		len += sprintf(buf+len, " (current setting)");
	if (work32[0]&0x00000004)
		len += sprintf(buf+len, ", forced");
	else
		len += sprintf(buf+len, ", toggle");
	len += sprintf(buf+len, "\n");

	len += sprintf(buf+len, "Max Rx batch count : %d\n", work32[5]);
	len += sprintf(buf+len, "Max Rx batch delay : %d\n", work32[6]);
	len += sprintf(buf+len, "Max Tx batch delay : %d\n", work32[7]);
	len += sprintf(buf+len, "Max Tx batch count : %d\n", work32[8]);

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* LAN group 0004h - LAN Operation (scalar) */
int i2o_proc_read_lan_operation(char *buf, char **start, off_t offset, int len,
				int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	static u32 work32[5];
	int token;

	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0004, -1, &work32, 20);
	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0004 LAN Operation");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf, "Packet prepadding (32b words) : %d\n", work32[0]);
	len += sprintf(buf+len, "Transmission error reporting  : %s\n",
		       (work32[1]&1)?"on":"off");
	len += sprintf(buf+len, "Bad packet handling           : %s\n",
				(work32[1]&0x2)?"by host":"by DDM");
	len += sprintf(buf+len, "Packet orphan limit           : %d\n", work32[2]);

	len += sprintf(buf+len, "Tx modes : 0x%08x\n", work32[3]);
	len += sprintf(buf+len, "    [%s] HW CRC suppression\n",
			(work32[3]&0x00000004) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] HW IPv4 checksum\n",
			(work32[3]&0x00000100) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] HW TCP checksum\n",
			(work32[3]&0x00000200) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] HW UDP checksum\n",
			(work32[3]&0x00000400) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] HW RSVP checksum\n",
			(work32[3]&0x00000800) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] HW ICMP checksum\n",
			(work32[3]&0x00001000) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] Loopback suppression enable\n",
			(work32[3]&0x00002000) ? "+" : "-");

	len += sprintf(buf+len, "Rx modes : 0x%08x\n", work32[4]);
	len += sprintf(buf+len, "    [%s] FCS in payload\n",
			(work32[4]&0x00000004) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] HW IPv4 checksum validation\n",
			(work32[4]&0x00000100) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] HW TCP checksum validation\n",
			(work32[4]&0x00000200) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] HW UDP checksum validation\n",
			(work32[4]&0x00000400) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] HW RSVP checksum validation\n",
			(work32[4]&0x00000800) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] HW ICMP checksum validation\n",
			(work32[4]&0x00001000) ? "+" : "-");
 
	spin_unlock(&i2o_proc_lock);
	return len;
}

/* LAN group 0005h - Media operation (scalar) */
int i2o_proc_read_lan_media_operation(char *buf, char **start, off_t offset,
				      int len, int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;

	struct
	{
		u32 connector_type;
		u32 connection_type;
		u64 current_tx_wire_speed;
		u64 current_rx_wire_speed;
		u8  duplex_mode;
		u8  link_status;
		u8  reserved;
		u8  duplex_mode_target;
		u32 connector_type_target;
		u32 connection_type_target;
	} result;	

	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0005, -1, &result, sizeof(result));
	if (token < 0) {
		len += i2o_report_query_status(buf+len, token, "0x0005 LAN Media Operation");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf, "Connector type         : %s\n",
		       i2o_get_connector_type(result.connector_type));
	len += sprintf(buf+len, "Connection type        : %s\n",
		       i2o_get_connection_type(result.connection_type));

	len += sprintf(buf+len, "Current Tx wire speed  : %d bps\n", (int)result.current_tx_wire_speed);
	len += sprintf(buf+len, "Current Rx wire speed  : %d bps\n", (int)result.current_rx_wire_speed);
	len += sprintf(buf+len, "Duplex mode            : %s duplex\n",
			(result.duplex_mode)?"Full":"Half");
			
	len += sprintf(buf+len, "Link status            : ");
	switch (result.link_status)
	{
	case 0x00:
		len += sprintf(buf+len, "Unknown\n");
		break;
	case 0x01:
		len += sprintf(buf+len, "Normal\n");
		break;
	case 0x02:
		len += sprintf(buf+len, "Failure\n");
		break;
	case 0x03:
		len += sprintf(buf+len, "Reset\n");
		break;
	default:
		len += sprintf(buf+len, "Unspecified\n");
	}
	
	len += sprintf(buf+len, "Duplex mode target     : ");
	switch (result.duplex_mode_target){
		case 0:
			len += sprintf(buf+len, "Half duplex\n");
			break;
		case 1:
			len += sprintf(buf+len, "Full duplex\n");
			break;
		default:
			len += sprintf(buf+len, "\n");
	}

	len += sprintf(buf+len, "Connector type target  : %s\n",
		       i2o_get_connector_type(result.connector_type_target));
	len += sprintf(buf+len, "Connection type target : %s\n",
		       i2o_get_connection_type(result.connection_type_target));

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* LAN group 0006h - Alternate address (table) (optional) */
int i2o_proc_read_lan_alt_addr(char *buf, char **start, off_t offset, int len,
			       int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;
	int i;
	u8 alt_addr[8];
	struct
	{
		u16 result_count;
		u16 pad;
		u16 block_size;
		u8  block_status;
		u8  error_info_size;
		u16 row_count;
		u16 more_flag;
		u8  alt_addr[256][8];
	} result;	

	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_table(I2O_PARAMS_TABLE_GET,
				d->controller, d->lct_data.tid,
				0x0006, -1, NULL, 0, &result, sizeof(result));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token, "0x0006 LAN Alternate Address (optional)");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	for (i=0; i < result.row_count; i++)
	{
		memcpy(alt_addr,result.alt_addr[i],8);
		len += sprintf(buf+len, "Alternate address[%d]: "
			       "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
			       i, alt_addr[0], alt_addr[1], alt_addr[2],
			       alt_addr[3], alt_addr[4], alt_addr[5],
			       alt_addr[6], alt_addr[7]);
	}

	spin_unlock(&i2o_proc_lock);
	return len;
}


/* LAN group 0007h - Transmit info (scalar) */
int i2o_proc_read_lan_tx_info(char *buf, char **start, off_t offset, int len, 
			      int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	static u32 work32[8];
	int token;

	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0007, -1, &work32, 8*4);
	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0007 LAN Transmit Info");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf,     "Tx Max SG elements per packet : %d\n", work32[0]);
	len += sprintf(buf+len, "Tx Max SG elements per chain  : %d\n", work32[1]);
	len += sprintf(buf+len, "Tx Max outstanding packets    : %d\n", work32[2]);
	len += sprintf(buf+len, "Tx Max packets per request    : %d\n", work32[3]);

	len += sprintf(buf+len, "Tx modes : 0x%08x\n", work32[4]);
	len += sprintf(buf+len, "    [%s] No DA in SGL\n",
				(work32[4]&0x00000002) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] CRC suppression\n",
				(work32[4]&0x00000004) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] MAC insertion\n",
				(work32[4]&0x00000010) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] RIF insertion\n",
				(work32[4]&0x00000020) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] IPv4 checksum generation\n",
				(work32[4]&0x00000100) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] TCP checksum generation\n",
				(work32[4]&0x00000200) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] UDP checksum generation\n",
				(work32[4]&0x00000400) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] RSVP checksum generation\n",
				(work32[4]&0x00000800) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] ICMP checksum generation\n",
				(work32[4]&0x00001000) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] Loopback enabled\n",
				(work32[4]&0x00010000) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] Loopback suppression enabled\n",
				(work32[4]&0x00020000) ? "+" : "-");

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* LAN group 0008h - Receive info (scalar) */
int i2o_proc_read_lan_rx_info(char *buf, char **start, off_t offset, int len, 
			      int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	static u32 work32[8];
	int token;

	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0008, -1, &work32, 8*4);
	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0008 LAN Receive Info");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf     ,"Rx Max size of chain element : %d\n", work32[0]);
	len += sprintf(buf+len, "Rx Max Buckets               : %d\n", work32[1]);
	len += sprintf(buf+len, "Rx Max Buckets in Reply      : %d\n", work32[3]);
	len += sprintf(buf+len, "Rx Max Packets in Bucket     : %d\n", work32[4]);
	len += sprintf(buf+len, "Rx Max Buckets in Post       : %d\n", work32[5]);

	len += sprintf(buf+len, "Rx Modes : 0x%08x\n", work32[2]);
	len += sprintf(buf+len, "    [%s] FCS reception\n",
				(work32[2]&0x00000004) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] IPv4 checksum validation \n",
				(work32[2]&0x00000100) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] TCP checksum validation \n",
				(work32[2]&0x00000200) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] UDP checksum validation \n",
				(work32[2]&0x00000400) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] RSVP checksum validation \n",
				(work32[2]&0x00000800) ? "+" : "-");
	len += sprintf(buf+len, "    [%s] ICMP checksum validation \n",
				(work32[2]&0x00001000) ? "+" : "-");

	spin_unlock(&i2o_proc_lock);
	return len;
}

static int i2o_report_opt_field(char *buf, char *field_name,
				int field_nbr, int supp_fields, u64 *value)
{
	if (supp_fields & (1 << field_nbr))
		return sprintf(buf, "%-24s : " FMT_U64_HEX "\n", field_name, U64_VAL(value));
	else	
		return sprintf(buf, "%-24s : Not supported\n", field_name);	
}

/* LAN group 0100h - LAN Historical statistics (scalar) */
/* LAN group 0180h - Supported Optional Historical Statistics (scalar) */
/* LAN group 0182h - Optional Non Media Specific Transmit Historical Statistics (scalar) */
/* LAN group 0183h - Optional Non Media Specific Receive Historical Statistics (scalar) */

int i2o_proc_read_lan_hist_stats(char *buf, char **start, off_t offset, int len,
				 int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;

	struct
	{
		u64 tx_packets;
		u64 tx_bytes;
		u64 rx_packets;
		u64 rx_bytes;
		u64 tx_errors;
		u64 rx_errors;
		u64 rx_dropped;
		u64 adapter_resets;
		u64 adapter_suspends;
	} stats;			// 0x0100

	static u64 supp_groups[4];	// 0x0180

	struct
	{
		u64 tx_retries;
		u64 tx_directed_bytes;
		u64 tx_directed_packets;
		u64 tx_multicast_bytes;
		u64 tx_multicast_packets;
		u64 tx_broadcast_bytes;
		u64 tx_broadcast_packets;
		u64 tx_group_addr_packets;
		u64 tx_short_packets;
	} tx_stats;			// 0x0182

	struct
	{
		u64 rx_crc_errors;
		u64 rx_directed_bytes;
		u64 rx_directed_packets;
		u64 rx_multicast_bytes;
		u64 rx_multicast_packets;
		u64 rx_broadcast_bytes;
		u64 rx_broadcast_packets;
		u64 rx_group_addr_packets;
		u64 rx_short_packets;
		u64 rx_long_packets;
		u64 rx_runt_packets;
	} rx_stats;			// 0x0183

	struct
	{
		u64 ipv4_generate;
		u64 ipv4_validate_success;
		u64 ipv4_validate_errors;
		u64 tcp_generate;
		u64 tcp_validate_success;
		u64 tcp_validate_errors;
		u64 udp_generate;
		u64 udp_validate_success;
		u64 udp_validate_errors;
		u64 rsvp_generate;
		u64 rsvp_validate_success;
		u64 rsvp_validate_errors;		
		u64 icmp_generate;
		u64 icmp_validate_success;
		u64 icmp_validate_errors;
	} chksum_stats;			// 0x0184

	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0100, -1, &stats, sizeof(stats));
	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x100 LAN Statistics");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf+len, "Tx packets       : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.tx_packets));
	len += sprintf(buf+len, "Tx bytes         : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.tx_bytes));
	len += sprintf(buf+len, "Rx packets       : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.rx_packets));
	len += sprintf(buf+len, "Rx bytes         : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.rx_bytes));
	len += sprintf(buf+len, "Tx errors        : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.tx_errors));
	len += sprintf(buf+len, "Rx errors        : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.rx_errors));
	len += sprintf(buf+len, "Rx dropped       : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.rx_dropped));
	len += sprintf(buf+len, "Adapter resets   : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.adapter_resets));
	len += sprintf(buf+len, "Adapter suspends : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.adapter_suspends));

	/* Optional statistics follows */
	/* Get 0x0180 to see which optional groups/fields are supported */

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0180, -1, &supp_groups, sizeof(supp_groups));
	
	if (token < 0) {
		len += i2o_report_query_status(buf+len, token, "0x180 LAN Supported Optional Statistics");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	if (supp_groups[1]) /* 0x0182 */
	{
		token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 	0x0182, -1, &tx_stats, sizeof(tx_stats));

		if (token < 0) {
			len += i2o_report_query_status(buf+len, token,"0x182 LAN Optional Tx Historical Statistics");
			spin_unlock(&i2o_proc_lock);
			return len;
		}

		len += sprintf(buf+len, "==== Optional TX statistics (group 0182h)\n");

		len += i2o_report_opt_field(buf+len, "Tx RetryCount",
					0, supp_groups[1], &tx_stats.tx_retries);
		len += i2o_report_opt_field(buf+len, "Tx DirectedBytes",
					1, supp_groups[1], &tx_stats.tx_directed_bytes);
		len += i2o_report_opt_field(buf+len, "Tx DirectedPackets",
					2, supp_groups[1], &tx_stats.tx_directed_packets);
		len += i2o_report_opt_field(buf+len, "Tx MulticastBytes",
					3, supp_groups[1], &tx_stats.tx_multicast_bytes);
		len += i2o_report_opt_field(buf+len, "Tx MulticastPackets",
					4, supp_groups[1], &tx_stats.tx_multicast_packets);
		len += i2o_report_opt_field(buf+len, "Tx BroadcastBytes",
					5, supp_groups[1], &tx_stats.tx_broadcast_bytes);
		len += i2o_report_opt_field(buf+len, "Tx BroadcastPackets",
					6, supp_groups[1], &tx_stats.tx_broadcast_packets);
		len += i2o_report_opt_field(buf+len, "Tx TotalGroupAddrPackets",
					7, supp_groups[1], &tx_stats.tx_group_addr_packets);
		len += i2o_report_opt_field(buf+len, "Tx TotalPacketsTooShort",
					8, supp_groups[1], &tx_stats.tx_short_packets);
	}

	if (supp_groups[2]) /* 0x0183 */
	{
		token = i2o_query_scalar(d->controller, d->lct_data.tid,
					 0x0183, -1, &rx_stats, sizeof(rx_stats));
		if (token < 0) {
			len += i2o_report_query_status(buf+len, token,"0x183 LAN Optional Rx Historical Stats");
			spin_unlock(&i2o_proc_lock);
			return len;
		}

		len += sprintf(buf+len, "==== Optional RX statistics (group 0183h)\n");

		len += i2o_report_opt_field(buf+len, "Rx CRCErrorCount",
					0, supp_groups[2], &rx_stats.rx_crc_errors);
		len += i2o_report_opt_field(buf+len, "Rx DirectedBytes",
					1, supp_groups[2], &rx_stats.rx_directed_bytes);
		len += i2o_report_opt_field(buf+len, "Rx DirectedPackets",
					2, supp_groups[2], &rx_stats.rx_directed_packets);
		len += i2o_report_opt_field(buf+len, "Rx MulticastBytes",
					3, supp_groups[2], &rx_stats.rx_multicast_bytes);
		len += i2o_report_opt_field(buf+len, "Rx MulticastPackets",
					4, supp_groups[2], &rx_stats.rx_multicast_packets);
		len += i2o_report_opt_field(buf+len, "Rx BroadcastBytes",
					5, supp_groups[2], &rx_stats.rx_broadcast_bytes);
		len += i2o_report_opt_field(buf+len, "Rx BroadcastPackets",
					6, supp_groups[2], &rx_stats.rx_broadcast_packets);
		len += i2o_report_opt_field(buf+len, "Rx TotalGroupAddrPackets",
					7, supp_groups[2], &rx_stats.rx_group_addr_packets);
		len += i2o_report_opt_field(buf+len, "Rx TotalPacketsTooShort",
					8, supp_groups[2], &rx_stats.rx_short_packets);
		len += i2o_report_opt_field(buf+len, "Rx TotalPacketsTooLong",
					9, supp_groups[2], &rx_stats.rx_long_packets);
		len += i2o_report_opt_field(buf+len, "Rx TotalPacketsRunt",
					10, supp_groups[2], &rx_stats.rx_runt_packets);
	}
	
	if (supp_groups[3]) /* 0x0184 */
	{
		token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 	0x0184, -1, &chksum_stats, sizeof(chksum_stats));

		if (token < 0) {
			len += i2o_report_query_status(buf+len, token,"0x184 LAN Optional Chksum Historical Stats");
			spin_unlock(&i2o_proc_lock);
			return len;
		}

		len += sprintf(buf+len, "==== Optional CHKSUM statistics (group 0x0184)\n");

		len += i2o_report_opt_field(buf+len, "IPv4 Generate",
					0, supp_groups[3], &chksum_stats.ipv4_generate);
		len += i2o_report_opt_field(buf+len, "IPv4 ValidateSuccess",
					1, supp_groups[3], &chksum_stats.ipv4_validate_success);
		len += i2o_report_opt_field(buf+len, "IPv4 ValidateError",
					2, supp_groups[3], &chksum_stats.ipv4_validate_errors);
		len += i2o_report_opt_field(buf+len, "TCP  Generate",
					3, supp_groups[3], &chksum_stats.tcp_generate);
		len += i2o_report_opt_field(buf+len, "TCP  ValidateSuccess",
					4, supp_groups[3], &chksum_stats.tcp_validate_success);
		len += i2o_report_opt_field(buf+len, "TCP  ValidateError",
					5, supp_groups[3], &chksum_stats.tcp_validate_errors);
		len += i2o_report_opt_field(buf+len, "UDP  Generate",
					6, supp_groups[3], &chksum_stats.udp_generate);
		len += i2o_report_opt_field(buf+len, "UDP  ValidateSuccess",
					7, supp_groups[3], &chksum_stats.udp_validate_success);
		len += i2o_report_opt_field(buf+len, "UDP  ValidateError",
					8, supp_groups[3], &chksum_stats.udp_validate_errors);
		len += i2o_report_opt_field(buf+len, "RSVP Generate",
					9, supp_groups[3], &chksum_stats.rsvp_generate);
		len += i2o_report_opt_field(buf+len, "RSVP ValidateSuccess",
					10, supp_groups[3], &chksum_stats.rsvp_validate_success);
		len += i2o_report_opt_field(buf+len, "RSVP ValidateError",
					11, supp_groups[3], &chksum_stats.rsvp_validate_errors);
		len += i2o_report_opt_field(buf+len, "ICMP Generate",
					12, supp_groups[3], &chksum_stats.icmp_generate);
		len += i2o_report_opt_field(buf+len, "ICMP ValidateSuccess",
					13, supp_groups[3], &chksum_stats.icmp_validate_success);
		len += i2o_report_opt_field(buf+len, "ICMP ValidateError",
					14, supp_groups[3], &chksum_stats.icmp_validate_errors);
	}

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* LAN group 0200h - Required Ethernet Statistics (scalar) */
/* LAN group 0280h - Optional Ethernet Statistics Supported (scalar) */
/* LAN group 0281h - Optional Ethernet Historical Statistics (scalar) */
int i2o_proc_read_lan_eth_stats(char *buf, char **start, off_t offset,
				int len, int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	int token;

	struct
	{
		u64 rx_align_errors;
		u64 tx_one_collisions;
		u64 tx_multiple_collisions;
		u64 tx_deferred;
		u64 tx_late_collisions;
		u64 tx_max_collisions;
		u64 tx_carrier_lost;
		u64 tx_excessive_deferrals;
	} stats;	

	static u64 supp_fields;
	struct
	{
		u64 rx_overrun;
		u64 tx_underrun;
		u64 tx_heartbeat_failure;	
	} hist_stats;

	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0200, -1, &stats, sizeof(stats));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0200 LAN Ethernet Statistics");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf+len, "Rx alignment errors    : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.rx_align_errors));
	len += sprintf(buf+len, "Tx one collisions      : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.tx_one_collisions));
	len += sprintf(buf+len, "Tx multicollisions     : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.tx_multiple_collisions));
	len += sprintf(buf+len, "Tx deferred            : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.tx_deferred));
	len += sprintf(buf+len, "Tx late collisions     : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.tx_late_collisions));
	len += sprintf(buf+len, "Tx max collisions      : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.tx_max_collisions));
	len += sprintf(buf+len, "Tx carrier lost        : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.tx_carrier_lost));
	len += sprintf(buf+len, "Tx excessive deferrals : " FMT_U64_HEX "\n",
		       U64_VAL(&stats.tx_excessive_deferrals));

	/* Optional Ethernet statistics follows  */
	/* Get 0x0280 to see which optional fields are supported */

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0280, -1, &supp_fields, sizeof(supp_fields));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0280 LAN Supported Optional Ethernet Statistics");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	if (supp_fields) /* 0x0281 */
	{
		token = i2o_query_scalar(d->controller, d->lct_data.tid,
					 0x0281, -1, &stats, sizeof(stats));

		if (token < 0) {
			len += i2o_report_query_status(buf+len, token,"0x0281 LAN Optional Ethernet Statistics");
			spin_unlock(&i2o_proc_lock);
			return len;
		}

		len += sprintf(buf+len, "==== Optional ETHERNET statistics (group 0x0281)\n");

		len += i2o_report_opt_field(buf+len, "Rx Overrun",
					0, supp_fields, &hist_stats.rx_overrun);
		len += i2o_report_opt_field(buf+len, "Tx Underrun",
					1, supp_fields, &hist_stats.tx_underrun);
		len += i2o_report_opt_field(buf+len, "Tx HeartbeatFailure",
					2, supp_fields, &hist_stats.tx_heartbeat_failure);
	}

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* LAN group 0300h - Required Token Ring Statistics (scalar) */
/* LAN group 0380h, 0381h - Optional Statistics not yet defined (TODO) */
int i2o_proc_read_lan_tr_stats(char *buf, char **start, off_t offset,
			       int len, int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	static u64 work64[13];
	int token;

	static char *ring_status[] =
	{
		"",
		"",
		"",
		"",
		"",
		"Ring Recovery",
		"Single Station",
		"Counter Overflow",
		"Remove Received",
		"",
		"Auto-Removal Error 1",
		"Lobe Wire Fault",
		"Transmit Beacon",
		"Soft Error",
		"Hard Error",
		"Signal Loss"
	};

	spin_lock(&i2o_proc_lock);	
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0300, -1, &work64, sizeof(work64));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0300 Token Ring Statistics");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf,     "LineErrors          : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[0]));
	len += sprintf(buf+len, "LostFrames          : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[1]));
	len += sprintf(buf+len, "ACError             : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[2]));
	len += sprintf(buf+len, "TxAbortDelimiter    : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[3]));
	len += sprintf(buf+len, "BursErrors          : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[4]));
	len += sprintf(buf+len, "FrameCopiedErrors   : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[5]));
	len += sprintf(buf+len, "FrequencyErrors     : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[6]));
	len += sprintf(buf+len, "InternalErrors      : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[7]));
	len += sprintf(buf+len, "LastRingStatus      : %s\n", ring_status[work64[8]]);
	len += sprintf(buf+len, "TokenError          : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[9]));
	len += sprintf(buf+len, "UpstreamNodeAddress : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[10]));
	len += sprintf(buf+len, "LastRingID          : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[11]));
	len += sprintf(buf+len, "LastBeaconType      : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[12]));

	spin_unlock(&i2o_proc_lock);
	return len;
}

/* LAN group 0400h - Required FDDI Statistics (scalar) */
/* LAN group 0480h, 0481h - Optional Statistics, not yet defined (TODO) */
int i2o_proc_read_lan_fddi_stats(char *buf, char **start, off_t offset,
				 int len, int *eof, void *data)
{
	struct i2o_device *d = (struct i2o_device*)data;
	static u64 work64[11];
	int token;

	static char *conf_state[] =
	{
		"Isolated",
		"Local a",
		"Local b",
		"Local ab",
		"Local s",
		"Wrap a",
		"Wrap b",
		"Wrap ab",
		"Wrap s",
		"C-Wrap a",
		"C-Wrap b",
		"C-Wrap s",
		"Through",
	};

	static char *ring_state[] =
	{
		"Isolated",
		"Non-op",
		"Rind-op",
		"Detect",
		"Non-op-Dup",
		"Ring-op-Dup",
		"Directed",
		"Trace"
	};

	static char *link_state[] =
	{
		"Off",
		"Break",
		"Trace",
		"Connect",
		"Next",
		"Signal",
		"Join",
		"Verify",
		"Active",
		"Maintenance"
	};

	spin_lock(&i2o_proc_lock);
	len = 0;

	token = i2o_query_scalar(d->controller, d->lct_data.tid,
				 0x0400, -1, &work64, sizeof(work64));

	if (token < 0) {
		len += i2o_report_query_status(buf+len, token,"0x0400 FDDI Required Statistics");
		spin_unlock(&i2o_proc_lock);
		return len;
	}

	len += sprintf(buf+len, "ConfigurationState : %s\n", conf_state[work64[0]]);
	len += sprintf(buf+len, "UpstreamNode       : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[1]));
	len += sprintf(buf+len, "DownStreamNode     : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[2]));
	len += sprintf(buf+len, "FrameErrors        : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[3]));
	len += sprintf(buf+len, "FramesLost         : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[4]));
	len += sprintf(buf+len, "RingMgmtState      : %s\n", ring_state[work64[5]]);
	len += sprintf(buf+len, "LCTFailures        : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[6]));
	len += sprintf(buf+len, "LEMRejects         : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[7]));
	len += sprintf(buf+len, "LEMCount           : " FMT_U64_HEX "\n",
		       U64_VAL(&work64[8]));
	len += sprintf(buf+len, "LConnectionState   : %s\n",
		       link_state[work64[9]]);

	spin_unlock(&i2o_proc_lock);
	return len;
}

static int i2o_proc_create_entries(void *data, i2o_proc_entry *pentry,
				   struct proc_dir_entry *parent)
{
	struct proc_dir_entry *ent;
	
	while(pentry->name != NULL)
	{
		ent = create_proc_entry(pentry->name, pentry->mode, parent);
		if(!ent) return -1;

		ent->data = data;
		ent->read_proc = pentry->read_proc;
		ent->write_proc = pentry->write_proc;
		ent->nlink = 1;

		pentry++;
	}

	return 0;
}

static void i2o_proc_remove_entries(i2o_proc_entry *pentry, 
				    struct proc_dir_entry *parent)
{
	while(pentry->name != NULL)
	{
		remove_proc_entry(pentry->name, parent);
		pentry++;
	}
}

static int i2o_proc_add_controller(struct i2o_controller *pctrl, 
				   struct proc_dir_entry *root )
{
	struct proc_dir_entry *dir, *dir1;
	struct i2o_device *dev;
	char buff[10];

	sprintf(buff, "iop%d", pctrl->unit);

	dir = proc_mkdir(buff, root);
	if(!dir)
		return -1;

	pctrl->proc_entry = dir;

	i2o_proc_create_entries(pctrl, generic_iop_entries, dir);
	
	for(dev = pctrl->devices; dev; dev = dev->next)
	{
		sprintf(buff, "%0#5x", dev->lct_data.tid);

		dir1 = proc_mkdir(buff, dir);
		dev->proc_entry = dir1;

		if(!dir1)
			printk(KERN_INFO "i2o_proc: Could not allocate proc dir\n");

		i2o_proc_add_device(dev, dir1);
	}

	return 0;
}

void i2o_proc_new_dev(struct i2o_controller *c, struct i2o_device *d)
{
	char buff[10];

#ifdef DRIVERDEBUG
	printk(KERN_INFO "Adding new device to /proc/i2o/iop%d\n", c->unit);
#endif
	sprintf(buff, "%0#5x", d->lct_data.tid);

	d->proc_entry = proc_mkdir(buff, c->proc_entry);

	if(!d->proc_entry)
	{
		printk(KERN_WARNING "i2o: Could not allocate procdir!\n");
		return;
	}

	i2o_proc_add_device(d, d->proc_entry);
}

void i2o_proc_add_device(struct i2o_device *dev, struct proc_dir_entry *dir)
{	
	i2o_proc_create_entries(dev, generic_dev_entries, dir);

	/* Inform core that we want updates about this device's status */
	i2o_device_notify_on(dev, &i2o_proc_handler);
	switch(dev->lct_data.class_id)
	{
		case I2O_CLASS_SCSI_PERIPHERAL:
		case I2O_CLASS_RANDOM_BLOCK_STORAGE:
			i2o_proc_create_entries(dev, rbs_dev_entries, dir);
			break;
		case I2O_CLASS_LAN:
			i2o_proc_create_entries(dev, lan_entries, dir);
			switch(dev->lct_data.sub_class)
			{
				case I2O_LAN_ETHERNET:
					i2o_proc_create_entries(dev, lan_eth_entries, dir);
					break;
				case I2O_LAN_FDDI:
					i2o_proc_create_entries(dev, lan_fddi_entries, dir);
					break;
				case I2O_LAN_TR:
					i2o_proc_create_entries(dev, lan_tr_entries, dir);
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
}

static void i2o_proc_remove_controller(struct i2o_controller *pctrl, 
				       struct proc_dir_entry *parent)
{
	char buff[10];
	struct i2o_device *dev;

	/* Remove unused device entries */
	for(dev=pctrl->devices; dev; dev=dev->next)
		i2o_proc_remove_device(dev);

	if(!atomic_read(&pctrl->proc_entry->count))
	{
		sprintf(buff, "iop%d", pctrl->unit);

		i2o_proc_remove_entries(generic_iop_entries, pctrl->proc_entry);

		remove_proc_entry(buff, parent);
		pctrl->proc_entry = NULL;
	}
}

void i2o_proc_remove_device(struct i2o_device *dev)
{
	struct proc_dir_entry *de=dev->proc_entry;
	char dev_id[10];

	sprintf(dev_id, "%0#5x", dev->lct_data.tid);

	i2o_device_notify_off(dev, &i2o_proc_handler);
	/* Would it be safe to remove _files_ even if they are in use? */
	if((de) && (!atomic_read(&de->count)))
	{
		i2o_proc_remove_entries(generic_dev_entries, de);
		switch(dev->lct_data.class_id)
		{
			case I2O_CLASS_SCSI_PERIPHERAL:
			case I2O_CLASS_RANDOM_BLOCK_STORAGE:
				i2o_proc_remove_entries(rbs_dev_entries, de);
				break;
			case I2O_CLASS_LAN:
			{
				i2o_proc_remove_entries(lan_entries, de);
				switch(dev->lct_data.sub_class)
				{
				case I2O_LAN_ETHERNET:
					i2o_proc_remove_entries(lan_eth_entries, de);
					break;
				case I2O_LAN_FDDI:
					i2o_proc_remove_entries(lan_fddi_entries, de);
					break;
				case I2O_LAN_TR:
					i2o_proc_remove_entries(lan_tr_entries, de);
					break;
				}
			}
			remove_proc_entry(dev_id, dev->controller->proc_entry);
		}
	}
}
	
void i2o_proc_dev_del(struct i2o_controller *c, struct i2o_device *d)
{
#ifdef DRIVERDEBUG
	printk(KERN_INFO "Deleting device %d from iop%d\n", 
		d->lct_data.tid, c->unit);
#endif

	i2o_proc_remove_device(d);
}

static int create_i2o_procfs(void)
{
	struct i2o_controller *pctrl = NULL;
	int i;

	i2o_proc_dir_root = proc_mkdir("i2o", 0);
	if(!i2o_proc_dir_root)
		return -1;

	for(i = 0; i < MAX_I2O_CONTROLLERS; i++)
	{
		pctrl = i2o_find_controller(i);
		if(pctrl)
		{
			i2o_proc_add_controller(pctrl, i2o_proc_dir_root);
			i2o_unlock_controller(pctrl);
		}
	};

	return 0;
}

static int __exit destroy_i2o_procfs(void)
{
	struct i2o_controller *pctrl = NULL;
	int i;

	for(i = 0; i < MAX_I2O_CONTROLLERS; i++)
	{
		pctrl = i2o_find_controller(i);
		if(pctrl)
		{
			i2o_proc_remove_controller(pctrl, i2o_proc_dir_root);
			i2o_unlock_controller(pctrl);
		}
	}

	if(!atomic_read(&i2o_proc_dir_root->count))
		remove_proc_entry("i2o", 0);
	else
		return -1;

	return 0;
}

int __init i2o_proc_init(void)
{
	if (i2o_install_handler(&i2o_proc_handler) < 0)
	{
		printk(KERN_ERR "i2o_proc: Unable to install PROC handler.\n");
		return 0;
	}

	if(create_i2o_procfs())
		return -EBUSY;

	return 0;
}

MODULE_AUTHOR("Deepak Saxena");
MODULE_DESCRIPTION("I2O procfs Handler");
MODULE_LICENSE("GPL");

static void __exit i2o_proc_exit(void)
{
	destroy_i2o_procfs();
	i2o_remove_handler(&i2o_proc_handler);
}

module_init(i2o_proc_init);
module_exit(i2o_proc_exit);
