# $FreeBSD: src/usr.sbin/pcvt/Misc/Etc/Makefile,v 1.4.2.1 1999/08/29 15:44:53 peter Exp $

FILES=	Termcap Terminfo pcvt.el rc.local uemacs.tar.Z.uu xmodmap-german

NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/Etc

afterdistribute: beforeinstall

.include <bsd.prog.mk>
