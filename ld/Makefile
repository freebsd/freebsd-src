# $Id: Makefile 2910 2013-02-03 06:06:23Z kaiwang27 $

TOP=	..

PROG=	ld
WARNS?=	6

SRCS=	amd64.c			\
	amd64_script.c		\
	i386.c			\
	i386_script.c		\
	ld_arch.c		\
	ld_dynamic.c		\
	ld_ehframe.c		\
	ld_error.c 		\
	ld_exp.c		\
	ld_file.c		\
	ld_hash.c		\
	ld_input.c		\
	ld_layout.c		\
	ld_main.c 		\
	ld_options.c		\
	ld_output.c		\
	ld_path.c		\
	ld_reloc.c		\
	ld_script.c		\
	ld_script_lexer.l	\
	ld_script_parser.y	\
	ld_strtab.c		\
	ld_symbols.c		\
	ld_symver.c		\
	y.tab.h

.SUFFIXES:	.ld .c
.ld.c:
	awk -f ld_script.awk ${.ALLSRC} > ${.TARGET}

GENSRCS=	amd64_script.c i386_script.c

CLEANFILES+=	y.tab.h ${GENSRCS}

DPADD=	${LIBELFTC} ${LIBELF} ${LIBDWARF}
LDADD=	-lelftc -ldwarf -lelf

CFLAGS+= -I. -I${.CURDIR}

NOMAN=

.include "${TOP}/mk/elftoolchain.prog.mk"
