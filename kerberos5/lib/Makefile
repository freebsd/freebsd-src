
# $FreeBSD$

SUBDIR=	libasn1 libgssapi_krb5 libgssapi_ntlm libgssapi_spnego libhdb \
	libheimntlm libhx509 libkadm5clnt libkadm5srv libkrb5 \
	libroken libsl libvers libkdc libwind libheimbase libheimipcc libheimipcs

SUBDIR+= libkafs5  # requires krb_err.h from libkrb5
SUBDIR_DEPEND_libkafs5=	libkrb5
SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
