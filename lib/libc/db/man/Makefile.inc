#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

.PATH: ${.CURDIR}/../libc/db/man

.if ${LIB} == "c"
MAN3+=	btree.3 dbopen.3 hash.3 mpool.3 recno.3

MLINKS+= dbopen.3 db.3
.endif
