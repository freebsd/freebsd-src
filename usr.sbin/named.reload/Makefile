# $Id: Makefile,v 1.1 1996/08/29 21:47:33 peter Exp $

.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/named
.PATH:	${BIND_DIR}/man

MAN8=	named.reload.8
CLEANFILES+= named.reload

all:	named.reload

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    named.reload ${DESTDIR}${BINDIR}

named.reload:	named.reload.sh ${BIND_DIR}/Makefile
	sed -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    < ${BIND_DIR}/named/named.reload.sh > named.reload

.include <bsd.prog.mk>
