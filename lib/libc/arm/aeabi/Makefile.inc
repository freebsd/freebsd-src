# $FreeBSD$

.PATH: ${.CURDIR}/arm/aeabi

SRCS+=	aeabi_atexit.c		\
	aeabi_double.c		\
	aeabi_float.c		\
	aeabi_unwind_cpp.c

SYM_MAPS+=${.CURDIR}/arm/aeabi/Symbol.map

