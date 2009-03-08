# $FreeBSD$

CFLAGS += -D__DBINTERFACE_PRIVATE

.PATH: ${.CURDIR}/db/btree

SRCS += \
	bt_close.c \
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

.PATH: ${.CURDIR}/db/db

SRCS += db.c

.PATH: ${.CURDIR}/db/hash

SRCS += \
	hash.c \
	hash_bigkey.c \
	hash_buf.c \
	hash_func.c \
	hash_log2.c \
	hash_page.c \
	ndbm.c

.PATH: ${.CURDIR}/db/mpool

SRCS +=	mpool.c

.PATH: ${.CURDIR}/db/recno

SRCS += \
	rec_close.c \
	rec_delete.c \
	rec_get.c \
	rec_open.c \
	rec_put.c \
	rec_search.c \
	rec_seq.c \
	rec_utils.c

SYM_MAPS += ${.CURDIR}/db/Symbol.map
