# Makefile for ntpd.
# $FreeBSD: src/usr.sbin/ntp/Makefile,v 1.6 2002/11/16 08:06:43 roberto Exp $

MAINTAINER=	roberto

SUBDIR=	libntp libparse ntpd ntpdc ntpq ntpdate ntptrace \
	ntptimeset ntptime ntp-genkeys

.if !defined(NOMAN)
SUBDIR+=	doc
.endif

.include <bsd.subdir.mk>
