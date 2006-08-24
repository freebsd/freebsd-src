# $FreeBSD$

SUBDIR=	doc libgdb gdb gdbtui

.if ${MACHINE_ARCH} != "powerpc"
SUBDIR+= kgdb
.endif

.include <bsd.subdir.mk>
