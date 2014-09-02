# $FreeBSD$

PROG=	gdbtui${GDB_SUFFIX}
SRCS=	tui-main.c
MAN=

BULIBS= ${OBJ_BU}/libbfd/libbfd.a ${OBJ_BU}/libopcodes/libopcodes.a \
	${OBJ_BU}/libiberty/libiberty.a
GDBLIBS= ${OBJ_GDB}/libgdb/libgdb.a

# libthread_db.so calls back into gdb for the proc services.  Make all the
# global symbols visible.
LDFLAGS+= -Wl,-E

DPADD=	${GDBLIBS} ${BULIBS} ${LIBM} ${LIBREADLINE} ${LIBTERMCAPW} ${LIBGNUREGEX}
LDADD=	${GDBLIBS} ${BULIBS} -lm ${LDREADLINE} -ltermcapw -lgnuregex

NO_PIE=	yes

.include <bsd.prog.mk>
