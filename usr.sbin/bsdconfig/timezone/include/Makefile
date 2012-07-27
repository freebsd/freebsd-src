# $FreeBSD$

NO_OBJ=

FILESDIR=	${LIBEXECDIR}/bsdconfig/090.timezone/include
FILES=		continents.subr countries.subr iso3166.subr menus.subr \
		messages.subr zones.subr

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
