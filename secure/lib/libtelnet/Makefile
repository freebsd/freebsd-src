#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$Id: Makefile,v 1.11 1995/10/11 00:03:59 gibbs Exp $

LIB=	telnet
SRCS=   auth.c encrypt.c genget.c getent.c misc.c
SRCS+=  enc_des.c
SRCS+=	spx.c rsaencpwd.c read_password.c
CFLAGS+= -DHAS_CGETENT

#ifdef	ENCRYPTION

CFLAGS+= -DENCRYPTION -DAUTHENTICATION

.if exists(/usr/lib/libkrb.a)
CFLAGS+= -DKRB4 -I/usr/include/kerberosIV -DDES_ENCRYPTION
# KRB4_ENCPWD not yet defined
#CFLAGS+= -DKRB4_ENCPWD
SRCS+=  kerberos.c
# KRB4_ENCPWD not yet defined
#SRCS+=   krb4encpwd.c
LDADD+= -ldes -lkrb
.endif

.if exists(/usr/lib/libkrb5.a)
CFLAGS+= -DKRB5 -DFORWARD -DDES_ENCRYPTION
SRCS+=	kerberos5.c forward.c
LDADD+= -ldes -lkrb5
.endif

# Used only in krb4encpwd.c and rsaencpwd.c, not yet active
#LDADD+= -ldescrypt

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
