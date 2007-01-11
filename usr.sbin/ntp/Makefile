# Makefile for ntpd.
# $FreeBSD: src/usr.sbin/ntp/Makefile,v 1.11 2004/12/21 08:47:33 ru Exp $

SUBDIR=	libntp libparse ntpd ntpdc ntpq ntpdate ntptrace \
	ntptime ntp-keygen sntp

.if !defined(NO_MAN)
SUBDIR+=	doc
.endif

.include <bsd.subdir.mk>
