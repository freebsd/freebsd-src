# $FreeBSD: src/cddl/Makefile.inc,v 1.2.2.3.2.1 2010/02/10 00:26:20 kensmith Exp $

OPENSOLARIS_USR_DISTDIR= ${.CURDIR}/../../../cddl/contrib/opensolaris
OPENSOLARIS_SYS_DISTDIR= ${.CURDIR}/../../../sys/cddl/contrib/opensolaris

IGNORE_PRAGMA=	YES

CFLAGS+=	-DNEED_SOLARIS_BOOLEAN

CSTD?=		gnu89
