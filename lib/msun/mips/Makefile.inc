# $FreeBSD$

LDBL_PREC = 53
SYM_MAPS += ${.CURDIR}/mips/Symbol.map
# Clang will compile nearbyint() into an infinite recursion without this flag
CFLAGS += -fno-builtin
