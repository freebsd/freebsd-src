# $FreeBSD$

.SUFFIXES: .uu
.uu:
	rm -f ${.TARGET}
	uudecode ${.IMPSRC}

FILES=	2681.sys cdk.sys
CLEANFILES= 	${FILES}

MAN=	stl.4
MLINKS=	stl.4 stli.4
MANSUBDIR=/i386

all: $(FILES)

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${FILES} ${DESTDIR}${BOOTDIR}

.include <bsd.prog.mk>
