# $FreeBSD$

.include <src.opts.mk>

NO_LINT=

KRB5DIR=	${.CURDIR}/../../../crypto/heimdal

CFLAGS+=	-DHAVE_CONFIG_H -I${.CURDIR}/../../include

.if ${MK_OPENLDAP} != "no" && !defined(COMPAT_32BIT)
OPENLDAPBASE?=	/usr/local
LDAPLDADD=	-lldap -llber
LDAPDPADD=	${LDAPLDADD:C;^-l(.*)$;${OPENLDAPBASE}/lib/lib\1.a;}
LDAPCFLAGS=	-I${OPENLDAPBASE}/include -DOPENLDAP=1 -DLDAP_DEPRECATED=1
LDAPLDFLAGS=	-L${OPENLDAPBASE}/lib -rpath ${OPENLDAPBASE}/lib
LDFLAGS+=	${LDAPLDFLAGS}
.endif

.if defined(SRCS)

ETSRCS=	\
	${KRB5DIR}/lib/asn1/asn1_err.et \
	${KRB5DIR}/lib/hdb/hdb_err.et \
	${KRB5DIR}/lib/kadm5/kadm5_err.et \
	${KRB5DIR}/lib/krb5/heim_err.et \
	${KRB5DIR}/lib/krb5/k524_err.et \
	${KRB5DIR}/lib/krb5/krb5_err.et \
	${KRB5DIR}/lib/krb5/krb_err.et \
	${KRB5DIR}/lib/gssapi/krb5/gkrb5_err.et \
	${KRB5DIR}/lib/hx509/hx509_err.et \
	${KRB5DIR}/lib/wind/wind_err.et \
	${KRB5DIR}/lib/ntlm/ntlm_err.et

.for ET in ${ETSRCS}
.for _ET in ${ET:T:R}
.if ${SRCS:M${_ET}.[ch]} != ""
.ORDER: ${_ET}.h ${_ET}.c
${_ET}.c ${_ET}.h: ${ET}
	compile_et ${.ALLSRC}
CLEANFILES+=	${_ET}.h ${_ET}.c
.endif
.endfor
.endfor

.endif # defined(SRCS)

ASN1_COMPILE=	asn1_compile
MAKE_ROKEN=	make-roken
SLC=		slc
