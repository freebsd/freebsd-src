#!/bin/sh
# $FreeBSD: src/share/examples/hast/vip-up.sh,v 1.1.2.2.6.1 2012/03/03 06:15:13 kensmith Exp $

set -m
/root/hast/sbin/hastd/ucarp_up.sh &
set +m
exit 0
