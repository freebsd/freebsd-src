#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH: ${KRB5_DIR}/lib/crypto/builtin/aes

SRCS+=	aescrypt.c \
	aeskey.c \
	aestab.c
