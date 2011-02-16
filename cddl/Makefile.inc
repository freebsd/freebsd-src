# $FreeBSD: src/cddl/Makefile.inc,v 1.6.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

OPENSOLARIS_USR_DISTDIR= ${.CURDIR}/../../../cddl/contrib/opensolaris
OPENSOLARIS_SYS_DISTDIR= ${.CURDIR}/../../../sys/cddl/contrib/opensolaris

IGNORE_PRAGMA=	YES

CFLAGS+=	-DNEED_SOLARIS_BOOLEAN

CSTD?=		gnu89
