# $FreeBSD$

ETCFILES	 = default.rc releng_4.rc releng_5.rc stable.rc current.rc
ETCFILES	+= update_stable.rc update_current.rc
ETCDIR		?= ${HOME}/etc
ETCOWN		?= ${USER}
ETCGRP		?= ${USER}

realinstall:
.for FILE in ${ETCFILES}
	${INSTALL} -m0644 -o${ETCOWN} -g${ETCGRP} ${.CURDIR}/${FILE} ${ETCDIR}
.endfor

.include <bsd.prog.mk>
