#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/usd/19.memacros/Makefile,v 1.8.36.1.6.1 2010/12/21 17:09:25 kensmith Exp $

VOLUME=		usd/19.memacros
SRCS=		meintro.me-sed
MACROS=		-me
GROFFDIR=	${.CURDIR}/../../../../contrib/groff
SRCDIR=		${GROFFDIR}/doc

version=`cat ${GROFFDIR}/VERSION`
revision=`sed -e 's/^0$$//' -e 's/^[1-9].*$$/.&/' ${GROFFDIR}/REVISION`

meintro.me-sed: meintro.me
	sed -e "s;@VERSION@;$(version)$(revision);" ${.ALLSRC} > ${.TARGET}

CLEANFILES=	${SRCS}

.include <bsd.doc.mk>
