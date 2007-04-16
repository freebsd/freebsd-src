# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/opensolaris/lib/libuutil/common
.PATH: ${.CURDIR}/../../../sys/contrib/opensolaris/common/avl

LIB=	uutil
SRCS=	avl.c \
	uu_alloc.c \
	uu_avl.c \
	uu_dprintf.c \
	uu_ident.c \
	uu_list.c \
	uu_misc.c \
	uu_open.c \
	uu_pname.c \
	uu_strtoint.c

CFLAGS+= -DNATIVE_BUILD
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libuutil/common
CFLAGS+= -I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/head

.include <bsd.lib.mk>
