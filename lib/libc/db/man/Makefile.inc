#       @(#)Makefile.inc	8.1 (Berkeley) 6/4/93

.PATH: ${.CURDIR}/db/man

# mpool.3
MAN3+=	db/man/btree.3 db/man/dbopen.3 db/man/hash.3 db/man/recno.3
MLINKS+= dbopen.3 db.3
