#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/hash/Makefile.inc,v 1.2.2.1 1999/08/29 14:45:54 peter Exp $

.PATH: ${.CURDIR}/../libc/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c hsearch.c ndbm.c
