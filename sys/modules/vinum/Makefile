#       $Id: Makefile,v 1.5 1998/12/28 05:27:24 peter Exp $ 

SOURCE=	${.CURDIR}/../../dev/vinum

.PATH:  ${SOURCE}
KMOD=   vinum
SRCS=   vnode_if.h opt_vinum.h vinumstate.h vinumvar.h \
	vinumhdr.h request.h vinumext.h vinumio.h vinumkw.h \
	vinum.c vinumparser.c vinumconfig.c vinumio.c vinumutil.c \
	vinumstate.c vinummemory.c vinumrequest.c vinumlock.c \
	vinumrevive.c vinumioctl.c vinuminterrupt.c
NOMAN=
CFLAGS = -I${SOURCE} -O -DVINUMDEBUG -Wall -Wno-unused -Wno-parentheses

CLEANFILES+=    vnode_if.h vnode_if.c opt_vinum.h

all:

# VINUMDEBUG in CFLAGS instead..
opt_vinum.h:
	@touch opt_vinum.h

state.h:  maketabs vinumstate.h
	./maketabs >state.h

maketabs: maketabs.c
	${CC} -g -o maketabs maketabs.c

.include <bsd.kmod.mk>
