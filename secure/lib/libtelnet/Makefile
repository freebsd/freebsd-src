#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$Id$

LIB=	telnet
SRCS=   encrypt.c genget.c getent.c misc.c
CFLAGS+= -DHAS_CGETENT

.if defined(MAKE_EBONES)

CFLAGS+= -DENCRYPTION

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_EBONES)
CFLAGS+= -DDES_ENCRYPTION -DAUTHENTICATION
CFLAGS+= -DKRB4 -I/usr/include/kerberosIV
# KRB4_ENCPWD not yet defined
SRCS+=  auth.c kerberos.c enc_des.c
LDADD+= -ldes -lkrb
DPADD+= ${LIBDES} ${LIBKRB}
.endif

# Not Yet
#SRCS += spx.c rsaencpwd.c read_password.c

# Used only in krb4encpwd.c and rsaencpwd.c, not yet active
#LDADD+= -ldescrypt

.endif	/* ENCRYPTION */

# These are the sources that have encryption stuff in them.
CRYPT_SRC= auth.c enc-proto.h enc_des.c encrypt.c
CRYPT_SRC+= encrypt.h kerberos.c kerberos5.c krb4encpwd.c
CRYPT_SRC+= misc.c spx.c Makefile
NOCRYPT_DIR=${.CURDIR}/Nocrypt

.include <bsd.lib.mk>

nocrypt:
.if defined(ENCRYPTION)
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
.else
	@echo "Encryption code already removed."
.endif
