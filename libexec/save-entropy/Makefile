# $FreeBSD$

NOMAN=	noman

beforeinstall:
	${INSTALL} -c -o operator -g operator -m 500 \
	    ${.CURDIR}/save-entropy.sh ${DESTDIR}${BINDIR}/save-entropy

.include <bsd.prog.mk>

