# $FreeBSD$ 

MAINTAINER  =  newton@atdot.dotat.org
SOURCE=	${.CURDIR}/../../dev/streams
.if defined(DEBUG)
CFLAGS+= -DDEBUG_SVR4
.endif

.PATH:  ${SOURCE}
KMOD=   streams
SRCS=   streams.c streams.h opt_streams.h opt_devfs.h

NOMAN=

CFLAGS+=	-O

CLEANFILES+=    vnode_if.h vnode_if.c streams.h opt_streams.h opt_devfs.h

all:

streams.h:
	@echo "#define NSTREAMS 1" > streams.h

opt_streams.h:
	@touch opt_streams.h

opt_devfs.h:
	@touch opt_devfs.h

.include <bsd.kmod.mk>
