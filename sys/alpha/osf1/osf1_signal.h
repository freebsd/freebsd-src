/* $FreeBSD$ */

#ifndef _OSF1_SIGNAL_H
#define	_OSF1_SIGNAL_H

#define	OSF1_NSIG	64

#define	OSF1_SIG_DFL		0
#define	OSF1_SIG_ERR		-1
#define	OSF1_SIG_IGN		1
#define	OSF1_SIG_HOLD		2

#define	OSF1_SIGNO(a)	((a) & OSF1_SIGNO_MASK)
#define	OSF1_SIGCALL(a)	((a) & ~OSF1_SIGNO_MASK)

#define	OSF1_SIG_BLOCK		1
#define	OSF1_SIG_UNBLOCK	2
#define	OSF1_SIG_SETMASK	3


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
