# $FreeBSD: src/usr.sbin/stallion/bootcode/Makefile,v 1.9 1999/08/28 01:20:05 peter Exp $

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
