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
# $FreeBSD$

TYPE="FreeBSD"
REVISION="11.0"
BRANCH="RELEASE-p7"
if [ -n "${BRANCH_OVERRIDE}" ]; then
	BRANCH=${BRANCH_OVERRIDE}
fi
RELEASE="${REVISION}-${BRANCH}"
VERSION="${TYPE} ${RELEASE}"

if [ -z "${SYSDIR}" ]; then
    SYSDIR=$(dirname $0)/..
fi

if [ -n "${PARAMFILE}" ]; then
	RELDATE=$(awk '/__FreeBSD_version.*propagated to newvers/ {print $3}' \
		${PARAMFILE})
else
	RELDATE=$(awk '/__FreeBSD_version.*propagated to newvers/ {print $3}' \
		${SYSDIR}/sys/param.h)
fi

b=share/examples/etc/bsd-style-copyright
if [ -r "${SYSDIR}/../COPYRIGHT" ]; then
	year=$(sed -Ee '/^Copyright .* The FreeBSD Project/!d;s/^.*1992-([0-9]*) .*$/\1/g' ${SYSDIR}/../COPYRIGHT)
else
	year=$(date +%Y)
fi
# look for copyright template
for bsd_copyright in ../$b ../../$b ../../../$b /usr/src/$b /usr/$b
do
	if [ -r "$bsd_copyright" ]; then
		COPYRIGHT=`sed \
		    -e "s/\[year\]/1992-$year/" \
		    -e 's/\[your name here\]\.* /The FreeBSD Project./' \
		    -e 's/\[your name\]\.*/The FreeBSD Project./' \
		    -e '/\[id for your version control system, if any\]/d' \
		    $bsd_copyright` 
		break
	fi
done

# no copyright found, use a dummy
if [ -z "$COPYRIGHT" ]; then
	COPYRIGHT="/*-
 * Copyright (c) 1992-$year The FreeBSD Project.
 * All rights reserved.
 *
 */"
fi

# add newline
COPYRIGHT="$COPYRIGHT
"

# VARS_ONLY means no files should be generated, this is just being
# included.
if [ -n "$VARS_ONLY" ]; then
	return 0
fi

LC_ALL=C; export LC_ALL
if [ ! -r version ]
then
	echo 0 > version
fi

touch version
v=`cat version`
u=${USER:-root}
d=`pwd`
h=${HOSTNAME:-`hostname`}
if [ -n "$SOURCE_DATE_EPOCH" ]; then
	if ! t=`date -r $SOURCE_DATE_EPOCH 2>/dev/null`; then
		echo "Invalid SOURCE_DATE_EPOCH" >&2
		exit 1
	fi
else
	t=`date`
fi
i=`${MAKE:-make} -V KERN_IDENT`
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

for dir in /usr/bin /usr/local/bin; do
	if [ -x "${dir}/p4" ] && [ -z ${p4_cmd} ] ; then
		p4_cmd=${dir}/p4
	fi
done
if [ -d "${SYSDIR}/../.git" ] ; then
	for dir in /usr/bin /usr/local/bin; do
		if [ -x "${dir}/git" ] ; then
			git_cmd="${dir}/git --git-dir=${SYSDIR}/../.git"
			break
		fi
	done
fi

if [ -d "${SYSDIR}/../.hg" ] ; then
	for dir in /usr/bin /usr/local/bin; do
		if [ -x "${dir}/hg" ] ; then
			hg_cmd="${dir}/hg -R ${SYSDIR}/.."
			break
		fi
	done
fi

if [ -n "$svnversion" ] ; then
	svn=`cd ${SYSDIR} && $svnversion 2>/dev/null`
	case "$svn" in
	[0-9]*)	svn=" r${svn}" ;;
	*)	unset svn ;;
	esac
fi

if [ -n "$git_cmd" ] ; then
	git=`$git_cmd rev-parse --verify --short HEAD 2>/dev/null`
	svn=`$git_cmd svn find-rev $git 2>/dev/null`
	if [ -n "$svn" ] ; then
		svn=" r${svn}"
		git="=${git}"
	else
		svn=`$git_cmd log | fgrep 'git-svn-id:' | head -1 | \
		     sed -n 's/^.*@\([0-9][0-9]*\).*$/\1/p'`
		if [ -z "$svn" ] ; then
			svn=`$git_cmd log --format='format:%N' | \
			     grep '^svn ' | head -1 | \
			     sed -n 's/^.*revision=\([0-9][0-9]*\).*$/\1/p'`
		fi
		if [ -n "$svn" ] ; then
			svn=" r${svn}"
			git="+${git}"
		else
			git=" ${git}"
		fi
	fi
	git_b=`$git_cmd rev-parse --abbrev-ref HEAD`
	if [ -n "$git_b" ] ; then
		git="${git}(${git_b})"
	fi
	if $git_cmd --work-tree=${SYSDIR}/.. diff-index \
	    --name-only HEAD | read dummy; then
		git="${git}-dirty"
	fi
fi

if [ -n "$p4_cmd" ] ; then
	p4version=`cd ${SYSDIR} && $p4_cmd changes -m1 "./...#have" 2>&1 | \
		awk '{ print $2 }'`
	case "$p4version" in
	[0-9]*)
		p4version=" ${p4version}"
		p4opened=`cd ${SYSDIR} && $p4_cmd opened ./... 2>&1`
		case "$p4opened" in
		File*) ;;
		//*)	p4version="${p4version}+edit" ;;
		esac
		;;
	*)	unset p4version ;;
	esac
fi

if [ -n "$hg_cmd" ] ; then
	hg=`$hg_cmd id 2>/dev/null`
	svn=`$hg_cmd svn info 2>/dev/null | \
		awk -F': ' '/Revision/ { print $2 }'`
	if [ -n "$svn" ] ; then
		svn=" r${svn}"
	fi
	if [ -n "$hg" ] ; then
		hg=" ${hg}"
	fi
fi

cat << EOF > vers.c
$COPYRIGHT
#define SCCSSTR "@(#)${VERSION} #${v}${svn}${git}${hg}${p4version}: ${t}"
#define VERSTR "${VERSION} #${v}${svn}${git}${hg}${p4version}: ${t}\\n    ${u}@${h}:${d}\\n"
#define RELSTR "${RELEASE}"

char sccs[sizeof(SCCSSTR) > 128 ? sizeof(SCCSSTR) : 128] = SCCSSTR;
char version[sizeof(VERSTR) > 256 ? sizeof(VERSTR) : 256] = VERSTR;
char compiler_version[] = "${compiler_v}";
char ostype[] = "${TYPE}";
char osrelease[sizeof(RELSTR) > 32 ? sizeof(RELSTR) : 32] = RELSTR;
int osreldate = ${RELDATE};
char kern_ident[] = "${i}";
EOF

echo $((v + 1)) > version
