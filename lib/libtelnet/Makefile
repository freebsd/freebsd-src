#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $FreeBSD: src/lib/libtelnet/Makefile,v 1.9.2.1 1999/08/29 14:57:43 peter Exp $

LIB=	telnet
SRCS=	genget.c getent.c misc.c
CFLAGS+= -DHAS_CGETENT
NOPIC=	true

#
# Remove obsolete shared libraries, if any.  We don't bother moving them
# to/usr/lib/compat, since they were only used by telnet, telnetd and
# tn3270.
#
beforeinstall:
	rm -f ${DESTDIR}${SHLIBDIR}/lib${LIB}.so.2.0

.include <bsd.lib.mk>

