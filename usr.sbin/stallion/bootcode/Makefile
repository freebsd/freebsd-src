# $Id: Makefile,v 1.2 1996/05/05 07:40:46 peter Exp $

.SUFFIXES: .uu
.uu:
	rm -f ${.TARGET}
	uudecode ${.IMPSRC}

FILES=	2681.sys cdk.sys
CLEANFILES= 	${FILES}

MAN4=	stl.4
MLINKS=	stl.4 stli.4

all: $(FILES)

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${FILES} ${DESTDIR}${BOOTDIR}

.include <bsd.prog.mk>
