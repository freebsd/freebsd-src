# $FreeBSD: src/usr.bin/lex/lib/Makefile,v 1.7.26.1 2005/02/13 07:23:38 obrien Exp $

LIB=    ln
SRCS=   libmain.c libyywrap.c
NOPIC=

LINKS=  ${LIBDIR}/libln.a ${LIBDIR}/libl.a
LINKS+=	${LIBDIR}/libln.a ${LIBDIR}/libfl.a

.if !defined(NOPROFILE)
LINKS+= ${LIBDIR}/libln_p.a ${LIBDIR}/libl_p.a
LINKS+=	${LIBDIR}/libln_p.a ${LIBDIR}/libfl_p.a
.endif

.include <bsd.lib.mk>

