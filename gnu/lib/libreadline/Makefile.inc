# $FreeBSD$

SHLIB_MAJOR= 4
SHLIB_MINOR= 2
RL_LIBRARY_VERSION= 4.2

SRCDIR= ${.CURDIR}/../../../../contrib/libreadline
.PATH: ${SRCDIR} ${.CURDIR}/..

LOCAL_CFLAGS= -DRL_LIBRARY_VERSION='"$(RL_LIBRARY_VERSION)"'
CFLAGS+= -I${.CURDIR}/.. -I${SRCDIR} -DHAVE_CONFIG_H ${LOCAL_CFLAGS}

HISTSRC=  history.c histexpand.c histfile.c histsearch.c shell.c
