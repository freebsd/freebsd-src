# $FreeBSD$

NO_OBJ=

FILESDIR=	${LIBEXECDIR}/bsdconfig/include
FILES=		bsdconfig.hlp media.hlp messages.subr network_device.hlp \
		options.hlp tcp.hlp usage.hlp

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
