/* GDB interface to ARM RDI library.
   Copyright 1997, 1998 Free Software Foundation, Inc.

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

#include "defs.h"
#include "gdb_string.h"
#include <fcntl.h>
#include "frame.h"
#include "inferior.h"
#include "bfd.h"
#include "symfile.h"
#include "target.h"
#include "wait.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "gdb-stabs.h"
#include "gdbthread.h"
#include "gdbcore.h"

#ifdef USG
#include <sys/types.h>
#endif

#include <signal.h>

#include "rdi-share/ardi.h"
#include "rdi-share/adp.h"
#include "rdi-share/hsys.h"

extern int isascii PARAMS ((int));

/* Prototypes for local functions */

static void arm_rdi_files_info PARAMS ((struct target_ops *ignore));

static int arm_rdi_xfer_memory PARAMS ((CORE_ADDR memaddr, char *myaddr,
				       int len, int should_write,
				       struct target_ops *target));

static void arm_rdi_prepare_to_store PARAMS ((void));

static void arm_rdi_fetch_registers PARAMS ((int regno));

static void arm_rdi_resume PARAMS ((int pid, int step,
				   enum target_signal siggnal));

static int arm_rdi_start_remote PARAMS ((char *dummy));

static void arm_rdi_open PARAMS ((char *name, int from_tty));

static void arm_rdi_create_inferior PARAMS ((char *exec_file, char *args,
					     char **env));

static void arm_rdi_close PARAMS ((int quitting));

static void arm_rdi_store_registers PARAMS ((int regno));

static void arm_rdi_mourn PARAMS ((void));

static void arm_rdi_send PARAMS ((char *buf));

static int arm_rdi_wait PARAMS ((int pid, struct target_waitstatus *status));

static void arm_rdi_kill PARAMS ((void));

static void arm_rdi_detach PARAMS ((char *args, int from_tty));

static void arm_rdi_interrupt PARAMS ((int signo));

static void arm_rdi_interrupt_twice PARAMS ((int signo));

static void interrupt_query PARAMS ((void));

static int arm_rdi_insert_breakpoint PARAMS ((CORE_ADDR, char *));

static int arm_rdi_remove_breakpoint PARAMS ((CORE_ADDR, char *));

static char *rdi_error_message PARAMS ((int err));

static enum target_signal rdi_error_signal PARAMS ((int err));

/* Global variables.  */

struct target_ops arm_rdi_ops;

static struct Dbg_ConfigBlock gdb_config;

static struct Dbg_HostosInterface gdb_hostif;

static int max_load_size;

static int execute_status;

/* A little list of breakpoints that have been set.  */

static struct local_bp_list_entry {
  CORE_ADDR addr;
  PointHandle point;
  struct local_bp_list_entry *next;
} *local_bp_list;


/* Stub for catch_errors.  */

static int
arm_rdi_start_remote (dummy)
     char *dummy;
{
  return 1;
}

/* Helper callbacks for the "host interface" structure.  RDI functions call
   these to forward output from the target system and so forth.  */

void
voiddummy ()
{
  fprintf_unfiltered (gdb_stdout, "void dummy\n");
}

static void
myprint (arg, format, ap)
     PTR arg;
     const char *format;
     va_list ap;
{
  vfprintf_unfiltered (gdb_stdout, format, ap);
}

static void
mywritec (arg, c)
     PTR arg;
     int c;
{
  if (isascii (c))
    fputc_unfiltered (c, gdb_stdout);
}

static int
mywrite (arg, buffer, len)
     PTR arg;
     char const *buffer;
     int len;
{
  int i;
  char *e;

  e = (char *) buffer;
  for (i = 0; i < len; i++)
{
      if (isascii ((int) *e))
        {
          fputc_unfiltered ((int) *e, gdb_stdout);
          e++;
        }
}

  return len;
}

static void
mypause (arg)
     PTR arg;
{
}

/* These last two are tricky as we have to handle the special case of
   being interrupted more carefully */

static int
myreadc (arg)
     PTR arg;
{ 
  return fgetc (stdin);
}

static char *
mygets (arg, buffer, len)
     PTR arg;
     char *buffer;
     int len;
{
  return fgets(buffer, len, stdin);
}

/* Prevent multiple calls to angel_RDI_close().  */
static int closed_already = 1;

/* Open a connection to a remote debugger.  NAME is the filename used
   for communication.  */

static void
arm_rdi_open (name, from_tty)
     char *name;
     int from_tty;
{
  int rslt, i;
  unsigned long arg1, arg2;

  if (name == NULL)
    error ("To open an RDI connection, you need to specify what serial\n\
device is attached to the remote system (e.g. /dev/ttya).");

  /* Make the basic low-level connection.  */

  rslt = Adp_OpenDevice (name, NULL, 1);

  if (rslt != adp_ok)
    error ("Could not open device \"%s\"", name);

  gdb_config.bytesex = 2 | (TARGET_BYTE_ORDER == BIG_ENDIAN ? 1 : 0);
  gdb_config.fpe = 1;
  gdb_config.rditype = 2;
  gdb_config.heartbeat_on = 1;
  gdb_config.flags = 2;

  gdb_hostif.dbgprint = myprint;
  gdb_hostif.dbgpause = mypause;
  gdb_hostif.dbgarg = NULL;
  gdb_hostif.writec = mywritec;
  gdb_hostif.readc = myreadc;
  gdb_hostif.write = mywrite;
  gdb_hostif.gets = mygets;
  gdb_hostif.hostosarg = NULL;
  gdb_hostif.reset = voiddummy;

  rslt = angel_RDI_open (10, &gdb_config, &gdb_hostif, NULL);
  if (rslt == RDIError_BigEndian || rslt == RDIError_LittleEndian)
    ;  /* do nothing, this is the expected return */
  else if (rslt)
    {
      printf_filtered ("RDI_open: %s\n", rdi_error_message (rslt));
    }

  rslt = angel_RDI_info (RDIInfo_Target, &arg1, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }
  rslt = angel_RDI_info (RDIInfo_Points, &arg1, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }
  rslt = angel_RDI_info (RDIInfo_Step, &arg1, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }
  rslt = angel_RDI_info (RDIInfo_CoPro, &arg1, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }
  rslt = angel_RDI_info (RDIInfo_SemiHosting, &arg1, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }

  rslt = angel_RDI_info (RDIInfo_GetLoadSize, &arg1, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }
  max_load_size = arg1;

  push_target (&arm_rdi_ops);

  target_fetch_registers (-1);

  rslt = angel_RDI_open (1, &gdb_config, NULL, NULL);
  if (rslt)
    {
      printf_filtered ("RDI_open: %s\n", rdi_error_message (rslt));
    }

  arg1 = 0x13b;
  rslt = angel_RDI_info (RDIVector_Catch, &arg1, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }

  arg1 = (unsigned long) "";
  rslt = angel_RDI_info (RDISet_Cmdline, &arg1, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }

  /* Clear out any existing records of breakpoints.  */
  {
    struct local_bp_list_entry *entry, *preventry = NULL;

    for (entry = local_bp_list; entry != NULL; entry = entry->next)
      {
	if (preventry)
	  free (preventry);
      }
  }

  printf_filtered ("Connected to ARM RDI target.\n");
  closed_already = 0;
  inferior_pid = 42;
}

/* Start an inferior process and set inferior_pid to its pid.
   EXEC_FILE is the file to run.
   ARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  Errors reported with error().
   On VxWorks and various standalone systems, we ignore exec_file.  */
/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */

static void
arm_rdi_create_inferior (exec_file, args, env)
     char *exec_file;
     char *args;
     char **env;
{
  int len, rslt;
  unsigned long arg1, arg2;
  char *arg_buf;
  CORE_ADDR entry_point;

  if (exec_file == 0 || exec_bfd == 0)
   error ("No executable file specified.");

  entry_point = (CORE_ADDR) bfd_get_start_address (exec_bfd);

  arm_rdi_kill ();	 
  remove_breakpoints ();
  init_wait_for_inferior ();

  len = strlen (exec_file) + 1 + strlen (args) + 1 + /*slop*/ 10;
  arg_buf = (char *) alloca (len);
  arg_buf[0] = '\0';
  strcat (arg_buf, exec_file);
  strcat (arg_buf, " ");
  strcat (arg_buf, args);

  inferior_pid = 42;
  insert_breakpoints ();  /* Needed to get correct instruction in cache */

  if (env != NULL)
    {
      while (*env)
	{
	  if (strncmp (*env, "MEMSIZE=", sizeof ("MEMSIZE=") - 1) == 0)
	    {
	      unsigned long top_of_memory;
	      char *end_of_num;

	      /* Set up memory limit */
	      top_of_memory = strtoul (*env + sizeof ("MEMSIZE=") - 1,
				       &end_of_num, 0);
	      printf_filtered ("Setting top-of-memory to 0x%x\n",
			       top_of_memory);
	  
	      rslt = angel_RDI_info (RDIInfo_SetTopMem, &top_of_memory, &arg2);
	      if (rslt)
		{
		  printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
		}
	    }
	  env++;
	}
    }

  arg1 = (unsigned long) arg_buf;
  rslt = angel_RDI_info (RDISet_Cmdline, /* &arg1 */ (unsigned long *)arg_buf, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }

  proceed (entry_point, TARGET_SIGNAL_DEFAULT, 0);
}

/* This takes a program previously attached to and detaches it.  After
   this is done, GDB can be used to debug some other program.  We
   better not have left any breakpoints in the target program or it'll
   die when it hits one.  */

static void
arm_rdi_detach (args, from_tty)
     char *args;
     int from_tty;
{
  pop_target ();
}

/* Clean up connection to a remote debugger.  */

static void
arm_rdi_close (quitting)
     int quitting;
{
  int rslt;

  if (! closed_already)
    {
      rslt = angel_RDI_close ();
      if (rslt)
	{
	  printf_filtered ("RDI_close: %s\n", rdi_error_message (rslt));
	}
      closed_already = 1;
      inferior_pid = 0;
    }
}

/* Tell the remote machine to resume.  */

static void
arm_rdi_resume (pid, step, siggnal)
     int pid, step;
     enum target_signal siggnal;
{
  int rslt;
  PointHandle point;

  if (0 /* turn on when hardware supports single-stepping */)
    {
      rslt = angel_RDI_step (1, &point);
      if (rslt)
	{
	  printf_filtered ("RDI_step: %s\n", rdi_error_message (rslt));
	}
    }
  else
    {
      char handle[4];
      CORE_ADDR pc;

      if (step)
	{
	  pc = read_register (PC_REGNUM);
	  pc = arm_get_next_pc (pc);
	  arm_rdi_insert_breakpoint (pc, handle);
	}
      execute_status = rslt = angel_RDI_execute (&point);
      if (rslt == RDIError_BreakpointReached)
	;
      else if (rslt)
	{
	  printf_filtered ("RDI_execute: %s\n", rdi_error_message (rslt));
	}
      if (step)
	{
	  arm_rdi_remove_breakpoint (pc, handle);
	}
    }
}

/* Send ^C to target to halt it.  Target will respond, and send us a
   packet.  */

static void
arm_rdi_interrupt (signo)
     int signo;
{
}

static void (*ofunc)();

/* The user typed ^C twice.  */
static void
arm_rdi_interrupt_twice (signo)
     int signo;
{
}

/* Ask the user what to do when an interrupt is received.  */

static void
interrupt_query ()
{
}

/* Wait until the remote machine stops, then return, storing status in
   STATUS just as `wait' would.  Returns "pid" (though it's not clear
   what, if anything, that means in the case of this target).  */

static int
arm_rdi_wait (pid, status)
     int pid;
     struct target_waitstatus *status;
{
  status->kind = (execute_status == RDIError_NoError ?
    TARGET_WAITKIND_EXITED : TARGET_WAITKIND_STOPPED);

  /* convert stopped code from target into right signal */
  status->value.sig = rdi_error_signal (execute_status);

  return inferior_pid;
}

/* Read the remote registers into the block REGS.  */

/* ARGSUSED */
static void
arm_rdi_fetch_registers (regno)
     int regno;
{
  int rslt, rdi_regmask;
  unsigned long rawreg, rawregs[32];
  char cookedreg[4];

  if (regno == -1) 
    {
      rslt = angel_RDI_CPUread (255, 0x27fff, rawregs);
      if (rslt)
	{
	  printf_filtered ("RDI_CPUread: %s\n", rdi_error_message (rslt));
	}

      for (regno = 0; regno < 15; regno++)
	{
	  store_unsigned_integer (cookedreg, 4, rawregs[regno]);
	  supply_register (regno, (char *) cookedreg);
	}
      store_unsigned_integer (cookedreg, 4, rawregs[15]);
      supply_register (PS_REGNUM, (char *) cookedreg);
      arm_rdi_fetch_registers (PC_REGNUM);
    }
  else
    {
      if (regno == PC_REGNUM)
	rdi_regmask = RDIReg_PC;
      else if (regno == PS_REGNUM)
	rdi_regmask = RDIReg_CPSR;
      else if (regno < 0 || regno > 15)
	{
	  rawreg = 0;
	  supply_register (regno, (char *) &rawreg);
	  return;
	}
      else
	rdi_regmask = 1 << regno;

      rslt = angel_RDI_CPUread (255, rdi_regmask, &rawreg);
      if (rslt)
	{
	  printf_filtered ("RDI_CPUread: %s\n", rdi_error_message (rslt));
	}
      store_unsigned_integer (cookedreg, 4, rawreg);
      supply_register (regno, (char *) cookedreg);
    }
}

static void 
arm_rdi_prepare_to_store ()
{
  /* Nothing to do.  */
}

/* Store register REGNO, or all registers if REGNO == -1, from the contents
   of REGISTERS.  FIXME: ignores errors.  */

static void
arm_rdi_store_registers (regno)
     int regno;
{
  int rslt, rdi_regmask;

  /* These need to be able to take 'floating point register' contents */
  unsigned long rawreg[3], rawerreg[3];

  if (regno  == -1) 
    {
      for (regno = 0; regno < NUM_REGS; regno++)
	arm_rdi_store_registers (regno);
    }
  else
    {
      read_register_gen (regno, (char *) rawreg);
      /* RDI manipulates data in host byte order, so convert now. */
      store_unsigned_integer (rawerreg, 4, rawreg[0]);

      if (regno == PC_REGNUM)
	rdi_regmask = RDIReg_PC;
      else if (regno == PS_REGNUM)
	rdi_regmask = RDIReg_CPSR;
      else if (regno < 0 || regno > 15)
	return;
      else
	rdi_regmask = 1 << regno;

      rslt = angel_RDI_CPUwrite (255, rdi_regmask, rawerreg);
      if (rslt)
	{
	  printf_filtered ("RDI_CPUwrite: %s\n", rdi_error_message (rslt));
	}
    }
}

/* Read or write LEN bytes from inferior memory at MEMADDR,
   transferring to or from debugger address MYADDR.  Write to inferior
   if SHOULD_WRITE is nonzero.  Returns length of data written or
   read; 0 for error.  */

/* ARGSUSED */
static int
arm_rdi_xfer_memory(memaddr, myaddr, len, should_write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int should_write;
     struct target_ops *target;			/* ignored */
{
  int rslt, i;

  if (should_write)
    {
      rslt = angel_RDI_write (myaddr, memaddr, &len);
      if (rslt)
	{
	  printf_filtered ("RDI_write: %s\n", rdi_error_message (rslt));
	}
    }
  else 
    {
      rslt = angel_RDI_read (memaddr, myaddr, &len);
      if (rslt)
	{
	  printf_filtered ("RDI_read: %s\n", rdi_error_message (rslt));
	  len = 0;
	}
    }
  return len;
}

/* Display random info collected from the target.  */

static void
arm_rdi_files_info (ignore)
     struct target_ops *ignore;
{
  char *file = "nothing";
  int rslt;
  unsigned long arg1, arg2;

  rslt = angel_RDI_info (RDIInfo_Target, &arg1, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }
  if (arg1 & (1 << 15))
    printf_filtered ("Target supports Thumb code.\n");
  if (arg1 & (1 << 14))
    printf_filtered ("Target can do profiling.\n");
  if (arg1 & (1 << 4))
    printf_filtered ("Target is real hardware.\n");
  
  rslt = angel_RDI_info (RDIInfo_Step, &arg1, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }
  printf_filtered ("Target can%s single-step.\n", (arg1 & 0x4 ? "" : "not"));

  rslt = angel_RDI_info (RDIInfo_Icebreaker, &arg1, &arg2);
  if (rslt)
    {
      printf_filtered ("RDI_info: %s\n", rdi_error_message (rslt));
    }
  else
    printf_filtered ("Target includes an EmbeddedICE.\n");
}

static void
arm_rdi_kill ()
{
  int rslt;

  rslt = angel_RDI_open (1, &gdb_config, NULL, NULL);
  if (rslt)
    {
      printf_filtered ("RDI_open: %s\n", rdi_error_message (rslt));
    }
}

static void
arm_rdi_mourn_inferior ()
{
  unpush_target (&arm_rdi_ops);
  generic_mourn_inferior ();
}

/* While the RDI library keeps track of its own breakpoints, we need
   to remember "handles" so that we can delete them later.  Since
   breakpoints get used for stepping, be careful not to leak memory
   here.  */

static int
arm_rdi_insert_breakpoint (addr, contents_cache)
     CORE_ADDR addr;
     char *contents_cache;
{
  int rslt;
  PointHandle point;
  struct local_bp_list_entry *entry;
  int type = RDIPoint_EQ;

  if (arm_pc_is_thumb (addr) || arm_pc_is_thumb_dummy (addr))
    type |= RDIPoint_16Bit;
  rslt = angel_RDI_setbreak (addr, type, 0, &point);
  if (rslt)
    {
      printf_filtered ("RDI_setbreak: %s\n", rdi_error_message (rslt));
    }
  entry =
    (struct local_bp_list_entry *) xmalloc (sizeof (struct local_bp_list_entry));
  entry->addr = addr;
  entry->point = point;
  entry->next = local_bp_list;
  local_bp_list = entry;
  return rslt;
}

static int
arm_rdi_remove_breakpoint (addr, contents_cache)
     CORE_ADDR addr;
     char *contents_cache;
{
  int rslt;
  PointHandle point;
  struct local_bp_list_entry *entry, *preventry;

  for (entry = local_bp_list; entry != NULL; entry = entry->next)
    {
      if (entry->addr == addr)
	{
	  break;
	}
      preventry = entry;
    }
  if (entry)
    {
      rslt = angel_RDI_clearbreak (entry->point);
      if (rslt)
	{
	  printf_filtered ("RDI_clearbreak: %s\n", rdi_error_message (rslt));
	}
      /* Delete the breakpoint entry locally.  */
      if (entry == local_bp_list)
	{
	  local_bp_list = entry->next;
	}
      else
	{
	  preventry->next = entry->next;
	}
      free (entry);
    }
  return 0;
}

static char *
rdi_error_message (err)
     int err;
{
  switch (err)
    {
    case RDIError_NoError: 
      return "no error";
    case RDIError_Reset:
      return "debuggee reset";
    case RDIError_UndefinedInstruction:
      return "undefined instruction";
    case RDIError_SoftwareInterrupt:
      return "SWI trapped";
    case RDIError_PrefetchAbort:
      return "prefetch abort, execution ran into unmapped memory?";
    case RDIError_DataAbort:
      return "data abort, no memory at specified address?";
    case RDIError_AddressException:
      return "address exception, access >26bit in 26bit mode";
    case RDIError_IRQ:
      return "IRQ, interrupt trapped";
    case RDIError_FIQ:
      return "FIQ, fast interrupt trapped";
    case RDIError_Error:
      return "a miscellaneous type of error";
    case RDIError_BranchThrough0:
      return "branch through location 0";
    case RDIError_NotInitialised:
      return "internal error, RDI_open not called first";
    case RDIError_UnableToInitialise:
      return "internal error, target world is broken";
    case RDIError_WrongByteSex:
      return "See Operator: WrongByteSex";
    case RDIError_UnableToTerminate:
      return "See Operator: Unable to Terminate";
    case RDIError_BadInstruction:
      return "bad instruction, illegal to execute this instruction";
    case RDIError_IllegalInstruction:
      return "illegal instruction, the effect of executing it is undefined";
    case RDIError_BadCPUStateSetting:
      return "internal error, tried to set SPSR of user mode";
    case RDIError_UnknownCoPro:
      return "unknown co-processor";
    case RDIError_UnknownCoProState:
      return "cannot execute co-processor request";
    case RDIError_BadCoProState:
      return "recognizably broken co-processor request";
    case RDIError_BadPointType:
      return "internal error, bad point yype";
    case RDIError_UnimplementedType:
      return "internal error, unimplemented type";
    case RDIError_BadPointSize:
      return "internal error, bad point size";
    case RDIError_UnimplementedSize:
      return "internal error, unimplemented size";
    case RDIError_NoMorePoints:
      return "last break/watch point was used";
    case RDIError_BreakpointReached:
      return "breakpoint reached";
    case RDIError_WatchpointAccessed:
      return "watchpoint accessed";
    case RDIError_NoSuchPoint:
      return "attempted to clear non-existent break/watch point";
    case RDIError_ProgramFinishedInStep:
      return "end of the program reached while stepping";
    case RDIError_UserInterrupt:
      return "you pressed Escape";
    case RDIError_CantSetPoint:
      return "no more break/watch points available";
    case RDIError_IncompatibleRDILevels:
      return "incompatible RDI levels";
    case RDIError_LittleEndian:
      return "debuggee is little endian";
    case RDIError_BigEndian:
      return "debuggee is big endian";
    case RDIError_SoftInitialiseError:
      return "recoverable error in RDI initialization";
    case RDIError_InsufficientPrivilege:
      return "internal error, supervisor state not accessible to monitor";
    case RDIError_UnimplementedMessage:
      return "internal error, unimplemented message";
    case RDIError_UndefinedMessage:
      return "internal error, undefined message";
    default:
      return "undefined error message, should reset target"; 
    }
}

/* Convert the ARM error messages to signals that GDB knows about.  */

static enum target_signal
rdi_error_signal (err)
     int err;
{
  switch (err)
    {
    case RDIError_NoError:
      return 0;
    case RDIError_Reset:
      return TARGET_SIGNAL_TERM; /* ??? */
    case RDIError_UndefinedInstruction:
      return TARGET_SIGNAL_ILL;
    case RDIError_SoftwareInterrupt:
    case RDIError_PrefetchAbort:
    case RDIError_DataAbort:
      return TARGET_SIGNAL_TRAP;
    case RDIError_AddressException:
      return TARGET_SIGNAL_SEGV;
    case RDIError_IRQ:
    case RDIError_FIQ:
      return TARGET_SIGNAL_TRAP;
    case RDIError_Error:
      return TARGET_SIGNAL_TERM;
    case RDIError_BranchThrough0:
      return TARGET_SIGNAL_TRAP;
    case RDIError_NotInitialised:
    case RDIError_UnableToInitialise:
    case RDIError_WrongByteSex:
    case RDIError_UnableToTerminate:
      return TARGET_SIGNAL_UNKNOWN;
    case RDIError_BadInstruction:
    case RDIError_IllegalInstruction:
      return TARGET_SIGNAL_ILL;
    case RDIError_BadCPUStateSetting:
    case RDIError_UnknownCoPro:
    case RDIError_UnknownCoProState:
    case RDIError_BadCoProState:
    case RDIError_BadPointType:
    case RDIError_UnimplementedType:
    case RDIError_BadPointSize:
    case RDIError_UnimplementedSize:
    case RDIError_NoMorePoints:
      return TARGET_SIGNAL_UNKNOWN;
    case RDIError_BreakpointReached:
    case RDIError_WatchpointAccessed:
      return TARGET_SIGNAL_TRAP;
    case RDIError_NoSuchPoint:
    case RDIError_ProgramFinishedInStep:
      return TARGET_SIGNAL_UNKNOWN;
    case RDIError_UserInterrupt:
      return TARGET_SIGNAL_INT;
    case RDIError_IncompatibleRDILevels:
    case RDIError_LittleEndian:
    case RDIError_BigEndian:
    case RDIError_SoftInitialiseError:
    case RDIError_InsufficientPrivilege:
    case RDIError_UnimplementedMessage:
    case RDIError_UndefinedMessage:
    default:
      return TARGET_SIGNAL_UNKNOWN; 
    }
}

/* Define the target operations structure.  */

static void
init_rdi_ops ()
{
  arm_rdi_ops.to_shortname = "rdi";	
  arm_rdi_ops.to_longname = "ARM RDI";
  arm_rdi_ops.to_doc = "Use a remote ARM-based computer; via the RDI library.\n\
Specify the serial device it is connected to (e.g. /dev/ttya)." ; 
  arm_rdi_ops.to_open = arm_rdi_open;		
  arm_rdi_ops.to_close = arm_rdi_close;	
  arm_rdi_ops.to_detach = arm_rdi_detach;	
  arm_rdi_ops.to_resume = arm_rdi_resume;	
  arm_rdi_ops.to_wait = arm_rdi_wait;	
  arm_rdi_ops.to_fetch_registers = arm_rdi_fetch_registers;
  arm_rdi_ops.to_store_registers = arm_rdi_store_registers;
  arm_rdi_ops.to_prepare_to_store = arm_rdi_prepare_to_store;
  arm_rdi_ops.to_xfer_memory = arm_rdi_xfer_memory;
  arm_rdi_ops.to_files_info = arm_rdi_files_info;
  arm_rdi_ops.to_insert_breakpoint = arm_rdi_insert_breakpoint;
  arm_rdi_ops.to_remove_breakpoint = arm_rdi_remove_breakpoint;	
  arm_rdi_ops.to_kill = arm_rdi_kill;		
  arm_rdi_ops.to_load = generic_load;		
  arm_rdi_ops.to_create_inferior = arm_rdi_create_inferior;
  arm_rdi_ops.to_mourn_inferior = arm_rdi_mourn_inferior;
  arm_rdi_ops.to_stratum = process_stratum;
  arm_rdi_ops.to_has_all_memory = 1;	
  arm_rdi_ops.to_has_memory = 1;	
  arm_rdi_ops.to_has_stack = 1;	
  arm_rdi_ops.to_has_registers = 1;	
  arm_rdi_ops.to_has_execution = 1;	
  arm_rdi_ops.to_magic = OPS_MAGIC;	
}

void
_initialize_remote_rdi ()
{
  init_rdi_ops () ;
  add_target (&arm_rdi_ops);
}

/* A little dummy to make linking with the library succeed. */

int Fail() { return 0; }
