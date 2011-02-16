# $FreeBSD: src/lib/libdwarf/Makefile,v 1.1.2.1.8.1 2010/12/21 17:10:29 kensmith Exp $

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

SHLIB_MAJOR=	1

WARNS?=	6

WITHOUT_MAN=	yes

.include <bsd.lib.mk>
