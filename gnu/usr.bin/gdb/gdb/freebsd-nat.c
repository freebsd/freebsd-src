/* Native-dependent code for BSD Unix running on i386's, for GDB.
   Copyright 1988, 1989, 1991, 1992 Free Software Foundation, Inc.

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

	$Id: freebsd-nat.c,v 1.7 1995/05/30 04:57:05 rgrimes Exp $
*/

#include <sys/types.h>
#include <sys/param.h>
#include <signal.h>
#include <sys/user.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <sys/ptrace.h>

#include "defs.h"

/* this table must line up with REGISTER_NAMES in tm-i386v.h */
/* symbols like 'tEAX' come from <machine/reg.h> */
static int tregmap[] =
{
  tEAX, tECX, tEDX, tEBX,
  tESP, tEBP, tESI, tEDI,
  tEIP, tEFLAGS, tCS, tSS,
  tDS, tES, tSS, tSS,		/* lies: no fs or gs */
};

/* blockend is the value of u.u_ar0, and points to the
   place where ES is stored.  */

int
i386_register_u_addr (blockend, regnum)
     int blockend;
     int regnum;
{
    return (blockend + 4 * tregmap[regnum]);
}


#define	fpstate		save87
#define	U_FPSTATE(u)	u.u_pcb.pcb_savefpu

static void
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

#if 0
static void
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
#endif

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

/* static */ void
print_387_control_word (control)
unsigned int control;
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

/* static */ void
print_387_status_word (status)
     unsigned int status;
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

static void
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
      print_387_status_word ((unsigned int)status);
    }

  if (ep->status != 0)
    {
      if (bothstatus)
	printf ("e: ");
      print_387_status_word ((unsigned int)ep->status);
    }

  print_387_control_word ((unsigned int)ep->control);
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
       * tag word.  Logical `%st' numbers are required for indexing ep->regs.
       */
      st_regno = (fpreg + 8 - top) & 7;

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
}

void
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
  /*extern int corechan;*/
  int skip;
  extern int inferior_pid;

  uaddr = (char *)&U_FPSTATE(u) - (char *)&u;
  if (inferior_pid)
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
#if 1
       printf("float info: can't do a core file (yet)\n");
       return;
#else
      if (lseek (corechan, uaddr, 0) < 0)
	perror_with_name ("seek on core file");
      if (myread (corechan, buf, sizeof (struct fpstate)) < 0)
	perror_with_name ("read from core file");
      skip = 0;
#endif
    }

  fpstatep = (struct fpstate *)(buf + skip);
  print_387_status (fpstatep->sv_ex_sw, (struct env387 *)fpstatep);
}

#ifdef SETUP_ARBITRARY_FRAME
FRAME
setup_arbitrary_frame (numargs, args)
int numargs;
unsigned int *args;
{
      if (numargs > 2)
                    error ("Too many args in frame specification");
      return create_new_frame ((CORE_ADDR)args[0], (CORE_ADDR)args[1]);
}
#endif

#ifdef KERNEL_DEBUG
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <machine/vmparam.h>
#include <machine/pcb.h>

#define	KERNOFF		((unsigned)KERNBASE)
#define	INKERNEL(x)	((x) >= KERNOFF)

static CORE_ADDR sbr;
static CORE_ADDR curpcb;
static CORE_ADDR kstack;
static int found_pcb;
static int devmem;
static int kfd;
static struct pcb pcb;
int read_pcb (int, CORE_ADDR);
static CORE_ADDR kvtophys (int, CORE_ADDR);
static physrd(int, u_int, char*, int);

extern CORE_ADDR ksym_lookup(const char *);

/* substitutes for the stuff in libkvm which doesn't work */
/* most of this was taken from the old kgdb */

/* we don't need all this stuff, but the call should look the same */
kvm_open (efile, cfile, sfile, perm, errout)
char *efile;
char *cfile;
char *sfile; /* makes this kvm_open more compatible to the one in libkvm */
int perm;
char *errout; /* makes this kvm_open more compatible to the one in libkvm */
{
	struct stat stb;
	CORE_ADDR addr;
	int cfd;

	if ((cfd = open(cfile, perm, 0)) < 0)
		return (cfd);

	fstat(cfd, &stb);
	if ((stb.st_mode & S_IFMT) == S_IFCHR && stb.st_rdev == makedev(2, 0)) {
		devmem = 1;
		kfd = open ("/dev/kmem", perm, 0);
	}

	physrd(cfd, ksym_lookup("IdlePTD") - KERNOFF, (char*)&sbr, sizeof sbr);
	printf("IdlePTD %x\n", sbr);
	curpcb = ksym_lookup("curpcb") - KERNOFF;
	physrd(cfd, curpcb, (char*)&curpcb, sizeof curpcb);
	kstack = ksym_lookup("kstack");

	found_pcb = 1; /* for vtophys */
	if (!devmem)
		read_pcb(cfd, ksym_lookup("dumppcb") - KERNOFF);
	else
		read_pcb(cfd, kvtophys(cfd, kstack));

	return (cfd);
}

kvm_close (fd)
{
	return (close (fd));
}

kvm_write(core_kd, memaddr, myaddr, len)
CORE_ADDR memaddr;
char *myaddr;
{
	int cc;

	if (devmem) {
		if (kfd > 0) {
			/*
		 	* Just like kvm_read, only we write.
		 	*/
			errno = 0;
			if (lseek(kfd, (off_t)memaddr, 0) < 0 && errno != 0) {
				error("kvm_write:invalid address (%x)", memaddr);
				return (0);
			}
			cc = write(kfd, myaddr, len);
			if (cc < 0) {
				error("kvm_write:write failed");
				return (0);
			} else if (cc < len)
				error("kvm_write:short write");
			return (cc);
		} else
			return (0);
	} else {
		printf("kvm_write not implemented for dead kernels\n");
		return (0);
	}
	/* NOTREACHED */
}

kvm_read(core_kd, memaddr, myaddr, len)
CORE_ADDR memaddr;
char *myaddr;
{
	return (kernel_core_file_hook (core_kd, memaddr, myaddr, len));
}

kvm_uread(core_kd, p, memaddr, myaddr, len)
register struct proc *p;
CORE_ADDR memaddr;
char *myaddr;
{
	register char *cp;
	char procfile[MAXPATHLEN];
	ssize_t amount;
	int fd;

	if (devmem) {
		cp = myaddr;

		sprintf(procfile, "/proc/%d/mem", p->p_pid);
		fd = open(procfile, O_RDONLY, 0);

		if (fd < 0) {
			error("cannot open %s", procfile);
			close(fd);
			return (0);
		}

		while (len > 0) {
			if (lseek(fd, memaddr, 0) == -1 && errno != 0) {
				error("invalid address (%x) in %s",
					memaddr, procfile);
				break;
			}
			amount = read(fd, cp, len);
			if (amount < 0) {
				error("error reading %s", procfile);
				break;
			}
			cp += amount;
			memaddr += amount;
			len -= amount;
		}

		close(fd);
		return (ssize_t)(cp - myaddr);
	} else {
		return (kernel_core_file_hook (core_kd, memaddr, myaddr, len));
	}
}

static
physrd(cfd, addr, dat, len)
u_int addr;
char *dat;
{
	if (lseek(cfd, (off_t)addr, L_SET) == -1)
		return (-1);
	return (read(cfd, dat, len));
}

static CORE_ADDR
kvtophys (fd, addr)
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
			PTD = kvtophys(fd, ksym_lookup("PTD"));
		current_ptd = PTD;
	} else
		current_ptd = pcb.pcb_ptd;

	/*
	 * Read the first-level page table (ptd).
	 */
	v = current_ptd + ((unsigned)addr >> PD_SHIFT) * sizeof pte;
	if (physrd(fd, v, (char *)&pte, sizeof pte) < 0 || pte.pg_v == 0)
		return (~0);

	/*
	 * Read the second-level page table.
	 */
	v = i386_ptob(pte.pg_pfnum) + ((addr&PT_MASK) >> PG_SHIFT) * sizeof pte;
	if (physrd(fd, v, (char *) &pte, sizeof(pte)) < 0 || pte.pg_v == 0)
		return (~0);

	addr = i386_ptob(pte.pg_pfnum) + (addr & PGOFSET);
#if 0
	printf("vtophys(%x) -> %x\n", oldaddr, addr);
#endif
	return (addr);
}

read_pcb (fd, uaddr)
CORE_ADDR uaddr;
{
	int i;
	int *pcb_regs = (int *)&pcb;
	int	eip;

	if (physrd(fd, uaddr, (char *)&pcb, sizeof pcb) < 0) {
		error("cannot read pcb at %x\n", uaddr);
		return (-1);
	}
	printf("current pcb at %x\n", uaddr);

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

#if 0 /* doesn't work ??? */
	/* Hmm... */
	if (target_read_memory(pcb_regs[5+10]+4, &eip, sizeof eip, 0))
		error("Cannot read PC.");
	supply_register(8, &eip);	/* eip */
#endif

	/* XXX 80387 registers? */
}

/*
 * read len bytes from kernel virtual address 'addr' into local
 * buffer 'buf'.  Return numbert of bytes if read ok, 0 otherwise.  On read
 * errors, portion of buffer not read is zeroed.
 */
kernel_core_file_hook(fd, addr, buf, len)
	CORE_ADDR addr;
	char *buf;
	int len;
{
	int i;
	CORE_ADDR paddr;
	register char *cp;
	int cc;

	cp = buf;

	while (len > 0) {
		paddr = kvtophys(fd, addr);
		if (paddr == ~0) {
			bzero(buf, len);
			break;
		}
		/* we can't read across a page boundary */
		i = min(len, NBPG - (addr & PGOFSET));
		if ((cc = physrd(fd, paddr, cp, i)) <= 0) {
			bzero(cp, len);
			return (cp - buf);
		}
		cp += cc;
		addr += cc;
		len -= cc;
	}
	return (cp - buf);
}
#endif /* KERNEL_DEBUG */
