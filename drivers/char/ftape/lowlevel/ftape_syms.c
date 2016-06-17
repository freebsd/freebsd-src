/*
 *      Copyright (C) 1996-1997 Claus-Justus Heine

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
 * $Source: /homes/cvs/ftape-stacked/ftape/lowlevel/ftape_syms.c,v $
 * $Revision: 1.4 $
 * $Date: 1997/10/17 00:03:51 $
 *
 *      This file contains the symbols that the ftape low level
 *      part of the QIC-40/80/3010/3020 floppy-tape driver "ftape"
 *      exports to it's high level clients
 */

#include <linux/config.h>
#define __NO_VERSION__
#include <linux/module.h>

#include <linux/ftape.h>
#include "../lowlevel/ftape-tracing.h"
#include "../lowlevel/ftape-init.h"
#include "../lowlevel/fdc-io.h"
#include "../lowlevel/ftape-read.h"
#include "../lowlevel/ftape-write.h"
#include "../lowlevel/ftape-io.h"
#include "../lowlevel/ftape-ctl.h"
#include "../lowlevel/ftape-rw.h"
#include "../lowlevel/ftape-bsm.h"
#include "../lowlevel/ftape-buffer.h"
#include "../lowlevel/ftape-format.h"

#if LINUX_VERSION_CODE >= KERNEL_VER(2,1,18)
# define FT_KSYM(sym) EXPORT_SYMBOL(sym);
#else
# define FT_KSYM(sym) X(sym),
#endif

#if LINUX_VERSION_CODE < KERNEL_VER(2,1,18)
struct symbol_table ftape_symbol_table = {
#include <linux/symtab_begin.h>
#endif
/* bad sector handling from ftape-bsm.c */
FT_KSYM(ftape_get_bad_sector_entry)
FT_KSYM(ftape_find_end_of_bsm_list)
/* from ftape-rw.c */
FT_KSYM(ftape_set_state)
/* from ftape-ctl.c */
FT_KSYM(ftape_seek_to_bot)
FT_KSYM(ftape_seek_to_eot)
FT_KSYM(ftape_abort_operation)
FT_KSYM(ftape_get_status)
FT_KSYM(ftape_enable)
FT_KSYM(ftape_disable)
FT_KSYM(ftape_mmap)
FT_KSYM(ftape_calibrate_data_rate)
/* from ftape-io.c */
FT_KSYM(ftape_reset_drive)
FT_KSYM(ftape_command)
FT_KSYM(ftape_parameter)
FT_KSYM(ftape_ready_wait)
FT_KSYM(ftape_report_operation)
FT_KSYM(ftape_report_error)
/* from ftape-read.c */
FT_KSYM(ftape_read_segment_fraction)
FT_KSYM(ftape_zap_read_buffers)
FT_KSYM(ftape_read_header_segment)
FT_KSYM(ftape_decode_header_segment)
/* from ftape-write.c */
FT_KSYM(ftape_write_segment)
FT_KSYM(ftape_start_writing)
FT_KSYM(ftape_loop_until_writes_done)
/* from ftape-buffer.h */
FT_KSYM(ftape_set_nr_buffers)
/* from ftape-format.h */
FT_KSYM(ftape_format_track)
FT_KSYM(ftape_format_status)
FT_KSYM(ftape_verify_segment)
/* from tracing.c */
#ifndef CONFIG_FT_NO_TRACE_AT_ALL
FT_KSYM(ftape_tracing)
FT_KSYM(ftape_function_nest_level)
FT_KSYM(ftape_trace_call)
FT_KSYM(ftape_trace_exit)
FT_KSYM(ftape_trace_log)
#endif
/* end of ksym table */
#if LINUX_VERSION_CODE < KERNEL_VER(2,1,18)
#include <linux/symtab_end.h>
};
#endif
