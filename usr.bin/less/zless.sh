#!/bin/sh
#
# $FreeBSD: src/usr.bin/less/zless.sh,v 1.1.20.1 2008/11/25 02:59:29 kensmith Exp $
#

export LESSOPEN="|/usr/bin/lesspipe.sh %s"
exec /usr/bin/less "$@"
