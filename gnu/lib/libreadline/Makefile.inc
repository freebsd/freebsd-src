# $FreeBSD: src/gnu/lib/libreadline/Makefile.inc,v 1.4.2.1 2000/07/06 23:11:07 ache Exp $

SHLIB_MAJOR= 4
SHLIB_MINOR= 1
RL_LIBRARY_VERSION= 4.1

SRCDIR= ${.CURDIR}/../../../../contrib/libreadline
.PATH: ${SRCDIR} ${.CURDIR}/..

LOCAL_CFLAGS= -DRL_LIBRARY_VERSION='"$(RL_LIBRARY_VERSION)"'
CFLAGS+= -I${.CURDIR}/.. -I${SRCDIR} -DHAVE_CONFIG_H ${LOCAL_CFLAGS}

HISTSRC=  history.c histexpand.c histfile.c histsearch.c shell.c
