#!/bin/sh
#
# Copyright 2001 Bruce A. Mah
#
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
# THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# test-pkg_version.sh
#
# Regression testing for pkg_version
# Originally from an idea by "Akinori MUSHA" <knu@iDaemons.org>
#
# $FreeBSD$
#

ECHO=echo
PKG_VERSION=./pkg_version

test-pv ( ) { \
    setvar v1 $1
    setvar answer $2
    setvar v2 $3
    setvar type $4
    res=`${PKG_VERSION} -t ${v1} ${v2}`
    if [ ${res} != ${answer} ]; then \
	${ECHO} "${type} test failed (${v1} ${res} ${v2}, should have been ${answer})"; \
    fi
}

# Test coercion of default PORTREVISION and PORTEPOCH
test-pv 0.10 "=" 0.10_0 coercion
test-pv 0.10 "=" 0.10,0 coercion
test-pv 0.10 "=" 0.10_0,0 coercion
  
# Test various comparisons
test-pv 1.0 "=" 1.0 equality
test-pv 2.15a "=" 2.15a equality

test-pv 0.10 ">" 0.9 inequality
test-pv 0.9 "<" 0.10 inequality

test-pv 2.3p10 ">" 2.3p9 number/letter
test-pv 1.6.0 ">" 1.6.0.p3 number/letter
test-pv 1.0.b ">" 1.0.a3 number/letter
test-pv 1.0a ">" 1.0 number/letter
test-pv 1.0a "<" 1.0b number/letter
test-pv 5.0a ">" 5.0.b number/letter

test-pv 1.5_1 ">" 1.5 portrevision
test-pv 1.5_2 ">" 1.5_1 portrevision
test-pv 1.5_1 "<" 1.5.0.1 portrevision
test-pv 1.5 "<" 1.5.0.1 portrevision

test-pv 00.01.01,1 ">" 99.12.31 portepoch
test-pv 0.0.1_1,2 ">" 0.0.1,2 portrevision/portepoch
test-pv 0.0.1_1,3 ">" 0.0.1_2,2 portrevision/portepoch

test-pv 2.0 ">" 2.a2 number/letter
test-pv 3 "=" 3.0 equality
test-pv 4a "<" 4a0 letter/zero
test-pv 10a1b2 "=" 10a1.b2 separator

test-pv 7pl "=" 7.pl patchevel
test-pv 8.0.a "=" 8.0alpha alpha
test-pv 9.b3.0 "=" 9beta3 beta
test-pv 10.pre7 "=" 10pre7.0 pre
test-pv 11.r "=" 11.rc rc

test-pv 12pl "<" 12alpha alpha/patchevel
test-pv 13.* "<" 13.pl star/patchevel

test-pv 1.0.0+2003.09.06 "=" 1.0+2003.09.06 plus/multiple
test-pv 1.0.1+2003.09.06 ">" 1.0+2003.09.06 plus/multiple
test-pv 1.0.0+2003.09.06 "<" 1.0+2003.09.06_1 plus/portrevision
test-pv 1.0.1+2003.09.06 ">" 1.0+2003.09.06_1 plus/portrevision
