/*
 * Copyright (c) 1989, 1990, 1991, 1992 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This procedure implements a minimal program execution facility for
 * 386BSD. It interfaces to the BSD kernel as the execve system call.
 * Significant limitations and lack of compatiblity with POSIX are
 * present with this version, to make its basic operation more clear.
 *
 *	$Id: kern_execve.c,v 1.8 1993/10/25 17:26:01 davidg Exp $
 */

#include "param.h"
#include "systm.h"
#include "signalvar.h"
#include "resourcevar.h"
#include "proc.h"
#include "mount.h"
#include "namei.h"
#include "vnode.h"
#include "file.h"
#include "acct.h"
#include "exec.h"
#include "stat.h"
#include "wait.h"
#include "mman.h"
#include "malloc.h"

#include "vm/vm.h"
#include "vm/vm_param.h"
#include "vm/vm_map.h"
#include "vm/vm_kern.h"

#include "machine/reg.h"

extern int dostacklimits;
#define	copyinoutstr	copyinstr

/*
 * execve() system call.
 */

struct execve_args {
	char	*fname;
	char	**argp;
	char	**envp;
};

/* ARGSUSED */
execve(p, uap, retval)
	struct proc *p;
	register struct execve_args *uap;
	int *retval;
{
	register struct nameidata *ndp;
	struct nameidata nd;
	char **argbuf, **argbufp, *stringbuf, *stringbufp;
	char **vectp, *ep;
	int needsenv, limitonargs, stringlen, addr, size, len,
		rv, amt, argc, tsize, dsize, bsize, cnt, file_offset,
		virtual_offset;
	struct vattr attr;
	struct vmspace *vs;
	caddr_t newframe;
	char shellname[MAXINTERP];			/* 05 Aug 92*/
 	char *shellargs;
	union {
		char	ex_shell[MAXINTERP];	/* #! and interpreter name */
		struct	exec ex_hdr;
	} exdata;
	int indir = 0;

	/*
	 * Step 1. Lookup filename to see if we have something to execute.
	 */
	ndp = &nd;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;

again:							/* 05 Aug 92*/
	ndp->ni_nameiop = LOOKUP | LOCKLEAF | FOLLOW | SAVENAME;

	/* is it there? */
	if (rv = namei(ndp, p))
		return (rv);

	if (ndp->ni_vp->v_writecount) { /* don't exec if file is busy */
		rv = EBUSY;
		goto exec_fail;
	}
	/* does it have any attributes? */
	rv = VOP_GETATTR(ndp->ni_vp, &attr, p->p_ucred, p);
	if (rv)
		goto exec_fail;

	if (ndp->ni_vp->v_mount->mnt_flag & MNT_NOEXEC) { /* no exec on fs ?*/
		rv = EACCES;
		goto exec_fail;
	}

	/* is it executable, and a regular file? */
	if ((ndp->ni_vp->v_mount->mnt_flag & MNT_NOEXEC) ||	/* 29 Jul 92*/
		(VOP_ACCESS(ndp->ni_vp, VEXEC, p->p_ucred, p)) ||
		((attr.va_mode & 0111) == 0) ||
		(attr.va_type != VREG)) {
		rv = EACCES;
		goto exec_fail;
	}

	/*
	 * Step 2. Does the file contain a format we can
	 * understand and execute
	 *
	 * XXX 05 Aug 92
	 * Read in first few bytes of file for segment sizes, magic number:
	 *      ZMAGIC = demand paged RO text
	 * Also an ASCII line beginning with #! is
	 * the file name of a ``shell'' and arguments may be prepended
	 * to the argument list if given here.
	 */
	exdata.ex_shell[0] = '\0';	/* for zero length files */

	rv = vn_rdwr(UIO_READ, ndp->ni_vp, (caddr_t)&exdata, sizeof(exdata),
		0, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred, &amt, p);

	/* big enough to hold a header? */
	if (rv)
		goto exec_fail;

        if (exdata.ex_hdr.a_text != 0 && (ndp->ni_vp->v_flag & VTEXT) == 0 &&
	    ndp->ni_vp->v_writecount != 0) {
		rv = ETXTBSY;
		goto exec_fail;
	}
		
#define SHELLMAGIC	0x2123 /* #! */

	switch (exdata.ex_hdr.a_magic) {
	case ZMAGIC:
		virtual_offset = 0;
		if (exdata.ex_hdr.a_text) {
			file_offset = NBPG;
		} else {
			/* Bill's "screwball mode" */
			file_offset = 0;
		}
		break;
	case QMAGIC:
		virtual_offset = NBPG;
		file_offset = 0;
		break;
	default:
		if ((exdata.ex_hdr.a_magic & 0xffff) != SHELLMAGIC) {
			/* NetBSD compatibility */
			switch (ntohl(exdata.ex_hdr.a_magic) & 0xffff) {
			case ZMAGIC:
			case QMAGIC:
				virtual_offset = NBPG;
				file_offset = 0;
				break;
			default:
				rv = ENOEXEC;
				goto exec_fail;
			}
		} else {
			char *cp, *sp;

			if (indir) {
				rv = ENOEXEC;
				goto exec_fail;
			}
			for (cp = &exdata.ex_shell[2];; ++cp) {
				if (cp >= &exdata.ex_shell[MAXINTERP]) {
					rv = ENOEXEC;
					goto exec_fail;
				}
				if (*cp == '\n') {
					*cp = '\0';
					break;
				}
				if (*cp == '\t')
					*cp = ' ';
			}
			cp = &exdata.ex_shell[2]; /* get shell interpreter name */
			while (*cp == ' ')
				cp++;

			sp = shellname;
			while (*cp && *cp != ' ')
				*sp++ = *cp++;
			*sp = '\0';

 			/* copy the args in the #! line */
 			while (*cp == ' ')
 			  cp++;
 			if (*cp) {
 			    sp++;
 			    shellargs = sp;
 			    while (*cp)
 			      *sp++ = *cp++;
 			    *sp = '\0';
 			} else {
 			    shellargs = 0;
 			}

			indir = 1;              /* indicate this is a script file */
			vput(ndp->ni_vp);
			FREE(ndp->ni_pnbuf, M_NAMEI);

			ndp->ni_dirp = shellname;       /* find shell interpreter */
			ndp->ni_segflg = UIO_SYSSPACE;
			goto again;
		}
			/* NOT REACHED */
	}

	/* sanity check  "ain't not such thing as a sanity clause" -groucho */
	rv = ENOMEM;
	if (/*exdata.ex_hdr.a_text == 0 || */ exdata.ex_hdr.a_text > MAXTSIZ ||
	    exdata.ex_hdr.a_text % NBPG || exdata.ex_hdr.a_text > attr.va_size)
		goto exec_fail;

	if (exdata.ex_hdr.a_data == 0 || exdata.ex_hdr.a_data > DFLDSIZ
		|| exdata.ex_hdr.a_data > attr.va_size
		|| exdata.ex_hdr.a_data + exdata.ex_hdr.a_text > attr.va_size)
		goto exec_fail;

	if (exdata.ex_hdr.a_bss > MAXDSIZ)
		goto exec_fail;
	
	if (exdata.ex_hdr.a_text + exdata.ex_hdr.a_data + exdata.ex_hdr.a_bss > MAXTSIZ + MAXDSIZ)
		goto exec_fail;

	if (exdata.ex_hdr.a_data + exdata.ex_hdr.a_bss > p->p_rlimit[RLIMIT_DATA].rlim_cur)
		goto exec_fail;

	if (exdata.ex_hdr.a_entry > exdata.ex_hdr.a_text + exdata.ex_hdr.a_data)
		goto exec_fail;
	
	/*
	 * Step 3.  File and header are valid. Now, dig out the strings
	 * out of the old process image.
	 */

	/*
	 * We implement a single-pass algorithm that builds a new stack
	 * frame within the address space of the "old" process image,
	 * avoiding the second pass entirely. Thus, the new frame is
	 * in position to be run. This consumes much virtual address space,
	 * and two pages more of 'real' memory, such are the costs.
	 * [Also, note the cache wipe that's avoided!]
	 */

	/* create anonymous memory region for new stack */
	vs = p->p_vmspace;
	if ((unsigned)vs->vm_maxsaddr + MAXSSIZ < USRSTACK)
		newframe = (caddr_t) USRSTACK - MAXSSIZ;
	else
		vs->vm_maxsaddr = newframe = (caddr_t) USRSTACK - 2*MAXSSIZ;

	/* don't do stack limit checking on traps temporarily XXX*/
	dostacklimits = 0;

	rv = vm_allocate(&vs->vm_map, &newframe, MAXSSIZ, FALSE);
	if (rv) goto exec_fail;

	/* allocate string buffer and arg buffer */
	argbuf = (char **) (newframe + MAXSSIZ - 3*ARG_MAX);
	stringbuf = stringbufp = ((char *)argbuf) + 2*ARG_MAX;
	argbufp = argbuf;

	/* first, do args */
	vectp = uap->argp;
	needsenv = 1;
	limitonargs = ARG_MAX;
	cnt = 0;

	/* first, do (shell name if any then) args */
	if (indir)  {
		ep = shellname;
thrice:
		if (ep) {
			/* did we outgrow initial argbuf, if so, die */
			if (argbufp >= (char **)stringbuf) {
				rv = E2BIG;
				goto exec_dealloc;
			}

			if (rv = copyoutstr(ep, stringbufp,
				(u_int)limitonargs, (u_int *)&stringlen)) {
				if (rv == ENAMETOOLONG)
					rv = E2BIG;
				goto exec_dealloc;
			}
			suword(argbufp++, (int)stringbufp);
			cnt++;
			stringbufp += stringlen;
			limitonargs -= stringlen;
		}

		if (shellargs) {
		    ep = shellargs;
		    shellargs = 0;
		    goto thrice;
		}

		if (indir) {
			indir = 0;
			/* orginal executable is 1st argument with scripts */
			ep = uap->fname;
			goto thrice;
		}
		/* terminate in case no more args to script */
		suword(argbufp, 0);
		if (vectp = uap->argp) vectp++; /* manually doing the first
						   argument with scripts */
	}

do_env_as_well:
	if(vectp == 0) goto dont_bother;

	/* for each envp, copy in string */
	do {
		/* did we outgrow initial argbuf, if so, die */
		if (argbufp == (char **)stringbuf) {
			rv = E2BIG;
			goto exec_dealloc;
		}
	
		/* get an string pointer */
		ep = (char *)fuword(vectp++);
		if (ep == (char *)-1) {
			rv = EFAULT;
			goto exec_dealloc;
		}

		/* if not a null pointer, copy string */
		if (ep) {
			if (rv = copyinoutstr(ep, stringbufp,
				(u_int)limitonargs, (u_int *) &stringlen)) {
				if (rv == ENAMETOOLONG)
					rv = E2BIG;
				goto exec_dealloc;
			}
			suword(argbufp++, (int)stringbufp);
			cnt++;
			stringbufp += stringlen;
			limitonargs -= stringlen;
		} else {
			suword(argbufp++, 0);
			break;
		}
	} while (limitonargs > 0);

dont_bother:
	if (limitonargs <= 0) {
		rv = E2BIG;
		goto exec_dealloc;
	}

	/* have we done the environment yet ? */
	if (needsenv) {
		/* remember the arg count for later */
		argc = cnt;
		vectp = uap->envp;
		needsenv = 0;
		goto do_env_as_well;
	}
 
	/* At this point, one could optionally implement a
	 * second pass to condense the strings, arguement vectors,
	 * and stack to fit the fewest pages.
	 *
	 * One might selectively do this when copying was cheaper
	 * than leaving allocated two more pages per process.
	 */

	/* stuff arg count on top of "new" stack */
	/* argbuf[-1] = (char *)argc;*/
	suword(argbuf-1,argc);

	/*
	 * Step 4. Build the new processes image.
	 *
	 * At this point, we are committed -- destroy old executable!
	 */

	/* blow away all address space, except the stack */
	rv = vm_deallocate(&vs->vm_map, 0, USRSTACK - 2*MAXSSIZ);
	if (rv)
		goto exec_abort;

	/* destroy "old" stack */
	if ((unsigned)newframe < USRSTACK - MAXSSIZ) {
		rv = vm_deallocate(&vs->vm_map, USRSTACK - MAXSSIZ, MAXSSIZ);
		if (rv)
			goto exec_abort;
	} else {
		rv = vm_deallocate(&vs->vm_map, USRSTACK - 2*MAXSSIZ, MAXSSIZ);
		if (rv)
			goto exec_abort;
	}

	/* build a new address space */



	/* treat text, data, and bss in terms of integral page size */
	tsize = roundup(exdata.ex_hdr.a_text, NBPG);
	dsize = roundup(exdata.ex_hdr.a_data, NBPG);
	bsize = roundup(exdata.ex_hdr.a_bss, NBPG);

	addr = virtual_offset;

	/* map text as being read/execute only and demand paged */
	rv = vm_mmap(&vs->vm_map, &addr, tsize, VM_PROT_READ|VM_PROT_EXECUTE,
		VM_PROT_DEFAULT, MAP_FILE|MAP_PRIVATE|MAP_FIXED,
		(caddr_t)ndp->ni_vp, file_offset);
	if (rv)
		goto exec_abort;

	addr = virtual_offset + tsize;

	/* map data as being read/write and demand paged */
	rv = vm_mmap(&vs->vm_map, &addr, dsize,
		VM_PROT_READ | VM_PROT_WRITE | (tsize ? 0 : VM_PROT_EXECUTE),
		VM_PROT_DEFAULT, MAP_FILE|MAP_PRIVATE|MAP_FIXED,
		(caddr_t)ndp->ni_vp, file_offset + tsize);
	if (rv)
		goto exec_abort;

	/* create anonymous memory region for bss */
	addr = virtual_offset + tsize + dsize;
	rv = vm_allocate(&vs->vm_map, &addr, bsize, FALSE);
	if (rv)
		goto exec_abort;

	/*
	 * Step 5. Prepare process for execution.
	 */

	/* touchup process information -- vm system is unfinished! */
	vs->vm_tsize = tsize/NBPG;		/* text size (pages) XXX */
	vs->vm_dsize = (dsize+bsize)/NBPG;	/* data size (pages) XXX */
	vs->vm_taddr = (caddr_t) virtual_offset; /* virtual address of text */
	vs->vm_daddr = (caddr_t) virtual_offset + tsize; /* virtual address of data */
	vs->vm_maxsaddr = newframe;	/* user VA at max stack growth XXX */
	vs->vm_ssize =  ((unsigned)vs->vm_maxsaddr + MAXSSIZ
		- (unsigned)argbuf)/ NBPG + 1; /* stack size (pages) */
	dostacklimits = 1;	/* allow stack limits to be enforced XXX */

	/* close files on exec, fixup signals */
	fdcloseexec(p);
	execsigs(p);

	/* name this process - nameiexec(p, ndp) */
	len = MIN(ndp->ni_namelen,MAXCOMLEN);
	bcopy(ndp->ni_ptr, p->p_comm, len);
	p->p_comm[len] = 0;
	
	/* mark as executable, wakeup any process that was vforked and tell
	 * it that it now has it's own resources back */
	p->p_flag |= SEXEC;
	if (p->p_pptr && (p->p_flag & SPPWAIT)) {
	    p->p_flag &= ~SPPWAIT;
	    wakeup(p->p_pptr);
	}
	
	/* implement set userid/groupid */
	if ((attr.va_mode&VSUID) && (p->p_flag & STRC) == 0) {
	    p->p_ucred = crcopy(p->p_ucred);
	    p->p_cred->p_svuid = p->p_ucred->cr_uid = attr.va_uid;
	}
	if ((attr.va_mode&VSGID) && (p->p_flag & STRC) == 0) {
	    p->p_ucred = crcopy(p->p_ucred);
	    p->p_cred->p_svgid = p->p_ucred->cr_groups[0] = attr.va_gid;
	}

	/* setup initial register state */
	p->p_regs[SP] = (unsigned) (argbuf - 1);
	setregs(p, exdata.ex_hdr.a_entry);

 	ndp->ni_vp->v_flag |= VTEXT;		/* mark vnode pure text */

	vput(ndp->ni_vp);
	FREE(ndp->ni_pnbuf, M_NAMEI);

	/* if tracing process, pass control back to debugger so breakpoints
	   can be set before the program "runs" */
	if (p->p_flag & STRC)
		psignal(p, SIGTRAP);
	p->p_acflag &= ~AFORK;		/* remove fork, but no exec flag */

	return (0);

exec_dealloc:
	/* remove interim "new" stack frame we were building */
	vm_deallocate(&vs->vm_map, newframe, MAXSSIZ);

exec_fail:
	dostacklimits = 1;
	vput(ndp->ni_vp);
	FREE(ndp->ni_pnbuf, M_NAMEI);

	return(rv);

exec_abort:
	/* sorry, no more process anymore. exit gracefully */
	vm_deallocate(&vs->vm_map, newframe, MAXSSIZ);
	vput(ndp->ni_vp);
	FREE(ndp->ni_pnbuf, M_NAMEI);
	kexit(p, W_EXITCODE(0, SIGABRT));

	/* NOTREACHED */
	return(0);
}
