#	@(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.8 1998/01/03 14:13:46 wosch Exp $

FILES=	0.g711a 1.g711a 2.g711a 3.g711a 4.g711a 5.g711a 6.g711a 7.g711a \
	8.g711a 9.g711a beep.g711a msg.g711a

CLEANFILES+=	${FILES}

all: ${FILES}

${FILES}:
	uudecode < ${.CURDIR}/${.TARGET}.uu

depend lint tags:

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/isdn

.include <bsd.prog.mk>
