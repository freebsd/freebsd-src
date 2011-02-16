# $FreeBSD: src/usr.bin/procstat/Makefile,v 1.2.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	procstat
MAN=	procstat.1
SRCS=	procstat.c		\
	procstat_args.c		\
	procstat_basic.c	\
	procstat_bin.c		\
	procstat_cred.c		\
	procstat_files.c	\
	procstat_kstack.c	\
	procstat_sigs.c		\
	procstat_threads.c	\
	procstat_vm.c

LDADD+=	-lutil
DPADD+=	${LIBUTIL}
WARNS?=	4

.include <bsd.prog.mk>
