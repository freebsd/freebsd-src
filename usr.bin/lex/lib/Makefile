# $Id: Makefile,v 1.2 1994/08/25 23:11:07 wollman Exp $

LIB=    ln
SRCS=   libmain.c libyywrap.c
LINKS=  ${LIBDIR}/libln.a ${LIBDIR}/libl.a
LINKS+=	${LIBDIR}/libln.a ${LIBDIR}/libfl.a

.if !defined(NOPROFILE)
LINKS+= ${LIBDIR}/libln_p.a ${LIBDIR}/libl_p.a
LINKS+=	${LIBDIR}/libln_p.a ${LIBDIR}/libfl_p.a
.endif

NOSHARED=

.include <bsd.lib.mk>

