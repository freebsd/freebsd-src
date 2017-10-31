#!/bin/sh
#
# Copyright (c) 2017 Ngie Cooper
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

# usage: gather_coverage

SCRIPT=${0##*/}

: ${COVERAGE_OUTPUT=coverage-output}
: ${GCOV=gcov}

error()
{
	printf >&2 "${SCRIPT}: ERROR: $@\n"
}

require_command()
{
	local cmd=$1; shift

	if ! command -v $cmd >/dev/null; then
		error "required command not found: $cmd"
		if [ $# -gt 0 ]; then
			printf >&2 "$@\n"
		fi
		exit 1
	fi
}



require_command ${GCOV} \
    "Install gcov from base or the appropriate version from ports"
for cmd in lcov genhtml; do
	require_command ${cmd} "Install devel/lcov from ports"
done

if ! COVERAGE_TMP=$(mktemp -d tmp.XXXXXX); then
	error "failed to create COVERAGE_TMP"
	exit 1
fi
trap "rm -Rf '$COVERAGE_TMP'" EXIT INT TERM

set -e

lcov --gcov-tool ${GCOV} --capture --directory ${COVERAGE_TMP} --output-file \
    ${COVERAGE_TMP}/coverage.info
genhtml ${COVERAGE_TMP}/coverage.info --output-directory ${COVERAGE_OUTPUT}

printf "${SCRIPT}: INFO: coverage output successfully placed in ${COVERAGE_OUTPUT}\n"
