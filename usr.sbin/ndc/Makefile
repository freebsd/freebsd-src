# $Id: Makefile,v 1.1 1996/08/29 21:46:45 peter Exp $

.include "${.CURDIR}/../named/Makefile.inc"

.PATH:	${BIND_DIR}/named
.PATH:	${BIND_DIR}/man

MAN8=	ndc.8
CLEANFILES+= ndc

all:	ndc

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ndc \
		${DESTDIR}${BINDIR}

ndc:	ndc.sh ndcedit.awk ${BIND_DIR}/Makefile
	sed -e "s|%PIDDIR%|${PIDDIR}|" \
	    -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%PS%|${PS}|" \
	    -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    -e "s|%IOT%|${IOT}|" \
	    < ${BIND_DIR}/named/ndc.sh | awk -f ${.CURDIR}/ndcedit.awk > ndc

.include <bsd.prog.mk>
