# $Id: Makefile,v 1.6 2003/10/24 15:41:26 simokawa Exp $
# $FreeBSD: src/sys/modules/dcons/Makefile,v 1.2 2004/07/13 09:37:49 simokawa Exp $

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons
SRCS	= dcons.c dcons.h \
		opt_dcons.h opt_ddb.h opt_comconsole.h

opt_ddb.h:
	echo "#define KDB 1" > $@

opt_comconsole.h:
	echo "#define ALT_BREAK_TO_DEBUGGER 1" > $@

CFLAGS+= -I${.CURDIR}/../.. 

.include <bsd.kmod.mk>
