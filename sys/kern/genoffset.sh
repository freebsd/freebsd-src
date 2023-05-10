#!/bin/sh

# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2000, Bruce Evans <bde@freebsd.org>
# Copyright (c) 2018, Jeff Roberson <jeff@freebsd.org>
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

usage()
{
	echo "usage: genoffset [-o outfile] objfile"
	exit 1
}

work()
(
    local last off x1 x2 x3 struct field type lastoff lasttype

    echo "#ifndef _OFFSET_INC_"
    echo "#define _OFFSET_INC_"
    echo "#if !defined(GENOFFSET) && (!defined(KLD_MODULE) || defined(KLD_TIED))"
    last=
    temp=$(mktemp -d genoffset.XXXXXXXXXX)
    trap "rm -rf ${temp}" EXIT
    # Note: we need to print symbol values in decimal so the numeric sort works
    ${NM:='nm'} ${NMFLAGS} -t d "$1" | grep __assym_offset__ | sed -e 's/__/ /g' | sort -k 4 -k 1 -n |
    while read off x1 x2 struct field type x3; do
	off=$(echo "$off" | sed -E 's/^0+//')
	if [ "$last" != "$struct" ]; then
	    if [ -n "$last" ]; then
		echo "};"
	    fi
	    echo "struct ${struct}_lite {"
	    last=$struct
	    printf "%b" "\tu_char\tpad_${field}[${off}];\n"
	else
	    printf "%b" "\tu_char\tpad_${field}[${off} - (${lastoff} + sizeof(${lasttype}))];\n"
	fi
	printf "%b" "\t${type}\t${field};\n"
	lastoff="$off"
	lasttype="$type"
	echo "_SA(${struct}, ${field}, ${off});" >> "$temp/asserts"
    done
    echo "};"
    echo "#define _SA(s,f,o) _Static_assert(__builtin_offsetof(struct s ## _lite, f) == o, \\"
    printf '\t"struct "#s"_lite field "#f" not at offset "#o)\n'
    cat "$temp/asserts"
    echo "#undef _SA"
    echo "#endif"
    echo "#endif"
)


#
#MAIN PROGGRAM
#
use_outfile="no"
while getopts "o:" option
do
	case "$option" in
	o)	outfile="$OPTARG"
		use_outfile="yes";;
	*)	usage;;
	esac
done
shift $((OPTIND - 1))
case $# in
1)	;;
*)	usage;;
esac

if [ "$use_outfile" = "yes" ]
then
	work "$1"  3>"$outfile" >&3 3>&-
else
	work "$1"
fi

