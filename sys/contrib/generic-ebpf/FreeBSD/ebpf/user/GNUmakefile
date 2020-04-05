BASE=	../../..
MACHINE!=	uname -m
MACHINE_ARCH!=	uname -p
include ${BASE}/Makefile.inc
ifeq (${COVERAGE},"1")
CFLAGS+=	-coverage
LDFLAGS+=	-coverage
endif
include Makefile.common
