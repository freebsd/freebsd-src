#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

# .PATH: ${KRB5_DIR}/lib/crypto/builtin

# SRCS+=cmac.c \
# 	hmac.c \
# 	kdf.c \
# 	pbkdf2.c

# .include "${KRB5_SRCTOP}/lib/crypto/builtin/camellia/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/crypto/builtin/aes/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/crypto/builtin/des/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/crypto/builtin/md4/Makefile.inc"
# .include "${KRB5_SRCTOP}/lib/crypto/builtin/md5/Makefile.inc"
# .include "${KRB5_SRCTOP}/lib/crypto/builtin/sha1/Makefile.inc"
# .include "${KRB5_SRCTOP}/lib/crypto/builtin/sha2/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/crypto/builtin/enc_provider/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/crypto/builtin/hash_provider/Makefile.inc"
