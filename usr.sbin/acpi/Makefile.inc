# $Id: Makefile.inc,v 1.1 2000/07/14 18:16:22 iwasaki Exp $
# $FreeBSD$

CFLAGS+= -Wall -static -I${.CURDIR}/../../../sys
BINDIR?= /usr/sbin
