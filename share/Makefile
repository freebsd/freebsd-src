#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/Makefile,v 1.22.2.1 2000/08/17 00:49:25 jhb Exp $

# Do not include `info' in the SUBDIR list, it is handled separately.

SUBDIR= colldef dict examples isdn man me misc mk mklocale skel \
	syscons tabset termcap timedef zoneinfo

.if !defined(NO_SHAREDOCS)
SUBDIR+= doc
.endif

.include <bsd.subdir.mk>
