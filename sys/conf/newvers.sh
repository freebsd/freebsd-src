#!/bin/sh -
#
# Copyright (c) 1984, 1986, 1990, 1993
#	The Regents of the University of California.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by the University of
#	California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	@(#)newvers.sh	8.1 (Berkeley) 4/20/94
#	$Id: newvers.sh,v 1.26.2.22 1998/03/07 12:08:12 jkh Exp $

TYPE="FreeBSD"
REVISION="2.2.6"
BRANCH="BETA"
RELEASE="${REVISION}-${BRANCH}"
SNAPDATE=""
if [ "X${SNAPDATE}" != "X" ]; then
	RELEASE="${RELEASE}-${SNAPDATE}"
fi
VERSION="${TYPE} ${RELEASE}"

RELDATE="226000"

b=share/examples/etc/bsd-style-copyright
year=`date '+%Y'`
# look for copyright template
for bsd_copyright in ../$b ../../$b ../../../$b /usr/src/$b /usr/$b
do
	if [ -r "$bsd_copyright" ]; then
		COPYRIGHT=`sed \
		    -e "s/\[year\]/$year/" \
		    -e 's/\[your name here\]\.* /FreeBSD Inc./' \
		    -e 's/\[your name\]\.*/FreeBSD Inc./' \
		    -e '/\[id for your version control system, if any\]/d' \
		    $bsd_copyright` 
		break
	fi
done

# no copyright found, use a dummy
if [ X"$COPYRIGHT" = X ]; then
	COPYRIGHT="/*
 * Copyright (c) $year
 *	FreeBSD Inc. All rights reserved.
 *
 */"
fi

# add newline
COPYRIGHT="$COPYRIGHT
"

LC_TIME=C; export LC_TIME
if [ ! -r version ]
then
	echo 0 > version
fi

touch version
v=`cat version` u=${USER-root} d=`pwd` h=`hostname` t=`date`
cat /dev/null > vers.c
echo "$COPYRIGHT" >> vers.c
echo "char ostype[] = \"${TYPE}\";" >> vers.c
echo "char osrelease[] = \"${RELEASE}\";" >> vers.c
echo "int osreldate = ${RELDATE};" >> vers.c
echo "char sccs[4] = { '@', '(', '#', ')' };" >>vers.c
echo "char version[] = \
	\"${VERSION} #${v}: ${t}\\n    ${u}@${h}:${d}\\n\";" >>vers.c

echo `expr ${v} + 1` > version
