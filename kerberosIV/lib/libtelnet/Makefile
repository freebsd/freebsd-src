#	$Id$

LIB=		telnet

SRCS=		genget.c getent.c misc.c encrypt.c auth.c kerberos.c enc_des.c

CFLAGS+=	-DHAS_CGETENT -DENCRYPTION -DDES_ENCRYPTION -DAUTHENTICATION \
		-DKRB4 -I${TELNETDIR} \
		-I${KRB4DIR}/lib/krb

INCLUDES=	${TELNETDIR}/arpa/telnet.h 

.include <bsd.lib.mk>

.PATH:	${TELNETDIR}/libtelnet
