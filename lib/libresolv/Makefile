#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=resolv
LIBDIR=/usr/lib/compat

SRCS=	fakelib.c

afterinstall:
	rm -f ${DESTDIR}${LIBDIR}/libresolv.a
	rm -f ${DESTDIR}/usr/lib/libresolv.*

.include <bsd.lib.mk>
