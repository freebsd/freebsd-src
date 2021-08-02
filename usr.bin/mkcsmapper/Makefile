# $FreeBSD$

.PATH: ${SRCTOP}/lib/libc/iconv

PROG=	mkcsmapper
.if defined(BOOTSTRAPPING)
# When bootstrapping, the host libc might not contain these expected symbols
# so compile them directly into the binary.
SRCS=	citrus_bcs.c citrus_db_factory.c citrus_db_hash.c \
	citrus_lookup_factory.c citrus_pivot_factory.c
.endif
LDFLAGS+= -L${SRCTOP}/lib/libc

NO_WMISSING_VARIABLE_DECLARATIONS=

.include "${.CURDIR}/Makefile.inc"
.include <bsd.prog.mk>
