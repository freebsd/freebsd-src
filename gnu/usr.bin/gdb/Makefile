# $FreeBSD$

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "arm" || ${MACHINE_CPUARCH} == "powerpc"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
