# $FreeBSD$

FILES=	Termcap Terminfo pcvt.el xmodmap-german pcvt.sh

NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/Etc

afterdistribute: beforeinstall

.include <bsd.prog.mk>
