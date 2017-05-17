#-
# Copyright (c) 2015, 2016 (SRI International)
# All rights reserved.
#
# This software was developed by SRI International and the University of
# Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
# ("CTSRD"), as part of the DARPA CRASH research programme.
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

.if !${.TARGETS:Mbuild-tools} && !defined(BOOTSTRAPPING)
.if defined(NEED_CHERI)
.if ${MK_CHERI} == "no"
.error NEED_CHERI defined, but CHERI is not enabled
.endif
.if ${NEED_CHERI} != "hybrid" && ${NEED_CHERI} != "pure" && ${NEED_CHERI} != "sandbox"
.error NEED_CHERI must be 'hybrid', 'pure', or 'sandbox'
.endif
WANT_CHERI:= ${NEED_CHERI}
.endif

.if defined(LIB_CXX) || defined(PROG_CXX) || defined(SHLIB_CXX)
WANT_CHERI=	none
.endif

.if ${MK_CHERI} != "no" && defined(WANT_CHERI) && ${WANT_CHERI} != "none"
.if !defined(CHERI_CC)
.error CHERI is enabled and request, but CHERI_CC is undefined
.endif
.if !exists(${CHERI_CC}) 
.error CHERI_CC is defined to ${CHERI_CC} which does not exist
.endif

_CHERI_CC=	${CHERI_CC} -g -integrated-as --target=cheri-unknown-freebsd \
		-msoft-float
.if defined(SYSROOT)
_CHERI_CC+=	--sysroot=${SYSROOT}
.endif
.if ${MK_CHERI_EXACT_EQUALS} == "yes"
_CHERI_CC+=	-mllvm -cheri-exact-equals
.endif

# Turn off deprecated warnings
_CHERI_CC+= -Wno-deprecated-declarations

.if ${WANT_CHERI} == "pure" || ${WANT_CHERI} == "sandbox"
OBJCOPY:=	objcopy
MIPS_ABI=	purecap
_CHERI_CC+=	-mxgot -fpic
LIBDIR:=	/usr/libcheri
ROOTOBJDIR=	${.OBJDIR:S,${.CURDIR},,}${SRCTOP}/worldcheri${SRCTOP}
CFLAGS+=	${CHERI_OPTIMIZATION_FLAGS:U-O2} -ftls-model=local-exec
.ifdef NO_WERROR
# Implicit function declarations should always be an error in purecap mode as
# we will probably generate wrong code for calling them
CFLAGS+=-Werror=implicit-function-declaration
.endif
# Clang no longer defines __LP64__ for Cheri purecap ABI but there are a
# lot of files that use it to check for not 32-bit
# XXXAR: Remove this once we have checked all the #ifdef __LP64__ uses
CFLAGS+=	-D__LP64__=1
ALLOW_SHARED_TEXTREL=	yes
LDFLAGS+=	-Wl,-melf64btsmip_cheri_fbsd
.if defined(__BSD_PROG_MK)
_LIB_OBJTOP=	${ROOTOBJDIR}
.endif
.ifdef LIBCHERI
LDFLAGS+=	-Wl,-init=crt_init_globals
.endif
.if ${WANT_CHERI} == "sandbox"
CHERI_LLD_BROKEN=	yes
.endif
.ifdef CHERI_LLD_BROKEN
LDFLAGS+=	-fuse-ld=bfd
.else
LDFLAGS+=	-fuse-ld=lld -Wl,-z,norelro
.endif
.else
STATIC_CFLAGS+= -ftls-model=local-exec # MIPS/hybrid case
.endif

.if ${MK_CHERI128} == "yes"
_CHERI_CC+=	-mllvm -cheri128
# XXX: Needed as Clang rejects -mllvm -cheri128 when using $CC to link.
_CHERI_CFLAGS+=	-Qunused-arguments
.endif

.if ${WANT_CHERI} != "variables"
.if ${MK_CHERI_SHARED} == "no"
NO_SHARED=	yes
.elif defined(__BSD_PROG_MK) && ${MK_CHERI_SHARED_PROG} == "no"
NO_SHARED=	yes
.endif
CC:=	${_CHERI_CC}
COMPILER_TYPE=	clang
CFLAGS+=	${_CHERI_CFLAGS}
# Don't remove CHERI symbols from the symbol table
STRIP_FLAGS+=	-w --keep-symbol=__cheri_callee_method.\* \
		--keep-symbol=__cheri_method.\*
.endif
.endif
.endif
