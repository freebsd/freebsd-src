#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
#	$Id: Makefile.inc,v 1.6 1997/10/16 14:26:09 bde Exp $

.PATH: ${.CURDIR}/../libc/db/man

.if ${LIB} == "c"
MAN3+=	btree.3 dbopen.3 hash.3 mpool.3 recno.3

MLINKS+= dbopen.3 db.3
.endif
