# Makefile for ntpd.
# $FreeBSD$

SUBDIR=	libntp libparse ntpd ntpdc ntpq ntpdate ntptrace \
	ntptime ntp-keygen sntp

.if !defined(NO_MAN)
SUBDIR+=	doc
.endif

.include <bsd.subdir.mk>
