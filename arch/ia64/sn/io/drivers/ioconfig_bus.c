/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 *  ioconfig_bus - SGI's Persistent PCI Bus Numbering.
 *
 * Copyright (C) 1992-1997, 2000-2003 Silicon Graphics, Inc.  All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/pci.h>

#include <asm/sn/sgi.h>
#include <asm/uaccess.h>

#include <linux/devfs_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/io.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm//sn/sn_sal.h>
#include <asm/sn/addrs.h>
#include <asm/sn/ioconfig_bus.h>

#define SGI_IOCONFIG_BUS "SGI-PERSISTENT PCI BUS NUMBERING"
#define SGI_IOCONFIG_BUS_VERSION "1.0"

/*
 * Some Global definitions.
 */
static vertex_hdl_t ioconfig_bus_handle;
static unsigned long ioconfig_bus_debug;
static struct ioconfig_parm parm;

#ifdef IOCONFIG_BUS_DEBUG
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif

static u64 ioconfig_file;
static u64 ioconfig_file_size;
static u64 ioconfig_activated;
static char ioconfig_kernopts[128];

/*
 * For debugging purpose .. hardcode a table ..
 */
struct  ascii_moduleid *ioconfig_bus_table;
u64 ioconfig_bus_table_size;


static int free_entry;
static int new_entry;

int next_basebus_number;

void
ioconfig_get_busnum(char *io_moduleid, int *bus_num)
{
	struct	ascii_moduleid  *temp;
	int index;

	DBG("ioconfig_get_busnum io_moduleid %s\n", io_moduleid);

	*bus_num = -1;
	temp = ioconfig_bus_table;
	for (index = 0; index < free_entry; temp++, index++) {
		if ( (io_moduleid[0] == temp->io_moduleid[0]) &&
		     (io_moduleid[1] == temp->io_moduleid[1]) &&
		     (io_moduleid[2] == temp->io_moduleid[2]) &&
		     (io_moduleid[4] == temp->io_moduleid[4]) &&
		     (io_moduleid[5] == temp->io_moduleid[5]) ) {
			*bus_num = index * 0x10;
			return;
		}
	}

	/*
	 * New IO Brick encountered.
	 */
	if (((int)io_moduleid[0]) == 0) {
		DBG("ioconfig_get_busnum: Invalid Module Id given %s\n", io_moduleid);
		return;
	}

	io_moduleid[3] = '#';
	strcpy((char *)&(ioconfig_bus_table[free_entry].io_moduleid), io_moduleid);
	*bus_num = free_entry * 0x10;
	free_entry++;
}

static void
dump_ioconfig_table(void)
{

	int index = 0;
	struct ascii_moduleid *temp;

	temp = ioconfig_bus_table;
	while (index < free_entry) {
		DBG("ASSCI Module ID %s\n", temp->io_moduleid);
		temp++;
		index++;
	}
}

/*
 * nextline
 *	This routine returns the nextline in the buffer.
 */
int nextline(char *buffer, char **next, char *line)
{

	char *temp;

	if (buffer[0] == 0x0) {
		return(0);
	}

	temp = buffer;
	while (*temp != 0) {
		*line = *temp;
		if (*temp != '\n'){
			*line = *temp;
			temp++; line++;
		} else
			break;
	}

	if (*temp == 0)
		*next = temp;
	else
		*next = ++temp;

	return(1);
}

/*
 * build_pcibus_name
 *	This routine parses the ioconfig contents read into
 *	memory by ioconfig command in EFI and builds the
 *	persistent pci bus naming table.
 */
void
build_moduleid_table(char *file_contents, struct ascii_moduleid *table)
{
	/*
	 * Read the whole file into memory.
	 */
	int rc;
	char *name;
	char *temp;
	char *next;
	char *current;
	char *line;
	struct ascii_moduleid *moduleid;

	line = kmalloc(256, GFP_KERNEL);
	if (line <= 0)
		BUG(); /* Do not want to continue system boot .. */
	memset(line, 0,256);
	name = kmalloc(125, GFP_KERNEL);
	if (name <= 0){
		BUG(); /* Do not want to continue system boot .. */
	}
	memset(name, 0, 125);
	moduleid = table;
	current = file_contents;
	while (nextline(current, &next, line)){

		DBG("current 0x%lx next 0x%lx\n", current, next);

		temp = line;
		/*
		 * Skip all leading Blank lines ..
		 */
		while (isspace(*temp))
			if (*temp != '\n')
				temp++;
			else
				break;

		if (*temp == '\n') {
			current = next;
			memset(line, 0, 256);
			continue;
		}
 
		/*
		 * Skip comment lines
		 */
		if (*temp == '#') {
			current = next;
			memset(line, 0, 256);
			continue;
		}

		/*
		 * Get the next free entry in the table.
		 */
		rc = sscanf(temp, "%s", name);
		strcpy(&moduleid->io_moduleid[0], name);
		DBG("Found %s\n", name);
		moduleid++;
		free_entry++;
		current = next;
		memset(line, 0, 256);
	}

	new_entry = free_entry;
	kfree(line);
	kfree(name);

	return;
}

void
ioconfig_bus_init(void)
{

	struct ia64_sal_retval ret_stuff;
	u64	*temp;
	int	cnode;

	DBG("ioconfig_bus_init called.\n");

        for (cnode = 0; cnode < numnodes; cnode++) {
		nasid_t nasid;
		/*
	 	 * Make SAL call to get the address of the bus configuration table.
	 	 */
		ret_stuff.status = (uint64_t)0;
		ret_stuff.v0 = (uint64_t)0;
		ret_stuff.v1 = (uint64_t)0;
		ret_stuff.v2 = (uint64_t)0;
		nasid = COMPACT_TO_NASID_NODEID(cnode);
		SAL_CALL(ret_stuff, SN_SAL_BUS_CONFIG, 0, nasid, 0, 0, 0, 0, 0);
		temp = (u64 *)TO_NODE_CAC(nasid, ret_stuff.v0);
		ioconfig_file = *temp;
		DBG("ioconfig_bus_init: Nasid %d ret_stuff.v0 0x%lx\n", nasid,
			ret_stuff.v0);
		if (ioconfig_file) {
			ioconfig_file_size = ret_stuff.v1;
			ioconfig_file = (ioconfig_file | CACHEABLE_MEM_SPACE);
			ioconfig_activated = 1;
			break;
		}
	}

	DBG("ioconfig_bus_init: ret_stuff.v0 %p ioconfig_file %p %d\n",
		ret_stuff.v0, (void *)ioconfig_file, (int)ioconfig_file_size);

	ioconfig_bus_table = kmalloc( 512, GFP_KERNEL );
	if (ioconfig_bus_table <= 0)
		BUG(); /* Seriously, we should not be out of memory at init */

	memset(ioconfig_bus_table, 0, 512);

	/*
	 * If ioconfig options are given on the bootline .. take it.
	 */
	if (*ioconfig_kernopts != '\0') {
		/*
		 * ioconfig="..." kernel options given.
		 */
		DBG("ioconfig_bus_init: Kernel Options given.\n");
		(void) build_moduleid_table((char *)ioconfig_kernopts, ioconfig_bus_table);
		(void) dump_ioconfig_table();
	}

	if (ioconfig_activated) {
		DBG("ioconfig_bus_init: ioconfig file given.\n");
		(void) build_moduleid_table((char *)ioconfig_file, ioconfig_bus_table);
		(void) dump_ioconfig_table();
	} else {
		DBG("ioconfig_bus_init: ioconfig command not executed in prom\n");
	}

}

void
ioconfig_bus_new_entries(void)
{

	
	int index = 0;
	struct ascii_moduleid *temp;

	if ((ioconfig_activated) && (free_entry > new_entry)) {
		printk("### Please add the following new IO Bricks Module ID \n");
		printk("### to your Persistent Bus Numbering Config File\n");
	} else
		return;

	index = new_entry;
	temp = &ioconfig_bus_table[index];
        while (index < free_entry) {
                printk("%s\n", (char *)temp);
		temp++;
                index++;
        }
	printk("### End\n");

}
static int ioconfig_bus_ioctl(struct inode * inode, struct file * file,
        unsigned int cmd, unsigned long arg)
{

	int length;
	/*
	 * Copy in the parameters.
	 */
	length = copy_from_user(&parm, (char *)arg, sizeof(struct ioconfig_parm));
	if (length <= 0)
		return -EFAULT;
	parm.number = free_entry - new_entry;
	parm.ioconfig_activated = ioconfig_activated;
	if (copy_to_user((char *)arg, &parm, sizeof(struct ioconfig_parm)))
		return -EFAULT;

	if (copy_to_user((char *)parm.buffer, &ioconfig_bus_table[new_entry], sizeof(struct  ascii_moduleid) * (free_entry - new_entry)))
		return -EFAULT;

	return 0;
}

/*
 * ioconfig_bus_open - Opens the special device node "/dev/hw/.ioconfig_bus".
 */
static int ioconfig_bus_open(struct inode * inode, struct file * filp)
{
	if (ioconfig_bus_debug) {
        	DBG("ioconfig_bus_open called.\n");
	}

        return(0);

}

/*
 * ioconfig_bus_close - Closes the special device node "/dev/hw/.ioconfig_bus".
 */
static int ioconfig_bus_close(struct inode * inode, struct file * filp)
{

	if (ioconfig_bus_debug) {
        	DBG("ioconfig_bus_close called.\n");
	}

        return(0);
}

struct file_operations ioconfig_bus_fops = {
	ioctl:ioconfig_bus_ioctl,
	open:ioconfig_bus_open,		/* open */
	release:ioconfig_bus_close	/* release */
};


/*
 * init_ifconfig_bus() - Boot time initialization.  Ensure that it is called 
 *	after devfs has been initialized.
 *
 */
int init_ioconfig_bus(void)
{
	ioconfig_bus_handle = NULL;
	ioconfig_bus_handle = hwgraph_register(hwgraph_root, ".ioconfig_bus",
			0, DEVFS_FL_AUTO_DEVNUM,
			0, 0,
			S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, 0, 0,
			&ioconfig_bus_fops, NULL);

	if (ioconfig_bus_handle == NULL) {
		panic("Unable to create SGI PERSISTENT BUS NUMBERING Driver.\n");
	}

	return(0);

}

static int __init ioconfig_bus_setup (char *str)
{

	char *temp;

	DBG("ioconfig_bus_setup: Kernel Options %s\n", str);

	temp = (char *)ioconfig_kernopts;
	memset(temp, 0, 128);
	while ( (*str != '\0') && !isspace (*str) ) {
		if (*str == ',') {
			*temp = '\n';
			temp++;
			str++;
			continue;
		}
		*temp = *str;
		temp++;
		str++;
	}

	return(0);
		
}
__setup("ioconfig=", ioconfig_bus_setup);
