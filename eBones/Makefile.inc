#	From: @(#)Makefile.inc	5.1 (Berkeley) 6/25/90
#	$Id$

DISTRIBUTION=	krb

.if exists(${.CURDIR}/../../../secure/lib/libdes/obj)
DESOBJDIR=	${.CURDIR}/../../../secure/lib/libdes/obj
.else
DESOBJDIR=	${.CURDIR}/../../../secure/lib/libdes
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

.if exists(${.CURDIR}/../../lib/librkinit/obj)
RKINITOBJDIR= ${.CURDIR}/../../lib/librkinit/obj
.else
RKINITOBJDIR= ${.CURDIR}/../../lib/librkinit
.endif

CFLAGS+= -I${.CURDIR}/../../../secure/lib/libdes -I${.CURDIR}/../../include -Wall

COMPILE_ET=     compile_et

${KRBOBJDIR}/krb_err.h: ${.CURDIR}/../../lib/libkrb/krb_err.et          
	test -e ${KRBOBJDIR}/krb_err.et || ln -s ${.ALLSRC} ${KRBOBJDIR}
	cd ${KRBOBJDIR}; compile_et krb_err.et

CLEANFILES+=${KRBOBJDIR}/krb_err.h ${KRBOBJDIR}/krb_err.c

${KADMOBJDIR}/kadm_err.h: ${.CURDIR}/../../lib/libkadm/kadm_err.et          
	test -e ${KADMOBJDIR}/kadm_err.et || ln -s ${.ALLSRC} ${KADMOBJDIR}
	cd ${KADMOBJDIR}; compile_et kadm_err.et

CLEANFILES+=${KADMOBJDIR}/kadm_err.h ${KADMOBJDIR}/kadm_err.c

${RKINITOBJDIR}/rkinit_err.h: ${.CURDIR}/../../lib/librkinit/rkinit_err.et
	test -e ${RKINITOBJDIR}/rkinit_err.et || \
		ln -s ${.ALLSRC} ${RKINITOBJDIR}
	cd ${RKINITOBJDIR}; compile_et rkinit_err.et

CLEANFILES+=${RKINITOBJDIR}/rkinit_err.h ${RKINITOBJDIR}/rkinit_err.c
