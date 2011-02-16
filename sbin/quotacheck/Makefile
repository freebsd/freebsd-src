# $FreeBSD: src/sbin/quotacheck/Makefile,v 1.9.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	quotacheck
SRCS=	quotacheck.c preen.c fsutil.c utilities.c
WARNS?=	2
MAN=	quotacheck.8

.PATH:	${.CURDIR}/../fsck ${.CURDIR}/../fsck_ffs

.include <bsd.prog.mk>
