# $FreeBSD: src/sys/boot/arc/Makefile.inc,v 1.2.26.1 2008/10/02 02:57:24 kensmith Exp $
# Options used when building app-specific libalpha components
LOAD_ADDRESS=		0xffffffff80900000
DPADD+=			${DESTDIR}/${LIBDIR}/libstand.a
LIBSTANDDIR=		${.CURDIR}/../../../../lib/libstand
LIBSTAND=		-lstand
LIBARC=			${.OBJDIR}/../lib/libarc.a
