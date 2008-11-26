#	$FreeBSD: src/tools/tools/kttcp/Makefile,v 1.2.26.1 2008/10/02 02:57:24 kensmith Exp $

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
