# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	libgdb gdb kgdb

.if ${MK_GDB_LIBEXEC} == "no"
SUBDIR+= gdbtui

.if exists(${.CURDIR}/gdbserver/reg-${MACHINE_CPUARCH}.c)
SUBDIR+=gdbserver
.endif
.endif

.include <bsd.subdir.mk>
