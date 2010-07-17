# $FreeBSD: src/cddl/Makefile.inc,v 1.6.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

OPENSOLARIS_USR_DISTDIR= ${.CURDIR}/../../../cddl/contrib/opensolaris
OPENSOLARIS_SYS_DISTDIR= ${.CURDIR}/../../../sys/cddl/contrib/opensolaris

IGNORE_PRAGMA=	YES

CFLAGS+=	-DNEED_SOLARIS_BOOLEAN

CSTD?=		gnu89
