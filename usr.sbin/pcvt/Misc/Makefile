SUBDIR=	Doc Etc

FILES=	README.FIRST

beforeinstall:
	for file in ${FILES}; \
	do \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/$$file ${DESTDIR}${BINDIR}/$$file ; \
	done

afterdistribute: beforeinstall

.include "Makefile.inc"
.include <bsd.subdir.mk>
