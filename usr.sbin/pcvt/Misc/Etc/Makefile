# $Id: Makefile,v 1.4 1999/01/01 08:31:54 peter Exp $

FILES=	Termcap Terminfo pcvt.el rc.local uemacs.tar.Z.uu xmodmap-german

NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/Etc

afterdistribute: beforeinstall

.include <bsd.prog.mk>
