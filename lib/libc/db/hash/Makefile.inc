#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/hash/Makefile.inc,v 1.5.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c ndbm.c
