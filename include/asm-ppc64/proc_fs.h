#ifndef _PPC64_PROC_FS_H
#define _PPC64_PROC_FS_H
/*
 * proc_fs.h
 * Copyright (C) 2001  Mike Corrigan  IBM Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/* Change Activity: */
/* tgall -- merge of iSeries/iSeries_proc.h and proc_pmc.h */
/* End Change Activity */

#include <linux/proc_fs.h>

void pmc_proc_init(struct proc_dir_entry *iSeries_proc);
void proc_ppc64_init(void);

#include <asm/iSeries/iSeries_proc.h>

#endif
