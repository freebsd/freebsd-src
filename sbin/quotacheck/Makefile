# $FreeBSD: src/sbin/quotacheck/Makefile,v 1.9.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	quotacheck
SRCS=	quotacheck.c preen.c fsutil.c utilities.c
WARNS?=	2
MAN=	quotacheck.8

.PATH:	${.CURDIR}/../fsck ${.CURDIR}/../fsck_ffs

.include <bsd.prog.mk>
