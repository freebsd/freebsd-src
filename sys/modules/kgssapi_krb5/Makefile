.PATH: ${SRCTOP}/sys/kgssapi/krb5
KMOD=	kgssapi_krb5

SRCS=	krb5_mech.c \
	kcrypto.c \
	kcrypto_aes.c \
	opt_inet6.h

SRCS+=		kgss_if.h gssd.h
CLEANFILES=	gssd.h

S=		${SRCTOP}/sys

gssd.h:		$S/kgssapi/gssd.x
	RPCGEN_CPP=${CPP:Q} rpcgen -hM $S/kgssapi/gssd.x | grep -v pthread.h > gssd.h

.include <bsd.kmod.mk>
