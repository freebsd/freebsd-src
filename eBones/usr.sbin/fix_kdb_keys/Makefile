#	From: @(#)Makefile	5.2 (Berkeley) 3/5/91
#	$FreeBSD$

PROG=   fix_kdb_keys
CFLAGS+=-DKERBEROS -DDEBUG
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN8=	fix_kdb_keys.8

.include <bsd.prog.mk>
