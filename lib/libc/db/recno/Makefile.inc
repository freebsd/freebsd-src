#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
#	$Id: Makefile.inc,v 1.1.1.1.8.1 1997/06/24 03:51:30 julian Exp $

.PATH: ${.CURDIR}/../libc/db/recno

SRCS+=	rec_close.c rec_delete.c rec_get.c rec_open.c rec_put.c rec_search.c \
	rec_seq.c rec_utils.c
