#	@(#)Makefile.inc	8.2 (Berkeley) 2/21/94
#
CFLAGS+=-D__DBINTERFACE_PRIVATE

.include "${.CURDIR}/db/btree/Makefile.inc"
.include "${.CURDIR}/db/db/Makefile.inc"
.include "${.CURDIR}/db/hash/Makefile.inc"
.include "${.CURDIR}/db/mpool/Makefile.inc"
.include "${.CURDIR}/db/recno/Makefile.inc"
