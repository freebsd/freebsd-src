# $FreeBSD: src/tools/tools/tinderbox/etc/Makefile,v 1.2 2003/10/10 07:10:33 des Exp $

ETCFILES	 = default.rc releng_4.rc releng_5.rc current.rc
ETCFILES	+= ada.rc dsa.rc dwp.rc stanley.rc
ETCDIR		?= ${HOME}/etc
ETCOWN		?= ${USER}
ETCGRP		?= ${USER}

realinstall:
.for FILE in ${ETCFILES}
	${INSTALL} -m0644 -o${ETCOWN} -g${ETCGRP} ${.CURDIR}/${FILE} ${ETCDIR}
.endfor

.include <bsd.prog.mk>
