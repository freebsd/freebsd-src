#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
#	$Id: Makefile.inc,v 1.2 1997/05/03 03:49:53 jb Exp $

.PATH: ${.CURDIR}/../libc/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c hsearch.c ndbm.c
