#       $Id: Makefile,v 1.4 1998/10/23 11:56:44 peter Exp $ 

SOURCE=	${.CURDIR}/../../dev/vinum

.PATH:  ${SOURCE}
KMOD=   vinum
SRCS=   vinum.c vnode_if.h parser.c config.c io.c util.c vinumhdr.h request.h \
	state.c memory.c request.c lock.c vinumext.h vinumio.h vinumkw.h \
	vinumstate.h vinumvar.h revive.c vinumioctl.c interrupt.c
NOMAN=
CFLAGS = -I${SOURCE} -O -g -I/usr/include/machine -DDEBUG -Wall -Wno-unused -Wno-parentheses

CLEANFILES+=    vnode_if.h vnode_if.c

all:

state.h:  maketabs vinumstate.h
	./maketabs >state.h

maketabs: maketabs.c
	${CC} -g -o maketabs maketabs.c

.include <bsd.kmod.mk>
