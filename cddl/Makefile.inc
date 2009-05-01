# $FreeBSD: src/cddl/Makefile.inc,v 1.2.2.2.4.1 2009/04/15 03:14:26 kensmith Exp $

OPENSOLARIS_USR_DISTDIR= ${.CURDIR}/../../../cddl/contrib/opensolaris
OPENSOLARIS_SYS_DISTDIR= ${.CURDIR}/../../../sys/cddl/contrib/opensolaris

IGNORE_PRAGMA=	YES

CFLAGS+=	-DNEED_SOLARIS_BOOLEAN
