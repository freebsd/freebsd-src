/*
 *	Adaptec AAC series RAID controller driver
 *	(c) Copyright 2001 Red Hat Inc.	<alan@redhat.com>
 *
 * based on the old aacraid driver that is..
 * Adaptec aacraid device driver for Linux.
 *
 * Copyright (c) 2000 Adaptec, Inc. (aacraid@adaptec.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Module Name:
 *   linit.c
 *
 * Abstract: Linux Driver entry module for Adaptec RAID Array Controller
 *				
 *	Provides the following driver entry points:
 *		aac_detect()
 *		aac_release()
 *		aac_queuecommand()
 *		aac_resetcommand()
 *		aac_biosparm()
 *	
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <asm/semaphore.h>
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"

#include "aacraid.h"
#include "sd.h"

#define AAC_DRIVER_NAME	"aacraid"

MODULE_AUTHOR("Red Hat Inc and Adaptec");
MODULE_DESCRIPTION("Supports Dell PERC2, 2/Si, 3/Si, 3/Di, Adaptec Advanced Raid Products, and HP NetRAID-4M devices. http://domsch.com/linux/ or http://linux.adaptec.com");
MODULE_LICENSE("GPL");

struct aac_dev *aac_devices[MAXIMUM_NUM_ADAPTERS];

static unsigned aac_count = 0;
static int aac_cfg_major = -1;

/*
 * Because of the way Linux names scsi devices, the order in this table has
 * become important.  Check for on-board Raid first, add-in cards second.
 *
 * dmb - For now we add the number of channels to this structure.  
 * In the future we should add a fib that reports the number of channels
 * for the card.  At that time we can remove the channels from here
 */
 
static struct aac_driver_ident aac_drivers[] = {
	{ 0x1028, 0x0001, 0x1028, 0x0001, aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2 }, 		/* PERC 2/Si (Iguana/PERC2Si) */
	{ 0x1028, 0x0002, 0x1028, 0x0002, aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2 }, 		/* PERC 3/Di (Opal/PERC3Di) */
	{ 0x1028, 0x0003, 0x1028, 0x0003, aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2 }, 		/* PERC 3/Si (SlimFast/PERC3Si */
	{ 0x1028, 0x0004, 0x1028, 0x00d0, aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2 }, 		/* PERC 3/Di (Iguana FlipChip/PERC3DiF */
	{ 0x1028, 0x0002, 0x1028, 0x00d1, aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2 }, 		/* PERC 3/Di (Viper/PERC3DiV) */
	{ 0x1028, 0x0002, 0x1028, 0x00d9, aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2 }, 		/* PERC 3/Di (Lexus/PERC3DiL) */
	{ 0x1028, 0x000a, 0x1028, 0x0106, aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2 }, 		/* PERC 3/Di (Jaguar/PERC3DiJ) */
	{ 0x1028, 0x000a, 0x1028, 0x011b, aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2 }, 		/* PERC 3/Di (Dagger/PERC3DiD) */
	{ 0x1028, 0x000a, 0x1028, 0x0121, aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2 }, 		/* PERC 3/Di (Boxster/PERC3DiB) */
	{ 0x9005, 0x0283, 0x9005, 0x0283, aac_rx_init, "aacraid",  "ADAPTEC ", "catapult        ", 2 }, 		/* catapult */
	{ 0x9005, 0x0284, 0x9005, 0x0284, aac_rx_init, "aacraid",  "ADAPTEC ", "tomcat          ", 2 }, 		/* tomcat */
	{ 0x9005, 0x0285, 0x9005, 0x0286, aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 2120S   ", 1, AAC_QUIRK_31BIT },/* Adaptec 2120S (Crusader) */
	{ 0x9005, 0x0285, 0x9005, 0x0285, aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 2200S   ", 2, AAC_QUIRK_31BIT },/* Adaptec 2200S (Vulcan) */
	{ 0x9005, 0x0285, 0x9005, 0x0287, aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 2200S   ", 2, AAC_QUIRK_31BIT },/* Adaptec 2200S (Vulcan-2m) */
	{ 0x9005, 0x0285, 0x17aa, 0x0286, aac_rx_init, "aacraid",  "Legend  ", "Legend S220     ", 1 }, 		/* Legend S220 (Legend Crusader) */
	{ 0x9005, 0x0285, 0x17aa, 0x0287, aac_rx_init, "aacraid",  "Legend  ", "Legend S230     ", 2 }, 		/* Legend S230 (Legend Vulcan) */

	{ 0x9005, 0x0285, 0x9005, 0x0288, aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 3230S   ", 2 }, 		/* Adaptec 3230S (Harrier) */
	{ 0x9005, 0x0285, 0x9005, 0x0289, aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 3240S   ", 2 }, 		/* Adaptec 3240S (Tornado) */
	{ 0x9005, 0x0285, 0x9005, 0x028a, aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2020S PCI-X ", 2 }, 		/* ASR-2020S PCI-X ZCR (Skyhawk) */
	{ 0x9005, 0x0285, 0x9005, 0x028b, aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2020S PCI-X ", 2 }, 		/* ASR-2020S SO-DIMM PCI-X ZCR (Terminator) */
	{ 0x9005, 0x0285, 0x9005, 0x0290, aac_rx_init, "aacraid",  "ADAPTEC ", "AAR-2410SA SATA ", 2 }, 		/* AAR-2410SA PCI SATA 4ch (Jaguar II) */
	{ 0x9005, 0x0285, 0x1028, 0x0291, aac_rx_init, "aacraid",  "DELL    ", "CERC SATA RAID 2 ", 2 },		/* CERC SATA RAID 2 PCI SATA 8ch (DellCorsair) */
	{ 0x9005, 0x0285, 0x9005, 0x0292, aac_rx_init, "aacraid",  "ADAPTEC ", "AAR-2810SA SATA ", 2 }, 		/* AAR-2810SA PCI SATA 8ch (Corsair-8) */
	{ 0x9005, 0x0285, 0x9005, 0x0293, aac_rx_init, "aacraid",  "ADAPTEC ", "AAR-21610SA SATA ", 2 },		/* AAR-21610SA PCI SATA 16ch (Corsair-16) */
	{ 0x9005, 0x0285, 0x9005, 0x0294, aac_rx_init, "aacraid",  "ADAPTEC ", "SO-DIMM SATA ZCR ", 2 },		/* ESD SO-DIMM PCI-X SATA ZCR (Prowler) */
	{ 0x9005, 0x0285, 0x0E11, 0x0295, aac_rx_init, "aacraid",  "ADAPTEC ", "SATA 6Channel   ", 1 }, 		/* SATA 6Ch (Bearcat) */

	{ 0x9005, 0x0285, 0x1028, 0x0287, aac_rx_init, "percraid", "DELL    ", "PERC 320/DC     ", 2 },			/* Perc 320/DC*/
	{ 0x1011, 0x0046, 0x9005, 0x0365, aac_sa_init, "aacraid",  "ADAPTEC ", "Adaptec 5400S   ", 4 }, 		/* Adaptec 5400S (Mustang)*/
	{ 0x1011, 0x0046, 0x9005, 0x0364, aac_sa_init, "aacraid",  "ADAPTEC ", "AAC-364         ", 4 },			/* Adaptec 5400S (Mustang)*/
	{ 0x1011, 0x0046, 0x9005, 0x1364, aac_sa_init, "percraid", "DELL    ", "PERCRAID        ", 4 },			/* Dell PERC2 "Quad Channel" */
	{ 0x1011, 0x0046, 0x103c, 0x10c2, aac_sa_init, "hpnraid",  "HP      ", "NetRAID         ", 4 },			/* HP NetRAID-4M */
	{ 0x9005, 0x0285, 0x1028, PCI_ANY_ID,
					  aac_rx_init, "aacraid",  "DELL    ", "RAID            ", 2, AAC_QUIRK_31BIT },/* Dell Catchall */
	{ 0x9005, 0x0285, 0x17aa, PCI_ANY_ID,
					  aac_rx_init, "aacraid",  "Legend  ", "RAID            ", 2, AAC_QUIRK_31BIT },/* Legend Catchall */
	{ 0x9005, 0x0285, PCI_ANY_ID, PCI_ANY_ID,
					  aac_rx_init, "aacraid",  "ADAPTEC ", "RAID            ", 2, AAC_QUIRK_31BIT } /* Adaptec Catch All */
};

#define NUM_AACTYPES	(sizeof(aac_drivers) / sizeof(struct aac_driver_ident))
static int num_aacdrivers = NUM_AACTYPES;

static int aac_cfg_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg);
static int aac_cfg_open(struct inode * inode, struct file * file);
static int aac_cfg_release(struct inode * inode,struct file * file);

static struct file_operations aac_cfg_fops = {
	owner: THIS_MODULE,
	ioctl: aac_cfg_ioctl,
	open: aac_cfg_open,
	release: aac_cfg_release
};

static int aac_detect(Scsi_Host_Template *);
static int aac_release(struct Scsi_Host *);
static int aac_queuecommand(Scsi_Cmnd *, void (*CompletionRoutine)(Scsi_Cmnd *));
static int aac_biosparm(Scsi_Disk *, kdev_t, int *);
static int aac_procinfo(char *, char **, off_t, int, int, int);
static int aac_ioctl(Scsi_Device *, int, void *);
static int aac_eh_abort(Scsi_Cmnd * cmd);
static int aac_eh_device_reset(Scsi_Cmnd* cmd);
static int aac_eh_bus_reset(Scsi_Cmnd* cmd);
static int aac_eh_reset(Scsi_Cmnd* cmd);

static void aac_queuedepth(struct Scsi_Host *, Scsi_Device *);

/**
 *	aac_detect	-	Probe for aacraid cards
 *	@template: SCSI driver template
 *
 *	Probe for AAC Host Adapters initialize, register, and report the 
 *	configuration of each AAC Host Adapter found.
 *	Returns the number of adapters successfully initialized and 
 *	registered.
 *	Initializes all data necessary for this particular SCSI driver.
 *	Notes:
 *	The detect routine must not call any of the mid level functions 
 *	to queue commands because things are not guaranteed to be set 
 *	up yet. The detect routine can send commands to the host adapter 
 *	as long as the program control will not be passed to scsi.c in 
 *	the processing of the command. Note especially that 
 *	scsi_malloc/scsi_free must not be called.
 *
 */
 
static int aac_detect(Scsi_Host_Template *template)
{
	int index;
	int container;
	u16 vendor_id, device_id;
	struct Scsi_Host *host_ptr;
	struct pci_dev *dev = NULL;
	struct aac_dev *aac;
	struct fsa_scsi_hba *fsa_dev_ptr;
	char *name = NULL;
	
	printk(KERN_INFO "Red Hat/Adaptec %s driver (%d.%d-%d %s)\n",
		AAC_DRIVER_NAME,
		AAC_DRIVER_VERSION >> 24,
		(AAC_DRIVER_VERSION >> 16) & 0xFF,
		(AAC_DRIVER_VERSION >> 8) & 0xFF,
		AAC_DRIVER_BUILD_DATE);
		
	/* setting up the proc directory structure */
	
	template->proc_name = "aacraid";
	spin_unlock_irq(&io_request_lock);

	for( index = 0; index != num_aacdrivers; index++ )
	{
		device_id = aac_drivers[index].device;
		vendor_id = aac_drivers[index].vendor;
		name = aac_drivers[index].name;
		dprintk((KERN_DEBUG "Checking %s %x/%x/%x/%x.\n", 
			name, vendor_id, device_id,
			aac_drivers[index].subsystem_vendor,
			aac_drivers[index].subsystem_device));

		dev = NULL;
		while((dev = pci_find_device(vendor_id, device_id, dev))) {
			if (pci_enable_device(dev))
				continue;
			pci_set_master(dev);
			
			if(aac_drivers[index].quirks & AAC_QUIRK_31BIT)
				pci_set_dma_mask(dev, 0x7FFFFFFFULL);
			else
				pci_set_dma_mask(dev, 0xFFFFFFFFULL);

			if((dev->subsystem_vendor != aac_drivers[index].subsystem_vendor) || 
			   (dev->subsystem_device != aac_drivers[index].subsystem_device))
					continue;

			dprintk((KERN_DEBUG "%s device detected.\n", name));
			dprintk((KERN_DEBUG "%x/%x/%x/%x.\n", vendor_id, device_id, 
				aac_drivers[index].subsystem_vendor, aac_drivers[index].subsystem_device));
			/*
			 * scsi_register() allocates memory for a Scsi_Hosts structure and
			 * links it into the linked list of host adapters. This linked list
			 * contains the data for all possible <supported> scsi hosts.
			 * This is similar to the Scsi_Host_Template, except that we have
			 * one entry for each actual physical host adapter on the system,
			 * stored as a linked list. If there are two AAC boards, then we
			 * will need to make two Scsi_Host entries, but there will be only
			 * one Scsi_Host_Template entry. The second argument to scsi_register()
			 * specifies the size of the extra memory we want to hold any device 
			 * specific information.
			 */
			host_ptr = scsi_register( template, sizeof(struct aac_dev) );
			if(host_ptr == NULL)
				continue;
			/* Increment the host adapter count */
			aac_count++;
			/* 
			 * These three parameters can be used to allow for wide SCSI 
			 * and for host adapters that support multiple buses.
			 */
			host_ptr->irq = dev->irq;		/* Adapter IRQ number */
			/* host_ptr->base = ( char * )(dev->resource[0].start & ~0xff); */
			host_ptr->base = dev->resource[0].start;
			scsi_set_pci_device(host_ptr, dev);
			dprintk((KERN_DEBUG "Device base address = 0x%lx [0x%lx].\n", host_ptr->base, dev->resource[0].start));
			dprintk((KERN_DEBUG "Device irq = 0x%x.\n", dev->irq));
			/*
			 * The unique_id field is a unique identifier that must
			 * be assigned so that we have some way of identifying
			 * each host adapter properly and uniquely. For hosts 
			 * that do not support more than one card in the
			 * system, this does not need to be set. It is
			 * initialized to zero in scsi_register(). This is the 
			 * value returned as aac->id.
			 */
			host_ptr->unique_id = aac_count - 1;
			/*
			 *	This function is called after the device list has
			 *	been built to find the tagged queueing depth 
			 *	supported for each device.
			 */
			host_ptr->select_queue_depths = aac_queuedepth;
			aac = (struct aac_dev *)host_ptr->hostdata;
			/* attach a pointer back to Scsi_Host */
			aac->scsi_host_ptr = host_ptr;	
			aac->pdev = dev;
			aac->name = aac->scsi_host_ptr->hostt->name;
			aac->id = aac->scsi_host_ptr->unique_id;
			aac->cardtype =  index;

			aac->fibs = (struct fib*) kmalloc(sizeof(struct fib)*AAC_NUM_FIB, GFP_KERNEL);
			spin_lock_init(&aac->fib_lock);

			/* Initialize the ordinal number of the device to -1 */
			fsa_dev_ptr = &(aac->fsa_dev);
			for( container = 0; container < MAXIMUM_NUM_CONTAINERS; container++ )
				fsa_dev_ptr->devno[container] = -1;

			dprintk((KERN_DEBUG "Initializing Hardware...\n"));
			if((*aac_drivers[index].init)(aac , host_ptr->unique_id) != 0)
			{
				/* device initialization failed */
				printk(KERN_WARNING "aacraid: device initialization failed.\n");
				scsi_unregister(host_ptr);
				aac_count--;
				continue;
			} 
			dprintk((KERN_DEBUG "%s:%d device initialization successful.\n", name, host_ptr->unique_id));
			aac_get_adapter_info(aac);
			if(aac->nondasd_support == 1)
			{
				/*
				 * max channel will be the physical channels plus 1 virtual channel 
				 * all containers are on the virtual channel 0
				 * physical channels are address by their actual physical number+1
				 */
				host_ptr->max_channel = aac_drivers[index].channels+1;
			} else {
				host_ptr->max_channel = 1;
 			}
			dprintk((KERN_DEBUG "Device has %d logical channels\n", host_ptr->max_channel));
			aac_get_containers(aac);
			aac_devices[aac_count-1] = aac;

			/*
			 * dmb - we may need to move the setting of these parms somewhere else once
			 * we get a fib that can report the actual numbers
			 */
			host_ptr->max_id = AAC_MAX_TARGET;
			host_ptr->max_lun = AAC_MAX_LUN;
		}
	}

	if( aac_count ){
		if((aac_cfg_major = register_chrdev( 0, "aac", &aac_cfg_fops))<0)
			printk(KERN_WARNING "aacraid: unable to register \"aac\" device.\n");
	}
	spin_lock_irq(&io_request_lock);

	template->present = aac_count; /* # of cards of this type found */
	return aac_count;
}

/**
 *	aac_release	-	release SCSI host resources
 *	@host_ptr: SCSI host to clean up
 *
 *	Release all resources previously acquired to support a specific Host 
 *	Adapter and unregister the AAC Host Adapter.
 *
 *	BUGS: Does not wait for the thread it kills to die.
 */

static int aac_release(struct Scsi_Host *host_ptr)
{
	struct aac_dev *dev;
	dprintk((KERN_DEBUG "aac_release.\n"));
	dev = (struct aac_dev *)host_ptr->hostdata;
	/*
	 *	kill any threads we started
	 */
	kill_proc(dev->thread_pid, SIGKILL, 0);
	wait_for_completion(&dev->aif_completion);
	/*
	 *	Call the comm layer to detach from this adapter
	 */
	aac_detach(dev);
	/* Check free orderings... */
	/* remove interrupt binding */
	free_irq(host_ptr->irq, dev);
	iounmap((void * )dev->regs.sa);
	/* unregister adapter */
	scsi_unregister(host_ptr);
	/*
	 *	FIXME: This assumes no hot plugging is going on...
	 */
	if( aac_cfg_major >= 0 )
	{
		unregister_chrdev(aac_cfg_major, "aac");
		aac_cfg_major = -1;
	}
	return 0;
}

/**
 *	aac_queuecommand	-	queue a SCSI command
 *	@scsi_cmnd_ptr:	SCSI command to queue
 *	@CompletionRoutine: Function to call on command completion
 *
 *	Queues a command for execution by the associated Host Adapter.
 */ 

static int aac_queuecommand(Scsi_Cmnd *scsi_cmnd_ptr, void (*complete)(Scsi_Cmnd *))
{
	int ret;

	scsi_cmnd_ptr->scsi_done = complete;
	/*
	 *	aac_scsi_cmd() handles command processing, setting the 
	 *	result code and calling completion routine. 
	 */
	if((ret = aac_scsi_cmd(scsi_cmnd_ptr)) != 0)
		dprintk((KERN_DEBUG "aac_scsi_cmd failed.\n"));
	return ret;
} 

/**
 *	aac_driverinfo		-	Returns the host adapter name
 *	@host_ptr:	Scsi host to report on
 *
 *	Returns a static string describing the device in question
 */

const char *aac_driverinfo(struct Scsi_Host *host_ptr)
{
	struct aac_dev *dev = (struct aac_dev *)host_ptr->hostdata;
	return aac_drivers[dev->cardtype].name;
}

/**
 *	aac_get_driver_ident
 * 	@devtype: index into lookup table
 *
 * 	Returns a pointer to the entry in the driver lookup table.
 */
struct aac_driver_ident* aac_get_driver_ident(int devtype)
{
	return  &aac_drivers[devtype];
}

/**
 *	aac_biosparm	-	return BIOS parameters for disk
 *	@disk: SCSI disk object to process
 *	@device: kdev_t of the disk in question
 *	@geom: geometry block to fill in
 *
 *	Return the Heads/Sectors/Cylinders BIOS Disk Parameters for Disk.  
 *	The default disk geometry is 64 heads, 32 sectors, and the appropriate 
 *	number of cylinders so as not to exceed drive capacity.  In order for 
 *	disks equal to or larger than 1 GB to be addressable by the BIOS
 *	without exceeding the BIOS limitation of 1024 cylinders, Extended 
 *	Translation should be enabled.   With Extended Translation enabled, 
 *	drives between 1 GB inclusive and 2 GB exclusive are given a disk 
 *	geometry of 128 heads and 32 sectors, and drives above 2 GB inclusive 
 *	are given a disk geometry of 255 heads and 63 sectors.  However, if 
 *	the BIOS detects that the Extended Translation setting does not match 
 *	the geometry in the partition table, then the translation inferred 
 *	from the partition table will be used by the BIOS, and a warning may 
 *	be displayed.
 */
 
static int aac_biosparm(Scsi_Disk *disk, kdev_t dev, int *geom)
{
	struct diskparm *param = (struct diskparm *)geom;
	struct buffer_head * buf;

	dprintk((KERN_DEBUG "aac_biosparm.\n"));

	/*
	 *	Assuming extended translation is enabled - #REVISIT#
	 */
	if( disk->capacity >= 2 * 1024 * 1024 ) /* 1 GB in 512 byte sectors */
	{
		if( disk->capacity >= 4 * 1024 * 1024 ) /* 2 GB in 512 byte sectors */
		{
			param->heads = 255;
			param->sectors = 63;
		}
		else
		{
			param->heads = 128;
			param->sectors = 32;
		}
	}
	else
	{
		param->heads = 64;
		param->sectors = 32;
	}

	param->cylinders = disk->capacity/(param->heads * param->sectors);

	/*
	 *	Read the first 1024 bytes from the disk device
	 */

	buf = bread(MKDEV(MAJOR(dev), MINOR(dev)&~0xf), 0, block_size(dev));
	if(buf == NULL)
		return 0;
	/* 
	 *	If the boot sector partition table is valid, search for a partition 
	 *	table entry whose end_head matches one of the standard geometry 
	 *	translations ( 64/32, 128/32, 255/63 ).
	 */
	 
	if(*(unsigned short *)(buf->b_data + 0x1fe) == cpu_to_le16(0xaa55))
	{
		struct partition *first = (struct partition * )(buf->b_data + 0x1be);
		struct partition *entry = first;
		int saved_cylinders = param->cylinders;
		int num;
		unsigned char end_head, end_sec;

		for(num = 0; num < 4; num++)
		{
			end_head = entry->end_head;
			end_sec = entry->end_sector & 0x3f;

			if(end_head == 63)
			{
				param->heads = 64;
				param->sectors = 32;
				break;
			}
			else if(end_head == 127)
			{
				param->heads = 128;
				param->sectors = 32;
				break;
			}
			else if(end_head == 254) 
			{
				param->heads = 255;
				param->sectors = 63;
				break;
			}
			entry++;
		}

		if(num == 4)
		{
			end_head = first->end_head;
			end_sec = first->end_sector & 0x3f;
		}

		param->cylinders = disk->capacity / (param->heads * param->sectors);

		if(num < 4 && end_sec == param->sectors)
		{
			if(param->cylinders != saved_cylinders)
				dprintk((KERN_DEBUG "Adopting geometry: heads=%d, sectors=%d from partition table %d.\n",
					param->heads, param->sectors, num));
		}
		else if(end_head > 0 || end_sec > 0)
		{
			dprintk((KERN_DEBUG "Strange geometry: heads=%d, sectors=%d in partition table %d.\n",
				end_head + 1, end_sec, num));
			dprintk((KERN_DEBUG "Using geometry: heads=%d, sectors=%d.\n",
					param->heads, param->sectors));
		}
	}
	brelse(buf);
	return 0;
}

/**
 *	aac_queuedepth		-	compute queue depths
 *	@host:	SCSI host in question
 *	@dev:	SCSI device we are considering
 *
 *	Selects queue depths for each target device based on the host adapter's
 *	total capacity and the queue depth supported by the target device.
 *	A queue depth of one automatically disables tagged queueing.
 */

static void aac_queuedepth(struct Scsi_Host * host, Scsi_Device * dev )
{
	Scsi_Device * dptr;

	dprintk((KERN_DEBUG "aac_queuedepth.\n"));
	dprintk((KERN_DEBUG "Device #   Q Depth   Online\n"));
	dprintk((KERN_DEBUG "---------------------------\n"));
	for(dptr = dev; dptr != NULL; dptr = dptr->next)
	{
		if(dptr->host == host)
		{
			dptr->queue_depth = 10;		
			dprintk((KERN_DEBUG "  %2d         %d        %d\n", 
				dptr->id, dptr->queue_depth, dptr->online));
		}
	}
}


/**
 *	aac_eh_abort	-	Abort command if possible.
 *	@cmd:	SCSI command block to abort
 *
 *	Called when the midlayer wishes to abort a command. We don't support
 *	this facility, and our firmware looks after life for us. We just
 *	report this as failing
 */
 
static int aac_eh_abort(Scsi_Cmnd *cmd)
{
	return FAILED;
}

/**
 *	aac_eh_device_reset	-	Reset command handling
 *	@cmd:	SCSI command block causing the reset
 *
 *	Issue a reset of a SCSI device. We are ourselves not truely a SCSI
 *	controller and our firmware will do the work for us anyway. Thus this
 *	is a no-op. We just return FAILED.
 */

static int aac_eh_device_reset(Scsi_Cmnd *cmd)
{
	return FAILED;
}

/**
 *	aac_eh_bus_reset	-	Reset command handling
 *	@scsi_cmd:	SCSI command block causing the reset
 *
 *	Issue a reset of a SCSI bus. We are ourselves not truely a SCSI
 *	controller and our firmware will do the work for us anyway. Thus this
 *	is a no-op. We just return FAILED.
 */

static int aac_eh_bus_reset(Scsi_Cmnd* cmd)
{
	return FAILED;
}

/**
 *	aac_eh_hba_reset	-	Reset command handling
 *	@scsi_cmd:	SCSI command block causing the reset
 *
 *	Issue a reset of a SCSI host. If things get this bad then arguably we should
 *	go take a look at what the host adapter is doing and see if something really
 *	broke (as can occur at least on my Dell QC card if a drive keeps failing spinup)
 */

static int aac_eh_reset(Scsi_Cmnd* cmd)
{
	printk(KERN_ERR "%s: Host adapter reset request. SCSI hang ?\n", AAC_DRIVER_NAME);
	return FAILED;
}

/**
 *	aac_ioctl 	-	Handle SCSI ioctls
 *	@scsi_dev_ptr: scsi device to operate upon
 *	@cmd: ioctl command to use issue
 *	@arg: ioctl data pointer
 *
 *	Issue an ioctl on an aacraid device. Returns a standard unix error code or
 *	zero for success
 */
 
static int aac_ioctl(Scsi_Device * scsi_dev_ptr, int cmd, void * arg)
{
	struct aac_dev *dev;
	dprintk((KERN_DEBUG "aac_ioctl.\n"));
	dev = (struct aac_dev *)scsi_dev_ptr->host->hostdata;
	return aac_do_ioctl(dev, cmd, arg);
}

/**
 *	aac_cfg_open		-	open a configuration file
 *	@inode: inode being opened
 *	@file: file handle attached
 *
 *	Called when the configuration device is opened. Does the needed
 *	set up on the handle and then returns
 *
 *	Bugs: This needs extending to check a given adapter is present
 *	so we can support hot plugging, and to ref count adapters.
 */

static int aac_cfg_open(struct inode * inode, struct file * file )
{
	unsigned minor_number = MINOR(inode->i_rdev);
	if(minor_number >= aac_count)
		return -ENODEV;
	return 0;
}

/**
 *	aac_cfg_release		-	close down an AAC config device
 *	@inode: inode of configuration file
 *	@file: file handle of configuration file
 *	
 *	Called when the last close of the configuration file handle
 *	is performed.
 */
 
static int aac_cfg_release(struct inode * inode, struct file * file )
{
	return 0;
}

/**
 *	aac_cfg_ioctl		-	AAC configuration request
 *	@inode: inode of device
 *	@file: file handle
 *	@cmd: ioctl command code
 *	@arg: argument
 *
 *	Handles a configuration ioctl. Currently this involves wrapping it
 *	up and feeding it into the nasty windowsalike glue layer.
 *
 *	Bugs: Needs locking against parallel ioctls lower down
 *	Bugs: Needs to handle hot plugging
 */
 
static int aac_cfg_ioctl(struct inode * inode,  struct file * file, unsigned int cmd, unsigned long arg )
{
	struct aac_dev *dev = aac_devices[MINOR(inode->i_rdev)];
	return aac_do_ioctl(dev, cmd, (void *)arg);
}

/*
 *	To use the low level SCSI driver support using the linux kernel loadable 
 *	module interface we should initialize the global variable driver_interface  
 *	(datatype Scsi_Host_Template) and then include the file scsi_module.c.
 */
 
static Scsi_Host_Template driver_template = {
	module:			THIS_MODULE,
	name:           	"AAC",
	proc_info:      	aac_procinfo,
	detect:         	aac_detect,
	release:        	aac_release,
	info:           	aac_driverinfo,
	ioctl:          	aac_ioctl,
	queuecommand:   	aac_queuecommand,
	bios_param:     	aac_biosparm,	
	can_queue:      	AAC_NUM_IO_FIB,	
	this_id:        	16,
	sg_tablesize:   	16,
	max_sectors:    	128,
	cmd_per_lun:    	AAC_NUM_IO_FIB,
	eh_abort_handler:       aac_eh_abort,
	eh_device_reset_handler:aac_eh_device_reset,
	eh_bus_reset_handler:	aac_eh_bus_reset,
	eh_host_reset_handler:	aac_eh_reset,
	use_new_eh_code:	1, 

	use_clustering:		ENABLE_CLUSTERING,
};

#include "scsi_module.c"

/**
 *	aac_procinfo	-	Implement /proc/scsi/<drivername>/<n>
 *	@proc_buffer: memory buffer for I/O
 *	@start_ptr: pointer to first valid data
 *	@offset: offset into file
 *	@bytes_available: space left
 *	@host_no: scsi host ident
 *	@write: direction of I/O
 *
 *	Used to export driver statistics and other infos to the world outside 
 *	the kernel using the proc file system. Also provides an interface to
 *	feed the driver with information.
 *
 *		For reads
 *			- if offset > 0 return 0
 *			- if offset == 0 write data to proc_buffer and set the start_ptr to
 *			beginning of proc_buffer, return the number of characters written.
 *		For writes
 *			- writes currently not supported, return 0
 *
 *	Bugs:	Only offset zero is handled
 */

static int aac_procinfo(char *proc_buffer, char **start_ptr,off_t offset,
			int bytes_available, int host_no, int write)
{
	struct aac_dev * dev;
	int index, ret, tmp;

	if(write || offset > 0)
		return 0;
	*start_ptr = proc_buffer;
	ret = sprintf(proc_buffer,
		  "Adaptec Raid Controller %d.%d-%d %s, scsi hba number %d\n",
		  AAC_DRIVER_VERSION >> 24,
		  (AAC_DRIVER_VERSION >> 16) & 0xFF,
		  (AAC_DRIVER_VERSION >> 8) & 0xFF,
		  AAC_DRIVER_BUILD_DATE,
		  host_no);
	for (index = 0; index < aac_count; ++index) {
		if (((dev = aac_devices[index]) != NULL) && dev->scsi_host_ptr->host_no == host_no)
			break;
	}
	if (index >= aac_count || dev == NULL)
		return ret;
	tmp = dev->adapter_info.kernelrev;
	ret += sprintf(proc_buffer + ret, "kernel: %d.%d-%d[%d]\n", 
		tmp >> 24, (tmp >> 16) & 0xff, (tmp >> 8) & 0xff,
		dev->adapter_info.kernelbuild);
	tmp = dev->adapter_info.monitorrev;
	ret += sprintf(proc_buffer + ret, "monitor: %d.%d-%d[%d]\n", 
		tmp >> 24, (tmp >> 16) & 0xff, (tmp >> 8) & 0xff,
		dev->adapter_info.monitorbuild);
	tmp = dev->adapter_info.biosrev;
	ret += sprintf(proc_buffer + ret, "bios: %d.%d-%d[%d]\n", 
		tmp >> 24, (tmp >> 16) & 0xff, (tmp >> 8) & 0xff,
		dev->adapter_info.biosbuild);
	ret += sprintf(proc_buffer + ret, "serial: %x%x\n",
		dev->adapter_info.serial[0],
		dev->adapter_info.serial[1]);
	return ret;
}

EXPORT_NO_SYMBOLS;
