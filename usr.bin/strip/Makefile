#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/strip/Makefile,v 1.15 2002/07/29 09:40:16 ru Exp $

PROG=	strip
MAN=	strip.1aout
BINDIR= /usr/libexec/aout
CLEANFILES += maybe_stripped

all: maybe_stripped

maybe_stripped: strip
	cp -p strip maybe_stripped
.if defined(STRIP)
.if ${STRIP:M-s} != ""
	-strip maybe_stripped
.endif
.endif

realinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    maybe_stripped ${DESTDIR}${BINDIR}/strip

.include <bsd.prog.mk>
