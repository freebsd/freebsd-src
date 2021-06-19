# $FreeBSD$

BINDIR=		/usr/share/certs/untrusted

UNTRUSTED_CERTS!=	echo ${.CURDIR}/*.pem 2> /dev/null || true

FILES+=	 ${UNTRUSTED_CERTS}

.include <bsd.prog.mk>
