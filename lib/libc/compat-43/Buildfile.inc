# $FreeBSD$

# compat-43 sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/compat-43 ${.CURDIR}/compat-43

SRCS += \
	creat.c \
	gethostid.c \
	getwd.c \
	killpg.c \
	sethostid.c \
	setpgrp.c \
	setrgid.c \
	setruid.c \
	sigcompat.c

SYM_MAPS += ${.CURDIR}/compat-43/Symbol.map
