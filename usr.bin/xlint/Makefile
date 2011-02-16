#	$NetBSD: Makefile,v 1.2 1995/07/03 21:23:45 cgd Exp $
# $FreeBSD: src/usr.bin/xlint/Makefile,v 1.14.32.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.if ${LINT} == "lint"
_llib=	llib
.else
_llib=
.endif

SUBDIR=	lint1 lint2 xlint ${_llib}

.include <bsd.subdir.mk>
