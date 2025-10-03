#
# Copyright (c) 2025 Mark Johnston <markj@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

set -e

n=$(sysctl -n hw.ncpu)
if [ $n -lt 2 ]; then
    echo "This test requires at least 2 CPUs"
    exit 1
fi

# Set up some things expected by selftest.py.
kldload -n pf siftr
pfctl -e || true
jail -c name=gdbselftest vnet persist

echo "I'm about to panic your system, ctrl-C now if that's not what you want."
sleep 10
sysctl debug.debugger_on_panic=0
sysctl debug.kdb.panic=1
