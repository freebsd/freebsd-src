#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/usd/20.meref/Makefile,v 1.5.2.2 2000/12/07 10:00:05 ru Exp $

VOLUME=	usd/20.meref
SRCS=	meref.me-sed
MACROS=	-me
SRCDIR=	${.OBJDIR}

GROFFDIR=${.CURDIR}/../../../../contrib/groff

.PATH:	${GROFFDIR}/doc

version=`cat ${GROFFDIR}/VERSION`
revision=`sed -e 's/^0$$//' -e 's/^[1-9].*$$/.&/' ${GROFFDIR}/REVISION`

meref.me-sed: meref.me
	sed -e "s;@VERSION@;$(version)$(revision);" ${.ALLSRC} > ${.TARGET}

CLEANFILES+=${SRCS}

.include <bsd.doc.mk>
