# Copyright (c) 2001-2003
#	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
#	All rights reserved.
# Author: Harti Brandt <brandt@fokus.gmd.de>
#
# $FreeBSD$

PROG=	atmconfig
SRCS=	main.c diag.c natm.c
MAN=	atmconfig.8

.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	9
.endif

FILES=	atmconfig.help
FILESDIR= /usr/share/doc/atm

.include <bsd.prog.mk>
