#!/bin/sh
#-
# Copyright (c) 2003 Dag-Erling Coïdan Smørgrav
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
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
# $FreeBSD$
#

#
# This script should not be used without customization.  Remove or
# comment out the next two lines once you've adapted it to your
# environment.
#
echo "Please customize $0 for your environment."
exit 1
#

PATH=/bin:/sbin:/usr/bin:/usr/sbin
export PATH

arch=$(/usr/bin/uname -m)
mail="current@freebsd.org"
kernels="GENERIC LINT"
log="/home/des/public_html/${arch}.log"
base="/home/des/tinderbox"
src="${base}/${arch}/src"
obj="${base}/${arch}/obj"
logcmd="${base}/whereintheworld"

if /bin/test ! -x "${logcmd}" ; then
    echo "${logcmd} is absent or not executable"
    exit 1
fi

exec >"${log}.$$" 2>&1
trap '${logcmd} "${log}.$$" 2>&1 |
    /usr/bin/mail -s "${arch} tinderbox failure" "${mail}";
    mv "${log}.$$" "${log}"; exit 1' EXIT
/bin/date
set -e
cd "${src}"
/usr/bin/cvs -f -q -R -d /home/ncvs up -A -P -d
if /bin/test -d "${obj}" ; then
    /bin/mv "${obj}" "${obj}.old"
    /bin/rm -rf "${obj}.old" &
fi
/bin/mkdir -p "${obj}"
MAKEOBJDIRPREFIX="${obj}"; export MAKEOBJDIRPREFIX
__MAKE_CONF="${base}/make.conf"; export __MAKE_CONF
    /usr/bin/make -s buildworld
for kc in ${kernels} ; do
    (cd sys/${arch}/conf && make ${kc})
    /usr/bin/make -s buildkernel KERNCONF=${kc} #-DNO_WERROR
done
trap EXIT
/bin/date
/bin/mv "${log}.$$" "${log}"
