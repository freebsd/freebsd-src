# $FreeBSD$

SUBDIR=	doc libgdb gdb gdbtui kgdb

TARGET_ARCH?= ${MACHINE_ARCH}
.if exists(${.CURDIR}/gdbserver/reg-${TARGET_ARCH}.c)
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
