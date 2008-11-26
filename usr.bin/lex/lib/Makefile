# $FreeBSD: src/usr.bin/lex/lib/Makefile,v 1.9.12.1 2008/10/02 02:57:24 kensmith Exp $

LIB=    ln
SRCS=   libmain.c libyywrap.c
NO_PIC=

LINKS=  ${LIBDIR}/libln.a ${LIBDIR}/libl.a
LINKS+=	${LIBDIR}/libln.a ${LIBDIR}/libfl.a

.if !defined(NO_PROFILE)
LINKS+= ${LIBDIR}/libln_p.a ${LIBDIR}/libl_p.a
LINKS+=	${LIBDIR}/libln_p.a ${LIBDIR}/libfl_p.a
.endif

.include <bsd.lib.mk>

