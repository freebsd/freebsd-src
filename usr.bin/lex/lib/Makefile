# $FreeBSD: src/usr.bin/lex/lib/Makefile,v 1.10.2.1.10.1 2010/12/21 17:10:29 kensmith Exp $

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

