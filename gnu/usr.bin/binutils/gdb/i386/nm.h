/* $FreeBSD: src/gnu/usr.bin/binutils/gdb/i386/nm.h,v 1.9.4.1 2000/08/22 12:28:19 joerg Exp $ */
/* Native-dependent definitions for Intel 386 running BSD Unix, for GDB.
   Copyright 1986, 1987, 1989, 1992, 1996 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef NM_FREEBSD_H
#define NM_FREEBSD_H

#define	ATTACH_DETACH

/* Be shared lib aware */
#include "solib.h"
#ifdef FREEBSD_ELF
#define SVR4_SHARED_LIBS
#endif

/* This is the amount to subtract from u.u_ar0
   to get the offset in the core file of the register values.  */

#include <machine/vmparam.h>
#define KERNEL_U_ADDR USRSTACK

#define REGISTER_U_ADDR(addr, blockend, regno) \
	(addr) = i386_register_u_addr ((blockend),(regno));

/* We define our own fetch/store methods */
#define FETCH_INFERIOR_REGISTERS

extern int
i386_register_u_addr PARAMS ((int, int));

#define PTRACE_ARG3_TYPE char*

#ifndef FREEBSD_ELF

/* make structure definitions match up with those expected in solib.c */
#define link_object	sod
#define lo_name		sod_name
#define lo_library	sod_library
#define lo_unused	sod_reserved
#define lo_major	sod_major
#define lo_minor	sod_minor
#define lo_next		sod_next

#define link_map	so_map
#define lm_addr		som_addr
#define lm_name		som_path
#define lm_next		som_next
#define lm_lop		som_sod
#define lm_lob		som_sodbase
#define lm_rwt		som_write
#define lm_ld		som_dynamic
#define lm_lpd		som_spd

#define link_dynamic_2	section_dispatch_table
#define ld_loaded	sdt_loaded
#define ld_need		sdt_sods
#define ld_rules	sdt_filler1
#define ld_got		sdt_got
#define ld_plt		sdt_plt
#define ld_rel		sdt_rel
#define ld_hash		sdt_hash
#define ld_stab		sdt_nzlist
#define ld_stab_hash	sdt_filler2
#define ld_buckets	sdt_buckets
#define ld_symbols	sdt_strings
#define ld_symb_size	sdt_str_sz
#define ld_text		sdt_text_sz
#define ld_plt_sz	sdt_plt_sz

#define rtc_symb	rt_symbol
#define rtc_sp		rt_sp
#define rtc_next	rt_next

#define ld_debug	so_debug
#define ldd_version	dd_version
#define ldd_in_debugger	dd_in_debugger
#define ldd_sym_loaded	dd_sym_loaded
#define ldd_bp_addr	dd_bpt_addr
#define ldd_bp_inst	dd_bpt_shadow
#define ldd_cp		dd_cc

#define link_dynamic	_dynamic
#define ld_version	d_version
#define ldd		d_debug
#define ld_un		d_un
#define ld_2		d_sdt

#endif

/* Return sizeof user struct to callers in less machine dependent routines */

#define KERNEL_U_SIZE kernel_u_size()
extern int kernel_u_size PARAMS ((void));

#define ADDITIONAL_OPTIONS \
	{"kernel", no_argument, &kernel_debugging, 1}, \
	{"k", no_argument, &kernel_debugging, 1}, \
	{"wcore", no_argument, &kernel_writablecore, 1}, \
	{"w", no_argument, &kernel_writablecore, 1},

#define ADDITIONAL_OPTION_HELP \
	"\
  --kernel           Enable kernel debugging.\n\
  --wcore            Make core file writable (only works for /dev/mem).\n\
                     This option only works while debugging a kernel !!\n\
"

extern int kernel_debugging;
extern int kernel_writablecore;

#define DEFAULT_PROMPT kernel_debugging?"(kgdb) ":"(gdb) "

/* misuse START_PROGRESS to test whether we're running as kgdb */
/* START_PROGRESS is called at the top of main */
#undef START_PROGRESS
#define START_PROGRESS(STR,N) \
  if (!strcmp(STR, "kgdb")) \
     kernel_debugging = 1;

#include <sys/types.h>
#include <sys/ptrace.h>

#ifdef PT_GETDBREGS
#define TARGET_HAS_HARDWARE_WATCHPOINTS

extern int can_watch PARAMS((int type, int cnt, int othertype));
extern int stopped_by_watchpoint PARAMS((void));
extern int insert_watchpoint PARAMS((int addr, int len, int type));
extern int remove_watchpoint PARAMS((int addr, int len, int type));

#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) \
	can_watch(type, cnt, ot)

/* After a watchpoint trap, the PC points to the instruction after
   the one that caused the trap.  Therefore we don't need to step over it.
   But we do need to reset the status register to avoid another trap.  */
#define HAVE_CONTINUABLE_WATCHPOINT

#define STOPPED_BY_WATCHPOINT(W)  \
	stopped_by_watchpoint()

/* Use these macros for watchpoint insertion/removal.  */

#define target_insert_watchpoint(addr, len, type)  \
	insert_watchpoint(addr, len, type)

#define target_remove_watchpoint(addr, len, type)  \
	remove_watchpoint(addr, len, type)

#endif /* PT_GETDBREGS */

#endif /* NM_FREEBSD_H */
