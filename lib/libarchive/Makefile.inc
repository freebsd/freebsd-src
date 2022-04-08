# $FreeBSD$
# These defines are needed for both libarchive and the tests. If we don't keep
# them in sync we can get run-time crashes while running tests due to mismatches
# between structures such as archive_md5_ctx, etc.

LIBADD=	z bz2 lzma bsdxml zstd
CFLAGS+=	-DHAVE_BZLIB_H=1 -DHAVE_LIBLZMA=1 -DHAVE_LZMA_H=1 \
		-DHAVE_ZSTD_H=1 -DHAVE_LIBZSTD=1 -DHAVE_LIBZSTD_COMPRESSOR=1
CFLAGS+=	-DPLATFORM_CONFIG_H=\"${.CURDIR}/config_freebsd.h\"

.if ${MK_OPENSSL} != "no"
CFLAGS+=	-DWITH_OPENSSL
LIBADD+=	crypto
.else
LIBADD+=	md
.endif

.if ${MK_ICONV} != "no"
# TODO: This can be changed back to CFLAGS once iconv works correctly
# with statically linked binaries.
SHARED_CFLAGS+=	-DHAVE_ICONV=1 -DHAVE_ICONV_H=1 -DICONV_CONST=
.endif
