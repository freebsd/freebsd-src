#	$FreeBSD: src/usr.sbin/makefs/compat/Makefile.inc,v 1.1.2.2.2.1 2012/03/03 06:15:13 kensmith Exp $
#

.PATH:	${.CURDIR}/compat

CFLAGS+=	-I${.CURDIR}/compat

SRCS+=	pwcache.c strsuftoll.c
