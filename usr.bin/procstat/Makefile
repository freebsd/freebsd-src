# $FreeBSD$

.include <src.opts.mk>

PROG=	procstat
MAN=	procstat.1
SRCS=	procstat.c		\
	procstat_args.c		\
	procstat_auxv.c		\
	procstat_basic.c	\
	procstat_bin.c		\
	procstat_cred.c		\
	procstat_cs.c		\
	procstat_files.c	\
	procstat_kstack.c	\
	procstat_penv.c		\
	procstat_ptlwpinfo.c	\
	procstat_pwdx.c		\
	procstat_rlimit.c	\
	procstat_rusage.c	\
	procstat_sigs.c		\
	procstat_threads.c	\
	procstat_vm.c

MLINKS+=	procstat.1 pargs.1
MLINKS+=	procstat.1 penv.1
MLINKS+=	procstat.1 pwdx.1

LIBADD+=	procstat xo util sbuf

LINKS+=		${BINDIR}/procstat ${BINDIR}/pargs
LINKS+=		${BINDIR}/procstat ${BINDIR}/penv
LINKS+=		${BINDIR}/procstat ${BINDIR}/pwdx

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
