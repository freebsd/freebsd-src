/* Live and postmortem kernel debugging functions for FreeBSD.
   Copyright 1996 Free Software Foundation, Inc.

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

/* $FreeBSD$ */

#include "defs.h"

#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <paths.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/user.h>
#include "frame.h"  /* required by inferior.h */
#include "inferior.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "command.h"
#include "bfd.h"
#include "target.h"
#include "gdbcore.h"
#include <sys/stat.h>
#include <unistd.h>
#include <vm/vm.h>
#include <vm/vm_param.h>

#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/tss.h>
#include <machine/frame.h>

static void kcore_files_info PARAMS ((struct target_ops *));

static void kcore_close PARAMS ((int));

static void get_kcore_registers PARAMS ((int));

static int kcore_xfer_kmem PARAMS ((CORE_ADDR, char *, int, int, struct target_ops *));

static int xfer_umem PARAMS ((CORE_ADDR, char *, int, int));

static CORE_ADDR ksym_lookup PARAMS ((const char *));

static int read_pcb PARAMS ((int, CORE_ADDR));

static struct proc * curProc PARAMS ((void));

static int set_proc_context PARAMS ((CORE_ADDR paddr));

static void kcore_open PARAMS ((char *filename, int from_tty));

static void kcore_detach PARAMS ((char *args, int from_tty));

static void set_proc_cmd PARAMS ((char *arg, int from_tty));

static void set_cpu_cmd PARAMS ((char *arg, int from_tty));

static CORE_ADDR kvtophys PARAMS ((int, CORE_ADDR));

static int physrd PARAMS ((int, u_int, char*, int));

static int kvm_open PARAMS ((const char *efile, char *cfile, char *sfile,
			     int perm, char *errout));

static int kvm_close PARAMS ((int fd));

static int kvm_write PARAMS ((int core_kd, CORE_ADDR memaddr,
			      char *myaddr, int len));

static int kvm_read PARAMS ((int core_kd, CORE_ADDR memaddr,
			     char *myaddr, int len));

static int kvm_uread PARAMS ((int core_kd, struct proc *p,
			      CORE_ADDR memaddr, char *myaddr,
			      int len));

static int kernel_core_file_hook PARAMS ((int fd, CORE_ADDR addr,
					  char *buf, int len));

static struct kinfo_proc * kvm_getprocs PARAMS ((int cfd, int op,
						CORE_ADDR proc, int *cnt));

extern struct target_ops kcore_ops;	/* Forward decl */

/* Non-zero means we are debugging a kernel core file */
int kernel_debugging = 0;
int kernel_writablecore = 0;

static char *core_file;
static int core_kd = -1;
static struct proc *cur_proc;
static CORE_ADDR kernel_start;

static int ncpus;
static int cpuid;
static CORE_ADDR prv_space;	/* per-cpu private space */
static int prv_space_size;
#define prv_start	(prv_space + cpuid * prv_space_size)

/*
 * Read the "thing" at kernel address 'addr' into the space pointed to
 * by point.  The length of the "thing" is determined by the type of p.
 * Result is non-zero if transfer fails.
 */
#define kvread(addr, p) \
	(target_read_memory ((CORE_ADDR)(addr), (char *)(p), sizeof(*(p))))



/*
 * The following is FreeBSD-specific hackery to decode special frames
 * and elide the assembly-language stub.  This could be made faster by
 * defining a frame_type field in the machine-dependent frame information,
 * but we don't think that's too important right now.
 */
enum frametype { tf_normal, tf_trap, tf_interrupt, tf_syscall };

CORE_ADDR
fbsd_kern_frame_saved_pc (fr)
struct frame_info *fr;
{
       struct minimal_symbol *sym;
       CORE_ADDR this_saved_pc;
       enum frametype frametype;

       this_saved_pc = read_memory_integer (fr->frame + 4, 4);
       sym = lookup_minimal_symbol_by_pc (this_saved_pc);
       frametype = tf_normal;
       if (sym != NULL) {
               if (strcmp (SYMBOL_NAME(sym), "calltrap") == 0)
                       frametype = tf_trap;
               else if (strncmp (SYMBOL_NAME(sym), "Xresume", 7) == 0)
                       frametype = tf_interrupt;
               else if (strcmp (SYMBOL_NAME(sym), "Xsyscall") == 0)
                       frametype = tf_syscall;
       }

       switch (frametype) {
       case tf_normal:
               return (this_saved_pc);

#define oEIP   offsetof(struct trapframe, tf_eip)

       case tf_trap:
               return (read_memory_integer (fr->frame + 8 + oEIP, 4));

       case tf_interrupt:
               return (read_memory_integer (fr->frame + 16 + oEIP, 4));

       case tf_syscall:
               return (read_memory_integer (fr->frame + 8 + oEIP, 4));
#undef oEIP
       }
}

static CORE_ADDR
ksym_lookup (name)
const char *name;
{
	struct minimal_symbol *sym;

	sym = lookup_minimal_symbol (name, NULL, NULL);
	if (sym == NULL)
		error ("kernel symbol `%s' not found.", name);

	return SYMBOL_VALUE_ADDRESS (sym);
}

static struct proc *
curProc ()
{
  struct proc *p;
  CORE_ADDR addr = ksym_lookup ("gd_curproc") + prv_start;

  if (kvread (addr, &p))
    error ("cannot read proc pointer at %x\n", addr);
  return p;
}

/*
 * Set the process context to that of the proc structure at
 * system address paddr.
 */
static int
set_proc_context (paddr)
	CORE_ADDR paddr;
{
  struct proc p;

  if (paddr < kernel_start)
    return (1);

  cur_proc = (struct proc *)paddr;
#ifdef notyet
  set_kernel_boundaries (cur_proc);
#endif

  /* Fetch all registers from core file */
  target_fetch_registers (-1);

  /* Now, set up the frame cache, and print the top of stack */
  flush_cached_frames ();
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

  if (core_kd)
    {
      kvm_close (core_kd);
      free (core_file);
      core_file = NULL;
      core_kd = -1;
    }
}

/* This routine opens and sets up the core file bfd */

static void
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

  unpush_target (&kcore_ops);

  if (!filename)
    {
      /*error (core_kd?*/
      error ( (core_kd >= 0)?
	     "No core file specified.  (Use `detach' to stop debugging a core file.)"
	     : "No core file specified.");
    }

  filename = tilde_expand (filename);
  if (filename[0] != '/')
    {
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
		      kernel_writablecore? O_RDWR : O_RDONLY, "kgdb: ");
  if (core_kd < 0)
    perror_with_name (filename);

  /* Looks semi-reasonable. Toss the old core file and work on the new. */

  discard_cleanups (old_chain);	/* Don't free filename any more */
  core_file = filename;
  ontop = !push_target (&kcore_ops);

  kernel_start = bfd_get_start_address (exec_bfd); /* XXX */

  /* print out the panic string if there is one */
  if (kvread (ksym_lookup ("panicstr"), &addr) == 0
      && addr != 0
      && target_read_memory (addr, buf, sizeof (buf)) == 0)
    {
      for (cp = buf; cp < &buf[sizeof (buf)] && *cp; cp++)
	if (!isascii (*cp) || (!isprint (*cp) && !isspace (*cp)))
	  *cp = '?';
      *cp = '\0';
      if (buf[0] != '\0')
	printf ("panicstr: %s\n", buf);
    }

  /* Print all the panic messages if possible. */
  if (symfile_objfile != NULL)
    {
      printf ("panic messages:\n---\n");
      snprintf (buf, sizeof buf,
		"/sbin/dmesg -N %s -M %s | \
		 /usr/bin/awk '/^(panic:|Fatal trap) / { printing = 1 } \
			       { if (printing) print $0 }'",
		symfile_objfile->name, filename);
      fflush(stdout);
      system (buf);
      printf ("---\n");
    }

  if (!ontop)
    {
      warning ("you won't be able to access this core file until you terminate\n\
your %s; do ``info files''", target_longname);
      return;
    }

  /* we may need this later */
  cur_proc = (struct proc *)curProc ();
  /* Now, set up the frame cache, and print the top of stack */
  flush_cached_frames ();
  set_current_frame (create_new_frame (read_fp (), read_pc ()));
  select_frame (get_current_frame (), 0);
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
  struct user *uaddr;

  /* find the pcb for the current process */
  if (cur_proc == NULL || kvread (&cur_proc->p_addr, &uaddr))
    error ("cannot read u area ptr for proc at %#x", cur_proc);
  if (read_pcb (core_kd, (CORE_ADDR)&uaddr->u_pcb) < 0)
    error ("cannot read pcb at %#x", &uaddr->u_pcb);
}

static void
kcore_files_info (t)
     struct target_ops *t;
{
  printf ("\t`%s'\n", core_file);
}

static CORE_ADDR
ksym_maxuseraddr()
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

static int
kcore_xfer_kmem (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;
{
  int ns;
  int nu;

  if (memaddr >= ksym_maxuseraddr())
    nu = 0;
  else
    {
      nu = xfer_umem (memaddr, myaddr, len, write);
      if (nu <= 0)
	return (0);
      if (nu == len)
	return (nu);
      memaddr += nu;
      if (memaddr != ksym_maxuseraddr())
	return (nu);
      myaddr += nu;
      len -= nu;
    }

  ns = (write ? kvm_write : kvm_read) (core_kd, memaddr, myaddr, len);
  if (ns < 0)
    ns = 0;

  return (nu + ns);
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

  if (cur_proc == NULL || kvread (cur_proc, &proc))
    error ("cannot read proc at %#x", cur_proc);
  n = kvm_uread (core_kd, &proc, memaddr, myaddr, len) ;

  if (n < 0)
    return 0;
  return n;
}

static CORE_ADDR
ksym_kernbase()
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

#define	KERNOFF		(ksym_kernbase())
#define	INKERNEL(x)	((x) >= KERNOFF)

static CORE_ADDR sbr;
static CORE_ADDR curpcb;
static int found_pcb;
static int devmem;
static int kfd;
static struct pcb pcb;

static void
set_proc_cmd (arg, from_tty)
     char *arg;
     int from_tty;
{
  CORE_ADDR paddr;
  struct kinfo_proc *kp;
  int cnt = 0;

  if (!arg)
    error_no_arg ("proc address for new current process");
  if (!kernel_debugging)
    error ("not debugging kernel");

  paddr = (CORE_ADDR)parse_and_eval_address (arg);
  /* assume it's a proc pointer if it's in the kernel */
  if (paddr >= kernel_start) {
    if (set_proc_context(paddr))
      error("invalid proc address");
    } else {
      kp = kvm_getprocs(core_kd, KERN_PROC_PID, paddr, &cnt);
      if (!cnt)
        error("invalid pid");
      if (set_proc_context((CORE_ADDR)kp->kp_eproc.e_paddr))
        error("invalid proc address");
  }
}

static void
set_cpu_cmd (arg, from_tty)
     char *arg;
     int from_tty;
{
  CORE_ADDR paddr;
  struct kinfo_proc *kp;
  int cpu, cfd;

  if (!arg)
    error_no_arg ("cpu number");
  if (!kernel_debugging)
    error ("not debugging kernel");
  if (!ncpus)
    error ("not debugging SMP kernel");

  cpu = (int)parse_and_eval_address (arg);
  if (cpu < 0 || cpu > ncpus)
    error ("cpu number out of range");
  cpuid = cpu;

  cfd = core_kd;
  curpcb = kvtophys(cfd, ksym_lookup ("gd_curpcb") + prv_start);
  physrd (cfd, curpcb, (char*)&curpcb, sizeof curpcb);

  if (!devmem)
    paddr = ksym_lookup ("dumppcb") - KERNOFF;
  else
    paddr = kvtophys (cfd, curpcb);
  read_pcb (cfd, paddr);
  printf ("initial pcb at %lx\n", (unsigned long)paddr);

  if ((cur_proc = curProc()))
    target_fetch_registers (-1);

  /* Now, set up the frame cache, and print the top of stack */
  flush_cached_frames ();
  set_current_frame (create_new_frame (read_fp (), read_pc ()));
  select_frame (get_current_frame (), 0);
  print_stack_frame (selected_frame, selected_frame_level, 1);
}

/* substitutes for the stuff in libkvm which doesn't work */
/* most of this was taken from the old kgdb */

/* we don't need all this stuff, but the call should look the same */

static int
kvm_open (efile, cfile, sfile, perm, errout)
     const char *efile;
     char *cfile;
     char *sfile;		/* makes this kvm_open more compatible to the one in libkvm */
     int perm;
     char *errout;		/* makes this kvm_open more compatible to the one in libkvm */
{
  struct stat stb;
  int cfd;
  CORE_ADDR paddr;

  if ((cfd = open (cfile, perm, 0)) < 0)
    return (cfd);

  fstat (cfd, &stb);
  if ((stb.st_mode & S_IFMT) == S_IFCHR
      && stb.st_rdev == makedev (2, 0))
    {
      devmem = 1;
      kfd = open (_PATH_KMEM, perm, 0);
    }

  if (lookup_minimal_symbol("mp_ncpus", NULL, NULL)) {
    physrd(cfd, ksym_lookup("mp_ncpus") - KERNOFF,
	   (char*)&ncpus, sizeof(ncpus));
    prv_space = ksym_lookup("SMP_prvspace");
    prv_space_size = (int)ksym_lookup("gd_idlestack_top");
    printf ("SMP %d cpus\n", ncpus);
  } else {
    ncpus = 0;
    prv_space = 0;
    prv_space_size = 0;
  }
  cpuid = 0;

  physrd (cfd, ksym_lookup ("IdlePTD") - KERNOFF, (char*)&sbr, sizeof sbr);
  printf ("IdlePTD at phsyical address 0x%08lx\n", (unsigned long)sbr);
  curpcb = kvtophys(cfd, ksym_lookup ("gd_curpcb") + prv_start);
  physrd (cfd, curpcb, (char*)&curpcb, sizeof curpcb);

  found_pcb = 1; /* for vtophys */
  if (!devmem)
    paddr = ksym_lookup ("dumppcb") - KERNOFF;
  else
    paddr = kvtophys (cfd, curpcb);
  read_pcb (cfd, paddr);
  printf ("initial pcb at physical address 0x%08lx\n", (unsigned long)paddr);

  return (cfd);
}

static int
kvm_close (fd)
     int fd;
{
  return (close (fd));
}

static int
kvm_write (core_kd, memaddr, myaddr, len)
     int core_kd;
     CORE_ADDR memaddr;
     char *myaddr;
{
  int cc;

  if (devmem)
    {
      if (kfd > 0)
	{
	  /*
	   * Just like kvm_read, only we write.
	   */
	  errno = 0;
	  if (lseek (kfd, (off_t)memaddr, 0) < 0
	      && errno != 0)
	    {
	      error ("kvm_write:invalid address (%x)", memaddr);
	      return (0);
	    }
	  cc = write (kfd, myaddr, len);
	  if (cc < 0)
	    {
	      error ("kvm_write:write failed");
	      return (0);
	    }
	  else if (cc < len)
	    error ("kvm_write:short write");
	  return (cc);
	}
      else
	return (0);
    }
  else
    {
      printf ("kvm_write not implemented for dead kernels\n");
      return (0);
    }
  /* NOTREACHED */
}

static int
kvm_read (core_kd, memaddr, myaddr, len)
     int core_kd;
     CORE_ADDR memaddr;
     char *myaddr;
{
  return (kernel_core_file_hook (core_kd, memaddr, myaddr, len));
}

static int
kvm_uread (core_kd, p, memaddr, myaddr, len)
     int core_kd;
     register struct proc *p;
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  register char *cp;
  char procfile[MAXPATHLEN];
  ssize_t amount;
  int fd;

  if (devmem) 
    {
      sprintf (procfile, "/proc/%d/mem", p->p_pid);
      fd = open (procfile, O_RDONLY, 0);
      if (fd < 0)
	{
	  error ("cannot open %s", procfile);
	  close (fd);
	  return (0);
	}

      cp = myaddr;
      while (len > 0)
	{
	  errno = 0;
	  if (lseek (fd, (off_t)memaddr, 0) == -1 && errno != 0)
	    {
	      error ("invalid address (%x) in %s", memaddr, procfile);
	      break;
	    }
	  amount = read (fd, cp, len);
	  if (amount < 0)
	    {
	      error ("error reading %s", procfile);
	      break;
	    }
	  if (amount == 0)
	    {
	      error ("EOF reading %s", procfile);
	      break;
	    }
	  cp += amount;
	  memaddr += amount;
	  len -= amount;
	}

      close (fd);
      return ((ssize_t) (cp - myaddr));
    }
  else
    return (kernel_core_file_hook (core_kd, memaddr, myaddr, len));
}

static struct kinfo_proc kp;

/*
 * try to do what kvm_proclist in libkvm would do
 */
static int
kvm_proclist (cfd, pid, p, cnt)
int cfd, pid, *cnt;
struct proc *p;
{
  struct proc lp;

  for (; p != NULL; p = lp.p_list.le_next) {
      if (!kvm_read(cfd, (CORE_ADDR)p, (char *)&lp, sizeof (lp)))
            return (0);
       if (lp.p_pid != pid)
           continue;
       kp.kp_eproc.e_paddr = p;
       *cnt = 1;
       return (1);
  }
  *cnt = 0;
  return (0);
}

/*
 * try to do what kvm_deadprocs in libkvm would do
 */
static struct kinfo_proc *
kvm_deadprocs (cfd, pid, cnt)
int cfd, pid, *cnt;
{
  CORE_ADDR allproc, zombproc;
  struct proc *p;

  allproc = ksym_lookup("allproc");
  if (kvm_read(cfd, allproc, (char *)&p, sizeof (p)) == 0)
      return (NULL);
  kvm_proclist (cfd, pid, p, cnt);
  if (!*cnt) {
      zombproc = ksym_lookup("zombproc");
      if (kvm_read(cfd, zombproc, (char *)&p, sizeof (p)) == 0)
           return (NULL);
       kvm_proclist (cfd, pid, p, cnt);
  }
  return (&kp);
}

/*
 * try to do what kvm_getprocs in libkvm would do
 */
static struct kinfo_proc *
kvm_getprocs (cfd, op, proc, cnt)
int cfd, op, *cnt;
CORE_ADDR proc;
{
  int mib[4], size;

  *cnt = 0;
  /* assume it's a pid */
  if (devmem) { /* "live" kernel, use sysctl */
      mib[0] = CTL_KERN;
      mib[1] = KERN_PROC;
      mib[2] = KERN_PROC_PID;
      mib[3] = (int)proc;
      size = sizeof (kp);
      if (sysctl (mib, 4, &kp, &size, NULL, 0) < 0) {
            perror("sysctl");
            *cnt = 0;
            return (NULL);
      }
      if (!size)
            *cnt = 0;
      else
            *cnt = 1;
      return (&kp);
  } else
      return (kvm_deadprocs (cfd, (int)proc, cnt));
}

static int
physrd (cfd, addr, dat, len)
     int cfd;
     u_int addr;
     char *dat;
     int len;
{
  if (lseek (cfd, (off_t)addr, L_SET) == -1)
    return (-1);
  return (read (cfd, dat, len));
}

static CORE_ADDR
kvtophys (fd, addr)
     int fd;
     CORE_ADDR addr;
{
  CORE_ADDR v;
  unsigned int pte;
  static CORE_ADDR PTD = -1;
  CORE_ADDR current_ptd;

  /*
   * We may no longer have a linear system page table...
   *
   * Here's the scoop.  IdlePTD contains the physical address
   * of a page table directory that always maps the kernel.
   * IdlePTD is in memory that is mapped 1-to-1, so we can
   * find it easily given its 'virtual' address from ksym_lookup().
   * For hysterical reasons, the value of IdlePTD is stored in sbr.
   *
   * To look up a kernel address, we first convert it to a 1st-level
   * address and look it up in IdlePTD.  This gives us the physical
   * address of a page table page; we extract the 2nd-level part of
   * VA and read the 2nd-level pte.  Finally, we add the offset part
   * of the VA into the physical address from the pte and return it.
   *
   * User addresses are a little more complicated.  If we don't have
   * a current PCB from read_pcb(), we use PTD, which is the (fixed)
   * virtual address of the current ptd.  Since it's NOT in 1-to-1
   * kernel space, we must look it up using IdlePTD.  If we do have
   * a pcb, we get the ptd from pcb_ptd.
   */

  if (INKERNEL (addr))
    current_ptd = sbr;
  else if (found_pcb == 0)
    {
      if (PTD == -1)
	PTD = kvtophys (fd, ksym_lookup ("PTD"));
      current_ptd = PTD;
    }
  else
    current_ptd = pcb.pcb_cr3;

  /*
   * Read the first-level page table (ptd).
   */
  v = current_ptd + ( (unsigned)addr >> PDRSHIFT) * sizeof pte;
  if (physrd (fd, v, (char *)&pte, sizeof pte) < 0 || (pte&PG_V) == 0)
    return (~0);

  if (pte & PG_PS)
    {
      /*
       * No second-level page table; ptd describes one 4MB page.
       * (We assume that the kernel wouldn't set PG_PS without enabling
       * it cr0, and that the kernel doesn't support 36-bit physical
       * addresses).
       */
#define	PAGE4M_MASK	(NBPDR - 1)
#define	PG_FRAME4M	(~PAGE4M_MASK)
      addr = (pte & PG_FRAME4M) + (addr & PAGE4M_MASK);
    }
  else
    {
      /*
       * Read the second-level page table.
       */
      v = (pte&PG_FRAME) + ((addr >> PAGE_SHIFT)&(NPTEPG-1)) * sizeof pte;
      if (physrd (fd, v, (char *) &pte, sizeof (pte)) < 0 || (pte&PG_V) == 0)
	return (~0);

      addr = (pte & PG_FRAME) + (addr & PAGE_MASK);
    }
#if 0
  printf ("vtophys (%x) -> %x\n", oldaddr, addr);
#endif
  return (addr);
}

static int
read_pcb (fd, uaddr)
     int fd;
     CORE_ADDR uaddr;
{
  int i;
  int noreg;
  CORE_ADDR nuaddr = uaddr;

  /* need this for the `proc' command to work */
  if (INKERNEL(uaddr))
      nuaddr = kvtophys(fd, uaddr);

  if (physrd (fd, nuaddr, (char *)&pcb, sizeof pcb) < 0)
    {
      error ("cannot read pcb at %x\n", uaddr);
      return (-1);
    }

  /*
   * get the register values out of the sys pcb and
   * store them where `read_register' will find them.
   */
  /*
   * XXX many registers aren't available.
   * XXX for the non-core case, the registers are stale - they are for
   *     the last context switch to the debugger.
   * XXX gcc's register numbers aren't all #defined in tm-i386.h.
   */
  noreg = 0;
  for (i = 0; i < 3; ++i)		/* eax,ecx,edx */
    supply_register (i, (char *)&noreg);
  supply_register (3, (char *)&pcb.pcb_ebx);
  supply_register (SP_REGNUM, (char *)&pcb.pcb_esp);
  supply_register (FP_REGNUM, (char *)&pcb.pcb_ebp);
  supply_register (6, (char *)&pcb.pcb_esi);
  supply_register (7, (char *)&pcb.pcb_edi);
  supply_register (PC_REGNUM, (char *)&pcb.pcb_eip);
  for (i = 9; i < 14; ++i)		/* eflags, cs, ss, ds, es, fs */
    supply_register (i, (char *)&noreg);
  supply_register (15, (char *)&pcb.pcb_gs);

  /* XXX 80387 registers? */
}

/*
 * read len bytes from kernel virtual address 'addr' into local
 * buffer 'buf'.  Return numbert of bytes if read ok, 0 otherwise.  On read
 * errors, portion of buffer not read is zeroed.
 */

static int
kernel_core_file_hook (fd, addr, buf, len)
     int fd;
     CORE_ADDR addr;
     char *buf;
     int len;
{
  int i;
  CORE_ADDR paddr;
  register char *cp;
  int cc;

  cp = buf;

  while (len > 0)
    {
      paddr = kvtophys (fd, addr);
      if (paddr == ~0)
	{
	  memset (buf, '\000', len);
	  break;
	}
      /* we can't read across a page boundary */
      i = min (len, PAGE_SIZE - (addr & PAGE_MASK));
      if ( (cc = physrd (fd, paddr, cp, i)) <= 0)
	{
	  memset (cp, '\000', len);
	  return (cp - buf);
	}
      cp += cc;
      addr += cc;
      len -= cc;
    }
  return (cp - buf);
}

static struct target_ops kcore_ops;

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
  kcore_ops.to_xfer_memory = kcore_xfer_kmem;
  kcore_ops.to_files_info = kcore_files_info;
  kcore_ops.to_create_inferior = find_default_create_inferior;
  kcore_ops.to_stratum = kcore_stratum;
  kcore_ops.to_has_memory = 1;
  kcore_ops.to_has_stack = 1;
  kcore_ops.to_has_registers = 1;
  kcore_ops.to_magic = OPS_MAGIC;

  add_target (&kcore_ops);
  add_com ("proc", class_obscure, set_proc_cmd, "Set current process context");
  add_com ("cpu", class_obscure, set_cpu_cmd, "Set current cpu");
}
