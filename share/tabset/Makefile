#	@(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.6.2.1 1998/03/07 11:46:30 jkh Exp $

FILES=	3101 9837 aa aed512 beehive diablo dtc382 hp700-wy ibm3101 std \
	stdcrt tandem653 teleray vt100 vt100-w wyse-adds xerox1720 xerox1730 \
	xerox1730-lm zenith29

CLEANFILES+=	${FILES}

all: ${FILES}

${FILES}:
	uudecode < ${.CURDIR}/${.TARGET}.uu

depend lint tags:

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/tabset

.include <bsd.prog.mk>
