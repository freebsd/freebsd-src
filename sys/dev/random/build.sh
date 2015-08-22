#!/bin/sh
#-
# Copyright (c) 2013-2015 Mark R V Murray
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#
# Basic script to build crude unit tests.
#
# Diff-reduction checking between Yarrow and fortuna is done like so:
#
# $ diff -u -B <(sed -e 's/yarrow/wombat/g' \
#                    -e 's/YARROW/WOMBAT/g' yarrow.c) \
#              <(sed -e 's/fortuna/wombat/g' \
#                    -e 's/FORTUNA/WOMBAT/g' fortuna.c) | less
#
cc -g -O0 -pthread \
	-I../.. -lstdthreads -Wall \
	unit_test.c \
	yarrow.c \
	hash.c \
	../../crypto/rijndael/rijndael-api-fst.c \
	../../crypto/rijndael/rijndael-alg-fst.c \
	../../crypto/sha2/sha2.c \
	../../crypto/sha2/sha256c.c \
	-lz \
	-o yunit_test
cc -g -O0 -pthread \
	-I../.. -lstdthreads -Wall \
	unit_test.c \
	fortuna.c \
	hash.c \
	../../crypto/rijndael/rijndael-api-fst.c \
	../../crypto/rijndael/rijndael-alg-fst.c \
	../../crypto/sha2/sha2.c \
	../../crypto/sha2/sha256c.c \
	-lz \
	-o funit_test
