#	From: @(#)Makefile.inc	5.1 (Berkeley) 6/25/90
#	$Id: Makefile.inc,v 1.6 1995/09/07 21:36:27 markm Exp $

DISTRIBUTION=	krb

.if exists(${.CURDIR}/../../des/obj)
DESOBJDIR=	${.CURDIR}/../../des/obj
.else
DESOBJDIR=	${.CURDIR}/../../des
.endif

.if exists(${.CURDIR}/../../lib/libkrb/obj)
KRBOBJDIR=	${.CURDIR}/../../lib/libkrb/obj
.else
KRBOBJDIR=	${.CURDIR}/../../lib/libkrb
.endif

.if exists(${.CURDIR}/../../lib/libkdb/obj)
KDBOBJDIR=	${.CURDIR}/../../lib/libkdb/obj
.else
KDBOBJDIR=	${.CURDIR}/../../lib/libkdb
.endif

.if exists(${.CURDIR}/../../lib/libacl/obj)
ACLOBJDIR=	${.CURDIR}/../../lib/libacl/obj
.else
ACLOBJDIR=	${.CURDIR}/../../lib/libacl
.endif

.if exists(${.CURDIR}/../../lib/libkadm/obj)
KADMOBJDIR=	${.CURDIR}/../../lib/libkadm/obj
.else
KADMOBJDIR=	${.CURDIR}/../../lib/libkadm
.endif

CFLAGS+= -I${.CURDIR}/../../include -Wall

COMPILE_ET=     compile_et

${KRBOBJDIR}/krb_err.h: ${.CURDIR}/../../lib/libkrb/krb_err.et          
	test -e ${KRBOBJDIR}/krb_err.et || ln -s ${.IMPSRC} ${KRBOBJDIR}
	cd ${KRBOBJDIR}; compile_et krb_err.et

CLEANFILES+=${KRBOBJDIR}/krb_err.h ${KRBOBJDIR}/krb_err.c

${KADMOBJDIR}/kadm_err.h: ${.CURDIR}/../../lib/libkadm/kadm_err.et          
	test -e ${KADMOBJDIR}/kadm_err.et || ln -s ${.IMPSRC} ${KADMOBJDIR}
	cd ${KADMOBJDIR}; compile_et kadm_err.et

CLEANFILES+=${KADMOBJDIR}/kadm_err.h ${KADMOBJDIR}/kadm_err.c
