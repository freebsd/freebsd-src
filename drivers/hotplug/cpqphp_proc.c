/*
 * Compaq Hot Plug Controller Driver
 *
 * Copyright (C) 1995,2001 Compaq Computer Corporation
 * Copyright (C) 2001 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2001 IBM Corp.
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send feedback to <greg@kroah.com>
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include "cpqphp.h"



static struct proc_dir_entry *ctrl_proc_root;

/* A few routines that create proc entries for the hot plug controller */

static int read_ctrl (char *buf, char **start, off_t offset, int len, int *eof, void *data)
{
	struct controller *ctrl = (struct controller *)data;
	char * out = buf;
	int index;
	struct pci_resource *res;

	if (offset > 0)	return 0; /* no partial requests */
	len  = 0;
	*eof = 1;

	out += sprintf(out, "hot plug ctrl Info Page\n");
	out += sprintf(out, "bus = %d, device = %d, function = %d\n",ctrl->bus,
		       ctrl->device, ctrl->function);
	out += sprintf(out, "Free resources: memory\n");
	index = 11;
	res = ctrl->mem_head;
	while (res && index--) {
		out += sprintf(out, "start = %8.8x, length = %8.8x\n", res->base, res->length);
		res = res->next;
	}
	out += sprintf(out, "Free resources: prefetchable memory\n");
	index = 11;
	res = ctrl->p_mem_head;
	while (res && index--) {
		out += sprintf(out, "start = %8.8x, length = %8.8x\n", res->base, res->length);
		res = res->next;
	}
	out += sprintf(out, "Free resources: IO\n");
	index = 11;
	res = ctrl->io_head;
	while (res && index--) {
		out += sprintf(out, "start = %8.8x, length = %8.8x\n", res->base, res->length);
		res = res->next;
	}
	out += sprintf(out, "Free resources: bus numbers\n");
	index = 11;
	res = ctrl->bus_head;
	while (res && index--) {
		out += sprintf(out, "start = %8.8x, length = %8.8x\n", res->base, res->length);
		res = res->next;
	}

	*start = buf;
	len = out-buf;

	return len;
}

static int read_dev (char *buf, char **start, off_t offset, int len, int *eof, void *data)
{
	struct controller *ctrl = (struct controller *)data;
	char * out = buf;
	int index;
	struct pci_resource *res;
	struct pci_func *new_slot;
	struct slot *slot;

	if (offset > 0)	return 0; /* no partial requests */
	len  = 0;
	*eof = 1;

	out += sprintf(out, "hot plug ctrl Info Page\n");
	out += sprintf(out, "bus = %d, device = %d, function = %d\n",ctrl->bus,
		       ctrl->device, ctrl->function);

	slot=ctrl->slot;

	while (slot) {
		new_slot = cpqhp_slot_find(slot->bus, slot->device, 0);
		out += sprintf(out, "assigned resources: memory\n");
		index = 11;
		res = new_slot->mem_head;
		while (res && index--) {
			out += sprintf(out, "start = %8.8x, length = %8.8x\n", res->base, res->length);
			res = res->next;
		}
		out += sprintf(out, "assigned resources: prefetchable memory\n");
		index = 11;
		res = new_slot->p_mem_head;
		while (res && index--) {
			out += sprintf(out, "start = %8.8x, length = %8.8x\n", res->base, res->length);
			res = res->next;
		}
		out += sprintf(out, "assigned resources: IO\n");
		index = 11;
		res = new_slot->io_head;
		while (res && index--) {
			out += sprintf(out, "start = %8.8x, length = %8.8x\n", res->base, res->length);
			res = res->next;
		}
		out += sprintf(out, "assigned resources: bus numbers\n");
		index = 11;
		res = new_slot->bus_head;
		while (res && index--) {
			out += sprintf(out, "start = %8.8x, length = %8.8x\n", res->base, res->length);
			res = res->next;
		}
		slot=slot->next;
	}

	*start = buf;
	len = out-buf;

	return len;
}

int cpqhp_proc_create_ctrl (struct controller *ctrl)
{
	strcpy(ctrl->proc_name, "hpca");
	ctrl->proc_name[3] = 'a' + ctrl->bus;

	ctrl->proc_entry = create_proc_entry(ctrl->proc_name, S_IFREG | S_IRUGO, ctrl_proc_root);
	ctrl->proc_entry->data = ctrl;
	ctrl->proc_entry->read_proc = &read_ctrl;

	strcpy(ctrl->proc_name2, "slot_a");
	ctrl->proc_name2[5] = 'a' + ctrl->bus;
	ctrl->proc_entry2 = create_proc_entry(ctrl->proc_name2, S_IFREG | S_IRUGO, ctrl_proc_root);
	ctrl->proc_entry2->data = ctrl;
	ctrl->proc_entry2->read_proc = &read_dev;

	return 0;
}

int cpqhp_proc_remove_ctrl (struct controller *ctrl)
{
	if (ctrl->proc_entry)
		remove_proc_entry(ctrl->proc_name, ctrl_proc_root);
	if (ctrl->proc_entry2)
		remove_proc_entry(ctrl->proc_name2, ctrl_proc_root);

	return 0;
}
	
int cpqhp_proc_init_ctrl (void)
{
	ctrl_proc_root = proc_mkdir("hpc", proc_root_driver);
	if (!ctrl_proc_root)
		return -ENOMEM;
	ctrl_proc_root->owner = THIS_MODULE;
	return 0;
}

int cpqhp_proc_destroy_ctrl (void)
{
	remove_proc_entry("hpc", proc_root_driver);
	return 0;
}

