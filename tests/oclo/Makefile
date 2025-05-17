.PATH:	${SRCTOP}/cddl/contrib/opensolaris/tests/oclo

PLAIN_TESTS_C=	oclo oclo_errors ocloexec_verify

_ALLOW_ABSOLUTE_OBJ_PATH= 1

SRCS.oclo=	oclo.c \
		${SRCTOP}/crypto/openssh/openbsd-compat/recallocarray.c
CFLAGS.oclo+=	-I${SRCTOP}/crypto/openssh
LIBADD.ocloexec_verify+= util

.include <bsd.test.mk>
