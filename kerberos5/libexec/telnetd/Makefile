# $FreeBSD$

# Do not define -DKLUDGELINEMODE, as it does not interact well with many
# telnet implementations.

PROG=		telnetd
MAN=		telnetd.8

CFLAGS+=	-DLINEMODE -DUSE_TERMIO -DDIAGNOSTICS -DOLD_ENVIRON \
		-DENV_HACK -DAUTHENTICATION -DENCRYPTION \
		-I${TELNETDIR} -DINET6

SRCS=		global.c slc.c state.c sys_term.c telnetd.c \
		termstat.c utility.c authenc.c

DPADD=
LDADD=

.if defined(MAKE_KERBEROS4)
DPADD+=		${LIBKRB} ${LIBCOM_ERR}
LDADD+=		-lkrb -lcom_err
.endif

DPADD+=		${LIBUTIL} ${LIBTERMCAP} ${LIBTELNET} ${LIBMP} ${LIBCRYPTO} \
		${LIBCRYPT} ${LIBPAM}
LDADD+=		-lutil -ltermcap ${LIBTELNET} -lmp -lcrypto \
		-lcrypt ${MINUSLPAM}

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnetd
