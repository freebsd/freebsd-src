# $FreeBSD$

LIB=		telnet

INTERNALLIB=	yes
INTERNALSTATICLIB=yes
NOPIC=		yes

SRCS=		genget.c getent.c misc.c encrypt.c auth.c \
		enc_des.c sra.c pk.c
.if defined(MAKE_KERBEROS4)
SRCS+=		kerberos.c
CFLAGS+=	-DKRB4 -I${KRB4DIR}/lib/krb
.endif

#SRCS+=		kerberos5.c

CFLAGS+=	-DHAS_CGETENT -DENCRYPTION -DDES_ENCRYPTION -DAUTHENTICATION \
		-DSRA -I${TELNETDIR}
#CFLAGS+=	-DKRB5 -I${KRB5DIR}/lib/krb5

INCS=		${TELNETDIR}/arpa/telnet.h
INCDIR=		/usr/include/arpa

#
# Remove obsolete shared libraries, if any.  We don't bother moving them
# to /usr/lib/compat, since they were only used by telnet, telnetd and
# tn3270.
#
beforeinstall:
	rm -f ${DESTDIR}${SHLIBDIR}/lib${LIB}.so.2.0

.include <bsd.lib.mk>

.PATH:	${TELNETDIR}/libtelnet
