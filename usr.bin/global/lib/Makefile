# $Id: Makefile,v 1.2.2.1 1998/02/05 04:51:10 cwt Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/lib

LIB=		globalutil
SRCS=	dbio.c dbname.c find.c getdbpath.c gtagsopen.c locatestring.c \
	lookup.c makepath.c mgets.c strop.c tab.c tag.c test.c

NOPROFILE=		noprofile
INTERNALLIB=		true
INTERNALSTATICLIB=	true

.include <bsd.lib.mk>
