#	$Id$

.if !defined(DISTRIBUTION)

DISTRIBUTION=	krb

.if !defined(INCLUDEOBJDIR)

KRB4DIR=	${.CURDIR}/../../../crypto/kerberosIV
TELNETDIR=	${.CURDIR}/../../../crypto/telnet

.if exists(${.OBJDIR}/../../include)
INCLUDEOBJDIR=	${.OBJDIR}/../../include
.else
INCLUDEOBJDIR=	${.CURDIR}/../../include
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

.if exists(${.OBJDIR}/../../lib/libkafs)
KAFSOBJDIR=	${.OBJDIR}/../../lib/libkafs
.else
KAFSOBJDIR=	${.CURDIR}/../../lib/libkafs
.endif

.if exists(${.OBJDIR}/../../lib/libkdb)
KDBOBJDIR=	${.OBJDIR}/../../lib/libkdb
.else
KDBOBJDIR=	${.CURDIR}/../../lib/libkdb
.endif

.if exists(${.OBJDIR}/../../lib/libkrb)
KRBOBJDIR=	${.OBJDIR}/../../lib/libkrb
.else
KRBOBJDIR=	${.CURDIR}/../../lib/libkrb
.endif

.if exists(${.OBJDIR}/../../lib/libroken)
ROKENOBJDIR=	${.OBJDIR}/../../lib/libroken
.else
ROKENOBJDIR=	${.CURDIR}/../../lib/libroken
.endif

.if exists(${.OBJDIR}/../../lib/libsl)
SLOBJDIR=	${.OBJDIR}/../../lib/libsl
.else
SLOBJDIR=	${.CURDIR}/../../lib/libsl
.endif

.if exists(${.OBJDIR}/../../lib/libtelnet)
TELNETOBJDIR=	${.OBJDIR}/../../lib/libtelnet
.else
TELNETOBJDIR=	${.CURDIR}/../../lib/libtelnet
.endif

CFLAGS+= -I${.CURDIR}/../../include -Wall -DHAVE_CONFIG_H \
	-I${INCLUDEOBJDIR} -DBINDIR=\"/usr/bin\" -DSBINDIR=\"/usr/sbin\"

#${INCLUDEOBJDIR}/protos.h: ${KRB4DIR}/include/protos.H
#	cd ${INCLUDEOBJDIR} ; ln -sf ${.ALLSRC} ${.TARGET} 

COMPILE_ET=     compile_et

.ORDER: ${KADMOBJDIR}/kadm_err.c ${KADMOBJDIR}/kadm_err.h
${KADMOBJDIR}/kadm_err.c ${KADMOBJDIR}/kadm_err.h: ${KRB4DIR}/lib/kadm/kadm_err.et          
	test -e ${KADMOBJDIR}/kadm_err.et || ln -s ${.ALLSRC} ${KADMOBJDIR}
	cd ${KADMOBJDIR}; ${COMPILE_ET} kadm_err.et

CLEANFILES+=${KADMOBJDIR}/kadm_err.h ${KADMOBJDIR}/kadm_err.c ${KADMOBJDIR}/kadm_err.et

.ORDER: ${KRBOBJDIR}/krb_err.c ${KRBOBJDIR}/krb_err.h
${KRBOBJDIR}/krb_err.c ${KRBOBJDIR}/krb_err.h: ${KRB4DIR}/lib/krb/krb_err.et
	test -e ${KRBOBJDIR}/krb_err.et || ln -s ${.ALLSRC} ${KRBOBJDIR}
	cd ${KRBOBJDIR}; ${COMPILE_ET} krb_err.et

CLEANFILES+=${KRBOBJDIR}/krb_err.h ${KRBOBJDIR}/krb_err.c ${KRBOBJDIR}/krb_err.et

.if defined(INCLUDES)
beforeinstall: ${INCLUDES}
.for INC in ${INCLUDES}
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${INC} ${DESTDIR}/usr/include
.endfor
.endif

.else

KRB4DIR=	${.CURDIR}/../../crypto/kerberosIV

.endif

.endif
