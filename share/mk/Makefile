#	$Id: Makefile,v 1.5 1994/09/14 21:59:27 wollman Exp $
#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93

FILES=	bsd.README
FILES+=	bsd.dep.mk bsd.doc.mk bsd.info.mk bsd.kmod.mk
FILES+=	bsd.lib.mk bsd.man.mk bsd.own.mk bsd.port.mk
FILES+=	bsd.port.subdir.mk bsd.prog.mk bsd.subdir.mk
FILES+=	sys.mk
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/mk

.include <bsd.prog.mk>
