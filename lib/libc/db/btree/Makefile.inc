#	from @(#)Makefile.inc	8.2 (Berkeley) 7/14/94
# $FreeBSD: src/lib/libc/db/btree/Makefile.inc,v 1.4.38.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/db/btree

SRCS+=	bt_close.c bt_conv.c bt_debug.c bt_delete.c bt_get.c bt_open.c \
	bt_overflow.c bt_page.c bt_put.c bt_search.c bt_seq.c bt_split.c \
	bt_utils.c
