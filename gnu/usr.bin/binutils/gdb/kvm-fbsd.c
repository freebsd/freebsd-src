/* Kernel core dump functions below target vector, for GDB.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995
   Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/* $FreeBSD$ */

/*
 * This works like "remote" but, you use it like this:
 *     target kcore /dev/mem
 * or
 *     target kcore /var/crash/host/core.0
 *
 * This way makes it easy to short-circut the whole bfd monster,
 * and direct the inferior stuff to our libkvm implementation.
 *
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <kvm.h>
#include <sys/sysctl.h>
#include <paths.h>
#include <readline/tilde.h>
#include <machine/frame.h>

#include "defs.h"
#include "gdb_string.h"
#include "frame.h"  /* required by inferior.h */
#include "inferior.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "command.h"
#include "bfd.h"
#include "target.h"
#include "gdbcore.h"

static void
kcore_files_info (struct target_ops *);

static void
kcore_close (int);

static void
get_kcore_registers (int);

static int
xfer_mem (CORE_ADDR, char *, int, int, struct mem_attrib *,
          struct target_ops *);

static int
xfer_umem (CORE_ADDR, char *, int, int);

static char		*core_file;
static kvm_t		*core_kd;
static struct pcb	cur_pcb;
static struct kinfo_proc *cur_proc;

static struct target_ops kcore_ops;

int kernel_debugging;
int kernel_writablecore;

/* Read the "thing" at kernel address 'addr' into the space pointed to
   by point.  The length of the "thing" is determined by the type of p.
   Result is non-zero if transfer fails.  */

#define kvread(addr, p) \
  (target_read_memory ((CORE_ADDR) (addr), (char *) (p), sizeof (*(p))))

static CORE_ADDR
ksym_kernbase (void)
{
  static CORE_ADDR kernbase;
  struct minimal_symbol *sym;

  if (kernbase == 0)
    {
      sym = lookup_minimal_symbol ("kernbase", NULL, NULL);
      if (sym == NULL) {
	kernbase = KERNBASE;
      } else {
	kernbase = SYMBOL_VALUE_ADDRESS (sym);
      }
    }
  return kernbase;
}

#define	KERNOFF		(ksym_kernbase ())
#define	INKERNEL(x)	((x) >= KERNOFF)

CORE_ADDR
ksym_lookup(const char *name)
{
  struct minimal_symbol *sym;

  sym = lookup_minimal_symbol (name, NULL, NULL);
  if (sym == NULL)
    error ("kernel symbol `%s' not found.", name);

  return SYMBOL_VALUE_ADDRESS (sym);
}

/* Provide the address of an initial PCB to use.
   If this is a crash dump, try for "dumppcb".
   If no "dumppcb" or it's /dev/mem, use proc0.
   Return the core address of the PCB we found.  */

static CORE_ADDR
initial_pcb (void)
{
  struct minimal_symbol *sym;
  CORE_ADDR addr;
  void *val;

  /* Make sure things are open...  */
  if (!core_kd || !core_file)
    return (0);

  /* If this is NOT /dev/mem try for dumppcb.  */
  if (strncmp (core_file, _PATH_DEV, sizeof _PATH_DEV - 1))
    {
      sym = lookup_minimal_symbol ("dumppcb", NULL, NULL);
      if (sym != NULL)
	{
	  addr = SYMBOL_VALUE_ADDRESS (sym);
	  return (addr);
	}
  }

  /* OK, just use thread0's pcb.  Note that curproc might
     not exist, and if it does, it will point to gdb.
     Therefore, just use proc0 and let the user set
     some other context if they care about it.  */

  addr = ksym_lookup ("thread0");
  if (kvread (addr, &val))
    {
      error ("cannot read thread0 pointer at %x\n", addr);
      val = 0;
    }
  else
    {
      /* Read the PCB address in thread structure.  */
      addr += offsetof (struct thread, td_pcb);
      if (kvread (addr, &val))
	{
	  error ("cannot read thread0->td_pcb pointer at %x\n", addr);
	  val = 0;
	}
    }

  /* thread0 is wholly in the kernel and cur_proc is only used for
     reading user mem, so no point in setting this up.  */
  cur_proc = 0;

  return ((CORE_ADDR)val);
}

/* Set the current context to that of the PCB struct at the system address
   passed.  */

static int
set_context (CORE_ADDR addr)
{
  CORE_ADDR procaddr = 0;

  if (kvread (addr, &cur_pcb))
    error ("cannot read pcb at %#x", addr);

  /* Fetch all registers from core file.  */
  target_fetch_registers (-1);

  /* Now, set up the frame cache, and print the top of stack.  */
  flush_cached_frames ();
  set_current_frame (create_new_frame (read_fp (), read_pc ()));
  select_frame (get_current_frame (), 0);
  return (0);
}

/* Discard all vestiges of any previous core file and mark data and stack
   spaces as empty.  */

/* ARGSUSED */
static void
kcore_close (int quitting)
{

  inferior_ptid = null_ptid;	/* Avoid confusion from thread stuff.  */

  if (core_kd)
    {
      kvm_close (core_kd);
      free (core_file);
      core_file = NULL;
      core_kd = NULL;
    }
}

/* This routine opens and sets up the core file bfd.  */

static void
kcore_open (char *filename /* the core file */, int from_tty)
{
  kvm_t *kd;
  const char *p;
  struct cleanup *old_chain;
  char buf[256], *cp;
  int ontop;
  CORE_ADDR addr;

  target_preopen (from_tty);

  /* The exec file is required for symbols.  */
  if (exec_bfd == NULL)
    error ("No kernel exec file specified");

  if (core_kd)
    {
      error ("No core file specified."
	     "  (Use `detach' to stop debugging a core file.)");
      return;
    }

  if (!filename)
    {
      error ("No core file specified.");
      return;
    }

  filename = tilde_expand (filename);
  if (filename[0] != '/')
    {
      cp = concat (current_directory, "/", filename, NULL);
      free (filename);
      filename = cp;
    }

  old_chain = make_cleanup (free, filename);

  kd = kvm_open (bfd_get_filename(exec_bfd), filename, NULL,
		 kernel_writablecore ? O_RDWR: O_RDONLY, 0);
  if (kd == NULL)
    {
      perror_with_name (filename);
      return;
    }

  /* Looks semi-reasonable.  Toss the old core file and work on the new.  */

  discard_cleanups (old_chain);		/* Don't free filename any more.  */
  core_file = filename;
  unpush_target (&kcore_ops);
  ontop = !push_target (&kcore_ops);

  /* Note unpush_target (above) calls kcore_close.  */
  core_kd = kd;

  /* Print out the panic string if there is one.  */
  if (kvread (ksym_lookup ("panicstr"), &addr) == 0 &&
      addr != 0 && 
      target_read_memory (addr, buf, sizeof(buf)) == 0)
    {

      for (cp = buf; cp < &buf[sizeof(buf)] && *cp; cp++)
	if (!isascii (*cp) || (!isprint (*cp) && !isspace (*cp)))
	  *cp = '?';
      *cp = '\0';
      if (buf[0] != '\0')
	printf_filtered ("panic: %s\n", buf);
    }

  /* Print all the panic messages if possible.  */
  if (symfile_objfile != NULL)
    {
      printf ("panic messages:\n---\n");
      snprintf (buf, sizeof buf,
                "/sbin/dmesg -N %s -M %s | \
                 /usr/bin/awk '/^(panic:|Fatal trap) / { printing = 1 } \
                               { if (printing) print $0 }'",
                symfile_objfile->name, filename);
      fflush (stdout);
      system (buf);
      printf ("---\n");
    }

  if (!ontop)
    {
      warning ("you won't be able to access this core file until you terminate\n"
		"your %s; do ``info files''", target_longname);
      return;
    }

  /* Now, set up process context, and print the top of stack.  */
  (void)set_context (initial_pcb());
  print_stack_frame (selected_frame, selected_frame_level, 1);
}

static void
kcore_detach (char *args, int from_tty)
{
  if (args)
    error ("Too many arguments");
  unpush_target (&kcore_ops);
  reinit_frame_cache ();
  if (from_tty)
    printf_filtered ("No kernel core file now.\n");
}

#ifdef __alpha__

#include "alpha/tm-alpha.h"
#ifndef S0_REGNUM
#define S0_REGNUM (T7_REGNUM+1)
#endif

fetch_kcore_registers (struct pcb *pcbp)
{

  /* First clear out any garbage.  */
  memset (registers, '\0', REGISTER_BYTES);

  /* SP */
  *(long *) &registers[REGISTER_BYTE (SP_REGNUM)] =
    pcbp->pcb_hw.apcb_ksp;

  /* S0 through S6 */
  memcpy (&registers[REGISTER_BYTE (S0_REGNUM)],
          &pcbp->pcb_context[0], 7 * sizeof (long));

  /* PC */
  *(long *) &registers[REGISTER_BYTE (PC_REGNUM)] =
    pcbp->pcb_context[7];

  registers_fetched ();
}


CORE_ADDR
fbsd_kern_frame_saved_pc (struct frame_info *fi)
{
  struct minimal_symbol *sym;
  CORE_ADDR this_saved_pc;

  this_saved_pc = alpha_frame_saved_pc (fi);

  sym = lookup_minimal_symbol_by_pc (this_saved_pc);

  if (sym != NULL &&
      (strcmp (SYMBOL_NAME (sym), "XentArith") == 0 ||
       strcmp (SYMBOL_NAME (sym), "XentIF") == 0 ||
       strcmp (SYMBOL_NAME (sym), "XentInt") == 0 ||
       strcmp (SYMBOL_NAME (sym), "XentMM") == 0 ||
       strcmp (SYMBOL_NAME (sym), "XentSys") == 0 ||
       strcmp (SYMBOL_NAME (sym), "XentUna") == 0 ||
       strcmp (SYMBOL_NAME (sym), "XentRestart") == 0))
    {
      return (read_memory_integer (fi->frame + 32 * 8, 8));
    }
  else
    {
      return (this_saved_pc);
    }
}

#endif /* __alpha__ */

#ifdef __i386__

static CORE_ADDR
ksym_maxuseraddr (void)
{
  static CORE_ADDR maxuseraddr;
  struct minimal_symbol *sym;

  if (maxuseraddr == 0)
    {
      sym = lookup_minimal_symbol ("PTmap", NULL, NULL);
      if (sym == NULL) {
	maxuseraddr = VM_MAXUSER_ADDRESS;
      } else {
	maxuseraddr = SYMBOL_VALUE_ADDRESS (sym);
      }
    }
  return maxuseraddr;
}


/* Symbol names of kernel entry points.  Use special frames.  */
#define	KSYM_TRAP	"calltrap"
#define	KSYM_INTR	"Xintr"
#define	KSYM_FASTINTR	"Xfastintr"
#define	KSYM_OLDSYSCALL	"Xlcall_syscall"
#define	KSYM_SYSCALL	"Xint0x80_syscall"

/* The following is FreeBSD-specific hackery to decode special frames
   and elide the assembly-language stub.  This could be made faster by
   defining a frame_type field in the machine-dependent frame information,
   but we don't think that's too important right now.  */
enum frametype { tf_normal, tf_trap, tf_interrupt, tf_syscall };

CORE_ADDR
fbsd_kern_frame_saved_pc (struct frame_info *fr)
{
  struct minimal_symbol *sym;
  CORE_ADDR this_saved_pc;
  enum frametype frametype;

  this_saved_pc = read_memory_integer (fr->frame + 4, 4);
  sym = lookup_minimal_symbol_by_pc (this_saved_pc);
  frametype = tf_normal;
  if (sym != NULL)
    {
      if (strcmp (SYMBOL_NAME (sym), KSYM_TRAP) == 0)
	frametype = tf_trap;
      else
	if (strncmp (SYMBOL_NAME (sym), KSYM_INTR,
	    strlen (KSYM_INTR)) == 0 || strncmp (SYMBOL_NAME(sym),
	    KSYM_FASTINTR, strlen (KSYM_FASTINTR)) == 0)
	  frametype = tf_interrupt;
      else
	if (strcmp (SYMBOL_NAME (sym), KSYM_SYSCALL) == 0 ||
	    strcmp (SYMBOL_NAME (sym), KSYM_OLDSYSCALL) == 0)
	  frametype = tf_syscall;
    }

  switch (frametype)
    {
      case tf_normal:
        return (this_saved_pc);
#define oEIP   offsetof (struct trapframe, tf_eip)

      case tf_trap:
	return (read_memory_integer (fr->frame + 8 + oEIP, 4));

      case tf_interrupt:
	return (read_memory_integer (fr->frame + 12 + oEIP, 4));

      case tf_syscall:
	return (read_memory_integer (fr->frame + 8 + oEIP, 4));
#undef oEIP
    }
}

static int
fetch_kcore_registers (struct pcb *pcb)
{
  int i;
  int noreg;

  /* Get the register values out of the sys pcb and store them where
     `read_register' will find them.  */
  /*
   * XXX many registers aren't available.
   * XXX for the non-core case, the registers are stale - they are for
   *     the last context switch to the debugger.
   * XXX gcc's register numbers aren't all #defined in tm-i386.h.
   */
  noreg = 0;
  for (i = 0; i < 3; ++i)		/* eax,ecx,edx */
    supply_register (i, (char *)&noreg);

  supply_register (3, (char *) &pcb->pcb_ebx);
  supply_register (SP_REGNUM, (char *) &pcb->pcb_esp);
  supply_register (FP_REGNUM, (char *) &pcb->pcb_ebp);
  supply_register (6, (char *) &pcb->pcb_esi);
  supply_register (7, (char *) &pcb->pcb_edi);
  supply_register (PC_REGNUM, (char *) &pcb->pcb_eip);

  for (i = 9; i < 14; ++i)		/* eflags, cs, ss, ds, es, fs */
    supply_register (i, (char *) &noreg);
  supply_register (15, (char *) &pcb->pcb_gs);

  /* XXX 80387 registers?  */
}

#endif /* __i386__ */

#ifdef __sparc64__

/*
#include "sparc/tm-sp64.h"
*/

fetch_kcore_registers (struct pcb *pcbp)
{
}


CORE_ADDR
fbsd_kern_frame_saved_pc (struct frame_info *fi)
{
	return NULL;
}

#endif /* __sparc64__ */

/* Get the registers out of a core file.  This is the machine-
   independent part.  Fetch_core_registers is the machine-dependent
   part, typically implemented in the xm-file for each architecture.  */

/* We just get all the registers, so we don't use regno.  */

/* ARGSUSED */
static void
get_kcore_registers (int regno)
{

  /* XXX - Only read the pcb when set_context() is called.
     When looking at a live kernel this may be a problem,
     but the user can do another "proc" or "pcb" command to
     grab a new copy of the pcb...  */

  /* Zero out register set then fill in the ones we know about.  */
  fetch_kcore_registers (&cur_pcb);
}

static void
kcore_files_info (t)
  struct target_ops *t;
{
  printf_filtered ("\t`%s'\n", core_file);
}

/* If mourn is being called in all the right places, this could be say
   `gdb internal error' (since generic_mourn calls breakpoint_init_inferior). */

static int
ignore (CORE_ADDR addr, char *contents)
{
  return 0;
}

static int
xfer_kmem (CORE_ADDR memaddr, char *myaddr, int len, int write,
	   struct mem_attrib *attrib, struct target_ops *target)
{
  int n;


  if (!INKERNEL (memaddr))
    return xfer_umem (memaddr, myaddr, len, write);

  if (core_kd == NULL)
    return 0;

  if (write)
    n = kvm_write (core_kd, memaddr, myaddr, len);
  else
    n = kvm_read (core_kd, memaddr, myaddr, len) ;
  if (n < 0) {
    fprintf_unfiltered (gdb_stderr, "can not access 0x%x, %s\n",
			memaddr, kvm_geterr (core_kd));
    n = 0;
  }

  return n;
}


static int
xfer_umem (CORE_ADDR memaddr, char *myaddr, int len, int write /* ignored */)
{
  int n = 0;

  if (cur_proc == 0)
    {
      error ("---Can't read userspace from dump, or kernel process---\n");
      return 0;
    }

  if (write)
    error ("kvm_uwrite unimplemented\n");
  else
    n = kvm_uread (core_kd, cur_proc, memaddr, myaddr, len) ;

  if (n < 0)
    return 0;

  return n;
}

static void
set_proc_cmd (char *arg, int from_tty)
{
  CORE_ADDR addr, pid_addr, first_td;
  void *val;
  struct kinfo_proc *kp;
  int cnt;
  pid_t pid;

  if (!arg)
    error_no_arg ("proc address for the new context");

  if (core_kd == NULL)
    error ("no kernel core file");

  addr = (CORE_ADDR) parse_and_eval_address (arg);
  
  if (!INKERNEL (addr))
    {
      kp = kvm_getprocs (core_kd, KERN_PROC_PID, addr, &cnt);
      if (!cnt)
	error ("invalid pid");	  
      addr = (CORE_ADDR)kp->ki_paddr;
      cur_proc = kp;
    }
  else
    {
      /* Update cur_proc.  */
      pid_addr = addr + offsetof (struct proc, p_pid);
      if (kvread (pid_addr, &pid))
	error ("cannot read pid ptr");
      cur_proc = kvm_getprocs (core_kd, KERN_PROC_PID, pid, &cnt);
      if (!cnt)
	error("invalid pid");	  
    }

  /* Find the first thread in the process.  XXXKSE  */
  addr += offsetof (struct proc, p_threads.tqh_first);
  if (kvread (addr, &first_td))
    error ("cannot read thread ptr");

  /* Read the PCB address in thread structure. */
  addr = first_td + offsetof (struct thread, td_pcb);
  if (kvread (addr, &val))
    error("cannot read pcb ptr");

  /* Read the PCB address in proc structure. */
  if (set_context ((CORE_ADDR) val))
    error ("invalid proc address");
}

void
_initialize_kcorelow (void)
{
  kcore_ops.to_shortname = "kcore";
  kcore_ops.to_longname = "Kernel core dump file";
  kcore_ops.to_doc =
    "Use a core file as a target.  Specify the filename of the core file.";
  kcore_ops.to_open = kcore_open;
  kcore_ops.to_close = kcore_close;
  kcore_ops.to_attach = find_default_attach;
  kcore_ops.to_detach = kcore_detach;
  kcore_ops.to_fetch_registers = get_kcore_registers;
  kcore_ops.to_xfer_memory = xfer_kmem;
  kcore_ops.to_files_info = kcore_files_info;
  kcore_ops.to_create_inferior = find_default_create_inferior;
  kcore_ops.to_stratum = kcore_stratum;
  kcore_ops.to_has_memory = 1;
  kcore_ops.to_has_stack = 1;
  kcore_ops.to_has_registers = 1;
  kcore_ops.to_magic = OPS_MAGIC;

  add_target (&kcore_ops);
  add_com ("proc", class_obscure, set_proc_cmd, "Set current process context");
}
