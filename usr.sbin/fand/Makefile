.include <src.opts.mk>

PROG=	fand
MAN=	fand.8 fand.cprofile.5
SRCS=	fand.c cprofile.c utils.c
INCS=	cprofile.h utils.h

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

# Will limit sysctl sensors to the ones having CTLFLAG_CAPRD.
# CFLAGS+=	-DUSE_CAPSICUM

# Will require cprofile to be blessed by veriexec(4) if loaded.
CFLAGS+=	-DUSE_VERIEXEC

CFLAGS+= -I${SRCTOP}/contrib/libucl/include
LIBADD=  ucl util

.include <bsd.prog.mk>
