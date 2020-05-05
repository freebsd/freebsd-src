#
# $FreeBSD$
#

.include <src.opts.mk>

PROG=	bhyvectl
SRCS=	bhyvectl.c
PACKAGE=	bhyve

MAN=	bhyvectl.8

LIBADD=	vmmapi util

WARNS?=	3

CFLAGS+= -I${SRCTOP}/sys/amd64/vmm

.if ${MK_BHYVE_SNAPSHOT} != "no"
CFLAGS+= -DBHYVE_SNAPSHOT
.endif

.include <bsd.prog.mk>
