# $FreeBSD$

MAINTAINER=	billf@FreeBSD.org
MAINTAINER+=	doug@gorean.org
MAN8=		mergemaster.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mergemaster.sh ${DESTDIR}${BINDIR}/mergemaster

.include <bsd.prog.mk>

