/*-
 * Copyright (c) 1999 Marcel Moolenaar
 * Copyright (c) 2003 Peter Wemm
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

struct ia32_sigaltstack {
	u_int32_t	ss_sp;		/* signal stack base */
	u_int32_t	ss_size;	/* signal stack length */
	int		ss_flags;	/* SS_DISABLE and/or SS_ONSTACK */
};

struct ia32_mcontext {
	int	mc_onstack;		/* XXX - sigcontext compat. */
	int	mc_gs;			/* machine state (struct trapframe) */
	int	mc_fs;
	int	mc_es;
	int	mc_ds;
	int	mc_edi;
	int	mc_esi;
	int	mc_ebp;
	int	mc_isp;
	int	mc_ebx;
	int	mc_edx;
	int	mc_ecx;
	int	mc_eax;
	int	mc_trapno;
	int	mc_err;
	int	mc_eip;
	int	mc_cs;
	int	mc_eflags;
	int	mc_esp;
	int	mc_ss;
	int	mc_len;			/* sizeof(struct ia32_mcontext) */
	/* We use the same values for fpformat and ownedfp */
	int	mc_fpformat;
	int	mc_ownedfp;
	int	mc_spare1[1];		/* align next field to 16 bytes */
	/*
	 * See <i386/include/npx.h> for the internals of mc_fpstate[].
	 */
	int	mc_fpstate[128] __aligned(16);
	int	mc_spare2[8];
};

struct ia32_ucontext {
	sigset_t		uc_sigmask;
	struct ia32_mcontext	uc_mcontext;
	u_int32_t		uc_link;
	struct ia32_sigaltstack	uc_stack;
	int			uc_flags;
	int			__spare__[4];
};


#if defined(COMPAT_FREEBSD4)
struct ia32_mcontext4 {
	int	mc_onstack;		/* XXX - sigcontext compat. */
	int	mc_gs;			/* machine state (struct trapframe) */
	int	mc_fs;
	int	mc_es;
	int	mc_ds;
	int	mc_edi;
	int	mc_esi;
	int	mc_ebp;
	int	mc_isp;
	int	mc_ebx;
	int	mc_edx;
	int	mc_ecx;
	int	mc_eax;
	int	mc_trapno;
	int	mc_err;
	int	mc_eip;
	int	mc_cs;
	int	mc_eflags;
	int	mc_esp;	
	int	mc_ss;
	int	mc_fpregs[28];
	int	__spare__[17];
};

struct ia32_ucontext4 {
	sigset_t		uc_sigmask;
	struct ia32_mcontext4	uc_mcontext;
	u_int32_t		uc_link;
	struct ia32_sigaltstack	uc_stack;
	int			__spare__[8];
};
#endif

/*
 * Signal frames, arguments passed to application signal handlers.
 */
union ia32_sigval {
	int			sigval_int;
	u_int32_t		sigval_ptr;
};
struct ia32_siginfo {
	int			si_signo;	/* signal number */
	int			si_errno;	/* errno association */
	int			si_code;	/* signal code */
	int32_t			si_pid;		/* sending process */
	u_int32_t		si_uid;		/* sender's ruid */
	int			si_status;	/* exit value */
	u_int32_t		si_addr;	/* faulting instruction */
	union ia32_sigval	si_value;	/* signal value */
	int32_t			si_band;	/* band event for SIGPOLL */
	int			__spare__[7];	/* gimme some slack */
};

#ifdef COMPAT_FREEBSD4
struct ia32_sigframe4 {
	u_int32_t		sf_signum;
	u_int32_t		sf_siginfo;	/* code or pointer to sf_si */
	u_int32_t		sf_ucontext;	/* points to sf_uc */
	u_int32_t		sf_addr;	/* undocumented 4th arg */
	u_int32_t		sf_ah;		/* action/handler pointer */
	struct ia32_ucontext4	sf_uc;		/* = *sf_ucontext */
	struct ia32_siginfo	sf_si;		/* = *sf_siginfo (SA_SIGINFO case) */
};
#endif

struct ia32_sigframe {
	u_int32_t		sf_signum;
	u_int32_t		sf_siginfo;	/* code or pointer to sf_si */
	u_int32_t		sf_ucontext;	/* points to sf_uc */
	u_int32_t		sf_addr;	/* undocumented 4th arg */
	u_int32_t		sf_ah;		/* action/handler pointer */
	struct ia32_ucontext	sf_uc;		/* = *sf_ucontext */
	struct ia32_siginfo	sf_si;		/* = *sf_siginfo (SA_SIGINFO case) */
};

extern char ia32_sigcode[];
extern char freebsd4_ia32_sigcode[];
extern int sz_ia32_sigcode;
extern int sz_freebsd4_ia32_sigcode;
extern void ia32_sendsig(sig_t, int, sigset_t *, u_long);
extern void ia32_setregs(struct thread *td, u_long entry, u_long stack,
    u_long ps_strings);
