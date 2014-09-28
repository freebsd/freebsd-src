# $FreeBSD$

SHLIBDIR?= /lib

.include <src.opts.mk>

LIB=	nv
SHLIB_MAJOR= 0

SRCS=	dnvlist.c
SRCS+=	msgio.c
SRCS+=	nvlist.c
SRCS+=	nvpair.c

INCS=	dnv.h
INCS+=	nv.h

MAN+=	nv.3

MLINKS+=nv.3 libnv.3 \
	nv.3 nvlist.3
MLINKS+=nv.3 nvlist_create.3 \
	nv.3 nvlist_destroy.3 \
	nv.3 nvlist_error.3 \
	nv.3 nvlist_empty.3 \
	nv.3 nvlist_clone.3 \
	nv.3 nvlist_dump.3 \
	nv.3 nvlist_fdump.3 \
	nv.3 nvlist_size.3 \
	nv.3 nvlist_pack.3 \
	nv.3 nvlist_unpack.3 \
	nv.3 nvlist_send.3 \
	nv.3 nvlist_recv.3 \
	nv.3 nvlist_xfer.3 \
	nv.3 nvlist_next.3 \
	nv.3 nvlist_exists.3 \
	nv.3 nvlist_exists_type.3 \
	nv.3 nvlist_exists_null.3 \
	nv.3 nvlist_exists_bool.3 \
	nv.3 nvlist_exists_number.3 \
	nv.3 nvlist_exists_string.3 \
	nv.3 nvlist_exists_nvlist.3 \
	nv.3 nvlist_exists_descriptor.3 \
	nv.3 nvlist_exists_binary.3 \
	nv.3 nvlist_add_null.3 \
	nv.3 nvlist_add_bool.3 \
	nv.3 nvlist_add_number.3 \
	nv.3 nvlist_add_string.3 \
	nv.3 nvlist_add_stringf.3 \
	nv.3 nvlist_add_stringv.3 \
	nv.3 nvlist_add_nvlist.3 \
	nv.3 nvlist_add_descriptor.3 \
	nv.3 nvlist_add_binary.3 \
	nv.3 nvlist_move_string.3 \
	nv.3 nvlist_move_nvlist.3 \
	nv.3 nvlist_move_descriptor.3 \
	nv.3 nvlist_move_binary.3 \
	nv.3 nvlist_get_bool.3 \
	nv.3 nvlist_get_number.3 \
	nv.3 nvlist_get_string.3 \
	nv.3 nvlist_get_nvlist.3 \
	nv.3 nvlist_get_descriptor.3 \
	nv.3 nvlist_get_binary.3 \
	nv.3 nvlist_take_bool.3 \
	nv.3 nvlist_take_number.3 \
	nv.3 nvlist_take_string.3 \
	nv.3 nvlist_take_nvlist.3 \
	nv.3 nvlist_take_descriptor.3 \
	nv.3 nvlist_take_binary.3 \
	nv.3 nvlist_free.3 \
	nv.3 nvlist_free_type.3 \
	nv.3 nvlist_free_null.3 \
	nv.3 nvlist_free_bool.3 \
	nv.3 nvlist_free_number.3 \
	nv.3 nvlist_free_string.3 \
	nv.3 nvlist_free_nvlist.3 \
	nv.3 nvlist_free_descriptor.3 \
	nv.3 nvlist_free_binary.3
MLINKS+=nv.3 nvlist_existsf.3 \
	nv.3 nvlist_existsf_type.3 \
	nv.3 nvlist_existsf_null.3 \
	nv.3 nvlist_existsf_bool.3 \
	nv.3 nvlist_existsf_number.3 \
	nv.3 nvlist_existsf_string.3 \
	nv.3 nvlist_existsf_nvlist.3 \
	nv.3 nvlist_existsf_descriptor.3 \
	nv.3 nvlist_existsf_binary.3 \
	nv.3 nvlist_addf_null.3 \
	nv.3 nvlist_addf_bool.3 \
	nv.3 nvlist_addf_number.3 \
	nv.3 nvlist_addf_string.3 \
	nv.3 nvlist_addf_nvlist.3 \
	nv.3 nvlist_addf_descriptor.3 \
	nv.3 nvlist_addf_binary.3 \
	nv.3 nvlist_movef_string.3 \
	nv.3 nvlist_movef_nvlist.3 \
	nv.3 nvlist_movef_descriptor.3 \
	nv.3 nvlist_movef_binary.3 \
	nv.3 nvlist_getf_bool.3 \
	nv.3 nvlist_getf_number.3 \
	nv.3 nvlist_getf_string.3 \
	nv.3 nvlist_getf_nvlist.3 \
	nv.3 nvlist_getf_descriptor.3 \
	nv.3 nvlist_getf_binary.3 \
	nv.3 nvlist_takef_bool.3 \
	nv.3 nvlist_takef_number.3 \
	nv.3 nvlist_takef_string.3 \
	nv.3 nvlist_takef_nvlist.3 \
	nv.3 nvlist_takef_descriptor.3 \
	nv.3 nvlist_takef_binary.3 \
	nv.3 nvlist_freef.3 \
	nv.3 nvlist_freef_type.3 \
	nv.3 nvlist_freef_null.3 \
	nv.3 nvlist_freef_bool.3 \
	nv.3 nvlist_freef_number.3 \
	nv.3 nvlist_freef_string.3 \
	nv.3 nvlist_freef_nvlist.3 \
	nv.3 nvlist_freef_descriptor.3 \
	nv.3 nvlist_freef_binary.3
MLINKS+=nv.3 nvlist_existsv.3 \
	nv.3 nvlist_existsv_type.3 \
	nv.3 nvlist_existsv_null.3 \
	nv.3 nvlist_existsv_bool.3 \
	nv.3 nvlist_existsv_number.3 \
	nv.3 nvlist_existsv_string.3 \
	nv.3 nvlist_existsv_nvlist.3 \
	nv.3 nvlist_existsv_descriptor.3 \
	nv.3 nvlist_existsv_binary.3 \
	nv.3 nvlist_addv_null.3 \
	nv.3 nvlist_addv_bool.3 \
	nv.3 nvlist_addv_number.3 \
	nv.3 nvlist_addv_string.3 \
	nv.3 nvlist_addv_nvlist.3 \
	nv.3 nvlist_addv_descriptor.3 \
	nv.3 nvlist_addv_binary.3 \
	nv.3 nvlist_movev_string.3 \
	nv.3 nvlist_movev_nvlist.3 \
	nv.3 nvlist_movev_descriptor.3 \
	nv.3 nvlist_movev_binary.3 \
	nv.3 nvlist_getv_bool.3 \
	nv.3 nvlist_getv_number.3 \
	nv.3 nvlist_getv_string.3 \
	nv.3 nvlist_getv_nvlist.3 \
	nv.3 nvlist_getv_descriptor.3 \
	nv.3 nvlist_getv_binary.3 \
	nv.3 nvlist_takev_bool.3 \
	nv.3 nvlist_takev_number.3 \
	nv.3 nvlist_takev_string.3 \
	nv.3 nvlist_takev_nvlist.3 \
	nv.3 nvlist_takev_descriptor.3 \
	nv.3 nvlist_takev_binary.3 \
	nv.3 nvlist_freef.3 \
	nv.3 nvlist_freev_type.3 \
	nv.3 nvlist_freev_null.3 \
	nv.3 nvlist_freev_bool.3 \
	nv.3 nvlist_freev_number.3 \
	nv.3 nvlist_freev_string.3 \
	nv.3 nvlist_freev_nvlist.3 \
	nv.3 nvlist_freev_descriptor.3 \
	nv.3 nvlist_freev_binary.3

WARNS?=	6

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
