# $FreeBSD: src/gnu/usr.bin/gdb/kgdb/Makefile,v 1.1.2.1 2005/03/01 20:27:45 obrien Exp $

PROG=	kgdb
SRCS=	kthr.c main.c trgt.c trgt_${MACHINE_ARCH}.c
WARNS?=	2

BULIBS= ${OBJ_BU}/libbfd/libbfd.a ${OBJ_BU}/libopcodes/libopcodes.a \
	${OBJ_BU}/libiberty/libiberty.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb.a

DPADD=	${GDBLIBS} ${BULIBS} ${LIBKVM} ${LIBM} ${LIBREADLINE} ${LIBTERMCAP} ${LIBGNUREGEX}
LDADD=	${GDBLIBS} ${BULIBS} -lkvm -lm -lreadline -ltermcap -lgnuregex

.include <bsd.prog.mk>
