# $FreeBSD: src/tools/regression/fstest/Makefile,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

OSTYPE=$(shell uname)

ifeq "${OSTYPE}" "FreeBSD"
CFLAGS += -D__OS_FreeBSD__
CFLAGS += -DHAS_LCHMOD -DHAS_CHFLAGS -DHAS_LCHFLAGS
endif

ifeq "${OSTYPE}" "SunOS"
CFLAGS += -D__OS_Solaris__
CFLAGS += -DHAS_TRUNCATE64 -DHAS_STAT64
endif

ifeq "${OSTYPE}" "Darwin"
CFLAGS += -D__OS_Darwin__
CFLAGS += -DHAS_LCHMOD -DHAS_CHFLAGS -DHAS_LCHFLAGS
endif

ifeq "${OSTYPE}" "Linux"
CFLAGS += -D__OS_Linux__
CFLAGS += -D_GNU_SOURCE
endif

all:	fstest

fstest:	fstest.c
	gcc -Wall ${CFLAGS} fstest.c -o fstest

clean:
	rm -f fstest
