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
	dwarf_init.c		\
	dwarf_loc.c

INCS=	dwarf.h libdwarf.h

CFLAGS+=	-I. -I${.CURDIR}

SHLIB_MAJOR=	2

WITHOUT_MAN=	yes

.include <bsd.lib.mk>
