#	From: @(#)Makefile.inc	5.1 (Berkeley) 6/25/90
#	$Id: Makefile.inc,v 1.1.1.1 1994/09/30 14:49:48 csgr Exp $

BINDIR?=	/usr/sbin
SHLIB_MAJOR?=	2
SHLIB_MINOR?=	0
DISTRIBUTION=	krb

.if exists(${.CURDIR}/../des/obj)
DESOBJDIR=	${.CURDIR}/../des/obj
.else
DESOBJDIR=	${.CURDIR}/../des
.endif

.if exists(${.CURDIR}/../krb/obj)
KRBOBJDIR=	${.CURDIR}/../krb/obj
.else
KRBOBJDIR=	${.CURDIR}/../krb
.endif

.if exists(${.CURDIR}/../kdb/obj)
KDBOBJDIR=	${.CURDIR}/../kdb/obj
.else
KDBOBJDIR=	${.CURDIR}/../kdb
.endif

.if exists(${.CURDIR}/../acl/obj)
ACLOBJDIR=	${.CURDIR}/../acl/obj
.else
ACLOBJDIR=	${.CURDIR}/../acl
.endif

.if exists(${.CURDIR}/../compile_et/obj)
COMPILE_ET=     ${.CURDIR}/../compile_et/obj/compile_et
.else
COMPILE_ET=     ${.CURDIR}/../compile_et/compile_et
.endif

