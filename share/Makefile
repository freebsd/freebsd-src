#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/Makefile,v 1.22.2.3 2001/07/09 13:49:57 ru Exp $

# Do not include `info' in the SUBDIR list, it is handled separately.

SUBDIR= colldef dict examples man me misc mk mklocale skel \
	syscons tabset termcap timedef zoneinfo

.if !defined(NO_SHAREDOCS)
SUBDIR+= doc
.endif

.if !defined(NO_I4B)
SUBDIR+= isdn
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+= sendmail
.endif

.include <bsd.subdir.mk>
