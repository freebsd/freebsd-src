# $FreeBSD$
.include <bsd.own.mk>

PACKAGE=lib${LIB}
INTERNALLIB=

ELFTCDIR=	${SRCTOP}/contrib/elftoolchain

.PATH:	${ELFTCDIR}/libelftc

LIB=	elftc

SRCS=	elftc_bfdtarget.c                       \
	elftc_copyfile.c                        \
	elftc_demangle.c                        \
	elftc_reloc_type_str.c			\
	elftc_set_timestamps.c                  \
	elftc_string_table.c                    \
	elftc_version.c                         \
	libelftc_bfdtarget.c                    \
	libelftc_dem_arm.c                      \
	libelftc_dem_gnu2.c                     \
	libelftc_dem_gnu3.c                     \
	libelftc_hash.c                         \
	libelftc_vstr.c

INCS=	libelftc.h
CFLAGS+=-I${ELFTCDIR}/libelftc -I${ELFTCDIR}/common

MAN=

.include <bsd.lib.mk>
