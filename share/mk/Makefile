#	$Id$
#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93

FILES=	bsd.README bsd.dep.mk bsd.doc.mk bsd.kmod.mk bsd.lib.mk bsd.man.mk
FILES+=	bsd.prog.mk bsd.subdir.mk bsd.own.mk bsd.port.mk
FILES+=	bsd.port.subdir.mk sys.mk
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/mk

.include <bsd.prog.mk>
