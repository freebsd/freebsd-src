#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$Id: Makefile,v 1.1.1.1 1994/08/12 22:02:57 csgr Exp $

LIB=	telnet
SRCS=	auth.c encrypt.c genget.c getent.c misc.c
SRCS+=	kerberos.c enc_des.c
SRCS+=	spx.c rsaencpwd.c read_password.c
CFLAGS+= -DHAS_CGETENT

#ifdef	ENCRYPTION
CFLAGS+= -DENCRYPTION -DAUTHENTICATION

.if exists(/usr/lib/libkrb.a)
CFLAGS+= -DKRB4 -DKRB4_ENCPWD -DDES_ENCRYPTION -I/usr/include/kerberosIV
SRCS+=   krb4encpwd.c
.endif

.if exists(/usr/lib/libkrb5.a)
CFLAGS+= -DKRB5 -DDES_ENCRYPTION -DFORWARD
SRCS+=	kerberos5.c forward.c
.endif
#endif	/* ENCRYPTION */

# These are the sources that have encryption stuff in them.
CRYPT_SRC= auth.c enc-proto.h enc_des.c encrypt.c
CRYPT_SRC+= encrypt.h kerberos.c kerberos5.c krb4encpwd.c
CRYPT_SRC+= misc.c spx.c Makefile
NOCRYPT_DIR=${.CURDIR}/Nocrypt

.include <bsd.lib.mk>

nocrypt:
#ifdef	ENCRYPTION
	@for i in ${CRYPT_SRC}; do \
	    if [ ! -d ${NOCRYPT_DIR} ]; then \
		echo Creating subdirectory ${NOCRYPT_DIR}; \
		mkdir ${NOCRYPT_DIR}; \
	    fi; \
	    echo ${NOCRYPT_DIR}/$$i; \
	    unifdef -UENCRYPTION ${.CURDIR}/$$i | \
		sed "s/ || defined(ENCRYPTION)//" > ${NOCRYPT_DIR}/$$i; \
	done

placeholder:
#else	/* ENCRYPTION */
	@echo "Encryption code already removed."
#endif	/* ENCRYPTION */
