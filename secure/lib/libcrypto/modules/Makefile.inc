SHLIBDIR=	${LIBDIR}/ossl-modules

# Same compile flags as libcrypto (asm dispatch defines, NDEBUG, ...), as
# upstream builds its modules with the library flags.
.include "../Makefile.inc"

WARNS?=		0
