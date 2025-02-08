#!/bin/sh
#
# Copyright (c) 2003 Thomas Klausner.
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

set -u
grep=grep
zcat=zstdcat

endofopts=0
pattern_file=0
pattern_found=0
grep_args=""
hyphen=0
silent=0

prg=${0##*/}

# handle being called 'zegrep' or 'zfgrep'
case ${prg} in
*egrep)
	grep_args="-E";;
*fgrep)
	grep_args="-F";;
esac

catargs="-f"
case ${prg} in
zstd*)
	cattool="/usr/bin/zstdcat"
	catargs="-fq"
	;;
bz*)
	cattool="/usr/bin/bzcat"
	;;
z*)
	cattool="/usr/bin/zcat"
	;;
xz*)
	cattool="/usr/bin/xzcat"
	;;
lz*)
	cattool="/usr/bin/lzcat"
	;;
*)
	echo "Invalid command: ${prg}" >&2
	exit 1
	;;
esac

# skip all options and pass them on to grep taking care of options
# with arguments, and if -e was supplied

while [ $# -gt 0 -a ${endofopts} -eq 0 ]
do
    case $1 in
    # from GNU grep-2.6.0 -- keep in sync!
	--)
	    shift
	    endofopts=1
	    ;;
	--file=*)
	    pattern_file=1
	    grep_args="${grep_args} ${1}"
	    shift
	    ;;
	--regexp=*)
	    if [ ${pattern_found} -ne 0 ]; then
		grep_args="${grep_args} -e ${pattern}"
	    fi
	    pattern="${1#--regexp=}"
	    pattern_found=1
	    shift
	    ;;
	-h|--no-filename)
	    silent=1
	    shift
	    ;;
	-V|--version)
	    exec ${grep} -V
	    ;;
	--*)
	    grep_args="${grep_args} $1"
	    shift
	    ;;
	-[EFGHILOSUVabchilnopqsuvwxyz]*)
	    post="${1#-?}"
	    pre=${1%${post}}
	    grep_args="${grep_args} ${pre}"
	    shift
	    # Put back partial arg
	    set -- "-${post}" $*
	    ;;
	-[ABCDdefm])
	    if [ $# -lt 2 ]
		then
		echo "${prg}: missing argument for $1 flag" >&2
		exit 1
	    fi
	    case $1 in
		-e)
		    if [ ${pattern_found} -ne 0 ]; then
			grep_args="${grep_args} -e ${pattern}"
		    fi
		    pattern="$2"
		    pattern_found=1
		    shift 2
		    continue
		    ;;
		-f)
		    pattern_file=1
		    ;;
		*)
		    ;;
	    esac
	    grep_args="${grep_args} $1 $2"
	    shift 2
	    ;;
	-[ABCDdefm]*)
	    post="${1#-e}"
	    case ${1} in
		-e*)
		    if [ ${pattern_found} -ne 0 ]; then
			grep_args="${grep_args} -e ${pattern}"
		    fi
		    pattern="${post}"
		    pattern_found=1
		    shift
		    continue
		    ;;
		-f*)
		    pattern_file=1
		    ;;
		*)
		    ;;
	    esac
	    grep_args="${grep_args} ${post}"
	    shift
	    ;;
	-)
	    hyphen=1
	    shift
	    ;;
	-r|-R)
	    echo "${prg}: the ${1} flag is not currently supported" >&2
	    exit 1
	    ;;
	-?)
	    grep_args="${grep_args} $1"
	    shift
	    ;;
	*)
	    # pattern to grep for
	    endofopts=1
	    ;;
    esac
done

# if no -e option was found, take next argument as grep-pattern
if [ ${pattern_file} -eq 0 -a ${pattern_found} -eq 0 ]
then
    if [ $# -ge 1 ]; then
	pattern="$1"
	shift
    elif [ ${hyphen} -gt 0 ]; then
	pattern="-"
    else
	echo "${prg}: missing pattern" >&2
	exit 1
    fi
    pattern_found=1
fi

# Clean up possible leading blank
grep_args="${grep_args# }"

# call grep ...
if [ $# -lt 1 ]
then
    # ... on stdin
    if [ ${pattern_file} -eq 0 ]; then
	${cattool} ${catargs} - | ${grep} ${grep_args} -e "${pattern}" -- -
    else
	${cattool} ${catargs} - | ${grep} ${grep_args} -- -
    fi
    ret=$?
else
    # ... on all files given on the command line
    if [ ${silent} -lt 1 -a $# -gt 1 ]; then
	grep_args="-H ${grep_args}"
    fi
    # Succeed if any file matches.  First assume no match.
    ret=1
    for file; do
	if [ ${pattern_file} -eq 0 ]; then
	    ${cattool} ${catargs} -- "${file}" |
		${grep} --label="${file}" ${grep_args} -e "${pattern}" -- -
	else
	    ${cattool} ${catargs} -- "${file}" |
		${grep} --label="${file}" ${grep_args} -- -
	fi
	this_ret=$?
	# A match (0) overrides a no-match (1).  An error (>=2) overrides all.
	if [ ${this_ret} -eq 0 -a ${ret} -eq 1 ] || [ ${this_ret} -ge 2 ]; then
	    ret=${this_ret}
	fi
    done
fi

exit ${ret}
