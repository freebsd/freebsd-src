# $FreeBSD: src/cddl/Makefile.inc,v 1.2.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $

OPENSOLARIS_USR_DISTDIR= ${.CURDIR}/../../../cddl/contrib/opensolaris
OPENSOLARIS_SYS_DISTDIR= ${.CURDIR}/../../../sys/cddl/contrib/opensolaris

IGNORE_PRAGMA=	YES

CFLAGS+=	-DNEED_SOLARIS_BOOLEAN
