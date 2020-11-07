# $NetBSD: qequals.mk,v 1.3 2020/10/24 08:50:17 rillig Exp $

M=	i386
V.i386=	OK
V.$M?=	bug

all:
	@echo 'V.$M ?= ${V.$M}'
