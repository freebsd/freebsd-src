#	@(#)Makefile	8.2 (Berkeley) 12/15/93

# Do not define -DKLUDGELINEMODE, as it does not interact well with many
# telnet implementations.

PROG=	telnetd
MAN8=	telnetd.8

SRCS=	authenc.c global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c

DPADD=	${TELNETOBJDIR}/libtelnet.a ${LIBUTIL} ${LIBTERMCAP}
LDADD=	-L${TELNETOBJDIR} -lutil -ltermcap -ltelnet

CFLAGS+=-DLINEMODE -DUSE_TERMIO -DDIAGNOSTICS -DOLD_ENVIRON -DENV_HACK
CFLAGS+=-I${.CURDIR}/../../lib

#ifdef	ENCRYPTION

CFLAGS+=-DAUTHENTICATION -DENCRYPTION

.if exists(/usr/lib/libkrb.a)
LDADD+= -ldes -lkrb
.endif

.if exists(/usr/lib/libkrb5.a)
LDADD+= -ldes -lkrb5
.endif

# Used only in krb4encpwd.c and rsaencpwd.c (libtelnet), not yet active
#LDADD+= -ldescrypt

#endif	/* ENCRYPTION */

# These are the sources that have encryption stuff in them.
CRYPT_SRC= authenc.c ext.h state.c telnetd.c termstat.c
CRYPT_SRC+= utility.c Makefile
NOCRYPT_DIR=${.CURDIR}/Nocrypt

.include <bsd.prog.mk>

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
