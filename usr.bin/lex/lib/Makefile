#	@(#)Makefile	5.1 (Berkeley) 6/18/90

SHLIB_MAJOR=1
SHLIB_MINOR=0

LIB=	ln
SRCS=	libmain.c
LINKS=	${LIBDIR}/libln.a ${LIBDIR}/libl.a \

.if !defined(NOPROFILE)
LINKS+=	${LIBDIR}/libln_p.a ${LIBDIR}/libl_p.a
.endif

.include <bsd.lib.mk>
