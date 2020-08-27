# $FreeBSD$

BINDIR=		/usr/share/certs/blacklisted

BLACKLISTED_CERTS!=	echo ${.CURDIR}/*.pem 2> /dev/null || true

FILES+=	 ${BLACKLISTED_CERTS}

.include <bsd.prog.mk>
