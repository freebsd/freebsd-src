#	from @(#)Makefile.inc	8.2 (Berkeley) 2/21/94
# $FreeBSD: src/lib/libc/db/Makefile.inc,v 1.4.26.1 2008/10/02 02:57:24 kensmith Exp $
#
CFLAGS+=-D__DBINTERFACE_PRIVATE

.include "${.CURDIR}/db/btree/Makefile.inc"
.include "${.CURDIR}/db/db/Makefile.inc"
.include "${.CURDIR}/db/hash/Makefile.inc"
.include "${.CURDIR}/db/man/Makefile.inc"
.include "${.CURDIR}/db/mpool/Makefile.inc"
.include "${.CURDIR}/db/recno/Makefile.inc"
