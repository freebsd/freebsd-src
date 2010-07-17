# $FreeBSD: src/sbin/geom/class/Makefile,v 1.16.10.2.2.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	cache
SUBDIR+=concat
.if ${MK_OPENSSL} != "no"
SUBDIR+=eli
.endif
SUBDIR+=journal
SUBDIR+=label
SUBDIR+=mirror
SUBDIR+=multipath
SUBDIR+=nop
SUBDIR+=part
SUBDIR+=raid3
SUBDIR+=sched
SUBDIR+=shsec
SUBDIR+=stripe
SUBDIR+=virstor

.include <bsd.subdir.mk>
