# $FreeBSD$
# Options used when building app-specific libalpha components
LOAD_ADDRESS=		0xffffffff80900000
DPADD+=			${DESTDIR}/${LIBDIR}/libstand.a
LIBSTANDDIR=		${.CURDIR}/../../../../lib/libstand
LIBSTAND=		-lstand
LIBARC=			${.OBJDIR}/../lib/libarc.a
