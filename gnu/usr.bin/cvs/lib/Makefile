# $FreeBSD$

MAINTAINER=	peter@FreeBSD.org

.include "${.CURDIR}/../Makefile.inc"

.PATH: ${CVSDIR}/src
.PATH: ${CVSDIR}/lib
.PATH: ${CVSDIR}/man

LIB=		cvs
INTERNALLIB=	YES

CFLAGS+= -I. -I${CVSDIR}/src -I${CVSDIR}/lib
CFLAGS+= -DHAVE_CONFIG_H
YFLAGS=
CLEANFILES+=	config.h

CVS_UMASK_DFLT?=	002
CVS_ADMIN_GROUP?=	cvsadmin
CVS_TMPDIR_DFLT?=	/tmp

SRCS=	config.h argmatch.c getdate.y getline.c \
	getopt.c getopt1.c savecwd.c \
	sighandle.c stripslash.c \
	xgetwd.c yesno.c

config.h: config.h.proto
	version=`sed < ${CVSDIR}/configure \
	    -e '/^[ 	]*VERSION=/!d' -e 's/.*=\(.*\)/\1/' -e q`; \
	sed -e "s,@VERSION@,$${version}-FreeBSD,g" \
	    -e "s,@UMASK_DFLT@,${CVS_UMASK_DFLT},g" \
	    -e "s,@TMPDIR_DFLT@,${CVS_TMPDIR_DFLT},g" \
	    -e "s,@CVS_ADMIN_GROUP@,${CVS_ADMIN_GROUP},g" \
	    ${.ALLSRC} > ${.TARGET}

.include <bsd.lib.mk>
