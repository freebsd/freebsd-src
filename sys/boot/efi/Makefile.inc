# $FreeBSD$
# Options used when building app-specific efi components

DPADD+=			${DESTDIR}/${LIBDIR}/libstand.a
LIBSTANDDIR=		${.CURDIR}/../../../../lib/libstand
LIBSTAND=		-lstand
LIBEFI=			${.OBJDIR}/../libefi/libefi.a
