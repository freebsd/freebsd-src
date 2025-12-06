#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include <src.opts.mk>

.include "../Makefile.inc"

LIB=		k5crypto
# SHLIB_MAJOR=	3
LDFLAGS=-Wl,--no-undefined
LIBADD=	com_err krb5support crypto
VERSION_MAP=	${.CURDIR}/version.map

# XXX The following doesn't work. Even though the pathnames are the same
# XXX we need to use the alternative .include statements.
# .include "${KRB5_CRYPTOLIBDIR}/krb/Makefile.inc"
# .include "${KRB5_CRYPTOLIBDIR}/builtin/Makefile.inc"
# .include "${KRB5_CRYPTOLIBDIR}/openssl/Makefile.inc"

.include "${KRB5_SRCTOP}/lib/crypto/krb/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/crypto/builtin/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/crypto/openssl/Makefile.inc"

# Not normally configured to use the openssl provider
# .include "${KRB5_SRCTOP}/lib/crypto/openssl/Makefile.inc"

CFLAGS+=-I${.CURDIR:H:H}/include \
	-I${KRB5_DIR}/include \
	-I${KRB5_DIR}/lib/crypto \
	-I${KRB5_DIR}/lib/crypto/krb \
	-I${KRB5_DIR}/lib/crypto/builtin/aes \
	-I${KRB5_DIR}/lib/crypto/builtin/camellia \
	-I${KRB5_DIR}/lib/crypto/builtin/des \
	-I${KRB5_DIR}/lib/crypto/builtin/md4 \
	-I${KRB5_DIR}/lib/crypto/builtin/md5 \
	-I${KRB5_DIR}/lib/crypto/builtin/sha1 \
	-I${KRB5_DIR}/lib/crypto/builtin/sha2 \
	-I${SRCTOP}/crypto/openssl/include

.include <bsd.lib.mk>
