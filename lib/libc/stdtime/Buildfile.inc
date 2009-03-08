# $FreeBSD$

.PATH: ${.CURDIR}/stdtime ${.CURDIR}/../locale

SRCS += \
	asctime.c \
	difftime.c \
	localtime.c \
	strftime.c \
	strptime.c \
	time32.c \
	timelocal.c

SYM_MAPS += ${.CURDIR}/stdtime/Symbol.map
