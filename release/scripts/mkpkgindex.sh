#! /bin/sh
# ex:ts=8

# Copyright (c) 2003 David E. O'Brien
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

# Creates an INDEX file suitable for an ISO distribution image from a master
# INDEX file.  The generated INDEX file contains only the packages in the
# supplied directory.

case $# in
	3) PKG_EXT="tbz" ;;
	4) PKG_EXT=$4 ;;
	*)
	echo `basename $0` "<master index file> <output index file> <pkg dir> [pkg ext]"
	exit 1
	;;
esac

PKG_LIST=$(basename `ls $3/*.${PKG_EXT}` | sed -e "s/\.${PKG_EXT}$//")
REGEX=$(echo ${PKG_LIST} | sed \
	-e 's/ /|/g' \
	-e 's/\./\\\./g' \
	-e 's/\+/\\\+/g' \
	-e 's/\^/\\\^/g')

egrep "^(${REGEX})" $1 > $2

if [ $(echo ${PKG_LIST} | wc -w) != $(wc -l < $2) ]; then
	echo "ERROR: some packages not found in $1"
	exit 1
else
	exit 0
fi
