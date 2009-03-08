# $FreeBSD$

.PATH:	${BSDSRCTOP}/contrib/ipfilter \
	${BSDSRCTOP}/contrib/ipfilter/lib \
	${BSDSRCTOP}/contrib/ipfilter/tools \
	${BSDSRCTOP}/contrib/ipfilter/man

CFLAGS += -I${BSDSRCTOP}/contrib/ipfilter
CFLAGS += -I${BSDSRCTOP}/contrib/ipfilter/tools
CFLAGS += -I${BSDSRCTOP}/sys
CFLAGS += -I${BSDSRCTOP}/sys/contrib/ipfilter
CFLAGS += -DSTATETOP -D__UIO_EXPOSE

LDADD += -lipf -lkvm

.include "../Buildfile.inc"
