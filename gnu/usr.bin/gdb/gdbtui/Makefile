# $FreeBSD$

PROG=	gdbtui
NOMAN=	yes
SRCS=	tui-main.c

BULIBS= ${OBJ_BU}/libbfd/libbfd.a ${OBJ_BU}/libopcodes/libopcodes.a	\
	${OBJ_BU}/libiberty/libiberty.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb.a

# libthread_db.so calls back into gdb for the proc services. Make all the
# global symbols visible.
LDFLAGS+= -Wl,-E

DPADD+= ${GDBLIBS} ${BULIBS} ${LIBREADLINE} ${LIBM}
LDADD+= ${GDBLIBS} ${BULIBS} -lreadline -lm

.include <bsd.prog.mk>
