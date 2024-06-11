.include <src.opts.mk>

.PATH: ${.CURDIR}/${MACHINE_CPUARCH}

PROG=	bhyvectl
SRCS=	bhyvectl.c
PACKAGE=	bhyve

.include "${MACHINE_CPUARCH}/Makefile.inc"

MAN=	bhyvectl.8

LIBADD=	vmmapi util

CFLAGS+= -I${.CURDIR} -I${SRCTOP}/sys/amd64/vmm

.if ${MK_BHYVE_SNAPSHOT} != "no"
LIBADD+= nv
CFLAGS+= -DBHYVE_SNAPSHOT

# usr.sbin/bhyve/snapshot.h needs ucl header
CFLAGS+= -I${SRCTOP}/contrib/libucl/include
CFLAGS+= -I${SRCTOP}/usr.sbin/bhyve
.endif

.include <bsd.prog.mk>
