# $FreeBSD$

PROG=	gdb${GDB_SUFFIX}
SRCS=	gdb.c

BULIBS= ${OBJ_BU}/libbfd/libbfd${PIE_SUFFIX}.a \
	${OBJ_BU}/libopcodes/libopcodes${PIE_SUFFIX}.a \
	${OBJ_BU}/libiberty/libiberty${PIE_SUFFIX}.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb${PIE_SUFFIX}.a

# libthread_db.so calls back into gdb for the proc services.  Make all the
# global symbols visible.
LDFLAGS+= -Wl,-E

DPADD=	${GDBLIBS} ${BULIBS}
LDADD=	${GDBLIBS} ${BULIBS}
LIBADD+=	m edit ncursesw gnuregex

.include <bsd.prog.mk>
CFLAGS+=	-DDEBUGDIR=\"${DEBUGDIR}\"
