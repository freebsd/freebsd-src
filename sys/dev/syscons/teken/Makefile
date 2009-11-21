# $FreeBSD: src/sys/dev/syscons/teken/Makefile,v 1.1.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=		teken_demo
SRCS=		teken_demo.c teken.c teken_state.h
CLEANFILES=	teken_state.h teken.log
LDADD=		-lncurses -lutil
NO_MAN=
WARNS?=		6

teken_state.h: gensequences sequences
	awk -f gensequences sequences > ${.TARGET}

.include <bsd.prog.mk>
