/* Am29k-dependent portions of the RPC protocol
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
  char am29k_greg_packet[AM29K_GREG_PLEN];
  char am29k_fpreg_packet[AM29K_FPREG_PLEN];

  /* Get general-purpose registers.  When copying values into
     registers [], don't assume that a location in registers []
     is properly aligned for the target data type.  */

  net_read_registers (am29k_greg_packet, AM29K_GREG_PLEN, PTRACE_GETREGS);

  /* Now copy the register values into registers[].
     Note that this code depends on the ordering of the REGNUMs
     as defined in "tm-29k.h".  */

  bcopy (&am29k_greg_packet[AM29K_R_GR96],
	 &registers[REGISTER_BYTE (GR96_REGNUM)], 160 * AM29K_GREG_SIZE);
  bcopy (&am29k_greg_packet[AM29K_R_VAB],
	 &registers[REGISTER_BYTE (VAB_REGNUM)], 15 * AM29K_GREG_SIZE);
  registers[REGISTER_BYTE (INTE_REGNUM)] = am29k_greg_packet[AM29K_R_INTE];
  bcopy (&am29k_greg_packet[AM29K_R_RSP],
	 &registers[REGISTER_BYTE (GR1_REGNUM)], 5 * AM29K_GREG_SIZE);

  /* PAD For now, don't care about exop register */

  memset (&registers[REGISTER_BYTE (EXO_REGNUM)], '\0', AM29K_GREG_SIZE);

  /* If the target has floating point registers, fetch them.
     Otherwise, zero the floating point register values in
     registers[] for good measure, even though we might not
     need to.  */

  if (target_has_fp)
    {
      net_read_registers (am29k_fpreg_packet, AM29K_FPREG_PLEN,
                          PTRACE_GETFPREGS);
      registers[REGISTER_BYTE (FPE_REGNUM)] = am29k_fpreg_packet[AM29K_R_FPE];
      registers[REGISTER_BYTE (FPS_REGNUM)] = am29k_fpreg_packet[AM29K_R_FPS];

      /* PAD For now, don't care about registers (?) AI0 to q */

      memset (&registers[REGISTER_BYTE (161)], '\0', 21 * AM29K_FPREG_SIZE);
    }
  else
    { 
      memset (&registers[REGISTER_BYTE (FPE_REGNUM)], '\0', AM29K_FPREG_SIZE);
      memset (&registers[REGISTER_BYTE (FPS_REGNUM)], '\0', AM29K_FPREG_SIZE);

      /* PAD For now, don't care about registers (?) AI0 to q */

      memset (&registers[REGISTER_BYTE (161)], '\0', 21 * AM29K_FPREG_SIZE);
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
  char am29k_greg_packet[AM29K_GREG_PLEN];
  char am29k_fpreg_packet[AM29K_FPREG_PLEN];

  /* Store general purpose registers.  When copying values from
     registers [], don't assume that a location in registers []
     is properly aligned for the target data type.  */

  bcopy (&registers[REGISTER_BYTE (GR96_REGNUM)],
	 &am29k_greg_packet[AM29K_R_GR96], 160 * AM29K_GREG_SIZE);
  bcopy (&registers[REGISTER_BYTE (VAB_REGNUM)],
	 &am29k_greg_packet[AM29K_R_VAB], 15 * AM29K_GREG_SIZE);
  am29k_greg_packet[AM29K_R_INTE] = registers[REGISTER_BYTE (INTE_REGNUM)];
  bcopy (&registers[REGISTER_BYTE (GR1_REGNUM)],
	 &am29k_greg_packet[AM29K_R_RSP], 5 * AM29K_GREG_SIZE);

  net_write_registers (am29k_greg_packet, AM29K_GREG_PLEN, PTRACE_SETREGS);

  /* Store floating point registers if the target has them.  */

  if (target_has_fp)
    {
      am29k_fpreg_packet[AM29K_R_FPE] = registers[REGISTER_BYTE (FPE_REGNUM)];
      am29k_fpreg_packet[AM29K_R_FPS] = registers[REGISTER_BYTE (FPS_REGNUM)];

      net_write_registers (am29k_fpreg_packet, AM29K_FPREG_PLEN,
                           PTRACE_SETFPREGS);
    }
}

/* VxWorks zeroes fp when the task is initialized; we use this
   to terminate the frame chain. Chain means here the nominal address of
   a frame, that is, the return address (lr0) address in the stack. To
   obtain the frame pointer (lr1) contents, we must add 4 bytes.
   Note : may be we should modify init_frame_info() to get the frame pointer
          and store it into the frame_info struct rather than reading its
          contents when FRAME_CHAIN_VALID is invoked. */

int
get_fp_contents (chain, thisframe)
     CORE_ADDR chain;
     struct frame_info *thisframe;      /* not used here */
{
   int fp_contents;

   read_memory ((CORE_ADDR)(chain + 4), (char *) &fp_contents, 4);
   return (fp_contents != 0);
}

