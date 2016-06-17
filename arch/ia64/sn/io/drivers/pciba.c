/*
 * arch/ia64/sn/io/pciba.c
 *
 * IRIX PCIBA-inspired user mode PCI interface
 *
 * requires: devfs
 *
 * device nodes show up in /dev/pci/BB/SS.F (where BB is the bus the
 * device is on, SS is the slot the device is in, and F is the
 * device's function on a multi-function card).
 *
 * when compiled into the kernel, it will only be initialized by the
 * sgi sn1 specific initialization code.  in this case, device nodes
 * are under /dev/hw/..../
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file "COPYING" in the main directory of
 * this archive for more details.
 *
 * Copyright (C) 2001-2003 Silicon Graphics, Inc.  All rights reserved.
 *
 * 03262001 - Initial version by Chad Talbott
 */


/* jesse's beefs:

   register_pci_device should be documented
   
   grossness with do_swap should be documented
   
   big, gross union'ized node_data should be replaced with independent
   structures

   replace global list of nodes with global lists of resources.  could
   use object oriented approach of allocating and cleaning up
   resources.
   
*/


#include <linux/config.h>
#include <linux/module.h>
#include <asm/sn/sgi.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <linux/pci.h>
#include <linux/list.h>

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/raw.h>
#include <linux/capability.h>

#include <asm/uaccess.h>
#include <asm/sn/sgi.h>
#include <asm/io.h>
#include <asm/pgalloc.h>
#include <asm/page.h>
#include <asm/system.h>

#include <asm/sn/pci/pciba.h>


MODULE_DESCRIPTION("User mode PCI interface");
MODULE_AUTHOR("Chad Talbott");


#undef DEBUG_PCIBA
/* #define DEBUG_PCIBA */

#undef TRACE_PCIBA
/* #define TRACE_PCIBA */

#if defined(DEBUG_PCIBA)
#  define DPRINTF(x...) printk(KERN_DEBUG x)
#else
#  define DPRINTF(x...)
#endif

#if defined(TRACE_PCIBA)
#  if defined(__GNUC__)
#    define TRACE()	printk(KERN_DEBUG "%s:%d:%s\n", \
			       __FILE__, __LINE__, __FUNCTION__)
#  else
#    define TRACE()	printk(KERN_DEBUG "%s:%d\n", __LINE__, __FILE__)
#  endif
#else
#  define TRACE()
#endif


typedef enum { failure, success } status;
typedef enum { false, true } boolean;


/* major data structures:

   struct node_data -
   
   	one for each file registered with devfs.  contains everything
   	that any file's fops would need to know about.

   struct dma_allocation -

   	a single DMA allocation.  only the 'dma' nodes care about
   	these.  they are there primarily to allow the driver to look
   	up the kernel virtual address of dma buffers allocated by
   	pci_alloc_consistent, as the application is only given the
   	physical address (to program the device's dma, presumably) and
   	cannot supply the kernel virtual address when freeing the
   	buffer.

	it's also useful to maintain a list of buffers allocated
	through a specific node to allow some sanity checking by this
	driver.  this prevents (for example) a broken application from
	freeing buffers that it didn't allocate, or buffers allocated
	on another node.
   
   global_node_list -

   	a list of all nodes allocated.  this allows the driver to free
   	all the memory it has 'kmalloc'd in case of an error, or on
   	module removal.

   global_dma_list -

        a list of all dma buffers allocated by this driver.  this
	allows the driver to 'pci_free_consistent' all buffers on
	module removal or error.

*/


struct node_data {
	/* flat list of all the device nodes.  makes it easy to free
	   them all when we're unregistered */
	struct list_head global_node_list;
	vertex_hdl_t devfs_handle;

	void (* cleanup)(struct node_data *);

	union {
		struct {
			struct pci_dev * dev;
			struct list_head dma_allocs;
			boolean mmapped;
		} dma;
		struct {
			struct pci_dev * dev;
			u32 saved_rom_base_reg;
			boolean mmapped;
		} rom;
		struct {
			struct resource * res;
		} base;
		struct {
			struct pci_dev * dev;
		} config;
	} u;
};

struct dma_allocation {
	struct list_head list;

	dma_addr_t handle;
	void * va;
	size_t size;
};


static LIST_HEAD(global_node_list);
static LIST_HEAD(global_dma_list);


/* module entry points */
int __init pciba_init(void);
void __exit pciba_exit(void);

static status __init register_with_devfs(void);
static void __exit unregister_with_devfs(void);

static status __init register_pci_device(vertex_hdl_t device_dir_handle,
					 struct pci_dev * dev);

/* file operations */
static int generic_open(struct inode * inode, struct file * file);
static int rom_mmap(struct file * file, struct vm_area_struct * vma);
static int rom_release(struct inode * inode, struct file * file);
static int base_mmap(struct file * file, struct vm_area_struct * vma);
static int config_ioctl(struct inode * inode, struct file * file, 
			unsigned int cmd, 
			unsigned long arg);
static int dma_ioctl(struct inode * inode, struct file * file, 
		     unsigned int cmd, 
		     unsigned long arg);
static int dma_mmap(struct file * file, struct vm_area_struct * vma);

/* support routines */
static int mmap_pci_address(struct vm_area_struct * vma, unsigned long pci_va);
static int mmap_kernel_address(struct vm_area_struct * vma, void * kernel_va);

#ifdef DEBUG_PCIBA
static void dump_nodes(struct list_head * nodes);
static void dump_allocations(struct list_head * dalp);
#endif

/* file operations for each type of node */
static struct file_operations rom_fops = {
	owner:		THIS_MODULE,
	mmap:		rom_mmap,
	open:		generic_open,
	release:	rom_release
};
 

static struct file_operations base_fops = {
	owner:		THIS_MODULE,
	mmap:		base_mmap,
	open:		generic_open
};


static struct file_operations config_fops = {
	owner:		THIS_MODULE,
	ioctl:		config_ioctl,
	open:		generic_open
};	

static struct file_operations dma_fops = {
	owner:		THIS_MODULE,
	ioctl:		dma_ioctl,
	mmap:		dma_mmap,
	open:		generic_open
};	


module_init(pciba_init);
module_exit(pciba_exit);


int __init
pciba_init(void)
{
	if (!ia64_platform_is("sn2"))
		return -ENODEV;

	TRACE();

	if (register_with_devfs() == failure)
		return 1; /* failure */

	printk("PCIBA (a user mode PCI interface) initialized.\n");

	return 0; /* success */
}


void __exit
pciba_exit(void)
{
	TRACE();

	/* FIXME: should also free all that memory that we allocated
           ;) */
	unregister_with_devfs();
}


# if 0
static void __exit
free_nodes(void)
{
	struct node_data * nd;
	
	TRACE();

	list_for_each(nd, &node_list) {
		kfree(list_entry(nd, struct nd, node_list));
	}
}
#endif


static vertex_hdl_t pciba_devfs_handle;


extern vertex_hdl_t
devfn_to_vertex(unsigned char busnum, unsigned int devfn);

static status __init
register_with_devfs(void)
{
	struct pci_dev * dev;
	vertex_hdl_t device_dir_handle;

	TRACE();

	/* FIXME: don't forget /dev/.../pci/mem & /dev/.../pci/io */

	pci_for_each_dev(dev) {
		device_dir_handle = devfn_to_vertex(dev->bus->number,
						    dev->devfn);
		if (device_dir_handle == NULL)
			return failure;
	
		if (register_pci_device(device_dir_handle, dev) == failure) {
			hwgraph_vertex_destroy(pciba_devfs_handle);
			return failure;
		}
	}

	return success;
}

static void __exit
unregister_with_devfs(void)
{
	struct list_head * lhp;
	struct node_data * nd;
	
	TRACE();

	list_for_each(lhp, &global_node_list) {
		nd = list_entry(lhp, struct node_data, global_node_list);
		hwgraph_vertex_destroy(nd->devfs_handle);
	}

}


struct node_data * new_node(void)
{
	struct node_data * node;
	
	TRACE();
	
	node = kmalloc(sizeof(struct node_data), GFP_KERNEL);
	if (node <= 0)
		return node;
	list_add(&node->global_node_list, &global_node_list);
	return node;
}


void dma_cleanup(struct node_data * dma_node)
{
	TRACE();

	/* FIXME: should free these allocations */
#ifdef DEBUG_PCIBA
	dump_allocations(&dma_node->u.dma.dma_allocs);
#endif
	hwgraph_vertex_destroy(dma_node->devfs_handle);
}


void init_dma_node(struct node_data * node,
		   struct pci_dev * dev, vertex_hdl_t dh)
{
	TRACE();

	node->devfs_handle = dh;
	node->u.dma.dev = dev;
	node->cleanup = dma_cleanup;
	INIT_LIST_HEAD(&node->u.dma.dma_allocs);
}


void rom_cleanup(struct node_data * rom_node)
{
	TRACE();

	if (rom_node->u.rom.mmapped)
		pci_write_config_dword(rom_node->u.rom.dev,
				       PCI_ROM_ADDRESS,
				       rom_node->u.rom.saved_rom_base_reg);
	hwgraph_vertex_destroy(rom_node->devfs_handle);
}


void init_rom_node(struct node_data * node,
		   struct pci_dev * dev, vertex_hdl_t dh)
{
	TRACE();

	node->devfs_handle = dh;
	node->u.rom.dev = dev;
	node->cleanup = rom_cleanup;
	node->u.rom.mmapped = false;
}


static status __init
register_pci_device(vertex_hdl_t device_dir_handle, struct pci_dev * dev)
{
	struct node_data * nd;
	char devfs_path[20];
	vertex_hdl_t node_devfs_handle;
	int ri;

	TRACE();


	/* register nodes for all the device's base address registers */
	for (ri = 0; ri < PCI_ROM_RESOURCE; ri++) {
		if (pci_resource_len(dev, ri) != 0) {
			sprintf(devfs_path, "base/%d", ri);
			if (hwgraph_register(device_dir_handle, devfs_path,
					   0, DEVFS_FL_NONE,
					   0, 0,
					   S_IFCHR | S_IRUSR | S_IWUSR, 0, 0,
					   &base_fops, 
					   &dev->resource[ri]) == NULL)
				return failure;
		}
	}
	
	/* register a node corresponding to the first MEM resource on
           the device */
	for (ri = 0; ri < PCI_ROM_RESOURCE; ri++) {
		if (dev->resource[ri].flags & IORESOURCE_MEM &&
		    pci_resource_len(dev, ri) != 0) {
			if (hwgraph_register(device_dir_handle, "mem",
					   0, DEVFS_FL_NONE, 0, 0,
					   S_IFCHR | S_IRUSR | S_IWUSR, 0, 0,
					   &base_fops, 
					   &dev->resource[ri]) == NULL)
				return failure;
			break;
		}
	}

	/* also register a node corresponding to the first IO resource
           on the device */
	for (ri = 0; ri < PCI_ROM_RESOURCE; ri++) {
		if (dev->resource[ri].flags & IORESOURCE_IO &&
		    pci_resource_len(dev, ri) != 0) {
			if (hwgraph_register(device_dir_handle, "io",
					   0, DEVFS_FL_NONE, 0, 0,
					   S_IFCHR | S_IRUSR | S_IWUSR, 0, 0,
					   &base_fops, 
					   &dev->resource[ri]) == NULL)
				return failure;
			break;
		}
	}

	/* register a node corresponding to the device's ROM resource,
           if present */
	if (pci_resource_len(dev, PCI_ROM_RESOURCE) != 0) {
		nd = new_node();
		if (nd <= 0)
			return failure;
		node_devfs_handle = hwgraph_register(device_dir_handle, "rom",
						   0, DEVFS_FL_NONE, 0, 0,
						   S_IFCHR | S_IRUSR, 0, 0,
						   &rom_fops, nd);
		if (node_devfs_handle == NULL)
			return failure;
		init_rom_node(nd, dev, node_devfs_handle);
	}

	/* register a node that allows ioctl's to read and write to
           the device's config space */
	if (hwgraph_register(device_dir_handle, "config", 0, DEVFS_FL_NONE,
			   0, 0, S_IFCHR | S_IRUSR | S_IWUSR, 0, 0,
			   &config_fops, dev) == NULL)
		return failure;


	/* finally, register a node that allows ioctl's to allocate
           and free DMA buffers, as well as memory map those
           buffers. */
	nd = new_node();
	if (nd <= 0)
		return failure;
	node_devfs_handle =
		hwgraph_register(device_dir_handle, "dma", 0, DEVFS_FL_NONE,
			       0, 0, S_IFCHR | S_IRUSR | S_IWUSR, 0, 0,
			       &dma_fops, nd);
	if (node_devfs_handle == NULL)
		return failure;
	init_dma_node(nd, dev, node_devfs_handle);

#ifdef DEBUG_PCIBA
	dump_nodes(&global_node_list);
#endif
	
	return success;
}


static int
generic_open(struct inode * inode, struct file * file)
{
	TRACE();

	/* FIXME: should check that they're not trying to open the ROM
           writable */

	return 0; /* success */
}


static int
rom_mmap(struct file * file, struct vm_area_struct * vma)
{
	unsigned long pci_pa;
	struct node_data * nd;

	TRACE();

#ifdef CONFIG_HWGFS_FS
	nd = (struct node_data * )file->f_dentry->d_fsdata;
#else
	nd = (struct node_data * )file->private_data;
#endif

	pci_pa = pci_resource_start(nd->u.rom.dev, PCI_ROM_RESOURCE);

	if (!nd->u.rom.mmapped) {
		nd->u.rom.mmapped = true;
		DPRINTF("Enabling ROM address decoder.\n");
		DPRINTF(
"rom_mmap: FIXME: some cards do not allow both ROM and memory addresses to\n"
"rom_mmap: FIXME: be enabled simultaneously, as they share a decoder.\n");
		pci_read_config_dword(nd->u.rom.dev, PCI_ROM_ADDRESS,
				      &nd->u.rom.saved_rom_base_reg);
		DPRINTF("ROM base address contains %x\n",
			nd->u.rom.saved_rom_base_reg);
		pci_write_config_dword(nd->u.rom.dev, PCI_ROM_ADDRESS,
				       nd->u.rom.saved_rom_base_reg |
				       PCI_ROM_ADDRESS_ENABLE);
	}
	
	return mmap_pci_address(vma, pci_pa);
}


static int
rom_release(struct inode * inode, struct file * file)
{
	struct node_data * nd;

	TRACE();

#ifdef CONFIG_HWGFS_FS
	nd = (struct node_data * )file->f_dentry->d_fsdata;
#else
	nd = (struct node_data * )file->private_data;
#endif

	if (nd->u.rom.mmapped) {
		nd->u.rom.mmapped = false;
		DPRINTF("Disabling ROM address decoder.\n");
		pci_write_config_dword(nd->u.rom.dev, PCI_ROM_ADDRESS,
				       nd->u.rom.saved_rom_base_reg);
	}
	return 0; /* indicate success */
}


static int
base_mmap(struct file * file, struct vm_area_struct * vma)
{
	struct resource * resource;

	TRACE();

#ifdef CONFIG_HWGFS_FS
	resource = (struct resource *)file->f_dentry->d_fsdata;
#else
	resource = (struct resource *)file->private_data;
#endif

	return mmap_pci_address(vma, resource->start);
}


static int
config_ioctl(struct inode * inode, struct file * file, 
	     unsigned int cmd, 
	     unsigned long arg)
{
	struct pci_dev * dev;

	union cfg_data {
		uint8_t byte;
		uint16_t word;
		uint32_t dword;
	} read_data, write_data;

	int dir, size, offset;

	TRACE();

	DPRINTF("cmd = %x (DIR = %x, TYPE = %x, NR = %x, SIZE = %x)\n", 
		cmd, 
		_IOC_DIR(cmd), _IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd));
	DPRINTF("arg = %lx\n", arg);

#ifdef CONFIG_HWGFS_FS
	dev = (struct pci_dev *)file->f_dentry->d_fsdata;
#else
	dev = (struct pci_dev *)file->private_data;
#endif

	/* PCIIOCCFG{RD,WR}: read and/or write PCI configuration
	   space. If both, the read happens first (this becomes a swap
	   operation, atomic with respect to other updates through
	   this path).  */

	dir = _IOC_DIR(cmd);

#define do_swap(suffix, type)	 					\
	do {								\
		if (dir & _IOC_READ) {					\
			pci_read_config_##suffix(dev, _IOC_NR(cmd), 	\
						 &read_data.suffix);	\
		}							\
		if (dir & _IOC_WRITE) {					\
			get_user(write_data.suffix, (type)arg);		\
			pci_write_config_##suffix(dev, _IOC_NR(cmd), 	\
						  write_data.suffix);	\
		}							\
		if (dir & _IOC_READ) {					\
			put_user(read_data.suffix, (type)arg);		\
		}							\
	} while (0)

	size = _IOC_SIZE(cmd);
	offset = _IOC_NR(cmd);

	DPRINTF("sanity check\n");
	if (((size > 0) || (size <= 4)) &&
	    ((offset + size) <= 256) &&
	    (dir & (_IOC_READ | _IOC_WRITE))) {

		switch (size)
		{
		case 1:
			do_swap(byte, uint8_t *);
			break;
		case 2:
			do_swap(word, uint16_t *);
			break;
		case 4:
			do_swap(dword, uint32_t *);
			break;
		default:
			DPRINTF("invalid ioctl\n");
			return -EINVAL;
		}
	} else
		return -EINVAL;
		
	return 0;
}


#ifdef DEBUG_PCIBA
static void
dump_allocations(struct list_head * dalp)
{
	struct dma_allocation * dap;
	struct list_head * p;
	
	printk("{\n");
	list_for_each(p, dalp) {
		dap = list_entry(p, struct dma_allocation, 
				 list);
		printk("  handle = %lx, va = %p\n",
		       dap->handle, dap->va);
	}
	printk("}\n");
}

static void
dump_nodes(struct list_head * nodes)
{
	struct node_data * ndp;
	struct list_head * p;
	
	printk("{\n");
	list_for_each(p, nodes) {
		ndp = list_entry(p, struct node_data, 
				 global_node_list);
		printk("  %p\n", (void *)ndp);
	}
	printk("}\n");
}


#if 0
#define NEW(ptr) (ptr = kmalloc(sizeof (*(ptr)), GFP_KERNEL))

static void
test_list(void)
{
	u64 i;
	LIST_HEAD(the_list);

	for (i = 0; i < 5; i++) {
		struct dma_allocation * new_alloc;
		NEW(new_alloc);
		new_alloc->va = (void *)i;
		new_alloc->handle = 5*i;
		printk("%d - the_list->next = %lx\n", i, the_list.next);
		list_add(&new_alloc->list, &the_list);
	}
	dump_allocations(&the_list);
}
#endif
#endif


static LIST_HEAD(dma_buffer_list);


static int
dma_ioctl(struct inode * inode, struct file * file, 
	  unsigned int cmd, 
	  unsigned long arg)
{
	struct node_data * nd;
	uint64_t argv;
	int result;
	struct dma_allocation * dma_alloc;
	struct list_head * iterp;

	TRACE();

	DPRINTF("cmd = %x\n", cmd);
	DPRINTF("arg = %lx\n", arg);

#ifdef CONFIG_HWGFS_FS
	nd = (struct node_data *)file->f_dentry->d_fsdata;
#else
	nd = (struct node_data *)file->private_data;
#endif

#ifdef DEBUG_PCIBA
	DPRINTF("at dma_ioctl entry\n");
	dump_allocations(&nd->u.dma.dma_allocs);
#endif

	switch (cmd) {
	case PCIIOCDMAALLOC:
		/* PCIIOCDMAALLOC: allocate a chunk of physical memory
		   and set it up for DMA. Return the PCI address that
		   gets to it.  */
		DPRINTF("case PCIIOCDMAALLOC (%lx)\n", PCIIOCDMAALLOC);
		
		if ( (result = get_user(argv, (uint64_t *)arg)) )
			return result;
		DPRINTF("argv (size of buffer) = %lx\n", argv);

		dma_alloc = (struct dma_allocation *)
			kmalloc(sizeof(struct dma_allocation), GFP_KERNEL);
		if (dma_alloc <= 0)
			return -ENOMEM;

		dma_alloc->size = (size_t)argv;
		dma_alloc->va = pci_alloc_consistent(nd->u.dma.dev,
						     dma_alloc->size,
						     &dma_alloc->handle);
		DPRINTF("dma_alloc->va = %p, dma_alloc->handle = %lx\n",
			dma_alloc->va, dma_alloc->handle);
		if (dma_alloc->va == NULL) {
			kfree(dma_alloc);
			return -ENOMEM;
		}

		list_add(&dma_alloc->list, &nd->u.dma.dma_allocs);
		if ( (result = put_user((uint64_t)dma_alloc->handle, 
				      (uint64_t *)arg)) ) {
			DPRINTF("put_user failed\n");
			pci_free_consistent(nd->u.dma.dev, (size_t)argv,
					    dma_alloc->va, dma_alloc->handle);
			kfree(dma_alloc);
			return result;
		}

#ifdef DEBUG_PCIBA
		DPRINTF("after insertion\n");
		dump_allocations(&nd->u.dma.dma_allocs);
#endif
		break;

	case PCIIOCDMAFREE:
		DPRINTF("case PCIIOCDMAFREE (%lx)\n", PCIIOCDMAFREE);

		if ( (result = get_user(argv, (uint64_t *)arg)) ) {
			DPRINTF("get_user failed\n");
			return result;
		}

		DPRINTF("argv (physical address of DMA buffer) = %lx\n", argv);
		list_for_each(iterp, &nd->u.dma.dma_allocs) {
			struct dma_allocation * da =
				list_entry(iterp, struct dma_allocation, list);
			if (da->handle == argv) {
				pci_free_consistent(nd->u.dma.dev, da->size,
						    da->va, da->handle);
				list_del(&da->list);
				kfree(da);
#ifdef DEBUG_PCIBA
				DPRINTF("after deletion\n");
				dump_allocations(&nd->u.dma.dma_allocs);
#endif
				return 0; /* success */
			}
		}
		/* previously allocated dma buffer wasn't found */
		DPRINTF("attempt to free invalid dma handle\n");
		return -EINVAL;

	default:
		DPRINTF("undefined ioctl\n");
		return -EINVAL;
	}

	DPRINTF("success\n");
	return 0;
}
		

static int
dma_mmap(struct file * file, struct vm_area_struct * vma)
{
	struct node_data * nd;
	struct list_head * iterp;
	int result;
	
	TRACE();

#ifdef CONFIG_HWGFS_FS
	nd = (struct node_data *)file->f_dentry->d_fsdata;
#else
	nd = (struct node_data *)file->private_data;
#endif
	
	DPRINTF("vma->vm_start is %lx\n", vma->vm_start);
	DPRINTF("vma->vm_end is %lx\n", vma->vm_end);
	DPRINTF("offset = %lx\n", vma->vm_pgoff);

	/* get kernel virtual address for the dma buffer (necessary
	 * for the mmap). */
	list_for_each(iterp, &nd->u.dma.dma_allocs) {
		struct dma_allocation * da =
			list_entry(iterp, struct dma_allocation, list);
		/* why does mmap shift its offset argument? */
		if (da->handle == vma->vm_pgoff << PAGE_SHIFT) {
			DPRINTF("found dma handle\n");
			if ( (result = mmap_kernel_address(vma,
							   da->va)) ) {
				return result; /* failure */
			} else {
				/* it seems like at least one of these
				   should show up in user land....
				   I'm missing something */
				*(char *)da->va = 0xaa;
				strncpy(da->va, "        Toastie!", da->size);
				if (put_user(0x18badbeeful,
					     (u64 *)vma->vm_start))
					DPRINTF("put_user failed?!\n");
				return 0; /* success */
			}

		}
	}
	DPRINTF("attempt to mmap an invalid dma handle\n");
	return -EINVAL;
}


static int
mmap_pci_address(struct vm_area_struct * vma, unsigned long pci_va)
{
	unsigned long pci_pa;

	TRACE();

	DPRINTF("vma->vm_start is %lx\n", vma->vm_start);
	DPRINTF("vma->vm_end is %lx\n", vma->vm_end);

	/* the size of the vma doesn't necessarily correspond to the
           size specified in the mmap call.  So we can't really do any
           kind of sanity check here.  This is a dangerous driver, and
           it's very easy for a user process to kill the machine.  */

	DPRINTF("PCI base at virtual address %lx\n", pci_va);
	/* the __pa macro is intended for region 7 on IA64, so it
	   doesn't work for region 6 */
  	/* pci_pa = __pa(pci_va); */
	/* should be replaced by ia64_tpa or equivalent (preferably a
	   generic equivalent) */
	pci_pa = pci_va & ~0xe000000000000000ul;
	DPRINTF("PCI base at physical address %lx\n", pci_pa);

	/* there are various arch-specific versions of this function
           defined in linux/drivers/char/mem.c, but it would be nice
           if all architectures put it in pgtable.h.  it's defined
           there for ia64.... */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	vma->vm_flags |= VM_NONCACHED | VM_RESERVED | VM_IO;

	return io_remap_page_range(vma->vm_start, pci_pa, 
				   vma->vm_end-vma->vm_start,
				   vma->vm_page_prot);
}


static int
mmap_kernel_address(struct vm_area_struct * vma, void * kernel_va)
{
	unsigned long kernel_pa;

	TRACE();

	DPRINTF("vma->vm_start is %lx\n", vma->vm_start);
	DPRINTF("vma->vm_end is %lx\n", vma->vm_end);

	/* the size of the vma doesn't necessarily correspond to the
           size specified in the mmap call.  So we can't really do any
           kind of sanity check here.  This is a dangerous driver, and
           it's very easy for a user process to kill the machine.  */

	DPRINTF("mapping virtual address %p\n", kernel_va);
	kernel_pa = __pa(kernel_va);
	DPRINTF("mapping physical address %lx\n", kernel_pa);

	vma->vm_flags |= VM_NONCACHED | VM_RESERVED | VM_IO;

	return remap_page_range(vma->vm_start, kernel_pa, 
				vma->vm_end-vma->vm_start,
				vma->vm_page_prot);
}	
