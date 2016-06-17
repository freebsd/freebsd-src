/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2001-2003 Silicon Graphics, Inc.  All rights reserved.
*/


#ifndef _ASM_IA64_SN_NAG_H
#define _ASM_IA64_SN_NAG_H


#define NAG(mesg...) \
do {										\
	static unsigned int how_broken = 1;					\
	static unsigned int threshold = 1;					\
	if (how_broken == threshold) {						\
		if (threshold < 10000)						\
			threshold *= 10;					\
		if (how_broken > 1)						\
			printk(KERN_WARNING "%u times: ", how_broken);		\
		else								\
			printk(KERN_WARNING);					\
		printk(mesg);							\
	}									\
	how_broken++;								\
} while (0)


#endif /* _ASM_IA64_SN_NAG_H */
