# $FreeBSD: src/share/examples/smbfs/Makefile,v 1.1.2.2 2002/04/01 16:22:23 roam Exp $

NOOBJ=	noobj
BINDIR=	/usr/share/examples

DISTFILES=	dot.nsmbrc

beforeinstall:
	cd ${.CURDIR}/../../../contrib/smbfs/examples; \
	    ${INSTALL} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
		${DISTFILES} ${DESTDIR}${BINDIR}/smbfs

SUBDIR=	print

.include <bsd.prog.mk>
