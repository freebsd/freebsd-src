# $FreeBSD$

DISTRIBUTION?=	krb5

.if !defined(INCLUDEOBJDIR)

KRB4DIR=	${.CURDIR}/../../../crypto/kerberosIV
KRB5DIR=	${.CURDIR}/../../../crypto/heimdal
ROKENDIR=	${.CURDIR}/../../lib/libroken
TELNETDIR=	${.CURDIR}/../../../crypto/telnet

CFLAGS+=-Wall -I${INCLUDEOBJDIR} -DHAVE_CONFIG_H

.if defined(MAKE_KERBEROS4)
CFLAGS+=-DKRB5_KRB4_COMPAT -DKRB4
.endif

CFLAGS+=-DINET6

.if exists(${.OBJDIR}/../../include)
INCLUDEOBJDIR=	${.OBJDIR}/../../include
.else
INCLUDEOBJDIR=	${.CURDIR}/../../include
.endif

.if exists(${.OBJDIR}/../../lib/libasn1)
ASN1OBJDIR=	${.OBJDIR}/../../lib/libasn1
.else
ASN1OBJDIR=	${.CURDIR}/../../lib/libasn1
.endif

.if exists(${.OBJDIR}/../../lib/libhdb)
HDBOBJDIR=	${.OBJDIR}/../../lib/libhdb
.else
HDBOBJDIR=	${.CURDIR}/../../lib/libhdb
.endif

.if exists(${.OBJDIR}/../../lib/libroken)
ROKENOBJDIR=	${.OBJDIR}/../../lib/libroken
.else
ROKENOBJDIR=	${.CURDIR}/../../lib/libroken
.endif

.if exists(${.OBJDIR}/../../lib/libvers)
VERSOBJDIR=	${.OBJDIR}/../../lib/libvers
.else
VERSOBJDIR=	${.CURDIR}/../../lib/libvers
.endif

.if exists(${.OBJDIR}/../../lib/libkrb4)
KRB4OBJDIR=	${.OBJDIR}/../../lib/libkrb4
.else
KRB4OBJDIR=	${.CURDIR}/../../lib/libkrb4
.endif

.if exists(${.OBJDIR}/../../lib/libkrb5)
KRB5OBJDIR=	${.OBJDIR}/../../lib/libkrb5
.else
KRB5OBJDIR=	${.CURDIR}/../../lib/libkrb5
.endif

.if exists(${.OBJDIR}/../../lib/libkadm5clnt)
KADM5COBJDIR=	${.OBJDIR}/../../lib/libkadm5clnt
.else
KADM5COBJDIR=	${.CURDIR}/../../lib/libkadm5clnt
.endif

.if exists(${.OBJDIR}/../../lib/libkadm5srv)
KADM5SOBJDIR=	${.OBJDIR}/../../lib/libkadm5srv
.else
KADM5SOBJDIR=	${.CURDIR}/../../lib/libkadm5srv
.endif

.if exists(${.OBJDIR}/../../lib/libkafs5)
KAFS5OBJDIR=	${.OBJDIR}/../../lib/libkafs5
.else
KAFS5OBJDIR=	${.CURDIR}/../../lib/libkafs5
.endif

.if exists(${.OBJDIR}/../../lib/libkrb5util)
KRB5UTILOBJDIR=	${.OBJDIR}/../../lib/libkrb5util
.else
KRB5UTILOBJDIR=	${.CURDIR}/../../lib/libkrb5util
.endif

.if exists(${.OBJDIR}/../../lib/libkdb5)
KDB5OBJDIR=	${.OBJDIR}/../../lib/libkdb5
.else
KDB5OBJDIR=	${.CURDIR}/../../lib/libkdb5
.endif

.if exists(${.OBJDIR}/../../lib/libsl)
SLOBJDIR=	${.OBJDIR}/../../lib/libsl
.else
SLOBJDIR=	${.CURDIR}/../../lib/libsl
.endif

.if exists(${.OBJDIR}/../../lib/libtelnet)
LIBTELNET=	${.OBJDIR}/../../lib/libtelnet/libtelnet.a
.else
LIBTELNET=	${.CURDIR}/../../lib/libtelnet/libtelnet.a
.endif

COMPILE_ET=     compile_et

.ORDER: asn1_err.c asn1_err.h
asn1_err.c asn1_err.h: ${KRB5DIR}/lib/asn1/asn1_err.et
	test -e ${.OBJDIR}/asn1_err.et || ln -sf ${.ALLSRC}
	${COMPILE_ET} asn1_err.et

CLEANFILES+=asn1_err.h asn1_err.c asn1_err.et

.ORDER: hdb_err.c hdb_err.h
hdb_err.c hdb_err.h: ${KRB5DIR}/lib/hdb/hdb_err.et
	test -e ${.OBJDIR}/hdb_err.et || ln -sf ${.ALLSRC}
	${COMPILE_ET} hdb_err.et

CLEANFILES+=hdb_err.h hdb_err.c hdb_err.et

.ORDER: heim_err.c heim_err.h
heim_err.c heim_err.h: ${KRB5DIR}/lib/krb5/heim_err.et
	test -e ${.OBJDIR}/heim_err.et || ln -sf ${.ALLSRC}
	${COMPILE_ET} heim_err.et

CLEANFILES+=heim_err.h heim_err.c heim_err.et

.ORDER: krb5_err.c krb5_err.h
krb5_err.c krb5_err.h: ${KRB5DIR}/lib/krb5/krb5_err.et
	test -e ${.OBJDIR}/krb5_err.et || ln -sf ${.ALLSRC}
	${COMPILE_ET} krb5_err.et

CLEANFILES+=krb5_err.h krb5_err.c krb5_err.et

.ORDER: kadm5_err.c kadm5_err.h
kadm5_err.c kadm5_err.h: \
	${KRB5DIR}/lib/kadm5/kadm5_err.et
	test -e ${.OBJDIR}/kadm5_err.et || ln -sf ${.ALLSRC}
	${COMPILE_ET} kadm5_err.et

CLEANFILES+=kadm5_err.h kadm5_err.c kadm5_err.et

.ORDER: k524_err.c k524_err.h
k524_err.c k524_err.h: \
	${KRB5DIR}/lib/krb5/k524_err.et
	test -e ${.OBJDIR}/k524_err.et || ln -sf ${.ALLSRC}
	${COMPILE_ET} k524_err.et

CLEANFILES+=k524_err.h k524_err.c k524_err.et

roken.h: make-roken
	./make-roken > tmp.h ;\
	if [ -f roken.h ] && cmp -s tmp.h roken.h ; then rm -f tmp.h ; \
	else rm -f roken.h; mv tmp.h roken.h; fi

make-roken.c: ${KRB5DIR}/lib/roken/roken.awk ${KRB5DIR}/lib/roken/roken.h.in
	awk -f ${.ALLSRC} > ${.TARGET}

CLEANFILES+=	make-roken.c make-roken roken.h

.else

KRB4DIR=	${.CURDIR}/../../crypto/kerberosIV
KRB5DIR=	${.CURDIR}/../../crypto/heimdal
ROKENDIR=	${.CURDIR}/../../lib/libroken

.endif
