#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $FreeBSD$

LIB=	telnet

INTERNALLIB=yes
INTERNALSTATICLIB=yes
NOPIC=	yes

SRCS=	genget.c getent.c misc.c
CFLAGS+= -DHAS_CGETENT

#
# Remove obsolete shared libraries, if any.  We don't bother moving them
# to/usr/lib/compat, since they were only used by telnet, telnetd and
# tn3270.
#
beforeinstall:
	rm -f ${DESTDIR}${SHLIBDIR}/lib${LIB}.so.2.0

.include <bsd.lib.mk>

