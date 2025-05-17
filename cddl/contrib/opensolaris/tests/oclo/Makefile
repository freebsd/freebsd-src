PROGS=		oclo oclo_errors ocloexec_verify

_ALLOW_ABSOLUTE_OBJ_PATH=1

SRCS.oclo=	oclo.c
SRCS.oclo+=	/usr/src/crypto/openssh/openbsd-compat/recallocarray.c
CFLAGS.oclo+=	-I/usr/src/crypto/openssh
SRCS.oclo_errors=	oclo_errors.c
LIBADD.ocloexec_verify+=	util
SRCS.ocloexec_verify=	ocloexec_verify.c

BINDIR=	/usr/tests/bin/
MAN=

.include <bsd.progs.mk>
