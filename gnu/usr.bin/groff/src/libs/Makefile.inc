# $FreeBSD$

TOPREL=		../../..
DIST_SUBDIR=	src/libs/${.CURDIR:T}

CXX:=${CXX:C/^c\+\+|^clang\+\+/g++/}

.include "../../Makefile.inc"
