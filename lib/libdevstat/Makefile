# $FreeBSD$

MAINTAINER=ken@FreeBSD.ORG

LIB=	devstat
SRCS=	devstat.c
INCS=	devstat.h

MAN=	devstat.3

MLINKS+=devstat.3 getnumdevs.3 
MLINKS+=devstat.3 getgeneration.3 
MLINKS+=devstat.3 getversion.3
MLINKS+=devstat.3 checkversion.3 
MLINKS+=devstat.3 getdevs.3 
MLINKS+=devstat.3 selectdevs.3
MLINKS+=devstat.3 buildmatch.3 
MLINKS+=devstat.3 compute_stats.3
MLINKS+=devstat.3 compute_etime.3

CFLAGS+=-I${.CURDIR}

.include <bsd.lib.mk>
