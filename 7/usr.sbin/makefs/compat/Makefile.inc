#	$FreeBSD$
#

.PATH:	${.CURDIR}/compat

CFLAGS+=	-I${.CURDIR}/compat

SRCS+=	pwcache.c strsuftoll.c
