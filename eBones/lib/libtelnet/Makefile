#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$FreeBSD$

LIB=	telnet
SRCS=   encrypt.c genget.c getent.c misc.c

CFLAGS+= -DHAS_CGETENT -DENCRYPTION

.if exists(${DESTDIR}/usr/lib/libkrb.a) && defined(MAKE_EBONES)
CFLAGS+= -DDES_ENCRYPTION -DAUTHENTICATION -DKRB4 -I/usr/include/kerberosIV
SRCS+=  auth.c kerberos.c enc_des.c
LDADD+= -ldes -lkrb
DPADD+= ${LIBDES} ${LIBKRB}
.endif

# Not Yet
#SRCS += spx.c rsaencpwd.c read_password.c

# KRB4_ENCPWD not yet defined
# Used only in krb4encpwd.c and rsaencpwd.c, not yet active
#LDADD+= -ldescrypt

.include <bsd.lib.mk>
