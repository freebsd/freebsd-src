# $FreeBSD$

.PATH:	${.CURDIR}/../../../contrib/opensolaris/lib/libuutil/common
.PATH:	${.CURDIR}/../../../sys/contrib/opensolaris/common/avl

LIB=	uutil
SHLIB_MAJOR?=1

SRCS=	avl.c
SRCS+=	uu_alloc.c
SRCS+=	uu_avl.c
SRCS+=	uu_dprintf.c
SRCS+=	uu_ident.c
SRCS+=	uu_list.c
SRCS+=	uu_misc.c
SRCS+=	uu_open.c
SRCS+=	uu_pname.c
SRCS+=	uu_strtoint.c

CFLAGS+=-DNATIVE_BUILD
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libuutil/common
CFLAGS+=-I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+=-I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/head

.include <bsd.lib.mk>
