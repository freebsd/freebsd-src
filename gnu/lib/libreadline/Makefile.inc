# $FreeBSD: src/gnu/lib/libreadline/Makefile.inc,v 1.4 1999/09/29 15:18:26 marcel Exp $

SHLIB_MAJOR= 4
SHLIB_MINOR= 0
RL_LIBRARY_VERSION= 4.0

SRCDIR= ${.CURDIR}/../../../../contrib/libreadline
.PATH: ${SRCDIR} ${.CURDIR}/..

LOCAL_CFLAGS= -DRL_LIBRARY_VERSION='"$(RL_LIBRARY_VERSION)"'
CFLAGS+= -I${.CURDIR}/.. -I${SRCDIR} -DHAVE_CONFIG_H ${LOCAL_CFLAGS}

HISTSRC=  history.c histexpand.c histfile.c histsearch.c shell.c
