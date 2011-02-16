# $FreeBSD: src/gnu/usr.bin/gdb/gdb/Makefile,v 1.44.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	gdb${GDB_SUFFIX}
SRCS=	gdb.c

BULIBS= ${OBJ_BU}/libbfd/libbfd.a ${OBJ_BU}/libopcodes/libopcodes.a \
	${OBJ_BU}/libiberty/libiberty.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb.a

# libthread_db.so calls back into gdb for the proc services.  Make all the
# global symbols visible.
LDFLAGS+= -Wl,-E

DPADD=	${GDBLIBS} ${BULIBS} ${LIBM} ${LIBREADLINE} ${LIBTERMCAP} ${LIBGNUREGEX}
LDADD=	${GDBLIBS} ${BULIBS} -lm -lreadline -ltermcap -lgnuregex

.include <bsd.prog.mk>
