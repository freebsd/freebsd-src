#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/plugins/kdb/db2/libdb2/recno

SRCS+=	rec_close.c \
	rec_delete.c \
	rec_get.c \
	rec_open.c \
	rec_put.c \
	rec_search.c \
	rec_seq.c \
	rec_utils.c
