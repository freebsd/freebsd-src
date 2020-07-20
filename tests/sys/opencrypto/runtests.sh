#!/bin/sh -
#
# Copyright (c) 2014 The FreeBSD Foundation
# All rights reserved.
# Copyright 2019 Enji Cooper
#
# This software was developed by John-Mark Gurney under
# the sponsorship from the FreeBSD Foundation.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

: ${PYTHON=python3}

if [ ! -d /usr/local/share/nist-kat ]; then
	echo "1..0 # SKIP: nist-kat package not installed for test vectors"
	exit 0
fi

if ! $PYTHON -c "from dpkt import dpkt"; then
	echo "1..0 # SKIP: py-dpkt package not installed"
	exit 0
fi

loaded_modules=
cleanup_tests()
{
	trap - EXIT INT TERM

	set +e

	if [ -n "$oldcdas" ]; then
		sysctl "$oldcdas" 2>/dev/null
	fi

	# Unload modules in reverse order
	for loaded_module in $(echo $loaded_modules | tr ' ' '\n' | sort -r); do
		kldunload $loaded_module
	done
}
trap cleanup_tests EXIT INT TERM

cpu_type="$(uname -p)"
cpu_module=

case ${cpu_type} in
aarch64)
	cpu_module=nexus/armv8crypto
	;;
amd64|i386)
	cpu_module=nexus/aesni
	;;
esac

for required_module in $cpu_module cryptodev; do
	if ! kldstat -q -m $required_module; then
		module_to_load=${required_module#nexus/}
		if ! kldload ${module_to_load}; then
			echo "1..0 # SKIP: could not load ${module_to_load}"
			exit 0
		fi
		loaded_modules="$loaded_modules $required_module"
	fi
done

cdas_sysctl=kern.cryptodevallowsoft
if ! oldcdas=$(sysctl -e $cdas_sysctl); then
	echo "1..0 # SKIP: could not resolve sysctl: $cdas_sysctl"
	exit 0
fi
if ! sysctl $cdas_sysctl=1; then
	echo "1..0 # SKIP: could not enable /dev/crypto access via $cdas_sysctl sysctl."
	exit 0
fi

echo "1..1"
if "$PYTHON" $(dirname $0)/cryptotest.py $CRYPTOTEST_ARGS; then
	echo "ok 1"
else
	echo "not ok 1"
fi
