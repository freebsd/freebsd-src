# $FreeBSD$

SCRIPTS=	grog
MAN=		grog.1
CLEANFILES=	${SCRIPTS} ${MAN}

grog: grog.sh
	sed -e 's/@g@/$(g)/g' ${.ALLSRC} > ${.TARGET}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
