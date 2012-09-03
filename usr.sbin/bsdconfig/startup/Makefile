# $FreeBSD$

NO_OBJ=

SUBDIR= include

FILESDIR=	${LIBEXECDIR}/bsdconfig/140.startup
FILES=		INDEX USAGE

SCRIPTSDIR=	${FILESDIR}
SCRIPTS=	misc rcadd rcconf rcdelete rcedit rcvar startup

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
