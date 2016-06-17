/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

/*
 * Hardware Inventory
 *
 * See sys/sn/invent.h for an explanation of the hardware inventory contents.
 *
 */
#include <linux/types.h>
#include <asm/sn/sgi.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>

void
inventinit(void)
{
}

/*
 * For initializing/updating an inventory entry.
 */
void
replace_in_inventory(
	inventory_t *pinv, int class, int type,
	int controller, int unit, int state)
{
}

/*
 * Inventory addition 
 *
 * XXX NOTE: Currently must be called after dynamic memory allocator is
 * initialized.
 *
 */
void
add_to_inventory(int class, int type, int controller, int unit, int state)
{
}


/*
 * Inventory retrieval 
 *
 * These two routines are intended to prevent the caller from having to know
 * the internal structure of the inventory table.
 *
 * The caller of get_next_inventory is supposed to call start_scan_invent
 * before the irst call to get_next_inventory, and the caller is required
 * to call end_scan_invent after the last call to get_next_inventory.
 */
inventory_t *
get_next_inventory(invplace_t *place)
{
	return((inventory_t *) NULL);
}

/* ARGSUSED */
int
get_sizeof_inventory(int abi)
{
	return sizeof(inventory_t);
}

/* Must be called prior to first call to get_next_inventory */
void
start_scan_inventory(invplace_t *iplace)
{
}

/* Must be called after last call to get_next_inventory */
void
end_scan_inventory(invplace_t *iplace)
{
}

/*
 * Hardware inventory scanner.
 *
 * Calls fun() for every entry in inventory list unless fun() returns something
 * other than 0.
 */
int
scaninvent(int (*fun)(inventory_t *, void *), void *arg)
{
	return 0;
}

/*
 * Find a particular inventory object
 *
 * pinv can be a pointer to an inventory entry and the search will begin from
 * there, or it can be 0 in which case the search starts at the beginning.
 * A -1 for any of the other arguments is a wildcard (i.e. it always matches).
 */
inventory_t *
find_inventory(inventory_t *pinv, int class, int type, int controller,
	       int unit, int state)
{
	return((inventory_t *) NULL);
}


/*
** Retrieve inventory data associated with a device.
*/
inventory_t *
device_inventory_get_next(	vertex_hdl_t device,
				invplace_t *invplace)
{
		return((inventory_t *) NULL);
}


/*
** Associate canonical inventory information with a device (and
** add it to the general inventory).
*/
void
device_inventory_add(	vertex_hdl_t device,
			int class, 
			int type, 
			major_t controller, 
			minor_t unit, 
			int state)
{
}

int
device_controller_num_get(vertex_hdl_t device)
{
	return (0);
}

void
device_controller_num_set(vertex_hdl_t device, int contr_num)
{
}
