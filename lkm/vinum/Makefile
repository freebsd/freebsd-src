# $FreeBSD$ 

.PATH:  ${.CURDIR}/../../sys/dev/vinum
KMOD=   vinum_mod
SRCS=   vnode_if.h opt_vinum.h vinumstate.h vinumvar.h \
	vinumhdr.h request.h vinumext.h vinumio.h vinumkw.h \
	vinum.c vinumparser.c vinumconfig.c vinumio.c vinumutil.c \
	vinumstate.c vinummemory.c vinumrequest.c vinumlock.c \
	vinumrevive.c vinumioctl.c vinuminterrupt.c vinumdaemon.c

PSEUDO_LKM=
NOMAN=

# option VINUMDEBUG enables a number of functions.  If you enable
# it, you must also enable VINUMDEBUG in sbin/vinum.  It influences
# the size of a number of data structures, and ioctls will fail
# if there is a mismatch.
# CFLAGS += -I${.CURDIR} -I. -DVINUMDEBUG 
CFLAGS += -I${.CURDIR} -I.
CLEANFILES+=    vnode_if.h vnode_if.c opt_vinum.h

all:

opt_vinum.h:
	@touch opt_vinum.h

state.h:  maketabs vinumstate.h
	./maketabs >state.h

maketabs: maketabs.c
	${CC} -g -o maketabs maketabs.c

.include <bsd.kmod.mk>

