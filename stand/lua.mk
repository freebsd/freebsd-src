# $FreeBSD$

# Common flags to build lua related files

CFLAGS+=	-I${LUASRC} -I${LDRSRC} -I${LIBLUASRC}
# CFLAGS+=	-Ddouble=jagged-little-pill -Dfloat=poison-shake -D__OMIT_FLOAT
CFLAGS+=	-DLUA_FLOAT_TYPE=LUA_FLOAT_INT64
