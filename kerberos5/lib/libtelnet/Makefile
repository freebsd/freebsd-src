# $FreeBSD$

LIB=		telnet

INTERNALLIB=	yes
INTERNALSTATICLIB=yes
NOPIC=		yes

SRCS=		genget.c getent.c misc.c encrypt.c auth.c kerberos.c \
		enc_des.c pk.c sra.c

CFLAGS+=	-DHAS_CGETENT -DENCRYPTION -DDES_ENCRYPTION -DAUTHENTICATION \
		-DKRB4 -DSRA -I${TELNETDIR} \
		-I${KRB4DIR}/lib/krb

INCS=		${TELNETDIR}/arpa/telnet.h 
INCDIR=		/usr/include/arpa

#
# Remove obsolete shared libraries, if any.  We don't bother moving them
# to/usr/lib/compat, since they were only used by telnet, telnetd and
# tn3270.
#
beforeinstall:
	rm -f ${DESTDIR}${SHLIBDIR}/lib${LIB}.so.2.0

.include <bsd.lib.mk>

.PATH:	${TELNETDIR}/libtelnet
