#       $Id: Makefile.vinum,v 1.1 1999/01/22 04:43:27 grog Exp $ 

.PATH:  ${.CURDIR}/../../sys/dev/vinum
KMOD=   vinum_mod
SRCS=   vnode_if.h opt_vinum.h vinumstate.h vinumvar.h \
	vinumhdr.h request.h vinumext.h vinumio.h vinumkw.h \
	vinum.c vinumparser.c vinumconfig.c vinumio.c vinumutil.c \
	vinumstate.c vinummemory.c vinumrequest.c vinumlock.c \
	vinumrevive.c vinumioctl.c vinuminterrupt.c vinumdaemon.c

PSEUDO_LKM=
NOMAN=

CLEANFILES+=    vnode_if.h vnode_if.c opt_vinum.h

all:

opt_vinum.h:
	@touch opt_vinum.h

state.h:  maketabs vinumstate.h
	./maketabs >state.h

maketabs: maketabs.c
	${CC} -g -o maketabs maketabs.c

.include <bsd.kmod.mk>

CFLAGS += -I${.CURDIR}/../../sys/dev/vinum -I${SOURCE} -I. -g -DVINUMDEBUG -Wall -Wno-unused -Wno-parentheses
