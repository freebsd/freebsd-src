# $Id: Makefile,v 1.3.2.1 1997/06/29 06:05:18 pst Exp $

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

