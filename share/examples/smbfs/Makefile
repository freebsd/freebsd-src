# $FreeBSD$

NOOBJ=	noobj
BINDIR=	/usr/share/examples

DISTFILES=	dot.nsmbrc

beforeinstall:
	cd ${.CURDIR}/../../../contrib/smbfs/examples; \
	    ${INSTALL} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
		${DISTFILES} ${DESTDIR}${BINDIR}/smbfs

SUBDIR=	print

.include <bsd.prog.mk>
