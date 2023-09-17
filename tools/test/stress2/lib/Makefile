LIB=	stress
SRCS=	main.c options.c random_int.c resources.c
INTERNALLIB=	true
MK_AUTO_OBJ=	no

.include "../testcases/Makefile.inc"
CFLAGS+=-fPIE

.include <bsd.lib.mk>
