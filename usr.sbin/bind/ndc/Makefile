# $Id$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${BIND_DIR}/named
.PATH:	${BIND_DIR}/man

BINDIR= ${DESTSBIN}
MAN8=	ndc.8
CLEANFILES+= ndc

all:	ndc

realinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ndc \
		${DESTDIR}${BINDIR}

ndc:	ndc.sh awkedit ${BIND_DIR}/Makefile
	sed -e "s|%PIDDIR%|${PIDDIR}|" \
	    -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%PS%|${PS}|" \
	    -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    -e "s|%IOT%|${IOT}|" \
	    < ${BIND_DIR}/named/ndc.sh | awk -f ${.CURDIR}/awkedit > ndc

.include <bsd.prog.mk>
