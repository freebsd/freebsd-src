#	From: @(#)Makefile.inc	5.1 (Berkeley) 6/25/90
#	$Id$

DISTRIBUTION=	krb

.if exists(${.CURDIR}/../../lib/libtelnet/obj)
TELNETOBJDIR=	${.CURDIR}/../../lib/libtelnet/obj
.else
TELNETOBJDIR=	${.CURDIR}/../../lib/libtelnet
.endif

.if exists(${.OBJDIR}/../../../secure/lib/libdes)
DESOBJDIR=	${.OBJDIR}/../../../secure/lib/libdes
.else
DESOBJDIR=	${.CURDIR}/../../../secure/lib/libdes
.endif

.if exists(${.OBJDIR}/../../lib/libkrb)
KRBOBJDIR=	${.OBJDIR}/../../lib/libkrb
.else
KRBOBJDIR=	${.CURDIR}/../../lib/libkrb
.endif

.if exists(${.OBJDIR}/../../lib/libkdb)
KDBOBJDIR=	${.OBJDIR}/../../lib/libkdb
.else
KDBOBJDIR=	${.CURDIR}/../../lib/libkdb
.endif

.if exists(${.OBJDIR}/../../lib/libacl)
ACLOBJDIR=	${.OBJDIR}/../../lib/libacl
.else
ACLOBJDIR=	${.CURDIR}/../../lib/libacl
.endif

.if exists(${.OBJDIR}/../../lib/libkadm)
KADMOBJDIR=	${.OBJDIR}/../../lib/libkadm
.else
KADMOBJDIR=	${.CURDIR}/../../lib/libkadm
.endif

.if exists(${.OBJDIR}/../../lib/librkinit)
RKINITOBJDIR= ${.OBJDIR}/../../lib/librkinit
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
