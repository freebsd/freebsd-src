# $FreeBSD: src/usr.sbin/named.reload/Makefile,v 1.6 1999/08/28 01:17:23 peter Exp $

.include "${.CURDIR}/../named/Makefile.inc"

MAN8=	named.reload.8
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
