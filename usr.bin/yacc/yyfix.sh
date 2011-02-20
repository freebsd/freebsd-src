#!/bin/sh -
#
# Copyright (c) 1990 The Regents of the University of California.
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
# $FreeBSD$
#
#	@(#)yyfix.sh	5.2 (Berkeley) 5/12/90
#
OLDYACC="yyexca yyact yypact yypgo yyr1 yyr2 yychk yydef"
NEWYACC="yylhs yylen yydefred yydgoto yysindex yyrindex yygindex \
	 yytable yycheck"

if [ $# -eq 0 ]; then
	echo "usage: $0 file [tables]" >&2
	exit 1
fi

file=$1
>$file
shift

if [ $# -eq 0 ] ; then
	if grep yylhs y.tab.c > /dev/null ; then
		if grep yyname y.tab.c > /dev/null ; then
			NEWYACC="$NEWYACC yyname"
		fi
		if grep yyrule y.tab.c > /dev/null ; then
			NEWYACC="$NEWYACC yyrule"
		fi
		set $NEWYACC
	else
		set $OLDYACC
	fi
fi

for i
do
ed - y.tab.c << END
/^\(.*\)$i[ 	]*\[]/s//extern \1 $i[];\\
\1 $i []/
.ka
/}/kb
'br $file
'a,.w $file
'a,.d
w
q
END
done
