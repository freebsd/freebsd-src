#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/hash/Makefile.inc,v 1.3 1999/08/27 23:58:18 peter Exp $

.PATH: ${.CURDIR}/../libc/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c hsearch.c ndbm.c
