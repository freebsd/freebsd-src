# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: src/usr.sbin/amd/fixmount/Makefile,v 1.4.2.1 2001/04/25 12:09:13 ru Exp $
#

.PATH:		${.CURDIR}/../../../contrib/amd/fixmount \
		${.CURDIR}/../../../contrib/amd/conf/checkmount

PROG=		fixmount
MAN=		fixmount.8

SRCS=		fixmount.c 

#		These would be links created by the GNU-style configure
SRCS+=		checkmount_bsd44.c

DPADD=		${LIBAMU} ${LIBRPCSVC}
LDADD=		${LIBAMU} -lrpcsvc

.include <bsd.prog.mk>
