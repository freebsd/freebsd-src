# $FreeBSD$ 

MAINTAINER  =  newton@atdot.dotat.org
SOURCE=	${.CURDIR}/../../dev/streams
.if defined(DEBUG)
CFLAGS+= -DDEBUG_SVR4
.endif

.PATH:  ${SOURCE}
KMOD=   streams
SRCS=   streams.c opt_streams.h opt_devfs.h

NOMAN=

CFLAGS+=	-O

CLEANFILES+=    vnode_if.h vnode_if.c opt_streams.h opt_devfs.h

opt_streams.h:
	@touch opt_streams.h

opt_devfs.h:
	@touch opt_devfs.h

.include <bsd.kmod.mk>
