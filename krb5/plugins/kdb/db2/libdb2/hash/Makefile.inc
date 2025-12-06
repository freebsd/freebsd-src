#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/plugins/kdb/db2/libdb2/hash

SRCS+=	dbm.c \
	hash.c \
	hash_bigkey.c \
	hash_debug.c \
	hash_func.c \
	hash_log2.c \
	hash_page.c \
	hsearch.c
