#!/bin/sh
# $FreeBSD: src/share/examples/hast/vip-up.sh,v 1.1.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $

set -m
/root/hast/sbin/hastd/ucarp_up.sh &
set +m
exit 0
