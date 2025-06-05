#       @(#)Makefile.inc	8.2 (Berkeley) 11/14/94

.PATH: ${.CURDIR}/db/man

MAN3+=	db_btree.0 db_hash.0 db_lock.0 db_log.0 db_mpool.0 db_open.0 \
	db_recno.0
MLINKS+=db_open.3 db.3 db_open.3 dbopen.3
