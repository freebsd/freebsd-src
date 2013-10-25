# $FreeBSD$

LIB=		compositor
SHLIB_MAJOR=	1

SRCS=	compositor.c
INCS=	compositor.h

# FIXME: Install this in /usr/include properly, rather than copying it into the
# libcompositor directory manually. */
INCS+= cheri_compositor.h
CFLAGS+=-I${.CURDIR}

.include <bsd.lib.mk>
