/* Core dump and executable file functions below target vector, for GDB.
   Copyright 1986, 1987, 1989, 1991, 1992 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	$FreeBSD$
*/

#include "defs.h"

#ifdef KERNEL_DEBUG

#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/sysctl.h>
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
static int		core_kd = -1;
static struct proc	*cur_proc;
static CORE_ADDR	kernel_start;

/*
 * Read the "thing" at kernel address 'addr' into the space pointed to
 * by point.  The length of the "thing" is determined by the type of p.
 * Result is non-zero if transfer fails.
 */
#define kvread(addr, p) \
	(target_read_memory((CORE_ADDR)(addr), (char *)(p), sizeof(*(p))))

int read_pcb (int, CORE_ADDR);
extern struct kinfo_proc* kvm_getprocs (int, int, CORE_ADDR, int*);

CORE_ADDR
ksym_lookup(name)
const char *name;
{
	struct minimal_symbol *sym;

	sym = lookup_minimal_symbol(name, (struct objfile *)NULL);
	if (sym == NULL)
		error("kernel symbol `%s' not found.", name);

	return SYMBOL_VALUE_ADDRESS(sym);
}

static struct proc *
curProc()
{
	struct proc *p;
	CORE_ADDR addr = ksym_lookup("curproc");

	if (kvread(addr, &p))
		error("cannot read proc pointer at %x\n", addr);
	return p;
}

/*
 * Set the process context to that of the proc structure at
 * system address paddr.
 */
static int
set_proc_context(paddr)
	CORE_ADDR paddr;
{
	struct proc p;

	if (paddr < kernel_start)
		return (1);

	cur_proc = (struct proc *)paddr;
#ifdef notyet
	set_kernel_boundaries(cur_proc);
#endif

	/* Fetch all registers from core file */
	target_fetch_registers (-1);

	/* Now, set up the frame cache, and print the top of stack */
	flush_cached_frames();
	set_current_frame (create_new_frame (read_fp (), read_pc ()));
	select_frame (get_current_frame (), 0);
	return (0);
}

/* Discard all vestiges of any previous core file
   and mark data and stack spaces as empty.  */

/* ARGSUSED */
static void
kcore_close (quitting)
	int quitting;
{
	inferior_pid = 0;	/* Avoid confusion from thread stuff */

	if (core_kd) {
		kvm_close(core_kd);
		free(core_file);
		core_file = NULL;
		core_kd = -1;
	}
}

/* This routine opens and sets up the core file bfd */

void
kcore_open (filename, from_tty)
	char *filename;
	int from_tty;
{
	const char *p;
	struct cleanup *old_chain;
	char buf[256], *cp;
	int ontop;
	CORE_ADDR addr;
	struct pcb pcb;

	target_preopen (from_tty);
	if (!filename) {
		/*error (core_kd?*/
		error ((core_kd >= 0)?
	"No core file specified.  (Use `detach' to stop debugging a core file.)"
			: "No core file specified.");
	}

	filename = tilde_expand (filename);
	if (filename[0] != '/') {
		cp = concat (current_directory, "/", filename, NULL);
		free (filename);
		filename = cp;
	}

	old_chain = make_cleanup (free, filename);

      /*
       * gdb doesn't really do anything if the exec-file couldn't
       * be opened (in that case exec_bfd is NULL). Usually that's
       * no big deal, but kvm_open needs the exec-file's name,
       * which results in dereferencing a NULL pointer, a real NO-NO !
       * So, check here if the open of the exec-file succeeded.
       */
      if (exec_bfd == NULL) /* the open failed */
              error ("kgdb could not open the exec-file, please check the name you used !");

	core_kd = kvm_open (exec_bfd->filename, filename, NULL,
                          kernel_writablecore? O_RDWR: O_RDONLY, "kgdb: ");
	if (core_kd < 0)
		perror_with_name (filename);

	/* Looks semi-reasonable. Toss the old core file and work on the new. */

	discard_cleanups (old_chain);	/* Don't free filename any more */
	core_file = filename;
	unpush_target (&kcore_ops);
	ontop = !push_target (&kcore_ops);

	kernel_start = bfd_get_start_address (exec_bfd); /* XXX */

	/* print out the panic string if there is one */
	if (kvread(ksym_lookup("panicstr"), &addr) == 0 &&
		   addr != 0 &&
		   target_read_memory(addr, buf, sizeof(buf)) == 0) {

		for (cp = buf; cp < &buf[sizeof(buf)] && *cp; cp++)
			if (!isascii(*cp) || (!isprint(*cp) && !isspace(*cp)))
				*cp = '?';
		*cp = '\0';
		if (buf[0] != '\0')
			printf("panic: %s\n", buf);
	}

	if (!ontop) {
		warning (
"you won't be able to access this core file until you terminate\n\
your %s; do ``info files''", current_target->to_longname);
		return;
	}

	/* we may need this later */
	cur_proc = (struct proc *)curProc();
	/* Now, set up the frame cache, and print the top of stack */
	flush_cached_frames();
	set_current_frame (create_new_frame (read_fp (), read_pc ()));
	select_frame (get_current_frame (), 0);
	print_stack_frame (selected_frame, selected_frame_level, 1);
}

void
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
	struct user *uaddr;

	/* find the pcb for the current process */
	if (kvread(&cur_proc->p_addr, &uaddr))
		error("cannot read u area ptr for proc at %#x", cur_proc);
	if (read_pcb (core_kd, (CORE_ADDR)&uaddr->u_pcb) < 0)
		error("cannot read pcb at %#x", &uaddr->u_pcb);
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

static void
kcore_files_info (t)
  struct target_ops *t;
{
	printf("\t`%s'\n", core_file);
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

	if (!memaddr)
		return (0);

	if (memaddr < kernel_start)
		return xfer_umem(memaddr, myaddr, len, write);

	n = write ?
		kvm_write(core_kd, memaddr, myaddr, len) :
		kvm_read(core_kd, memaddr, myaddr, len) ;

	if (n < 0)
		return 0;
	return n;
}

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

static void
set_proc_cmd(arg)
	char *arg;
{
	CORE_ADDR paddr;
	struct kinfo_proc *kp;
	int cnt = 0;

	if (!arg)
		error_no_arg("proc address for new current process");
	if (!kernel_debugging)
		error("not debugging kernel");

	paddr = (CORE_ADDR)parse_and_eval_address(arg);
fprintf(stderr, "paddr %#x kernel_start %#x ", paddr, kernel_start);
	/* assume it's a proc pointer if it's in the kernel */
	if (paddr >= kernel_start) {
        	if (set_proc_context(paddr))
        	        error("invalid proc address");
	} else {
		kp = kvm_getprocs(core_kd, KERN_PROC_PID, paddr, &cnt);
fprintf(stderr, "cnt %d\n", cnt);
		if (!cnt)
        	        error("invalid pid");
        	if (set_proc_context((CORE_ADDR)kp->kp_eproc.e_paddr))
                	error("invalid proc address");
	}
}

struct target_ops kcore_ops = {
	"kcore", "Kernel core dump file",
	"Use a core file as a target.  Specify the filename of the core file.",
	kcore_open, kcore_close,
	find_default_attach, kcore_detach, 0, 0, /* resume, wait */
	get_kcore_registers,
	0, 0, /* store_regs, prepare_to_store */
	xfer_kmem, kcore_files_info,
	ignore, ignore, /* core_insert_breakpoint, core_remove_breakpoint, */
	0, 0, 0, 0, 0, /* terminal stuff */
	0, 0, 0, /* kill, load, lookup sym */
	find_default_create_inferior, 0, /* mourn_inferior */
	0, /* can_run */
	0, /* notice_signals */
	kcore_stratum, 0, /* next */
	0, 1, 1, 1, 0,	/* all mem, mem, stack, regs, exec */
	0, 0,			/* section pointers */
	OPS_MAGIC,		/* Always the last thing */
};

void
_initialize_kcorelow()
{
  add_target (&kcore_ops);
  add_com ("proc", class_obscure, set_proc_cmd, "Set current process context");
}

#endif /* KERNEL_DEBUG */
