#	$Id: Makefile,v 1.7.4.4 1996/06/20 14:35:29 jkh Exp $
#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93

FILES=	bsd.README
FILES+=	bsd.dep.mk bsd.doc.mk bsd.info.mk bsd.kern.mk bsd.kmod.mk bsd.obj.mk
FILES+=	bsd.lib.mk bsd.man.mk bsd.own.mk bsd.port.mk
FILES+=	bsd.port.subdir.mk bsd.prog.mk bsd.sgml.mk bsd.subdir.mk
FILES+=	sys.mk
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/mk

.include <bsd.prog.mk>
