#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/plugins/kdb/db2/libdb2/btree

SRCS+=	bt_close.c \
	bt_conv.c \
	bt_debug.c \
	bt_delete.c \
	bt_get.c \
	bt_open.c \
	bt_overflow.c \
	bt_page.c \
	bt_put.c \
	bt_search.c \
	bt_seq.c \
	bt_split.c \
	bt_utils.c
