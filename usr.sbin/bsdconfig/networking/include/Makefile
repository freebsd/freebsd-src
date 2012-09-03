# $FreeBSD$

NO_OBJ=

FILESDIR=	${LIBEXECDIR}/bsdconfig/120.networking/include
FILES=		common.subr device.subr hostname.subr ipaddr.subr media.subr \
		messages.subr netmask.subr resolv.subr routing.subr

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
