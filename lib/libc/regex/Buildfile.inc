# $FreeBSD$

# regex sources
.PATH: ${.CURDIR}/regex

CFLAGS += -DPOSIX_MISTAKE

SRCS += \
	regcomp.c \
	regerror.c \
	regexec.c \
	regfree.c

SYM_MAPS +=${.CURDIR}/regex/Symbol.map

