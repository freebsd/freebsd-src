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
LIBADD+= nv
CFLAGS+= -DBHYVE_SNAPSHOT

# usr.sbin/bhyve/snapshot.h needs ucl header
CFLAGS+= -I${SRCTOP}/contrib/libucl/include
CFLAGS+= -I${SRCTOP}/usr.sbin/bhyve
.endif

.include <bsd.prog.mk>
