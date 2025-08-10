LIB= rapl
SHLIB_MAJOR = 0

SRCS= rapl-read.c
INCS= librapl.h

CFLAGS+= -I${.CURDIR}
# CFLAGS+= -I${SRCTOP}/lib/libm
LDADD += -lm
DPADD += /usr/lib/libm.so

.include <bsd.lib.mk>
