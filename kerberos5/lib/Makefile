
# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	libasn1 libgssapi_krb5 libgssapi_ntlm libgssapi_spnego libhdb \
	libheimntlm libhx509 libkadm5clnt libkadm5srv libkrb5 \
	libroken libsl libvers libkdc libwind libheimbase libheimipcc libheimipcs

SUBDIR+= libkafs5  # requires krb_err.h from libkrb5
SUBDIR_DEPEND_libkafs5=	libkrb5
SUBDIR_PARALLEL=

.if defined(LIBCHERI)
.if ${MK_LIBCHERI_ALL} == "no"
SUBDIR:=
.else
.if ${MK_LIBCHERI_BROKEN} == "no"
# __DECONT-like thing in roken.h is broken
SUBDIR:=	${SUBDIR:Nlibgssapi_krb5:Nlibgssapi_ntlm:Nlibgssapi_spnego:Nlibhx509}
# missing krb_err.h
SUBDIR:=	${SUBDIR:Nlibkafs5}
.endif
.endif
.endif

.include <bsd.subdir.mk>
