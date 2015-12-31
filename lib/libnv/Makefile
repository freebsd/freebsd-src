# $FreeBSD$

SHLIBDIR?= /lib

.include <bsd.own.mk>

LIB=	nv
SHLIB_MAJOR= 0

.PATH: ${.CURDIR}/../../sys/kern ${.CURDIR}/../../sys/sys
CFLAGS+=-I${.CURDIR}/../../sys -I${.CURDIR}

SRCS=	subr_dnvlist.c
SRCS+=	msgio.c
SRCS+=	subr_nvlist.c
SRCS+=	subr_nvpair.c

INCS=	dnv.h
INCS+=	nv.h

MAN+=	nv.3

MLINKS+=nv.3 libnv.3 \
	nv.3 nvlist.3
MLINKS+=nv.3 nvlist_add_binary.3 \
	nv.3 nvlist_add_bool.3 \
	nv.3 nvlist_add_descriptor.3 \
	nv.3 nvlist_add_null.3 \
	nv.3 nvlist_add_number.3 \
	nv.3 nvlist_add_nvlist.3 \
	nv.3 nvlist_add_string.3 \
	nv.3 nvlist_add_stringf.3 \
	nv.3 nvlist_add_stringv.3 \
	nv.3 nvlist_clone.3 \
	nv.3 nvlist_create.3 \
	nv.3 nvlist_destroy.3 \
	nv.3 nvlist_dump.3 \
	nv.3 nvlist_empty.3 \
	nv.3 nvlist_error.3 \
	nv.3 nvlist_exists.3 \
	nv.3 nvlist_exists_binary.3 \
	nv.3 nvlist_exists_bool.3 \
	nv.3 nvlist_exists_descriptor.3 \
	nv.3 nvlist_exists_null.3 \
	nv.3 nvlist_exists_number.3 \
	nv.3 nvlist_exists_nvlist.3 \
	nv.3 nvlist_exists_string.3 \
	nv.3 nvlist_exists_type.3 \
	nv.3 nvlist_fdump.3 \
	nv.3 nvlist_flags.3 \
	nv.3 nvlist_free.3 \
	nv.3 nvlist_free_binary.3 \
	nv.3 nvlist_free_bool.3 \
	nv.3 nvlist_free_descriptor.3 \
	nv.3 nvlist_free_null.3 \
	nv.3 nvlist_free_number.3 \
	nv.3 nvlist_free_nvlist.3 \
	nv.3 nvlist_free_string.3 \
	nv.3 nvlist_free_type.3 \
	nv.3 nvlist_get_binary.3 \
	nv.3 nvlist_get_bool.3 \
	nv.3 nvlist_get_descriptor.3 \
	nv.3 nvlist_get_number.3 \
	nv.3 nvlist_get_nvlist.3 \
	nv.3 nvlist_get_parent.3 \
	nv.3 nvlist_get_string.3 \
	nv.3 nvlist_move_binary.3 \
	nv.3 nvlist_move_descriptor.3 \
	nv.3 nvlist_move_nvlist.3 \
	nv.3 nvlist_move_string.3 \
	nv.3 nvlist_next.3 \
	nv.3 nvlist_pack.3 \
	nv.3 nvlist_recv.3 \
	nv.3 nvlist_send.3 \
	nv.3 nvlist_set_error.3 \
	nv.3 nvlist_size.3 \
	nv.3 nvlist_take_binary.3 \
	nv.3 nvlist_take_bool.3 \
	nv.3 nvlist_take_descriptor.3 \
	nv.3 nvlist_take_number.3 \
	nv.3 nvlist_take_nvlist.3 \
	nv.3 nvlist_take_string.3 \
	nv.3 nvlist_unpack.3 \
	nv.3 nvlist_xfer.3

WARNS?=	6

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
