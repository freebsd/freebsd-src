#	$FreeBSD$
#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93

FILES=	bsd.README
FILES+=	bsd.dep.mk bsd.doc.mk bsd.info.mk bsd.kern.mk bsd.kmod.mk
FILES+=	bsd.lib.mk bsd.libnames.mk bsd.man.mk bsd.obj.mk bsd.own.mk
FILES+=	bsd.port.mk bsd.port.subdir.mk bsd.prog.mk bsd.sgml.mk bsd.subdir.mk
FILES+=	sys.mk
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	cd ${.CURDIR} && ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/mk

.include <bsd.prog.mk>
