# $FreeBSD$

# Some of the required math functions (div & mod) are implemented in
# libcompiler_rt on ARM. The library also needs to be placed first to be
# correctly linked. As some of the functions are used before we have
# shared libraries.
LIBADD+=	compiler_rt
CFLAGS+=	-mfpu=none
