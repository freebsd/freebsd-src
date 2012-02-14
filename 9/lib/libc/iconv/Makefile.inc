# $FreeBSD$

# iconv sources
.PATH: ${.CURDIR}/iconv

MAN+=	iconv.3 iconvctl.3 iconv_canonicalize.3 iconvlist.3 __iconv_get_list.3
MLINKS+= iconv.3 iconv_open.3 \
	iconv.3 iconv_open_into.3 \
	iconv.3 iconv_close.3 \
	iconv.3 __iconv.3 \
	__iconv_get_list.3 __iconv_free_list.3
SRCS+=	citrus_bcs.c citrus_bcs_strtol.c citrus_bcs_strtoul.c \
	citrus_csmapper.c citrus_db.c citrus_db_factory.c citrus_db_hash.c \
	citrus_esdb.c citrus_hash.c citrus_iconv.c citrus_lookup.c \
	citrus_lookup_factory.c citrus_mapper.c citrus_memstream.c \
	citrus_mmap.c citrus_module.c citrus_none.c citrus_pivot_factory.c \
	citrus_prop.c citrus_stdenc.c iconv.c
SYM_MAPS+= ${.CURDIR}/iconv/Symbol.map
