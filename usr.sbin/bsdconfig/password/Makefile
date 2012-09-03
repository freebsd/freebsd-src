# $FreeBSD$

NO_OBJ=

SUBDIR= include

FILESDIR=	${LIBEXECDIR}/bsdconfig/040.password
FILES=		INDEX USAGE

SCRIPTSDIR=	${FILESDIR}
SCRIPTS=	password

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
