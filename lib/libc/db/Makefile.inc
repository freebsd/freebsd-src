#	from @(#)Makefile.inc	8.2 (Berkeley) 2/21/94
#	$Id: Makefile.inc,v 1.1.1.1.8.1 1997/06/24 03:51:27 julian Exp $
#
CFLAGS+=-D__DBINTERFACE_PRIVATE

.include "${.CURDIR}/../libc/db/btree/Makefile.inc"
.include "${.CURDIR}/../libc/db/db/Makefile.inc"
.include "${.CURDIR}/../libc/db/hash/Makefile.inc"
.include "${.CURDIR}/../libc/db/man/Makefile.inc"
.include "${.CURDIR}/../libc/db/mpool/Makefile.inc"
.include "${.CURDIR}/../libc/db/recno/Makefile.inc"
