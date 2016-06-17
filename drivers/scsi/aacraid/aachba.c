/*
 *	Adaptec AAC series RAID controller driver
 *	(c) Copyright 2001 Red Hat Inc.	<alan@redhat.com>
 *
 * based on the old aacraid driver that is..
 * Adaptec aacraid device driver for Linux.
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
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <asm/semaphore.h>
#include <asm/uaccess.h>
#define MAJOR_NR SCSI_DISK0_MAJOR	/* For DEVICE_NR() */
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "sd.h"

#include "aacraid.h"

/*	SCSI Commands */
/*	TODO:  dmb - use the ones defined in include/scsi/scsi.h */

#define	SS_TEST			0x00	/* Test unit ready */
#define SS_REZERO		0x01	/* Rezero unit */
#define	SS_REQSEN		0x03	/* Request Sense */
#define SS_REASGN		0x07	/* Reassign blocks */
#define	SS_READ			0x08	/* Read 6   */
#define	SS_WRITE		0x0A	/* Write 6  */
#define	SS_INQUIR		0x12	/* inquiry */
#define	SS_ST_SP		0x1B	/* Start/Stop unit */
#define	SS_LOCK			0x1E	/* prevent/allow medium removal */
#define SS_RESERV		0x16	/* Reserve */
#define SS_RELES		0x17	/* Release */
#define SS_MODESEN		0x1A	/* Mode Sense 6 */
#define	SS_RDCAP		0x25	/* Read Capacity */
#define	SM_READ			0x28	/* Read 10  */
#define	SM_WRITE		0x2A	/* Write 10 */
#define SS_SEEK			0x2B	/* Seek */

/* values for inqd_pdt: Peripheral device type in plain English */
#define	INQD_PDT_DA	0x00		/* Direct-access (DISK) device */
#define	INQD_PDT_PROC	0x03		/* Processor device */
#define	INQD_PDT_CHNGR	0x08		/* Changer (jukebox, scsi2) */
#define	INQD_PDT_COMM	0x09		/* Communication device (scsi2) */
#define	INQD_PDT_NOLUN2 0x1f		/* Unknown Device (scsi2) */
#define	INQD_PDT_NOLUN	0x7f		/* Logical Unit Not Present */

#define	INQD_PDT_DMASK	0x1F		/* Peripheral Device Type Mask */
#define	INQD_PDT_QMASK	0xE0		/* Peripheral Device Qualifer Mask */

#define	TARGET_LUN_TO_CONTAINER(target, lun)	(target)
#define CONTAINER_TO_TARGET(cont)		((cont))
#define CONTAINER_TO_LUN(cont)			(0)

#define MAX_FIB_DATA (sizeof(struct hw_fib) - sizeof(FIB_HEADER))

#define MAX_DRIVER_SG_SEGMENT_COUNT 17

/*
 *	Sense keys
 */
#define SENKEY_NO_SENSE      			0x00	
#define SENKEY_UNDEFINED     			0x01	
#define SENKEY_NOT_READY     			0x02	
#define SENKEY_MEDIUM_ERR    			0x03	
#define SENKEY_HW_ERR        			0x04	
#define SENKEY_ILLEGAL       			0x05	
#define SENKEY_ATTENTION     			0x06	
#define SENKEY_PROTECTED     			0x07	
#define SENKEY_BLANK         			0x08	
#define SENKEY_V_UNIQUE      			0x09	
#define SENKEY_CPY_ABORT     			0x0A	
#define SENKEY_ABORT         			0x0B	
#define SENKEY_EQUAL         			0x0C	
#define SENKEY_VOL_OVERFLOW  			0x0D	
#define SENKEY_MISCOMP       			0x0E	
#define SENKEY_RESERVED      			0x0F	

/*
 *	Sense codes
 */
 
#define SENCODE_NO_SENSE                        0x00
#define SENCODE_END_OF_DATA                     0x00
#define SENCODE_BECOMING_READY                  0x04
#define SENCODE_INIT_CMD_REQUIRED               0x04
#define SENCODE_PARAM_LIST_LENGTH_ERROR         0x1A
#define SENCODE_INVALID_COMMAND                 0x20
#define SENCODE_LBA_OUT_OF_RANGE                0x21
#define SENCODE_INVALID_CDB_FIELD               0x24
#define SENCODE_LUN_NOT_SUPPORTED               0x25
#define SENCODE_INVALID_PARAM_FIELD             0x26
#define SENCODE_PARAM_NOT_SUPPORTED             0x26
#define SENCODE_PARAM_VALUE_INVALID             0x26
#define SENCODE_RESET_OCCURRED                  0x29
#define SENCODE_LUN_NOT_SELF_CONFIGURED_YET     0x3E
#define SENCODE_INQUIRY_DATA_CHANGED            0x3F
#define SENCODE_SAVING_PARAMS_NOT_SUPPORTED     0x39
#define SENCODE_DIAGNOSTIC_FAILURE              0x40
#define SENCODE_INTERNAL_TARGET_FAILURE         0x44
#define SENCODE_INVALID_MESSAGE_ERROR           0x49
#define SENCODE_LUN_FAILED_SELF_CONFIG          0x4c
#define SENCODE_OVERLAPPED_COMMAND              0x4E

/*
 *	Additional sense codes
 */
 
#define ASENCODE_NO_SENSE                       0x00
#define ASENCODE_END_OF_DATA                    0x05
#define ASENCODE_BECOMING_READY                 0x01
#define ASENCODE_INIT_CMD_REQUIRED              0x02
#define ASENCODE_PARAM_LIST_LENGTH_ERROR        0x00
#define ASENCODE_INVALID_COMMAND                0x00
#define ASENCODE_LBA_OUT_OF_RANGE               0x00
#define ASENCODE_INVALID_CDB_FIELD              0x00
#define ASENCODE_LUN_NOT_SUPPORTED              0x00
#define ASENCODE_INVALID_PARAM_FIELD            0x00
#define ASENCODE_PARAM_NOT_SUPPORTED            0x01
#define ASENCODE_PARAM_VALUE_INVALID            0x02
#define ASENCODE_RESET_OCCURRED                 0x00
#define ASENCODE_LUN_NOT_SELF_CONFIGURED_YET    0x00
#define ASENCODE_INQUIRY_DATA_CHANGED           0x03
#define ASENCODE_SAVING_PARAMS_NOT_SUPPORTED    0x00
#define ASENCODE_DIAGNOSTIC_FAILURE             0x80
#define ASENCODE_INTERNAL_TARGET_FAILURE        0x00
#define ASENCODE_INVALID_MESSAGE_ERROR          0x00
#define ASENCODE_LUN_FAILED_SELF_CONFIG         0x00
#define ASENCODE_OVERLAPPED_COMMAND             0x00

#define BYTE0(x) (unsigned char)(x)
#define BYTE1(x) (unsigned char)((x) >> 8)
#define BYTE2(x) (unsigned char)((x) >> 16)
#define BYTE3(x) (unsigned char)((x) >> 24)

/*------------------------------------------------------------------------------
 *              S T R U C T S / T Y P E D E F S
 *----------------------------------------------------------------------------*/
/* SCSI inquiry data */
struct inquiry_data {
	u8 inqd_pdt;		/* Peripheral qualifier | Peripheral Device Type  */
	u8 inqd_dtq;		/* RMB | Device Type Qualifier  */
	u8 inqd_ver;		/* ISO version | ECMA version | ANSI-approved version */
	u8 inqd_rdf;		/* AENC | TrmIOP | Response data format */
	u8 inqd_len;		/* Additional length (n-4) */
	u8 inqd_pad1[2];	/* Reserved - must be zero */
	u8 inqd_pad2;		/* RelAdr | WBus32 | WBus16 |  Sync  | Linked |Reserved| CmdQue | SftRe */
	u8 inqd_vid[8];		/* Vendor ID */
	u8 inqd_pid[16];	/* Product ID */
	u8 inqd_prl[4];		/* Product Revision Level */
};

struct sense_data {
	u8 error_code;		/* 70h (current errors), 71h(deferred errors) */
	u8 valid:1;		/* A valid bit of one indicates that the information  */
	/* field contains valid information as defined in the
	 * SCSI-2 Standard.
	 */
	u8 segment_number;	/* Only used for COPY, COMPARE, or COPY AND VERIFY Commands */
	u8 sense_key:4;		/* Sense Key */
	u8 reserved:1;
	u8 ILI:1;		/* Incorrect Length Indicator */
	u8 EOM:1;		/* End Of Medium - reserved for random access devices */
	u8 filemark:1;		/* Filemark - reserved for random access devices */

	u8 information[4];	/* for direct-access devices, contains the unsigned 
				 * logical block address or residue associated with 
				 * the sense key 
				 */
	u8 add_sense_len;	/* number of additional sense bytes to follow this field */
	u8 cmnd_info[4];	/* not used */
	u8 ASC;			/* Additional Sense Code */
	u8 ASCQ;		/* Additional Sense Code Qualifier */
	u8 FRUC;		/* Field Replaceable Unit Code - not used */
	u8 bit_ptr:3;		/* indicates which byte of the CDB or parameter data
				 * was in error
				 */
	u8 BPV:1;		/* bit pointer valid (BPV): 1- indicates that 
				 * the bit_ptr field has valid value
				 */
	u8 reserved2:2;
	u8 CD:1;		/* command data bit: 1- illegal parameter in CDB.
				 * 0- illegal parameter in data.
				 */
	u8 SKSV:1;
	u8 field_ptr[2];	/* byte of the CDB or parameter data in error */
};

/*
 *              M O D U L E   G L O B A L S
 */
 
static struct fsa_scsi_hba *fsa_dev[MAXIMUM_NUM_ADAPTERS];	/*  SCSI Device Instance Pointers */
static struct sense_data sense_data[MAXIMUM_NUM_CONTAINERS];
static void get_sd_devname(int disknum, char *buffer);
static unsigned long aac_build_sg(Scsi_Cmnd* scsicmd, struct sgmap* sgmap);
static unsigned long aac_build_sg64(Scsi_Cmnd* scsicmd, struct sgmap64* psg);
static int aac_send_srb_fib(Scsi_Cmnd* scsicmd);
static char *aac_get_status_string(u32 status);

/*
 *	Non dasd selection is handled entirely in aachba now
 */	
 
MODULE_PARM(nondasd, "i");
MODULE_PARM_DESC(nondasd, "Control scanning of hba for nondasd devices. 0=off, 1=on");
MODULE_PARM(paemode, "i");
MODULE_PARM_DESC(paemode, "Control whether dma addressing is using PAE. 0=off, 1=on");

static int nondasd = -1;
static int paemode = -1;

/**
 *	aac_get_containers	-	list containers
 *	@common: adapter to probe
 *
 *	Make a list of all containers on this controller
 */
int aac_get_containers(struct aac_dev *dev)
{
	struct fsa_scsi_hba *fsa_dev_ptr;
	u32 index;
	int status = 0;
	struct aac_query_mount *dinfo;
	struct aac_mount *dresp;
	struct fib * fibptr;
	unsigned instance;

	fsa_dev_ptr = &(dev->fsa_dev);
	instance = dev->scsi_host_ptr->unique_id;

	if (!(fibptr = fib_alloc(dev)))
		return -ENOMEM;

	for (index = 0; index < MAXIMUM_NUM_CONTAINERS; index++) {
		fib_init(fibptr);
		dinfo = (struct aac_query_mount *) fib_data(fibptr);

		dinfo->command = cpu_to_le32(VM_NameServe);
		dinfo->count = cpu_to_le32(index);
		dinfo->type = cpu_to_le32(FT_FILESYS);

		status = fib_send(ContainerCommand,
				    fibptr,
				    sizeof (struct aac_query_mount),
				    FsaNormal,
				    1, 1,
				    NULL, NULL);
		if (status < 0 ) {
			printk(KERN_WARNING "aac_get_containers: SendFIB failed.\n");
			break;
		}
		dresp = (struct aac_mount *)fib_data(fibptr);

		if ((le32_to_cpu(dresp->status) == ST_OK) &&
		    (le32_to_cpu(dresp->mnt[0].vol) != CT_NONE) &&
		    (le32_to_cpu(dresp->mnt[0].state) != FSCS_HIDDEN)) {
			fsa_dev_ptr->valid[index] = 1;
			fsa_dev_ptr->type[index] = le32_to_cpu(dresp->mnt[0].vol);
			fsa_dev_ptr->size[index] = le32_to_cpu(dresp->mnt[0].capacity);
			if (le32_to_cpu(dresp->mnt[0].state) & FSCS_READONLY)
				    fsa_dev_ptr->ro[index] = 1;
		}
		fib_complete(fibptr);
		/*
		 *	If there are no more containers, then stop asking.
		 */
		if ((index + 1) >= le32_to_cpu(dresp->count))
			break;
	}
	fib_free(fibptr);
	fsa_dev[instance] = fsa_dev_ptr;
	return status;
}

/**
 *	aac_get_container_name	-	get container name
 */
static int aac_get_container_name(struct aac_dev *dev, int cid, char * pid)
{
	struct fsa_scsi_hba *fsa_dev_ptr;
	int status = 0;
	struct aac_get_name *dinfo;
	struct aac_get_name_resp *dresp;
	struct fib * fibptr;
	unsigned instance;

	fsa_dev_ptr = &(dev->fsa_dev);
	instance = dev->scsi_host_ptr->unique_id;

	if (!(fibptr = fib_alloc(dev)))
		return -ENOMEM;

	fib_init(fibptr);
	dinfo = (struct aac_get_name *) fib_data(fibptr);

	dinfo->command = cpu_to_le32(VM_ContainerConfig);
	dinfo->type = cpu_to_le32(CT_READ_NAME);
	dinfo->cid = cpu_to_le32(cid);
	dinfo->count = cpu_to_le32(sizeof(((struct aac_get_name_resp *)NULL)->data));

	status = fib_send(ContainerCommand,
			    fibptr,
			    sizeof (struct aac_get_name),
			    FsaNormal,
			    1, 1,
			    NULL, NULL);
	if (status < 0 ) {
		printk(KERN_WARNING "aac_get_container_name: SendFIB failed.\n");
	} else {
		dresp = (struct aac_get_name_resp *)fib_data(fibptr);

		status = (le32_to_cpu(dresp->status) != CT_OK)
		      || (dresp->data[0] == '\0');
		if (status == 0) {
			char * sp = dresp->data;
			char * dp = pid;
			do {
				if ((*sp == '\0')
				 || ((dp - pid) >= sizeof(((struct aac_get_name_resp *)NULL)->data))) {
					*dp = ' ';
				} else {
					*dp = *sp++;
				}
			} while (++dp < &pid[sizeof(((struct inquiry_data *)NULL)->inqd_pid)]);
		}
	}
	fib_complete(fibptr);
	fib_free(fibptr);
	fsa_dev[instance] = fsa_dev_ptr;
	return status;
}

/**
 *	probe_container		-	query a logical volume
 *	@dev: device to query
 *	@cid: container identifier
 *
 *	Queries the controller about the given volume. The volume information
 *	is updated in the struct fsa_scsi_hba structure rather than returned.
 */
 
static int probe_container(struct aac_dev *dev, int cid)
{
	struct fsa_scsi_hba *fsa_dev_ptr;
	int status;
	struct aac_query_mount *dinfo;
	struct aac_mount *dresp;
	struct fib * fibptr;
	unsigned instance;

	fsa_dev_ptr = &(dev->fsa_dev);
	instance = dev->scsi_host_ptr->unique_id;

	if (!(fibptr = fib_alloc(dev)))
		return -ENOMEM;

	fib_init(fibptr);

	dinfo = (struct aac_query_mount *)fib_data(fibptr);

	dinfo->command = cpu_to_le32(VM_NameServe);
	dinfo->count = cpu_to_le32(cid);
	dinfo->type = cpu_to_le32(FT_FILESYS);

	status = fib_send(ContainerCommand,
			    fibptr,
			    sizeof(struct aac_query_mount),
			    FsaNormal,
			    1, 1,
			    NULL, NULL);
	if (status < 0) {
		printk(KERN_WARNING "aacraid: probe_containers query failed.\n");
		goto error;
	}

	dresp = (struct aac_mount *) fib_data(fibptr);

	if ((le32_to_cpu(dresp->status) == ST_OK) &&
	    (le32_to_cpu(dresp->mnt[0].vol) != CT_NONE) &&
	    (le32_to_cpu(dresp->mnt[0].state) != FSCS_HIDDEN)) {
		fsa_dev_ptr->valid[cid] = 1;
		fsa_dev_ptr->type[cid] = le32_to_cpu(dresp->mnt[0].vol);
		fsa_dev_ptr->size[cid] = le32_to_cpu(dresp->mnt[0].capacity);
		if (le32_to_cpu(dresp->mnt[0].state) & FSCS_READONLY)
			fsa_dev_ptr->ro[cid] = 1;
	}

error:
	fib_complete(fibptr);
	fib_free(fibptr);

	return status;
}

/* Local Structure to set SCSI inquiry data strings */
struct scsi_inq {
	char vid[8];         /* Vendor ID */
	char pid[16];        /* Product ID */
	char prl[4];         /* Product Revision Level */
};

/**
 *	InqStrCopy	-	string merge
 *	@a:	string to copy from
 *	@b:	string to copy to
 *
 * 	Copy a String from one location to another
 *	without copying \0
 */

static void inqstrcpy(char *a, char *b)
{

	while(*a != (char)0) 
		*b++ = *a++;
}

static char *container_types[] = {
        "None",
        "Volume",
        "Mirror",
        "Stripe",
        "RAID5",
        "SSRW",
        "SSRO",
        "Morph",
        "Legacy",
        "RAID4",
        "RAID10",             
        "RAID00",             
        "V-MIRRORS",          
        "PSEUDO R4",          
	"RAID50",
        "Unknown"
};



/* Function: setinqstr
 *
 * Arguments: [1] pointer to void [1] int
 *
 * Purpose: Sets SCSI inquiry data strings for vendor, product
 * and revision level. Allows strings to be set in platform dependant
 * files instead of in OS dependant driver source.
 */

static void setinqstr(int devtype, void *data, int tindex)
{
	struct scsi_inq *str;
	char *findit;
	struct aac_driver_ident *mp;

	mp = aac_get_driver_ident(devtype);
   
	str = (struct scsi_inq *)(data); /* cast data to scsi inq block */

	inqstrcpy (mp->vname, str->vid); 
	inqstrcpy (mp->model, str->pid); /* last six chars reserved for vol type */

	findit = str->pid;

	for ( ; *findit != ' '; findit++); /* walk till we find a space then incr by 1 */
		findit++;
	
	if (tindex < (sizeof(container_types)/sizeof(char *))){
		inqstrcpy (container_types[tindex], findit);
	}
	inqstrcpy ("V1.0", str->prl);
}

void set_sense(u8 *sense_buf, u8 sense_key, u8 sense_code,
		    u8 a_sense_code, u8 incorrect_length,
		    u8 bit_pointer, u16 field_pointer,
		    u32 residue)
{
	sense_buf[0] = 0xF0;	/* Sense data valid, err code 70h (current error) */
	sense_buf[1] = 0;	/* Segment number, always zero */

	if (incorrect_length) {
		sense_buf[2] = sense_key | 0x20;	/* Set ILI bit | sense key */
		sense_buf[3] = BYTE3(residue);
		sense_buf[4] = BYTE2(residue);
		sense_buf[5] = BYTE1(residue);
		sense_buf[6] = BYTE0(residue);
	} else
		sense_buf[2] = sense_key;	/* Sense key */

	if (sense_key == SENKEY_ILLEGAL)
		sense_buf[7] = 10;	/* Additional sense length */
	else
		sense_buf[7] = 6;	/* Additional sense length */

	sense_buf[12] = sense_code;	/* Additional sense code */
	sense_buf[13] = a_sense_code;	/* Additional sense code qualifier */
	if (sense_key == SENKEY_ILLEGAL) {
		sense_buf[15] = 0;

		if (sense_code == SENCODE_INVALID_PARAM_FIELD)
			sense_buf[15] = 0x80;	/* Std sense key specific field */
		/* Illegal parameter is in the parameter block */

		if (sense_code == SENCODE_INVALID_CDB_FIELD)
			sense_buf[15] = 0xc0;	/* Std sense key specific field */
		/* Illegal parameter is in the CDB block */
		sense_buf[15] |= bit_pointer;
		sense_buf[16] = field_pointer >> 8;	/* MSB */
		sense_buf[17] = field_pointer;		/* LSB */
	}
}

static void aac_io_done(Scsi_Cmnd * scsicmd)
{
	unsigned long cpu_flags;
	spin_lock_irqsave(&io_request_lock, cpu_flags);
	scsicmd->scsi_done(scsicmd);
	spin_unlock_irqrestore(&io_request_lock, cpu_flags);
}

static void __aac_io_done(Scsi_Cmnd * scsicmd)
{
	scsicmd->scsi_done(scsicmd);
}

int aac_get_adapter_info(struct aac_dev* dev)
{
	struct fib* fibptr;
	struct aac_adapter_info* info;
	int rcode;
	u32 tmp;
	if (!(fibptr = fib_alloc(dev)))
		return -ENOMEM;

	fib_init(fibptr);
	info = (struct aac_adapter_info*) fib_data(fibptr);

	memset(info,0,sizeof(struct aac_adapter_info));

	rcode = fib_send(RequestAdapterInfo,
			fibptr, 
			sizeof(struct aac_adapter_info),
			FsaNormal, 
			1, 1, 
			NULL, 
			NULL);

	memcpy(&dev->adapter_info, info, sizeof(struct aac_adapter_info));

	tmp = dev->adapter_info.kernelrev;
	printk(KERN_INFO "%s%d: kernel %d.%d.%d build %d\n", 
			dev->name, dev->id,
			tmp>>24,(tmp>>16)&0xff,(tmp>>8)&0xff,
			dev->adapter_info.kernelbuild);
	tmp = dev->adapter_info.monitorrev;
	printk(KERN_INFO "%s%d: monitor %d.%d.%d build %d\n", 
			dev->name, dev->id,
			tmp>>24,(tmp>>16)&0xff,(tmp>>8)&0xff,
			dev->adapter_info.monitorbuild);
	tmp = dev->adapter_info.biosrev;
	printk(KERN_INFO "%s%d: bios %d.%d.%d build %d\n", 
			dev->name, dev->id,
			tmp>>24,(tmp>>16)&0xff,(tmp>>8)&0xff,
			dev->adapter_info.biosbuild);
	printk(KERN_INFO "%s%d: serial %x%x\n",
			dev->name, dev->id,
			dev->adapter_info.serial[0],
			dev->adapter_info.serial[1]);

	dev->nondasd_support = 0;
	dev->raid_scsi_mode = 0;
	if(dev->adapter_info.options & AAC_OPT_NONDASD){
		dev->nondasd_support = 1;
	}

	/*
	 * If the firmware supports ROMB RAID/SCSI mode and we are currently
	 * in RAID/SCSI mode, set the flag. For now if in this mode we will
	 * force nondasd support on. If we decide to allow the non-dasd flag
	 * additional changes changes will have to be made to support
	 * RAID/SCSI.  the function aac_scsi_cmd in this module will have to be
	 * changed to support the new dev->raid_scsi_mode flag instead of
	 * leaching off of the dev->nondasd_support flag. Also in linit.c the
	 * function aac_detect will have to be modified where it sets up the
	 * max number of channels based on the aac->nondasd_support flag only.
	 */
	if ((dev->adapter_info.options & AAC_OPT_SCSI_MANAGED)
		&& (dev->adapter_info.options & AAC_OPT_RAID_SCSI_MODE))
	{
		dev->nondasd_support = 1;
		dev->raid_scsi_mode = 1;
	}
	if (dev->raid_scsi_mode != 0)
		printk(KERN_INFO "%s%d: ROMB RAID/SCSI mode enabled\n",dev->name, dev->id);
		
	if (nondasd != -1)
		dev->nondasd_support = (nondasd!=0);

	if(dev->nondasd_support != 0)
		printk(KERN_INFO "%s%d: Non-DASD support enabled\n",dev->name, dev->id);

	dev->pae_support = 0;
	if( (sizeof(dma_addr_t) > 4) && (dev->adapter_info.options & AAC_OPT_SGMAP_HOST64)){
		dev->pae_support = 1;
	}

	if(paemode != -1)
		dev->pae_support = (paemode != 0);

	if(dev->pae_support != 0) 
	{
		printk(KERN_INFO "%s%d: 64 Bit PAE enabled\n", dev->name, dev->id);
		pci_set_dma_mask(dev->pdev, (dma_addr_t)0xFFFFFFFFFFFFFFFFULL);
	}

	fib_complete(fibptr);
	fib_free(fibptr);

	return rcode;
}


static void read_callback(void *context, struct fib * fibptr)
{
	struct aac_dev *dev;
	struct aac_read_reply *readreply;
	Scsi_Cmnd *scsicmd;
	u32 lba;
	u32 cid;

	scsicmd = (Scsi_Cmnd *) context;

	dev = (struct aac_dev *)scsicmd->host->hostdata;
	cid =TARGET_LUN_TO_CONTAINER(scsicmd->target, scsicmd->lun);

	lba = ((scsicmd->cmnd[1] & 0x1F) << 16) | (scsicmd->cmnd[2] << 8) | scsicmd->cmnd[3];
	dprintk((KERN_DEBUG "read_callback[cpu %d]: lba = %u, t = %ld.\n", smp_processor_id(), lba, jiffies));

	if (fibptr == NULL)
		BUG();
		
	if(scsicmd->use_sg)
		pci_unmap_sg(dev->pdev, 
			(struct scatterlist *)scsicmd->buffer,
			scsicmd->use_sg,
			scsi_to_pci_dma_dir(scsicmd->sc_data_direction));
	else if(scsicmd->request_bufflen)
		pci_unmap_single(dev->pdev, (dma_addr_t)(unsigned long)scsicmd->SCp.ptr,
				 scsicmd->request_bufflen,
				 scsi_to_pci_dma_dir(scsicmd->sc_data_direction));
	readreply = (struct aac_read_reply *)fib_data(fibptr);
	if (le32_to_cpu(readreply->status) == ST_OK)
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | GOOD;
	else {
		printk(KERN_WARNING "read_callback: read failed, status = %d\n", readreply->status);
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | CHECK_CONDITION;
		set_sense((u8 *) &sense_data[cid],
				    SENKEY_HW_ERR,
				    SENCODE_INTERNAL_TARGET_FAILURE,
				    ASENCODE_INTERNAL_TARGET_FAILURE, 0, 0,
				    0, 0);
	}
	fib_complete(fibptr);
	fib_free(fibptr);

	aac_io_done(scsicmd);
}

static void write_callback(void *context, struct fib * fibptr)
{
	struct aac_dev *dev;
	struct aac_write_reply *writereply;
	Scsi_Cmnd *scsicmd;
	u32 lba;
	u32 cid;

	scsicmd = (Scsi_Cmnd *) context;
	dev = (struct aac_dev *)scsicmd->host->hostdata;
	cid = TARGET_LUN_TO_CONTAINER(scsicmd->target, scsicmd->lun);

	lba = ((scsicmd->cmnd[1] & 0x1F) << 16) | (scsicmd->cmnd[2] << 8) | scsicmd->cmnd[3];
	dprintk((KERN_DEBUG "write_callback[cpu %d]: lba = %u, t = %ld.\n", smp_processor_id(), lba, jiffies));
	if (fibptr == NULL)
		BUG();

	if(scsicmd->use_sg)
		pci_unmap_sg(dev->pdev, 
			(struct scatterlist *)scsicmd->buffer,
			scsicmd->use_sg,
			scsi_to_pci_dma_dir(scsicmd->sc_data_direction));
	else if(scsicmd->request_bufflen)
		pci_unmap_single(dev->pdev, (dma_addr_t)(unsigned long)scsicmd->SCp.ptr,
				 scsicmd->request_bufflen,
				 scsi_to_pci_dma_dir(scsicmd->sc_data_direction));

	writereply = (struct aac_write_reply *) fib_data(fibptr);
	if (le32_to_cpu(writereply->status) == ST_OK)
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | GOOD;
	else {
		printk(KERN_WARNING "write_callback: write failed, status = %d\n", writereply->status);
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | CHECK_CONDITION;
		set_sense((u8 *) &sense_data[cid],
				    SENKEY_HW_ERR,
				    SENCODE_INTERNAL_TARGET_FAILURE,
				    ASENCODE_INTERNAL_TARGET_FAILURE, 0, 0,
				    0, 0);
	}

	fib_complete(fibptr);
	fib_free(fibptr);
	aac_io_done(scsicmd);
}

int aac_read(Scsi_Cmnd * scsicmd, int cid)
{
	u32 lba;
	u32 count;
	int status;

	u16 fibsize;
	struct aac_dev *dev;
	struct fib * cmd_fibcontext;

	dev = (struct aac_dev *)scsicmd->host->hostdata;
	/*
	 *	Get block address and transfer length
	 */
	if (scsicmd->cmnd[0] == SS_READ)	/* 6 byte command */
	{
		dprintk((KERN_DEBUG "aachba: received a read(6) command on target %d.\n", cid));

		lba = ((scsicmd->cmnd[1] & 0x1F) << 16) | (scsicmd->cmnd[2] << 8) | scsicmd->cmnd[3];
		count = scsicmd->cmnd[4];

		if (count == 0)
			count = 256;
	} else {
		dprintk((KERN_DEBUG "aachba: received a read(10) command on target %d.\n", cid));

		lba = (scsicmd->cmnd[2] << 24) | (scsicmd->cmnd[3] << 16) | (scsicmd->cmnd[4] << 8) | scsicmd->cmnd[5];
		count = (scsicmd->cmnd[7] << 8) | scsicmd->cmnd[8];
	}
	dprintk((KERN_DEBUG "aac_read[cpu %d]: lba = %u, t = %ld.\n", smp_processor_id(), lba, jiffies));
	/*
	 *	Alocate and initialize a Fib
	 */
	if (!(cmd_fibcontext = fib_alloc(dev))) {
		scsicmd->result = DID_ERROR << 16;
		aac_io_done(scsicmd);
		return (-1);
	}

	fib_init(cmd_fibcontext);

	if(dev->pae_support == 1){
		struct aac_read64 *readcmd;
		readcmd = (struct aac_read64 *) fib_data(cmd_fibcontext);
		readcmd->command = cpu_to_le32(VM_CtHostRead64);
		readcmd->cid = cpu_to_le16(cid);
		readcmd->sector_count = cpu_to_le16(count);
		readcmd->block = cpu_to_le32(lba);
		readcmd->pad   = cpu_to_le16(0);
		readcmd->flags = cpu_to_le16(0); 
		
		aac_build_sg64(scsicmd, &readcmd->sg);
		if(readcmd->sg.count > MAX_DRIVER_SG_SEGMENT_COUNT)
			BUG();
		fibsize = sizeof(struct aac_read64) + ((readcmd->sg.count - 1) * sizeof (struct sgentry64));
		/*
		 *	Now send the Fib to the adapter
		 */
		status = fib_send(ContainerCommand64, 
			  cmd_fibcontext, 
			  fibsize, 
			  FsaNormal, 
			  0, 1, 
			  (fib_callback) read_callback, 
			  (void *) scsicmd);
	} else {
		struct aac_read *readcmd;
		readcmd = (struct aac_read *) fib_data(cmd_fibcontext);
		readcmd->command = cpu_to_le32(VM_CtBlockRead);
		readcmd->cid = cpu_to_le32(cid);
		readcmd->block = cpu_to_le32(lba);
		readcmd->count = cpu_to_le32(count * 512);

		if (count * 512 > (64 * 1024))
			BUG();

		aac_build_sg(scsicmd, &readcmd->sg);
		if(readcmd->sg.count > MAX_DRIVER_SG_SEGMENT_COUNT)
			BUG();
		fibsize = sizeof(struct aac_read) + ((readcmd->sg.count - 1) * sizeof (struct sgentry));
		/*
		 *	Now send the Fib to the adapter
		 */
		status = fib_send(ContainerCommand, 
			  cmd_fibcontext, 
			  fibsize, 
			  FsaNormal, 
			  0, 1, 
			  (fib_callback) read_callback, 
			  (void *) scsicmd);
	}
	
	
	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS) 
		return 0;
		
	printk(KERN_WARNING "aac_read: fib_send failed with status: %d.\n", status);
	/*
	 *	For some reason, the Fib didn't queue, return QUEUE_FULL
	 */
	scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | QUEUE_FULL;
	aac_io_done(scsicmd);
	fib_complete(cmd_fibcontext);
	fib_free(cmd_fibcontext);
	return -1;
}

static int aac_write(Scsi_Cmnd * scsicmd, int cid)
{
	u32 lba;
	u32 count;
	int status;
	u16 fibsize;
	struct aac_dev *dev;
	struct fib * cmd_fibcontext;

	dev = (struct aac_dev *)scsicmd->host->hostdata;
	/*
	 *	Get block address and transfer length
	 */
	if (scsicmd->cmnd[0] == SS_WRITE)	/* 6 byte command */
	{
		lba = ((scsicmd->cmnd[1] & 0x1F) << 16) | (scsicmd->cmnd[2] << 8) | scsicmd->cmnd[3];
		count = scsicmd->cmnd[4];
		if (count == 0)
			count = 256;
	} else {
		dprintk((KERN_DEBUG "aachba: received a write(10) command on target %d.\n", cid));
		lba = (scsicmd->cmnd[2] << 24) | (scsicmd->cmnd[3] << 16) | (scsicmd->cmnd[4] << 8) | scsicmd->cmnd[5];
		count = (scsicmd->cmnd[7] << 8) | scsicmd->cmnd[8];
	}
	dprintk((KERN_DEBUG "aac_write[cpu %d]: lba = %u, t = %ld.\n", smp_processor_id(), lba, jiffies));
	/*
	 *	Allocate and initialize a Fib then setup a BlockWrite command
	 */
	if (!(cmd_fibcontext = fib_alloc(dev))) {
		scsicmd->result = DID_ERROR << 16;
		aac_io_done(scsicmd);
		return -1;
	}
	fib_init(cmd_fibcontext);

	if(dev->pae_support == 1)
	{
		struct aac_write64 *writecmd;
		writecmd = (struct aac_write64 *) fib_data(cmd_fibcontext);
		writecmd->command = cpu_to_le32(VM_CtHostWrite64);
		writecmd->cid = cpu_to_le16(cid);
		writecmd->sector_count = cpu_to_le16(count); 
		writecmd->block = cpu_to_le32(lba);
		writecmd->pad	= cpu_to_le16(0);
		writecmd->flags	= cpu_to_le16(0);

		aac_build_sg64(scsicmd, &writecmd->sg);
		if(writecmd->sg.count > MAX_DRIVER_SG_SEGMENT_COUNT)
			BUG();
		fibsize = sizeof(struct aac_write64) + ((writecmd->sg.count - 1) * sizeof (struct sgentry64));
		/*
		 *	Now send the Fib to the adapter
		 */
		status = fib_send(ContainerCommand64, 
			  cmd_fibcontext, 
			  fibsize, 
			  FsaNormal, 
			  0, 1, 
			  (fib_callback) write_callback, 
			  (void *) scsicmd);
	}
	else 
	{
		struct aac_write *writecmd;
		writecmd = (struct aac_write *) fib_data(cmd_fibcontext);
		writecmd->command = cpu_to_le32(VM_CtBlockWrite);
		writecmd->cid = cpu_to_le32(cid);
		writecmd->block = cpu_to_le32(lba);
		writecmd->count = cpu_to_le32(count * 512);
		writecmd->sg.count = cpu_to_le32(1);
		/* ->stable is not used - it did mean which type of write */

		if (count * 512 > (64 * 1024))
			BUG();
		aac_build_sg(scsicmd, &writecmd->sg);
		if(writecmd->sg.count > MAX_DRIVER_SG_SEGMENT_COUNT)
			BUG();
		fibsize = sizeof(struct aac_write) + ((writecmd->sg.count - 1) * sizeof (struct sgentry));
		/*
		 *	Now send the Fib to the adapter
		 */
		status = fib_send(ContainerCommand, 
			  cmd_fibcontext, 
			  fibsize, 
			  FsaNormal, 
			  0, 1, 
			  (fib_callback) write_callback, 
			  (void *) scsicmd);
	}

	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS)
		return 0;

	printk(KERN_WARNING "aac_write: fib_send failed with status: %d\n", status);
	/*
	 *	For some reason, the Fib didn't queue, return QUEUE_FULL
	 */
	scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | QUEUE_FULL;
	aac_io_done(scsicmd);

	fib_complete(cmd_fibcontext);
	fib_free(cmd_fibcontext);
	return -1;
}


/**
 *	aac_scsi_cmd()		-	Process SCSI command
 *	@scsicmd:		SCSI command block
 *	@wait:			1 if the user wants to await completion
 *
 *	Emulate a SCSI command and queue the required request for the
 *	aacraid firmware.
 */
 
int aac_scsi_cmd(Scsi_Cmnd * scsicmd)
{
	u32 cid = 0;
	struct fsa_scsi_hba *fsa_dev_ptr;
	int cardtype;
	int ret;
	struct aac_dev *dev = (struct aac_dev *)scsicmd->host->hostdata;
	
	cardtype = dev->cardtype;

	fsa_dev_ptr = fsa_dev[scsicmd->host->unique_id];

	/*
	 *	If the bus, target or lun is out of range, return fail
	 *	Test does not apply to ID 16, the pseudo id for the controller
	 *	itself.
	 */
	if (scsicmd->target != scsicmd->host->this_id) {
		if ((scsicmd->channel == 0) ){
			if( (scsicmd->target >= AAC_MAX_TARGET) || (scsicmd->lun != 0)){ 
				scsicmd->result = DID_NO_CONNECT << 16;
				__aac_io_done(scsicmd);
				return 0;
			}
			cid = TARGET_LUN_TO_CONTAINER(scsicmd->target, scsicmd->lun);

			/*
			 *	If the target container doesn't exist, it may have
			 *	been newly created
			 */
			if (fsa_dev_ptr->valid[cid] == 0) {
				switch (scsicmd->cmnd[0]) {
				case SS_INQUIR:
				case SS_RDCAP:
				case SS_TEST:
					spin_unlock_irq(&io_request_lock);
					probe_container(dev, cid);
					spin_lock_irq(&io_request_lock);
					if (fsa_dev_ptr->valid[cid] == 0) {
						scsicmd->result = DID_NO_CONNECT << 16;
						__aac_io_done(scsicmd);
						return 0;
					}
				default:
					break;
				}
			}
			/*
			 *	If the target container still doesn't exist, 
			 *	return failure
			 */
			if (fsa_dev_ptr->valid[cid] == 0) {
				scsicmd->result = DID_BAD_TARGET << 16;
				__aac_io_done(scsicmd);
				return -1;
			}
		} else {  /* check for physical non-dasd devices */
			if(dev->nondasd_support == 1){
				return aac_send_srb_fib(scsicmd);
			} else {
				scsicmd->result = DID_NO_CONNECT << 16;
				__aac_io_done(scsicmd);
				return 0;
			}
		}
	}
	/*
	 * else Command for the controller itself
	 */
	else if ((scsicmd->cmnd[0] != SS_INQUIR) &&	/* only INQUIRY & TUR cmnd supported for controller */
		(scsicmd->cmnd[0] != SS_TEST)) 
	{
		dprintk((KERN_WARNING "Only INQUIRY & TUR command supported for controller, rcvd = 0x%x.\n", scsicmd->cmnd[0]));
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | CHECK_CONDITION;
		set_sense((u8 *) &sense_data[cid],
			    SENKEY_ILLEGAL,
			    SENCODE_INVALID_COMMAND,
			    ASENCODE_INVALID_COMMAND, 0, 0, 0, 0);
		__aac_io_done(scsicmd);
		return -1;
	}


	/* Handle commands here that don't really require going out to the adapter */
	switch (scsicmd->cmnd[0]) {
	case SS_INQUIR:
	{
		struct inquiry_data *inq_data_ptr;

		dprintk((KERN_DEBUG "INQUIRY command, ID: %d.\n", scsicmd->target));
		inq_data_ptr = (struct inquiry_data *)scsicmd->request_buffer;
		memset(inq_data_ptr, 0, sizeof (struct inquiry_data));

		inq_data_ptr->inqd_ver = 2;	/* claim compliance to SCSI-2 */
		inq_data_ptr->inqd_rdf = 2;	/* A response data format value of two indicates that the data shall be in the format specified in SCSI-2 */
		inq_data_ptr->inqd_len = 31;
		/*Format for "pad2" is  RelAdr | WBus32 | WBus16 |  Sync  | Linked |Reserved| CmdQue | SftRe */
		inq_data_ptr->inqd_pad2= 0x32 ;	 /*WBus16|Sync|CmdQue */
		/*
		 *	Set the Vendor, Product, and Revision Level
		 *	see: <vendor>.c i.e. aac.c
		 */
		if (scsicmd->target == scsicmd->host->this_id) {
			setinqstr(cardtype, (void *) (inq_data_ptr->inqd_vid), (sizeof(container_types)/sizeof(char *)));
			inq_data_ptr->inqd_pdt = INQD_PDT_PROC;	/* Processor device */
		} else {
			setinqstr(cardtype, (void *) (inq_data_ptr->inqd_vid), fsa_dev_ptr->type[cid]);
			aac_get_container_name(dev, cid, inq_data_ptr->inqd_pid);
			inq_data_ptr->inqd_pdt = INQD_PDT_DA;	/* Direct/random access device */
		}
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | GOOD;
		__aac_io_done(scsicmd);
		return 0;
	}
	case SS_RDCAP:
	{
		int capacity;
		char *cp;

		dprintk((KERN_DEBUG "READ CAPACITY command.\n"));
		capacity = fsa_dev_ptr->size[cid] - 1;
		cp = scsicmd->request_buffer;
		cp[0] = (capacity >> 24) & 0xff;
		cp[1] = (capacity >> 16) & 0xff;
		cp[2] = (capacity >> 8) & 0xff;
		cp[3] = (capacity >> 0) & 0xff;
		cp[4] = 0;
		cp[5] = 0;
		cp[6] = 2;
		cp[7] = 0;

		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | GOOD;
		__aac_io_done(scsicmd);

		return 0;
	}

	case SS_MODESEN:
	{
		char *mode_buf;

		dprintk((KERN_DEBUG "MODE SENSE command.\n"));
		mode_buf = scsicmd->request_buffer;
		mode_buf[0] = 0;	/* Mode data length (MSB) */
		mode_buf[1] = 6;	/* Mode data length (LSB) */
		mode_buf[2] = 0;	/* Medium type - default */
		mode_buf[3] = 0;	/* Device-specific param, bit 8: 0/1 = write enabled/protected */
		mode_buf[4] = 0;	/* reserved */
		mode_buf[5] = 0;	/* reserved */
		mode_buf[6] = 0;	/* Block descriptor length (MSB) */
		mode_buf[7] = 0;	/* Block descriptor length (LSB) */

		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | GOOD;
		__aac_io_done(scsicmd);

		return 0;
	}
	case SS_REQSEN:
		dprintk((KERN_DEBUG "REQUEST SENSE command.\n"));
		memcpy(scsicmd->sense_buffer, &sense_data[cid], sizeof (struct sense_data));
		memset(&sense_data[cid], 0, sizeof (struct sense_data));
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | GOOD;
		__aac_io_done(scsicmd);
		return (0);

	case SS_LOCK:
		dprintk((KERN_DEBUG "LOCK command.\n"));
		if (scsicmd->cmnd[4])
			fsa_dev_ptr->locked[cid] = 1;
		else
			fsa_dev_ptr->locked[cid] = 0;

		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | GOOD;
		__aac_io_done(scsicmd);
		return 0;
	/*
	 *	These commands are all No-Ops
	 */
	case SS_TEST:
	case SS_RESERV:
	case SS_RELES:
	case SS_REZERO:
	case SS_REASGN:
	case SS_SEEK:
	case SS_ST_SP:
		scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | GOOD;
		__aac_io_done(scsicmd);
		return (0);
	}

	switch (scsicmd->cmnd[0]) 
	{
		case SS_READ:
		case SM_READ:
			/*
			 *	Hack to keep track of ordinal number of the device that
			 *	corresponds to a container. Needed to convert
			 *	containers to /dev/sd device names
			 */
			 
			spin_unlock_irq(&io_request_lock);
			fsa_dev_ptr->devno[cid] = DEVICE_NR(scsicmd->request.rq_dev);
			ret = aac_read(scsicmd, cid);
			spin_lock_irq(&io_request_lock);
			return ret;

		case SS_WRITE:
		case SM_WRITE:
			spin_unlock_irq(&io_request_lock);
			ret = aac_write(scsicmd, cid);
			spin_lock_irq(&io_request_lock);
			return ret;
		default:
			/*
			 *	Unhandled commands
			 */
			printk(KERN_WARNING "Unhandled SCSI Command: 0x%x.\n", scsicmd->cmnd[0]);
			scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | CHECK_CONDITION;
			set_sense((u8 *) &sense_data[cid],
				SENKEY_ILLEGAL, SENCODE_INVALID_COMMAND,
			ASENCODE_INVALID_COMMAND, 0, 0, 0, 0);
			__aac_io_done(scsicmd);
			return 0;
	}
}

static int query_disk(struct aac_dev *dev, void *arg)
{
	struct aac_query_disk qd;
	struct fsa_scsi_hba *fsa_dev_ptr;

	fsa_dev_ptr = &(dev->fsa_dev);
	if (copy_from_user(&qd, arg, sizeof (struct aac_query_disk)))
		return -EFAULT;
	if (qd.cnum == -1)
		qd.cnum = TARGET_LUN_TO_CONTAINER(qd.target, qd.lun);
	else if ((qd.bus == -1) && (qd.target == -1) && (qd.lun == -1)) 
	{
		if (qd.cnum < 0 || qd.cnum > MAXIMUM_NUM_CONTAINERS)
			return -EINVAL;
		qd.instance = dev->scsi_host_ptr->host_no;
		qd.bus = 0;
		qd.target = CONTAINER_TO_TARGET(qd.cnum);
		qd.lun = CONTAINER_TO_LUN(qd.cnum);
	}
	else return -EINVAL;

	qd.valid = fsa_dev_ptr->valid[qd.cnum];
	qd.locked = fsa_dev_ptr->locked[qd.cnum];
	qd.deleted = fsa_dev_ptr->deleted[qd.cnum];

	if (fsa_dev_ptr->devno[qd.cnum] == -1)
		qd.unmapped = 1;
	else
		qd.unmapped = 0;

	get_sd_devname(fsa_dev_ptr->devno[qd.cnum], qd.name);

	if (copy_to_user(arg, &qd, sizeof (struct aac_query_disk)))
		return -EFAULT;
	return 0;
}

static void get_sd_devname(int disknum, char *buffer)
{
	if (disknum < 0) {
		sprintf(buffer, "%s", "");
		return;
	}

	if (disknum < 26)
		sprintf(buffer, "sd%c", 'a' + disknum);
	else {
		unsigned int min1;
		unsigned int min2;
		/*
		 * For larger numbers of disks, we need to go to a new
		 * naming scheme.
		 */
		min1 = disknum / 26;
		min2 = disknum % 26;
		sprintf(buffer, "sd%c%c", 'a' + min1 - 1, 'a' + min2);
	}
}

static int force_delete_disk(struct aac_dev *dev, void *arg)
{
	struct aac_delete_disk dd;
	struct fsa_scsi_hba *fsa_dev_ptr;

	fsa_dev_ptr = &(dev->fsa_dev);

	if (copy_from_user(&dd, arg, sizeof (struct aac_delete_disk)))
		return -EFAULT;

	if (dd.cnum > MAXIMUM_NUM_CONTAINERS)
		return -EINVAL;
	/*
	 *	Mark this container as being deleted.
	 */
	fsa_dev_ptr->deleted[dd.cnum] = 1;
	/*
	 *	Mark the container as no longer valid
	 */
	fsa_dev_ptr->valid[dd.cnum] = 0;
	return 0;
}

static int delete_disk(struct aac_dev *dev, void *arg)
{
	struct aac_delete_disk dd;
	struct fsa_scsi_hba *fsa_dev_ptr;

	fsa_dev_ptr = &(dev->fsa_dev);

	if (copy_from_user(&dd, arg, sizeof (struct aac_delete_disk)))
		return -EFAULT;

	if (dd.cnum > MAXIMUM_NUM_CONTAINERS)
		return -EINVAL;
	/*
	 *	If the container is locked, it can not be deleted by the API.
	 */
	if (fsa_dev_ptr->locked[dd.cnum])
		return -EBUSY;
	else {
		/*
		 *	Mark the container as no longer being valid.
		 */
		fsa_dev_ptr->valid[dd.cnum] = 0;
		fsa_dev_ptr->devno[dd.cnum] = -1;
		return 0;
	}
}

int aac_dev_ioctl(struct aac_dev *dev, int cmd, void *arg)
{
	switch (cmd) {
	case FSACTL_QUERY_DISK:
		return query_disk(dev, arg);
	case FSACTL_DELETE_DISK:
		return delete_disk(dev, arg);
	case FSACTL_FORCE_DELETE_DISK:
		return force_delete_disk(dev, arg);
	case 2131:
		return aac_get_containers(dev);
	default:
		return -ENOTTY;
	}
}

/**
 *
 * aac_srb_callback
 * @context: the context set in the fib - here it is scsi cmd
 * @fibptr: pointer to the fib
 *
 * Handles the completion of a scsi command to a non dasd device
 *
 */

static void aac_srb_callback(void *context, struct fib * fibptr)
{
	struct aac_dev *dev;
	struct aac_srb_reply *srbreply;
	Scsi_Cmnd *scsicmd;

	scsicmd = (Scsi_Cmnd *) context;
	dev = (struct aac_dev *)scsicmd->host->hostdata;

	if (fibptr == NULL)
		BUG();

	srbreply = (struct aac_srb_reply *) fib_data(fibptr);

	scsicmd->sense_buffer[0] = '\0';  // initialize sense valid flag to false
	// calculate resid for sg 
	scsicmd->resid = scsicmd->request_bufflen - srbreply->data_xfer_length;

	if(scsicmd->use_sg)
		pci_unmap_sg(dev->pdev, 
			(struct scatterlist *)scsicmd->buffer,
			scsicmd->use_sg,
			scsi_to_pci_dma_dir(scsicmd->sc_data_direction));
	else if(scsicmd->request_bufflen)
		pci_unmap_single(dev->pdev, (ulong)scsicmd->SCp.ptr, scsicmd->request_bufflen,
			scsi_to_pci_dma_dir(scsicmd->sc_data_direction));

	/*
	 * First check the fib status
	 */

	if (le32_to_cpu(srbreply->status) != ST_OK){
		int len;
		printk(KERN_WARNING "aac_srb_callback: srb failed, status = %d\n", le32_to_cpu(srbreply->status));
		len = (srbreply->sense_data_size > sizeof(scsicmd->sense_buffer))?
				sizeof(scsicmd->sense_buffer):srbreply->sense_data_size;
		scsicmd->result = DID_ERROR << 16 | COMMAND_COMPLETE << 8 | CHECK_CONDITION;
		memcpy(scsicmd->sense_buffer, srbreply->sense_data, len);
	}

	/*
	 * Next check the srb status
	 */
	switch( (le32_to_cpu(srbreply->srb_status))&0x3f){
	case SRB_STATUS_ERROR_RECOVERY:
	case SRB_STATUS_PENDING:
	case SRB_STATUS_SUCCESS:
		if(scsicmd->cmnd[0] == INQUIRY ){
			u8 b;
			u8 b1;
			/* We can't expose disk devices because we can't tell whether they
			 * are the raw container drives or stand alone drives.  If they have
			 * the removable bit set then we should expose them though.
			 */
			b = (*(u8*)scsicmd->buffer)&0x1f;
			b1 = ((u8*)scsicmd->buffer)[1];
			if( b==TYPE_TAPE || b==TYPE_WORM || b==TYPE_ROM || b==TYPE_MOD|| b==TYPE_MEDIUM_CHANGER 
					|| (b==TYPE_DISK && (b1&0x80)) ){
				scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
			/*
			 * We will allow disk devices if in RAID/SCSI mode and
			 * the channel is 2
			 */
			} else if((dev->raid_scsi_mode)&&(scsicmd->channel == 2)){
				scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
			} else {
				scsicmd->result = DID_NO_CONNECT << 16 | COMMAND_COMPLETE << 8;
			}
		} else {
			scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
		}
		break;
	case SRB_STATUS_DATA_OVERRUN:
		switch(scsicmd->cmnd[0]){
		case  READ_6:
		case  WRITE_6:
		case  READ_10:
		case  WRITE_10:
		case  READ_12:
		case  WRITE_12:
			if(le32_to_cpu(srbreply->data_xfer_length) < scsicmd->underflow ) {
				printk(KERN_WARNING"aacraid: SCSI CMD underflow\n");
			} else {
				printk(KERN_WARNING"aacraid: SCSI CMD Data Overrun\n");
			}
			scsicmd->result = DID_ERROR << 16 | COMMAND_COMPLETE << 8;
			break;
		case INQUIRY: {
			u8 b;
			u8 b1;
			/* We can't expose disk devices because we can't tell whether they
			* are the raw container drives or stand alone drives
			*/
			b = (*(u8*)scsicmd->buffer)&0x0f;
			b1 = ((u8*)scsicmd->buffer)[1];
			if( b==TYPE_TAPE || b==TYPE_WORM || b==TYPE_ROM || b==TYPE_MOD|| b==TYPE_MEDIUM_CHANGER
					|| (b==TYPE_DISK && (b1&0x80)) ){
				scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
			/*
			 * We will allow disk devices if in RAID/SCSI mode and
			 * the channel is 2
			 */
			} else if((dev->raid_scsi_mode)&&(scsicmd->channel == 2)){
				scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
			} else {
				scsicmd->result = DID_NO_CONNECT << 16 | COMMAND_COMPLETE << 8;
			}
			break;
		}
		default:
			scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8;
			break;
		}
		break;
	case SRB_STATUS_ABORTED:
		scsicmd->result = DID_ABORT << 16 | ABORT << 8;
		break;
	case SRB_STATUS_ABORT_FAILED:
		// Not sure about this one - but assuming the hba was trying to abort for some reason
		scsicmd->result = DID_ERROR << 16 | ABORT << 8;
		break;
	case SRB_STATUS_PARITY_ERROR:
		scsicmd->result = DID_PARITY << 16 | MSG_PARITY_ERROR << 8;
		break;
	case SRB_STATUS_NO_DEVICE:
	case SRB_STATUS_INVALID_PATH_ID:
	case SRB_STATUS_INVALID_TARGET_ID:
	case SRB_STATUS_INVALID_LUN:
	case SRB_STATUS_SELECTION_TIMEOUT:
		scsicmd->result = DID_NO_CONNECT << 16 | COMMAND_COMPLETE << 8;
		break;

	case SRB_STATUS_COMMAND_TIMEOUT:
	case SRB_STATUS_TIMEOUT:
		scsicmd->result = DID_TIME_OUT << 16 | COMMAND_COMPLETE << 8;
		break;

	case SRB_STATUS_BUSY:
		scsicmd->result = DID_NO_CONNECT << 16 | COMMAND_COMPLETE << 8;
		break;

	case SRB_STATUS_BUS_RESET:
		scsicmd->result = DID_RESET << 16 | COMMAND_COMPLETE << 8;
		break;

	case SRB_STATUS_MESSAGE_REJECTED:
		scsicmd->result = DID_ERROR << 16 | MESSAGE_REJECT << 8;
		break;
	case SRB_STATUS_REQUEST_FLUSHED:
	case SRB_STATUS_ERROR:
	case SRB_STATUS_INVALID_REQUEST:
	case SRB_STATUS_REQUEST_SENSE_FAILED:
	case SRB_STATUS_NO_HBA:
	case SRB_STATUS_UNEXPECTED_BUS_FREE:
	case SRB_STATUS_PHASE_SEQUENCE_FAILURE:
	case SRB_STATUS_BAD_SRB_BLOCK_LENGTH:
	case SRB_STATUS_DELAYED_RETRY:
	case SRB_STATUS_BAD_FUNCTION:
	case SRB_STATUS_NOT_STARTED:
	case SRB_STATUS_NOT_IN_USE:
	case SRB_STATUS_FORCE_ABORT:
	case SRB_STATUS_DOMAIN_VALIDATION_FAIL:
	default:
		printk("aacraid: SRB ERROR(%u) %s scsi cmd 0x%x - scsi status 0x%x\n",le32_to_cpu(srbreply->srb_status&0x3f),aac_get_status_string(le32_to_cpu(srbreply->srb_status)), scsicmd->cmnd[0], le32_to_cpu(srbreply->scsi_status) );
		scsicmd->result = DID_ERROR << 16 | COMMAND_COMPLETE << 8;
		break;
	}
	if (le32_to_cpu(srbreply->scsi_status) == 0x02 ){  // Check Condition
		int len;
		scsicmd->result |= CHECK_CONDITION;
		len = (srbreply->sense_data_size > sizeof(scsicmd->sense_buffer))?
				sizeof(scsicmd->sense_buffer):srbreply->sense_data_size;
		printk(KERN_WARNING "aac_srb_callback: check condition, status = %d len=%d\n", le32_to_cpu(srbreply->status), len);
		memcpy(scsicmd->sense_buffer, srbreply->sense_data, len);
	}
	/*
	 * OR in the scsi status (already shifted up a bit)
	 */
	scsicmd->result |= le32_to_cpu(srbreply->scsi_status);

	fib_complete(fibptr);
	fib_free(fibptr);
	aac_io_done(scsicmd);
}

/**
 *
 * aac_send_scb_fib
 * @scsicmd: the scsi command block
 *
 * This routine will form a FIB and fill in the aac_srb from the 
 * scsicmd passed in.
 */

static int aac_send_srb_fib(Scsi_Cmnd* scsicmd)
{
	struct fib* cmd_fibcontext;
	struct aac_dev* dev;
	int status;
	struct aac_srb *srbcmd;
	u16 fibsize;
	u32 flag;
	u32 timeout;

	if( scsicmd->target > 15 || scsicmd->lun > 7) {
		scsicmd->result = DID_NO_CONNECT << 16;
		__aac_io_done(scsicmd);
		return 0;
	}

	dev = (struct aac_dev *)scsicmd->host->hostdata;
	switch(scsicmd->sc_data_direction){
	case SCSI_DATA_WRITE:
		flag = SRB_DataOut;
		break;
	case SCSI_DATA_UNKNOWN:  
		flag = SRB_DataIn | SRB_DataOut;
		break;
	case SCSI_DATA_READ:
		flag = SRB_DataIn;
		break;
	case SCSI_DATA_NONE: 
	default:
		flag = SRB_NoDataXfer;
		break;
	}


	/*
	 *	Allocate and initialize a Fib then setup a BlockWrite command
	 */
	if (!(cmd_fibcontext = fib_alloc(dev))) {
		scsicmd->result = DID_ERROR << 16;
		__aac_io_done(scsicmd);
		return -1;
	}
	fib_init(cmd_fibcontext);

	srbcmd = (struct aac_srb*) fib_data(cmd_fibcontext);
	srbcmd->function = cpu_to_le32(SRBF_ExecuteScsi);
	srbcmd->channel  = cpu_to_le32(aac_logical_to_phys(scsicmd->channel));
	srbcmd->target   = cpu_to_le32(scsicmd->target);
	srbcmd->lun      = cpu_to_le32(scsicmd->lun);
	srbcmd->flags    = cpu_to_le32(flag);
	timeout = (scsicmd->timeout-jiffies)/HZ;
	if(timeout == 0){
		timeout = 1;
	}
	srbcmd->timeout  = cpu_to_le32(timeout);  // timeout in seconds
	srbcmd->retry_limit =cpu_to_le32(0); // Obsolete parameter
	srbcmd->cdb_size = cpu_to_le32(scsicmd->cmd_len);
	
	if( dev->pae_support ==1 ) {
		aac_build_sg64(scsicmd, (struct sgmap64*) &srbcmd->sg);
		srbcmd->count = cpu_to_le32(scsicmd->request_bufflen);

		memset(srbcmd->cdb, 0, sizeof(srbcmd->cdb));
		memcpy(srbcmd->cdb, scsicmd->cmnd, scsicmd->cmd_len);
		/*
		 *	Build Scatter/Gather list
		 */
		fibsize = sizeof (struct aac_srb) - sizeof (struct sgentry) + ((srbcmd->sg.count & 0xff) * sizeof (struct sgentry64));

		/*
		 *	Now send the Fib to the adapter
		 */
		status = fib_send(ScsiPortCommand64, cmd_fibcontext, fibsize, FsaNormal, 0, 1,
				  (fib_callback) aac_srb_callback, (void *) scsicmd);
	} else {
		aac_build_sg(scsicmd, (struct sgmap*)&srbcmd->sg);
		srbcmd->count = cpu_to_le32(scsicmd->request_bufflen);

		memset(srbcmd->cdb, 0, sizeof(srbcmd->cdb));
		memcpy(srbcmd->cdb, scsicmd->cmnd, scsicmd->cmd_len);
		/*
		 *	Build Scatter/Gather list
		 */
		fibsize = sizeof (struct aac_srb) + (((srbcmd->sg.count & 0xff) - 1) * sizeof (struct sgentry));

		/*
		 *	Now send the Fib to the adapter
		 */
		status = fib_send(ScsiPortCommand, cmd_fibcontext, fibsize, FsaNormal, 0, 1,
				  (fib_callback) aac_srb_callback, (void *) scsicmd);
	}
	/*
	 *	Check that the command queued to the controller
	 */
	if (status == -EINPROGRESS){
		return 0;
	}

	printk(KERN_WARNING "aac_srb: fib_send failed with status: %d\n", status);
	/*
	 *	For some reason, the Fib didn't queue, return QUEUE_FULL
	 */
	scsicmd->result = DID_OK << 16 | COMMAND_COMPLETE << 8 | QUEUE_FULL;
	__aac_io_done(scsicmd);

	fib_complete(cmd_fibcontext);
	fib_free(cmd_fibcontext);

	return -1;
}

static unsigned long aac_build_sg(Scsi_Cmnd* scsicmd, struct sgmap* psg)
{
	struct aac_dev *dev;
	unsigned long byte_count = 0;

	dev = (struct aac_dev *)scsicmd->host->hostdata;
	// Get rid of old data
	psg->count = cpu_to_le32(0);
	psg->sg[0].addr = cpu_to_le32(NULL);
	psg->sg[0].count = cpu_to_le32(0);  
	if (scsicmd->use_sg) {
		struct scatterlist *sg;
		int i;
		int sg_count;
		sg = (struct scatterlist *) scsicmd->request_buffer;

		sg_count = pci_map_sg(dev->pdev, sg, scsicmd->use_sg,
			scsi_to_pci_dma_dir(scsicmd->sc_data_direction));
		psg->count = cpu_to_le32(sg_count);

		byte_count = 0;

		for (i = 0; i < sg_count; i++) {
			psg->sg[i].addr = cpu_to_le32(sg_dma_address(sg));
			psg->sg[i].count = cpu_to_le32(sg_dma_len(sg));
			byte_count += sg_dma_len(sg);
			sg++;
		}
		/* hba wants the size to be exact */
		if(byte_count > scsicmd->request_bufflen){
			psg->sg[i-1].count -= (byte_count - scsicmd->request_bufflen);
			byte_count = scsicmd->request_bufflen;
		}
		/* Check for command underflow */
		if(scsicmd->underflow && (byte_count < scsicmd->underflow)){
			printk(KERN_WARNING"aacraid: cmd len %08lX cmd underflow %08X\n",
					byte_count, scsicmd->underflow);
		}
	}
	else if(scsicmd->request_bufflen) {
		dma_addr_t addr; 
		addr = pci_map_single(dev->pdev,
				scsicmd->request_buffer,
				scsicmd->request_bufflen,
				scsi_to_pci_dma_dir(scsicmd->sc_data_direction));
		psg->count = cpu_to_le32(1);
		psg->sg[0].addr = cpu_to_le32(addr);
		psg->sg[0].count = cpu_to_le32(scsicmd->request_bufflen);  
		/* Cast to pointer from integer of different size */
		scsicmd->SCp.ptr = (void *)addr;
		byte_count = scsicmd->request_bufflen;
	}
	return byte_count;
}


static unsigned long aac_build_sg64(Scsi_Cmnd* scsicmd, struct sgmap64* psg)
{
	struct aac_dev *dev;
	unsigned long byte_count = 0;
	u64 le_addr;

	dev = (struct aac_dev *)scsicmd->host->hostdata;
	// Get rid of old data
	psg->count = cpu_to_le32(0);
	psg->sg[0].addr[0] = cpu_to_le32(NULL);
	psg->sg[0].addr[1] = cpu_to_le32(NULL);
	psg->sg[0].count = cpu_to_le32(0);  
	if (scsicmd->use_sg) {
		struct scatterlist *sg;
		int i;
		int sg_count;
		sg = (struct scatterlist *) scsicmd->request_buffer;

		sg_count = pci_map_sg(dev->pdev, sg, scsicmd->use_sg,
			scsi_to_pci_dma_dir(scsicmd->sc_data_direction));
		psg->count = cpu_to_le32(sg_count);

		byte_count = 0;

		for (i = 0; i < sg_count; i++) {
			le_addr = cpu_to_le64(sg_dma_address(sg));
			psg->sg[i].addr[1] = (u32)(le_addr>>32);
			psg->sg[i].addr[0] = (u32)(le_addr & 0xffffffff);
			psg->sg[i].count = cpu_to_le32(sg_dma_len(sg));
			byte_count += sg_dma_len(sg);
			sg++;
		}
		/* hba wants the size to be exact */
		if(byte_count > scsicmd->request_bufflen){
			psg->sg[i-1].count -= (byte_count - scsicmd->request_bufflen);
			byte_count = scsicmd->request_bufflen;
		}
		/* Check for command underflow */
		if(scsicmd->underflow && (byte_count < scsicmd->underflow)){
			printk(KERN_WARNING"aacraid: cmd len %08lX cmd underflow %08X\n",
					byte_count, scsicmd->underflow);
		}
	}
	else if(scsicmd->request_bufflen) {
		dma_addr_t addr; 
		addr = pci_map_single(dev->pdev,
				scsicmd->request_buffer,
				scsicmd->request_bufflen,
				scsi_to_pci_dma_dir(scsicmd->sc_data_direction));
		psg->count = cpu_to_le32(1);
		le_addr = cpu_to_le64(addr);
		psg->sg[0].addr[1] = (u32)(le_addr>>32);
		psg->sg[0].addr[0] = (u32)(le_addr & 0xffffffff);
		psg->sg[0].count = cpu_to_le32(scsicmd->request_bufflen);  
		/* Cast to pointer from integer of different size */
		scsicmd->SCp.ptr = (void *)addr;
		byte_count = scsicmd->request_bufflen;
	}
	return byte_count;
}

struct aac_srb_status_info {
	u32	status;
	char	*str;
};


static struct aac_srb_status_info srb_status_info[] = {
	{ SRB_STATUS_PENDING,		"Pending Status"},
	{ SRB_STATUS_SUCCESS,		"Success"},
	{ SRB_STATUS_ABORTED,		"Aborted Command"},
	{ SRB_STATUS_ABORT_FAILED,	"Abort Failed"},
	{ SRB_STATUS_ERROR,		"Error Event"}, 
	{ SRB_STATUS_BUSY,		"Device Busy"},
	{ SRB_STATUS_INVALID_REQUEST,	"Invalid Request"},
	{ SRB_STATUS_INVALID_PATH_ID,	"Invalid Path ID"},
	{ SRB_STATUS_NO_DEVICE,		"No Device"},
	{ SRB_STATUS_TIMEOUT,		"Timeout"},
	{ SRB_STATUS_SELECTION_TIMEOUT,	"Selection Timeout"},
	{ SRB_STATUS_COMMAND_TIMEOUT,	"Command Timeout"},
	{ SRB_STATUS_MESSAGE_REJECTED,	"Message Rejected"},
	{ SRB_STATUS_BUS_RESET,		"Bus Reset"},
	{ SRB_STATUS_PARITY_ERROR,	"Parity Error"},
	{ SRB_STATUS_REQUEST_SENSE_FAILED,"Request Sense Failed"},
	{ SRB_STATUS_NO_HBA,		"No HBA"},
	{ SRB_STATUS_DATA_OVERRUN,	"Data Overrun/Data Underrun"},
	{ SRB_STATUS_UNEXPECTED_BUS_FREE,"Unexpected Bus Free"},
	{ SRB_STATUS_PHASE_SEQUENCE_FAILURE,"Phase Error"},
	{ SRB_STATUS_BAD_SRB_BLOCK_LENGTH,"Bad Srb Block Length"},
	{ SRB_STATUS_REQUEST_FLUSHED,	"Request Flushed"},
	{ SRB_STATUS_DELAYED_RETRY,	"Delayed Retry"},
	{ SRB_STATUS_INVALID_LUN,	"Invalid LUN"}, 
	{ SRB_STATUS_INVALID_TARGET_ID,	"Invalid TARGET ID"},
	{ SRB_STATUS_BAD_FUNCTION,	"Bad Function"},
	{ SRB_STATUS_ERROR_RECOVERY,	"Error Recovery"},
	{ SRB_STATUS_NOT_STARTED,	"Not Started"},
	{ SRB_STATUS_NOT_IN_USE,	"Not In Use"},
    	{ SRB_STATUS_FORCE_ABORT,	"Force Abort"},
	{ SRB_STATUS_DOMAIN_VALIDATION_FAIL,"Domain Validation Failure"},
	{ 0xff,				"Unknown Error"}
};

char *aac_get_status_string(u32 status)
{
	int i;

	for(i=0; i < (sizeof(srb_status_info)/sizeof(struct aac_srb_status_info)); i++ ){
		if(srb_status_info[i].status == status){
			return srb_status_info[i].str;
		}
	}

	return "Bad Status Code";
}

