/*-
 * Copyright 2000 Hans Reiser
 * See README for licensing and copyright details
 * 
 * Ported to FreeBSD by Jean-Sébastien Pédron <jspedron@club-internet.fr>
 * 
 * $FreeBSD$
 */

#include <gnu/fs/reiserfs/reiserfs_fs.h>

/* -------------------------------------------------------------------
 * Stat data functions
 * -------------------------------------------------------------------*/

static int
sd_bytes_number(struct item_head *ih, int block_size)
{ 

	return (0);
}

struct item_operations stat_data_ops = {
	.bytes_number      = sd_bytes_number,
	//.decrement_key     = sd_decrement_key,
	//.is_left_mergeable = sd_is_left_mergeable,
	//.print_item        = sd_print_item,
	//.check_item        = sd_check_item,

	//.create_vi         = sd_create_vi,
	//.check_left        = sd_check_left,
	//.check_right       = sd_check_right,
	//.part_size         = sd_part_size,
	//.unit_num          = sd_unit_num,
	//.print_vi          = sd_print_vi
};

/* -------------------------------------------------------------------
 * Direct item functions
 * -------------------------------------------------------------------*/

static int
direct_bytes_number(struct item_head *ih, int block_size)
{

	return (ih_item_len(ih));
}

struct item_operations direct_ops = {
	.bytes_number      = direct_bytes_number,
	//.decrement_key     = direct_decrement_key,
	//.is_left_mergeable = direct_is_left_mergeable,
	//.print_item        = direct_print_item,
	//.check_item        = direct_check_item,

	//.create_vi         = direct_create_vi,
	//.check_left        = direct_check_left,
	//.check_right       = direct_check_right,
	//.part_size         = direct_part_size,
	//.unit_num          = direct_unit_num,
	//.print_vi          = direct_print_vi
};

/* -------------------------------------------------------------------
 * Indirect item functions
 * -------------------------------------------------------------------*/

static int
indirect_bytes_number(struct item_head *ih, int block_size)
{

	return (ih_item_len(ih) / UNFM_P_SIZE * block_size);
}

struct item_operations indirect_ops = {
	.bytes_number      = indirect_bytes_number,
	//.decrement_key     = indirect_decrement_key,
	//.is_left_mergeable = indirect_is_left_mergeable,
	//.print_item        = indirect_print_item,
	//.check_item        = indirect_check_item,

	//.create_vi         = indirect_create_vi,
	//.check_left        = indirect_check_left,
	//.check_right       = indirect_check_right,
	//.part_size         = indirect_part_size,
	//.unit_num          = indirect_unit_num,
	//.print_vi          = indirect_print_vi
};

/* -------------------------------------------------------------------
 * Direntry functions
 * -------------------------------------------------------------------*/

static int
direntry_bytes_number(struct item_head *ih, int block_size)
{

	reiserfs_log(LOG_WARNING, "bytes number is asked for direntry\n");
	return (0);
}

struct item_operations direntry_ops = {
	.bytes_number      = direntry_bytes_number,
	//.decrement_key     = direntry_decrement_key,
	//.is_left_mergeable = direntry_is_left_mergeable,
	//.print_item        = direntry_print_item,
	//.check_item        = direntry_check_item,

	//.create_vi         = direntry_create_vi,
	//.check_left        = direntry_check_left,
	//.check_right       = direntry_check_right,
	//.part_size         = direntry_part_size,
	//.unit_num          = direntry_unit_num,
	//.print_vi          = direntry_print_vi
};

/* -------------------------------------------------------------------
 * Error catching functions to catch errors caused by incorrect item
 * types.
 * -------------------------------------------------------------------*/

static int
errcatch_bytes_number(struct item_head *ih, int block_size)
{

	reiserfs_log(LOG_WARNING, "invalid item type observed, run fsck ASAP");
	return (0);
}

struct item_operations errcatch_ops = {
	errcatch_bytes_number,
	//errcatch_decrement_key,
	//errcatch_is_left_mergeable,
	//errcatch_print_item,
	//errcatch_check_item,

	//errcatch_create_vi,
	//errcatch_check_left,
	//errcatch_check_right,
	//errcatch_part_size,
	//errcatch_unit_num,
	//errcatch_print_vi
};

#if !(TYPE_STAT_DATA == 0 && TYPE_INDIRECT == 1 &&			\
    TYPE_DIRECT == 2 && TYPE_DIRENTRY == 3)
#error
#endif

struct item_operations *item_ops[TYPE_ANY + 1] = {
	&stat_data_ops,
	&indirect_ops,
	&direct_ops,
	&direntry_ops,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	&errcatch_ops /* This is to catch errors with invalid type (15th
			 entry for TYPE_ANY) */
};
