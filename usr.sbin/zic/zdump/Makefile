#	$FreeBSD$

.PATH:	${.CURDIR}/..

PROG=	zdump

SRCS=	zdump.c ialloc.c scheck.c
MAN8=	${.CURDIR}/../zdump.8
CFLAGS+= -I${.CURDIR}/.. -I${.CURDIR}/../../../lib/libc/stdtime
CFLAGS+= -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone -DSTD_INSPIRED -DPCTS
CFLAGS+= -DHAVE_LONG_DOUBLE -DTZDIR=\"/usr/share/zoneinfo\" -Demkdir=mkdir

.include <bsd.prog.mk>
