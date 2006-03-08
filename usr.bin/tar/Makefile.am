# $FreeBSD$

# Process this file with automake to create Makefile.in

bin_PROGRAMS=	bsdtar
bsdtar_SOURCES=				\
		bsdtar.c		\
		bsdtar.h		\
		bsdtar_platform.h	\
		getdate.y		\
		matching.c		\
		read.c			\
		tree.c			\
		tree.h			\
		util.c			\
		write.c
bsdtar_LDADD= -L../libarchive -larchive -lbz2 -lz
bsdtar_CPPFLAGS= -I../libarchive
dist_man_MANS=	bsdtar.1
EXTRA_DIST= test/test.sh
