# $FreeBSD$

SUBDIR=	libroken libvers libasn1 libhdb libkrb5 libkadm5clnt libkadm5srv libsl libgssapi
.if defined(MAKE_KERBEROS4) && \
	(${MAKE_KERBEROS4} == "YES" || ${MAKE_KERBEROS4} == "yes")
SUBDIR+=libkafs5
.endif
.include <bsd.subdir.mk>
