# $FreeBSD: src/usr.sbin/fdformat/Makefile,v 1.5.14.1 2001/04/25 12:09:33 ru Exp $

PROG=	fdformat

# the -I's seem to be confusing, but necessery this way
# (so the right <unistd.h> will be found in /usr/include, and the
# "../i386/isa/ic/nec765.h" included from fdreg.h is accessible, too)
CFLAGS+= -Wall

.include <bsd.prog.mk>
