# $FreeBSD$

# Process this file with automake to create Makefile.in

bin_PROGRAMS=	bsdtar
bsdtar_SOURCES=				\
		bsdtar.c		\
		bsdtar.h		\
		bsdtar_platform.h	\
		fts.c			\
		fts.h			\
		getdate.y		\
		matching.c		\
		read.c			\
		util.c			\
		write.c
bsdtar_LDADD=	-larchive -lbz2 -lz

dist_man_MANS=	bsdtar.1

DISTCLEANFILES=	getdate.c getdate.h
