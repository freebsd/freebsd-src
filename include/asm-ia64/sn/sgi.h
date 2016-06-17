/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

/*
 * This file is a bit of a dumping ground. A lot of things that don't really
 * have a home, but which make life easier, end up here.
 */

#ifndef _ASM_IA64_SN_SGI_H
#define _ASM_IA64_SN_SGI_H

#include <linux/config.h>
#include <asm/sn/types.h>
#include <linux/mm.h>
#include <linux/devfs_fs_kernel.h>
#ifdef CONFIG_HWGFS_FS
#include <linux/fs.h>
#include <asm/sn/hwgfs.h>
typedef hwgfs_handle_t vertex_hdl_t;
#else
typedef devfs_handle_t vertex_hdl_t;
#endif

/* Nice general name length that lots of people like to use */
#ifndef MAXDEVNAME
#define MAXDEVNAME 256
#endif


/*
 * Possible return values from graph routines.
 */
typedef enum graph_error_e {
	GRAPH_SUCCESS,		/* 0 */
	GRAPH_DUP,		/* 1 */
	GRAPH_NOT_FOUND,	/* 2 */
	GRAPH_BAD_PARAM,	/* 3 */
	GRAPH_HIT_LIMIT,	/* 4 */
	GRAPH_CANNOT_ALLOC,	/* 5 */
	GRAPH_ILLEGAL_REQUEST,	/* 6 */
	GRAPH_IN_USE		/* 7 */
} graph_error_t;

#define CNODEID_NONE ((cnodeid_t)-1)
#define CPU_NONE		(-1)


/* print_register() defs */

/*
 * register values
 * map between numeric values and symbolic values
 */
struct reg_values {
	unsigned long long rv_value;
	char *rv_name;
};

/*
 * register descriptors are used for formatted prints of register values
 * rd_mask and rd_shift must be defined, other entries may be null
 */
struct reg_desc {
	unsigned long long rd_mask;	/* mask to extract field */
	int rd_shift;		/* shift for extracted value, - >>, + << */
	char *rd_name;		/* field name */
	char *rd_format;	/* format to print field */
	struct reg_values *rd_values;	/* symbolic names of values */
};

extern void print_register(unsigned long long, struct reg_desc *);


/*
 * No code is complete without an Assertion macro
 */

#if defined(DISABLE_ASSERT)
#define ASSERT(expr)
#define ASSERT_ALWAYS(expr)
#else
#define ASSERT(expr)  do {	\
        if(!(expr)) { \
		printk( "Assertion [%s] failed! %s:%s(line=%d)\n",\
			#expr,__FILE__,__FUNCTION__,__LINE__); \
		panic("Assertion panic\n"); 	\
        } } while(0)

#define ASSERT_ALWAYS(expr)	do {\
        if(!(expr)) { \
		printk( "Assertion [%s] failed! %s:%s(line=%d)\n",\
			#expr,__FILE__,__FUNCTION__,__LINE__); \
		panic("Assertion always panic\n"); 	\
        } } while(0)
#endif	/* DISABLE_ASSERT */

#endif /* _ASM_IA64_SN_SGI_H */
