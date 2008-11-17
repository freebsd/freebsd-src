# $FreeBSD$

LIB=		zfsboot
INTERNALLIB=

SRCS+=		zfs.c

CFLAGS+=	-I${.CURDIR}/../common -I${.CURDIR}/../.. -I.
CFLAGS+=	-I${.CURDIR}/../../../lib/libstand
CFLAGS+=	-I${.CURDIR}/../../cddl/boot/zfs

# XXX need arch-specific bootstrap CFLAGS here
# 
CFLAGS+=	-ffreestanding -mpreferred-stack-boundary=2 \
		-mno-mmx -mno-3dnow -mno-sse -mno-sse2 -mno-sse3

CFLAGS+=	-Wformat -Wall

.if ${MACHINE_ARCH} == "amd64"
CLEANFILES+=    machine
machine:
	ln -sf ${.CURDIR}/../../../i386/include machine
.endif

.include <bsd.lib.mk>

.if ${MACHINE_ARCH} == "amd64"
beforedepend ${OBJS}: machine
.endif
