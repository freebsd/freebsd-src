/* OBSOLETE /* Remote debugging interface for AMD 29k interfaced via UDI, for GDB. */
/* OBSOLETE    Copyright 1990, 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000, 2001 */
/* OBSOLETE    Free Software Foundation, Inc. */
/* OBSOLETE    Written by Daniel Mann.  Contributed by AMD. */
/* OBSOLETE  */
/* OBSOLETE    This file is part of GDB. */
/* OBSOLETE  */
/* OBSOLETE    This program is free software; you can redistribute it and/or modify */
/* OBSOLETE    it under the terms of the GNU General Public License as published by */
/* OBSOLETE    the Free Software Foundation; either version 2 of the License, or */
/* OBSOLETE    (at your option) any later version. */
/* OBSOLETE  */
/* OBSOLETE    This program is distributed in the hope that it will be useful, */
/* OBSOLETE    but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* OBSOLETE    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* OBSOLETE    GNU General Public License for more details. */
/* OBSOLETE  */
/* OBSOLETE    You should have received a copy of the GNU General Public License */
/* OBSOLETE    along with this program; if not, write to the Free Software */
/* OBSOLETE    Foundation, Inc., 59 Temple Place - Suite 330, */
/* OBSOLETE    Boston, MA 02111-1307, USA.  */ */
/* OBSOLETE  */
/* OBSOLETE /* This is like remote.c but uses the Universal Debug Interface (UDI) to  */
/* OBSOLETE    talk to the target hardware (or simulator).  UDI is a TCP/IP based */
/* OBSOLETE    protocol; for hardware that doesn't run TCP, an interface adapter  */
/* OBSOLETE    daemon talks UDI on one side, and talks to the hardware (typically */
/* OBSOLETE    over a serial port) on the other side. */
/* OBSOLETE  */
/* OBSOLETE    - Originally written by Daniel Mann at AMD for MiniMON and gdb 3.91.6. */
/* OBSOLETE    - David Wood (wood@lab.ultra.nyu.edu) at New York University adapted this */
/* OBSOLETE    file to gdb 3.95.  I was unable to get this working on sun3os4 */
/* OBSOLETE    with termio, only with sgtty. */
/* OBSOLETE    - Daniel Mann at AMD took the 3.95 adaptions above and replaced */
/* OBSOLETE    MiniMON interface with UDI-p interface.        */ */
/* OBSOLETE  */
/* OBSOLETE #include "defs.h" */
/* OBSOLETE #include "frame.h" */
/* OBSOLETE #include "inferior.h" */
/* OBSOLETE #include "value.h" */
/* OBSOLETE #include <ctype.h> */
/* OBSOLETE #include <fcntl.h> */
/* OBSOLETE #include <errno.h> */
/* OBSOLETE #include "gdb_string.h" */
/* OBSOLETE #include "terminal.h" */
/* OBSOLETE #include "target.h" */
/* OBSOLETE #include "29k-share/udi/udiproc.h" */
/* OBSOLETE #include "gdbcmd.h" */
/* OBSOLETE #include "bfd.h" */
/* OBSOLETE #include "gdbcore.h"		/* For download function */ */
/* OBSOLETE #include "regcache.h" */
/* OBSOLETE  */
/* OBSOLETE /* access the register store directly, without going through */
/* OBSOLETE    the normal handler functions. This avoids an extra data copy.  */ */
/* OBSOLETE  */
/* OBSOLETE extern int stop_soon_quietly;	/* for wait_for_inferior */ */
/* OBSOLETE extern struct value *call_function_by_hand (); */
/* OBSOLETE static void udi_resume (ptid_t ptid, int step, enum target_signal sig); */
/* OBSOLETE static void udi_fetch_registers (int regno); */
/* OBSOLETE static void udi_load (char *args, int from_tty); */
/* OBSOLETE static void fetch_register (int regno); */
/* OBSOLETE static void udi_store_registers (int regno); */
/* OBSOLETE static int store_register (int regno); */
/* OBSOLETE static int regnum_to_srnum (int regno); */
/* OBSOLETE static void udi_close (int quitting); */
/* OBSOLETE static CPUSpace udi_memory_space (CORE_ADDR addr); */
/* OBSOLETE static int udi_write_inferior_memory (CORE_ADDR memaddr, char *myaddr, */
/* OBSOLETE 				      int len); */
/* OBSOLETE static int udi_read_inferior_memory (CORE_ADDR memaddr, char *myaddr, */
/* OBSOLETE 				     int len); */
/* OBSOLETE static void download (char *load_arg_string, int from_tty); */
/* OBSOLETE char CoffFileName[100] = ""; */
/* OBSOLETE  */
/* OBSOLETE #define FREEZE_MODE     (read_register(CPS_REGNUM) & 0x400) */
/* OBSOLETE #define USE_SHADOW_PC	((processor_type == a29k_freeze_mode) && FREEZE_MODE) */
/* OBSOLETE  */
/* OBSOLETE static int timeout = 5; */
/* OBSOLETE extern struct target_ops udi_ops;	/* Forward declaration */ */
/* OBSOLETE  */
/* OBSOLETE /* Special register enumeration. */
/* OBSOLETE  */ */
/* OBSOLETE  */
/* OBSOLETE /******************************************************************* UDI DATA*/ */
/* OBSOLETE #define	MAXDATA		2*1024	/* max UDI[read/write] byte size */ */
/* OBSOLETE /* Descriptor for I/O to remote machine.  Initialize it to -1 so that */
/* OBSOLETE    udi_open knows that we don't have a file open when the program */
/* OBSOLETE    starts.  */ */
/* OBSOLETE  */
/* OBSOLETE UDISessionId udi_session_id = -1; */
/* OBSOLETE static char *udi_config_id; */
/* OBSOLETE  */
/* OBSOLETE CPUOffset IMemStart = 0; */
/* OBSOLETE CPUSizeT IMemSize = 0; */
/* OBSOLETE CPUOffset DMemStart = 0; */
/* OBSOLETE CPUSizeT DMemSize = 0; */
/* OBSOLETE CPUOffset RMemStart = 0; */
/* OBSOLETE CPUSizeT RMemSize = 0; */
/* OBSOLETE UDIUInt32 CPUPRL; */
/* OBSOLETE UDIUInt32 CoProcPRL; */
/* OBSOLETE  */
/* OBSOLETE UDIMemoryRange address_ranges[2];	/* Text and data */ */
/* OBSOLETE UDIResource entry = */
/* OBSOLETE {0, 0};				/* Entry point */ */
/* OBSOLETE CPUSizeT stack_sizes[2];	/* Regular and memory stacks */ */
/* OBSOLETE  */
/* OBSOLETE #define	SBUF_MAX	1024	/* maximum size of string handling buffer */ */
/* OBSOLETE char sbuf[SBUF_MAX]; */
/* OBSOLETE  */
/* OBSOLETE typedef struct bkpt_entry_str */
/* OBSOLETE   { */
/* OBSOLETE     UDIResource Addr; */
/* OBSOLETE     UDIUInt32 PassCount; */
/* OBSOLETE     UDIBreakType Type; */
/* OBSOLETE     unsigned int BreakId; */
/* OBSOLETE   } */
/* OBSOLETE bkpt_entry_t; */
/* OBSOLETE #define		BKPT_TABLE_SIZE 40 */
/* OBSOLETE static bkpt_entry_t bkpt_table[BKPT_TABLE_SIZE]; */
/* OBSOLETE extern char dfe_errmsg[];	/* error string */ */
/* OBSOLETE  */
/* OBSOLETE /* malloc'd name of the program on the remote system.  */ */
/* OBSOLETE static char *prog_name = NULL; */
/* OBSOLETE  */
/* OBSOLETE /* This is called not only when we first attach, but also when the */
/* OBSOLETE    user types "run" after having attached.  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE udi_create_inferior (char *execfile, char *args, char **env) */
/* OBSOLETE { */
/* OBSOLETE   char *args1; */
/* OBSOLETE  */
/* OBSOLETE   if (execfile) */
/* OBSOLETE     { */
/* OBSOLETE       if (prog_name != NULL) */
/* OBSOLETE 	xfree (prog_name); */
/* OBSOLETE       prog_name = savestring (execfile, strlen (execfile)); */
/* OBSOLETE     } */
/* OBSOLETE   else if (entry.Offset) */
/* OBSOLETE     execfile = ""; */
/* OBSOLETE   else */
/* OBSOLETE     error ("No image loaded into target."); */
/* OBSOLETE  */
/* OBSOLETE   if (udi_session_id < 0) */
/* OBSOLETE     { */
/* OBSOLETE       /* If the TIP is not open, open it.  */ */
/* OBSOLETE       if (UDIConnect (udi_config_id, &udi_session_id)) */
/* OBSOLETE 	error ("UDIConnect() failed: %s\n", dfe_errmsg); */
/* OBSOLETE       /* We will need to download the program.  */ */
/* OBSOLETE       entry.Offset = 0; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   inferior_ptid = pid_to_ptid (40000); */
/* OBSOLETE  */
/* OBSOLETE   if (!entry.Offset) */
/* OBSOLETE     download (execfile, 0); */
/* OBSOLETE  */
/* OBSOLETE   args1 = alloca (strlen (execfile) + strlen (args) + 2); */
/* OBSOLETE  */
/* OBSOLETE   if (execfile[0] == '\0') */
/* OBSOLETE  */
/* OBSOLETE     /* It is empty.  We need to quote it somehow, or else the target */
/* OBSOLETE        will think there is no argument being passed here.  According */
/* OBSOLETE        to the UDI spec it is quoted "according to TIP OS rules" which */
/* OBSOLETE        I guess means quoting it like the Unix shell should work */
/* OBSOLETE        (sounds pretty bogus to me...).  In fact it doesn't work (with */
/* OBSOLETE        isstip anyway), but passing in two quotes as the argument seems */
/* OBSOLETE        like a reasonable enough behavior anyway (I guess).  */ */
/* OBSOLETE  */
/* OBSOLETE     strcpy (args1, "''"); */
/* OBSOLETE   else */
/* OBSOLETE     strcpy (args1, execfile); */
/* OBSOLETE   strcat (args1, " "); */
/* OBSOLETE   strcat (args1, args); */
/* OBSOLETE  */
/* OBSOLETE   UDIInitializeProcess (address_ranges,		/* ProcessMemory[] */ */
/* OBSOLETE 			(UDIInt) 2,	/* NumberOfRanges */ */
/* OBSOLETE 			entry,	/* EntryPoint */ */
/* OBSOLETE 			stack_sizes,	/* *StackSizes */ */
/* OBSOLETE 			(UDIInt) 2,	/* NumberOfStacks */ */
/* OBSOLETE 			args1);	/* ArgString */ */
/* OBSOLETE  */
/* OBSOLETE   init_wait_for_inferior (); */
/* OBSOLETE   clear_proceed_status (); */
/* OBSOLETE   proceed (-1, TARGET_SIGNAL_DEFAULT, 0); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE udi_mourn (void) */
/* OBSOLETE { */
/* OBSOLETE #if 0 */
/* OBSOLETE   /* Requiring "target udi" each time you run is a major pain.  I suspect */
/* OBSOLETE      this was just blindy copied from remote.c, in which "target" and */
/* OBSOLETE      "run" are combined.  Having a udi target without an inferior seems */
/* OBSOLETE      to work between "target udi" and "run", so why not now?  */ */
/* OBSOLETE   pop_target ();		/* Pop back to no-child state */ */
/* OBSOLETE #endif */
/* OBSOLETE   /* But if we're going to want to run it again, we better remove the */
/* OBSOLETE      breakpoints...  */ */
/* OBSOLETE   remove_breakpoints (); */
/* OBSOLETE   generic_mourn_inferior (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /******************************************************************** UDI_OPEN */
/* OBSOLETE ** Open a connection to remote TIP. */
/* OBSOLETE    NAME is the socket domain used for communication with the TIP, */
/* OBSOLETE    then a space and the socket name or TIP-host name. */
/* OBSOLETE    '<udi_udi_config_id>' for example. */
/* OBSOLETE  */ */
/* OBSOLETE  */
/* OBSOLETE /* XXX - need cleanups for udiconnect for various failures!!! */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE udi_open (char *name, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   unsigned int prl; */
/* OBSOLETE   char *p; */
/* OBSOLETE   int cnt; */
/* OBSOLETE   UDIMemoryRange KnownMemory[10]; */
/* OBSOLETE   UDIUInt32 ChipVersions[10]; */
/* OBSOLETE   UDIInt NumberOfRanges = 10; */
/* OBSOLETE   UDIInt NumberOfChips = 10; */
/* OBSOLETE   UDIPId PId; */
/* OBSOLETE   UDIUInt32 TIPId, TargetId, DFEId, DFE, TIP, DFEIPCId, TIPIPCId; */
/* OBSOLETE  */
/* OBSOLETE   target_preopen (from_tty); */
/* OBSOLETE  */
/* OBSOLETE   entry.Offset = 0; */
/* OBSOLETE  */
/* OBSOLETE   for (cnt = 0; cnt < BKPT_TABLE_SIZE; cnt++) */
/* OBSOLETE     bkpt_table[cnt].Type = 0; */
/* OBSOLETE  */
/* OBSOLETE   if (udi_config_id) */
/* OBSOLETE     xfree (udi_config_id); */
/* OBSOLETE  */
/* OBSOLETE   if (!name) */
/* OBSOLETE     error ("Usage: target udi config_id, where config_id appears in udi_soc file"); */
/* OBSOLETE  */
/* OBSOLETE   udi_config_id = xstrdup (strtok (name, " \t")); */
/* OBSOLETE  */
/* OBSOLETE   if (UDIConnect (udi_config_id, &udi_session_id)) */
/* OBSOLETE     /* FIXME: Should set udi_session_id to -1 here.  */ */
/* OBSOLETE     error ("UDIConnect() failed: %s\n", dfe_errmsg); */
/* OBSOLETE  */
/* OBSOLETE   push_target (&udi_ops); */
/* OBSOLETE  */
/* OBSOLETE   /* */
/* OBSOLETE      ** Initialize target configuration structure (global) */
/* OBSOLETE    */ */
/* OBSOLETE   if (UDIGetTargetConfig (KnownMemory, &NumberOfRanges, */
/* OBSOLETE 			  ChipVersions, &NumberOfChips)) */
/* OBSOLETE     error ("UDIGetTargetConfig() failed"); */
/* OBSOLETE   if (NumberOfChips > 2) */
/* OBSOLETE     fprintf_unfiltered (gdb_stderr, "Target has more than one processor\n"); */
/* OBSOLETE   for (cnt = 0; cnt < NumberOfRanges; cnt++) */
/* OBSOLETE     { */
/* OBSOLETE       switch (KnownMemory[cnt].Space) */
/* OBSOLETE 	{ */
/* OBSOLETE 	default: */
/* OBSOLETE 	  fprintf_unfiltered (gdb_stderr, "UDIGetTargetConfig() unknown memory space\n"); */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case UDI29KCP_S: */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case UDI29KIROMSpace: */
/* OBSOLETE 	  RMemStart = KnownMemory[cnt].Offset; */
/* OBSOLETE 	  RMemSize = KnownMemory[cnt].Size; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case UDI29KIRAMSpace: */
/* OBSOLETE 	  IMemStart = KnownMemory[cnt].Offset; */
/* OBSOLETE 	  IMemSize = KnownMemory[cnt].Size; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case UDI29KDRAMSpace: */
/* OBSOLETE 	  DMemStart = KnownMemory[cnt].Offset; */
/* OBSOLETE 	  DMemSize = KnownMemory[cnt].Size; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   a29k_get_processor_type (); */
/* OBSOLETE  */
/* OBSOLETE   if (UDICreateProcess (&PId)) */
/* OBSOLETE     fprintf_unfiltered (gdb_stderr, "UDICreateProcess() failed\n"); */
/* OBSOLETE  */
/* OBSOLETE   /* Print out some stuff, letting the user now what's going on */ */
/* OBSOLETE   if (UDICapabilities (&TIPId, &TargetId, DFEId, DFE, &TIP, &DFEIPCId, */
/* OBSOLETE 		       &TIPIPCId, sbuf)) */
/* OBSOLETE     error ("UDICapabilities() failed"); */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     { */
/* OBSOLETE       printf_filtered ("Connected via UDI socket,\n\ */
/* OBSOLETE  DFE-IPC version %x.%x.%x  TIP-IPC version %x.%x.%x  TIP version %x.%x.%x\n %s\n", */
/* OBSOLETE 	       (DFEIPCId >> 8) & 0xf, (DFEIPCId >> 4) & 0xf, DFEIPCId & 0xf, */
/* OBSOLETE 	       (TIPIPCId >> 8) & 0xf, (TIPIPCId >> 4) & 0xf, TIPIPCId & 0xf, */
/* OBSOLETE 	       (TargetId >> 8) & 0xf, (TargetId >> 4) & 0xf, TargetId & 0xf, */
/* OBSOLETE 		       sbuf); */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /******************************************************************* UDI_CLOSE */
/* OBSOLETE    Close the open connection to the TIP process. */
/* OBSOLETE    Use this when you want to detach and do something else */
/* OBSOLETE    with your gdb.  */ */
/* OBSOLETE static void */
/* OBSOLETE udi_close (			/*FIXME: how is quitting used */ */
/* OBSOLETE 	    int quitting) */
/* OBSOLETE { */
/* OBSOLETE   if (udi_session_id < 0) */
/* OBSOLETE     return; */
/* OBSOLETE  */
/* OBSOLETE   /* We should never get here if there isn't something valid in */
/* OBSOLETE      udi_session_id.  */ */
/* OBSOLETE  */
/* OBSOLETE   if (UDIDisconnect (udi_session_id, UDITerminateSession)) */
/* OBSOLETE     { */
/* OBSOLETE       if (quitting) */
/* OBSOLETE 	warning ("UDIDisconnect() failed in udi_close"); */
/* OBSOLETE       else */
/* OBSOLETE 	error ("UDIDisconnect() failed in udi_close"); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* Do not try to close udi_session_id again, later in the program.  */ */
/* OBSOLETE   udi_session_id = -1; */
/* OBSOLETE   inferior_ptid = null_ptid; */
/* OBSOLETE  */
/* OBSOLETE   printf_filtered ("  Ending remote debugging\n"); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /**************************************************************** UDI_ATACH */ */
/* OBSOLETE /* Attach to a program that is already loaded and running  */
/* OBSOLETE  * Upon exiting the process's execution is stopped. */
/* OBSOLETE  */ */
/* OBSOLETE static void */
/* OBSOLETE udi_attach (char *args, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   UDIResource From; */
/* OBSOLETE   UDIInt32 PC_adds; */
/* OBSOLETE   UDICount Count = 1; */
/* OBSOLETE   UDISizeT Size = 4; */
/* OBSOLETE   UDICount CountDone; */
/* OBSOLETE   UDIBool HostEndian = 0; */
/* OBSOLETE   UDIError err; */
/* OBSOLETE  */
/* OBSOLETE   if (args == NULL) */
/* OBSOLETE     error_no_arg ("program to attach"); */
/* OBSOLETE  */
/* OBSOLETE   if (udi_session_id < 0) */
/* OBSOLETE     error ("UDI connection not opened yet, use the 'target udi' command.\n"); */
/* OBSOLETE  */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     printf_unfiltered ("Attaching to remote program %s...\n", prog_name); */
/* OBSOLETE  */
/* OBSOLETE   UDIStop (); */
/* OBSOLETE   From.Space = UDI29KSpecialRegs; */
/* OBSOLETE   From.Offset = 11; */
/* OBSOLETE   if (err = UDIRead (From, &PC_adds, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIRead failed in udi_attach"); */
/* OBSOLETE   printf_unfiltered ("Remote process is now halted, pc1 = 0x%x.\n", PC_adds); */
/* OBSOLETE } */
/* OBSOLETE /************************************************************* UDI_DETACH */ */
/* OBSOLETE /* Terminate the open connection to the TIP process. */
/* OBSOLETE    Use this when you want to detach and do something else */
/* OBSOLETE    with your gdb.  Leave remote process running (with no breakpoints set). */ */
/* OBSOLETE static void */
/* OBSOLETE udi_detach (char *args, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE  */
/* OBSOLETE   remove_breakpoints ();	/* Just in case there were any left in */ */
/* OBSOLETE  */
/* OBSOLETE   if (UDIDisconnect (udi_session_id, UDIContinueSession)) */
/* OBSOLETE     error ("UDIDisconnect() failed in udi_detach"); */
/* OBSOLETE  */
/* OBSOLETE   /* Don't try to UDIDisconnect it again in udi_close, which is called from */
/* OBSOLETE      pop_target.  */ */
/* OBSOLETE   udi_session_id = -1; */
/* OBSOLETE   inferior_ptid = null_ptid; */
/* OBSOLETE  */
/* OBSOLETE   pop_target (); */
/* OBSOLETE  */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     printf_unfiltered ("Detaching from TIP\n"); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /****************************************************************** UDI_RESUME */
/* OBSOLETE ** Tell the remote machine to resume.  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE udi_resume (ptid_t ptid, int step, enum target_signal sig) */
/* OBSOLETE { */
/* OBSOLETE   UDIError tip_error; */
/* OBSOLETE   UDIUInt32 Steps = 1; */
/* OBSOLETE   UDIStepType StepType = UDIStepNatural; */
/* OBSOLETE   UDIRange Range; */
/* OBSOLETE  */
/* OBSOLETE   if (step)			/* step 1 instruction */ */
/* OBSOLETE     { */
/* OBSOLETE       tip_error = UDIStep (Steps, StepType, Range); */
/* OBSOLETE       if (!tip_error) */
/* OBSOLETE 	return; */
/* OBSOLETE  */
/* OBSOLETE       fprintf_unfiltered (gdb_stderr, "UDIStep() error = %d\n", tip_error); */
/* OBSOLETE       error ("failed in udi_resume"); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   if (UDIExecute ()) */
/* OBSOLETE     error ("UDIExecute() failed in udi_resume"); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /******************************************************************** UDI_WAIT */
/* OBSOLETE ** Wait until the remote machine stops, then return, */
/* OBSOLETE    storing status in STATUS just as `wait' would.  */ */
/* OBSOLETE  */
/* OBSOLETE static ptid_t */
/* OBSOLETE udi_wait (ptid_t ptid, struct target_waitstatus *status) */
/* OBSOLETE { */
/* OBSOLETE   UDIInt32 MaxTime; */
/* OBSOLETE   UDIPId PId; */
/* OBSOLETE   UDIInt32 StopReason; */
/* OBSOLETE   UDISizeT CountDone; */
/* OBSOLETE   int old_timeout = timeout; */
/* OBSOLETE   int old_immediate_quit = immediate_quit; */
/* OBSOLETE   int i; */
/* OBSOLETE  */
/* OBSOLETE   status->kind = TARGET_WAITKIND_EXITED; */
/* OBSOLETE   status->value.integer = 0; */
/* OBSOLETE  */
/* OBSOLETE /* wait for message to arrive. It should be: */
/* OBSOLETE    If the target stops executing, udi_wait() should return. */
/* OBSOLETE  */ */
/* OBSOLETE   timeout = 0;			/* Wait indefinetly for a message */ */
/* OBSOLETE   immediate_quit = 1;		/* Helps ability to QUIT */ */
/* OBSOLETE  */
/* OBSOLETE   while (1) */
/* OBSOLETE     { */
/* OBSOLETE       i = 0; */
/* OBSOLETE       MaxTime = UDIWaitForever; */
/* OBSOLETE       UDIWait (MaxTime, &PId, &StopReason); */
/* OBSOLETE       QUIT;			/* Let user quit if they want */ */
/* OBSOLETE  */
/* OBSOLETE       switch (StopReason & UDIGrossState) */
/* OBSOLETE 	{ */
/* OBSOLETE 	case UDIStdoutReady: */
/* OBSOLETE 	  if (UDIGetStdout (sbuf, (UDISizeT) SBUF_MAX, &CountDone)) */
/* OBSOLETE 	    /* This is said to happen if the program tries to output */
/* OBSOLETE 	       a whole bunch of output (more than SBUF_MAX, I would */
/* OBSOLETE 	       guess).  It doesn't seem to happen with the simulator.  */ */
/* OBSOLETE 	    warning ("UDIGetStdout() failed in udi_wait"); */
/* OBSOLETE 	  fwrite (sbuf, 1, CountDone, stdout); */
/* OBSOLETE 	  gdb_flush (gdb_stdout); */
/* OBSOLETE 	  continue; */
/* OBSOLETE  */
/* OBSOLETE 	case UDIStderrReady: */
/* OBSOLETE 	  UDIGetStderr (sbuf, (UDISizeT) SBUF_MAX, &CountDone); */
/* OBSOLETE 	  fwrite (sbuf, 1, CountDone, stderr); */
/* OBSOLETE 	  gdb_flush (gdb_stderr); */
/* OBSOLETE 	  continue; */
/* OBSOLETE  */
/* OBSOLETE 	case UDIStdinNeeded: */
/* OBSOLETE 	  { */
/* OBSOLETE 	    int ch; */
/* OBSOLETE 	    i = 0; */
/* OBSOLETE 	    do */
/* OBSOLETE 	      { */
/* OBSOLETE 		ch = getchar (); */
/* OBSOLETE 		if (ch == EOF) */
/* OBSOLETE 		  break; */
/* OBSOLETE 		sbuf[i++] = ch; */
/* OBSOLETE 	      } */
/* OBSOLETE 	    while (i < SBUF_MAX && ch != '\n'); */
/* OBSOLETE 	    UDIPutStdin (sbuf, (UDISizeT) i, &CountDone); */
/* OBSOLETE 	    continue; */
/* OBSOLETE 	  } */
/* OBSOLETE  */
/* OBSOLETE 	case UDIRunning: */
/* OBSOLETE 	  /* In spite of the fact that we told UDIWait to wait forever, it will */
/* OBSOLETE 	     return spuriously sometimes.  */ */
/* OBSOLETE 	case UDIStdinModeX: */
/* OBSOLETE 	  continue; */
/* OBSOLETE 	default: */
/* OBSOLETE 	  break; */
/* OBSOLETE 	} */
/* OBSOLETE       break; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   switch (StopReason & UDIGrossState) */
/* OBSOLETE     { */
/* OBSOLETE     case UDITrapped: */
/* OBSOLETE       printf_unfiltered ("Am290*0 received vector number %d\n", StopReason >> 24); */
/* OBSOLETE  */
/* OBSOLETE       switch ((StopReason >> 8) & 0xff) */
/* OBSOLETE 	{ */
/* OBSOLETE 	case 0:		/* Illegal opcode */ */
/* OBSOLETE 	  printf_unfiltered ("	(break point)\n"); */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE 	  status->value.sig = TARGET_SIGNAL_TRAP; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case 1:		/* Unaligned Access */ */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE 	  status->value.sig = TARGET_SIGNAL_BUS; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case 3: */
/* OBSOLETE 	case 4: */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE 	  status->value.sig = TARGET_SIGNAL_FPE; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case 5:		/* Protection Violation */ */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE 	  /* Why not SEGV?  What is a Protection Violation?  */ */
/* OBSOLETE 	  status->value.sig = TARGET_SIGNAL_ILL; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case 6: */
/* OBSOLETE 	case 7: */
/* OBSOLETE 	case 8:		/* User Instruction Mapping Miss */ */
/* OBSOLETE 	case 9:		/* User Data Mapping Miss */ */
/* OBSOLETE 	case 10:		/* Supervisor Instruction Mapping Miss */ */
/* OBSOLETE 	case 11:		/* Supervisor Data Mapping Miss */ */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE 	  status->value.sig = TARGET_SIGNAL_SEGV; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case 12: */
/* OBSOLETE 	case 13: */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE 	  status->value.sig = TARGET_SIGNAL_ILL; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case 14:		/* Timer */ */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE 	  status->value.sig = TARGET_SIGNAL_ALRM; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case 15:		/* Trace */ */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE 	  status->value.sig = TARGET_SIGNAL_TRAP; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case 16:		/* INTR0 */ */
/* OBSOLETE 	case 17:		/* INTR1 */ */
/* OBSOLETE 	case 18:		/* INTR2 */ */
/* OBSOLETE 	case 19:		/* INTR3/Internal */ */
/* OBSOLETE 	case 20:		/* TRAP0 */ */
/* OBSOLETE 	case 21:		/* TRAP1 */ */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE 	  status->value.sig = TARGET_SIGNAL_INT; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case 22:		/* Floating-Point Exception */ */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE 	  /* Why not FPE?  */ */
/* OBSOLETE 	  status->value.sig = TARGET_SIGNAL_ILL; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	case 77:		/* assert 77 */ */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE 	  status->value.sig = TARGET_SIGNAL_TRAP; */
/* OBSOLETE 	  break; */
/* OBSOLETE 	default: */
/* OBSOLETE 	  status->kind = TARGET_WAITKIND_EXITED; */
/* OBSOLETE 	  status->value.integer = 0; */
/* OBSOLETE 	} */
/* OBSOLETE       break; */
/* OBSOLETE     case UDINotExecuting: */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_TERM; */
/* OBSOLETE       break; */
/* OBSOLETE     case UDIStopped: */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_TSTP; */
/* OBSOLETE       break; */
/* OBSOLETE     case UDIWarned: */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_URG; */
/* OBSOLETE       break; */
/* OBSOLETE     case UDIStepped: */
/* OBSOLETE     case UDIBreak: */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_TRAP; */
/* OBSOLETE       break; */
/* OBSOLETE     case UDIWaiting: */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_STOP; */
/* OBSOLETE       break; */
/* OBSOLETE     case UDIHalted: */
/* OBSOLETE       status->kind = TARGET_WAITKIND_STOPPED; */
/* OBSOLETE       status->value.sig = TARGET_SIGNAL_KILL; */
/* OBSOLETE       break; */
/* OBSOLETE     case UDIExited: */
/* OBSOLETE     default: */
/* OBSOLETE       status->kind = TARGET_WAITKIND_EXITED; */
/* OBSOLETE       status->value.integer = 0; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   timeout = old_timeout;	/* Restore original timeout value */ */
/* OBSOLETE   immediate_quit = old_immediate_quit; */
/* OBSOLETE   return inferior_ptid; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE #if 0 */
/* OBSOLETE /* Handy for debugging */ */
/* OBSOLETE udi_pc (void) */
/* OBSOLETE { */
/* OBSOLETE   UDIResource From; */
/* OBSOLETE   UDIUInt32 *To; */
/* OBSOLETE   UDICount Count; */
/* OBSOLETE   UDISizeT Size = 4; */
/* OBSOLETE   UDICount CountDone; */
/* OBSOLETE   UDIBool HostEndian = 0; */
/* OBSOLETE   UDIError err; */
/* OBSOLETE   int pc[2]; */
/* OBSOLETE   unsigned long myregs[256]; */
/* OBSOLETE   int i; */
/* OBSOLETE  */
/* OBSOLETE   From.Space = UDI29KPC; */
/* OBSOLETE   From.Offset = 0; */
/* OBSOLETE   To = (UDIUInt32 *) pc; */
/* OBSOLETE   Count = 2; */
/* OBSOLETE  */
/* OBSOLETE   err = UDIRead (From, To, Count, Size, &CountDone, HostEndian); */
/* OBSOLETE  */
/* OBSOLETE   printf_unfiltered ("err = %d, CountDone = %d, pc[0] = 0x%x, pc[1] = 0x%x\n", */
/* OBSOLETE 		     err, CountDone, pc[0], pc[1]); */
/* OBSOLETE  */
/* OBSOLETE   udi_fetch_registers (-1); */
/* OBSOLETE  */
/* OBSOLETE   printf_unfiltered ("other pc1 = 0x%x, pc0 = 0x%x\n", *(int *) &registers[4 * PC_REGNUM], */
/* OBSOLETE 		     *(int *) &registers[4 * NPC_REGNUM]); */
/* OBSOLETE  */
/* OBSOLETE   /* Now, read all the registers globally */ */
/* OBSOLETE  */
/* OBSOLETE   From.Space = UDI29KGlobalRegs; */
/* OBSOLETE   From.Offset = 0; */
/* OBSOLETE   err = UDIRead (From, myregs, 256, 4, &CountDone, HostEndian); */
/* OBSOLETE  */
/* OBSOLETE   printf ("err = %d, CountDone = %d\n", err, CountDone); */
/* OBSOLETE  */
/* OBSOLETE   printf ("\n"); */
/* OBSOLETE  */
/* OBSOLETE   for (i = 0; i < 256; i += 2) */
/* OBSOLETE     printf ("%d:\t%#10x\t%11d\t%#10x\t%11d\n", i, myregs[i], myregs[i], */
/* OBSOLETE 	    myregs[i + 1], myregs[i + 1]); */
/* OBSOLETE   printf ("\n"); */
/* OBSOLETE  */
/* OBSOLETE   return pc[0]; */
/* OBSOLETE } */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE /********************************************************** UDI_FETCH_REGISTERS */
/* OBSOLETE  * Read a remote register 'regno'.  */
/* OBSOLETE  * If regno==-1 then read all the registers. */
/* OBSOLETE  */ */
/* OBSOLETE static void */
/* OBSOLETE udi_fetch_registers (int regno) */
/* OBSOLETE { */
/* OBSOLETE   UDIResource From; */
/* OBSOLETE   UDIUInt32 *To; */
/* OBSOLETE   UDICount Count; */
/* OBSOLETE   UDISizeT Size = 4; */
/* OBSOLETE   UDICount CountDone; */
/* OBSOLETE   UDIBool HostEndian = 0; */
/* OBSOLETE   UDIError err; */
/* OBSOLETE   int i; */
/* OBSOLETE  */
/* OBSOLETE   if (regno >= 0) */
/* OBSOLETE     { */
/* OBSOLETE       fetch_register (regno); */
/* OBSOLETE       return; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE /* Gr1/rsp */ */
/* OBSOLETE  */
/* OBSOLETE   From.Space = UDI29KGlobalRegs; */
/* OBSOLETE   From.Offset = 1; */
/* OBSOLETE   To = (UDIUInt32 *) & registers[4 * GR1_REGNUM]; */
/* OBSOLETE   Count = 1; */
/* OBSOLETE   if (err = UDIRead (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIRead() failed in udi_fetch_registers"); */
/* OBSOLETE  */
/* OBSOLETE   register_valid[GR1_REGNUM] = 1; */
/* OBSOLETE  */
/* OBSOLETE #if defined(GR64_REGNUM)	/* Read gr64-127 */ */
/* OBSOLETE  */
/* OBSOLETE /* Global Registers gr64-gr95 */ */
/* OBSOLETE  */
/* OBSOLETE   From.Space = UDI29KGlobalRegs; */
/* OBSOLETE   From.Offset = 64; */
/* OBSOLETE   To = (UDIUInt32 *) & registers[4 * GR64_REGNUM]; */
/* OBSOLETE   Count = 32; */
/* OBSOLETE   if (err = UDIRead (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIRead() failed in udi_fetch_registers"); */
/* OBSOLETE  */
/* OBSOLETE   for (i = GR64_REGNUM; i < GR64_REGNUM + 32; i++) */
/* OBSOLETE     register_valid[i] = 1; */
/* OBSOLETE  */
/* OBSOLETE #endif /*  GR64_REGNUM */ */
/* OBSOLETE  */
/* OBSOLETE /* Global Registers gr96-gr127 */ */
/* OBSOLETE  */
/* OBSOLETE   From.Space = UDI29KGlobalRegs; */
/* OBSOLETE   From.Offset = 96; */
/* OBSOLETE   To = (UDIUInt32 *) & registers[4 * GR96_REGNUM]; */
/* OBSOLETE   Count = 32; */
/* OBSOLETE   if (err = UDIRead (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIRead() failed in udi_fetch_registers"); */
/* OBSOLETE  */
/* OBSOLETE   for (i = GR96_REGNUM; i < GR96_REGNUM + 32; i++) */
/* OBSOLETE     register_valid[i] = 1; */
/* OBSOLETE  */
/* OBSOLETE /* Local Registers */ */
/* OBSOLETE  */
/* OBSOLETE   From.Space = UDI29KLocalRegs; */
/* OBSOLETE   From.Offset = 0; */
/* OBSOLETE   To = (UDIUInt32 *) & registers[4 * LR0_REGNUM]; */
/* OBSOLETE   Count = 128; */
/* OBSOLETE   if (err = UDIRead (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIRead() failed in udi_fetch_registers"); */
/* OBSOLETE  */
/* OBSOLETE   for (i = LR0_REGNUM; i < LR0_REGNUM + 128; i++) */
/* OBSOLETE     register_valid[i] = 1; */
/* OBSOLETE  */
/* OBSOLETE /* Protected Special Registers */ */
/* OBSOLETE  */
/* OBSOLETE   From.Space = UDI29KSpecialRegs; */
/* OBSOLETE   From.Offset = 0; */
/* OBSOLETE   To = (UDIUInt32 *) & registers[4 * SR_REGNUM (0)]; */
/* OBSOLETE   Count = 15; */
/* OBSOLETE   if (err = UDIRead (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIRead() failed in udi_fetch_registers"); */
/* OBSOLETE  */
/* OBSOLETE   for (i = SR_REGNUM (0); i < SR_REGNUM (0) + 15; i++) */
/* OBSOLETE     register_valid[i] = 1; */
/* OBSOLETE  */
/* OBSOLETE   if (USE_SHADOW_PC) */
/* OBSOLETE     {				/* Let regno_to_srnum() handle the register number */ */
/* OBSOLETE       fetch_register (NPC_REGNUM); */
/* OBSOLETE       fetch_register (PC_REGNUM); */
/* OBSOLETE       fetch_register (PC2_REGNUM); */
/* OBSOLETE  */
/* OBSOLETE /* Unprotected Special Registers sr128-sr135 */ */
/* OBSOLETE  */
/* OBSOLETE       From.Space = UDI29KSpecialRegs; */
/* OBSOLETE       From.Offset = 128; */
/* OBSOLETE       To = (UDIUInt32 *) & registers[4 * SR_REGNUM (128)]; */
/* OBSOLETE       Count = 135 - 128 + 1; */
/* OBSOLETE       if (err = UDIRead (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE 	error ("UDIRead() failed in udi_fetch_registers"); */
/* OBSOLETE  */
/* OBSOLETE       for (i = SR_REGNUM (128); i < SR_REGNUM (128) + 135 - 128 + 1; i++) */
/* OBSOLETE 	register_valid[i] = 1; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   if (remote_debug) */
/* OBSOLETE     { */
/* OBSOLETE       fprintf_unfiltered (gdb_stdlog, "Fetching all registers\n"); */
/* OBSOLETE       fprintf_unfiltered (gdb_stdlog, */
/* OBSOLETE 			  "Fetching PC0 = 0x%x, PC1 = 0x%x, PC2 = 0x%x\n", */
/* OBSOLETE 			  read_register (NPC_REGNUM), */
/* OBSOLETE 			  read_register (PC_REGNUM), */
/* OBSOLETE 			  read_register (PC2_REGNUM)); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* There doesn't seem to be any way to get these.  */ */
/* OBSOLETE   { */
/* OBSOLETE     int val = -1; */
/* OBSOLETE     supply_register (FPE_REGNUM, (char *) &val); */
/* OBSOLETE     supply_register (INTE_REGNUM, (char *) &val); */
/* OBSOLETE     supply_register (FPS_REGNUM, (char *) &val); */
/* OBSOLETE     supply_register (EXO_REGNUM, (char *) &val); */
/* OBSOLETE   } */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /********************************************************* UDI_STORE_REGISTERS */
/* OBSOLETE ** Store register regno into the target.   */
/* OBSOLETE  * If regno==-1 then store all the registers. */
/* OBSOLETE  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE udi_store_registers (int regno) */
/* OBSOLETE { */
/* OBSOLETE   UDIUInt32 *From; */
/* OBSOLETE   UDIResource To; */
/* OBSOLETE   UDICount Count; */
/* OBSOLETE   UDISizeT Size = 4; */
/* OBSOLETE   UDICount CountDone; */
/* OBSOLETE   UDIBool HostEndian = 0; */
/* OBSOLETE  */
/* OBSOLETE   if (regno >= 0) */
/* OBSOLETE     { */
/* OBSOLETE       store_register (regno); */
/* OBSOLETE       return; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   if (remote_debug) */
/* OBSOLETE     { */
/* OBSOLETE       fprintf_unfiltered (gdb_stdlog, "Storing all registers\n"); */
/* OBSOLETE       fprintf_unfiltered (gdb_stdlog, */
/* OBSOLETE 			  "PC0 = 0x%x, PC1 = 0x%x, PC2 = 0x%x\n", */
/* OBSOLETE 			  read_register (NPC_REGNUM), */
/* OBSOLETE 			  read_register (PC_REGNUM), */
/* OBSOLETE 			  read_register (PC2_REGNUM)); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE /* Gr1/rsp */ */
/* OBSOLETE  */
/* OBSOLETE   From = (UDIUInt32 *) & registers[4 * GR1_REGNUM]; */
/* OBSOLETE   To.Space = UDI29KGlobalRegs; */
/* OBSOLETE   To.Offset = 1; */
/* OBSOLETE   Count = 1; */
/* OBSOLETE   if (UDIWrite (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIWrite() failed in udi_store_regisetrs"); */
/* OBSOLETE  */
/* OBSOLETE #if defined(GR64_REGNUM) */
/* OBSOLETE  */
/* OBSOLETE /* Global registers gr64-gr95 */ */
/* OBSOLETE  */
/* OBSOLETE   From = (UDIUInt32 *) & registers[4 * GR64_REGNUM]; */
/* OBSOLETE   To.Space = UDI29KGlobalRegs; */
/* OBSOLETE   To.Offset = 64; */
/* OBSOLETE   Count = 32; */
/* OBSOLETE   if (UDIWrite (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIWrite() failed in udi_store_regisetrs"); */
/* OBSOLETE  */
/* OBSOLETE #endif /* GR64_REGNUM */ */
/* OBSOLETE  */
/* OBSOLETE /* Global registers gr96-gr127 */ */
/* OBSOLETE  */
/* OBSOLETE   From = (UDIUInt32 *) & registers[4 * GR96_REGNUM]; */
/* OBSOLETE   To.Space = UDI29KGlobalRegs; */
/* OBSOLETE   To.Offset = 96; */
/* OBSOLETE   Count = 32; */
/* OBSOLETE   if (UDIWrite (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIWrite() failed in udi_store_regisetrs"); */
/* OBSOLETE  */
/* OBSOLETE /* Local Registers */ */
/* OBSOLETE  */
/* OBSOLETE   From = (UDIUInt32 *) & registers[4 * LR0_REGNUM]; */
/* OBSOLETE   To.Space = UDI29KLocalRegs; */
/* OBSOLETE   To.Offset = 0; */
/* OBSOLETE   Count = 128; */
/* OBSOLETE   if (UDIWrite (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIWrite() failed in udi_store_regisetrs"); */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE   /* Protected Special Registers *//* VAB through TMR */ */
/* OBSOLETE  */
/* OBSOLETE   From = (UDIUInt32 *) & registers[4 * SR_REGNUM (0)]; */
/* OBSOLETE   To.Space = UDI29KSpecialRegs; */
/* OBSOLETE   To.Offset = 0; */
/* OBSOLETE   Count = 10; */
/* OBSOLETE   if (UDIWrite (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIWrite() failed in udi_store_regisetrs"); */
/* OBSOLETE  */
/* OBSOLETE /* PC0, PC1, PC2 possibly as shadow registers */ */
/* OBSOLETE  */
/* OBSOLETE   From = (UDIUInt32 *) & registers[4 * SR_REGNUM (10)]; */
/* OBSOLETE   To.Space = UDI29KSpecialRegs; */
/* OBSOLETE   Count = 3; */
/* OBSOLETE   if (USE_SHADOW_PC) */
/* OBSOLETE     To.Offset = 20;		/* SPC0 */ */
/* OBSOLETE   else */
/* OBSOLETE     To.Offset = 10;		/* PC0 */ */
/* OBSOLETE   if (UDIWrite (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIWrite() failed in udi_store_regisetrs"); */
/* OBSOLETE  */
/* OBSOLETE /* PC1 via UDI29KPC */ */
/* OBSOLETE  */
/* OBSOLETE   From = (UDIUInt32 *) & registers[4 * PC_REGNUM]; */
/* OBSOLETE   To.Space = UDI29KPC; */
/* OBSOLETE   To.Offset = 0;		/* PC1 */ */
/* OBSOLETE   Count = 1; */
/* OBSOLETE   if (UDIWrite (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIWrite() failed in udi_store_regisetrs"); */
/* OBSOLETE  */
/* OBSOLETE   /* LRU and MMU */ */
/* OBSOLETE  */
/* OBSOLETE   From = (UDIUInt32 *) & registers[4 * SR_REGNUM (13)]; */
/* OBSOLETE   To.Space = UDI29KSpecialRegs; */
/* OBSOLETE   To.Offset = 13; */
/* OBSOLETE   Count = 2; */
/* OBSOLETE   if (UDIWrite (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIWrite() failed in udi_store_regisetrs"); */
/* OBSOLETE  */
/* OBSOLETE /* Unprotected Special Registers */ */
/* OBSOLETE  */
/* OBSOLETE   From = (UDIUInt32 *) & registers[4 * SR_REGNUM (128)]; */
/* OBSOLETE   To.Space = UDI29KSpecialRegs; */
/* OBSOLETE   To.Offset = 128; */
/* OBSOLETE   Count = 135 - 128 + 1; */
/* OBSOLETE   if (UDIWrite (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIWrite() failed in udi_store_regisetrs"); */
/* OBSOLETE  */
/* OBSOLETE   registers_changed (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /****************************************************** UDI_PREPARE_TO_STORE */ */
/* OBSOLETE /* Get ready to modify the registers array.  On machines which store */
/* OBSOLETE    individual registers, this doesn't need to do anything.  On machines */
/* OBSOLETE    which store all the registers in one fell swoop, this makes sure */
/* OBSOLETE    that registers contains all the registers from the program being */
/* OBSOLETE    debugged.  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE udi_prepare_to_store (void) */
/* OBSOLETE { */
/* OBSOLETE   /* Do nothing, since we can store individual regs */ */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /********************************************************** TRANSLATE_ADDR */ */
/* OBSOLETE static CORE_ADDR */
/* OBSOLETE translate_addr (CORE_ADDR addr) */
/* OBSOLETE { */
/* OBSOLETE #if defined(ULTRA3) && defined(KERNEL_DEBUGGING) */
/* OBSOLETE   /* Check for a virtual address in the kernel */ */
/* OBSOLETE   /* Assume physical address of ublock is in  paddr_u register */ */
/* OBSOLETE   /* FIXME: doesn't work for user virtual addresses */ */
/* OBSOLETE   if (addr >= UVADDR) */
/* OBSOLETE     { */
/* OBSOLETE       /* PADDR_U register holds the physical address of the ublock */ */
/* OBSOLETE       CORE_ADDR i = (CORE_ADDR) read_register (PADDR_U_REGNUM); */
/* OBSOLETE       return (i + addr - (CORE_ADDR) UVADDR); */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       return (addr); */
/* OBSOLETE     } */
/* OBSOLETE #else */
/* OBSOLETE   return (addr); */
/* OBSOLETE #endif */
/* OBSOLETE } */
/* OBSOLETE /************************************************* UDI_XFER_INFERIOR_MEMORY */ */
/* OBSOLETE /* FIXME!  Merge these two.  */ */
/* OBSOLETE static int */
/* OBSOLETE udi_xfer_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len, int write, */
/* OBSOLETE 			  struct mem_attrib *attrib ATTRIBUTE_UNUSED, */
/* OBSOLETE 			  struct target_ops *target ATTRIBUTE_UNUSED) */
/* OBSOLETE { */
/* OBSOLETE  */
/* OBSOLETE   memaddr = translate_addr (memaddr); */
/* OBSOLETE  */
/* OBSOLETE   if (write) */
/* OBSOLETE     return udi_write_inferior_memory (memaddr, myaddr, len); */
/* OBSOLETE   else */
/* OBSOLETE     return udi_read_inferior_memory (memaddr, myaddr, len); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /********************************************************** UDI_FILES_INFO */ */
/* OBSOLETE static void */
/* OBSOLETE udi_files_info (struct target_ops *target) */
/* OBSOLETE { */
/* OBSOLETE   printf_unfiltered ("\tAttached to UDI socket to %s", udi_config_id); */
/* OBSOLETE   if (prog_name != NULL) */
/* OBSOLETE     printf_unfiltered ("and running program %s", prog_name); */
/* OBSOLETE   printf_unfiltered (".\n"); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /**************************************************** UDI_INSERT_BREAKPOINT */ */
/* OBSOLETE static int */
/* OBSOLETE udi_insert_breakpoint (CORE_ADDR addr, char *contents_cache) */
/* OBSOLETE { */
/* OBSOLETE   int cnt; */
/* OBSOLETE   UDIError err; */
/* OBSOLETE  */
/* OBSOLETE   for (cnt = 0; cnt < BKPT_TABLE_SIZE; cnt++) */
/* OBSOLETE     if (bkpt_table[cnt].Type == 0)	/* Find first free slot */ */
/* OBSOLETE       break; */
/* OBSOLETE  */
/* OBSOLETE   if (cnt >= BKPT_TABLE_SIZE) */
/* OBSOLETE     error ("Too many breakpoints set"); */
/* OBSOLETE  */
/* OBSOLETE   bkpt_table[cnt].Addr.Offset = addr; */
/* OBSOLETE   bkpt_table[cnt].Addr.Space = UDI29KIRAMSpace; */
/* OBSOLETE   bkpt_table[cnt].PassCount = 1; */
/* OBSOLETE   bkpt_table[cnt].Type = UDIBreakFlagExecute; */
/* OBSOLETE  */
/* OBSOLETE   err = UDISetBreakpoint (bkpt_table[cnt].Addr, */
/* OBSOLETE 			  bkpt_table[cnt].PassCount, */
/* OBSOLETE 			  bkpt_table[cnt].Type, */
/* OBSOLETE 			  &bkpt_table[cnt].BreakId); */
/* OBSOLETE  */
/* OBSOLETE   if (err == 0) */
/* OBSOLETE     return 0;			/* Success */ */
/* OBSOLETE  */
/* OBSOLETE   bkpt_table[cnt].Type = 0; */
/* OBSOLETE   error ("UDISetBreakpoint returned error code %d\n", err); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /**************************************************** UDI_REMOVE_BREAKPOINT */ */
/* OBSOLETE static int */
/* OBSOLETE udi_remove_breakpoint (CORE_ADDR addr, char *contents_cache) */
/* OBSOLETE { */
/* OBSOLETE   int cnt; */
/* OBSOLETE   UDIError err; */
/* OBSOLETE  */
/* OBSOLETE   for (cnt = 0; cnt < BKPT_TABLE_SIZE; cnt++) */
/* OBSOLETE     if (bkpt_table[cnt].Addr.Offset == addr)	/* Find matching breakpoint */ */
/* OBSOLETE       break; */
/* OBSOLETE  */
/* OBSOLETE   if (cnt >= BKPT_TABLE_SIZE) */
/* OBSOLETE     error ("Can't find breakpoint in table"); */
/* OBSOLETE  */
/* OBSOLETE   bkpt_table[cnt].Type = 0; */
/* OBSOLETE  */
/* OBSOLETE   err = UDIClearBreakpoint (bkpt_table[cnt].BreakId); */
/* OBSOLETE   if (err == 0) */
/* OBSOLETE     return 0;			/* Success */ */
/* OBSOLETE  */
/* OBSOLETE   error ("UDIClearBreakpoint returned error code %d\n", err); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE udi_kill (void) */
/* OBSOLETE { */
/* OBSOLETE  */
/* OBSOLETE #if 0 */
/* OBSOLETE /* */
/* OBSOLETE    UDIStop does not really work as advertised.  It causes the TIP to close it's */
/* OBSOLETE    connection, which usually results in GDB dying with a SIGPIPE.  For now, we */
/* OBSOLETE    just invoke udi_close, which seems to get things right. */
/* OBSOLETE  */ */
/* OBSOLETE   UDIStop (); */
/* OBSOLETE  */
/* OBSOLETE   udi_session_id = -1; */
/* OBSOLETE   inferior_ptid = null_ptid; */
/* OBSOLETE  */
/* OBSOLETE   if (from_tty) */
/* OBSOLETE     printf_unfiltered ("Target has been stopped."); */
/* OBSOLETE #endif /* 0 */ */
/* OBSOLETE #if 0 */
/* OBSOLETE   udi_close (0); */
/* OBSOLETE   pop_target (); */
/* OBSOLETE #endif /* 0 */ */
/* OBSOLETE  */
/* OBSOLETE   /* Keep the target around, e.g. so "run" can do the right thing when */
/* OBSOLETE      we are already debugging something.  */ */
/* OBSOLETE  */
/* OBSOLETE   if (UDIDisconnect (udi_session_id, UDITerminateSession)) */
/* OBSOLETE     { */
/* OBSOLETE       warning ("UDIDisconnect() failed"); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   /* Do not try to close udi_session_id again, later in the program.  */ */
/* OBSOLETE   udi_session_id = -1; */
/* OBSOLETE   inferior_ptid = null_ptid; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /*  */
/* OBSOLETE    Load a program into the target.  Args are: `program {options}'.  The options */
/* OBSOLETE    are used to control loading of the program, and are NOT passed onto the */
/* OBSOLETE    loaded code as arguments.  (You need to use the `run' command to do that.) */
/* OBSOLETE  */
/* OBSOLETE    The options are: */
/* OBSOLETE    -ms %d       Set mem stack size to %d */
/* OBSOLETE    -rs %d       Set regular stack size to %d */
/* OBSOLETE    -i   send init info (default) */
/* OBSOLETE    -noi don't send init info */
/* OBSOLETE    -[tT]        Load Text section */
/* OBSOLETE    -[dD]        Load Data section */
/* OBSOLETE    -[bB]        Load BSS section */
/* OBSOLETE    -[lL]        Load Lit section */
/* OBSOLETE  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE download (char *load_arg_string, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE #define DEFAULT_MEM_STACK_SIZE 		0x6000 */
/* OBSOLETE #define DEFAULT_REG_STACK_SIZE 		0x2000 */
/* OBSOLETE  */
/* OBSOLETE   char *token; */
/* OBSOLETE   char *filename; */
/* OBSOLETE   asection *section; */
/* OBSOLETE   bfd *pbfd; */
/* OBSOLETE   UDIError err; */
/* OBSOLETE   int load_text = 1, load_data = 1, load_bss = 1, load_lit = 1; */
/* OBSOLETE  */
/* OBSOLETE   address_ranges[0].Space = UDI29KIRAMSpace; */
/* OBSOLETE   address_ranges[0].Offset = 0xffffffff; */
/* OBSOLETE   address_ranges[0].Size = 0; */
/* OBSOLETE  */
/* OBSOLETE   address_ranges[1].Space = UDI29KDRAMSpace; */
/* OBSOLETE   address_ranges[1].Offset = 0xffffffff; */
/* OBSOLETE   address_ranges[1].Size = 0; */
/* OBSOLETE  */
/* OBSOLETE   stack_sizes[0] = DEFAULT_REG_STACK_SIZE; */
/* OBSOLETE   stack_sizes[1] = DEFAULT_MEM_STACK_SIZE; */
/* OBSOLETE  */
/* OBSOLETE   dont_repeat (); */
/* OBSOLETE  */
/* OBSOLETE   filename = strtok (load_arg_string, " \t"); */
/* OBSOLETE   if (!filename) */
/* OBSOLETE     error ("Must specify at least a file name with the load command"); */
/* OBSOLETE  */
/* OBSOLETE   filename = tilde_expand (filename); */
/* OBSOLETE   make_cleanup (xfree, filename); */
/* OBSOLETE  */
/* OBSOLETE   while (token = strtok (NULL, " \t")) */
/* OBSOLETE     { */
/* OBSOLETE       if (token[0] == '-') */
/* OBSOLETE 	{ */
/* OBSOLETE 	  token++; */
/* OBSOLETE  */
/* OBSOLETE 	  if (STREQ (token, "ms")) */
/* OBSOLETE 	    stack_sizes[1] = atol (strtok (NULL, " \t")); */
/* OBSOLETE 	  else if (STREQ (token, "rs")) */
/* OBSOLETE 	    stack_sizes[0] = atol (strtok (NULL, " \t")); */
/* OBSOLETE 	  else */
/* OBSOLETE 	    { */
/* OBSOLETE 	      load_text = load_data = load_bss = load_lit = 0; */
/* OBSOLETE  */
/* OBSOLETE 	      while (*token) */
/* OBSOLETE 		{ */
/* OBSOLETE 		  switch (*token++) */
/* OBSOLETE 		    { */
/* OBSOLETE 		    case 't': */
/* OBSOLETE 		    case 'T': */
/* OBSOLETE 		      load_text = 1; */
/* OBSOLETE 		      break; */
/* OBSOLETE 		    case 'd': */
/* OBSOLETE 		    case 'D': */
/* OBSOLETE 		      load_data = 1; */
/* OBSOLETE 		      break; */
/* OBSOLETE 		    case 'b': */
/* OBSOLETE 		    case 'B': */
/* OBSOLETE 		      load_bss = 1; */
/* OBSOLETE 		      break; */
/* OBSOLETE 		    case 'l': */
/* OBSOLETE 		    case 'L': */
/* OBSOLETE 		      load_lit = 1; */
/* OBSOLETE 		      break; */
/* OBSOLETE 		    default: */
/* OBSOLETE 		      error ("Unknown UDI load option -%s", token - 1); */
/* OBSOLETE 		    } */
/* OBSOLETE 		} */
/* OBSOLETE 	    } */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   pbfd = bfd_openr (filename, gnutarget); */
/* OBSOLETE  */
/* OBSOLETE   if (!pbfd) */
/* OBSOLETE     /* FIXME: should be using bfd_errmsg, not assuming it was */
/* OBSOLETE        bfd_error_system_call.  */ */
/* OBSOLETE     perror_with_name (filename); */
/* OBSOLETE  */
/* OBSOLETE   /* FIXME: should be checking for errors from bfd_close (for one thing, */
/* OBSOLETE      on error it does not free all the storage associated with the */
/* OBSOLETE      bfd).  */ */
/* OBSOLETE   make_cleanup_bfd_close (pbfd); */
/* OBSOLETE  */
/* OBSOLETE   QUIT; */
/* OBSOLETE   immediate_quit++; */
/* OBSOLETE  */
/* OBSOLETE   if (!bfd_check_format (pbfd, bfd_object)) */
/* OBSOLETE     error ("It doesn't seem to be an object file"); */
/* OBSOLETE  */
/* OBSOLETE   for (section = pbfd->sections; section; section = section->next) */
/* OBSOLETE     { */
/* OBSOLETE       if (bfd_get_section_flags (pbfd, section) & SEC_ALLOC) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  UDIResource To; */
/* OBSOLETE 	  UDICount Count; */
/* OBSOLETE 	  unsigned long section_size, section_end; */
/* OBSOLETE 	  const char *section_name; */
/* OBSOLETE  */
/* OBSOLETE 	  section_name = bfd_get_section_name (pbfd, section); */
/* OBSOLETE 	  if (STREQ (section_name, ".text") && !load_text) */
/* OBSOLETE 	    continue; */
/* OBSOLETE 	  else if (STREQ (section_name, ".data") && !load_data) */
/* OBSOLETE 	    continue; */
/* OBSOLETE 	  else if (STREQ (section_name, ".bss") && !load_bss) */
/* OBSOLETE 	    continue; */
/* OBSOLETE 	  else if (STREQ (section_name, ".lit") && !load_lit) */
/* OBSOLETE 	    continue; */
/* OBSOLETE  */
/* OBSOLETE 	  To.Offset = bfd_get_section_vma (pbfd, section); */
/* OBSOLETE 	  section_size = bfd_section_size (pbfd, section); */
/* OBSOLETE 	  section_end = To.Offset + section_size; */
/* OBSOLETE  */
/* OBSOLETE 	  if (section_size == 0) */
/* OBSOLETE 	    /* This is needed at least in the BSS case, where the code */
/* OBSOLETE 	       below starts writing before it even checks the size.  */ */
/* OBSOLETE 	    continue; */
/* OBSOLETE  */
/* OBSOLETE 	  printf_unfiltered ("[Loading section %s at %x (%d bytes)]\n", */
/* OBSOLETE 			     section_name, */
/* OBSOLETE 			     To.Offset, */
/* OBSOLETE 			     section_size); */
/* OBSOLETE  */
/* OBSOLETE 	  if (bfd_get_section_flags (pbfd, section) & SEC_CODE) */
/* OBSOLETE 	    { */
/* OBSOLETE 	      To.Space = UDI29KIRAMSpace; */
/* OBSOLETE  */
/* OBSOLETE 	      address_ranges[0].Offset = min (address_ranges[0].Offset, */
/* OBSOLETE 					      To.Offset); */
/* OBSOLETE 	      address_ranges[0].Size = max (address_ranges[0].Size, */
/* OBSOLETE 					    section_end */
/* OBSOLETE 					    - address_ranges[0].Offset); */
/* OBSOLETE 	    } */
/* OBSOLETE 	  else */
/* OBSOLETE 	    { */
/* OBSOLETE 	      To.Space = UDI29KDRAMSpace; */
/* OBSOLETE  */
/* OBSOLETE 	      address_ranges[1].Offset = min (address_ranges[1].Offset, */
/* OBSOLETE 					      To.Offset); */
/* OBSOLETE 	      address_ranges[1].Size = max (address_ranges[1].Size, */
/* OBSOLETE 					    section_end */
/* OBSOLETE 					    - address_ranges[1].Offset); */
/* OBSOLETE 	    } */
/* OBSOLETE  */
/* OBSOLETE 	  if (bfd_get_section_flags (pbfd, section) & SEC_LOAD)		/* Text, data or lit */ */
/* OBSOLETE 	    { */
/* OBSOLETE 	      file_ptr fptr; */
/* OBSOLETE  */
/* OBSOLETE 	      fptr = 0; */
/* OBSOLETE  */
/* OBSOLETE 	      while (section_size > 0) */
/* OBSOLETE 		{ */
/* OBSOLETE 		  char buffer[1024]; */
/* OBSOLETE  */
/* OBSOLETE 		  Count = min (section_size, 1024); */
/* OBSOLETE  */
/* OBSOLETE 		  bfd_get_section_contents (pbfd, section, buffer, fptr, */
/* OBSOLETE 					    Count); */
/* OBSOLETE  */
/* OBSOLETE 		  err = UDIWrite ((UDIHostMemPtr) buffer,	/* From */ */
/* OBSOLETE 				  To,	/* To */ */
/* OBSOLETE 				  Count,	/* Count */ */
/* OBSOLETE 				  (UDISizeT) 1,		/* Size */ */
/* OBSOLETE 				  &Count,	/* CountDone */ */
/* OBSOLETE 				  (UDIBool) 0);		/* HostEndian */ */
/* OBSOLETE 		  if (err) */
/* OBSOLETE 		    error ("UDIWrite failed, error = %d", err); */
/* OBSOLETE  */
/* OBSOLETE 		  To.Offset += Count; */
/* OBSOLETE 		  fptr += Count; */
/* OBSOLETE 		  section_size -= Count; */
/* OBSOLETE 		} */
/* OBSOLETE 	    } */
/* OBSOLETE 	  else */
/* OBSOLETE 	    /* BSS */ */
/* OBSOLETE 	    { */
/* OBSOLETE 	      UDIResource From; */
/* OBSOLETE 	      unsigned long zero = 0; */
/* OBSOLETE  */
/* OBSOLETE 	      /* Write a zero byte at the vma */ */
/* OBSOLETE 	      /* FIXME: Broken for sections of 1-3 bytes (we test for */
/* OBSOLETE 	         zero above).  */ */
/* OBSOLETE 	      err = UDIWrite ((UDIHostMemPtr) & zero,	/* From */ */
/* OBSOLETE 			      To,	/* To */ */
/* OBSOLETE 			      (UDICount) 1,	/* Count */ */
/* OBSOLETE 			      (UDISizeT) 4,	/* Size */ */
/* OBSOLETE 			      &Count,	/* CountDone */ */
/* OBSOLETE 			      (UDIBool) 0);	/* HostEndian */ */
/* OBSOLETE 	      if (err) */
/* OBSOLETE 		error ("UDIWrite failed, error = %d", err); */
/* OBSOLETE  */
/* OBSOLETE 	      From = To; */
/* OBSOLETE 	      To.Offset += 4; */
/* OBSOLETE  */
/* OBSOLETE 	      /* Now, duplicate it for the length of the BSS */ */
/* OBSOLETE 	      err = UDICopy (From,	/* From */ */
/* OBSOLETE 			     To,	/* To */ */
/* OBSOLETE 			     (UDICount) (section_size / 4 - 1),		/* Count */ */
/* OBSOLETE 			     (UDISizeT) 4,	/* Size */ */
/* OBSOLETE 			     &Count,	/* CountDone */ */
/* OBSOLETE 			     (UDIBool) 1);	/* Direction */ */
/* OBSOLETE 	      if (err) */
/* OBSOLETE 		{ */
/* OBSOLETE 		  char message[100]; */
/* OBSOLETE 		  int xerr; */
/* OBSOLETE  */
/* OBSOLETE 		  xerr = UDIGetErrorMsg (err, 100, message, &Count); */
/* OBSOLETE 		  if (!xerr) */
/* OBSOLETE 		    fprintf_unfiltered (gdb_stderr, "Error is %s\n", message); */
/* OBSOLETE 		  else */
/* OBSOLETE 		    fprintf_unfiltered (gdb_stderr, "xerr is %d\n", xerr); */
/* OBSOLETE 		  error ("UDICopy failed, error = %d", err); */
/* OBSOLETE 		} */
/* OBSOLETE 	    } */
/* OBSOLETE  */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   entry.Space = UDI29KIRAMSpace; */
/* OBSOLETE   entry.Offset = bfd_get_start_address (pbfd); */
/* OBSOLETE  */
/* OBSOLETE   immediate_quit--; */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /* Function to download an image into the remote target.  */ */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE udi_load (char *args, int from_tty) */
/* OBSOLETE { */
/* OBSOLETE   download (args, from_tty); */
/* OBSOLETE  */
/* OBSOLETE   /* As a convenience, pick up any symbol info that is in the program */
/* OBSOLETE      being loaded.  Note that we assume that the program is the``mainline''; */
/* OBSOLETE      if this is not always true, then this code will need to be augmented.  */ */
/* OBSOLETE   symbol_file_add (strtok (args, " \t"), from_tty, NULL, 1, 0); */
/* OBSOLETE  */
/* OBSOLETE   /* Getting new symbols may change our opinion about what is */
/* OBSOLETE      frameless.  */ */
/* OBSOLETE   reinit_frame_cache (); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /*************************************************** UDI_WRITE_INFERIOR_MEMORY */
/* OBSOLETE ** Copy LEN bytes of data from debugger memory at MYADDR */
/* OBSOLETE    to inferior's memory at MEMADDR.  Returns number of bytes written.  */ */
/* OBSOLETE static int */
/* OBSOLETE udi_write_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len) */
/* OBSOLETE { */
/* OBSOLETE   int nwritten = 0; */
/* OBSOLETE   UDIUInt32 *From; */
/* OBSOLETE   UDIResource To; */
/* OBSOLETE   UDICount Count; */
/* OBSOLETE   UDISizeT Size = 1; */
/* OBSOLETE   UDICount CountDone = 0; */
/* OBSOLETE   UDIBool HostEndian = 0; */
/* OBSOLETE  */
/* OBSOLETE   To.Space = udi_memory_space (memaddr); */
/* OBSOLETE   From = (UDIUInt32 *) myaddr; */
/* OBSOLETE  */
/* OBSOLETE   while (nwritten < len) */
/* OBSOLETE     { */
/* OBSOLETE       Count = len - nwritten; */
/* OBSOLETE       if (Count > MAXDATA) */
/* OBSOLETE 	Count = MAXDATA; */
/* OBSOLETE       To.Offset = memaddr + nwritten; */
/* OBSOLETE       if (UDIWrite (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  error ("UDIWrite() failed in udi_write_inferior_memory"); */
/* OBSOLETE 	  break; */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	{ */
/* OBSOLETE 	  nwritten += CountDone; */
/* OBSOLETE 	  From += CountDone; */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE   return (nwritten); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /**************************************************** UDI_READ_INFERIOR_MEMORY */
/* OBSOLETE ** Read LEN bytes from inferior memory at MEMADDR.  Put the result */
/* OBSOLETE    at debugger address MYADDR.  Returns number of bytes read.  */ */
/* OBSOLETE static int */
/* OBSOLETE udi_read_inferior_memory (CORE_ADDR memaddr, char *myaddr, int len) */
/* OBSOLETE { */
/* OBSOLETE   int nread = 0; */
/* OBSOLETE   UDIResource From; */
/* OBSOLETE   UDIUInt32 *To; */
/* OBSOLETE   UDICount Count; */
/* OBSOLETE   UDISizeT Size = 1; */
/* OBSOLETE   UDICount CountDone = 0; */
/* OBSOLETE   UDIBool HostEndian = 0; */
/* OBSOLETE   UDIError err; */
/* OBSOLETE  */
/* OBSOLETE   From.Space = udi_memory_space (memaddr); */
/* OBSOLETE   To = (UDIUInt32 *) myaddr; */
/* OBSOLETE  */
/* OBSOLETE   while (nread < len) */
/* OBSOLETE     { */
/* OBSOLETE       Count = len - nread; */
/* OBSOLETE       if (Count > MAXDATA) */
/* OBSOLETE 	Count = MAXDATA; */
/* OBSOLETE       From.Offset = memaddr + nread; */
/* OBSOLETE       if (err = UDIRead (From, To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE 	{ */
/* OBSOLETE 	  error ("UDIRead() failed in udi_read_inferior_memory"); */
/* OBSOLETE 	  break; */
/* OBSOLETE 	} */
/* OBSOLETE       else */
/* OBSOLETE 	{ */
/* OBSOLETE 	  nread += CountDone; */
/* OBSOLETE 	  To += CountDone; */
/* OBSOLETE 	} */
/* OBSOLETE     } */
/* OBSOLETE   return (nread); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE /********************************************************************* WARNING */
/* OBSOLETE */ */
/* OBSOLETE udi_warning (int num) */
/* OBSOLETE { */
/* OBSOLETE   error ("ERROR while loading program into remote TIP: $d\n", num); */
/* OBSOLETE } */
/* OBSOLETE  */
/* OBSOLETE  */
/* OBSOLETE /*****************************************************************************/ */
/* OBSOLETE /* Fetch a single register indicatated by 'regno'.  */
/* OBSOLETE  * Returns 0/-1 on success/failure.   */
/* OBSOLETE  */ */
/* OBSOLETE static void */
/* OBSOLETE fetch_register (int regno) */
/* OBSOLETE { */
/* OBSOLETE   UDIResource From; */
/* OBSOLETE   UDIUInt32 To; */
/* OBSOLETE   UDICount Count = 1; */
/* OBSOLETE   UDISizeT Size = 4; */
/* OBSOLETE   UDICount CountDone; */
/* OBSOLETE   UDIBool HostEndian = 0; */
/* OBSOLETE   UDIError err; */
/* OBSOLETE   int result; */
/* OBSOLETE  */
/* OBSOLETE   if (regno == GR1_REGNUM) */
/* OBSOLETE     { */
/* OBSOLETE       From.Space = UDI29KGlobalRegs; */
/* OBSOLETE       From.Offset = 1; */
/* OBSOLETE     } */
/* OBSOLETE   else if (regno >= GR96_REGNUM && regno < GR96_REGNUM + 32) */
/* OBSOLETE     { */
/* OBSOLETE       From.Space = UDI29KGlobalRegs; */
/* OBSOLETE       From.Offset = (regno - GR96_REGNUM) + 96;; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE #if defined(GR64_REGNUM) */
/* OBSOLETE  */
/* OBSOLETE   else if (regno >= GR64_REGNUM && regno < GR64_REGNUM + 32) */
/* OBSOLETE     { */
/* OBSOLETE       From.Space = UDI29KGlobalRegs; */
/* OBSOLETE       From.Offset = (regno - GR64_REGNUM) + 64; */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE #endif /* GR64_REGNUM */ */
/* OBSOLETE  */
/* OBSOLETE   else if (regno >= LR0_REGNUM && regno < LR0_REGNUM + 128) */
/* OBSOLETE     { */
/* OBSOLETE       From.Space = UDI29KLocalRegs; */
/* OBSOLETE       From.Offset = (regno - LR0_REGNUM); */
/* OBSOLETE     } */
/* OBSOLETE   else if (regno >= FPE_REGNUM && regno <= EXO_REGNUM) */
/* OBSOLETE     { */
/* OBSOLETE       int val = -1; */
/* OBSOLETE       /*supply_register(160 + (regno - FPE_REGNUM),(char *) &val); */ */
/* OBSOLETE       supply_register (regno, (char *) &val); */
/* OBSOLETE       return;			/* Pretend Success */ */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     { */
/* OBSOLETE       From.Space = UDI29KSpecialRegs; */
/* OBSOLETE       From.Offset = regnum_to_srnum (regno); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   if (err = UDIRead (From, &To, Count, Size, &CountDone, HostEndian)) */
/* OBSOLETE     error ("UDIRead() failed in udi_fetch_registers"); */
/* OBSOLETE  */
/* OBSOLETE   supply_register (regno, (char *) &To); */
/* OBSOLETE  */
/* OBSOLETE   if (remote_debug) */
/* OBSOLETE     fprintf_unfiltered (gdb_stdlog, "Fetching register %s = 0x%x\n", */
/* OBSOLETE 			REGISTER_NAME (regno), To); */
/* OBSOLETE } */
/* OBSOLETE /*****************************************************************************/ */
/* OBSOLETE /* Store a single register indicated by 'regno'.  */
/* OBSOLETE  * Returns 0/-1 on success/failure.   */
/* OBSOLETE  */ */
/* OBSOLETE static int */
/* OBSOLETE store_register (int regno) */
/* OBSOLETE { */
/* OBSOLETE   int result; */
/* OBSOLETE   UDIUInt32 From; */
/* OBSOLETE   UDIResource To; */
/* OBSOLETE   UDICount Count = 1; */
/* OBSOLETE   UDISizeT Size = 4; */
/* OBSOLETE   UDICount CountDone; */
/* OBSOLETE   UDIBool HostEndian = 0; */
/* OBSOLETE  */
/* OBSOLETE   From = read_register (regno);	/* get data value */ */
/* OBSOLETE  */
/* OBSOLETE   if (remote_debug) */
/* OBSOLETE     fprintf_unfiltered (gdb_stdlog, "Storing register %s = 0x%x\n", */
/* OBSOLETE 			REGISTER_NAME (regno), From); */
/* OBSOLETE  */
/* OBSOLETE   if (regno == GR1_REGNUM) */
/* OBSOLETE     { */
/* OBSOLETE       To.Space = UDI29KGlobalRegs; */
/* OBSOLETE       To.Offset = 1; */
/* OBSOLETE       result = UDIWrite (&From, To, Count, Size, &CountDone, HostEndian); */
/* OBSOLETE       /* Setting GR1 changes the numbers of all the locals, so invalidate the  */
/* OBSOLETE        * register cache.  Do this *after* calling read_register, because we want  */
/* OBSOLETE        * read_register to return the value that write_register has just stuffed  */
/* OBSOLETE        * into the registers array, not the value of the register fetched from  */
/* OBSOLETE        * the inferior.   */
/* OBSOLETE        */ */
/* OBSOLETE       registers_changed (); */
/* OBSOLETE     } */
/* OBSOLETE #if defined(GR64_REGNUM) */
/* OBSOLETE   else if (regno >= GR64_REGNUM && regno < GR64_REGNUM + 32) */
/* OBSOLETE     { */
/* OBSOLETE       To.Space = UDI29KGlobalRegs; */
/* OBSOLETE       To.Offset = (regno - GR64_REGNUM) + 64; */
/* OBSOLETE       result = UDIWrite (&From, To, Count, Size, &CountDone, HostEndian); */
/* OBSOLETE     } */
/* OBSOLETE #endif /* GR64_REGNUM */ */
/* OBSOLETE   else if (regno >= GR96_REGNUM && regno < GR96_REGNUM + 32) */
/* OBSOLETE     { */
/* OBSOLETE       To.Space = UDI29KGlobalRegs; */
/* OBSOLETE       To.Offset = (regno - GR96_REGNUM) + 96; */
/* OBSOLETE       result = UDIWrite (&From, To, Count, Size, &CountDone, HostEndian); */
/* OBSOLETE     } */
/* OBSOLETE   else if (regno >= LR0_REGNUM && regno < LR0_REGNUM + 128) */
/* OBSOLETE     { */
/* OBSOLETE       To.Space = UDI29KLocalRegs; */
/* OBSOLETE       To.Offset = (regno - LR0_REGNUM); */
/* OBSOLETE       result = UDIWrite (&From, To, Count, Size, &CountDone, HostEndian); */
/* OBSOLETE     } */
/* OBSOLETE   else if (regno >= FPE_REGNUM && regno <= EXO_REGNUM) */
/* OBSOLETE     return 0;			/* Pretend Success */ */
/* OBSOLETE   else if (regno == PC_REGNUM) */
/* OBSOLETE     { */
/* OBSOLETE       /* PC1 via UDI29KPC */ */
/* OBSOLETE  */
/* OBSOLETE       To.Space = UDI29KPC; */
/* OBSOLETE       To.Offset = 0;		/* PC1 */ */
/* OBSOLETE       result = UDIWrite (&From, To, Count, Size, &CountDone, HostEndian); */
/* OBSOLETE  */
/* OBSOLETE       /* Writing to this loc actually changes the values of pc0 & pc1 */ */
/* OBSOLETE  */
/* OBSOLETE       register_valid[PC_REGNUM] = 0;	/* pc1 */ */
/* OBSOLETE       register_valid[NPC_REGNUM] = 0;	/* pc0 */ */
/* OBSOLETE     } */
/* OBSOLETE   else */
/* OBSOLETE     /* An unprotected or protected special register */ */
/* OBSOLETE     { */
/* OBSOLETE       To.Space = UDI29KSpecialRegs; */
/* OBSOLETE       To.Offset = regnum_to_srnum (regno); */
/* OBSOLETE       result = UDIWrite (&From, To, Count, Size, &CountDone, HostEndian); */
/* OBSOLETE     } */
/* OBSOLETE  */
/* OBSOLETE   if (result != 0) */
/* OBSOLETE     error ("UDIWrite() failed in store_registers"); */
/* OBSOLETE  */
/* OBSOLETE   return 0; */
/* OBSOLETE } */
/* OBSOLETE /********************************************************** REGNUM_TO_SRNUM */ */
/* OBSOLETE /*  */
/* OBSOLETE  * Convert a gdb special register number to a 29000 special register number. */
/* OBSOLETE  */ */
/* OBSOLETE static int */
/* OBSOLETE regnum_to_srnum (int regno) */
/* OBSOLETE { */
/* OBSOLETE   switch (regno) */
/* OBSOLETE     { */
/* OBSOLETE     case VAB_REGNUM: */
/* OBSOLETE       return (0); */
/* OBSOLETE     case OPS_REGNUM: */
/* OBSOLETE       return (1); */
/* OBSOLETE     case CPS_REGNUM: */
/* OBSOLETE       return (2); */
/* OBSOLETE     case CFG_REGNUM: */
/* OBSOLETE       return (3); */
/* OBSOLETE     case CHA_REGNUM: */
/* OBSOLETE       return (4); */
/* OBSOLETE     case CHD_REGNUM: */
/* OBSOLETE       return (5); */
/* OBSOLETE     case CHC_REGNUM: */
/* OBSOLETE       return (6); */
/* OBSOLETE     case RBP_REGNUM: */
/* OBSOLETE       return (7); */
/* OBSOLETE     case TMC_REGNUM: */
/* OBSOLETE       return (8); */
/* OBSOLETE     case TMR_REGNUM: */
/* OBSOLETE       return (9); */
/* OBSOLETE     case NPC_REGNUM: */
/* OBSOLETE       return (USE_SHADOW_PC ? (20) : (10)); */
/* OBSOLETE     case PC_REGNUM: */
/* OBSOLETE       return (USE_SHADOW_PC ? (21) : (11)); */
/* OBSOLETE     case PC2_REGNUM: */
/* OBSOLETE       return (USE_SHADOW_PC ? (22) : (12)); */
/* OBSOLETE     case MMU_REGNUM: */
/* OBSOLETE       return (13); */
/* OBSOLETE     case LRU_REGNUM: */
/* OBSOLETE       return (14); */
/* OBSOLETE     case IPC_REGNUM: */
/* OBSOLETE       return (128); */
/* OBSOLETE     case IPA_REGNUM: */
/* OBSOLETE       return (129); */
/* OBSOLETE     case IPB_REGNUM: */
/* OBSOLETE       return (130); */
/* OBSOLETE     case Q_REGNUM: */
/* OBSOLETE       return (131); */
/* OBSOLETE     case ALU_REGNUM: */
/* OBSOLETE       return (132); */
/* OBSOLETE     case BP_REGNUM: */
/* OBSOLETE       return (133); */
/* OBSOLETE     case FC_REGNUM: */
/* OBSOLETE       return (134); */
/* OBSOLETE     case CR_REGNUM: */
/* OBSOLETE       return (135); */
/* OBSOLETE     case FPE_REGNUM: */
/* OBSOLETE       return (160); */
/* OBSOLETE     case INTE_REGNUM: */
/* OBSOLETE       return (161); */
/* OBSOLETE     case FPS_REGNUM: */
/* OBSOLETE       return (162); */
/* OBSOLETE     case EXO_REGNUM: */
/* OBSOLETE       return (164); */
/* OBSOLETE     default: */
/* OBSOLETE       return (255);		/* Failure ? */ */
/* OBSOLETE     } */
/* OBSOLETE } */
/* OBSOLETE /****************************************************************************/ */
/* OBSOLETE /* */
/* OBSOLETE  * Determine the Target memory space qualifier based on the addr.  */
/* OBSOLETE  * FIXME: Can't distinguis I_ROM/D_ROM.   */
/* OBSOLETE  * FIXME: Doesn't know anything about I_CACHE/D_CACHE. */
/* OBSOLETE  */ */
/* OBSOLETE static CPUSpace */
/* OBSOLETE udi_memory_space (CORE_ADDR addr) */
/* OBSOLETE { */
/* OBSOLETE   UDIUInt32 tstart = IMemStart; */
/* OBSOLETE   UDIUInt32 tend = tstart + IMemSize; */
/* OBSOLETE   UDIUInt32 dstart = DMemStart; */
/* OBSOLETE   UDIUInt32 dend = tstart + DMemSize; */
/* OBSOLETE   UDIUInt32 rstart = RMemStart; */
/* OBSOLETE   UDIUInt32 rend = tstart + RMemSize; */
/* OBSOLETE  */
/* OBSOLETE   if (((UDIUInt32) addr >= tstart) && ((UDIUInt32) addr < tend)) */
/* OBSOLETE     { */
/* OBSOLETE       return UDI29KIRAMSpace; */
/* OBSOLETE     } */
/* OBSOLETE   else if (((UDIUInt32) addr >= dstart) && ((UDIUInt32) addr < dend)) */
/* OBSOLETE     { */
/* OBSOLETE       return UDI29KDRAMSpace; */
/* OBSOLETE     } */
/* OBSOLETE   else if (((UDIUInt32) addr >= rstart) && ((UDIUInt32) addr < rend)) */
/* OBSOLETE     { */
/* OBSOLETE       /* FIXME: how do we determine between D_ROM and I_ROM */ */
/* OBSOLETE       return UDI29KIROMSpace; */
/* OBSOLETE     } */
/* OBSOLETE   else				/* FIXME: what do me do now? */ */
/* OBSOLETE     return UDI29KDRAMSpace;	/* Hmmm! */ */
/* OBSOLETE } */
/* OBSOLETE /*********************************************************************** STUBS */
/* OBSOLETE */ */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE convert16 (void) */
/* OBSOLETE {; */
/* OBSOLETE } */
/* OBSOLETE void */
/* OBSOLETE convert32 (void) */
/* OBSOLETE {; */
/* OBSOLETE } */
/* OBSOLETE struct ui_file *EchoFile = 0;	/* used for debugging */ */
/* OBSOLETE int QuietMode = 0;		/* used for debugging */ */
/* OBSOLETE  */
/* OBSOLETE #ifdef NO_HIF_SUPPORT */
/* OBSOLETE service_HIF (union msg_t *msg) */
/* OBSOLETE { */
/* OBSOLETE   return (0);			/* Emulate a failure */ */
/* OBSOLETE } */
/* OBSOLETE #endif */
/* OBSOLETE  */
/* OBSOLETE /* Target_ops vector.  Not static because there does not seem to be */
/* OBSOLETE    any portable way to do a forward declaration of a static variable. */
/* OBSOLETE    The RS/6000 doesn't like "extern" followed by "static"; SunOS */
/* OBSOLETE    /bin/cc doesn't like "static" twice.  */ */
/* OBSOLETE  */
/* OBSOLETE struct target_ops udi_ops; */
/* OBSOLETE  */
/* OBSOLETE static void */
/* OBSOLETE init_udi_ops (void) */
/* OBSOLETE { */
/* OBSOLETE   udi_ops.to_shortname = "udi"; */
/* OBSOLETE   udi_ops.to_longname = "Remote UDI connected TIP"; */
/* OBSOLETE   udi_ops.to_doc = "Remote debug an AMD 29k using UDI socket connection to TIP process.\n\ */
/* OBSOLETE Arguments are\n\ */
/* OBSOLETE `configuration-id AF_INET hostname port-number'\n\ */
/* OBSOLETE To connect via the network, where hostname and port-number specify the\n\ */
/* OBSOLETE host and port where you can connect via UDI.\n\ */
/* OBSOLETE configuration-id is unused.\n\ */
/* OBSOLETE \n\ */
/* OBSOLETE `configuration-id AF_UNIX socket-name tip-program'\n\ */
/* OBSOLETE To connect using a local connection to the \"tip.exe\" program which is\n\ */
/* OBSOLETE     supplied by AMD.  If socket-name specifies an AF_UNIX socket then the\n\ */
/* OBSOLETE     tip program must already be started; connect to it using that socket.\n\ */
/* OBSOLETE     If not, start up tip-program, which should be the name of the tip\n\ */
/* OBSOLETE     program.  If appropriate, the PATH environment variable is searched.\n\ */
/* OBSOLETE     configuration-id is unused.\n\ */
/* OBSOLETE \n\ */
/* OBSOLETE `configuration-id'\n\ */
/* OBSOLETE     Look up the configuration in ./udi_soc or /etc/udi_soc, which\n\ */
/* OBSOLETE     are files containing lines in the above formats.  configuration-id is\n\ */
/* OBSOLETE     used to pick which line of the file to use."; */
/* OBSOLETE   udi_ops.to_open = udi_open; */
/* OBSOLETE   udi_ops.to_close = udi_close; */
/* OBSOLETE   udi_ops.to_attach = udi_attach; */
/* OBSOLETE   udi_ops.to_detach = udi_detach; */
/* OBSOLETE   udi_ops.to_resume = udi_resume; */
/* OBSOLETE   udi_ops.to_wait = udi_wait; */
/* OBSOLETE   udi_ops.to_fetch_registers = udi_fetch_registers; */
/* OBSOLETE   udi_ops.to_store_registers = udi_store_registers; */
/* OBSOLETE   udi_ops.to_prepare_to_store = udi_prepare_to_store; */
/* OBSOLETE   udi_ops.to_xfer_memory = udi_xfer_inferior_memory; */
/* OBSOLETE   udi_ops.to_files_info = udi_files_info; */
/* OBSOLETE   udi_ops.to_insert_breakpoint = udi_insert_breakpoint; */
/* OBSOLETE   udi_ops.to_remove_breakpoint = udi_remove_breakpoint; */
/* OBSOLETE   udi_ops.to_terminal_init = 0; */
/* OBSOLETE   udi_ops.to_terminal_inferior = 0; */
/* OBSOLETE   udi_ops.to_terminal_ours_for_output = 0; */
/* OBSOLETE   udi_ops.to_terminal_ours = 0; */
/* OBSOLETE   udi_ops.to_terminal_info = 0; */
/* OBSOLETE   udi_ops.to_kill = udi_kill; */
/* OBSOLETE   udi_ops.to_load = udi_load; */
/* OBSOLETE   udi_ops.to_lookup_symbol = 0; */
/* OBSOLETE   udi_ops.to_create_inferior = udi_create_inferior; */
/* OBSOLETE   udi_ops.to_mourn_inferior = udi_mourn; */
/* OBSOLETE   udi_ops.to_can_run = 0; */
/* OBSOLETE   udi_ops.to_notice_signals = 0; */
/* OBSOLETE   udi_ops.to_thread_alive = 0; */
/* OBSOLETE   udi_ops.to_stop = 0; */
/* OBSOLETE   udi_ops.to_stratum = process_stratum; */
/* OBSOLETE   udi_ops.DONT_USE = 0; */
/* OBSOLETE   udi_ops.to_has_all_memory = 1; */
/* OBSOLETE   udi_ops.to_has_memory = 1; */
/* OBSOLETE   udi_ops.to_has_stack = 1; */
/* OBSOLETE   udi_ops.to_has_registers = 1; */
/* OBSOLETE   udi_ops.to_has_execution = 1; */
/* OBSOLETE   udi_ops.to_sections = 0; */
/* OBSOLETE   udi_ops.to_sections_end = 0; */
/* OBSOLETE   udi_ops.to_magic = OPS_MAGIC; */
/* OBSOLETE }; */
/* OBSOLETE  */
/* OBSOLETE void */
/* OBSOLETE _initialize_remote_udi (void) */
/* OBSOLETE { */
/* OBSOLETE   init_udi_ops (); */
/* OBSOLETE   add_target (&udi_ops); */
/* OBSOLETE } */
