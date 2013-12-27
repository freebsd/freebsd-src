# $FreeBSD$

NO_OBJ=

FILESDIR=	${SHAREDIR}/bsdconfig/timezone
FILES=		continents.subr countries.subr iso3166.subr menus.subr \
		zones.subr

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
