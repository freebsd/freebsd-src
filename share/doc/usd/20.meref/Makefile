#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/usd/20.meref/Makefile,v 1.8 2002/10/29 14:56:09 ru Exp $

VOLUME=		usd/20.meref
SRCS=		meref.me-sed
MACROS=		-me
GROFFDIR=	${.CURDIR}/../../../../contrib/groff
SRCDIR=		${GROFFDIR}/doc

version=`cat ${GROFFDIR}/VERSION`
revision=`sed -e 's/^0$$//' -e 's/^[1-9].*$$/.&/' ${GROFFDIR}/REVISION`

meref.me-sed: meref.me
	sed -e "s;@VERSION@;$(version)$(revision);" ${.ALLSRC} > ${.TARGET}

CLEANFILES=	${SRCS}

.include <bsd.doc.mk>
