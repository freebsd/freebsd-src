/* Definitions to make GDB run on a mips box under Mach 3.0
   Copyright (C) 1992 Free Software Foundation, Inc.

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

/* Mach specific definitions for little endian mips (e.g. pmax)
 * running Mach 3.0
 *
 * Author: Jukka Virtanen <jtv@hut.fi>
 */

/* Include common definitions for Mach3 systems */
#include "nm-m3.h"

/* Define offsets to access CPROC stack when it does not have
 * a kernel thread.
 */

/* From mk/user/threads/mips/csw.s */
#define SAVED_FP	(12*4)
#define SAVED_PC	(13*4)
#define SAVED_BYTES	(14*4)

/* Using these, define our offsets to items strored in
 * cproc_switch in csw.s
 */
#define MACHINE_CPROC_SP_OFFSET SAVED_BYTES
#define MACHINE_CPROC_PC_OFFSET SAVED_PC
#define MACHINE_CPROC_FP_OFFSET SAVED_FP

/* Thread flavors used in setting the Trace state.
 *
 * In <mach/machine/thread_status.h>
 */
#define TRACE_FLAVOR		MIPS_EXC_STATE
#define TRACE_FLAVOR_SIZE	MIPS_EXC_STATE_COUNT
#define TRACE_SET(x,state)	((struct mips_exc_state *)state)->cause = EXC_SST;
#define TRACE_CLEAR(x,state)	0

/* Mach supports attach/detach */
#define ATTACH_DETACH 1

#include "mips/tm-mips.h"

/* Address of end of user stack space.
 * for MACH, see <machine/vmparam.h>
 */
#undef  STACK_END_ADDR
#define STACK_END_ADDR USRSTACK

/* Don't output r?? names for registers, since they
 * can't be used as reg names anyway
 */
#define NUMERIC_REG_NAMES

/* Output registers in tabular format */
#define TABULAR_REGISTER_OUTPUT
