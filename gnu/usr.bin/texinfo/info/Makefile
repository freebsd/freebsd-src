#
# Bmakefile for GNU info
#
# $id$
#

PROG= info

SRCS+= dir.c display.c dribble.c echo_area.c filesys.c info-utils.c info.c
SRCS+= infodoc.c infomap.c m-x.c nodes.c search.c session.c signals.c
SRCS+= terminal.c tilde.c window.c indices.c doc.c nodemenu.c
SRCS+= footnotes.c variables.c gc.c xmalloc.c getopt1.c getopt.c

CFLAGS=

CFLAGS+= -I${.CURDIR}
CFLAGS+= -DNAMED_FUNCTIONS=1 -DSTDC_HEADERS=1
CFLAGS+= -DHAVE_UNISTD_H=1 -DHAVE_STRING_H=1 -DHAVE_VARARGS_H=1
CFLAGS+= -DHAVE_SYS_FCNTL_H=1 -DHAVE_SETVBUF=1 -DHAVE_GETCWD=1 -DHAVE_BZERO=1
CFLAGS+= -DHAVE_RINDEX=1 -DHAVE_VFPRINTF=1 -DHAVE_VSPRINTF=1
CFLAGS+= -DHAVE_SYS_TIME_H=1 -DDEFAULT_INFOPATH='"${INFODIR}"'

LDADD+= -ltermcap
DPADD+= ${LIBTERMCAP}

.include "../../Makefile.inc"
.include <bsd.prog.mk>

