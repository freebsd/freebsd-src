#	from @(#)Makefile.inc	8.2 (Berkeley) 2/21/94
# $FreeBSD: src/lib/libc/db/Makefile.inc,v 1.4 2002/11/18 09:50:54 ru Exp $
#
CFLAGS+=-D__DBINTERFACE_PRIVATE

.include "${.CURDIR}/db/btree/Makefile.inc"
.include "${.CURDIR}/db/db/Makefile.inc"
.include "${.CURDIR}/db/hash/Makefile.inc"
.include "${.CURDIR}/db/man/Makefile.inc"
.include "${.CURDIR}/db/mpool/Makefile.inc"
.include "${.CURDIR}/db/recno/Makefile.inc"
