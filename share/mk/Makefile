#	$Id: Makefile,v 1.16 1998/01/03 14:04:55 wosch Exp $
#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93

FILES=	bsd.README
FILES+=	bsd.dep.mk bsd.doc.mk bsd.info.mk bsd.kern.mk bsd.kmod.mk
FILES+=	bsd.lib.mk bsd.libnames.mk bsd.locale.mk bsd.man.mk bsd.obj.mk
FILES+=	bsd.own.mk bsd.port.mk bsd.port.subdir.mk bsd.prog.mk bsd.sgml.mk
FILES+=	bsd.subdir.mk sys.mk
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/mk

.include <bsd.prog.mk>
