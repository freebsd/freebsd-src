/*
 * PowerPC64 LPAR Configuration Information Driver
 *
 * Dave Engebretsen engebret@us.ibm.com
 *    Copyright (c) 2003 Dave Engebretsen
 * Will Schmidt willschm@us.ibm.com
 *    SPLPAR updates, Copyright (c) 2003 Will Schmidt IBM Corporation.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 * This driver creates a proc file at /proc/ppc64/lparcfg which contains
 * keyword - value pairs that specify the configuration of the partition.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/iSeries/HvLpConfig.h>
#include <asm/iSeries/ItLpPaca.h>
#include <asm/hvcall.h>
#include <asm/cputable.h>

#define MODULE_VERSION "1.0"
#define MODULE_NAME "lparcfg"

static struct proc_dir_entry *proc_ppc64_lparcfg;
#define LPARCFG_BUFF_SIZE 4096

#ifdef CONFIG_PPC_ISERIES
static unsigned char e2a(unsigned char x)
{
	switch (x) {
	case 0xF0:
		return '0';
	case 0xF1:
		return '1';
	case 0xF2:
		return '2';
	case 0xF3:
		return '3';
	case 0xF4:
		return '4';
	case 0xF5:
		return '5';
	case 0xF6:
		return '6';
	case 0xF7:
		return '7';
	case 0xF8:
		return '8';
	case 0xF9:
		return '9';
	case 0xC1:
		return 'A';
	case 0xC2:
		return 'B';
	case 0xC3:
		return 'C';
	case 0xC4:
		return 'D';
	case 0xC5:
		return 'E';
	case 0xC6:
		return 'F';
	case 0xC7:
		return 'G';
	case 0xC8:
		return 'H';
	case 0xC9:
		return 'I';
	case 0xD1:
		return 'J';
	case 0xD2:
		return 'K';
	case 0xD3:
		return 'L';
	case 0xD4:
		return 'M';
	case 0xD5:
		return 'N';
	case 0xD6:
		return 'O';
	case 0xD7:
		return 'P';
	case 0xD8:
		return 'Q';
	case 0xD9:
		return 'R';
	case 0xE2:
		return 'S';
	case 0xE3:
		return 'T';
	case 0xE4:
		return 'U';
	case 0xE5:
		return 'V';
	case 0xE6:
		return 'W';
	case 0xE7:
		return 'X';
	case 0xE8:
		return 'Y';
	case 0xE9:
		return 'Z';
	}
	return ' ';
}

/*
 * Methods used to fetch LPAR data when running on an iSeries platform.
 */
static int lparcfg_data(unsigned char *buf, unsigned long size)
{
	unsigned long n = 0, pool_id, lp_index;
	int shared, entitled_capacity, max_entitled_capacity;
	int processors, max_processors;
	struct paca_struct *lpaca = get_paca();

	if((buf == NULL) || (size > LPARCFG_BUFF_SIZE)) {
		return -EFAULT;
	}
	memset(buf, 0, size);

	shared = (int)(lpaca->xLpPacaPtr->xSharedProc);
	n += snprintf(buf, LPARCFG_BUFF_SIZE - n,
		      "serial_number=%c%c%c%c%c%c%c\n",
		      e2a(xItExtVpdPanel.mfgID[2]),
		      e2a(xItExtVpdPanel.mfgID[3]),
		      e2a(xItExtVpdPanel.systemSerial[1]),
		      e2a(xItExtVpdPanel.systemSerial[2]),
		      e2a(xItExtVpdPanel.systemSerial[3]),
		      e2a(xItExtVpdPanel.systemSerial[4]),
		      e2a(xItExtVpdPanel.systemSerial[5]));

	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "system_type=%c%c%c%c\n",
		      e2a(xItExtVpdPanel.machineType[0]),
		      e2a(xItExtVpdPanel.machineType[1]),
		      e2a(xItExtVpdPanel.machineType[2]),
		      e2a(xItExtVpdPanel.machineType[3]));

	lp_index = HvLpConfig_getLpIndex();
	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "partition_id=%d\n", (int)lp_index);

	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "system_active_processors=%d\n",
		      (int)HvLpConfig_getSystemPhysicalProcessors());

	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "system_potential_processors=%d\n",
		      (int)HvLpConfig_getSystemPhysicalProcessors());

	processors = (int)HvLpConfig_getPhysicalProcessors();
	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "partition_active_processors=%d\n", processors);

	max_processors = (int)HvLpConfig_getMaxPhysicalProcessors();
	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "partition_potential_processors=%d\n", max_processors);

	if(shared) {
		entitled_capacity = HvLpConfig_getSharedProcUnits();
		max_entitled_capacity = HvLpConfig_getMaxSharedProcUnits();
	} else {
		entitled_capacity = processors * 100;
		max_entitled_capacity = max_processors * 100;
	}
	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "partition_entitled_capacity=%d\n", entitled_capacity);

	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "partition_max_entitled_capacity=%d\n",
		      max_entitled_capacity);

	if(shared) {
		pool_id = HvLpConfig_getSharedPoolIndex();
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n, "pool=%d\n",
			      (int)pool_id);
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "pool_capacity=%d\n", (int)(HvLpConfig_getNumProcsInSharedPool(pool_id)*100));
	}

	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "shared_processor_mode=%d\n", shared);

	return 0;
}
#endif /* CONFIG_PPC_ISERIES */

#ifdef CONFIG_PPC_PSERIES
/*
 * Methods used to fetch LPAR data when running on a pSeries platform.
 */

/*
 * H_GET_PPP hcall returns info in 4 parms.
 *  entitled_capacity,unallocated_capacity,
 *  aggregation, resource_capability).
 *
 *  R4 = Entitled Processor Capacity Percentage.
 *  R5 = Unallocated Processor Capacity Percentage.
 *  R6 (AABBCCDDEEFFGGHH).
 *      XXXX - reserved (0)
 *          XXXX - reserved (0)
 *              XXXX - Group Number
 *                  XXXX - Pool Number.
 *  R7 (PPOONNMMLLKKJJII)
 *      XX - reserved. (0)
 *        XX - bit 0-6 reserved (0).   bit 7 is Capped indicator.
 *          XX - variable processor Capacity Weight
 *            XX - Unallocated Variable Processor Capacity Weight.
 *              XXXX - Active processors in Physical Processor Pool.
 *                  XXXX  - Processors active on platform.
 */
unsigned int h_get_ppp(unsigned long *entitled,unsigned long  *unallocated,unsigned long *aggregation,unsigned long *resource)
{
	unsigned long rc;
	rc = plpar_hcall_4out(H_GET_PPP,0,0,0,0,entitled,unallocated,aggregation,resource);
	return 0;
}

/*
 * get_splpar_potential_characteristics().
 * Retrieve the potential_processors and max_entitled_capacity values
 * through the get-system-parameter rtas call.
 */
#define SPLPAR_CHARACTERISTICS_TOKEN 20
#define SPLPAR_MAXLENGTH 1026*(sizeof(char))
unsigned int get_splpar_potential_characteristics()
{
	/* return 0 for now.  Underlying rtas functionality is not yet complete. 12/01/2003*/
	return 0;
#if 0
	long call_status;
	unsigned long ret[2];

	char * buffer = kmalloc(SPLPAR_MAXLENGTH, GFP_KERNEL);

	printk("token for ibm,get-system-parameter (0x%x)\n",rtas_token("ibm,get-system-parameter"));

	call_status = rtas_call(rtas_token("ibm,get-system-parameter"), 3, 1,
				NULL,
				SPLPAR_CHARACTERISTICS_TOKEN,
				&buffer,
				SPLPAR_MAXLENGTH,
				(void *)&ret);

	if (call_status!=0) {
		printk("Error calling get-system-parameter (0x%lx)\n",call_status);
		kfree(buffer);
		return -1;
	} else {
		printk("get-system-parameter (%s)\n",buffer);
		kfree(buffer);
		/* TODO: Add code here to parse out value for system_potential_processors and partition_max_entitled_capacity */
		return 1;
	}
#endif
}

static int lparcfg_data(unsigned char *buf, unsigned long size)
{
	unsigned long n = 0;
	int shared, max_entitled_capacity;
	int processors, system_active_processors, system_potential_processors;
	struct device_node *root;
	const char *model = "";
	const char *system_id = "";
	unsigned int *lp_index_ptr, lp_index = 0;
	struct device_node *rtas_node;
	int *ip;
	unsigned long h_entitled,h_unallocated,h_aggregation,h_resource;

	if((buf == NULL) || (size > LPARCFG_BUFF_SIZE)) {
		return -EFAULT;
	}
	memset(buf, 0, size);

	root = find_path_device("/");
	if (root) {
		model = get_property(root, "model", NULL);
		system_id = get_property(root, "system-id", NULL);
		lp_index_ptr = (unsigned int *)get_property(root, "ibm,partition-no", NULL);
		if(lp_index_ptr) lp_index = *lp_index_ptr;
	}

	n  = snprintf(buf, LPARCFG_BUFF_SIZE - n,
		      "serial_number=%s\n", system_id);

	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "system_type=%s\n", model);

	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "partition_id=%d\n", (int)lp_index);

	rtas_node = find_path_device("/rtas");
	ip = (int *)get_property(rtas_node, "ibm,lrdr-capacity", NULL);
	if (ip == NULL) {
		system_active_processors = systemcfg->processorCount;
	} else {
		system_active_processors = *(ip + 4);
	}

	if (cur_cpu_spec->firmware_features & FW_FEATURE_SPLPAR) {
		h_get_ppp(&h_entitled,&h_unallocated,&h_aggregation,&h_resource);
#ifdef DEBUG
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "R4=0x%lx\n", h_entitled);
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "R5=0x%lx\n", h_unallocated);
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "R6=0x%lx\n", h_aggregation);
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "R7=0x%lx\n", h_resource);
#endif /* DEBUG */
	}

	if (cur_cpu_spec->firmware_features & FW_FEATURE_SPLPAR) {
		system_potential_processors =  get_splpar_potential_characteristics();
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "system_active_processors=%d\n",
			      (h_resource >> 2*8) & 0xffff);
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "system_potential_processors=%d\n",
			      system_potential_processors);
	} else {
		system_potential_processors = system_active_processors;
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "system_active_processors=%d\n",
			      system_active_processors);
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "system_potential_processors=%d\n",
			      system_potential_processors);
	}

	processors = systemcfg->processorCount;
	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "partition_active_processors=%d\n", processors);
	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "partition_potential_processors=%d\n",
		      system_active_processors);

	max_entitled_capacity = system_active_processors * 100;
	if (cur_cpu_spec->firmware_features & FW_FEATURE_SPLPAR) {
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "partition_entitled_capacity=%ld\n", h_entitled);
	} else {
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "partition_entitled_capacity=%d\n", system_active_processors*100);
	}

	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "partition_max_entitled_capacity=%d\n",
		      max_entitled_capacity);

	shared = 0;
	n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
		      "shared_processor_mode=%d\n", shared);

	if (cur_cpu_spec->firmware_features & FW_FEATURE_SPLPAR) {
		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "pool=%d\n", (h_aggregation >> 0*8)&0xffff);

		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "pool_capacity=%d\n", (h_resource >> 3*8) &0xffff);

		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "group=%d\n", (h_aggregation >> 2*8)&0xffff);

		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "capped=%d\n", (h_resource >> 6*8)&0x40);

		n += snprintf(buf+n, LPARCFG_BUFF_SIZE - n,
			      "capacity_weight=%d\n", (int)(h_resource>>5*8)&0xFF);
	}
	return 0;
}
#endif /* CONFIG_PPC_PSERIES */


static ssize_t lparcfg_read(struct file *file, char *buf,
			    size_t count, loff_t *ppos)
{
	struct proc_dir_entry *dp = file->f_dentry->d_inode->u.generic_ip;
	unsigned long *data = (unsigned long *)dp->data;
	unsigned long p;
	ssize_t read;
	char * pnt;

	if (!data) {
		printk(KERN_ERR "lparcfg: read failed no data\n");
		return -EIO;
	}

	if(ppos) {
		p = *ppos;
	} else {
		return -EFAULT;
	}

	if (p >= LPARCFG_BUFF_SIZE) return 0;

	lparcfg_data((unsigned char *)data, LPARCFG_BUFF_SIZE);
	if (count > (strlen((char *)data) - p))
		count = (strlen((char *)data)) - p;
	read = 0;

	pnt = (char *)(data) + p;
	copy_to_user(buf, (void *)pnt, count);
	read += count;
	*ppos += read;
	return read;
}

static int lparcfg_open(struct inode * inode, struct file * file)
{
	struct proc_dir_entry *dp = file->f_dentry->d_inode->u.generic_ip;
	unsigned int *data = (unsigned int *)dp->data;

	if (!data) {
		printk(KERN_ERR "lparcfg: open failed no data\n");
		return -EIO;
	}

	return 0;
}

struct file_operations lparcfg_fops = {
	owner:		THIS_MODULE,
	read:		lparcfg_read,
	open:		lparcfg_open,
};

int __init lparcfg_init(void)
{
	struct proc_dir_entry *ent;

	ent = create_proc_entry("ppc64/lparcfg", S_IRUSR, NULL);
	if (ent) {
		ent->proc_fops = &lparcfg_fops;
		ent->data = kmalloc(LPARCFG_BUFF_SIZE, GFP_KERNEL);
		if (!ent->data) {
			printk(KERN_ERR "Failed to allocate buffer for lparcfg\n");
			remove_proc_entry("lparcfg", ent->parent);
			return -ENOMEM;
		}
	} else {
		printk(KERN_ERR "Failed to create ppc64/lparcfg\n");
		return -EIO;
	}

	proc_ppc64_lparcfg = ent;
	return 0;
}

void __exit lparcfg_cleanup(void)
{
	if (proc_ppc64_lparcfg) {
		if (proc_ppc64_lparcfg->data) {
		    kfree(proc_ppc64_lparcfg->data);
		}
		remove_proc_entry("lparcfg", proc_ppc64_lparcfg->parent);
	}
}

module_init(lparcfg_init);
module_exit(lparcfg_cleanup);
MODULE_DESCRIPTION("Interface for LPAR configuration data");
MODULE_AUTHOR("Dave Engebretsen");
MODULE_LICENSE("GPL");
