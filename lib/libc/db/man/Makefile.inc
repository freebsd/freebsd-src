#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
#	$Id: Makefile.inc,v 1.3.6.1 1997/06/24 03:51:29 julian Exp $


.PATH: ${.CURDIR}/../libc/db/man

# Only build man pages with libc.
.if ${LIB} == "c"
# mpool.3
MAN3+=	db/man/btree.3 db/man/dbopen.3 db/man/hash.3 db/man/recno.3
MAN3+=	db/man/mpool.3
MLINKS+= dbopen.3 db.3
.endif
