#	$Id: Makefile,v 1.21 1998/11/11 05:21:26 asami Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

FILES=	bsd.README
FILES+=	bsd.dep.mk bsd.doc.mk bsd.docb.mk bsd.info.mk bsd.kern.mk bsd.kmod.mk
FILES+=	bsd.lib.mk bsd.libnames.mk bsd.man.mk bsd.obj.mk bsd.own.mk
FILES+=	bsd.port.mk bsd.port.post.mk  bsd.port.pre.mk bsd.port.subdir.mk
FILES+=	bsd.prog.mk bsd.sgml.mk bsd.subdir.mk
FILES+=	sys.mk
NOOBJ=	noobj

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} ${FILES} \
	    ${DESTDIR}${BINDIR}/mk

afterinstall:
	date '+%Y%m%d' > /var/db/pkg/.mkversion

.include <bsd.prog.mk>
