/*
 * AGPGART module frontend version 0.99
 * Copyright (C) 1999 Jeff Hartmann
 * Copyright (C) 1999 Precision Insight, Inc.
 * Copyright (C) 1999 Xi Graphics, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JEFF HARTMANN, OR ANY OTHER CONTRIBUTORS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#define __NO_VERSION__
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/miscdevice.h>
#include <linux/agp_backend.h>
#include <linux/agpgart.h>
#include <linux/smp_lock.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/mman.h>

#include "agp.h"

static struct agp_front_data agp_fe;

static agp_memory *agp_find_mem_by_key(int key)
{
	agp_memory *curr;

	if (agp_fe.current_controller == NULL) {
		return NULL;
	}
	curr = agp_fe.current_controller->pool;

	while (curr != NULL) {
		if (curr->key == key) {
			return curr;
		}
		curr = curr->next;
	}

	return NULL;
}

static void agp_remove_from_pool(agp_memory * temp)
{
	agp_memory *prev;
	agp_memory *next;

	/* Check to see if this is even in the memory pool */

	if (agp_find_mem_by_key(temp->key) != NULL) {
		next = temp->next;
		prev = temp->prev;

		if (prev != NULL) {
			prev->next = next;
			if (next != NULL) {
				next->prev = prev;
			}
		} else {
			/* This is the first item on the list */
			if (next != NULL) {
				next->prev = NULL;
			}
			agp_fe.current_controller->pool = next;
		}
	}
}

/*
 * Routines for managing each client's segment list -
 * These routines handle adding and removing segments
 * to each auth'ed client.
 */

static agp_segment_priv *agp_find_seg_in_client(const agp_client * client,
						unsigned long offset,
					    int size, pgprot_t page_prot)
{
	agp_segment_priv *seg;
	int num_segments, pg_start, pg_count, i;

	pg_start = offset / 4096;
	pg_count = size / 4096;
	seg = *(client->segments);
	num_segments = client->num_segments;

	for (i = 0; i < client->num_segments; i++) {
		if ((seg[i].pg_start == pg_start) &&
		    (seg[i].pg_count == pg_count) &&
		    (pgprot_val(seg[i].prot) == pgprot_val(page_prot))) {
			return seg + i;
		}
	}

	return NULL;
}

static void agp_remove_seg_from_client(agp_client * client)
{
	if (client->segments != NULL) {
		if (*(client->segments) != NULL) {
			kfree(*(client->segments));
		}
		kfree(client->segments);
	}
}

static void agp_add_seg_to_client(agp_client * client,
			       agp_segment_priv ** seg, int num_segments)
{
	agp_segment_priv **prev_seg;

	prev_seg = client->segments;

	if (prev_seg != NULL) {
		agp_remove_seg_from_client(client);
	}
	client->num_segments = num_segments;
	client->segments = seg;
}

/* Originally taken from linux/mm/mmap.c from the array
 * protection_map.
 * The original really should be exported to modules, or 
 * some routine which does the conversion for you 
 */

static const pgprot_t my_protect_map[16] =
{
	__P000, __P001, __P010, __P011, __P100, __P101, __P110, __P111,
	__S000, __S001, __S010, __S011, __S100, __S101, __S110, __S111
};

static pgprot_t agp_convert_mmap_flags(int prot)
{
#define _trans(x,bit1,bit2) \
((bit1==bit2)?(x&bit1):(x&bit1)?bit2:0)

	unsigned long prot_bits;
	pgprot_t temp;

	prot_bits = _trans(prot, PROT_READ, VM_READ) |
	    _trans(prot, PROT_WRITE, VM_WRITE) |
	    _trans(prot, PROT_EXEC, VM_EXEC);

	prot_bits |= VM_SHARED;

	temp = my_protect_map[prot_bits & 0x0000000f];

	return temp;
}

static int agp_create_segment(agp_client * client, agp_region * region)
{
	agp_segment_priv **ret_seg;
	agp_segment_priv *seg;
	agp_segment *user_seg;
	int i;

	seg = kmalloc((sizeof(agp_segment_priv) * region->seg_count),
		      GFP_KERNEL);
	if (seg == NULL) {
		kfree(region->seg_list);
		return -ENOMEM;
	}
	memset(seg, 0, (sizeof(agp_segment_priv) * region->seg_count));
	user_seg = region->seg_list;

	for (i = 0; i < region->seg_count; i++) {
		seg[i].pg_start = user_seg[i].pg_start;
		seg[i].pg_count = user_seg[i].pg_count;
		seg[i].prot = agp_convert_mmap_flags(user_seg[i].prot);
	}
	ret_seg = kmalloc(sizeof(void *), GFP_KERNEL);
	if (ret_seg == NULL) {
		kfree(region->seg_list);
		kfree(seg);
		return -ENOMEM;
	}
	*ret_seg = seg;
	kfree(region->seg_list);
	agp_add_seg_to_client(client, ret_seg, region->seg_count);
	return 0;
}

/* End - Routines for managing each client's segment list */

/* This function must only be called when current_controller != NULL */
static void agp_insert_into_pool(agp_memory * temp)
{
	agp_memory *prev;

	prev = agp_fe.current_controller->pool;

	if (prev != NULL) {
		prev->prev = temp;
		temp->next = prev;
	}
	agp_fe.current_controller->pool = temp;
}


/* File private list routines */

agp_file_private *agp_find_private(pid_t pid)
{
	agp_file_private *curr;

	curr = agp_fe.file_priv_list;

	while (curr != NULL) {
		if (curr->my_pid == pid) {
			return curr;
		}
		curr = curr->next;
	}

	return NULL;
}

void agp_insert_file_private(agp_file_private * priv)
{
	agp_file_private *prev;

	prev = agp_fe.file_priv_list;

	if (prev != NULL) {
		prev->prev = priv;
	}
	priv->next = prev;
	agp_fe.file_priv_list = priv;
}

void agp_remove_file_private(agp_file_private * priv)
{
	agp_file_private *next;
	agp_file_private *prev;

	next = priv->next;
	prev = priv->prev;

	if (prev != NULL) {
		prev->next = next;

		if (next != NULL) {
			next->prev = prev;
		}
	} else {
		if (next != NULL) {
			next->prev = NULL;
		}
		agp_fe.file_priv_list = next;
	}
}

/* End - File flag list routines */

/* 
 * Wrappers for agp_free_memory & agp_allocate_memory 
 * These make sure that internal lists are kept updated.
 */
static void agp_free_memory_wrap(agp_memory * memory)
{
	agp_remove_from_pool(memory);
	agp_free_memory(memory);
}

static agp_memory *agp_allocate_memory_wrap(size_t pg_count, u32 type)
{
	agp_memory *memory;

	memory = agp_allocate_memory(pg_count, type);
//   	printk(KERN_DEBUG "memory : %p\n", memory);
	if (memory == NULL) {
		return NULL;
	}
	agp_insert_into_pool(memory);
	return memory;
}

/* Routines for managing the list of controllers -
 * These routines manage the current controller, and the list of
 * controllers
 */

static agp_controller *agp_find_controller_by_pid(pid_t id)
{
	agp_controller *controller;

	controller = agp_fe.controllers;

	while (controller != NULL) {
		if (controller->pid == id) {
			return controller;
		}
		controller = controller->next;
	}

	return NULL;
}

static agp_controller *agp_create_controller(pid_t id)
{
	agp_controller *controller;

	controller = kmalloc(sizeof(agp_controller), GFP_KERNEL);

	if (controller == NULL) {
		return NULL;
	}
	memset(controller, 0, sizeof(agp_controller));
	controller->pid = id;

	return controller;
}

static int agp_insert_controller(agp_controller * controller)
{
	agp_controller *prev_controller;

	prev_controller = agp_fe.controllers;
	controller->next = prev_controller;

	if (prev_controller != NULL) {
		prev_controller->prev = controller;
	}
	agp_fe.controllers = controller;

	return 0;
}

static void agp_remove_all_clients(agp_controller * controller)
{
	agp_client *client;
	agp_client *temp;

	client = controller->clients;

	while (client) {
		agp_file_private *priv;

		temp = client;
		agp_remove_seg_from_client(temp);
		priv = agp_find_private(temp->pid);

		if (priv != NULL) {
			clear_bit(AGP_FF_IS_VALID, &priv->access_flags);
			clear_bit(AGP_FF_IS_CLIENT, &priv->access_flags);
		}
		client = client->next;
		kfree(temp);
	}
}

static void agp_remove_all_memory(agp_controller * controller)
{
	agp_memory *memory;
	agp_memory *temp;

	memory = controller->pool;

	while (memory) {
		temp = memory;
		memory = memory->next;
		agp_free_memory_wrap(temp);
	}
}

static int agp_remove_controller(agp_controller * controller)
{
	agp_controller *prev_controller;
	agp_controller *next_controller;

	prev_controller = controller->prev;
	next_controller = controller->next;

	if (prev_controller != NULL) {
		prev_controller->next = next_controller;
		if (next_controller != NULL) {
			next_controller->prev = prev_controller;
		}
	} else {
		if (next_controller != NULL) {
			next_controller->prev = NULL;
		}
		agp_fe.controllers = next_controller;
	}

	agp_remove_all_memory(controller);
	agp_remove_all_clients(controller);

	if (agp_fe.current_controller == controller) {
		agp_fe.current_controller = NULL;
		agp_fe.backend_acquired = FALSE;
		agp_backend_release();
	}
	kfree(controller);
	return 0;
}

static void agp_controller_make_current(agp_controller * controller)
{
	agp_client *clients;

	clients = controller->clients;

	while (clients != NULL) {
		agp_file_private *priv;

		priv = agp_find_private(clients->pid);

		if (priv != NULL) {
			set_bit(AGP_FF_IS_VALID, &priv->access_flags);
			set_bit(AGP_FF_IS_CLIENT, &priv->access_flags);
		}
		clients = clients->next;
	}

	agp_fe.current_controller = controller;
}

static void agp_controller_release_current(agp_controller * controller,
				      agp_file_private * controller_priv)
{
	agp_client *clients;

	clear_bit(AGP_FF_IS_VALID, &controller_priv->access_flags);
	clients = controller->clients;

	while (clients != NULL) {
		agp_file_private *priv;

		priv = agp_find_private(clients->pid);

		if (priv != NULL) {
			clear_bit(AGP_FF_IS_VALID, &priv->access_flags);
		}
		clients = clients->next;
	}

	agp_fe.current_controller = NULL;
	agp_fe.used_by_controller = FALSE;
	agp_backend_release();
}

/* 
 * Routines for managing client lists -
 * These routines are for managing the list of auth'ed clients.
 */

static agp_client *agp_find_client_in_controller(agp_controller * controller,
						 pid_t id)
{
	agp_client *client;

	if (controller == NULL) {
		return NULL;
	}
	client = controller->clients;

	while (client != NULL) {
		if (client->pid == id) {
			return client;
		}
		client = client->next;
	}

	return NULL;
}

static agp_controller *agp_find_controller_for_client(pid_t id)
{
	agp_controller *controller;

	controller = agp_fe.controllers;

	while (controller != NULL) {
		if ((agp_find_client_in_controller(controller, id)) != NULL) {
			return controller;
		}
		controller = controller->next;
	}

	return NULL;
}

static agp_client *agp_find_client_by_pid(pid_t id)
{
	agp_client *temp;

	if (agp_fe.current_controller == NULL) {
		return NULL;
	}
	temp = agp_find_client_in_controller(agp_fe.current_controller, id);
	return temp;
}

static void agp_insert_client(agp_client * client)
{
	agp_client *prev_client;

	prev_client = agp_fe.current_controller->clients;
	client->next = prev_client;

	if (prev_client != NULL) {
		prev_client->prev = client;
	}
	agp_fe.current_controller->clients = client;
	agp_fe.current_controller->num_clients++;
}

static agp_client *agp_create_client(pid_t id)
{
	agp_client *new_client;

	new_client = kmalloc(sizeof(agp_client), GFP_KERNEL);

	if (new_client == NULL) {
		return NULL;
	}
	memset(new_client, 0, sizeof(agp_client));
	new_client->pid = id;
	agp_insert_client(new_client);
	return new_client;
}

static int agp_remove_client(pid_t id)
{
	agp_client *client;
	agp_client *prev_client;
	agp_client *next_client;
	agp_controller *controller;

	controller = agp_find_controller_for_client(id);

	if (controller == NULL) {
		return -EINVAL;
	}
	client = agp_find_client_in_controller(controller, id);

	if (client == NULL) {
		return -EINVAL;
	}
	prev_client = client->prev;
	next_client = client->next;

	if (prev_client != NULL) {
		prev_client->next = next_client;
		if (next_client != NULL) {
			next_client->prev = prev_client;
		}
	} else {
		if (next_client != NULL) {
			next_client->prev = NULL;
		}
		controller->clients = next_client;
	}

	controller->num_clients--;
	agp_remove_seg_from_client(client);
	kfree(client);
	return 0;
}

/* End - Routines for managing client lists */

/* File Operations */

static int agp_mmap(struct file *file, struct vm_area_struct *vma)
{
	int size;
	int current_size;
	unsigned long offset;
	agp_client *client;
	agp_file_private *priv = (agp_file_private *) file->private_data;
	agp_kern_info kerninfo;

	lock_kernel();
	AGP_LOCK();

	if (agp_fe.backend_acquired != TRUE) {
		AGP_UNLOCK();
		unlock_kernel();
		return -EPERM;
	}
	if (!(test_bit(AGP_FF_IS_VALID, &priv->access_flags))) {
		AGP_UNLOCK();
		unlock_kernel();
		return -EPERM;
	}
	agp_copy_info(&kerninfo);
	size = vma->vm_end - vma->vm_start;
	current_size = kerninfo.aper_size;
	current_size = current_size * 0x100000;
	offset = vma->vm_pgoff << PAGE_SHIFT;

	if (test_bit(AGP_FF_IS_CLIENT, &priv->access_flags)) {
		if ((size + offset) > current_size) {
			AGP_UNLOCK();
			unlock_kernel();
			return -EINVAL;
		}
		client = agp_find_client_by_pid(current->pid);

		if (client == NULL) {
			AGP_UNLOCK();
			unlock_kernel();
			return -EPERM;
		}
		if (!agp_find_seg_in_client(client, offset,
					    size, vma->vm_page_prot)) {
			AGP_UNLOCK();
			unlock_kernel();
			return -EINVAL;
		}
		if (remap_page_range(vma->vm_start,
				     (kerninfo.aper_base + offset),
				     size, vma->vm_page_prot)) {
			AGP_UNLOCK();
			unlock_kernel();
			return -EAGAIN;
		}
		AGP_UNLOCK();
		unlock_kernel();
		return 0;
	}
	if (test_bit(AGP_FF_IS_CONTROLLER, &priv->access_flags)) {
		if (size != current_size) {
			AGP_UNLOCK();
			unlock_kernel();
			return -EINVAL;
		}
		if (remap_page_range(vma->vm_start, kerninfo.aper_base,
				     size, vma->vm_page_prot)) {
			AGP_UNLOCK();
			unlock_kernel();
			return -EAGAIN;
		}
		AGP_UNLOCK();
		unlock_kernel();
		return 0;
	}
	AGP_UNLOCK();
	unlock_kernel();
	return -EPERM;
}

static int agp_release(struct inode *inode, struct file *file)
{
	agp_file_private *priv = (agp_file_private *) file->private_data;

	lock_kernel();
	AGP_LOCK();

	if (test_bit(AGP_FF_IS_CONTROLLER, &priv->access_flags)) {
		agp_controller *controller;

		controller = agp_find_controller_by_pid(priv->my_pid);

		if (controller != NULL) {
			if (controller == agp_fe.current_controller) {
				agp_controller_release_current(controller,
							       priv);
			}
			agp_remove_controller(controller);
		}
	}
	if (test_bit(AGP_FF_IS_CLIENT, &priv->access_flags)) {
		agp_remove_client(priv->my_pid);
	}
	agp_remove_file_private(priv);
	kfree(priv);
	AGP_UNLOCK();
	unlock_kernel();
	return 0;
}

static int agp_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	agp_file_private *priv;
	agp_client *client;
	int rc = -ENXIO;

	AGP_LOCK();

	if (minor != AGPGART_MINOR)
		goto err_out;

	priv = kmalloc(sizeof(agp_file_private), GFP_KERNEL);
	if (priv == NULL)
		goto err_out_nomem;

	memset(priv, 0, sizeof(agp_file_private));
	set_bit(AGP_FF_ALLOW_CLIENT, &priv->access_flags);
	priv->my_pid = current->pid;

	if ((current->uid == 0) || (current->suid == 0)) {
		/* Root priv, can be controller */
		set_bit(AGP_FF_ALLOW_CONTROLLER, &priv->access_flags);
	}
	client = agp_find_client_by_pid(current->pid);

	if (client != NULL) {
		set_bit(AGP_FF_IS_CLIENT, &priv->access_flags);
		set_bit(AGP_FF_IS_VALID, &priv->access_flags);
	}
	file->private_data = (void *) priv;
	agp_insert_file_private(priv);
	AGP_UNLOCK();
	return 0;

err_out_nomem:
	rc = -ENOMEM;
err_out:
	AGP_UNLOCK();
	return rc;
}


static ssize_t agp_read(struct file *file, char *buf,
			size_t count, loff_t * ppos)
{
	return -EINVAL;
}

static ssize_t agp_write(struct file *file, const char *buf,
			 size_t count, loff_t * ppos)
{
	return -EINVAL;
}

static int agpioc_info_wrap(agp_file_private * priv, unsigned long arg)
{
	agp_info userinfo;
	agp_kern_info kerninfo;

	agp_copy_info(&kerninfo);

	userinfo.version.major = kerninfo.version.major;
	userinfo.version.minor = kerninfo.version.minor;
	userinfo.bridge_id = kerninfo.device->vendor |
	    (kerninfo.device->device << 16);
	userinfo.agp_mode = kerninfo.mode;
	userinfo.aper_base = kerninfo.aper_base;
	userinfo.aper_size = kerninfo.aper_size;
	userinfo.pg_total = userinfo.pg_system = kerninfo.max_memory;
	userinfo.pg_used = kerninfo.current_memory;

	if (copy_to_user((void *) arg, &userinfo, sizeof(agp_info))) {
		return -EFAULT;
	}
	return 0;
}

static int agpioc_acquire_wrap(agp_file_private * priv, unsigned long arg)
{
	agp_controller *controller;
	if (!(test_bit(AGP_FF_ALLOW_CONTROLLER, &priv->access_flags))) {
		return -EPERM;
	}
	if (agp_fe.current_controller != NULL) {
		return -EBUSY;
	}
	if ((agp_backend_acquire()) == 0) {
		agp_fe.backend_acquired = TRUE;
	} else {
		return -EBUSY;
	}

	controller = agp_find_controller_by_pid(priv->my_pid);

	if (controller != NULL) {
		agp_controller_make_current(controller);
	} else {
		controller = agp_create_controller(priv->my_pid);

		if (controller == NULL) {
			agp_fe.backend_acquired = FALSE;
			agp_backend_release();
			return -ENOMEM;
		}
		agp_insert_controller(controller);
		agp_controller_make_current(controller);
	}

	set_bit(AGP_FF_IS_CONTROLLER, &priv->access_flags);
	set_bit(AGP_FF_IS_VALID, &priv->access_flags);
	return 0;
}

static int agpioc_release_wrap(agp_file_private * priv, unsigned long arg)
{
	agp_controller_release_current(agp_fe.current_controller, priv);
	return 0;
}

static int agpioc_setup_wrap(agp_file_private * priv, unsigned long arg)
{
	agp_setup mode;

	if (copy_from_user(&mode, (void *) arg, sizeof(agp_setup))) {
		return -EFAULT;
	}
	agp_enable(mode.agp_mode);
	return 0;
}

static int agpioc_reserve_wrap(agp_file_private * priv, unsigned long arg)
{
	agp_region reserve;
	agp_client *client;
	agp_file_private *client_priv;


	if (copy_from_user(&reserve, (void *) arg, sizeof(agp_region))) {
		return -EFAULT;
	}
	if ((unsigned) reserve.seg_count >= ~0U/sizeof(agp_segment))
		return -EFAULT;

	client = agp_find_client_by_pid(reserve.pid);

	if (reserve.seg_count == 0) {
		/* remove a client */
		client_priv = agp_find_private(reserve.pid);

		if (client_priv != NULL) {
			set_bit(AGP_FF_IS_CLIENT,
				&client_priv->access_flags);
			set_bit(AGP_FF_IS_VALID,
				&client_priv->access_flags);
		}
		if (client == NULL) {
			/* client is already removed */
			return 0;
		}
		return agp_remove_client(reserve.pid);
	} else {
		agp_segment *segment;

		if (reserve.seg_count >= 16384)
			return -EINVAL;
			
		segment = kmalloc((sizeof(agp_segment) * reserve.seg_count),
				  GFP_KERNEL);

		if (segment == NULL) {
			return -ENOMEM;
		}
		if (copy_from_user(segment, (void *) reserve.seg_list,
				   sizeof(agp_segment) * reserve.seg_count)) {
			kfree(segment);
			return -EFAULT;
		}
		reserve.seg_list = segment;

		if (client == NULL) {
			/* Create the client and add the segment */
			client = agp_create_client(reserve.pid);

			if (client == NULL) {
				kfree(segment);
				return -ENOMEM;
			}
			client_priv = agp_find_private(reserve.pid);

			if (client_priv != NULL) {
				set_bit(AGP_FF_IS_CLIENT,
					&client_priv->access_flags);
				set_bit(AGP_FF_IS_VALID,
					&client_priv->access_flags);
			}
			return agp_create_segment(client, &reserve);
		} else {
			return agp_create_segment(client, &reserve);
		}
	}
	/* Will never really happen */
	return -EINVAL;
}

static int agpioc_protect_wrap(agp_file_private * priv, unsigned long arg)
{
	/* This function is not currently implemented */
	return -EINVAL;
}

static int agpioc_allocate_wrap(agp_file_private * priv, unsigned long arg)
{
	agp_memory *memory;
	agp_allocate alloc;

	if (copy_from_user(&alloc, (void *) arg, sizeof(agp_allocate))) {
		return -EFAULT;
	}
	memory = agp_allocate_memory_wrap(alloc.pg_count, alloc.type);

	if (memory == NULL) {
		return -ENOMEM;
	}
	alloc.key = memory->key;
	alloc.physical = memory->physical;

	if (copy_to_user((void *) arg, &alloc, sizeof(agp_allocate))) {
		agp_free_memory_wrap(memory);
		return -EFAULT;
	}
	return 0;
}

static int agpioc_deallocate_wrap(agp_file_private * priv, unsigned long arg)
{
	agp_memory *memory;

	memory = agp_find_mem_by_key((int) arg);

	if (memory == NULL) {
		return -EINVAL;
	}
	agp_free_memory_wrap(memory);
	return 0;
}

static int agpioc_bind_wrap(agp_file_private * priv, unsigned long arg)
{
	agp_bind bind_info;
	agp_memory *memory;

	if (copy_from_user(&bind_info, (void *) arg, sizeof(agp_bind))) {
		return -EFAULT;
	}
	memory = agp_find_mem_by_key(bind_info.key);

	if (memory == NULL) {
		return -EINVAL;
	}
	return agp_bind_memory(memory, bind_info.pg_start);
}

static int agpioc_unbind_wrap(agp_file_private * priv, unsigned long arg)
{
	agp_memory *memory;
	agp_unbind unbind;

	if (copy_from_user(&unbind, (void *) arg, sizeof(agp_unbind))) {
		return -EFAULT;
	}
	memory = agp_find_mem_by_key(unbind.key);

	if (memory == NULL) {
		return -EINVAL;
	}
	return agp_unbind_memory(memory);
}

static int agp_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	agp_file_private *curr_priv = (agp_file_private *) file->private_data;
	int ret_val = -ENOTTY;

	AGP_LOCK();

	if ((agp_fe.current_controller == NULL) &&
	    (cmd != AGPIOC_ACQUIRE)) {
		ret_val = -EINVAL;
	   	goto ioctl_out;
	}
	if ((agp_fe.backend_acquired != TRUE) &&
	    (cmd != AGPIOC_ACQUIRE)) {
		ret_val = -EBUSY;
	   	goto ioctl_out;
	}
	if (cmd != AGPIOC_ACQUIRE) {
		if (!(test_bit(AGP_FF_IS_CONTROLLER,
			       &curr_priv->access_flags))) {
			ret_val = -EPERM;
		   	goto ioctl_out;
		}
		/* Use the original pid of the controller,
		 * in case it's threaded */

		if (agp_fe.current_controller->pid != curr_priv->my_pid) {
			ret_val = -EBUSY;
		   	goto ioctl_out;
		}
	}
	switch (cmd) {
	case AGPIOC_INFO:
		{
			ret_val = agpioc_info_wrap(curr_priv, arg);
		   	goto ioctl_out;
		}
	case AGPIOC_ACQUIRE:
		{
			ret_val = agpioc_acquire_wrap(curr_priv, arg);
		   	goto ioctl_out;
		}
	case AGPIOC_RELEASE:
		{
			ret_val = agpioc_release_wrap(curr_priv, arg);
		   	goto ioctl_out;
		}
	case AGPIOC_SETUP:
		{
			ret_val = agpioc_setup_wrap(curr_priv, arg);
		   	goto ioctl_out;
		}
	case AGPIOC_RESERVE:
		{
			ret_val = agpioc_reserve_wrap(curr_priv, arg);
		   	goto ioctl_out;
		}
	case AGPIOC_PROTECT:
		{
			ret_val = agpioc_protect_wrap(curr_priv, arg);
		   	goto ioctl_out;
		}
	case AGPIOC_ALLOCATE:
		{
			ret_val = agpioc_allocate_wrap(curr_priv, arg);
		   	goto ioctl_out;
		}
	case AGPIOC_DEALLOCATE:
		{
			ret_val = agpioc_deallocate_wrap(curr_priv, arg);
		   	goto ioctl_out;
		}
	case AGPIOC_BIND:
		{
			ret_val = agpioc_bind_wrap(curr_priv, arg);
		   	goto ioctl_out;
		}
	case AGPIOC_UNBIND:
		{
			ret_val = agpioc_unbind_wrap(curr_priv, arg);
		   	goto ioctl_out;
		}
	}
   
ioctl_out:
	AGP_UNLOCK();
	return ret_val;
}

static struct file_operations agp_fops =
{
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		agp_read,
	write:		agp_write,
	ioctl:		agp_ioctl,
	mmap:		agp_mmap,
	open:		agp_open,
	release:	agp_release,
};

static struct miscdevice agp_miscdev =
{
	AGPGART_MINOR,
	AGPGART_MODULE_NAME,
	&agp_fops
};

int __init agp_frontend_initialize(void)
{
	memset(&agp_fe, 0, sizeof(struct agp_front_data));
	AGP_LOCK_INIT();

	if (misc_register(&agp_miscdev)) {
		printk(KERN_ERR PFX "unable to get minor: %d\n", AGPGART_MINOR);
		return -EIO;
	}
	return 0;
}

void __exit agp_frontend_cleanup(void)
{
	misc_deregister(&agp_miscdev);
}

