#	@(#)Makefile	8.1 (Berkeley) 6/9/93
# $FreeBSD$

PROG=	vfontedpr
SRCS=	regexp.c vfontedpr.c
MAN=	vgrind.1 vgrindefs.5
BINDIR=	/usr/libexec
BIN2DIR=/usr/bin
EXTRA=	vgrindefs.src.db
CLEANFILES+=	${EXTRA}

all: ${EXTRA}

${EXTRA}: ${.CURDIR}/vgrindefs.src
	cap_mkdb -f vgrindefs.src ${.CURDIR}/vgrindefs.src

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/vgrind.sh ${DESTDIR}${BIN2DIR}/vgrind
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/vgrindefs.src \
	    ${DESTDIR}/usr/share/misc/vgrindefs
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    vgrindefs.src.db ${DESTDIR}/usr/share/misc/vgrindefs.db
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/tmac.vgrind \
	    ${DESTDIR}/usr/share/tmac

.include <bsd.prog.mk>
