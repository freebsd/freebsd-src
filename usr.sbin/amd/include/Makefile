# ex:ts=8
#
# $FreeBSD$
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998.
#	Portions derived from amd/libamu/Makefile
#	$NetBSD: Makefile,v 1.8 1998/08/08 22:33:37 christos Exp $

SRCS=	config_local.h
CLEANFILES=	${SRCS}

all depend: ${SRCS}

config_local.h: newvers.sh
	@rm -f ${.TARGET}
	sh ${.ALLSRC} ${.CURDIR}/../../../sys/conf/newvers.sh > ${.TARGET}

.include <bsd.prog.mk>
