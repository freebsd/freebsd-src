#
# $FreeBSD: src/gnu/usr.bin/perl/x2p/a2pl/Makefile,v 1.11.2.1 2000/10/05 09:54:57 markm Exp $
#

PROG=	a2p
CFLAGS+=-I${PERL5SRC}/x2p -I${.OBJDIR} -I${.OBJDIR}/temp
LDFLAGS+=-lperl -lm -lcrypt
SRCS=	a2p.c hash.c str.c util.c walk.c config.h

CLEANDIRS+=	temp
CLEANFILES+=	${PROG}.1

config.h: temp

temp:
	@mkdir temp

.include <bsd.prog.mk>

.PATH: ${PERL5SRC}/x2p
.PATH: ${PERL5SRC}

.SUFFIXES:
.SUFFIXES: .c .h .o

${PROG}.1: ${PROG}.pod
	${MINIPERL} ${MINIPERLOPT} ${.OBJDIR}/../../pod/pod2man/pod2man ${.OODATE} > ${.TARGET}
