# $FreeBSD$

.include "${.CURDIR}/../named/Makefile.inc"

MAN=	named.reload.8
CLEANFILES+= named.reload

all:	named.reload

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    named.reload ${DESTDIR}${BINDIR}

named.reload:	named.reload.sh ${BIND_DIR}/Makefile
	sed -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    < ${.CURDIR}/named.reload.sh > named.reload

.include <bsd.prog.mk>
