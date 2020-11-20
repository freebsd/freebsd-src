# $NetBSD: sunshcmd.mk,v 1.3 2020/10/24 08:50:17 rillig Exp $

BYECMD=		  echo bye
LATERCMD=	  echo later
TEST1 :sh	= echo hello
TEST2 :sh	= ${BYECMD}
TEST3=		  ${LATERCMD:sh}

all:
	@echo "TEST1=${TEST1}"
	@echo "TEST2=${TEST2}"
	@echo "TEST3=${TEST3}"
