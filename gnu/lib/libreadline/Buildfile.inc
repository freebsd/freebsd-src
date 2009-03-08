# $FreeBSD$

SHLIB_MAJOR = 7
RL_LIBRARY_VERSION = 5.2

SRCDIR = ${BSDSRCTOP}/contrib/libreadline

.PATH: ${SRCDIR} ${BSDSRCTOP}/gnu/lib/libreadline

LOCAL_CFLAGS = -DRL_LIBRARY_VERSION='"$(RL_LIBRARY_VERSION)"'
CFLAGS += -I${.CURDIR}/.. -I${SRCDIR} -DHAVE_CONFIG_H ${LOCAL_CFLAGS}

HISTSRC = history.c histexpand.c histfile.c histsearch.c shell.c mbutil.c

.include "../Buildfile.inc"
