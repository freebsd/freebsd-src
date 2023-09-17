#!/bin/sh

# From https://reviews.freebsd.org/D17011

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

cd /tmp
kldstat -v | grep -q hwpmc  || { kldload hwpmc; loaded=1; }
pmcstat -S unhalted_core_cycles -O ppid.pmcstat sleep 10
pmcstat -R ppid.pmcstat -z100 -G ppid.stacks
[ $loaded ] && kldunload hwpmc
rm -f ppid.pmcstat ppid.stacks
