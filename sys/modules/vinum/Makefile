#       $Id: Makefile,v 1.1.1.1 1998/09/16 05:56:21 grog Exp $ 

.PATH:  ${.CURDIR}/../../sys/dev/ccd
KMOD=   vinum_mod
SRCS=   vinum.c vinum.h vnode_if.h parser.c config.c io.c util.c vinumhdr.h request.h \
	state.c memory.c request.c lock.c vinumext.h vinumio.h vinumkw.h \
	vinumstate.h vinumvar.h revive.c vinumioctl.c interrupt.c
NOMAN=
PSEUDO_LKM=
CFLAGS = -I${.CURDIR} -O -g -I/usr/include/machine -DDEBUG -Wall -Wno-unused -Wno-parentheses

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
