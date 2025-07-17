#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Add support for kldload(8) of compressed modules.

[ $# -ne 1 ] && echo "Usage: $0 <module>" && exit 1

# If possible avoid using a temporary file due to kgdb.
kldstat | grep -q $1 && exit 0
kldload $1 2>/dev/null && exit 0

module=$(basename $1 .ko)
paths=`sysctl -n kern.module_path`

: ${TMPDIR:=/tmp}
tmpkld=$(mktemp $TMPDIR/$module.XXXXXX) || exit
trap "rm -f $tmpkld" EXIT INT TERM
IFS=';'
for path in $paths; do
	kld_base=$path/$module.ko
	for ext in "" .gz .bz2 .xz; do
		kld=$kld_base$ext
		[ -s $kld ] || continue
		case "$kld" in
		*.gz)
		    gzcat $kld
		    ;;
		*.bz2)
		    bzcat $kld
		    ;;
		*.xz)
		    xzcat $kld
		    ;;
		*)
		    cat $kld
		;;
		esac > $tmpkld
		[ $? -eq 0 ] && break
		rm -f $tmpkld
	done
	[ -s $tmpkld ] && kldload $tmpkld; ec=$?
	[ $ec -eq 0 ] && exit
done

echo "Could not kldload $kld_base*"
exit $ec
