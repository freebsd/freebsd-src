# $FreeBSD$ 

MAINTAINER  =  grog@FreeBSD.org

.PATH:  ${.CURDIR}/../../dev/vinum
KMOD=   vinum
SRCS=   vnode_if.h opt_vinum.h vinumstate.h vinumvar.h \
	vinumhdr.h request.h vinumext.h vinumio.h vinumkw.h \
	vinum.c vinumparser.c vinumconfig.c vinumio.c vinumutil.c \
	vinumstate.c vinummemory.c vinumrequest.c vinumlock.c \
	vinumrevive.c vinumioctl.c vinuminterrupt.c vinumdaemon.c vinumraid5.c

NOMAN=

# If you remove -DVINUMDEBUG here, you must also do it in src/sbin/Makefile.
# Otherwise a number of ioctls will fail.
CFLAGS+=	-DVINUMDEBUG -O

all:

state.h: vinumstate.h
	./maketabs >state.h

build-tools: maketabs

maketabs: maketabs.c
	${CC} -o maketabs maketabs.c

.include <bsd.kmod.mk>
