#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.3 1994/09/09 21:43:41 g89r4222 Exp $

SHLIB_MAJOR= 2
SHLIB_MINOR= 0

LIB=	kdb
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../include
SRCS=	krb_cache.c krb_dbm.c krb_kdb_utils.c krb_lib.c print_princ.c

.include <bsd.lib.mk>
