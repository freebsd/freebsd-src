# $Id: Makefile,v 1.1.2.1 1997/06/28 07:41:56 peter Exp $

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
