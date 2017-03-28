# $FreeBSD$

.include <src.opts.mk>

.if ${MK_CHERI_PURE} == "yes"
# linker error / uses sbrk
WANT_CHERI=	none
.endif

PROG=	gdbtui${GDB_SUFFIX}
SRCS=	tui-main.c
MAN=

# libthread_db.so calls back into gdb for the proc services.  Make all the
# global symbols visible.
LDFLAGS+= -Wl,-E

LIBADD+=	gdb bfd opcodes iberty m readline ncursesw gnuregex

.include <bsd.prog.mk>
