#	$NetBSD: Makefile.inc,v 1.2 2015/01/22 04:20:50 christos Exp $

WARNS=6
.if !defined(LIB)
LDADD+=	-lblacklist
DPADD+= ${LIBBLACKLIST}
.endif
CPPFLAGS+= -I${.CURDIR}/../include
CPPFLAGS+=-DHAVE_STRUCT_SOCKADDR_SA_LEN -DHAVE_UTIL_H -DHAVE_DB_H

