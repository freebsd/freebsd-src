#	$Id$

LIB=		acl
CFLAGS+=	-I${KRB4DIR}/include \
		-I${INCLUDEOBJDIR} \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/lib/acl
SRCS=		acl_files.c
MAN3=		${KRB4DIR}/man/acl_check.3
MLINKS=		acl_check.3 acl_canonicalize_principal.3 \
		acl_check.3 acl_exact_match.3 \
		acl_check.3 acl_add.3 \
		acl_check.3 acl_delete.3 \
		acl_check.3 acl_initialize.3

INCLUDES=	${KRB4DIR}/lib/acl/acl.h

.include <bsd.lib.mk>

.PATH:		${KRB4DIR}/lib/acl
