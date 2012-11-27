# $FreeBSD$

NO_OBJ=

SUBDIR= include

FILESDIR=	${LIBEXECDIR}/bsdconfig/120.networking
FILES=		INDEX USAGE

SCRIPTSDIR=	${FILESDIR}
SCRIPTS=	defaultrouter devices hostname nameservers networking

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
