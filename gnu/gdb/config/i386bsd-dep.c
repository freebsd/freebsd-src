/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)i386bsd-dep.c	6.10 (Berkeley) 6/26/91";
#endif /* not lint */

/* Low level interface to ptrace, for GDB when running on the Intel 386.
   Copyright (C) 1988, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include "defs.h"
#include "param.h"
#include "frame.h"
#include "inferior.h"
#include "value.h"

#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <a.out.h>

#ifndef N_SET_MAGIC
#define N_SET_MAGIC(exec, val) ((exec).a_magic = (val))
#endif

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/uio.h>
#define curpcb Xcurpcb	/* XXX avoid leaking declaration from pcb.h */
#include <sys/user.h>
#undef curpcb
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ptrace.h>

#include <machine/reg.h>

#ifdef KERNELDEBUG
#ifndef NEWVM
#include <sys/vmmac.h>
#include <machine/pte.h>
#else
#include <sys/proc.h>	/* for curproc */
#endif
#include <machine/vmparam.h>
#include <machine/cpu.h>
#include <ctype.h>
#include "symtab.h"	/* XXX */

#undef	vtophys		/* XXX */

extern int kernel_debugging;

#define	KERNOFF		((unsigned)KERNBASE)
#ifndef NEWVM
#define	INKERNEL(x)	((x) >= KERNOFF && (x) < KERNOFF + ctob(slr))
#define INUPAGE(x)	\
	((x) >= KERNEL_U_ADDR && (x) < KERNEL_U_ADDR + NBPG)
#else
#define	INKERNEL(x)	((x) >= KERNOFF)
#endif

#define	PT_ADDR_ANY	((caddr_t) 1)

/*
 * Convert from sysmap pte index to system virtual address & vice-versa.
 * (why aren't these in one of the system vm macro files???)
 */
#define smxtob(a)       (sbr + (a) * sizeof(pte))
#define btosmx(b)       (((b) - sbr) / sizeof(pte))

static int ok_to_cache();
static int found_pcb;
#ifdef NEWVM
static CORE_ADDR curpcb;
static CORE_ADDR kstack;
#endif

static void setregmap();

extern int errno;

/*
 * This function simply calls ptrace with the given arguments.  It exists so
 * that all calls to ptrace are isolated in this machine-dependent file. 
 */
int
call_ptrace(request, pid, arg3, arg4)
	int request;
	pid_t pid;
	caddr_t arg3;
	int arg4;
{
	return(ptrace(request, pid, arg3, arg4));
}

kill_inferior()
{
	if (remote_debugging) {
#ifdef KERNELDEBUG
		if (kernel_debugging)
			/*
			 * It's a very, very bad idea to go away leaving
			 * breakpoints in a remote kernel or to leave it
			 * stopped at a breakpoint. 
			 */
			clear_breakpoints();
#endif
		remote_close(0);
		inferior_died();
	} else if (inferior_pid != 0) {
		ptrace(PT_KILL, inferior_pid, 0, 0);
		wait(0);
		inferior_died();
	}
}

/*
 * This is used when GDB is exiting.  It gives less chance of error.
 */
kill_inferior_fast()
{
	if (remote_debugging) {
#ifdef KERNELDEBUG
		if (kernel_debugging)
			clear_breakpoints();
#endif
		remote_close(0);
		return;
	}
	if (inferior_pid == 0)
		return;

	ptrace(PT_KILL, inferior_pid, 0, 0);
	wait(0);
}

/*
 * Resume execution of the inferior process. If STEP is nonzero, single-step
 * it. If SIGNAL is nonzero, give it that signal.  
 */
void
resume(step, signal)
	int step;
	int signal;
{
	errno = 0;
	if (remote_debugging)
		remote_resume(step, signal);
	else {
		ptrace(step ? PT_STEP : PT_CONTINUE, inferior_pid,
		       PT_ADDR_ANY, signal);
		if (errno)
			perror_with_name("ptrace");
	}
}

#ifdef ATTACH_DETACH
extern int attach_flag;

/*
 * Start debugging the process whose number is PID.
 */
attach(pid)
	int pid;
{
	errno = 0;
	ptrace(PT_ATTACH, pid, 0, 0);
	if (errno)
		perror_with_name("ptrace");
	attach_flag = 1;
	return pid;
}

/*
 * Stop debugging the process whose number is PID and continue it
 * with signal number SIGNAL.  SIGNAL = 0 means just continue it.  
 */
void
detach(signal)
	int signal;
{
	errno = 0;
	ptrace(PT_DETACH, inferior_pid, PT_ADDR_ANY, signal);
	if (errno)
		perror_with_name("ptrace");
	attach_flag = 0;
}
#endif	/* ATTACH_DETACH */

static unsigned int
get_register_offset()
{
	unsigned int offset;
	struct user u;	/* XXX */
	unsigned int    flags = (char *) &u.u_pcb.pcb_flags - (char *) &u;

	setregmap(ptrace(PT_READ_U, inferior_pid, (caddr_t)flags, 0));

#ifdef NEWVM
	offset = (char *) &u.u_kproc.kp_proc.p_regs - (char *) &u;
	offset = ptrace(PT_READ_U, inferior_pid, (caddr_t)offset, 0) -
		USRSTACK;
#else
	offset = (char *) &u.u_ar0 - (char *) &u;
	offset = ptrace(PT_READ_U, inferior_pid, (caddr_t)offset, 0) -
		KERNEL_U_ADDR;
#endif

	return offset;
}

void
fetch_inferior_registers()
{
	register int    regno;
	register unsigned int regaddr;
	char            buf[MAX_REGISTER_RAW_SIZE];
	register int    i;
	unsigned int    offset;

	if (remote_debugging) {
		extern char     registers[];

		remote_fetch_registers(registers);
		return;
	}

	offset = get_register_offset();

	for (regno = 0; regno < NUM_REGS; regno++) {
		regaddr = register_addr(regno, offset);
		for (i = 0; i < REGISTER_RAW_SIZE(regno); i += sizeof(int)) {
			*(int *)&buf[i] = ptrace(PT_READ_U, inferior_pid, 
						 (caddr_t)regaddr, 0);
			regaddr += sizeof(int);
		}
		supply_register(regno, buf);
	}
}

/*
 * Store our register values back into the inferior. If REGNO is -1, do this
 * for all registers. Otherwise, REGNO specifies which register (so we can
 * save time).  
 */
store_inferior_registers(regno)
	int             regno;
{
	register unsigned int regaddr;
	char            buf[80];
	extern char     registers[];
	register int    i;
	unsigned int    offset;

	if (remote_debugging) {
		extern char     registers[];

		remote_store_registers(registers);
		return;
	}

	offset = get_register_offset();

	if (regno >= 0) {
		regaddr = register_addr(regno, offset);
		for (i = 0; i < REGISTER_RAW_SIZE(regno); i += sizeof(int)) {
			errno = 0;
			ptrace(PT_WRITE_U, inferior_pid, (caddr_t)regaddr,
			     *(int *) &registers[REGISTER_BYTE(regno) + i]);
			if (errno != 0) {
				sprintf(buf, "writing register number %d(%d)",
					regno, i);
				perror_with_name(buf);
			}
			regaddr += sizeof(int);
		}
	} else
		for (regno = 0; regno < NUM_REGS; regno++) {
			regaddr = register_addr(regno, offset);
			for (i = 0; i < REGISTER_RAW_SIZE(regno);
			     i += sizeof(int)) {
				errno = 0;
				ptrace(PT_WRITE_U, inferior_pid,
				       (caddr_t)regaddr,
				       *(int *) &registers[REGISTER_BYTE(regno) + i]);
				if (errno != 0) {
					sprintf(buf,
					   "writing register number %d(%d)", 
					    regno, i);
					perror_with_name(buf);
				}
				regaddr += sizeof(int);
			}
		}
}

/*
 * Copy LEN bytes from inferior's memory starting at MEMADDR to debugger
 * memory starting at MYADDR. On failure (cannot read from inferior, usually
 * because address is out of bounds) returns the value of errno. 
 */
int
read_inferior_memory(memaddr, myaddr, len)
	CORE_ADDR       memaddr;
	char           *myaddr;
	int             len;
{
	register int i;
	/* Round starting address down to longword boundary.  */
	register CORE_ADDR addr = memaddr & -sizeof(int);
	/* Round ending address up; get number of longwords that makes.  */
	register int count = (((memaddr + len) - addr) + sizeof(int) - 1) / 
				sizeof(int);
	/* Allocate buffer of that many longwords.  */
	register int *buffer = (int *) alloca(count * sizeof(int));
	extern int errno;

	if (remote_debugging)
		return (remote_read_inferior_memory(memaddr, myaddr, len));

	/* Read all the longwords */
	errno = 0;
	for (i = 0; i < count && errno == 0; i++, addr += sizeof(int)) 
		buffer[i] = ptrace(PT_READ_I, inferior_pid, (caddr_t)addr, 0);

	/* Copy appropriate bytes out of the buffer.  */
	bcopy((char *) buffer + (memaddr & (sizeof(int) - 1)), myaddr, len);
	return(errno);
}

/*
 * Copy LEN bytes of data from debugger memory at MYADDR to inferior's memory
 * at MEMADDR. On failure (cannot write the inferior) returns the value of
 * errno.  
 */

int
write_inferior_memory(memaddr, myaddr, len)
	CORE_ADDR       memaddr;
	char           *myaddr;
	int             len;
{
	register int    i;
	/* Round starting address down to longword boundary.  */
	register CORE_ADDR addr = memaddr & -sizeof(int);
	/* Round ending address up; get number of longwords that makes.  */
	register int count = (((memaddr + len) - addr) + sizeof(int) - 1) / 
				sizeof(int);
	/* Allocate buffer of that many longwords.  */
	register int *buffer = (int *) alloca(count * sizeof(int));
	extern int errno;

	/*
	 * Fill start and end extra bytes of buffer with existing memory
	 * data.  
	 */
	if (remote_debugging)
		return (remote_write_inferior_memory(memaddr, myaddr, len));

	/*
	 * Fill start and end extra bytes of buffer with existing memory
	 * data.  
	 */
	buffer[0] = ptrace(PT_READ_I, inferior_pid, (caddr_t)addr, 0);

	if (count > 1)
		buffer[count - 1] = ptrace(PT_READ_I, inferior_pid,
				 (caddr_t)addr + (count - 1) * sizeof(int), 0);

	/* Copy data to be written over corresponding part of buffer */

	bcopy(myaddr, (char *) buffer + (memaddr & (sizeof(int) - 1)), len);

	/* Write the entire buffer.  */

	errno = 0;
	for (i = 0; i < count && errno == 0; i++, addr += sizeof(int))
		ptrace(PT_WRITE_I, inferior_pid, (caddr_t)addr, buffer[i]);

	return(errno);
}


/*
 * Work with core dump and executable files, for GDB. 
 * This code would be in core.c if it weren't machine-dependent. 
 */

#ifndef N_TXTADDR
#define N_TXTADDR(hdr) 0
#endif				/* no N_TXTADDR */

#ifndef N_DATADDR
#define N_DATADDR(hdr) hdr.a_text
#endif				/* no N_DATADDR */

/*
 * Make COFF and non-COFF names for things a little more compatible to reduce
 * conditionals later.  
 */

#ifndef AOUTHDR
#define AOUTHDR struct exec
#endif

extern char *sys_siglist[];


/* Hook for `exec_file_command' command to call.  */

extern void (*exec_file_display_hook) ();
   
/* File names of core file and executable file.  */

extern char *corefile;
extern char *execfile;

/* Descriptors on which core file and executable file are open.
   Note that the execchan is closed when an inferior is created
   and reopened if the inferior dies or is killed.  */

extern int corechan;
extern int execchan;

/* Last modification time of executable file.
   Also used in source.c to compare against mtime of a source file.  */

extern int exec_mtime;

/* Virtual addresses of bounds of the two areas of memory in the core file.  */

extern CORE_ADDR data_start;
extern CORE_ADDR data_end;
extern CORE_ADDR stack_start;
extern CORE_ADDR stack_end;

/* Virtual addresses of bounds of two areas of memory in the exec file.
   Note that the data area in the exec file is used only when there is no core file.  */

extern CORE_ADDR text_start;
extern CORE_ADDR text_end;

extern CORE_ADDR exec_data_start;
extern CORE_ADDR exec_data_end;

/* Address in executable file of start of text area data.  */

extern int text_offset;

/* Address in executable file of start of data area data.  */

extern int exec_data_offset;

/* Address in core file of start of data area data.  */

extern int data_offset;

/* Address in core file of start of stack area data.  */

extern int stack_offset;

/* a.out header saved in core file.  */
  
extern AOUTHDR core_aouthdr;

/* a.out header of exec file.  */

extern AOUTHDR exec_aouthdr;

extern void validate_files ();

extern int (*core_file_hook)();

#ifdef KERNELDEBUG
/*
 * Kernel debugging routines.
 */

#define	IOTOP	0x100000	/* XXX should get this from include file */
#define	IOBASE	 0xa0000	/* XXX should get this from include file */

static CORE_ADDR file_offset;
static CORE_ADDR lowram;
static CORE_ADDR sbr;
static CORE_ADDR slr;
static struct pcb pcb;

static CORE_ADDR
ksym_lookup(name)
	char *name;
{
	struct symbol *sym;
	int i;

	if ((i = lookup_misc_func(name)) < 0)
		error("kernel symbol `%s' not found.", name);

	return (misc_function_vector[i].address);
}

/*
 * return true if 'len' bytes starting at 'addr' can be read out as
 * longwords and/or locally cached (this is mostly for memory mapped
 * i/o register access when debugging remote kernels).
 *
 * XXX the HP code does this differently with NEWVM
 */
static int
ok_to_cache(addr, len)
{
	static CORE_ADDR atdevbase;

	if (! atdevbase)
		atdevbase = ksym_lookup("atdevbase");

	if (addr >= atdevbase && addr < atdevbase + (IOTOP - IOBASE))
		return (0);

	return (1);
}

static
physrd(addr, dat, len)
	u_int addr;
	char *dat;
{
	if (lseek(corechan, addr - file_offset, L_SET) == -1)
		return (-1);
	if (read(corechan, dat, len) != len)
		return (-1);

	return (0);
}

/*
 * When looking at kernel data space through /dev/mem or with a core file, do
 * virtual memory mapping.
 */
#ifdef NEWVM
static CORE_ADDR
vtophys(addr)
	CORE_ADDR addr;
{
	CORE_ADDR v;
	struct pte pte;
	static CORE_ADDR PTD = -1;
	CORE_ADDR current_ptd;

	/*
	 * If we're looking at the kernel stack,
	 * munge the address to refer to the user space mapping instead;
	 * that way we get the requested process's kstack, not the running one.
	 */
	if (addr >= kstack && addr < kstack + ctob(UPAGES))
		addr = (addr - kstack) + curpcb;

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

	if (INKERNEL(addr))
		current_ptd = sbr;
	else if (found_pcb == 0) {
		if (PTD == -1)
			PTD = vtophys(ksym_lookup("PTD"));
		current_ptd = PTD;
	} else
		current_ptd = pcb.pcb_ptd;

	/*
	 * Read the first-level page table (ptd).
	 */
	v = current_ptd + ((unsigned)addr >> PD_SHIFT) * sizeof pte;
	if (physrd(v, (char *)&pte, sizeof pte) || pte.pg_v == 0)
		return (~0);

	/*
	 * Read the second-level page table.
	 */
	v = i386_ptob(pte.pg_pfnum) + ((addr&PT_MASK) >> PG_SHIFT) * sizeof pte;
	if (physrd(v, (char *) &pte, sizeof(pte)) || pte.pg_v == 0)
		return (~0);

	addr = i386_ptob(pte.pg_pfnum) + (addr & PGOFSET);
#if 0
	printf("vtophys(%x) -> %x\n", oldaddr, addr);
#endif
	return (addr);
}
#else
static CORE_ADDR
vtophys(addr)
	CORE_ADDR addr;
{
	CORE_ADDR v;
	struct pte pte;
	CORE_ADDR oldaddr = addr;

	if (found_pcb == 0 && INUPAGE(addr)) {
		static CORE_ADDR pSwtchmap;

		if (pSwtchmap == 0)
			pSwtchmap = vtophys(ksym_lookup("Swtchmap"));
		addr = pSwtchmap;
	} else if (INKERNEL(addr)) {
		/*
		 * In system space get system pte.  If valid or reclaimable
		 * then physical address is combination of its page number
		 * and the page offset of the original address.
		 */
		addr = smxtob(btop(addr - KERNOFF)) - KERNOFF;
	} else {
		v = btop(addr);
		if (v < pcb.pcb_p0lr)
			addr = (CORE_ADDR) pcb.pcb_p0br +
				v * sizeof (struct pte);
		else if (v >= pcb.pcb_p1lr && v < P1PAGES)
			addr = (CORE_ADDR) pcb.pcb_p0br +
				((pcb.pcb_szpt * NPTEPG - HIGHPAGES) -
				 (BTOPUSRSTACK - v)) * sizeof (struct pte);
		else
			return (~0);

		/*
		 * For p0/p1 address, user-level page table should be in
		 * kernel vm.  Do second-level indirect by recursing.
		 */
		if (!INKERNEL(addr))
			return (~0);

		addr = vtophys(addr);
	}
	/*
	 * Addr is now address of the pte of the page we are interested in;
	 * get the pte and paste up the physical address.
	 */
	if (physrd(addr, (char *) &pte, sizeof(pte)))
		return (~0);

	if (pte.pg_v == 0 && (pte.pg_fod || pte.pg_pfnum == 0))
		return (~0);

	addr = (CORE_ADDR)ptob(pte.pg_pfnum) + (oldaddr & PGOFSET);
#if 0
	printf("vtophys(%x) -> %x\n", oldaddr, addr);
#endif
	return (addr);
}

#endif

static
kvread(addr)
	CORE_ADDR addr;
{
	CORE_ADDR paddr = vtophys(addr);

	if (paddr != ~0)
		if (physrd(paddr, (char *)&addr, sizeof(addr)) == 0);
			return (addr);

	return (~0);
}

static void
read_pcb(uaddr)
     u_int uaddr;
{
	int i;
	int *pcb_regs = (int *)&pcb;

#ifdef NEWVM
	if (physrd(uaddr, (char *)&pcb, sizeof pcb))
		error("cannot read pcb at %x\n", uaddr);
	printf("current pcb at %x\n", uaddr);
#else
	if (physrd(uaddr, (char *)&pcb, sizeof pcb))
		error("cannot read pcb at %x\n", uaddr);
	printf("p0br %x p0lr %x p1br %x p1lr %x\n",
	       pcb.pcb_p0br, pcb.pcb_p0lr, pcb.pcb_p1br, pcb.pcb_p1lr);
#endif

	/*
	 * get the register values out of the sys pcb and
	 * store them where `read_register' will find them.
	 */
	for (i = 0; i < 8; ++i)
		supply_register(i, &pcb_regs[i+10]);
	supply_register(8, &pcb_regs[8]);	/* eip */
	supply_register(9, &pcb_regs[9]);	/* eflags */
	for (i = 10; i < 13; ++i)		/* cs, ss, ds */
		supply_register(i, &pcb_regs[i+9]);
	supply_register(13, &pcb_regs[18]);	/* es */
	for (i = 14; i < 16; ++i)		/* fs, gs */
		supply_register(i, &pcb_regs[i+8]);

	/* XXX 80387 registers? */
}

static void
setup_kernel_debugging()
{
	struct stat stb;
	int devmem = 0;
	CORE_ADDR addr;

	fstat(corechan, &stb);
	if ((stb.st_mode & S_IFMT) == S_IFCHR && stb.st_rdev == makedev(2, 0))
		devmem = 1;

#ifdef NEWVM
	physrd(ksym_lookup("IdlePTD") - KERNOFF, &sbr, sizeof sbr);
	slr = 2 * NPTEPG;			/* XXX temporary */
	printf("IdlePTD %x\n", sbr);
	curpcb = ksym_lookup("curpcb") - KERNOFF;
	physrd(curpcb, &curpcb, sizeof curpcb);
	kstack = ksym_lookup("kstack");
#else
	sbr = ksym_lookup("Sysmap");
	slr = ksym_lookup("Syssize");
	printf("sbr %x slr %x\n", sbr, slr);
#endif

	/*
	 * pcb where "panic" saved registers in first thing in current
	 * u area.
	 */
#ifndef NEWVM
	read_pcb(vtophys(ksym_lookup("u")));
#endif
	found_pcb = 1;
	if (!devmem) {
		/* find stack frame */
		CORE_ADDR panicstr;
		char buf[256];
		register char *cp;

		panicstr = kvread(ksym_lookup("panicstr"));
		if (panicstr == ~0)
			return;
		(void) kernel_core_file_hook(panicstr, buf, sizeof(buf));
		for (cp = buf; cp < &buf[sizeof(buf)] && *cp; cp++)
			if (!isascii(*cp) || (!isprint(*cp) && !isspace(*cp)))
				*cp = '?';
		if (*cp)
			*cp = '\0';
		printf("panic: %s\n", buf);
		read_pcb(ksym_lookup("dumppcb") - KERNOFF);
	}
#ifdef NEWVM
	else
	read_pcb(vtophys(kstack));
#endif

	stack_start = USRSTACK;
	stack_end = USRSTACK + ctob(UPAGES);
}

set_paddr_command(arg)
	char *arg;
{
	u_int uaddr;

	if (!arg)
		error_no_arg("ps-style address for new current process");
	if (!kernel_debugging)
		error("not debugging kernel");
	uaddr = (u_int) parse_and_eval_address(arg);
#ifndef NEWVM
	read_pcb(ctob(uaddr));
#else
	/* p_addr is now a pcb virtual address */
	read_pcb(vtophys(uaddr));
	curpcb = uaddr;
#endif

	flush_cached_frames();
	set_current_frame(create_new_frame(read_register(FP_REGNUM), read_pc()));
	select_frame(get_current_frame(), 0);
}

/*
 * read len bytes from kernel virtual address 'addr' into local 
 * buffer 'buf'.  Return 0 if read ok, 1 otherwise.  On read
 * errors, portion of buffer not read is zeroed.
 */
kernel_core_file_hook(addr, buf, len)
	CORE_ADDR addr;
	char *buf;
	int len;
{
	int i;
	CORE_ADDR paddr;

	while (len > 0) {
		paddr = vtophys(addr);
		if (paddr == ~0) {
			bzero(buf, len);
			return (1);
		}
		/* we can't read across a page boundary */
		i = min(len, NBPG - (addr & PGOFSET));
		if (physrd(paddr, buf, i)) {
			bzero(buf, len);
			return (1);
		}
		buf += i;
		addr += i;
		len -= i;
	}
	return (0);
}
#endif

core_file_command(filename, from_tty)
	char           *filename;
	int             from_tty;
{
	int             val;
	extern char     registers[];
#ifdef KERNELDEBUG
	struct stat stb;
#endif

	/*
	 * Discard all vestiges of any previous core file and mark data and
	 * stack spaces as empty.  
	 */
	if (corefile)
		free(corefile);
	corefile = 0;
	core_file_hook = 0;

	if (corechan >= 0)
		close(corechan);
	corechan = -1;

	/* Now, if a new core file was specified, open it and digest it.  */

	if (filename == 0) {
		if (from_tty)
			printf("No core file now.\n");
		return;
	}
	filename = tilde_expand(filename);
	make_cleanup(free, filename);
	if (have_inferior_p())
		error("To look at a core file, you must kill the inferior with \"kill\".");
	corechan = open(filename, O_RDONLY, 0);
	if (corechan < 0)
		perror_with_name(filename);

#ifdef KERNELDEBUG
	fstat(corechan, &stb);

	if (kernel_debugging) {
		setup_kernel_debugging();
		core_file_hook = kernel_core_file_hook;
	} else if ((stb.st_mode & S_IFMT) == S_IFCHR &&
		   stb.st_rdev == makedev(2, 1)) {
		/* looking at /dev/kmem */
		data_offset = data_start = KERNOFF;
		data_end = ~0; /* XXX */
		stack_end = stack_start = data_end;
	} else
#endif
	{
		/*
		 * 4.2-style core dump file.
		 */
		struct user u;
		unsigned int reg_offset;

		val = myread(corechan, &u, sizeof u);
		if (val < 0)
			perror_with_name("Not a core file: reading upage");
		if (val != sizeof u)
			error("Not a core file: could only read %d bytes", val);

		/*
		 * We are depending on exec_file_command having been
		 * called previously to set exec_data_start.  Since
		 * the executable and the core file share the same
		 * text segment, the address of the data segment will
		 * be the same in both.  
		 */
		data_start = exec_data_start;

#ifndef NEWVM
		data_end = data_start + NBPG * u.u_dsize;
		stack_start = stack_end - NBPG * u.u_ssize;
		data_offset = NBPG * UPAGES;
		stack_offset = NBPG * (UPAGES + u.u_dsize);

		/*
		 * Some machines put an absolute address in here and
		 * some put the offset in the upage of the regs.  
		 */
		reg_offset = (int) u.u_ar0 - KERNEL_U_ADDR;
#else
		data_end = data_start +
			NBPG * u.u_kproc.kp_eproc.e_vm.vm_dsize;
		stack_start = stack_end -
			NBPG * u.u_kproc.kp_eproc.e_vm.vm_ssize;
		data_offset = NBPG * UPAGES;
		stack_offset = NBPG *
			(UPAGES + u.u_kproc.kp_eproc.e_vm.vm_dsize);

		reg_offset = (int) u.u_kproc.kp_proc.p_regs - USRSTACK;
#endif

		setregmap(u.u_pcb.pcb_flags);

		/*
		 * I don't know where to find this info. So, for now,
		 * mark it as not available.  
		 */
	/*	N_SET_MAGIC (core_aouthdr, 0);  */
		bzero ((char *) &core_aouthdr, sizeof core_aouthdr);

		/*
		 * Read the register values out of the core file and
		 * store them where `read_register' will find them.  
		 */
		{
			register int    regno;

			for (regno = 0; regno < NUM_REGS; regno++) {
				char buf[MAX_REGISTER_RAW_SIZE];

				val = lseek(corechan, register_addr(regno, reg_offset), 0);
				if (val < 0
				    || (val = myread(corechan, buf, sizeof buf)) < 0) {
					char *buffer = (char *) alloca(strlen(reg_names[regno]) + 30);
					strcpy(buffer, "Reading register ");
					strcat(buffer, reg_names[regno]);
					perror_with_name(buffer);
				}
				supply_register(regno, buf);
			}
		}
	}
#endif
	if (filename[0] == '/')
		corefile = savestring(filename, strlen(filename));
	else
		corefile = concat(current_directory, "/", filename);

	set_current_frame(create_new_frame(read_register(FP_REGNUM),
					   read_pc()));
	select_frame(get_current_frame(), 0);
	validate_files();
}

exec_file_command(filename, from_tty)
	char           *filename;
	int             from_tty;
{
	int             val;

	/*
	 * Eliminate all traces of old exec file. Mark text segment as empty.  
	 */

	if (execfile)
		free(execfile);
	execfile = 0;
	data_start = 0;
	data_end = 0;
	stack_start = 0;
	stack_end = 0;
	text_start = 0;
	text_end = 0;
	exec_data_start = 0;
	exec_data_end = 0;
	if (execchan >= 0)
		close(execchan);
	execchan = -1;

	/* Now open and digest the file the user requested, if any.  */

	if (filename) {
		filename = tilde_expand(filename);
		make_cleanup(free, filename);

		execchan = openp(getenv("PATH"), 1, filename, O_RDONLY, 0,
				 &execfile);
		if (execchan < 0)
			perror_with_name(filename);

		{
			struct stat     st_exec;

#ifdef HEADER_SEEK_FD
			HEADER_SEEK_FD(execchan);
#endif

			val = myread(execchan, &exec_aouthdr, sizeof(AOUTHDR));

			if (val < 0)
				perror_with_name(filename);

#ifdef KERNELDEBUG
			if (kernel_debugging) {
				/* Gross and disgusting XXX */
				text_start = KERNTEXT_BASE;
				exec_data_start = KERNTEXT_BASE +
					(exec_aouthdr.a_text + 4095) & ~ 4095;
			} else {
#endif
				text_start = N_TXTADDR(exec_aouthdr);
				exec_data_start = N_DATADDR(exec_aouthdr);
#ifdef KERNELDEBUG
			}
#endif

			text_offset = N_TXTOFF(exec_aouthdr);
			exec_data_offset = N_TXTOFF(exec_aouthdr) + exec_aouthdr.a_text;

			text_end = text_start + exec_aouthdr.a_text;
			exec_data_end = exec_data_start + exec_aouthdr.a_data;

			fstat(execchan, &st_exec);
			exec_mtime = st_exec.st_mtime;
		}

		validate_files();
	} else if (from_tty)
		printf("No exec file now.\n");

	/* Tell display code (if any) about the changed file name.  */
	if (exec_file_display_hook)
		(*exec_file_display_hook) (filename);
}

int dummy_code[] = {
	0xb8909090,		/* nop; nop; nop; movl $0x32323232,%eax */
	0x32323232,
#define DUMMY_CALL_INDEX 1
	0x90ccd0ff,		/* call %eax; int3; nop */
};

/*
 * Build `dummy' call instructions on inferior's stack to cause
 * it to call a subroutine.
 *
 * N.B. - code in wait_for_inferior requires that sp < pc < fp when
 * we take the trap 2 above so it will recognize that we stopped
 * at a `dummy' call.  So, after the call sp is *not* decremented
 * to clean the arguments, code & other stuff we lay on the stack.
 * Since the regs are restored to saved values at the breakpoint,
 * sp will get reset correctly.  Also, this restore means we don't
 * have to construct frame linkage info to save pc & fp.  The lack
 * of frame linkage means we can't do a backtrace, etc., if the
 * called function gets a fault or hits a breakpoint but code in
 * run_stack_dummy makes this impossible anyway.
 */
CORE_ADDR
setup_dummy(sp, funaddr, nargs, args, struct_return_bytes, pushfn)
	CORE_ADDR sp;
	CORE_ADDR funaddr;
	int nargs;
	value *args;
	int struct_return_bytes;
	CORE_ADDR (*pushfn)();
{
	int padding, i;
	CORE_ADDR top = sp, struct_addr, pc;

	i = arg_stacklen(nargs, args) + struct_return_bytes
	    + sizeof(dummy_code);
	if (i & 3)
		padding = 4 - (i & 3);
	else
		padding = 0;
	pc = sp - sizeof(dummy_code);
	sp = pc - padding - struct_return_bytes;
	struct_addr = sp;
	while (--nargs >= 0)
		sp = (*pushfn)(sp, *args++);
	if (struct_return_bytes)
		STORE_STRUCT_RETURN(struct_addr, sp);
	write_register(SP_REGNUM, sp);

	dummy_code[DUMMY_CALL_INDEX] = (int)funaddr;
	write_memory(pc, (char *)dummy_code, sizeof(dummy_code));

	return pc;
}

/* helper functions for m-i386.h */

/* stdio style buffering to minimize calls to ptrace */
static CORE_ADDR codestream_next_addr;
static CORE_ADDR codestream_addr;
static unsigned char codestream_buf[sizeof (int)];
static int codestream_off;
static int codestream_cnt;

#define codestream_tell() (codestream_addr + codestream_off)
#define codestream_peek() (codestream_cnt == 0 ? \
			   codestream_fill(1): codestream_buf[codestream_off])
#define codestream_get() (codestream_cnt-- == 0 ? \
			 codestream_fill(0) : codestream_buf[codestream_off++])

static unsigned char 
codestream_fill (peek_flag)
{
  codestream_addr = codestream_next_addr;
  codestream_next_addr += sizeof (int);
  codestream_off = 0;
  codestream_cnt = sizeof (int);
  read_memory (codestream_addr,
	       (unsigned char *)codestream_buf,
	       sizeof (int));
  
  if (peek_flag)
    return (codestream_peek());
  else
    return (codestream_get());
}

static void
codestream_seek (place)
{
  codestream_next_addr = place & -sizeof (int);
  codestream_cnt = 0;
  codestream_fill (1);
  while (codestream_tell() != place)
    codestream_get ();
}

static void
codestream_read (buf, count)
     unsigned char *buf;
{
  unsigned char *p;
  int i;
  p = buf;
  for (i = 0; i < count; i++)
    *p++ = codestream_get ();
}

/* next instruction is a jump, move to target */
static
i386_follow_jump ()
{
  int long_delta;
  short short_delta;
  char byte_delta;
  int data16;
  int pos;
  
  pos = codestream_tell ();
  
  data16 = 0;
  if (codestream_peek () == 0x66)
    {
      codestream_get ();
      data16 = 1;
    }
  
  switch (codestream_get ())
    {
    case 0xe9:
      /* relative jump: if data16 == 0, disp32, else disp16 */
      if (data16)
	{
	  codestream_read ((unsigned char *)&short_delta, 2);
	  pos += short_delta + 3; /* include size of jmp inst */
	}
      else
	{
	  codestream_read ((unsigned char *)&long_delta, 4);
	  pos += long_delta + 5;
	}
      break;
    case 0xeb:
      /* relative jump, disp8 (ignore data16) */
      codestream_read ((unsigned char *)&byte_delta, 1);
      pos += byte_delta + 2;
      break;
    }
  codestream_seek (pos + data16);
}

/*
 * find & return amound a local space allocated, and advance codestream to
 * first register push (if any)
 *
 * if entry sequence doesn't make sense, return -1, and leave 
 * codestream pointer random
 */
static long
i386_get_frame_setup (pc)
{
  unsigned char op;
  
  codestream_seek (pc);
  
  i386_follow_jump ();
  
  op = codestream_get ();
  
  if (op == 0x58)		/* popl %eax */
    {
      /*
       * this function must start with
       * 
       *    popl %eax		  0x58
       *    xchgl %eax, (%esp)  0x87 0x04 0x24
       * or xchgl %eax, 0(%esp) 0x87 0x44 0x24 0x00
       *
       * (the system 5 compiler puts out the second xchg
       * inst, and the assembler doesn't try to optimize it,
       * so the 'sib' form gets generated)
       * 
       * this sequence is used to get the address of the return
       * buffer for a function that returns a structure
       */
      int pos;
      unsigned char buf[4];
      static unsigned char proto1[3] = { 0x87,0x04,0x24 };
      static unsigned char proto2[4] = { 0x87,0x44,0x24,0x00 };
      pos = codestream_tell ();
      codestream_read (buf, 4);
      if (bcmp (buf, proto1, 3) == 0)
	pos += 3;
      else if (bcmp (buf, proto2, 4) == 0)
	pos += 4;
      
      codestream_seek (pos);
      op = codestream_get (); /* update next opcode */
    }
  
  if (op == 0x55)		/* pushl %esp */
    {			
      /* check for movl %esp, %ebp - can be written two ways */
      switch (codestream_get ())
	{
	case 0x8b:
	  if (codestream_get () != 0xec)
	    return (-1);
	  break;
	case 0x89:
	  if (codestream_get () != 0xe5)
	    return (-1);
	  break;
	default:
	  return (-1);
	}
      /* check for stack adjustment 
       *
       *  subl $XXX, %esp
       *
       * note: you can't subtract a 16 bit immediate
       * from a 32 bit reg, so we don't have to worry
       * about a data16 prefix 
       */
      op = codestream_peek ();
      if (op == 0x83)
	{
	  /* subl with 8 bit immed */
	  codestream_get ();
	  if (codestream_get () != 0xec)
	    return (-1);
	  /* subl with signed byte immediate 
	   * (though it wouldn't make sense to be negative)
	   */
	  return (codestream_get());
	}
      else if (op == 0x81)
	{
	  /* subl with 32 bit immed */
	  int locals;
	  codestream_get();
	  if (codestream_get () != 0xec)
	    return (-1);
	  /* subl with 32 bit immediate */
	  codestream_read ((unsigned char *)&locals, 4);
	  return (locals);
	}
      else
	{
	  return (0);
	}
    }
  else if (op == 0xc8)
    {
      /* enter instruction: arg is 16 bit unsigned immed */
      unsigned short slocals;
      codestream_read ((unsigned char *)&slocals, 2);
      codestream_get (); /* flush final byte of enter instruction */
      return (slocals);
    }
  return (-1);
}

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.  */

/* on the 386, the instruction following the call could be:
 *  popl %ecx        -  one arg
 *  addl $imm, %esp  -  imm/4 args; imm may be 8 or 32 bits
 *  anything else    -  zero args
 */

int
i386_frame_num_args (fi)
     struct frame_info fi;
{
  int retpc;						
  unsigned char op;					
  struct frame_info *pfi;

  pfi = get_prev_frame_info ((fi));			
  if (pfi == 0)
    {
      /* Note:  this can happen if we are looking at the frame for
	 main, because FRAME_CHAIN_VALID won't let us go into
	 start.  If we have debugging symbols, that's not really
	 a big deal; it just means it will only show as many arguments
	 to main as are declared.  */
      return -1;
    }
  else
    {
      retpc = pfi->pc;					
      op = read_memory_integer (retpc, 1);			
      if (op == 0x59)					
	/* pop %ecx */			       
	return 1;				
      else if (op == 0x83)
	{
	  op = read_memory_integer (retpc+1, 1);	
	  if (op == 0xc4)				
	    /* addl $<signed imm 8 bits>, %esp */	
	    return (read_memory_integer (retpc+2,1)&0xff)/4;
	  else
	    return 0;
	}
      else if (op == 0x81)
	{ /* add with 32 bit immediate */
	  op = read_memory_integer (retpc+1, 1);	
	  if (op == 0xc4)				
	    /* addl $<imm 32>, %esp */		
	    return read_memory_integer (retpc+2, 4) / 4;
	  else
	    return 0;
	}
      else
	{
	  return 0;
	}
    }
}

/*
 * parse the first few instructions of the function to see
 * what registers were stored.
 *
 * We handle these cases:
 *
 * The startup sequence can be at the start of the function,
 * or the function can start with a branch to startup code at the end.
 *
 * %ebp can be set up with either the 'enter' instruction, or 
 * 'pushl %ebp, movl %esp, %ebp' (enter is too slow to be useful,
 * but was once used in the sys5 compiler)
 *
 * Local space is allocated just below the saved %ebp by either the
 * 'enter' instruction, or by 'subl $<size>, %esp'.  'enter' has
 * a 16 bit unsigned argument for space to allocate, and the
 * 'addl' instruction could have either a signed byte, or
 * 32 bit immediate.
 *
 * Next, the registers used by this function are pushed.  In
 * the sys5 compiler they will always be in the order: %edi, %esi, %ebx
 * (and sometimes a harmless bug causes it to also save but not restore %eax);
 * however, the code below is willing to see the pushes in any order,
 * and will handle up to 8 of them.
 *
 * If the setup sequence is at the end of the function, then the
 * next instruction will be a branch back to the start.
 */

i386_frame_find_saved_regs (fip, fsrp)
     struct frame_info *fip;
     struct frame_saved_regs *fsrp;
{
  unsigned long locals;
  unsigned char *p;
  unsigned char op;
  CORE_ADDR dummy_bottom;
  CORE_ADDR adr;
  int i;
  
  bzero (fsrp, sizeof *fsrp);
  
#if 0
  /* if frame is the end of a dummy, compute where the
   * beginning would be
   */
  dummy_bottom = fip->frame - 4 - NUM_REGS*4 - CALL_DUMMY_LENGTH;
  
  /* check if the PC is in the stack, in a dummy frame */
  if (dummy_bottom <= fip->pc && fip->pc <= fip->frame) 
    {
      /* all regs were saved by push_call_dummy () */
      adr = fip->frame - 4;
      for (i = 0; i < NUM_REGS; i++) 
	{
	  fsrp->regs[i] = adr;
	  adr -= 4;
	}
      return;
    }
#endif

  locals = i386_get_frame_setup (get_pc_function_start (fip->pc));
  
  if (locals >= 0) 
    {
      adr = fip->frame - 4 - locals;
      for (i = 0; i < 8; i++) 
	{
	  op = codestream_get ();
	  if (op < 0x50 || op > 0x57)
	    break;
	  fsrp->regs[op - 0x50] = adr;
	  adr -= 4;
	}
    }
  
  fsrp->regs[PC_REGNUM] = fip->frame + 4;
  fsrp->regs[FP_REGNUM] = fip->frame;
}

/* return pc of first real instruction */
i386_skip_prologue (pc)
{
  unsigned char op;
  int i;
  
  if (i386_get_frame_setup (pc) < 0)
    return (pc);
  
  /* found valid frame setup - codestream now points to 
   * start of push instructions for saving registers
   */
  
  /* skip over register saves */
  for (i = 0; i < 8; i++)
    {
      op = codestream_peek ();
      /* break if not pushl inst */
      if (op < 0x50 || op > 0x57) 
	break;
      codestream_get ();
    }
  
  i386_follow_jump ();
  
  return (codestream_tell ());
}

i386_pop_frame ()
{
  FRAME frame = get_current_frame ();
  CORE_ADDR fp;
  int regnum;
  struct frame_saved_regs fsr;
  struct frame_info *fi;
  
  fi = get_frame_info (frame);
  fp = fi->frame;
  get_frame_saved_regs (fi, &fsr);
  for (regnum = 0; regnum < NUM_REGS; regnum++) 
    {
      CORE_ADDR adr;
      adr = fsr.regs[regnum];
      if (adr)
	write_register (regnum, read_memory_integer (adr, 4));
    }
  write_register (FP_REGNUM, read_memory_integer (fp, 4));
  write_register (PC_REGNUM, read_memory_integer (fp + 4, 4));
  write_register (SP_REGNUM, fp + 8);
  flush_cached_frames ();
  set_current_frame ( create_new_frame (read_register (FP_REGNUM),
					read_pc ()));
}

/* this table must line up with REGISTER_NAMES in m-i386.h */
/* symbols like 'EAX' come from <sys/reg.h> */
static int trapmap[] = 
{
	tEAX, tECX, tEDX, tEBX,
	tESP, tEBP, tESI, tEDI,
	tEIP, tEFLAGS, tCS, tSS,
	tDS, tES, tES, tES		/* lies: no fs or gs */
};
static int syscallmap[] = 
{
	sEAX, sECX, sEDX, sEBX,
	sESP, sEBP, sESI, sEDI,
	sEIP, sEFLAGS, sCS, sSS,
	sCS, sCS, sCS, sCS		/* lies: no ds, es, fs or gs */
};
static int *regmap;

static void
setregmap(flags)
	int flags;
{
#ifdef FM_TRAP
	regmap = flags & FM_TRAP ? trapmap: syscallmap;
#elif EX_TRAPSTK
	regmap = flags & EX_TRAPSTK ? trapmap : syscallmap;
#else
	regmap = trapmap;	/* the lesser evil */
#endif
}

/* blockend is the value of u.u_ar0, and points to the
 * place where GS is stored
 */
i386_register_u_addr (blockend, regnum)
{
#if 0
  /* this will be needed if fp registers are reinstated */
  /* for now, you can look at them with 'info float'
   * sys5 wont let you change them with ptrace anyway
   */
  if (regnum >= FP0_REGNUM && regnum <= FP7_REGNUM) 
    {
      int ubase, fpstate;
      struct user u;
      ubase = blockend + 4 * (SS + 1) - KSTKSZ;
      fpstate = ubase + ((char *)&u.u_fpstate - (char *)&u);
      return (fpstate + 0x1c + 10 * (regnum - FP0_REGNUM));
    } 
  else
#endif
    return (blockend + 4 * regmap[regnum]);
}

i387_to_double (from, to)
     char *from;
     char *to;
{
  long *lp;
  /* push extended mode on 387 stack, then pop in double mode
   *
   * first, set exception masks so no error is generated -
   * number will be rounded to inf or 0, if necessary 
   */
  asm ("pushl %eax"); 		/* grab a stack slot */
  asm ("fstcw (%esp)");		/* get 387 control word */
  asm ("movl (%esp),%eax");	/* save old value */
  asm ("orl $0x3f,%eax");		/* mask all exceptions */
  asm ("pushl %eax");
  asm ("fldcw (%esp)");		/* load new value into 387 */
  
  asm ("movl 8(%ebp),%eax");
  asm ("fldt (%eax)");		/* push extended number on 387 stack */
  asm ("fwait");
  asm ("movl 12(%ebp),%eax");
  asm ("fstpl (%eax)");		/* pop double */
  asm ("fwait");
  
  asm ("popl %eax");		/* flush modified control word */
  asm ("fnclex");			/* clear exceptions */
  asm ("fldcw (%esp)");		/* restore original control word */
  asm ("popl %eax");		/* flush saved copy */
}

double_to_i387 (from, to)
     char *from;
     char *to;
{
  /* push double mode on 387 stack, then pop in extended mode
   * no errors are possible because every 64-bit pattern
   * can be converted to an extended
   */
  asm ("movl 8(%ebp),%eax");
  asm ("fldl (%eax)");
  asm ("fwait");
  asm ("movl 12(%ebp),%eax");
  asm ("fstpt (%eax)");
  asm ("fwait");
}

struct env387 
{
  unsigned short control;
  unsigned short r0;
  unsigned short status;
  unsigned short r1;
  unsigned short tag;
  unsigned short r2;
  unsigned long eip;
  unsigned short code_seg;
  unsigned short opcode;
  unsigned long operand;
  unsigned short operand_seg;
  unsigned short r3;
  unsigned char regs[8][10];
};

static
print_387_control_word (control)
unsigned short control;
{
  printf ("control 0x%04x: ", control);
  printf ("compute to ");
  switch ((control >> 8) & 3) 
    {
    case 0: printf ("24 bits; "); break;
    case 1: printf ("(bad); "); break;
    case 2: printf ("53 bits; "); break;
    case 3: printf ("64 bits; "); break;
    }
  printf ("round ");
  switch ((control >> 10) & 3) 
    {
    case 0: printf ("NEAREST; "); break;
    case 1: printf ("DOWN; "); break;
    case 2: printf ("UP; "); break;
    case 3: printf ("CHOP; "); break;
    }
  if (control & 0x3f) 
    {
      printf ("mask:");
      if (control & 0x0001) printf (" INVALID");
      if (control & 0x0002) printf (" DENORM");
      if (control & 0x0004) printf (" DIVZ");
      if (control & 0x0008) printf (" OVERF");
      if (control & 0x0010) printf (" UNDERF");
      if (control & 0x0020) printf (" LOS");
      printf (";");
    }
  printf ("\n");
  if (control & 0xe080) printf ("warning: reserved bits on 0x%x\n",
				control & 0xe080);
}

static
print_387_status_word (status)
     unsigned short status;
{
  printf ("status 0x%04x: ", status);
  if (status & 0xff) 
    {
      printf ("exceptions:");
      if (status & 0x0001) printf (" INVALID");
      if (status & 0x0002) printf (" DENORM");
      if (status & 0x0004) printf (" DIVZ");
      if (status & 0x0008) printf (" OVERF");
      if (status & 0x0010) printf (" UNDERF");
      if (status & 0x0020) printf (" LOS");
      if (status & 0x0040) printf (" FPSTACK");
      printf ("; ");
    }
  printf ("flags: %d%d%d%d; ",
	  (status & 0x4000) != 0,
	  (status & 0x0400) != 0,
	  (status & 0x0200) != 0,
	  (status & 0x0100) != 0);
  
  printf ("top %d\n", (status >> 11) & 7);
}

static
print_387_status (status, ep)
     unsigned short status;
     struct env387 *ep;
{
  int i;
  int bothstatus;
  int top;
  int fpreg;
  unsigned char *p;
  
  bothstatus = ((status != 0) && (ep->status != 0));
  if (status != 0) 
    {
      if (bothstatus)
	printf ("u: ");
      print_387_status_word (status);
    }
  
  if (ep->status != 0) 
    {
      if (bothstatus)
	printf ("e: ");
      print_387_status_word (ep->status);
    }
  
  print_387_control_word (ep->control);
  printf ("last exception: ");
  printf ("opcode 0x%x; ", ep->opcode);
  printf ("pc 0x%x:0x%x; ", ep->code_seg, ep->eip);
  printf ("operand 0x%x:0x%x\n", ep->operand_seg, ep->operand);
  
  top = (ep->status >> 11) & 7;
  
  printf (" regno     tag  msb              lsb  value\n");
  for (fpreg = 7; fpreg >= 0; fpreg--) 
    {
      int st_regno;
      double val;
      
      /* The physical regno `fpreg' is only relevant as an index into the
       * tag word.  Logical `%st' numbers are required for indexing `p->regs.
       */
      st_regno = (fpreg + 8 - top) & 0x7;

      printf ("%%st(%d) %s ", st_regno, fpreg == top ? "=>" : "  ");
      
      switch ((ep->tag >> (fpreg * 2)) & 3) 
	{
	case 0: printf ("valid "); break;
	case 1: printf ("zero  "); break;
	case 2: printf ("trap  "); break;
	case 3: printf ("empty "); break;
	}
      for (i = 9; i >= 0; i--)
	printf ("%02x", ep->regs[st_regno][i]);
      
      i387_to_double (ep->regs[st_regno], (char *)&val);
      printf ("  %g\n", val);
    }
#if 0 /* reserved fields are always 0xffff on 486's */
  if (ep->r0)
    printf ("warning: reserved0 is 0x%x\n", ep->r0);
  if (ep->r1)
    printf ("warning: reserved1 is 0x%x\n", ep->r1);
  if (ep->r2)
    printf ("warning: reserved2 is 0x%x\n", ep->r2);
  if (ep->r3)
    printf ("warning: reserved3 is 0x%x\n", ep->r3);
#endif
}

#ifdef __386BSD__
#define	fpstate		save87
#define	U_FPSTATE(u)	u.u_pcb.pcb_savefpu
#endif

#ifndef U_FPSTATE
#define U_FPSTATE(u) u.u_fpstate
#endif

i386_float_info ()
{
  struct user u; /* just for address computations */
  int i;
  /* fpstate defined in <sys/user.h> */
  struct fpstate *fpstatep;
  char buf[sizeof (struct fpstate) + 2 * sizeof (int)];
  unsigned int uaddr;
  char fpvalid;
  unsigned int rounded_addr;
  unsigned int rounded_size;
  extern int corechan;
  int skip;
  
#ifndef __386BSD__	/* XXX - look at pcb flags */
  uaddr = (char *)&u.u_fpvalid - (char *)&u;
  if (have_inferior_p()) 
    {
      unsigned int data;
      unsigned int mask;
      
      rounded_addr = uaddr & -sizeof (int);
      data = ptrace (PT_READ_U, inferior_pid, (caddr_t)rounded_addr, 0);
      mask = 0xff << ((uaddr - rounded_addr) * 8);
      
      fpvalid = ((data & mask) != 0);
    } 
  else 
    {
      if (lseek (corechan, uaddr, 0) < 0)
	perror ("seek on core file");
      if (myread (corechan, &fpvalid, 1) < 0) 
	perror ("read on core file");
      
    }
  
  if (fpvalid == 0) 
    {
      printf ("no floating point status saved\n");
      return;
    }
#endif	/* not __386BSD__ */
  
  uaddr = (char *)&U_FPSTATE(u) - (char *)&u;
  if (have_inferior_p ()) 
    {
      int *ip;
      
      rounded_addr = uaddr & -sizeof (int);
      rounded_size = (((uaddr + sizeof (struct fpstate)) - uaddr) +
		      sizeof (int) - 1) / sizeof (int);
      skip = uaddr - rounded_addr;
      
      ip = (int *)buf;
      for (i = 0; i < rounded_size; i++) 
	{
	  *ip++ = ptrace (PT_READ_U, inferior_pid, (caddr_t)rounded_addr, 0);
	  rounded_addr += sizeof (int);
	}
    } 
  else 
    {
      if (lseek (corechan, uaddr, 0) < 0)
	perror_with_name ("seek on core file");
      if (myread (corechan, buf, sizeof (struct fpstate)) < 0) 
	perror_with_name ("read from core file");
      skip = 0;
    }
  
#ifdef __386BSD__
  print_387_status (0, (struct env387 *)buf);
#else
  fpstatep = (struct fpstate *)(buf + skip);
  print_387_status (fpstatep->status, (struct env387 *)fpstatep->state);
#endif
}

void
_initialize_i386bsd_dep()
{
#ifdef KERNELDEBUG
	add_com ("process-address", class_obscure, set_paddr_command,
		 "The process identified by (ps-style) ADDR becomes the\n\
\"current\" process context for kernel debugging.");
	add_com_alias ("paddr", "process-address", class_obscure, 0);
#endif
}
