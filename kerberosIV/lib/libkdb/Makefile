# $FreeBSD$

LIB=		kdb
CFLAGS+=	-I${KRB4DIR}/include \
		-I${INCLUDEOBJDIR} \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/lib/kdb \
		-I${KADMOBJDIR}
SRCS=		krb_cache.c krb_kdb_utils.c copykey.c krb_lib.c krb_dbm.c \
		print_princ.c
INCS=		kdc.h krb_db.h

.include <bsd.lib.mk>

.PATH:		${KRB4DIR}/lib/kdb
