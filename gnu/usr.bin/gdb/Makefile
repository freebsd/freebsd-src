# $FreeBSD$

SUBDIR=	doc libgdb gdb gdbtui kgdb

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "arm"
SUBDIR+=gdbserver
.endif

.include <bsd.subdir.mk>
