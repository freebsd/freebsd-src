/*-
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         3       00136
 * --------------------         -----   ----------------------
 *
 * 08 Sep 92	Greenman & Kranenburg	Change vaddr calc, move bogus #endif
 * 05 Aug 92	David Greenman          Fix kernel namelist db create/use
 * 08 Aug 93	Paul Kranenburg		Fix for command line args from ps and w
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)kvm.c	5.18 (Berkeley) 5/7/91";
#endif /* LIBC_SCCS and not lint */

/*
 *  Updated for 386BSD 0.1 by David Greenman (davidg%implode@percy.rain.com)
 *     and Paul Kranenburg (pk@cs.few.eur.nl)
 *  20-Aug-1992
 *  And again by same on 04-Aug-1993
 */


#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/kinfo.h>
#include <sys/tty.h>
#include <machine/vmparam.h>
#include <fcntl.h>
#include <nlist.h>
#include <kvm.h>
#include <ndbm.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>

#ifdef SPPWAIT
#define NEWVM
#endif

#ifdef NEWVM
#define	btop(x)		(((unsigned)(x)) >> PGSHIFT)	/* XXX */
#define	ptob(x)		((caddr_t)((x) << PGSHIFT))	/* XXX */
#include <vm/vm.h>	/* ??? kinfo_proc currently includes this*/
#include <vm/vm_page.h>
#include <vm/swap_pager.h>
#include <sys/kinfo_proc.h>
#ifdef hp300
#include <hp300/hp300/pte.h>
#endif
#else /* NEWVM */
#include <machine/pte.h>
#include <sys/vmmac.h>
#include <sys/text.h>
#endif /* NEWVM */

/*
 * files
 */
static	const char *unixf, *memf, *kmemf, *swapf;
static	int unixx, mem, kmem, swap;
static	DBM *db;
/*
 * flags
 */
static	int deadkernel;
static	int kvminit = 0;
static	int kvmfilesopen = 0;
/*
 * state
 */
static	struct kinfo_proc *kvmprocbase, *kvmprocptr;
static	int kvmnprocs;
/*
 * u. buffer
 */
static union {
	struct	user user;
	char	upages[UPAGES][NBPG];
} user;

#ifdef NEWVM
struct swapblk {
	long	offset;		/* offset in swap device */
	long	size;		/* remaining size of block in swap device */
};
#endif
/*
 * random other stuff
 */
#ifndef NEWVM
static	struct pte *Usrptmap, *usrpt;
static	struct	pte *Sysmap;
static	int	Syssize;
#endif
static	int	dmmin, dmmax;
static	int	pcbpf;
static	int	argaddr0;	/* XXX */
static	int	argaddr1;
static	int	swaddr;
static	int	nswap;
static	char	*tmp;
#if defined(hp300)
static	int	lowram;
static	struct ste *Sysseg;
#endif
#if defined(i386)
static	struct pde *PTD;
#endif

#define basename(cp)	((tmp=rindex((cp), '/')) ? tmp+1 : (cp))
#define	MAXSYMSIZE	256

#if defined(hp300)
#define pftoc(f)	((f) - lowram)
#define iskva(v)	(1)
#endif

#ifndef pftoc
#define pftoc(f)	(f)
#endif
#ifndef iskva
#define iskva(v)	((u_long)(v) & KERNBASE)
#endif

static struct nlist nl[] = {
	{ "_Usrptmap" },
#define	X_USRPTMAP	0
	{ "_usrpt" },
#define	X_USRPT		1
	{ "_nswap" },
#define	X_NSWAP		2
	{ "_dmmin" },
#define	X_DMMIN		3
	{ "_dmmax" },
#define	X_DMMAX		4
	{ "_vm_page_buckets" },
#define X_VM_PAGE_BUCKETS	5
	{ "_vm_page_hash_mask" },
#define X_VM_PAGE_HASH_MASK	6
	{ "_page_shift" },
#define X_PAGE_SHIFT	7
	/*
	 * everything here and down, only if a dead kernel
	 */
	{ "_Sysmap" },
#define	X_SYSMAP	8
#define	X_DEADKERNEL	X_SYSMAP
	{ "_Syssize" },
#define	X_SYSSIZE	9
	{ "_allproc" },
#define X_ALLPROC	10
	{ "_zombproc" },
#define X_ZOMBPROC	11
	{ "_nproc" },
#define	X_NPROC		12
#define	X_LAST		12
#if defined(hp300)
	{ "_Sysseg" },
#define	X_SYSSEG	(X_LAST+1)
	{ "_lowram" },
#define	X_LOWRAM	(X_LAST+2)
#endif
#if defined(i386)
	{ "_IdlePTD" },
#define	X_IdlePTD	(X_LAST+1)
#endif
	{ "" },
};

static off_t Vtophys();
static void klseek(), seterr(), setsyserr(), vstodb();
static int getkvars(), kvm_doprocs(), kvm_init();
#ifdef NEWVM
static int vatosw();
static int findpage();
#endif

/*
 * returns 	0 if files were opened now,
 * 		1 if files were already opened,
 *		-1 if files could not be opened.
 */
kvm_openfiles(uf, mf, sf)
	const char *uf, *mf, *sf; 
{
	if (kvmfilesopen)
		return (1);
	unixx = mem = kmem = swap = -1;
	unixf = (uf == NULL) ? _PATH_UNIX : uf; 
	memf = (mf == NULL) ? _PATH_MEM : mf;

	if ((unixx = open(unixf, O_RDONLY, 0)) == -1) {
		setsyserr("can't open %s", unixf);
		goto failed;
	}
	if ((mem = open(memf, O_RDONLY, 0)) == -1) {
		setsyserr("can't open %s", memf);
		goto failed;
	}
	if (sf != NULL)
		swapf = sf;
	if (mf != NULL) {
		deadkernel++;
		kmemf = mf;
		kmem = mem;
		swap = -1;
	} else {
		kmemf = _PATH_KMEM;
		if ((kmem = open(kmemf, O_RDONLY, 0)) == -1) {
			setsyserr("can't open %s", kmemf);
			goto failed;
		}
		swapf = (sf == NULL) ?  _PATH_DRUM : sf;
		/*
		 * live kernel - avoid looking up nlist entries
		 * past X_DEADKERNEL.
		 */
		nl[X_DEADKERNEL].n_name = "";
	}
	if (swapf != NULL && ((swap = open(swapf, O_RDONLY, 0)) == -1)) {
		seterr("can't open %s", swapf);
		goto failed;
	}
	kvmfilesopen++;
	if (kvminit == 0 && kvm_init(NULL, NULL, NULL, 0) == -1) /*XXX*/
		return (-1);
	return (0);
failed:
	kvm_close();
	return (-1);
}

static
kvm_init(uf, mf, sf)
	char *uf, *mf, *sf;
{
	if (kvmfilesopen == 0 && kvm_openfiles(NULL, NULL, NULL) == -1)
		return (-1);
	if (getkvars() == -1)
		return (-1);
	kvminit = 1;

	return (0);
}

kvm_close()
{
	if (unixx != -1) {
		close(unixx);
		unixx = -1;
	}
	if (kmem != -1) {
		if (kmem != mem)
			close(kmem);
		/* otherwise kmem is a copy of mem, and will be closed below */
		kmem = -1;
	}
	if (mem != -1) {
		close(mem);
		mem = -1;
	}
	if (swap != -1) {
		close(swap);
		swap = -1;
	}
	if (db != NULL) {
		dbm_close(db);
		db = NULL;
	}
	kvminit = 0;
	kvmfilesopen = 0;
	deadkernel = 0;
#ifndef NEWVM
	if (Sysmap) {
		free(Sysmap);
		Sysmap = NULL;
	}
#endif
}

kvm_nlist(nl)
	struct nlist *nl;
{
	datum key, data;
	char dbname[MAXPATHLEN];
	char dbversion[_POSIX2_LINE_MAX];
	char kversion[_POSIX2_LINE_MAX];
	int dbversionlen;
	char symbuf[MAXSYMSIZE];
	struct nlist nbuf, *n;
	int num, did;

	if (kvmfilesopen == 0 && kvm_openfiles(NULL, NULL, NULL) == -1)
		return (-1);
	if (deadkernel)
		goto hard2;
	/*
	 * initialize key datum
	 */
	key.dptr = symbuf;

	if (db != NULL)
		goto win;	/* off to the races */
	/*
	 * open database
	 */
	sprintf(dbname, "%s/kvm_%s", _PATH_VARRUN, basename(unixf));
	if ((db = dbm_open(dbname, O_RDONLY, 0)) == NULL)
		goto hard2;
	/*
	 * read version out of database
	 */
	bcopy("VERSION", symbuf, sizeof ("VERSION")-1);
	key.dsize = (sizeof ("VERSION") - 1);
	data = dbm_fetch(db, key);
	if (data.dptr == NULL)
		goto hard1;
	bcopy(data.dptr, dbversion, data.dsize);
	dbversionlen = data.dsize;
	/*
	 * read version string from kernel memory
	 */
	bcopy("_version", symbuf, sizeof ("_version")-1);
	key.dsize = (sizeof ("_version")-1);
	data = dbm_fetch(db, key);
	if (data.dptr == NULL)
		goto hard1;
	if (data.dsize != sizeof (struct nlist))
		goto hard1;
	bcopy(data.dptr, &nbuf, sizeof (struct nlist));
	lseek(kmem, nbuf.n_value, 0);
	if (read(kmem, kversion, dbversionlen) != dbversionlen)
		goto hard1;
	/*
	 * if they match, we win - otherwise do it the hard way
	 */
	if (bcmp(dbversion, kversion, dbversionlen) != 0)
		goto hard1;
	/*
	 * getem from the database.
	 */
win:
	num = did = 0;
	for (n = nl; n->n_name && n->n_name[0]; n++, num++) {
		int len;
		/*
		 * clear out fields from users buffer
		 */
		n->n_type = 0;
		n->n_other = 0;
		n->n_desc = 0;
		n->n_value = 0;
		/*
		 * query db
		 */
		if ((len = strlen(n->n_name)) > MAXSYMSIZE) {
			seterr("symbol too large");
			return (-1);
		}
		(void)strcpy(symbuf, n->n_name);
		key.dsize = len;
		data = dbm_fetch(db, key);
		if (data.dptr == NULL || data.dsize != sizeof (struct nlist))
			continue;
		bcopy(data.dptr, &nbuf, sizeof (struct nlist));
		n->n_value = nbuf.n_value;
		n->n_type = nbuf.n_type;
		n->n_desc = nbuf.n_desc;
		n->n_other = nbuf.n_other;
		did++;
	}
	return (num - did);
hard1:
	dbm_close(db);
	db = NULL;
hard2:
	num = nlist(unixf, nl);
	if (num == -1)
		seterr("nlist (hard way) failed");
	return (num);
}

kvm_getprocs(what, arg)
	int what, arg;
{
	static int	ocopysize = -1;

	if (kvminit == 0 && kvm_init(NULL, NULL, NULL, 0) == -1)
		return (NULL);
	if (!deadkernel) {
		int ret, copysize;

		if ((ret = getkerninfo(what, NULL, NULL, arg)) == -1) {
			setsyserr("can't get estimate for kerninfo");
			return (-1);
		}
		copysize = ret;
		if (copysize > ocopysize &&
			(kvmprocbase = (struct kinfo_proc *)malloc(copysize)) 
								     == NULL) {
			seterr("out of memory");
			return (-1);
		}
		ocopysize = copysize;
		if ((ret = getkerninfo(what, kvmprocbase, &copysize, 
		     arg)) == -1) {
			setsyserr("can't get proc list");
			return (-1);
		}
		if (copysize % sizeof (struct kinfo_proc)) {
			seterr("proc size mismatch (got %d total, kinfo_proc: %d)",
				copysize, sizeof (struct kinfo_proc));
			return (-1);
		}
		kvmnprocs = copysize / sizeof (struct kinfo_proc);
	} else {
		int nproc;

		if (kvm_read((void *) nl[X_NPROC].n_value, &nproc,
		    sizeof (int)) != sizeof (int)) {
			seterr("can't read nproc");
			return (-1);
		}
		if ((kvmprocbase = (struct kinfo_proc *)
		     malloc(nproc * sizeof (struct kinfo_proc))) == NULL) {
			seterr("out of memory (addr: %x nproc = %d)",
				nl[X_NPROC].n_value, nproc);
			return (-1);
		}
		kvmnprocs = kvm_doprocs(what, arg, kvmprocbase);
		realloc(kvmprocbase, kvmnprocs * sizeof (struct kinfo_proc));
	}
	kvmprocptr = kvmprocbase;

	return (kvmnprocs);
}

/*
 * XXX - should NOT give up so easily - especially since the kernel
 * may be corrupt (it died).  Should gather as much information as possible.
 * Follows proc ptrs instead of reading table since table may go
 * away soon.
 */
static
kvm_doprocs(what, arg, buff)
	int what, arg;
	char *buff;
{
	struct proc *p, proc;
	register char *bp = buff;
	int i = 0;
	int doingzomb = 0;
	struct eproc eproc;
	struct pgrp pgrp;
	struct session sess;
	struct tty tty;
#ifndef NEWVM
	struct text text;
#endif

	/* allproc */
	if (kvm_read((void *) nl[X_ALLPROC].n_value, &p, 
	    sizeof (struct proc *)) != sizeof (struct proc *)) {
		seterr("can't read allproc");
		return (-1);
	}

again:
	for (; p; p = proc.p_nxt) {
		if (kvm_read(p, &proc, sizeof (struct proc)) !=
		    sizeof (struct proc)) {
			seterr("can't read proc at %x", p);
			return (-1);
		}
#ifdef NEWVM
		if (kvm_read(proc.p_cred, &eproc.e_pcred,
		    sizeof (struct pcred)) == sizeof (struct pcred))
			(void) kvm_read(eproc.e_pcred.pc_ucred, &eproc.e_ucred,
			    sizeof (struct ucred));
		switch(ki_op(what)) {
			
		case KINFO_PROC_PID:
			if (proc.p_pid != (pid_t)arg)
				continue;
			break;


		case KINFO_PROC_UID:
			if (eproc.e_ucred.cr_uid != (uid_t)arg)
				continue;
			break;

		case KINFO_PROC_RUID:
			if (eproc.e_pcred.p_ruid != (uid_t)arg)
				continue;
			break;
		}
#else
		switch(ki_op(what)) {
			
		case KINFO_PROC_PID:
			if (proc.p_pid != (pid_t)arg)
				continue;
			break;


		case KINFO_PROC_UID:
			if (proc.p_uid != (uid_t)arg)
				continue;
			break;

		case KINFO_PROC_RUID:
			if (proc.p_ruid != (uid_t)arg)
				continue;
			break;
		}
#endif
		/*
		 * gather eproc
		 */
		eproc.e_paddr = p;
		if (kvm_read(proc.p_pgrp, &pgrp, sizeof (struct pgrp)) !=
	            sizeof (struct pgrp)) {
			seterr("can't read pgrp at %x", proc.p_pgrp);
			return (-1);
		}
		eproc.e_sess = pgrp.pg_session;
		eproc.e_pgid = pgrp.pg_id;
		eproc.e_jobc = pgrp.pg_jobc;
		if (kvm_read(pgrp.pg_session, &sess, sizeof (struct session))
		   != sizeof (struct session)) {
			seterr("can't read session at %x", pgrp.pg_session);
			return (-1);
		}
		if ((proc.p_flag&SCTTY) && sess.s_ttyp != NULL) {
			if (kvm_read(sess.s_ttyp, &tty, sizeof (struct tty))
			    != sizeof (struct tty)) {
				seterr("can't read tty at %x", sess.s_ttyp);
				return (-1);
			}
			eproc.e_tdev = tty.t_dev;
			eproc.e_tsess = tty.t_session;
			if (tty.t_pgrp != NULL) {
				if (kvm_read(tty.t_pgrp, &pgrp, sizeof (struct
				    pgrp)) != sizeof (struct pgrp)) {
					seterr("can't read tpgrp at &x", 
						tty.t_pgrp);
					return (-1);
				}
				eproc.e_tpgid = pgrp.pg_id;
			} else
				eproc.e_tpgid = -1;
		} else
			eproc.e_tdev = NODEV;
		if (proc.p_wmesg)
			kvm_read(proc.p_wmesg, eproc.e_wmesg, WMESGLEN);
#ifdef NEWVM
		(void) kvm_read(proc.p_vmspace, &eproc.e_vm,
		    sizeof (struct vmspace));
		eproc.e_xsize = eproc.e_xrssize =
			eproc.e_xccount = eproc.e_xswrss = 0;
#else
		if (proc.p_textp) {
			kvm_read(proc.p_textp, &text, sizeof (text));
			eproc.e_xsize = text.x_size;
			eproc.e_xrssize = text.x_rssize;
			eproc.e_xccount = text.x_ccount;
			eproc.e_xswrss = text.x_swrss;
		} else {
			eproc.e_xsize = eproc.e_xrssize =
			  eproc.e_xccount = eproc.e_xswrss = 0;
		}
#endif

		switch(ki_op(what)) {

		case KINFO_PROC_PGRP:
			if (eproc.e_pgid != (pid_t)arg)
				continue;
			break;

		case KINFO_PROC_TTY:
			if ((proc.p_flag&SCTTY) == 0 || 
			     eproc.e_tdev != (dev_t)arg)
				continue;
			break;
		}

		i++;
		bcopy(&proc, bp, sizeof (struct proc));
		bp += sizeof (struct proc);
		bcopy(&eproc, bp, sizeof (struct eproc));
		bp+= sizeof (struct eproc);
	}
	if (!doingzomb) {
		/* zombproc */
		if (kvm_read((void *) nl[X_ZOMBPROC].n_value, &p, 
		    sizeof (struct proc *)) != sizeof (struct proc *)) {
			seterr("can't read zombproc");
			return (-1);
		}
		doingzomb = 1;
		goto again;
	}

	return (i);
}

struct proc *
kvm_nextproc()
{

	if (!kvmprocbase && kvm_getprocs(0, 0) == -1)
		return (NULL);
	if (kvmprocptr >= (kvmprocbase + kvmnprocs)) {
		seterr("end of proc list");
		return (NULL);
	}
	return((struct proc *)(kvmprocptr++));
}

struct eproc *
kvm_geteproc(p)
	const struct proc *p;
{
	return ((struct eproc *)(((char *)p) + sizeof (struct proc)));
}

kvm_setproc()
{
	kvmprocptr = kvmprocbase;
}

kvm_freeprocs()
{

	if (kvmprocbase) {
		free(kvmprocbase);
		kvmprocbase = NULL;
	}
}

#ifdef i386
/* See also ./sys/kern/kern_execve.c */
#define ARGSIZE		(roundup(ARG_MAX, NBPG))
#endif

#ifdef NEWVM
struct user *
kvm_getu(p)
	const struct proc *p;
{
	register struct kinfo_proc *kp = (struct kinfo_proc *)p;
	register int i;
	register char *up;
	u_int vaddr;
	struct swapblk swb;

	if (kvminit == 0 && kvm_init(NULL, NULL, NULL, 0) == -1)
		return (NULL);
	if (p->p_stat == SZOMB) {
		seterr("zombie process");
		return (NULL);
	}

	argaddr0 = argaddr1 = swaddr = 0;
	if ((p->p_flag & SLOAD) == 0) {
		vm_offset_t	maddr;

		if (swap < 0) {
			seterr("no swap");
			return (NULL);
		}
		/*
		 * Costly operation, better set enable_swap to zero
		 * in vm/vm_glue.c, since paging of user pages isn't
		 * done yet anyway.
		 */
		if (vatosw(p, USRSTACK + i * NBPG, &maddr, &swb) == 0)
			return NULL;

		if (maddr == 0 && swb.size < UPAGES * NBPG)
			return NULL;

		for (i = 0; i < UPAGES; i++) {
			if (maddr) {
				(void) lseek(mem, maddr + i * NBPG, 0);
				if (read(mem,
				    (char *)user.upages[i], NBPG) != NBPG) {
					seterr(
					    "can't read u for pid %d from %s",
					    p->p_pid, swapf);
					return NULL;
				}
			} else {
				(void) lseek(swap, swb.offset + i * NBPG, 0);
				if (read(swap,
				    (char *)user.upages[i], NBPG) != NBPG) {
					seterr(
					    "can't read u for pid %d from %s",
					    p->p_pid, swapf);
					return NULL;
				}
			}
		}
		return(&user.user);
	}
	/*
	 * Read u-area one page at a time for the benefit of post-mortems
	 */
	up = (char *) p->p_addr;
	for (i = 0; i < UPAGES; i++) {
		klseek(kmem, (long)up, 0);
		if (read(kmem, user.upages[i], CLBYTES) != CLBYTES) {
			seterr("cant read page %x of u of pid %d from %s",
			    up, p->p_pid, kmemf);
			return(NULL);
		}
		up += CLBYTES;
	}
	pcbpf = (int) btop(p->p_addr);	/* what should this be really? */
	/*
	 * Conjure up a physical address for the arguments.
	 */
#ifdef hp300
	if (kp->kp_eproc.e_vm.vm_pmap.pm_ptab) {
		struct pte pte[CLSIZE*2];

		klseek(kmem,
		    (long)&kp->kp_eproc.e_vm.vm_pmap.pm_ptab
		    [btoc(USRSTACK-CLBYTES*2)], 0);
		if (read(kmem, (char *)&pte, sizeof(pte)) == sizeof(pte)) {
#if CLBYTES < 2048
			argaddr0 = ctob(pftoc(pte[CLSIZE*0].pg_pfnum));
#endif
			argaddr1 = ctob(pftoc(pte[CLSIZE*1].pg_pfnum));
		}
	}
#endif
	kp->kp_eproc.e_vm.vm_rssize =
	    kp->kp_eproc.e_vm.vm_pmap.pm_stats.resident_count; /* XXX */

	vaddr = (u_int)kp->kp_eproc.e_vm.vm_maxsaddr + MAXSSIZ - ARGSIZE;

#ifdef i386
	if (kp->kp_eproc.e_vm.vm_pmap.pm_pdir) {
		struct pde pde;

		klseek(kmem,
		(long)(&kp->kp_eproc.e_vm.vm_pmap.pm_pdir[pdei(vaddr)]), 0);

		if (read(kmem, (char *)&pde, sizeof pde) == sizeof pde
				&& pde.pd_v) {

			struct pte pte;

			if (lseek(mem, (long)ctob(pde.pd_pfnum) +
					(ptei(vaddr) * sizeof pte), 0) == -1)
				seterr("kvm_getu: lseek");
			if (read(mem, (char *)&pte, sizeof pte) == sizeof pte) {
				if (pte.pg_v) {
					argaddr1 = (long)ctob(pte.pg_pfnum);
				} else {
					goto hard;
				}
			} else {
				seterr("kvm_getu: read");
			}
		} else {
			goto hard;
		}
	}
#endif	/* i386 */

hard:
	if (vatosw(p, vaddr, &argaddr1, &swb)) {
		if (argaddr1 == 0 && swb.size >= ARGSIZE)
			swaddr = swb.offset;
	}

	return(&user.user);
}
#else
struct user *
kvm_getu(p)
	const struct proc *p;
{
	struct pte *pteaddr, apte;
	struct pte arguutl[HIGHPAGES+(CLSIZE*2)];
	register int i;
	int ncl;

	if (kvminit == 0 && kvm_init(NULL, NULL, NULL, 0) == -1)
		return (NULL);
	if (p->p_stat == SZOMB) {
		seterr("zombie process");
		return (NULL);
	}
	if ((p->p_flag & SLOAD) == 0) {
		if (swap < 0) {
			seterr("no swap");
			return (NULL);
		}
		(void) lseek(swap, (long)dtob(p->p_swaddr), 0);
		if (read(swap, (char *)&user.user, sizeof (struct user)) != 
		    sizeof (struct user)) {
			seterr("can't read u for pid %d from %s",
			    p->p_pid, swapf);
			return (NULL);
		}
		pcbpf = 0;
		argaddr0 = 0;
		argaddr1 = 0;
		return (&user.user);
	}
	pteaddr = &Usrptmap[btokmx(p->p_p0br) + p->p_szpt - 1];
	klseek(kmem, (long)pteaddr, 0);
	if (read(kmem, (char *)&apte, sizeof(apte)) != sizeof(apte)) {
		seterr("can't read indir pte to get u for pid %d from %s",
		    p->p_pid, kmemf);
		return (NULL);
	}
	lseek(mem, (long)ctob(pftoc(apte.pg_pfnum+1)) - sizeof(arguutl), 0);
	if (read(mem, (char *)arguutl, sizeof(arguutl)) != sizeof(arguutl)) {
		seterr("can't read page table for u of pid %d from %s",
		    p->p_pid, memf);
		return (NULL);
	}
	if (arguutl[0].pg_fod == 0 && arguutl[0].pg_pfnum)
		argaddr0 = ctob(pftoc(arguutl[0].pg_pfnum));
	else
		argaddr0 = 0;
	if (arguutl[CLSIZE*1].pg_fod == 0 && arguutl[CLSIZE*1].pg_pfnum)
		argaddr1 = ctob(pftoc(arguutl[CLSIZE*1].pg_pfnum));
	else
		argaddr1 = 0;
	pcbpf = arguutl[CLSIZE*2].pg_pfnum;
	ncl = (sizeof (struct user) + CLBYTES - 1) / CLBYTES;
	while (--ncl >= 0) {
		i = ncl * CLSIZE;
		lseek(mem,
		      (long)ctob(pftoc(arguutl[(CLSIZE*2)+i].pg_pfnum)), 0);
		if (read(mem, user.upages[i], CLBYTES) != CLBYTES) {
			seterr("can't read page %d of u of pid %d from %s",
			    arguutl[(CLSIZE*2)+i].pg_pfnum, p->p_pid, memf);
			return(NULL);
		}
	}
	return (&user.user);
}
#endif

char *
kvm_getargs(p, up)
	const struct proc *p;
	const struct user *up;
{
#ifdef i386
	/* See also ./sys/kern/kern_execve.c */
	static char cmdbuf[ARGSIZE];
	static union {
		char	argc[ARGSIZE];
		int	argi[ARGSIZE/sizeof (int)];
	} argspac;
#else
	static char cmdbuf[CLBYTES*2];
	static union {
		char	argc[CLBYTES*2];
		int	argi[CLBYTES*2/sizeof (int)];
	} argspac;
#endif
	register char *cp;
	register int *ip;
	char c;
	int nbad;
#ifndef NEWVM
	struct dblock db;
#endif
	const char *file;
	int stkoff = 0;

#if defined(NEWVM) && defined(hp300)
	stkoff = 20;			/* XXX for sigcode */
#endif
	if (up == NULL || p->p_pid == 0 || p->p_pid == 2)
		goto retucomm;
	if ((p->p_flag & SLOAD) == 0 || argaddr1 == 0) {
#ifdef NEWVM
		if (swaddr == 0)
			goto retucomm;	/* XXX for now */
#ifdef i386
		(void) lseek(swap, swaddr, 0);
		if (read(swap, &argspac.argc[0], ARGSIZE) != ARGSIZE)
			goto bad;
#else
		if (argaddr0) {
			lseek(swap, (long)argaddr0, 0);
			if (read(swap, (char *)&argspac, CLBYTES) != CLBYTES)
				goto bad;
		} else
			bzero(&argspac, CLBYTES);
		lseek(swap, (long)argaddr1, 0);
		if (read(swap, &argspac.argc[CLBYTES], CLBYTES) != CLBYTES)
			goto bad;
#endif
#else
		if (swap < 0 || p->p_ssize == 0)
			goto retucomm;
		vstodb(0, CLSIZE, &up->u_smap, &db, 1);
		(void) lseek(swap, (long)dtob(db.db_base), 0);
		if (read(swap, (char *)&argspac.argc[CLBYTES], CLBYTES)
			!= CLBYTES)
			goto bad;
		vstodb(1, CLSIZE, &up->u_smap, &db, 1);
		(void) lseek(swap, (long)dtob(db.db_base), 0);
		if (read(swap, (char *)&argspac.argc[0], CLBYTES) != CLBYTES)
			goto bad;
		file = swapf;
#endif
	} else {
#ifdef i386
		lseek(mem, (long)argaddr1, 0);
		if (read(mem, &argspac.argc[0], ARGSIZE) != ARGSIZE)
			goto bad;
#else
		if (argaddr0) {
			lseek(mem, (long)argaddr0, 0);
			if (read(mem, (char *)&argspac, CLBYTES) != CLBYTES)
				goto bad;
		} else
			bzero(&argspac, CLBYTES);
		lseek(mem, (long)argaddr1, 0);
		if (read(mem, &argspac.argc[CLBYTES], CLBYTES) != CLBYTES)
			goto bad;
#endif
		file = (char *) memf;
	}

	nbad = 0;
#ifdef i386
	ip = &argspac.argi[(ARGSIZE-ARG_MAX)/sizeof (int)];

	for (cp = (char *)ip; cp < &argspac.argc[ARGSIZE-stkoff]; cp++) {
#else
	ip = &argspac.argi[CLBYTES*2/sizeof (int)];
	ip -= 2;                /* last arg word and .long 0 */
	ip -= stkoff / sizeof (int);
	while (*--ip) {
		if (ip == argspac.argi)
			goto retucomm;
	}
	*(char *)ip = ' ';
	ip++;

	for (cp = (char *)ip; cp < &argspac.argc[CLBYTES*2-stkoff]; cp++) {
#endif
		c = *cp;
		if (c == 0) {	/* convert null between arguments to space */
			*cp = ' ';
			if (*(cp+1) == 0) break;	/* if null argument follows then no more args */
			}
		else if (c < ' ' || c > 0176) {
			if (++nbad >= 5*(0+1)) {	/* eflg -> 0 XXX */ /* limit number of bad chars to 5 */
				*cp++ = '?';
				break;
			}
			*cp = '?';
		}
		else if (0 == 0 && c == '=') {		/* eflg -> 0 XXX */
			while (*--cp != ' ')
				if (cp <= (char *)ip)
					break;
			break;
		}
	}
	*cp = 0;
	while (*--cp == ' ')
		*cp = 0;
	cp = (char *)ip;
	(void) strcpy(cmdbuf, cp);
	if (cp[0] == '-' || cp[0] == '?' || cp[0] <= ' ') {
		(void) strcat(cmdbuf, " (");
		(void) strncat(cmdbuf, p->p_comm, sizeof(p->p_comm));
		(void) strcat(cmdbuf, ")");
	}
	return (cmdbuf);

bad:
	seterr("error locating command name for pid %d from %s",
	    p->p_pid, file);
retucomm:
	(void) strcpy(cmdbuf, " (");
	(void) strncat(cmdbuf, p->p_comm, sizeof (p->p_comm));
	(void) strcat(cmdbuf, ")");
	return (cmdbuf);
}


static
getkvars()
{
	if (kvm_nlist(nl) == -1)
		return (-1);
	if (deadkernel) {
		/* We must do the sys map first because klseek uses it */
		long	addr;

#ifndef NEWVM
		Syssize = nl[X_SYSSIZE].n_value;
		Sysmap = (struct pte *)
			calloc((unsigned) Syssize, sizeof (struct pte));
		if (Sysmap == NULL) {
			seterr("out of space for Sysmap");
			return (-1);
		}
		addr = (long) nl[X_SYSMAP].n_value;
		addr &= ~KERNBASE;
		(void) lseek(kmem, addr, 0);
		if (read(kmem, (char *) Sysmap, Syssize * sizeof (struct pte))
		    != Syssize * sizeof (struct pte)) {
			seterr("can't read Sysmap");
			return (-1);
		}
#endif
#if defined(hp300)
		addr = (long) nl[X_LOWRAM].n_value;
		(void) lseek(kmem, addr, 0);
		if (read(kmem, (char *) &lowram, sizeof (lowram))
		    != sizeof (lowram)) {
			seterr("can't read lowram");
			return (-1);
		}
		lowram = btop(lowram);
		Sysseg = (struct ste *) malloc(NBPG);
		if (Sysseg == NULL) {
			seterr("out of space for Sysseg");
			return (-1);
		}
		addr = (long) nl[X_SYSSEG].n_value;
		(void) lseek(kmem, addr, 0);
		read(kmem, (char *)&addr, sizeof(addr));
		(void) lseek(kmem, (long)addr, 0);
		if (read(kmem, (char *) Sysseg, NBPG) != NBPG) {
			seterr("can't read Sysseg");
			return (-1);
		}
#endif
#if defined(i386)
		PTD = (struct pde *) malloc(NBPG);
		if (PTD == NULL) {
			seterr("out of space for PTD");
			return (-1);
		}
		addr = (long) nl[X_IdlePTD].n_value;
		(void) lseek(kmem, addr, 0);
		read(kmem, (char *)&addr, sizeof(addr));
		(void) lseek(kmem, (long)addr, 0);
		if (read(kmem, (char *) PTD, NBPG) != NBPG) {
			seterr("can't read PTD");
			return (-1);
		}
#endif
	}
#ifndef NEWVM
	usrpt = (struct pte *)nl[X_USRPT].n_value;
	Usrptmap = (struct pte *)nl[X_USRPTMAP].n_value;
#endif
	if (kvm_read((void *) nl[X_NSWAP].n_value, &nswap, sizeof (long)) !=
	    sizeof (long)) {
		seterr("can't read nswap");
		return (-1);
	}
	if (kvm_read((void *) nl[X_DMMIN].n_value, &dmmin, sizeof (long)) !=
	    sizeof (long)) {
		seterr("can't read dmmin");
		return (-1);
	}
	if (kvm_read((void *) nl[X_DMMAX].n_value, &dmmax, sizeof (long)) !=
	    sizeof (long)) {
		seterr("can't read dmmax");
		return (-1);
	}
	return (0);
}

kvm_read(loc, buf, len)
	void *loc;
	void *buf;
{
	if (kvmfilesopen == 0 && kvm_openfiles(NULL, NULL, NULL) == -1)
		return (-1);
	if (iskva(loc)) {
		klseek(kmem, (off_t) loc, 0);
		if (read(kmem, buf, len) != len) {
			seterr("error reading kmem at %x", loc);
			return (-1);
		}
	} else {
		lseek(mem, (off_t) loc, 0);
		if (read(mem, buf, len) != len) {
			seterr("error reading mem at %x", loc);
			return (-1);
		}
	}
	return (len);
}

static void
klseek(fd, loc, off)
	int fd;
	off_t loc;
	int off;
{

	if (deadkernel) {
		if ((loc = Vtophys(loc)) == -1)
			return;
	}
	(void) lseek(fd, (off_t)loc, off);
}

#ifndef NEWVM
/*
 * Given a base/size pair in virtual swap area,
 * return a physical base/size pair which is the
 * (largest) initial, physically contiguous block.
 */
static void
vstodb(vsbase, vssize, dmp, dbp, rev)
	register int vsbase;
	int vssize;
	struct dmap *dmp;
	register struct dblock *dbp;
{
	register int blk = dmmin;
	register swblk_t *ip = dmp->dm_map;

	vsbase = ctod(vsbase);
	vssize = ctod(vssize);
	if (vsbase < 0 || vsbase + vssize > dmp->dm_size)
		/*panic("vstodb")*/;
	while (vsbase >= blk) {
		vsbase -= blk;
		if (blk < dmmax)
			blk *= 2;
		ip++;
	}
	if (*ip <= 0 || *ip + blk > nswap)
		/*panic("vstodb")*/;
	dbp->db_size = MIN(vssize, blk - vsbase);
	dbp->db_base = *ip + (rev ? blk - (vsbase + dbp->db_size) : vsbase);
}
#endif

#ifdef NEWVM
static off_t
Vtophys(loc)
	u_long	loc;
{
	off_t newloc = (off_t) -1;
#ifdef hp300
	int p, ste, pte;

	ste = *(int *)&Sysseg[loc >> SG_ISHIFT];
	if ((ste & SG_V) == 0) {
		seterr("vtophys: segment not valid");
		return((off_t) -1);
	}
	p = btop(loc & SG_PMASK);
	newloc = (ste & SG_FRAME) + (p * sizeof(struct pte));
	(void) lseek(kmem, (long)(newloc-(off_t)ptob(lowram)), 0);
	if (read(kmem, (char *)&pte, sizeof pte) != sizeof pte) {
		seterr("vtophys: cannot locate pte");
		return((off_t) -1);
	}
	newloc = pte & PG_FRAME;
	if (pte == PG_NV || newloc < (off_t)ptob(lowram)) {
		seterr("vtophys: page not valid");
		return((off_t) -1);
	}
	newloc = (newloc - (off_t)ptob(lowram)) + (loc & PGOFSET);
#endif
#ifdef i386
	struct pde pde;
	struct pte pte;
	int p;

	pde = PTD[loc >> PD_SHIFT];
	if (pde.pd_v == 0) {
		seterr("vtophys: page directory entry not valid");
		return((off_t) -1);
	}
	p = btop(loc & PT_MASK);
	newloc = pde.pd_pfnum + (p * sizeof(struct pte));
	(void) lseek(kmem, (long)newloc, 0);
	if (read(kmem, (char *)&pte, sizeof pte) != sizeof pte) {
		seterr("vtophys: cannot obtain desired pte");
		return((off_t) -1);
	}
	newloc = pte.pg_pfnum;
	if (pte.pg_v == 0) {
		seterr("vtophys: page table entry not valid");
		return((off_t) -1);
	}
	newloc += (loc & PGOFSET);
#endif
	return((off_t) newloc);
}
#else
static off_t
vtophys(loc)
	long loc;
{
	int p;
	off_t newloc;
	register struct pte *pte;

	newloc = loc & ~KERNBASE;
	p = btop(newloc);
#if defined(vax) || defined(tahoe)
	if ((loc & KERNBASE) == 0) {
		seterr("vtophys: translating non-kernel address");
		return((off_t) -1);
	}
#endif
	if (p >= Syssize) {
		seterr("vtophys: page out of bound (%d>=%d)", p, Syssize);
		return((off_t) -1);
	}
	pte = &Sysmap[p];
	if (pte->pg_v == 0 && (pte->pg_fod || pte->pg_pfnum == 0)) {
		seterr("vtophys: page not valid");
		return((off_t) -1);
	}
#if defined(hp300)
	if (pte->pg_pfnum < lowram) {
		seterr("vtophys: non-RAM page (%d<%d)", pte->pg_pfnum, lowram);
		return((off_t) -1);
	}
#endif
	loc = (long) (ptob(pftoc(pte->pg_pfnum)) + (loc & PGOFSET));
	return(loc);
}
#endif


#ifdef NEWVM
/*
 * locate address of unwired or swapped page
 */

#define DEBUG 0

#define KREAD(off, addr, len) \
	(kvm_read((void *)(off), (char *)(addr), (len)) == (len))


static int
vatosw(p, vaddr, maddr, swb)
struct proc	*p ;
vm_offset_t	vaddr;
vm_offset_t	*maddr;
struct swapblk	*swb;
{
	register struct kinfo_proc *kp = (struct kinfo_proc *)p;
	vm_map_t		mp = &kp->kp_eproc.e_vm.vm_map;
	struct vm_object	vm_object;
	struct vm_map_entry	vm_entry;
	struct pager_struct	pager;
	struct swpager		swpager;
	struct swblock		swblock;
	long			addr, off;
	int			i;

	if (p->p_pid == 0 || p->p_pid == 2)
		return 0;

	addr = (long)mp->header.next;
	for (i = 0; i < mp->nentries; i++) {
		/* Weed through map entries until vaddr in range */
		if (!KREAD(addr, &vm_entry, sizeof(vm_entry))) {
			setsyserr("vatosw: read vm_map_entry");
			return 0;
		}
		if ((vaddr >= vm_entry.start) && (vaddr <= vm_entry.end) &&
				(vm_entry.object.vm_object != 0))
			break;

		addr = (long)vm_entry.next;
	}
	if (i == mp->nentries) {
		seterr("%u: map not found\n", p->p_pid);
		return 0;
	}

	if (vm_entry.is_a_map || vm_entry.is_sub_map) {
		seterr("%u: Is a map\n", p->p_pid);
		return 0;
	}

	/* Locate memory object */
	off = (vaddr - vm_entry.start) + vm_entry.offset;
	addr = (long)vm_entry.object.vm_object;
	while (1) {
		if (!KREAD(addr, &vm_object, sizeof vm_object)) {
			setsyserr("vatosw: read vm_object");
			return 0;
		}

#if DEBUG
		fprintf(stderr, "%u: find page: object %#x offset %x\n",
				p->p_pid, addr, off);
#endif

		/* Lookup in page queue */
		if (findpage(addr, off, maddr))
			return 1;

		if (vm_object.shadow == 0)
			break;

#if DEBUG
		fprintf(stderr, "%u: shadow obj at %x: offset %x+%x\n",
				p->p_pid, addr, off, vm_object.shadow_offset);
#endif

		addr = (long)vm_object.shadow;
		off += vm_object.shadow_offset;
	}

	if (!vm_object.pager) {
		seterr("%u: no pager\n", p->p_pid);
		return 0;
	}

	/* Find address in swap space */
	if (!KREAD(vm_object.pager, &pager, sizeof pager)) {
		setsyserr("vatosw: read pager");
		return 0;
	}
	if (pager.pg_type != PG_SWAP) {
		seterr("%u: weird pager\n", p->p_pid);
		return 0;
	}

	/* Get swap pager data */
	if (!KREAD(pager.pg_data, &swpager, sizeof swpager)) {
		setsyserr("vatosw: read swpager");
		return 0;
	}

	off += vm_object.paging_offset;

	/* Read swap block array */
	if (!KREAD((long)swpager.sw_blocks +
			(off/dbtob(swpager.sw_bsize)) * sizeof swblock,
			&swblock, sizeof swblock)) {
		setsyserr("vatosw: read swblock");
		return 0;
	}
	swb->offset = dbtob(swblock.swb_block)+ (off % dbtob(swpager.sw_bsize));
	swb->size = dbtob(swpager.sw_bsize) - (off % dbtob(swpager.sw_bsize));
	return 1;
}


#define atop(x)		(((unsigned)(x)) >> page_shift)
#define vm_page_hash(object, offset) \
        (((unsigned)object+(unsigned)atop(offset))&vm_page_hash_mask)

static int
findpage(object, offset, maddr)
long			object;
long			offset;
vm_offset_t		*maddr;
{
static	long		vm_page_hash_mask;
static	long		vm_page_buckets;
static	long		page_shift;
	queue_head_t	bucket;
	struct vm_page	mem;
	long		addr, baddr;

	if (vm_page_hash_mask == 0 && !KREAD(nl[X_VM_PAGE_HASH_MASK].n_value,
			&vm_page_hash_mask, sizeof (long))) {
		seterr("can't read vm_page_hash_mask");
		return 0;
	}
	if (page_shift == 0 && !KREAD(nl[X_PAGE_SHIFT].n_value,
			&page_shift, sizeof (long))) {
		seterr("can't read page_shift");
		return 0;
	}
	if (vm_page_buckets == 0 && !KREAD(nl[X_VM_PAGE_BUCKETS].n_value,
			&vm_page_buckets, sizeof (long))) {
		seterr("can't read vm_page_buckets");
		return 0;
	}

	baddr = vm_page_buckets + vm_page_hash(object,offset) * sizeof(queue_head_t);
	if (!KREAD(baddr, &bucket, sizeof (bucket))) {
		seterr("can't read vm_page_bucket");
		return 0;
	}

	addr = (long)bucket.next;
	while (addr != baddr) {
		if (!KREAD(addr, &mem, sizeof (mem))) {
			seterr("can't read vm_page");
			return 0;
		}
		if ((long)mem.object == object && mem.offset == offset) {
			*maddr = (long)mem.phys_addr;
			return 1;
		}
		addr = (long)mem.hashq.next;
	}
	return 0;
}
#endif	/* NEWVM */

#include <varargs.h>
static char errbuf[_POSIX2_LINE_MAX];

static void
seterr(va_alist)
	va_dcl
{
	char *fmt;
	va_list ap;

	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vsnprintf(errbuf, _POSIX2_LINE_MAX, fmt, ap);
#if DEBUG
	(void) vfprintf(stderr, fmt, ap);
#endif
	va_end(ap);
}

static void
setsyserr(va_alist)
	va_dcl
{
	char *fmt, *cp;
	va_list ap;
	extern int errno;

	va_start(ap);
	fmt = va_arg(ap, char *);
	(void) vsnprintf(errbuf, _POSIX2_LINE_MAX, fmt, ap);
	for (cp=errbuf; *cp; cp++)
		;
	snprintf(cp, _POSIX2_LINE_MAX - (cp - errbuf), ": %s", strerror(errno));
	va_end(ap);
}

char *
kvm_geterr()
{
	return (errbuf);
}
