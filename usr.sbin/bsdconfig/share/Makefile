# $FreeBSD$

NO_OBJ=

SUBDIR=	media packages

FILESDIR=	${SHAREDIR}/bsdconfig
FILES=		common.subr device.subr dialog.subr keymap.subr \
		mustberoot.subr script.subr strings.subr struct.subr \
		sysrc.subr variable.subr

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
