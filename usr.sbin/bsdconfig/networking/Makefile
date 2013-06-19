# $FreeBSD$

NO_OBJ=

SUBDIR= include share

FILESDIR=	${LIBEXECDIR}/bsdconfig/120.networking
FILES=		INDEX USAGE

SCRIPTSDIR=	${FILESDIR}
SCRIPTS=	defaultrouter devices hostname nameservers networking

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
