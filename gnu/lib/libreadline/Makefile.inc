#       $Id: Makefile.inc,v 1.1.2.1 1999/08/18 19:15:44 obrien Exp $

SHLIB_MAJOR= 3
SHLIB_MINOR= 0
RL_LIBRARY_VERSION= 4.0

SRCDIR= ${.CURDIR}/../../../../contrib/libreadline
.PATH: ${SRCDIR} ${.CURDIR}/..

LOCAL_CFLAGS= -DRL_LIBRARY_VERSION='"$(RL_LIBRARY_VERSION)"'
CFLAGS+= -I${.CURDIR}/.. -I${SRCDIR} -DHAVE_CONFIG_H ${LOCAL_CFLAGS}

HISTSRC=  history.c histexpand.c histfile.c histsearch.c shell.c
