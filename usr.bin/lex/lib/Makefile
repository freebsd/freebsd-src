# $FreeBSD$

LIB=    ln
SRCS=   libmain.c libyywrap.c
LINKS=  ${LIBDIR}/libln.a ${LIBDIR}/libl.a \

.if !defined(NOPROFILE)
LINKS+= ${LIBDIR}/libln_p.a ${LIBDIR}/libl_p.a
.endif

NOSHARED=

.include <bsd.lib.mk>

