# Makefile for ntpd.
# $FreeBSD: src/usr.sbin/ntp/Makefile,v 1.10 2004/07/22 08:35:41 roberto Exp $

SUBDIR=	libntp libparse ntpd ntpdc ntpq ntpdate ntptrace \
	ntptime ntp-keygen sntp

.if !defined(NOMAN)
SUBDIR+=	doc
.endif

.include <bsd.subdir.mk>
