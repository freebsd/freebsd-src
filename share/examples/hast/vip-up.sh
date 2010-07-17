#!/bin/sh
# $FreeBSD: src/share/examples/hast/vip-up.sh,v 1.1.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $

set -m
/root/hast/sbin/hastd/ucarp_up.sh &
set +m
exit 0
