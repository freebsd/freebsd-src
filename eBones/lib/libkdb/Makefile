#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id$

LIB=	kdb
CFLAGS+=-DKERBEROS -DDEBUG -I${.CURDIR}/../../include -Wall
SRCS=	krb_cache.c krb_dbm.c krb_kdb_utils.c krb_lib.c print_princ.c

.include <bsd.lib.mk>
