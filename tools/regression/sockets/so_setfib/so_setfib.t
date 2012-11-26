#!/bin/sh
#-
# Copyright (c) 2012 Cisco Systems, Inc.
# All rights reserved.
#
# This software was developed by Bjoern Zeeb under contract to
# Cisco Systems, Inc..
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
#

cd `dirname $0`

EXECUTABLE=`basename $0 .t`

FIBS=`sysctl -n net.fibs`
INET=`sysctl -n kern.features.inet`
INET6=`sysctl -n kern.features.inet6`

case "${INET}" in
1)	OPTS="${OPTS} -DINET" ;;
*)	INET=0 ;;
esac
case "${INET6}" in
1)	OPTS="${OPTS} -DINET6" ;;
*)	INET6=0 ;;
esac

make ${EXECUTABLE} ${OPTS} 2>&1 > /dev/null

# two out of bounds on each side + 3 random
FIBS=$((2 + FIBS + 2 + 3))
# ROUTE and LOCAL are 1 domain together given 2 or 1 types only
TESTS=$(((1 + ${INET} + ${INET6}) * 3 * ${FIBS}))

echo "1..${TESTS}"

exec ./${EXECUTABLE}
