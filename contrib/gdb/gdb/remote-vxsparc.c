/* sparc-dependent portions of the RPC protocol
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

/* sparc floating point format descriptor, from "sparc-tdep.c."  */

extern struct ext_format ext_format_sparc;

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
  char sparc_greg_packet[SPARC_GREG_PLEN];
  char sparc_fpreg_packet[SPARC_FPREG_PLEN];
  CORE_ADDR sp;

  /* Get general-purpose registers.  When copying values into
     registers [], don't assume that a location in registers []
     is properly aligned for the target data type.  */

  net_read_registers (sparc_greg_packet, SPARC_GREG_PLEN, PTRACE_GETREGS);

  /* Now copy the register values into registers[].
     Note that this code depends on the ordering of the REGNUMs
     as defined in "tm-sparc.h".  */

  bcopy (&sparc_greg_packet[SPARC_R_G0],
	 &registers[REGISTER_BYTE (G0_REGNUM)], 32 * SPARC_GREG_SIZE);
  bcopy (&sparc_greg_packet[SPARC_R_Y],
	 &registers[REGISTER_BYTE (Y_REGNUM)], 6 * SPARC_GREG_SIZE);

  /* Now write the local and in registers to the register window
     spill area in the frame.  VxWorks does not do this for the
     active frame automatically; it greatly simplifies debugging
     (FRAME_FIND_SAVED_REGS, in particular, depends on this).  */

  sp = extract_address (&registers[REGISTER_BYTE (SP_REGNUM)], 
	REGISTER_RAW_SIZE (CORE_ADDR));
  write_memory (sp, &registers[REGISTER_BYTE (L0_REGNUM)],
		16 * REGISTER_RAW_SIZE (L0_REGNUM));

  /* If the target has floating point registers, fetch them.
     Otherwise, zero the floating point register values in
     registers[] for good measure, even though we might not
     need to.  */

  if (target_has_fp)
    {
      net_read_registers (sparc_fpreg_packet, SPARC_FPREG_PLEN,
                          PTRACE_GETFPREGS);
      bcopy (&sparc_fpreg_packet[SPARC_R_FP0], 
             &registers[REGISTER_BYTE (FP0_REGNUM)], 32 * SPARC_FPREG_SIZE);
      bcopy (&sparc_fpreg_packet[SPARC_R_FSR],
	     &registers[REGISTER_BYTE (FPS_REGNUM)], 1 * SPARC_FPREG_SIZE);
    }
  else
    { 
      bzero (&registers[REGISTER_BYTE (FP0_REGNUM)], 32 * SPARC_FPREG_SIZE);
      bzero (&registers[REGISTER_BYTE (FPS_REGNUM)], 1 * SPARC_FPREG_SIZE);
    }

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
  char sparc_greg_packet[SPARC_GREG_PLEN];
  char sparc_fpreg_packet[SPARC_FPREG_PLEN];
  int in_gp_regs;
  int in_fp_regs;
  CORE_ADDR sp;

  /* Store general purpose registers.  When copying values from
     registers [], don't assume that a location in registers []
     is properly aligned for the target data type.  */

  in_gp_regs = 1;
  in_fp_regs = 1;
  if (regno >= 0)
    {
      if ((G0_REGNUM <= regno && regno <= I7_REGNUM)
          || (Y_REGNUM <= regno && regno <= NPC_REGNUM))
	in_fp_regs = 0;
      else
	in_gp_regs = 0;
    }
  if (in_gp_regs)
    {
      bcopy (&registers[REGISTER_BYTE (G0_REGNUM)],
	     &sparc_greg_packet[SPARC_R_G0], 32 * SPARC_GREG_SIZE);
      bcopy (&registers[REGISTER_BYTE (Y_REGNUM)],
	     &sparc_greg_packet[SPARC_R_Y], 6 * SPARC_GREG_SIZE);

      net_write_registers (sparc_greg_packet, SPARC_GREG_PLEN, PTRACE_SETREGS);

      /* If this is a local or in register, or we're storing all
         registers, update the register window spill area.  */

      if (regno < 0 || (L0_REGNUM <= regno && regno <= I7_REGNUM))
        {
  	  sp = extract_address (&registers[REGISTER_BYTE (SP_REGNUM)], 
		REGISTER_RAW_SIZE (CORE_ADDR));
	  write_memory (sp, &registers[REGISTER_BYTE (L0_REGNUM)],
			16 * REGISTER_RAW_SIZE (L0_REGNUM));
	}
    }

  /* Store floating point registers if the target has them.  */

  if (in_fp_regs && target_has_fp)
    {
      bcopy (&registers[REGISTER_BYTE (FP0_REGNUM)], 
	     &sparc_fpreg_packet[SPARC_R_FP0], 32 * SPARC_FPREG_SIZE);
      bcopy (&registers[REGISTER_BYTE (FPS_REGNUM)], 
	     &sparc_fpreg_packet[SPARC_R_FSR], 1 * SPARC_FPREG_SIZE);

      net_write_registers (sparc_fpreg_packet, SPARC_FPREG_PLEN,
                           PTRACE_SETFPREGS);
    }
}
