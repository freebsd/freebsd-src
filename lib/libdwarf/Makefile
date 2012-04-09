# $FreeBSD: src/lib/libdwarf/Makefile,v 1.2.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

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

WARNS?=	6

WITHOUT_MAN=	yes

.include <bsd.lib.mk>
