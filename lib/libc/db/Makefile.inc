#	from @(#)Makefile.inc	8.2 (Berkeley) 2/21/94
# $FreeBSD: src/lib/libc/db/Makefile.inc,v 1.3 1999/08/27 23:58:15 peter Exp $
#
CFLAGS+=-D__DBINTERFACE_PRIVATE

.include "${.CURDIR}/../libc/db/btree/Makefile.inc"
.include "${.CURDIR}/../libc/db/db/Makefile.inc"
.include "${.CURDIR}/../libc/db/hash/Makefile.inc"
.include "${.CURDIR}/../libc/db/man/Makefile.inc"
.include "${.CURDIR}/../libc/db/mpool/Makefile.inc"
.include "${.CURDIR}/../libc/db/recno/Makefile.inc"
