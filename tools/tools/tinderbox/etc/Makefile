# $FreeBSD$

ETCFILES	 = default.rc releng_4.rc current.rc experimental.rc
ETCFILES	+= ada.rc dsa.rc dwp.rc stanley.rc
ETCDIR		?= ${HOME}/etc
ETCOWN		?= ${USER}
ETCGRP		?= ${USER}

realinstall:
.for FILE in ${ETCFILES}
	${INSTALL} -m0644 -o${ETCOWN} -g${ETCGRP} ${.CURDIR}/${FILE} ${ETCDIR}
.endfor

.include <bsd.prog.mk>
