#	from @(#)Makefile.inc	8.2 (Berkeley) 2/21/94
#	$Id: Makefile.inc,v 1.2 1997/05/03 03:49:52 jb Exp $
#
CFLAGS+=-D__DBINTERFACE_PRIVATE

.include "${.CURDIR}/../libc/db/btree/Makefile.inc"
.include "${.CURDIR}/../libc/db/db/Makefile.inc"
.include "${.CURDIR}/../libc/db/hash/Makefile.inc"
.include "${.CURDIR}/../libc/db/man/Makefile.inc"
.include "${.CURDIR}/../libc/db/mpool/Makefile.inc"
.include "${.CURDIR}/../libc/db/recno/Makefile.inc"
