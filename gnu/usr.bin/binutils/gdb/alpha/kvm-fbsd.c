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
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <kvm.h>
#include <paths.h>

#include "defs.h"
#include "gdb_string.h"
#include "frame.h"  /* required by inferior.h */
#include "inferior.h"
#include "symtab.h"
#include "command.h"
#include "bfd.h"
#include "target.h"
#include "gdbcore.h"

static void
kcore_files_info PARAMS ((struct target_ops *));

static void
kcore_close PARAMS ((int));

static void
get_kcore_registers PARAMS ((int));

static int
xfer_mem PARAMS ((CORE_ADDR, char *, int, int, struct target_ops *));

static int
xfer_umem PARAMS ((CORE_ADDR, char *, int, int));

static char		*core_file;
static kvm_t		*core_kd;
static struct pcb	cur_pcb;

static struct target_ops kcore_ops;
int kernel_writablecore;

/*
 * Read the "thing" at kernel address 'addr' into the space pointed to
 * by point.  The length of the "thing" is determined by the type of p.
 * Result is non-zero if transfer fails.
 */
#define kvread(addr, p) \
(target_read_memory((CORE_ADDR)(addr), (char *)(p), sizeof(*(p))))


CORE_ADDR
ksym_lookup(name)
  const char *name;
{
  struct minimal_symbol *sym;

  sym = lookup_minimal_symbol(name, NULL, NULL);
  if (sym == NULL)
    error("kernel symbol `%s' not found.", name);

  return SYMBOL_VALUE_ADDRESS(sym);
}

/*
 * Provide the address of an initial PCB to use.
 * If this is a crash dump, try for "dumppcb".
 * If no "dumppcb" or it's /dev/mem, use proc0.
 * Return the core address of the PCB we found.
 */
static CORE_ADDR
initial_pcb()
{
  struct minimal_symbol *sym;
  CORE_ADDR addr;
  void *val;

  /* Make sure things are open... */
  if (!core_kd || !core_file)
    return (0);

  /* If this is NOT /dev/mem try for dumppcb. */
  if (strncmp(core_file, _PATH_DEV, sizeof _PATH_DEV - 1)) {
    sym = lookup_minimal_symbol("dumppcb", NULL, NULL);
    if (sym != NULL) {
      addr = SYMBOL_VALUE_ADDRESS(sym);
      return (addr);
    }
  }

  /*
   * OK, just use proc0pcb.  Note that curproc might
   * not exist, and if it does, it will point to gdb.
   * Therefore, just use proc0 and let the user set
   * some other context if they care about it.
   */
  addr = ksym_lookup("proc0paddr");
  if (kvread(addr, &val)) {
    error("cannot read proc0paddr pointer at %x\n", addr);
    val = 0;
  }

  return ((CORE_ADDR)val);
}

/*
 * Set the current context to that of the PCB struct
 * at the system address passed.
 */
static int
set_context(addr)
  CORE_ADDR addr;
{

  if (kvread(addr, &cur_pcb))
    error("cannot read pcb at %#x", addr);

  /* Fetch all registers from core file */
  target_fetch_registers (-1);

  /* Now, set up the frame cache, and print the top of stack */
  flush_cached_frames();
  set_current_frame (create_new_frame (read_fp (), read_pc ()));
  select_frame (get_current_frame (), 0);
  return (0);
}

/* Discard all vestiges of any previous core file and mark data and stack
   spaces as empty.  */

/* ARGSUSED */
static void
kcore_close (quitting)
     int quitting;
{

  inferior_pid = 0;		/* Avoid confusion from thread stuff */

  if (core_kd) {
    kvm_close(core_kd);
    free(core_file);
    core_file = NULL;
    core_kd = NULL;
  }
}

/* This routine opens and sets up the core file bfd.  */

static void
kcore_open (filename, from_tty)
     char *filename;	/* the core file */
     int from_tty;
{
  kvm_t		*kd;
  const char *p;
  struct cleanup *old_chain;
  char buf[256], *cp;
  int ontop;
  CORE_ADDR addr;

  target_preopen (from_tty);

  /* The exec file is required for symbols. */
  if (exec_bfd == NULL)
    error("No kernel exec file specified");

  if (core_kd) {
    error ("No core file specified."
	   "  (Use `detach' to stop debugging a core file.)");
    return;
  }

  if (!filename) {
    error ("No core file specified.");
    return;
  }

  filename = tilde_expand (filename);
  if (filename[0] != '/') {
    cp = concat (current_directory, "/", filename, NULL);
    free (filename);
    filename = cp;
  }

  old_chain = make_cleanup (free, filename);

  kd = kvm_open (bfd_get_filename(exec_bfd), filename, NULL,
		 kernel_writablecore ? O_RDWR: O_RDONLY, 0);
  if (kd == NULL) {
    perror_with_name (filename);
    return;
  }

  /* Looks semi-reasonable.  Toss the old core file and work on the new.  */

  discard_cleanups (old_chain);		/* Don't free filename any more */
  core_file = filename;
  unpush_target (&kcore_ops);
  ontop = !push_target (&kcore_ops);

  /* Note unpush_target (above) calls kcore_close. */
  core_kd = kd;

  /* print out the panic string if there is one */
  if (kvread(ksym_lookup("panicstr"), &addr) == 0 &&
      addr != 0 && 
      target_read_memory(addr, buf, sizeof(buf)) == 0) {

    for (cp = buf; cp < &buf[sizeof(buf)] && *cp; cp++)
      if (!isascii(*cp) || (!isprint(*cp) && !isspace(*cp)))
	*cp = '?';
    *cp = '\0';
    if (buf[0] != '\0')
      printf_filtered("panic: %s\n", buf);
  }

  if (!ontop) {
    warning (
"you won't be able to access this core file until you terminate\n\
your %s; do ``info files''", target_longname);
    return;
  }

  /* Now, set up process context, and print the top of stack */
  (void)set_context(initial_pcb());
  print_stack_frame (selected_frame, selected_frame_level, 1);
}

static void
kcore_detach (args, from_tty)
     char *args;
     int from_tty;
{
  if (args)
    error ("Too many arguments");
  unpush_target (&kcore_ops);
  reinit_frame_cache ();
  if (from_tty)
    printf_filtered ("No kernel core file now.\n");
}

/* Get the registers out of a core file.  This is the machine-
   independent part.  Fetch_core_registers is the machine-dependent
   part, typically implemented in the xm-file for each architecture.  */

/* We just get all the registers, so we don't use regno.  */

/* ARGSUSED */
static void
get_kcore_registers (regno)
     int regno;
{

  /*
   * XXX - Only read the pcb when set_context() is called.
   * When looking at a live kernel this may be a problem,
   * but the user can do another "proc" or "pcb" command to
   * grab a new copy of the pcb...
   */

  /*
   * Zero out register set then fill in the ones we know about.
   */
  fetch_kcore_registers (&cur_pcb);
}

static void
kcore_files_info (t)
  struct target_ops *t;
{
  printf_filtered ("\t`%s'\n", core_file);
}

/* If mourn is being called in all the right places, this could be say
   `gdb internal error' (since generic_mourn calls breakpoint_init_inferior).  */

static int
ignore (addr, contents)
     CORE_ADDR addr;
     char *contents;
{
  return 0;
}

static int
xfer_kmem (memaddr, myaddr, len, write, target)
  CORE_ADDR memaddr;
  char *myaddr;
  int len;
  int write;
  struct target_ops *target;
{
  int n;

#if 0	/* XXX */
  if (it is a user address)
    return xfer_umem(memaddr, myaddr, len, write);
#endif

  if (core_kd == NULL)
    return 0;

  if (write)
    n = kvm_write(core_kd, memaddr, myaddr, len);
  else
    n = kvm_read (core_kd, memaddr, myaddr, len) ;
  if (n < 0) {
    fprintf_unfiltered (gdb_stderr, "can not access 0x%x, %s\n",
			memaddr, kvm_geterr(core_kd));
    n = 0;
  }

  return n;
}

#if 0	/* XXX */
static int
xfer_umem (memaddr, myaddr, len, write)
  CORE_ADDR memaddr;
  char *myaddr;
  int len;
  int write; /* ignored */
{
  int n;
  struct proc proc;

  if (kvread(cur_proc, &proc))
    error("cannot read proc at %#x", cur_proc);
  n = kvm_uread(core_kd, &proc, memaddr, myaddr, len) ;

  if (n < 0)
    return 0;
  return n;
}
#endif

static void
set_proc_cmd(arg)
  char *arg;
{
  CORE_ADDR addr;
  void *val;

  if (!arg)
    error_no_arg("proc address for the new context");

  if (core_kd == NULL)
    error("no kernel core file");

  addr = (CORE_ADDR)parse_and_eval_address(arg);

  /* Read the PCB address in proc structure. */
  addr += (int) &((struct proc *)0)->p_addr;
  if (kvread(addr, &val))
    error("cannot read u area ptr");

  if (set_context((CORE_ADDR)val))
    error("invalid proc address");
}

static void
set_pcb_cmd(arg)
  char *arg;
{
  CORE_ADDR addr;
  void *val;

  if (!arg)
    error_no_arg("pcb address for the new context");

  if (core_kd == NULL)
    error("no kernel core file");

  addr = (CORE_ADDR)parse_and_eval_address(arg);

  if (set_context(addr))
    error("invalid pcb address");
}



void
_initialize_kcorelow()
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
