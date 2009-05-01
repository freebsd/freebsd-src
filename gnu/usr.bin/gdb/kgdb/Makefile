# $FreeBSD: src/gnu/usr.bin/gdb/kgdb/Makefile,v 1.6.10.1.4.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	kgdb${GDB_SUFFIX}
SRCS=	main.c kld.c kthr.c trgt.c trgt_${TARGET_ARCH}.c
WARNS?=	2

BULIBS= ${OBJ_BU}/libbfd/libbfd.a ${OBJ_BU}/libopcodes/libopcodes.a \
	${OBJ_BU}/libiberty/libiberty.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb.a

DPADD=	${GDBLIBS} ${BULIBS} ${LIBKVM} ${LIBM} ${LIBREADLINE} ${LIBTERMCAP} ${LIBGNUREGEX}
LDADD=	${GDBLIBS} ${BULIBS} -lkvm -lm -lreadline -ltermcap -lgnuregex

.include <bsd.prog.mk>
