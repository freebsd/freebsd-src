/* $FreeBSD: src/sys/alpha/osf1/osf1_signal.h,v 1.1 1999/12/14 22:35:33 gallatin Exp $ */

#ifndef _OSF1_SIGNAL_H
#define	_OSF1_SIGNAL_H

#define	OSF1_SIGHUP	 1
#define	OSF1_SIGINT	 2
#define	OSF1_SIGQUIT	 3
#define	OSF1_SIGILL	 4
#define	OSF1_SIGTRAP	 5
#define	OSF1_SIGABRT	 6
#define	OSF1_SIGEMT	 7
#define	OSF1_SIGFPE	 8
#define	OSF1_SIGKILL	 9
#define	OSF1_SIGBUS	10
#define	OSF1_SIGSEGV	11
#define	OSF1_SIGSYS	12
#define	OSF1_SIGPIPE	13
#define	OSF1_SIGALRM	14
#define	OSF1_SIGTERM	15
#define	OSF1_SIGURG	16
#define	OSF1_SIGSTOP	17
#define	OSF1_SIGTSTP	18
#define	OSF1_SIGCONT	19
#define	OSF1_SIGCHLD	20
#define	OSF1_SIGTTIN	21
#define	OSF1_SIGTTOU	22
#define	OSF1_SIGIO	23
#define	OSF1_SIGXCPU	24
#define	OSF1_SIGXFSZ	25
#define	OSF1_SIGVTALRM	26
#define	OSF1_SIGPROF	27
#define	OSF1_SIGWINCH	28
#define	OSF1_SIGINFO	29
#define	OSF1_SIGUSR1	30
#define	OSF1_SIGUSR2	31
#define	OSF1_NSIG	32
#define	OSF1_SIGTBLSZ	32

#define	OSF1_SIG_DFL		0
#define	OSF1_SIG_ERR		-1
#define	OSF1_SIG_IGN		1
#define	OSF1_SIG_HOLD		2

#define	OSF1_SIGNO(a)	((a) & OSF1_SIGNO_MASK)
#define	OSF1_SIGCALL(a)	((a) & ~OSF1_SIGNO_MASK)

#define	OSF1_SIG_BLOCK		1
#define	OSF1_SIG_UNBLOCK	2
#define	OSF1_SIG_SETMASK	3


#define	OSF1_BSD2OSF1_SIG(sig)  \
	(((sig) <= OSF1_SIGTBLSZ) ? bsd_to_osf1_sig[_SIG_IDX(sig)] : sig)
#define	OSF1_OSF12BSD_SIG(sig)  \
	(((sig) <= OSF1_SIGTBLSZ) ? osf1_to_bsd_sig[_SIG_IDX(sig)] : sig)


typedef u_long	osf1_sigset_t;
typedef void	(*osf1_handler_t) __P((int));

struct osf1_sigaction {
	osf1_handler_t	osa_handler;
	osf1_sigset_t	osa_mask;
	int		osa_flags;
};

struct osf1_sigaltstack {
	caddr_t		ss_sp;
	int		ss_flags;
	size_t		ss_size;
};

/* sa_flags */
#define	OSF1_SA_ONSTACK		0x00000001
#define	OSF1_SA_RESTART		0x00000002
#define	OSF1_SA_NOCLDSTOP	0x00000004
#define	OSF1_SA_NODEFER		0x00000008
#define	OSF1_SA_RESETHAND	0x00000010
#define	OSF1_SA_NOCLDWAIT	0x00000020
#define	OSF1_SA_SIGINFO		0x00000040

/* ss_flags */
#define	OSF1_SS_ONSTACK		0x00000001
#define	OSF1_SS_DISABLE		0x00000002


#define	OSF1_SIGNO_MASK		0x00FF
#define	OSF1_SIGNAL_MASK	0x0000
#define	OSF1_SIGDEFER_MASK	0x0100
#define	OSF1_SIGHOLD_MASK	0x0200
#define	OSF1_SIGRELSE_MASK	0x0400
#define	OSF1_SIGIGNORE_MASK	0x0800
#define	OSF1_SIGPAUSE_MASK	0x1000


extern int osf1_to_linux_sig[];
void bsd_to_osf1_sigaltstack __P((const struct sigaltstack *, struct osf1_sigaltstack *));
void bsd_to_osf1_sigset __P((const sigset_t *, osf1_sigset_t *));
void osf1_to_bsd_sigaltstack __P((const struct osf1_sigaltstack *, struct sigaltstack *));
void osf1_to_bsd_sigset __P((const osf1_sigset_t *, sigset_t *));
void osf1_sendsig __P((sig_t, int , sigset_t *, u_long ));


#endif /* !_OSF1_SIGNAL_H */
