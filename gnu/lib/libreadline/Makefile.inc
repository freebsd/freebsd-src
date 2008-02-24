# $FreeBSD: src/gnu/lib/libreadline/Makefile.inc,v 1.15 2007/06/18 18:47:53 rafan Exp $

SHLIB_MAJOR=7
RL_LIBRARY_VERSION=5.2

SRCDIR= ${.CURDIR}/../../../../contrib/libreadline
.PATH: ${SRCDIR} ${.CURDIR}/..

LOCAL_CFLAGS= -DRL_LIBRARY_VERSION='"$(RL_LIBRARY_VERSION)"'
CFLAGS+=-I${.CURDIR}/.. -I${SRCDIR} -DHAVE_CONFIG_H ${LOCAL_CFLAGS}

HISTSRC=history.c histexpand.c histfile.c histsearch.c shell.c mbutil.c

.include "../Makefile.inc"
