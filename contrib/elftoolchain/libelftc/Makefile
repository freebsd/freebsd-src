# $Id: Makefile 2859 2013-01-05 09:21:54Z jkoshy $

TOP=	${.CURDIR}/..

LIB=	elftc

SRCS=	elftc_bfdtarget.c			\
	elftc_copyfile.c			\
	elftc_demangle.c			\
	elftc_set_timestamps.c			\
	elftc_string_table.c			\
	elftc_version.c				\
	libelftc_bfdtarget.c			\
	libelftc_dem_arm.c			\
	libelftc_dem_gnu2.c			\
	libelftc_dem_gnu3.c			\
	libelftc_hash.c				\
	libelftc_vstr.c

INCS=		libelftc.h
INCSDIR=	/usr/include

RELEASE=	HEAD		# Change this on release branches.

SHLIB_MAJOR=	1

WARNS?=	6

CLEANFILES+=	elftc_version.c

LDADD+=	-lelf

MAN=	elftc.3 \
	elftc_bfd_find_target.3 \
	elftc_copyfile.3 \
	elftc_demangle.3 \
	elftc_set_timestamps.3 \
	elftc_string_table_create.3 \
	elftc_version.3

MLINKS=	elftc_bfd_find_target.3 elftc_bfd_target_byteorder.3 \
	elftc_bfd_find_target.3 elftc_bfd_target_class.3 \
	elftc_bfd_find_target.3 elftc_bfd_target_flavor.3 \
	elftc_string_table_create.3 elftc_string_table_from_section.3 \
	elftc_string_table_create.3 elftc_string_table_destroy.3 \
	elftc_string_table_create.3 elftc_string_table_image.3 \
	elftc_string_table_create.3 elftc_string_table_insert.3 \
	elftc_string_table_create.3 elftc_string_table_lookup.3 \
	elftc_symbol_table_create.3 elftc_symbol_table_create_nested.3 \
	elftc_symbol_table_create.3 elftc_symbol_table_delete_name.3 \
	elftc_symbol_table_create.3 elftc_symbol_table_delete_entry.3 \
	elftc_symbol_table_create.3 elftc_symbol_table_destroy.3 \
	elftc_symbol_table_create.3 elftc_symbol_table_from_section.3 \
	elftc_symbol_table_create.3 elftc_symbol_table_insert.3 \
	elftc_symbol_table_create.3 elftc_symbol_table_iterate.3 \
	elftc_symbol_table_create.3 elftc_symbol_table_lookup.3 \
	elftc_symbol_table_create.3 elftc_symbol_table_to_image.3

.if	!make(clean) && !make(clobber)
.BEGIN:	.SILENT
	${.CURDIR}/make-toolchain-version -t ${TOP} -r ${RELEASE} \
	    -h ${OS_HOST}
.endif

.include "${TOP}/mk/elftoolchain.lib.mk"
