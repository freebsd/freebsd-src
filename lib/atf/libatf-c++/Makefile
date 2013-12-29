#-
# Copyright (c) 2011 Google, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

.include <bsd.init.mk>

LIB=		atf-c++
SHLIB_MAJOR=	1

# libatf-c++ depends on the C version of the ATF library to build.
DPADD=		${LIBATFC}
LDADD=		-latf-c

LDFLAGS+=	-L${.OBJDIR}/../libatf-c

ATF=		${.CURDIR:H:H:H}/contrib/atf
.PATH:		${ATF}
.PATH:		${ATF}/atf-c++
.PATH:		${ATF}/atf-c++/detail

CFLAGS+=	-I${ATF}
CFLAGS+=	-I${.CURDIR}/../libatf-c
CFLAGS+=	-I.

CFLAGS+=	-DHAVE_CONFIG_H

SRCS=		application.cpp \
		build.cpp \
		check.cpp \
		config.cpp \
		env.cpp \
		exceptions.cpp \
		expand.cpp \
		fs.cpp \
		parser.cpp \
		process.cpp \
		tests.cpp \
		text.cpp \
		ui.cpp \
		utils.cpp

INCS=		build.hpp \
		check.hpp \
		config.hpp \
		macros.hpp \
		noncopyable.hpp \
		tests.hpp \
		utils.hpp
INCSDIR=	${INCLUDEDIR}/atf-c++

INCS+=		atf-c++.hpp
INCSDIR_atf-c++.hpp= ${INCLUDEDIR}

MAN=		atf-c++-api.3

.if ${MK_TESTS} != "no"
SUBDIR=		tests
.endif

.include <bsd.lib.mk>
