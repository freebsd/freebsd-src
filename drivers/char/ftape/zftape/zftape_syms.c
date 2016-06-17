/*
 *      Copyright (C) 1997 Claus-Justus Heine

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *
 * $Source: /homes/cvs/ftape-stacked/ftape/zftape/zftape_syms.c,v $
 * $Revision: 1.3 $
 * $Date: 1997/10/05 19:19:14 $
 *
 *      This file contains the symbols that the zftape frontend to 
 *      the ftape floppy tape driver exports 
 */		 

#define __NO_VERSION__
#include <linux/module.h>

#include <linux/zftape.h>

#include "../zftape/zftape-init.h"
#include "../zftape/zftape-read.h"
#include "../zftape/zftape-buffers.h"
#include "../zftape/zftape-ctl.h"

#if LINUX_VERSION_CODE >= KERNEL_VER(2,1,18)
# define FT_KSYM(sym) EXPORT_SYMBOL(sym);
#else
# define FT_KSYM(sym) X(sym),
#endif

#if LINUX_VERSION_CODE < KERNEL_VER(2,1,18)
struct symbol_table zft_symbol_table = {
#include <linux/symtab_begin.h>
#endif
/* zftape-init.c */
FT_KSYM(zft_cmpr_register)
FT_KSYM(zft_cmpr_unregister)
/* zftape-read.c */
FT_KSYM(zft_fetch_segment_fraction)
/* zftape-buffers.c */
FT_KSYM(zft_vmalloc_once)
FT_KSYM(zft_vmalloc_always)
FT_KSYM(zft_vfree)
#if LINUX_VERSION_CODE < KERNEL_VER(2,1,18)
#include <linux/symtab_end.h>
};
#endif
