# $FreeBSD: src/usr.bin/lex/lib/Makefile,v 1.6.2.1 1999/08/29 15:29:37 peter Exp $

LIB=    ln
SRCS=   libmain.c libyywrap.c
NOPIC=	yes

LINKS=  ${LIBDIR}/libln.a ${LIBDIR}/libl.a
LINKS+=	${LIBDIR}/libln.a ${LIBDIR}/libfl.a

.if !defined(NOPROFILE)
LINKS+= ${LIBDIR}/libln_p.a ${LIBDIR}/libl_p.a
LINKS+=	${LIBDIR}/libln_p.a ${LIBDIR}/libfl_p.a
.endif

.include <bsd.lib.mk>

