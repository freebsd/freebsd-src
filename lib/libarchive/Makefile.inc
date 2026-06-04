# These defines are needed for both libarchive and the tests. If we don't keep
# them in sync we can get run-time crashes while running tests due to mismatches
# between structures such as archive_md5_ctx, etc.

_LIBARCHIVEDIR=	${SRCTOP}/contrib/libarchive
LIBARCHIVE_VERSION_ONLY_STRING!= \
	sed -n '/define.*ARCHIVE_VERSION_ONLY_STRING/{s,[^0-9.],,gp;q;}' \
	    ${LIBARCHIVEDIR}/libarchive/archive.h

LIBADD+=	z bz2 lzma bsdxml zstd
CFLAGS+=	-DPLATFORM_CONFIG_H=\"config_freebsd.h\"
CFLAGS+=	-I${SRCTOP}/lib/libarchive
CFLAGS+=	-I${_LIBARCHIVEDIR}/libarchive
CFLAGS+=	-I${_LIBARCHIVEDIR}/libarchive_fe
CFLAGS+=	-I${SRCTOP}/sys/contrib/zstd/lib

.if ${MK_OPENSSL} != "no"
CFLAGS+=	-DWITH_OPENSSL
CFLAGS+=	-DOPENSSL_API_COMPAT=0x10100000L
LIBADD+=	crypto
CFLAGS.archive_hmac.c+=-Wno-error=cast-qual
.else
LIBADD+=	md
.endif

.if ${MK_ICONV} != "no"
# TODO: This can be changed back to CFLAGS once iconv works correctly
# with statically linked binaries.
SHARED_CFLAGS+=	-DHAVE_ICONV=1 -DHAVE_ICONV_H=1 -DICONV_CONST=
.endif
