# $FreeBSD$

NO_OBJ=

FILESDIR=	${SHAREDIR}/bsdconfig
FILES=		common.subr dialog.subr mustberoot.subr script.subr \
		strings.subr sysrc.subr variable.subr

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
