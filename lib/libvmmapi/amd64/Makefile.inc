SRCS+=	ppt.c			\
	vmmapi_machdep.c	\
	vmmapi_freebsd_machdep.c

CFLAGS+= -DWITH_VMMAPI_SNAPSHOT
