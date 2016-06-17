/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_LABELCL_H
#define _ASM_IA64_SN_LABELCL_H

#define LABELCL_MAGIC 0x4857434c	/* 'HWLC' */
#define LABEL_LENGTH_MAX 256		/* Includes NULL char */
#define INFO_DESC_PRIVATE (-1)      	/* default */
#define INFO_DESC_EXPORT  0       	/* export info itself */

/*
 * Internal Error codes.
 */
typedef enum labelcl_error_e {  LABELCL_SUCCESS,          /* 0 */
                                LABELCL_DUP,              /* 1 */
                                LABELCL_NOT_FOUND,        /* 2 */
                                LABELCL_BAD_PARAM,        /* 3 */
                                LABELCL_HIT_LIMIT,        /* 4 */
                                LABELCL_CANNOT_ALLOC,     /* 5 */
                                LABELCL_ILLEGAL_REQUEST,  /* 6 */
                                LABELCL_IN_USE            /* 7 */
                                } labelcl_error_t;


/*
 * Description of a label entry.
 */
typedef struct label_info_s {
        char			*name;
        arb_info_desc_t		desc;
        arbitrary_info_t	info;
} label_info_t;

/*
 * Definition of the data structure that provides the link to 
 * the hwgraph fastinfo and the label entries associated with a 
 * particular devfs entry.
 */
typedef struct labelcl_info_s {
	unsigned long	hwcl_magic;
	unsigned long	num_labels;
	void		*label_list;
	arbitrary_info_t IDX_list[HWGRAPH_NUM_INDEX_INFO];
} labelcl_info_t;

/*
 * Definitions for the string table that holds the actual names 
 * of the labels.
 */
struct string_table_item {
        struct string_table_item        *next;
        char                            string[1];
};

struct string_table {
        struct string_table_item        *string_table_head;
        long                            string_table_generation;
};


#define STRTBL_BASIC_SIZE ((size_t)(((struct string_table_item *)0)->string))
#define STRTBL_ITEM_SIZE(str_length) (STRTBL_BASIC_SIZE + (str_length) + 1)

#define STRTBL_ALLOC(str_length) \
        ((struct string_table_item *)kmalloc(STRTBL_ITEM_SIZE(str_length), GFP_KERNEL))

#define STRTBL_FREE(ptr) kfree(ptr)


extern labelcl_info_t *labelcl_info_create(void);
extern int labelcl_info_destroy(labelcl_info_t *);
extern int labelcl_info_add_LBL(vertex_hdl_t, char *, arb_info_desc_t, arbitrary_info_t);
extern int labelcl_info_remove_LBL(vertex_hdl_t, char *, arb_info_desc_t *, arbitrary_info_t *);
extern int labelcl_info_replace_LBL(vertex_hdl_t, char *, arb_info_desc_t,
                        arbitrary_info_t, arb_info_desc_t *, arbitrary_info_t *);
extern int labelcl_info_get_LBL(vertex_hdl_t, char *, arb_info_desc_t *,
                      arbitrary_info_t *);
extern int labelcl_info_get_next_LBL(vertex_hdl_t, char *, arb_info_desc_t *,
                           arbitrary_info_t *, labelcl_info_place_t *);
extern int labelcl_info_replace_IDX(vertex_hdl_t, int, arbitrary_info_t, 
			arbitrary_info_t *);
extern int labelcl_info_connectpt_set(vertex_hdl_t, vertex_hdl_t);
extern int labelcl_info_get_IDX(vertex_hdl_t, int, arbitrary_info_t *);
extern struct devfs_handle_t device_info_connectpt_get(vertex_hdl_t);

#endif /* _ASM_IA64_SN_LABELCL_H */
