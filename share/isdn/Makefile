#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/isdn/Makefile,v 1.1.2.2 1999/11/16 17:21:32 joe Exp $

FILES=	0.al 1.al 2.al 3.al 4.al 5.al 6.al 7.al 8.al 9.al beep.al msg.al

CLEANFILES+=	${FILES}

all: ${FILES}

${FILES}:
	uudecode < ${.CURDIR}/${.TARGET}.uu

depend lint tags:

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/isdn

.include <bsd.prog.mk>
