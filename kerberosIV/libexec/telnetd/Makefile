# $FreeBSD$

# Do not define -DKLUDGELINEMODE, as it does not interact well with many
# telnet implementations.

PROG=		telnetd
MAN=		telnetd.8

SRCS=		global.c slc.c state.c sys_term.c telnetd.c \
		termstat.c utility.c authenc.c

CFLAGS+=	-DLINEMODE -DUSE_TERMIO -DDIAGNOSTICS -DOLD_ENVIRON \
		-DENV_HACK -DAUTHENTICATION -DENCRYPTION \
		-I${TELNETDIR} -DINET6
CFLAGS+=	-DKRB4

WARNS?=		2

DPADD=		${LIBUTIL} ${LIBTERMCAP} ${LIBTELNET} ${LIBMP} ${LIBCRYPTO} \
		${LIBKRB} ${LIBCOM_ERR} \
		${LIBCRYPT} ${LIBPAM}
LDADD=		-lutil -ltermcap ${LIBTELNET} -lmp -lcrypto \
		-L${KRBOBJDIR} -lkrb -lcom_err \
		-lcrypt ${MINUSLPAM}

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnetd
