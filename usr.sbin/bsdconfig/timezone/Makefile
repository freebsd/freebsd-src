# $FreeBSD$

NO_OBJ=

SUBDIR= include

FILESDIR=	${LIBEXECDIR}/bsdconfig/090.timezone
FILES=		INDEX USAGE

SCRIPTSDIR=	${FILESDIR}
SCRIPTS=	timezone

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
