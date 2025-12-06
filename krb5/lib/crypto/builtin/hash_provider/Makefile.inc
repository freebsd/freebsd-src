#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH: ${KRB5_DIR}/lib/crypto/builtin/hash_provider

SRCS+=	hash_md4.c \
	hash_md5.c \
	hash_sha1.c \
	hash_sha2.c
