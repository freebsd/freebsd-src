#	@(#)Makefile	8.2 (Berkeley) 12/15/93

# This stuff need original libdes to run (new_rnd_key.c module),
# current eBones/des lib don't have it
# Kerberos4 stuff ifdefed by MAKE_KERBEROS (make.conf) because of it

PROG=	telnetd
CFLAGS+=-DLINEMODE -DKLUDGELINEMODE -DUSE_TERMIO -DDIAGNOSTICS
CFLAGS+=-DOLD_ENVIRON -DENV_HACK
CFLAGS+=-I${.CURDIR}/../../lib
SRCS=	authenc.c global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c
DPADD=	${LIBUTIL} ${LIBTERMCAP}
LDADD=	-lutil -ltermcap -ltelnet

MAN8=	telnetd.8

#ifdef	ENCRYPTION

CFLAGS+=-DAUTHENTICATION -DENCRYPTION

.ifdef MAKE_KERBEROS
.if exists(/usr/lib/libkrb.a)
LDADD+= -ldes -lkrb
.endif
.endif

.if exists(/usr/lib/libkrb5.a)
LDADD+= -ldes -lkrb5
.endif

LDADD+= -ldescrypt

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
