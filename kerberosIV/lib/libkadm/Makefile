# $FreeBSD$

LIB=		kadm
CFLAGS+=	-I${KRB4DIR}/include \
		-I${INCLUDEOBJDIR} \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/lib/kdb \
		-I${KRB4DIR}/lib/kadm \
		-I${KRB4DIR}/lib/roken \
		-I${KRBOBJDIR} \
		-I${KADMOBJDIR} \
		-I${.OBJDIR}
SRCS=		kadm_cli_wrap.c kadm_stream.c kadm_supp.c \
		${KADMOBJDIR}/kadm_err.c ${KADMOBJDIR}/kadm_err.h \
		${KRBOBJDIR}/krb_err.h check_password.c protos.h

INCLUDES=	${KRB4DIR}/lib/kadm/kadm.h ${KADMOBJDIR}/kadm_err.h

.include <bsd.lib.mk>

.PATH:		${KRB4DIR}/lib/kadm

CLEANFILES+=	protos.h
