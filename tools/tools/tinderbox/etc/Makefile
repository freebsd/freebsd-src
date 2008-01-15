# $FreeBSD: src/tools/tools/tinderbox/etc/Makefile,v 1.9 2004/08/19 07:28:30 des Exp $

ETCFILES	 =
ETCFILES	+= default.rc
ETCFILES	+= releng_4.rc update_releng_4.rc
ETCFILES	+= releng_5.rc update_releng_5.rc
ETCFILES	+= stable.rc update_stable.rc
ETCFILES	+= current.rc update_current.rc
ETCDIR		?= ${HOME}/etc
ETCOWN		?= ${USER}
ETCGRP		?= ${USER}

realinstall:
.for FILE in ${ETCFILES}
	${INSTALL} -m0644 -o${ETCOWN} -g${ETCGRP} ${.CURDIR}/${FILE} ${ETCDIR}
.endfor

.include <bsd.prog.mk>
