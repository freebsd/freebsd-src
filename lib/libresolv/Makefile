#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=		resolv
SHLIBDIR=	${LIBDIR}/compat
INTERNALLIB=	yes		# Do not build or install libresolv*.a

SRCS=	fakelib.c

afterinstall:
	rm -f ${DESTDIR}${LIBDIR}/libresolv*

.include <bsd.lib.mk>
