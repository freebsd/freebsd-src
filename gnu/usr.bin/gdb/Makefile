# $FreeBSD$

SUBDIR=	doc libgdb gdb gdbtui kgdb

TARGET_ARCH?= ${MACHINE_ARCH}
.if ${TARGET_ARCH} == "amd64" || ${TARGET_ARCH} == "arm" || \
    ${TARGET_ARCH} == "i386" || ${TARGET_ARCH} == "powerpc"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
