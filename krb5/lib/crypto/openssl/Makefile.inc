#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH: ${KRB5_DIR}/lib/crypto/openssl

SRCS+=	cmac.c \
	hmac.c \
	kdf.c \
	pbkdf2.c \
	sha256.c


.include "${KRB5_SRCTOP}/lib/crypto/openssl/des/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/crypto/openssl/enc_provider/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/crypto/openssl/hash_provider/Makefile.inc"
