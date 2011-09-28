# $FreeBSD$

LIB=	dwarf

SRCS=	\
	dwarf_abbrev.c		\
	dwarf_attr.c		\
	dwarf_attrval.c		\
	dwarf_cu.c		\
	dwarf_dealloc.c		\
	dwarf_die.c		\
	dwarf_dump.c		\
	dwarf_errmsg.c		\
	dwarf_errno.c		\
	dwarf_finish.c		\
	dwarf_form.c		\
	dwarf_func.c		\
	dwarf_init.c		\
	dwarf_loc.c

INCS=	dwarf.h libdwarf.h

CFLAGS+=	-I${.CURDIR}

SHLIB_MAJOR=	3

WITHOUT_MAN=

.include <bsd.lib.mk>
