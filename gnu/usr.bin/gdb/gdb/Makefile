# $FreeBSD: src/gnu/usr.bin/gdb/gdb/Makefile,v 1.42 2004/06/26 08:48:50 bde Exp $

PROG=	gdb
SRCS=	gdb.c

BULIBS= ${OBJ_BU}/libbfd/libbfd.a ${OBJ_BU}/libopcodes/libopcodes.a \
	${OBJ_BU}/libiberty/libiberty.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb.a

# libthread_db.so calls back into gdb for the proc services.  Make all the
# global symbols visible.
LDFLAGS+= -Wl,-E

DPADD=	${GDBLIBS} ${BULIBS} ${LIBM} ${LIBREADLINE} ${LIBTERMCAP}
LDADD=	${GDBLIBS} ${BULIBS} -lm -lreadline -ltermcap

.include <bsd.prog.mk>
