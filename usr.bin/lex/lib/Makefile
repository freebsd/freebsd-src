# $FreeBSD: src/usr.bin/lex/lib/Makefile,v 1.11.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.include <bsd.own.mk>

LIB=    ln
SRCS=   libmain.c libyywrap.c
NO_PIC=

.if ${MK_INSTALLLIB} != "no"
LINKS=  ${LIBDIR}/libln.a ${LIBDIR}/libl.a
LINKS+=	${LIBDIR}/libln.a ${LIBDIR}/libfl.a
.endif

.if ${MK_PROFILE} != "no"
LINKS+= ${LIBDIR}/libln_p.a ${LIBDIR}/libl_p.a
LINKS+=	${LIBDIR}/libln_p.a ${LIBDIR}/libfl_p.a
.endif

.include <bsd.lib.mk>

