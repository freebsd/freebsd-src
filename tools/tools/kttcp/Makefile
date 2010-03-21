#	$FreeBSD: src/tools/tools/kttcp/Makefile,v 1.2.34.1 2010/02/10 00:26:20 kensmith Exp $

SHELL=	/bin/sh

PROG=	kttcp
SRCS=	kttcp.c
BINDIR?=/usr/local/bin
SYSDIR?=/usr/src/sys

CFLAGS	+= -I${SYSDIR} -Isys

all:	kttcp module

module:
	cd sys; SYSDIR=${SYSDIR} make

install:
	install kttcp ${DESTDIR}/${BINDIR}
	cd sys; SYSDIR=${SYSDIR} make install

clean:
	rm -f ${PROG}
	cd sys; SYSDIR=${SYSDIR} make clean
