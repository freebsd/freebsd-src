# $FreeBSD$

FILES=	README checklist ftreebox infobox inputbox menubox msgbox \
	prgbox radiolist textbox treebox yesno

beforeinstall:
.for file in ${FILES}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/${file} \
		${DESTDIR}/usr/share/examples/dialog
.endfor

.include <bsd.prog.mk>
