# $FreeBSD: src/sys/boot/arc/Makefile.inc,v 1.1.1.1.2.1 2000/07/06 23:29:47 obrien Exp $
# Options used when building app-specific libalpha components
LOAD_ADDRESS=		0xffffffff80900000
DPADD+=			${DESTDIR}/${LIBDIR}/libstand.a
LIBSTANDDIR=		${.CURDIR}/../../../../lib/libstand
LIBSTAND=		-lstand
LIBARC=			${.OBJDIR}/../lib/libarc.a
