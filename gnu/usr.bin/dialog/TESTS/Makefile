# $FreeBSD: src/gnu/usr.bin/dialog/TESTS/Makefile,v 1.2.4.1 2000/04/24 11:25:09 phantom Exp $

FILES=	README checklist ftreebox infobox inputbox menubox msgbox \
	prgbox radiolist textbox treebox yesno

beforeinstall:
.for file in ${FILES}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/${file} \
		${DESTDIR}/usr/share/examples/dialog
.endfor

.include <bsd.prog.mk>
