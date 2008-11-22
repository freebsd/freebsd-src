# ex:ts=8
#
# $FreeBSD$
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998.
#	Portions derived from amd/libamu/Makefile
#	$NetBSD: Makefile,v 1.8 1998/08/08 22:33:37 christos Exp $

SRCS=	config_local.h
.if defined(NO_NIS)
SRCS+=	localconfig.h
.endif
CLEANFILES=	${SRCS}

all depend: ${SRCS}

config_local.h: newvers.sh
	@rm -f ${.TARGET}
	sh ${.ALLSRC} ${.CURDIR}/../../../sys/conf/newvers.sh > ${.TARGET}

localconfig.h:
	@rm -f ${.TARGET}
	@echo "/* NIS disabled by NO_NIS make.conf option */" >> ${.TARGET}
	@echo "#undef HAVE_MAP_NIS" >> ${.TARGET}

.include <bsd.prog.mk>
