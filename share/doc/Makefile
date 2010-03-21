#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/doc/Makefile,v 1.21.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	${_bind9} IPv6 legal papers psd smm usd

.if ${MK_BIND} != "no"
_bind9=	bind9
.endif

# Default output format for troff documents is ascii.
# To generate postscript versions of troff documents, use:
#  make PRINTERDEVICE=ps

.include <bsd.subdir.mk>
