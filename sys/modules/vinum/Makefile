#       $Id: Makefile.vinum,v 1.2 1999/02/11 05:19:49 grog Exp grog $ 

MAINTAINER  =  grog@FreeBSD.org
SOURCE=	${.CURDIR}/../../dev/vinum

.PATH:  ${SOURCE}
KMOD=   vinum
SRCS=   vnode_if.h opt_vinum.h vinumstate.h vinumvar.h \
	vinumhdr.h request.h vinumext.h vinumio.h vinumkw.h \
	vinum.c vinumparser.c vinumconfig.c vinumio.c vinumutil.c \
	vinumstate.c vinummemory.c vinumrequest.c vinumlock.c \
	vinumrevive.c vinumioctl.c vinuminterrupt.c vinumdaemon.c vinumraid5.c

NOMAN=

# If you remove -DVINUMDEBUG here, you must also do it in src/sbin/Makefile.
# Otherwise a number of ioctls will fail.
CFLAGS+=	-DVINUMDEBUG -g -O

CLEANFILES+=    vnode_if.h vnode_if.c opt_vinum.h

all:

opt_vinum.h:
	@touch opt_vinum.h

state.h:  maketabs vinumstate.h
	./maketabs >state.h

maketabs: maketabs.c
	${CC} -g -o maketabs maketabs.c

.include <bsd.kmod.mk>
