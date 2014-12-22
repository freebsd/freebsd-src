# $Id: Makefile 3025 2014-04-18 16:20:25Z jkoshy $

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

.include "${TOP}/mk/elftoolchain.subdir.mk"
