# $Id: Makefile 2141 2011-11-10 15:03:40Z jkoshy $

TOP=		../..

ELFCOPY=	${TOP}/elfcopy/elfcopy

TEST_LOG=	test.log

.MAIN:	all

.PHONY:	clobber execute test

execute test: ${ELFCOPY}
	/bin/sh run.sh

clobber:	clean
	rm -f ${TEST_LOG}

SUBDIR=		plugin

.include <bsd.subdir.mk>
