#       @(#)Makefile.inc	8.2 (Berkeley) 11/7/95

.PATH: ${.CURDIR}/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c hsearch.c dbm.c
