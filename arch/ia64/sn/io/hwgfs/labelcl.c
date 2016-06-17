/*  labelcl - SGI's Hwgraph Compatibility Layer.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2001-2003 Silicon Graphics, Inc.  All rights reserved.
*/

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/sched.h>                /* needed for smp_lock.h :( */
#include <linux/smp_lock.h>
#include <asm/sn/sgi.h>
#include <asm/sn/hwgfs.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>

/*
** Very simple and dumb string table that supports only find/insert.
** In practice, if this table gets too large, we may need a more
** efficient data structure.   Also note that currently there is no 
** way to delete an item once it's added.  Therefore, name collision 
** will return an error.
*/

struct string_table label_string_table;



/*
 * string_table_init - Initialize the given string table.
 */
void
string_table_init(struct string_table *string_table)
{
	string_table->string_table_head = NULL;
	string_table->string_table_generation = 0;

	/*
	 * We nedd to initialize locks here!
	 */

	return;
}


/*
 * string_table_destroy - Destroy the given string table.
 */
void
string_table_destroy(struct string_table *string_table)
{
	struct string_table_item *item, *next_item;

	item = string_table->string_table_head;
	while (item) {
		next_item = item->next;

		STRTBL_FREE(item);
		item = next_item;
	}

	/*
	 * We need to destroy whatever lock we have here
	 */

	return;
}



/*
 * string_table_insert - Insert an entry in the string table .. duplicate 
 *	names are not allowed.
 */
char *
string_table_insert(struct string_table *string_table, char *name)
{
	struct string_table_item *item, *new_item = NULL, *last_item = NULL;

again:
	/*
	 * Need to lock the table ..
	 */
	item = string_table->string_table_head;
	last_item = NULL;

	while (item) {
		if (!strcmp(item->string, name)) {
			/*
			 * If we allocated space for the string and the found that
			 * someone else already entered it into the string table,
			 * free the space we just allocated.
			 */
			if (new_item)
				STRTBL_FREE(new_item);


			/*
			 * Search optimization: move the found item to the head
			 * of the list.
			 */
			if (last_item != NULL) {
				last_item->next = item->next;
				item->next = string_table->string_table_head;
				string_table->string_table_head = item;
			}
			goto out;
		}
		last_item = item;
		item=item->next;
	}

	/*
	 * name was not found, so add it to the string table.
	 */
	if (new_item == NULL) {
		long old_generation = string_table->string_table_generation;

		new_item = STRTBL_ALLOC(strlen(name));

		strcpy(new_item->string, name);

		/*
		 * While we allocated memory for the new string, someone else 
		 * changed the string table.
		 */
		if (old_generation != string_table->string_table_generation) {
			goto again;
		}
	} else {
		/* At this we only have the string table lock in access mode.
		 * Promote the access lock to an update lock for the string
		 * table insertion below.
		 */
			long old_generation = 
				string_table->string_table_generation;

			/*
			 * After we did the unlock and wer waiting for update
			 * lock someone could have potentially updated
			 * the string table. Check the generation number
			 * for this case. If it is the case we have to
			 * try all over again.
			 */
			if (old_generation != 
			    string_table->string_table_generation) {
				goto again;
			}
		}

	/*
	 * At this point, we're committed to adding new_item to the string table.
	 */
	new_item->next = string_table->string_table_head;
	item = string_table->string_table_head = new_item;
	string_table->string_table_generation++;

out:
	/*
	 * Need to unlock here.
	 */
	return(item->string);
}

/*
 * labelcl_info_create - Creates the data structure that will hold the
 *	device private information asscoiated with a entry.
 *	The pointer to this structure is what gets stored in the 
 *	(void * info).
 */
labelcl_info_t *
labelcl_info_create()
{

	labelcl_info_t *new = NULL;

	/* Initial allocation does not include any area for labels */
	if ( ( new = (labelcl_info_t *)kmalloc (sizeof(labelcl_info_t), GFP_KERNEL) ) == NULL )
		return NULL;

	memset (new, 0, sizeof(labelcl_info_t));
	new->hwcl_magic = LABELCL_MAGIC;
	return( new);

}

/*
 * labelcl_info_destroy - Frees the data structure that holds the
 *      device private information asscoiated with a entry.  This 
 *	data structure was created by device_info_create().
 *
 *	The caller is responsible for nulling the (void *info) in the 
 *	corresponding entry.
 */
int
labelcl_info_destroy(labelcl_info_t *labelcl_info)
{

	if (labelcl_info == NULL)
		return(0);

	/* Free the label list */
	if (labelcl_info->label_list)
		kfree(labelcl_info->label_list);

	/* Now free the label info area */
	labelcl_info->hwcl_magic = 0;
	kfree(labelcl_info);

	return(0);
}

/*
 * labelcl_info_add_LBL - Adds a new label entry in the labelcl info 
 *	structure.
 *
 *	Error is returned if we find another label with the same name.
 */
int
labelcl_info_add_LBL(vertex_hdl_t de,
			char *info_name,
			arb_info_desc_t info_desc,
			arbitrary_info_t info)
{
	labelcl_info_t	*labelcl_info = NULL;
	int num_labels;
	int new_label_list_size;
	label_info_t *old_label_list, *new_label_list = NULL;
	char *name;
	int i;

	if (de == NULL)
		return(-1);

        labelcl_info = hwgfs_get_info(de);
	if (labelcl_info == NULL)
		return(-1);

	if (labelcl_info->hwcl_magic != LABELCL_MAGIC)
		return(-1);

	if (info_name == NULL)
		return(-1);

	if (strlen(info_name) >= LABEL_LENGTH_MAX)
		return(-1);

	name = string_table_insert(&label_string_table, info_name);

	num_labels = labelcl_info->num_labels;
	new_label_list_size = sizeof(label_info_t) * (num_labels+1);

	/*
	 * Create a new label info area.
	 */
	if (new_label_list_size != 0) {
		new_label_list = (label_info_t *) kmalloc(new_label_list_size, GFP_KERNEL);

		if (new_label_list == NULL)
			return(-1);
	}

	/*
	 * At this point, we are committed to adding the labelled info, 
	 * if there isn't already information there with the same name.
	 */
	old_label_list = labelcl_info->label_list;

	/* 
	 * Look for matching info name.
	 */
	for (i=0; i<num_labels; i++) {
		if (!strcmp(info_name, old_label_list[i].name)) {
			/* Not allowed to add duplicate labelled info names. */
			kfree(new_label_list);
			return(-1);
		}
		new_label_list[i] = old_label_list[i]; /* structure copy */
	}

	new_label_list[num_labels].name = name;
	new_label_list[num_labels].desc = info_desc;
	new_label_list[num_labels].info = info;

	labelcl_info->num_labels = num_labels+1;
	labelcl_info->label_list = new_label_list;

	if (old_label_list != NULL)
		kfree(old_label_list);

	return(0);
}

/*
 * labelcl_info_remove_LBL - Remove a label entry.
 */
int
labelcl_info_remove_LBL(vertex_hdl_t de,
			 char *info_name,
			 arb_info_desc_t *info_desc,
			 arbitrary_info_t *info)
{
	labelcl_info_t	*labelcl_info = NULL;
	int num_labels;
	int new_label_list_size;
	label_info_t *old_label_list, *new_label_list = NULL;
	arb_info_desc_t label_desc_found;
	arbitrary_info_t label_info_found;
	int i;

	if (de == NULL)
		return(-1);

	labelcl_info = hwgfs_get_info(de);
	if (labelcl_info == NULL)
		return(-1);

	if (labelcl_info->hwcl_magic != LABELCL_MAGIC)
		return(-1);

	num_labels = labelcl_info->num_labels;
	if (num_labels == 0) {
		return(-1);
	}

	/*
	 * Create a new info area.
	 */
	new_label_list_size = sizeof(label_info_t) * (num_labels-1);
	if (new_label_list_size) {
		new_label_list = (label_info_t *) kmalloc(new_label_list_size, GFP_KERNEL);
		if (new_label_list == NULL)
			return(-1);
	}

	/*
	 * At this point, we are committed to removing the labelled info, 
	 * if it still exists.
	 */
	old_label_list = labelcl_info->label_list;

	/* 
	 * Find matching info name.
	 */
	for (i=0; i<num_labels; i++) {
		if (!strcmp(info_name, old_label_list[i].name)) {
			label_desc_found = old_label_list[i].desc;
			label_info_found = old_label_list[i].info;
			goto found;
		}
		if (i < num_labels-1) /* avoid walking off the end of the new vertex */
			new_label_list[i] = old_label_list[i]; /* structure copy */
	}

	/* The named info doesn't exist. */
	if (new_label_list)
		kfree(new_label_list);

	return(-1);

found:
	/* Finish up rest of labelled info */
	for (i=i+1; i<num_labels; i++)
		new_label_list[i-1] = old_label_list[i]; /* structure copy */

	labelcl_info->num_labels = num_labels+1;
	labelcl_info->label_list = new_label_list;

	kfree(old_label_list);

	if (info != NULL)
		*info = label_info_found;

	if (info_desc != NULL)
		*info_desc = label_desc_found;

	return(0);
}


/*
 * labelcl_info_replace_LBL - Replace an existing label entry with the 
 *	given new information.
 *
 *	Label entry must exist.
 */
int
labelcl_info_replace_LBL(vertex_hdl_t de,
			char *info_name,
			arb_info_desc_t info_desc,
			arbitrary_info_t info,
			arb_info_desc_t *old_info_desc,
			arbitrary_info_t *old_info)
{
	labelcl_info_t	*labelcl_info = NULL;
	int num_labels;
	label_info_t *label_list;
	int i;

	if (de == NULL)
		return(-1);

	labelcl_info = hwgfs_get_info(de);
	if (labelcl_info == NULL)
		return(-1);

	if (labelcl_info->hwcl_magic != LABELCL_MAGIC)
		return(-1);

	num_labels = labelcl_info->num_labels;
	if (num_labels == 0) {
		return(-1);
	}

	if (info_name == NULL)
		return(-1);

	label_list = labelcl_info->label_list;

	/* 
	 * Verify that information under info_name already exists.
	 */
	for (i=0; i<num_labels; i++)
		if (!strcmp(info_name, label_list[i].name)) {
			if (old_info != NULL)
				*old_info = label_list[i].info;

			if (old_info_desc != NULL)
				*old_info_desc = label_list[i].desc;

			label_list[i].info = info;
			label_list[i].desc = info_desc;

			return(0);
		}


	return(-1);
}

/*
 * labelcl_info_get_LBL - Retrieve and return the information for the 
 *	given label entry.
 */
int
labelcl_info_get_LBL(vertex_hdl_t de,
		      char *info_name,
		      arb_info_desc_t *info_desc,
		      arbitrary_info_t *info)
{
	labelcl_info_t	*labelcl_info = NULL;
	int num_labels;
	label_info_t *label_list;
	int i;

	if (de == NULL)
		return(-1);

	labelcl_info = hwgfs_get_info(de);
	if (labelcl_info == NULL)
		return(-1);

	if (labelcl_info->hwcl_magic != LABELCL_MAGIC)
		return(-1);

	num_labels = labelcl_info->num_labels;
	if (num_labels == 0) {
		return(-1);
	}

	label_list = labelcl_info->label_list;

	/* 
	 * Find information under info_name.
	 */
	for (i=0; i<num_labels; i++)
		if (!strcmp(info_name, label_list[i].name)) {
			if (info != NULL)
				*info = label_list[i].info;
			if (info_desc != NULL)
				*info_desc = label_list[i].desc;

			return(0);
		}

	return(-1);
}

/*
 * labelcl_info_get_next_LBL - returns the next label entry on the list.
 */
int
labelcl_info_get_next_LBL(vertex_hdl_t de,
			   char *buffer,
			   arb_info_desc_t *info_descp,
			   arbitrary_info_t *infop,
			   labelcl_info_place_t *placeptr)
{
	labelcl_info_t	*labelcl_info = NULL;
	uint which_info;
	label_info_t *label_list;

	if ((buffer == NULL) && (infop == NULL))
		return(-1);

	if (placeptr == NULL)
		return(-1);

	if (de == NULL)
		return(-1);

	labelcl_info = hwgfs_get_info(de);
	if (labelcl_info == NULL)
		return(-1);

	if (labelcl_info->hwcl_magic != LABELCL_MAGIC)
		return(-1);

	which_info = *placeptr;

	if (which_info >= labelcl_info->num_labels) {
		return(-1);
	}

	label_list = (label_info_t *) labelcl_info->label_list;

	if (buffer != NULL)
		strcpy(buffer, label_list[which_info].name);

	if (infop)
		*infop = label_list[which_info].info;

	if (info_descp)
		*info_descp = label_list[which_info].desc;

	*placeptr = which_info + 1;

	return(0);
}


int
labelcl_info_replace_IDX(vertex_hdl_t de,
			int index,
			arbitrary_info_t info,
			arbitrary_info_t *old_info)
{
	arbitrary_info_t *info_list_IDX;
	labelcl_info_t	*labelcl_info = NULL;

	if (de == NULL) {
		printk(KERN_ALERT "labelcl: NULL handle given.\n");
		return(-1);
	}

	labelcl_info = hwgfs_get_info(de);
	if (labelcl_info == NULL) {
		printk(KERN_ALERT "labelcl: Entry %p does not have info pointer.\n", (void *)de);
		return(-1);
	}

	if (labelcl_info->hwcl_magic != LABELCL_MAGIC)
		return(-1);

	if ( (index < 0) || (index >= HWGRAPH_NUM_INDEX_INFO) )
		return(-1);

	/*
	 * Replace information at the appropriate index in this vertex with 
	 * the new info.
	 */
	info_list_IDX = labelcl_info->IDX_list;
	if (old_info != NULL)
		*old_info = info_list_IDX[index];
	info_list_IDX[index] = info;

	return(0);

}

/*
 * labelcl_info_connectpt_set - Sets the connectpt.
 */
int
labelcl_info_connectpt_set(hwgfs_handle_t de,
			  hwgfs_handle_t connect_de)
{
	arbitrary_info_t old_info;
	int	rv;

	rv = labelcl_info_replace_IDX(de, HWGRAPH_CONNECTPT, 
		(arbitrary_info_t) connect_de, &old_info);

	if (rv) {
		return(rv);
	}

	return(0);
}


/*
 * labelcl_info_get_IDX - Returns the information pointed at by index.
 *
 */
int
labelcl_info_get_IDX(vertex_hdl_t de,
			int index,
			arbitrary_info_t *info)
{
	arbitrary_info_t *info_list_IDX;
	labelcl_info_t	*labelcl_info = NULL;

	if (de == NULL)
		return(-1);

	labelcl_info = hwgfs_get_info(de);
	if (labelcl_info == NULL)
		return(-1);

	if (labelcl_info->hwcl_magic != LABELCL_MAGIC)
		return(-1);

	if ( (index < 0) || (index >= HWGRAPH_NUM_INDEX_INFO) )
		return(-1);

	/*
	 * Return information at the appropriate index in this vertex.
	 */
	info_list_IDX = labelcl_info->IDX_list;
	if (info != NULL)
		*info = info_list_IDX[index];

	return(0);
}

/*
 * labelcl_info_connectpt_get - Retrieve the connect point for a device entry.
 */
hwgfs_handle_t
labelcl_info_connectpt_get(hwgfs_handle_t de)
{
	int rv;
	arbitrary_info_t info;

	rv = labelcl_info_get_IDX(de, HWGRAPH_CONNECTPT, &info);
	if (rv)
		return(NULL);

	return((hwgfs_handle_t) info);
}
