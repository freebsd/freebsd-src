# ex:ts=8
#
# Makefile for amd
# 	This file is under a "BSD" copyright (c) by David O'Brien 1998
#
# $FreeBSD: src/usr.sbin/amd/fixmount/Makefile,v 1.4 1999/08/28 01:15:14 peter Exp $
#

.PATH:		${.CURDIR}/../../../contrib/amd/fixmount \
		${.CURDIR}/../../../contrib/amd/conf/checkmount

PROG=		fixmount
MAN8=		fixmount.8

SRCS=		fixmount.c 

#		These are generated at compile time
SRCS+=		mount.h nfs_prot.h

#		These would be links created by the GNU-style configure
SRCS+=		checkmount_bsd44.c

DPADD=		${LIBAMU} ${LIBRPCSVC}
LDADD=		${LIBAMU} -lrpcsvc

.include <bsd.prog.mk>
