# Makefile for ntpd.
# $FreeBSD: src/usr.sbin/ntp/Makefile,v 1.7 2003/06/05 05:03:01 roberto Exp $

SUBDIR=	libntp libparse ntpd ntpdc ntpq ntpdate ntptrace \
	ntptimeset ntptime ntp-genkeys

.if !defined(NOMAN)
SUBDIR+=	doc
.endif

.include <bsd.subdir.mk>
