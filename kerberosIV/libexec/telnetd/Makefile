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

DPADD=		${LIBUTIL} ${LIBTERMCAP} ${LIBTELNET} ${LIBDES} ${LIBKRB} \
		${LIBCRYPT}
LDADD=		-lutil -ltermcap -L${TELNETOBJDIR} -ltelnet -ldes \
		-L${KRBOBJDIR} -lkrb -lcrypt

.include <bsd.prog.mk>

.PATH:	${TELNETDIR}/telnetd
