# $FreeBSD$

NOOBJ=	noobj
BINDIR=	/usr/share/examples

DISTFILES=	lj6l ljspool printcap.sample tolj

beforeinstall:
	cd ${.CURDIR}/../../../../contrib/smbfs/examples/print; \
	    ${INSTALL} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
		${DISTFILES} ${DESTDIR}${BINDIR}/smbfs/print

.include <bsd.prog.mk>
