#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
#	$Id: Makefile.inc,v 1.5 1997/09/24 20:38:10 julian Exp $

.PATH: ${.CURDIR}/../libc/db/man

.if ${LIB} == "c"
# mpool.3
MAN3+=	btree.3 dbopen.3 hash.3 recno.3
MAN3+=	mpool.3
MLINKS+= dbopen.3 db.3
.endif
