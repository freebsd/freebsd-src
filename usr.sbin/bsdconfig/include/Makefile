# $FreeBSD$

NO_OBJ=

FILESDIR=	${LIBEXECDIR}/bsdconfig/include
FILES=		common.subr dialog.subr messages.subr mustberoot.subr \
		strings.subr sysrc.subr

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
