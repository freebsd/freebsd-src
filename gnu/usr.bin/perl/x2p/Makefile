#	$Id: Makefile,v 1.13 1998/05/05 05:25:21 bde Exp $

PROG= a2p

SRCS= a2p.y hash.c str.c walk.c util.c
CFLAGS+= -I${.CURDIR}/../perl
YFLAGS=

LDADD= -lm
DPADD= ${LIBM}

# If perl exists in none of these places, something is horribly wrong.
.if exists(${.OBJDIR}/../perl/perl)
PERL=${.OBJDIR}/../perl/perl
.elif !defined(PERL) && exists(${.CURDIR}/../perl/perl)
PERL=${.CURDIR}/../perl/perl
.else
PERL=	${DESTDIR}/usr/bin/perl
.endif

MAN1+= a2p.1 s2p.1 h2ph.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/s2p \
		${DESTDIR}${BINDIR}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 ${.CURDIR}/h2ph \
		${DESTDIR}${BINDIR}

afterinstall:
	(DESTDIR=${DESTDIR}; cd ${DESTDIR}/usr/include; ${PERL} ${DESTDIR}${BINDIR}/h2ph * machine/* sys/*)

.include "../../Makefile.inc"
.include <bsd.prog.mk>
