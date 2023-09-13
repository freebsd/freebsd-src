#!/bin/sh -
#
# SPDX-License-Identifier: BSD-3-Clause
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
# 3. Neither the name of the University nor the names of its contributors
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

# Command line options:
#
#	-c	Print the copyright / license statement as a C comment and exit
#
#	-r	Reproducible build.  Do not embed directory names, user	names,
#		time stamps or other dynamic information into the output file.
#		This is intended to allow two builds done at different times
#		and even by different people on different hosts to produce
#		identical output.
#
#	-R	Reproducible build if the tree represents an unmodified
#		checkout from a version control system.  Metadata is included
#		if the tree is modified.
#
#	-V var	Print ${var}="${val-of-var}" and exit
#
#	-v	Print TYPE REVISION BRANCH RELEASE VERSION RELDATE variables
#		like the -V command
#

TYPE="FreeBSD"
REVISION="15.0"
BRANCH="CURRENT"
if [ -n "${BRANCH_OVERRIDE}" ]; then
	BRANCH=${BRANCH_OVERRIDE}
fi
unset RELEASE
unset VERSION

if [ -z "${SYSDIR}" ]; then
	SYSDIR=$(dirname $0)/..
fi

# allow random overrides
while :
do
	case "$1" in
	*=*) eval "$1"; shift;;
	*) break;;
	esac
done

RELEASE="${RELEASE:-${REVISION}-${BRANCH}}"
VERSION="${VERSION:-${TYPE} ${RELEASE}}"

RELDATE=$(awk '/^#define[[:space:]]*__FreeBSD_version/ {print $3}' ${PARAMFILE:-${SYSDIR}/sys/param.h})

if [ -r "${SYSDIR}/../COPYRIGHT" ]; then
	year=$(sed -Ee '/^Copyright .* The FreeBSD Project/!d;s/^.*1992-([0-9]*) .*$/\1/g' ${SYSDIR}/../COPYRIGHT)
else
	year=$(date +%Y)
fi
# look for copyright template
b=share/examples/etc/bsd-style-copyright
for bsd_copyright in $b ../$b ../../$b ../../../$b /usr/src/$b /usr/$b
do
	if [ -r "$bsd_copyright" ]; then
		COPYRIGHT=$(sed \
		    -e "s/\[year\]/1992-$year/" \
		    -e 's/\[your name here\]\.* /The FreeBSD Project./' \
		    -e 's/\[your name\]\.*/The FreeBSD Project./' \
		    -e '/\[id for your version control system, if any\]/d' \
		    $bsd_copyright)
		break
	fi
done

# no copyright found, use a dummy
if [ -z "$COPYRIGHT" ]; then
	COPYRIGHT="/*-
 * Copyright (c) 1992-$year The FreeBSD Project.
 *
 */"
fi

# add newline
COPYRIGHT="$COPYRIGHT
"

# We expand include_metadata later since we may set it to the
# future value of modified.
include_metadata=yes
modified=no
while getopts crRvV: opt; do
	case "$opt" in
	c)
		echo "$COPYRIGHT"
		exit 0
		;;
	r)
		include_metadata=no
		;;
	R)
		include_metadata=if-modified
		;;
	v)
		# Only put variables that are single lines here.
		for v in TYPE REVISION BRANCH RELEASE VERSION RELDATE; do
			eval val=\$${v}
			echo ${v}=\"${val}\"
		done
		exit 0
		;;
	V)
		v=$OPTARG
		eval val=\$${v}
		echo ${v}=\"${val}\"
		VARS_ONLY_EXIT=1
		;;
	esac
done
shift $((OPTIND - 1))

# VARS_ONLY means no files should be generated, this is just being
# included.
[ -n "$VARS_ONLY" ] && return 0

# VARS_ONLY_EXIT means no files should be generated, only the value of
# variables are being output.
[ -n "$VARS_ONLY_EXIT" ] && exit 0

#
# findvcs dir
#	Looks up directory dir at world root and up the filesystem
#
findvcs()
{
	local savedir

	savedir=$(pwd)
	cd ${SYSDIR}/..
	while [ $(pwd) != "/" ]; do
		if [ -e "./$1" ]; then
			VCSTOP=$(pwd)
			VCSDIR=${VCSTOP}"/$1"
			cd ${savedir}
			return 0
		fi
		cd ..
	done
	cd ${savedir}
	return 1
}

git_tree_modified()
{
	! $git_cmd "--work-tree=${VCSTOP}" -c core.checkStat=minimal -c core.fileMode=off diff --quiet
}

LC_ALL=C; export LC_ALL
if [ ! -r version ]
then
	echo 0 > version
fi

touch version
v=$(cat version)
u=${USER:-root}
d=$(pwd)
h=${HOSTNAME:-$(hostname)}
if [ -n "$SOURCE_DATE_EPOCH" ]; then
	if ! t=$(date -r $SOURCE_DATE_EPOCH 2>/dev/null); then
		echo "Invalid SOURCE_DATE_EPOCH" >&2
		exit 1
	fi
else
	t=$(date)
fi
i=$(${MAKE:-make} -V KERN_IDENT)
compiler_v=$($(${MAKE:-make} -V CC) -v 2>&1 | grep -w 'version')

for dir in /usr/bin /usr/local/bin; do
	if [ ! -z "${svnversion}" ] ; then
		break
	fi
	if [ -x "${dir}/svnversion" ] && [ -z ${svnversion} ] ; then
		# Run svnversion from ${dir} on this script; if return code
		# is not zero, the checkout might not be compatible with the
		# svnversion being used.
		${dir}/svnversion $(realpath ${0}) >/dev/null 2>&1
		if [ $? -eq 0 ]; then
			svnversion=${dir}/svnversion
			break
		fi
	fi
done

if [ -z "${svnversion}" ] && [ -x /usr/bin/svnliteversion ] ; then
	/usr/bin/svnliteversion $(realpath ${0}) >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		svnversion=/usr/bin/svnliteversion
	else
		svnversion=
	fi
fi

if findvcs .git; then
	for dir in /usr/bin /usr/local/bin; do
		if [ -x "${dir}/git" ] ; then
			git_cmd="${dir}/git -c help.autocorrect=0 --git-dir=${VCSDIR}"
			break
		fi
	done
fi

if findvcs .gituprevision; then
	gituprevision="${VCSTOP}/.gituprevision"
fi

if findvcs .hg; then
	for dir in /usr/bin /usr/local/bin; do
		if [ -x "${dir}/hg" ] ; then
			hg_cmd="${dir}/hg -R ${VCSDIR}"
			break
		fi
	done
fi

if [ -n "$svnversion" ] ; then
	svn=$(cd ${SYSDIR} && $svnversion 2>/dev/null)
	case "$svn" in
	[0-9]*[MSP]|*:*)
		svn=" r${svn}"
		modified=yes
		;;
	[0-9]*)
		svn=" r${svn}"
		;;
	*)
		unset svn
		;;
	esac
fi

if [ -n "$git_cmd" ] ; then
	git=$($git_cmd rev-parse --verify --short HEAD 2>/dev/null)
	if [ "$($git_cmd rev-parse --is-shallow-repository)" = false ] ; then
		git_cnt=$($git_cmd rev-list --first-parent --count HEAD 2>/dev/null)
		if [ -n "$git_cnt" ] ; then
			git="n${git_cnt}-${git}"
		fi
	fi
	git_b=$($git_cmd rev-parse --abbrev-ref HEAD)
	if [ -n "$git_b" -a "$git_b" != "HEAD" ] ; then
		git="${git_b}-${git}"
	fi
	if git_tree_modified; then
		git="${git}-dirty"
		modified=yes
	fi
	git=" ${git}"
fi

if [ -n "$gituprevision" ] ; then
	gitup=" $(awk -F: '{print $2}' $gituprevision)"
fi

if [ -n "$hg_cmd" ] ; then
	hg=$($hg_cmd id 2>/dev/null)
	hgsvn=$($hg_cmd svn info 2>/dev/null | \
		awk -F': ' '/Revision/ { print $2 }')
	if [ -n "$hgsvn" ] ; then
		svn=" r${hgsvn}"
	fi
	if [ -n "$hg" ] ; then
		hg=" ${hg}"
	fi
fi

[ ${include_metadata} = "if-modified" -a ${modified} = "yes" ] && include_metadata=yes
if [ ${include_metadata} != "yes" ]; then
	VERINFO="${VERSION}${svn}${git}${gitup}${hg} ${i}"
	VERSTR="${VERINFO}\\n"
else
	VERINFO="${VERSION} #${v}${svn}${git}${gitup}${hg}: ${t}"
	VERSTR="${VERINFO}\\n    ${u}@${h}:${d}\\n"
fi

vers_content_new=$(cat << EOF
$COPYRIGHT
#define SCCSSTR "@(#)${VERINFO}"
#define VERSTR "${VERSTR}"
#define RELSTR "${RELEASE}"

char sccs[sizeof(SCCSSTR) > 128 ? sizeof(SCCSSTR) : 128] = SCCSSTR;
char version[sizeof(VERSTR) > 256 ? sizeof(VERSTR) : 256] = VERSTR;
char compiler_version[] = "${compiler_v}";
char ostype[] = "${TYPE}";
char osrelease[sizeof(RELSTR) > 32 ? sizeof(RELSTR) : 32] = RELSTR;
int osreldate = ${RELDATE};
char kern_ident[] = "${i}";
EOF
)
vers_content_old=$(cat vers.c 2>/dev/null || true)
if [ "$vers_content_new" != "$vers_content_old" ]; then
	printf "%s\n" "$vers_content_new" > vers.c
fi

echo $((v + 1)) > version
