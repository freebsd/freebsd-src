#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$Id$

LIB=	telnet
SRCS=	genget.c getent.c misc.c
CFLAGS+= -DHAS_CGETENT
NOPIC=	true

#
# Before complaining about this, please *double-check* that you have
# updated the ldconfig path in /etc/rc to include /usr/lib/compat that
# was added in src/etc/rc rev 1.98.
# This is so that `ld' will not continue to generate binaries linked
# shared against libtelnet, so that in a future release we can move this
# off to a compat dist (like compat22).
#
beforeinstall:
	rm -f ${DESTDIR}${LIBDIR}/lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}

.include <bsd.lib.mk>

