# $FreeBSD: src/share/examples/smbfs/print/Makefile,v 1.1.2.1 2001/12/18 12:53:16 sheldonh Exp $

NOOBJ=	noobj
BINDIR=	/usr/share/examples

DISTFILES=	lj6l ljspool printcap.sample tolj

beforeinstall:
	cd ${.CURDIR}/../../../../contrib/smbfs/examples/print; \
	    ${INSTALL} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
		${DISTFILES} ${DESTDIR}${BINDIR}/smbfs/print

.include <bsd.prog.mk>
