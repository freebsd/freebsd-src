#!/bin/sh

# $FreeBSD$

cd `dirname $0`
. ../../common.sh

setup_test()
{
	cat > ${WORK_DIR}/Makefile << "_EOF_"
FILES	= \
		main.c globals.h \
		util.c util.h \
		map.c map.h \
		parser.y lexer.l \
		cmdman.1 format.5
all:
	@echo "all files: ${FILES}"
	@echo "cfiles: ${FILES:M*.c}"
	@echo "hfiles: ${FILES:M*.h}"
	@echo "grammer and lexer: ${FILES:M*.[ly]}"
	@echo "man page: ${FILES:M*.[1-9]}"
	@echo "utility files: ${FILES:Mutil.?}"
	@echo "m files: ${FILES:Mm*}"
_EOF_
}

desc_test()
{
	echo "Variable expansion with M modifier"
}

eval_cmd $1
