# $FreeBSD: src/gnu/lib/libreadline/Makefile.inc,v 1.8 2002/09/28 00:25:27 peter Exp $

SHLIB_MAJOR= 4
RL_LIBRARY_VERSION= 4.2

SRCDIR= ${.CURDIR}/../../../../contrib/libreadline
.PATH: ${SRCDIR} ${.CURDIR}/..

LOCAL_CFLAGS= -DRL_LIBRARY_VERSION='"$(RL_LIBRARY_VERSION)"'
CFLAGS+= -I${.CURDIR}/.. -I${SRCDIR} -DHAVE_CONFIG_H ${LOCAL_CFLAGS}

HISTSRC=  history.c histexpand.c histfile.c histsearch.c shell.c

.include "../Makefile.inc"
