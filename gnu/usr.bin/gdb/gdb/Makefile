# $FreeBSD$

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

NO_PIE=	yes

.include <bsd.prog.mk>
CFLAGS+=	-DDEBUGDIR=\"${DEBUGDIR}\"
