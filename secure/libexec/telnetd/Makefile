# $FreeBSD$

# Do not define -DKLUDGELINEMODE, as it does not interact well with many
# telnet implementations.

PROG=		telnetd
MAN8=		telnetd.8

CFLAGS+=	-DLINEMODE -DUSE_TERMIO -DDIAGNOSTICS -DOLD_ENVIRON \
		-DENV_HACK -DAUTHENTICATION -DENCRYPTION \
		-I${TELNETDIR} -DINET6

SRCS=		global.c slc.c state.c sys_term.c telnetd.c \
		termstat.c utility.c authenc.c

DPADD=		${LIBUTIL} ${LIBTERMCAP} ${LIBTELNET} ${LIBCRYPTO} ${LIBMP} \
                ${LIBCRYPT}
LDADD=		-lutil -ltermcap -L${TELNETOBJDIR} -ltelnet -lcrypto -lcrypt
LDADD+=		-lmp -lpam

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnetd
