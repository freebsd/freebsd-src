# $FreeBSD: src/usr.bin/tar/Makefile.am,v 1.1 2004/08/07 03:24:48 kientzle Exp $

# Process this file with automake to create Makefile.in

bin_PROGRAMS=	bsdtar
bsdtar_SOURCES=				\
		bsdtar.c		\
		bsdtar.h		\
		bsdtar_platform.h	\
		fts.c			\
		fts.h			\
		matching.c		\
		read.c			\
		util.c			\
		write.c
bsdtar_LDADD=	-larchive -lbz2 -lz

dist_man_MANS=	bsdtar.1
