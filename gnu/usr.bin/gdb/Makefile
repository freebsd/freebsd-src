# $FreeBSD$

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "powerpc"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
