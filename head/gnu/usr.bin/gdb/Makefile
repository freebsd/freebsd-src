# $FreeBSD$

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if exists(${.CURDIR}/gdbserver/reg-${MACHINE_CPUARCH}.c)
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
