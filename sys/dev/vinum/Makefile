# $FreeBSD: src/sys/dev/vinum/Makefile,v 1.5.2.1 1999/08/29 16:24:12 peter Exp $ 

.PATH:  ${.CURDIR}/../../sys/dev/ccd
KMOD=   vinum_mod
SRCS=   vinum.c vinum.h vnode_if.h parser.c config.c io.c util.c vinumhdr.h request.h \
	state.c memory.c request.c lock.c vinumext.h vinumio.h vinumkw.h \
	vinumstate.h vinumvar.h revive.c vinumioctl.c interrupt.c
NOMAN=
PSEUDO_LKM=
CFLAGS = -I${.CURDIR} -O -g -DVINUMDEBUG -Wall -Wno-unused -Wno-parentheses

CLEANFILES+=    vinum.h vnode_if.h vnode_if.c

all:

# We don't need this, but the Makefile wants it
vinum.h:
	touch $@

state.h:  maketabs vinumstate.h
	./maketabs >state.h

maketabs: maketabs.c
	${CC} -g -o maketabs maketabs.c

.include <bsd.kmod.mk>
