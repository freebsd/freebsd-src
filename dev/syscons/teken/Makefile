# $FreeBSD$

PROG=		teken_demo
SRCS=		teken_demo.c teken.c teken_state.h
CLEANFILES=	teken_state.h teken.log
LDADD=		-lncurses -lutil
NO_MAN=
WARNS?=		6

teken_state.h: gensequences sequences
	awk -f gensequences sequences > ${.TARGET}

.include <bsd.prog.mk>
