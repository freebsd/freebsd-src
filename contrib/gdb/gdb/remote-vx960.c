/* i80960-dependent portions of the RPC protocol
   used with a VxWorks target 

Contributed by Wind River Systems.

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

#include <stdio.h>
#include "defs.h"

#include "vx-share/regPacket.h"  
#include "frame.h"
#include "inferior.h"
#include "wait.h"
#include "target.h"
#include "gdbcore.h"
#include "command.h"
#include "symtab.h"
#include "symfile.h"		/* for struct complaint */

#include "gdb_string.h"
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#ifdef _AIX                     /* IBM claims "void *malloc()" not char * */
#define malloc bogon_malloc
#endif

#include <rpc/rpc.h>
#include <sys/time.h>		/* UTek's <rpc/rpc.h> doesn't #incl this */
#include <netdb.h>
#include "vx-share/ptrace.h"
#include "vx-share/xdr_ptrace.h"
#include "vx-share/xdr_ld.h"
#include "vx-share/xdr_rdb.h"
#include "vx-share/dbgRpcLib.h"

/* get rid of value.h if possible */
#include <value.h>
#include <symtab.h>

/* Flag set if target has fpu */

extern int target_has_fp;

/* 960 floating point format descriptor, from "i960-tdep.c."  */

extern struct ext_format ext_format_i960;

/* Generic register read/write routines in remote-vx.c.  */

extern void net_read_registers ();
extern void net_write_registers ();

/* Read a register or registers from the VxWorks target.
   REGNO is the register to read, or -1 for all; currently,
   it is ignored.  FIXME look at regno to improve efficiency.  */

void
vx_read_register (regno)
     int regno;
{
  char i960_greg_packet[I960_GREG_PLEN];
  char i960_fpreg_packet[I960_FPREG_PLEN];

  /* Get general-purpose registers.  When copying values into
     registers [], don't assume that a location in registers []
     is properly aligned for the target data type.  */

  net_read_registers (i960_greg_packet, I960_GREG_PLEN, PTRACE_GETREGS);

  bcopy (&i960_greg_packet[I960_R_R0],
	 &registers[REGISTER_BYTE (R0_REGNUM)], 16 * I960_GREG_SIZE);
  bcopy (&i960_greg_packet[I960_R_G0],
	 &registers[REGISTER_BYTE (G0_REGNUM)], 16 * I960_GREG_SIZE);
  bcopy (&i960_greg_packet[I960_R_PCW],
	 &registers[REGISTER_BYTE (PCW_REGNUM)], sizeof (int));
  bcopy (&i960_greg_packet[I960_R_ACW],
	 &registers[REGISTER_BYTE (ACW_REGNUM)], sizeof (int));
  bcopy (&i960_greg_packet[I960_R_TCW],
	 &registers[REGISTER_BYTE (TCW_REGNUM)], sizeof (int));

  /* If the target has floating point registers, fetch them.
     Otherwise, zero the floating point register values in
     registers[] for good measure, even though we might not
     need to.  */

  if (target_has_fp)
    {
      net_read_registers (i960_fpreg_packet, I960_FPREG_PLEN,
                          PTRACE_GETFPREGS);
      bcopy (&i960_fpreg_packet[I960_R_FP0], 
             &registers[REGISTER_BYTE (FP0_REGNUM)],
  	     REGISTER_RAW_SIZE (FP0_REGNUM) * 4);
    }
  else
    bzero (&registers[REGISTER_BYTE (FP0_REGNUM)],
           REGISTER_RAW_SIZE (FP0_REGNUM) * 4);

  /* Mark the register cache valid.  */

  registers_fetched ();
}

/* Store a register or registers into the VxWorks target.
   REGNO is the register to store, or -1 for all; currently,
   it is ignored.  FIXME look at regno to improve efficiency.  */

void
vx_write_register (regno)
     int regno;
{
  char i960_greg_packet[I960_GREG_PLEN];
  char i960_fpreg_packet[I960_FPREG_PLEN];

  /* Store floating-point registers.  When copying values from
     registers [], don't assume that a location in registers []
     is properly aligned for the target data type.  */

  bcopy (&registers[REGISTER_BYTE (R0_REGNUM)],
	 &i960_greg_packet[I960_R_R0], 16 * I960_GREG_SIZE);
  bcopy (&registers[REGISTER_BYTE (G0_REGNUM)],
	 &i960_greg_packet[I960_R_G0], 16 * I960_GREG_SIZE);
  bcopy (&registers[REGISTER_BYTE (PCW_REGNUM)],
	 &i960_greg_packet[I960_R_PCW], sizeof (int));
  bcopy (&registers[REGISTER_BYTE (ACW_REGNUM)],
	 &i960_greg_packet[I960_R_ACW], sizeof (int));
  bcopy (&registers[REGISTER_BYTE (TCW_REGNUM)],
	 &i960_greg_packet[I960_R_TCW], sizeof (int));

  net_write_registers (i960_greg_packet, I960_GREG_PLEN, PTRACE_SETREGS);

  /* Store floating point registers if the target has them.  */

  if (target_has_fp)
    {
      bcopy (&registers[REGISTER_BYTE (FP0_REGNUM)], 
	     &i960_fpreg_packet[I960_R_FP0],
	     REGISTER_RAW_SIZE (FP0_REGNUM) * 4);

      net_write_registers (i960_fpreg_packet, I960_FPREG_PLEN,
                           PTRACE_SETFPREGS);
    }
}

