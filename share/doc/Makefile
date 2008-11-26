#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/doc/Makefile,v 1.19.12.1 2008/10/02 02:57:24 kensmith Exp $

SUBDIR=	${_bind9} IPv6 papers psd smm usd

.if !defined(NO_BIND)
_bind9=	bind9
.endif

# Default output format for troff documents is ascii.
# To generate postscript versions of troff documents, use:
#  make PRINTERDEVICE=ps

.include <bsd.subdir.mk>
